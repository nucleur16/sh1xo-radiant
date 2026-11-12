// Image compiler: mip chain, DXT1 / DXT3 / DXT5 encoder, IWI version 9 writer, GfxImage pieces, plain .iwd writer.
// The encoder is this library's own ( principal axis fit + least squares refinement of the end points ).

#include "codol_assets/image.hpp"

#include "codol_render/iwd.hpp"
#include "codol_render/iwi.hpp"
#include "codol_zone/c1_layouts.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>

namespace codol::assets
{
	namespace
	{
		namespace r = codol::render;

		constexpr std::uint32_t D3DFMT_A8R8G8B8 = 21, D3DFMT_X8R8G8B8 = 22, D3DFMT_A8 = 28, D3DFMT_L8 = 50, D3DFMT_A8L8 = 51;
		constexpr std::uint32_t D3DFMT_DXT1 = 0x31545844, D3DFMT_DXT3 = 0x33545844, D3DFMT_DXT5 = 0x35545844;

		struct SrgbTables
		{
			float toLinear[256];
			SrgbTables()
			{
				for (int k = 0; k < 256; k++)
				{
					const float c = static_cast<float>(k) / 255.0f;
					toLinear[k] = c <= 0.04045f ? c / 12.92f : std::pow((c + 0.055f) / 1.055f, 2.4f);
				}
			}
			static std::uint8_t to_srgb(const float linear)
			{
				const float c = linear <= 0.0031308f ? linear * 12.92f : 1.055f * std::pow(linear, 1.0f / 2.4f) - 0.055f;
				return static_cast<std::uint8_t>(std::clamp(c * 255.0f + 0.5f, 0.0f, 255.0f));
			}
		};

		const SrgbTables& srgb_tables()
		{
			static const SrgbTables tables;
			return tables;
		}

		// ---- DXT ------------------------------------------------------------------------------------

		struct Vec3
		{
			float x = 0, y = 0, z = 0;
		};

		std::uint16_t to565(const Vec3& c)
		{
			const int red = std::clamp(static_cast<int>(c.x * 31.0f / 255.0f + 0.5f), 0, 31);
			const int green = std::clamp(static_cast<int>(c.y * 63.0f / 255.0f + 0.5f), 0, 63);
			const int blue = std::clamp(static_cast<int>(c.z * 31.0f / 255.0f + 0.5f), 0, 31);
			return static_cast<std::uint16_t>((red << 11) | (green << 5) | blue);
		}

		void from565(const std::uint16_t c, int rgb[3])
		{
			const int red = (c >> 11) & 31, green = (c >> 5) & 63, blue = c & 31;
			rgb[0] = (red << 3) | (red >> 2);
			rgb[1] = (green << 2) | (green >> 4);
			rgb[2] = (blue << 3) | (blue >> 2);
		}

		// palette of a colour block; four: c0 > c1 interpretation ( always for DXT3 / DXT5 )
		void palette_of(const std::uint16_t c0, const std::uint16_t c1, const bool four, int palette[4][3])
		{
			from565(c0, palette[0]);
			from565(c1, palette[1]);
			for (int k = 0; k < 3; k++)
			{
				if (four)
				{
					palette[2][k] = (2 * palette[0][k] + palette[1][k]) / 3;
					palette[3][k] = (palette[0][k] + 2 * palette[1][k]) / 3;
				}
				else
				{
					palette[2][k] = (palette[0][k] + palette[1][k]) / 2;
					palette[3][k] = 0;
				}
			}
		}

		// nearest palette entry per pixel; returns the squared error over the used pixels
		int assign(const std::uint8_t pixels[16][4], const bool used[16], const int palette[4][3], const int entries, std::uint8_t indices[16])
		{
			int total = 0;
			for (int p = 0; p < 16; p++)
			{
				if (!used[p])
				{
					indices[p] = 3;
					continue;
				}
				int best = 0, bestError = INT32_MAX;
				for (int e = 0; e < entries; e++)
				{
					const int dr = pixels[p][0] - palette[e][0], dg = pixels[p][1] - palette[e][1], db = pixels[p][2] - palette[e][2];
					const int error = dr * dr + dg * dg + db * db;
					if (error < bestError)
					{
						bestError = error;
						best = e;
					}
				}
				indices[p] = static_cast<std::uint8_t>(best);
				total += bestError;
			}
			return total;
		}

