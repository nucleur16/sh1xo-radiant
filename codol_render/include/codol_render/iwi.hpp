#pragma once

// IWI image files ( images/<name>.iwi inside the iw_image_NN.iwd archives ): header and surface layout.
// Port of Codol.Assets\Textures\IwiFile.cs. Version 9 ( 1.1.1.13 ) and version 8 ( IW4, same header and data ).

#include "build_1_1_1_13.hpp"
#include "common.hpp"

#include <array>
#include <vector>

namespace codol::render
{
	// IwiFile.cs:6-21
	enum class iwi_format : std::uint8_t
	{
		rgba32 = 1,						// stored B G R A
		rgb24 = 2,						// stored B G R
		luminance_alpha8 = 3,			// stored L A
		luminance8 = 4,
		alpha8 = 5,
		wavelet_rgba = 6,
		wavelet_rgb = 7,
		wavelet_luminance_alpha = 8,
		wavelet_luminance = 9,
		wavelet_alpha = 10,
		dxt1 = 11,
		dxt3 = 12,
		dxt5 = 13,
	};

	// IwiFile.cs:23-46
	namespace iwi_flags
	{
		inline constexpr std::uint32_t NO_PICMIP = 0x1;
		inline constexpr std::uint32_t NO_MIPMAPS = 0x2;
		inline constexpr std::uint32_t STREAMING = 0x4;
		inline constexpr std::uint32_t LEGACY_NORMALS = 0x8;
		inline constexpr std::uint32_t CLAMP_U = 0x10;
		inline constexpr std::uint32_t CLAMP_V = 0x20;
		inline constexpr std::uint32_t ALPHA_WEIGHTED_COLORS = 0x40;
		inline constexpr std::uint32_t DXTC_APPROX_WEIGHTS = 0x80;
		inline constexpr std::uint32_t GAMMA_SRGB = 0x100;
		inline constexpr std::uint32_t GAMMA_PWL = 0x200;
		inline constexpr std::uint32_t MAPTYPE_CUBE = 0x10000;
		inline constexpr std::uint32_t MAPTYPE_3D = 0x20000;
		inline constexpr std::uint32_t MAPTYPE_MASK = 0x30000;
		inline constexpr std::uint32_t NORMAL_MAP = 0x40000;
		inline constexpr std::uint32_t INTENSITY_TO_ALPHA = 0x80000;
		inline constexpr std::uint32_t DYNAMIC = 0x1000000;
		inline constexpr std::uint32_t RENDER_TARGET = 0x2000000;
		inline constexpr std::uint32_t SYSTEM_MEMORY = 0x4000000;
	}

	// IwiFile.cs:146 ( Header ): bytes 0-2 "IWi", 3 version, 4 flags u32, 8 format, 9 unused, 10 width s16,
	// 12 height s16, 14 depth s16, 16 fileSizeForPicmip s32[4]; pixel data from byte 32.
	struct iwi_header
	{
		std::uint8_t version = 0;
		std::uint32_t flags = 0;
		iwi_format format = iwi_format::rgba32;
		int width = 0;
		int height = 0;
		int depth = 0;
		std::array<std::int32_t, 4> file_size_for_picmip{};

		bool is_cube() const { return (flags & iwi_flags::MAPTYPE_MASK) == iwi_flags::MAPTYPE_CUBE; }
		bool is_volume() const { return (flags & iwi_flags::MAPTYPE_MASK) == iwi_flags::MAPTYPE_3D; }
		bool is_normal_map() const { return (flags & iwi_flags::NORMAL_MAP) != 0; }
		bool is_wavelet() const { return format >= iwi_format::wavelet_rgba && format <= iwi_format::wavelet_alpha; }
		int face_count() const { return is_cube() ? 6 : 1; }
		int mip_count() const;		// IwiFile.cs:98-110
	};

	// IwiFile.cs:48: one stored surface of a non wavelet file
	struct iwi_surface
	{
		int level = 0;			// 0 = full size
		int face = 0;			// cube face in D3DCUBEMAP_FACES order: +x -x +y -y +z -z
		int width = 0;
		int height = 0;
		std::uint32_t offset = 0;	// into the file
		std::uint32_t size = 0;
	};

	struct iwi_file
	{
		iwi_header header;
		byte_span data;							// the whole file; the caller keeps it alive
		std::vector<iwi_surface> surfaces;		// file order: smallest level first, faces within a level. Empty for wavelet files.

		const iwi_surface* surface(int level, int face) const;		// IwiFile.cs:223-225
	};

	const char* iwi_format_name(iwi_format format);					// IwiFile.cs:114-130
	int iwi_mip_level_count(std::uint32_t flags, int width, int height, int depth);		// IwiFile.cs:98-110
	inline int iwi_level_dimension(const int size, const int level) { return size >> level > 1 ? size >> level : 1; }	// IwiFile.cs:112
	// IwiFile.cs:132-144: bytes of one surface, -1 for the wavelet formats
	std::int64_t iwi_surface_size(iwi_format format, int width, int height);

	// IwiFile.cs:148-168 ( ParseHeader ): magic and length only
	bool parse_iwi_header(byte_span data, iwi_header& out, std::string* error = nullptr);

	// IwiFile.cs:170-221 ( Parse ): version 8 / 9, known format, no volume textures; for the plain formats the
	// surface table, which must cover the file exactly.
	bool parse_iwi(byte_span data, iwi_file& out, std::string* error = nullptr);
}
