#include "std_include.hpp"

// windows.h macros
#undef min
#undef max

#include "host.hpp"
#include "scene.hpp"

#include <ImGuizmo.h>

// The 3D view of the standalone editor: the map drawn by codol_view, with selection, outlines, a transform gizmo,
// and browsers for the models and materials of the game.

namespace editor
{
	scene& get_scene()
	{
		static scene s;
		return s;
	}

	namespace
	{
		constexpr float DEG = 0.0174532925f;

		// rows forward, left, up ( scaled ), translation: a D3D row vector world matrix of a placement
		void matrix_of(const view::transform& t, float m[16])
		{
			codol::render::camera c;
			c.pitch = t.angles.x;
			c.yaw = t.angles.y;
			c.roll = t.angles.z;

			const auto a = c.axes();
			const float3 rows[3] = { a.forward, a.left, a.up };

			for (int r = 0; r < 3; r++)
			{
				m[r * 4 + 0] = rows[r].x * t.scale;
				m[r * 4 + 1] = rows[r].y * t.scale;
				m[r * 4 + 2] = rows[r].z * t.scale;
				m[r * 4 + 3] = 0.0f;
			}

			m[12] = t.origin.x;
			m[13] = t.origin.y;
			m[14] = t.origin.z;
			m[15] = 1.0f;
		}

		void placement_of(const float m[16], view::transform& t)
		{
			double d[16];
			for (int i = 0; i < 16; i++)
			{
				d[i] = m[i];
			}

			double angles[3] = {}, scale = 1.0;
			codol::mapgeo::angles_from_axes(d, angles, &scale);

			t.origin = { m[12], m[13], m[14] };
			t.angles = { static_cast<float>(angles[0]), static_cast<float>(angles[1]), static_cast<float>(angles[2]) };
			t.scale = static_cast<float>(scale);
		}

		const char* kind_name(const view::object_kind kind)
		{
			switch (kind)
			{
			case view::object_kind::world_surface: return "World surface";
			case view::object_kind::static_model: return "Static model";
			case view::object_kind::entity_model: return "Entity model";
			case view::object_kind::brush_model: return "Brush model";
			case view::object_kind::placed_model: return "Placed model";
			case view::object_kind::dynamic_mesh: return "Brush";
			}

			return "";
		}

		bool movable(const view::object_kind kind)
		{
			return kind == view::object_kind::static_model || kind == view::object_kind::entity_model
				|| kind == view::object_kind::brush_model || kind == view::object_kind::placed_model;
		}

	}

	struct line_vertex
	{
		float x, y, z;
		DWORD color;
	};

	// ---------------------------------------------------------------------------------------------------------

	bool scene::load(const std::string& map, std::string& error)
	{
		const auto install = components::codol_game::install_path();
		if (install.empty())
		{
			error = "Set the CoDOL install folder in File > Settings.";
			return false;
		}

		if (!game::glob::d3d9_device)
		{
			error = "No Direct3D device.";
			return false;
		}

		if (!renderer.initialised())
		{
			view::options options;
			options.game_root = std::filesystem::u8path(install);
			options.load_model_lods = true;
			options.keep_pick_geometry = true;
			options.keep_light_grid = true;

			if (!renderer.init(game::glob::d3d9_device, options, &error))
			{
				error = "Could not start the CoDOL renderer: " + error;
				return false;
			}
		}

		const auto t0 = std::chrono::steady_clock::now();

		selection.clear();
		outline_dirty = true;

		if (!renderer.load_map(map, &error))
		{
			error = "Could not load " + map + ": " + error;
			return false;
		}

		map_name = map;
		entities = {};

		std::string parse_error;
		codol::mapents::parse_entity_string(renderer.entity_string(), entities, parse_error);

		load_seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
		spawn_index = 0;
		renderer.spawn_camera(0, cam);

		const auto stats = renderer.stats();
		game::printf_to_console("[CoDOL] %s loaded in %.1f s: %d surfaces, %d static models, %d entity models, %d brush models, %d entities\n",
			map.c_str(), load_seconds, stats.world_surface_count, stats.static_model_count, stats.entity_model_count, stats.brush_model_count,
			static_cast<int>(entities.ents.size()));

		for (const auto& warning : renderer.warnings())
		{
			game::printf_to_console("  %s\n", warning.c_str());
		}

		revision++;
		return true;
	}

	bool scene::is_selected(const view::object_ref& ref) const
	{
		return std::find(selection.begin(), selection.end(), ref) != selection.end();
	}

	void scene::select(const view::object_ref& ref, const bool add)
	{
		if (!add)
		{
			selection.clear();
			selection.push_back(ref);
		}
		else if (const auto it = std::find(selection.begin(), selection.end(), ref); it != selection.end())
		{
			selection.erase(it);
		}
		else
		{
			selection.push_back(ref);
		}

		outline_dirty = true;
	}

	void scene::deselect()
	{
		selection.clear();
		outline_dirty = true;
	}

	void scene::hide_selection()
	{
		for (const auto& ref : selection)
		{
			renderer.set_hidden(ref, true);
		}

		deselect();
	}

	const codol::mapents::entity* scene::entity_of(const int index) const
	{
		return index >= 0 && static_cast<std::size_t>(index) < entities.ents.size() ? &entities.ents[static_cast<std::size_t>(index)] : nullptr;
	}

	// ---------------------------------------------------------------------------------------------------------
}

namespace ggui
{
	using editor::float3;
	namespace view = codol::view;

	struct codol_viewport_dialog::impl
	{
		std::string map_input;
		std::string error;

