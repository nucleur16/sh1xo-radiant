// codol_mapc_prefab_tests: a misc_prefab in the edited copy of a shipped map is compiled as what its file holds.
//
//   codol_mapc_prefab_tests [--game <install>] [--out <folder>] [--map mp_rust]
//
// The import of the shipped map gets a misc_prefab ( turned by 90 degrees ) high above a spawn point; its file,
// written next to the map as prefabs\mapc_box.map, holds one clip brush and a script_origin. After mapc::compile
// the zone has one more collision brush where the turned prefab puts it - found with the trace of
// codol_mapc/trace.hpp - and one more entity. Skipped without the install.

#include "codol_mapc/mapc.hpp"
#include "codol_mapc/trace.hpp"
#include "codol_zone/zone_editor.hpp"

#include <cmath>
#include <cstdio>
#include <filesystem>

namespace
{
	using namespace codol;

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

	// where a segment straight down at ( x, y ) from `top` to `bottom` ends
	float drop(const mapc::clip_trace& trace, const float x, const float y, const float top, const float bottom)
	{
		const float a[3] = { x, y, top }, b[3] = { x, y, bottom };
		const mapc::trace_result r = trace.trace(a, b, -1);
		return a[2] + (b[2] - a[2]) * r.fraction;
	}
}

int main(const int argc, char** argv)
{
	std::filesystem::path game = CODOL_MAPC_GAME_DIR, out = "mapc_prefab_test";
	std::string map = "mp_rust";
	for (int i = 1; i + 1 < argc; i += 2)
	{
		const std::string arg = argv[i];
		if (arg == "--game") { game = argv[i + 1]; }
		else if (arg == "--out") { out = argv[i + 1]; }
		else if (arg == "--map") { map = argv[i + 1]; }
	}

	const std::filesystem::path fastfile = game / "client_shipRetail" / "chinese" / (map + ".ff"), exe = game / "C1.exe";
	std::error_code ec;
	if (!std::filesystem::exists(exe, ec) || !std::filesystem::exists(fastfile, ec))
	{
		std::printf("skipped: no install at %s\n", game.string().c_str());
		return 0;
	}
	std::filesystem::create_directories(out / "prefabs", ec);

	edit::document doc;
	std::string error;
	CHECK(doc.import_fastfile(fastfile, exe, error));
	if (failed)
	{
		std::printf("%s\n", error.c_str());
		return 1;
	}
	const std::string baseline(doc.import_baseline_map());

	// far above the map: nothing of the shipped collision is there
	edit::vec3 at;
	for (const edit::object_id id : doc.entities())
	{
		const edit::entity* e = doc.find_entity(id);
		if (e && e->classname().find("spawn") != std::string_view::npos && e->find("origin"))
		{
			at = e->origin();
			break;
		}
	}
	at.z += 2000.0;

	// the prefab: a clip box of 96 x 32 x 64 that lies along + x, and an entity
	{
		edit::document prefab;
		prefab.create_box({ 0, -16, 0 }, { 96, 16, 64 }, "clip");
		prefab.create_point_entity("script_origin", { 0, 0, 80 }, pairs{ { "targetname", "mapc_prefab_origin" } });
		CHECK(prefab.save_map_file(out / "prefabs" / "mapc_box.map", error));
	}

	doc.create_point_entity("misc_prefab", at, pairs{ { "model", "prefabs/mapc_box.map" }, { "angles", "0 90 0" } });
	const std::filesystem::path map_file = out / (map + "_prefab.map"), baseline_file = out / (map + "_prefab.codol_base.map");
	CHECK(doc.save_map_file(map_file, error));
	{
		edit::document base;
		CHECK(base.load_map_text(baseline, error) && base.save_map_file(baseline_file, error));
	}

	mapc::options opts;
	opts.base = fastfile;
	opts.exe = exe;
	opts.map = map_file;
	opts.baseline = baseline_file;
	opts.out = out / (map + "_prefab.ff");
	mapc::report rep;
	const bool ok = mapc::compile(opts, rep, error);
	if (!ok)
	{
		std::printf("%s\n", error.c_str());
	}
	CHECK(ok);
	CHECK(has_line(rep.notes, "prefabs: 1 expanded into 1 brushes, 0 patches, 1 entities"));
	CHECK(rep.brushes_added == 1);
	CHECK(rep.entities_after == rep.entities_before + 1);		// the script_origin; the prefab itself is no entity of the map
	CHECK(!has_line(rep.warnings, "prefab:"));

	if (ok)
	{
		zone::ClientKey key;
		CHECK(static_cast<bool>(key.load_from_exe(exe)));
		zone::ZoneEditor base, compiled;
		CHECK(static_cast<bool>(base.open(fastfile, key)));
		CHECK(static_cast<bool>(compiled.open(opts.out, key)));
		const mapc::clip_trace before(base), after(compiled);
		CHECK(before.valid() && after.valid());

		const float x = static_cast<float>(at.x), y = static_cast<float>(at.y), z = static_cast<float>(at.z);
		// turned by 90 degrees the box lies along + y: x -16 .. 16, y 0 .. 96, 64 high
		CHECK(std::fabs(drop(after, x, y + 48.0f, z + 200.0f, z - 100.0f) - (z + 64.0f)) < 0.05f);
		CHECK(std::fabs(drop(after, x + 12.0f, y + 90.0f, z + 200.0f, z - 100.0f) - (z + 64.0f)) < 0.05f);
		// not where it would lie unturned, and not in the shipped zone
		CHECK(std::fabs(drop(after, x + 48.0f, y - 8.0f, z + 200.0f, z - 100.0f) - (z - 100.0f)) < 0.05f);
		CHECK(std::fabs(drop(before, x, y + 48.0f, z + 200.0f, z - 100.0f) - (z - 100.0f)) < 0.05f);
	}

	std::printf("%d checks, %d failed\n", checks, failed);
	return failed ? 1 : 0;
}
