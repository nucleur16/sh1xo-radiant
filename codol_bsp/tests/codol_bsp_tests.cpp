// codol_bsp_tests: writes the test maps, compiles them and checks the result.
//
//   codol_bsp_tests --write-maps <dir>          only writes the test maps ( .map files )
//   codol_bsp_tests [--out <dir>] [--game <install>] [--assets <project zone.ff>]...
//
// --assets: codol_assetc zones for the maps that use a modder's own assets - the materials bsp_test_wall / _fence /
// _decal ( tests/make_modder_assets.py prints the command that builds the zone ) and the model codol_test_crate
// ( codol_assets' test zone ). Without them those checks are skipped.
//
// Plain asserts in the style of the other test programs of this repository: every check prints when it fails, the
// program returns 1 when one did. Needs the 1.1.1.13 install ( materials come from its zones ); without it only the
// maps are written.

#include "test_maps.hpp"

#include "codol_bsp/bsp.hpp"
#include "codol_mapc/server_check.hpp"
#include "codol_mapc/trace.hpp"
#include "codol_zone/assets.hpp"
#include "codol_zone/zone_editor.hpp"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <random>
#include <set>

namespace
{
	using namespace codol;
	namespace fs = std::filesystem;

	int checks = 0, failed = 0;

	void check(const bool ok, const char* what, const char* file, const int line)
	{
		checks++;
		if (!ok)
		{
			failed++;
			std::printf("FAILED %s ( %s:%d )\n", what, file, line);
		}
	}
#define CHECK(x) check((x), #x, __FILE__, __LINE__)

	struct test_map
	{
		const char* name;
		void (*make)(edit::document&);
	};

	const test_map MAPS[] =
	{
		{ "box_room", [](edit::document& doc) { bsp::test::make_box_room(doc); } },
		{ "sky_room", [](edit::document& doc) { bsp::test::make_box_room(doc, {}, true); } },
		{ "two_rooms", [](edit::document& doc) { bsp::test::make_two_rooms(doc); } },
		{ "two_rooms_leak", [](edit::document& doc) { bsp::test::make_two_rooms(doc, {}, true); } },
		{ "content", [](edit::document& doc) { bsp::test::make_content(doc); } },
		{ "big", [](edit::document& doc) { bsp::test::make_big(doc, 12); } },
		{ "modder", [](edit::document& doc) { bsp::test::make_modder(doc); } },
		{ "terrain", [](edit::document& doc) { bsp::test::make_terrain(doc); } },
		{ "yard", [](edit::document& doc) { bsp::test::make_yard(doc); } },
	};

	bool write_maps(const fs::path& dir)
	{
		std::error_code ec;
		fs::create_directories(dir, ec);
		for (const test_map& m : MAPS)
		{
			std::string error;
			edit::document doc;
			m.make(doc);
			if (!doc.save_map_file(dir / (std::string(m.name) + ".map"), error))
			{
				std::printf("%s\n", error.c_str());
				return false;
			}
		}
		return true;
	}

	bool has_note(const bsp::report& rep, const char* text)
	{
		for (const std::string& n : rep.notes)
		{
			if (n.find(text) != std::string::npos)
			{
				return true;
			}
		}
		return false;
	}

	struct compiled
	{
		bsp::report rep;
		fs::path file;
		bool ok = false;
	};

	std::vector<fs::path> project_zones;

	compiled compile(const fs::path& out, const fs::path& game, const char* map, const bool light = true, const bool extras = false)
	{
		compiled c;
		bsp::options opts;
		opts.donors = project_zones;
		if (extras)
		{
			opts.minimap = out / "extras" / (std::string("compass_map_mp_") + map + ".tga");
			opts.scripts_out = out / "extras" / "scripts";
		}
		opts.map = out / "maps" / (std::string(map) + ".map");
		opts.exe = game / "C1.exe";
		opts.install = game;
		c.file = out / "ff" / ("mp_" + std::string(map) + ".ff");
		opts.out = c.file;
		opts.cache = out;
		opts.light = light;
		opts.quality = 0;
		std::string error;
		c.ok = bsp::compile(opts, c.rep, error);
		if (!c.ok)
		{
			std::printf("%s: %s\n", map, error.c_str());
		}
		return c;
	}

	// ---- the light of a compiled zone, as the game reads it ( codol_bsp/src/light.cpp, head )
	struct zone_light
	{
		zone::Zone z;
		zone::GfxWorldView world;
		zone::ImageView primary, secondary;		// lightmap 0
		bool ok = false;

		struct com_light
		{
			std::uint8_t type, canUseShadowMap, exponent, unused;
			float color[3], dir[3], origin[3], radius, cosHalfFovOuter, cosHalfFovInner, cosHalfFovExpanded, rotationLimit, translationLimit;
			std::uint32_t defName;
		};
		std::vector<com_light> lights;

