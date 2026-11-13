// Stage 4: render geometry for the new and the edited brushes of the world, and for the patches of the world ( their
// tessellation, one surface per patch; every vertex takes the nearest texel of the bake that faces its way ).
//
// What the GfxWorld holds for world geometry ( shape of the data: codol_mapc_probe gfx / fields ):
//
//   draw.vertices, draw.indices   GfxWorldVertex ( 44 bytes ) and u16 indices, relative to a surface's first vertex
//   dpvs.surfaces                 GfxSurface: vertex / index range, material, lightmap, reflection probe, primary light.
//                                 The first staticSurfaceCount surfaces are the world, sorted by material inside the
//                                 ranges litOpaque / litTrans / shadowCaster / emissive; the surfaces of the brush
//                                 models follow ( GfxBrushModel.startSurfIndex ). The material field points at an
//                                 entry of GfxWorld.materialMemory.
//   dpvs.surfacesBounds           box per surface
//   dpvs.sortedSurfIndex          the static surfaces grouped by AABB tree leaf, twice ( the second half is the
//                                 "no decal" list; in the shipped maps it repeats the first ). A tree node names a
//                                 run of it; a node's run is the runs of its children back to back.
//   shadow geometry               per primary light a list of surfaces
//
// A new surface takes everything it cannot know from an existing surface of the same material, the one nearest
// to it ( the "template" ): the material pointer, lightmap index, reflection probe, primary light, flags - and it is
// inserted right behind it, which keeps the array sorted the way it was. All vertices of one face get the lightmap
// coordinate of one vertex of the template ( the nearest one that faces the same way ), so a new face is lit with
// one texel of the existing light bake: evenly, in the brightness of a surface next to it. No light is baked.
//
// There is no record in a zone of which surfaces came from which brush ( the map compiler merges and sorts the
// world's triangles ), so the surfaces of a brush that was deleted or moved away cannot be found and stay.

#include "mapc_impl.hpp"
#include "codol_edit/patch_ops.hpp"

#include <functional>
#include <set>

namespace codol::mapc
{
	namespace
	{
		// PackedUnitVec ( codol_zone assets.cpp unpack_unit_vector ): bytes ( n * 127 + 127 ), scale byte 63 = 1 / 127
		std::uint32_t pack_unit_vector(const float n[3])
		{
			std::uint32_t packed = 63u << 24;
			for (int k = 0; k < 3; k++)
			{
				const long byte = std::clamp(std::lround(n[k] * 127.0f) + 127l, 0l, 255l);
				packed |= static_cast<std::uint32_t>(byte) << (8 * k);
			}
			return packed;
		}

		struct tree
		{
			ZoneDataId id = 0;
			std::vector<aabb_node> nodes;
			std::vector<int> parent;
			bool changed = false;
		};
	}

