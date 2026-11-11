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
}

int main(int argc, char** argv)
{
	std::string materials_path, root;
	int max_size = 512;
	bool force = false;

	for (int i = 1; i < argc; i++)
	{
		const std::string key = argv[i];

		if (key == "--force") { force = true; }
		else if (i + 1 >= argc) { return fail("missing value for " + key); }
		else if (key == "--materials") { materials_path = argv[++i]; }
		else if (key == "--root") { root = argv[++i]; }
		else if (key == "--max-size") { max_size = std::clamp(std::atoi(argv[++i]), 16, 4096); }
		else { return fail("unknown option " + key); }
	}

	if (materials_path.empty() || root.empty())
	{
		return fail("usage: codol_assetconv --materials <export>.map.json --root <radiant root> [--max-size 512] [--force]");
	}

	const fs::path raw = fs::u8path(root) / "raw";
	const fs::path export_dir = fs::u8path(materials_path).parent_path();

	// templates: stock materials by ( world / model, kind )
	std::map<std::pair<bool, kind>, raw_material> templates;
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

			const auto mat = parse_material(data);
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

			// world textures only ( usage / surface flags of a wall ), not weapons or decals: sort key of the default opaque / blend layers
			templates.try_emplace({ true, k }, mat);

			if (templates.size() == 3)
			{
				break;
			}
		}
	}

	if (!templates.contains({ true, kind::opaque }))
	{
		return fail("no stock l_sm_r0c0 material found in " + (raw / "materials").string() + " to take the render state from");
	}

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

	if (FAILED(CoInitializeEx(nullptr, COINIT_MULTITHREADED))
		|| FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&g_wic))))
	{
		return fail("could not start the Windows imaging component");
	}

	int written = 0, kept_stock = 0, no_texture = 0, failed = 0, images = 0;
	std::map<std::string, std::pair<int, int>> converted; // image -> size
	std::unordered_set<std::string> alpha_images;

	for (const auto& mat : materials->GetArray())
	{
		const std::string game_name = str(mat, "name");
		if (game_name.empty())
		{
			continue;
		}

		// colour texture
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
			no_texture++;
			continue;
		}

		const std::string radiant_name = game_name.starts_with("wc/") ? game_name.substr(3) : game_name;
		const fs::path material_path = raw / "materials" / fs::u8path(radiant_name);

		// never replace a stock material
		if (!force)
		{
			std::string existing;
			if (read_file(material_path, existing) && !parse_material(existing).color_image.starts_with("codol_"))
			{
				kept_stock++;
				continue;
			}
		}

		const std::string iwi_name = "codol_" + lower(image_name);
		int width = 0, height = 0;

		if (const auto done = converted.find(iwi_name); done != converted.end())
		{
			width = done->second.first;
			height = done->second.second;
		}
		else
		{
			image img;
			if (!load_png(export_dir / fs::u8path(image_file), img))
			{
				std::fprintf(stderr, "codol_assetconv: can not decode %s\n", image_file.c_str());
				failed++;
				continue;
			}

			const bool alpha = has_alpha(img);
			if (!write_iwi(raw / "images" / (iwi_name + ".iwi"), std::move(img), max_size, alpha, width, height))
			{
				failed++;
				continue;
			}

			if (alpha)
			{
				alpha_images.insert(iwi_name);
			}

			converted[iwi_name] = { width, height };
			images++;
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

		if (k != kind::opaque && !alpha_images.contains(iwi_name))
		{
			k = kind::opaque; // no alpha channel to test or blend with
		}

		auto tmpl = templates.find({ true, k });
		if (tmpl == templates.end())
		{
			tmpl = templates.find({ true, kind::opaque });
		}

		if (!write_file(material_path, build_material(tmpl->second, radiant_name, iwi_name, width, height)))
		{
			failed++;
			continue;
		}

		written++;
	}

	g_wic->Release();
	CoUninitialize();

	std::printf("%d materials written ( %d images ), %d stock materials kept, %d without a colour texture, %d failed -> %s\n",
		written, images, kept_stock, no_texture, failed, raw.string().c_str());

	return 0;
}
