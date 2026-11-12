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
				const std::string name = "wc/" + face.material;
				int index;
				if (const auto it = material_index.find(name); it != material_index.end())
				{
					index = it->second;
				}
				else
				{
					const material_desc& desc = ctx.importer.describe(name);
					if (!desc.ref.valid())
					{
						if (missing.insert(name).second)
						{
							ctx.rep.warnings.push_back("render: material '" + name + "' is not drawn: " + desc.why);
						}
						material_index.emplace(name, -1);
						continue;
					}
					render_material m;
					m.name = name;
					m.ref = desc.ref;
					m.desc = &desc;
					m.draw_class = desc.sort_key >= static_cast<int>(gfx::SORT_KEY_LIT_DECAL) && !desc.sky ? 1 : 0;
					index = static_cast<int>(rw.materials.size());
					rw.materials.push_back(std::move(m));
					material_index.emplace(name, index);
				}
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
				const auto cell = [](const double v) { return static_cast<std::int64_t>(std::floor(v / SURFACE_GRID)) & 0xFFFFF; };
				// a face that is seen from several cells is drawn by each of them ( a surface belongs to one cell )
				static const std::vector<int> first_cell{ 0 };
				for (const int c : vis.built && brush.model == 0 && !face.cells.empty() ? face.cells : first_cell)
				{
					faces.push_back({ static_cast<int>(bi), static_cast<int>(fi), index, brush.model, c, cell(centre.x) | (cell(centre.y) << 20) | (cell(centre.z) << 40) });
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
				const map_brush& brush = w.brushes[static_cast<std::size_t>(faces[at].brush)];
				const map_face& face = brush.faces[static_cast<std::size_t>(faces[at].face)];
				if (surface.vertices.size() + face.winding.size() > SURFACE_VERTICES)
				{
					break;
				}
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
