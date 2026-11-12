// clipMap_t ( col_map_mp ) from the brushes of the map: clip materials, brushes with sides and edge lists, the
// collision BSP with its leafs, the leaf brush trees, the sub models, and the MapEnts nested in it.
//
// Everything is laid out the way the game's map compiler does, which is what the dedicated server's decoder takes
// ( codol_mapc/server_check.hpp ): sides and edge bytes are the runs of the brushes in brush order, the leaf brush
// lists the runs of the nodes in node order, BSP children lead forward, plane pointers lead into the plane array
// the GfxWorld stores.

#include "world.hpp"

namespace codol::bsp
{
	namespace
	{
#pragma pack(push, 1)
		struct cnode
		{
			c1::zptr plane;
			std::int16_t children[2];		// >= 0: node, < 0: leaf -1 - child
		};
		struct smodel_node
		{
			c1::Bounds bounds;
			std::uint16_t firstChild;
			std::uint16_t childCount;
		};
		struct map_stage
		{
			c1::zptr name;
			float origin[3];
			std::uint16_t triggerIndex;
			std::uint8_t sunPrimaryLightIndex;
			std::uint8_t pad;
		};
#pragma pack(pop)
		static_assert(sizeof(cnode) == 8 && sizeof(smodel_node) == 28 && sizeof(map_stage) == 20);

		constexpr float LEAF_PAD = 0.125f;		// a leaf's box around its brushes, as measured on the shipped maps
		constexpr float MODEL_PAD = 1.0f;		// a sub model's box around its brushes
		constexpr std::size_t LEAF_BRUSHES = 12;	// a collision leaf is not split further below this many brushes

		// the edge lists every shipped axis aligned box has ( codol_mapc stage_brushes.cpp )
		constexpr std::uint8_t BOX_ADJACENT[2][3][4] =
		{
			{ { 2, 4, 3, 5 }, { 5, 1, 4, 0 }, { 0, 2, 1, 3 } },
			{ { 5, 3, 4, 2 }, { 0, 4, 1, 5 }, { 3, 1, 2, 0 } },
		};

		struct clip_side
		{
			std::uint32_t plane = 0;
			int material = 0;
			std::vector<std::uint8_t> adjacent;
		};

		struct clip_brush
		{
			int source = 0;						// index into world::brushes
			box3 box;
			std::int32_t contents = 0;
			std::int16_t axial_material[2][3] = {};
			std::vector<std::uint8_t> axial_adjacent[2][3];
			std::vector<clip_side> sides;
		};

		struct tree_node
		{
			int axis = 0;
			float dist = 0;
			int child[2] = { 0, 0 };			// >= 0 node index, < 0: leaf -1 - child
		};

		struct tree_leaf
		{
			std::vector<int> brushes;			// clip brush indices
			box3 box;
			std::int32_t contents = 0;
		};

		struct builder
		{
			std::vector<clip_brush>& brushes;
			std::vector<tree_node> nodes;
			std::vector<tree_leaf> leafs;		// [0]: the empty leaf

