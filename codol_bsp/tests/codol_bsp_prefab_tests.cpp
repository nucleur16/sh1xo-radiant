// codol_bsp_prefab_tests: a map with misc_prefab entities ( one of them turned, one nested, one missing ) and brushes
// with "contents weaponClip;" / "contents nonColliding;" is compiled; the clip map of the result is traced.
//
//   codol_bsp_prefab_tests [--out <dir>] [--game <install>]
//
// Needs the 1.1.1.13 install ( materials come from its zones ); without it only the maps are written and read back.

#include "test_maps.hpp"

#include "codol_bsp/bsp.hpp"
#include "codol_mapc/trace.hpp"
#include "codol_zone/zone_editor.hpp"

#include <cstdio>
#include <filesystem>

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

	using pairs = std::vector<std::pair<std::string, std::string>>;

	bool has_line(const std::vector<std::string>& lines, const char* text)
	{
		for (const std::string& n : lines)
		{
			if (n.find(text) != std::string::npos)
			{
				return true;
			}
		}
		return false;
	}

	bool write_maps(const fs::path& dir)
	{
		std::error_code ec;
		fs::create_directories(dir / "prefabs", ec);
		std::string error;
		const bsp::test::materials m;

		// prefabs\cap.map: a plate; prefabs\pillar.map: a pillar with the plate on top, off centre in x
		edit::document cap;
		cap.create_box({ -24, -24, 0 }, { 24, 24, 8 }, m.crate);
		edit::document pillar;
		pillar.create_box({ 0, -16, 0 }, { 32, 16, 96 }, m.crate);
		pillar.create_point_entity("misc_prefab", { 16, 0, 96 }, pairs{ { "model", "prefabs/cap.map" } });
		pillar.create_point_entity("script_origin", { 16, 0, 120 }, pairs{ { "targetname", "pillar_top" } });

		edit::document doc;
		bsp::test::make_box_room(doc, m);
		// a pillar as it is, one turned by 90 degrees ( its + x half ends at + y ), one whose file does not exist
		doc.create_point_entity("misc_prefab", { 0, 300, 0 }, pairs{ { "model", "prefabs/pillar.map" } });
		doc.create_point_entity("misc_prefab", { -300, -300, 0 }, pairs{ { "model", "prefabs/pillar.map" }, { "angles", "0 90 0" } });
		doc.create_point_entity("misc_prefab", { 0, 0, 0 }, pairs{ { "model", "prefabs/missing.map" } });
		// stops bullets, not players
		doc.create_box({ 300, -100, 0 }, { 364, -36, 64 }, m.crate);
		doc.set_content_flag("weaponClip", true);
		// seen, never touched
		doc.create_box({ 300, 100, 0 }, { 364, 164, 64 }, m.crate);
		doc.set_content_flag("nonColliding", true);
		doc.deselect_all();

		return cap.save_map_file(dir / "prefabs" / "cap.map", error) && pillar.save_map_file(dir / "prefabs" / "pillar.map", error)
			&& doc.save_map_file(dir / "prefab_room.map", error);
	}
}