	void stage_surfaces(context& ctx)
	{
		const Zone& zone = ctx.zone();
		ZoneEditor& editor = ctx.editor;
		const map_diff& diff = ctx.rep.diff;
		const c1::GfxWorld& raw = ctx.gfx.raw;
		const c1::GfxWorldDpvsStatic& dpvs = raw.dpvs;

		int world_brushes = 0, model_brushes = 0, replaced = 0;
		for (const brush_edit& e : diff.brushes)
		{
			(e.sub_model == 0 ? world_brushes : model_brushes)++;
			replaced += e.sub_model == 0 && e.replaces >= 0;
		}
		const int gone = replaced + static_cast<int>(diff.brushes_deleted.size());
		if (gone)
		{
			ctx.warn(format("surfaces: %d brushes were deleted or changed shape. A zone does not say which render surfaces a brush produced, so whatever the base zone draws for them "
				"stays: delete or move world brushes that are drawn only where the old surfaces are hidden or wanted", gone));
		}
		if (model_brushes)
		{
			ctx.warn(format("surfaces: %d brushes of brush models changed; the render surfaces of brush models are not compiled ( their collision is )", model_brushes));
		}
		if (!world_brushes && diff.patches.empty())
		{
			return;
		}

		// ---- the arrays
		const std::uint32_t old_static = dpvs.staticSurfaceCount, old_surfaces = raw.surfaceCount;
		const ZoneDataId vertices_id = ctx.data(raw.draw.vertices, "GfxWorld vertices");
		const ZoneDataId indices_id = ctx.data(raw.draw.indices, "GfxWorld indices");
		const ZoneDataId surfaces_id = ctx.data(dpvs.surfaces, "dpvs.surfaces");
		const ZoneDataId bounds_id = ctx.data(dpvs.surfacesBounds, "dpvs.surfacesBounds");
		const ZoneDataId sorted_id = ctx.data(dpvs.sortedSurfIndex, "dpvs.sortedSurfIndex");
		const ZoneDataId models_id = ctx.data(raw.models, "GfxWorld.models");
		auto vertices = array_of<c1::GfxWorldVertex>(editor.data_bytes(vertices_id));
		auto indices = array_of<std::uint16_t>(editor.data_bytes(indices_id));
		auto surfaces = array_of<c1::GfxSurface>(editor.data_bytes(surfaces_id));
		auto bounds = array_of<c1::Bounds>(editor.data_bytes(bounds_id));
		auto sorted = array_of<std::uint16_t>(editor.data_bytes(sorted_id));
		auto models = array_of<c1::GfxBrushModel>(editor.data_bytes(models_id));
		if (surfaces.size() != old_surfaces || bounds.size() != old_surfaces || sorted.size() != 2u * old_static || vertices.size() != raw.draw.vertexCount
			|| indices.size() != raw.draw.indexCount || dpvs.staticSurfaceCountNoDecal != old_static || ctx.gfx.surfaces.size() != old_surfaces)
		{
			throw compile_error("surfaces: the GfxWorld's surface arrays do not have the sizes the compiler expects ( surfaces, bounds, two sorted lists of the static surfaces )");
		}
		std::vector<std::uint16_t> first(sorted.begin(), sorted.begin() + old_static), second(sorted.begin() + old_static, sorted.end());
		if (first != second)
		{
			throw compile_error("surfaces: the \"no decal\" half of dpvs.sortedSurfIndex is not a copy of the first half; the compiler does not know how to extend it");
		}
		std::vector<ZoneTarget> materials(old_surfaces);
		{
			std::vector<std::uint8_t> have(old_surfaces, 0);
			for (const ZonePointer& p : editor.data_pointers(surfaces_id))
			{
				if (p.offset % 24 == offsetof(c1::GfxSurface, material) && p.offset / 24 < old_surfaces)
				{
					materials[p.offset / 24] = p.target;
					have[p.offset / 24] = 1;
				}
			}
			if (std::find(have.begin(), have.end(), std::uint8_t{ 0 }) != have.end())
			{
				throw compile_error("surfaces: a GfxSurface carries its material itself instead of pointing at GfxWorld.materialMemory");
			}
		}

		// ---- the cells' trees
		const zaddr counts_address = zone.get<zaddr>(ctx.gfx_address + WORLD_AABB_TREE_COUNTS), trees_address = zone.get<zaddr>(ctx.gfx_address + WORLD_AABB_TREES);
		std::vector<tree> trees(static_cast<std::size_t>(std::max(raw.cellCount, 0)));
		for (std::size_t c = 0; c < trees.size(); c++)
		{
			tree& t = trees[c];
			const int count = zone.get<std::int32_t>(counts_address + 4u * static_cast<std::uint32_t>(c));
			const zaddr address = zone.get<zaddr>(trees_address + 4u * static_cast<std::uint32_t>(c));
			if (count <= 0)
			{
				continue;
			}
			t.id = ctx.data(address, "cell AABB tree");
			t.nodes = array_of<aabb_node>(editor.data_bytes(t.id));		// stream values: the list pointers stay as they are
			t.parent.assign(t.nodes.size(), -1);
			for (std::size_t n = 0; n < t.nodes.size(); n++)
			{
				const aabb_node& node = t.nodes[n];
				if (node.surfaceCount != node.surfaceCountNoDecal || (node.surfaceCount && node.startSurfIndexNoDecal != node.startSurfIndex + old_static)
					|| static_cast<std::uint32_t>(node.startSurfIndex) + node.surfaceCount > old_static)
				{
					throw compile_error("surfaces: an AABB tree node's two surface runs are not the same run of the two halves of dpvs.sortedSurfIndex");
				}
				if (node.childCount == 0)
				{
					continue;
				}
				const std::size_t child = n + static_cast<std::size_t>(node.childrenOffset / 44);
				if (node.childrenOffset % 44 != 0 || child <= n || child + node.childCount > t.nodes.size())
				{
					throw compile_error("surfaces: an AABB tree node has children outside its tree");
				}
				std::uint32_t at = node.startSurfIndex, listed = 0;
				for (std::size_t k = child; k < child + node.childCount; k++)
				{
					t.parent[k] = static_cast<int>(n);
					if (t.nodes[k].surfaceCount)
					{
						if (t.nodes[k].startSurfIndex != at)
						{
							throw compile_error("surfaces: the surface runs of the children of an AABB tree node do not follow each other");
						}
						at += t.nodes[k].surfaceCount;
						listed += t.nodes[k].surfaceCount;
					}
				}
				if (listed != node.surfaceCount)
				{
					throw compile_error("surfaces: an AABB tree node lists surfaces its children do not");
				}
			}
		}
		const cell_finder cells(ctx);

		// ---- shadow geometry: surface lists per light
		const zaddr shadow_address = zone.get<zaddr>(ctx.gfx_address + WORLD_SHADOW_GEOM);
		std::vector<shadow_geometry> shadow_image;
		std::vector<std::vector<std::uint16_t>> shadow_lists;
		std::vector<ZoneDataId> shadow_ids;
		if (shadow_address && raw.primaryLightCount)
		{
			shadow_image = zone.get_array<shadow_geometry>(shadow_address, raw.primaryLightCount);
			for (const shadow_geometry& g : shadow_image)
			{
				shadow_lists.push_back(zone.get_array<std::uint16_t>(g.sortedSurfIndex, g.surfaceCount));
				shadow_ids.push_back(g.surfaceCount && g.sortedSurfIndex ? ctx.data(g.sortedSurfIndex, "shadow geometry surface list") : 0);
			}
		}

		// ---- which way round the world's triangles are: the normal of ( v1 - v0 ) x ( v2 - v0 ) against the stored one
		bool flip = false;
		{
			int along = 0, against = 0;
			for (std::uint32_t s = 0; s < old_static && along + against < 4000; s += std::max(1u, old_static / 800))
			{
				const c1::GfxSurface& surface = surfaces[s];
				for (std::uint32_t t = 0; t < surface.triCount && t < 5; t++)
				{
					const std::size_t at = surface.baseIndex + 3u * t;
					if (at + 2 >= indices.size())
					{
						break;
					}
					const c1::GfxWorldVertex& a = vertices[surface.firstVertex + indices[at]];
					const c1::GfxWorldVertex& b = vertices[surface.firstVertex + indices[at + 1]];
					const c1::GfxWorldVertex& c = vertices[surface.firstVertex + indices[at + 2]];
					const float e1[3] = { b.xyz[0] - a.xyz[0], b.xyz[1] - a.xyz[1], b.xyz[2] - a.xyz[2] }, e2[3] = { c.xyz[0] - a.xyz[0], c.xyz[1] - a.xyz[1], c.xyz[2] - a.xyz[2] };
					const float n[3] = { e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2], e1[0] * e2[1] - e1[1] * e2[0] };
					float stored[3];
					zone::unpack_unit_vector(a.normal, stored);
					const float d = n[0] * stored[0] + n[1] * stored[1] + n[2] * stored[2];
					(d > 0 ? along : against) += d != 0;
				}
			}
			flip = against > along;
			ctx.note(format("surfaces: of %d sampled triangles of the base zone %d are counter clockwise seen from their normal's side, %d clockwise; the new ones are made %s", along + against,
				along, against, flip ? "clockwise" : "counter clockwise"));
		}

