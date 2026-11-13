#include "codol_render/texture.hpp"

#include <algorithm>

namespace codol::render
{
	namespace
	{
		// GameTextureCache.cs:158-165 ( Layout )
		void set_layout(const std::uint32_t d3d_format, texture_surface& s)
		{
			switch (d3d_format)
			{
			case d3d::FMT_DXT1:
				s.rows = (s.height + 3) / 4;
				s.row_bytes = 8 * ((s.width + 3) / 4);
				break;
			case d3d::FMT_DXT3:
			case d3d::FMT_DXT5:
				s.rows = (s.height + 3) / 4;
				s.row_bytes = 16 * ((s.width + 3) / 4);
				break;
			case d3d::FMT_A8L8:
				s.rows = s.height;
				s.row_bytes = 2 * s.width;
				break;
			case d3d::FMT_L8:
			case d3d::FMT_A8:
				s.rows = s.height;
				s.row_bytes = s.width;
				break;
			default:
				s.rows = s.height;
				s.row_bytes = 4 * s.width;
				break;
			}
		}

		// TextureExtractor.cs:379-388 ( EmbeddedLayout.SurfaceBytes )
		std::int64_t d3d_surface_bytes(const std::uint32_t format, const int width, const int height)
		{
			const std::int64_t w = width, h = height;
			switch (format)
			{
			case d3d::FMT_A8R8G8B8:
			case d3d::FMT_X8R8G8B8: return 4 * w * h;
			case d3d::FMT_A8L8: return 2 * w * h;
			case d3d::FMT_L8:
			case d3d::FMT_A8: return w * h;
			case d3d::FMT_DXT1: return 8 * ((w + 3) / 4) * ((h + 3) / 4);
			case d3d::FMT_DXT3:
			case d3d::FMT_DXT5: return 16 * ((w + 3) / 4) * ((h + 3) / 4);
			default: return -1;
			}
		}

		// Orders the surfaces face major, level 0 first, as GameTextureCache.cs:96-112 walks them
		void sort_surfaces(texture_upload& upload)
		{
			std::sort(upload.surfaces.begin(), upload.surfaces.end(), [](const texture_surface& a, const texture_surface& b)
			{
				return a.face != b.face ? a.face < b.face : a.level < b.level;
			});
		}
	}

	const texture_surface* texture_upload::surface(const int level, const int face) const
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

	bool build_texture_upload(const iwi_file& file, const wavelet_tables* tables, texture_upload& out, std::string* error)
	{
		try
		{
			out = {};
			const auto& h = file.header;
			out.width = h.width;
			out.height = h.height;
			out.levels = h.mip_count();
			out.faces = h.face_count();

			if (h.is_wavelet())
			{
				// TextureExtractor.cs:209-248
				if (!tables)
				{
					set_error(error, std::string(iwi_format_name(h.format)) + " needs the wavelet tables of the client executable");
					return false;
				}
				switch (h.format)
				{
				case iwi_format::wavelet_rgba: out.d3d_format = d3d::FMT_A8R8G8B8; break;
				case iwi_format::wavelet_rgb: out.d3d_format = d3d::FMT_X8R8G8B8; break;
				case iwi_format::wavelet_luminance_alpha: out.d3d_format = d3d::FMT_A8L8; break;
				case iwi_format::wavelet_luminance: out.d3d_format = d3d::FMT_L8; break;
				default: out.d3d_format = d3d::FMT_A8; break;
				}
				std::vector<wavelet_level> levels;
				if (!decode_wavelet(file, *tables, levels, error))
				{
					return false;
				}
				out.surfaces.reserve(levels.size());
				for (auto& level : levels)
				{
					texture_surface s;
					s.level = level.level;
					s.face = level.face;
					s.width = level.width;
					s.height = level.height;
					s.data = std::move(level.data);
					set_layout(out.d3d_format, s);
					out.surfaces.push_back(std::move(s));
				}
				sort_surfaces(out);
				return true;
			}

			// TextureExtractor.cs:262-290 ( RawFromIwi )
			switch (h.format)
			{
			case iwi_format::rgba32: out.d3d_format = d3d::FMT_A8R8G8B8; break;
			case iwi_format::rgb24: out.d3d_format = d3d::FMT_X8R8G8B8; break;
			case iwi_format::luminance_alpha8: out.d3d_format = d3d::FMT_A8L8; break;
			case iwi_format::luminance8: out.d3d_format = d3d::FMT_L8; break;
			case iwi_format::alpha8: out.d3d_format = d3d::FMT_A8; break;
			case iwi_format::dxt1: out.d3d_format = d3d::FMT_DXT1; break;
			case iwi_format::dxt3: out.d3d_format = d3d::FMT_DXT3; break;
			case iwi_format::dxt5: out.d3d_format = d3d::FMT_DXT5; break;
			default:
				set_error(error, std::string("no Direct3D format for ") + iwi_format_name(h.format));
				return false;
			}
			out.surfaces.reserve(file.surfaces.size());
			for (const auto& surface : file.surfaces)
			{
				if (static_cast<std::uint64_t>(surface.offset) + surface.size > file.data.size())
				{
					set_error(error, "IWI surface lies outside the file");
					return false;
				}
				const std::uint8_t* data = file.data.data() + surface.offset;
				texture_surface s;
				s.level = surface.level;
				s.face = surface.face;
				s.width = surface.width;
				s.height = surface.height;
				if (h.format == iwi_format::rgb24)
				{
					// B G R -> B G R X
					const std::size_t count = static_cast<std::size_t>(surface.width) * surface.height;
					s.data.resize(count * 4);
					for (std::size_t p = 0; p < count; p++)
					{
						s.data[4 * p] = data[3 * p];
						s.data[4 * p + 1] = data[3 * p + 1];
						s.data[4 * p + 2] = data[3 * p + 2];
						s.data[4 * p + 3] = 255;
					}
				}
				else
				{
					s.data.assign(data, data + surface.size);
				}
				set_layout(out.d3d_format, s);
				out.surfaces.push_back(std::move(s));
			}
			sort_surfaces(out);
			return true;
		}
		catch (...)
		{
			set_error(error, "out of memory");
			return false;
		}
	}