		bool world = true;
		bool static_models = true;
		bool entity_models = true;
		bool brush_models = true;
		bool placed_models = true;
		bool sun_shadows = true;
		bool wireframe = false;
		bool model_lods = false;
		bool show_outline = true;

		float fov = 80.0f;
		float fly_speed = 600.0f;

		bool next_spawn = false;
		bool overview = false;
		bool focus = false;

		ImGuizmo::OPERATION gizmo = ImGuizmo::TRANSLATE;
		bool gizmo_world = true;
		bool gizmo_was_using = false;
		float snap_move = 8.0f;
		float snap_angle = 15.0f;
		bool snap = false;

		ImVec2 press_pos = {};
		bool press_valid = false;

		// outline of the selection ( world space line list )
		std::vector<editor::line_vertex> outline;
		std::vector<editor::line_vertex> boxes;

		// browsers
		bool show_models = false;
		bool show_materials = false;
		bool show_selection = true;
		std::string model_filter;
		std::string material_filter;
		int material_group = 0;
		std::string picked_material;

		void rebuild_outline();
		void draw_overlay(const view::overlay_context& ctx);
		void draw_selection_window();
		void draw_models_window();
		void draw_materials_window();
	};

	codol_viewport_dialog::codol_viewport_dialog() : m(std::make_unique<impl>())
	{
		// console commands ( also how the editor is driven in unattended tests )

		// ed_pick [yaw offset] [pitch offset]: select what the camera looks at
		command::register_command("ed_pick", [](const std::vector<std::string>& args)
		{
			auto& s = editor::get_scene();
			auto c = s.cam;
			c.yaw += args.size() > 1 ? utils::try_stof(args[1]) : 0.0f;
			c.pitch += args.size() > 2 ? utils::try_stof(args[2]) : 0.0f;

			view::pick_result hit;
			if (s.renderer.pick_ray(c.origin, c.axes().forward, hit))
			{
				s.select(hit.object, args.size() > 3);
				game::printf_to_console("picked %s #%d at %.0f units ( %s )\n", editor::kind_name(hit.object.kind), hit.object.id, hit.distance, hit.material.c_str());
			}
			else
			{
				game::printf_to_console("nothing there\n");
			}
		});

		// ed_camera x y z [pitch yaw]
		command::register_command("ed_camera", [](const std::vector<std::string>& args)
		{
			auto& s = editor::get_scene();
			if (args.size() > 3)
			{
				s.cam.origin = { utils::try_stof(args[1]), utils::try_stof(args[2]), utils::try_stof(args[3]) };
			}

			if (args.size() > 5)
			{
				s.cam.pitch = utils::try_stof(args[4]);
				s.cam.yaw = utils::try_stof(args[5]);
			}
		});

		// ed_place <xmodel>: put a model where the camera looks
		command::register_command("ed_place", [](const std::vector<std::string>& args)
		{
			auto& s = editor::get_scene();
			if (args.size() < 2 || !s.renderer.loaded())
			{
				game::printf_to_console("usage: ed_place <xmodel> ( with a map open )\n");
				return;
			}

			view::transform t;
			const auto axes = s.cam.axes();
			t.origin = { s.cam.origin.x + axes.forward.x * 300.0f, s.cam.origin.y + axes.forward.y * 300.0f, s.cam.origin.z + axes.forward.z * 300.0f };

			view::pick_result hit;
			if (s.renderer.pick_ray(s.cam.origin, axes.forward, hit))
			{
				t.origin = hit.point;
			}

			std::string error;
			if (const int handle = s.renderer.add_model(args[1], t, &error))
			{
				s.select({ view::object_kind::placed_model, handle }, false);
				s.revision++;
			}
			else
			{
				game::printf_to_console("^1%s\n", error.c_str());
			}
		});

		// ed_move dx dy dz: move the selection
		command::register_command("ed_move", [](const std::vector<std::string>& args)
		{
			auto& s = editor::get_scene();
			if (args.size() < 4)
			{
				return;
			}

			for (const auto& ref : s.selection)
			{
				view::transform t;
				if (editor::movable(ref.kind) && s.renderer.get_transform(ref, t))
				{
					t.origin = { t.origin.x + utils::try_stof(args[1]), t.origin.y + utils::try_stof(args[2]), t.origin.z + utils::try_stof(args[3]) };
					s.renderer.set_transform(ref, t);
				}
			}

			s.outline_dirty = true;
			s.revision++;
		});

		command::register_command("ed_windows", [this](const std::vector<std::string>& args)
		{
			m->show_models = args.size() > 1 && args[1].find('m') != std::string::npos;
			m->show_materials = args.size() > 1 && args[1].find('t') != std::string::npos;
			m->show_selection = args.size() < 2 || args[1].find('s') != std::string::npos;
		});
	}

	codol_viewport_dialog::~codol_viewport_dialog() = default;

	void codol_viewport_dialog::unload()
	{
		editor::get_scene().renderer.unload();
	}

	bool codol_viewport_dialog::load_map(const std::string& map_name)
	{
		m->error.clear();

		if (!editor::get_scene().load(map_name, m->error))
		{
			game::printf_to_console("^1[CoDOL] %s\n", m->error.c_str());
			return false;
		}

		m->map_input = map_name;
		this->open();
		return true;
	}

	void codol_viewport_dialog::on_device_lost()
	{
		if (auto& s = editor::get_scene(); s.renderer.initialised())
		{
			s.renderer.on_device_lost();
		}
	}

	void codol_viewport_dialog::on_device_reset()
	{
		if (auto& s = editor::get_scene(); s.renderer.initialised())
		{
			s.renderer.on_device_reset();
		}
	}

