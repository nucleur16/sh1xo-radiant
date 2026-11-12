// codol_asset_probe: research aid of codol_assets. Prints how shipped assets are laid out in the zone stream and
// writes surveys of the shipped images / materials / models as tab separated tables. The rules in codol_assets
// ( which format, flags, technique set, state bits .. go with which kind of asset ) were derived from its output.
//
//   codol_asset_probe graph <zone.ff> <C1.exe> <type> <name> [max pieces]     stream order of an asset's data
//   codol_asset_probe images <folder> <C1.exe> <out.tsv>                      every image of every zone
//   codol_asset_probe materials <folder> <C1.exe> <out.tsv>                   every material of every zone
//   codol_asset_probe models <folder> <C1.exe> <out.tsv>                      every xmodel of every zone
//   codol_asset_probe iwis <install> <C1.exe> <out.tsv>                       header of every .iwi of the image archives

#include "codol_zone/assets.hpp"
#include "codol_zone/zone_editor.hpp"
#include "codol_render/iwd.hpp"
#include "codol_render/iwi.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <set>
#include <string>

using namespace codol::zone;
namespace c1 = codol::c1;
namespace fs = std::filesystem;

namespace
{
	std::string hex(const std::span<const std::uint8_t> data, const std::size_t limit = SIZE_MAX)
	{
		std::string out;
		char t[4];
		for (std::size_t k = 0; k < data.size() && k < limit; k++)
		{
			std::snprintf(t, sizeof(t), "%02X", data[k]);
			out += t;
		}
		return out;
	}

	std::string describe(const ZoneEditor& editor, const ZoneTarget& t)
	{
		char text[256];
		ZoneDataInfo info;
		switch (t.kind)
		{
		case ZoneTarget::Kind::data:
			editor.data_info(t.data, info);
			std::snprintf(text, sizeof(text), "data( asset %zd piece %u, %u bytes )+%u", static_cast<std::ptrdiff_t>(info.asset), info.index, info.size, t.offset);
			break;
		case ZoneTarget::Kind::asset:
			std::snprintf(text, sizeof(text), "asset %zu %s '%s'+%u", t.asset, asset_type_name(editor.asset(t.asset).type).c_str(), editor.asset(t.asset).name.c_str(), t.offset);
			break;
		case ZoneTarget::Kind::allocation:
			std::snprintf(text, sizeof(text), "allocation %u of asset %zd+%u", t.allocation, static_cast<std::ptrdiff_t>(t.asset), t.offset);
			break;
		case ZoneTarget::Kind::raw:
			std::snprintf(text, sizeof(text), "raw 0x%08X", t.raw);
			break;
		default:
			std::snprintf(text, sizeof(text), "plain");
			break;
		}
		return text;
	}

	int graph(const fs::path& ff, const ClientKey& key, const std::string& typeName, const std::string& name, const std::uint32_t limit)
	{
		ZoneEditor editor;
		if (const Status s = editor.open(ff, key); !s)
		{
			std::fprintf(stderr, "%s\n", s.message.c_str());
			return 1;
		}
		const std::int32_t type = asset_type_from_name(typeName);
		const Zone& zone = editor.zone();
		zaddr header = 0;
		for (const AssetRecord& r : zone.all_assets())
		{
			if (r.type == type && zone.asset_name(r.type, r.header) == name && !zone.reference(r.header))
			{
				header = r.header;
				std::printf("found in top level asset #%d\n", r.owner);
				break;
			}
		}
		const ZoneDataId first = editor.header_data(header);
		ZoneDataInfo info;
		if (!first || !editor.data_info(first, info))
		{
			std::fprintf(stderr, "no such asset with data in this zone\n");
			return 1;
		}
		const EditorAsset owner = editor.asset(info.asset);
		std::printf("owner: asset #%zu %s '%s', %u pieces; header is piece %u\n", info.asset, asset_type_name(owner.type).c_str(), owner.name.c_str(), owner.chunks, info.index);
		for (std::uint32_t k = info.index; k < owner.chunks && k < info.index + limit; k++)
		{
			const ZoneDataId id = editor.asset_data(info.asset, k);
			ZoneDataInfo piece;
			editor.data_info(id, piece);
			const auto bytes = editor.data_bytes(id);
			std::printf("piece %u: %u bytes @%08X%s", k, piece.size, piece.address, piece.string ? " string" : "");
			if (piece.string)
			{
				std::printf(" \"%.*s\"\n", static_cast<int>(bytes.size() ? bytes.size() - 1 : 0), reinterpret_cast<const char*>(bytes.data()));
			}
			else
			{
				std::printf(" %s%s\n", hex(bytes, 96).c_str(), bytes.size() > 96 ? ".." : "");
			}
			for (const ZonePointer& p : editor.data_pointers(id))
			{
				std::printf("      +%u -> %s\n", p.offset, describe(editor, p.target).c_str());
			}
		}
		return 0;
	}