		// four: 4 colour block; else 3 colours + transparent ( pixels with used == false )
		void encode_color_block(const std::uint8_t pixels[16][4], const bool used[16], const bool four, std::uint8_t out[8])
		{
			int count = 0;
			Vec3 mean;
			for (int p = 0; p < 16; p++)
			{
				if (used[p])
				{
					mean.x += pixels[p][0];
					mean.y += pixels[p][1];
					mean.z += pixels[p][2];
					count++;
				}
			}
			std::uint16_t c0 = 0, c1 = 0;
			std::uint8_t indices[16];
			if (!count)
			{
				std::fill(indices, indices + 16, static_cast<std::uint8_t>(3));
			}
			else
			{
				mean.x /= static_cast<float>(count);
				mean.y /= static_cast<float>(count);
				mean.z /= static_cast<float>(count);
				float cov[6] = {};
				for (int p = 0; p < 16; p++)
				{
					if (used[p])
					{
						const float dx = pixels[p][0] - mean.x, dy = pixels[p][1] - mean.y, dz = pixels[p][2] - mean.z;
						cov[0] += dx * dx;
						cov[1] += dx * dy;
						cov[2] += dx * dz;
						cov[3] += dy * dy;
						cov[4] += dy * dz;
						cov[5] += dz * dz;
					}
				}
				Vec3 axis{ cov[0], cov[3], cov[5] };
				if (axis.x + axis.y + axis.z < 1e-6f)
				{
					axis = { 1, 1, 1 };
				}
				else
				{
					// start from the largest column of the covariance matrix: never orthogonal to the main axis
					const float n0 = cov[0] * cov[0] + cov[1] * cov[1] + cov[2] * cov[2];
					const float n1 = cov[1] * cov[1] + cov[3] * cov[3] + cov[4] * cov[4];
					const float n2 = cov[2] * cov[2] + cov[4] * cov[4] + cov[5] * cov[5];
					axis = n0 >= n1 && n0 >= n2 ? Vec3{ cov[0], cov[1], cov[2] } : n1 >= n2 ? Vec3{ cov[1], cov[3], cov[4] } : Vec3{ cov[2], cov[4], cov[5] };
				}
				for (int iteration = 0; iteration < 8; iteration++)
				{
					const Vec3 next{ cov[0] * axis.x + cov[1] * axis.y + cov[2] * axis.z, cov[1] * axis.x + cov[3] * axis.y + cov[4] * axis.z,
						cov[2] * axis.x + cov[4] * axis.y + cov[5] * axis.z };
					const float length = std::sqrt(next.x * next.x + next.y * next.y + next.z * next.z);
					if (length < 1e-9f)
					{
						break;
					}
					axis = { next.x / length, next.y / length, next.z / length };
				}
				float low = 1e30f, high = -1e30f;
				for (int p = 0; p < 16; p++)
				{
					if (used[p])
					{
						const float t = (pixels[p][0] - mean.x) * axis.x + (pixels[p][1] - mean.y) * axis.y + (pixels[p][2] - mean.z) * axis.z;
						low = std::min(low, t);
						high = std::max(high, t);
					}
				}
				Vec3 a{ mean.x + axis.x * high, mean.y + axis.y * high, mean.z + axis.z * high };
				Vec3 b{ mean.x + axis.x * low, mean.y + axis.y * low, mean.z + axis.z * low };
				const int entries = four ? 4 : 3;
				const float weights4[4] = { 1.0f, 0.0f, 2.0f / 3.0f, 1.0f / 3.0f };
				const float weights3[3] = { 1.0f, 0.0f, 0.5f };
				const float* weights = four ? weights4 : weights3;

				int bestError = INT32_MAX;
				for (int iteration = 0; iteration < 4; iteration++)
				{
					const std::uint16_t q0 = to565(a), q1 = to565(b);
					int palette[4][3];
					palette_of(q0, q1, four, palette);
					std::uint8_t trial[16];
					const int error = assign(pixels, used, palette, entries, trial);
					if (error < bestError)
					{
						bestError = error;
						c0 = q0;
						c1 = q1;
						std::memcpy(indices, trial, 16);
					}
					else
					{
						break;
					}
					if (!error)
					{
						break;
					}
					// least squares end points for these indices: p = w a + ( 1 - w ) b
					float aa = 0, ab = 0, bb = 0;
					Vec3 pa, pb;
					for (int p = 0; p < 16; p++)
					{
						if (used[p])
						{
							const float w = weights[trial[p]], v = 1.0f - w;
							aa += w * w;
							ab += w * v;
							bb += v * v;
							pa.x += w * pixels[p][0];
							pa.y += w * pixels[p][1];
							pa.z += w * pixels[p][2];
							pb.x += v * pixels[p][0];
							pb.y += v * pixels[p][1];
							pb.z += v * pixels[p][2];
						}
					}
					const float det = aa * bb - ab * ab;
					if (std::fabs(det) < 1e-4f)
					{
						break;
					}
					a = { (pa.x * bb - pb.x * ab) / det, (pa.y * bb - pb.y * ab) / det, (pa.z * bb - pb.z * ab) / det };
					b = { (pb.x * aa - pa.x * ab) / det, (pb.y * aa - pa.y * ab) / det, (pb.z * aa - pa.z * ab) / det };
				}
			}
			// the order of the end points selects the block mode
			if (four)
			{
				if (c0 < c1)
				{
					std::swap(c0, c1);
					for (std::uint8_t& index : indices)
					{
						index ^= 1;		// 0 <-> 1, 2 <-> 3
					}
				}
				else if (c0 == c1)
				{
					std::fill(indices, indices + 16, static_cast<std::uint8_t>(0));
				}
			}
			else if (c0 > c1)
			{
				std::swap(c0, c1);
				for (std::uint8_t& index : indices)
				{
					if (index < 2)
					{
						index ^= 1;
					}
				}
			}
			out[0] = static_cast<std::uint8_t>(c0);
			out[1] = static_cast<std::uint8_t>(c0 >> 8);
			out[2] = static_cast<std::uint8_t>(c1);
			out[3] = static_cast<std::uint8_t>(c1 >> 8);
			for (int row = 0; row < 4; row++)
			{
				out[4 + row] = static_cast<std::uint8_t>(indices[row * 4] | (indices[row * 4 + 1] << 2) | (indices[row * 4 + 2] << 4) | (indices[row * 4 + 3] << 6));
			}
		}

