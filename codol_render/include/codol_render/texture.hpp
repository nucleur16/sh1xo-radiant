#pragma once

// Pixels: IWI surfaces -> RGBA8 ( for the CPU side: picking, thumbnails ) and -> what is uploaded to Direct3D.
// Port of Codol.Assets\Textures\TextureDecoder.cs, WaveletDecoder.cs, the IWI / embedded image paths of
// TextureExtractor.cs and the upload decision of Codol.Rendering\GameTextureCache.cs.

#include "d3d9_constants.hpp"
#include "iwi.hpp"
#include "material.hpp"

#include <filesystem>
#include <memory>
#include <optional>
#include <vector>

namespace codol::render
{
	// TextureDecoder.cs:5-24: 8 bit R G B A, top row first
	struct rgba_image
	{
		int width = 0;
		int height = 0;
		std::vector<std::uint8_t> pixels;		// width * height * 4
	};

	// TextureDecoder.cs:54-122 ( Decode ): one surface of a plain ( non wavelet ) format, or decoded wavelet data
	// under its plain equivalent. DXT blocks are decoded exactly as the C# source does ( and as the oracle PNGs are ).
	bool decode_surface(iwi_format format, byte_span data, int width, int height, rgba_image& out, std::string* error = nullptr);

	// TextureDecoder.cs:124-134 ( FromD3DFormat ); nullopt for formats decode_surface cannot read.
	// X8R8G8B8 is not in that table: the callers decode it as rgba32 and set alpha to 255.
	std::optional<iwi_format> iwi_format_from_d3d(std::uint32_t d3d_format);

	// ---------------------------------------------------------------------------------------------------
	// wavelet coded IWI ( formats 6 - 10 )

	// WaveletDecoder.cs:7-155: the three Huffman tables of the client executable, 4096 entries of
	// { s16 value, u16 bit length } each. Immutable once loaded; share one instance.
	class wavelet_tables
	{
	public:
		static constexpr int ENTRIES = 4096;

		// Reads the tables from C1.exe at the 1.1.1.13 file offsets ( build::EXE_WAVELET_* ). Fails when the file
		// is too short or an entry has a bit length outside 1 .. 12 ( not the 1.1.1.13 executable ).
		static std::shared_ptr<const wavelet_tables> load_from_executable(const std::filesystem::path& exe, std::string* error = nullptr);
		// From three in-memory tables of 4096 * 4 bytes each ( tests, or a caller that already mapped the exe )
		static std::shared_ptr<const wavelet_tables> from_memory(byte_span primary, byte_span secondary, byte_span alpha, std::string* error = nullptr);

		const std::int16_t* primary() const { return primary_.data(); }
		const std::int16_t* secondary() const { return secondary_.data(); }
		const std::int16_t* alpha() const { return alpha_.data(); }

	private:
		std::vector<std::int16_t> primary_, secondary_, alpha_;		// 2 * ENTRIES
	};

	// WaveletDecoder.cs:167 ( DecodedLevel ): bytes per pixel 4 ( B G R A; alpha 255 for the RGB format ),
	// 2 ( L A ) or 1 ( L / A )
	struct wavelet_level
	{
		int face = 0;
		int level = 0;
		int width = 0;
		int height = 0;
		std::vector<std::uint8_t> data;
	};

	// WaveletDecoder.cs:169-232 ( Decode ): every level of every face, per face from the smallest level up
	bool decode_wavelet(const iwi_file& file, const wavelet_tables& tables, std::vector<wavelet_level>& out, std::string* error = nullptr);

	// ---------------------------------------------------------------------------------------------------
	// upload

	// One IDirect3DTexture9 / IDirect3DCubeTexture9 level, ready for LockRect + row copies
	struct texture_surface
	{
		int level = 0;
		int face = 0;				// D3DCUBEMAP_FACES
		int width = 0;
		int height = 0;
		int rows = 0;				// rows to copy ( block rows for DXT )
		int row_bytes = 0;			// bytes per row ( copy row by row, the locked pitch can be larger )
		std::vector<std::uint8_t> data;		// rows * row_bytes
	};

	// What GameTextureCache.Create makes of a loaded image
	struct texture_upload
	{
		std::uint32_t d3d_format = 0;	// D3DFORMAT of every surface
		int width = 0;					// level 0
		int height = 0;
		int levels = 0;
		int faces = 1;					// 6: create a cube texture of edge `width`
		std::vector<texture_surface> surfaces;		// faces x levels, face major, level 0 first

		bool is_cube() const { return faces == 6; }
		const texture_surface* surface(int level, int face) const;
	};

	// TextureExtractor.cs:207-290 ( FromIwi / RawFromIwi ) + GameTextureCache.cs:85-114, 158-165:
	//   DXT1 / DXT3 / DXT5, A8L8, L8, A8 and A8R8G8B8 files go to the GPU as stored;
	//   R8G8B8 is expanded to X8R8G8B8;
	//   wavelet files are decoded on the CPU: RGBA -> A8R8G8B8, RGB -> X8R8G8B8, LA -> A8L8, L -> L8, A -> A8;
	//   6 faces -> cube texture; every mip level of the file is uploaded.
	// `tables` is needed for the wavelet formats only.
	bool build_texture_upload(const iwi_file& file, const wavelet_tables* tables, texture_upload& out, std::string* error = nullptr);

	// TextureExtractor.cs:292-389 ( FromEmbedded / EmbeddedLayout ): an image whose pixels are in the zone
	// ( GfxImageLoadDef ). `data` is zone.read( image.data, image.data_size ). The data is uploaded as stored in
	// image.d3d_format; cube when map_type is MAPTYPE_CUBE. Fails when the size matches no level layout.
	bool build_embedded_upload(const embedded_image& image, byte_span data, texture_upload& out, std::string* error = nullptr);

	// The RGBA8 view of one surface of an upload ( any of the formats above ), for the CPU side.
	bool upload_surface_to_rgba(const texture_upload& upload, const texture_surface& surface, rgba_image& out, std::string* error = nullptr);

	// TextureExtractor.cs:123: "images/<name>.iwi", the path of a streamed image inside the IWD archives
	std::string iwi_game_path(std::string_view image_name);
}
