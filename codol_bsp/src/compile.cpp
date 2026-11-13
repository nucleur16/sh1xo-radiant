// The compile: document -> world -> the map assets -> zone -> fast file.

#include "world.hpp"
#include "codol_render/iwd.hpp"
#include "codol_render/iwi.hpp"
#include "gfx_layout.hpp"

#include <fstream>
#include <sstream>

namespace codol::bsp
{
	std::string report::text() const
	{
		std::string out;
		out += format("map %s: %zu brushes ( %zu structural, %zu detail ), %zu patches, %zu entities, %zu lights, %zu static models\n", map_name.c_str(), brushes,
			structural_brushes, detail_brushes, patches, entities, lights, static_models);
		out += format("collision: %zu brushes, %zu sides, %zu planes, %zu materials, %zu nodes, %zu leafs, %zu leaf brush nodes, %zu sub models\n", clip_brushes, clip_sides,
			clip_planes, clip_materials, clip_nodes, clip_leafs, leaf_brush_nodes, sub_models);
		out += format("render: %zu surfaces, %zu vertices, %zu triangles, %zu materials, %zu cells, %zu portals, %zu faces removed\n", surfaces, vertices, triangles,
			world_materials, cells, portals, faces_culled);
		out += format("light: %zu lightmaps, %zu light grid points, %zu primary lights, %zu reflection probes\n", lightmaps, light_grid_points, primary_lights,
			reflection_probes);
		if (leaked)
		{
			out += format("LEAK: entity %s reaches the outside of the map; %zu points:\n", leak_entity.c_str(), leak_path.size());
			for (const leak_point& p : leak_path)
			{
				out += format("  %.1f %.1f %.1f\n", p.xyz[0], p.xyz[1], p.xyz[2]);
			}
		}
		for (const std::string& n : notes)
		{
			out += "  " + n + "\n";
		}
		for (const std::string& n : warnings)
		{
			out += "  warning: " + n + "\n";
		}
		for (const auto& [stage, took] : stage_seconds)
		{
			out += format("  %-12s %.2f s\n", stage.c_str(), took);
		}
		out += format("zone: %zu assets, %zu bytes; file %zu bytes; %.2f s\n", assets, zone_bytes, file_bytes, seconds);
		return out;
	}

	void resolve_refs(asset_builder& b, const world& w)
	{
		for (ZoneBuildChunk& chunk : b.chunks)
		{
			for (ZoneBuildPointer& p : chunk.pointers)
			{
				if (p.asset == REF_COM)
				{
					p.asset = w.com_asset;
					p.chunk = w.name_chunk;
				}
				else if (p.asset == REF_GFX)
				{
					if (w.gfx_asset == npos)
					{
						throw compile_error("internal: a pointer into the GfxWorld before it exists");
					}
					p.asset = w.gfx_asset;
					p.chunk = w.gfx_planes_chunk;
				}
			}
		}
	}