		void alpha_palette(const int a0, const int a1, int palette[8])
		{
			palette[0] = a0;
			palette[1] = a1;
			if (a0 > a1)
			{
				for (int k = 1; k < 7; k++)
				{
					palette[1 + k] = ((7 - k) * a0 + k * a1) / 7;
				}
			}
			else
			{
				for (int k = 1; k < 5; k++)
				{
					palette[1 + k] = ((5 - k) * a0 + k * a1) / 5;
				}
				palette[6] = 0;
				palette[7] = 255;
			}
		}

		int alpha_assign(const std::uint8_t alpha[16], const int palette[8], std::uint8_t indices[16])
		{
			int total = 0;
			for (int p = 0; p < 16; p++)
			{
				int best = 0, bestError = INT32_MAX;
				for (int e = 0; e < 8; e++)
				{
					const int d = alpha[p] - palette[e];
					if (d * d < bestError)
					{
						bestError = d * d;
						best = e;
					}
				}
				indices[p] = static_cast<std::uint8_t>(best);
				total += bestError;
			}
			return total;
		}

		void encode_alpha_block(const std::uint8_t alpha[16], std::uint8_t out[8])
		{
			int low = 255, high = 0, innerLow = 256, innerHigh = -1;
			for (int p = 0; p < 16; p++)
			{
				low = std::min<int>(low, alpha[p]);
				high = std::max<int>(high, alpha[p]);
				if (alpha[p] != 0 && alpha[p] != 255)
				{
					innerLow = std::min<int>(innerLow, alpha[p]);
					innerHigh = std::max<int>(innerHigh, alpha[p]);
				}
			}
			int bestError = INT32_MAX, bestA0 = high, bestA1 = low;
			std::uint8_t indices[16] = {};
			const auto attempt = [&](const int a0, const int a1)
			{
				int palette[8];
				alpha_palette(a0, a1, palette);
				std::uint8_t trial[16];
				const int error = alpha_assign(alpha, palette, trial);
				if (error < bestError)
				{
					bestError = error;
					bestA0 = a0;
					bestA1 = a1;
					std::memcpy(indices, trial, 16);
				}
			};
			if (high == low)
			{
				attempt(high, low);		// a0 == a1: six value mode, index 0
			}
			else
			{
				// eight values between the extremes, and the range pulled in a little ( the extremes then round to the ends )
				attempt(high, low);
				const int inset = (high - low) / 34;
				if (inset > 0)
				{
					attempt(high - inset, low + inset);
				}
				// one least squares step on the best eight value assignment
				if (bestError > 0)
				{
					float aa = 0, ab = 0, bb = 0, pa = 0, pb = 0;
					for (int p = 0; p < 16; p++)
					{
						const int index = indices[p];
						const float w = index == 0 ? 1.0f : index == 1 ? 0.0f : static_cast<float>(8 - index) / 7.0f, v = 1.0f - w;
						aa += w * w;
						ab += w * v;
						bb += v * v;
						pa += w * alpha[p];
						pb += v * alpha[p];
					}
					const float det = aa * bb - ab * ab;
					if (std::fabs(det) > 1e-4f)
					{
						const int a0 = std::clamp(static_cast<int>((pa * bb - pb * ab) / det + 0.5f), 0, 255);
						const int a1 = std::clamp(static_cast<int>((pb * aa - pa * ab) / det + 0.5f), 0, 255);
						if (a0 > a1)
						{
							attempt(a0, a1);
						}
					}
				}
				// six values + exact 0 and 255
				if ((low == 0 || high == 255) && innerHigh >= 0)
				{
					attempt(innerLow, innerHigh == innerLow ? innerLow : innerHigh);
				}
				else if ((low == 0 || high == 255) && innerHigh < 0)
				{
					attempt(0, 255);
				}
			}
			out[0] = static_cast<std::uint8_t>(bestA0);
			out[1] = static_cast<std::uint8_t>(bestA1);
			std::uint64_t bits = 0;
			for (int p = 0; p < 16; p++)
			{
				bits |= static_cast<std::uint64_t>(indices[p]) << (3 * p);
			}
			for (int k = 0; k < 6; k++)
			{
				out[2 + k] = static_cast<std::uint8_t>(bits >> (8 * k));
			}
		}

