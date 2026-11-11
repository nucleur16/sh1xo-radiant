// codol_assetconv: make the materials of a CoDOL map usable by radiant.
//
//   codol_assetconv --materials <export>.map.json --root <radiant root> [--max-size 512] [--force]
//
// Reads the material list the asset tool writes next to a map export ( name, technique set, textures with the
// png it exported, blend / alpha test ) and writes for every material that has a colour texture:
//   <root>/raw/images/codol_<image>.iwi     DXT1 / DXT5 with mip maps, at most --max-size pixels wide
//   <root>/raw/materials/<name>             a CoD4 raw material that uses it
// World materials are called "wc/<name>" in CoDOL; brush sides name them without the prefix, so that is the name
// the radiant material gets. Stock materials are never replaced: a file that exists is only rewritten when it was
// written by this tool ( its colour map is a codol_ image ) or with --force.
//
// The render state of each material is taken from a stock CoD4 material of the same kind ( opaque / alpha tested /
// blended: technique sets l_sm_r0c0 / l_sm_t0c0 / l_sm_b0c0 ), found by scanning <root>/raw/materials.

#include <windows.h>
#include <wincodec.h>

#undef min
#undef max

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <set>
#include <unordered_set>
#include <vector>

#include <rapidjson/document.h>

namespace fs = std::filesystem;

namespace
{
	// ------------------------------------------------------------------------------------------
	// files

	bool read_file(const fs::path& path, std::string& out)
	{
		std::ifstream file(path, std::ios::binary);
		if (!file.is_open())
		{
			return false;
		}

		out.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
		return true;
	}

	bool write_file(const fs::path& path, const std::string& data)
	{
		std::error_code ec;
		fs::create_directories(path.parent_path(), ec);

		std::ofstream file(path, std::ios::binary | std::ios::trunc);
		if (!file.is_open())
		{
			return false;
		}

		file.write(data.data(), static_cast<std::streamsize>(data.size()));
		return file.good();
	}

	std::string lower(std::string s)
	{
		for (auto& c : s)
		{
			c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
		}

		return s;
	}

	// ------------------------------------------------------------------------------------------
	// CoD4 raw material ( raw/materials/<name> )

#pragma pack(push, 1)
	struct MaterialInfoRaw
	{
		std::uint32_t nameOffset;
		std::uint32_t refImageNameOffset;
		std::uint8_t gameFlags;
		std::uint8_t sortKey;
		std::uint8_t textureAtlasRowCount;
		std::uint8_t textureAtlasColumnCount;
		float maxDeformMove;
		std::uint8_t deformFlags;
		std::uint8_t usage;
		std::uint16_t toolFlags;
		std::uint32_t locale;
		std::uint16_t autoTexScaleWidth;
		std::uint16_t autoTexScaleHeight;
		float tessSize;
		std::int32_t surfaceFlags;
		std::int32_t contents;
	};

	struct MaterialRaw
	{
		MaterialInfoRaw info;
		std::uint32_t refStateBits[2];
		std::uint16_t textureCount;
		std::uint16_t constantCount;
		std::uint32_t techSetNameOffset;
		std::uint32_t textureTableOffset;
		std::uint32_t constantTableOffset;
	};

	struct MaterialTextureDefRaw
	{
		std::uint32_t nameOffset;
		std::uint8_t samplerState;
		std::uint8_t semantic;
		std::uint16_t pad;
		std::uint32_t imageNameOffset;
	};

	struct MaterialConstantDefRaw
	{
		std::uint32_t nameOffset;
		float literal[4];
	};

	struct IwiHeader
	{
		char tag[3];					// "IWi"
		std::uint8_t version;			// 6
		std::uint8_t format;			// 0x0B DXT1, 0x0D DXT5
		std::uint8_t flags;
		std::uint16_t dimensions[3];
		std::uint32_t fileSizeForPicmip[4];
	};
#pragma pack(pop)

	static_assert(sizeof(MaterialInfoRaw) == 0x28);
	static_assert(sizeof(MaterialRaw) == 0x40);
	static_assert(sizeof(MaterialTextureDefRaw) == 12);
	static_assert(sizeof(MaterialConstantDefRaw) == 20);
	static_assert(sizeof(IwiHeader) == 28);

	enum class kind { opaque, alpha_test, blend };

	struct raw_material
	{
		MaterialRaw header = {};
		std::string techset;
		std::string color_image;
		std::uint8_t color_sampler = 0x0B;
		std::string file_name;	// of the stock material, for its material_properties record
		bool valid = false;
	};

	const char* cstr_at(const std::string& data, const std::uint32_t offset)
	{
		return offset < data.size() ? data.data() + offset : "";
	}