	std::vector<fs::path> zones_of(const fs::path& folder)
	{
		std::vector<fs::path> out;
		if (fs::is_regular_file(folder))
		{
			out.push_back(folder);
			return out;
		}
		for (const char* sub : { "client_shipRetail/chinese", "client_shipRetail/patch", "" })
		{
			const fs::path dir = folder / sub;
			std::error_code ec;
			if (!*sub && !out.empty())
			{
				break;
			}
			for (const auto& e : fs::directory_iterator(dir, ec))
			{
				if (e.is_regular_file() && e.path().extension() == ".ff")
				{
					out.push_back(e.path());
				}
			}
		}
		std::sort(out.begin(), out.end());
		return out;
	}

	int images(const fs::path& folder, const ClientKey& key, const fs::path& out)
	{
		std::FILE* f = std::fopen(out.string().c_str(), "wb");
		if (!f)
		{
			return 1;
		}
		std::fprintf(f, "zone\tname\ttop\tmapType\tsemantic\tcategory\twidth\theight\tdepth\tdelayLoad\tstreamed\tlevels\tflags\tformat\tsize\theader\tloaddef\n");
		for (const fs::path& ff : zones_of(folder))
		{
			Zone zone;
			if (!zone.open(ff, key))
			{
				continue;
			}
			std::set<zaddr> top;
			for (const XAssetEntry& e : zone.assets())
			{
				top.insert(e.header);
			}
			for (const AssetRecord& r : zone.all_assets())
			{
				if (r.type != c1::ASSET_TYPE_IMAGE || zone.reference(r.header))
				{
					continue;
				}
				ImageView v;
				if (!read_image(zone, r.header, v) || v.isReference)
				{
					continue;
				}
				std::fprintf(f, "%s\t%.*s\t%d\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%d\t%u\t%08X\t%u\t%u\t%s\t%s\n", ff.stem().string().c_str(),
					static_cast<int>(v.name.size()), v.name.data(), top.count(r.header) ? 1 : 0, v.raw.mapType, v.raw.semantic, v.raw.category, v.raw.width,
					v.raw.height, v.raw.depth, v.raw.delayLoad, v.streamed ? 1 : 0, v.loadDef.levelCount, v.loadDef.flags, v.loadDef.format,
					v.loadDef.resourceSize, hex({ reinterpret_cast<const std::uint8_t*>(&v.raw), sizeof(v.raw) }).c_str(),
					hex({ reinterpret_cast<const std::uint8_t*>(&v.loadDef), sizeof(v.loadDef) }).c_str());
			}
		}
		std::fclose(f);
		return 0;
	}

	int materials(const fs::path& folder, const ClientKey& key, const fs::path& out)
	{
		std::FILE* f = std::fopen(out.string().c_str(), "wb");
		if (!f)
		{
			return 1;
		}
		std::fprintf(f, "zone\tname\ttop\ttechset\theader\ttextures\tconstants\tstatebits\n");
		for (const fs::path& ff : zones_of(folder))
		{
			Zone zone;
			if (!zone.open(ff, key))
			{
				continue;
			}
			std::set<zaddr> top;
			for (const XAssetEntry& e : zone.assets())
			{
				top.insert(e.header);
			}
			for (const AssetRecord& r : zone.all_assets())
			{
				if (r.type != c1::ASSET_TYPE_MATERIAL || zone.reference(r.header))
				{
					continue;
				}
				MaterialView v;
				if (!read_material(zone, r.header, v) || v.isReference)
				{
					continue;
				}
				std::string textures, constants, state;
				char t[256];
				for (const MaterialTexture& tex : v.textures)
				{
					ImageView image;
					const bool have = tex.raw.semantic != 11 && read_image(zone, tex.image, image);
					std::snprintf(t, sizeof(t), "%08X:%c%c:%02X:%u:%.*s:%u;", tex.raw.nameHash, tex.raw.nameStart, tex.raw.nameEnd, tex.raw.samplerState, tex.raw.semantic,
						static_cast<int>(tex.imageName.size()), tex.imageName.data(), have ? image.raw.semantic : 255u);
					textures += t;
				}
				for (const c1::MaterialConstantDef& c : v.constants)
				{
					char nm[13] = {};
					std::memcpy(nm, c.name, 12);
					std::snprintf(t, sizeof(t), "%08X:%s:%g,%g,%g,%g;", c.nameHash, nm, c.literal[0], c.literal[1], c.literal[2], c.literal[3]);
					constants += t;
				}
				for (const auto& b : v.stateBits)
				{
					std::snprintf(t, sizeof(t), "%08X:%08X;", b[0], b[1]);
					state += t;
				}
				std::fprintf(f, "%s\t%.*s\t%d\t%.*s\t%s\t%s\t%s\t%s\n", ff.stem().string().c_str(), static_cast<int>(v.name.size()), v.name.data(),
					top.count(r.header) ? 1 : 0, static_cast<int>(v.techsetName.size()), v.techsetName.data(),
					hex({ reinterpret_cast<const std::uint8_t*>(&v.raw), sizeof(v.raw) }).c_str(), textures.c_str(), constants.c_str(), state.c_str());
			}
		}
		std::fclose(f);
		return 0;
	}