		ImageFormat pick_format(const ImageOptions& options, const SourceImage& source)
		{
			if (options.format != ImageFormat::automatic)
			{
				return options.format;
			}
			bool alpha = false, grey = true;
			for (const Bitmap& face : source.faces)
			{
				alpha = alpha || uses_alpha(face);
				for (std::size_t k = 0; grey && k < face.rgba.size(); k += 4)
				{
					grey = face.rgba[k] == face.rgba[k + 1] && face.rgba[k] == face.rgba[k + 2];
				}
			}
			switch (options.use)
			{
			case ImageUse::color: return alpha ? ImageFormat::dxt5 : ImageFormat::dxt1;
			case ImageUse::normal: return ImageFormat::dxt5;
			case ImageUse::specular: return ImageFormat::dxt5;
			case ImageUse::detail: return ImageFormat::dxt1;
			case ImageUse::ui: return ImageFormat::rgba32;
			default: return grey && !alpha ? ImageFormat::luminance8 : ImageFormat::rgba32;
			}
		}

		std::vector<std::uint8_t> encode_surface(const Bitmap& image, const ImageFormat format)
		{
			std::vector<std::uint8_t> out;
			const std::size_t count = static_cast<std::size_t>(image.width) * image.height;
			const std::uint8_t* p = image.rgba.data();
			switch (format)
			{
			case ImageFormat::dxt1:
			case ImageFormat::dxt3:
			case ImageFormat::dxt5:
				return compress_dxt(image, format);
			case ImageFormat::rgba32:
				out.resize(count * 4);
				for (std::size_t k = 0; k < count; k++)
				{
					out[k * 4] = p[k * 4 + 2];
					out[k * 4 + 1] = p[k * 4 + 1];
					out[k * 4 + 2] = p[k * 4];
					out[k * 4 + 3] = p[k * 4 + 3];
				}
				break;
			case ImageFormat::rgb24:
				out.resize(count * 3);
				for (std::size_t k = 0; k < count; k++)
				{
					out[k * 3] = p[k * 4 + 2];
					out[k * 3 + 1] = p[k * 4 + 1];
					out[k * 3 + 2] = p[k * 4];
				}
				break;
			case ImageFormat::luminance_alpha8:
				out.resize(count * 2);
				for (std::size_t k = 0; k < count; k++)
				{
					out[k * 2] = p[k * 4];
					out[k * 2 + 1] = p[k * 4 + 3];
				}
				break;
			case ImageFormat::luminance8:
				out.resize(count);
				for (std::size_t k = 0; k < count; k++)
				{
					out[k] = p[k * 4];
				}
				break;
			default:
				out.resize(count);
				for (std::size_t k = 0; k < count; k++)
				{
					out[k] = p[k * 4 + 3];
				}
				break;
			}
			return out;
		}
	}

	Bitmap downsample(const Bitmap& image, const bool srgb, const bool normal)
	{
		Bitmap out;
		out.resize(std::max(1, image.width / 2), std::max(1, image.height / 2));
		const SrgbTables& tables = srgb_tables();
		for (int y = 0; y < out.height; y++)
		{
			for (int x = 0; x < out.width; x++)
			{
				const std::uint8_t* s[4];
				for (int k = 0; k < 4; k++)
				{
					s[k] = image.at(std::min(image.width - 1, x * 2 + (k & 1)), std::min(image.height - 1, y * 2 + (k >> 1)));
				}
				std::uint8_t* d = out.at(x, y);
				if (normal)
				{
					float v[3] = {};
					for (int k = 0; k < 4; k++)
					{
						for (int c = 0; c < 3; c++)
						{
							v[c] += static_cast<float>(s[k][c]) / 127.5f - 1.0f;
						}
					}
					const float length = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
					if (length < 1e-6f)
					{
						v[0] = v[1] = 0;
						v[2] = 1;
					}
					else
					{
						for (float& c : v)
						{
							c /= length;
						}
					}
					for (int c = 0; c < 3; c++)
					{
						d[c] = static_cast<std::uint8_t>(std::clamp((v[c] + 1.0f) * 127.5f + 0.5f, 0.0f, 255.0f));
					}
				}
				else if (srgb)
				{
					for (int c = 0; c < 3; c++)
					{
						d[c] = SrgbTables::to_srgb((tables.toLinear[s[0][c]] + tables.toLinear[s[1][c]] + tables.toLinear[s[2][c]] + tables.toLinear[s[3][c]]) * 0.25f);
					}
				}
				else
				{
					for (int c = 0; c < 3; c++)
					{
						d[c] = static_cast<std::uint8_t>((s[0][c] + s[1][c] + s[2][c] + s[3][c] + 2) / 4);
					}
				}
				d[3] = static_cast<std::uint8_t>((s[0][3] + s[1][3] + s[2][3] + s[3][3] + 2) / 4);
			}
		}
		return out;
	}

