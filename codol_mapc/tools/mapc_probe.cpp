// codol_mapc_probe: prints how the map assets of a zone are laid out in the asset graph and what the fields that
// codol_mapc rebuilds hold in the shipped maps. The layout statements in codol_mapc ( which array is read where,
// which lists are runs of which, how the leaf brush tree is shaped ) were established with it.
//
//   codol_mapc_probe graph   <map.ff> <C1.exe> <gfx_map|col_map_mp|..> [max pieces]   stream order of an asset's data
//   codol_mapc_probe fields  <map.ff> <C1.exe> <gfx_map|col_map_mp|..>                where the header's pointers lead
//   codol_mapc_probe gfx     <map.ff> <C1.exe>                                        cells, AABB trees, surfaces
//   codol_mapc_probe smodels <map.ff> <C1.exe>                                        static models, shadow lists, BSP
//   codol_mapc_probe clip    <map.ff> <C1.exe>                                        leafs, leaf brush trees, brushes

#include "codol_zone/assets.hpp"
#include "codol_zone/zone_editor.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <functional>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <set>
#include <string>

using namespace codol::zone;
namespace c1 = codol::c1;

namespace
{
	const char* kind_name(const ZoneTarget::Kind kind)
	{
		switch (kind)
		{
		case ZoneTarget::Kind::data: return "data";
		case ZoneTarget::Kind::allocation: return "alloc";
		case ZoneTarget::Kind::asset: return "asset";
		case ZoneTarget::Kind::raw: return "raw";
		default: return "plain";
		}
	}