	raw_material parse_material(const std::string& data)
	{
		raw_material out;
		if (data.size() < sizeof(MaterialRaw))
		{
			return out;
		}

		std::memcpy(&out.header, data.data(), sizeof(MaterialRaw));
		out.techset = cstr_at(data, out.header.techSetNameOffset);

		for (std::uint32_t i = 0; i < out.header.textureCount; i++)
		{
			const auto offset = out.header.textureTableOffset + i * sizeof(MaterialTextureDefRaw);
			if (offset + sizeof(MaterialTextureDefRaw) > data.size())
			{
				return out;
			}

			MaterialTextureDefRaw def;
			std::memcpy(&def, data.data() + offset, sizeof(def));

			if (std::strcmp(cstr_at(data, def.nameOffset), "colorMap") == 0)
			{
				out.color_image = cstr_at(data, def.imageNameOffset);
				out.color_sampler = def.samplerState;
			}
		}

		out.valid = !out.techset.empty();
		return out;
	}

	kind kind_of(const std::uint32_t state_bits0)
	{
		// GFXS0: source / destination blend in the low byte ( 0x12 = one, zero ), alpha test in 0x3800 ( 0x800 = off )
		if ((state_bits0 & 0xFF) != 0x12)
		{
			return kind::blend;
		}

		return (state_bits0 & 0x3800) != 0x800 ? kind::alpha_test : kind::opaque;
	}

	std::string build_material(const raw_material& tmpl, const std::string& name, const std::string& image, const int width, const int height)
	{
		// header, 2 textures ( colorMap, normalMap ), 1 constant ( colorTint ), strings
		MaterialRaw header = tmpl.header;
		header.textureCount = 2;
		header.constantCount = 1;
		header.textureTableOffset = sizeof(MaterialRaw);
		header.constantTableOffset = header.textureTableOffset + 2 * sizeof(MaterialTextureDefRaw);
		header.info.textureAtlasRowCount = 1;
		header.info.textureAtlasColumnCount = 1;
		header.info.autoTexScaleWidth = static_cast<std::uint16_t>(width);
		header.info.autoTexScaleHeight = static_cast<std::uint16_t>(height);

		std::string strings;
		const std::uint32_t string_base = header.constantTableOffset + sizeof(MaterialConstantDefRaw);

		const auto add = [&](const std::string& s)
		{
			const auto offset = string_base + static_cast<std::uint32_t>(strings.size());
			strings += s;
			strings += '\0';
			return offset;
		};

		header.techSetNameOffset = add(tmpl.techset);
		header.info.nameOffset = add(name);
		header.info.refImageNameOffset = add(image);

		MaterialTextureDefRaw textures[2] = {};
		textures[0].nameOffset = add("colorMap");
		textures[0].samplerState = tmpl.color_sampler;
		textures[0].semantic = 2;	// TS_COLOR_MAP
		textures[0].imageNameOffset = header.info.refImageNameOffset;
		textures[1].nameOffset = add("normalMap");
		textures[1].samplerState = 0x01;
		textures[1].semantic = 5;	// TS_NORMAL_MAP
		textures[1].imageNameOffset = add("$identitynormalmap");

		MaterialConstantDefRaw tint = {};
		tint.nameOffset = add("colorTint");
		tint.literal[0] = tint.literal[1] = tint.literal[2] = tint.literal[3] = 1.0f;

		std::string out;
		out.append(reinterpret_cast<const char*>(&header), sizeof(header));
		out.append(reinterpret_cast<const char*>(textures), sizeof(textures));
		out.append(reinterpret_cast<const char*>(&tint), sizeof(tint));
		out += strings;
		return out;
	}

	// ------------------------------------------------------------------------------------------
	// images

	struct image
	{
		int width = 0;
		int height = 0;
		std::vector<std::uint8_t> bgra;
	};

	IWICImagingFactory* g_wic = nullptr;

	bool load_png(const fs::path& path, image& out)
	{
		IWICBitmapDecoder* decoder = nullptr;
		if (FAILED(g_wic->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &decoder)))
		{
			return false;
		}

		bool ok = false;
		IWICBitmapFrameDecode* frame = nullptr;
		IWICFormatConverter* converter = nullptr;

		if (SUCCEEDED(decoder->GetFrame(0, &frame)) && SUCCEEDED(g_wic->CreateFormatConverter(&converter))
			&& SUCCEEDED(converter->Initialize(frame, GUID_WICPixelFormat32bppBGRA, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom)))
		{
			UINT w = 0, h = 0;
			converter->GetSize(&w, &h);

			if (w > 0 && h > 0 && w <= 16384 && h <= 16384)
			{
				out.width = static_cast<int>(w);
				out.height = static_cast<int>(h);
				out.bgra.resize(static_cast<std::size_t>(w) * h * 4);
				ok = SUCCEEDED(converter->CopyPixels(nullptr, w * 4, static_cast<UINT>(out.bgra.size()), out.bgra.data()));
			}
		}