	void build_com_world(compile_context& ctx)
	{
		world& w = ctx.w;
		asset_builder b;
		c1::ComWorld header{};
		header.name = FOLLOWS;
		header.isInUse = 1;
		header.primaryLightCount = static_cast<std::uint32_t>(w.lights.size());
		header.primaryLights = FOLLOWS;
		b.add(header);
		w.name_chunk = b.add_string(w.bsp_name);
		std::vector<gfx::ComPrimaryLight> lights(w.lights.size());
		for (std::size_t l = 0; l < lights.size(); l++)
		{
			const map_light& in = w.lights[l];
			gfx::ComPrimaryLight& out = lights[l];
			out.type = static_cast<std::uint8_t>(in.type);
			if (in.type == 0)
			{
				continue;
			}
			std::memcpy(out.color, in.color, sizeof(out.color));
			std::memcpy(out.dir, in.dir, sizeof(out.dir));
			if (in.type == 1)
			{
				continue;		// the sun: colour and direction only, as in the shipped maps
			}
			out.canUseShadowMap = in.can_shadow ? 1 : 0;
			out.exponent = static_cast<std::uint8_t>(in.exponent);
			std::memcpy(out.origin, in.origin, sizeof(out.origin));
			out.radius = in.radius;
			out.cosHalfFovOuter = in.cos_half_fov_outer;
			out.cosHalfFovInner = in.cos_half_fov_inner;
			// the cone the beam stays in however a script turns the light: the outer cone widened by "maxturn". Every
			// light of the shipped maps that may not turn has the outer cone here; the four of zm_asylum_sh with
			// maxturn 360 have -1. The client tests what a light's shadow map holds against this cone.
			out.cosHalfFovExpanded = in.scriptable && in.rotation_limit < 0.9999f
				? static_cast<float>(std::cos(std::min(3.14159265358979, std::acos(std::clamp(static_cast<double>(in.cos_half_fov_outer), -1.0, 1.0))
					+ std::acos(std::clamp(static_cast<double>(in.rotation_limit), -1.0, 1.0)))))
				: in.cos_half_fov_outer;
			out.rotationLimit = in.rotation_limit;
			out.translationLimit = in.translation_limit;
		}
		const std::uint32_t array = b.add_array(lights);
		std::map<std::string, std::uint32_t> def_chunk;
		for (std::size_t l = 0; l < lights.size(); l++)
		{
			if (w.lights[l].type < 2)
			{
				continue;
			}
			const std::size_t field = l * sizeof(gfx::ComPrimaryLight) + offsetof(gfx::ComPrimaryLight, defName);
			if (const auto it = def_chunk.find(w.lights[l].def); it != def_chunk.end())
			{
				b.point(array, field, it->second);
			}
			else
			{
				b.follows(array, field);
				def_chunk[w.lights[l].def] = b.add_string(w.lights[l].def);
			}
		}
		check(ctx.editor.add_built_asset(c1::ASSET_TYPE_COMWORLD, w.bsp_name, b.chunks, npos, &w.com_asset), "adding the ComWorld");
	}

	void build_fx_world(compile_context& ctx)
	{
		// no glass: everything zero but the free list head ( FxGlassSystem.firstFreePiece = 0xFFFF )
		asset_builder b;
		const std::uint32_t head = b.add_zero(c1::XASSET_HEADER_SIZE[c1::ASSET_TYPE_FXWORLD]);
		b.point(head, 0, 0, 0, REF_COM);
		b.put(head, 0x24, std::uint32_t{ 0xFFFF });
		resolve_refs(b, ctx.w);
		check(ctx.editor.add_built_asset(c1::ASSET_TYPE_FXWORLD, ctx.w.bsp_name, b.chunks), "adding the FxWorld");
	}