	std::string describe(const ZoneEditor& editor, const ZoneTarget& t)
	{
		char text[256];
		ZoneDataInfo info;
		switch (t.kind)
		{
		case ZoneTarget::Kind::data:
			editor.data_info(t.data, info);
			std::snprintf(text, sizeof(text), "data #%u ( asset %zd piece %u, %u bytes ) + %u", t.data, static_cast<std::ptrdiff_t>(info.asset), info.index, info.size, t.offset);
			break;
		case ZoneTarget::Kind::asset:
			std::snprintf(text, sizeof(text), "asset %zu %s '%s' + %u", t.asset, asset_type_name(editor.asset(t.asset).type).c_str(), editor.asset(t.asset).name.c_str(), t.offset);
			break;
		case ZoneTarget::Kind::allocation:
			std::snprintf(text, sizeof(text), "allocation %u of asset %zd + %u", t.allocation, static_cast<std::ptrdiff_t>(t.asset), t.offset);
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

	template <typename T>
	T rd(const std::span<const std::uint8_t> data, const std::size_t offset)
	{
		T value{};
		if (offset + sizeof(T) <= data.size())
		{
			std::memcpy(&value, data.data() + offset, sizeof(T));
		}
		return value;
	}

	int graph(const ZoneEditor& editor, const std::string& type_name, const std::uint32_t limit)
	{
		const std::int32_t type = asset_type_from_name(type_name);
		for (std::size_t a = 0; a < editor.asset_count(); a++)
		{
			const EditorAsset asset = editor.asset(a);
			if (asset.type != type)
			{
				continue;
			}
			std::printf("asset %zu %s '%s': %u pieces, %u pointers, %llu bytes\n", a, type_name.c_str(), asset.name.c_str(), asset.chunks, asset.relocations,
				static_cast<unsigned long long>(asset.streamBytes));
			std::map<std::string, int> kinds;
			for (std::uint32_t k = 0; k < asset.chunks; k++)
			{
				const ZoneDataId id = editor.asset_data(a, k);
				ZoneDataInfo info;
				editor.data_info(id, info);
				const auto pointers = editor.data_pointers(id);
				for (const ZonePointer& p : pointers)
				{
					std::string key = kind_name(p.target.kind);
					if (p.target.kind == ZoneTarget::Kind::data)
					{
						ZoneDataInfo target;
						editor.data_info(p.target.data, target);
						key += target.asset == a ? " ( own )" : " ( other asset )";
					}
					kinds[key]++;
				}
				if (k < limit)
				{
					std::printf("  piece %5u #%-7u %9u bytes at 0x%08X%s %u pointers\n", k, id, info.size, info.address, info.string ? " string" : "", info.pointers);
					if (k == 0 || pointers.size() <= 12)
					{
						const auto bytes = editor.data_bytes(id);
						for (const ZonePointer& p : pointers)
						{
							std::printf("      +0x%03X -> %s\n", p.offset, describe(editor, p.target).c_str());
						}
						if (k == 0)
						{
							// header fields that say "follows"
							for (std::uint32_t o = 0; o + 4 <= bytes.size(); o += 4)
							{
								const std::uint32_t v = rd<std::uint32_t>(bytes, o);
								if (v == 0xFFFFFFFFu || v == 0xFFFFFFFEu)
								{
									std::printf("      +0x%03X = %s ( loaded 0x%08X )\n", o, v == 0xFFFFFFFFu ? "follows" : "follows, insert",
										editor.zone().get<std::uint32_t>(editor.zone().assets()[a].header + o));
								}
							}
						}
					}
				}
			}
			for (const auto& [key, count] : kinds)
			{
				std::printf("  pointers to %s: %d\n", key.c_str(), count);
			}
		}
		return 0;
	}

	struct aabb_node
	{
		c1::Bounds bounds;
		std::uint16_t childCount, surfaceCount, startSurfIndex, surfaceCountNoDecal, startSurfIndexNoDecal, smodelIndexCount;
		zaddr smodelIndexes;
		std::int32_t childrenOffset;
	};
	static_assert(sizeof(aabb_node) == 44);

	int gfx(const ZoneEditor& editor)
	{
		const Zone& zone = editor.zone();
		const zaddr address = first_asset(zone, c1::ASSET_TYPE_GFXWORLD);
		GfxWorldView world;
		if (!address || !read_gfx_world(zone, address, world))
		{
			std::printf("no gfx_map\n");
			return 1;
		}
		const auto& raw = world.raw;
		const auto& d = raw.dpvs;
		std::printf("gfx_map %s: planes %d nodes %d surfaces %u cells %d models %d verts %u indices %u layer %u lightmaps %d probes %u primaryLights %u lastSun %u\n",
			std::string(world.name).c_str(), raw.planeCount, raw.nodeCount, raw.surfaceCount, raw.cellCount, raw.modelCount, raw.draw.vertexCount, raw.draw.indexCount,
			raw.draw.vertexLayerDataSize, raw.draw.lightmapCount, raw.draw.reflectionProbeCount, raw.primaryLightCount, raw.lastSunPrimaryLightIndex);
		std::printf("dpvs: smodels %u staticSurf %u noDecal %u litOpaque %u..%u litTrans %u..%u shadowCaster %u..%u emissive %u..%u smodelVis %u surfVis %u\n", d.smodelCount,
			d.staticSurfaceCount, d.staticSurfaceCountNoDecal, d.litOpaqueSurfsBegin, d.litOpaqueSurfsEnd, d.litTransSurfsBegin, d.litTransSurfsEnd, d.shadowCasterSurfsBegin,
			d.shadowCasterSurfsEnd, d.emissiveSurfsBegin, d.emissiveSurfsEnd, d.smodelVisDataCount, d.surfaceVisDataCount);
		const auto header = zone.bytes(address, sizeof(c1::GfxWorld));
		std::printf("header words:");
		for (std::uint32_t o = 0; o < sizeof(c1::GfxWorld); o += 4)
		{
			if (o % 32 == 0)
			{
				std::printf("\n  +0x%03X", o);
			}
			std::printf(" %08X", rd<std::uint32_t>(header, o));
		}
		std::printf("\n");

		// static models
		const auto insts = zone.get_array<c1::GfxStaticModelInst>(d.smodelInsts, d.smodelCount);
		std::map<int, int> flags, lights, probes, skins;
		int handles = 0, origin_center = 0, ground = 0;
		double worst_center = 0;
		std::map<std::string, std::map<int, int>> cull;
		for (std::size_t i = 0; i < world.staticModels.size(); i++)
		{
			const auto& s = world.staticModels[i].raw;
			flags[s.flags]++;
			lights[s.primaryLightIndex]++;
			probes[s.reflectionProbeIndex]++;
			skins[s.firstMtlSkinIndex]++;
			handles += s.lightingHandle != 0;
			ground += rd<std::uint32_t>({ s.groundLighting, 12 }, 0) != 0;
			cull[std::string(world.staticModels[i].modelName)][s.cullDist]++;
			if (i < insts.size())
			{
				double dist = 0;
				for (int k = 0; k < 3; k++)
				{
					dist = std::max(dist, std::fabs(static_cast<double>(insts[i].lightingOrigin[k]) - insts[i].bounds.midPoint[k]));
				}
				origin_center += dist < 0.01;
				worst_center = std::max(worst_center, dist);
			}
			if (i < 6)
			{
				std::printf("  smodel %zu %s origin %.1f %.1f %.1f scale %.3f cull %u handle %u probe %u light %u flags 0x%02X skin %u ground %08X %08X %08X | bounds mid %.1f %.1f %.1f half %.1f %.1f %.1f ltOrigin %.1f %.1f %.1f\n",
					i, std::string(world.staticModels[i].modelName).c_str(), s.origin[0], s.origin[1], s.origin[2], s.scale, s.cullDist, s.lightingHandle, s.reflectionProbeIndex,
					s.primaryLightIndex, s.flags, s.firstMtlSkinIndex, rd<std::uint32_t>({ s.groundLighting, 12 }, 0), rd<std::uint32_t>({ s.groundLighting, 12 }, 4),
					rd<std::uint32_t>({ s.groundLighting, 12 }, 8), insts[i].bounds.midPoint[0], insts[i].bounds.midPoint[1], insts[i].bounds.midPoint[2], insts[i].bounds.halfSize[0],
					insts[i].bounds.halfSize[1], insts[i].bounds.halfSize[2], insts[i].lightingOrigin[0], insts[i].lightingOrigin[1], insts[i].lightingOrigin[2]);
			}
		}
		std::printf("smodels: %zu, lightingHandle != 0: %d, groundLighting != 0: %d, lightingOrigin == bounds centre: %d ( worst %.2f )\n", world.staticModels.size(), handles, ground,
			origin_center, worst_center);
		const auto print_map = [](const char* name, const std::map<int, int>& m)
		{
			std::printf("  %s:", name);
			for (const auto& [value, count] : m)
			{
				std::printf(" %d x%d", value, count);
			}
			std::printf("\n");
		};
		print_map("flags", flags);
		print_map("primaryLight", lights);
		print_map("reflectionProbe", probes);
		print_map("firstMtlSkin", skins);
		int varying = 0;
		for (const auto& [name, dists] : cull)
		{
			varying += dists.size() > 1;
		}
		std::printf("  models with more than one cullDist: %d of %zu\n", varying, cull.size());

		// model pointers of the draw insts
		const ZoneDataId draw = editor.data_at(d.smodelDrawInsts);
		std::map<std::string, int> targets;
		for (const ZonePointer& p : editor.data_pointers(draw))
		{
			targets[std::string(kind_name(p.target.kind)) + " +" + std::to_string(p.target.offset)]++;
		}
		for (const auto& [key, count] : targets)
		{
			std::printf("  smodelDrawInsts pointer targets %s: %d\n", key.c_str(), count);
		}

		// cells
		const zaddr tree_counts = rd<std::uint32_t>(header, 68), trees = rd<std::uint32_t>(header, 72);
		int cells_with_models = 0, runs_ok = 0, runs_bad = 0, node_total = 0, listed_total = 0, sorted_runs = 0, unsorted_runs = 0, leaf_nodes = 0, inner_with_surfs = 0;
		std::vector<int> listed(d.smodelCount, 0);
		std::vector<int> surf_listed(raw.surfaceCount, 0);
		int surf_dup = 0, max_depth = 0, max_children = 0, cells_no_tree = 0;
		std::map<int, int> leaf_surface_counts;
		for (int c = 0; c < raw.cellCount; c++)
		{
			const int count = zone.get<std::int32_t>(tree_counts + 4 * c);
			const zaddr tree = zone.get<zaddr>(trees + 4 * c);
			const auto nodes = zone.get_array<aabb_node>(tree, static_cast<std::size_t>(std::max(count, 0)));
			if (nodes.empty())
			{
				cells_no_tree++;
				continue;
			}
			node_total += count;
			const aabb_node& root = nodes[0];
			cells_with_models += root.smodelIndexCount != 0;
			const zaddr root_end = root.smodelIndexes + 2u * root.smodelIndexCount;
			for (int n = 0; n < count; n++)
			{
				const aabb_node& node = nodes[n];
				if (n > 0 && node.smodelIndexCount)
				{
					(node.smodelIndexes >= root.smodelIndexes && node.smodelIndexes + 2u * node.smodelIndexCount <= root_end ? runs_ok : runs_bad)++;
				}
				max_children = std::max<int>(max_children, node.childCount);
				if (node.childCount == 0)
				{
					leaf_nodes++;
					leaf_surface_counts[std::min<int>(node.surfaceCount, 40)]++;
					for (int s = 0; s < node.surfaceCount; s++)
					{
						const std::uint16_t surf = zone.get<std::uint16_t>(d.sortedSurfIndex + 2u * (node.startSurfIndex + s));
						if (surf < surf_listed.size())
						{
							surf_dup += surf_listed[surf]++ != 0;
						}
					}
				}
				if (c < 2 && n < 12)
				{
					std::printf("  cell %d node %d: mid %.0f %.0f %.0f half %.0f %.0f %.0f children %u ( offset %d ) surfaces %u @%u noDecal %u @%u smodels %u ( list +%d )\n", c, n,
						node.bounds.midPoint[0], node.bounds.midPoint[1], node.bounds.midPoint[2], node.bounds.halfSize[0], node.bounds.halfSize[1], node.bounds.halfSize[2],
						node.childCount, node.childrenOffset, node.surfaceCount, node.startSurfIndex, node.surfaceCountNoDecal, node.startSurfIndexNoDecal, node.smodelIndexCount,
						node.smodelIndexCount ? static_cast<int>(node.smodelIndexes - root.smodelIndexes) / 2 : -1);
				}
			}
			const auto list = zone.get_array<std::uint16_t>(root.smodelIndexes, root.smodelIndexCount);
			(std::is_sorted(list.begin(), list.end()) ? sorted_runs : unsorted_runs)++;
			for (const std::uint16_t index : list)
			{
				if (index < listed.size())
				{
					listed[index]++;
					listed_total++;
				}
			}
			const ZoneDataId nodes_id = editor.data_at(tree);
			if (c < 3)
			{
				ZoneDataInfo info;
				editor.data_info(nodes_id, info);
				std::printf("  cell %d: %d nodes ( piece %u ), root lists %u smodels:", c, count, info.index, root.smodelIndexCount);
				for (const ZonePointer& p : editor.data_pointers(nodes_id))
				{
					if (p.offset < 44 * 3)
					{
						std::printf(" [+%u -> %s]", p.offset, describe(editor, p.target).c_str());
					}
				}
				std::printf("\n");
			}
		}
		const int never = static_cast<int>(std::count(listed.begin(), listed.end(), 0));
		const int multi = static_cast<int>(std::count_if(listed.begin(), listed.end(), [](const int n) { return n > 1; }));
		std::printf("cells: %d ( %d without a tree ), %d aabb nodes ( %d leaves, most children %d ), %d cells with smodels; node lists that are runs of the root list %d, not %d\n", raw.cellCount,
			cells_no_tree, node_total, leaf_nodes, max_children, cells_with_models, runs_ok, runs_bad);
		std::printf("  root lists sorted ascending %d, not %d; smodels listed by no cell %d, by several %d ( %d entries ); surfaces listed twice by leaves %d, never %d\n", sorted_runs,
			unsorted_runs, never, multi, listed_total, surf_dup, static_cast<int>(std::count(surf_listed.begin(), surf_listed.end(), 0)));
		(void)max_depth;
		(void)inner_with_surfs;

		// shadow geometry
		const zaddr shadow = rd<std::uint32_t>(header, 0x1C0);
		int shadow_models = 0, shadow_surfs = 0;
		for (std::uint32_t i = 0; shadow && i < raw.primaryLightCount; i++)
		{
			const auto g = zone.bytes(shadow + 12 * i, 12);
			shadow_surfs += rd<std::uint16_t>(g, 0);
			shadow_models += rd<std::uint16_t>(g, 2);
		}
		std::printf("shadow geometry: %d surface entries, %d smodel entries over %u lights\n", shadow_surfs, shadow_models, raw.primaryLightCount);

		// surfaces
		std::map<int, int> lightmap, probe, light, sflags, layer;
		for (const auto& s : world.surfaces)
		{
			lightmap[s.raw.lightmapIndex]++;
			probe[s.raw.reflectionProbeIndex]++;
			light[s.raw.primaryLightIndex]++;
			sflags[s.raw.flags]++;
			layer[s.raw.vertexLayerData == 0 ? 0 : 1]++;
		}
		print_map("surface lightmapIndex", lightmap);
		print_map("surface reflectionProbe", probe);
		print_map("surface primaryLight", light);
		print_map("surface flags", sflags);
		print_map("surface has vertexLayerData", layer);
		for (std::size_t m = 0; m < world.brushModels.size() && m < 6; m++)
		{
			const auto& b = world.brushModels[m];
			std::printf("  brush model %zu: surfaces %u @%u noDecal %u radius %.1f\n", m, b.surfaceCount, b.startSurfIndex, b.surfaceCountNoDecal, b.radius);
		}
		// is sortedSurfIndex a permutation, and what is its relation to the surface order
		const auto sorted = zone.get_array<std::uint16_t>(d.sortedSurfIndex, d.staticSurfaceCount + d.staticSurfaceCountNoDecal);
		int identity = 0;
		std::set<std::uint16_t> distinct(sorted.begin(), sorted.end());
		for (std::size_t i = 0; i < sorted.size(); i++)
		{
			identity += sorted[i] == i;
		}
		std::printf("sortedSurfIndex: %zu entries, %zu distinct, %d equal to their position, max %u\n", sorted.size(), distinct.size(), identity,
			sorted.empty() ? 0 : *std::max_element(sorted.begin(), sorted.end()));
		return 0;
	}

	// every pointer field of an asset header: where its data is in the stream order
	int fields(const ZoneEditor& editor, const std::string& type_name)
	{
		const Zone& zone = editor.zone();
		const std::int32_t type = asset_type_from_name(type_name);
		for (std::size_t a = 0; a < editor.asset_count(); a++)
		{
			if (editor.asset(a).type != type)
			{
				continue;
			}
			const zaddr header = zone.assets()[a].header;
			const ZoneDataId id = editor.header_data(header);
			const auto bytes = editor.data_bytes(id);
			for (std::uint32_t o = 0; o + 4 <= bytes.size(); o += 4)
			{
				const std::uint32_t stream = rd<std::uint32_t>(bytes, o);
				const std::uint32_t loaded = zone.get<std::uint32_t>(header + o);
				const ZoneTarget t = editor.pointer_at(id, o);
				if (stream == 0xFFFFFFFFu || stream == 0xFFFFFFFEu)
				{
					std::uint32_t offset = 0;
					const ZoneDataId target = editor.data_at(loaded, &offset);
					ZoneDataInfo info;
					editor.data_info(target, info);
					std::printf("  +0x%03X ( %3u ) follows%s: loaded 0x%08X = piece %u + %u, %u bytes, %u pointers, %zu inbound\n", o, o, stream == 0xFFFFFFFEu ? " ( insert )" : "", loaded,
						info.index, offset, info.size, info.pointers, target ? editor.pointers_to(target).size() : 0);
				}
				else if (t.kind != ZoneTarget::Kind::none)
				{
					std::printf("  +0x%03X ( %3u ) -> %s\n", o, o, describe(editor, t).c_str());
				}
			}
		}
		return 0;
	}

	int smodels(const ZoneEditor& editor)
	{
		const Zone& zone = editor.zone();
		GfxWorldView world;
		const zaddr address = first_asset(zone, c1::ASSET_TYPE_GFXWORLD);
		read_gfx_world(zone, address, world);
		const auto& d = world.raw.dpvs;
		const ZoneDataId draw = editor.data_at(d.smodelDrawInsts);
		ZoneDataInfo draw_info;
		editor.data_info(draw, draw_info);
		std::map<std::string, int> kinds;
		int shown = 0;
		for (const ZonePointer& p : editor.data_pointers(draw))
		{
			std::string key = kind_name(p.target.kind);
			if (p.target.kind == ZoneTarget::Kind::data)
			{
				key += p.target.data == draw ? " ( the array itself )" : " ( other data )";
			}
			kinds[key]++;
			if (shown++ < 6 || (p.target.kind != ZoneTarget::Kind::data && shown < 400 && kinds[key] < 4))
			{
				std::printf("  draw inst %u field +%u -> %s\n", p.offset / 76, p.offset % 76, describe(editor, p.target).c_str());
			}
		}
		for (const auto& [key, count] : kinds)
		{
			std::printf("smodelDrawInsts ( piece %u, %u bytes ): %d pointers to %s\n", draw_info.index, draw_info.size, count, key.c_str());
		}
		// the model fields that are not pointers
		const auto bytes = editor.data_bytes(draw);
		std::map<std::uint32_t, int> plain;
		const auto pointers = editor.data_pointers(draw);
		std::set<std::uint32_t> pointer_offsets;
		for (const ZonePointer& p : pointers)
		{
			pointer_offsets.insert(p.offset);
		}
		for (std::uint32_t i = 0; i < d.smodelCount; i++)
		{
			if (!pointer_offsets.count(76 * i + 52))
			{
				plain[rd<std::uint32_t>(bytes, 76 * i + 52)]++;
				if (plain.size() < 4 && plain[rd<std::uint32_t>(bytes, 76 * i + 52)] < 3)
				{
					std::printf("  draw inst %u: model field 0x%08X in the stream, loaded 0x%08X ( %s )\n", i, rd<std::uint32_t>(bytes, 76 * i + 52), world.staticModels[i].raw.model,
						std::string(world.staticModels[i].modelName).c_str());
				}
			}
		}
		for (const auto& [value, count] : plain)
		{
			std::printf("model fields that are not zone pointers: 0x%08X x%d\n", value, count);
		}

		// model bounds: XModel +316 against the instances
		const auto insts = zone.get_array<c1::GfxStaticModelInst>(d.smodelInsts, d.smodelCount);
		int in_zone = 0, reference = 0;
		double worst[2] = {};
		int good[2] = {};
		for (std::size_t i = 0; i < world.staticModels.size(); i++)
		{
			const auto& s = world.staticModels[i].raw;
			if (zone.reference(s.model))
			{
				reference++;
				continue;
			}
			in_zone++;
			const auto x = zone.bytes(s.model, sizeof(c1::XModel));
			for (int variant = 0; variant < 2; variant++)
			{
				// variant 0: mins, maxs at +316; variant 1: at +320 ( after a radius )
				const std::size_t at = variant == 0 ? 316 : 320;
				float mins[3], maxs[3];
				for (int k = 0; k < 3; k++)
				{
					mins[k] = rd<float>(x, at + 4 * k);
					maxs[k] = rd<float>(x, at + 12 + 4 * k);
				}
				double error = 0;
				for (int k = 0; k < 3; k++)
				{
					double mid = s.origin[k], half = 0;
					for (int a = 0; a < 3; a++)
					{
						mid += s.axis[a][k] * s.scale * 0.5 * (mins[a] + maxs[a]);
						half += std::fabs(s.axis[a][k]) * s.scale * 0.5 * (maxs[a] - mins[a]);
					}
					error = std::max(error, std::fabs(mid - insts[i].bounds.midPoint[k]));
					error = std::max(error, std::fabs(half - insts[i].bounds.halfSize[k]));
				}
				good[variant] += error < 0.05;
				worst[variant] = std::max(worst[variant], error);
				if (variant == 0 && i < 3)
				{
					std::printf("  %s: +316 %.2f %.2f %.2f | %.2f %.2f %.2f | %.2f %.2f, error %.3f\n", std::string(world.staticModels[i].modelName).c_str(), mins[0], mins[1], mins[2],
						maxs[0], maxs[1], maxs[2], rd<float>(x, 340), rd<float>(x, 344), error);
				}
			}
		}
		std::printf("static models: %d of a model in the zone, %d of a model of another zone; instance bounds == placed ( mins, maxs at XModel+316 ): %d ( worst %.3f ); at +320: %d ( worst %.3f )\n",
			in_zone, reference, good[0], worst[0], good[1], worst[1]);

		// shadow geometry lists
		const auto header = zone.bytes(address, sizeof(c1::GfxWorld));
		const zaddr shadow = rd<std::uint32_t>(header, 0x1C0);
		const ZoneDataId shadow_id = editor.data_at(shadow);
		ZoneDataInfo shadow_info;
		editor.data_info(shadow_id, shadow_info);
		std::printf("shadow geometry: piece %u, %u bytes, %u pointers\n", shadow_info.index, shadow_info.size, shadow_info.pointers);
		std::vector<int> in_light(d.smodelCount, -1);
		int agree = 0, differ = 0;
		for (std::uint32_t i = 0; i < world.raw.primaryLightCount; i++)
		{
			const auto g = zone.bytes(shadow + 12 * i, 12);
			const std::uint16_t surfaces = rd<std::uint16_t>(g, 0), models = rd<std::uint16_t>(g, 2);
			const auto list = zone.get_array<std::uint16_t>(rd<std::uint32_t>(g, 8), models);
			const auto surface_list = zone.get_array<std::uint16_t>(rd<std::uint32_t>(g, 4), surfaces);
			int surf_agree = 0;
			for (const std::uint16_t surf : surface_list)
			{
				surf_agree += surf < world.surfaces.size() && world.surfaces[surf].raw.primaryLightIndex == i;
			}
			for (const std::uint16_t m : list)
			{
				if (m < in_light.size())
				{
					in_light[m] = static_cast<int>(i);
					(world.staticModels[m].raw.primaryLightIndex == i ? agree : differ)++;
				}
			}
			std::printf("  light %u: %u surfaces ( %d with that primaryLightIndex ) at 0x%08X, %u smodels at 0x%08X ( sorted %d )\n", i, surfaces, surf_agree, rd<std::uint32_t>(g, 4), models,
				rd<std::uint32_t>(g, 8), static_cast<int>(std::is_sorted(list.begin(), list.end())));
		}
		std::printf("  smodels in the list of their primaryLightIndex: %d, of another light: %d, in no list: %d\n", agree, differ,
			static_cast<int>(std::count(in_light.begin(), in_light.end(), -1)));
		for (const ZonePointer& p : editor.data_pointers(shadow_id))
		{
			if (p.offset < 60)
			{
				std::printf("  shadow geometry +%u -> %s\n", p.offset, describe(editor, p.target).c_str());
			}
		}

		// order of the instances: by model? cells' root lists: order
		int model_runs = 1;
		for (std::size_t i = 1; i < world.staticModels.size(); i++)
		{
			model_runs += world.staticModels[i].raw.model != world.staticModels[i - 1].raw.model;
		}
		std::set<zaddr> distinct;
		for (const auto& s : world.staticModels)
		{
			distinct.insert(s.raw.model);
		}
		std::printf("instance order: %d runs of equal model over %zu distinct models\n", model_runs, distinct.size());

		// the clip map's list
		ClipMapView clip_view;
		if (read_clip_map(zone, first_asset(zone, c1::ASSET_TYPE_CLIPMAP_MP), clip_view))
		{
			const ZoneDataId list = editor.data_at(clip_view.raw.staticModelList);
			std::map<std::string, int> clip_kinds;
			int k = 0;
			for (const ZonePointer& p : editor.data_pointers(list))
			{
				std::string key = kind_name(p.target.kind);
				if (p.target.kind == ZoneTarget::Kind::data)
				{
					key += p.target.data == list ? " ( the array itself )" : p.target.data == draw ? " ( smodelDrawInsts )" : " ( other data )";
				}
				clip_kinds[key]++;
				if (k++ < 4)
				{
					std::printf("  clip smodel %u field +%u -> %s\n", p.offset / 76, p.offset % 76, describe(editor, p.target).c_str());
				}
			}
			for (const auto& [key, count] : clip_kinds)
			{
				std::printf("clip staticModelList: %d pointers to %s\n", count, key.c_str());
			}
			// BSP nodes
			const auto nodes = zone.bytes(clip_view.raw.nodes, 8u * clip_view.raw.numNodes);
			int leaf_children = 0, node_children = 0, min_child = 0, max_child = 0;
			std::set<int> leaves;
			for (std::uint32_t n = 0; n < clip_view.raw.numNodes; n++)
			{
				for (int c = 0; c < 2; c++)
				{
					const int child = rd<std::int16_t>(nodes, 8 * n + 4 + 2 * c);
					(child < 0 ? leaf_children : node_children)++;
					min_child = std::min(min_child, child);
					max_child = std::max(max_child, child);
					if (child < 0)
					{
						leaves.insert(-1 - child);
					}
				}
				if (n < 4)
				{
					const c1::cplane_s plane = zone.get<c1::cplane_s>(rd<std::uint32_t>(nodes, 8 * n));
					std::printf("  bsp node %u: plane %.3f %.3f %.3f | %.2f children %d %d\n", n, plane.normal[0], plane.normal[1], plane.normal[2], plane.dist,
						rd<std::int16_t>(nodes, 8 * n + 4), rd<std::int16_t>(nodes, 8 * n + 6));
				}
			}
			std::printf("bsp: %u nodes, children that are nodes %d ( max %d ), leaves %d ( min %d, %zu distinct of %u leafs )\n", clip_view.raw.numNodes, node_children, max_child,
				leaf_children, min_child, leaves.size(), clip_view.raw.numLeafs);
		}
		return 0;
	}

	int clip(const ZoneEditor& editor)
	{
		const Zone& zone = editor.zone();
		zaddr address = first_asset(zone, c1::ASSET_TYPE_CLIPMAP_MP);
		ClipMapView view;
		if (!address || !read_clip_map(zone, address, view))
		{
			std::printf("no col_map_mp\n");
			return 1;
		}
		const auto& raw = view.raw;
		std::printf("col_map %s: planes %d smodels %u materials %u sides %u edges %u nodes %u leafs %u leafBrushNodes %u leafBrushes %u verts %u tris %d borders %d partitions %d aabbTrees %d subModels %u brushes %u smodelNodes %u dynEnts %u %u\n",
			std::string(view.name).c_str(), raw.planeCount, raw.numStaticModels, raw.numMaterials, raw.numBrushSides, raw.numBrushEdges, raw.numNodes, raw.numLeafs,
			raw.leafbrushNodesCount, raw.numLeafBrushes, raw.vertCount, raw.triCount, raw.borderCount, raw.partitionCount, raw.aabbTreeCount, raw.numSubModels, raw.numBrushes,
			raw.smodelNodeCount, raw.dynEntCount[0], raw.dynEntCount[1]);
		const auto header = zone.bytes(address, sizeof(c1::clipMap_t));
		std::printf("header words:");
		for (std::uint32_t o = 0; o < sizeof(c1::clipMap_t); o += 4)
		{
			if (o % 32 == 0)
			{
				std::printf("\n  +0x%03X", o);
			}
			std::printf(" %08X", rd<std::uint32_t>(header, o));
		}
		std::printf("\n");

		// leafs
		const auto leafs = zone.get_array<c1::cLeaf_t>(raw.leafs, raw.numLeafs);
		std::map<int, int> leaf_nodes;
		for (const auto& leaf : leafs)
		{
			leaf_nodes[leaf.leafBrushNode]++;
		}
		std::printf("leafs: %zu, distinct leafBrushNode %zu:", leafs.size(), leaf_nodes.size());
		int shown = 0;
		for (const auto& [node, count] : leaf_nodes)
		{
			if (shown++ < 12)
			{
				std::printf(" %d x%d", node, count);
			}
		}
		std::printf("\n");
		for (std::size_t i = 0; i < leafs.size() && i < 4; i++)
		{
			const auto& l = leafs[i];
			std::printf("  leaf %zu: aabb %u+%u brushContents %08X terrainContents %08X mid %.0f %.0f %.0f half %.0f %.0f %.0f node %d\n", i, l.firstCollAabbIndex, l.collAabbCount,
				l.brushContents, l.terrainContents, l.bounds.midPoint[0], l.bounds.midPoint[1], l.bounds.midPoint[2], l.bounds.halfSize[0], l.bounds.halfSize[1], l.bounds.halfSize[2],
				l.leafBrushNode);
		}
		for (std::size_t i = 0; i < view.subModels.size() && i < 5; i++)
		{
			const auto& m = view.subModels[i];
			std::printf("  cmodel %zu: mid %.0f %.0f %.0f half %.0f %.0f %.0f radius %.1f leaf: aabb %u+%u brushContents %08X terrain %08X node %d leafmid %.0f %.0f %.0f leafhalf %.0f %.0f %.0f\n", i,
				m.bounds.midPoint[0], m.bounds.midPoint[1], m.bounds.midPoint[2], m.bounds.halfSize[0], m.bounds.halfSize[1], m.bounds.halfSize[2], m.radius, m.leaf.firstCollAabbIndex,
				m.leaf.collAabbCount, m.leaf.brushContents, m.leaf.terrainContents, m.leaf.leafBrushNode, m.leaf.bounds.midPoint[0], m.leaf.bounds.midPoint[1], m.leaf.bounds.midPoint[2],
				m.leaf.bounds.halfSize[0], m.leaf.bounds.halfSize[1], m.leaf.bounds.halfSize[2]);
		}

		// leaf brush nodes
		const auto nodes = zone.get_array<c1::cLeafBrushNode_s>(raw.leafbrushNodes, raw.leafbrushNodesCount);
		std::map<int, int> counts, axes;
		int leaves = 0, inner = 0, in_array = 0, out_array = 0;
		for (std::size_t n = 0; n < nodes.size(); n++)
		{
			const auto& node = nodes[n];
			axes[node.axis]++;
			if (node.leafBrushCount > 0)
			{
				leaves++;
				const zaddr list = rd<std::uint32_t>({ node.data, 12 }, 0);
				(list >= raw.leafbrushes && list + 2u * node.leafBrushCount <= raw.leafbrushes + 2u * raw.numLeafBrushes ? in_array : out_array)++;
			}
			else
			{
				inner++;
				counts[node.leafBrushCount]++;
			}
			if (n < 16)
			{
				std::printf("  lbnode %zu: axis %u unk %u count %d contents %08X", n, node.axis, node._unk1, node.leafBrushCount, node.contents);
				if (node.leafBrushCount > 0)
				{
					std::printf(" list +%d\n", static_cast<int>(rd<std::uint32_t>({ node.data, 12 }, 0) - raw.leafbrushes) / 2);
				}
				else
				{
					std::printf(" dist %.2f range %.2f children +%u +%u\n", rd<float>({ node.data, 12 }, 0), rd<float>({ node.data, 12 }, 4), rd<std::uint16_t>({ node.data, 12 }, 8),
						rd<std::uint16_t>({ node.data, 12 }, 10));
				}
			}
		}
		std::printf("leaf brush nodes: %zu, leaves %d ( list inside leafbrushes %d, elsewhere %d ), inner %d; inner leafBrushCount:", nodes.size(), leaves, in_array, out_array, inner);
		for (const auto& [value, count] : counts)
		{
			std::printf(" %d x%d", value, count);
		}
		std::printf("; axis:");
		for (const auto& [value, count] : axes)
		{
			std::printf(" %d x%d", value, count);
		}
		std::printf("\n");

		// which side of dist do the children lie on, and what is range
		{
			const auto bounds = zone.get_array<c1::Bounds>(raw.brushBounds, raw.numBrushes);
			struct extent { float lo = 1e30f, hi = -1e30f; };
			std::vector<std::array<extent, 3>> below(nodes.size());
			std::vector<std::uint8_t> done(nodes.size(), 0);
			const std::function<void(std::size_t)> walk = [&](const std::size_t n)
			{
				if (n >= nodes.size() || done[n])
				{
					return;
				}
				done[n] = 1;
				const auto& node = nodes[n];
				const auto merge = [&](const std::size_t child)
				{
					walk(child);
					for (int k = 0; k < 3; k++)
					{
						below[n][k].lo = std::min(below[n][k].lo, below[child][k].lo);
						below[n][k].hi = std::max(below[n][k].hi, below[child][k].hi);
					}
				};
				if (node.leafBrushCount > 0)
				{
					const auto list = zone.get_array<std::uint16_t>(rd<std::uint32_t>({ node.data, 12 }, 0), static_cast<std::size_t>(node.leafBrushCount));
					for (const std::uint16_t b : list)
					{
						if (b < bounds.size())
						{
							for (int k = 0; k < 3; k++)
							{
								below[n][k].lo = std::min(below[n][k].lo, bounds[b].midPoint[k] - bounds[b].halfSize[k]);
								below[n][k].hi = std::max(below[n][k].hi, bounds[b].midPoint[k] + bounds[b].halfSize[k]);
							}
						}
					}
					return;
				}
				if (node.leafBrushCount < 0)
				{
					merge(n + 1);
				}
				for (int c = 0; c < 2; c++)
				{
					const std::uint16_t off = rd<std::uint16_t>({ node.data, 12 }, 8 + 2 * c);
					if (off)
					{
						merge(n + off);
					}
				}
			};
			for (std::size_t n = 0; n < nodes.size(); n++)
			{
				walk(n);
			}
			int front0 = 0, back0 = 0, other = 0, checked = 0, range_ok = 0, range_bad = 0;
			double worst = 0;
			for (std::size_t n = 0; n < nodes.size(); n++)
			{
				const auto& node = nodes[n];
				if (node.leafBrushCount > 0 || node.axis > 2)
				{
					continue;
				}
				const float dist = rd<float>({ node.data, 12 }, 0), range = rd<float>({ node.data, 12 }, 4);
				const std::uint16_t c0 = rd<std::uint16_t>({ node.data, 12 }, 8), c1o = rd<std::uint16_t>({ node.data, 12 }, 10);
				if (!c0 || !c1o)
				{
					continue;
				}
				const extent a = below[n + c0][node.axis], b = below[n + c1o][node.axis];
				checked++;
				// child 0 in front ( +axis ) of dist ?
				if (a.lo >= dist - 0.01f - std::fabs(range) && b.hi <= dist + 0.01f + std::fabs(range))
				{
					front0++;
					// how far do they reach across: front child's lo vs dist, back child's hi vs dist
					const double across = std::max<double>(dist - a.lo, b.hi - dist);
					(std::fabs(across - range) < 0.02 || across <= range + 0.02 ? range_ok : range_bad)++;
					worst = std::max(worst, across - range);
					if (checked < 12)
					{
						std::printf("  inner %zu axis %u dist %.2f range %.3f: child0 [%.2f %.2f] child1 [%.2f %.2f] across %.3f\n", n, node.axis, dist, range, a.lo, a.hi, b.lo, b.hi, across);
					}
				}
				else if (b.lo >= dist - 0.01f - std::fabs(range) && a.hi <= dist + 0.01f + std::fabs(range))
				{
					back0++;
				}
				else
				{
					other++;
					if (other < 6)
					{
						std::printf("  inner %zu axis %u dist %.2f range %.3f: child0 [%.2f %.2f] child1 [%.2f %.2f] ??\n", n, node.axis, dist, range, a.lo, a.hi, b.lo, b.hi);
					}
				}
			}
			std::printf("inner nodes with two children %d: child0 on the + side %d ( reach across <= range %d, more %d, worst excess %.3f ), child0 on the - side %d, neither %d\n", checked,
				front0, range_ok, range_bad, worst, back0, other);
		}

		// static models: clip list against the gfx list
		{
			GfxWorldView world;
			read_gfx_world(zone, first_asset(zone, c1::ASSET_TYPE_GFXWORLD), world);
			std::map<std::string, int> gfx_count, clip_count;
			for (const auto& s : world.staticModels)
			{
				gfx_count[std::string(s.modelName)]++;
			}
			int matched = 0;
			for (const auto& s : view.staticModels)
			{
				clip_count[std::string(s.modelName)]++;
				for (const auto& g : world.staticModels)
				{
					if (g.modelName == s.modelName && std::fabs(g.raw.origin[0] - s.raw.origin[0]) < 0.01f && std::fabs(g.raw.origin[1] - s.raw.origin[1]) < 0.01f
						&& std::fabs(g.raw.origin[2] - s.raw.origin[2]) < 0.01f)
					{
						matched++;
						break;
					}
				}
			}
			int all = 0, none = 0, some = 0;
			for (const auto& [name, count] : gfx_count)
			{
				const int c = clip_count.count(name) ? clip_count[name] : 0;
				(c == count ? all : c == 0 ? none : some)++;
				if (c != count && c != 0)
				{
					std::printf("  model %s: %d gfx instances, %d clip entries\n", name.c_str(), count, c);
				}
			}
			std::printf("static models: gfx %zu, clip %zu ( %d have a gfx instance of the same model at the same origin ); models whose instances all have a clip entry %d, none %d, some %d\n",
				world.staticModels.size(), view.staticModels.size(), matched, all, none, some);
			// collision of the xmodels
			int with_coll = 0, without = 0, mismatch = 0;
			for (const auto& [name, count] : gfx_count)
			{
				const zaddr model = zone.find_asset(c1::ASSET_TYPE_XMODEL, name);
				if (!model)
				{
					continue;
				}
				const c1::XModel x = zone.get<c1::XModel>(model);
				const bool has = clip_count.count(name) != 0;
				const bool coll = x.numCollSurfs > 0 || x.physCollmap != 0;
				(coll ? with_coll : without)++;
				if (coll != has)
				{
					mismatch++;
					if (mismatch < 8)
					{
						std::printf("  model %s: collSurfs %d contents %08X physCollmap %08X flags %02X, clip entries %d\n", name.c_str(), x.numCollSurfs, x.contents, x.physCollmap, x.flags,
							has ? clip_count[name] : 0);
					}
				}
			}
			std::printf("  xmodels in the zone: with collision %d, without %d; ( has collision ) != ( has clip entries ): %d\n", with_coll, without, mismatch);
			for (std::size_t i = 0; i < view.staticModels.size() && i < 3; i++)
			{
				const auto& s = view.staticModels[i].raw;
				std::printf("  clip smodel %zu %s origin %.1f %.1f %.1f inv %.3f %.3f %.3f / %.3f %.3f %.3f / %.3f %.3f %.3f abs mid %.1f %.1f %.1f half %.1f %.1f %.1f\n", i,
					std::string(view.staticModels[i].modelName).c_str(), s.origin[0], s.origin[1], s.origin[2], s.invScaledAxis[0][0], s.invScaledAxis[0][1], s.invScaledAxis[0][2],
					s.invScaledAxis[1][0], s.invScaledAxis[1][1], s.invScaledAxis[1][2], s.invScaledAxis[2][0], s.invScaledAxis[2][1], s.invScaledAxis[2][2], s.absBounds.midPoint[0],
					s.absBounds.midPoint[1], s.absBounds.midPoint[2], s.absBounds.halfSize[0], s.absBounds.halfSize[1], s.absBounds.halfSize[2]);
			}
			const auto smnodes = zone.bytes(raw.smodelNodes, 28u * raw.smodelNodeCount);
			for (std::uint32_t n = 0; n < raw.smodelNodeCount && n < 6; n++)
			{
				std::printf("  smodel node %u: mid %.0f %.0f %.0f half %.0f %.0f %.0f first %u count %u\n", n, rd<float>(smnodes, 28 * n), rd<float>(smnodes, 28 * n + 4),
					rd<float>(smnodes, 28 * n + 8), rd<float>(smnodes, 28 * n + 12), rd<float>(smnodes, 28 * n + 16), rd<float>(smnodes, 28 * n + 20),
					rd<std::uint16_t>(smnodes, 28 * n + 24), rd<std::uint16_t>(smnodes, 28 * n + 26));
			}
		}

		// brushes: sides inside the brushsides array, edges
		{
			int sides_in = 0, sides_out = 0, edges_in = 0, edges_out = 0, edges_null = 0;
			std::map<int, int> numsides;
			for (std::size_t b = 0; b < view.brushes.size(); b++)
			{
				const auto& br = view.brushes[b].raw;
				numsides[std::min<int>(br.numsides, 12)]++;
				if (br.numsides)
				{
					(br.sides >= raw.brushsides && br.sides + 8u * br.numsides <= raw.brushsides + 8u * raw.numBrushSides ? sides_in : sides_out)++;
				}
				if (!br.baseAdjacentSide)
				{
					edges_null++;
				}
				else
				{
					(br.baseAdjacentSide >= raw.brushEdges && br.baseAdjacentSide < raw.brushEdges + raw.numBrushEdges ? edges_in : edges_out)++;
				}
				if (b < 4 || (br.numsides >= 2 && b < 400 && b % 97 == 0))
				{
					std::printf("  brush %zu: sides %u ( +%d ) glass %u base edges +%d axialMat %d %d %d %d %d %d first %u %u %u %u %u %u count %u %u %u %u %u %u contents %08X\n", b, br.numsides,
						br.numsides ? static_cast<int>(br.sides - raw.brushsides) / 8 : -1, br.glassPieceIndex, br.baseAdjacentSide ? static_cast<int>(br.baseAdjacentSide - raw.brushEdges) : -1,
						br.axialMaterialNum[0][0], br.axialMaterialNum[0][1], br.axialMaterialNum[0][2], br.axialMaterialNum[1][0], br.axialMaterialNum[1][1], br.axialMaterialNum[1][2],
						br.firstAdjacentSideOffsets[0][0], br.firstAdjacentSideOffsets[0][1], br.firstAdjacentSideOffsets[0][2], br.firstAdjacentSideOffsets[1][0],
						br.firstAdjacentSideOffsets[1][1], br.firstAdjacentSideOffsets[1][2], br.edgeCount[0][0], br.edgeCount[0][1], br.edgeCount[0][2], br.edgeCount[1][0], br.edgeCount[1][1],
						br.edgeCount[1][2], view.brushes[b].contents);
					const auto sides = zone.get_array<c1::cbrushside_t>(br.sides, br.numsides);
					for (std::size_t s = 0; s < sides.size(); s++)
					{
						const c1::cplane_s plane = zone.get<c1::cplane_s>(sides[s].plane);
						std::printf("      side %zu: plane +%d ( %.3f %.3f %.3f | %.2f type %u sign %u ) material %u first %u edges %u\n", s, static_cast<int>(sides[s].plane - raw.planes) / 20,
							plane.normal[0], plane.normal[1], plane.normal[2], plane.dist, plane.type, plane.signbits, sides[s].materialNum, sides[s].firstAdjacentSideOffset, sides[s].edgeCount);
					}
					std::uint32_t total = 0;
					for (int a = 0; a < 2; a++)
					{
						for (int k = 0; k < 3; k++)
						{
							total = std::max<std::uint32_t>(total, br.firstAdjacentSideOffsets[a][k] + br.edgeCount[a][k]);
						}
					}
					for (const auto& s : sides)
					{
						total = std::max<std::uint32_t>(total, s.firstAdjacentSideOffset + s.edgeCount);
					}
					const auto edges = zone.bytes(br.baseAdjacentSide, total);
					std::printf("      edges:");
					for (const std::uint8_t e : edges)
					{
						std::printf(" %u", e);
					}
					std::printf("\n");
				}
			}
			std::printf("brushes: sides inside brushsides %d, elsewhere %d; edges inside brushEdges %d, elsewhere %d, null %d; numsides:", sides_in, sides_out, edges_in, edges_out, edges_null);
			for (const auto& [value, count] : numsides)
			{
				std::printf(" %d x%d", value, count);
			}
			std::printf("\n");
		}
		return 0;
	}
}

int main(const int argc, char** argv)
{
	if (argc < 4)
	{
		std::fprintf(stderr, "codol_mapc_probe graph|fields|gfx|smodels|clip <map.ff> <C1.exe> [asset type] [max pieces]\n");
		return 2;
	}
	ClientKey key;
	if (const Status s = key.load_from_exe(argv[3]); !s)
	{
		std::fprintf(stderr, "%s\n", s.message.c_str());
		return 1;
	}
	ZoneEditor editor;
	if (const Status s = editor.open(argv[2], key); !s)
	{
		std::fprintf(stderr, "%s\n", s.message.c_str());
		return 1;
	}
	const std::string command = argv[1];
	if (command == "graph" && argc >= 5)
	{
		return graph(editor, argv[4], argc >= 6 ? static_cast<std::uint32_t>(std::atoi(argv[5])) : 40u);
	}
	if (command == "fields" && argc >= 5)
	{
		return fields(editor, argv[4]);
	}
	if (command == "smodels")
	{
		return smodels(editor);
	}
	if (command == "gfx")
	{
		return gfx(editor);
	}
	if (command == "clip")
	{
		return clip(editor);
	}
	std::fprintf(stderr, "unknown command\n");
	return 2;
}
