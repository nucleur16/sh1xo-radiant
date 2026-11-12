// GfxWorld ( gfx_map ): the world's render surfaces from the brush faces, sorted by material, the cells with their
// AABB trees, the BSP that puts a point into a cell, lightmaps, reflection probes, the light grid, the lists per
// primary light, and the plane array the clip map shares.
//
// The asset is written in the order Load_GfxWorld reads it ( see gfx_layout.hpp ).

#include "world.hpp"
#include "gfx_layout.hpp"
#include "render.hpp"

namespace codol::bsp
{
	using namespace gfx;

	namespace
	{
		constexpr float SURFACE_GRID = 1024.0f;		// faces of one material are grouped into surfaces by this grid
		constexpr std::size_t TREE_LEAF_SURFACES = 8;

		// GfxImage with its pixels in the zone, stored behind the field `chunk` + `offset`
		void add_inline_image(asset_builder& b, const std::uint32_t chunk, const std::size_t offset, const std::string& name, const std::uint8_t map_type,
			const std::uint8_t category, const int width, const int height, const std::uint32_t format, const std::uint32_t flags, const std::uint8_t levels,
			const std::vector<std::uint8_t>& pixels)
		{
			c1::GfxImage image{};
			image.loadDef = FOLLOWS;
			image.mapType = map_type;
			image.semantic = 1;
			image.category = category;
			const std::uint32_t memory = static_cast<std::uint32_t>(pixels.size());
			std::memcpy(image._unk7 + 5, &memory, 4);		// cardMemory
			std::memcpy(image._unk7 + 9, &memory, 4);
			image.width = static_cast<std::uint16_t>(width);
			image.height = static_cast<std::uint16_t>(height);
			image.depth = 1;
			image._unk26 = 1;
			image.name = FOLLOWS;
			b.follows(chunk, offset);
			b.add(image);
			b.add_string(name);
			c1::GfxImageLoadDef def{};
			def.levelCount = levels;
			def.flags = flags;
			def.format = format;
			def.resourceSize = memory;
			b.add(def);
			b.add(pixels.data(), pixels.size());
		}

		struct tree_build
		{
			const std::vector<render_surface>& surfaces;
			std::vector<GfxAabbTree> nodes;
			std::vector<std::uint16_t> order;		// sortedSurfIndex of this cell, relative to the cell's first entry

			// breadth first, so that the children of a node are next to each other
			void build(const std::vector<int>& all)
			{
				struct pending
				{
					std::vector<int> list;
					std::size_t node;
				};
				std::vector<pending> queue;
				nodes.push_back({});
				queue.push_back({ all, 0 });
				// the order of the surfaces is decided depth first ( a node's run is its children's runs back to back )
				// so the tree is made first and the runs are numbered afterwards
				std::vector<std::vector<int>> leaf_lists(1);
				std::vector<std::array<std::size_t, 2>> children(1, { 0, 0 });
				for (std::size_t q = 0; q < queue.size(); q++)
				{
					const std::vector<int> list = queue[q].list;
					const std::size_t node = queue[q].node;
					box3 box;
					for (const int s : list)
					{
						box.add(surfaces[static_cast<std::size_t>(s)].box);
					}
					nodes[node].bounds = box.bounds();
					if (list.size() <= TREE_LEAF_SURFACES)
					{
						leaf_lists[node] = list;
						continue;
					}
					int axis = 0;
					for (int k = 1; k < 3; k++)
					{
						if (box.maxs[k] - box.mins[k] > box.maxs[axis] - box.mins[axis])
						{
							axis = k;
						}
					}
					std::vector<int> sorted = list;
					std::sort(sorted.begin(), sorted.end(), [&](const int a, const int c)
					{
						const box3& x = surfaces[static_cast<std::size_t>(a)].box;
						const box3& y = surfaces[static_cast<std::size_t>(c)].box;
						return x.mins[axis] + x.maxs[axis] < y.mins[axis] + y.maxs[axis];
					});
					const std::size_t half = sorted.size() / 2;
					const std::size_t first = nodes.size();
					nodes.push_back({});
					nodes.push_back({});
					leaf_lists.resize(nodes.size());
					children.resize(nodes.size(), { 0, 0 });
					children[node] = { first, first + 1 };
					nodes[node].childCount = 2;
					nodes[node].childrenOffset = static_cast<std::int32_t>((first - node) * sizeof(GfxAabbTree));
					queue.push_back({ std::vector<int>(sorted.begin(), sorted.begin() + static_cast<std::ptrdiff_t>(half)), first });
					queue.push_back({ std::vector<int>(sorted.begin() + static_cast<std::ptrdiff_t>(half), sorted.end()), first + 1 });
				}
				number(0, leaf_lists, children);
			}

