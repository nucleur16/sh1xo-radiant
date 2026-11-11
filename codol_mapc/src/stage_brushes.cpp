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
// or of its brush model: into the list of a tree that is one list, else among the brushes on the split of the
// tree's root ( tested by every trace that enters the tree ). A removed brush leaves the lists that name it; a
// list left empty goes, with every split that has nothing left to choose between.
//
// The arrays are written again whole, in the shape the game's map compiler gives them and a dedicated server
// insists on ( server_check.hpp ): brushsides and brushEdges are the runs of the brushes in brush order, leafbrushes
// the lists of the nodes in node order, each back to back and without anything else.

#include "mapc_impl.hpp"

#include <functional>
#include <optional>
#include <set>
#include <tuple>

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
				// the editor names world materials "wc/<name>", the clip map without the prefix
				const std::string_view bare = name.rfind("wc/", 0) == 0 ? std::string_view(name).substr(3) : std::string_view(name);
				if (const auto it = material_index.find(bare); it != material_index.end())
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

		// ---- brush arrays: the kept brushes in order ( a replaced one in its place ), then the new ones.
		// brushsides and brushEdges are written again from the first entry: the runs of the brushes back to back in
		// brush order, nothing else in them. That is how the base zones have them, and a dedicated server reads them
		// by walking the brushes ( server_check.hpp ), so a run that is appended behind the others, or one that a
		// removed brush leaves behind, makes the map unloadable there.
		std::map<std::uint32_t, ZoneTarget> brush_pointers, side_planes;
		for (const ZonePointer& p : editor.data_pointers(brushes_id))
		{
			brush_pointers.emplace(p.offset, p.target);
		}
		for (const ZonePointer& p : editor.data_pointers(sides_id))
		{
			side_planes.emplace(p.offset, p.target);
		}
		for (const auto& [id, owner, what] : { std::tuple{ sides_id, brushes_id, "clipMap.brushsides" }, std::tuple{ edges_id, brushes_id, "clipMap.brushEdges" },
			std::tuple{ lists_id, nodes_id, "clipMap.leafbrushes" } })
		{
			for (const zone::ZoneInbound& in : editor.pointers_to(id))
			{
				if (in.from != owner)
				{
					throw compile_error(format("brushes: something else than the clip map's brushes / leaf brush nodes points into %s; the array cannot be written again", what));
				}
			}
		}
		std::vector<c1::cbrush_t> new_brushes(next_index);
		std::vector<c1::Bounds> new_bounds(next_index);
		std::vector<std::int32_t> new_contents(next_index);
		std::vector<ZonePointer> new_brush_pointers, new_side_pointers;
		std::vector<std::uint8_t> new_sides, new_edges;
		std::vector<int> old_of(next_index, -1);
		std::vector<const new_brush*> fresh_at(next_index, nullptr);
		for (const new_brush& b : fresh)
		{
			fresh_at[static_cast<std::size_t>(b.index)] = &b;
		}
		for (std::uint32_t b = 0; b < old_count; b++)
		{
			if (remap[b] >= 0 && !fresh_at[static_cast<std::size_t>(remap[b])])
			{
				old_of[static_cast<std::size_t>(remap[b])] = static_cast<int>(b);
			}
		}
		const std::uint32_t sides_offset = offsetof(c1::cbrush_t, sides), edges_offset = offsetof(c1::cbrush_t, baseAdjacentSide);
		const std::size_t old_planes = planes.size(), old_sides = sides.size() / 8;
		struct empty_run		// the pointer of a brush without sides / edges that led into the array: it keeps its place between the runs
		{
			std::uint32_t field;
			bool is_sides;
			std::uint32_t offset;
		};
		std::vector<empty_run> empty_runs;
		int new_side_count = 0;
		for (std::uint32_t n = 0; n < next_index; n++)
		{
			const std::uint32_t at = 36u * n;
			if (const new_brush* fb = fresh_at[n])
			{
				const new_brush& b = *fb;
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
				if (!b.sides.empty())
				{
					new_brush_pointers.push_back({ at + sides_offset, ZoneTarget::to_data(sides_id, static_cast<std::uint32_t>(new_sides.size())) });
				}
				for (const new_side& s : b.sides)
				{
					// the plane: one the map has, else a new one at the end of the array. A unit normal: the server
					// refuses a plane array with a normal whose length^2 is more than 0.001 from 1.
					float normal[3] = { s.normal[0], s.normal[1], s.normal[2] };
					const float length = std::sqrt(normal[0] * normal[0] + normal[1] * normal[1] + normal[2] * normal[2]);
					if (!(length > 0.5f) || !std::isfinite(length) || !std::isfinite(s.dist))
					{
						throw compile_error("brushes: a side of a new brush has no usable plane");
					}
					const float dist = s.dist / length;
					for (float& v : normal)
					{
						v /= length;
					}
					std::size_t plane = planes.size();
					for (std::size_t p = 0; p < planes.size(); p++)
					{
						if (std::fabs(planes[p].dist - dist) < 1e-3f && std::fabs(planes[p].normal[0] - normal[0]) < 1e-5f && std::fabs(planes[p].normal[1] - normal[1]) < 1e-5f
							&& std::fabs(planes[p].normal[2] - normal[2]) < 1e-5f)
						{
							plane = p;
							break;
						}
					}
					if (plane == planes.size())
					{
						c1::cplane_s p{};
						std::memcpy(p.normal, normal, 12);
						p.dist = dist;
						p.type = 3;		// not axial; the base zones leave signbits 0
						planes.push_back(p);
					}
					c1::cbrushside_t side{};
					side.materialNum = static_cast<std::uint16_t>(s.material);
					side.firstAdjacentSideOffset = static_cast<std::uint8_t>(run.size());
					side.edgeCount = static_cast<std::uint8_t>(s.adjacent.size());
					run.insert(run.end(), s.adjacent.begin(), s.adjacent.end());
					new_side_pointers.push_back({ static_cast<std::uint32_t>(new_sides.size()), ZoneTarget::to_data(planes_id, static_cast<std::uint32_t>(20 * plane)) });
					append(new_sides, side);
					new_side_count++;
				}
				if (!run.empty())
				{
					new_brush_pointers.push_back({ at + edges_offset, ZoneTarget::to_data(edges_id, static_cast<std::uint32_t>(new_edges.size())) });
					new_edges.insert(new_edges.end(), run.begin(), run.end());
				}
				new_brushes[n] = packed;
				new_bounds[n] = b.bounds.bounds();
				new_contents[n] = b.contents;
				continue;
			}
			const std::uint32_t b = static_cast<std::uint32_t>(old_of[n]);
			c1::cbrush_t packed = brushes[b];
			new_bounds[n] = bounds[b];
			new_contents[n] = contents[b];
			std::uint32_t edge_total = 0;
			for (int positive = 0; positive < 2; positive++)
			{
				for (int axis = 0; axis < 3; axis++)
				{
					edge_total += packed.edgeCount[positive][axis];
				}
			}
			const auto old_sides_pointer = brush_pointers.find(36 * b + sides_offset), old_edges_pointer = brush_pointers.find(36 * b + edges_offset);
			const auto into = [&](const std::map<std::uint32_t, ZoneTarget>::const_iterator it, const ZoneDataId array)
			{
				return it != brush_pointers.end() && it->second.kind == ZoneTarget::Kind::data && it->second.data == array;
			};
			if (packed.numsides)
			{
				if (!into(old_sides_pointer, sides_id) || old_sides_pointer->second.offset % 8 || old_sides_pointer->second.offset + 8u * packed.numsides > sides.size())
				{
					throw compile_error(format("brushes: the sides of brush %u are not a run of clipMap.brushsides", b));
				}
				new_brush_pointers.push_back({ at + sides_offset, ZoneTarget::to_data(sides_id, static_cast<std::uint32_t>(new_sides.size())) });
				for (std::uint32_t s = 0; s < packed.numsides; s++)
				{
					const std::uint32_t source = old_sides_pointer->second.offset + 8 * s;
					const auto plane = side_planes.find(source);
					if (plane == side_planes.end())
					{
						throw compile_error(format("brushes: a side of brush %u does not point at its plane", b));
					}
					new_side_pointers.push_back({ static_cast<std::uint32_t>(new_sides.size()), plane->second });
					new_sides.insert(new_sides.end(), sides.begin() + source, sides.begin() + source + 8);
					edge_total += sides[source + offsetof(c1::cbrushside_t, edgeCount)];
				}
			}
			else if (into(old_sides_pointer, sides_id))
			{
				empty_runs.push_back({ at + sides_offset, true, static_cast<std::uint32_t>(new_sides.size()) });
				packed.sides = 0;
			}
			else if (old_sides_pointer != brush_pointers.end())
			{
				new_brush_pointers.push_back({ at + sides_offset, old_sides_pointer->second });
			}
			if (edge_total)
			{
				if (!into(old_edges_pointer, edges_id) || old_edges_pointer->second.offset + edge_total > edges.size())
				{
					throw compile_error(format("brushes: the edge lists of brush %u are not a run of clipMap.brushEdges", b));
				}
				new_brush_pointers.push_back({ at + edges_offset, ZoneTarget::to_data(edges_id, static_cast<std::uint32_t>(new_edges.size())) });
				new_edges.insert(new_edges.end(), edges.begin() + old_edges_pointer->second.offset, edges.begin() + old_edges_pointer->second.offset + edge_total);
			}
			else if (into(old_edges_pointer, edges_id))
			{
				empty_runs.push_back({ at + edges_offset, false, static_cast<std::uint32_t>(new_edges.size()) });
				packed.baseAdjacentSide = 0;
			}
			else if (old_edges_pointer != brush_pointers.end())
			{
				new_brush_pointers.push_back({ at + edges_offset, old_edges_pointer->second });
			}
			new_brushes[n] = packed;
		}
		for (const empty_run& r : empty_runs)
		{
			// behind the last run there is nothing to point at: the field is null then ( nothing reads it )
			if (r.offset < (r.is_sides ? new_sides.size() : new_edges.size()))
			{
				new_brush_pointers.push_back({ r.field, ZoneTarget::to_data(r.is_sides ? sides_id : edges_id, r.offset) });
			}
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

		// ---- the leaf brush trees.
		// What a dedicated server takes ( server_check.hpp ): a node lists brushes, or it splits with two children that
		// lie behind it - there is no node that does neither - and in node order the lists that are stored in
		// clipMap.leafbrushes are that array, back to back. So the trees are edited as trees and written again:
		//   a removed brush leaves its list; a list left empty goes, and with it every split that has nothing left
		//     on one side and nothing on the split ( the other side takes its place );
		//   a new brush joins the list of a tree that is one list. In a tree that splits it joins the brushes *on* the
		//     root's split ( the list a node with leafBrushCount -1 has behind it; a root without one gets one ): those
		//     are tested by every trace that enters the tree, whatever side of the split it is on, so this does not
		//     depend on reading dist / range right, and no node is made up.
		struct node_state
		{
			std::vector<std::uint16_t> list;		// what the node lists now ( new brush indices )
			ZoneDataId inline_list = 0;				// the list is stored behind the node array
			bool changed = false;
		};
		const std::size_t node_count = nodes.size();
		std::vector<node_state> states(node_count);
		std::map<std::uint32_t, ZoneTarget> node_pointers;
		for (const ZonePointer& p : editor.data_pointers(nodes_id))
		{
			node_pointers.emplace(p.offset, p.target);
		}
		const auto is_list = [&](const std::size_t n) { return nodes[n].leafBrushCount > 0; };
		const auto is_reserved = [&](const std::size_t n)
		{
			static const c1::cLeafBrushNode_s zero{};
			return n == 0 && std::memcmp(&nodes[0], &zero, sizeof(zero)) == 0;
		};
		const auto child_of = [&](const std::size_t n, const int c) { return n + rd<std::uint16_t>({ nodes[n].data, 12 }, 8 + 2 * static_cast<std::size_t>(c)); };
		for (std::size_t n = 0; n < node_count; n++)
		{
			if (!is_list(n))
			{
				if (is_reserved(n))
				{
					continue;
				}
				// a split: children behind it, inside the array; with leafBrushCount -1 the node right behind it is the
				// tree ( mostly one list ) of the brushes on the split
				for (int c = 0; c < 2; c++)
				{
					if (child_of(n, c) <= n || child_of(n, c) >= node_count)
					{
						throw compile_error(format("brushes: leaf brush node %zu neither lists brushes nor splits; the trees of the base zone are not shaped as expected", n));
					}
				}
				if (nodes[n].leafBrushCount < -1 || (nodes[n].leafBrushCount < 0 && n + 1 >= node_count))
				{
					throw compile_error(format("brushes: leaf brush node %zu ( leafBrushCount %d ) has no node behind it", n, nodes[n].leafBrushCount));
				}
				continue;
			}
			node_state& s = states[n];
			const zaddr address = rd<zaddr>({ node_image[n].data, 12 }, 0);
			const auto old_list = zone.get_array<std::uint16_t>(address, static_cast<std::size_t>(nodes[n].leafBrushCount));
			for (const std::uint16_t b : old_list)
			{
				if (b < old_count && !removed[b] && remap[b] >= 0)
				{
					s.list.push_back(static_cast<std::uint16_t>(remap[b]));
				}
			}
			s.changed = s.list != old_list;
			if (rd<std::uint32_t>({ nodes[n].data, 12 }, 0) == FOLLOWS)
			{
				s.inline_list = ctx.data(address, "leaf brush list");
			}
			else
			{
				const auto it = node_pointers.find(static_cast<std::uint32_t>(NODE_SIZE * n + 8));
				if (it == node_pointers.end() || it->second.kind != ZoneTarget::Kind::data || it->second.data != lists_id)
				{
					throw compile_error(format("brushes: the list of leaf brush node %zu is neither in clipMap.leafbrushes nor behind the nodes", n));
				}
			}
		}
		std::map<std::size_t, std::vector<std::uint16_t>> new_split_lists;		// split -> a new list of brushes on its split, written right behind it
		// what stands for the tree below node n once the empty parts are gone: a node of the old array, or -1 for nothing
		const std::function<int(std::size_t, int)> resolve = [&](const std::size_t n, const int depth) -> int
		{
			if (depth > 256)
			{
				throw compile_error("brushes: a leaf brush tree of the base zone is deeper than 256 nodes ( or not a tree )");
			}
			if (is_list(n))
			{
				return states[n].list.empty() ? -1 : static_cast<int>(n);
			}
			if (is_reserved(n))
			{
				return -1;
			}
			const int a = resolve(child_of(n, 0), depth + 1), b = resolve(child_of(n, 1), depth + 1);
			const int on_split = nodes[n].leafBrushCount < 0 ? resolve(n + 1, depth + 1) : -1;
			if (new_split_lists.count(n) || on_split >= 0)
			{
				// with nothing left on either side, what is on the split is the tree
				return a < 0 && b < 0 ? on_split : static_cast<int>(n);
			}
			return a >= 0 && b >= 0 ? static_cast<int>(n) : a >= 0 ? a : b;
		};
		const auto has_split_list = [&](const std::size_t n)
		{
			return new_split_lists.count(n) != 0 || (nodes[n].leafBrushCount < 0 && resolve(n + 1, 1) >= 0);
		};

		// ---- new brushes into the trees
		std::vector<std::pair<std::vector<int>, int>> fresh_roots;		// trees of their own: brushes, contents
		std::map<int, int> fresh_root_of_leaf, fresh_root_of_model;
		std::size_t split_lists_grown = 0;
		const auto contents_of = [&](const std::vector<int>& list)
		{
			int all = 0;
			for (const int b : list)
			{
				all |= new_contents[static_cast<std::size_t>(b)];
			}
			return all;
		};
		const auto add_to_list = [](std::vector<std::uint16_t>& list, const std::vector<int>& add)
		{
			for (const int b : add)
			{
				if (std::find(list.begin(), list.end(), static_cast<std::uint16_t>(b)) == list.end())
				{
					list.push_back(static_cast<std::uint16_t>(b));
				}
			}
		};
		// adds the brushes to the tree below node n so that every trace that enters the tree tests them. false: nothing
		// is left of that tree
		const std::function<bool(std::size_t, const std::vector<int>&, int)> add_to_tree = [&](const std::size_t n, const std::vector<int>& add, const int depth) -> bool
		{
			if (is_list(n))
			{
				add_to_list(states[n].list, add);
				states[n].changed = true;
				nodes[n].contents |= contents_of(add);
				return true;
			}
			if (is_reserved(n) || depth > 256)
			{
				return false;
			}
			const bool sides_left = resolve(child_of(n, 0), 1) >= 0 || resolve(child_of(n, 1), 1) >= 0;
			if (nodes[n].leafBrushCount < 0 && resolve(n + 1, 1) >= 0)
			{
				// among the brushes on the split
				if (!add_to_tree(n + 1, add, depth + 1))
				{
					return false;
				}
			}
			else if (sides_left)
			{
				add_to_list(new_split_lists[n], add);
			}
			else
			{
				return false;
			}
			nodes[n].contents |= contents_of(add);
			return true;
		};
		const auto add_to_root = [&](const int root_index, const std::vector<int>& add) -> bool		// false: there is no tree, make one
		{
			if (root_index <= 0 || root_index >= static_cast<int>(node_count) || !add_to_tree(static_cast<std::size_t>(root_index), add, 0))
			{
				return false;
			}
			split_lists_grown += is_list(static_cast<std::size_t>(root_index)) ? 0 : 1;
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
		{
			const std::function<void(int, std::vector<int>&, int)> collect = [&](const int n, std::vector<int>& out, const int depth)
			{
				if (n < 0 || n >= static_cast<int>(node_count) || depth > 256)
				{
					return;
				}
				const std::size_t at = static_cast<std::size_t>(n);
				if (is_list(at))
				{
					out.insert(out.end(), states[at].list.begin(), states[at].list.end());
					return;
				}
				if (is_reserved(at))
				{
					return;
				}
				if (nodes[at].leafBrushCount < 0)
				{
					collect(n + 1, out, depth + 1);
				}
				if (const auto extra = new_split_lists.find(at); extra != new_split_lists.end())
				{
					out.insert(out.end(), extra->second.begin(), extra->second.end());
				}
				for (int c = 0; c < 2; c++)
				{
					collect(static_cast<int>(child_of(at, c)), out, depth + 1);
				}
			};
			// a model that lost a brush is changed too: what the base zone lists below its root
			for (std::size_t m = 1; m < models.size(); m++)
			{
				if (models[m].leaf.leafBrushNode <= 0)
				{
					continue;
				}
				const std::function<void(int, int)> mark = [&](const int n, const int depth)
				{
					if (n < 0 || n >= static_cast<int>(node_count) || depth > 256)
					{
						return;
					}
					const std::size_t at = static_cast<std::size_t>(n);
					if (is_list(at))
					{
						const auto old_list = zone.get_array<std::uint16_t>(rd<zaddr>({ node_image[at].data, 12 }, 0), static_cast<std::size_t>(nodes[at].leafBrushCount));
						for (const std::uint16_t b : old_list)
						{
							if (b < old_count && removed[b])
							{
								models_changed.insert(static_cast<int>(m));
							}
						}
						return;
					}
					if (is_reserved(at))
					{
						return;
					}
					if (nodes[at].leafBrushCount < 0)
					{
						mark(n + 1, depth + 1);
					}
					for (int c = 0; c < 2; c++)
					{
						mark(static_cast<int>(child_of(at, c)), depth + 1);
					}
				};
				mark(models[m].leaf.leafBrushNode, 0);
			}
			for (const int m : models_changed)
			{
				if (m <= 0 || m >= static_cast<int>(models.size()))
				{
					continue;
				}
				c1::cmodel_t& model = models[static_cast<std::size_t>(m)];
				if (const auto add = model_adds.find(m); add != model_adds.end())
				{
					if (!add_to_root(model.leaf.brushContents != 0 || model.leaf.leafBrushNode > 0 ? model.leaf.leafBrushNode : 0, add->second))
					{
						fresh_root_of_model[m] = static_cast<int>(fresh_roots.size());
						fresh_roots.emplace_back(add->second, contents_of(add->second));
					}
				}
				std::vector<int> all;
				if (const auto own = fresh_root_of_model.find(m); own != fresh_root_of_model.end())
				{
					all = fresh_roots[static_cast<std::size_t>(own->second)].first;
				}
				else if (model.leaf.leafBrushNode > 0)
				{
					collect(model.leaf.leafBrushNode, all, 0);
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

		// ---- which nodes stay: everything that still stands for itself
		std::vector<std::uint8_t> keep(node_count, 1);
		for (std::size_t n = 0; n < node_count; n++)
		{
			if (is_reserved(n))
			{
				continue;
			}
			keep[n] = resolve(n, 0) == static_cast<int>(n);
		}
		// what is on the split of a node with leafBrushCount -1 stays exactly when it is not empty ( resolve says so ); a
		// split that stays keeps its place in front of it

		// ---- write the nodes again; the lists of clipMap.leafbrushes in node order, back to back
		struct emitted
		{
			c1::cLeafBrushNode_s node{};
			std::optional<ZoneTarget> pointer;
		};
		std::vector<emitted> out_nodes;
		std::vector<std::uint16_t> pool;
		std::vector<int> new_of_node(node_count, -1);
		std::vector<ZoneDataId> inline_removed;
		std::vector<std::pair<ZoneDataId, std::vector<std::uint16_t>>> inline_rewritten;
		std::size_t nodes_dropped = 0, lists_added = 0;
		const auto pooled_list = [&](c1::cLeafBrushNode_s node, const std::vector<std::uint16_t>& list)
		{
			if (list.empty() || list.size() > 0x7FFF)
			{
				throw compile_error("brushes: a leaf brush list with no or more than 32767 brushes");
			}
			emitted e;
			e.node = node;
			e.node.leafBrushCount = static_cast<std::int16_t>(list.size());
			std::memset(e.node.data, 0, sizeof(e.node.data));
			e.pointer = ZoneTarget::to_data(lists_id, static_cast<std::uint32_t>(2 * pool.size()));
			pool.insert(pool.end(), list.begin(), list.end());
			return e;
		};
		const auto fresh_list = [&](const std::vector<std::uint16_t>& list, const int list_contents)
		{
			c1::cLeafBrushNode_s node{};
			node.contents = list_contents;
			lists_added++;
			return pooled_list(node, list);
		};
		for (std::size_t n = 0; n < node_count; n++)
		{
			if (!keep[n])
			{
				nodes_dropped++;
				if (states[n].inline_list)
				{
					inline_removed.push_back(states[n].inline_list);
				}
				continue;
			}
			new_of_node[n] = static_cast<int>(out_nodes.size());
			if (is_list(n))
			{
				const node_state& s = states[n];
				if (s.inline_list)
				{
					emitted e;
					e.node = nodes[n];
					e.node.leafBrushCount = static_cast<std::int16_t>(s.list.size());
					if (s.changed)
					{
						inline_rewritten.emplace_back(s.inline_list, s.list);
					}
					out_nodes.push_back(e);
				}
				else
				{
					out_nodes.push_back(pooled_list(nodes[n], s.list));
				}
				continue;
			}
			emitted e;
			e.node = nodes[n];
			if (!is_reserved(n))
			{
				e.node.leafBrushCount = has_split_list(n) ? std::int16_t{ -1 } : std::int16_t{ 0 };
			}
			out_nodes.push_back(e);
			if (const auto extra = new_split_lists.find(n); extra != new_split_lists.end())
			{
				std::vector<int> as_int(extra->second.begin(), extra->second.end());
				out_nodes.push_back(fresh_list(extra->second, contents_of(as_int)));
			}
		}
		const int first_fresh_root = static_cast<int>(out_nodes.size());
		for (const auto& [brush_list, list_contents] : fresh_roots)
		{
			std::vector<std::uint16_t> list;
			for (const int b : brush_list)
			{
				list.push_back(static_cast<std::uint16_t>(b));
			}
			out_nodes.push_back(fresh_list(list, list_contents));
		}
		// the children of a split are relative: to wherever what stands for the child is now
		for (std::size_t n = 0; n < node_count; n++)
		{
			if (!keep[n] || is_list(n) || is_reserved(n))
			{
				continue;
			}
			c1::cLeafBrushNode_s& node = out_nodes[static_cast<std::size_t>(new_of_node[n])].node;
			int target[2] = { resolve(child_of(n, 0), 1), resolve(child_of(n, 1), 1) };
			if (target[0] < 0 && target[1] < 0)
			{
				throw compile_error("brushes: a split of a leaf brush tree was kept without anything below it");
			}
			// a side with nothing left ( only possible when the node has brushes on its split ): both sides lead to the other
			target[0] = target[0] < 0 ? target[1] : target[0];
			target[1] = target[1] < 0 ? target[0] : target[1];
			for (int c = 0; c < 2; c++)
			{
				const int offset = new_of_node[static_cast<std::size_t>(target[c])] - new_of_node[n];
				if (new_of_node[static_cast<std::size_t>(target[c])] < 0 || offset <= 0 || offset > 0xFFFF)
				{
					throw compile_error("brushes: a leaf brush tree could not be written again ( a child does not lie behind its node )");
				}
				const std::uint16_t value = static_cast<std::uint16_t>(offset);
				std::memcpy(node.data + 8 + 2 * c, &value, 2);
			}
			if (node.leafBrushCount < 0 && !new_split_lists.count(n))
			{
				const int on_split = resolve(n + 1, 1);
				if (on_split < 0 || new_of_node[static_cast<std::size_t>(on_split)] != new_of_node[n] + 1)
				{
					throw compile_error("brushes: a leaf brush tree could not be written again ( the brushes on a split are not right behind it )");
				}
			}
		}
		// roots: what stands for the old root; a tree with nothing left is node 0 and no brush contents
		const auto root_now = [&](const int old_root, std::int32_t& brush_contents)
		{
			const int now = resolve(static_cast<std::size_t>(old_root), 0);
			if (now < 0)
			{
				brush_contents = 0;
				return 0;
			}
			return new_of_node[static_cast<std::size_t>(now)];
		};
		for (std::size_t l = 0; l < leafs.size(); l++)
		{
			if (const auto f = fresh_root_of_leaf.find(static_cast<int>(l)); f != fresh_root_of_leaf.end())
			{
				leafs[l].leafBrushNode = first_fresh_root + f->second;
			}
			else if (leafs[l].leafBrushNode > 0 && leafs[l].leafBrushNode < static_cast<int>(node_count))
			{
				leafs[l].leafBrushNode = root_now(leafs[l].leafBrushNode, leafs[l].brushContents);
			}
		}
		for (std::size_t m = 0; m < models.size(); m++)
		{
			if (const auto f = fresh_root_of_model.find(static_cast<int>(m)); f != fresh_root_of_model.end())
			{
				models[m].leaf.leafBrushNode = first_fresh_root + f->second;
			}
			else if (models[m].leaf.leafBrushNode > 0 && models[m].leaf.leafBrushNode < static_cast<int>(node_count))
			{
				models[m].leaf.leafBrushNode = root_now(models[m].leaf.leafBrushNode, models[m].leaf.brushContents);
			}
		}

		// ---- write. Arrays that are pointed into are set before the pointers that lead into them.
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
		ctx.set(sides_id, new_sides, new_side_pointers, "clipMap.brushsides");
		ctx.set(edges_id, new_edges, {}, "clipMap.brushEdges");
		ctx.set(lists_id, bytes_of(pool), {}, "clipMap.leafbrushes");
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
		ctx.patch(ctx.clip_header, offsetof(c1::clipMap_t, numBrushSides), static_cast<std::uint32_t>(new_sides.size() / 8), "clipMap.numBrushSides");
		ctx.patch(ctx.clip_header, offsetof(c1::clipMap_t, numBrushEdges), static_cast<std::uint32_t>(new_edges.size()), "clipMap.numBrushEdges");
		ctx.patch(ctx.clip_header, offsetof(c1::clipMap_t, leafbrushNodesCount), static_cast<std::uint32_t>(packed_nodes.size()), "clipMap.leafbrushNodesCount");
		ctx.patch(ctx.clip_header, offsetof(c1::clipMap_t, numLeafBrushes), static_cast<std::uint32_t>(pool.size()), "clipMap.numLeafBrushes");
		ctx.patch(ctx.clip_header, offsetof(c1::clipMap_t, planeCount), static_cast<std::int32_t>(planes.size()), "clipMap.planeCount");
		ctx.patch(ctx.gfx_header, offsetof(c1::GfxWorld, planeCount), static_cast<std::int32_t>(planes.size()), "GfxWorld.planeCount");

		for (const new_brush& b : fresh)
		{
			(static_cast<std::uint32_t>(b.index) < kept ? ctx.rep.brushes_replaced : ctx.rep.brushes_added)++;
		}
		ctx.rep.brushes_deleted = static_cast<int>(std::count(deleted.begin(), deleted.end(), std::uint8_t{ 1 }));
		ctx.rep.brush_sides_added = new_side_count;
		ctx.rep.planes_added = static_cast<int>(planes.size() - old_planes);
		ctx.note(format("brushes: %u -> %u brushes; brush sides %zu -> %zu, edge bytes %zu -> %zu ( both written again in brush order ); leaf brush nodes %zu -> %zu "
			"( %zu lists added: %zu on the split of a tree's root, %zu trees of their own; %zu nodes without brushes removed ); leafbrushes %u -> %zu entries",
			old_count, next_index, old_sides, new_sides.size() / 8, edges.size(), new_edges.size(), node_count, packed_nodes.size(), lists_added, lists_added - fresh_roots.size(),
			fresh_roots.size(), nodes_dropped, raw.numLeafBrushes, pool.size()));
		if (split_lists_grown > lists_added - fresh_roots.size())
		{
			ctx.note(format("brushes: %zu trees already had brushes on the split of their root; the new brushes joined that list", split_lists_grown - (lists_added - fresh_roots.size())));
		}
		if (ctx.rep.brushes_deleted + ctx.rep.brushes_replaced > 0)
		{
			ctx.note("brushes: the planes of removed brushes stay in the plane array ( it is shared with the GfxWorld ), unreferenced");
		}
	}
}
