#include "codol_render/texture.hpp"

namespace codol::render
{
	namespace
	{
		// TextureDecoder.cs:262-271
		void expand565(const std::uint16_t c, std::uint8_t* rgba)
		{
			const int r = (c >> 11) & 0x1F;
			const int g = (c >> 5) & 0x3F;
			const int b = c & 0x1F;
			rgba[0] = static_cast<std::uint8_t>((r << 3) | (r >> 2));
			rgba[1] = static_cast<std::uint8_t>((g << 2) | (g >> 4));
			rgba[2] = static_cast<std::uint8_t>((b << 3) | (b >> 2));
			rgba[3] = 255;
		}

		// TextureDecoder.cs:152-260. mode 1 DXT1, 2 DXT3, 3 DXT5
		void decode_bc(const byte_span data, const int width, const int height, std::uint8_t* pixels, const int mode)
		{
			const int blocks_x = (width + 3) / 4;
			const int blocks_y = (height + 3) / 4;
			const int block_size = mode == 1 ? 8 : 16;
			std::uint8_t colors[16];
			std::uint8_t alphas[8] = {};
			for (int by = 0; by < blocks_y; by++)
			{
				for (int bx = 0; bx < blocks_x; bx++)
				{
					const std::uint8_t* block = data.data() + static_cast<std::size_t>(by * blocks_x + bx) * block_size;
					const std::uint8_t* color_block = mode == 1 ? block : block + 8;
					const std::uint16_t c0 = rd_u16(color_block);
					const std::uint16_t c1 = rd_u16(color_block + 2);
					expand565(c0, colors);
					expand565(c1, colors + 4);
					if (mode != 1 || c0 > c1)
					{
						for (int k = 0; k < 3; k++)
						{
							colors[8 + k] = static_cast<std::uint8_t>((2 * colors[k] + colors[4 + k] + 1) / 3);
							colors[12 + k] = static_cast<std::uint8_t>((colors[k] + 2 * colors[4 + k] + 1) / 3);
						}
						colors[11] = 255;
						colors[15] = 255;
					}
					else
					{
						for (int k = 0; k < 3; k++)
						{
							colors[8 + k] = static_cast<std::uint8_t>((colors[k] + colors[4 + k]) / 2);
							colors[12 + k] = 0;
						}
						colors[11] = 255;
						colors[15] = 0;
					}
					const std::uint32_t indices = rd_u32(color_block + 4);

					if (mode == 3)
					{
						alphas[0] = block[0];
						alphas[1] = block[1];
						if (alphas[0] > alphas[1])
						{
							for (int k = 1; k <= 6; k++)
							{
								alphas[1 + k] = static_cast<std::uint8_t>(((7 - k) * alphas[0] + k * alphas[1] + 3) / 7);
							}
						}
						else
						{
							for (int k = 1; k <= 4; k++)
							{
								alphas[1 + k] = static_cast<std::uint8_t>(((5 - k) * alphas[0] + k * alphas[1] + 2) / 5);
							}
							alphas[6] = 0;
							alphas[7] = 255;
						}
					}
					std::uint64_t alpha_bits = 0;
					if (mode == 3)
					{
						for (int k = 0; k < 6; k++)
						{
							alpha_bits |= static_cast<std::uint64_t>(block[2 + k]) << (8 * k);
						}
					}
					else if (mode == 2)
					{
						for (int k = 0; k < 8; k++)
						{
							alpha_bits |= static_cast<std::uint64_t>(block[k]) << (8 * k);
						}
					}

					for (int py = 0; py < 4; py++)
					{
						const int y = by * 4 + py;
						if (y >= height)
						{
							break;
						}
						for (int px = 0; px < 4; px++)
						{
							const int x = bx * 4 + px;
							const int texel = py * 4 + px;
							if (x >= width)
							{
								continue;
							}
							const int ci = static_cast<int>((indices >> (2 * texel)) & 3);
							std::uint8_t* o = pixels + (static_cast<std::size_t>(y) * width + x) * 4;
							o[0] = colors[ci * 4];
							o[1] = colors[ci * 4 + 1];
							o[2] = colors[ci * 4 + 2];
							o[3] = mode == 1 ? colors[ci * 4 + 3]
								: mode == 2 ? static_cast<std::uint8_t>(((alpha_bits >> (4 * texel)) & 0xF) * 17)
								: alphas[static_cast<int>((alpha_bits >> (3 * texel)) & 7)];
						}
					}
				}
			}
		}
	}

