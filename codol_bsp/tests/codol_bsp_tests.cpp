// codol_bsp_tests: writes the test maps, compiles them and checks the result.
//
//   codol_bsp_tests --write-maps <dir>          only writes the test maps ( .map files )
//   codol_bsp_tests [--out <dir>] [--game <install>]
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

	compiled compile(const fs::path& out, const fs::path& game, const char* map, const bool light = true)
	{
		compiled c;
		bsp::options opts;
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
		CHECK(c.rep.cells == 5 && c.rep.portals == 8 && !c.rep.leaked);
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
	}

	// ---- a larger map
	{
		const compiled c = compile(out, game, "big");
		check_zone(c, game, out / "maps" / "big.map");
		CHECK(c.rep.clip_brushes > 1000 && c.rep.cells > 20 && !c.rep.leaked);
		std::printf("big: %zu brushes, %zu surfaces, %zu cells, %zu portals, %.1f s\n", c.rep.clip_brushes, c.rep.surfaces, c.rep.cells, c.rep.portals, c.rep.seconds);
	}

	std::printf("%d checks, %d failed\n", checks, failed);
	return failed ? 1 : 0;
}
