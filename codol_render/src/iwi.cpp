#include "codol_render/iwi.hpp"

namespace codol::render
{
	const char* iwi_format_name(const iwi_format format)
	{
		switch (format)
		{
		case iwi_format::rgba32: return "RGBA8 (A8R8G8B8)";
		case iwi_format::rgb24: return "RGB8 (R8G8B8)";
		case iwi_format::luminance_alpha8: return "LA8 (A8L8)";
		case iwi_format::luminance8: return "L8";
		case iwi_format::alpha8: return "A8";
		case iwi_format::wavelet_rgba: return "Wavelet RGBA";
		case iwi_format::wavelet_rgb: return "Wavelet RGB";
		case iwi_format::wavelet_luminance_alpha: return "Wavelet LA";
		case iwi_format::wavelet_luminance: return "Wavelet L";
		case iwi_format::wavelet_alpha: return "Wavelet A";
		case iwi_format::dxt1: return "DXT1 (BC1)";
		case iwi_format::dxt3: return "DXT3 (BC2)";
		case iwi_format::dxt5: return "DXT5 (BC3)";
		default: return "unknown format";
		}
	}

	int iwi_mip_level_count(const std::uint32_t flags, const int width, const int height, const int depth)
	{
		// IwiFile.cs:98-110
		if ((flags & iwi_flags::NO_MIPMAPS) != 0)
		{
			return 1;
		}
		int count = 1;
		for (std::int64_t size = 1; size < width || size < height || size < depth; size *= 2)
		{
			count++;
		}
		return count;
	}

	int iwi_header::mip_count() const
	{
		return iwi_mip_level_count(flags, width, height, depth);
	}

	std::int64_t iwi_surface_size(const iwi_format format, const int width, const int height)
	{
		// IwiFile.cs:132-144
		const std::int64_t w = width, h = height;
		switch (format)
		{
		case iwi_format::rgba32: return 4 * w * h;
		case iwi_format::rgb24: return 3 * w * h;
		case iwi_format::luminance_alpha8: return 2 * w * h;
		case iwi_format::luminance8:
		case iwi_format::alpha8: return w * h;
		case iwi_format::dxt1: return 8 * ((w + 3) / 4) * ((h + 3) / 4);
		case iwi_format::dxt3:
		case iwi_format::dxt5: return 16 * ((w + 3) / 4) * ((h + 3) / 4);
		default: return -1;
		}
	}

	const iwi_surface* iwi_file::surface(const int level, const int face) const
	{
		for (const auto& s : surfaces)
		{
			if (s.level == level && s.face == face)
			{
				return &s;
			}
		}
		return nullptr;
	}

	bool parse_iwi_header(const byte_span data, iwi_header& out, std::string* error)
	{
		// IwiFile.cs:148-168 ( the T5 version 13 branch is not ported: 1.1.1.13 only )
		out = {};
		if (data.size() < build::IWI_HEADER_SIZE)
		{
			set_error(error, "IWI file is " + std::to_string(data.size()) + " bytes, shorter than its 32-byte header");
			return false;
		}
		if (data[0] != 'I' || data[1] != 'W' || data[2] != 'i')
		{
			set_error(error, "not an IWI image");
			return false;
		}
		out.version = data[3];
		out.flags = rd_u32(data.data() + 4);
		out.format = static_cast<iwi_format>(data[8]);
		out.width = rd_s16(data.data() + 10);
		out.height = rd_s16(data.data() + 12);
		out.depth = rd_s16(data.data() + 14);
		for (int i = 0; i < 4; i++)
		{
			out.file_size_for_picmip[static_cast<std::size_t>(i)] = rd_s32(data.data() + 16 + 4 * i);
		}
		return true;
	}

	bool parse_iwi(const byte_span data, iwi_file& out, std::string* error)
	{
		try
		{
			// IwiFile.cs:170-221
			out = {};
			iwi_header h;
			if (!parse_iwi_header(data, h, error))
			{
				return false;
			}
			if (h.version != build::IWI_VERSION && h.version != 8)
			{
				set_error(error, "IWI version " + std::to_string(h.version) + ", expected 9");
				return false;
			}
			if (h.width <= 0 || h.height <= 0 || h.depth <= 0)
			{
				set_error(error, "invalid dimensions " + std::to_string(h.width) + "x" + std::to_string(h.height) + "x" + std::to_string(h.depth));
				return false;
			}
			if (h.format < iwi_format::rgba32 || h.format > iwi_format::dxt5)
			{
				set_error(error, "unknown IWI format " + std::to_string(static_cast<int>(h.format)));
				return false;
			}
			if (h.is_volume() || h.depth > 1)
			{
				set_error(error, "volume textures are not supported");
				return false;
			}
			out.header = h;
			out.data = data;
			if (h.is_wavelet())
			{
				return true;
			}

			std::int64_t offset = build::IWI_HEADER_SIZE;
			const int levels = h.mip_count();
			for (int level = levels - 1; level >= 0; level--)
			{
				const int w = iwi_level_dimension(h.width, level);
				const int hh = iwi_level_dimension(h.height, level);
				const std::int64_t size = iwi_surface_size(h.format, w, hh);
				for (int face = 0; face < h.face_count(); face++)
				{
					if (offset + size > static_cast<std::int64_t>(data.size()))
					{
						set_error(error, "IWI data ends at " + std::to_string(data.size()) + " bytes, level " + std::to_string(level) + " face "
							+ std::to_string(face) + " needs bytes " + std::to_string(offset) + ".." + std::to_string(offset + size));
						return false;
					}
					out.surfaces.push_back({ level, face, w, hh, static_cast<std::uint32_t>(offset), static_cast<std::uint32_t>(size) });
					offset += size;
				}
			}
			if (offset != static_cast<std::int64_t>(data.size()))
			{
				set_error(error, "IWI layout covers " + std::to_string(offset) + " bytes but the file has " + std::to_string(data.size()));
				return false;
			}
			return true;
		}
		catch (...)
		{
			set_error(error, "out of memory");
			return false;
		}
	}
}
