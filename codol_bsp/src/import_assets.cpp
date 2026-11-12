// Carries materials - with their technique set, vertex declarations, shaders and images - from the zones of the
// install into the zone that is being built.
//
// A shipped zone stores these assets nested in whatever used them first ( a world material inside the gfx_map, a
// technique inside the first technique set that has it ), so they cannot be taken as whole top level assets. They are
// written again from the loaded image of the donor zone, in the order the game's loaders read them
// ( Load_Material: name, technique set, texture table + images, constants, state bits; Load_MaterialTechniqueSet:
// name, per technique { header, passes, per pass { vertex declaration, vertex shader, pixel shader, arguments +
// literal constants }, name } ), as top level assets of the output zone. ZoneEditor::write runs the game's loaders
// over the result, so a sequence that does not fit is refused there.

#include "bsp_impl.hpp"

namespace codol::bsp
{
	namespace
	{
		constexpr std::uint32_t TECHNIQUE_HEADER = 8;

		template <typename T>
		std::vector<std::uint8_t> raw_of(const T& value)
		{
			const auto* p = reinterpret_cast<const std::uint8_t*>(&value);
			return { p, p + sizeof(T) };
		}
	}

	struct asset_importer::state
	{
		ZoneEditor& out;
		const zone::ClientKey& key;
		std::filesystem::path install;
		std::vector<std::filesystem::path> donors;
		const catalog& cat;

		std::map<std::string, std::unique_ptr<Zone>> zones;			// by path
		std::map<std::string, std::unique_ptr<ZoneEditor>> editors;
		std::vector<std::unique_ptr<Zone>> donor_zones;
		bool donors_open = false;

		std::map<std::string, material_desc, std::less<>> described;
		std::map<std::pair<int, std::string>, zone_ref> by_name;		// ( type, name ) -> asset of the output zone
		std::map<std::pair<const Zone*, zaddr>, zone_ref> by_address;	// techniques, declarations, shaders of a donor

		state(ZoneEditor& o, const zone::ClientKey& k, std::filesystem::path i, std::vector<std::filesystem::path> d, const catalog& c)
			: out(o), key(k), install(std::move(i)), donors(std::move(d)), cat(c) {}

		std::size_t add(const std::int32_t type, const std::string& name, const asset_builder& b)
		{
			std::size_t index = npos;
			check(out.add_built_asset(type, name, b.chunks, npos, &index), "adding " + zone::asset_type_name(type) + " '" + name + "'");
			return index;
		}

		// zeroed header with the name ",<name>": the asset of that name of a zone that is loaded already
		static void reference_stub(asset_builder& b, const std::uint32_t size, const std::uint32_t name_offset, const std::string_view name)
		{
			const std::uint32_t header = b.add_zero(size);
			b.follows(header, name_offset);
			b.add_string("," + std::string(name));
		}

		zone_ref image_from(const Zone& donor, const zaddr header)
		{
			zone::ImageView view;
			if (!zone::read_image(donor, header, view))
			{
				throw compile_error("an image of zone '" + donor.name() + "' cannot be read");
			}
			const std::string name(view.name);
			const auto key_name = std::make_pair(static_cast<int>(c1::ASSET_TYPE_IMAGE), name);
			if (const auto it = by_name.find(key_name); it != by_name.end())
			{
				return it->second;
			}
			asset_builder b;
			c1::GfxImage raw = view.raw;
			const zaddr load_def = raw.loadDef;
			raw.name = FOLLOWS;
			raw.loadDef = load_def ? FOLLOWS : 0;
			b.add(raw);
			b.add_string(name);
			if (load_def)
			{
				c1::GfxImageLoadDef def = view.streamed ? donor.get<c1::GfxImageLoadDef>(load_def) : view.loadDef;
				if (view.streamed)
				{
					def.resourceSize = 0;
				}
				b.add(def);
				if (def.resourceSize)
				{
					if (view.pixels.size() != def.resourceSize)
					{
						throw compile_error("image '" + name + "' of zone '" + donor.name() + "': its pixels are not in the zone image");
					}
					b.add(view.pixels.data(), view.pixels.size());
				}
			}
			zone_ref ref;
			ref.slot = true;
			ref.asset = add(c1::ASSET_TYPE_IMAGE, name, b);
			by_name[key_name] = ref;
			return ref;
		}