	namespace
	{
		// The falloff picture of a light definition ( images/falloff_linear.iwi .. in the install's image archives ): one
		// row of brightness from the light's origin to its radius. The game's map compiler lays it into row 0 of the
		// secondary lightmaps, where the shaders of the primary lights read it; so does this one, and the bake uses
		// the same curve for the lights it bakes. Kept in a small file next to the catalog ( the archives take a
		// moment to open ). false: not read, `why` says more.
		bool load_falloff(compile_context& ctx, const std::string& image_name, std::array<std::array<float, 3>, 32>& curve, std::string& source, std::string& why)
		{
			std::filesystem::path folder = ctx.opts.cache;
			if (folder.empty())
			{
				folder = ctx.opts.out.empty() ? std::filesystem::current_path() : std::filesystem::absolute(ctx.opts.out).parent_path();
			}
			const std::filesystem::path cache = folder / "codol_bsp_falloff.txt";
			const std::string tag = "falloff pictures 2 " + ctx.opts.install.generic_string();
			std::map<std::string, std::array<std::array<float, 3>, 32>> known;
			{
				std::ifstream in(cache);
				std::string line;
				if (in && std::getline(in, line) && line == tag)
				{
					while (std::getline(in, line))
					{
						std::istringstream text(line);
						std::string name;
						std::array<std::array<float, 3>, 32> values{};
						bool ok = static_cast<bool>(text >> name);
						for (std::size_t k = 0; ok && k < 96; k++)
						{
							ok = static_cast<bool>(text >> values[k / 3][k % 3]);
						}
						if (ok)
						{
							known[name] = values;
						}
					}
				}
			}
			if (const auto it = known.find(image_name); it != known.end())
			{
				curve = it->second;
				source = "images/" + image_name + ".iwi ( " + cache.filename().string() + " )";
				return true;
			}
			render::iwd_key key{};
			std::string error;
			std::vector<std::uint8_t> file;
			if (!render::derive_iwd_key(ctx.opts.exe, key, &error))
			{
				why = error;
				return false;
			}
			std::vector<std::filesystem::path> directories{ ctx.opts.install / "main" };
			std::error_code ec;
			if (std::filesystem::is_directory(ctx.opts.install / "main_shared", ec))
			{
				directories.push_back(ctx.opts.install / "main_shared");
			}
			const std::unique_ptr<render::iwd_archive_set> set = render::iwd_archive_set::open(directories, &key, "images/falloff_");
			render::iwi_file image;
			if (!set || !set->read("images/" + image_name + ".iwi", file, &error) || !render::parse_iwi(file, image, &error))
			{
				why = error;
				return false;
			}
			const render::iwi_surface* surface = image.surface(0, 0);
			const int width = image.header.width;
			// bytes per texel, and where red, green and blue are ( the 24 and 32 bit formats store b g r ( a ) )
			std::size_t bytes = 0;
			int channel[3] = { 0, 0, 0 };
			if (image.header.format == render::iwi_format::luminance8 || image.header.format == render::iwi_format::alpha8)
			{
				bytes = 1;
			}
			else if (image.header.format == render::iwi_format::rgba32 || image.header.format == render::iwi_format::rgb24)
			{
				bytes = image.header.format == render::iwi_format::rgba32 ? 4 : 3;
				channel[0] = 2;
				channel[1] = 1;
				channel[2] = 0;
			}
			if (!surface || !bytes || width < 1 || surface->size < bytes * static_cast<std::size_t>(width))
			{
				why = std::string("its format ( ") + render::iwi_format_name(image.header.format) + " ) is not one this compiler reads";
				return false;
			}
			// the first row, brought to 32 texels ( the mean of what a texel covers )
			for (int k = 0; k < 32; k++)
			{
				const int first = k * width / 32, last = std::max(first + 1, (k + 1) * width / 32);
				for (int c = 0; c < 3; c++)
				{
					double sum = 0;
					for (int x = first; x < last; x++)
					{
						sum += file[surface->offset + static_cast<std::size_t>(std::min(x, width - 1)) * bytes + static_cast<std::size_t>(channel[c])];
					}
					curve[static_cast<std::size_t>(k)][static_cast<std::size_t>(c)] = static_cast<float>(sum / (last - first) / 255.0);
				}
			}
			source = format("images/%s.iwi ( %d x %d, %s )", image_name.c_str(), width, image.header.height, render::iwi_format_name(image.header.format));
			known[image_name] = curve;
			std::ofstream out(cache, std::ios::trunc);
			out << tag << "\n";
			for (const auto& [name, values] : known)
			{
				out << name;
				for (const std::array<float, 3>& v : values)
				{
					out << " " << v[0] << " " << v[1] << " " << v[2];
				}
				out << "\n";
			}
			return true;
		}
	}

