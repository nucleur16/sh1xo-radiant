#pragma once

// shared by the parts of codol_asset_verify

#include <cstdint>
#include <filesystem>
#include <vector>

struct Rate
{
	std::uint32_t equal = 0, total = 0;
	void add(const bool same)
	{
		equal += same ? 1 : 0;
		total++;
	}
};

void print_rate(const char* name, const Rate& r);

// the fast files of <install>/client_shipRetail/chinese and /patch, sorted
std::vector<std::filesystem::path> install_zones(const std::filesystem::path& install);

int material_roundtrip(const std::filesystem::path& install, const std::filesystem::path& exe, const std::filesystem::path& cache, int every,
	const std::filesystem::path& dump);