	bool decode_surface(const iwi_format format, const byte_span data, const int width, const int height, rgba_image& out, std::string* error)
	{
		try
		{
			// TextureDecoder.cs:54-122
			out = {};
			if (width <= 0 || height <= 0)
			{
				set_error(error, "invalid surface size");
				return false;
			}
			const std::int64_t expected = iwi_surface_size(format, width, height);
			if (expected < 0)
			{
				set_error(error, std::string(iwi_format_name(format)) + " cannot be decoded per surface");
				return false;
			}
			if (static_cast<std::int64_t>(data.size()) < expected)
			{
				set_error(error, std::string(iwi_format_name(format)) + " " + std::to_string(width) + "x" + std::to_string(height) + " needs "
					+ std::to_string(expected) + " bytes, got " + std::to_string(data.size()));
				return false;
			}
			out.width = width;
			out.height = height;
			out.pixels.resize(static_cast<std::size_t>(width) * height * 4);
			std::uint8_t* pixels = out.pixels.data();
			const std::size_t count = static_cast<std::size_t>(width) * height;
			const std::uint8_t* d = data.data();
			switch (format)
			{
			case iwi_format::rgba32:
				for (std::size_t p = 0; p < count; p++, d += 4, pixels += 4)
				{
					pixels[0] = d[2]; pixels[1] = d[1]; pixels[2] = d[0]; pixels[3] = d[3];
				}
				break;
			case iwi_format::rgb24:
				for (std::size_t p = 0; p < count; p++, d += 3, pixels += 4)
				{
					pixels[0] = d[2]; pixels[1] = d[1]; pixels[2] = d[0]; pixels[3] = 255;
				}
				break;
			case iwi_format::luminance_alpha8:
				for (std::size_t p = 0; p < count; p++, d += 2, pixels += 4)
				{
					pixels[0] = pixels[1] = pixels[2] = d[0]; pixels[3] = d[1];
				}
				break;
			case iwi_format::luminance8:
				for (std::size_t p = 0; p < count; p++, d++, pixels += 4)
				{
					pixels[0] = pixels[1] = pixels[2] = d[0]; pixels[3] = 255;
				}
				break;
			case iwi_format::alpha8:
				for (std::size_t p = 0; p < count; p++, d++, pixels += 4)
				{
					pixels[0] = pixels[1] = pixels[2] = 255; pixels[3] = d[0];
				}
				break;
			case iwi_format::dxt1:
				decode_bc(data, width, height, pixels, 1);
				break;
			case iwi_format::dxt3:
				decode_bc(data, width, height, pixels, 2);
				break;
			case iwi_format::dxt5:
				decode_bc(data, width, height, pixels, 3);
				break;
			default:
				set_error(error, "unsupported format");
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

	std::optional<iwi_format> iwi_format_from_d3d(const std::uint32_t d3d_format)
	{
		// TextureDecoder.cs:124-134
		switch (d3d_format)
		{
		case d3d::FMT_A8R8G8B8: return iwi_format::rgba32;
		case d3d::FMT_A8: return iwi_format::alpha8;
		case d3d::FMT_L8: return iwi_format::luminance8;
		case d3d::FMT_A8L8: return iwi_format::luminance_alpha8;
		case d3d::FMT_DXT1: return iwi_format::dxt1;
		case d3d::FMT_DXT3: return iwi_format::dxt3;
		case d3d::FMT_DXT5: return iwi_format::dxt5;
		default: return std::nullopt;
		}
	}

	bool upload_surface_to_rgba(const texture_upload& upload, const texture_surface& surface, rgba_image& out, std::string* error)
	{
		// TextureExtractor.cs:230-240, 296-311: X8R8G8B8 is read as A8R8G8B8 with alpha forced to 255
		const bool x8 = upload.d3d_format == d3d::FMT_X8R8G8B8;
		const auto format = x8 ? std::optional(iwi_format::rgba32) : iwi_format_from_d3d(upload.d3d_format);
		if (!format)
		{
			set_error(error, "unsupported D3DFORMAT " + std::to_string(upload.d3d_format));
			return false;
		}
		if (!decode_surface(*format, surface.data, surface.width, surface.height, out, error))
		{
			return false;
		}
		if (x8)
		{
			for (std::size_t i = 3; i < out.pixels.size(); i += 4)
			{
				out.pixels[i] = 255;
			}
		}
		return true;
	}

	std::string iwi_game_path(std::string_view image_name)
	{
		while (!image_name.empty() && image_name.front() == ',')
		{
			image_name.remove_prefix(1);
		}
		return "images/" + std::string(image_name) + ".iwi";
	}
}
