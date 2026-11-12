// codol_bsp_tests: writes the test maps, compiles them and checks the result.
//
//   codol_bsp_tests --write-maps <dir>          only writes the test maps ( .map files )
//   codol_bsp_tests [--out <dir>] [--game <install>]
//
// Plain asserts in the style of the other test programs of this repository: every check prints when it fails, the
// program returns 1 when one did.

#include "test_maps.hpp"

#include "codol_bsp/bsp.hpp"

#include <cstdio>
#include <cstring>
#include <filesystem>

namespace
{
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

	bool write_maps(const std::filesystem::path& dir)
	{
		std::error_code ec;
		std::filesystem::create_directories(dir, ec);
		std::string error;
		codol::edit::document doc;
		codol::bsp::test::make_box_room(doc);
		if (!doc.save_map_file(dir / "box_room.map", error))
		{
			std::printf("%s\n", error.c_str());
			return false;
		}
		return true;
	}
}

int main(const int argc, char** argv)
{
	std::filesystem::path out = "codol_bsp_test_out", game = CODOL_BSP_GAME_DIR;
	for (int i = 1; i < argc; i++)
	{
		const std::string arg = argv[i];
		if (arg == "--write-maps" && i + 1 < argc)
		{
			return write_maps(argv[i + 1]) ? 0 : 1;
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
	std::printf("%d checks, %d failed\n", checks, failed);
	return failed ? 1 : 0;
}