		// vertex declaration / vertex shader / pixel shader of a pass: inline at its first use, later uses point at
		// the field that held it
		void pass_asset(const Zone& donor, asset_builder& b, const std::uint32_t pass_chunk, const std::uint32_t field, const zaddr header,
			const bool declaration, const std::size_t self, std::vector<std::pair<std::pair<const Zone*, zaddr>, zone_ref>>& pending)
		{
			if (!header)
			{
				return;
			}
			const auto key_address = std::make_pair(&donor, header);
			if (const auto it = by_address.find(key_address); it != by_address.end())
			{
				point_ref(b, pass_chunk, field, it->second);
				return;
			}
			for (const auto& [k, ref] : pending)
			{
				if (k == key_address)
				{
					b.point(pass_chunk, field, ref.chunk, ref.offset);
					return;
				}
			}
			b.follows(pass_chunk, field);
			if (const zone::AssetReference* reference = donor.reference(header))
			{
				reference_stub(b, declaration ? sizeof(c1::MaterialVertexDeclaration) : sizeof(c1::MaterialShader), 0, reference->name);
			}
			else if (declaration)
			{
				c1::MaterialVertexDeclaration decl = donor.get<c1::MaterialVertexDeclaration>(header);
				const std::string name(donor.c_string(decl.name));
				decl.name = FOLLOWS;
				std::memset(decl.decl, 0, sizeof(decl.decl));
				b.add(decl);
				b.add_string(name);
			}
			else
			{
				c1::MaterialShader shader = donor.get<c1::MaterialShader>(header);
				const std::string name(donor.c_string(shader.name));
				const zaddr program = shader.program;
				const std::size_t bytes = static_cast<std::size_t>(shader.programSize) * 4;
				shader.name = FOLLOWS;
				std::memset(shader._unk4, 0, sizeof(shader._unk4));
				const auto code = program && bytes ? donor.bytes(program, bytes) : std::span<const std::uint8_t>();
				shader.program = code.empty() ? 0 : FOLLOWS;
				b.add(shader);
				b.add_string(name);
				if (!code.empty())
				{
					b.add(code.data(), code.size());
				}
			}
			zone_ref ref;
			ref.asset = self;
			ref.chunk = pass_chunk;
			ref.offset = field;
			pending.emplace_back(key_address, ref);
		}

