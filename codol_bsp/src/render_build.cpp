// Brush faces -> render surfaces.

#include "vis.hpp"

namespace codol::bsp
{
	namespace
	{
		constexpr float SURFACE_GRID = 1024.0f;		// faces of one material are grouped into surfaces by this grid
		constexpr std::size_t SURFACE_VERTICES = 60000;

		struct pending_face
		{
			int brush, face, material, model, cell;
			std::int64_t group;
		};
	}

	void build_render_surfaces(compile_context& ctx, render_world& rw)
	{
		world& w = ctx.w;

		// ---- what is hidden, and which cell sees what
		vis_result vis;
		build_vis(ctx, vis);

		// ---- the faces that are drawn, and their materials
		std::map<std::string, int> material_index;
		std::set<std::string> missing;
		std::vector<pending_face> faces;
		const auto material_of = [&](const std::string& map_name)
		{
			const std::string name = "wc/" + map_name;
			if (const auto it = material_index.find(name); it != material_index.end())
			{
				return it->second;
			}
			const material_desc& desc = ctx.importer.describe(name);
			if (!desc.ref.valid())
			{
				if (missing.insert(name).second)
				{
					ctx.rep.warnings.push_back("render: material '" + name + "' is not drawn: " + desc.why);
				}
				material_index.emplace(name, -1);
				return -1;
			}
			render_material m;
			m.name = name;
			m.ref = desc.ref;
			m.desc = &desc;
			m.draw_class = desc.sort_key >= static_cast<int>(gfx::SORT_KEY_LIT_DECAL) && !desc.sky ? 1 : 0;
			const int index = static_cast<int>(rw.materials.size());
			rw.materials.push_back(std::move(m));
			material_index.emplace(name, index);
			return index;
		};
		const auto grid_cell = [](const double v) { return static_cast<std::int64_t>(std::floor(v / SURFACE_GRID)) & 0xFFFFF; };
		// the cell a point is in, by the node stream the game walks
		const auto cell_of = [&](const vec3& p)
		{
			if (!vis.built)
			{
				return 0;
			}
			std::size_t at = 0;
			const int cells = static_cast<int>(vis.cells.size());
			while (at < vis.cell_nodes.size())
			{
				const int value = vis.cell_nodes[at];
				if (value <= cells)
				{
					return std::max(value - 1, 0);
				}
				const c1::cplane_s& plane = w.planes.planes[static_cast<std::size_t>(value - cells - 1)];
				const double d = plane.normal[0] * p.x + plane.normal[1] * p.y + plane.normal[2] * p.z - plane.dist;
				at = d > 0 ? at + 2 : at + vis.cell_nodes[at + 1];
			}
			return 0;
		};
		// patches: every triangle of the tessellation is a face; "brush" -1 - patch, "face" the triangle
		for (std::size_t pi = 0; pi < w.patches.size(); pi++)
		{
			const map_patch& patch = w.patches[pi];
			const int index = is_tool_material(patch.material) ? -1 : material_of(patch.material);
			for (std::size_t t = 0; index >= 0 && t + 2 < patch.indices.size(); t += 3)
			{
				vec3 centre{};
				for (int k = 0; k < 3; k++)
				{
					const edit::mesh_vertex& v = patch.vertices[patch.indices[t + static_cast<std::size_t>(k)]];
					centre = centre + vec3{ v.position[0], v.position[1], v.position[2] } * (1.0 / 3.0);
				}
				const edit::mesh_vertex& first = patch.vertices[patch.indices[t]];
				const int c = cell_of(centre + vec3{ first.normal[0], first.normal[1], first.normal[2] } * 2.0);
				faces.push_back({ -1 - static_cast<int>(pi), static_cast<int>(t / 3), index, 0, c, grid_cell(centre.x) | (grid_cell(centre.y) << 20) | (grid_cell(centre.z) << 40) });
			}
		}
		for (std::size_t bi = 0; bi < w.brushes.size(); bi++)
		{
			map_brush& brush = w.brushes[bi];
			for (std::size_t fi = 0; fi < brush.faces.size(); fi++)
			{
				map_face& face = brush.faces[fi];
				face.draw = false;
				if (face.winding.size() < 3 || !face.visible || is_tool_material(face.material))
				{
					continue;
				}
				const int index = material_of(face.material);
				if (index < 0)
				{
					continue;
				}
				face.draw = true;
				vec3 centre{};
				for (const vec3& p : face.winding)
				{
					centre = centre + p;
				}
				centre = centre * (1.0 / static_cast<double>(face.winding.size()));

				// a face that is seen from several cells is drawn by each of them ( a surface belongs to one cell )
				static const std::vector<int> first_cell{ 0 };
				for (const int c : vis.built && brush.model == 0 && !face.cells.empty() ? face.cells : first_cell)
				{
					faces.push_back({ static_cast<int>(bi), static_cast<int>(fi), index, brush.model, c, grid_cell(centre.x) | (grid_cell(centre.y) << 20) | (grid_cell(centre.z) << 40) });
				}
			}
		}

		// ---- order: static surfaces by draw class, sort key and material, then the brush models
		const auto rank = [&](const pending_face& f)
		{
			const render_material& m = rw.materials[static_cast<std::size_t>(f.material)];
			return std::make_tuple(f.model != 0 ? f.model : 0, f.model != 0 ? 0 : m.draw_class, m.desc->sort_key, f.material, f.cell, f.group, f.brush, f.face);
		};
		std::sort(faces.begin(), faces.end(), [&](const pending_face& a, const pending_face& c) { return rank(a) < rank(c); });

		rw.models.assign(static_cast<std::size_t>(w.model_count), {});
		for (std::size_t at = 0; at < faces.size();)
		{
			render_surface surface;
			const pending_face& first = faces[at];
			const render_material& material = rw.materials[static_cast<std::size_t>(first.material)];
			surface.material = first.material;
			surface.model = first.model;
			surface.cell = first.cell;
			surface.sky = material.desc->sky;
			if (surface.sky)
			{
				surface.casts_sun_shadow = false;
				surface.lightmap = 31;
				surface.primary_light = 0;
			}
			for (; at < faces.size() && faces[at].model == first.model && faces[at].material == first.material && faces[at].cell == first.cell
				&& faces[at].group == first.group; at++)
			{
				if (surface.vertices.size() + 64 > SURFACE_VERTICES)
				{
					break;
				}
				if (faces[at].brush < 0)
				{
					// a triangle of a patch
					const map_patch& patch = w.patches[static_cast<std::size_t>(-1 - faces[at].brush)];
					render_face rf;
					rf.first_vertex = static_cast<std::uint32_t>(surface.vertices.size());
					rf.vertex_count = 3;
					rf.brush = faces[at].brush;
					rf.face = faces[at].face;
					for (int k = 0; k < 3; k++)
					{
						const edit::mesh_vertex& in = patch.vertices[patch.indices[static_cast<std::size_t>(faces[at].face) * 3 + static_cast<std::size_t>(k)]];
						c1::GfxWorldVertex v{};
						std::memcpy(v.xyz, in.position, sizeof(v.xyz));
						v.binormalSign = 1.0f;
						v.color = static_cast<std::uint32_t>(in.color[2]) | (static_cast<std::uint32_t>(in.color[1]) << 8) | (static_cast<std::uint32_t>(in.color[0]) << 16)
							| (static_cast<std::uint32_t>(in.color[3]) << 24);
						// the mesh has its coordinates in units of a 256 texel texture
						v.texCoord[0] = in.uv[0] * 256.0f / static_cast<float>(material.desc->width);
						v.texCoord[1] = in.uv[1] * 256.0f / static_cast<float>(material.desc->height);
						v.lmapCoord[0] = v.lmapCoord[1] = 0.5f;
						const vec3 n{ in.normal[0], in.normal[1], in.normal[2] };
						vec3 t = cross(std::fabs(n.z) < 0.99 ? vec3{ 0, 0, 1 } : vec3{ 0, 1, 0 }, n);
						t = t * (1.0 / std::max(length(t), 1e-9));
						const float tangent[3] = { static_cast<float>(t.x), static_cast<float>(t.y), static_cast<float>(t.z) };
						v.normal = pack_unit_vector(in.normal);
						v.tangent = pack_unit_vector(tangent);
						if (k == 0)
						{
							std::memcpy(rf.normal, in.normal, sizeof(rf.normal));
						}
						surface.vertices.push_back(v);
						surface.box.add(v.xyz);
					}
					surface.indices.push_back(static_cast<std::uint16_t>(rf.first_vertex));
					surface.indices.push_back(static_cast<std::uint16_t>(rf.first_vertex + 2));
					surface.indices.push_back(static_cast<std::uint16_t>(rf.first_vertex + 1));
					surface.faces.push_back(rf);
					continue;
				}
				const map_brush& brush = w.brushes[static_cast<std::size_t>(faces[at].brush)];
				const map_face& face = brush.faces[static_cast<std::size_t>(faces[at].face)];
				vec3 u_axis, v_axis;
				edit::face_texture_axes(face.plane.normal, face.texture.rotation, u_axis, v_axis);
				const float normal[3] = { static_cast<float>(face.plane.normal.x), static_cast<float>(face.plane.normal.y), static_cast<float>(face.plane.normal.z) };
				const float tangent[3] = { static_cast<float>(u_axis.x), static_cast<float>(u_axis.y), static_cast<float>(u_axis.z) };
				const vec3 binormal = cross(face.plane.normal, u_axis);
				const float sign = dot(binormal, v_axis) >= 0 ? 1.0f : -1.0f;
				render_face rf;
				rf.first_vertex = static_cast<std::uint32_t>(surface.vertices.size());
				rf.vertex_count = static_cast<std::uint32_t>(face.winding.size());
				rf.brush = faces[at].brush;
				rf.face = faces[at].face;
				std::memcpy(rf.normal, normal, sizeof(normal));
				for (const vec3& p : face.winding)
				{
					c1::GfxWorldVertex v{};
					v.xyz[0] = static_cast<float>(p.x);
					v.xyz[1] = static_cast<float>(p.y);
					v.xyz[2] = static_cast<float>(p.z);
					v.binormalSign = sign;
					v.color = 0xFFFFFFFFu;
					const edit::vec2 uv = edit::face_uv(p, face.plane.normal, face.texture, material.desc->width, material.desc->height);
					v.texCoord[0] = static_cast<float>(uv.x);
					v.texCoord[1] = static_cast<float>(uv.y);
					v.lmapCoord[0] = v.lmapCoord[1] = 0.5f;
					v.normal = pack_unit_vector(normal);
					v.tangent = pack_unit_vector(tangent);
					surface.vertices.push_back(v);
					surface.box.add(v.xyz);
				}
				// the winding is counter clockwise seen from outside; the game's triangles are clockwise
				for (std::uint32_t k = 2; k < rf.vertex_count; k++)
				{
					surface.indices.push_back(static_cast<std::uint16_t>(rf.first_vertex));
					surface.indices.push_back(static_cast<std::uint16_t>(rf.first_vertex + k));
					surface.indices.push_back(static_cast<std::uint16_t>(rf.first_vertex + k - 1));
				}
				surface.faces.push_back(rf);
			}
			render_model& model = rw.models[static_cast<std::size_t>(surface.model)];
			if (model.surface_count == 0)
			{
				model.first_surface = static_cast<std::uint32_t>(rw.surfaces.size());
			}
			model.surface_count++;
			model.box.add(surface.box);
			if (surface.model == 0)
			{
				(material.draw_class == 0 ? rw.lit_opaque_end : rw.lit_trans_end) = static_cast<std::uint32_t>(rw.surfaces.size() + 1);
				if (surface.sky)
				{
					rw.sky_surfaces.push_back(static_cast<std::int32_t>(rw.surfaces.size()));
					if (!rw.sky_image.valid())
					{
						rw.sky_image = material.desc->sky_image;
					}
				}
			}
			rw.surfaces.push_back(std::move(surface));
		}
		rw.lit_trans_end = std::max(rw.lit_trans_end, rw.lit_opaque_end);

		// ---- bounds
		rw.bounds = w.bounds;
		rw.bounds.add(rw.models[0].box);
		rw.models[0].box = rw.bounds;
		for (int m = 1; m < w.model_count; m++)
		{
			for (const map_brush& brush : w.brushes)
			{
				if (brush.model == m)
				{
					rw.models[static_cast<std::size_t>(m)].box.add(brush.box);
				}
			}
		}

		// ---- static models: each in the cell its centre is in
		for (std::size_t m = 0; m < w.models.size(); m++)
		{
			const map_model& model = w.models[m];
			if (!model.ref.valid())
			{
				continue;
			}
			render_smodel s;
			s.model = static_cast<int>(m);
			s.cell = cell_of(vec3{ (model.box.mins[0] + model.box.maxs[0]) * 0.5, (model.box.mins[1] + model.box.maxs[1]) * 0.5,
				(model.box.mins[2] + model.box.maxs[2]) * 0.5 });
			rw.smodels.push_back(s);
			rw.bounds.add(model.box);
		}
		if (rw.smodels.size() > 0xFFFF)
		{
			throw compile_error("more than 65535 static models");
		}
		rw.models[0].box = rw.bounds;

		// ---- cells
		if (vis.built)
		{
			rw.cells = std::move(vis.cells);
			rw.cell_nodes = std::move(vis.cell_nodes);
			for (const render_surface& surface : rw.surfaces)
			{
				if (surface.model == 0)
				{
					rw.cells[static_cast<std::size_t>(surface.cell)].box.add(surface.box);
				}
			}
		}
		else
		{
			// one cell around everything
			render_cell cell;
			cell.box = rw.bounds;
			rw.cells.push_back(std::move(cell));
			rw.cell_nodes = { 1 };			// a leaf: cell 0 ( stored + 1; 0 would be "no cell" )
		}
	}
}
