// codol_bsp_probe: development aid. Prints pieces of the zone stream of an asset, so that what the compiler writes can
// be compared with what the game's own map compiler wrote.
//
//   codol_bsp_probe dump <zone.ff> <C1.exe> <asset type name> [first piece] [piece count] [max bytes per piece]
//   codol_bsp_probe pieces <zone.ff> <C1.exe> <asset type name>          one line per piece: size, string, pointer count
//   codol_bsp_probe check <zone.ff> <C1.exe>                             the offline validators of codol_bsp on any map zone

#include "codol_bsp/bsp.hpp"
#include "codol_zone/assets.hpp"
#include "codol_zone/zone_editor.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

using namespace codol;

int main(int argc, char** argv)
{
	if (argc < 4)
	{
		std::puts("codol_bsp_probe dump|pieces <zone.ff> <C1.exe> <asset type> [first piece] [piece count] [max bytes]\n"
			"codol_bsp_probe check <zone.ff> <C1.exe>");
		return 1;
	}
	const std::string mode = argv[1];
	zone::ClientKey key;
	if (const zone::Status s = key.load_from_exe(argv[3]); !s)
	{
		std::printf("%s\n", s.message.c_str());
		return 1;
	}
	if (mode == "check")
	{
		std::vector<std::uint8_t> bytes;
		if (const zone::Status s = zone::read_fastfile(argv[2], key, bytes); !s)
		{
			std::printf("%s\n", s.message.c_str());
			return 1;
		}
		std::vector<std::string> problems;
		std::string summary;
		const bool ok = bsp::validate_zone(bytes, problems, &summary);
		std::printf("%s", summary.c_str());
		for (const std::string& p : problems)
		{
			std::printf("problem: %s\n", p.c_str());
		}
		std::printf("%s\n", ok ? "valid" : "INVALID");
		return ok ? 0 : 1;
	}
	if (mode == "portals")
	{
		// how the shipped portals are oriented: plane against the centres of the owning and the target cell,
		// winding order against the plane normal, the hull axes
		zone::Zone z;
		if (const zone::Status s = z.open(argv[2], key); !s)
		{
			std::printf("%s\n", s.message.c_str());
			return 1;
		}
		zone::GfxWorldView world;
		if (!zone::read_gfx_world(z, zone::first_asset(z, c1::ASSET_TYPE_GFXWORLD), world))
		{
			return 1;
		}
		int shown = 0;
		for (std::size_t c = 0; c < world.cells.size(); c++)
		{
			for (const c1::GfxPortal& p : world.cells[c].portals)
			{
				float plane[4], axis[6];
				std::memcpy(plane, p._unk0 + 12, 16);
				std::memcpy(axis, p._unk35 + 1, 24);
				const std::vector<float> v = z.get_array<float>(p.vertices, 3u * p.vertexCount);
				const auto side = [&](const c1::Bounds& b) { return plane[0] * b.midPoint[0] + plane[1] * b.midPoint[1] + plane[2] * b.midPoint[2] - plane[3]; };
				float n[3] = {};
				if (v.size() >= 9)
				{
					const float a[3] = { v[3] - v[0], v[4] - v[1], v[5] - v[2] }, b[3] = { v[6] - v[0], v[7] - v[1], v[8] - v[2] };
					n[0] = a[1] * b[2] - a[2] * b[1];
					n[1] = a[2] * b[0] - a[0] * b[2];
					n[2] = a[0] * b[1] - a[1] * b[0];
				}
				const float facing = n[0] * plane[0] + n[1] * plane[1] + n[2] * plane[2];
				if (shown++ < 14)
				{
					std::printf("cell %zu -> %u: plane %.3f %.3f %.3f | %.1f, own centre %+.1f, target centre %+.1f, vertex 0 on plane %+.3f, %u verts wound %s, axes ( %.2f %.2f %.2f ) ( %.2f %.2f %.2f )\n",
						c, p.cellIndex, plane[0], plane[1], plane[2], plane[3], side(world.cells[c].raw.bounds), side(world.cells[p.cellIndex].raw.bounds),
						v.size() >= 3 ? plane[0] * v[0] + plane[1] * v[1] + plane[2] * v[2] - plane[3] : 0.0f, p.vertexCount,
						facing > 0 ? "ccw seen from the normal's side" : "cw seen from the normal's side", axis[0], axis[1], axis[2], axis[3], axis[4], axis[5]);
				}
			}
		}
		return 0;
	}
	if (argc < 5)
	{
		return 1;
	}
	zone::ZoneEditor editor;
	if (const zone::Status s = editor.open(argv[2], key); !s)
	{
		std::printf("%s\n", s.message.c_str());
		return 1;
	}
	const std::string type = argv[4];
	const std::uint32_t first = argc > 5 ? static_cast<std::uint32_t>(std::atoi(argv[5])) : 0;
	const std::uint32_t count = argc > 6 ? static_cast<std::uint32_t>(std::atoi(argv[6])) : 8;
	const std::uint32_t max_bytes = argc > 7 ? static_cast<std::uint32_t>(std::atoi(argv[7])) : 256;
	for (std::size_t a = 0; a < editor.asset_count(); a++)
	{
		const zone::EditorAsset asset = editor.asset(a);
		if (type != c1::XASSET_NAMES[asset.type])
		{
			continue;
		}
		std::printf("asset %zu %s '%s': %u pieces\n", a, type.c_str(), asset.name.c_str(), asset.chunks);
		for (std::uint32_t p = mode == "pieces" ? 0 : first; p < asset.chunks && (mode == "pieces" || p < first + count); p++)
		{
			const zone::ZoneDataId id = editor.asset_data(a, p);
			zone::ZoneDataInfo info;
			editor.data_info(id, info);
			const auto bytes = editor.data_bytes(id);
			if (mode == "save")
			{
				// codol_bsp_probe save <ff> <exe> <type> <piece> 1 <file>
				if (std::FILE* f = argc > 7 ? std::fopen(argv[7], "wb") : nullptr)
				{
					std::fwrite(bytes.data(), 1, bytes.size(), f);
					std::fclose(f);
					std::printf("piece %u: %zu bytes -> %s\n", p, bytes.size(), argv[7]);
				}
				continue;
			}
			std::printf("  piece %u: %u bytes%s, %u pointers, loaded at %08X\n", p, info.size, info.string ? " string" : "", info.pointers, info.address);
			if (mode == "pieces")
			{
				continue;
			}
			if (info.string)
			{
				std::printf("    \"%.*s\"\n", static_cast<int>(std::min<std::size_t>(bytes.size(), 200)), reinterpret_cast<const char*>(bytes.data()));
				continue;
			}
			for (std::uint32_t k = 0; k < bytes.size() && k < max_bytes; k += 4)
			{
				if (k % 32 == 0)
				{
					std::printf("%s    +0x%03X", k ? "\n" : "", k);
				}
				std::uint32_t word = 0;
				std::memcpy(&word, bytes.data() + k, std::min<std::size_t>(4, bytes.size() - k));
				std::printf(" %08X", word);
			}
			std::printf("\n");
		}
		break;
	}
	return 0;
}