	std::vector<std::uint8_t> compress_dxt(const Bitmap& image, const ImageFormat format)
	{
		const int blocksX = (image.width + 3) / 4, blocksY = (image.height + 3) / 4;
		const std::size_t blockBytes = format == ImageFormat::dxt1 ? 8 : 16;
		std::vector<std::uint8_t> out(static_cast<std::size_t>(blocksX) * blocksY * blockBytes);
		for (int by = 0; by < blocksY; by++)
		{
			for (int bx = 0; bx < blocksX; bx++)
			{
				std::uint8_t pixels[16][4];
				std::uint8_t alpha[16];
				bool used[16];
				bool transparent = false;
				for (int p = 0; p < 16; p++)
				{
					const std::uint8_t* s = image.at(std::min(image.width - 1, bx * 4 + (p & 3)), std::min(image.height - 1, by * 4 + (p >> 2)));
					std::memcpy(pixels[p], s, 4);
					alpha[p] = s[3];
					used[p] = format != ImageFormat::dxt1 || s[3] >= 128;
					transparent = transparent || !used[p];
				}
				std::uint8_t* block = out.data() + (static_cast<std::size_t>(by) * blocksX + bx) * blockBytes;
				if (format == ImageFormat::dxt1)
				{
					encode_color_block(pixels, used, !transparent, block);
				}
				else
				{
					if (format == ImageFormat::dxt3)
					{
						for (int k = 0; k < 8; k++)
						{
							block[k] = static_cast<std::uint8_t>(((alpha[k * 2] * 15 + 127) / 255) | (((alpha[k * 2 + 1] * 15 + 127) / 255) << 4));
						}
					}
					else
					{
						encode_alpha_block(alpha, block);
					}
					encode_color_block(pixels, used, true, block + 8);
				}
			}
		}
		return out;
	}

	std::uint32_t CompiledImage::data_bytes(const int firstLevel) const
	{
		std::uint32_t total = 0;
		for (std::size_t k = static_cast<std::size_t>(std::max(0, firstLevel)) * faces; k < surfaces.size(); k++)
		{
			total += static_cast<std::uint32_t>(surfaces[k].size());
		}
		return total;
	}