	bool build_embedded_upload(const embedded_image& image, const byte_span data, texture_upload& out, std::string* error)
	{
		try
		{
			// TextureExtractor.cs:324-348 ( EmbeddedLayout.Find )
			out = {};
			const int faces = image.map_type == build::MAPTYPE_CUBE ? 6 : 1;
			const int width = std::max(image.width, 1), height = std::max(image.height, 1);
			if (data.size() < image.data_size)
			{
				set_error(error, "embedded image data is not loaded");
				return false;
			}
			const int candidates[3] = { iwi_mip_level_count(image.flags, width, height, 1), std::max(image.level_count, 1), 1 };
			int levels = 0;
			std::int64_t face_size = 0;
			for (const int candidate : candidates)
			{
				std::int64_t size = 0;
				for (int level = 0; level < candidate; level++)
				{
					const std::int64_t bytes = d3d_surface_bytes(image.d3d_format, iwi_level_dimension(width, level), iwi_level_dimension(height, level));
					if (bytes < 0)
					{
						set_error(error, "embedded image uses unsupported D3DFORMAT " + std::to_string(image.d3d_format));
						return false;
					}
					size += bytes;
				}
				if (size * faces == static_cast<std::int64_t>(image.data_size))
				{
					levels = candidate;
					face_size = size;
					break;
				}
			}
			if (levels == 0)
			{
				set_error(error, "embedded data of " + std::to_string(image.data_size) + " bytes does not match its size");
				return false;
			}

			// TextureExtractor.cs:350-362 ( Read ): per face, level 0 first
			out.d3d_format = image.d3d_format;
			out.width = width;
			out.height = height;
			out.levels = levels;
			out.faces = faces;
			out.surfaces.reserve(static_cast<std::size_t>(faces) * levels);
			for (int face = 0; face < faces; face++)
			{
				std::int64_t offset = face * face_size;
				for (int level = 0; level < levels; level++)
				{
					texture_surface s;
					s.level = level;
					s.face = face;
					s.width = iwi_level_dimension(width, level);
					s.height = iwi_level_dimension(height, level);
					const std::int64_t bytes = d3d_surface_bytes(image.d3d_format, s.width, s.height);
					s.data.assign(data.data() + offset, data.data() + offset + bytes);
					set_layout(out.d3d_format, s);
					out.surfaces.push_back(std::move(s));
					offset += bytes;
				}
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