	// ---------------------------------------------------------------------------------------------------------

	void codol_viewport_dialog::impl::rebuild_outline()
	{
		auto& s = editor::get_scene();

		outline.clear();
		boxes.clear();
		s.outline_dirty = false;

		constexpr DWORD line_color = 0xFFFFA020;
		constexpr DWORD box_color = 0xFFFFE060;

		std::vector<float3> tris;

		for (const auto& ref : s.selection)
		{
			tris.clear();

			// triangles of big objects would only be noise ( and slow ): those get the box alone
			if (s.renderer.object_triangles(ref, tris) && tris.size() <= 3 * 60000)
			{
				for (std::size_t i = 0; i + 2 < tris.size(); i += 3)
				{
					for (int e = 0; e < 3; e++)
					{
						const auto& a = tris[i + static_cast<std::size_t>(e)];
						const auto& b = tris[i + static_cast<std::size_t>((e + 1) % 3)];
						outline.push_back({ a.x, a.y, a.z, line_color });
						outline.push_back({ b.x, b.y, b.z, line_color });
					}
				}
			}

			view::oriented_box box;
			if (s.renderer.object_box(ref, box))
			{
				float3 c[8];
				for (int i = 0; i < 8; i++)
				{
					const float sx = (i & 1) ? 1.0f : -1.0f, sy = (i & 2) ? 1.0f : -1.0f, sz = (i & 4) ? 1.0f : -1.0f;
					c[i] = {
						box.center.x + box.axis[0].x * box.half_size.x * sx + box.axis[1].x * box.half_size.y * sy + box.axis[2].x * box.half_size.z * sz,
						box.center.y + box.axis[0].y * box.half_size.x * sx + box.axis[1].y * box.half_size.y * sy + box.axis[2].y * box.half_size.z * sz,
						box.center.z + box.axis[0].z * box.half_size.x * sx + box.axis[1].z * box.half_size.y * sy + box.axis[2].z * box.half_size.z * sz };
				}

				static constexpr int edges[12][2] = { { 0, 1 }, { 2, 3 }, { 4, 5 }, { 6, 7 }, { 0, 2 }, { 1, 3 }, { 4, 6 }, { 5, 7 }, { 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 } };

				for (const auto& e : edges)
				{
					boxes.push_back({ c[e[0]].x, c[e[0]].y, c[e[0]].z, box_color });
					boxes.push_back({ c[e[1]].x, c[e[1]].y, c[e[1]].z, box_color });
				}
			}
		}
	}

	void codol_viewport_dialog::impl::draw_overlay(const view::overlay_context& ctx)
	{
		if (!show_outline || (outline.empty() && boxes.empty()))
		{
			return;
		}

		auto* device = ctx.device;

		const auto draw = [&](const std::vector<editor::line_vertex>& lines)
		{
			constexpr std::size_t chunk = 20000; // lines per call

			for (std::size_t first = 0; first < lines.size(); first += chunk * 2)
			{
				const auto count = std::min(chunk, (lines.size() - first) / 2);
				device->DrawPrimitiveUP(D3DPT_LINELIST, static_cast<UINT>(count), lines.data() + first, sizeof(editor::line_vertex));
			}
		};

		device->SetFVF(D3DFVF_XYZ | D3DFVF_DIFFUSE);
		device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);

		// the wires lie exactly on the surfaces: pull them towards the camera
		const float bias = -0.00003f;
		device->SetRenderState(D3DRS_DEPTHBIAS, *reinterpret_cast<const DWORD*>(&bias));
		draw(outline);