		// ---- new surfaces
		std::set<std::string> undrawn;
		int other_cell = 0;
		const std::uint32_t old_vertices = static_cast<std::uint32_t>(vertices.size()), old_indices = static_cast<std::uint32_t>(indices.size());
		std::uint32_t static_now = old_static;
		std::vector<std::string_view> names(old_surfaces);		// material name per surface of the working array
		for (std::uint32_t s = 0; s < old_surfaces; s++)
		{
			names[s] = ctx.gfx.surfaces[s].materialName;
		}
		// what is drawn: the new / edited world brushes, then the patches of the world ( terrain as it is, curves at their
		// subdivision ) - a patch is one part of one material
		std::vector<std::pair<edit::object_id, bool>> drawn;		// id, is a patch
		for (const brush_edit& e : diff.brushes)
		{
			if (e.sub_model == 0)
			{
				drawn.emplace_back(e.brush, false);
			}
		}
		for (const edit::object_id id : diff.patches)
		{
			drawn.emplace_back(id, true);
		}
		for (const auto& [object, is_patch] : drawn)
		{
			const edit::brush* source = is_patch ? nullptr : ctx.edited.find_brush(object);
			const edit::patch* source_patch = is_patch ? ctx.edited.find_patch(object) : nullptr;
			edit::mesh mesh;
			if ((!source && !source_patch) || !ctx.edited.build_mesh(object, mesh, source_patch ? edit::patch_subdivisions(*source_patch) : 4) || mesh.parts.empty())
			{
				continue;
			}
			if (source_patch)
			{
				ctx.rep.patches_drawn++;
			}
			// faces by material
			std::map<std::string, std::vector<const edit::mesh_part*>> by_material;
			for (const edit::mesh_part& part : mesh.parts)
			{
				if (part.index_count >= 3 && !is_tool_material(part.material))
				{
					by_material[part.material].push_back(&part);
				}
			}
			const edit::vec3 brush_centre = mesh.box.center();
			for (const auto& [material, parts] : by_material)
			{
				// the template: the nearest static surface with this material ( a lightmapped one when there is one )
				const std::string wanted = material.rfind("wc/", 0) == 0 ? material : "wc/" + material;
				int like = -1;
				double best = 0;
				for (std::uint32_t s = 0; s < static_now; s++)
				{
					if (names[s] != wanted || surfaces[s].firstVertex >= old_vertices)
					{
						continue;		// another material, or a surface this stage made
					}
					const c1::Bounds& b = bounds[s];
					double distance = 0;
					for (int k = 0; k < 3; k++)
					{
						const double d = std::max(0.0, std::fabs(b.midPoint[k] - brush_centre[k]) - b.halfSize[k]);
						distance += d * d;
					}
					distance += surfaces[s].lightmapIndex == 31 ? 1e12 : 0.0;
					if (like < 0 || distance < best)
					{
						like = static_cast<int>(s);
						best = distance;
					}
				}
				if (like < 0)
				{
					undrawn.insert(material);
					ctx.rep.faces_not_drawn += static_cast<int>(parts.size());
					continue;
				}
				const c1::GfxSurface model = surfaces[static_cast<std::size_t>(like)];
				c1::GfxSurface made = model;
				made.vertexLayerData = 0;
				made.firstVertex = static_cast<std::uint32_t>(vertices.size());
				made.baseIndex = static_cast<std::uint32_t>(indices.size());
				box made_box;
				std::uint32_t vertex_count = 0, triangle_count = 0;
				for (const edit::mesh_part* part : parts)
				{
					// the vertices of this face, in the order the part's indices use them
					std::map<std::uint32_t, std::uint16_t> local;
					const edit::mesh_vertex& any = mesh.vertices[mesh.indices[part->first_index]];
					const float normal[3] = { any.normal[0], any.normal[1], any.normal[2] };
					// lightmap coordinate, colour: one vertex of the template, the nearest that faces the way this face does
					// ( for a patch: per vertex, below, as a curve faces many ways )
					const auto find_donor = [&](const float* position, const float* facing_normal) -> const c1::GfxWorldVertex*
					{
						const c1::GfxWorldVertex* found = nullptr;
						float found_score = 0;
						for (std::uint32_t v = model.firstVertex; v < model.firstVertex + model.vertexCount && v < old_vertices; v++)
						{
							float theirs[3];
							zone::unpack_unit_vector(vertices[v].normal, theirs);
							const float facing = theirs[0] * facing_normal[0] + theirs[1] * facing_normal[1] + theirs[2] * facing_normal[2];
							float distance = 0;
							for (int k = 0; k < 3; k++)
							{
								const float d = vertices[v].xyz[k] - position[k];
								distance += d * d;
							}
							const float score = std::sqrt(distance) + (facing > 0.7f ? 0.0f : 100000.0f);
							if (!found || score < found_score)
							{
								found = &vertices[v];
								found_score = score;
							}
						}
						return found;
					};
					const c1::GfxWorldVertex* donor = find_donor(any.position, normal);
					// copied: the vertex array grows below
					const std::uint32_t donor_color = donor ? donor->color : 0xFFFFFFFFu;
					const float donor_lmap[2] = { donor ? donor->lmapCoord[0] : 0.0f, donor ? donor->lmapCoord[1] : 0.0f };
					// tangent: the texture's u direction on the face
					edit::vec3 u_axis, v_axis;
					const edit::vec3 n(normal[0], normal[1], normal[2]);
					const double rotation = source && part->face >= 0 && static_cast<std::size_t>(part->face) < source->faces.size() ? source->faces[static_cast<std::size_t>(part->face)].texture.rotation : 0.0;
					edit::face_texture_axes(n, rotation, u_axis, v_axis);
					const float tangent[3] = { static_cast<float>(u_axis.x), static_cast<float>(u_axis.y), static_cast<float>(u_axis.z) };
					const float sign = edit::dot(edit::cross(n, u_axis), v_axis) >= 0 ? 1.0f : -1.0f;
					for (std::uint32_t k = 0; k < part->index_count; k++)
					{
						// triangles the way round the world has them
						const std::uint32_t corner = flip ? (k % 3 == 1 ? k + 1 : k % 3 == 2 ? k - 1 : k) : k;
						const std::uint32_t index = mesh.indices[part->first_index + corner];
						auto it = local.find(index);
						if (it == local.end())
						{
							const edit::mesh_vertex& from = mesh.vertices[index];
							c1::GfxWorldVertex v{};
							std::memcpy(v.xyz, from.position, 12);
							v.binormalSign = sign;
							v.color = donor_color;
							v.texCoord[0] = from.uv[0];
							v.texCoord[1] = from.uv[1];
							v.lmapCoord[0] = donor_lmap[0];
							v.lmapCoord[1] = donor_lmap[1];
							v.normal = pack_unit_vector(from.normal);
							v.tangent = pack_unit_vector(tangent);
							if (source_patch)
							{
								// a patch: its own vertex colour ( the blend of a terrain ), the texel of the template nearest to this
								// vertex that faces its way, a tangent from its own normal
								const c1::GfxWorldVertex* near_donor = find_donor(from.position, from.normal);
								v.color = static_cast<std::uint32_t>(from.color[2]) | (static_cast<std::uint32_t>(from.color[1]) << 8) | (static_cast<std::uint32_t>(from.color[0]) << 16)
									| (static_cast<std::uint32_t>(from.color[3]) << 24);
								if (near_donor)
								{
									v.lmapCoord[0] = near_donor->lmapCoord[0];
									v.lmapCoord[1] = near_donor->lmapCoord[1];
								}
								edit::vec3 pu, pv;
								const edit::vec3 pn(from.normal[0], from.normal[1], from.normal[2]);
								edit::face_texture_axes(pn, 0.0, pu, pv);
								const float patch_tangent[3] = { static_cast<float>(pu.x), static_cast<float>(pu.y), static_cast<float>(pu.z) };
								v.tangent = pack_unit_vector(patch_tangent);
								v.binormalSign = edit::dot(edit::cross(pn, pu), pv) >= 0 ? 1.0f : -1.0f;
							}
							it = local.emplace(index, static_cast<std::uint16_t>(vertex_count++)).first;
							vertices.push_back(v);
							made_box.add(v.xyz);
						}
						indices.push_back(it->second);
					}
					triangle_count += part->index_count / 3;
				}
				if (vertex_count > 0xFFFF || triangle_count > 0xFFFF)
				{
					throw compile_error("surfaces: a brush has more vertices than one surface holds");
				}
				made.vertexCount = static_cast<std::uint16_t>(vertex_count);
				made.triCount = static_cast<std::uint16_t>(triangle_count);

				// ---- into the surface array, right behind the template
				const std::uint32_t at = static_cast<std::uint32_t>(like) + 1;
				surfaces.insert(surfaces.begin() + at, made);
				materials.insert(materials.begin() + at, materials[static_cast<std::size_t>(like)]);
				bounds.insert(bounds.begin() + at, made_box.bounds());
				names.insert(names.begin() + at, names[static_cast<std::size_t>(like)]);
				for (std::uint16_t& v : first)
				{
					v += v >= at ? 1 : 0;
				}
				for (auto& list : shadow_lists)
				{
					for (std::uint16_t& v : list)
					{
						v += v >= at ? 1 : 0;
					}
				}
				static_now++;

				// ---- into a leaf of the cell it lies in: the leaf whose box has to grow least
				const float centre[3] = { made_box.bounds().midPoint[0], made_box.bounds().midPoint[1], made_box.bounds().midPoint[2] };
				int cell = cells.find(centre);
				const auto best_leaf = [&](const tree& t, float& growth)
				{
					int found = -1;
					for (std::size_t n = 0; n < t.nodes.size(); n++)
					{
						if (t.nodes[n].childCount == 0 && t.nodes[n].surfaceCount > 0)
						{
							const float g = box(t.nodes[n].bounds).growth(made_box);
							if (found < 0 || g < growth)
							{
								found = static_cast<int>(n);
								growth = g;
							}
						}
					}
					return found;
				};
				int leaf = -1;
				float growth = 0;
				if (cell >= 0 && cell < static_cast<int>(trees.size()))
				{
					leaf = best_leaf(trees[static_cast<std::size_t>(cell)], growth);
				}
				if (leaf < 0)
				{
					// no cell, or a cell without surfaces: the best leaf of any cell
					for (std::size_t c = 0; c < trees.size(); c++)
					{
						float g = 0;
						const int found = best_leaf(trees[c], g);
						if (found >= 0 && (leaf < 0 || g < growth))
						{
							leaf = found;
							growth = g;
							cell = static_cast<int>(c);
						}
					}
					if (leaf < 0)
					{
						throw compile_error("surfaces: no cell of the map has an AABB tree leaf with surfaces");
					}
					other_cell++;
				}
				tree& home = trees[static_cast<std::size_t>(cell)];
				const std::uint32_t slot = static_cast<std::uint32_t>(home.nodes[static_cast<std::size_t>(leaf)].startSurfIndex) + home.nodes[static_cast<std::size_t>(leaf)].surfaceCount;
				first.insert(first.begin() + slot, static_cast<std::uint16_t>(at));
				std::vector<std::uint8_t> on_path(home.nodes.size(), 0);
				for (int n = leaf; n >= 0; n = home.parent[static_cast<std::size_t>(n)])
				{
					on_path[static_cast<std::size_t>(n)] = 1;
				}
				for (tree& t : trees)
				{
					for (std::size_t n = 0; n < t.nodes.size(); n++)
					{
						aabb_node& node = t.nodes[n];
						if (&t == &home && on_path[n])
						{
							node.surfaceCount++;
							box grown(node.bounds);
							grown.add(made_box);
							node.bounds = grown.bounds();
							t.changed = true;
						}
						else if (node.surfaceCount && node.startSurfIndex >= slot)
						{
							node.startSurfIndex++;
							t.changed = true;
						}
					}
				}
				ctx.rep.surfaces_added++;
				ctx.rep.vertices_added += static_cast<int>(vertex_count);
				ctx.rep.triangles_added += static_cast<int>(triangle_count);
			}
		}
		for (const std::string& name : undrawn)
		{
			ctx.warn("surfaces: no surface of the world uses material '" + (name.rfind("wc/", 0) == 0 ? name : "wc/" + name) + "' ( it would have to be added to GfxWorld.materialMemory and sorted in ); faces with it "
				"collide but are not drawn");
		}
		if (!ctx.rep.surfaces_added)
		{
			return;
		}
		if (other_cell)
		{
			ctx.warn(format("surfaces: %d new surfaces lie where the GfxWorld's BSP names no cell, or a cell that draws no surfaces; they went into the nearest AABB tree leaf of "
				"another cell and are culled with that cell", other_cell));
		}
		const std::uint32_t added = static_cast<std::uint32_t>(ctx.rep.surfaces_added), new_static = old_static + added;
		if (old_surfaces + added > 0xFFFF)
		{
			throw compile_error("surfaces: more than 65535 surfaces");
		}