		zone_ref techset_from(const Zone& donor, const zaddr header)
		{
			c1::MaterialTechniqueSet set = donor.get<c1::MaterialTechniqueSet>(header);
			const std::string name(donor.c_string(set.name));
			const auto key_name = std::make_pair(static_cast<int>(c1::ASSET_TYPE_TECHNIQUE_SET), name);
			if (const auto it = by_name.find(key_name); it != by_name.end())
			{
				return it->second;
			}
			const std::size_t self = out.asset_count();		// the index the new asset gets
			asset_builder b;
			std::vector<std::pair<std::pair<const Zone*, zaddr>, zone_ref>> pending;	// what this asset defines
			zaddr techniques[c1::MATERIAL_TECHNIQUE_COUNT];
			std::memcpy(techniques, set.techniques, sizeof(techniques));
			set.name = FOLLOWS;
			std::memset(set._unk5 + 3, 0, 4);				// remappedTechniqueSet: runtime
			const std::uint32_t head = b.add(set);
			b.add_string(name);
			for (int t = 0; t < c1::MATERIAL_TECHNIQUE_COUNT; t++)
			{
				const std::size_t field = offsetof(c1::MaterialTechniqueSet, techniques) + 4 * static_cast<std::size_t>(t);
				const zaddr technique = techniques[t];
				if (!technique)
				{
					b.put(head, field, std::uint32_t{ 0 });
					continue;
				}
				const auto key_address = std::make_pair(&donor, technique);
				if (const auto it = by_address.find(key_address); it != by_address.end())
				{
					point_ref(b, head, field, it->second);
					continue;
				}
				bool found = false;
				for (const auto& [k, ref] : pending)
				{
					if (k == key_address)
					{
						b.point(head, field, ref.chunk, ref.offset);
						found = true;
						break;
					}
				}
				if (found)
				{
					continue;
				}
				b.follows(head, field);
				std::uint8_t tech_header[TECHNIQUE_HEADER];
				const auto tech_bytes = donor.bytes(technique, TECHNIQUE_HEADER);
				if (tech_bytes.size() != TECHNIQUE_HEADER)
				{
					throw compile_error("technique set '" + name + "' of zone '" + donor.name() + "': a technique is not in the zone image");
				}
				std::memcpy(tech_header, tech_bytes.data(), TECHNIQUE_HEADER);
				std::uint32_t tech_name_address;
				std::uint16_t pass_count;
				std::memcpy(&tech_name_address, tech_header, 4);
				std::memcpy(&pass_count, tech_header + 6, 2);
				const std::string tech_name(donor.c_string(tech_name_address));
				const std::uint32_t tech_chunk = b.add(tech_header, TECHNIQUE_HEADER);
				b.follows(tech_chunk, 0);
				zone_ref tech_ref;
				tech_ref.asset = self;
				tech_ref.chunk = tech_chunk;
				pending.emplace_back(key_address, tech_ref);
				std::vector<c1::MaterialPass> passes = donor.get_array<c1::MaterialPass>(technique + TECHNIQUE_HEADER, pass_count);
				if (passes.size() != pass_count)
				{
					throw compile_error("technique '" + tech_name + "' of zone '" + donor.name() + "': its passes are not in the zone image");
				}
				if (pass_count)
				{
					const std::uint32_t pass_chunk = b.add_array(passes);
					for (std::uint32_t p = 0; p < pass_count; p++)
					{
						const c1::MaterialPass& pass = passes[p];
						const std::uint32_t base = p * sizeof(c1::MaterialPass);
						pass_asset(donor, b, pass_chunk, base + offsetof(c1::MaterialPass, vertexDecl), pass.vertexDecl, true, self, pending);
						pass_asset(donor, b, pass_chunk, base + offsetof(c1::MaterialPass, vertexShader), pass.vertexShader, false, self, pending);
						pass_asset(donor, b, pass_chunk, base + offsetof(c1::MaterialPass, pixelShader), pass.pixelShader, false, self, pending);
						const std::uint32_t arg_count = static_cast<std::uint32_t>(pass.perPrimArgCount) + pass.perObjArgCount + pass.stableArgCount;
						if (pass.args && arg_count)
						{
							std::vector<c1::MaterialShaderArgument> args = donor.get_array<c1::MaterialShaderArgument>(pass.args, arg_count);
							if (args.size() != arg_count)
							{
								throw compile_error("technique '" + tech_name + "' of zone '" + donor.name() + "': shader arguments are not in the zone image");
							}
							b.follows(pass_chunk, base + offsetof(c1::MaterialPass, args));
							const std::uint32_t arg_chunk = b.add_array(args);
							for (std::uint32_t a = 0; a < arg_count; a++)
							{
								// literal vertex ( 1 ) / pixel ( 7 ) constant: a pointer to four floats
								if (args[a].type == 1 || args[a].type == 7)
								{
									const auto literal = donor.bytes(args[a].value, 16);
									if (literal.size() != 16)
									{
										throw compile_error("technique '" + tech_name + "' of zone '" + donor.name() + "': a literal constant is not in the zone image");
									}
									b.follows(arg_chunk, a * sizeof(c1::MaterialShaderArgument) + offsetof(c1::MaterialShaderArgument, value));
									b.add(literal.data(), 16);
								}
							}
						}
						else
						{
							b.put(pass_chunk, base + offsetof(c1::MaterialPass, args), std::uint32_t{ 0 });
						}
					}
				}
				b.add_string(tech_name);
			}
			zone_ref ref;
			ref.slot = true;
			ref.asset = add(c1::ASSET_TYPE_TECHNIQUE_SET, name, b);
			if (ref.asset != self)
			{
				throw compile_error("internal: the technique set was not appended");
			}
			by_name[key_name] = ref;
			for (auto& [k, r] : pending)
			{
				by_address[k] = r;
			}
			return ref;
		}

