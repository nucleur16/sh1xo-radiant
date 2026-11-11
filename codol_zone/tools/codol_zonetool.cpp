// temporary bring-up tool, replaced by the full CLI
#include "codol_zone/zone.hpp"

#include <chrono>
#include <cstdio>
#include <fstream>

using namespace codol::zone;

int main(int argc, char** argv)
{
	if (argc < 4)
	{
		return 2;
	}
	const std::string cmd = argv[1];
	ClientKey key;
	if (const Status s = key.load_from_exe(argv[2]); !s)
	{
		std::printf("key: %s\n", s.message.c_str());
		return 1;
	}
	if (cmd == "decrypt")
	{
		std::vector<std::uint8_t> zone;
		FastFileInfo info;
		if (const Status s = read_fastfile(argv[3], key, zone, &info); !s)
		{
			std::printf("%s\n", s.message.c_str());
			return 1;
		}
		std::ofstream(argv[4], std::ios::binary).write(reinterpret_cast<const char*>(zone.data()), static_cast<std::streamsize>(zone.size()));
		std::printf("%s: %zu bytes, %u chunks\n", info.authName.c_str(), zone.size(), info.dataChunks);
		return 0;
	}
	if (cmd == "check")
	{
		int failures = 0, count = 0;
		for (const auto& e : std::filesystem::directory_iterator(argv[3]))
		{
			if (e.path().extension() != ".ff")
			{
				continue;
			}
			count++;
			const auto t0 = std::chrono::steady_clock::now();
			Zone zone;
			const Status s = zone.open(e.path(), key);
			const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
			if (!s)
			{
				failures++;
				std::printf("FAIL %s\n", s.message.c_str());
			}
			else
			{
				std::printf("ok   %-48s %6zu assets %7zu total %6lld ms\n", zone.name().c_str(), zone.assets().size(), zone.all_assets().size(),
					static_cast<long long>(ms));
			}
			std::fflush(stdout);
		}
		std::printf("%d zones, %d failures\n", count, failures);
		return failures ? 1 : 0;
	}
	return 2;
}