			// returns the child reference of the sub tree for `list` inside `region`
			int build(const std::vector<int>& list, const box3& region, const int depth)
			{
				if (list.empty())
				{
					return -1;					// leaf 0
				}
				int best_axis = -1;
				float best_dist = 0;
				std::size_t best_cost = list.size() * 2;
				if (list.size() > LEAF_BRUSHES && depth < 48 && nodes.size() < 30000)
				{
					for (int axis = 0; axis < 3; axis++)
					{
						std::vector<float> candidates;
						for (const int b : list)
						{
							for (const float v : { brushes[static_cast<std::size_t>(b)].box.mins[axis], brushes[static_cast<std::size_t>(b)].box.maxs[axis] })
							{
								if (v > region.mins[axis] + 0.01f && v < region.maxs[axis] - 0.01f)
								{
									candidates.push_back(v);
								}
							}
						}
						std::sort(candidates.begin(), candidates.end());
						candidates.erase(std::unique(candidates.begin(), candidates.end()), candidates.end());
						const std::size_t step = std::max<std::size_t>(1, candidates.size() / 48);
						for (std::size_t c = 0; c < candidates.size(); c += step)
						{
							const float dist = candidates[c];
							std::size_t front = 0, back = 0;
							for (const int b : list)
							{
								const box3& box = brushes[static_cast<std::size_t>(b)].box;
								front += box.maxs[axis] > dist;
								back += box.mins[axis] < dist;
							}
							if (front == list.size() || back == list.size())
							{
								continue;		// does not separate anything
							}
							const std::size_t cost = std::max(front, back) + (front + back - list.size());
							if (cost < best_cost)
							{
								best_cost = cost;
								best_axis = axis;
								best_dist = dist;
							}
						}
					}
				}
				if (best_axis < 0)
				{
					tree_leaf leaf;
					leaf.brushes = list;
					for (const int b : list)
					{
						const clip_brush& brush = brushes[static_cast<std::size_t>(b)];
						box3 part = brush.box;
						for (int k = 0; k < 3; k++)
						{
							part.mins[k] = std::max(part.mins[k], region.mins[k]);
							part.maxs[k] = std::min(part.maxs[k], region.maxs[k]);
						}
						leaf.box.add(part);
						leaf.contents |= brush.contents;
					}
					leaf.box.expand(LEAF_PAD);
					leafs.push_back(std::move(leaf));
					return -1 - static_cast<int>(leafs.size() - 1);
				}
				const int index = static_cast<int>(nodes.size());
				nodes.push_back({});
				nodes[static_cast<std::size_t>(index)].axis = best_axis;
				nodes[static_cast<std::size_t>(index)].dist = best_dist;
				std::vector<int> front, back;
				for (const int b : list)
				{
					const box3& box = brushes[static_cast<std::size_t>(b)].box;
					if (box.maxs[best_axis] > best_dist)
					{
						front.push_back(b);
					}
					if (box.mins[best_axis] < best_dist)
					{
						back.push_back(b);
					}
				}
				box3 front_region = region, back_region = region;
				front_region.mins[best_axis] = best_dist;
				back_region.maxs[best_axis] = best_dist;
				const int f = build(front, front_region, depth + 1);
				nodes[static_cast<std::size_t>(index)].child[0] = f;
				const int k = build(back, back_region, depth + 1);
				nodes[static_cast<std::size_t>(index)].child[1] = k;
				return index;
			}
		};

		clip_material_info material_info(const catalog& cat, const std::string& name, bool& known)
		{
			if (const auto it = cat.clip_materials.find(name); it != cat.clip_materials.end())
			{
				known = true;
				return it->second;
			}
			known = false;
			clip_material_info info;
			info.contents = 1;			// CONTENTS_SOLID
			return info;
		}
	}