	// The light definitions the lights name. A GfxLightDef is a name, an attenuation image, a sampler state and where
	// its lookup starts in row 0 of the secondary lightmaps. The shipped maps name eleven of them ( light_point_linear,
	// light_point_quadratic, light_point_dark_edges, white_light .. ): the catalog knows which zone of the install has
	// which, the definition is read from there ( the name of its picture, the sampler state ) and written again; a
	// picture that is not an asset of the common zones comes along as an image asset ( its pixels are streamed ). The
	// lookup strips are handed out in the order of first use: 1, 35, 69 .. ( 34 texels each, as in the shipped maps ).
	void build_light_defs(compile_context& ctx)
	{
		world& w = ctx.w;
		w.light_defs.clear();
		struct found
		{
			const Zone* zone = nullptr;
			zaddr header = 0;
		};
		std::vector<found> where;
		std::set<std::string> unknown;
		const bool read_pictures = ctx.opts.light && w.lights.size() + w.bake_lights.size() > 2;
		const auto def_index = [&](const std::string& name) -> int
		{
			for (std::size_t k = 0; k < w.light_defs.size(); k++)
			{
				if (w.light_defs[k].name == name)
				{
					return static_cast<int>(k);
				}
			}
			light_def def;
			def.name = name;
			def.curve = straight_falloff();
			def.source = "a straight line ( the game's picture was not read )";
			found at;
			if (const auto it = ctx.cat.lightdef_zone.find(name); it != ctx.cat.lightdef_zone.end())
			{
				std::string why;
				at.zone = ctx.importer.open_zone(it->second, &why);
				at.header = at.zone ? at.zone->find_asset(c1::ASSET_TYPE_LIGHT_DEF, name) : 0;
				if (at.header)
				{
					def.zone = it->second;
					const zaddr image = at.zone->get<zaddr>(at.header + 4);
					if (const auto* reference = at.zone->reference(image))
					{
						def.image = std::string(reference->name);
					}
					else if (image)
					{
						def.image = std::string(at.zone->asset_name(c1::ASSET_TYPE_IMAGE, image));
					}
					if (!def.image.empty() && def.image[0] == ',')
					{
						def.image.erase(0, 1);
					}
				}
			}
			if (!at.header && name != "light_point_linear")
			{
				return -1;
			}
			if (read_pictures)
			{
				std::string why;
				if (!load_falloff(ctx, def.image, def.curve, def.source, why))
				{
					ctx.rep.warnings.push_back("light: the falloff picture images/" + def.image + ".iwi of '" + name + "' was not read ( " + why
						+ " ): its lights fall off along a straight line");
				}
			}
			w.light_defs.push_back(std::move(def));
			where.push_back(at);
			return static_cast<int>(w.light_defs.size() - 1);
		};
		def_index("light_point_linear");
		std::vector<int> used;		// by primary lights, in the order of first use
		for (int pass = 0; pass < 2; pass++)
		{
			for (map_light& light : pass == 0 ? w.lights : w.bake_lights)
			{
				if (light.type < 2)
				{
					continue;
				}
				int index = def_index(light.def);
				if (index >= 0 && pass == 0 && std::find(used.begin(), used.end(), index) == used.end() && used.size() >= 15)
				{
					ctx.rep.warnings.push_back("light: more than 15 light definitions in one map; '" + light.def + "' is replaced by light_point_linear");
					unknown.insert(light.def);
					index = -1;
				}
				if (index < 0)
				{
					if (unknown.insert(light.def).second)
					{
						ctx.rep.warnings.push_back("light: definition '" + light.def + "' is in no zone of the install, light_point_linear is used");
					}
					light.def = "light_point_linear";
					index = 0;
				}
				light.def_index = index;
				if (pass == 0 && std::find(used.begin(), used.end(), index) == used.end())
				{
					used.push_back(index);
				}
			}
		}
		if (read_pictures)
		{
			std::string text;
			for (const light_def& def : w.light_defs)
			{
				text += (text.empty() ? "" : "; ") + def.name + ": " + def.source;
			}
			ctx.rep.notes.push_back("light: falloff of the light entities: " + text);
		}
		for (std::size_t k = 0; k < used.size(); k++)
		{
			light_def& def = w.light_defs[static_cast<std::size_t>(used[k])];
			const found& at = where[static_cast<std::size_t>(used[k])];
			def.lookup_start = 1 + 34 * static_cast<int>(k);
			// the picture: an asset of a zone that is loaded ( named ",<picture>" ), or an image of the definition's zone
			zone_ref image_ref;
			std::uint32_t sampler = 0x62;
			if (at.header)
			{
				sampler = at.zone->get<std::uint8_t>(at.header + 8);
				const zaddr image = at.zone->get<zaddr>(at.header + 4);
				if (image && !at.zone->reference(image))
				{
					image_ref = ctx.importer.image_from(*at.zone, image);
				}
			}
			asset_builder b;
			const std::uint32_t header[4] = { FOLLOWS, image_ref.valid() ? 0u : FOLLOWS, sampler, static_cast<std::uint32_t>(def.lookup_start) };
			const std::uint32_t head = b.add(header, sizeof(header));
			b.add_string(def.name);
			if (image_ref.valid())
			{
				point_ref(b, head, 4, image_ref);
			}
			else
			{
				const std::uint32_t image = b.add_zero(sizeof(c1::GfxImage));
				b.follows(image, offsetof(c1::GfxImage, name));
				b.add_string("," + def.image);
			}
			check(ctx.editor.add_built_asset(c1::ASSET_TYPE_LIGHT_DEF, def.name, b.chunks), "adding light definition " + def.name);
		}
	}