	int models(const fs::path& folder, const ClientKey& key, const fs::path& out)
	{
		std::FILE* f = std::fopen(out.string().c_str(), "wb");
		if (!f)
		{
			return 1;
		}
		std::fprintf(f, "zone\tname\ttop\tbones\troots\tsurfs\tlods\texternal\tverts\theader\n");
		for (const fs::path& ff : zones_of(folder))
		{
			Zone zone;
			if (!zone.open(ff, key))
			{
				continue;
			}
			std::set<zaddr> top;
			for (const XAssetEntry& e : zone.assets())
			{
				top.insert(e.header);
			}
			for (const AssetRecord& r : zone.all_assets())
			{
				if (r.type != c1::ASSET_TYPE_XMODEL || zone.reference(r.header))
				{
					continue;
				}
				ModelView v;
				if (!read_model(zone, r.header, v, true) || v.isReference)
				{
					continue;
				}
				std::uint32_t verts = 0;
				bool external = false;
				for (const ModelLod& lod : v.lods)
				{
					external = external || lod.surfs.external;
					for (const SurfaceView& s : lod.surfs.surfaces)
					{
						verts += s.vertCount;
					}
				}
				std::fprintf(f, "%s\t%.*s\t%d\t%u\t%u\t%u\t%u\t%d\t%u\t%s\n", ff.stem().string().c_str(), static_cast<int>(v.name.size()), v.name.data(),
					top.count(r.header) ? 1 : 0, v.raw.numBones, v.raw.numRootBones, v.raw.numsurfs, v.raw.numLods, external ? 1 : 0, verts,
					hex({ reinterpret_cast<const std::uint8_t*>(&v.raw), sizeof(v.raw) }).c_str());
			}
		}
		std::fclose(f);
		return 0;
	}
}

namespace
{
	int iwis(const fs::path& install, const fs::path& exe, const fs::path& out)
	{
		namespace r = codol::render;
		r::iwd_key key;
		std::string error;
		r::image_archive_sets sets;
		if (!r::derive_iwd_key(exe, key, &error) || !r::open_image_archives(install, key, sets, &error))
		{
			std::fprintf(stderr, "%s\n", error.c_str());
			return 1;
		}
		std::FILE* f = std::fopen(out.string().c_str(), "wb");
		if (!f)
		{
			return 1;
		}
		std::fprintf(f, "path\tarchive\tversion\tflags\tformat\twidth\theight\tdepth\tsize\tpicmip\n");
		for (const auto* set : { sets.images.get(), sets.stripped.get() })
		{
			if (!set)
			{
				continue;
			}
			set->for_each([&](const std::string& path, const r::iwd_archive_set::hit& hit)
			{
				std::vector<std::uint8_t> head;
				r::iwi_header h;
				if (hit.archive->read_prefix(*hit.entry, 32, head) && r::parse_iwi_header(head, h))
				{
					std::fprintf(f, "%s\t%s\t%u\t%08X\t%u\t%d\t%d\t%d\t%u\t%d,%d,%d,%d\n", path.c_str(), hit.archive->file_name().c_str(), h.version, h.flags,
						static_cast<unsigned>(h.format), h.width, h.height, h.depth, hit.entry->uncompressed_size, h.file_size_for_picmip[0],
						h.file_size_for_picmip[1], h.file_size_for_picmip[2], h.file_size_for_picmip[3]);
				}
			});
		}
		std::fclose(f);
		return 0;
	}
}

int main(const int argc, char** argv)
{
	if (argc < 5)
	{
		std::fprintf(stderr, "usage: see the head of asset_probe.cpp\n");
		return 2;
	}
	const std::string command = argv[1];
	ClientKey key;
	if (const Status s = key.load_from_exe(argv[command == "iwis" ? 3 : 3]); !s)
	{
		std::fprintf(stderr, "%s\n", s.message.c_str());
		return 1;
	}
	if (command == "iwis")
	{
		return iwis(argv[2], argv[3], argv[4]);
	}
	if (command == "graph" && argc >= 6)
	{
		return graph(argv[2], key, argv[4], argv[5], argc > 6 ? static_cast<std::uint32_t>(std::atoi(argv[6])) : 40u);
	}
	if (command == "images")
	{
		return images(argv[2], key, argv[4]);
	}
	if (command == "materials")
	{
		return materials(argv[2], key, argv[4]);
	}
	if (command == "models")
	{
		return models(argv[2], key, argv[4]);
	}
	return 2;
}
