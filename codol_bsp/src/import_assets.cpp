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

		// An xmodel that is nested in another asset of its zone ( a static model inside a shipped map's gfx_map ),
		// written again from the loaded image in the order Load_XModel reads it: name, bone names, hierarchy, materials,
		// the surfaces of every LOD ( with the collision trees of the rigid lists ), collision surfaces, bone boxes.
		// Materials go through material_from. Physics preset and collision map are dropped: a static model does not
		// move.
		zone_ref model_from(const Zone& donor, const zaddr header)
		{
			const c1::XModel in = donor.get<c1::XModel>(header);
			const std::string name(donor.asset_name(c1::ASSET_TYPE_XMODEL, header));
			const auto key_name = std::make_pair(static_cast<int>(c1::ASSET_TYPE_XMODEL), name);
			if (const auto it = by_name.find(key_name); it != by_name.end())
			{
				return it->second;
			}
			const std::size_t bones = in.numBones, children = static_cast<std::size_t>(in.numBones - in.numRootBones);
			const std::vector<zaddr> handles = donor.get_array<zaddr>(in.materialHandles, in.numsurfs);
			std::vector<zone_ref> materials(handles.size());
			for (std::size_t m = 0; m < handles.size(); m++)
			{
				if (handles[m] && !donor.reference(handles[m]))
				{
					materials[m] = material_from(donor, handles[m]);
				}
			}

			asset_builder b;
			c1::XModel raw = in;
			raw.name = FOLLOWS;
			raw.boneNames = bones ? FOLLOWS : 0;
			raw.parentList = raw.quats = raw.trans = children ? FOLLOWS : 0;
			raw.partClassification = bones && in.partClassification ? FOLLOWS : 0;
			raw.baseMat = bones && in.baseMat ? FOLLOWS : 0;
			raw.materialHandles = handles.empty() ? 0 : FOLLOWS;
			raw.collSurfs = in.numCollSurfs > 0 && in.collSurfs ? FOLLOWS : 0;
			raw.boneInfo = bones && in.boneInfo ? FOLLOWS : 0;
			raw.physPreset = raw.physCollmap = 0;
			for (c1::XModelLodInfo& lod : raw.lodInfo)
			{
				lod.surfs = 0;
				lod.surfsName = 0;
				lod.modelSurfs = lod.modelSurfs ? c1::ZPTR_INLINE_INSERT : 0;
			}
			const std::uint32_t head = b.add(raw);
			b.add_string(name);
			if (bones)
			{
				std::vector<std::uint16_t> names = donor.get_array<std::uint16_t>(in.boneNames, bones);
				for (std::uint16_t& n : names)
				{
					n = out.script_string(donor.script_string(n));
				}
				const std::uint32_t chunk = b.add_array(names);
				for (std::size_t k = 0; k < bones; k++)
				{
					b.chunks[chunk].scriptStrings.push_back(static_cast<std::uint32_t>(2 * k));
				}
			}
			if (children)
			{
				b.add_array(donor.get_array<std::uint8_t>(in.parentList, children));
				b.add_array(donor.get_array<std::int16_t>(in.quats, 4 * children));
				b.add_array(donor.get_array<float>(in.trans, 3 * children));
			}
			if (raw.partClassification)
			{
				b.add_array(donor.get_array<std::uint8_t>(in.partClassification, bones));
			}
			if (raw.baseMat)
			{
				b.add_array(donor.get_array<float>(in.baseMat, 8 * bones));
			}
			if (!handles.empty())
			{
				const std::uint32_t chunk = b.add_zero(4 * handles.size());
				for (std::size_t m = 0; m < handles.size(); m++)
				{
					if (materials[m].valid())
					{
						point_ref(b, chunk, 4 * m, materials[m]);
					}
					else if (handles[m])
					{
						b.follows(chunk, 4 * m);
						reference_stub(b, sizeof(c1::Material), 0, donor.reference(handles[m])->name);
					}
				}
			}
			for (int l = 0; l < 4; l++)
			{
				const c1::XModelLodInfo& lod = in.lodInfo[l];
				if (!lod.modelSurfs)
				{
					continue;
				}
				if (donor.reference(lod.modelSurfs))
				{
					throw compile_error("xmodel '" + name + "' of zone '" + donor.name() + "': the surfaces of LOD " + std::to_string(l) + " are in another zone");
				}
				c1::XModelSurfs surfs = donor.get<c1::XModelSurfs>(lod.modelSurfs);
				const std::vector<c1::XSurface> list = donor.get_array<c1::XSurface>(surfs.surfs, surfs.numsurfs);
				const std::string surfs_name(donor.asset_name(c1::ASSET_TYPE_XMODEL_SURFS, lod.modelSurfs));
				surfs.name = FOLLOWS;
				surfs.surfs = list.empty() ? 0 : FOLLOWS;
				b.add(surfs);
				const std::uint32_t name_chunk = b.add_string(surfs_name);
				b.point(head, offsetof(c1::XModel, lodInfo) + static_cast<std::size_t>(l) * sizeof(c1::XModelLodInfo) + offsetof(c1::XModelLodInfo, surfsName), name_chunk);
				if (list.empty())
				{
					continue;
				}
				std::vector<c1::XSurface> headers = list;
				for (c1::XSurface& h : headers)
				{
					const int weighted = h.vertInfoCount[0] + 3 * h.vertInfoCount[1] + 5 * h.vertInfoCount[2] + 7 * h.vertInfoCount[3];
					h.triIndices = h.triCount ? FOLLOWS : 0;
					h.vertsBlend = h.vertsBlend && weighted ? FOLLOWS : 0;
					h.verts0 = h.vertCount ? FOLLOWS : 0;
					h.vertList = h.vertList && h.vertListCount ? FOLLOWS : 0;
				}
				b.add_array(headers);
				for (std::size_t k = 0; k < list.size(); k++)
				{
					const c1::XSurface& from = list[k];
					const c1::XSurface& h = headers[k];
					if (h.vertsBlend)
					{
						const std::size_t count = static_cast<std::size_t>(from.vertInfoCount[0] + 3 * from.vertInfoCount[1] + 5 * from.vertInfoCount[2] + 7 * from.vertInfoCount[3]);
						b.add_array(donor.get_array<std::uint16_t>(from.vertsBlend, count));
					}
					if (h.verts0)
					{
						b.add_array(donor.get_array<c1::GfxPackedVertex>(from.verts0, from.vertCount));
					}
					if (h.vertList)
					{
						// XSurfaceCollisionTree: translation, scale, node count, nodes ( 16 bytes ), leaf count, leafs ( u16 )
						struct tree_header
						{
							float trans[3], scale[3];
							std::uint32_t nodeCount, nodes, leafCount, leafs;
						};
						static_assert(sizeof(tree_header) == 40);
						const std::vector<c1::XRigidVertList> lists = donor.get_array<c1::XRigidVertList>(from.vertList, from.vertListCount);
						std::vector<c1::XRigidVertList> packed = lists;
						for (c1::XRigidVertList& r : packed)
						{
							r.collisionTree = r.collisionTree ? FOLLOWS : 0;
						}
						b.add_array(packed);
						for (const c1::XRigidVertList& r : lists)
						{
							if (!r.collisionTree)
							{
								continue;
							}
							tree_header tree = donor.get<tree_header>(r.collisionTree);
							const zaddr nodes = tree.nodes, leafs = tree.leafs;
							tree.nodes = tree.nodeCount && nodes ? FOLLOWS : 0;
							tree.leafs = tree.leafCount && leafs ? FOLLOWS : 0;
							b.add(tree);
							if (tree.nodes)
							{
								b.add_array(donor.get_array<std::uint8_t>(nodes, 16u * tree.nodeCount));
							}
							if (tree.leafs)
							{
								b.add_array(donor.get_array<std::uint16_t>(leafs, tree.leafCount));
							}
						}
					}
					if (h.triIndices)
					{
						b.add_array(donor.get_array<std::uint16_t>(from.triIndices, 3u * from.triCount));
					}
				}
			}
			if (raw.collSurfs)
			{
				struct coll_surf
				{
					std::uint32_t tris;
					std::int32_t count;
					c1::Bounds bounds;
					std::int32_t bone, contents, surfaceFlags;
				};
				static_assert(sizeof(coll_surf) == 44);
				const std::vector<coll_surf> table = donor.get_array<coll_surf>(in.collSurfs, static_cast<std::size_t>(in.numCollSurfs));
				std::vector<coll_surf> packed = table;
				for (coll_surf& c : packed)
				{
					c.tris = c.count > 0 && c.tris ? FOLLOWS : 0;
				}
				b.add_array(packed);
				for (std::size_t c = 0; c < table.size(); c++)
				{
					if (packed[c].tris)
					{
						b.add_array(donor.get_array<float>(table[c].tris, 12u * static_cast<std::size_t>(table[c].count)));
					}
				}
			}
			if (raw.boneInfo)
			{
				b.add_array(donor.get_array<float>(in.boneInfo, 7 * bones));
			}
			zone_ref ref;
			ref.slot = true;
			ref.asset = add(c1::ASSET_TYPE_XMODEL, name, b);
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
			d.surface_type_bits = view.raw.surfaceTypeBits;
			{
				// wc_l_sm_<r|b|t>0c0..: replace, blend, alpha test; wc_unlit_<replace|blend|add|multiply|screen>_lin
				const std::size_t lit = d.techset.find("_sm_");
				const char mode = lit != std::string::npos && lit + 4 < d.techset.size() ? d.techset[lit + 4] : 0;
				d.alpha_test = mode == 't';
				d.blend = mode == 'b' || (d.techset.find("unlit_") != std::string::npos && d.techset.find("unlit_replace") == std::string::npos);
				d.see_through = d.alpha_test || (d.blend && d.sort_key >= 6 && d.sort_key <= 13);
			}
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

	namespace
	{
		// what the compiler wants to know about a model: box, collision, draw distance, the triangles of LOD 0
		void describe_model(const Zone& z, const zaddr header, model_desc& desc)
		{
			const c1::XModel raw = z.get<c1::XModel>(header);
			desc.coll_surfs = raw.numCollSurfs;
			// XModel + 316: radius, then the box as centre and half size ( codol_mapc XMODEL_RADIUS / XMODEL_BOUNDS )
			desc.radius = z.get<float>(header + 316);
			const c1::Bounds box = z.get<c1::Bounds>(header + 320);
			std::memcpy(desc.mid, box.midPoint, sizeof(desc.mid));
			std::memcpy(desc.half, box.halfSize, sizeof(desc.half));
			desc.last_lod_dist = raw.numLods ? raw.lodInfo[raw.numLods - 1].dist : 0.0f;
			zone::ModelView view;
			if (zone::read_model(z, header, view, true) && !view.lods.empty())
			{
				for (const zone::SurfaceView& surface : view.lods[0].surfs.surfaces)
				{
					const std::vector<zone::SurfaceVertex> vertices = surface.vertices();
					const std::vector<std::uint16_t> indices = surface.indices();
					for (std::size_t k = 0; k + 2 < indices.size(); k += 3)
					{
						if (indices[k] == indices[k + 1] || indices[k + 1] == indices[k + 2] || indices[k] >= vertices.size() || indices[k + 1] >= vertices.size()
							|| indices[k + 2] >= vertices.size())
						{
							continue;
						}
						for (int c = 0; c < 3; c++)
						{
							const zone::SurfaceVertex& v = vertices[indices[k + static_cast<std::size_t>(c)]];
							desc.triangles.insert(desc.triangles.end(), { v.position[0], v.position[1], v.position[2] });
						}
					}
				}
			}
		}
	}

	zone_ref asset_importer::model(const std::string_view name, std::string* why, model_desc* desc)
	{
		// An xmodel that is a top level asset of its zone - a model of the common zones, of a ported map, one compiled by
		// codol_assets - is copied as the donor zone has it ( ZoneEditor::add_asset_closure: the model and every asset it
		// points into ). One that only exists nested in a shipped map's gfx_map is written again ( model_from ).
		const auto key_name = std::make_pair(static_cast<int>(c1::ASSET_TYPE_XMODEL), std::string(name));
		std::vector<std::filesystem::path> files = s->donors;
		if (const auto it = s->cat.model_zone.find(name); it != s->cat.model_zone.end())
		{
			files.push_back(s->install / it->second);
		}
		for (const std::filesystem::path& file : files)
		{
			const std::string id = file.generic_string();
			auto it = s->editors.find(id);
			if (it == s->editors.end())
			{
				auto editor = std::make_unique<ZoneEditor>();
				if (const zone::Status st = editor->open(file, s->key); !st)
				{
					if (why)
					{
						*why = file.filename().string() + ": " + st.message;
					}
					continue;
				}
				it = s->editors.emplace(id, std::move(editor)).first;
			}
			const ZoneEditor& donor = *it->second;
			const std::size_t index = donor.find_asset(c1::ASSET_TYPE_XMODEL, name);
			const zaddr header = donor.zone().find_asset(c1::ASSET_TYPE_XMODEL, name);
			if (index == ZoneEditor::npos || !header)
			{
				continue;
			}
			if (desc)
			{
				describe_model(donor.zone(), header, *desc);
			}
			if (const auto known = s->by_name.find(key_name); known != s->by_name.end())
			{
				return known->second;
			}
			if (const zone::Status st = s->out.add_asset_closure(donor, index); !st)
			{
				if (why)
				{
					*why = "xmodel '" + std::string(name) + "' of " + file.filename().string() + ": " + st.message;
				}
				return {};
			}
			zone_ref ref;
			ref.slot = true;
			ref.asset = s->out.find_asset(c1::ASSET_TYPE_XMODEL, name);
			if (ref.asset == ZoneEditor::npos)
			{
				return {};
			}
			s->by_name[key_name] = ref;
			return ref;
		}
		// nested in another asset ( a static model of a shipped map ): written again from the loaded zone
		std::vector<std::filesystem::path> nested = s->donors;
		if (const auto it = s->cat.nested_model_zone.find(name); it != s->cat.nested_model_zone.end())
		{
			nested.push_back(s->install / it->second);
		}
		for (const std::filesystem::path& file : nested)
		{
			const Zone* z = s->open(file, why);
			const zaddr header = z ? z->find_asset(c1::ASSET_TYPE_XMODEL, name) : 0;
			if (!header || z->reference(header))
			{
				continue;
			}
			if (desc)
			{
				describe_model(*z, header, *desc);
				desc->nested = true;
			}
			return s->model_from(*z, header);
		}
		if (why && why->empty())
		{
			*why = "no zone has xmodel '" + std::string(name) + "'";
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