int main(const int argc, char** argv)
{
	fs::path out = "codol_bsp_prefab_test_out", game = CODOL_BSP_GAME_DIR;
	for (int i = 1; i < argc; i++)
	{
		const std::string arg = argv[i];
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

	// ---- the document side: what the compiler sees after the prefabs are expanded
	{
		edit::document doc;
		std::string error;
		CHECK(doc.load_map_file(out / "maps" / "prefab_room.map", error));
		const std::size_t brushes_before = doc.brush_count();
		edit::prefab_result result;
		CHECK(edit::expand_prefabs_from_disk(doc, out / "maps" / "prefab_room.map", result));
		CHECK(result.expanded == 4);		// two pillars, the plate in each
		CHECK(result.warnings.size() == 1 && has_line(result.warnings, "missing.map"));
		CHECK(doc.brush_count() == brushes_before + 4);
		CHECK(!doc.can_undo());
		std::size_t tops = 0, prefabs = 0;
		for (const edit::object_id id : doc.entities())
		{
			const edit::entity* e = doc.find_entity(id);
			tops += e->value("targetname") == "pillar_top" ? 1 : 0;
			prefabs += edit::document::is_prefab(*e) ? 1 : 0;
		}
		CHECK(tops == 2 && prefabs == 1);
	}

	std::error_code ec;
	if (!fs::exists(game / "C1.exe", ec))
	{
		std::printf("no install at %s: the compile test is skipped\n%d checks, %d failed\n", game.string().c_str(), checks, failed);
		return failed ? 1 : 0;
	}

	bsp::options opts;
	opts.map = out / "maps" / "prefab_room.map";
	opts.exe = game / "C1.exe";
	opts.install = game;
	opts.out = out / "ff" / "mp_prefab_room.ff";
	opts.cache = out;
	opts.light = false;
	opts.quality = 0;
	bsp::report rep;
	std::string error;
	const bool ok = bsp::compile(opts, rep, error);
	if (!ok)
	{
		std::printf("prefab_room: %s\n", error.c_str());
	}
	CHECK(ok);
	CHECK(has_line(rep.notes, "prefabs: 4 expanded into 4 brushes"));
	CHECK(has_line(rep.warnings, "prefab: misc_prefab \"prefabs/missing.map\""));
	CHECK(!rep.leaked);

	if (ok)
	{
		zone::ClientKey key;
		key.load_from_exe(game / "C1.exe");
		zone::ZoneEditor editor;
		CHECK(static_cast<bool>(editor.open(opts.out, key)));
		const mapc::clip_trace trace(editor);
		CHECK(trace.valid());
		// where a segment straight down at ( x, y ) from z 250 ends, for brushes with any of the contents of `mask`
		const auto floor_at = [&](const float x, const float y, const int mask)
		{
			const float a[3] = { x, y, 250 }, b[3] = { x, y, -50 };
			const mapc::trace_result r = trace.trace(a, b, mask);
			return a[2] + (b[2] - a[2]) * r.fraction;
		};
		const int everything = -1, player = 0x10001 /* solid, player clip */, bullet = 0x2000 /* clip shot */;

		// the pillar at ( 0 300 ): x 0 .. 32, with the plate ( 48 x 48 x 8 ) on top around x 16
		CHECK(std::fabs(floor_at(16, 300, everything) - 104.0f) < 0.01f);
		CHECK(std::fabs(floor_at(-6, 300, everything) - 104.0f) < 0.01f);		// the plate reaches over the pillar
		CHECK(std::fabs(floor_at(-20, 300, everything) - 0.0f) < 0.01f);
		// the one at ( -300 -300 ), turned by 90 degrees: the pillar is now y -300 .. -268
		CHECK(std::fabs(floor_at(-300, -284, everything) - 104.0f) < 0.01f);
		CHECK(std::fabs(floor_at(-300, -330, everything) - 0.0f) < 0.01f);
		CHECK(std::fabs(floor_at(-270, -284, everything) - 0.0f) < 0.01f);
		// the missing prefab left nothing
		CHECK(std::fabs(floor_at(0, 0, everything) - 0.0f) < 0.01f);

		// weapon clip: bullets stop on it, a player walks through
		CHECK(std::fabs(floor_at(332, -68, bullet) - 64.0f) < 0.01f);
		CHECK(std::fabs(floor_at(332, -68, player) - 0.0f) < 0.01f);
		// a crate of the room stops both
		CHECK(std::fabs(floor_at(160, 160, player) - 64.0f) < 0.01f);
		// non colliding: nothing stops there
		CHECK(std::fabs(floor_at(332, 132, everything) - 0.0f) < 0.01f);
	}

	std::printf("%d checks, %d failed\n", checks, failed);
	return failed ? 1 : 0;
}