		// ---- the second halves, the tree nodes
		for (tree& t : trees)
		{
			if (!t.id)
			{
				continue;
			}
			for (aabb_node& node : t.nodes)
			{
				node.surfaceCountNoDecal = node.surfaceCount;
				if (node.surfaceCount || node.startSurfIndexNoDecal >= old_static)
				{
					// an empty node of the base zone that names the start of the second half keeps naming it
					node.startSurfIndexNoDecal = static_cast<std::uint16_t>(new_static + (node.surfaceCount ? node.startSurfIndex : node.startSurfIndexNoDecal - old_static));
				}
			}
			// every node moved ( the second half starts later ), the static model list pointers stay what they are
			std::vector<ZonePointer> pointers;
			for (const ZonePointer& p : editor.data_pointers(t.id))
			{
				pointers.push_back(p);
			}
			ctx.set(t.id, bytes_of(t.nodes), pointers, "cell AABB tree");
		}
		sorted = first;
		sorted.insert(sorted.end(), first.begin(), first.end());

		// ---- brush models: the world is model 0, the others name surfaces behind the static ones
		for (std::size_t m = 0; m < models.size(); m++)
		{
			c1::GfxBrushModel& model = models[m];
			if (m == 0)
			{
				model.surfaceCount = static_cast<std::uint16_t>(model.surfaceCount + added);
				model.surfaceCountNoDecal = static_cast<std::uint16_t>(model.surfaceCountNoDecal + added);
			}
			else if (model.startSurfIndex != 0xFFFF && model.startSurfIndex >= old_static)
			{
				model.startSurfIndex = static_cast<std::uint16_t>(model.startSurfIndex + added);
			}
		}

