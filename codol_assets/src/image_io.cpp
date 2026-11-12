// PNG / TGA / DDS readers and the PNG writer. Written for this library from the format specifications ( PNG: RFC 2083,
// TGA: Truevision 2.0, DDS: the DDS_HEADER / DDS_PIXELFORMAT layout ); inflate and deflate are codol_zone's.

#include "codol_assets/bitmap.hpp"

#include "codol_render/iwd.hpp"
#include "codol_render/texture.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace codol::assets
{
	namespace
	{
		std::uint32_t be32(const std::uint8_t* p)
		{
			return (static_cast<std::uint32_t>(p[0]) << 24) | (static_cast<std::uint32_t>(p[1]) << 16) | (static_cast<std::uint32_t>(p[2]) << 8) | p[3];
		}
		std::uint32_t le32(const std::uint8_t* p)
		{
			return static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8) | (static_cast<std::uint32_t>(p[2]) << 16)
				| (static_cast<std::uint32_t>(p[3]) << 24);
		}
		std::uint16_t le16(const std::uint8_t* p) { return static_cast<std::uint16_t>(p[0] | (p[1] << 8)); }

		void put_be32(std::vector<std::uint8_t>& out, const std::uint32_t v)
		{
			out.push_back(static_cast<std::uint8_t>(v >> 24));
			out.push_back(static_cast<std::uint8_t>(v >> 16));
			out.push_back(static_cast<std::uint8_t>(v >> 8));
			out.push_back(static_cast<std::uint8_t>(v));
		}

		const std::uint8_t PNG_SIGNATURE[8] = { 0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A };

		int paeth(const int a, const int b, const int c)
		{
			const int p = a + b - c;
			const int pa = std::abs(p - a), pb = std::abs(p - b), pc = std::abs(p - c);
			return pa <= pb && pa <= pc ? a : pb <= pc ? b : c;
		}
	}

	Status decode_png(const std::span<const std::uint8_t> file, Bitmap& out, bool* hasAlpha)
	try
	{
		if (file.size() < 8 || std::memcmp(file.data(), PNG_SIGNATURE, 8) != 0)
		{
			return Status::error("not a PNG file");
		}
		std::uint32_t width = 0, height = 0;
		int depth = 0, colorType = -1, interlace = 0;
		std::vector<std::uint8_t> idat, palette, trns;
		std::size_t at = 8;
		bool end = false;
		while (!end && at + 12 <= file.size())
		{
			const std::uint32_t length = be32(file.data() + at);
			const std::uint8_t* type = file.data() + at + 4;
			const std::uint8_t* data = file.data() + at + 8;
			if (length > file.size() - at - 12)
			{
				return Status::error("PNG: a chunk runs past the end of the file");
			}
			if (std::memcmp(type, "IHDR", 4) == 0 && length >= 13)
			{
				width = be32(data);
				height = be32(data + 4);
				depth = data[8];
				colorType = data[9];
				interlace = data[12];
			}
			else if (std::memcmp(type, "PLTE", 4) == 0)
			{
				palette.assign(data, data + length);
			}
			else if (std::memcmp(type, "tRNS", 4) == 0)
			{
				trns.assign(data, data + length);
			}
			else if (std::memcmp(type, "IDAT", 4) == 0)
			{
				idat.insert(idat.end(), data, data + length);
			}
			else if (std::memcmp(type, "IEND", 4) == 0)
			{
				end = true;
			}
			at += 12 + static_cast<std::size_t>(length);
		}
		int channels = 0;
		switch (colorType)
		{
		case 0: channels = 1; break;
		case 2: channels = 3; break;
		case 3: channels = 1; break;
		case 4: channels = 2; break;
		case 6: channels = 4; break;
		default: return Status::error("PNG: no header or an unknown colour type");
		}
		if (!width || !height || width > 16384 || height > 16384)
		{
			return Status::error("PNG: unsupported size");
		}
		if (interlace)
		{
			return Status::error("PNG: interlaced files are not supported, save it without interlacing");
		}
		if (!(depth == 8 || depth == 16 || ((colorType == 0 || colorType == 3) && (depth == 1 || depth == 2 || depth == 4))) || (colorType == 3 && depth == 16))
		{
			return Status::error("PNG: unsupported bit depth");
		}
		const std::size_t bitsPerPixel = static_cast<std::size_t>(channels) * depth;
		const std::size_t stride = (width * bitsPerPixel + 7) / 8;
		const std::size_t bpp = std::max<std::size_t>(1, bitsPerPixel / 8);
		std::vector<std::uint8_t> raw;
		if (const Status s = zone::zlib_inflate(idat, raw, (stride + 1) * height); !s)
		{
			return Status::error("PNG: " + s.message);
		}
		if (raw.size() < (stride + 1) * height)
		{
			return Status::error("PNG: the image data is too short");
		}
		std::vector<std::uint8_t> previous(stride, 0), line(stride);
		out.resize(static_cast<int>(width), static_cast<int>(height));
		const bool alpha = colorType == 4 || colorType == 6 || !trns.empty();
		if (hasAlpha)
		{
			*hasAlpha = alpha;
		}
		for (std::uint32_t y = 0; y < height; y++)
		{
			const std::uint8_t* row = raw.data() + (stride + 1) * y;
			const int filter = row[0];
			std::memcpy(line.data(), row + 1, stride);
			for (std::size_t x = 0; x < stride; x++)
			{
				const int a = x >= bpp ? line[x - bpp] : 0;
				const int b = previous[x];
				const int c = x >= bpp ? previous[x - bpp] : 0;
				int add = 0;
				switch (filter)
				{
				case 1: add = a; break;
				case 2: add = b; break;
				case 3: add = (a + b) / 2; break;
				case 4: add = paeth(a, b, c); break;
				default: break;
				}
				line[x] = static_cast<std::uint8_t>(line[x] + add);
			}
			std::uint8_t* dst = out.at(0, static_cast<int>(y));
			for (std::uint32_t x = 0; x < width; x++, dst += 4)
			{
				std::uint8_t v[4] = { 0, 0, 0, 255 };
				std::uint16_t wide[4] = {};
				if (depth >= 8)
				{
					const std::size_t step = static_cast<std::size_t>(depth) / 8;
					for (int ch = 0; ch < channels; ch++)
					{
						const std::uint8_t* s = line.data() + (static_cast<std::size_t>(x) * channels + ch) * step;
						v[ch] = s[0];
						wide[ch] = step == 2 ? static_cast<std::uint16_t>((s[0] << 8) | s[1]) : s[0];
					}
				}
				else
				{
					const std::size_t bit = static_cast<std::size_t>(x) * depth;
					const int sample = (line[bit / 8] >> (8 - depth - static_cast<int>(bit % 8))) & ((1 << depth) - 1);
					wide[0] = static_cast<std::uint16_t>(sample);
					v[0] = colorType == 3 ? static_cast<std::uint8_t>(sample) : static_cast<std::uint8_t>(sample * 255 / ((1 << depth) - 1));
				}
				switch (colorType)
				{
				case 0:
					dst[0] = dst[1] = dst[2] = v[0];
					dst[3] = trns.size() >= 2 && wide[0] == ((trns[0] << 8) | trns[1]) ? 0 : 255;
					break;
				case 2:
					dst[0] = v[0];
					dst[1] = v[1];
					dst[2] = v[2];
					dst[3] = trns.size() >= 6 && wide[0] == ((trns[0] << 8) | trns[1]) && wide[1] == ((trns[2] << 8) | trns[3])
						&& wide[2] == ((trns[4] << 8) | trns[5]) ? 0 : 255;
					break;
				case 3:
				{
					const std::size_t index = v[0];
					if (index * 3 + 2 < palette.size())
					{
						dst[0] = palette[index * 3];
						dst[1] = palette[index * 3 + 1];
						dst[2] = palette[index * 3 + 2];
					}
					dst[3] = index < trns.size() ? trns[index] : 255;
					break;
				}
				case 4:
					dst[0] = dst[1] = dst[2] = v[0];
					dst[3] = v[1];
					break;
				default:
					std::memcpy(dst, v, 4);
					break;
				}
			}
			previous.swap(line);
		}
		return {};
	}
	catch (const std::exception& e)
	{
		return Status::error(e.what());
	}

	Status decode_tga(const std::span<const std::uint8_t> file, Bitmap& out, bool* hasAlpha)
	try
	{
		if (file.size() < 18)
		{
			return Status::error("not a TGA file");
		}
		const int idLength = file[0], mapType = file[1], type = file[2];
		const int mapFirst = le16(file.data() + 3), mapLength = le16(file.data() + 5), mapBits = file[7];
		const int width = le16(file.data() + 12), height = le16(file.data() + 14), bits = file[16], descriptor = file[17];
		const bool rle = type >= 9;
		const int base = rle ? type - 8 : type;
		if ((base != 1 && base != 2 && base != 3) || !width || !height || mapType > 1)
		{
			return Status::error("TGA: unsupported image type");
		}
		if ((base == 2 && bits != 24 && bits != 32 && bits != 16 && bits != 15) || (base == 3 && bits != 8 && bits != 16) || (base == 1 && bits != 8))
		{
			return Status::error("TGA: unsupported bit depth");
		}
		std::size_t at = 18 + static_cast<std::size_t>(idLength);
		const std::size_t mapEntry = static_cast<std::size_t>(mapBits + 7) / 8;
		const std::uint8_t* map = file.data() + at;
		if (mapType)
		{
			at += mapEntry * mapLength;
		}
		if (at > file.size() || (base == 1 && (!mapType || mapEntry < 2)))
		{
			return Status::error("TGA: truncated");
		}
		const std::size_t bytes = static_cast<std::size_t>(bits + 7) / 8;
		std::vector<std::uint8_t> pixels(static_cast<std::size_t>(width) * height * bytes);
		if (!rle)
		{
			if (file.size() - at < pixels.size())
			{
				return Status::error("TGA: truncated");
			}
			std::memcpy(pixels.data(), file.data() + at, pixels.size());
		}
		else
		{
			std::size_t o = 0;
			while (o < pixels.size())
			{
				if (at >= file.size())
				{
					return Status::error("TGA: truncated");
				}
				const int head = file[at++];
				const std::size_t count = static_cast<std::size_t>(head & 0x7F) + 1;
				if (head & 0x80)
				{
					if (file.size() - at < bytes)
					{
						return Status::error("TGA: truncated");
					}
					for (std::size_t k = 0; k < count && o < pixels.size(); k++, o += bytes)
					{
						std::memcpy(pixels.data() + o, file.data() + at, bytes);
					}
					at += bytes;
				}
				else
				{
					const std::size_t n = std::min(count * bytes, pixels.size() - o);
					if (file.size() - at < n)
					{
						return Status::error("TGA: truncated");
					}
					std::memcpy(pixels.data() + o, file.data() + at, n);
					at += n;
					o += n;
				}
			}
		}
		const auto colour = [](const std::uint8_t* s, const std::size_t size, std::uint8_t* d)
		{
			if (size >= 3)
			{
				d[0] = s[2];
				d[1] = s[1];
				d[2] = s[0];
				d[3] = size == 4 ? s[3] : 255;
			}
			else
			{
				const int v = s[0] | (s[1] << 8);
				d[0] = static_cast<std::uint8_t>(((v >> 10) & 31) * 255 / 31);
				d[1] = static_cast<std::uint8_t>(((v >> 5) & 31) * 255 / 31);
				d[2] = static_cast<std::uint8_t>((v & 31) * 255 / 31);
				d[3] = 255;
			}
		};
		out.resize(width, height);
		const bool top = (descriptor & 0x20) != 0, right = (descriptor & 0x10) != 0;
		const bool alpha = (base == 2 && bits == 32 && (descriptor & 0x0F) != 0) || (base == 3 && bits == 16) || (base == 1 && mapEntry == 4);
		if (hasAlpha)
		{
			*hasAlpha = alpha;
		}
		for (int y = 0; y < height; y++)
		{
			for (int x = 0; x < width; x++)
			{
				const std::uint8_t* s = pixels.data() + (static_cast<std::size_t>(y) * width + x) * bytes;
				std::uint8_t* d = out.at(right ? width - 1 - x : x, top ? y : height - 1 - y);
				if (base == 2)
				{
					colour(s, bytes, d);
					if (bytes == 4 && !alpha)
					{
						d[3] = 255;
					}
				}
				else if (base == 3)
				{
					d[0] = d[1] = d[2] = s[0];
					d[3] = bytes == 2 ? s[1] : 255;
				}
				else
				{
					const int index = s[0] - mapFirst;
					if (index >= 0 && index < mapLength)
					{
						colour(map + static_cast<std::size_t>(index) * mapEntry, mapEntry, d);
					}
					else
					{
						d[3] = 255;
					}
				}
			}
		}
		return {};
	}
	catch (const std::exception& e)
	{
		return Status::error(e.what());
	}

	Status decode_dds(const std::span<const std::uint8_t> file, SourceImage& out)
	try
	{
		out = {};
		if (file.size() < 128 || std::memcmp(file.data(), "DDS ", 4) != 0 || le32(file.data() + 4) != 124)
		{
			return Status::error("not a DDS file");
		}
		const std::uint8_t* h = file.data() + 4;
		const int height = static_cast<int>(le32(h + 8)), width = static_cast<int>(le32(h + 12));
		const std::uint32_t mipCount = std::max<std::uint32_t>(1, le32(h + 24));
		const std::uint8_t* pf = h + 72;
		const std::uint32_t pfFlags = le32(pf + 4), fourcc = le32(pf + 8), bitCount = le32(pf + 12);
		const std::uint32_t masks[4] = { le32(pf + 16), le32(pf + 20), le32(pf + 24), le32(pf + 28) };
		const std::uint32_t caps2 = le32(h + 108);
		const int faces = (caps2 & 0x200) ? 6 : 1;
		if (width <= 0 || height <= 0 || width > 16384 || height > 16384 || (caps2 & 0x200000))
		{
			return Status::error("DDS: unsupported size or a volume texture");
		}
		if (faces == 6 && (caps2 & 0xFC00) != 0xFC00)
		{
			return Status::error("DDS: a cube map needs all six faces");
		}
		namespace r = codol::render;
		r::iwi_format dxt = r::iwi_format::rgba32;
		bool compressed = false;
		if (pfFlags & 0x4)
		{
			compressed = true;
			if (fourcc == 0x31545844)
			{
				dxt = r::iwi_format::dxt1;
			}
			else if (fourcc == 0x33545844 || fourcc == 0x32545844)
			{
				dxt = r::iwi_format::dxt3;
			}
			else if (fourcc == 0x35545844 || fourcc == 0x34545844)
			{
				dxt = r::iwi_format::dxt5;
			}
			else
			{
				return Status::error("DDS: unsupported compression ( DXT1, DXT3 and DXT5 are read )");
			}
		}
		else if (bitCount != 8 && bitCount != 16 && bitCount != 24 && bitCount != 32)
		{
			return Status::error("DDS: unsupported pixel format");
		}
		const auto level_bytes = [&](const int w, const int ht) -> std::size_t
		{
			if (compressed)
			{
				return static_cast<std::size_t>(r::iwi_surface_size(dxt, w, ht));
			}
			return static_cast<std::size_t>(w) * ht * (bitCount / 8);
		};
		std::size_t faceBytes = 0;
		for (std::uint32_t level = 0; level < mipCount; level++)
		{
			faceBytes += level_bytes(std::max(1, width >> level), std::max(1, height >> level));
		}
		const std::size_t top = level_bytes(width, height);
		const auto channel = [](const std::uint32_t value, const std::uint32_t mask) -> std::uint8_t
		{
			if (!mask)
			{
				return 0;
			}
			int shift = 0;
			while (!((mask >> shift) & 1))
			{
				shift++;
			}
			const std::uint32_t max = mask >> shift;
			return static_cast<std::uint8_t>(((value & mask) >> shift) * 255u / max);
		};
		const bool luminance = (pfFlags & 0x20000) != 0 || (!(pfFlags & 0x40) && !compressed && bitCount <= 16 && masks[1] == 0);
		const bool alphaOnly = !compressed && (pfFlags & 0x2) && !(pfFlags & 0x40) && !luminance;
		out.hasAlpha = compressed ? dxt != r::iwi_format::dxt1 : (pfFlags & 0x1) != 0 || alphaOnly;
		for (int face = 0; face < faces; face++)
		{
			const std::size_t at = 128 + faceBytes * face;
			if (at + top > file.size())
			{
				return Status::error("DDS: truncated");
			}
			Bitmap bitmap;
			if (compressed)
			{
				r::rgba_image image;
				std::string error;
				if (!r::decode_surface(dxt, file.subspan(at, top), width, height, image, &error))
				{
					return Status::error("DDS: " + error);
				}
				bitmap.width = image.width;
				bitmap.height = image.height;
				bitmap.rgba = std::move(image.pixels);
			}
			else
			{
				bitmap.resize(width, height);
				const std::size_t bytes = bitCount / 8;
				for (std::size_t k = 0; k < static_cast<std::size_t>(width) * height; k++)
				{
					const std::uint8_t* s = file.data() + at + k * bytes;
					std::uint32_t v = 0;
					for (std::size_t b = 0; b < bytes; b++)
					{
						v |= static_cast<std::uint32_t>(s[b]) << (8 * b);
					}
					std::uint8_t* d = bitmap.rgba.data() + k * 4;
					if (alphaOnly)
					{
						d[0] = d[1] = d[2] = 255;
						d[3] = channel(v, masks[3] ? masks[3] : 0xFF);
					}
					else if (luminance)
					{
						d[0] = d[1] = d[2] = channel(v, masks[0] ? masks[0] : 0xFF);
						d[3] = masks[3] ? channel(v, masks[3]) : 255;
					}
					else
					{
						d[0] = channel(v, masks[0]);
						d[1] = channel(v, masks[1]);
						d[2] = channel(v, masks[2]);
						d[3] = (pfFlags & 0x1) && masks[3] ? channel(v, masks[3]) : 255;
					}
				}
			}
			out.faces.push_back(std::move(bitmap));
		}
		return {};
	}
	catch (const std::exception& e)
	{
		return Status::error(e.what());
	}

	Status decode_image(const std::span<const std::uint8_t> file, SourceImage& out)
	{
		out = {};
		if (file.size() >= 4 && std::memcmp(file.data(), "DDS ", 4) == 0)
		{
			return decode_dds(file, out);
		}
		out.faces.resize(1);
		if (file.size() >= 8 && std::memcmp(file.data(), PNG_SIGNATURE, 8) == 0)
		{
			return decode_png(file, out.faces[0], &out.hasAlpha);
		}
		return decode_tga(file, out.faces[0], &out.hasAlpha);
	}

	Status load_image(const std::filesystem::path& path, SourceImage& out)
	{
		std::vector<std::uint8_t> file;
		if (const Status s = zone::read_file(path, file); !s)
		{
			return s;
		}
		const Status s = decode_image(file, out);
		return s ? s : Status::error(path.string() + ": " + s.message);
	}

	Status encode_png(const Bitmap& image, std::vector<std::uint8_t>& file)
	try
	{
		file.assign(PNG_SIGNATURE, PNG_SIGNATURE + 8);
		const auto chunk = [&](const char* type, const std::span<const std::uint8_t> data)
		{
			put_be32(file, static_cast<std::uint32_t>(data.size()));
			const std::size_t start = file.size();
			file.insert(file.end(), type, type + 4);
			file.insert(file.end(), data.begin(), data.end());
			put_be32(file, codol::render::crc32({ file.data() + start, file.size() - start }));
		};
		std::vector<std::uint8_t> header;
		put_be32(header, static_cast<std::uint32_t>(image.width));
		put_be32(header, static_cast<std::uint32_t>(image.height));
		header.insert(header.end(), { 8, 6, 0, 0, 0 });
		chunk("IHDR", header);
		const std::size_t stride = static_cast<std::size_t>(image.width) * 4;
		std::vector<std::uint8_t> raw;
		raw.reserve((stride + 1) * image.height);
		for (int y = 0; y < image.height; y++)
		{
			// filter 1 ( sub ) packs photographic content noticeably better than none
			raw.push_back(1);
			const std::uint8_t* row = image.at(0, y);
			for (std::size_t x = 0; x < stride; x++)
			{
				raw.push_back(static_cast<std::uint8_t>(row[x] - (x >= 4 ? row[x - 4] : 0)));
			}
		}
		std::vector<std::uint8_t> packed;
		if (const Status s = zone::zlib_deflate(raw, packed, 6); !s)
		{
			return s;
		}
		chunk("IDAT", packed);
		chunk("IEND", {});
		return {};
	}
	catch (const std::exception& e)
	{
		return Status::error(e.what());
	}

	Status save_png(const std::filesystem::path& path, const Bitmap& image)
	{
		std::vector<std::uint8_t> file;
		if (const Status s = encode_png(image, file); !s)
		{
			return s;
		}
		return zone::write_file(path, file);
	}

	bool uses_alpha(const Bitmap& image)
	{
		for (std::size_t k = 3; k < image.rgba.size(); k += 4)
		{
			if (image.rgba[k] != 255)
			{
				return true;
			}
		}
		return false;
	}

	double psnr(const Bitmap& a, const Bitmap& b, const unsigned channelMask)
	{
		if (a.width != b.width || a.height != b.height || a.rgba.size() != b.rgba.size() || a.rgba.empty())
		{
			return -1;
		}
		double sum = 0;
		std::size_t count = 0;
		for (std::size_t k = 0; k < a.rgba.size(); k++)
		{
			if (channelMask & (1u << (k & 3)))
			{
				const double d = static_cast<double>(a.rgba[k]) - b.rgba[k];
				sum += d * d;
				count++;
			}
		}
		if (!count || sum == 0)
		{
			return 99;
		}
		return 10.0 * std::log10(255.0 * 255.0 / (sum / static_cast<double>(count)));
	}
}
