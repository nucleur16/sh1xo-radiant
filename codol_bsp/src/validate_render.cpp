// Checks of GfxWorld, ComWorld, GameWorldMp and MapEnts of a map zone, on the zone as the game's loaders build it:
// every index the renderer follows is inside its array, the lists that have to be permutations are, the trees are
// consistent. ( The collision side is codol_mapc's check_server_collision. )

#include "world.hpp"
#include "gfx_layout.hpp"

namespace codol::bsp
{
	using namespace gfx;

	namespace
	{
		struct checker
		{
			std::vector<std::string>& problems;
			std::size_t limit = 40;
			void fail(std::string text)
			{
				if (problems.size() < limit)
				{
					problems.push_back(std::move(text));
				}
			}
			bool expect(const bool ok, const char* what)
			{
				if (!ok)
				{
					fail(what);
				}
				return ok;
			}
		};

		bool holds(const c1::Bounds& outer, const c1::Bounds& inner, const float eps)
		{
			for (int k = 0; k < 3; k++)
			{
				if (inner.midPoint[k] - inner.halfSize[k] < outer.midPoint[k] - outer.halfSize[k] - eps
					|| inner.midPoint[k] + inner.halfSize[k] > outer.midPoint[k] + outer.halfSize[k] + eps)
				{
					return false;
				}
			}
			return true;
		}
	}

