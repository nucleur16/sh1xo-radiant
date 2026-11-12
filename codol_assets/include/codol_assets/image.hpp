#pragma once

// Source image -> native 1.1.1.13 image: the GfxImage asset of a zone and, for a streamed image, the IWI version 9
// file the game reads from images/<name>.iwi.
//
// What the shipped images of each use look like ( survey of all 65281 images of the install against the 21695 .iwi
// files of the image archives, see README ):
//
//   use        semantic  IWI format                       IWI flags
//   colour     2         DXT1 ( 76 % ), DXT5, DXT3        0x340  alpha weighted colours | sRGB | PWL gamma
//   normal     5         DXT5 ( 99.9 % )                  0x40000 normal map; x in alpha, y in green
//   specular   8         DXT5 ( 99.9 % )                  0x340; rgb specular colour, alpha gloss
//   detail     3         DXT1                             0x340
//   2D / HUD   0         RGBA32, DXT5, L8A8, RGB24        0x173  no picmip | no mips | clamp u, v | 0x40 | sRGB
//
// The wavelet formats ( 6 - 10 ) are used by 55 files; they are not written. The compiler writes DXT1 / DXT3 / DXT5
// with its own encoder and the plain formats.
//
// An image is either *streamed* ( the zone holds the header only, the pixels are images/<name>.iwi in an .iwd; all
// shipped material textures are like this ) or *inline* ( the pixels are in the zone, as a GfxImageLoadDef in a
// D3DFORMAT; shipped: lightmaps, reflection probes, $white .., and the pad icons of codol_common_postgfx_mp ).

#include "codol_assets/bitmap.hpp"
#include "codol_zone/zone_editor.hpp"

#include <array>

namespace codol::assets
{
	enum class ImageUse
	{
		color,			// semantic 2
		normal,			// semantic 5: tangent space normal map, source as usual ( x in red, y in green, z in blue )
		specular,		// semantic 8: rgb specular colour, alpha gloss ( cosine power )
		detail,			// semantic 3
		ui,				// semantic 0: HUD / menu image, no mips, clamped
		function,		// semantic 1: data ( falloff, distortion .. ), linear, no sRGB
	};

	enum class ImageFormat
	{
		automatic,		// what shipped images of the use have: see the table above
		dxt1,
		dxt3,
		dxt5,
		rgba32,			// IWI format 1 / D3DFMT_A8R8G8B8
		rgb24,			// IWI format 2; inline it is stored as D3DFMT_X8R8G8B8
		luminance_alpha8,
		luminance8,
		alpha8,
	};

	struct ImageOptions
	{
		ImageUse use = ImageUse::color;
		ImageFormat format = ImageFormat::automatic;
		bool streamed = false;			// header only; write_iwi() gives the file that has to be in an .iwd
		int mips = -1;					// -1: by use ( ui: no ), 0: none, 1: full chain
		int noPicmip = -1;				// -1: by use ( ui: yes )
		bool clampU = false;
		bool clampV = false;
		bool flipNormalY = false;		// source normal map has green pointing down
	};

	struct CompiledImage
	{
		std::string name;
		std::uint8_t mapType = 3;			// 3: 2D, 5: cube
		std::uint8_t semantic = 0;
		std::uint16_t width = 0;
		std::uint16_t height = 0;
		std::uint8_t iwiFormat = 0;			// codol::render::iwi_format
		std::uint32_t d3dFormat = 0;		// D3DFORMAT of the same data
		std::uint32_t flags = 0;			// IWI flag bits ( also GfxImageLoadDef::flags )
		bool streamed = false;
		int faces = 1;
		int levelCount = 1;
		// surfaces[ level * faces + face ], level 0 = full size, in the IWI byte order of the format
		std::vector<std::vector<std::uint8_t>> surfaces;
		// the uncompressed level the surface was made from ( for quality reports )
		std::vector<Bitmap> reference;

		std::uint32_t data_bytes(int firstLevel = 0) const;
	};

	Status compile_image(std::string_view name, const SourceImage& source, const ImageOptions& options, CompiledImage& out);

	// images/<name>.iwi, version 9
	std::vector<std::uint8_t> write_iwi(const CompiledImage& image);

	// The GfxImage as the game's loaders read it: header, name, GfxImageLoadDef ( and the pixels when inline ).
	std::vector<zone::ZoneNewPiece> image_pieces(const CompiledImage& image);

	// A zip with the given files, every entry deflated ( the game cannot seek in a stored entry: a streamed sound
	// in one ends the client ). Without a key a plain one: the game only decrypts archives whose name
	// contains iw_image, iw_model, iw_config, iw_sound, iw_ui_image or iw_adlc - a plain archive needs any other name.
	// With `key` ( codol::render::derive_iwd_key, 24 bytes ) the entry names and the file data are encrypted the way
	// the game's own archives are ( AES-192 in counter mode, seeded with the CRC of the archive's FILE NAME, which
	// therefore must not change afterwards ): for an archive with one of those names. iwd_needs_key says which it is.
	Status write_iwd(const std::filesystem::path& path, std::span<const std::pair<std::string, std::vector<std::uint8_t>>> files,
		const std::array<std::uint8_t, 24>* key = nullptr);
	bool iwd_needs_key(const std::filesystem::path& path);
	// Whether the game opens an archive of that name in main\ at all ( C1.exe 0x6166E0 ): iw_ + nothing or one of
	// imagestripped_ / image_ / sound_ / model_ / config_ + two digits up to 23 + .iwd. Anything else in main\ is
	// never looked at ( localized_<language>_iw_.. follow the same rule with a limit per language: not judged here ).
	bool game_mounts_iwd(const std::filesystem::path& path);
	// The first free name iw_sound_NN.iwd of a main\ folder, from 23 down to 4 ( 00 .. 03 are the game's ); empty
	// when all are taken.
	std::filesystem::path free_sound_iwd(const std::filesystem::path& main_folder);

	// An IWI file read back through codol_render ( the reader of the renderer ): every surface as RGBA.
	struct IwiLevels
	{
		std::uint32_t flags = 0;
		std::uint8_t format = 0;
		int width = 0;
		int height = 0;
		int faces = 1;
		int levelCount = 0;
		std::array<std::int32_t, 4> fileSizeForPicmip{};
		std::vector<Bitmap> levels;			// [ level * faces + face ], level 0 = full size
	};
	// exe: C1.exe, only needed for the wavelet formats ( their Huffman tables are in the executable )
	Status read_iwi_levels(std::span<const std::uint8_t> file, const std::filesystem::path& exe, IwiLevels& out);

	// ---- pieces of the compiler, exposed for tests and tools --------------------------------------------------
	// next mip level: 2 x 2 box filter. srgb: colour is averaged in linear light; normal: xyz is averaged as a
	// vector and renormalised.
	Bitmap downsample(const Bitmap& image, bool srgb, bool normal);
	// width * height pixels ( RGBA ) -> DXT blocks; format: dxt1 ( punch through alpha when a pixel has alpha < 128 ),
	// dxt3, dxt5
	std::vector<std::uint8_t> compress_dxt(const Bitmap& image, ImageFormat format);
}