	void build_game_world(compile_context& ctx)
	{
		// The loader clears the path data ( +4 .. +0x47 ) and reads nothing for it; +0x48 vehicle track segments: none;
		// +0x50 G_GlassData, which the server dereferences without a check: present, without pieces.
		asset_builder b;
		const std::uint32_t head = b.add_zero(c1::XASSET_HEADER_SIZE[c1::ASSET_TYPE_GAMEWORLD_MP]);
		b.point(head, 0, 0, 0, REF_COM);
		b.follows(head, 0x50);
		b.add_zero(0x80);
		resolve_refs(b, ctx.w);
		check(ctx.editor.add_built_asset(c1::ASSET_TYPE_GAMEWORLD_MP, ctx.w.bsp_name, b.chunks), "adding the GameWorldMp");
	}

	namespace
	{
		std::vector<std::uint8_t> read_file(const std::filesystem::path& file)
		{
			std::ifstream in(file, std::ios::binary);
			if (!in)
			{
				throw compile_error("cannot read " + file.string());
			}
			return { std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>() };
		}

		void load_catalog(const options& opts, const zone::ClientKey& key, catalog& cat, report& rep)
		{
			std::filesystem::path folder = opts.cache;
			if (folder.empty())
			{
				folder = opts.out.empty() ? std::filesystem::current_path() : std::filesystem::absolute(opts.out).parent_path();
			}
			const std::filesystem::path file = folder / "codol_bsp_catalog.txt";
			if (cat.load(file, opts.install))
			{
				return;
			}
			const stopwatch watch;
			cat.scan(opts.install, key, rep.warnings);
			if (cat.material_zone.empty())
			{
				throw compile_error("no material was found in the zones of " + opts.install.string() + " ( --install must name the game folder )");
			}
			cat.save(file, opts.install);
			rep.notes.push_back(format("install catalog built in %.1f s: %zu materials, %zu xmodels, %zu clip materials ( %s )", watch.seconds(), cat.material_zone.size(),
				cat.model_zone.size(), cat.clip_materials.size(), file.string().c_str()));
		}
	}

	void write_tga(const std::filesystem::path& file, const int width, const int height, const std::vector<std::uint8_t>& bgra)
	{
		std::error_code ec;
		std::filesystem::create_directories(std::filesystem::absolute(file).parent_path(), ec);
		std::ofstream out(file, std::ios::binary | std::ios::trunc);
		const std::uint8_t header[18] = { 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, static_cast<std::uint8_t>(width & 0xFF), static_cast<std::uint8_t>(width >> 8),
			static_cast<std::uint8_t>(height & 0xFF), static_cast<std::uint8_t>(height >> 8), 32, 0x28 };
		out.write(reinterpret_cast<const char*>(header), sizeof(header));
		out.write(reinterpret_cast<const char*>(bgra.data()), static_cast<std::streamsize>(bgra.size()));
		if (!out)
		{
			throw compile_error("cannot write " + file.string());
		}
	}