		zone_ref material_from(const Zone& donor, const zaddr header)
		{
			zone::MaterialView view;
			if (!zone::read_material(donor, header, view) || view.isReference)
			{
				throw compile_error("a material of zone '" + donor.name() + "' cannot be read");
			}
			const std::string name(view.name);
			const auto key_name = std::make_pair(static_cast<int>(c1::ASSET_TYPE_MATERIAL), name);
			if (const auto it = by_name.find(key_name); it != by_name.end())
			{
				return it->second;
			}
			// what the material uses, in front of it
			zone_ref techset;
			const zone::AssetReference* techset_reference = view.raw.techniqueSet ? donor.reference(view.raw.techniqueSet) : nullptr;
			if (view.raw.techniqueSet && !techset_reference)
			{
				techset = techset_from(donor, view.raw.techniqueSet);
			}
			std::vector<zone_ref> images(view.textures.size());
			for (std::size_t t = 0; t < view.textures.size(); t++)
			{
				if (view.textures[t].raw.semantic == 11)
				{
					throw compile_error("material '" + name + "' uses a water map, which this compiler does not carry over");
				}
				if (view.textures[t].image && !donor.reference(view.textures[t].image))
				{
					images[t] = image_from(donor, view.textures[t].image);
				}
			}

			asset_builder b;
			c1::Material raw = view.raw;
			raw.name = FOLLOWS;
			const std::uint32_t head = b.add(raw);
			b.add_string(name);
			if (techset.valid())
			{
				point_ref(b, head, offsetof(c1::Material, techniqueSet), techset);
			}
			else if (techset_reference)
			{
				b.follows(head, offsetof(c1::Material, techniqueSet));
				reference_stub(b, sizeof(c1::MaterialTechniqueSet), 0, techset_reference->name);
			}
			else
			{
				b.put(head, offsetof(c1::Material, techniqueSet), std::uint32_t{ 0 });
			}
			if (!view.textures.empty())
			{
				b.follows(head, offsetof(c1::Material, textureTable));
				std::vector<c1::MaterialTextureDef> table;
				for (const zone::MaterialTexture& t : view.textures)
				{
					table.push_back(t.raw);
				}
				const std::uint32_t table_chunk = b.add_array(table);
				for (std::size_t t = 0; t < view.textures.size(); t++)
				{
					const std::size_t field = t * sizeof(c1::MaterialTextureDef) + offsetof(c1::MaterialTextureDef, image);
					if (images[t].valid())
					{
						point_ref(b, table_chunk, field, images[t]);
					}
					else if (view.textures[t].image)
					{
						b.follows(table_chunk, field);
						reference_stub(b, sizeof(c1::GfxImage), offsetof(c1::GfxImage, name), donor.reference(view.textures[t].image)->name);
					}
					else
					{
						b.put(table_chunk, field, std::uint32_t{ 0 });
					}
				}
			}
			else
			{
				b.put(head, offsetof(c1::Material, textureTable), std::uint32_t{ 0 });
			}
			if (!view.constants.empty())
			{
				b.follows(head, offsetof(c1::Material, constantTable));
				b.add_array(view.constants);
			}
			else
			{
				b.put(head, offsetof(c1::Material, constantTable), std::uint32_t{ 0 });
			}
			if (!view.stateBits.empty())
			{
				b.follows(head, offsetof(c1::Material, stateBitsTable));
				b.add_array(view.stateBits);
			}
			else
			{
				b.put(head, offsetof(c1::Material, stateBitsTable), std::uint32_t{ 0 });
			}
			zone_ref ref;
			ref.slot = true;
			ref.asset = add(c1::ASSET_TYPE_MATERIAL, name, b);
			by_name[key_name] = ref;
			return ref;
		}

		const Zone* open(const std::filesystem::path& file, std::string* why)
		{
			const std::string id = file.generic_string();
			if (const auto it = zones.find(id); it != zones.end())
			{
				return it->second.get();
			}
			auto z = std::make_unique<Zone>();
			if (const zone::Status st = z->open(file, key); !st)
			{
				if (why)
				{
					*why = file.filename().string() + ": " + st.message;
				}
				return nullptr;
			}
			return (zones[id] = std::move(z)).get();
		}