	void validate_render(const std::span<const std::uint8_t> zone_bytes, std::vector<std::string>& problems, std::string* summary)
	{
		checker c{ problems };
		Zone zone;
		if (const zone::Status s = zone.load(std::vector<std::uint8_t>(zone_bytes.begin(), zone_bytes.end()), "validate"); !s)
		{
			c.fail("the zone does not load: " + s.message);
			return;
		}
		const zaddr world_address = zone::first_asset(zone, c1::ASSET_TYPE_GFXWORLD);
		zone::GfxWorldView view;
		if (!world_address || !zone::read_gfx_world(zone, world_address, view))
		{
			c.fail("the zone has no GfxWorld");
			return;
		}
		const c1::GfxWorld& g = view.raw;
		const auto word = [&](const std::uint32_t offset) { return zone.get<std::uint32_t>(world_address + offset); };

		// ---- names: what the dedicated server pairs the assets by
		const std::string name(view.name), base(view.baseName);
		c.expect(name == "maps/mp/" + base + ".d3dbsp", "GfxWorld: the name is not maps/mp/<base name>.d3dbsp");
		zone::ClipMapView clip;
		const zaddr clip_address = zone::first_asset(zone, c1::ASSET_TYPE_CLIPMAP_MP);
		if (c.expect(clip_address && zone::read_clip_map(zone, clip_address, clip), "the zone has no clip map"))
		{
			c.expect(clip.name == view.name, "clip map and GfxWorld have different names");
			c.expect(clip.raw.planeCount == g.planeCount && clip.raw.planes == word(W_PLANES), "clip map and GfxWorld do not share one plane array");
			c.expect(clip.raw.checksum == g.checksum, "clip map and GfxWorld have different checksums");
		}

		// ---- planes and the cell tree
		const std::vector<c1::cplane_s> planes = zone.get_array<c1::cplane_s>(word(W_PLANES), static_cast<std::size_t>(std::max(g.planeCount, 0)));
		for (const c1::cplane_s& p : planes)
		{
			const float l = p.normal[0] * p.normal[0] + p.normal[1] * p.normal[1] + p.normal[2] * p.normal[2];
			if (!(std::fabs(l - 1.0f) < 0.001f) || !std::isfinite(p.dist))
			{
				c.fail("GfxWorld: a plane without a unit normal");
				break;
			}
		}
		const std::vector<std::uint16_t> nodes = zone.get_array<std::uint16_t>(word(W_NODES), static_cast<std::size_t>(std::max(g.nodeCount, 0)));
		std::vector<std::uint8_t> cell_in_tree(static_cast<std::size_t>(std::max(g.cellCount, 0)), 0);
		{
			// the stream is a tree in pre order: walking it uses every entry once
			bool ok = !nodes.empty();
			std::size_t at = 0;
			std::vector<std::size_t> back_starts;
			while (ok)
			{
				if (at >= nodes.size())
				{
					ok = false;
					break;
				}
				const std::uint32_t v = nodes[at];
				if (v <= static_cast<std::uint32_t>(g.cellCount))
				{
					if (v > 0)
					{
						cell_in_tree[v - 1] = 1;
					}
					at += 1;
					if (back_starts.empty())
					{
						break;
					}
					ok = back_starts.back() == at;
					back_starts.pop_back();
					continue;
				}
				ok = v - static_cast<std::uint32_t>(g.cellCount) - 1 < planes.size() && at + 1 < nodes.size() && nodes[at + 1] >= 3;
				if (ok)
				{
					back_starts.push_back(at + nodes[at + 1]);
					at += 2;
				}
			}
			c.expect(ok && at == nodes.size() && back_starts.empty(), "GfxWorld: the cell tree ( dpvsPlanes.nodes ) is not a well formed tree over the plane array");
		}
		c.expect(g.cellCount >= 1, "GfxWorld: no cell");
		for (std::size_t k = 0; k < cell_in_tree.size(); k++)
		{
			if (!cell_in_tree[k])
			{
				c.fail(format("GfxWorld: cell %zu is no leaf of the cell tree: a camera can never be in it", k));
				break;
			}
		}

		// ---- draw data
		const std::uint32_t vertex_count = g.draw.vertexCount, index_count = g.draw.indexCount, surface_count = g.surfaceCount;
		const std::vector<std::uint16_t> indices = view.indices();
		const std::uint32_t probe_count = g.draw.reflectionProbeCount, lightmap_count = static_cast<std::uint32_t>(g.draw.lightmapCount);
		c.expect(probe_count >= 2, "GfxWorld: fewer than two reflection probes ( probe 0 is the default, a cell needs another )");
		c.expect(g.primaryLightCount >= 2 && g.lastSunPrimaryLightIndex >= 1 && g.lastSunPrimaryLightIndex < g.primaryLightCount,
			"GfxWorld: primary lights need 'none' and a sun, and the last sun index inside the list");
		{
			// offset 420, the bits that say which cells have sun lit surfaces: null in every shipped map. A pointer makes the
			// loader allocate an array of zeros, which no shipped map gives the game
			std::uint32_t sun_lit_cells = 0;
			std::memcpy(&sun_lit_cells, g._unk416 + 4, sizeof(sun_lit_cells));
			c.expect(sun_lit_cells == 0, "GfxWorld: the sun lit cell bits ( offset 420 ) are set; they are null in every shipped map");
		}
		c.expect(view.surfaces.size() == surface_count && indices.size() == index_count, "GfxWorld: surface / index arrays are not what the counts say");
		std::size_t triangles = 0;
		for (std::size_t s = 0; s < view.surfaces.size(); s++)
		{
			const c1::GfxSurface& surface = view.surfaces[s].raw;
			bool ok = static_cast<std::uint64_t>(surface.firstVertex) + surface.vertexCount <= vertex_count
				&& static_cast<std::uint64_t>(surface.baseIndex) + 3ull * surface.triCount <= index_count && surface.vertexCount > 0 && surface.triCount > 0;
			for (std::uint32_t k = 0; ok && k < 3u * surface.triCount; k++)
			{
				ok = indices[surface.baseIndex + k] < surface.vertexCount;
			}
			ok = ok && (surface.lightmapIndex < lightmap_count || surface.lightmapIndex == 31) && surface.reflectionProbeIndex < probe_count
				&& surface.primaryLightIndex < g.primaryLightCount && !view.surfaces[s].materialName.empty();
			if (!ok)
			{
				c.fail(format("GfxWorld: surface %zu indexes outside its arrays ( vertices, indices, lightmap, probe, light ) or has no material", s));
				break;
			}
			triangles += surface.triCount;
		}
		const std::vector<c1::GfxWorldVertex> vertices = view.vertices();
		for (const c1::GfxWorldVertex& v : vertices)
		{
			if (!std::isfinite(v.xyz[0]) || !std::isfinite(v.xyz[1]) || !std::isfinite(v.xyz[2]) || !std::isfinite(v.texCoord[0]) || !std::isfinite(v.texCoord[1])
				|| !(v.lmapCoord[0] >= -0.001f && v.lmapCoord[0] <= 1.001f && v.lmapCoord[1] >= -0.001f && v.lmapCoord[1] <= 1.001f))
			{
				c.fail("GfxWorld: a vertex with a number that is not finite or a lightmap coordinate outside 0 .. 1");
				break;
			}
		}

		// ---- dpvs: ranges, the sorted list, the trees of the cells
		const c1::GfxWorldDpvsStatic& d = g.dpvs;
		const std::uint32_t static_count = d.staticSurfaceCount;
		c.expect(static_count <= surface_count && d.litOpaqueSurfsBegin <= d.litOpaqueSurfsEnd && d.litOpaqueSurfsEnd <= d.litTransSurfsBegin
			&& d.litTransSurfsBegin <= d.litTransSurfsEnd && d.litTransSurfsEnd <= static_count && d.emissiveSurfsBegin <= d.emissiveSurfsEnd
			&& d.emissiveSurfsEnd <= surface_count, "GfxWorld: the surface ranges of dpvs are not in order");
		c.expect(d.surfaceVisDataCount == (((static_count + 127) >> 7) << 2) && d.smodelVisDataCount == (((d.smodelCount + 127) >> 7) << 2),
			"GfxWorld: visibility word counts do not follow from the counts");
		const std::vector<std::uint16_t> sorted = zone.get_array<std::uint16_t>(d.sortedSurfIndex, static_cast<std::size_t>(static_count) + d.staticSurfaceCountNoDecal);
		std::vector<std::uint8_t> seen(static_count, 0);
		bool permutation = sorted.size() == static_cast<std::size_t>(static_count) + d.staticSurfaceCountNoDecal;
		for (std::uint32_t k = 0; permutation && k < static_count; k++)
		{
			permutation = sorted[k] < static_count && !seen[sorted[k]];
			if (permutation)
			{
				seen[sorted[k]] = 1;
			}
		}
		c.expect(permutation, "GfxWorld: sortedSurfIndex does not list every static surface once");
		const std::vector<c1::Bounds> surface_bounds = zone.get_array<c1::Bounds>(d.surfacesBounds, surface_count);
		const std::vector<std::uint32_t> tree_counts = zone.get_array<std::uint32_t>(word(W_AABB_TREE_COUNTS), static_cast<std::size_t>(std::max(g.cellCount, 0)));
		const std::vector<zaddr> tree_pointers = zone.get_array<zaddr>(word(W_AABB_TREES), static_cast<std::size_t>(std::max(g.cellCount, 0)));
		std::vector<std::uint8_t> in_leaf(static_count, 0);
		std::size_t portal_total = 0, tree_nodes = 0;
		for (std::size_t cell = 0; cell < view.cells.size() && cell < tree_counts.size() && permutation; cell++)
		{
			const std::vector<GfxAabbTree> tree = zone.get_array<GfxAabbTree>(tree_pointers[cell], tree_counts[cell]);
			if (!c.expect(!tree.empty(), "GfxWorld: a cell without an AABB tree"))
			{
				break;
			}
			tree_nodes += tree.size();
			bool ok = true;
			for (std::size_t n = 0; ok && n < tree.size(); n++)
			{
				const GfxAabbTree& node = tree[n];
				ok = static_cast<std::uint32_t>(node.startSurfIndex) + node.surfaceCount <= static_count
					&& static_cast<std::size_t>(node.startSurfIndexNoDecal) + node.surfaceCountNoDecal <= sorted.size() && node.smodelIndexCount <= d.smodelCount;
				if (!ok)
				{
					break;
				}
				if (node.childCount == 0)
				{
					for (std::uint32_t k = 0; ok && k < node.surfaceCount; k++)
					{
						const std::uint16_t s = sorted[node.startSurfIndex + k];
						ok = !in_leaf[s] && holds(node.bounds, surface_bounds[s], 0.5f);
						in_leaf[s] = 1;
					}
					continue;
				}
				const std::size_t first = n + static_cast<std::size_t>(node.childrenOffset) / sizeof(GfxAabbTree);
				ok = node.childrenOffset > 0 && node.childrenOffset % sizeof(GfxAabbTree) == 0 && first + node.childCount <= tree.size();
				std::uint32_t at = node.startSurfIndex;
				for (std::size_t k = 0; ok && k < node.childCount; k++)
				{
					// a node's run is its children's runs back to back
					ok = tree[first + k].startSurfIndex == at && holds(node.bounds, tree[first + k].bounds, 0.5f);
					at += tree[first + k].surfaceCount;
				}
				ok = ok && at == static_cast<std::uint32_t>(node.startSurfIndex) + node.surfaceCount;
			}
			if (!ok)
			{
				c.fail(format("GfxWorld: the AABB tree of cell %zu is inconsistent ( runs, children, boxes )", cell));
				break;
			}
			const c1::GfxCell& raw = view.cells[cell].raw;
			const std::vector<std::uint8_t> probes = zone.get_array<std::uint8_t>(raw.reflectionProbes, raw.reflectionProbeCount);
			bool probes_ok = !probes.empty() && !(probes.size() == 1 && probes[0] == 0);
			for (const std::uint8_t p : probes)
			{
				probes_ok = probes_ok && p < probe_count;
			}
			c.expect(probes_ok, "GfxWorld: a cell lists no reflection probe, only probe 0, or one outside the array");
			for (const c1::GfxPortal& portal : view.cells[cell].portals)
			{
				portal_total++;
				GfxPortal p;
				std::memcpy(&p, &portal, sizeof(p));
				const std::vector<float> corners = zone.get_array<float>(p.vertices, 3u * p.vertexCount);
				bool portal_ok = p.cellIndex < view.cells.size() && p.cellIndex != cell && p.vertexCount >= 3 && corners.size() == 3u * p.vertexCount;
				for (std::size_t k = 0; portal_ok && k + 2 < corners.size(); k += 3)
				{
					portal_ok = std::fabs(p.plane[0] * corners[k] + p.plane[1] * corners[k + 1] + p.plane[2] * corners[k + 2] + p.plane[3]) < 0.1f;
				}
				c.expect(portal_ok, "GfxWorld: a portal leads to no other cell or its corners are not on its plane");
			}
		}
		if (permutation)
		{
			c.expect(std::find(in_leaf.begin(), in_leaf.end(), std::uint8_t{ 0 }) == in_leaf.end(), "GfxWorld: a static surface is in no leaf of a cell's AABB tree");
		}

		// ---- brush models
		for (const c1::GfxBrushModel& m : view.brushModels)
		{
			if (m.surfaceCount && static_cast<std::uint32_t>(m.startSurfIndex) + m.surfaceCount > surface_count)
			{
				c.fail("GfxWorld: a brush model's surfaces are outside the surface array");
				break;
			}
		}
		c.expect(clip_address == 0 || view.brushModels.size() == clip.subModels.size(), "GfxWorld and clip map have different numbers of brush models");

		// ---- light grid
		GfxLightGrid grid;
		std::memcpy(&grid, g.lightGrid, sizeof(grid));
		std::size_t grid_points = 0;
		{
			bool ok = grid.rowAxis < 3 && grid.colAxis < 3 && grid.rowAxis != grid.colAxis && grid.maxs[grid.rowAxis] >= grid.mins[grid.rowAxis];
			const std::size_t rows = ok ? grid.maxs[grid.rowAxis] - grid.mins[grid.rowAxis] + 1u : 0;
			const std::vector<std::uint16_t> starts = zone.get_array<std::uint16_t>(grid.rowDataStart, rows);
			const auto raw = zone.bytes(grid.rawRowData, grid.rawRowDataSize);
			const std::vector<GfxLightGridEntry> entries = zone.get_array<GfxLightGridEntry>(grid.entries, grid.entryCount);
			ok = ok && starts.size() == rows && raw.size() == grid.rawRowDataSize && entries.size() == grid.entryCount;
			for (std::size_t r = 0; ok && r < rows; r++)
			{
				if (starts[r] == 0xFFFF)
				{
					continue;
				}
				std::size_t at = static_cast<std::size_t>(starts[r]) * 4;
				GfxLightGridRow row;
				ok = at + sizeof(row) <= raw.size();
				if (!ok)
				{
					break;
				}
				std::memcpy(&row, raw.data() + at, sizeof(row));
				at += sizeof(row);
				std::size_t first = row.firstEntry;
				for (std::uint32_t col = 0; ok && col < row.colCount;)
				{
					ok = at + 2 <= raw.size();
					if (!ok)
					{
						break;
					}
					const std::uint32_t run = raw[at], layers = raw[at + 1];
					at += 2;
					if (layers)
					{
						at += row.zCount > 255 ? 2 : 1;
					}
					ok = run > 0 && at <= raw.size();
					first += static_cast<std::size_t>(run) * layers;
					col += run;
				}
				ok = ok && first <= entries.size();
				grid_points = std::max(grid_points, first);
			}
			for (std::size_t e = 0; ok && e < entries.size(); e++)
			{
				ok = entries[e].colorsIndex < grid.colorCount && entries[e].primaryLightIndex < g.primaryLightCount;
			}
			c.expect(ok, "GfxWorld: the light grid's rows, entries or colours do not fit together");
		}

		// ---- lightmaps
		{
			const std::vector<zaddr> pairs = zone.get_array<zaddr>(g.draw.lightmaps, 2u * lightmap_count);
			for (const zaddr image : pairs)
			{
				zone::ImageView iv;
				if (!image || !zone::read_image(zone, image, iv) || iv.streamed || iv.pixels.empty())
				{
					c.fail("GfxWorld: a lightmap image without pixels in the zone");
					break;
				}
			}
		}

		// ---- shadow geometry per primary light
		{
			const std::vector<GfxShadowGeometry> geometry = zone.get_array<GfxShadowGeometry>(word(W_SHADOW_GEOM), g.primaryLightCount);
			bool ok = geometry.size() == g.primaryLightCount;
			for (const GfxShadowGeometry& sg : geometry)
			{
				const std::vector<std::uint16_t> list = zone.get_array<std::uint16_t>(sg.sortedSurfIndex, sg.surfaceCount);
				ok = ok && list.size() == sg.surfaceCount && sg.smodelCount <= d.smodelCount;
				for (const std::uint16_t s : list)
				{
					ok = ok && s < surface_count;
				}
			}
			c.expect(ok, "GfxWorld: shadow geometry lists name surfaces that do not exist");
		}

		// ---- ComWorld
		zone::ComWorldView com;
		const zaddr com_address = zone::first_asset(zone, c1::ASSET_TYPE_COMWORLD);
		if (c.expect(com_address && zone::read_com_world(zone, com_address, com), "the zone has no ComWorld"))
		{
			c.expect(com.name == view.name && com.raw.primaryLightCount == g.primaryLightCount, "ComWorld: name or primary light count differ from the GfxWorld's");
			const std::vector<ComPrimaryLight> lights = zone.get_array<ComPrimaryLight>(com.raw.primaryLights, com.raw.primaryLightCount);
			if (lights.size() >= 2)
			{
				const float l = lights[1].dir[0] * lights[1].dir[0] + lights[1].dir[1] * lights[1].dir[1] + lights[1].dir[2] * lights[1].dir[2];
				c.expect(lights[0].type == 0 && lights[1].type == 1 && std::fabs(l - 1.0f) < 0.01f, "ComWorld: light 0 is not 'none' or light 1 is not a sun with a unit direction");
			}
			for (std::size_t k = 2; k < lights.size(); k++)
			{
				if (lights[k].type < 2 || lights[k].type > 3 || !(lights[k].radius > 0) || zone.c_string(lights[k].defName).empty())
				{
					c.fail(format("ComWorld: light %zu is no spot / omni light with a radius and a definition", k));
					break;
				}
			}
		}

		// ---- GameWorldMp, FxWorld, MapEnts, the animation list
		const zaddr game_address = zone::first_asset(zone, c1::ASSET_TYPE_GAMEWORLD_MP);
		c.expect(game_address && zone.get<std::uint32_t>(game_address + 0x50) != 0, "GameWorldMp: missing, or without glass data ( the server reads it without a check )");
		c.expect(zone::first_asset(zone, c1::ASSET_TYPE_FXWORLD) != 0, "the zone has no FxWorld");
		zone::MapEntsView ents;
		if (c.expect(zone::read_map_ents(zone, zone::find_map_ents(zone), ents), "the zone has no MapEnts"))
		{
			const std::string_view text = ents.entityString;
			const std::size_t close = text.find('}');
			c.expect(close != std::string_view::npos && text.substr(0, close).find("\"classname\" \"worldspawn\"") != std::string_view::npos,
				"MapEnts: the first entity is not the worldspawn");
			c.expect(text.find("mp_global_intermission") != std::string_view::npos, "MapEnts: no mp_global_intermission ( every gametype needs one )");
		}
		c.expect(zone.find_asset(c1::ASSET_TYPE_RAWFILE, "animlists/" + base + ".animlist") != 0, "the zone has no animlists/<map>.animlist ( the server refuses the map )");

		if (summary)
		{
			*summary += format("render ok: %u surfaces ( %u static ), %zu triangles, %u vertices, %d cells, %zu portals, %zu cell tree nodes, %zu AABB tree nodes, "
				"%u lightmaps, %u probes, %zu light grid points, %u primary lights", surface_count, static_count, triangles, vertex_count, g.cellCount, portal_total,
				nodes.size(), tree_nodes, lightmap_count, probe_count, grid_points, g.primaryLightCount);
		}
	}
}