	asset_builder build_clip_map(compile_context& ctx)
	{
		world& w = ctx.w;

		// ---- materials
		std::vector<std::string> material_names;
		std::vector<clip_material_info> material_infos;
		std::map<std::string, int> material_index;
		std::set<std::string> unknown;
		const auto material_of = [&](const std::string& name)
		{
			if (const auto it = material_index.find(name); it != material_index.end())
			{
				return it->second;
			}
			bool known = false;
			const clip_material_info info = material_info(ctx.cat, name, known);
			if (!known)
			{
				unknown.insert(name);
			}
			const int index = static_cast<int>(material_names.size());
			material_names.push_back(name);
			material_infos.push_back(info);
			material_index.emplace(name, index);
			return index;
		};

		// ---- brushes
		std::vector<clip_brush> brushes;
		for (std::size_t source = 0; source < w.brushes.size(); source++)
		{
			map_brush& mb = w.brushes[source];
			clip_brush b;
			b.source = static_cast<int>(source);
			b.box = mb.box;
			std::vector<int> side_of(mb.faces.size(), -1);
			int first_material = -1;
			std::int32_t contents = 0;
			for (std::size_t f = 0; f < mb.faces.size(); f++)
			{
				map_face& face = mb.faces[f];
				face.clip_material = material_of(face.material);
				contents |= material_infos[static_cast<std::size_t>(face.clip_material)].contents;
				first_material = first_material < 0 ? face.clip_material : first_material;
			}
			mb.contents = contents;
			mb.opaque = mb.model == 0 && !mb.detail && (contents & 1) != 0;
			if (!mb.collides || contents == 0)
			{
				mb.collides = false;
				continue;
			}
			b.contents = contents;
			for (std::size_t f = 0; f < mb.faces.size(); f++)
			{
				const map_face& face = mb.faces[f];
				int axis = -1;
				for (int k = 0; k < 3; k++)
				{
					if (std::fabs(std::fabs(face.plane.normal[k]) - 1.0) < 1e-6)
					{
						axis = k;
					}
				}
				if (axis >= 0)
				{
					const int positive = face.plane.normal[axis] > 0 ? 1 : 0;
					side_of[f] = 2 * axis + positive;
					b.axial_material[positive][axis] = static_cast<std::int16_t>(face.clip_material);
					continue;
				}
				side_of[f] = 6 + static_cast<int>(b.sides.size());
				clip_side s;
				const double n[3] = { face.plane.normal.x, face.plane.normal.y, face.plane.normal.z };
				s.plane = w.planes.add(n, face.plane.dist);
				s.material = face.clip_material;
				b.sides.push_back(std::move(s));
			}
			for (int positive = 0; positive < 2; positive++)
			{
				for (int axis = 0; axis < 3; axis++)
				{
					// an axial side without a face of its own ( the box only touches the brush there )
					if (std::find(side_of.begin(), side_of.end(), 2 * axis + positive) == side_of.end())
					{
						b.axial_material[positive][axis] = static_cast<std::int16_t>(first_material);
					}
				}
			}
			// edge lists: per face the sides across its edges, clockwise from outside ( the windings are counter clockwise )
			bool box_like = b.sides.empty();
			std::size_t edge_total = 0;
			bool broken = false;
			for (std::size_t f = 0; f < mb.faces.size() && !broken; f++)
			{
				const std::vector<int>& loop = mb.faces[f].corner;
				std::vector<std::uint8_t> adjacent;
				for (std::size_t k = loop.size(); k-- > 0;)
				{
					const int v0 = loop[(k + 1) % loop.size()], v1 = loop[k];
					int other = -1;
					for (std::size_t g = 0; g < mb.faces.size() && other < 0; g++)
					{
						if (g == f)
						{
							continue;
						}
						const std::vector<int>& theirs = mb.faces[g].corner;
						if (std::find(theirs.begin(), theirs.end(), v0) != theirs.end() && std::find(theirs.begin(), theirs.end(), v1) != theirs.end())
						{
							other = side_of[g];
						}
					}
					if (other < 0)
					{
						broken = true;
						break;
					}
					adjacent.push_back(static_cast<std::uint8_t>(other));
				}
				if (broken)
				{
					break;
				}
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
			if (broken || edge_total > 255 || b.sides.size() > 250)
			{
				ctx.rep.warnings.push_back(format("collision: brush %u has %s and is left out of the collision", mb.id,
					broken ? "a face with an edge no other face shares" : "more edges than a cbrush_t can describe ( 255 )"));
				mb.collides = false;
				continue;
			}
			if (box_like)
			{
				for (int positive = 0; positive < 2; positive++)
				{
					for (int axis = 0; axis < 3; axis++)
					{
						b.axial_adjacent[positive][axis].assign(BOX_ADJACENT[positive][axis], BOX_ADJACENT[positive][axis] + 4);
					}
				}
			}
			mb.clip_index = static_cast<int>(brushes.size());
			brushes.push_back(std::move(b));
		}
		if (brushes.size() > 0xFFFF)
		{
			throw compile_error(format("the map has %zu collision brushes; a clip map holds 65535", brushes.size()));
		}
		for (const std::string& name : unknown)
		{
			ctx.rep.warnings.push_back("collision: no shipped map uses '" + name + "' on a brush; its sides are solid without a surface type");
		}

		// ---- BSP over the world's brushes
		builder tree{ brushes };
		tree.leafs.push_back({});
		std::vector<int> world_list;
		for (std::size_t b = 0; b < brushes.size(); b++)
		{
			if (w.brushes[static_cast<std::size_t>(brushes[b].source)].model == 0)
			{
				world_list.push_back(static_cast<int>(b));
			}
		}
		box3 region = w.bounds;
		region.expand(64.0f);
		int root = tree.build(world_list, region, 0);
		if (root < 0)
		{
			// a tree needs a node: one split above everything, the brushes below it
			tree_node node;
			node.axis = 2;
			node.dist = region.maxs[2];
			node.child[0] = -1;
			node.child[1] = root;
			tree.nodes.insert(tree.nodes.begin(), node);
		}
		if (tree.nodes.size() > 32767 || tree.leafs.size() > 32767)
		{
			throw compile_error("the collision tree has more than 32767 nodes or leafs");
		}

		// ---- leaf brush nodes: node 0 is reserved, then one list per leaf and per brush model
		std::vector<c1::cLeafBrushNode_s> brush_nodes(1);
		std::vector<std::uint16_t> leaf_brushes;
		std::vector<std::uint32_t> list_start;		// per brush node: offset of its list in leaf_brushes
		list_start.push_back(0);
		const auto add_list = [&](const std::vector<int>& list, const std::int32_t contents)
		{
			c1::cLeafBrushNode_s node{};
			node.leafBrushCount = static_cast<std::int16_t>(list.size());
			node.contents = contents;
			list_start.push_back(static_cast<std::uint32_t>(leaf_brushes.size()));
			for (const int b : list)
			{
				leaf_brushes.push_back(static_cast<std::uint16_t>(b));
			}
			brush_nodes.push_back(node);
			return static_cast<std::int32_t>(brush_nodes.size() - 1);
		};
		std::vector<c1::cLeaf_t> leafs(tree.leafs.size());
		for (std::size_t l = 1; l < tree.leafs.size(); l++)
		{
			const tree_leaf& leaf = tree.leafs[l];
			if (leaf.brushes.size() > 32767)
			{
				throw compile_error("a collision leaf holds more than 32767 brushes");
			}
			leafs[l].brushContents = leaf.contents;
			leafs[l].bounds = leaf.box.bounds();
			leafs[l].leafBrushNode = add_list(leaf.brushes, leaf.contents);
		}

		// ---- sub models
		std::vector<c1::cmodel_t> models(static_cast<std::size_t>(w.model_count));
		{
			box3 all = w.bounds;
			all.expand(MODEL_PAD);
			models[0].bounds = all.bounds();
			models[0].radius = all.radius();
		}
		for (int m = 1; m < w.model_count; m++)
		{
			std::vector<int> list;
			box3 box;
			std::int32_t contents = 0;
			for (std::size_t b = 0; b < brushes.size(); b++)
			{
				if (w.brushes[static_cast<std::size_t>(brushes[b].source)].model == m)
				{
					list.push_back(static_cast<int>(b));
					box.add(brushes[b].box);
					contents |= brushes[b].contents;
				}
			}
			c1::cmodel_t& model = models[static_cast<std::size_t>(m)];
			if (list.empty())
			{
				// a brush model without collision ( all its brushes are non colliding ): a point at its brushes' centre
				for (const map_brush& mb : w.brushes)
				{
					if (mb.model == m)
					{
						box.add(mb.box);
					}
				}
				model.bounds = box.bounds();
				model.radius = box.radius();
				continue;
			}
			box3 leaf_box = box, model_box = box;
			leaf_box.expand(LEAF_PAD);
			model_box.expand(MODEL_PAD);
			model.bounds = model_box.bounds();
			model.radius = model_box.radius();
			model.leaf.brushContents = contents;
			model.leaf.bounds = leaf_box.bounds();
			model.leaf.leafBrushNode = add_list(list, contents);
		}

		// ---- the asset, in the order Load_clipMap_t reads it
		asset_builder b;
		c1::clipMap_t header{};
		header.isInUse = 1;
		header.planeCount = static_cast<std::int32_t>(w.planes.planes.size());
		header.numMaterials = static_cast<std::uint32_t>(material_names.size());
		header.numNodes = static_cast<std::uint32_t>(tree.nodes.size());
		header.numLeafs = static_cast<std::uint32_t>(leafs.size());
		header.leafbrushNodesCount = static_cast<std::uint32_t>(brush_nodes.size());
		header.numLeafBrushes = static_cast<std::uint32_t>(leaf_brushes.size());
		header.numSubModels = static_cast<std::uint32_t>(models.size());
		header.numBrushes = static_cast<std::uint16_t>(brushes.size());
		header.smodelNodeCount = 1;
		header.checksum = w.checksum;
		const std::uint32_t head = b.add(header);
		b.point(head, offsetof(c1::clipMap_t, name), 0, 0, REF_COM);
		b.point(head, offsetof(c1::clipMap_t, planes), 0, 0, REF_GFX);

		// materials
		{
			std::vector<c1::ClipMaterial> list(material_names.size());
			for (std::size_t m = 0; m < list.size(); m++)
			{
				list[m].name = FOLLOWS;
				list[m].surfaceFlags = material_infos[m].surface_flags;
				list[m].contents = material_infos[m].contents;
			}
			b.follows(head, offsetof(c1::clipMap_t, materials));
			b.add_array(list);
			for (const std::string& name : material_names)
			{
				b.add_string(name);
			}
		}

		// brush sides and edges: the runs of the brushes, in brush order
		std::vector<c1::cbrushside_t> sides;
		std::vector<std::uint32_t> side_plane;
		std::vector<std::uint8_t> edges;
		std::vector<c1::cbrush_t> packed(brushes.size());
		std::vector<std::uint32_t> first_side(brushes.size()), first_edge(brushes.size());
		std::vector<c1::Bounds> brush_bounds(brushes.size());
		std::vector<std::int32_t> brush_contents(brushes.size());
		for (std::size_t n = 0; n < brushes.size(); n++)
		{
			const clip_brush& cb = brushes[n];
			c1::cbrush_t& p = packed[n];
			p.numsides = static_cast<std::uint16_t>(cb.sides.size());
			first_side[n] = static_cast<std::uint32_t>(sides.size());
			first_edge[n] = static_cast<std::uint32_t>(edges.size());
			std::uint32_t run = 0;
			for (int axis = 0; axis < 3; axis++)
			{
				for (int positive = 0; positive < 2; positive++)
				{
					p.axialMaterialNum[positive][axis] = cb.axial_material[positive][axis];
					p.firstAdjacentSideOffsets[positive][axis] = static_cast<std::uint8_t>(run);
					p.edgeCount[positive][axis] = static_cast<std::uint8_t>(cb.axial_adjacent[positive][axis].size());
					edges.insert(edges.end(), cb.axial_adjacent[positive][axis].begin(), cb.axial_adjacent[positive][axis].end());
					run += static_cast<std::uint32_t>(cb.axial_adjacent[positive][axis].size());
				}
			}
			for (const clip_side& s : cb.sides)
			{
				c1::cbrushside_t side{};
				side.materialNum = static_cast<std::uint16_t>(s.material);
				side.firstAdjacentSideOffset = static_cast<std::uint8_t>(run);
				side.edgeCount = static_cast<std::uint8_t>(s.adjacent.size());
				edges.insert(edges.end(), s.adjacent.begin(), s.adjacent.end());
				run += static_cast<std::uint32_t>(s.adjacent.size());
				sides.push_back(side);
				side_plane.push_back(s.plane);
			}
			brush_bounds[n] = cb.box.bounds();
			brush_contents[n] = cb.contents;
		}
		std::uint32_t sides_chunk = 0, edges_chunk = 0;
		if (!sides.empty())
		{
			header.numBrushSides = static_cast<std::uint32_t>(sides.size());
			b.follows(head, offsetof(c1::clipMap_t, brushsides));
			sides_chunk = b.add_array(sides);
			for (std::size_t s = 0; s < sides.size(); s++)
			{
				b.point(sides_chunk, s * sizeof(c1::cbrushside_t), 0, 20 * side_plane[s], REF_GFX);
			}
		}
		if (!edges.empty())
		{
			header.numBrushEdges = static_cast<std::uint32_t>(edges.size());
			b.follows(head, offsetof(c1::clipMap_t, brushEdges));
			edges_chunk = b.add_array(edges);
		}

		// nodes
		{
			std::vector<cnode> list(tree.nodes.size());
			std::vector<std::uint32_t> plane(tree.nodes.size());
			for (std::size_t n = 0; n < list.size(); n++)
			{
				double normal[3] = { 0, 0, 0 };
				normal[tree.nodes[n].axis] = 1.0;
				plane[n] = w.planes.add(normal, tree.nodes[n].dist);
				for (int side = 0; side < 2; side++)
				{
					int child = tree.nodes[n].child[side];
					if (root < 0 && child >= 0)
					{
						child++;			// the node put in front of a tree that was a single leaf
					}
					list[n].children[side] = static_cast<std::int16_t>(child);
				}
			}
			b.follows(head, offsetof(c1::clipMap_t, nodes));
			const std::uint32_t chunk = b.add_array(list);
			for (std::size_t n = 0; n < list.size(); n++)
			{
				b.point(chunk, n * sizeof(cnode), 0, 20 * plane[n], REF_GFX);
			}
		}
		b.follows(head, offsetof(c1::clipMap_t, leafs));
		b.add_array(leafs);

		// leaf brushes, then the nodes that list them
		std::uint32_t leaf_brushes_chunk = 0;
		if (!leaf_brushes.empty())
		{
			b.follows(head, offsetof(c1::clipMap_t, leafbrushes));
			leaf_brushes_chunk = b.add_array(leaf_brushes);
		}
		{
			b.follows(head, offsetof(c1::clipMap_t, leafbrushNodes));
			const std::uint32_t chunk = b.add_array(brush_nodes);
			for (std::size_t n = 1; n < brush_nodes.size(); n++)
			{
				if (brush_nodes[n].leafBrushCount > 0)
				{
					b.point(chunk, n * sizeof(c1::cLeafBrushNode_s) + offsetof(c1::cLeafBrushNode_s, data), leaf_brushes_chunk, 2 * list_start[n]);
				}
			}
		}

		b.follows(head, offsetof(c1::clipMap_t, cmodels));
		b.add_array(models);

		if (!brushes.empty())
		{
			b.follows(head, offsetof(c1::clipMap_t, brushes));
			const std::uint32_t chunk = b.add_array(packed);
			for (std::size_t n = 0; n < brushes.size(); n++)
			{
				if (packed[n].numsides)
				{
					b.point(chunk, n * sizeof(c1::cbrush_t) + offsetof(c1::cbrush_t, sides), sides_chunk, 8 * first_side[n]);
				}
				std::uint32_t total = 0;
				for (int k = 0; k < 6; k++)
				{
					total += packed[n].edgeCount[k & 1][k >> 1];
				}
				for (std::uint32_t s = 0; s < packed[n].numsides; s++)
				{
					total += sides[first_side[n] + s].edgeCount;
				}
				if (total)
				{
					b.point(chunk, n * sizeof(c1::cbrush_t) + offsetof(c1::cbrush_t, baseAdjacentSide), edges_chunk, first_edge[n]);
				}
			}
			b.follows(head, offsetof(c1::clipMap_t, brushBounds));
			b.add_array(brush_bounds);
			b.follows(head, offsetof(c1::clipMap_t, brushContents));
			b.add_array(brush_contents);
		}

		// the static model tree: one node over no models ( the root is read whatever the count )
		{
			smodel_node node{};
			box3 all = w.bounds;
			node.bounds = all.bounds();
			b.follows(head, offsetof(c1::clipMap_t, smodelNodes));
			b.add(node);
		}

		// MapEnts, nested
		{
			const std::string ents = build_entity_string(w);
			c1::MapEnts me{};
			me.entityString = FOLLOWS;
			me.numEntityChars = static_cast<std::int32_t>(ents.size() + 1);
			me.stages = FOLLOWS;
			me.stageCount = 1;
			b.follows(head, offsetof(c1::clipMap_t, mapEnts));
			const std::uint32_t chunk = b.add(me);
			b.point(chunk, offsetof(c1::MapEnts, name), 0, 0, REF_COM);
			b.add(ents.c_str(), ents.size() + 1);
			// one stage, as every map has: no trigger ( 1024 ), the sun
			map_stage stage{};
			stage.name = FOLLOWS;
			stage.triggerIndex = 0x400;
			stage.sunPrimaryLightIndex = 1;
			b.add(stage);
			b.add_string("stage 0");
		}

		// counts that were set after the header was added
		b.put(head, offsetof(c1::clipMap_t, numBrushSides), header.numBrushSides);
		b.put(head, offsetof(c1::clipMap_t, numBrushEdges), header.numBrushEdges);

		ctx.rep.clip_brushes = brushes.size();
		ctx.rep.clip_sides = sides.size();
		ctx.rep.clip_nodes = tree.nodes.size();
		ctx.rep.clip_leafs = leafs.size();
		ctx.rep.leaf_brush_nodes = brush_nodes.size();
		ctx.rep.clip_materials = material_names.size();
		ctx.rep.sub_models = models.size();
		return b;
	}
}