		// ---- the ranges of the static surfaces: a boundary moves by the surfaces inserted in front of it
		std::vector<std::uint32_t> inserted_before(old_static + 1, 0);		// new surfaces in front of old boundary k ( k old surfaces lie before it )
		{
			std::uint32_t count = 0, seen_old = 0;
			for (std::uint32_t s = 0; s < new_static; s++)
			{
				if (surfaces[s].firstVertex >= old_vertices)
				{
					count++;		// sits behind its template: in front of the boundary that follows the template
				}
				else
				{
					seen_old++;
				}
				if (seen_old <= old_static)
				{
					inserted_before[seen_old] = count;
				}
			}
		}
		const auto moved = [&](const std::uint32_t boundary) { return boundary <= old_static ? boundary + inserted_before[boundary] : boundary + added; };

		// ---- write
		ctx.set(vertices_id, bytes_of(vertices), {}, "GfxWorld vertices");
		ctx.set(indices_id, bytes_of(indices), {}, "GfxWorld indices");
		std::vector<ZonePointer> surface_pointers;
		for (std::size_t s = 0; s < surfaces.size(); s++)
		{
			surfaces[s].material = 0;
			surface_pointers.push_back({ static_cast<std::uint32_t>(24 * s + offsetof(c1::GfxSurface, material)), materials[s] });
		}
		ctx.set(surfaces_id, bytes_of(surfaces), surface_pointers, "dpvs.surfaces");
		ctx.set(bounds_id, bytes_of(bounds), {}, "dpvs.surfacesBounds");
		ctx.set(sorted_id, bytes_of(sorted), {}, "dpvs.sortedSurfIndex");
		ctx.set(models_id, bytes_of(models), {}, "GfxWorld.models");
		for (std::size_t l = 0; l < shadow_lists.size(); l++)
		{
			if (shadow_ids[l])
			{
				ctx.set(shadow_ids[l], bytes_of(shadow_lists[l]), {}, "shadow geometry surface list");
			}
		}
		const std::uint32_t d = offsetof(c1::GfxWorld, dpvs);
		ctx.patch(ctx.gfx_header, offsetof(c1::GfxWorld, surfaceCount), old_surfaces + added, "GfxWorld.surfaceCount");
		ctx.patch(ctx.gfx_header, offsetof(c1::GfxWorld, draw) + offsetof(c1::GfxWorldDraw, vertexCount), static_cast<std::uint32_t>(vertices.size()), "draw.vertexCount");
		ctx.patch(ctx.gfx_header, offsetof(c1::GfxWorld, draw) + offsetof(c1::GfxWorldDraw, indexCount), static_cast<std::uint32_t>(indices.size()), "draw.indexCount");
		ctx.patch(ctx.gfx_header, d + offsetof(c1::GfxWorldDpvsStatic, staticSurfaceCount), new_static, "dpvs.staticSurfaceCount");
		ctx.patch(ctx.gfx_header, d + offsetof(c1::GfxWorldDpvsStatic, staticSurfaceCountNoDecal), new_static, "dpvs.staticSurfaceCountNoDecal");
		ctx.patch(ctx.gfx_header, d + offsetof(c1::GfxWorldDpvsStatic, litOpaqueSurfsBegin), moved(dpvs.litOpaqueSurfsBegin), "dpvs range");
		ctx.patch(ctx.gfx_header, d + offsetof(c1::GfxWorldDpvsStatic, litOpaqueSurfsEnd), moved(dpvs.litOpaqueSurfsEnd), "dpvs range");
		ctx.patch(ctx.gfx_header, d + offsetof(c1::GfxWorldDpvsStatic, litTransSurfsBegin), moved(dpvs.litTransSurfsBegin), "dpvs range");
		ctx.patch(ctx.gfx_header, d + offsetof(c1::GfxWorldDpvsStatic, litTransSurfsEnd), moved(dpvs.litTransSurfsEnd), "dpvs range");
		ctx.patch(ctx.gfx_header, d + offsetof(c1::GfxWorldDpvsStatic, shadowCasterSurfsBegin), moved(dpvs.shadowCasterSurfsBegin), "dpvs range");
		ctx.patch(ctx.gfx_header, d + offsetof(c1::GfxWorldDpvsStatic, shadowCasterSurfsEnd), moved(dpvs.shadowCasterSurfsEnd), "dpvs range");
		ctx.patch(ctx.gfx_header, d + offsetof(c1::GfxWorldDpvsStatic, emissiveSurfsBegin), moved(dpvs.emissiveSurfsBegin), "dpvs range");
		ctx.patch(ctx.gfx_header, d + offsetof(c1::GfxWorldDpvsStatic, emissiveSurfsEnd), moved(dpvs.emissiveSurfsEnd), "dpvs range");
		ctx.patch(ctx.gfx_header, d + offsetof(c1::GfxWorldDpvsStatic, surfaceVisDataCount), vis_data_count(dpvs.surfaceVisDataCount, old_static, new_static), "dpvs.surfaceVisDataCount");
		ctx.note(format("surfaces: %u -> %u surfaces ( %u -> %u static ), %u -> %zu vertices, %u -> %zu indices", old_surfaces, old_surfaces + added, old_static, new_static, old_vertices,
			vertices.size(), old_indices, indices.size()));
		ctx.note("surfaces: every new face is lit with one lightmap texel of the nearest surface of its material ( no light bake ); it is in no light's shadow geometry list");
	}
}