	namespace
	{
		// The scripts a map brings: the effect list ( maps/mp/<map>_fx.gsc: loadfx per effect ), the placement
		// ( maps/createfx/<map>_fx.gsc ) and the level script.
		//
		// The two effect files are not only scripts: THE CLIENT READS THEIR TEXT when a level starts and makes the
		// effects and looping sounds itself. It wants them as the game's createfx tool writes them - the placement has
		// to begin with the line "//_createfx generated. Do not touch!!" ( anything else: "Unexpected text .. when
		// trying to find .. in map's effect file", and the client places nothing ). So both files have the shape of
		// the shipped ones ( mp_vacant, mp_rust, mp_nuked ), line for line, and like there the server only runs the
		// placement in a developer build that asks for server side effects - otherwise every effect would be there
		// twice. The two effect scripts are rawfiles of every zone - the client asks for both when a level starts and
		// logs "file not found" without them -; the level script is only written to the folder: the dedicated server
		// reads level scripts from its script folders, never from a zone.
		void write_scripts(compile_context& ctx)
		{
			const world& w = ctx.w;
			const auto id_of = [](const std::string& fx)
			{
				std::string id = fx.substr(fx.find_last_of('/') == std::string::npos ? 0 : fx.find_last_of('/') + 1);
				for (char& c : id)
				{
					c = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ? c : (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : '_';
				}
				return id;
			};
			std::map<std::string, std::string> effects;		// id -> effect
			std::string placement = "//_createfx generated. Do not touch!!\n#include common_scripts\\utility;\n#include common_scripts\\_createfx;\nmain()\n{\n";
			placement += format("\t// CreateFX entities placed: %zu\n", w.fx.size());
			std::size_t sounds = 0;
			for (const map_fx& fx : w.fx)
			{
				if (!fx.fx.empty())
				{
					const std::string id = id_of(fx.fx);
					effects[id] = fx.fx;
					placement += format("\tent = %s( \"%s\" );\n", fx.delay >= 0 ? "createLoopEffect" : "createOneshotEffect", id.c_str());
					placement += format("\tent.v[ \"origin\" ] = ( %g, %g, %g );\n\tent.v[ \"angles\" ] = ( %g, %g, %g );\n", fx.origin[0], fx.origin[1], fx.origin[2], fx.angles[0],
						fx.angles[1], fx.angles[2]);
					placement += format("\tent.v[ \"fxid\" ] = \"%s\";\n\tent.v[ \"delay\" ] = %g;\n", id.c_str(), fx.delay);
					if (!fx.alias.empty())
					{
						placement += format("\tent.v[ \"soundalias\" ] = \"%s\";\n", fx.alias.c_str());
					}
				}
				else
				{
					sounds++;
					placement += format("\tent = createLoopSound();\n\tent.v[ \"origin\" ] = ( %g, %g, %g );\n\tent.v[ \"angles\" ] = ( %g, %g, %g );\n\tent.v[ \"soundalias\" ] = \"%s\";\n",
						fx.origin[0], fx.origin[1], fx.origin[2], fx.angles[0], fx.angles[1], fx.angles[2], fx.alias.c_str());
				}
				placement += "\n";
			}
			placement += "}\n";
			std::string list = "main()\n{\n\n";
			for (const auto& [id, effect] : effects)
			{
				list += format("\tlevel._effect[ \"%s\" ] = loadfx( \"%s\" );\n", id.c_str(), effect.c_str());
			}
			list += "\n/#\n\tif ( common_scripts\\utility::shouldRunServerSideEffects() )\n\t\tmaps\\createfx\\" + w.name + "_fx::main();\n#/\n}\n";
			// line ends as in the shipped files
			const auto crlf = [](const std::string& text)
			{
				std::string out;
				for (const char c : text)
				{
					if (c == '\n')
					{
						out += '\r';
					}
					out += c;
				}
				return out;
			};
			placement = crlf(placement);
			list = crlf(list);
			std::string level = "// generated by codol_bsp: the level script of " + w.name + "\n#include maps\\mp\\_utility;\n\nmain()\n{\n\tmaps\\mp\\" + w.name
				+ "_fx::main();\n\n\tmaps\\mp\\_load::main();\n\n";
			if (!ctx.opts.minimap.empty())
			{
				level += "\tmaps\\mp\\_compass::setupMiniMap( \"compass_map_" + w.name + "\" );\n\n";
			}
			level += "\tgame[ \"attackers\" ] = \"allies\";\n\tgame[ \"defenders\" ] = \"axis\";\n}\n";
			const std::string fx_name = "maps/mp/" + w.name + "_fx.gsc", createfx_name = "maps/createfx/" + w.name + "_fx.gsc";
			const auto bytes = [](const std::string& text) { return std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(text.data()), text.size()); };
			check(ctx.editor.set_rawfile(fx_name, bytes(list), true), "adding rawfile " + fx_name);
			check(ctx.editor.set_rawfile(createfx_name, bytes(placement), true), "adding rawfile " + createfx_name);
			ctx.rep.notes.push_back(format("scripts: %zu effects ( %zu kinds ), %zu sounds; rawfiles %s, %s", w.fx.size() - sounds, effects.size(), sounds, fx_name.c_str(),
				createfx_name.c_str()));
			if (!ctx.opts.scripts_out.empty())
			{
				const auto save = [&](const std::string& name, const std::string& text)
				{
					const std::filesystem::path file = ctx.opts.scripts_out / name;
					std::error_code ec;
					std::filesystem::create_directories(file.parent_path(), ec);
					std::ofstream out(file, std::ios::binary | std::ios::trunc);
					out.write(text.data(), static_cast<std::streamsize>(text.size()));
					if (!out)
					{
						throw compile_error("cannot write " + file.string());
					}
				};
				save(fx_name, list);
				save(createfx_name, placement);
				save("maps/mp/" + w.name + ".gsc", level);
				ctx.rep.notes.push_back("scripts: written to " + ctx.opts.scripts_out.string() + " ( maps/mp/" + w.name + ".gsc is the level script for the server's script folder )");
			}
		}
	}