		zone_light(const fs::path& file, const fs::path& game)
		{
			zone::ClientKey key;
			if (!key.load_from_exe(game / "C1.exe") || !z.open(file, key) || !zone::read_gfx_world(z, zone::first_asset(z, c1::ASSET_TYPE_GFXWORLD), world)
				|| world.raw.draw.lightmapCount < 1)
			{
				return;
			}
			const auto pointers = z.get_array<zone::zaddr>(world.raw.draw.lightmaps, 2);
			ok = zone::read_image(z, pointers[0], primary) && zone::read_image(z, pointers[1], secondary) && !secondary.pixels.empty() && !primary.pixels.empty();
			if (const zone::zaddr at = zone::first_asset(z, c1::ASSET_TYPE_COMWORLD))
			{
				lights = z.get_array<com_light>(z.get<std::uint32_t>(at + 12), z.get<std::uint32_t>(at + 8));
			}
		}
		// a texel of a layer of the secondary lightmap ( b g r a )
		const std::uint8_t* texel(const int layer, const int x, const int y) const
		{
			const int w = secondary.raw.width, h = secondary.raw.height / 2;
			return secondary.pixels.data() + ((static_cast<std::size_t>(layer) * h + static_cast<std::size_t>(y)) * w + static_cast<std::size_t>(x)) * 4;
		}
		// the brightest texel of a layer below row 0
		int brightest(const int layer) const
		{
			int most = 0;
			for (int y = 1; y < secondary.raw.height / 2; y++)
			{
				for (int x = 0; x < secondary.raw.width; x++)
				{
					most = std::max<int>(most, texel(layer, x, y)[1]);
				}
			}
			return most;
		}
		// the light the shader makes of a lightmap coordinate on a flat normal: ( upper + lower * cos )^2, green
		float light_at(const float u, const float v) const
		{
			const int w = secondary.raw.width, h = secondary.raw.height / 2;
			const int x = std::clamp(static_cast<int>(u * static_cast<float>(w)), 0, w - 1), y = std::clamp(static_cast<int>(v * static_cast<float>(h)), 0, h - 1);
			const std::uint8_t* up = texel(0, x, y);
			const std::uint8_t* low = texel(1, x, y);
			const float dx = static_cast<float>(up[3]) / 255.0f * 4.08f - 2.08f, dy = static_cast<float>(low[3]) / 255.0f * 4.06451607f - 2.06451607f;
			const float root = static_cast<float>(up[1]) / 255.0f + static_cast<float>(low[1]) / 255.0f / std::sqrt(dx * dx + dy * dy + 1.0f);
			return root * root;
		}
		// the baked light at a point of the floor ( z = `height` ): the triangle of the world that lies there, its lightmap
		// coordinates between its corners; -1: no triangle
		float light_on_floor(const float x, const float y, const float height = 0.0f) const
		{
			const std::vector<c1::GfxWorldVertex> vertices = world.vertices();
			const std::vector<std::uint16_t> indices = world.indices();
			for (const zone::WorldSurface& surface : world.surfaces)
			{
				for (std::uint32_t t = 0; t < surface.raw.triCount; t++)
				{
					const c1::GfxWorldVertex* v[3];
					bool flat = true;
					for (int k = 0; k < 3; k++)
					{
						v[k] = &vertices[surface.raw.firstVertex + indices[surface.raw.baseIndex + 3 * t + static_cast<std::uint32_t>(k)]];
						flat = flat && std::fabs(v[k]->xyz[2] - height) < 0.01f;
					}
					const float d = (v[1]->xyz[1] - v[2]->xyz[1]) * (v[0]->xyz[0] - v[2]->xyz[0]) + (v[2]->xyz[0] - v[1]->xyz[0]) * (v[0]->xyz[1] - v[2]->xyz[1]);
					if (!flat || std::fabs(d) < 1e-6f)
					{
						continue;
					}
					const float a = ((v[1]->xyz[1] - v[2]->xyz[1]) * (x - v[2]->xyz[0]) + (v[2]->xyz[0] - v[1]->xyz[0]) * (y - v[2]->xyz[1])) / d;
					const float b = ((v[2]->xyz[1] - v[0]->xyz[1]) * (x - v[2]->xyz[0]) + (v[0]->xyz[0] - v[2]->xyz[0]) * (y - v[2]->xyz[1])) / d;
					if (a < 0 || b < 0 || a + b > 1)
					{
						continue;
					}
					return light_at(a * v[0]->lmapCoord[0] + b * v[1]->lmapCoord[0] + (1 - a - b) * v[2]->lmapCoord[0],
						a * v[0]->lmapCoord[1] + b * v[1]->lmapCoord[1] + (1 - a - b) * v[2]->lmapCoord[1]);
				}
			}
			return -1.0f;
		}
		std::string rawfile(const std::string& name) const
		{
			zone::RawFileView view;
			std::vector<std::uint8_t> contents;
			if (const zone::zaddr at = z.find_asset(c1::ASSET_TYPE_RAWFILE, name); at && zone::read_rawfile(z, at, view) && zone::rawfile_contents(view, contents))
			{
				return std::string(contents.begin(), contents.end());
			}
			return {};
		}
	};

	// the box room's .map with other lights: the worldspawn's light keys and the light entities taken out, `entities` added
	bool write_light_map(const fs::path& maps, const char* name, const std::string& worldspawn_keys, const std::string& entities)
	{
		std::ifstream in(maps / "box_room.map");
		std::string line, text;
		bool in_light = false;
		std::vector<std::string> block;
		int depth = 0;
		bool keys_done = false;
		while (std::getline(in, line))
		{
			if (!line.empty() && line.back() == '\r')
			{
				line.pop_back();
			}
			bool drop = false;
			for (const char* key : { "\"sunlight\"", "\"suncolor\"", "\"sundirection\"", "\"ambient\"", "\"_color\"", "\"skylight\"", "\"skycolor\"" })
			{
				drop = drop || (depth == 1 && !keys_done && line.rfind(key, 0) == 0);
			}
			if (line == "{")
			{
				depth++;
			}
			else if (line == "}")
			{
				depth--;
			}
			if (depth == 1 && line == "{")
			{
				block.clear();
				in_light = false;
			}
			if (line.find("\"classname\" \"light\"") == 0)
			{
				in_light = true;
			}
			if (drop)
			{
				continue;
			}
			block.push_back(line);
			if (line.find("\"classname\" \"worldspawn\"") == 0)
			{
				block.push_back(worldspawn_keys);
			}
			if (depth == 0 && line == "}")
			{
				keys_done = true;
				if (!in_light)
				{
					for (const std::string& b : block)
					{
						text += b + "\n";
					}
				}
				block.clear();
				in_light = false;
			}
			else if (depth == 0)
			{
				// between entities ( comments )
				text += line + "\n";
				block.clear();
			}
		}
		std::ofstream out(maps / (std::string(name) + ".map"), std::ios::trunc);
		out << text << entities;
		return static_cast<bool>(out) && !text.empty();
	}

