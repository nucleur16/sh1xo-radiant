#pragma once

// shared by the parts of codol_asset_verify

#include <cstdint>
#include <filesystem>
#include <string>
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

int model_roundtrip(const std::filesystem::path& install, const std::filesystem::path& exe, int every, const std::string& filter);
int model_file(const std::filesystem::path& zoneFile, const std::filesystem::path& exe, const std::string& name, const std::vector<std::filesystem::path>& exports);

int model_extract(const std::filesystem::path& zoneFile, const std::filesystem::path& exe, const std::string& name, const std::filesystem::path& outDir);
int model_compare(const std::filesystem::path& zoneA, const std::string& nameA, const std::filesystem::path& zoneB, const std::string& nameB, const std::filesystem::path& exe);

int material_roundtrip(const std::filesystem::path& install, const std::filesystem::path& exe, const std::filesystem::path& cache, int every,
	const std::filesystem::path& dump);

// verify_sound.cpp
int sound_survey(const std::filesystem::path& install, const std::filesystem::path& exe, const std::string& filter);
int sound_roundtrip(const std::filesystem::path& install, const std::filesystem::path& exe, int every, const std::string& filter);
int sound_extract(const std::filesystem::path& zoneFile, const std::filesystem::path& exe, const std::filesystem::path& outDir, const std::string& filter);
int sound_zone(const std::filesystem::path& zoneFile, const std::filesystem::path& exe, const std::filesystem::path& outFile, int limit);

// verify_anim*.cpp
int anim_survey(const std::filesystem::path& install, const std::filesystem::path& exe, const std::string& filter);
int anim_survey_rules(const std::filesystem::path& install, const std::filesystem::path& exe, const std::string& filter);
int anim_roundtrip(const std::filesystem::path& install, const std::filesystem::path& exe, int every, const std::string& filter, float rotationTolerance, float translationTolerance);
int anim_zone(const std::filesystem::path& install, const std::filesystem::path& exe, int every, const std::filesystem::path& outFile);
int anim_model_roundtrip(const std::filesystem::path& install, const std::filesystem::path& exe, int every, float epsilon);
int anim_extract(const std::filesystem::path& zoneFile, const std::filesystem::path& exe, const std::string& name, const std::filesystem::path& outDir);
int anim_compare(const std::filesystem::path& zoneA, const std::string& nameA, const std::filesystem::path& zoneB, const std::string& nameB, const std::filesystem::path& exe,
	float rotationTolerance, float translationTolerance);