	bool compile_document(const edit::document& doc, const options& opts, const zone::ClientKey& key, std::vector<std::uint8_t>& zone_bytes, report& rep,
		std::string& error)
	try
	{
		const stopwatch total;
		stopwatch stage;
		const auto stage_done = [&](const char* name)
		{
			rep.stage_seconds.emplace_back(name, stage.seconds());
			stage = stopwatch();
		};

		world w;
		w.name = !opts.name.empty() ? opts.name : opts.out.stem().string();
		if (w.name.empty())
		{
			throw compile_error("no map name ( --name, or the name of the output file )");
		}
		for (const char c : w.name)
		{
			if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_'))
			{
				throw compile_error("the map name '" + w.name + "' may only hold a-z, 0-9 and _ ( the server refuses other names )");
			}
		}
		w.bsp_name = "maps/mp/" + w.name + ".d3dbsp";
		rep.map_name = w.name;

		catalog cat;
		load_catalog(opts, key, cat, rep);
		stage_done("catalog");

		read_document(doc, opts, w, rep);
		{
			const std::string ents = build_entity_string(w);
			std::uint32_t hash = 0x811C9DC5u;
			for (const char c : ents)
			{
				hash = (hash ^ static_cast<std::uint8_t>(c)) * 16777619u;
			}
			for (const map_brush& b : w.brushes)
			{
				for (int k = 0; k < 3; k++)
				{
					std::uint32_t bits;
					std::memcpy(&bits, &b.box.mins[k], 4);
					hash = (hash ^ bits) * 16777619u;
					std::memcpy(&bits, &b.box.maxs[k], 4);
					hash = (hash ^ bits) * 16777619u;
				}
			}
			w.checksum = hash;
		}
		stage_done("read");

		ZoneEditor editor;
		check(editor.create(w.name), "creating the zone");
		asset_importer importer(editor, key, opts.install, opts.donors, cat);
		compile_context ctx{ opts, rep, w, editor, importer, cat };

		// the animation list the server asks every map for; empty
		check(editor.set_rawfile("animlists/" + w.name + ".animlist", {}, false), "adding the animation list");
		build_light_defs(ctx);
		build_vision_and_sun(ctx);
		build_com_world(ctx);
		build_fx_world(ctx);
		asset_builder clip = build_clip_map(ctx);
		stage_done("collision");
		build_gfx_world(ctx);
		stage_done("render");
		build_game_world(ctx);
		clip.put(0, offsetof(c1::clipMap_t, planeCount), static_cast<std::int32_t>(w.planes.planes.size()));
		resolve_refs(clip, w);
		check(editor.add_built_asset(c1::ASSET_TYPE_CLIPMAP_MP, w.bsp_name, clip.chunks), "adding the clip map");
		rep.clip_planes = w.planes.planes.size();

		for (const auto& [name, file] : opts.rawfiles)
		{
			const std::vector<std::uint8_t> contents = read_file(file);
			check(editor.set_rawfile(name, contents, true), "adding rawfile " + name);
		}
		write_scripts(ctx);

		{
			// lit technique sets come with their hardware shadow map sibling ( "_sm_" -> "_hsm_" ); where no zone had one
			std::size_t siblings = 0;
			for (std::size_t a = 0; a < editor.asset_count(); a++)
			{
				const zone::EditorAsset asset = editor.asset(a);
				siblings += asset.type == c1::ASSET_TYPE_TECHNIQUE_SET && asset.name.find("_hsm_") != std::string::npos;
			}
			rep.notes.push_back(format("materials: %zu technique sets for hardware shadow maps ( _hsm_ ) came with the lit ones", siblings));
			for (const std::string& name : importer.technique_sets_without_shadow_sibling())
			{
				rep.warnings.push_back("technique set " + name + " has no hardware shadow map sibling ( _hsm_ ) in the zone it came from: its materials get no dynamic sun shadow");
			}
		}

		zone::ZoneWriteReport written;
		check(editor.write(zone_bytes, &written), "laying out the zone ( the game's loaders refuse what was built )");
		rep.assets = editor.asset_count();
		rep.zone_bytes = zone_bytes.size();
		stage_done("zone");

		std::vector<std::string> problems;
		std::string summary;
		if (!validate_zone(zone_bytes, problems, &summary))
		{
			std::string text = "the compiled zone does not pass validation:";
			for (const std::string& p : problems)
			{
				text += "\n  " + p;
			}
			throw compile_error(text);
		}
		rep.notes.push_back("validated: " + summary);
		stage_done("validate");
		rep.seconds = total.seconds();
		return true;
	}
	catch (const std::exception& e)
	{
		error = e.what();
		return false;
	}

