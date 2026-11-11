// Stage 3: collision brushes edited, added, deleted - in the world and in brush models.
//
// What the clip map holds for a brush ( layouts: c1_layouts.hpp; shape of the data: codol_mapc_probe clip ):
//
//   brushes[ i ]         cbrush_t: the six axial sides are implied by brushBounds[ i ] ( material per side in
//                        axialMaterialNum ), `sides` is a run of brushsides with the other planes, and
//                        baseAdjacentSide a run of brushEdges: for every side that has a face the sides it shares
//                        an edge with, clockwise seen from outside ( axial sides -x +x -y +y -z +z are 0 .. 5,
//                        the stored sides follow )
//   brushsides           plane pointer ( into the plane array the GfxWorld and the clip map share ), material
//   brushBounds, brushContents
//   leafbrushes          u16 brush indices; the leaves of the leaf brush node trees name runs of it ( a few lists
//                        are stored behind the node array instead )
//   leafbrushNodes       one tree per collision leaf of the world BSP ( cLeaf_t.leafBrushNode ) and per brush
//                        model ( cmodel_t.leaf ). A node with leafBrushCount > 0 lists brushes; an inner node
//                        splits along `axis` at `dist`: child 0 ( node + childOffset[0] ) holds what lies at
//                        dist + range and above, child 1 what lies at dist - range and below; when
//                        leafBrushCount is -1 the node that follows holds the brushes that straddle the split.
//
// A new brush goes into the tree of every collision leaf its box touches ( found by walking the clip map's BSP ),
// or of its brush model. The existing trees are not rebuilt: a tree whose root is a list gets the brush appended;
// in front of any other root a node with leafBrushCount -1 is put, followed by a list node with the new
// brushes, both children leading to the old root ( range is set so that every trace takes child 0 ). A removed
// brush leaves the lists that name it; the brush array is compacted and every list renumbered.

#include "mapc_impl.hpp"

#include <functional>
#include <set>

namespace codol::mapc
{
	namespace
	{
		constexpr std::uint32_t FOLLOWS = 0xFFFFFFFFu;
		constexpr std::uint32_t NODE_SIZE = sizeof(c1::cLeafBrushNode_s);

		struct new_side
		{
			float normal[3];
			float dist;
			int material = 0;
			std::vector<std::uint8_t> adjacent;
		};

		struct new_brush
		{
			int index = -1;				// cbrush_t index in the new array
			int sub_model = 0;
			box bounds;
			int contents = 0;
			std::int16_t axial_material[2][3] = {};
			std::vector<std::uint8_t> axial_adjacent[2][3];
			std::vector<new_side> sides;	// the non axial ones
		};

		// -x +x -y +y -z +z of an axis aligned box, as every such brush of the shipped maps has them
		constexpr std::uint8_t BOX_ADJACENT[2][3][4] =
		{
			{ { 2, 4, 3, 5 }, { 5, 1, 4, 0 }, { 0, 2, 1, 3 } },
			{ { 5, 3, 4, 2 }, { 0, 4, 1, 5 }, { 3, 1, 2, 0 } },
		};
	}