		if (converter) { converter->Release(); }
		if (frame) { frame->Release(); }
		decoder->Release();
		return ok;
	}

	image halve(const image& src)
	{
		image out;
		out.width = std::max(1, src.width / 2);
		out.height = std::max(1, src.height / 2);
		out.bgra.resize(static_cast<std::size_t>(out.width) * out.height * 4);

		for (int y = 0; y < out.height; y++)
		{
			for (int x = 0; x < out.width; x++)
			{
				const int x0 = std::min(x * 2, src.width - 1), x1 = std::min(x * 2 + 1, src.width - 1);
				const int y0 = std::min(y * 2, src.height - 1), y1 = std::min(y * 2 + 1, src.height - 1);

				for (int c = 0; c < 4; c++)
				{
					const int sum = src.bgra[(static_cast<std::size_t>(y0) * src.width + x0) * 4 + c] + src.bgra[(static_cast<std::size_t>(y0) * src.width + x1) * 4 + c]
						+ src.bgra[(static_cast<std::size_t>(y1) * src.width + x0) * 4 + c] + src.bgra[(static_cast<std::size_t>(y1) * src.width + x1) * 4 + c];
					out.bgra[(static_cast<std::size_t>(y) * out.width + x) * 4 + c] = static_cast<std::uint8_t>((sum + 2) / 4);
				}
			}
		}

		return out;
	}

	std::uint16_t to565(const int r, const int g, const int b)
	{
		return static_cast<std::uint16_t>(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
	}

	// one 4x4 block ( 16 bgra pixels ) -> 8 bytes of DXT1 colour data, range fit along the bounding box diagonal
	void encode_color_block(const std::uint8_t px[16][4], std::uint8_t out[8])
	{
		int min_c[3] = { 255, 255, 255 }, max_c[3] = { 0, 0, 0 };
		for (int i = 0; i < 16; i++)
		{
			for (int c = 0; c < 3; c++)
			{
				min_c[c] = std::min<int>(min_c[c], px[i][c]);
				max_c[c] = std::max<int>(max_c[c], px[i][c]);
			}
		}

		// px is b g r
		std::uint16_t c0 = to565(max_c[2], max_c[1], max_c[0]);
		std::uint16_t c1 = to565(min_c[2], min_c[1], min_c[0]);

		if (c0 < c1)
		{
			std::swap(c0, c1);
		}

		std::uint32_t indices = 0;

		if (c0 != c1)
		{
			const auto expand = [](const std::uint16_t c, int rgb[3])
			{
				rgb[0] = ((c >> 11) & 31) * 255 / 31;
				rgb[1] = ((c >> 5) & 63) * 255 / 63;
				rgb[2] = (c & 31) * 255 / 31;
			};

			int pal[4][3];
			expand(c0, pal[0]);
			expand(c1, pal[1]);

			for (int c = 0; c < 3; c++)
			{
				pal[2][c] = (2 * pal[0][c] + pal[1][c]) / 3;
				pal[3][c] = (pal[0][c] + 2 * pal[1][c]) / 3;
			}

			for (int i = 0; i < 16; i++)
			{
				int best = 0, best_dist = INT_MAX;
				for (int p = 0; p < 4; p++)
				{
					const int dr = px[i][2] - pal[p][0], dg = px[i][1] - pal[p][1], db = px[i][0] - pal[p][2];
					const int dist = dr * dr + dg * dg + db * db;

					if (dist < best_dist)
					{
						best_dist = dist;
						best = p;
					}
				}

				indices |= static_cast<std::uint32_t>(best) << (i * 2);
			}
		}

		out[0] = static_cast<std::uint8_t>(c0 & 0xFF);
		out[1] = static_cast<std::uint8_t>(c0 >> 8);
		out[2] = static_cast<std::uint8_t>(c1 & 0xFF);
		out[3] = static_cast<std::uint8_t>(c1 >> 8);
		std::memcpy(out + 4, &indices, 4);
	}

	void encode_alpha_block(const std::uint8_t px[16][4], std::uint8_t out[8])
	{
		int a0 = 0, a1 = 255;
		for (int i = 0; i < 16; i++)
		{
			a0 = std::max<int>(a0, px[i][3]);
			a1 = std::min<int>(a1, px[i][3]);
		}

		out[0] = static_cast<std::uint8_t>(a0);
		out[1] = static_cast<std::uint8_t>(a1);

		std::uint64_t bits = 0;

		if (a0 != a1)
		{
			// 8 value mode ( a0 > a1 ): a0, a1, then 6 interpolated
			int pal[8];
			pal[0] = a0;
			pal[1] = a1;
			for (int k = 1; k <= 6; k++)
			{
				pal[k + 1] = ((7 - k) * a0 + k * a1) / 7;
			}

			for (int i = 0; i < 16; i++)
			{
				int best = 0, best_dist = INT_MAX;
				for (int p = 0; p < 8; p++)
				{
					const int dist = std::abs(px[i][3] - pal[p]);
					if (dist < best_dist)
					{
						best_dist = dist;
						best = p;
					}
				}

				bits |= static_cast<std::uint64_t>(best) << (i * 3);
			}
		}

		for (int k = 0; k < 6; k++)
		{
			out[2 + k] = static_cast<std::uint8_t>((bits >> (8 * k)) & 0xFF);
		}
	}

	std::string encode_dxt(const image& img, const bool with_alpha)
	{
		const int bw = std::max(1, (img.width + 3) / 4), bh = std::max(1, (img.height + 3) / 4);
		std::string out;
		out.resize(static_cast<std::size_t>(bw) * bh * (with_alpha ? 16 : 8));

		std::size_t pos = 0;
		for (int by = 0; by < bh; by++)
		{
			for (int bx = 0; bx < bw; bx++)
			{
				std::uint8_t px[16][4];
				for (int y = 0; y < 4; y++)
				{
					for (int x = 0; x < 4; x++)
					{
						const int sx = std::min(bx * 4 + x, img.width - 1), sy = std::min(by * 4 + y, img.height - 1);
						std::memcpy(px[y * 4 + x], &img.bgra[(static_cast<std::size_t>(sy) * img.width + sx) * 4], 4);
					}
				}

				auto* dst = reinterpret_cast<std::uint8_t*>(out.data()) + pos;

				if (with_alpha)
				{
					encode_alpha_block(px, dst);
					encode_color_block(px, dst + 8);
					pos += 16;
				}
				else
				{
					encode_color_block(px, dst);
					pos += 8;
				}
			}
		}

		return out;
	}

	// IWI version 6: header, then the mip chain from the smallest level to the largest
	bool write_iwi(const fs::path& path, image img, const int max_size, const bool with_alpha, int& out_w, int& out_h)
	{
		while (img.width > max_size || img.height > max_size)
		{
			img = halve(img);
		}

		out_w = img.width;
		out_h = img.height;

		std::vector<std::string> mips; // largest first
		for (image level = img;; level = halve(level))
		{
			mips.push_back(encode_dxt(level, with_alpha));

			if (level.width == 1 && level.height == 1)
			{
				break;
			}
		}

		std::size_t total = sizeof(IwiHeader);
		for (const auto& mip : mips)
		{
			total += mip.size();
		}

		IwiHeader header = {};
		header.tag[0] = 'I'; header.tag[1] = 'W'; header.tag[2] = 'i';
		header.version = 6;
		header.format = with_alpha ? 0x0D : 0x0B;
		header.flags = 0;
		header.dimensions[0] = static_cast<std::uint16_t>(img.width);
		header.dimensions[1] = static_cast<std::uint16_t>(img.height);
		header.dimensions[2] = 1;

		// size of the file when the first n levels are left out
		std::size_t size = total;
		for (int p = 0; p < 4; p++)
		{
			header.fileSizeForPicmip[p] = static_cast<std::uint32_t>(size);

			if (p < static_cast<int>(mips.size()) - 1)
			{
				size -= mips[p].size();
			}
		}

		std::string data;
		data.reserve(total);
		data.append(reinterpret_cast<const char*>(&header), sizeof(header));

		for (auto it = mips.rbegin(); it != mips.rend(); ++it)
		{
			data += *it;
		}

		return write_file(path, data);
	}

	bool has_alpha(const image& img)
	{
		for (std::size_t i = 3; i < img.bgra.size(); i += 4)
		{
			if (img.bgra[i] < 250)
			{
				return true;
			}
		}

		return false;
	}

	// ------------------------------------------------------------------------------------------

	const rapidjson::Value* member(const rapidjson::Value& obj, const char* name)
	{
		if (!obj.IsObject())
		{
			return nullptr;
		}

		const auto it = obj.FindMember(name);
		return it == obj.MemberEnd() ? nullptr : &it->value;
	}

	std::string str(const rapidjson::Value& obj, const char* name)
	{
		const auto* v = member(obj, name);
		return (v && v->IsString()) ? v->GetString() : "";
	}

	int fail(const std::string& message)
	{
		std::fprintf(stderr, "codol_assetconv: %s\n", message.c_str());
		return 1;
	}

	// ------------------------------------------------------------------------------------------
	// shared state of one run

	struct converter
	{
		fs::path raw;
		int max_size = 512;
		bool force = false;
		std::map<kind, raw_material> templates;

		std::map<std::string, std::pair<int, int>> converted;	// iwi name -> size
		std::unordered_set<std::string> alpha_images;

		int written = 0, kept_stock = 0, no_texture = 0, failed = 0, images = 0;

		bool load_templates()
		{
			std::error_code ec;
			for (auto it = fs::directory_iterator(raw / "materials", ec); !ec && it != fs::directory_iterator(); it.increment(ec))
			{
				if (!it->is_regular_file(ec) || it->file_size(ec) > 4096)
				{
					continue;
				}

				std::string data;
				if (!read_file(it->path(), data))
				{
					continue;
				}

				auto mat = parse_material(data);
				if (!mat.valid || mat.color_image.starts_with("codol_"))
				{
					continue;
				}

				// plain lit materials with only a colour map. The raw technique set names carry the kind:
				// l_sm_r0c0 replace ( opaque ), l_sm_t0c0 alpha test, l_sm_b0c0 blend
				kind k;
				if (mat.techset == "l_sm_r0c0") { k = kind::opaque; }
				else if (mat.techset == "l_sm_t0c0") { k = kind::alpha_test; }
				else if (mat.techset == "l_sm_b0c0") { k = kind::blend; }
				else { continue; }

				mat.file_name = it->path().filename().string();
				templates.try_emplace(k, mat);

				if (templates.size() == 3)
				{
					break;
				}
			}

			return templates.contains(kind::opaque);
		}

		// image names are limited ( 42 characters with the extension for converted images )
		static std::string iwi_name_for(const std::string& image)
		{
			std::string name = "codol_" + lower(image);
			for (auto& c : name)
			{
				if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-' || c == '~' || c == '$'))
				{
					c = '_';
				}
			}

			if (name.size() > 36)
			{
				char hash[16];
				std::snprintf(hash, sizeof(hash), "_%08x", static_cast<unsigned>(std::hash<std::string>{}(name)));
				name = name.substr(0, 27) + hash;
			}

			return name;
		}

		// png -> iwi ( once per image ) and the raw material that uses it. false: nothing was written
		bool emit(const std::string& radiant_name, const std::string& image_name, const fs::path& png, kind wanted)
		{
			const fs::path material_path = raw / "materials" / fs::u8path(radiant_name);

			// never replace a stock material
			if (!force)
			{
				std::string existing;
				if (read_file(material_path, existing) && !parse_material(existing).color_image.starts_with("codol_"))
				{
					kept_stock++;
					return false;
				}
			}

			const std::string iwi_name = iwi_name_for(image_name);
			int width = 0, height = 0;

			if (const auto done = converted.find(iwi_name); done != converted.end())
			{
				width = done->second.first;
				height = done->second.second;
			}
			else
			{
				image img;
				if (!load_png(png, img))
				{
					std::fprintf(stderr, "codol_assetconv: can not decode %s\n", png.string().c_str());
					failed++;
					return false;
				}

				const bool alpha = has_alpha(img);
				if (!write_iwi(raw / "images" / (iwi_name + ".iwi"), std::move(img), max_size, alpha, width, height))
				{
					failed++;
					return false;
				}

				if (alpha)
				{
					alpha_images.insert(iwi_name);
				}

				converted[iwi_name] = { width, height };
				images++;
			}

			if (wanted != kind::opaque && !alpha_images.contains(iwi_name))
			{
				wanted = kind::opaque; // no alpha channel to test or blend with
			}

			auto tmpl = templates.find(wanted);
			if (tmpl == templates.end())
			{
				tmpl = templates.find(kind::opaque);
			}

			if (!write_file(material_path, build_material(tmpl->second, radiant_name, iwi_name, width, height)))
			{
				failed++;
				return false;
			}

			// the mod tools ( xmodel conversion, map compile ) read contents / surface flags from this record
			std::string props;
			if (read_file(raw / "material_properties" / tmpl->second.file_name, props) && props.size() == 16)
			{
				std::memcpy(props.data() + 4, &tmpl->second.header.info.contents, 4);
				std::memcpy(props.data() + 8, &tmpl->second.header.info.surfaceFlags, 4);
				write_file(raw / "material_properties" / fs::u8path(radiant_name), props);
			}

			written++;
			return true;
		}
	};

	// run a program hidden, wait, capture stdout + stderr. -1: could not start, -2: timeout
	int run(const std::wstring& command_line, const fs::path& cwd, std::string& output, const DWORD timeout_ms)
	{
		SECURITY_ATTRIBUTES sa = { sizeof(sa), nullptr, TRUE };
		HANDLE read_pipe = nullptr, write_pipe = nullptr;

		if (!CreatePipe(&read_pipe, &write_pipe, &sa, 0))
		{
			return -1;
		}

		SetHandleInformation(read_pipe, HANDLE_FLAG_INHERIT, 0);

		// no stdin: the mod tools wait for a key otherwise
		HANDLE nul = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, 0, nullptr);

		STARTUPINFOW si = {};
		si.cb = sizeof(si);
		si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
		si.hStdInput = nul;
		si.hStdOutput = write_pipe;
		si.hStdError = write_pipe;
		si.wShowWindow = SW_HIDE;

		PROCESS_INFORMATION pi = {};
		std::wstring mutable_cmd = command_line;

		const BOOL ok = CreateProcessW(nullptr, mutable_cmd.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr,
			cwd.empty() ? nullptr : cwd.c_str(), &si, &pi);

		CloseHandle(write_pipe);
		CloseHandle(nul);

		if (!ok)
		{
			CloseHandle(read_pipe);
			return -1;
		}

		// reader thread, so a full pipe never blocks the child while we wait with a timeout
		std::string captured;
		HANDLE reader = CreateThread(nullptr, 0, [](LPVOID param) -> DWORD
		{
			auto* ctx = static_cast<std::pair<HANDLE, std::string*>*>(param);
			char buf[4096];
			DWORD read = 0;

			while (ReadFile(ctx->first, buf, sizeof(buf), &read, nullptr) && read)
			{
				ctx->second->append(buf, read);
			}

			return 0;
		}, new std::pair<HANDLE, std::string*>(read_pipe, &captured), 0, nullptr);

		int code;
		if (WaitForSingleObject(pi.hProcess, timeout_ms) == WAIT_TIMEOUT)
		{
			TerminateProcess(pi.hProcess, 1);
			code = -2;
		}
		else
		{
			DWORD exit_code = 0;
			GetExitCodeProcess(pi.hProcess, &exit_code);
			code = static_cast<int>(exit_code);
		}

		WaitForSingleObject(reader, 5000);
		CloseHandle(reader);
		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);
		CloseHandle(read_pipe);

		output += captured;
		return code;
	}

	std::wstring widen(const std::string& s)
	{
		return fs::u8path(s).wstring();
	}

	std::string gdt_entry(const std::string& name)
	{
		static const char* keys[][2] =
		{
			{ "type", "rigid" }, { "filename", nullptr }, { "highLodDist", "0" }, { "forceMediumLodRigid", "0" }, { "mediumLod", "" },
			{ "mediumLodDist", "0" }, { "forceLowLodRigid", "0" }, { "lowLod", "" }, { "lowLodDist", "0" }, { "forceLowestLodRigid", "0" },
			{ "lowestLod", "" }, { "lowestLodDist", "0" }, { "groundLit", "0" }, { "noRadialNormals", "0" }, { "radialOriginX", "0" },
			{ "radialOriginY", "0" }, { "radialOriginZ", "0" }, { "allowHidingAllParts", "0" }, { "collisionLOD", "None" }, { "hitBoxModel", "" },
			{ "skinOverride", "" }, { "boneControllers", "" }, { "boneStabilizers", "" }, { "physicsPreset", "" },
		};

		std::string out = "\t\"" + name + "\" ( \"xmodel.gdf\" )\r\n\t{\r\n";
		for (const auto& kv : keys)
		{
			out += std::string("\t\t\"") + kv[0] + "\" \"" + (kv[1] ? std::string(kv[1]) : "codol\\\\" + name + ".XMODEL_EXPORT") + "\"\r\n";
		}

		return out + "\t}\r\n";
	}

	// model names a map uses: static models of the export + "model" keys of the entities
	void collect_models(const std::string& smodels_json, const std::string& ents_path, std::set<std::string>& names)
	{
		std::string text;
		if (!smodels_json.empty() && read_file(fs::u8path(smodels_json), text))
		{
			rapidjson::Document doc;
			doc.Parse(text.data(), text.size());

			if (const auto* objects = doc.IsObject() ? member(doc, "objects") : nullptr; objects && objects->IsArray())
			{
				for (const auto& obj : objects->GetArray())
				{
					if (auto asset = str(obj, "asset"); !asset.empty())
					{
						names.insert(lower(asset));
					}
				}
			}
		}

		if (!ents_path.empty() && read_file(fs::u8path(ents_path), text))
		{
			static const std::string key = "\"model\" \"";
			for (auto pos = text.find(key); pos != std::string::npos; pos = text.find(key, pos + 1))
			{
				const auto start = pos + key.size();
				const auto end = text.find('"', start);

				if (end != std::string::npos && end > start && text[start] != '*' && text[start] != '?')
				{
					names.insert(lower(text.substr(start, end - start)));
				}
			}
		}
	}

	// CoDOL models -> CoD4 raw xmodels through the mod tools:
	// asset tool ( XMODEL_EXPORT + png ) -> materials + GDT entry -> converter.exe -single xmodel <name>
	int convert_models(converter& conv, const fs::path& root, const std::set<std::string>& wanted, const std::string& tool,
		const std::string& game, const fs::path& work)
	{
		std::vector<std::string> todo;
		int have = 0;

		for (const auto& name : wanted)
		{
			std::error_code ec;
			if (!conv.force && fs::exists(root / "raw" / "xmodel" / fs::u8path(name), ec))
			{
				have++; // stock CoD4 model of that name, or converted earlier
				continue;
			}

			todo.push_back(name);
		}

		int exported = 0, compiled = 0, model_failed = 0;
		const fs::path export_dir = root / "model_export" / "codol";
		std::error_code ec;
		fs::create_directories(export_dir, ec);

		// 1. export in batches ( command line length )
		for (std::size_t i = 0; i < todo.size(); i += 40)
		{
			std::wstring cmd = L"\"" + widen(tool) + L"\" export-models --game \"" + widen(game) + L"\" --out \"" + work.wstring()
				+ L"\" --format xmodel_export --lod0";

			for (std::size_t k = i; k < std::min(todo.size(), i + 40); k++)
			{
				cmd += L" --filter " + widen(todo[k]);
			}

			std::string output;
			if (const int code = run(cmd, {}, output, 600000); code != 0)
			{
				std::fprintf(stderr, "codol_assetconv: the asset tool failed ( %d ) exporting models\n%s\n", code, output.c_str());
			}
		}

		// 2. materials, export text with the radiant material names, GDT
		std::vector<std::string> ready;

		for (const auto& name : todo)
		{
			const fs::path dir = work / "models" / fs::u8path(name);
			std::string text;

			if (!read_file(dir / (name + ".xmodel_export"), text))
			{
				model_failed++; // not a model of this game ( eg. a CoD4 only name in a ported script )
				continue;
			}

			exported++;

			// MATERIAL <n> "<name>" "<type>" "color_map:<file>"
			std::string rewritten;
			rewritten.reserve(text.size());
			std::size_t pos = 0;

			while (pos < text.size())
			{
				auto end = text.find('\n', pos);
				if (end == std::string::npos)
				{
					end = text.size();
				}

				std::string line = text.substr(pos, end - pos);

				if (line.starts_with("MATERIAL "))
				{
					const auto q1 = line.find('"');
					const auto q2 = (q1 == std::string::npos) ? q1 : line.find('"', q1 + 1);

					if (q2 != std::string::npos)
					{
						const std::string game_name = line.substr(q1 + 1, q2 - q1 - 1);

						std::string radiant_name = "codol_" + lower(game_name);
						for (auto& c : radiant_name)
						{
							if (c == '/' || c == '\\' || c == ' ')
							{
								c = '_';
							}
						}

						static const std::string color_key = "color_map:";
						if (const auto ck = line.find(color_key); ck != std::string::npos)
						{
							const auto file_end = line.find('"', ck);
							const std::string file = line.substr(ck + color_key.size(), file_end == std::string::npos ? std::string::npos : file_end - ck - color_key.size());

							std::string image = file;
							if (const auto slash = image.find_last_of('/'); slash != std::string::npos)
							{
								image.erase(0, slash + 1);
							}

							if (const auto dot = image.find_last_of('.'); dot != std::string::npos)
							{
								image.erase(dot);
							}

							// only a real texture file: a material without a colour map has nothing after the key
							if (lower(file).ends_with(".png"))
							{
								conv.emit(radiant_name, image, dir / fs::u8path(file), kind::alpha_test);
							}
						}

						// a material the converter can not find aborts the model
						if (!fs::exists(conv.raw / "material_properties" / fs::u8path(radiant_name), ec))
						{
							radiant_name = conv.templates.at(kind::opaque).file_name; // a stock material that has a record
						}

						line = line.substr(0, q1 + 1) + radiant_name + line.substr(q2);
					}
				}

				rewritten += line;
				rewritten += '\n';
				pos = end + 1;
			}

			if (write_file(export_dir / (name + ".XMODEL_EXPORT"), rewritten))
			{
				ready.push_back(name);
			}
		}

		// the GDT lists every model that was ever exported here
		{
			std::string gdt = "{\r\n";
			for (auto it = fs::directory_iterator(export_dir, ec); !ec && it != fs::directory_iterator(); it.increment(ec))
			{
				if (lower(it->path().extension().string()) == ".xmodel_export")
				{
					gdt += gdt_entry(it->path().stem().string());
				}
			}

			gdt += "}\r\n";
			write_file(root / "source_data" / "codol_models.gdt", gdt);
		}

		// 3. compile
		for (const auto& name : ready)
		{
			std::string output;
			const int code = run(L"\"" + (root / "bin" / "converter.exe").wstring() + L"\" -nopause -nocachedownload -single xmodel " + widen(name),
				root / "bin", output, 180000);

			if (fs::exists(root / "raw" / "xmodel" / fs::u8path(name), ec))
			{
				compiled++;
			}
			else
			{
				model_failed++;
				const auto err = output.find("ERROR");
				std::fprintf(stderr, "codol_assetconv: %s was not converted ( exit %d ): %s\n", name.c_str(), code,
					err == std::string::npos ? "no output" : output.substr(err, 300).c_str());
			}
		}

		std::printf("models: %d of %zu converted, %d already in raw/xmodel, %d exported, %d failed\n",
			compiled, wanted.size(), have, exported, model_failed);

		return 0;
	}
}