		// the box is visible through everything
		device->SetRenderState(D3DRS_ZENABLE, FALSE);
		draw(boxes);
	}

	// ---------------------------------------------------------------------------------------------------------

	void codol_viewport_dialog::impl::draw_selection_window()
	{
		if (!show_selection)
		{
			return;
		}

		auto& s = editor::get_scene();

		ImGui::SetNextWindowSize(ImVec2(380.0f, 520.0f), ImGuiCond_FirstUseEver);

		if (!ImGui::Begin("Selection##window", &show_selection, ImGuiWindowFlags_NoCollapse))
		{
			ImGui::End();
			return;
		}

		if (s.selection.empty())
		{
			ImGui::TextDisabled("Nothing selected.\nClick something in the viewport ( Ctrl + click adds ).");
			ImGui::End();
			return;
		}

		if (s.selection.size() > 1)
		{
			ImGui::Text("%d objects selected", static_cast<int>(s.selection.size()));
			ImGui::Separator();
		}

		const auto ref = s.selection.back();

		view::scene_object object;
		if (!s.renderer.object(ref, object))
		{
			ImGui::TextDisabled("The object no longer exists.");
			ImGui::End();
			return;
		}

		ImGui::TextColored(ImVec4(1.0f, 0.72f, 0.25f, 1.0f), "%s", editor::kind_name(ref.kind));
		ImGui::SameLine();
		ImGui::TextDisabled("#%d", ref.id);
		ImGui::TextWrapped("%s", object.name.c_str());

		if (ref.kind == view::object_kind::world_surface && !object.name.empty())
		{
			if (auto* thumb = s.renderer.material_thumbnail(object.name, 128))
			{
				ImGui::Image(thumb, ImVec2(128.0f, 128.0f));
			}

			view::material_info info;
			if (s.renderer.material_details(object.name, info))
			{
				ImGui::TextDisabled("%s   %d x %d   %s", info.technique_set.c_str(), info.width, info.height, info.zone.c_str());
			}
		}

		ImGui::Spacing();

		// ---- placement
		view::transform t;
		if (editor::movable(ref.kind) && s.renderer.get_transform(ref, t))
		{
			bool changed = false;
			float origin[3] = { t.origin.x, t.origin.y, t.origin.z };
			float angles[3] = { t.angles.x, t.angles.y, t.angles.z };

			ImGui::SetNextItemWidth(-70.0f);
			changed |= ImGui::DragFloat3("origin", origin, 1.0f, 0.0f, 0.0f, "%.1f");
			ImGui::SetNextItemWidth(-70.0f);
			changed |= ImGui::DragFloat3("angles", angles, 0.5f, 0.0f, 0.0f, "%.1f");
			ImGui::SetNextItemWidth(-70.0f);
			changed |= ImGui::DragFloat("scale", &t.scale, 0.01f, 0.01f, 100.0f, "%.3f");

			if (changed)
			{
				t.origin = { origin[0], origin[1], origin[2] };
				t.angles = { angles[0], angles[1], angles[2] };
				s.renderer.set_transform(ref, t);
				s.outline_dirty = true;
				s.revision++;
			}

			if (ref.kind != view::object_kind::placed_model && object.moved)
			{
				if (ImGui::Button("Reset placement"))
				{
					s.renderer.reset_transform(ref);
					s.outline_dirty = true;
					s.revision++;
				}
			}
		}

		if (ImGui::Button("Hide"))
		{
			s.hide_selection();
			ImGui::End();
			return;
		}

		ImGui::SameLine();
		if (ImGui::Button("Show all"))
		{
			s.renderer.show_all();
		}

		if (ref.kind == view::object_kind::placed_model)
		{
			ImGui::SameLine();
			if (ImGui::Button("Delete"))
			{
				s.renderer.remove_model(ref.id);
				s.deselect();
				s.revision++;
				ImGui::End();
				return;
			}
		}

		// ---- entity
		if (const auto* ent = s.entity_of(object.entity))
		{
			ImGui::Spacing();
			ImGui::Separator();
			ImGui::TextDisabled("entity %d", object.entity);

			if (ImGui::BeginTable("##ent_kv", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_Resizable))
			{
				for (const auto& [key, value] : ent->pairs)
				{
					ImGui::TableNextRow();
					ImGui::TableNextColumn();
					ImGui::TextUnformatted(key.c_str());
					ImGui::TableNextColumn();
					ImGui::TextUnformatted(value.c_str());
				}

				ImGui::EndTable();
			}
		}

		ImGui::End();
	}

	void codol_viewport_dialog::impl::draw_models_window()
	{
		if (!show_models)
		{
			return;
		}

		auto& s = editor::get_scene();

		ImGui::SetNextWindowSize(ImVec2(380.0f, 600.0f), ImGuiCond_FirstUseEver);

		if (!ImGui::Begin("Models##window", &show_models, ImGuiWindowFlags_NoCollapse))
		{
			ImGui::End();
			return;
		}

		if (!s.renderer.loaded())
		{
			ImGui::TextDisabled("Open a map first: the models of its zones and of the shared zones are listed here.");
			ImGui::End();
			return;
		}

		const auto& names = s.renderer.model_names();

		ImGui::SetNextItemWidth(-1.0f);
		ImGui::InputTextWithHint("##model_filter", "filter", &model_filter);
		ImGui::TextDisabled("%d models. Double click places one in front of the camera.", static_cast<int>(names.size()));

		static std::vector<int> filtered;
		static std::string filtered_for = "\x01";
		static std::size_t filtered_count = 0;

		if (filtered_for != model_filter || filtered_count != names.size())
		{
			filtered_for = model_filter;
			filtered_count = names.size();
			filtered.clear();

			const auto needle = codol::script::to_lower(model_filter);
			for (std::size_t i = 0; i < names.size(); i++)
			{
				if (needle.empty() || names[i].find(needle) != std::string::npos)
				{
					filtered.push_back(static_cast<int>(i));
				}
			}
		}

		if (ImGui::BeginChild("##model_list", ImVec2(0.0f, 0.0f), true))
		{
			ImGuiListClipper clipper;
			clipper.Begin(static_cast<int>(filtered.size()));

			while (clipper.Step())
			{
				for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++)
				{
					const auto& name = names[static_cast<std::size_t>(filtered[static_cast<std::size_t>(row)])];

					if (ImGui::Selectable(name.c_str(), false, ImGuiSelectableFlags_AllowDoubleClick) && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
					{
						// on what the camera looks at, else a few steps ahead
						const auto axes = s.cam.axes();
						view::transform t;
						t.origin = { s.cam.origin.x + axes.forward.x * 300.0f, s.cam.origin.y + axes.forward.y * 300.0f, s.cam.origin.z + axes.forward.z * 300.0f };

						view::pick_result hit;
						if (s.renderer.pick_ray(s.cam.origin, axes.forward, hit) && hit.distance < 4000.0f)
						{
							t.origin = hit.point;
						}

						t.angles = { 0.0f, s.cam.yaw + 180.0f, 0.0f };

						std::string error;
						if (const int handle = s.renderer.add_model(name, t, &error))
						{
							s.select({ view::object_kind::placed_model, handle }, false);
							s.revision++;
						}
						else
						{
							ImGui::Toast(ImGuiToastType_Error, "Models", ("Could not place " + name + ": " + error).c_str());
						}
					}
				}
			}
		}

		ImGui::EndChild();
		ImGui::End();
	}

	void codol_viewport_dialog::impl::draw_materials_window()
	{
		if (!show_materials)
		{
			return;
		}

		auto& s = editor::get_scene();

		ImGui::SetNextWindowSize(ImVec2(560.0f, 600.0f), ImGuiCond_FirstUseEver);

		if (!ImGui::Begin("Materials##window", &show_materials, ImGuiWindowFlags_NoCollapse))
		{
			ImGui::End();
			return;
		}

		if (!s.renderer.loaded())
		{
			ImGui::TextDisabled("Open a map first: the materials of its zones and of the shared zones are listed here.");
			ImGui::End();
			return;
		}

		const auto& names = s.renderer.material_names();

		static const char* groups[] = { "World ( wc/ )", "Models ( mc/ )", "All" };
		ImGui::SetNextItemWidth(160.0f);
		ImGui::Combo("##material_group", &material_group, groups, 3);
		ImGui::SameLine();
		ImGui::SetNextItemWidth(-1.0f);
		ImGui::InputTextWithHint("##material_filter", "filter", &material_filter);

		static std::vector<int> filtered;
		static std::string filtered_for = "\x01";
		static int filtered_group = -1;
		static std::size_t filtered_count = 0;

		if (filtered_for != material_filter || filtered_group != material_group || filtered_count != names.size())
		{
			filtered_for = material_filter;
			filtered_group = material_group;
			filtered_count = names.size();
			filtered.clear();

			const auto needle = codol::script::to_lower(material_filter);
			const char* prefix = material_group == 0 ? "wc/" : material_group == 1 ? "mc/" : "";

			for (std::size_t i = 0; i < names.size(); i++)
			{
				if (names[i].rfind(prefix, 0) == 0 && (needle.empty() || names[i].find(needle) != std::string::npos))
				{
					filtered.push_back(static_cast<int>(i));
				}
			}
		}

		ImGui::TextDisabled("%d materials%s%s", static_cast<int>(filtered.size()), picked_material.empty() ? "" : "   selected: ", picked_material.c_str());

		if (ImGui::BeginChild("##material_grid", ImVec2(0.0f, 0.0f), true))
		{
			constexpr float cell = 104.0f, image = 96.0f;
			const int columns = std::max(1, static_cast<int>(ImGui::GetContentRegionAvail().x / cell));
			const int rows = (static_cast<int>(filtered.size()) + columns - 1) / columns;

			ImGuiListClipper clipper;
			clipper.Begin(rows, cell + ImGui::GetTextLineHeightWithSpacing());

			while (clipper.Step())
			{
				for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++)
				{
					for (int col = 0; col < columns; col++)
					{
						const int index = row * columns + col;
						if (index >= static_cast<int>(filtered.size()))
						{
							break;
						}

						const auto& name = names[static_cast<std::size_t>(filtered[static_cast<std::size_t>(index)])];

						if (col > 0)
						{
							ImGui::SameLine(static_cast<float>(col) * cell);
						}

						ImGui::PushID(index);
						ImGui::BeginGroup();

						const ImVec2 pos = ImGui::GetCursorScreenPos();
						const bool chosen = name == picked_material;

						if (auto* thumb = s.renderer.material_thumbnail(name, 96))
						{
							ImGui::Image(thumb, ImVec2(image, image), ImVec2(0, 0), ImVec2(1, 1), ImVec4(1, 1, 1, 1),
								chosen ? ImVec4(1.0f, 0.72f, 0.25f, 1.0f) : ImVec4(0.25f, 0.26f, 0.28f, 1.0f));
						}
						else
						{
							ImGui::Dummy(ImVec2(image, image));
							ImGui::GetWindowDrawList()->AddRect(pos, ImVec2(pos.x + image, pos.y + image), IM_COL32(70, 72, 76, 255));
						}

						if (ImGui::IsItemClicked())
						{
							picked_material = name;
						}

						if (ImGui::IsItemHovered())
						{
							view::material_info info;
							if (s.renderer.material_details(name, info))
							{
								ImGui::SetTooltip("%s\n%s\n%d x %d\n%s", name.c_str(), info.technique_set.c_str(), info.width, info.height, info.zone.c_str());
							}
							else
							{
								ImGui::SetTooltip("%s", name.c_str());
							}
						}

						// the part after the last '/' fits under the image
						const auto slash = name.find_last_of('/');
						const char* label = name.c_str() + (slash == std::string::npos ? 0 : slash + 1);
						ImGui::PushClipRect(pos, ImVec2(pos.x + image - 2.0f, pos.y + cell + ImGui::GetTextLineHeightWithSpacing()), true);
						ImGui::TextUnformatted(label);
						ImGui::PopClipRect();

						ImGui::EndGroup();
						ImGui::PopID();
					}
				}
			}
		}

		ImGui::EndChild();
		ImGui::End();
	}

	// ---------------------------------------------------------------------------------------------------------

	bool codol_viewport_dialog::gui()
	{
		auto& s = editor::get_scene();
		auto& cam = s.cam;

		ImGuizmo::BeginFrame();

		ImGui::SetNextWindowSizeConstraints(ImVec2(480.0f, 320.0f), ImVec2(FLT_MAX, FLT_MAX));
		ImGui::SetNextWindowSize(ImVec2(1100.0f, 700.0f), ImGuiCond_FirstUseEver);

		if (!ImGui::Begin("CoDOL Viewport##window", this->get_p_open(), ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
		{
			ImGui::End();
			m->draw_selection_window();
			m->draw_models_window();
			m->draw_materials_window();
			return false;
		}

		const bool loaded = s.renderer.loaded();

		// ---- toolbar
		const auto tool = [&](const char* label, const ImGuizmo::OPERATION op, const char* tip)
		{
			const bool on = m->gizmo == op;
			if (on)
			{
				ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
			}

			if (ImGui::Button(label))
			{
				m->gizmo = op;
			}

			if (on)
			{
				ImGui::PopStyleColor();
			}

			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("%s", tip);
			}

			ImGui::SameLine();
		};

		tool("Move", ImGuizmo::TRANSLATE, "Move the selection ( W )");
		tool("Rotate", ImGuizmo::ROTATE, "Rotate the selection ( E )");
		tool("Scale", ImGuizmo::SCALEU, "Scale the selection ( R )");

		ImGui::Checkbox("World axes", &m->gizmo_world);
		ImGui::SameLine();
		ImGui::Checkbox("Snap", &m->snap);

		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Move in steps of %.0f units, rotate in steps of %.0f degrees ( Camera menu ).\nHold Ctrl while dragging for the opposite.", m->snap_move, m->snap_angle);
		}

		ImGui::SameLine(0.0f, 16.0f);
		if (ImGui::Button("Show"))
		{
			ImGui::OpenPopup("##vp_show");
		}

		if (ImGui::BeginPopup("##vp_show"))
		{
			ImGui::Checkbox("World", &m->world);
			ImGui::Checkbox("Static models", &m->static_models);
			ImGui::Checkbox("Entity models", &m->entity_models);
			ImGui::Checkbox("Brush models", &m->brush_models);
			ImGui::Checkbox("Placed models", &m->placed_models);
			ImGui::Separator();
			ImGui::Checkbox("Sun shadows", &m->sun_shadows);
			ImGui::Checkbox("Model LODs", &m->model_lods);
			ImGui::Checkbox("Wireframe", &m->wireframe);
			ImGui::Checkbox("Selection outline", &m->show_outline);
			ImGui::Separator();

			if (ImGui::MenuItem("Show everything hidden", "Alt+H", false, loaded))
			{
				s.renderer.show_all();
			}

			ImGui::EndPopup();
		}

		ImGui::SameLine();
		if (ImGui::Button("Camera"))
		{
			ImGui::OpenPopup("##vp_camera");
		}

		if (ImGui::BeginPopup("##vp_camera"))
		{
			ImGui::SetNextItemWidth(220.0f);
			ImGui::SliderFloat("Fly speed", &m->fly_speed, 50.0f, 20000.0f, "%.0f units/s", ImGuiSliderFlags_Logarithmic);
			ImGui::SetNextItemWidth(220.0f);
			ImGui::SliderFloat("Field of view", &m->fov, 40.0f, 120.0f, "%.0f deg");
			ImGui::SetNextItemWidth(220.0f);
			ImGui::DragFloat("Move snap", &m->snap_move, 1.0f, 1.0f, 512.0f, "%.0f units");
			ImGui::SetNextItemWidth(220.0f);
			ImGui::DragFloat("Angle snap", &m->snap_angle, 1.0f, 1.0f, 90.0f, "%.0f deg");
			ImGui::Separator();

			if (ImGui::MenuItem("Next spawn point", "N", false, loaded)) { m->next_spawn = true; }
			if (ImGui::MenuItem("Whole map from above", "Home", false, loaded)) { m->overview = true; }
			if (ImGui::MenuItem("Go to the selection", "F", false, loaded && !s.selection.empty())) { m->focus = true; }
			ImGui::EndPopup();
		}

		ImGui::SameLine(0.0f, 16.0f);
		ImGui::Checkbox("Selection", &m->show_selection);
		ImGui::SameLine();
		ImGui::Checkbox("Models", &m->show_models);
		ImGui::SameLine();
		ImGui::Checkbox("Materials", &m->show_materials);

		if (!m->error.empty())
		{
			ImGui::TextColored(ImVec4(0.96f, 0.45f, 0.4f, 1.0f), "%s", m->error.c_str());
		}

		const ImVec2 avail = ImGui::GetContentRegionAvail();
		const float status_h = ImGui::GetTextLineHeightWithSpacing();
		const int width = static_cast<int>(avail.x);
		const int height = static_cast<int>(avail.y - status_h);

		if (!loaded || width < 16 || height < 16)
		{
			ImGui::Dummy(ImVec2(0.0f, 16.0f));
			ImGui::Indent(16.0f);
			ImGui::PushTextWrapPos(0.0f);
			ImGui::TextDisabled("Open a map: double click one in the Maps window, or File > Open map.");
			ImGui::PopTextWrapPos();
			ImGui::Unindent(16.0f);
			ImGui::End();

			m->draw_selection_window();
			m->draw_models_window();
			m->draw_materials_window();
			return true;
		}

		// ---- camera requests
		cam.fov_x = m->fov;

		if (m->next_spawn)
		{
			m->next_spawn = false;

			if (!s.renderer.spawn_camera(++s.spawn_index, cam))
			{
				s.spawn_index = 0;
				s.renderer.spawn_camera(0, cam);
			}
		}

		if (m->overview)
		{
			m->overview = false;

			float3 center, half;
			if (s.renderer.world_bounds(center, half))
			{
				const float reach = std::max(half.x, half.y) / std::tan(m->fov * 0.5f * editor::DEG);
				cam.origin = { center.x, center.y, center.z + half.z + reach };
				cam.pitch = 89.0f;
				cam.yaw = 90.0f;
				cam.roll = 0.0f;
			}
		}

		if (m->focus)
		{
			m->focus = false;

			view::oriented_box box;
			if (!s.selection.empty() && s.renderer.object_box(s.selection.back(), box))
			{
				const float radius = std::max(32.0f, std::sqrt(box.half_size.x * box.half_size.x + box.half_size.y * box.half_size.y + box.half_size.z * box.half_size.z));
				const auto axes = cam.axes();
				const float back = radius / std::tan(m->fov * 0.5f * editor::DEG) * 1.4f;
				cam.origin = { box.center.x - axes.forward.x * back, box.center.y - axes.forward.y * back, box.center.z - axes.forward.z * back };
			}
		}

		if (s.outline_dirty)
		{
			m->rebuild_outline();
		}

		// ---- draw
		view::frame_options frame;
		frame.game_time = static_cast<float>(ImGui::GetTime());
		frame.draw_world = m->world;
		frame.draw_static_models = m->static_models;
		frame.draw_entity_models = m->entity_models;
		frame.draw_brush_models = m->brush_models;
		frame.draw_placed_models = m->placed_models;
		frame.sun_shadows = m->sun_shadows;
		frame.model_lods = m->model_lods;
		frame.wireframe = m->wireframe;
		frame.clear_color = 0xFF202020;
		frame.overlay = [this](const view::overlay_context& ctx) { m->draw_overlay(ctx); };

		IDirect3DTexture9* texture = s.renderer.render(cam, width, height, frame);
		const ImVec2 image_pos = ImGui::GetCursorScreenPos();
		auto* dl = ImGui::GetWindowDrawList();

		if (texture)
		{
			// the alpha channel of the frame is whatever the game's shaders left there: draw it opaque
			dl->AddCallback([](const ImDrawList*, const ImDrawCmd*)
			{
				if (game::glob::d3d9_device)
				{
					game::glob::d3d9_device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
				}
			}, nullptr);

			ImGui::Image(texture, ImVec2(static_cast<float>(width), static_cast<float>(height)));
			dl->AddCallback(ImDrawCallback_ResetRenderState, nullptr);
		}
		else
		{
			ImGui::Dummy(ImVec2(static_cast<float>(width), static_cast<float>(height)));
			dl->AddText(ImVec2(image_pos.x + 16.0f, image_pos.y + 16.0f), IM_COL32(245, 115, 100, 255), ("render failed: " + s.renderer.last_error()).c_str());
		}

		// ---- input area
		ImGui::SetCursorScreenPos(image_pos);
		ImGui::InvisibleButton("##vp_nav", ImVec2(static_cast<float>(width), static_cast<float>(height)),
			ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight | ImGuiButtonFlags_MouseButtonMiddle);

		const bool hovered = ImGui::IsItemHovered();
		const bool active = ImGui::IsItemActive();
		const auto& io = ImGui::GetIO();

		// ---- gizmo on the last selected movable object
		bool gizmo_busy = false;

		if (!s.selection.empty() && editor::movable(s.selection.back().kind))
		{
			const auto ref = s.selection.back();
			view::transform t;

			if (s.renderer.get_transform(ref, t))
			{
				float view_m[16], proj_m[16], world_m[16];
				view::view_matrix(cam, view_m);
				view::projection_matrix(cam, width, height, proj_m);

				// the renderer's projection has no far plane; the gizmo wants one
				const float n = cam.z_near, f = 262144.0f;
				proj_m[10] = f / (f - n);
				proj_m[14] = -n * f / (f - n);
				proj_m[11] = 1.0f;
				proj_m[15] = 0.0f;

				editor::matrix_of(t, world_m);

				ImGuizmo::SetOrthographic(false);
				ImGuizmo::SetDrawlist(dl);
				ImGuizmo::SetRect(image_pos.x, image_pos.y, static_cast<float>(width), static_cast<float>(height));

				const bool snap = m->snap != io.KeyCtrl;
				const float step = m->gizmo == ImGuizmo::ROTATE ? m->snap_angle : m->gizmo == ImGuizmo::TRANSLATE ? m->snap_move : 0.1f;
				const float snap_values[3] = { step, step, step };

				dl->PushClipRect(image_pos, ImVec2(image_pos.x + static_cast<float>(width), image_pos.y + static_cast<float>(height)), true);

				if (ImGuizmo::Manipulate(view_m, proj_m, m->gizmo, m->gizmo_world && m->gizmo != ImGuizmo::SCALEU ? ImGuizmo::WORLD : ImGuizmo::LOCAL,
					world_m, nullptr, snap ? snap_values : nullptr))
				{
					view::transform moved;
					editor::placement_of(world_m, moved);

					// only what the tool is for changes: no drift in the other parts
					if (m->gizmo == ImGuizmo::TRANSLATE) { t.origin = moved.origin; }
					else if (m->gizmo == ImGuizmo::ROTATE) { t.angles = moved.angles; t.origin = moved.origin; }
					else { t.scale = std::clamp(moved.scale, 0.01f, 100.0f); }

					s.renderer.set_transform(ref, t);
					s.outline_dirty = true;
					s.revision++;
				}

				dl->PopClipRect();
				gizmo_busy = ImGuizmo::IsUsing() || ImGuizmo::IsOver();
			}
		}

		// ---- navigation and picking
		{
			const auto axes = cam.axes();
			const float boost = io.KeyShift ? 4.0f : 1.0f;

			const auto move = [&](const float3& dir, const float amount)
			{
				cam.origin.x += dir.x * amount;
				cam.origin.y += dir.y * amount;
				cam.origin.z += dir.z * amount;
			};

			const bool lmb = active && ImGui::IsMouseDown(ImGuiMouseButton_Left);
			const bool rmb = active && ImGui::IsMouseDown(ImGuiMouseButton_Right);
			const bool mmb = active && ImGui::IsMouseDown(ImGuiMouseButton_Middle);
			const float dx = io.MouseDelta.x, dy = io.MouseDelta.y;
			const float drag = m->fly_speed * boost / 200.0f;

			if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
			{
				m->press_pos = io.MousePos;
				m->press_valid = !gizmo_busy;
			}

			const bool dragged = std::fabs(io.MousePos.x - m->press_pos.x) + std::fabs(io.MousePos.y - m->press_pos.y) > 4.0f;

			if (mmb || (lmb && rmb))
			{
				move(axes.left, dx * drag);
				move(axes.up, dy * drag);
				ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
			}
			else if (rmb)
			{
				cam.yaw -= dx * 0.15f;
				cam.pitch = std::clamp(cam.pitch + dy * 0.15f, -89.0f, 89.0f);

				const float step = m->fly_speed * boost * io.DeltaTime;

				if (ImGui::IsKeyDown(ImGuiKey_W)) { move(axes.forward, step); }
				if (ImGui::IsKeyDown(ImGuiKey_S)) { move(axes.forward, -step); }
				if (ImGui::IsKeyDown(ImGuiKey_A)) { move(axes.left, step); }
				if (ImGui::IsKeyDown(ImGuiKey_D)) { move(axes.left, -step); }
				if (ImGui::IsKeyDown(ImGuiKey_E) || ImGui::IsKeyDown(ImGuiKey_Space)) { move({ 0.0f, 0.0f, 1.0f }, step); }
				if (ImGui::IsKeyDown(ImGuiKey_Q) || ImGui::IsKeyDown(ImGuiKey_C)) { move({ 0.0f, 0.0f, 1.0f }, -step); }

				if (io.MouseWheel != 0.0f)
				{
					m->fly_speed = std::clamp(m->fly_speed * std::pow(1.25f, io.MouseWheel), 50.0f, 20000.0f);
				}

				ImGui::SetMouseCursor(ImGuiMouseCursor_None);
			}
			else if (lmb && m->press_valid && dragged && !ImGuizmo::IsUsing())
			{
				// drive: up / down moves along the ground, left / right turns
				const float yaw = cam.yaw * editor::DEG;
				move({ std::cos(yaw), std::sin(yaw), 0.0f }, -dy * drag);
				cam.yaw -= dx * 0.15f;
			}
			else if (hovered)
			{
				if (io.MouseWheel != 0.0f)
				{
					move(axes.forward, io.MouseWheel * m->fly_speed * boost * 0.25f);
				}

				if (!io.WantTextInput)
				{
					if (ImGui::IsKeyPressed(ImGuiKey_N, false)) { m->next_spawn = true; }
					if (ImGui::IsKeyPressed(ImGuiKey_Home, false)) { m->overview = true; }
					if (ImGui::IsKeyPressed(ImGuiKey_F, false)) { m->focus = true; }
					if (ImGui::IsKeyPressed(ImGuiKey_W, false)) { m->gizmo = ImGuizmo::TRANSLATE; }
					if (ImGui::IsKeyPressed(ImGuiKey_E, false)) { m->gizmo = ImGuizmo::ROTATE; }
					if (ImGui::IsKeyPressed(ImGuiKey_R, false)) { m->gizmo = ImGuizmo::SCALEU; }
					if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) { s.deselect(); }

					if (ImGui::IsKeyPressed(ImGuiKey_H, false))
					{
						if (io.KeyAlt)
						{
							s.renderer.show_all();
						}
						else
						{
							s.hide_selection();
						}
					}
				}
			}

			// a click ( press and release without dragging ) selects
			if (m->press_valid && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
			{
				m->press_valid = false;

				if (!dragged && !gizmo_busy)
				{
					view::pick_options options;
					options.world = m->world;
					options.static_models = m->static_models;
					options.entity_models = m->entity_models;
					options.brush_models = m->brush_models;
					options.placed_models = m->placed_models;

					view::pick_result hit;
					if (s.renderer.pick(cam, width, height, io.MousePos.x - image_pos.x, io.MousePos.y - image_pos.y, hit, options))
					{
						s.select(hit.object, io.KeyCtrl || io.KeyShift);
					}
					else if (!io.KeyCtrl && !io.KeyShift)
					{
						s.deselect();
					}
				}
			}
		}

		// ---- hint
		if (hovered && !active && !gizmo_busy)
		{
			const char* hint = "Click: select ( Ctrl adds )    Right drag: look + W A S D fly    Left drag: drive    Middle drag: pan    Wheel: forward / back    W E R: move / rotate / scale    H: hide    F: go to selection";
			const ImVec2 size = ImGui::CalcTextSize(hint);
			const ImVec2 at(image_pos.x + 8.0f, image_pos.y + static_cast<float>(height) - size.y - 10.0f);

			dl->AddRectFilled(ImVec2(at.x - 4.0f, at.y - 3.0f), ImVec2(at.x + size.x + 4.0f, at.y + size.y + 3.0f), IM_COL32(0, 0, 0, 150), 3.0f);
			dl->AddText(at, IM_COL32(230, 230, 230, 255), hint);
		}

		// ---- status
		const auto stats = s.renderer.stats();
		ImGui::TextDisabled("%s   %d x %d   %d draws   %.1f ms   pos %.0f %.0f %.0f   pitch %.0f yaw %.0f   speed %.0f   %d selected   %d hidden   %d moved",
			s.map_name.c_str(), width, height, stats.draw_calls, stats.cpu_ms, cam.origin.x, cam.origin.y, cam.origin.z, cam.pitch, cam.yaw, m->fly_speed,
			static_cast<int>(s.selection.size()), stats.hidden_objects, stats.moved_objects);

		ImGui::End();

		m->draw_selection_window();
		m->draw_models_window();
		m->draw_materials_window();
		return true;
	}

	REGISTER_GUI(codol_viewport_dialog);
}
