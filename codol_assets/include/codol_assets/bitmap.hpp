#pragma once

// Source images: PNG, TGA and DDS files -> 8 bit RGBA, and PNG out ( for previews and tests ).
//
// Everything is decoded by this library's own code ( the inflater is codol_zone's ): PNG 8 / 16 bit, grey, RGB,
// palette, with or without alpha, not interlaced; TGA true colour / grey / colour mapped, plain or run length
// coded; DDS uncompressed ( 32 / 24 bit masks, L8, A8L8 ), DXT1 / DXT3 / DXT5, 2D or cube. Of a DDS with mip levels
// only the top level is used: the compiler makes its own.

#include "codol_zone/fastfile.hpp"

#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

namespace codol::assets
{
	using codol::zone::Status;

	// 8 bit R G B A, top row first
	struct Bitmap
	{
		int width = 0;
		int height = 0;
		std::vector<std::uint8_t> rgba;		// width * height * 4

		void resize(const int w, const int h)
		{
			width = w;
			height = h;
			rgba.assign(static_cast<std::size_t>(w) * static_cast<std::size_t>(h) * 4, 0);
		}
		std::uint8_t* at(const int x, const int y) { return rgba.data() + (static_cast<std::size_t>(y) * width + x) * 4; }
		const std::uint8_t* at(const int x, const int y) const { return rgba.data() + (static_cast<std::size_t>(y) * width + x) * 4; }
	};

	struct SourceImage
	{
		std::vector<Bitmap> faces;			// 1, or 6 for a cube map ( +x -x +y -y +z -z ), all of one size
		bool hasAlpha = false;				// the file has an alpha channel ( its content may still be opaque )
		bool is_cube() const { return faces.size() == 6; }
	};

	Status decode_png(std::span<const std::uint8_t> file, Bitmap& out, bool* hasAlpha = nullptr);
	Status decode_tga(std::span<const std::uint8_t> file, Bitmap& out, bool* hasAlpha = nullptr);
	Status decode_dds(std::span<const std::uint8_t> file, SourceImage& out);

	// by content ( PNG signature, "DDS " ), else TGA
	Status decode_image(std::span<const std::uint8_t> file, SourceImage& out);
	Status load_image(const std::filesystem::path& path, SourceImage& out);

	// 8 bit RGBA PNG
	Status encode_png(const Bitmap& image, std::vector<std::uint8_t>& file);
	Status save_png(const std::filesystem::path& path, const Bitmap& image);

	// true when any pixel has alpha below 255
	bool uses_alpha(const Bitmap& image);

	// 10 * log10( 255^2 / mse ) over the given channels ( mask bit 0 = R .. bit 3 = A ); 99 for identical images,
	// -1 when the sizes differ
	double psnr(const Bitmap& a, const Bitmap& b, unsigned channelMask = 0xF);
}