int main(int argc, char** argv)
{
	std::string materials_path, root, models_json, ents_path, tool, game, work;
	converter conv;

	for (int i = 1; i < argc; i++)
	{
		const std::string key = argv[i];

		if (key == "--force") { conv.force = true; }
		else if (i + 1 >= argc) { return fail("missing value for " + key); }
		else if (key == "--materials") { materials_path = argv[++i]; }
		else if (key == "--root") { root = argv[++i]; }
		else if (key == "--max-size") { conv.max_size = std::clamp(std::atoi(argv[++i]), 16, 4096); }
		else if (key == "--models") { models_json = argv[++i]; }
		else if (key == "--ents") { ents_path = argv[++i]; }
		else if (key == "--tool") { tool = argv[++i]; }
		else if (key == "--game") { game = argv[++i]; }
		else if (key == "--work") { work = argv[++i]; }
		else { return fail("unknown option " + key); }
	}

	const bool do_models = !models_json.empty() || !ents_path.empty();

	if (root.empty() || (materials_path.empty() && !do_models) || (do_models && (tool.empty() || game.empty() || work.empty())))
	{
		return fail("usage: codol_assetconv --root <radiant root> [--materials <export>.map.json] [--max-size 512] [--force]\n"
			"                       [--models <smodels>.map.json] [--ents <entities>] --tool <CodolSoundDumper.exe> --game <install> --work <folder>");
	}

	conv.raw = fs::u8path(root) / "raw";

	if (!conv.load_templates())
	{
		return fail("no stock l_sm_r0c0 material found in " + (conv.raw / "materials").string() + " to take the render state from");
	}

	if (FAILED(CoInitializeEx(nullptr, COINIT_MULTITHREADED))
		|| FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&g_wic))))
	{
		return fail("could not start the Windows imaging component");
	}

	if (!materials_path.empty())
	{
		const fs::path export_dir = fs::u8path(materials_path).parent_path();

		std::string json;
		if (!read_file(fs::u8path(materials_path), json))
		{
			return fail("can not read " + materials_path);
		}

		rapidjson::Document doc;
		doc.Parse(json.data(), json.size());

		const auto* materials = doc.IsObject() ? member(doc, "materials") : nullptr;
		if (!materials || !materials->IsArray())
		{
			return fail(materials_path + " has no \"materials\" array");
		}

		for (const auto& mat : materials->GetArray())
		{
			const std::string game_name = str(mat, "name");
			if (game_name.empty())
			{
				continue;
			}

			std::string image_name, image_file;
			if (const auto* textures = member(mat, "textures"); textures && textures->IsArray())
			{
				for (const auto& tex : textures->GetArray())
				{
					if (str(tex, "semantic") == "color" && !str(tex, "file").empty())
					{
						image_name = str(tex, "image");
						image_file = str(tex, "file");
						break;
					}
				}
			}

			if (image_name.empty())
			{
				conv.no_texture++;
				continue;
			}

			// render state: what the game uses, limited by what the texture can do
			const std::string blend = str(mat, "blend");
			const std::string alpha_test = str(mat, "alphaTest");

			kind k = kind::opaque;
			if (!blend.empty() && blend != "Opaque" && blend != "None")
			{
				k = kind::blend;
			}
			else if (!alpha_test.empty() && alpha_test != "None" && alpha_test != "Disabled" && alpha_test != "Always")
			{
				k = kind::alpha_test;
			}

			// world materials are "wc/<name>" in the game, brush sides name them without the prefix
			conv.emit(game_name.starts_with("wc/") ? game_name.substr(3) : game_name, image_name, export_dir / fs::u8path(image_file), k);
		}
	}

	if (do_models)
	{
		std::set<std::string> names;
		collect_models(models_json, ents_path, names);
		convert_models(conv, fs::u8path(root), names, tool, game, fs::u8path(work));
	}

	g_wic->Release();
	CoUninitialize();

	std::printf("%d materials written ( %d images ), %d stock materials kept, %d without a colour texture, %d failed -> %s\n",
		conv.written, conv.images, conv.kept_stock, conv.no_texture, conv.failed, conv.raw.string().c_str());

	return 0;
}