		// zones that may hold `name`: the donors given on the command line, then the zone the catalog names
		template <typename F>
		zone_ref find(const std::int32_t type, const std::string_view name, const std::map<std::string, std::string, std::less<>>& where,
			std::string* why, F&& take)
		{
			const auto key_name = std::make_pair(static_cast<int>(type), std::string(name));
			if (const auto it = by_name.find(key_name); it != by_name.end())
			{
				return it->second;
			}
			std::vector<std::filesystem::path> files = donors;
			if (const auto it = where.find(name); it != where.end())
			{
				files.push_back(install / it->second);
			}
			for (const std::filesystem::path& file : files)
			{
				const Zone* z = open(file, why);
				if (!z)
				{
					continue;
				}
				if (const zaddr header = z->find_asset(type, name))
				{
					return take(*z, header);
				}
			}
			if (why && why->empty())
			{
				*why = "no zone of the install has " + zone::asset_type_name(type) + " '" + std::string(name) + "'";
			}
			return {};
		}
	};

	asset_importer::asset_importer(ZoneEditor& out, const zone::ClientKey& key, std::filesystem::path install,
		std::vector<std::filesystem::path> donors, const catalog& cat)
		: s(std::make_unique<state>(out, key, std::move(install), std::move(donors), cat))
	{
	}

	asset_importer::~asset_importer() = default;

	zone_ref asset_importer::material(const std::string_view name, std::string* why)
	{
		return s->find(c1::ASSET_TYPE_MATERIAL, name, s->cat.material_zone, why, [&](const Zone& z, const zaddr h) { return s->material_from(z, h); });
	}

	const material_desc& asset_importer::describe(const std::string_view name)
	{
		if (const auto it = s->described.find(name); it != s->described.end())
		{
			return it->second;
		}
		material_desc d;
		const Zone* from = nullptr;
		zaddr header = 0;
		d.ref = s->find(c1::ASSET_TYPE_MATERIAL, name, s->cat.material_zone, &d.why, [&](const Zone& z, const zaddr h)
		{
			from = &z;
			header = h;
			return s->material_from(z, h);
		});
		if (d.ref.valid() && !from)
		{
			// imported before describe() was asked: find it again for its properties
			std::vector<std::filesystem::path> files = s->donors;
			if (const auto it = s->cat.material_zone.find(name); it != s->cat.material_zone.end())
			{
				files.push_back(s->install / it->second);
			}
			for (const auto& file : files)
			{
				if (const Zone* z = s->open(file, nullptr))
				{
					if ((header = z->find_asset(c1::ASSET_TYPE_MATERIAL, name)) != 0)
					{
						from = z;
						break;
					}
				}
			}
		}
		zone::MaterialView view;
		if (from && zone::read_material(*from, header, view))
		{
			d.sort_key = view.raw.sortKey;
			d.techset = std::string(view.techsetName);
			d.sky = d.techset.find("sky") != std::string::npos;
			bool sized = false;
			for (const zone::MaterialTexture& t : view.textures)
			{
				if (!t.image)
				{
					continue;
				}
				const bool reference = from->reference(t.image) != nullptr;
				c1::GfxImage image{};
				if (!reference)
				{
					image = from->get<c1::GfxImage>(t.image);
				}
				if (!reference && image.mapType == 5 && d.sky)
				{
					d.sky_image = s->image_from(*from, t.image);
					d.sky_image_name = std::string(t.imageName);
				}
				// semantic 2: the colour map
				if (!reference && image.width && image.height && (!sized || t.raw.semantic == 2) && image.mapType == 3)
				{
					d.width = image.width;
					d.height = image.height;
					sized = true;
				}
			}
		}
		return s->described.emplace(std::string(name), std::move(d)).first->second;
	}

	zone_ref asset_importer::image(const std::string_view name, std::string* why)
	{
		return s->find(c1::ASSET_TYPE_IMAGE, name, s->cat.image_zone, why, [&](const Zone& z, const zaddr h) { return s->image_from(z, h); });
	}

	zone_ref asset_importer::model(const std::string_view name, std::string* why)
	{
		if (why)
		{
			*why = "xmodel '" + std::string(name) + "': static models are not compiled yet";
		}
		return {};
	}

	zone_ref asset_importer::material_from(const Zone& donor, const zaddr header) { return s->material_from(donor, header); }
	zone_ref asset_importer::image_from(const Zone& donor, const zaddr header) { return s->image_from(donor, header); }

	const Zone* asset_importer::open_zone(const std::string& relative, std::string* why)
	{
		return s->open(s->install / relative, why);
	}

	const ZoneEditor* asset_importer::open_editor(const std::string&, std::string*)
	{
		return nullptr;
	}
}