	Status compile_image(const std::string_view name, const SourceImage& source, const ImageOptions& options, CompiledImage& out)
	try
	{
		out = {};
		if (name.empty() || (source.faces.size() != 1 && source.faces.size() != 6))
		{
			return Status::error("an image needs a name and one face, or six for a cube map");
		}
		const int width = source.faces[0].width, height = source.faces[0].height;
		for (const Bitmap& face : source.faces)
		{
			if (face.width != width || face.height != height || face.rgba.size() != static_cast<std::size_t>(width) * height * 4)
			{
				return Status::error("image '" + std::string(name) + "': all faces need the same size");
			}
		}
		if (width < 1 || height < 1 || width > 8192 || height > 8192)
		{
			return Status::error("image '" + std::string(name) + "': the size has to be 1 .. 8192");
		}
		const bool mips = options.mips < 0 ? options.use != ImageUse::ui : options.mips != 0;
		const bool noPicmip = options.noPicmip < 0 ? options.use == ImageUse::ui : options.noPicmip != 0;
		const auto power_of_two = [](const int v) { return (v & (v - 1)) == 0; };
		if (mips && (!power_of_two(width) || !power_of_two(height)))
		{
			return Status::error("image '" + std::string(name) + "': " + std::to_string(width) + " x " + std::to_string(height)
				+ " is not a power of two; only images without mip levels ( use ui, or mips off ) may have any size");
		}
		if (source.is_cube() && width != height)
		{
			return Status::error("image '" + std::string(name) + "': cube faces have to be square");
		}
		const ImageFormat format = pick_format(options, source);
		out.name = std::string(name);
		out.width = static_cast<std::uint16_t>(width);
		out.height = static_cast<std::uint16_t>(height);
		out.faces = static_cast<int>(source.faces.size());
		out.mapType = source.is_cube() ? 5 : 3;
		out.streamed = options.streamed;
		namespace f = r::iwi_flags;
		switch (options.use)
		{
		case ImageUse::color: out.semantic = 2; out.flags = f::ALPHA_WEIGHTED_COLORS | f::GAMMA_SRGB | f::GAMMA_PWL; break;
		case ImageUse::normal: out.semantic = 5; out.flags = f::NORMAL_MAP; break;
		case ImageUse::specular: out.semantic = 8; out.flags = f::ALPHA_WEIGHTED_COLORS | f::GAMMA_SRGB | f::GAMMA_PWL; break;
		case ImageUse::detail: out.semantic = 3; out.flags = f::ALPHA_WEIGHTED_COLORS | f::GAMMA_SRGB | f::GAMMA_PWL; break;
		case ImageUse::ui: out.semantic = 0; out.flags = f::CLAMP_U | f::CLAMP_V | f::ALPHA_WEIGHTED_COLORS | f::GAMMA_SRGB; break;
		default: out.semantic = 1; out.flags = 0; break;
		}
		out.flags |= (noPicmip ? f::NO_PICMIP : 0) | (mips ? 0 : f::NO_MIPMAPS) | (options.clampU ? f::CLAMP_U : 0) | (options.clampV ? f::CLAMP_V : 0)
			| (source.is_cube() ? f::MAPTYPE_CUBE : 0);
		switch (format)
		{
		case ImageFormat::dxt1: out.iwiFormat = 11; out.d3dFormat = D3DFMT_DXT1; break;
		case ImageFormat::dxt3: out.iwiFormat = 12; out.d3dFormat = D3DFMT_DXT3; break;
		case ImageFormat::dxt5: out.iwiFormat = 13; out.d3dFormat = D3DFMT_DXT5; break;
		case ImageFormat::rgba32: out.iwiFormat = 1; out.d3dFormat = D3DFMT_A8R8G8B8; break;
		case ImageFormat::rgb24: out.iwiFormat = 2; out.d3dFormat = D3DFMT_X8R8G8B8; break;
		case ImageFormat::luminance_alpha8: out.iwiFormat = 3; out.d3dFormat = D3DFMT_A8L8; break;
		case ImageFormat::luminance8: out.iwiFormat = 4; out.d3dFormat = D3DFMT_L8; break;
		case ImageFormat::alpha8: out.iwiFormat = 5; out.d3dFormat = D3DFMT_A8; break;
		default: return Status::error("unknown image format");
		}
		out.levelCount = r::iwi_mip_level_count(out.flags, width, height, 1);
		const bool normal = options.use == ImageUse::normal;
		const bool srgb = (out.flags & f::GAMMA_SRGB) != 0;
		std::vector<std::vector<Bitmap>> chain(source.faces.size());
		for (std::size_t face = 0; face < source.faces.size(); face++)
		{
			Bitmap level = source.faces[face];
			if (normal && options.flipNormalY)
			{
				for (std::size_t k = 1; k < level.rgba.size(); k += 4)
				{
					level.rgba[k] = static_cast<std::uint8_t>(255 - level.rgba[k]);
				}
			}
			for (int k = 0; k < out.levelCount; k++)
			{
				if (k)
				{
					level = downsample(level, srgb, normal);
				}
				chain[face].push_back(level);
			}
		}
		for (int level = 0; level < out.levelCount; level++)
		{
			for (std::size_t face = 0; face < source.faces.size(); face++)
			{
				Bitmap stored = chain[face][level];
				if (normal)
				{
					// as the shipped normal maps have it ( ch46e_body_damaged_nml: R == B, grey = y, alpha = x ): x in alpha,
					// y in all three colour channels; z is rebuilt by the shader
					for (std::size_t k = 0; k < stored.rgba.size(); k += 4)
					{
						stored.rgba[k + 3] = stored.rgba[k];
						stored.rgba[k] = stored.rgba[k + 2] = stored.rgba[k + 1];
					}
				}
				out.surfaces.push_back(encode_surface(stored, format));
				out.reference.push_back(std::move(stored));
			}
		}
		return {};
	}
	catch (const std::exception& e)
	{
		return Status::error(e.what());
	}