	// what every compiled zone has to satisfy
	void check_zone(const compiled& c, const fs::path& game, const fs::path& map_file)
	{
		CHECK(c.ok);
		if (!c.ok)
		{
			return;
		}
		CHECK(has_note(c.rep, "validated: server collision ok"));
		CHECK(has_note(c.rep, "render ok"));
		CHECK(!has_note(c.rep, "SELF CHECK FAILED"));

		zone::ClientKey key;
		CHECK(static_cast<bool>(key.load_from_exe(game / "C1.exe")));
		{
			// every lit technique set of the zone ( "_sm_" ) has its hardware shadow map sibling ( "_hsm_" ) in the zone, or
			// the compiler said that no donor has one: without it the game draws the material without the sun's shadow map
			zone::ZoneEditor editor;
			CHECK(static_cast<bool>(editor.open(c.file, key)));
			std::set<std::string> sets;
			for (std::size_t a = 0; a < editor.asset_count(); a++)
			{
				// ( a set of two stream pieces is a header and a name: the stand in for a set of a zone that is loaded already )
				if (editor.asset(a).type == c1::ASSET_TYPE_TECHNIQUE_SET && editor.asset(a).chunks > 2)
				{
					sets.insert(editor.asset(a).name);
				}
			}
			std::size_t lit = 0, with_sibling = 0, told = 0;
			for (const std::string& name : sets)
			{
				const std::size_t at = name.find("_sm_");
				if (at == std::string::npos || name[0] == ',')
				{
					continue;
				}
				lit++;
				std::string sibling = name;
				sibling.replace(at, 4, "_hsm_");
				with_sibling += sets.count(sibling);
				for (const std::string& warning : c.rep.warnings)
				{
					told += warning.find("technique set " + name + " has no hardware shadow map sibling") != std::string::npos;
				}
			}
			CHECK(lit > 0 && with_sibling + told == lit && with_sibling > 0);
		}
		std::vector<std::uint8_t> bytes;
		CHECK(static_cast<bool>(zone::read_fastfile(c.file, key, bytes)));

		// round trip: the zone read by codol_zone and written again is the zone
		zone::ZoneEditor editor;
		CHECK(static_cast<bool>(editor.load(bytes, "roundtrip")));
		std::vector<std::uint8_t> again;
		CHECK(static_cast<bool>(editor.write(again)));
		CHECK(again == bytes);

		// the validators accept it, read from the file
		std::vector<std::string> problems;
		CHECK(bsp::validate_zone(bytes, problems));
		for (const std::string& p : problems)
		{
			std::printf("  %s\n", p.c_str());
		}

		// collision: walking the trees gives what testing every brush gives
		const mapc::clip_trace trace(editor);
		CHECK(trace.valid());
		std::mt19937 random(1234);
		std::uniform_real_distribution<float> where(-1100.0f, 1100.0f), height(-40.0f, 340.0f);
		int disagreements = 0, hits = 0;
		for (int k = 0; k < 3000; k++)
		{
			const float a[3] = { where(random), where(random), height(random) }, b[3] = { where(random), where(random), height(random) };
			const float half[3] = { 15, 15, 35 };
			const mapc::trace_result tree = trace.trace(a, b, -1, k % 3 == 0 ? half : nullptr), all = trace.trace_all(a, b, -1, k % 3 == 0 ? half : nullptr);
			disagreements += std::fabs(tree.fraction - all.fraction) > 1e-4f || tree.start_solid != all.start_solid;
			hits += all.fraction < 1.0f;
		}
		CHECK(disagreements == 0);
		CHECK(hits > 100);

		// the import of the compiled zone gives back the brushes of the map
		edit::document source, imported;
		std::string error;
		CHECK(source.load_map_file(map_file, error));
		CHECK(imported.import_fastfile(c.file, game / "C1.exe", error));
		const auto boxes = [](const edit::document& doc, const bool tools)
		{
			std::multiset<std::array<long long, 6>> out;
			for (const edit::object_id e : doc.entities())
			{
				const edit::entity* entity = doc.find_entity(e);
				for (const edit::object_id child : entity->children)
				{
					const edit::brush* b = doc.find_brush(child);
					const edit::brush_geometry* geo = doc.geometry(child);
					if (!b || !geo || !geo->valid)
					{
						continue;
					}
					bool skip = false;
					for (const edit::face& f : b->faces)
					{
						skip = skip || (!tools && (f.texture.material == "portal" || f.texture.material == "portal_nodraw"));
					}
					if (!skip)
					{
						out.insert({ std::llround(geo->box.mins.x), std::llround(geo->box.mins.y), std::llround(geo->box.mins.z), std::llround(geo->box.maxs.x),
							std::llround(geo->box.maxs.y), std::llround(geo->box.maxs.z) });
					}
				}
			}
			return out;
		};
		// ( a map with patches has more brushes in the zone: the collision under every triangle of a patch )
		const auto wanted = boxes(source, false), got = boxes(imported, true);
		CHECK(source.patch_count() ? std::includes(got.begin(), got.end(), wanted.begin(), wanted.end()) : wanted == got);
	}
}

