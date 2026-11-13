#include "renderer_impl.hpp"

// Case textures: CoD4 radiant's render method of that name. Its technique "case texture" ( techniques/case_texture.tech
// of the CoD4 mod tools: colorMapSampler = material.colorMap, caseTextureSampler = sampler.caseTexture ) samples a
// code texture that CoD4Radiant.exe picks per material ( 0x53B630 ): of the images listed in bin/case_textures.txt
// ( case256, case1024x128, case512x64red .. ) the one whose width and height are those of the material's colour map,
// else a stand-in. So a surface shows a generic grid of the size of its texture: how many texels the texture has
// and how it is stretched over the surface.
//
// Here the grids are made, not read from the mod tools: for each size one image of that size ( at most 512 a side,
// the pattern kept in texels of the original ), a checker of 64 texel cells ( 16 for textures under 128 ) with a
// darker line every 256 texels, coloured by the larger side, and the size written in each 256 texel block.

namespace codol::view
{
	namespace
	{
		// 3 x 5 glyphs of "0123456789x", a row of 3 bits each from the top
		constexpr std::uint8_t GLYPHS[11][5] = {
			{ 7, 5, 5, 5, 7 }, { 2, 6, 2, 2, 7 }, { 7, 1, 7, 4, 7 }, { 7, 1, 7, 1, 7 }, { 5, 5, 7, 1, 1 },
			{ 7, 4, 7, 1, 7 }, { 7, 4, 7, 5, 7 }, { 7, 1, 1, 1, 1 }, { 7, 5, 7, 5, 7 }, { 7, 5, 7, 1, 7 },
			{ 0, 5, 2, 5, 0 } };

		std::uint32_t key_of(const int width, const int height)
		{
			return (static_cast<std::uint32_t>(width) << 16) | static_cast<std::uint32_t>(height & 0xFFFF);
		}
	}

	std::uint32_t map_renderer::case_texture_color(const int width, const int height)
	{
		// by the larger side: 64 and less blue, 128 cyan, 256 green, 512 grey, 1024 orange, 2048 and more red
		const int side = std::max(width, height);
		return side <= 64 ? 0xFF4070D0u : side <= 128 ? 0xFF40B0C0u : side <= 256 ? 0xFF50A050u : side <= 512 ? 0xFF909090u : side <= 1024 ? 0xFFD08030u : 0xFFC04040u;
	}

	std::vector<std::pair<int, int>> map_renderer::case_texture_sizes() const
	{
		std::vector<std::pair<int, int>> out;
		for (const std::uint32_t key : impl_->case_sizes_last)
		{
			out.emplace_back(static_cast<int>(key >> 16), static_cast<int>(key & 0xFFFF));
		}
		return out;
	}