	void stage_brushes(context& ctx)
	{
		const Zone& zone = ctx.zone();
		ZoneEditor& editor = ctx.editor;
		const map_diff& diff = ctx.rep.diff;
		const c1::clipMap_t& raw = ctx.clip.raw;
		const std::uint32_t old_count = raw.numBrushes;
		if (ctx.clip.brushes.size() != old_count)
		{
			throw compile_error("brushes: the clip map's brush array could not be read");
		}

		// ---- the arrays
		const ZoneDataId brushes_id = ctx.data(raw.brushes, "clipMap.brushes");
		const ZoneDataId bounds_id = ctx.data(raw.brushBounds, "clipMap.brushBounds");
		const ZoneDataId contents_id = ctx.data(raw.brushContents, "clipMap.brushContents");
		const ZoneDataId sides_id = ctx.data(raw.brushsides, "clipMap.brushsides");
		const ZoneDataId edges_id = ctx.data(raw.brushEdges, "clipMap.brushEdges");
		const ZoneDataId planes_id = ctx.data(raw.planes, "clipMap.planes");
		const ZoneDataId nodes_id = ctx.data(raw.leafbrushNodes, "clipMap.leafbrushNodes");
		const ZoneDataId lists_id = ctx.data(raw.leafbrushes, "clipMap.leafbrushes");
		const ZoneDataId leafs_id = ctx.data(raw.leafs, "clipMap.leafs");
		const ZoneDataId models_id = ctx.data(raw.cmodels, "clipMap.cmodels");
		auto brushes = array_of<c1::cbrush_t>(editor.data_bytes(brushes_id));
		auto bounds = array_of<c1::Bounds>(editor.data_bytes(bounds_id));
		auto contents = array_of<std::int32_t>(editor.data_bytes(contents_id));
		std::vector<std::uint8_t> sides(editor.data_bytes(sides_id).begin(), editor.data_bytes(sides_id).end());
		std::vector<std::uint8_t> edges(editor.data_bytes(edges_id).begin(), editor.data_bytes(edges_id).end());
		auto planes = array_of<c1::cplane_s>(editor.data_bytes(planes_id));
		auto nodes = array_of<c1::cLeafBrushNode_s>(editor.data_bytes(nodes_id));
		auto lists = array_of<std::uint16_t>(editor.data_bytes(lists_id));
		auto leafs = array_of<c1::cLeaf_t>(editor.data_bytes(leafs_id));
		auto models = array_of<c1::cmodel_t>(editor.data_bytes(models_id));
		const auto node_image = zone.get_array<c1::cLeafBrushNode_s>(raw.leafbrushNodes, raw.leafbrushNodesCount);
		if (brushes.size() != old_count || bounds.size() != old_count || contents.size() != old_count || nodes.size() != node_image.size()
			|| planes.size() != static_cast<std::size_t>(raw.planeCount) || static_cast<std::size_t>(ctx.gfx.raw.planeCount) != planes.size())
		{
			throw compile_error("brushes: the clip map's arrays do not have the size its header says ( or the plane array is not the one of the GfxWorld )");
		}

		// ---- clip materials by name
		std::map<std::string, int, std::less<>> material_index;
		int solid_material = -1;
		{
			std::map<int, int> uses;
			for (const zone::Brush& b : ctx.clip.brushes)
			{
				for (const zone::BrushSide& s : b.sides)
				{
					uses[s.materialNum]++;
				}
			}
			int best = 0;
			for (std::size_t m = 0; m < ctx.clip.materials.size(); m++)
			{
				material_index.emplace(std::string(ctx.clip.materials[m].name), static_cast<int>(m));
				if ((ctx.clip.materials[m].contents & 1) != 0 && uses[static_cast<int>(m)] > best)
				{
					best = uses[static_cast<int>(m)];
					solid_material = static_cast<int>(m);
				}
			}
		}
		// contents of a brush: the contents of its sides' materials together. Checked on the base zone.
		{
			int same = 0, checked = 0;
			for (std::size_t b = 0; b < ctx.clip.brushes.size(); b += std::max<std::size_t>(1, ctx.clip.brushes.size() / 3000))
			{
				int all = 0;
				for (const zone::BrushSide& s : ctx.clip.brushes[b].sides)
				{
					all |= s.materialNum < ctx.clip.materials.size() ? ctx.clip.materials[s.materialNum].contents : 0;
				}
				same += all == ctx.clip.brushes[b].contents;
				checked++;
			}
			ctx.note(format("brushes: the contents of %d of %d sampled brushes of the base zone are the contents of their sides' materials together", same, checked));
		}

		// How far the box of a brush model's leaf, and of the model, reach beyond the boxes of its brushes: measured on
		// the brush models of the base zone that hold brushes only, used for every box this stage writes.
		float leaf_pad = 0.125f, model_pad = 1.125f;
		{
			std::vector<float> leaf_pads, model_pads;
			const auto order = imported_brush_order(zone, ctx.clip);
			for (const auto& [m, list] : order)
			{
				if (m <= 0 || static_cast<std::size_t>(m) >= models.size() || models[static_cast<std::size_t>(m)].leaf.collAabbCount != 0 || list.empty())
				{
					continue;
				}
				box all;
				for (const int b : list)
				{
					all.add(box(bounds[static_cast<std::size_t>(b)]));
				}
				const box leaf(models[static_cast<std::size_t>(m)].leaf.bounds), model(models[static_cast<std::size_t>(m)].bounds);
				leaf_pads.push_back(leaf.maxs[2] - all.maxs[2]);
				model_pads.push_back(model.maxs[2] - all.maxs[2]);
			}
			if (!leaf_pads.empty())
			{
				std::nth_element(leaf_pads.begin(), leaf_pads.begin() + static_cast<std::ptrdiff_t>(leaf_pads.size() / 2), leaf_pads.end());
				std::nth_element(model_pads.begin(), model_pads.begin() + static_cast<std::ptrdiff_t>(model_pads.size() / 2), model_pads.end());
				leaf_pad = std::clamp(leaf_pads[leaf_pads.size() / 2], 0.0f, 2.0f);
				model_pad = std::clamp(model_pads[model_pads.size() / 2], leaf_pad, 4.0f);
			}
			ctx.note(format("brushes: in the base zone the box of a brush model's leaf reaches %.3f units beyond its brushes, the model's box %.3f ( median of %zu brush models ); "
				"new boxes are made the same way", leaf_pad, model_pad, leaf_pads.size()));
		}
		const auto padded = [](box b, const float pad)
		{
			for (int k = 0; k < 3; k++)
			{
				b.mins[k] -= pad;
				b.maxs[k] += pad;
			}
			return b;
		};

		// ---- the edits
		std::vector<std::uint8_t> removed(old_count, 0);		// deleted, or replaced ( the old shape leaves the lists )
		std::vector<std::uint8_t> deleted(old_count, 0);
		for (const int b : diff.brushes_deleted)
		{
			removed[static_cast<std::size_t>(b)] = deleted[static_cast<std::size_t>(b)] = 1;
		}
		for (const brush_edit& e : diff.brushes)
		{
			if (e.replaces >= 0)
			{
				removed[static_cast<std::size_t>(e.replaces)] = 1;
			}
		}
		std::vector<int> remap(old_count, -1);
		std::uint32_t kept = 0;
		for (std::uint32_t b = 0; b < old_count; b++)
		{
			if (!deleted[b])
			{
				remap[b] = static_cast<int>(kept++);
			}
		}

		// ---- the new brushes, from the geometry of the edited document
		std::vector<new_brush> fresh;
		std::set<std::string> unknown_materials;
		std::uint32_t next_index = kept;
		for (const brush_edit& e : diff.brushes)
		{
			const edit::brush* source = ctx.edited.find_brush(e.brush);
			const edit::brush_geometry* geo = ctx.edited.geometry(e.brush);
			if (!source || !geo || !geo->valid)
			{
				ctx.warn(format("brushes: a brush of %s is not a closed convex volume and is left out%s", e.sub_model ? format("brush model *%d", e.sub_model).c_str() : "the world",
					e.replaces >= 0 ? " ( the brush it replaces is removed )" : ""));
				if (e.replaces >= 0)
				{
					// nothing takes its place: it is a deleted brush
					deleted[static_cast<std::size_t>(e.replaces)] = 1;
				}
				continue;
			}
			new_brush b;
			b.sub_model = e.sub_model;
			for (const edit::vec3& v : geo->vertices)
			{
				const float p[3] = { static_cast<float>(v.x), static_cast<float>(v.y), static_cast<float>(v.z) };
				b.bounds.add(p);
			}
			// side number of every face: 0 .. 5 for a face on the box of the brush, 6 .. for the others
			std::vector<int> side_of(geo->planes.size(), -1);
			int first_material = -1;
			for (std::size_t f = 0; f < geo->planes.size() && f < source->faces.size(); f++)
			{
				if (geo->face_vertices[f].size() < 3)
				{
					continue;
				}
				const std::string& name = source->faces[f].texture.material;
				int material = solid_material;
				if (const auto it = material_index.find(name); it != material_index.end())
				{
					material = it->second;
				}
				else
				{
					unknown_materials.insert(name);
				}
				if (material < 0)
				{
					throw compile_error("brushes: the clip map has no solid material to stand in for '" + name + "'");
				}
				first_material = first_material < 0 ? material : first_material;
				b.contents |= ctx.clip.materials[static_cast<std::size_t>(material)].contents;
				const edit::plane& p = geo->planes[f];
				int axis = -1;
				for (int k = 0; k < 3; k++)
				{
					if (std::fabs(std::fabs(p.normal[k]) - 1.0) < 1e-6)
					{
						axis = k;
					}
				}
				if (axis >= 0)
				{
					const int positive = p.normal[axis] > 0 ? 1 : 0;
					side_of[f] = 2 * axis + positive;
					b.axial_material[positive][axis] = static_cast<std::int16_t>(material);
					continue;
				}
				side_of[f] = 6 + static_cast<int>(b.sides.size());
				new_side s;
				for (int k = 0; k < 3; k++)
				{
					s.normal[k] = static_cast<float>(p.normal[k]);
				}
				s.dist = static_cast<float>(p.dist);
				s.material = material;
				b.sides.push_back(std::move(s));
			}
			// an axial side without a face of its own ( the box only touches the brush there ) takes the first material
			for (int positive = 0; positive < 2; positive++)
			{
				for (int axis = 0; axis < 3; axis++)
				{
					if (std::find(side_of.begin(), side_of.end(), 2 * axis + positive) == side_of.end())
					{
						b.axial_material[positive][axis] = static_cast<std::int16_t>(first_material);
					}
				}
			}
			// edge lists: for every face the sides across its edges, clockwise from outside ( the windings are counter clockwise )
			bool box_like = b.sides.empty();
			std::size_t edge_total = 0;
			for (std::size_t f = 0; f < side_of.size(); f++)
			{
				if (side_of[f] < 0)
				{
					continue;
				}
				const std::vector<int>& loop = geo->face_vertices[f];
				std::vector<std::uint8_t> adjacent;
				for (std::size_t k = loop.size(); k-- > 0;)
				{
					// edge from corner k + 1 back to corner k
					const int v0 = loop[(k + 1) % loop.size()], v1 = loop[k];
					int other = -1;
					for (std::size_t g = 0; g < side_of.size() && other < 0; g++)
					{
						if (g == f || side_of[g] < 0)
						{
							continue;
						}
						const std::vector<int>& theirs = geo->face_vertices[g];
						if (std::find(theirs.begin(), theirs.end(), v0) != theirs.end() && std::find(theirs.begin(), theirs.end(), v1) != theirs.end())
						{
							other = side_of[g];
						}
					}
					if (other < 0)
					{
						throw compile_error("brushes: a face of a new brush has an edge no other face shares");
					}
					adjacent.push_back(static_cast<std::uint8_t>(other));
				}
				// the list is a cycle; it starts with its lowest side
				std::rotate(adjacent.begin(), std::min_element(adjacent.begin(), adjacent.end()), adjacent.end());
				edge_total += adjacent.size();
				box_like = box_like && adjacent.size() == 4;
				if (side_of[f] < 6)
				{
					b.axial_adjacent[side_of[f] & 1][side_of[f] >> 1] = std::move(adjacent);
				}
				else
				{
					b.sides[static_cast<std::size_t>(side_of[f] - 6)].adjacent = std::move(adjacent);
				}
			}
			if (box_like)
			{
				// exactly the lists the shipped boxes have ( the same cycles, their start as in the base zones )
				for (int positive = 0; positive < 2; positive++)
				{
					for (int axis = 0; axis < 3; axis++)
					{
						b.axial_adjacent[positive][axis].assign(BOX_ADJACENT[positive][axis], BOX_ADJACENT[positive][axis] + 4);
					}
				}
			}
			if (edge_total > 255 || b.sides.size() > 250)
			{
				ctx.warn("brushes: a brush has more edges than a cbrush_t can describe ( 255 ) and is left out");
				if (e.replaces >= 0)
				{
					deleted[static_cast<std::size_t>(e.replaces)] = 1;
				}
				continue;
			}
			b.index = e.replaces >= 0 ? -2 - e.replaces : -1;		// resolved below, once the deletions are final
			fresh.push_back(std::move(b));
		}
		for (const std::string& name : unknown_materials)
		{
			ctx.warn("brushes: material '" + name + "' is not among the clip map's materials; its sides get '"
				+ std::string(ctx.clip.materials[static_cast<std::size_t>(solid_material)].name) + "' ( surface type and contents of that material )");
		}
		// deletions may have grown ( brushes that could not be built ): number again
		kept = 0;
		for (std::uint32_t b = 0; b < old_count; b++)
		{
			remap[b] = deleted[b] ? -1 : static_cast<int>(kept++);
		}
		next_index = kept;
		for (new_brush& b : fresh)
		{
			b.index = b.index <= -2 ? remap[static_cast<std::size_t>(-2 - b.index)] : static_cast<int>(next_index++);
		}
		if (next_index > 0xFFFF)
		{
			throw compile_error("brushes: more than 65535 brushes");
		}

		// ---- brush arrays: the kept brushes in order ( a replaced one in its place ), then the new ones
		std::map<std::uint32_t, ZoneTarget> brush_pointers;
		for (const ZonePointer& p : editor.data_pointers(brushes_id))
		{
			brush_pointers.emplace(p.offset, p.target);
		}
		std::vector<c1::cbrush_t> new_brushes(next_index);
		std::vector<c1::Bounds> new_bounds(next_index);
		std::vector<std::int32_t> new_contents(next_index);
		std::vector<ZonePointer> new_brush_pointers;
		std::vector<std::uint8_t> is_fresh(next_index, 0);
		for (const new_brush& b : fresh)
		{
			is_fresh[static_cast<std::size_t>(b.index)] = 1;
		}
		const std::uint32_t sides_offset = offsetof(c1::cbrush_t, sides), edges_offset = offsetof(c1::cbrush_t, baseAdjacentSide);
		for (std::uint32_t b = 0; b < old_count; b++)
		{
			const int n = remap[b];
			if (n < 0 || is_fresh[static_cast<std::size_t>(n)])
			{
				continue;
			}
			new_brushes[static_cast<std::size_t>(n)] = brushes[b];
			new_bounds[static_cast<std::size_t>(n)] = bounds[b];
			new_contents[static_cast<std::size_t>(n)] = contents[b];
			for (const std::uint32_t field : { sides_offset, edges_offset })
			{
				if (const auto it = brush_pointers.find(36 * b + field); it != brush_pointers.end())
				{
					new_brush_pointers.push_back({ 36u * static_cast<std::uint32_t>(n) + field, it->second });
				}
			}
		}
		std::sort(fresh.begin(), fresh.end(), [](const new_brush& a, const new_brush& b) { return a.index < b.index; });
		std::vector<ZonePointer> side_pointers = editor.data_pointers(sides_id);
		const std::size_t old_planes = planes.size(), old_sides = sides.size() / 8;
		for (const new_brush& b : fresh)
		{
			c1::cbrush_t packed{};
			packed.numsides = static_cast<std::uint16_t>(b.sides.size());
			std::vector<std::uint8_t> run;
			for (int axis = 0; axis < 3; axis++)
			{
				for (int positive = 0; positive < 2; positive++)
				{
					packed.axialMaterialNum[positive][axis] = b.axial_material[positive][axis];
					packed.firstAdjacentSideOffsets[positive][axis] = static_cast<std::uint8_t>(run.size());
					packed.edgeCount[positive][axis] = static_cast<std::uint8_t>(b.axial_adjacent[positive][axis].size());
					run.insert(run.end(), b.axial_adjacent[positive][axis].begin(), b.axial_adjacent[positive][axis].end());
				}
			}
			const std::uint32_t at = 36u * static_cast<std::uint32_t>(b.index);
			if (!b.sides.empty())
			{
				new_brush_pointers.push_back({ at + sides_offset, ZoneTarget::to_data(sides_id, static_cast<std::uint32_t>(sides.size())) });
			}
			for (const new_side& s : b.sides)
			{
				// the plane: one the map has, else a new one at the end of the array
				std::size_t plane = planes.size();
				for (std::size_t p = 0; p < planes.size(); p++)
				{
					if (std::fabs(planes[p].dist - s.dist) < 1e-3f && std::fabs(planes[p].normal[0] - s.normal[0]) < 1e-5f && std::fabs(planes[p].normal[1] - s.normal[1]) < 1e-5f
						&& std::fabs(planes[p].normal[2] - s.normal[2]) < 1e-5f)
					{
						plane = p;
						break;
					}
				}
				if (plane == planes.size())
				{
					c1::cplane_s p{};
					std::memcpy(p.normal, s.normal, 12);
					p.dist = s.dist;
					p.type = 3;		// not axial; the base zones leave signbits 0
					planes.push_back(p);
				}
				c1::cbrushside_t side{};
				side.materialNum = static_cast<std::uint16_t>(s.material);
				side.firstAdjacentSideOffset = static_cast<std::uint8_t>(run.size());
				side.edgeCount = static_cast<std::uint8_t>(s.adjacent.size());
				run.insert(run.end(), s.adjacent.begin(), s.adjacent.end());
				side_pointers.push_back({ static_cast<std::uint32_t>(sides.size()), ZoneTarget::to_data(planes_id, static_cast<std::uint32_t>(20 * plane)) });
				append(sides, side);
			}
			if (!run.empty())
			{
				new_brush_pointers.push_back({ at + edges_offset, ZoneTarget::to_data(edges_id, static_cast<std::uint32_t>(edges.size())) });
				edges.insert(edges.end(), run.begin(), run.end());
			}
			new_brushes[static_cast<std::size_t>(b.index)] = packed;
			new_bounds[static_cast<std::size_t>(b.index)] = b.bounds.bounds();
			new_contents[static_cast<std::size_t>(b.index)] = b.contents;
		}
		std::sort(new_brush_pointers.begin(), new_brush_pointers.end(), [](const ZonePointer& a, const ZonePointer& b) { return a.offset < b.offset; });

		// ---- which trees a new brush goes into
		const auto bsp = zone.get_array<cnode>(raw.nodes, raw.numNodes);
		std::vector<c1::cplane_s> bsp_planes(bsp.size());
		for (std::size_t n = 0; n < bsp.size(); n++)
		{
			bsp_planes[n] = zone.get<c1::cplane_s>(bsp[n].plane);
		}
		const auto leafs_touched = [&](const box& b)
		{
			std::vector<int> found, stack{ 0 };
			while (!stack.empty() && !bsp.empty())
			{
				const int n = stack.back();
				stack.pop_back();
				if (n < 0)
				{
					found.push_back(-1 - n);
					continue;
				}
				if (n >= static_cast<int>(bsp.size()))
				{
					continue;
				}
				const c1::cplane_s& p = bsp_planes[static_cast<std::size_t>(n)];
				float mid = -p.dist, reach = 0.125f;
				for (int k = 0; k < 3; k++)
				{
					mid += p.normal[k] * 0.5f * (b.mins[k] + b.maxs[k]);
					reach += std::fabs(p.normal[k]) * 0.5f * (b.maxs[k] - b.mins[k]);
				}
				if (mid + reach >= 0)
				{
					stack.push_back(bsp[static_cast<std::size_t>(n)].children[0]);
				}
				if (mid - reach < 0)
				{
					stack.push_back(bsp[static_cast<std::size_t>(n)].children[1]);
				}
			}
			std::sort(found.begin(), found.end());
			found.erase(std::unique(found.begin(), found.end()), found.end());
			return found;
		};
		std::map<int, std::vector<int>> leaf_adds, model_adds;		// collision leaf / sub model -> new brush indices
		for (const new_brush& b : fresh)
		{
			if (b.sub_model > 0)
			{
				model_adds[b.sub_model].push_back(b.index);
				continue;
			}
			for (const int leaf : leafs_touched(b.bounds))
			{
				if (leaf < static_cast<int>(leafs.size()))
				{
					leaf_adds[leaf].push_back(b.index);
				}
			}
		}

		// ---- the lists of the existing trees: removed brushes leave, the rest is renumbered
		const zaddr lists_address = raw.leafbrushes;
		for (std::uint16_t& v : lists)
		{
			v = v < old_count && remap[v] >= 0 ? static_cast<std::uint16_t>(remap[v]) : std::uint16_t{ 0 };
		}
		struct node_state
		{
			std::vector<std::uint16_t> list;		// what the node lists now
			ZoneDataId inline_list = 0;				// the list is stored behind the node array
			std::optional<ZoneTarget> pointer;		// else: where it points
			bool rewrite = false;
		};
		std::vector<node_state> states(nodes.size());
		std::map<std::uint32_t, ZoneTarget> node_pointers;
		for (const ZonePointer& p : editor.data_pointers(nodes_id))
		{
			if (p.target.kind != ZoneTarget::Kind::raw)
			{
				node_pointers.emplace(p.offset, p.target);
			}
		}
		for (std::size_t n = 0; n < nodes.size(); n++)
		{
			if (nodes[n].leafBrushCount <= 0)
			{
				continue;
			}
			node_state& s = states[n];
			const zaddr address = rd<zaddr>({ node_image[n].data, 12 }, 0);
			const auto old_list = zone.get_array<std::uint16_t>(address, static_cast<std::size_t>(nodes[n].leafBrushCount));
			bool lost = false;
			for (const std::uint16_t b : old_list)
			{
				if (b < old_count && !removed[b] && remap[b] >= 0)
				{
					s.list.push_back(static_cast<std::uint16_t>(remap[b]));
				}
				else
				{
					lost = true;
				}
			}
			if (rd<std::uint32_t>({ nodes[n].data, 12 }, 0) == FOLLOWS)
			{
				s.inline_list = ctx.data(address, "leaf brush list");
				s.rewrite = lost || s.list != old_list;
			}
			else
			{
				const auto it = node_pointers.find(static_cast<std::uint32_t>(NODE_SIZE * n + 8));
				if (it == node_pointers.end())
				{
					throw compile_error("brushes: a leaf brush node lists brushes without pointing at them");
				}
				s.pointer = it->second;
				// a list in the leafbrushes array was renumbered in place; one that lost a brush moves to a new run
				s.rewrite = lost || !(address >= lists_address && it->second.kind == ZoneTarget::Kind::data && it->second.data == lists_id);
				if (!lost && !s.rewrite)
				{
					s.rewrite = false;
				}
				else if (!lost)
				{
					// stored elsewhere and not ours to renumber: only an issue when brushes were deleted
					s.rewrite = s.list != old_list;
				}
			}
		}

		// ---- new brushes into the trees
		struct root_add
		{
			std::vector<int> brushes;
			int contents = 0;
		};
		std::map<int, root_add> wrap;			// old root ( inner node ) -> brushes to put in front of it
		std::vector<std::pair<std::vector<int>, int>> fresh_roots;		// trees of their own: brushes, contents
		std::map<int, int> fresh_root_of_leaf, fresh_root_of_model;
		const auto contents_of = [&](const std::vector<int>& list)
		{
			int all = 0;
			for (const int b : list)
			{
				all |= new_contents[static_cast<std::size_t>(b)];
			}
			return all;
		};
		const auto add_to_root = [&](const int root, const std::vector<int>& add) -> bool		// false: there is no tree, make one
		{
			if (root <= 0 || root >= static_cast<int>(nodes.size()))
			{
				return false;
			}
			if (nodes[static_cast<std::size_t>(root)].leafBrushCount > 0)
			{
				node_state& s = states[static_cast<std::size_t>(root)];
				for (const int b : add)
				{
					if (std::find(s.list.begin(), s.list.end(), static_cast<std::uint16_t>(b)) == s.list.end())
					{
						s.list.push_back(static_cast<std::uint16_t>(b));
					}
				}
				s.rewrite = true;
				nodes[static_cast<std::size_t>(root)].contents |= contents_of(add);
				return true;
			}
			root_add& w = wrap[root];
			for (const int b : add)
			{
				if (std::find(w.brushes.begin(), w.brushes.end(), b) == w.brushes.end())
				{
					w.brushes.push_back(b);
				}
			}
			w.contents |= contents_of(add);
			return true;
		};
		for (const auto& [leaf, add] : leaf_adds)
		{
			c1::cLeaf_t& l = leafs[static_cast<std::size_t>(leaf)];
			const bool had_brushes = l.brushContents != 0 && l.leafBrushNode > 0;
			if (!add_to_root(had_brushes ? l.leafBrushNode : 0, add))
			{
				fresh_root_of_leaf[leaf] = static_cast<int>(fresh_roots.size());
				fresh_roots.emplace_back(add, contents_of(add));
			}
			box grown = had_brushes || l.terrainContents != 0 ? box(l.bounds) : box();
			for (const int b : add)
			{
				grown.add(padded(box(new_bounds[static_cast<std::size_t>(b)]), leaf_pad));
			}
			l.bounds = grown.bounds();
			l.brushContents |= contents_of(add);
			ctx.rep.leafs_touched++;
		}

		// ---- brush models: the tree, then bounds and contents from what the model lists now
		std::set<int> models_changed;
		for (const auto& [model, add] : model_adds)
		{
			models_changed.insert(model);
		}
		std::vector<std::vector<int>> model_lists(models.size());
		{
			const std::function<void(int, std::vector<int>&, int)> collect = [&](const int n, std::vector<int>& out, const int depth)
			{
				if (n < 0 || n >= static_cast<int>(nodes.size()) || depth > 128)
				{
					return;
				}
				const c1::cLeafBrushNode_s& node = nodes[static_cast<std::size_t>(n)];
				if (node.leafBrushCount > 0)
				{
					out.insert(out.end(), states[static_cast<std::size_t>(n)].list.begin(), states[static_cast<std::size_t>(n)].list.end());
					return;
				}
				if (node.leafBrushCount < 0)
				{
					collect(n + 1, out, depth + 1);
				}
				for (int c = 0; c < 2; c++)
				{
					if (const std::uint16_t offset = rd<std::uint16_t>({ node.data, 12 }, 8 + 2 * static_cast<std::size_t>(c)))
					{
						collect(n + offset, out, depth + 1);
					}
				}
			};
			// a model that lost a brush is changed too
			std::vector<int> owner(old_count, 0);
			for (std::size_t m = 1; m < models.size(); m++)
			{
				if (models[m].leaf.leafBrushNode > 0)
				{
					// before the removals: what the base zone lists
					const std::function<void(int, int)> mark = [&](const int n, const int depth)
					{
						if (n < 0 || n >= static_cast<int>(nodes.size()) || depth > 128)
						{
							return;
						}
						const c1::cLeafBrushNode_s& node = nodes[static_cast<std::size_t>(n)];
						if (node.leafBrushCount > 0)
						{
							const auto old_list = zone.get_array<std::uint16_t>(rd<zaddr>({ node_image[static_cast<std::size_t>(n)].data, 12 }, 0), static_cast<std::size_t>(node.leafBrushCount));
							for (const std::uint16_t b : old_list)
							{
								if (b < old_count && removed[b])
								{
									models_changed.insert(static_cast<int>(m));
								}
							}
							return;
						}
						if (node.leafBrushCount < 0)
						{
							mark(n + 1, depth + 1);
						}
						for (int c = 0; c < 2; c++)
						{
							if (const std::uint16_t offset = rd<std::uint16_t>({ node.data, 12 }, 8 + 2 * static_cast<std::size_t>(c)))
							{
								mark(n + offset, depth + 1);
							}
						}
					};
					mark(models[m].leaf.leafBrushNode, 0);
				}
			}
			for (const int m : models_changed)
			{
				if (m <= 0 || m >= static_cast<int>(models.size()))
				{
					continue;
				}
				c1::cmodel_t& model = models[static_cast<std::size_t>(m)];
				std::vector<int> all;
				if (model.leaf.leafBrushNode > 0)
				{
					collect(model.leaf.leafBrushNode, all, 0);
				}
				if (const auto add = model_adds.find(m); add != model_adds.end())
				{
					if (!add_to_root(model.leaf.brushContents != 0 || model.leaf.leafBrushNode > 0 ? model.leaf.leafBrushNode : 0, add->second))
					{
						fresh_root_of_model[m] = static_cast<int>(fresh_roots.size());
						fresh_roots.emplace_back(add->second, contents_of(add->second));
					}
					all.insert(all.end(), add->second.begin(), add->second.end());
				}
				box all_bounds;
				int all_contents = 0;
				for (const int b : all)
				{
					all_bounds.add(box(new_bounds[static_cast<std::size_t>(b)]));
					all_contents |= new_contents[static_cast<std::size_t>(b)];
				}
				if (model.leaf.collAabbCount != 0)
				{
					all_bounds.add(box(model.leaf.bounds));		// the model also has collision triangles
				}
				if (!all_bounds.valid())
				{
					ctx.warn(format("brush model *%d has no brush left; its bounds and contents are cleared", m));
					model.leaf.brushContents = 0;
					continue;
				}
				// the leaf's box and the model's box around the brushes, the radius to the far corner of the model's box.
				// The contents the leaf had stay ( in the base zones a leaf can hold more than its brushes' materials say ).
				model.leaf.bounds = padded(all_bounds, leaf_pad).bounds();
				model.leaf.brushContents |= all_contents;
				const box outer = padded(all_bounds, model_pad);
				float far_corner = 0;
				for (int k = 0; k < 3; k++)
				{
					const float reach = std::max(std::fabs(outer.mins[k]), std::fabs(outer.maxs[k]));
					far_corner += reach * reach;
				}
				model.bounds = outer.bounds();
				model.radius = std::sqrt(far_corner);
				ctx.rep.leafs_touched++;
			}
		}

		// ---- write the lists: new runs at the end of leafbrushes
		const auto new_run = [&](const std::vector<std::uint16_t>& list)
		{
			const std::uint32_t at = static_cast<std::uint32_t>(lists.size());
			lists.insert(lists.end(), list.begin(), list.end());
			return ZoneTarget::to_data(lists_id, 2 * at);
		};
		struct emitted
		{
			c1::cLeafBrushNode_s node{};
			std::optional<ZoneTarget> pointer;
		};
		std::vector<emitted> out_nodes;
		std::vector<int> new_of_node(nodes.size(), -1), root_of_node(nodes.size(), -1);
		std::vector<ZoneDataId> inline_removed;
		std::vector<std::pair<ZoneDataId, std::vector<std::uint16_t>>> inline_rewritten;
		const auto list_node = [&](const std::vector<int>& brush_list, const int list_contents)
		{
			emitted e;
			e.node.leafBrushCount = static_cast<std::int16_t>(brush_list.size());
			e.node.contents = list_contents;
			std::vector<std::uint16_t> run;
			for (const int b : brush_list)
			{
				run.push_back(static_cast<std::uint16_t>(b));
			}
			e.pointer = new_run(run);
			return e;
		};
		for (std::size_t n = 0; n < nodes.size(); n++)
		{
			if (const auto w = wrap.find(static_cast<int>(n)); w != wrap.end())
			{
				// the node in front of an old root: its own list in the next node, both children the old root
				emitted e;
				e.node.leafBrushCount = -1;
				e.node.contents = nodes[n].contents | w->second.contents;
				const float dist = 0.0f, range = 1e30f;		// every trace is on the child 0 side
				const std::uint16_t children[2] = { 2, 2 };
				std::memcpy(e.node.data, &dist, 4);
				std::memcpy(e.node.data + 4, &range, 4);
				std::memcpy(e.node.data + 8, children, 4);
				root_of_node[n] = static_cast<int>(out_nodes.size());
				out_nodes.push_back(e);
				out_nodes.push_back(list_node(w->second.brushes, w->second.contents));
			}
			new_of_node[n] = static_cast<int>(out_nodes.size());
			emitted e;
			e.node = nodes[n];
			if (nodes[n].leafBrushCount > 0)
			{
				node_state& s = states[n];
				if (s.list.empty())
				{
					// nothing left: a node no trace enters
					e.node.leafBrushCount = 0;
					e.node.contents = 0;
					std::memset(e.node.data, 0, sizeof(e.node.data));
					if (s.inline_list)
					{
						inline_removed.push_back(s.inline_list);
					}
				}
				else
				{
					e.node.leafBrushCount = static_cast<std::int16_t>(s.list.size());
					if (s.inline_list)
					{
						if (s.rewrite)
						{
							inline_rewritten.emplace_back(s.inline_list, s.list);
						}
					}
					else
					{
						e.pointer = s.rewrite ? new_run(s.list) : *s.pointer;
						std::memset(e.node.data, 0, 4);
					}
				}
			}
			out_nodes.push_back(e);
		}
		const int first_fresh_root = static_cast<int>(out_nodes.size());
		for (const auto& [brush_list, list_contents] : fresh_roots)
		{
			out_nodes.push_back(list_node(brush_list, list_contents));
		}
		// the children of the old nodes are relative: nothing may have come between a node and its children
		for (std::size_t n = 0; n < nodes.size(); n++)
		{
			if (nodes[n].leafBrushCount > 0)
			{
				continue;
			}
			for (int c = 0; c < 2; c++)
			{
				const std::uint16_t offset = rd<std::uint16_t>({ nodes[n].data, 12 }, 8 + 2 * static_cast<std::size_t>(c));
				if (offset && (n + offset >= nodes.size() || new_of_node[n + offset] - new_of_node[n] != offset || wrap.count(static_cast<int>(n + offset))))
				{
					throw compile_error("brushes: a leaf brush tree of the base zone is not laid out as one block per collision leaf; new brushes cannot be added to it");
				}
			}
			if (nodes[n].leafBrushCount < 0 && (n + 1 >= nodes.size() || new_of_node[n + 1] - new_of_node[n] != 1))
			{
				throw compile_error("brushes: a leaf brush tree of the base zone is not laid out as one block per collision leaf; new brushes cannot be added to it");
			}
		}
		const auto root_now = [&](const int old_root) { return root_of_node[static_cast<std::size_t>(old_root)] >= 0 ? root_of_node[static_cast<std::size_t>(old_root)] : new_of_node[static_cast<std::size_t>(old_root)]; };
		for (std::size_t l = 0; l < leafs.size(); l++)
		{
			if (const auto f = fresh_root_of_leaf.find(static_cast<int>(l)); f != fresh_root_of_leaf.end())
			{
				leafs[l].leafBrushNode = first_fresh_root + f->second;
			}
			else if (leafs[l].leafBrushNode > 0 && leafs[l].leafBrushNode < static_cast<int>(nodes.size()))
			{
				leafs[l].leafBrushNode = root_now(leafs[l].leafBrushNode);
			}
		}
		for (std::size_t m = 0; m < models.size(); m++)
		{
			if (const auto f = fresh_root_of_model.find(static_cast<int>(m)); f != fresh_root_of_model.end())
			{
				models[m].leaf.leafBrushNode = first_fresh_root + f->second;
			}
			else if (models[m].leaf.leafBrushNode > 0 && models[m].leaf.leafBrushNode < static_cast<int>(nodes.size()))
			{
				models[m].leaf.leafBrushNode = root_now(models[m].leaf.leafBrushNode);
			}
		}

		// ---- write. Arrays that are pointed into grow before the pointers that use the new part are set.
		std::vector<c1::cLeafBrushNode_s> packed_nodes(out_nodes.size());
		std::vector<ZonePointer> packed_pointers;
		for (std::size_t n = 0; n < out_nodes.size(); n++)
		{
			packed_nodes[n] = out_nodes[n].node;
			if (out_nodes[n].pointer)
			{
				packed_pointers.push_back({ static_cast<std::uint32_t>(NODE_SIZE * n + 8), *out_nodes[n].pointer });
			}
		}
		ctx.set(nodes_id, bytes_of(packed_nodes), {}, "clipMap.leafbrushNodes");
		ctx.set(brushes_id, bytes_of(new_brushes), {}, "clipMap.brushes");
		ctx.set(planes_id, bytes_of(planes), {}, "plane array");
		ctx.set(sides_id, sides, side_pointers, "clipMap.brushsides");
		ctx.set(edges_id, edges, {}, "clipMap.brushEdges");
		ctx.set(lists_id, bytes_of(lists), {}, "clipMap.leafbrushes");
		for (const ZoneDataId id : inline_removed)
		{
			check(editor.remove_data(id), "leaf brush list");
		}
		for (const auto& [id, list] : inline_rewritten)
		{
			ctx.set(id, bytes_of(list), {}, "leaf brush list");
		}
		ctx.set(nodes_id, bytes_of(packed_nodes), packed_pointers, "clipMap.leafbrushNodes");
		ctx.set(brushes_id, bytes_of(new_brushes), new_brush_pointers, "clipMap.brushes");
		ctx.set(bounds_id, bytes_of(new_bounds), {}, "clipMap.brushBounds");
		ctx.set(contents_id, bytes_of(new_contents), {}, "clipMap.brushContents");
		ctx.set(leafs_id, bytes_of(leafs), {}, "clipMap.leafs");
		ctx.set(models_id, bytes_of(models), {}, "clipMap.cmodels");
		ctx.patch(ctx.clip_header, offsetof(c1::clipMap_t, numBrushes), static_cast<std::uint16_t>(next_index), "clipMap.numBrushes");
		ctx.patch(ctx.clip_header, offsetof(c1::clipMap_t, numBrushSides), static_cast<std::uint32_t>(sides.size() / 8), "clipMap.numBrushSides");
		ctx.patch(ctx.clip_header, offsetof(c1::clipMap_t, numBrushEdges), static_cast<std::uint32_t>(edges.size()), "clipMap.numBrushEdges");
		ctx.patch(ctx.clip_header, offsetof(c1::clipMap_t, leafbrushNodesCount), static_cast<std::uint32_t>(packed_nodes.size()), "clipMap.leafbrushNodesCount");
		ctx.patch(ctx.clip_header, offsetof(c1::clipMap_t, numLeafBrushes), static_cast<std::uint32_t>(lists.size()), "clipMap.numLeafBrushes");
		ctx.patch(ctx.clip_header, offsetof(c1::clipMap_t, planeCount), static_cast<std::int32_t>(planes.size()), "clipMap.planeCount");
		ctx.patch(ctx.gfx_header, offsetof(c1::GfxWorld, planeCount), static_cast<std::int32_t>(planes.size()), "GfxWorld.planeCount");

		for (const new_brush& b : fresh)
		{
			(is_fresh[static_cast<std::size_t>(b.index)] && static_cast<std::uint32_t>(b.index) < kept ? ctx.rep.brushes_replaced : ctx.rep.brushes_added)++;
		}
		ctx.rep.brushes_deleted = static_cast<int>(std::count(deleted.begin(), deleted.end(), std::uint8_t{ 1 }));
		ctx.rep.brush_sides_added = static_cast<int>(sides.size() / 8 - old_sides);
		ctx.rep.planes_added = static_cast<int>(planes.size() - old_planes);
		ctx.note(format("brushes: %u -> %u brushes; %zu leaf brush nodes -> %zu ( %zu put in front of a tree, %zu trees of their own ); leafbrushes %u -> %zu entries", old_count,
			next_index, nodes.size(), packed_nodes.size(), 2 * wrap.size(), fresh_roots.size(), raw.numLeafBrushes, lists.size()));
		if (ctx.rep.brushes_deleted + ctx.rep.brushes_replaced > 0)
		{
			ctx.note("brushes: the sides, planes and edge lists of removed brushes stay in their arrays, unreferenced");
		}
	}
}
