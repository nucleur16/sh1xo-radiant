#pragma once

// PNG <-> 8 bit RGBA with the Windows Imaging Component ( no external library ). Tool code, not part of codol_view.

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace tools
{
	struct image
	{
		int width = 0;
		int height = 0;
		std::vector<std::uint8_t> rgba;		// width * height * 4, top row first
	};

	bool save_png(const std::filesystem::path& path, const image& img, std::string& error);
	bool load_png(const std::filesystem::path& path, image& out, std::string& error);
}
