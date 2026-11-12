// The compile: document -> world -> the map assets -> zone -> fast file.

#include "world.hpp"
#include "gfx_layout.hpp"

#include <fstream>

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
			out.canUseShadowMap = 1;
			out.exponent = 1;
			std::memcpy(out.origin, in.origin, sizeof(out.origin));
			out.radius = in.radius;
			out.cosHalfFovOuter = in.cos_half_fov_outer;
			out.cosHalfFovInner = in.cos_half_fov_inner;
			out.cosHalfFovExpanded = in.cos_half_fov_outer;
			out.rotationLimit = 1.0f;
			out.translationLimit = 0.0f;
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

	// The light definitions the primary lights name. A GfxLightDef is a name, an attenuation image and where its
	// lookup starts in the light map's lookup strip; the image lives in the game's common zones and is named here
	// ( ",falloff_linear" ). Only light_point_linear is known to this compiler: 0x62 / 1 are that definition's values
	// in every shipped map.
	void build_light_defs(compile_context& ctx)
	{
		std::set<std::string> defs;
		for (map_light& light : ctx.w.lights)
		{
			if (light.type < 2)
			{
				continue;
			}
			if (light.def != "light_point_linear")
			{
				ctx.rep.warnings.push_back("light: definition '" + light.def + "' is not known, light_point_linear is used");
				light.def = "light_point_linear";
			}
			defs.insert(light.def);
		}
		for (const std::string& def : defs)
		{
			asset_builder b;
			const std::uint32_t header[4] = { FOLLOWS, FOLLOWS, 0x62, 1 };
			b.add(header, sizeof(header));
			b.add_string(def);
			const std::uint32_t image = b.add_zero(sizeof(c1::GfxImage));
			b.follows(image, offsetof(c1::GfxImage, name));
			b.add_string(",falloff_linear");
			check(ctx.editor.add_built_asset(c1::ASSET_TYPE_LIGHT_DEF, def, b.chunks), "adding light definition " + def);
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
		// The scripts a map brings: the effect list ( maps/mp/<map>_fx.gsc: loadfx per effect, then the placement ),
		// the placement ( maps/createfx/<map>_fx.gsc, in the form the game's createfx tool writes ) and the level
		// script. The two effect scripts are rawfiles of the zone when the map places effects or sounds, or when the
		// scripts are asked for; the level script is only written to the folder: the dedicated server reads level
		// scripts from its script folders, never from a zone.
		void write_scripts(compile_context& ctx)
		{
			const world& w = ctx.w;
			if (w.fx.empty() && ctx.opts.scripts_out.empty())
			{
				return;
			}
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
			std::string placement = "//_createfx generated by codol_bsp. Do not touch!!\n#include common_scripts\\utility;\n#include common_scripts\\_createfx;\nmain()\n{\n";
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
			std::string list = "// generated by codol_bsp from the script_fx / script_sound entities of the map\nmain()\n{\n";
			for (const auto& [id, effect] : effects)
			{
				list += format("\tlevel._effect[ \"%s\" ] = loadfx( \"%s\" );\n", id.c_str(), effect.c_str());
			}
			list += "\n\tmaps\\createfx\\" + w.name + "_fx::main();\n}\n";
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
		std::vector<std::uint8_t> zone_bytes;
		if (!compile_document(doc, opts, key, zone_bytes, rep, error))
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