	bool compile(const options& opts, report& rep, std::string& error)
	try
	{
		const stopwatch total;
		zone::ClientKey key;
		check(key.load_from_exe(opts.exe), "reading the fast file key from " + opts.exe.string());
		edit::document doc;
		if (!doc.load_map_file(opts.map, error))
		{
			error = opts.map.string() + ": " + error;
			return false;
		}
		// misc_prefab: the contents of the prefab's .map in its place ( prefabs inside prefabs too )
		edit::prefab_result prefabs;
		edit::expand_prefabs_from_disk(doc, opts.map, prefabs, opts.prefab_roots);
		std::vector<std::uint8_t> zone_bytes;
		const bool compiled = compile_document(doc, opts, key, zone_bytes, rep, error);
		if (prefabs.expanded)
		{
			rep.notes.push_back(format("prefabs: %d expanded into %d brushes, %d patches, %d entities", prefabs.expanded, prefabs.brushes, prefabs.patches,
				prefabs.entities));
		}
		for (const std::string& warning : prefabs.warnings)
		{
			rep.warnings.push_back("prefab: " + warning);
		}
		if (!compiled)
		{
			return false;
		}
		if (!opts.zone_out.empty())
		{
			std::ofstream out(opts.zone_out, std::ios::binary | std::ios::trunc);
			out.write(reinterpret_cast<const char*>(zone_bytes.data()), static_cast<std::streamsize>(zone_bytes.size()));
		}
		std::error_code ec;
		std::filesystem::create_directories(std::filesystem::absolute(opts.out).parent_path(), ec);
		zone::FastFileWriteOptions write_options;
		write_options.compression = opts.level;
		write_options.exactFileSize = opts.exact_size;
		check(zone::write_fastfile(opts.out, zone_bytes, key, write_options), "writing " + opts.out.string());
		rep.file_bytes = static_cast<std::size_t>(std::filesystem::file_size(opts.out, ec));

		// <map>_load.ff: what the client loads in front of the map to draw the loading screen. The shipped ones hold
		// the materials $levelbriefing and $defeatbackdrop ( with the technique set "2d" ) and a rawfile; the
		// materials are taken from a shipped load zone, so the picture is that map's until the map brings its own.
		if (opts.load_zone)
		{
			const std::string map_name = !opts.name.empty() ? opts.name : opts.out.stem().string();
			const std::filesystem::path load_file = !opts.load_out.empty() ? opts.load_out
				: std::filesystem::absolute(opts.out).parent_path() / (map_name + "_load.ff");
			catalog cat;
			std::filesystem::path folder = opts.cache.empty() ? std::filesystem::absolute(opts.out).parent_path() : opts.cache;
			cat.load(folder / "codol_bsp_catalog.txt", opts.install);
			ZoneEditor editor;
			check(editor.create(map_name + "_load"), "creating the load zone");
			asset_importer importer(editor, key, opts.install, opts.donors, cat);
			for (const char* material : { "$defeatbackdrop", "$levelbriefing" })
			{
				std::string why;
				if (!importer.material(material, &why).valid())
				{
					rep.warnings.push_back(std::string("load zone: material ") + material + " not found: " + why);
				}
			}
			const std::string text = "codol_bsp " + map_name + "\n";
			check(editor.set_rawfile(map_name + "_load", { reinterpret_cast<const std::uint8_t*>(text.data()), text.size() }, true), "load zone rawfile");
			std::vector<std::uint8_t> load_bytes;
			check(editor.write(load_bytes), "laying out the load zone");
			zone::FastFileWriteOptions load_options;
			load_options.compression = opts.level;
			check(zone::write_fastfile(load_file, load_bytes, key, load_options), "writing " + load_file.string());
			rep.notes.push_back("load zone: " + load_file.string());
		}
		rep.seconds = total.seconds();
		return true;
	}
	catch (const std::exception& e)
	{
		error = e.what();
		return false;
	}
}