	std::vector<std::uint8_t> write_iwi(const CompiledImage& image)
	{
		std::vector<std::uint8_t> file(32);
		file[0] = 'I';
		file[1] = 'W';
		file[2] = 'i';
		file[3] = 9;
		std::memcpy(file.data() + 4, &image.flags, 4);
		file[8] = image.iwiFormat;
		const std::int16_t dims[3] = { static_cast<std::int16_t>(image.width), static_cast<std::int16_t>(image.height), 1 };
		std::memcpy(file.data() + 10, dims, 6);
		// smallest level first, the faces of a level together
		for (int level = image.levelCount - 1; level >= 0; level--)
		{
			for (int face = 0; face < image.faces; face++)
			{
				const auto& surface = image.surfaces[static_cast<std::size_t>(level) * image.faces + face];
				file.insert(file.end(), surface.begin(), surface.end());
			}
		}
		// the file size when the top n levels are left away
		for (int k = 0; k < 4; k++)
		{
			const std::int32_t size = 32 + static_cast<std::int32_t>(image.data_bytes(std::min(k, image.levelCount - 1)));
			std::memcpy(file.data() + 16 + 4 * k, &size, 4);
		}
		return file;
	}

	std::vector<zone::ZoneNewPiece> image_pieces(const CompiledImage& image)
	{
		std::vector<zone::ZoneNewPiece> pieces;
		const bool noPicmip = (image.flags & r::iwi_flags::NO_PICMIP) != 0;
		c1::GfxImage header{};
		header.loadDef = c1::ZPTR_INLINE;
		header.mapType = image.mapType;
		header.semantic = image.semantic;
		header.category = image.streamed ? 3 : 1;			// 3: loaded from a file, 1: the shipped inline material images
		// _unk7[13]: byte 7 and 8 zero, 9 the picmip of the second card memory figure, 10 "no picmip", 11 zero,
		// then the two card memory sizes ( all levels, and from level `picmip` on )
		const int picmip = image.streamed && !noPicmip ? std::min(2, image.levelCount - 1) : 0;
		header._unk7[2] = image.streamed && !noPicmip ? 2 : 0;
		header._unk7[3] = noPicmip ? 1 : 0;
		const std::uint32_t all = image.data_bytes(0), reduced = image.data_bytes(picmip);
		std::memcpy(header._unk7 + 5, &all, 4);
		std::memcpy(header._unk7 + 9, &reduced, 4);
		header.width = image.width;
		header.height = image.height;
		header.depth = 1;
		// 2 in 80 % of the streamed images and in the inline images that materials use ( $identitynormalmap of
		// code_post_gfx_mp, $white of the map zones, the pad icons ); 0 only in $white of the default zone, lightmaps ..
		header.delayLoad = 2;
		header.name = c1::ZPTR_INLINE;
		zone::ZoneNewPiece piece;
		piece.bytes.assign(reinterpret_cast<const std::uint8_t*>(&header), reinterpret_cast<const std::uint8_t*>(&header) + sizeof(header));
		pieces.push_back(piece);

		zone::ZoneNewPiece name;
		name.string = true;
		name.bytes.assign(image.name.begin(), image.name.end());
		name.bytes.push_back(0);
		pieces.push_back(name);

		c1::GfxImageLoadDef def{};
		def.levelCount = image.streamed ? 0 : static_cast<std::uint8_t>(image.levelCount);
		def.flags = image.flags;
		def.format = image.d3dFormat;
		def.resourceSize = 0;
		std::vector<std::uint8_t> pixels;
		if (!image.streamed)
		{
			// per face, level 0 first; 24 bit data becomes X8R8G8B8
			for (int face = 0; face < image.faces; face++)
			{
				for (int level = 0; level < image.levelCount; level++)
				{
					const auto& surface = image.surfaces[static_cast<std::size_t>(level) * image.faces + face];
					if (image.iwiFormat == 2)
					{
						for (std::size_t k = 0; k + 2 < surface.size(); k += 3)
						{
							pixels.insert(pixels.end(), { surface[k], surface[k + 1], surface[k + 2], std::uint8_t(255) });
						}
					}
					else
					{
						pixels.insert(pixels.end(), surface.begin(), surface.end());
					}
				}
			}
			def.resourceSize = static_cast<std::uint32_t>(pixels.size());
			std::memcpy(pieces[0].bytes.data() + offsetof(c1::GfxImage, _unk7) + 5, &def.resourceSize, 4);
			std::memcpy(pieces[0].bytes.data() + offsetof(c1::GfxImage, _unk7) + 9, &def.resourceSize, 4);
		}
		zone::ZoneNewPiece load;
		load.bytes.assign(reinterpret_cast<const std::uint8_t*>(&def), reinterpret_cast<const std::uint8_t*>(&def) + sizeof(def));
		pieces.push_back(load);
		if (!pixels.empty())
		{
			zone::ZoneNewPiece data;
			data.bytes = std::move(pixels);
			pieces.push_back(std::move(data));
		}
		return pieces;
	}

	bool iwd_needs_key(const std::filesystem::path& path)
	{
		const std::string name = path.filename().string();
		for (const char* part : { "iw_image", "iw_model", "iw_config", "iw_sound", "iw_ui_image", "iw_adlc" })
		{
			if (name.find(part) != std::string::npos)
			{
				return true;
			}
		}
		return false;
	}

