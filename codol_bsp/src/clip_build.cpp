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
			const std::vector<box3>& boxes;		// of the items: the collision brushes, then the terrain partitions
			const std::vector<std::int32_t>& contents;
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
							for (const float v : { boxes[static_cast<std::size_t>(b)].mins[axis], boxes[static_cast<std::size_t>(b)].maxs[axis] })
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
								const box3& box = boxes[static_cast<std::size_t>(b)];
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
						const box3& item_box = boxes[static_cast<std::size_t>(b)];
						box3 part = item_box;
						for (int k = 0; k < 3; k++)
						{
							part.mins[k] = std::max(part.mins[k], region.mins[k]);
							part.maxs[k] = std::min(part.maxs[k], region.maxs[k]);
						}
						leaf.box.add(part);
						leaf.contents |= contents[static_cast<std::size_t>(b)];
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
					const box3& box = boxes[static_cast<std::size_t>(b)];
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

		// ---- static models: the xmodels are carried into the zone here, in front of the map assets
		for (map_model& model : w.models)
		{
			model_desc desc;
			std::string why;
			model.ref = ctx.importer.model(model.name, &why, &desc);
			if (!model.ref.valid())
			{
				ctx.rep.warnings.push_back("static model '" + model.name + "' is left out: " + why);
				continue;
			}
			model.collides = desc.coll_surfs > 0;
			model.last_lod_dist = desc.last_lod_dist;
			for (std::size_t k = 0; k + 2 < desc.triangles.size(); k += 3)
			{
				vec3 p{ model.origin[0], model.origin[1], model.origin[2] };
				for (int a = 0; a < 3; a++)
				{
					const double local = static_cast<double>(desc.triangles[k + static_cast<std::size_t>(a)]) * model.scale;
					p = p + vec3{ model.axis[a][0], model.axis[a][1], model.axis[a][2] } * local;
				}
				model.triangles.push_back(p);
			}
			// the model's box, placed
			for (int corner = 0; corner < 8; corner++)
			{
				float p[3] = { model.origin[0], model.origin[1], model.origin[2] };
				for (int a = 0; a < 3; a++)
				{
					const float local = (desc.mid[a] + (corner & (1 << a) ? desc.half[a] : -desc.half[a])) * model.scale;
					for (int k = 0; k < 3; k++)
					{
						p[k] += model.axis[a][k] * local;
					}
				}
				model.box.add(p);
			}
		}

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
			clip_material_info info = material_info(ctx.cat, name, known);
			if (!known && !is_tool_material(name))
			{
				// a material no shipped map has on a brush ( a modder's own ): what its definition says - the surface type
				// as the shipped clip materials carry it ( type number << 20 ), solid unless it is blended ( a decal )
				const material_desc& desc = ctx.importer.describe("wc/" + name);
				if (desc.ref.valid())
				{
					int type = 0;
					for (int bit = 0; bit < 32 && !type; bit++)
					{
						type = desc.surface_type_bits & (1u << bit) ? bit + 1 : 0;
					}
					info.surface_flags = type << 20;
					info.contents = desc.see_through && desc.blend ? 0 : 1;
					known = true;
					ctx.rep.notes.push_back(format("collision: '%s' is on no brush of a shipped map; from its material: surface type %d, %s", name.c_str(), type,
						info.contents ? "solid" : "does not collide ( a decal )"));
				}
			}
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
			// a brush that can be seen through ( an alpha tested or blended material on a face ) hides nothing and
			// splits no cell, whatever its flags
			for (std::size_t f = 0; f < mb.faces.size() && mb.opaque; f++)
			{
				if (!is_tool_material(mb.faces[f].material))
				{
					const material_desc& desc = ctx.importer.describe("wc/" + mb.faces[f].material);
					mb.opaque = !(desc.ref.valid() && desc.see_through);
				}
			}
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

		// ---- terrain: the triangles of the patches, as the clip map stores them ( verts, triIndices, partitions with
		// their borders, AABB trees per leaf ). Shapes and rules read off the shipped maps with codol_bsp_probe terrain:
		//   * a triangle's corners are stored so that cross( v1 - v0, v2 - v0 ) points away from the side one stands on;
		//   * a partition is a group of triangles of one material that lie together ( here: up to 16 );
		//   * a border is an edge two triangles share whose surface does not go on beyond it seen from above ( a
		//     silhouette edge: the brink of a ledge, the fold of an overhang ) - open edges have none ( 177 of 20572 in mp_rust ) -
		//     as a line in the xy plane: normal pointing away from the surface, the edge's start along it, its
		//     length, the height at the start and the slope;
		//   * triEdgeIsWalkable: three bits per triangle, set for the triangles one can stand on ( normal z >= 0.7 ).
		struct terrain_tri
		{
			std::uint32_t v[3];
			float normal[3];
			int partition = 0;
		};
		struct terrain_partition
		{
			std::vector<std::uint32_t> tris;
			box3 box;
			int material = 0;
			std::uint32_t first_tri = 0, first_border = 0, border_count = 0;
		};
		struct terrain_border
		{
			float eq[3], z_base, z_slope, start, length;
		};
		static_assert(sizeof(terrain_border) == 28);
		std::vector<std::array<float, 3>> terrain_verts;
		std::vector<terrain_tri> terrain_tris;
		std::vector<terrain_partition> partitions;
		std::vector<terrain_border> borders;
		std::vector<std::uint32_t> walkable;
		{
			std::map<std::array<float, 3>, std::uint32_t> vertex_index;
			const auto vertex_of = [&](const edit::mesh_vertex& v)
			{
				const std::array<float, 3> key = { static_cast<float>(v.position[0]), static_cast<float>(v.position[1]), static_cast<float>(v.position[2]) };
				const auto [it, added] = vertex_index.emplace(key, static_cast<std::uint32_t>(terrain_verts.size()));
				if (added)
				{
					terrain_verts.push_back(key);
				}
				return it->second;
			};
			std::vector<terrain_tri> unordered;
			std::vector<std::pair<int, std::vector<std::uint32_t>>> groups;		// material, triangles: one per patch, then split
			for (const map_patch& patch : w.patches)
			{
				const int material = material_of(patch.material);
				if (material_infos[static_cast<std::size_t>(material)].contents == 0)
				{
					continue;			// a decal, a non colliding material
				}
				std::vector<std::uint32_t> list;
				for (std::size_t t = 0; t + 2 < patch.indices.size(); t += 3)
				{
					const edit::mesh_vertex& a = patch.vertices[patch.indices[t]];
					const edit::mesh_vertex& b = patch.vertices[patch.indices[t + 1]];
					const edit::mesh_vertex& c = patch.vertices[patch.indices[t + 2]];
					const vec3 pa{ a.position[0], a.position[1], a.position[2] }, pb{ b.position[0], b.position[1], b.position[2] }, pc{ c.position[0], c.position[1], c.position[2] };
					vec3 normal = cross(pb - pa, pc - pa);		// the patch's indices run counter clockwise seen from the visible side
					const double area = length(normal);
					if (area < 0.01)
					{
						continue;
					}
					normal = normal * (1.0 / area);
					terrain_tri tri;
					tri.v[0] = vertex_of(a);
					tri.v[1] = vertex_of(c);
					tri.v[2] = vertex_of(b);
					if (tri.v[0] == tri.v[1] || tri.v[1] == tri.v[2] || tri.v[0] == tri.v[2])
					{
						continue;
					}
					tri.normal[0] = static_cast<float>(normal.x);
					tri.normal[1] = static_cast<float>(normal.y);
					tri.normal[2] = static_cast<float>(normal.z);
					list.push_back(static_cast<std::uint32_t>(unordered.size()));
					unordered.push_back(tri);
				}
				if (!list.empty())
				{
					groups.emplace_back(material, std::move(list));
				}
			}
			if (terrain_verts.size() > 0xFFFF)
			{
				throw compile_error(format("the patches have %zu collision vertices; this compiler writes one vertex block ( 65535 )", terrain_verts.size()));
			}
			const auto centre = [&](const std::uint32_t t, const int axis)
			{
				const terrain_tri& tri = unordered[t];
				return terrain_verts[tri.v[0]][static_cast<std::size_t>(axis)] + terrain_verts[tri.v[1]][static_cast<std::size_t>(axis)] + terrain_verts[tri.v[2]][static_cast<std::size_t>(axis)];
			};
			constexpr std::size_t PARTITION_TRIS = 16;
			while (!groups.empty())
			{
				auto [material, list] = std::move(groups.back());
				groups.pop_back();
				box3 box;
				for (const std::uint32_t t : list)
				{
					for (const std::uint32_t v : unordered[t].v)
					{
						box.add(terrain_verts[v].data());
					}
				}
				if (list.size() > PARTITION_TRIS)
				{
					int axis = 0;
					for (int k = 1; k < 3; k++)
					{
						if (box.maxs[k] - box.mins[k] > box.maxs[axis] - box.mins[axis])
						{
							axis = k;
						}
					}
					std::sort(list.begin(), list.end(), [&](const std::uint32_t x, const std::uint32_t y)
					{
						const float cx = centre(x, axis), cy = centre(y, axis);
						return cx != cy ? cx < cy : x < y;
					});
					const std::size_t half = list.size() / 2;
					groups.emplace_back(material, std::vector<std::uint32_t>(list.begin(), list.begin() + static_cast<std::ptrdiff_t>(half)));
					groups.emplace_back(material, std::vector<std::uint32_t>(list.begin() + static_cast<std::ptrdiff_t>(half), list.end()));
					continue;
				}
				terrain_partition p;
				p.material = material;
				p.box = box;
				p.first_tri = static_cast<std::uint32_t>(terrain_tris.size());
				for (const std::uint32_t t : list)
				{
					p.tris.push_back(static_cast<std::uint32_t>(terrain_tris.size()));
					terrain_tris.push_back(unordered[t]);
					terrain_tris.back().partition = static_cast<int>(partitions.size());
				}
				partitions.push_back(std::move(p));
			}
			// borders
			std::map<std::pair<std::uint32_t, std::uint32_t>, std::vector<std::pair<std::uint32_t, std::uint32_t>>> edges;		// -> ( triangle, third vertex )
			for (std::uint32_t t = 0; t < terrain_tris.size(); t++)
			{
				for (int c = 0; c < 3; c++)
				{
					const std::uint32_t x = terrain_tris[t].v[c], y = terrain_tris[t].v[(c + 1) % 3];
					edges[{ std::min(x, y), std::max(x, y) }].emplace_back(t, terrain_tris[t].v[(c + 2) % 3]);
				}
			}
			std::vector<std::vector<terrain_border>> per_partition(partitions.size());
			for (const auto& [edge, users] : edges)
			{
				if (users.size() < 2)
				{
					continue;
				}
				const std::array<float, 3>& a = terrain_verts[edge.first];
				const std::array<float, 3>& b = terrain_verts[edge.second];
				const double dx = static_cast<double>(b[0]) - a[0], dy = static_cast<double>(b[1]) - a[1], l = std::sqrt(dx * dx + dy * dy);
				if (l < 0.01)
				{
					continue;
				}
				double n[2] = { -dy / l, dx / l };
				const double d = n[0] * a[0] + n[1] * a[1];
				double low = 0, high = 0;
				int inner = -1;
				for (const auto& [t, third] : users)
				{
					const double side = n[0] * terrain_verts[third][0] + n[1] * terrain_verts[third][1] - d;
					low = std::min(low, side);
					high = std::max(high, side);
					if (std::fabs(side) > 0.01 && inner < 0)
					{
						inner = static_cast<int>(t);
					}
				}
				if (inner < 0 || (low < -0.01 && high > 0.01) || std::getenv("CODOL_BSP_NO_BORDERS"))
				{
					continue;			// a wall's own diagonal, or the surface goes on beyond the edge ( the variable: an experiment )
				}
				if (high > 0.01)
				{
					n[0] = -n[0];
					n[1] = -n[1];
				}
				terrain_border e;
				e.eq[0] = static_cast<float>(n[0]);
				e.eq[1] = static_cast<float>(n[1]);
				e.eq[2] = static_cast<float>(n[0] * a[0] + n[1] * a[1]);
				const double sa = n[1] * a[0] - n[0] * a[1], sb = n[1] * b[0] - n[0] * b[1];
				const std::array<float, 3>& first = sa <= sb ? a : b;
				const std::array<float, 3>& second = sa <= sb ? b : a;
				e.start = static_cast<float>(std::min(sa, sb));
				e.length = static_cast<float>(std::fabs(sb - sa));
				e.z_base = first[2];
				e.z_slope = static_cast<float>((static_cast<double>(second[2]) - first[2]) / std::max(1e-6, std::fabs(sb - sa)));
				std::vector<terrain_border>& list = per_partition[static_cast<std::size_t>(terrain_tris[static_cast<std::size_t>(inner)].partition)];
				if (list.size() < 255)
				{
					list.push_back(e);
				}
			}
			for (std::size_t p = 0; p < partitions.size(); p++)
			{
				partitions[p].first_border = static_cast<std::uint32_t>(borders.size());
				partitions[p].border_count = static_cast<std::uint32_t>(per_partition[p].size());
				borders.insert(borders.end(), per_partition[p].begin(), per_partition[p].end());
			}
			walkable.assign((terrain_tris.size() * 3 + 31) / 32, 0);
			for (std::size_t t = 0; t < terrain_tris.size(); t++)
			{
				if (terrain_tris[t].normal[2] >= 0.7f)
				{
					for (std::size_t bit = 3 * t; bit < 3 * t + 3; bit++)
					{
						walkable[bit >> 5] |= 1u << (bit & 31);
					}
				}
			}
			if (!terrain_tris.empty())
			{
				ctx.rep.notes.push_back(format("collision: terrain of %zu patches: %zu triangles, %zu vertices, %zu partitions, %zu borders", w.patches.size(), terrain_tris.size(),
					terrain_verts.size(), partitions.size(), borders.size()));
			}
		}

		// ---- BSP over the world's brushes and terrain partitions
		std::vector<box3> item_boxes;
		std::vector<std::int32_t> item_contents;
		for (const clip_brush& cb : brushes)
		{
			item_boxes.push_back(cb.box);
			item_contents.push_back(cb.contents);
		}
		for (const terrain_partition& p : partitions)
		{
			// a little room around a partition: a flat one ( a level piece of terrain ) has no thickness, and an item that
			// lies in a split plane would be on neither side of it
			box3 padded = p.box;
			padded.expand(LEAF_PAD);
			item_boxes.push_back(padded);
			item_contents.push_back(material_infos[static_cast<std::size_t>(p.material)].contents);
		}
		builder tree{ item_boxes, item_contents };
		tree.leafs.push_back({});
		std::vector<int> world_list;
		for (std::size_t b = 0; b < brushes.size(); b++)
		{
			if (w.brushes[static_cast<std::size_t>(brushes[b].source)].model == 0)
			{
				world_list.push_back(static_cast<int>(b));
			}
		}
		for (std::size_t p = 0; p < partitions.size(); p++)
		{
			world_list.push_back(static_cast<int>(brushes.size() + p));
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
		// a leaf's terrain: per material one AABB tree whose leafs name partitions; the roots of a leaf follow each other
		struct aabb_node
		{
			float origin[3];
			std::uint16_t material, child_count;
			float half[3];
			std::uint32_t index;			// first child, or the partition
		};
		static_assert(sizeof(aabb_node) == 32);
		std::vector<aabb_node> aabbs;
		const auto fill_aabb = [&](const auto& self, const std::size_t node, std::vector<int> list, const int material) -> void
		{
			box3 box;
			for (const int p : list)
			{
				box.add(partitions[static_cast<std::size_t>(p)].box);
			}
			const c1::Bounds bounds = box.bounds();
			std::memcpy(aabbs[node].origin, bounds.midPoint, 12);
			std::memcpy(aabbs[node].half, bounds.halfSize, 12);
			aabbs[node].material = static_cast<std::uint16_t>(material);
			if (list.size() == 1)
			{
				aabbs[node].child_count = 0;
				aabbs[node].index = static_cast<std::uint32_t>(list[0]);
				return;
			}
			// up to 15 children ( the shipped trees have 2 .. 15 ); more partitions: runs along the longest axis
			int axis = 0;
			for (int k = 1; k < 3; k++)
			{
				if (box.maxs[k] - box.mins[k] > box.maxs[axis] - box.mins[axis])
				{
					axis = k;
				}
			}
			std::sort(list.begin(), list.end(), [&](const int x, const int y)
			{
				const box3& bx = partitions[static_cast<std::size_t>(x)].box;
				const box3& by = partitions[static_cast<std::size_t>(y)].box;
				const float cx = bx.mins[axis] + bx.maxs[axis], cy = by.mins[axis] + by.maxs[axis];
				return cx != cy ? cx < cy : x < y;
			});
			const std::size_t children = std::min<std::size_t>(15, list.size()), per = (list.size() + children - 1) / children;
			std::vector<std::vector<int>> runs;
			for (std::size_t at = 0; at < list.size(); at += per)
			{
				runs.emplace_back(list.begin() + static_cast<std::ptrdiff_t>(at), list.begin() + static_cast<std::ptrdiff_t>(std::min(list.size(), at + per)));
			}
			const std::size_t first = aabbs.size();
			aabbs.resize(first + runs.size());
			aabbs[node].child_count = static_cast<std::uint16_t>(runs.size());
			aabbs[node].index = static_cast<std::uint32_t>(first);
			for (std::size_t r = 0; r < runs.size(); r++)
			{
				self(self, first + r, std::move(runs[r]), material);
			}
		};
		std::vector<c1::cLeaf_t> leafs(tree.leafs.size());
		for (std::size_t l = 1; l < tree.leafs.size(); l++)
		{
			const tree_leaf& leaf = tree.leafs[l];
			std::vector<int> leaf_brush_list;
			std::map<int, std::vector<int>> leaf_terrain;		// material -> partitions
			std::int32_t brush_contents_of_leaf = 0, terrain_contents = 0;
			for (const int item : leaf.brushes)
			{
				if (static_cast<std::size_t>(item) < brushes.size())
				{
					leaf_brush_list.push_back(item);
					brush_contents_of_leaf |= brushes[static_cast<std::size_t>(item)].contents;
				}
				else
				{
					const int p = item - static_cast<int>(brushes.size());
					leaf_terrain[partitions[static_cast<std::size_t>(p)].material].push_back(p);
					terrain_contents |= material_infos[static_cast<std::size_t>(partitions[static_cast<std::size_t>(p)].material)].contents;
				}
			}
			if (leaf_brush_list.size() > 32767)
			{
				throw compile_error("a collision leaf holds more than 32767 brushes");
			}
			leafs[l].brushContents = brush_contents_of_leaf;
			leafs[l].bounds = leaf.box.bounds();
			// a leaf without brushes names node 0 and has no brush contents, so the list is never looked at
			leafs[l].leafBrushNode = leaf_brush_list.empty() ? 0 : add_list(leaf_brush_list, brush_contents_of_leaf);
			if (!leaf_terrain.empty())
			{
				const std::size_t first_root = aabbs.size();
				if (first_root + leaf_terrain.size() > 0xFFFF)
				{
					throw compile_error("the terrain has more than 65535 collision tree roots");
				}
				aabbs.resize(first_root + leaf_terrain.size());
				std::size_t root_at = first_root;
				for (auto& [material, list] : leaf_terrain)
				{
					fill_aabb(fill_aabb, root_at++, std::move(list), material);
				}
				leafs[l].firstCollAabbIndex = static_cast<std::uint16_t>(first_root);
				leafs[l].collAabbCount = static_cast<std::uint16_t>(leaf_terrain.size());
				leafs[l].terrainContents = terrain_contents;
			}
		}
		if (!aabbs.empty())
		{
			ctx.rep.notes.push_back(format("collision: %zu terrain tree nodes", aabbs.size()));
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

		// static models with collision, sorted along x so that the runs of the tree's leafs are near each other
		std::vector<int> colliding;
		for (std::size_t m = 0; m < w.models.size(); m++)
		{
			if (w.models[m].ref.valid() && w.models[m].collides)
			{
				colliding.push_back(static_cast<int>(m));
			}
		}
		std::sort(colliding.begin(), colliding.end(), [&](const int x, const int y)
		{
			return w.models[static_cast<std::size_t>(x)].origin[0] < w.models[static_cast<std::size_t>(y)].origin[0];
		});
		if (colliding.size() > 0xFFF0)
		{
			throw compile_error("more than 65520 static models with collision");
		}
		if (!colliding.empty())
		{
			std::vector<c1::cStaticModel_s> list(colliding.size());
			for (std::size_t k = 0; k < list.size(); k++)
			{
				const map_model& model = w.models[static_cast<std::size_t>(colliding[k])];
				std::memcpy(list[k].origin, model.origin, sizeof(model.origin));
				for (int i = 0; i < 3; i++)
				{
					for (int j = 0; j < 3; j++)
					{
						// the inverse of the scaled axis: transpose( axis ) / scale. Every clip entry of the shipped maps
						// checked is that ( codol_bsp_probe smodels ), and the engine's point trace reads it so.
						list[k].invScaledAxis[i][j] = model.axis[j][i] / model.scale;
					}
				}
				list[k].absBounds = model.box.bounds();
			}
			b.put(head, offsetof(c1::clipMap_t, numStaticModels), static_cast<std::uint32_t>(list.size()));
			b.follows(head, offsetof(c1::clipMap_t, staticModelList));
			const std::uint32_t chunk = b.add_array(list);
			for (std::size_t k = 0; k < list.size(); k++)
			{
				b.point_asset(chunk, k * sizeof(c1::cStaticModel_s), w.models[static_cast<std::size_t>(colliding[k])].ref.asset);
			}
		}

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

		// terrain: vertices, triangle indices ( relative to vertex block 0 ), the walkable bits, borders, partitions
		// ( their border pointers lead into the border array, in running order ), AABB trees
		if (!terrain_tris.empty())
		{
			b.put(head, offsetof(c1::clipMap_t, vertCount), static_cast<std::uint32_t>(terrain_verts.size()));
			b.follows(head, offsetof(c1::clipMap_t, verts));
			b.add_array(terrain_verts);
			std::vector<std::uint16_t> indices;
			for (const terrain_tri& t : terrain_tris)
			{
				for (const std::uint32_t v : t.v)
				{
					indices.push_back(static_cast<std::uint16_t>(v));
				}
			}
			b.put(head, offsetof(c1::clipMap_t, triCount), static_cast<std::int32_t>(terrain_tris.size()));
			b.follows(head, offsetof(c1::clipMap_t, triIndices));
			b.add_array(indices);
			b.follows(head, offsetof(c1::clipMap_t, triEdgeIsWalkable));
			b.add_array(walkable);
			std::uint32_t borders_chunk = 0;
			if (!borders.empty())
			{
				b.put(head, offsetof(c1::clipMap_t, borderCount), static_cast<std::int32_t>(borders.size()));
				b.follows(head, offsetof(c1::clipMap_t, borders));
				borders_chunk = b.add_array(borders);
			}
			struct packed_partition
			{
				std::uint8_t tri_count, border_count, vert_block, pad;
				std::uint32_t first_tri;
				c1::zptr borders;
			};
			static_assert(sizeof(packed_partition) == 12);
			std::vector<packed_partition> packed_partitions(partitions.size());
			for (std::size_t p = 0; p < partitions.size(); p++)
			{
				packed_partitions[p].tri_count = static_cast<std::uint8_t>(partitions[p].tris.size());
				packed_partitions[p].border_count = static_cast<std::uint8_t>(partitions[p].border_count);
				packed_partitions[p].first_tri = partitions[p].first_tri;
			}
			b.put(head, offsetof(c1::clipMap_t, partitionCount), static_cast<std::int32_t>(partitions.size()));
			b.follows(head, offsetof(c1::clipMap_t, partitions));
			const std::uint32_t chunk = b.add_array(packed_partitions);
			for (std::size_t p = 0; p < partitions.size(); p++)
			{
				if (partitions[p].border_count)
				{
					b.point(chunk, p * sizeof(packed_partition) + offsetof(packed_partition, borders), borders_chunk, 28 * partitions[p].first_border);
				}
			}
			b.put(head, offsetof(c1::clipMap_t, aabbTreeCount), static_cast<std::int32_t>(aabbs.size()));
			b.follows(head, offsetof(c1::clipMap_t, aabbTrees));
			b.add_array(aabbs);
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

		// the static model tree: the root, and under it one leaf per run of up to 12 models ( a leaf names a run of
		// staticModelList, a node its children as node index + model count ). Without models: one empty root, which
		// is read whatever the count.
		{
			std::vector<smodel_node> nodes(1);
			box3 all;
			const std::size_t n = colliding.size();
			if (n <= 12)
			{
				for (const int m : colliding)
				{
					all.add(w.models[static_cast<std::size_t>(m)].box);
				}
				nodes[0].childCount = static_cast<std::uint16_t>(n);
			}
			else
			{
				for (std::size_t first = 0; first < n; first += 12)
				{
					smodel_node leaf{};
					box3 box;
					for (std::size_t k = first; k < std::min(n, first + 12); k++)
					{
						box.add(w.models[static_cast<std::size_t>(colliding[k])].box);
					}
					leaf.bounds = box.bounds();
					leaf.firstChild = static_cast<std::uint16_t>(first);
					leaf.childCount = static_cast<std::uint16_t>(std::min<std::size_t>(12, n - first));
					all.add(box);
					nodes.push_back(leaf);
				}
				nodes[0].firstChild = static_cast<std::uint16_t>(n + 1);
				nodes[0].childCount = static_cast<std::uint16_t>(nodes.size() - 1);
			}
			nodes[0].bounds = all.valid() ? all.bounds() : w.bounds.bounds();
			b.put(head, offsetof(c1::clipMap_t, smodelNodeCount), static_cast<std::uint32_t>(nodes.size()));
			b.follows(head, offsetof(c1::clipMap_t, smodelNodes));
			b.add_array(nodes);
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
