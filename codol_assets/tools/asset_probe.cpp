// codol_asset_probe: research aid of codol_assets. Prints how shipped assets are laid out in the zone stream and
// writes surveys of the shipped images / materials / models as tab separated tables. The rules in codol_assets
// ( which format, flags, technique set, state bits .. go with which kind of asset ) were derived from its output.
//
//   codol_asset_probe graph <zone.ff> <C1.exe> <type> <name> [max pieces]     stream order of an asset's data
//   codol_asset_probe images <folder> <C1.exe> <out.tsv>                      every image of every zone
//   codol_asset_probe materials <folder> <C1.exe> <out.tsv>                   every material of every zone
//   codol_asset_probe models <folder> <C1.exe> <out.tsv>                      every xmodel of every zone
//   codol_asset_probe iwis <install> <C1.exe> <out.tsv>                       header of every .iwi of the image archives
//   codol_asset_probe trees <zone.ff> <C1.exe> <model | *>                    the collision trees of a model's rigid lists

#include "codol_zone/assets.hpp"
#include "codol_zone/zone_editor.hpp"
#include "codol_render/iwd.hpp"
#include "codol_render/iwi.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <map>
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
	// every surface that is in a zone: the XSurface fields the model compiler has to fill
	int surfs(const fs::path& folder, const ClientKey& key, const fs::path& out)
	{
		std::FILE* f = std::fopen(out.string().c_str(), "wb");
		if (!f)
		{
			return 1;
		}
		std::fprintf(f, "zone\tmodel\tbones\tlod\tsurf\ttileMode\tdeformed\tflags\tunk3\tverts\ttris\tbaseTri\tbaseVert\tinfo\tblend\tunk36\tlists\tunk48\tpartBits\ttrees\tlistsContiguous\ttrisInLists\tbinormal\tsurfsUnk10\tsurfsUnk44\n");
		std::set<std::string> seen;
		for (const fs::path& ff : zones_of(folder))
		{
			Zone zone;
			if (!zone.open(ff, key))
			{
				continue;
			}
			for (const AssetRecord& r : zone.all_assets())
			{
				if (r.type != c1::ASSET_TYPE_XMODEL || zone.reference(r.header))
				{
					continue;
				}
				ModelView v;
				if (!read_model(zone, r.header, v, true) || v.isReference || !seen.insert(std::string(v.name)).second)
				{
					continue;
				}
				for (std::size_t l = 0; l < v.lods.size(); l++)
				{
					const ModelSurfsView& ms = v.lods[l].surfs;
					for (std::size_t s = 0; s < ms.surfaces.size(); s++)
					{
						const SurfaceView& sv = ms.surfaces[s];
						const auto lists = sv.rigid_lists();
						std::uint32_t trees = 0, vsum = 0, tsum = 0;
						bool contiguous = true;
						for (std::size_t k = 0; k < lists.size(); k++)
						{
							c1::XRigidVertList raw;
							std::memcpy(&raw, sv.rigidBytes.data() + k * sizeof(raw), sizeof(raw));
							trees += raw.collisionTree ? 1 : 0;
							contiguous = contiguous && lists[k].triOffset == tsum;
							vsum += lists[k].vertCount;
							tsum += lists[k].triCount;
						}
						int plus = 0, minus = 0, other = 0;
						for (const SurfaceVertex& vert : sv.vertices())
						{
							plus += vert.binormalSign == 1.0f ? 1 : 0;
							minus += vert.binormalSign == -1.0f ? 1 : 0;
							other += vert.binormalSign != 1.0f && vert.binormalSign != -1.0f ? 1 : 0;
						}
						std::fprintf(f, "%s\t%.*s\t%u\t%zu\t%zu\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%d,%d,%d,%d\t%d\t%02X%02X%02X%02X\t%u\t%02X%02X%02X%02X\t%08X\t%u\t%d\t%d\t%d/%d/%d\t%02X%02X\t%02X%02X%02X%02X\n",
							ff.stem().string().c_str(), static_cast<int>(v.name.size()), v.name.data(), v.raw.numBones, l, s, sv.raw.tileMode, sv.raw.deformed, sv.raw.flags,
							sv.raw._unk3, sv.vertCount, sv.triCount, sv.raw.baseTriIndex, sv.raw.baseVertIndex, sv.raw.vertInfoCount[0], sv.raw.vertInfoCount[1],
							sv.raw.vertInfoCount[2], sv.raw.vertInfoCount[3], sv.raw.vertsBlend ? 1 : 0, sv.raw._unk36[0], sv.raw._unk36[1], sv.raw._unk36[2], sv.raw._unk36[3],
							sv.raw.vertListCount, sv.raw._unk48[0], sv.raw._unk48[1], sv.raw._unk48[2], sv.raw._unk48[3], sv.raw.partBits[0], trees, contiguous ? 1 : 0,
							vsum == sv.vertCount && tsum == sv.triCount ? 1 : 0, plus, minus, other, ms.raw._unk10[0], ms.raw._unk10[1], ms.raw._unk44[0], ms.raw._unk44[1],
							ms.raw._unk44[2], ms.raw._unk44[3]);
					}
				}
			}
		}
		std::fclose(f);
		return 0;
	}

	// The XSurfaceCollisionTree of the rigid lists of a model ( what bullet marks are projected with ): header, nodes,
	// leafs and the triangles in tree space, to read the rules off; model "*": counts over every tree of the zone that
	// test them ( leaf nodes, triangles per leaf, boxes )
	int trees(const fs::path& ff, const ClientKey& key, const std::string& name)
	{
		struct tree_header
		{
			float trans[3], scale[3];
			std::uint32_t nodeCount, nodes, leafCount, leafs;
		};
		struct tree_node
		{
			std::uint16_t mins[3], maxs[3], childBegin, childCount;
		};
		Zone zone;
		if (!zone.open(ff, key))
		{
			return 1;
		}
		const bool all = name == "*";
		std::map<std::string, int> stats;
		std::set<std::string> seen;
		for (const AssetRecord& r : zone.all_assets())
		{
			if (r.type != c1::ASSET_TYPE_XMODEL || zone.reference(r.header))
			{
				continue;
			}
			ModelView v;
			if (!read_model(zone, r.header, v, true) || v.isReference || (!all && v.name != name) || !seen.insert(std::string(v.name)).second)
			{
				continue;
			}
			for (std::size_t l = 0; l < v.lods.size(); l++)
			{
				const ModelSurfsView& ms = v.lods[l].surfs;
				for (std::size_t s = 0; s < ms.surfaces.size(); s++)
				{
					const SurfaceView& sv = ms.surfaces[s];
					const std::vector<SurfaceVertex> verts = sv.vertices();
					const std::vector<std::uint16_t> idx = sv.indices();
					const auto lists = sv.rigid_lists();
					for (std::size_t k = 0; k < lists.size(); k++)
					{
						c1::XRigidVertList raw;
						std::memcpy(&raw, sv.rigidBytes.data() + k * sizeof(raw), sizeof(raw));
						stats[raw.collisionTree ? (l == 0 ? "lists with a tree, lod 0" : "lists with a tree, other lods") : (l == 0 ? "lists without, lod 0" : "lists without, other lods")]++;
						if (raw.collisionTree && v.raw.numCollSurfs == 0)
						{
							stats["lists with a tree in a model without collision surfaces"]++;
						}
						if (!raw.collisionTree && v.raw.numCollSurfs != 0 && l == 0)
						{
							stats["lod 0 lists without a tree in a model with collision surfaces"]++;
						}
						if (!raw.collisionTree)
						{
							continue;
						}
						const tree_header tree = zone.get<tree_header>(raw.collisionTree);
						const std::vector<tree_node> nodes = zone.get_array<tree_node>(tree.nodes, tree.nodeCount);
						const std::vector<std::uint16_t> leafs = zone.get_array<std::uint16_t>(tree.leafs, tree.leafCount);
						// a triangle's box in tree units
						const auto unit = [&](const float p, const int axis) { return (p + tree.trans[axis]) * tree.scale[axis]; };
						const auto tri_box = [&](const std::uint32_t t, float lo[3], float hi[3])
						{
							for (int a = 0; a < 3; a++)
							{
								lo[a] = 1e9f;
								hi[a] = -1e9f;
							}
							for (int c = 0; c < 3; c++)
							{
								const SurfaceVertex& vert = verts[idx[(lists[k].triOffset + t) * 3 + static_cast<std::uint32_t>(c)]];
								for (int a = 0; a < 3; a++)
								{
									lo[a] = std::min(lo[a], unit(vert.position[a], a));
									hi[a] = std::max(hi[a], unit(vert.position[a], a));
								}
							}
						};
						if (!all)
						{
							std::printf("lod %zu surface %zu list %zu: %u vertices, %u triangles from %u; tree trans ( %g %g %g ) scale ( %g %g %g ), %u nodes, %u leafs\n", l, s, k,
								lists[k].vertCount, lists[k].triCount, lists[k].triOffset, tree.trans[0], tree.trans[1], tree.trans[2], tree.scale[0], tree.scale[1], tree.scale[2],
								tree.nodeCount, tree.leafCount);
							for (std::size_t n = 0; n < nodes.size(); n++)
							{
								std::printf("  node %zu: ( %u %u %u ) - ( %u %u %u ), child begin %u count %u\n", n, nodes[n].mins[0], nodes[n].mins[1], nodes[n].mins[2], nodes[n].maxs[0],
									nodes[n].maxs[1], nodes[n].maxs[2], nodes[n].childBegin, nodes[n].childCount);
							}
							std::printf("  leafs:");
							for (const std::uint16_t leaf : leafs)
							{
								std::printf(" %u", leaf);
							}
							std::printf("\n");
							for (std::uint32_t t = 0; t < lists[k].triCount && t < 40; t++)
							{
								float lo[3], hi[3];
								tri_box(t, lo, hi);
								std::printf("  triangle %u: ( %.1f %.1f %.1f ) - ( %.1f %.1f %.1f )\n", t, lo[0], lo[1], lo[2], hi[0], hi[1], hi[2]);
							}
						}
						// the rules: a node whose child count has bit 15 is a leaf node - child begin is its first entry of the
						// leaf table, the low bits how many entries; an entry is a triangle of the list, with bit 15: that one
						// and the next
						stats["trees"]++;
						bool child_after_parent = true, kids_cover = true;
						std::vector<int> referenced(nodes.size(), 0), covered(lists[k].triCount, 0);
						std::uint32_t leaf_entries = 0;
						for (std::size_t n = 0; n < nodes.size(); n++)
						{
							if (nodes[n].childCount & 0x8000)
							{
								const std::uint32_t count = nodes[n].childCount & 0x7FFFu;
								stats["entries per leaf node: " + std::to_string(count)]++;
								leaf_entries += count;
								bool inside = true, tight[6] = {};
								for (std::uint32_t e = 0; e < count && nodes[n].childBegin + e < leafs.size(); e++)
								{
									const std::uint16_t entry = leafs[nodes[n].childBegin + e];
									stats[entry & 0x8000 ? "leaf entries of two triangles" : "leaf entries of one triangle"]++;
									// a triangle index counts from the surface's first triangle, not the list's
									for (std::uint32_t at = entry & 0x7FFFu; at < (entry & 0x7FFFu) + (entry & 0x8000 ? 2u : 1u); at++)
									{
										const std::uint32_t t = at - lists[k].triOffset;
										if (at < lists[k].triOffset || t >= lists[k].triCount)
										{
											stats["leaf entries beyond the list's triangles"]++;
											continue;
										}
										covered[t]++;
										float lo[3], hi[3];
										tri_box(t, lo, hi);
										for (int x = 0; x < 3; x++)
										{
											inside = inside && lo[x] >= nodes[n].mins[x] - 1.5f && hi[x] <= nodes[n].maxs[x] + 1.5f;
											tight[x] = tight[x] || lo[x] < nodes[n].mins[x] + 1.5f;
											tight[3 + x] = tight[3 + x] || hi[x] > nodes[n].maxs[x] - 1.5f;
										}
									}
								}
								stats[inside ? "leaf boxes that hold their triangles" : "leaf boxes that do NOT hold their triangles"]++;
								stats[tight[0] && tight[1] && tight[2] && tight[3] && tight[4] && tight[5] ? "leaf boxes that are tight" : "leaf boxes that are loose"]++;
							}
							else
							{
								stats["children per inner node: " + std::to_string(nodes[n].childCount)]++;
								child_after_parent = child_after_parent && nodes[n].childBegin > n;
								for (std::uint32_t c = 0; c < nodes[n].childCount && nodes[n].childBegin + c < nodes.size(); c++)
								{
									referenced[nodes[n].childBegin + c]++;
									const tree_node& child = nodes[nodes[n].childBegin + c];
									for (int x = 0; x < 3; x++)
									{
										kids_cover = kids_cover && child.mins[x] >= nodes[n].mins[x] && child.maxs[x] <= nodes[n].maxs[x];
									}
								}
							}
						}
						stats[leaf_entries == tree.leafCount ? "entries of the leaf nodes == leaf count" : "entries of the leaf nodes != leaf count"]++;
						stats[child_after_parent ? "children behind their parent" : "children NOT behind their parent"]++;
						stats[kids_cover ? "child boxes inside the parent's" : "child boxes NOT inside the parent's"]++;
						bool once = true, each = true;
						for (std::size_t n = 1; n < nodes.size(); n++)
						{
							once = once && referenced[n] == 1;
						}
						for (const int c : covered)
						{
							each = each && c == 1;
						}
						stats[once ? "every node but 0 has one parent" : "nodes with no / several parents"]++;
						stats[each ? "every triangle in exactly one leaf entry" : "triangles in no / several leaf entries"]++;
						stats[nodes[0].mins[0] == 0 && nodes[0].maxs[0] == 65535 && nodes[0].maxs[2] == 65535 ? "root box 0 .. 65535" : "root box is something else"]++;
						// the scale: 65535 over the extent of the list's vertices ?
						float lo[3] = { 1e9f, 1e9f, 1e9f }, hi[3] = { -1e9f, -1e9f, -1e9f };
						for (std::uint32_t t = 0; t < lists[k].triCount; t++)
						{
							float a[3], b[3];
							tri_box(t, a, b);
							for (int x = 0; x < 3; x++)
							{
								lo[x] = std::min(lo[x], a[x]);
								hi[x] = std::max(hi[x], b[x]);
							}
						}
						const bool spans = lo[0] > -2 && lo[1] > -2 && lo[2] > -2 && hi[0] < 65537 && hi[1] < 65537 && hi[2] < 65537;
						const bool full = lo[0] < 2 && lo[1] < 2 && lo[2] < 2 && (hi[0] > 65533 || hi[1] > 65533 || hi[2] > 65533);
						stats[spans ? "triangles within 0 .. 65535" : "triangles OUTSIDE 0 .. 65535"]++;
						stats[full ? "tree space starts at the list's mins and one axis reaches 65535" : "tree space is something else"]++;
						stats[hi[0] > 65533 && hi[1] > 65533 && hi[2] > 65533 ? "every axis reaches 65535" : "not every axis reaches 65535"]++;
						if (!all)
						{
							std::printf("  triangles span ( %.1f %.1f %.1f ) - ( %.1f %.1f %.1f ) in tree units\n", lo[0], lo[1], lo[2], hi[0], hi[1], hi[2]);
						}
					}
				}
			}
		}
		for (const auto& [what, count] : stats)
		{
			std::printf("%8d  %s\n", count, what.c_str());
		}
		return 0;
	}

	// bone name -> part classification, over all models: is the classification a function of the name?
	int bones(const fs::path& folder, const ClientKey& key, const fs::path& out)
	{
		std::map<std::string, std::map<int, int>> counts;
		std::set<std::string> seen;
		for (const fs::path& ff : zones_of(folder))
		{
			Zone zone;
			if (!zone.open(ff, key))
			{
				continue;
			}
			for (const AssetRecord& r : zone.all_assets())
			{
				ModelView v;
				if (r.type != c1::ASSET_TYPE_XMODEL || zone.reference(r.header) || !read_model(zone, r.header, v, false) || v.isReference || !seen.insert(std::string(v.name)).second)
				{
					continue;
				}
				for (const ModelBone& b : v.bones)
				{
					counts[std::string(b.name)][b.partClassification]++;
				}
			}
		}
		std::FILE* f = std::fopen(out.string().c_str(), "wb");
		if (!f)
		{
			return 1;
		}
		for (const auto& [name, classes] : counts)
		{
			std::fprintf(f, "%s", name.c_str());
			for (const auto& [value, count] : classes)
			{
				std::fprintf(f, "\t%d:%d", value, count);
			}
			std::fprintf(f, "\n");
		}
		std::fclose(f);
		return 0;
	}

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
	if (command == "bones")
	{
		return bones(argv[2], key, argv[4]);
	}
	if (command == "surfs")
	{
		return surfs(argv[2], key, argv[4]);
	}
	if (command == "models")
	{
		return models(argv[2], key, argv[4]);
	}
	if (command == "trees")
	{
		return trees(argv[2], key, argv[4]);
	}
	return 2;
}