int main(const int argc, char** argv)
{
	fs::path out = "codol_bsp_test_out", game = CODOL_BSP_GAME_DIR;
	for (int i = 1; i < argc; i++)
	{
		const std::string arg = argv[i];
		if (arg == "--write-maps" && i + 1 < argc)
		{
			return write_maps(argv[i + 1]) ? 0 : 1;
		}
		if (arg == "--import" && i + 2 < argc)
		{
			// codol_bsp_tests --import <shipped map.ff> <out.map> [--game <install>]: the collision brushes and entities
			// of a shipped map as a .map ( codol_edit ), a stress test for the compiler's collision side
			edit::document doc;
			std::string error;
			if (!doc.import_fastfile(argv[i + 1], game / "C1.exe", error) || !doc.save_map_file(argv[i + 2], error))
			{
				std::printf("%s\n", error.c_str());
				return 1;
			}
			std::printf("%zu brushes, %zu patches, %zu entities -> %s\n", doc.brush_count(), doc.patch_count(), doc.entities().size(), argv[i + 2]);
			return 0;
		}
		if (arg == "--out" && i + 1 < argc)
		{
			out = argv[++i];
		}
		else if (arg == "--game" && i + 1 < argc)
		{
			game = argv[++i];
		}
		else if (arg == "--assets" && i + 1 < argc)
		{
			project_zones.emplace_back(argv[++i]);
		}
	}
	CHECK(write_maps(out / "maps"));
	std::error_code ec;
	if (!fs::exists(game / "C1.exe", ec))
	{
		std::printf("no install at %s: the compile tests are skipped\n%d checks, %d failed\n", game.string().c_str(), checks, failed);
		return failed ? 1 : 0;
	}

	// ---- the box room: collision where the map says
	{
		const compiled c = compile(out, game, "box_room");
		check_zone(c, game, out / "maps" / "box_room.map");
		CHECK(c.rep.clip_brushes == 11 && c.rep.sub_models == 2);
		CHECK(c.rep.cells == 1 && !c.rep.leaked);
		CHECK(c.rep.faces_culled >= 4);			// the undersides of the crates, the faces between the stacked ones
		if (c.ok)
		{
			zone::ClientKey key;
			key.load_from_exe(game / "C1.exe");
			zone::ZoneEditor editor;
			CHECK(static_cast<bool>(editor.open(c.file, key)));
			const mapc::clip_trace trace(editor);
			const auto hit = [&](const float ax, const float ay, const float az, const float bx, const float by, const float bz, const int axis)
			{
				const float a[3] = { ax, ay, az }, b[3] = { bx, by, bz };
				const mapc::trace_result r = trace.trace(a, b, -1);
				return a[axis] + (b[axis] - a[axis]) * r.fraction;
			};
			CHECK(std::fabs(hit(0, 0, 100, 0, 0, -100, 2) - 0.0f) < 0.01f);			// the floor
			CHECK(std::fabs(hit(0, 0, 100, 800, 0, 100, 0) - 512.0f) < 0.01f);		// a wall
			CHECK(std::fabs(hit(0, 0, 100, 0, 0, 600, 2) - 320.0f) < 0.01f);		// the ceiling
			CHECK(std::fabs(hit(160, 160, 200, 160, 160, -50, 2) - 64.0f) < 0.01f);	// a crate
			CHECK(std::fabs(hit(-224, 96, 300, -224, 96, -50, 2) - 128.0f) < 0.01f);	// the stacked crates
		}
	}

	// ---- lighting: baked and flat
	{
		const compiled baked = compile(out, game, "sky_room"), flat = compile(out, game, "sky_room", false);
		check_zone(baked, game, out / "maps" / "sky_room.map");
		CHECK(flat.ok);
		CHECK(baked.rep.lightmaps == 1 && baked.rep.light_grid_points > 1000 && baked.rep.reflection_probes == 2);
		CHECK(has_note(baked.rep, "charts do not overlap"));
		CHECK(has_note(baked.rep, "sun reaches"));
	}

	// ---- two rooms: cells, portals, hidden faces; and the same map with a hole in a wall
	{
		const compiled c = compile(out, game, "two_rooms");
		check_zone(c, game, out / "maps" / "two_rooms.map");
		// room, corridor, room: one portal ( two GfxPortals ) per doorway. The corridor has the doorways' cross section,
		// so a slice through it is no opening; the doorway's cross section in the other face of its wall is the same opening
		CHECK(c.rep.cells == 3 && c.rep.portals == 4 && !c.rep.leaked);
		CHECK(has_note(c.rep, "2 openings found"));
		CHECK(c.rep.faces_culled > 60);
		const compiled leak = compile(out, game, "two_rooms_leak");
		CHECK(leak.ok && leak.rep.leaked && leak.rep.cells == 1);
		CHECK(leak.rep.leak_path.size() >= 3);
		if (leak.rep.leak_path.size() >= 3)
		{
			// the path starts at an entity inside and ends outside the walls, south of room B
			const bsp::leak_point& first = leak.rep.leak_path.front();
			const bsp::leak_point& last = leak.rep.leak_path.back();
			CHECK(std::fabs(first.xyz[0]) < 1024 && std::fabs(first.xyz[1]) < 384);
			CHECK(last.xyz[1] < -384 || std::fabs(last.xyz[0]) > 1040 || last.xyz[2] > 272 || last.xyz[2] < -16);
		}
	}

	// ---- content: brush models, a portal brush, a patch
	{
		const compiled c = compile(out, game, "content");
		check_zone(c, game, out / "maps" / "content.map");
		CHECK(c.rep.sub_models >= 3);
		CHECK(c.rep.cells == 2 && c.rep.portals == 2);		// the portal brush in the doorway
		// the static model: a draw instance with the cull distance the shipped maps have ( last LOD x scale, 0 when that
		// is more than 16 bits ), and a clip map entry whose matrix is the inverse of the scaled axis
		CHECK(c.rep.static_models == 1);
		zone::ClientKey key;
		zone::Zone z;
		if (c.ok && key.load_from_exe(game / "C1.exe") && z.open(c.file, key))
		{
			zone::GfxWorldView world;
			zone::ClipMapView clip;
			CHECK(zone::read_gfx_world(z, zone::first_asset(z, c1::ASSET_TYPE_GFXWORLD), world));
			CHECK(zone::read_clip_map(z, zone::first_asset(z, c1::ASSET_TYPE_CLIPMAP_MP), clip));
			CHECK(world.staticModels.size() == 1 && clip.staticModels.size() == 1);
			if (world.staticModels.size() == 1 && clip.staticModels.size() == 1)
			{
				const c1::GfxStaticModelDrawInst& d = world.staticModels[0].raw;
				const c1::cStaticModel_s& s = clip.staticModels[0].raw;
				bool inverse = true;
				for (int a = 0; a < 3; a++)
				{
					for (int k = 0; k < 3; k++)
					{
						inverse = inverse && std::fabs(s.invScaledAxis[a][k] - d.axis[k][a] / d.scale) < 1e-5f;
					}
				}
				CHECK(inverse);
				CHECK(std::fabs(d.axis[0][0] - 0.8660254f) < 1e-4f && std::fabs(d.axis[0][1] - 0.5f) < 1e-4f);		// yaw 30
				const c1::XModel model = z.get<c1::XModel>(d.model);
				const float reach = model.numLods ? model.lodInfo[model.numLods - 1].dist * d.scale : 0.0f;
				CHECK(d.cullDist == (reach > 0 && reach <= 65535.0f ? static_cast<std::uint16_t>(reach) : 0));
			}
		}
	}

	// ---- a static model that only exists nested in a shipped map's gfx_map is written again into the zone
	{
		edit::document doc;
		bsp::test::make_box_room(doc);
		const std::vector<std::pair<std::string, std::string>> keys = { { "model", "concrete_barrier_damaged_2" }, { "angles", "0 90 0" } };
		doc.create_point_entity("misc_model", { 300, 300, 0 }, keys);
		std::string error;
		CHECK(doc.save_map_file(out / "maps" / "nested_model.map", error));
		const compiled c = compile(out, game, "nested_model");
		check_zone(c, game, out / "maps" / "nested_model.map");
		bool left_out = false;
		for (const std::string& warning : c.rep.warnings)
		{
			left_out = left_out || warning.find("is left out") != std::string::npos;
		}
		CHECK(!left_out);
		zone::ClientKey key;
		zone::Zone z;
		if (c.ok && key.load_from_exe(game / "C1.exe") && z.open(c.file, key))
		{
			CHECK(z.find_asset(c1::ASSET_TYPE_XMODEL, "concrete_barrier_damaged_2") != 0);
			zone::ClipMapView clip;
			CHECK(zone::read_clip_map(z, zone::first_asset(z, c1::ASSET_TYPE_CLIPMAP_MP), clip) && clip.staticModels.size() == 1);
		}
	}

	// ---- terrain: patches are the clip map's triangles, not brushes; a ramp, a plateau, a ledge with its border
	{
		const compiled c = compile(out, game, "terrain", true, true);
		check_zone(c, game, out / "maps" / "terrain.map");
		CHECK(c.rep.patches == 2 && c.rep.clip_brushes == 12);		// the brushes of the room only
		CHECK(has_note(c.rep, "144 triangles") && has_note(c.rep, "4 borders"));
		zone::ClientKey key;
		std::vector<std::uint8_t> bytes;
		if (c.ok && key.load_from_exe(game / "C1.exe") && zone::read_fastfile(c.file, key, bytes))
		{
			std::string error;
			mapc::server_collision_info info;
			CHECK(mapc::check_server_collision(bytes, error, &info));
			CHECK(info.terrain_triangles == 144 && info.terrain_borders == 4 && info.terrain_partitions >= 9 && info.terrain_tree_nodes >= info.terrain_partitions);
			zone::Zone z;
			CHECK(static_cast<bool>(z.load(bytes, "terrain")));
			const c1::clipMap_t cm = z.get<c1::clipMap_t>(zone::first_asset(z, c1::ASSET_TYPE_CLIPMAP_MP));
			const std::vector<float> verts = z.get_array<float>(cm.verts, 3u * cm.vertCount);
			const std::vector<std::uint16_t> tris = z.get_array<std::uint16_t>(cm.triIndices, 3u * static_cast<std::size_t>(cm.triCount));
			// every triangle of the ramp lies on the surface the map describes and faces up: stored clockwise seen from
			// above ( cross( v1 - v0, v2 - v0 ) points down, as in the shipped maps )
			int on_surface = 0, facing = 0, ramp = 0;
			for (std::size_t t = 0; t + 2 < tris.size(); t += 3)
			{
				const float* a = &verts[3u * tris[t]];
				const float* b = &verts[3u * tris[t + 1]];
				const float* d = &verts[3u * tris[t + 2]];
				if (a[0] > 100 || b[0] > 100 || d[0] > 100)
				{
					continue;		// the ledge
				}
				ramp++;
				on_surface += std::fabs(a[2] - bsp::test::terrain_height(a[0])) < 0.01 && std::fabs(b[2] - bsp::test::terrain_height(b[0])) < 0.01
					&& std::fabs(d[2] - bsp::test::terrain_height(d[0])) < 0.01;
				facing += (b[0] - a[0]) * (d[1] - a[1]) - (b[1] - a[1]) * (d[0] - a[0]) < 0;
			}
			CHECK(ramp == 128 && on_surface == ramp && facing == ramp);
		}
		// the extras: a minimap picture, its corners in the entities, the generated scripts
		std::error_code extras_ec;
		CHECK(fs::file_size(out / "extras" / "compass_map_mp_terrain.tga", extras_ec) == 18 + 512 * 512 * 4);
		CHECK(fs::exists(out / "extras" / "scripts" / "maps" / "mp" / "mp_terrain.gsc", extras_ec));
		CHECK(has_note(c.rep, "minimap: corners added"));
		zone::Zone z;
		if (c.ok && key.load_from_exe(game / "C1.exe") && z.open(c.file, key))
		{
			CHECK(z.find_asset(c1::ASSET_TYPE_RAWFILE, "maps/mp/mp_terrain_fx.gsc") != 0);
			CHECK(z.find_asset(c1::ASSET_TYPE_RAWFILE, "maps/createfx/mp_terrain_fx.gsc") != 0);
		}
	}

	// ---- the light bake: bounced light, pages
	{
		const compiled c = compile(out, game, "sky_room");
		CHECK(has_note(c.rep, "lightmap page") && has_note(c.rep, "1 bounce") && has_note(c.rep, "light grid colour sets"));
		zone::ClientKey key;
		zone::Zone z;
		if (c.ok && key.load_from_exe(game / "C1.exe") && z.open(c.file, key))
		{
			// the light grid is directional: some colour set differs between its directions
			const zone::zaddr world = zone::first_asset(z, c1::ASSET_TYPE_GFXWORLD);
			zone::GfxWorldView view;
			CHECK(zone::read_gfx_world(z, world, view));
			CHECK(view.raw.draw.lightmapCount >= 1);
		}
	}

	// ---- a modder's materials ( only with --assets <zone of tests/make_modder_assets.py> )
	if (!project_zones.empty())
	{
		const compiled c = compile(out, game, "modder");
		check_zone(c, game, out / "maps" / "modder.map");
		bool missing = false;
		for (const std::string& warning : c.rep.warnings)
		{
			missing = missing || warning.find("is not drawn") != std::string::npos;
		}
		CHECK(!missing && !c.rep.leaked);
		CHECK(has_note(c.rep, "'bsp_test_wall' is on no brush of a shipped map; from its material: surface type 2, solid"));
		CHECK(has_note(c.rep, "'bsp_test_decal'") && has_note(c.rep, "does not collide"));
		zone::ClientKey key;
		zone::Zone z;
		if (c.ok && key.load_from_exe(game / "C1.exe") && z.open(c.file, key))
		{
			for (const char* material : { "wc/bsp_test_wall", "wc/bsp_test_fence", "wc/bsp_test_decal" })
			{
				CHECK(z.find_asset(c1::ASSET_TYPE_MATERIAL, material) != 0);
			}
			CHECK(z.find_asset(c1::ASSET_TYPE_IMAGE, "bsp_test_wall_col") != 0);
			CHECK(z.find_asset(c1::ASSET_TYPE_TECHNIQUE_SET, "wc_l_sm_t0c0") != 0);
		}
		// the yard: everything in one map
		const compiled yard = compile(out, game, "yard", true, true);
		check_zone(yard, game, out / "maps" / "yard.map");
		CHECK(!yard.rep.leaked && yard.rep.static_models == 9 && yard.rep.patches == 2);
		// the yard and the two buildings; a portal in each of the ten doors and windows, none inside a room or in the open
		CHECK(yard.rep.cells == 3 && yard.rep.portals == 20);
		CHECK(has_note(yard.rep, "scripts: 2 effects ( 2 kinds ), 1 sounds"));
		std::printf("yard: %zu brushes, %zu surfaces, %zu cells, %zu portals, %zu lightmaps, %.1f s\n", yard.rep.clip_brushes, yard.rep.surfaces, yard.rep.cells,
			yard.rep.portals, yard.rep.lightmaps, yard.rep.seconds);
	}
	else
	{
		std::printf("no --assets: the modder material map and the yard are not compiled\n");
	}

	// ---- light: a room with one light entity without keys and a worldspawn without light keys is lit - the defaults
	// apply ( they used to be overwritten with 0 ), and the lightmap holds what the game's shaders make that light of
	{
		CHECK(write_light_map(out / "maps", "plain_light", "", "{\n\"classname\" \"light\"\n\"origin\" \"0 0 200\"\n}\n"));
		const compiled c = compile(out, game, "plain_light");
		check_zone(c, game, out / "maps" / "plain_light.map");
		CHECK(has_note(c.rep, "sun ( 1.30 1.23 1.11 )") && has_note(c.rep, "ambient ( 0.100 0.100 0.100 )"));
		CHECK(has_note(c.rep, "1 light entities, 0 of them primary"));
		const zone_light light(c.file, game);
		CHECK(light.ok);
		if (light.ok)
		{
			CHECK(light.lights.size() == 2 && light.lights[1].type == 1 && std::fabs(light.lights[1].color[0] - 1.3f) < 0.01f);
			// both layers are used: light without a direction above, the light entity's from its direction below
			CHECK(light.brightest(0) > 60 && light.brightest(1) > 60);
			// the floor right under the light ( 200 units, radius 600: falloff about 0.45 ) is far brighter than the ambient
			// light alone ( 0.1 ), and brighter than the floor in a corner of the room
			const float under = light.light_on_floor(40, -60), corner = light.light_on_floor(-480, -480);
			CHECK(under > 0.4f && under < 0.9f);
			CHECK(corner > 0.1f && corner < under * 0.6f);
			// the falloff curve of the light definition lies in row 0: full at the light, nothing at its radius, never rising
			bool falling = light.texel(0, 1, 0)[1] == 255 && light.texel(0, 32, 0)[1] <= 8 && light.texel(0, 0, 0)[1] == 255;
			for (int x = 2; x <= 32; x++)
			{
				falling = falling && light.texel(0, x, 0)[1] <= light.texel(0, x - 1, 0)[1];
			}
			CHECK(falling);
			// the vision file every map gets, with the game's numbers for the settings
			const std::string vision = light.rawfile("vision/mp_plain_light.vision");
			CHECK(vision.rfind("511 \"1\"", 0) == 0 && vision.find("\n520 \"1\"") != std::string::npos && vision.find("r_") == std::string::npos);
			// the two effect files the client reads as text, in the shape of the shipped ones ( also for a map without effects )
			CHECK(light.rawfile("maps/createfx/mp_plain_light_fx.gsc").rfind("//_createfx generated. Do not touch!!\r\n#include common_scripts\\utility;\r\n", 0) == 0);
			CHECK(light.rawfile("maps/mp/mp_plain_light_fx.gsc").rfind("main()\r\n{\r\n", 0) == 0);
			CHECK(light.rawfile("maps/mp/mp_plain_light_fx.gsc").find("/#\r\n\tif ( common_scripts\\utility::shouldRunServerSideEffects() )\r\n\t\tmaps\\createfx\\mp_plain_light_fx::main();\r\n#/")
				!= std::string::npos);
		}
	}

	// ---- light: the worldspawn keys - _color is the colour of the ambient light, colours count by their largest part
	{
		CHECK(write_light_map(out / "maps", "ambient_light", "\"ambient\" \"0.4\"\n\"_color\" \"0.5 0.25 0.25\"\n\"sunlight\" \"2\"\n\"suncolor\" \"0.5 0.5 0.25\"\n\"sundirection\" \"-90 0 0\"", ""));
		const compiled c = compile(out, game, "ambient_light");
		CHECK(c.ok && has_note(c.rep, "sun ( 2.00 2.00 1.00 ) from ( -0.000 -0.000 1.000 )") && has_note(c.rep, "ambient ( 0.400 0.200 0.200 )"));
		const zone_light light(c.file, game);
		CHECK(light.ok);
		if (light.ok)
		{
			// no light entity, a closed room: every lit texel is the ambient light and what bounces of it - red twice the green
			const float u = 0, v = 0;
			(void)u;
			(void)v;
			int reddest = 0, greenest = 0;
			for (int y = 1; y < light.secondary.raw.height / 2; y++)
			{
				for (int x = 0; x < light.secondary.raw.width; x++)
				{
					reddest = std::max<int>(reddest, light.texel(0, x, y)[2]);
					greenest = std::max<int>(greenest, light.texel(0, x, y)[1]);
				}
			}
			// square roots: sqrt( 0.4 .. ) = 161 and up, sqrt( 0.2 .. ) = 114 and up
			CHECK(reddest >= 161 && reddest < 215 && greenest >= 114 && greenest < 152);
		}
	}

	// ---- light: a light entity with PRIMARY_SPOT is a primary light of the map
	{
		CHECK(write_light_map(out / "maps", "spot_light", "", "{\n\"classname\" \"light\"\n\"origin\" \"-300 300 230\"\n\"spawnflags\" \"2\"\n\"radius\" \"520\"\n\"intensity\" \"1.6\"\n"
			"\"_color\" \"1 0.9 0.7\"\n\"fov_outer\" \"100\"\n\"fov_inner\" \"50\"\n\"exponent\" \"1\"\n\"angles\" \"90 0 0\"\n}\n"
			"{\n\"classname\" \"light\"\n\"origin\" \"300 -300 200\"\n\"radius\" \"400\"\n}\n"));
		const compiled c = compile(out, game, "spot_light");
		check_zone(c, game, out / "maps" / "spot_light.map");
		CHECK(c.rep.primary_lights == 3 && has_note(c.rep, "2 light entities, 1 of them primary") && has_note(c.rep, "primary light 2 ( spot at -300 300 230"));
		const zone_light light(c.file, game);
		CHECK(light.ok);
		if (light.ok)
		{
			CHECK(light.lights.size() == 3);
			if (light.lights.size() == 3)
			{
				const zone_light::com_light& spot = light.lights[2];
				CHECK(spot.type == 2 && spot.canUseShadowMap == 1 && spot.exponent == 1 && std::fabs(spot.radius - 520.0f) < 0.01f);
				CHECK(std::fabs(spot.color[0] - 1.6f) < 0.01f && std::fabs(spot.color[2] - 1.12f) < 0.01f);
				CHECK(std::fabs(spot.dir[2] - 1.0f) < 0.01f);			// it shines down; the record holds the way back
				CHECK(std::fabs(spot.cosHalfFovOuter - 0.6428f) < 0.001f && std::fabs(spot.cosHalfFovInner - 0.9063f) < 0.001f && spot.cosHalfFovExpanded == spot.cosHalfFovOuter);
				CHECK(spot.rotationLimit == 1.0f && spot.defName && light.z.c_string(spot.defName) == std::string_view("light_point_linear"));
			}
			CHECK(light.z.find_asset(c1::ASSET_TYPE_LIGHT_DEF, "light_point_linear") != 0);
			const c1::GfxWorld& h = light.world.raw;
			CHECK(h.primaryLightCount == 3 && h.lastSunPrimaryLightIndex == 1);
			// surfaces name it, and they are its shadow geometry
			std::set<std::uint16_t> named;
			for (std::size_t si = 0; si < light.world.surfaces.size(); si++)
			{
				if (light.world.surfaces[si].raw.primaryLightIndex == 2)
				{
					named.insert(static_cast<std::uint16_t>(si));
				}
			}
			CHECK(!named.empty());
			std::uint32_t geometry_at = 0, region_at = 0;
			std::memcpy(&geometry_at, h._unk416 + 32, 4);
			std::memcpy(&region_at, h._unk416 + 36, 4);
			CHECK(geometry_at && region_at);
			if (geometry_at && region_at)
			{
				const std::vector<std::uint32_t> geometry = light.z.get_array<std::uint32_t>(geometry_at, 9), regions = light.z.get_array<std::uint32_t>(region_at, 6);
				const std::uint16_t surface_count = static_cast<std::uint16_t>(geometry[6] & 0xFFFF);
				CHECK(surface_count == named.size() && geometry[7] != 0);
				if (surface_count && geometry[7])
				{
					const std::vector<std::uint16_t> list = light.z.get_array<std::uint16_t>(geometry[7], surface_count);
					CHECK(std::set<std::uint16_t>(list.begin(), list.end()) == named && std::is_sorted(list.begin(), list.end()));
				}
				CHECK((geometry[0] & 0xFFFF) == 0 && (geometry[3] & 0xFFFF) == 0);		// no surfaces for "none" and the sun, as in the shipped maps
				CHECK(regions[0] == 0 && regions[2] == 0 && regions[4] == 1 && regions[5] != 0);
				if (regions[4] == 1 && regions[5])
				{
					// one hull: nine ranges around the light. The floor under the light is inside, the far corner of the room is not
					const std::vector<float> hull = light.z.get_array<float>(regions[5], 18);
					const auto inside = [&](const float x, const float y, const float zz)
					{
						const float v[9] = { x, y, zz, x + y, x - y, x + zz, x - zz, y + zz, y - zz };
						for (int a = 0; a < 9; a++)
						{
							if (std::fabs(v[a] - hull[static_cast<std::size_t>(a)]) > hull[9 + static_cast<std::size_t>(a)])
							{
								return false;
							}
						}
						return true;
					};
					CHECK(inside(0, 0, -200) && inside(100, -80, -220) && inside(0, 0, 0));
					CHECK(!inside(700, -700, -200) && !inside(0, 0, 200));
				}
			}
			// its light is not in the secondary lightmap ( the shader adds it ) but its visibility is in the primary one
			CHECK(std::count(light.primary.pixels.begin(), light.primary.pixels.end(), std::uint8_t{ 255 }) > 100);
			const float under_spot = light.light_on_floor(-300, 300), under_baked = light.light_on_floor(300, -300);
			CHECK(under_baked > 0.4f && under_spot > 0.1f && under_spot < under_baked * 0.6f);
			// light grid points name it
			std::uint32_t entry_count = 0, entries_at = 0;
			std::memcpy(&entry_count, h.lightGrid + 40, 4);
			std::memcpy(&entries_at, h.lightGrid + 44, 4);
			int grid_points = 0;
			for (const std::uint32_t e : light.z.get_array<std::uint32_t>(entries_at, entry_count))
			{
				grid_points += ((e >> 16) & 0xFF) == 2;
			}
			CHECK(grid_points > 20);
		}
		// the same light with PRIMARY_OMNI
		CHECK(write_light_map(out / "maps", "omni_light", "", "{\n\"classname\" \"light\"\n\"origin\" \"0 0 200\"\n\"spawnflags\" \"1\"\n}\n"));
		const compiled omni = compile(out, game, "omni_light");
		check_zone(omni, game, out / "maps" / "omni_light.map");
		const zone_light omni_light(omni.file, game);
		CHECK(omni_light.ok && omni_light.lights.size() == 3);
		if (omni_light.ok && omni_light.lights.size() == 3)
		{
			CHECK(omni_light.lights[2].type == 3 && omni_light.lights[2].exponent == 0 && std::fabs(omni_light.lights[2].radius - 600.0f) < 0.01f);
			CHECK(std::fabs(omni_light.lights[2].cosHalfFovOuter - 0.5f) < 0.001f && omni_light.lights[2].canUseShadowMap == 1);
		}
	}

	// ---- vision file and sun flare: a mapper's files next to the .map, settings by name; the default flare of a map with sun and sky
	{
		{
			std::ofstream vision(out / "maps" / "sky_room.vision", std::ios::trunc);
			vision << "// a comment\nr_glow \"0\"\nr_filmContrast 1.5\n522 \"1.25\"\nr_notASetting \"1\"\n";
		}
		const compiled c = compile(out, game, "sky_room");
		std::error_code remove_error;
		fs::remove(out / "maps" / "sky_room.vision", remove_error);
		CHECK(c.ok && has_note(c.rep, "vision/mp_sky_room.vision with 3 settings") && has_note(c.rep, "sun: flare from the compiler's default"));
		CHECK(std::any_of(c.rep.warnings.begin(), c.rep.warnings.end(), [](const std::string& w) { return w.find("r_notASetting") != std::string::npos; }));
		const zone_light light(c.file, game);
		CHECK(light.ok);
		if (light.ok)
		{
			CHECK(light.rawfile("vision/mp_sky_room.vision") == "511 \"0\"\n522 \"1.5\"\n522 \"1.25\"\n");
			CHECK(light.rawfile("sun/mp_sky_room.sun").rfind("1704 \"sun\"\n1705 \"16\"\n1706 \"sun_flare\"\n", 0) == 0);
			// the sun block: valid, both materials, the sprite's size, the direction towards the sun of the ComWorld
			const std::uint8_t* sun = light.world.raw.sun;
			std::uint32_t sprite = 0, flare = 0;
			float size = 0, direction[3] = {}, max_dot = 0;
			std::memcpy(&sprite, sun + 4, 4);
			std::memcpy(&flare, sun + 8, 4);
			std::memcpy(&size, sun + 12, 4);
			std::memcpy(&max_dot, sun + 28, 4);
			std::memcpy(direction, sun + 84, 12);
			CHECK(sun[0] == 1 && sprite != 0 && flare != 0 && sprite != flare && size == 16.0f && std::fabs(max_dot - 0.99939f) < 0.0001f);
			CHECK(light.lights.size() >= 2 && std::fabs(direction[0] - light.lights[1].dir[0]) < 0.001f && std::fabs(direction[2] - light.lights[1].dir[2]) < 0.001f);
		}
	}

	// ---- a larger map
	{
		const compiled c = compile(out, game, "big");
		check_zone(c, game, out / "maps" / "big.map");
		CHECK(c.rep.clip_brushes > 1000 && !c.rep.leaked);
		// 12 x 12 rooms, 2 x 12 x 11 doorways: every room is a cell, every doorway has one portal
		CHECK(c.rep.cells == 144 && c.rep.portals == 2 * 264);
		std::printf("big: %zu brushes, %zu surfaces, %zu cells, %zu portals, %.1f s\n", c.rep.clip_brushes, c.rep.surfaces, c.rep.cells, c.rep.portals, c.rep.seconds);
	}

	std::printf("%d checks, %d failed\n", checks, failed);
	return failed ? 1 : 0;
}