			void number(const std::size_t node, const std::vector<std::vector<int>>& leaf_lists, const std::vector<std::array<std::size_t, 2>>& children)
			{
				const std::size_t start = order.size();
				if (nodes[node].childCount)
				{
					number(children[node][0], leaf_lists, children);
					number(children[node][1], leaf_lists, children);
				}
				else
				{
					for (const int s : leaf_lists[node])
					{
						order.push_back(static_cast<std::uint16_t>(s));
					}
				}
				nodes[node].startSurfIndex = static_cast<std::uint16_t>(start);
				nodes[node].surfaceCount = static_cast<std::uint16_t>(order.size() - start);
			}
		};
	}

	void build_gfx_world(compile_context& ctx)
	{
		world& w = ctx.w;
		render_world rw;
		build_render_surfaces(ctx, rw);
		bake_lighting(ctx, rw);

		const std::vector<render_surface>& surfaces = rw.surfaces;
		const std::size_t surface_count = surfaces.size();
		if (surface_count > 0xFFFF)
		{
			throw compile_error(format("the map has %zu render surfaces; a GfxWorld indexes them with 16 bits", surface_count));
		}
		const std::uint32_t light_count = static_cast<std::uint32_t>(w.lights.size());
		const std::int32_t cell_count = static_cast<std::int32_t>(rw.cells.size());

		// ---- vertices and indices
		std::vector<c1::GfxWorldVertex> vertices;
		std::vector<std::uint16_t> indices;
		std::vector<c1::GfxSurface> packed(surface_count);
		std::vector<c1::Bounds> surface_bounds(surface_count);
		for (std::size_t s = 0; s < surface_count; s++)
		{
			const render_surface& surface = surfaces[s];
			c1::GfxSurface& p = packed[s];
			p.firstVertex = static_cast<std::uint32_t>(vertices.size());
			p.vertexCount = static_cast<std::uint16_t>(surface.vertices.size());
			p.triCount = static_cast<std::uint16_t>(surface.indices.size() / 3);
			p.baseIndex = static_cast<std::uint32_t>(indices.size());
			p.lightmapIndex = static_cast<std::uint8_t>(surface.lightmap);
			p.reflectionProbeIndex = static_cast<std::uint8_t>(surface.reflection_probe);
			p.primaryLightIndex = static_cast<std::uint8_t>(surface.primary_light);
			p.flags = surface.casts_sun_shadow ? 1 : 0;
			vertices.insert(vertices.end(), surface.vertices.begin(), surface.vertices.end());
			indices.insert(indices.end(), surface.indices.begin(), surface.indices.end());
			surface_bounds[s] = surface.box.bounds();
		}

		// ---- cells: AABB trees over their surfaces, and the sorted surface list ( the runs of the tree leaves )
		std::vector<std::vector<GfxAabbTree>> trees(rw.cells.size());
		std::vector<std::vector<std::uint16_t>> tree_model_lists;		// per cell: the root's static model list
		std::vector<std::vector<std::uint32_t>> tree_model_starts;		// per cell, per node: where its run starts in that list
		std::vector<std::uint16_t> sorted;
		for (std::size_t c = 0; c < rw.cells.size(); c++)
		{
			tree_build tb{ surfaces };
			std::vector<int> list;
			for (std::size_t s = 0; s < surface_count; s++)
			{
				if (surfaces[s].cell == static_cast<int>(c) && surfaces[s].model == 0)
				{
					list.push_back(static_cast<int>(s));
				}
			}
			tb.build(list);
			// static models of the cell: each joins the leaf whose centre is nearest; a node's list is its children's
			// lists back to back ( the root's is stored, the others point into it ), and the boxes grow to hold them
			{
				std::vector<std::vector<std::uint16_t>> leaf_models(tb.nodes.size());
				bool any = false;
				for (std::size_t m = 0; m < rw.smodels.size(); m++)
				{
					if (rw.smodels[m].cell != static_cast<int>(c))
					{
						continue;
					}
					any = true;
					const box3& box = w.models[static_cast<std::size_t>(rw.smodels[m].model)].box;
					std::size_t best = 0;
					float best_d = 1e30f;
					for (std::size_t n = 0; n < tb.nodes.size(); n++)
					{
						if (tb.nodes[n].childCount)
						{
							continue;
						}
						float d = 0;
						for (int k = 0; k < 3; k++)
						{
							const float delta = (box.mins[k] + box.maxs[k]) * 0.5f - tb.nodes[n].bounds.midPoint[k];
							d += delta * delta;
						}
						if (d < best_d)
						{
							best_d = d;
							best = n;
						}
					}
					leaf_models[best].push_back(static_cast<std::uint16_t>(m));
				}
				std::vector<std::uint16_t> cell_list;
				std::vector<std::uint32_t> start(tb.nodes.size(), 0);
				const auto walk = [&](const auto& self, const std::size_t n) -> box3
				{
					box3 box;
					start[n] = static_cast<std::uint32_t>(cell_list.size());
					if (tb.nodes[n].childCount)
					{
						const std::size_t first = n + static_cast<std::size_t>(tb.nodes[n].childrenOffset) / sizeof(GfxAabbTree);
						for (std::size_t k = 0; k < tb.nodes[n].childCount; k++)
						{
							box.add(self(self, first + k));
						}
					}
					else
					{
						for (const std::uint16_t m : leaf_models[n])
						{
							cell_list.push_back(m);
							box.add(w.models[static_cast<std::size_t>(rw.smodels[m].model)].box);
						}
					}
					tb.nodes[n].smodelIndexCount = static_cast<std::uint16_t>(cell_list.size() - start[n]);
					if (box.valid())
					{
						box3 grown;
						const c1::Bounds& old = tb.nodes[n].bounds;
						if (list.empty() && n == 0)
						{
							grown = box;
						}
						else
						{
							for (int k = 0; k < 3; k++)
							{
								grown.mins[k] = std::min(old.midPoint[k] - old.halfSize[k], box.mins[k]);
								grown.maxs[k] = std::max(old.midPoint[k] + old.halfSize[k], box.maxs[k]);
							}
						}
						tb.nodes[n].bounds = grown.bounds();
					}
					return box;
				};
				if (any)
				{
					const box3 all = walk(walk, 0);
					rw.cells[c].box.add(all);
				}
				tree_model_lists.push_back(std::move(cell_list));
				tree_model_starts.push_back(std::move(start));
			}
			const std::size_t base = sorted.size();
			for (GfxAabbTree& node : tb.nodes)
			{
				node.startSurfIndex = static_cast<std::uint16_t>(node.startSurfIndex + base);
				node.surfaceCountNoDecal = node.surfaceCount;
			}
			if (list.empty() && tb.nodes[0].smodelIndexCount == 0)
			{
				tb.nodes[0].bounds = rw.cells[c].box.bounds();
			}
			sorted.insert(sorted.end(), tb.order.begin(), tb.order.end());
			trees[c] = std::move(tb.nodes);
		}
		const std::uint32_t static_count = static_cast<std::uint32_t>(sorted.size());
		for (auto& tree : trees)
		{
			for (GfxAabbTree& node : tree)
			{
				node.startSurfIndexNoDecal = static_cast<std::uint16_t>(node.startSurfIndex + static_count);
			}
		}
		// the list twice: with and without decals ( the same here )
		{
			const std::vector<std::uint16_t> once = sorted;
			sorted.insert(sorted.end(), once.begin(), once.end());
		}

		// ---- header
		c1::GfxWorld header{};
		header.baseName = FOLLOWS;
		header.nodeCount = static_cast<std::int32_t>(rw.cell_nodes.size());
		header.surfaceCount = static_cast<std::uint32_t>(surface_count);
		header.lastSunPrimaryLightIndex = 1;
		header.primaryLightCount = light_count;
		const std::uint32_t sort_keys[4] = { SORT_KEY_LIT_DECAL, SORT_KEY_EFFECT_DECAL, SORT_KEY_EFFECT_AUTO, SORT_KEY_DISTORTION };
		std::memcpy(reinterpret_cast<std::uint8_t*>(&header) + W_SORT_KEYS, sort_keys, sizeof(sort_keys));
		header.cellCount = cell_count;
		header.modelCount = w.model_count;
		header.bounds = rw.bounds.bounds();
		header.checksum = w.checksum;
		header.materialMemoryCount = static_cast<std::int32_t>(rw.materials.size());
		header.dpvs.smodelCount = static_cast<std::uint32_t>(rw.smodels.size());
		header.dpvs.staticSurfaceCount = static_count;
		header.dpvs.staticSurfaceCountNoDecal = static_count;
		header.dpvs.litOpaqueSurfsBegin = 0;
		header.dpvs.litOpaqueSurfsEnd = rw.lit_opaque_end;
		header.dpvs.litTransSurfsBegin = rw.lit_opaque_end;
		header.dpvs.litTransSurfsEnd = rw.lit_trans_end;
		header.dpvs.shadowCasterSurfsBegin = rw.lit_trans_end;
		header.dpvs.shadowCasterSurfsEnd = rw.lit_trans_end;
		header.dpvs.emissiveSurfsBegin = rw.lit_trans_end;
		header.dpvs.emissiveSurfsEnd = static_cast<std::uint32_t>(surface_count);
		header.dpvs.smodelVisDataCount = ((header.dpvs.smodelCount + 127) >> 7) << 2;
		header.dpvs.surfaceVisDataCount = ((static_count + 127) >> 7) << 2;

		asset_builder b;
		const std::uint32_t head = b.add(header);
		const auto hput = [&](const std::size_t offset, const std::uint32_t value) { b.put(head, offset, value); };
		b.point(head, offsetof(c1::GfxWorld, name), 0, 0, REF_COM);
		b.add_string(w.name);

		// ---- skies
		if (!rw.sky_surfaces.empty())
		{
			GfxSky sky{};
			sky.skySurfCount = static_cast<std::int32_t>(rw.sky_surfaces.size());
			sky.skyStartSurfs = FOLLOWS;
			sky.skySamplerState = 0xF3;			// as every shipped sky
			hput(offsetof(c1::GfxWorld, skyCount), 1);
			b.follows(head, offsetof(c1::GfxWorld, skies));
			const std::uint32_t chunk = b.add(sky);
			b.add_array(rw.sky_surfaces);
			if (rw.sky_image.valid())
			{
				point_ref(b, chunk, offsetof(GfxSky, skyImage), rw.sky_image);
			}
		}

		// ---- planes ( the clip map points at this array ), the cell BSP
		w.planes_final = true;
		header.planeCount = static_cast<std::int32_t>(w.planes.planes.size());
		hput(offsetof(c1::GfxWorld, planeCount), static_cast<std::uint32_t>(header.planeCount));
		b.follows(head, W_PLANES);
		w.gfx_planes_chunk = b.add_array(w.planes.planes);
		b.follows(head, W_NODES);
		b.add_array(rw.cell_nodes);
		hput(W_SCENE_ENT_CELL_BITS, FOLLOWS);		// runtime: nothing in the stream

		// ---- AABB trees
		{
			std::vector<std::uint32_t> counts;
			for (const auto& tree : trees)
			{
				counts.push_back(static_cast<std::uint32_t>(tree.size()));
			}
			b.follows(head, W_AABB_TREE_COUNTS);
			b.add_array(counts);
			b.follows(head, W_AABB_TREES);
			b.add_array(std::vector<std::uint32_t>(trees.size(), FOLLOWS));
			for (std::size_t c = 0; c < trees.size(); c++)
			{
				const std::uint32_t chunk = b.add_array(trees[c]);
				if (tree_model_lists[c].empty())
				{
					continue;
				}
				// the root's list follows the nodes; every other node with models points into it
				b.follows(chunk, offsetof(GfxAabbTree, smodelIndexes));
				const std::uint32_t list_chunk = b.add_array(tree_model_lists[c]);
				for (std::size_t n = 1; n < trees[c].size(); n++)
				{
					if (trees[c][n].smodelIndexCount)
					{
						b.point(chunk, n * sizeof(GfxAabbTree) + offsetof(GfxAabbTree, smodelIndexes), list_chunk, 2 * tree_model_starts[c][n]);
					}
				}
			}
		}

		// ---- cells
		{
			std::vector<c1::GfxCell> cells(rw.cells.size());
			for (std::size_t c = 0; c < cells.size(); c++)
			{
				cells[c].bounds = rw.cells[c].box.bounds();
				cells[c].portalCount = static_cast<std::int32_t>(rw.cells[c].portals.size());
				cells[c].portals = rw.cells[c].portals.empty() ? 0 : FOLLOWS;
				cells[c].reflectionProbeCount = static_cast<std::uint8_t>(rw.cells[c].probes.size());
				cells[c].reflectionProbes = rw.cells[c].probes.empty() ? 0 : FOLLOWS;
			}
			b.follows(head, offsetof(c1::GfxWorld, cells));
			b.add_array(cells);
			for (const render_cell& cell : rw.cells)
			{
				if (!cell.portals.empty())
				{
					std::vector<GfxPortal> portals(cell.portals.size());
					for (std::size_t p = 0; p < portals.size(); p++)
					{
						const render_portal& rp = cell.portals[p];
						std::memcpy(portals[p].plane, rp.plane, sizeof(rp.plane));
						portals[p].vertices = FOLLOWS;
						portals[p].cellIndex = static_cast<std::uint16_t>(rp.cell);
						portals[p].vertexCount = static_cast<std::uint8_t>(rp.vertices.size() / 3);
						std::memcpy(portals[p].hullAxis, rp.hull_axis, sizeof(rp.hull_axis));
					}
					b.add_array(portals);
					for (const render_portal& rp : cell.portals)
					{
						b.add_array(rp.vertices);
					}
				}
				if (!cell.probes.empty())
				{
					b.add_array(cell.probes);
				}
			}
		}

		// ---- draw: reflection probes, lightmaps, vertices, indices
		const std::size_t draw = offsetof(c1::GfxWorld, draw);
		{
			const std::uint32_t probe_count = static_cast<std::uint32_t>(rw.probes.size());
			hput(draw + offsetof(c1::GfxWorldDraw, reflectionProbeCount), probe_count);
			b.follows(head, draw + offsetof(c1::GfxWorldDraw, reflectionProbes));
			const std::uint32_t pointers = b.add_array(std::vector<std::uint32_t>(probe_count, 0));
			std::vector<float> origins;
			for (std::uint32_t p = 0; p < probe_count; p++)
			{
				// 64 x 64 cube map with all mip levels; flags and level count as the shipped probes have them
				add_inline_image(b, pointers, 4 * p, format("*reflection_probe%u", p), 5, 9, 64, 64, FMT_A8R8G8B8, 0x08010301, 0, rw.probes[p].pixels);
				origins.insert(origins.end(), rw.probes[p].origin, rw.probes[p].origin + 3);
			}
			b.follows(head, draw + offsetof(c1::GfxWorldDraw, reflectionProbeOrigins));
			b.add_array(origins);
			hput(draw + D_PROBE_TEXTURES, FOLLOWS);		// runtime

			const std::uint32_t lightmap_count = static_cast<std::uint32_t>(rw.lightmaps.size());
			hput(draw + offsetof(c1::GfxWorldDraw, lightmapCount), lightmap_count);
			if (lightmap_count)
			{
				b.follows(head, draw + offsetof(c1::GfxWorldDraw, lightmaps));
				const std::uint32_t array = b.add_array(std::vector<GfxLightmapArray>(lightmap_count));
				for (std::uint32_t l = 0; l < lightmap_count; l++)
				{
					add_inline_image(b, array, 8 * l + offsetof(GfxLightmapArray, primary), format("*lightmap%u_primary", l), 3, 10, LM_W * 2, LM_H * 2, FMT_L8,
						0x08000003, 1, rw.lightmaps[l].primary);
					add_inline_image(b, array, 8 * l + offsetof(GfxLightmapArray, secondary), format("*lightmap%u_secondary", l), 3, 10, LM_W, LM_H * 2,
						FMT_A8R8G8B8, 0x08000003, 1, rw.lightmaps[l].secondary);
				}
				hput(draw + D_LIGHTMAP_PRIMARY_TEX, FOLLOWS);
				hput(draw + D_LIGHTMAP_SECONDARY_TEX, FOLLOWS);
			}

			hput(draw + offsetof(c1::GfxWorldDraw, vertexCount), static_cast<std::uint32_t>(vertices.size()));
			if (!vertices.empty())
			{
				b.follows(head, draw + offsetof(c1::GfxWorldDraw, vertices));
				b.add_array(vertices);
			}
			// vertex layer data: four zero bytes in every shipped map
			hput(draw + offsetof(c1::GfxWorldDraw, vertexLayerDataSize), 4);
			b.follows(head, draw + offsetof(c1::GfxWorldDraw, vertexLayerData));
			b.add_zero(4);
			hput(draw + offsetof(c1::GfxWorldDraw, indexCount), static_cast<std::uint32_t>(indices.size()));
			if (!indices.empty())
			{
				b.follows(head, draw + offsetof(c1::GfxWorldDraw, indices));
				b.add_array(indices);
			}
		}

		// ---- light grid
		{
			GfxLightGrid grid = rw.grid.header;
			grid.rowDataStart = FOLLOWS;
			grid.rawRowDataSize = static_cast<std::uint32_t>(rw.grid.raw_rows.size());
			grid.rawRowData = rw.grid.raw_rows.empty() ? 0 : FOLLOWS;
			grid.entryCount = static_cast<std::uint32_t>(rw.grid.entries.size());
			grid.entries = rw.grid.entries.empty() ? 0 : FOLLOWS;
			grid.colorCount = static_cast<std::uint32_t>(rw.grid.colors.size());
			grid.colors = rw.grid.colors.empty() ? 0 : FOLLOWS;
			std::memcpy(b.chunks[head].bytes.data() + offsetof(c1::GfxWorld, lightGrid), &grid, sizeof(grid));
			b.add_array(rw.grid.row_starts);
			if (!rw.grid.raw_rows.empty())
			{
				b.add_array(rw.grid.raw_rows);
			}
			if (!rw.grid.entries.empty())
			{
				b.add_array(rw.grid.entries);
			}
			if (!rw.grid.colors.empty())
			{
				b.add_array(rw.grid.colors);
			}
		}

		// ---- brush models
		{
			std::vector<c1::GfxBrushModel> models(static_cast<std::size_t>(w.model_count));
			for (int m = 0; m < w.model_count; m++)
			{
				c1::GfxBrushModel& model = models[static_cast<std::size_t>(m)];
				const render_model& rm = rw.models[static_cast<std::size_t>(m)];
				model.bounds = rm.box.bounds();
				model.radius = rm.box.valid() ? rm.box.radius() : 0;
				model.surfaceCount = static_cast<std::uint16_t>(rm.surface_count);
				model.startSurfIndex = rm.surface_count ? static_cast<std::uint16_t>(rm.first_surface) : (m == 0 ? 0 : 0xFFFF);
				model.surfaceCountNoDecal = model.surfaceCount;
			}
			b.follows(head, offsetof(c1::GfxWorld, models));
			b.add_array(models);
		}

		// ---- material memory
		if (!rw.materials.empty())
		{
			b.follows(head, offsetof(c1::GfxWorld, materialMemory));
			std::vector<MaterialMemory> memory(rw.materials.size());
			const std::uint32_t chunk = b.add_array(memory);
			for (std::size_t m = 0; m < rw.materials.size(); m++)
			{
				point_ref(b, chunk, m * sizeof(MaterialMemory), rw.materials[m].ref);
			}
		}

		// no sun flare: hasValidData 0, both materials null ( sun: 96 bytes of zero )

		// ---- outdoor image ( height of what is open to the sky, for weather effects ) and its lookup matrix
		{
			for (int k = 0; k < 3; k++)
			{
				const float size = std::max(rw.bounds.maxs[k] - rw.bounds.mins[k], 1.0f);
				header.outdoorLookupMatrix[k][k] = 1.0f / size;
				header.outdoorLookupMatrix[3][k] = -rw.bounds.mins[k] / size;
			}
			header.outdoorLookupMatrix[3][3] = 1.0f;
			std::memcpy(b.chunks[head].bytes.data() + offsetof(c1::GfxWorld, outdoorLookupMatrix), header.outdoorLookupMatrix, sizeof(header.outdoorLookupMatrix));
			add_inline_image(b, head, offsetof(c1::GfxWorld, outdoorImage), "$outdoor", 3, 1, 512, 512, FMT_L8, 3, 1, std::vector<std::uint8_t>(512 * 512, 0));
		}

		// ---- runtime arrays: a pointer that says "allocate", nothing in the stream
		hput(W_CELL_CASTER_BITS, FOLLOWS);
		hput(W_CELL_SUN_LIT_BITS, FOLLOWS);
		if (light_count > header.lastSunPrimaryLightIndex + 1)
		{
			hput(W_LIGHT_ENT_SHADOW_VIS, FOLLOWS);
		}

		// ---- per primary light: what casts shadows in it, and its region
		{
			// a light's static models are those whose primary light it is ( as in the shipped maps, where the suns
			// list models and no surfaces )
			std::vector<std::vector<std::uint16_t>> light_models(light_count);
			for (std::size_t m = 0; m < rw.smodels.size(); m++)
			{
				light_models[static_cast<std::size_t>(std::min<int>(rw.smodels[m].primary_light, static_cast<int>(light_count) - 1))].push_back(static_cast<std::uint16_t>(m));
			}
			std::vector<GfxShadowGeometry> geometry(light_count);
			for (std::uint32_t l = 0; l < light_count; l++)
			{
				const std::size_t surfs = l < rw.light_surfaces.size() ? rw.light_surfaces[l].size() : 0;
				geometry[l].surfaceCount = static_cast<std::uint16_t>(surfs);
				geometry[l].sortedSurfIndex = surfs ? FOLLOWS : 0;
				geometry[l].smodelCount = static_cast<std::uint16_t>(light_models[l].size());
				geometry[l].smodelIndex = light_models[l].empty() ? 0 : FOLLOWS;
			}
			b.follows(head, W_SHADOW_GEOM);
			b.add_array(geometry);
			for (std::uint32_t l = 0; l < light_count; l++)
			{
				if (geometry[l].surfaceCount)
				{
					b.add_array(rw.light_surfaces[l]);
				}
				if (!light_models[l].empty())
				{
					b.add_array(light_models[l]);
				}
			}
			b.follows(head, W_LIGHT_REGION);
			b.add_array(std::vector<GfxLightRegion>(light_count));
		}

		// ---- dpvs
		const std::size_t dpvs = offsetof(c1::GfxWorld, dpvs);
		for (int k = 0; k < 3; k++)
		{
			hput(dpvs + offsetof(c1::GfxWorldDpvsStatic, smodelVisData) + 4 * static_cast<std::size_t>(k), FOLLOWS);
			hput(dpvs + offsetof(c1::GfxWorldDpvsStatic, surfaceVisData) + 4 * static_cast<std::size_t>(k), FOLLOWS);
		}
		b.follows(head, dpvs + offsetof(c1::GfxWorldDpvsStatic, sortedSurfIndex));
		b.add_array(sorted);
		// static models: bounds + lighting origin, and further down the draw instances
		std::vector<c1::GfxStaticModelInst> model_insts(rw.smodels.size());
		std::vector<c1::GfxStaticModelDrawInst> model_draws(rw.smodels.size());
		for (std::size_t m = 0; m < rw.smodels.size(); m++)
		{
			const map_model& model = w.models[static_cast<std::size_t>(rw.smodels[m].model)];
			model_insts[m].bounds = model.box.bounds();
			std::memcpy(model_insts[m].lightingOrigin, model_insts[m].bounds.midPoint, sizeof(model_insts[m].lightingOrigin));
			c1::GfxStaticModelDrawInst& d = model_draws[m];
			std::memcpy(d.origin, model.origin, sizeof(d.origin));
			std::memcpy(d.axis, model.axis, sizeof(d.axis));
			d.scale = model.scale;
			// drawn up to 40 times its size away, at least 2000 units ( the shipped values vary per model )
			d.cullDist = static_cast<std::uint16_t>(std::clamp(model.box.radius() * 40.0f, 2000.0f, 60000.0f));
			d.reflectionProbeIndex = static_cast<std::uint8_t>(rw.smodels[m].reflection_probe);
			d.primaryLightIndex = static_cast<std::uint8_t>(rw.smodels[m].primary_light);
		}
		if (!rw.smodels.empty())
		{
			b.follows(head, dpvs + offsetof(c1::GfxWorldDpvsStatic, smodelInsts));
			b.add_array(model_insts);
		}
		if (surface_count)
		{
			b.follows(head, dpvs + offsetof(c1::GfxWorldDpvsStatic, surfaces));
			const std::uint32_t chunk = b.add_array(packed);
			for (std::size_t s = 0; s < surface_count; s++)
			{
				point_ref(b, chunk, s * sizeof(c1::GfxSurface) + offsetof(c1::GfxSurface, material), rw.materials[static_cast<std::size_t>(surfaces[s].material)].ref);
			}
			b.follows(head, dpvs + offsetof(c1::GfxWorldDpvsStatic, surfacesBounds));
			b.add_array(surface_bounds);
		}
		if (!rw.smodels.empty())
		{
			b.follows(head, dpvs + offsetof(c1::GfxWorldDpvsStatic, smodelDrawInsts));
			const std::uint32_t chunk = b.add_array(model_draws);
			for (std::size_t m = 0; m < rw.smodels.size(); m++)
			{
				point_ref(b, chunk, m * sizeof(c1::GfxStaticModelDrawInst) + offsetof(c1::GfxStaticModelDrawInst, model), w.models[static_cast<std::size_t>(rw.smodels[m].model)].ref);
			}
		}
		hput(dpvs + offsetof(c1::GfxWorldDpvsStatic, surfaceMaterials), FOLLOWS);
		hput(dpvs + offsetof(c1::GfxWorldDpvsStatic, surfaceCastsSunShadow), FOLLOWS);

		// fog types: 1 in the shipped maps
		b.chunks[head].bytes[W_FOG_TYPES] = 1;

		std::size_t index = npos;
		resolve_refs(b, w);
		check(ctx.editor.add_built_asset(c1::ASSET_TYPE_GFXWORLD, w.bsp_name, b.chunks, npos, &index), "adding the GfxWorld");
		w.gfx_asset = index;

		ctx.rep.surfaces = surface_count;
		ctx.rep.vertices = vertices.size();
		ctx.rep.triangles = indices.size() / 3;
		ctx.rep.world_materials = rw.materials.size();
		ctx.rep.cells = rw.cells.size();
		ctx.rep.lightmaps = rw.lightmaps.size();
		ctx.rep.light_grid_points = rw.grid.entries.size();
		ctx.rep.primary_lights = light_count;
		ctx.rep.reflection_probes = rw.probes.size();
	}
}