	gpu_texture* map_renderer::impl::case_texture(const int width, const int height)
	{
		if (width <= 0 || height <= 0)
		{
			return nullptr;
		}

		const std::uint32_t key = key_of(width, height);
		case_sizes_frame.insert(key);

		if (const auto it = case_textures.find(key); it != case_textures.end())
		{
			return it->second.get();
		}

		// the image: as large as the texture, at most 512 a side
		const double fit = std::min(1.0, 512.0 / static_cast<double>(std::max(width, height)));
		const int w = std::max(4, static_cast<int>(width * fit));
		const int h = std::max(4, static_cast<int>(height * fit));
		const double texel_x = static_cast<double>(width) / w;		// texels of the original per pixel
		const double texel_y = static_cast<double>(height) / h;

		const std::uint32_t base = map_renderer::case_texture_color(width, height);
		const auto shade = [&](const double f) -> std::uint32_t
		{
			const auto c = [&](const int shift) { return static_cast<std::uint32_t>(std::clamp(((base >> shift) & 0xFF) * f, 0.0, 255.0)) << shift; };
			return 0xFF000000u | c(16) | c(8) | c(0);
		};

		const int cell = std::max(width, height) < 128 ? 16 : 64;
		const std::uint32_t light = shade(1.0), dark = shade(0.78), line = shade(0.45), text = 0xFFFFFFFFu;

		std::vector<std::uint32_t> pixels(static_cast<std::size_t>(w) * h);
		for (int y = 0; y < h; y++)
		{
			for (int x = 0; x < w; x++)
			{
				const int tx = static_cast<int>(x * texel_x), ty = static_cast<int>(y * texel_y);
				const bool odd = ((tx / cell) + (ty / cell)) & 1;
				std::uint32_t c = odd ? dark : light;
				// a line on the first pixel of every 256 texels, and on the edges of the image
				if (x == 0 || y == 0 || x == w - 1 || y == h - 1 || static_cast<int>((x + 1) * texel_x) / 256 != tx / 256 || static_cast<int>((y + 1) * texel_y) / 256 != ty / 256)
				{
					c = line;
				}
				pixels[static_cast<std::size_t>(y) * w + x] = c;
			}
		}

		// the size, "512x256", in every block of 256 texels ( or once, for smaller textures )
		std::vector<int> glyphs;
		for (const char ch : std::to_string(width) + "x" + std::to_string(height))
		{
			glyphs.push_back(ch == 'x' ? 10 : ch - '0');
		}

		const int block_w = std::max(1, static_cast<int>(std::min(256, width) / texel_x));
		const int block_h = std::max(1, static_cast<int>(std::min(256, height) / texel_y));
		const int text_w = static_cast<int>(glyphs.size()) * 4 - 1;
		// the text takes about a third of the block's width
		const int dot = std::max(1, std::min((block_w / 3) / std::max(1, text_w), (block_h / 6) / 5));

		if (text_w * dot + 4 <= block_w && 5 * dot + 4 <= block_h)
		{
			for (int by = 0; by + block_h <= h + 1; by += block_h)
			{
				for (int bx = 0; bx + block_w <= w + 1; bx += block_w)
				{
					for (std::size_t g = 0; g < glyphs.size(); g++)
					{
						for (int row = 0; row < 5; row++)
						{
							for (int col = 0; col < 3; col++)
							{
								if (!(GLYPHS[glyphs[g]][row] & (4 >> col)))
								{
									continue;
								}

								for (int dy = 0; dy < dot; dy++)
								{
									for (int dx = 0; dx < dot; dx++)
									{
										const int px = bx + 3 + (static_cast<int>(g) * 4 + col) * dot + dx;
										const int py = by + 3 + row * dot + dy;
										if (px < w && py < h) { pixels[static_cast<std::size_t>(py) * w + px] = text; }
									}
								}
							}
						}
					}
				}
			}
		}

		// the mip chain, by averages of 2 x 2
		render::texture_upload upload;
		upload.d3d_format = render::d3d::FMT_A8R8G8B8;
		upload.width = w;
		upload.height = h;

		std::vector<std::uint32_t> level = std::move(pixels);
		int lw = w, lh = h;

		while (true)
		{
			render::texture_surface surface;
			surface.level = upload.levels;
			surface.width = lw;
			surface.height = lh;
			surface.rows = lh;
			surface.row_bytes = lw * 4;
			surface.data.resize(level.size() * 4);
			std::memcpy(surface.data.data(), level.data(), surface.data.size());
			upload.surfaces.push_back(std::move(surface));
			upload.levels++;

			if (lw == 1 && lh == 1)
			{
				break;
			}

			const int nw = std::max(1, lw / 2), nh = std::max(1, lh / 2);
			std::vector<std::uint32_t> next(static_cast<std::size_t>(nw) * nh);

			for (int y = 0; y < nh; y++)
			{
				for (int x = 0; x < nw; x++)
				{
					std::uint32_t sum[4] = {};
					for (int k = 0; k < 4; k++)
					{
						const int sx = std::min(lw - 1, x * 2 + (k & 1)), sy = std::min(lh - 1, y * 2 + (k >> 1));
						const std::uint32_t p = level[static_cast<std::size_t>(sy) * lw + sx];
						for (int c = 0; c < 4; c++) { sum[c] += (p >> (c * 8)) & 0xFF; }
					}
					next[static_cast<std::size_t>(y) * nw + x] = (sum[0] / 4) | ((sum[1] / 4) << 8) | ((sum[2] / 4) << 16) | ((sum[3] / 4) << 24);
				}
			}

			level = std::move(next);
			lw = nw;
			lh = nh;
		}

		auto texture = std::make_unique<gpu_texture>();
		if (!create_texture("$case" + std::to_string(width) + "x" + std::to_string(height), upload, *texture, &last_error))
		{
			return nullptr;
		}

		return case_textures.emplace(key, std::move(texture)).first->second.get();
	}
}