	Status write_iwd(const std::filesystem::path& path, const std::span<const std::pair<std::string, std::vector<std::uint8_t>>> files,
		const std::array<std::uint8_t, 24>* key)
	try
	{
		std::vector<std::uint8_t> out, directory;
		// the cipher of an encrypted archive ( codol_render iwd.hpp ): counter { 0x01010101, crc32( file name ), entry
		// index, 0x01010101 } for an entry's name, { offset of the chunk in the file, crc32( file name ), 0x01010101,
		// 0x01010101 } for every 0x4000 bytes of its data
		const std::string file_name = path.filename().string();
		const std::uint32_t name_hash = r::crc32(r::byte_span(reinterpret_cast<const std::uint8_t*>(file_name.data()), file_name.size()));
		const auto counter = [](std::uint8_t iv[16], const std::uint32_t a, const std::uint32_t b, const std::uint32_t c, const std::uint32_t d)
		{
			const std::uint32_t words[4] = { a, b, c, d };
			std::memcpy(iv, words, 16);
		};
		std::size_t index = 0;
		const auto u16 = [](std::vector<std::uint8_t>& to, const std::uint32_t v)
		{
			to.push_back(static_cast<std::uint8_t>(v));
			to.push_back(static_cast<std::uint8_t>(v >> 8));
		};
		const auto u32 = [&](std::vector<std::uint8_t>& to, const std::uint32_t v)
		{
			u16(to, v & 0xFFFF);
			u16(to, v >> 16);
		};
		for (const auto& [name, data] : files)
		{
			if (name.empty() || name.size() > 0xFFFF || data.size() > 0x7FFFFFFFu || files.size() > 0xFFFF)
			{
				return Status::error("iwd: entry '" + name + "' cannot be stored");
			}
			const std::uint32_t crc = r::crc32(data), offset = static_cast<std::uint32_t>(out.size()), size = static_cast<std::uint32_t>(data.size());
			u32(out, 0x04034B50);
			u16(out, 20);
			u16(out, 0);
			u16(out, 0);			// stored
			u16(out, 0);
			u16(out, 0x21);			// 1980-01-01
			u32(out, crc);
			u32(out, size);
			u32(out, size);
			u16(out, static_cast<std::uint32_t>(name.size()));
			u16(out, 0);
			std::vector<std::uint8_t> stored_name(name.begin(), name.end());
			std::string cipher_error;
			if (key)
			{
				std::uint8_t iv[16];
				counter(iv, 0x01010101u, name_hash, static_cast<std::uint32_t>(index), 0x01010101u);
				if (!r::aes_ctr_transform(*key, iv, stored_name.data(), stored_name.size(), &cipher_error))
				{
					return Status::error("iwd: " + cipher_error);
				}
			}
			out.insert(out.end(), stored_name.begin(), stored_name.end());
			const std::size_t data_at = out.size();
			out.insert(out.end(), data.begin(), data.end());
			for (std::size_t k = 0; key && k < data.size(); k += 0x4000)
			{
				std::uint8_t iv[16];
				counter(iv, static_cast<std::uint32_t>(data_at + k), name_hash, 0x01010101u, 0x01010101u);
				if (!r::aes_ctr_transform(*key, iv, out.data() + data_at + k, std::min<std::size_t>(0x4000, data.size() - k), &cipher_error))
				{
					return Status::error("iwd: " + cipher_error);
				}
			}
			index++;

			u32(directory, 0x02014B50);
			u16(directory, 20);
			u16(directory, 20);
			u16(directory, 0);
			u16(directory, 0);
			u16(directory, 0);
			u16(directory, 0x21);
			u32(directory, crc);
			u32(directory, size);
			u32(directory, size);
			u16(directory, static_cast<std::uint32_t>(name.size()));
			u16(directory, 0);
			u16(directory, 0);
			u16(directory, 0);
			u16(directory, 0);
			u32(directory, 0);
			u32(directory, offset);
			directory.insert(directory.end(), stored_name.begin(), stored_name.end());
		}
		const std::uint32_t start = static_cast<std::uint32_t>(out.size());
		out.insert(out.end(), directory.begin(), directory.end());
		u32(out, 0x06054B50);
		u16(out, 0);
		u16(out, 0);
		u16(out, static_cast<std::uint32_t>(files.size()));
		u16(out, static_cast<std::uint32_t>(files.size()));
		u32(out, static_cast<std::uint32_t>(directory.size()));
		u32(out, start);
		u16(out, 0);
		return zone::write_file(path, out);
	}
	catch (const std::exception& e)
	{
		return Status::error(e.what());
	}
}
