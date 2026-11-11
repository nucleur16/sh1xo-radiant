#include "std_include.hpp"

// windows.h macros
#undef min
#undef max

#include <codol_view/map_renderer.hpp>

namespace ggui
{
	struct codol_viewport_dialog::impl
	{
		codol::view::map_renderer renderer;
		bool initialised = false;

		std::string map_input;
		std::string status;
		std::string error;

		// what is drawn
		bool world = true;
		bool static_models = true;
		bool entity_models = true;
		bool brush_models = true;
		bool sun_shadows = true;
		bool wireframe = false;
		bool model_lods = false;

		// camera
		bool follow_radiant = false;
		codol::render::camera cam;
		float fov = 80.0f;
		float fly_speed = 600.0f;

		double load_seconds = 0.0;
	};

	codol_viewport_dialog::codol_viewport_dialog() : m(std::make_unique<impl>())
	{
		set_gui_type(GUI_TYPE_DEF);
	}

	codol_viewport_dialog::~codol_viewport_dialog() = default;

	void codol_viewport_dialog::unload()
	{
		m->renderer.unload();
		m->status.clear();
	}

	bool codol_viewport_dialog::load_map(const std::string& map_name)
	{
		m->error.clear();

		const auto install = components::codol_game::install_path();
		if (install.empty())
		{
			m->error = "Set the CoDOL install folder in Script Workbench > Setup.";
			return false;
		}

		if (!game::glob::d3d9_device)
		{
			m->error = "The renderer of radiant is not up yet.";
			return false;
		}

		std::string error;

		if (!m->initialised)
		{
			codol::view::options options;
			options.game_root = std::filesystem::u8path(install);

			// radiant is a 32 bit process: do not keep a second copy of every texture in system memory
			options.pool = codol::view::resource_pool::default_pool;
			options.load_model_lods = true;

			if (!m->renderer.init(game::glob::d3d9_device, options, &error))
			{
				m->error = "Could not start the CoDOL renderer: " + error;
				game::printf_to_console("^1[CoDOL] %s\n", m->error.c_str());
				return false;
			}

			m->initialised = true;
		}

		const auto t0 = std::chrono::steady_clock::now();

		if (!m->renderer.load_map(map_name, &error))
		{
			m->error = "Could not load " + map_name + ": " + error;
			game::printf_to_console("^1[CoDOL] %s\n", m->error.c_str());
			return false;
		}

		m->load_seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
		m->map_input = map_name;

		// a sensible first view when the camera is not driven by radiant
		m->renderer.spawn_camera(0, m->cam);

		const auto warnings = m->renderer.warnings();
		game::printf_to_console("[CoDOL] viewport: %s loaded in %.1f s ( %d warnings )\n", map_name.c_str(), m->load_seconds, static_cast<int>(warnings.size()));

		for (std::size_t i = 0; i < warnings.size() && i < 20; i++)
		{
			game::printf_to_console("  %s\n", warnings[i].c_str());
		}

		this->open();
		return true;
	}

	void codol_viewport_dialog::on_device_lost()
	{
		if (const auto dlg = GET_GUI(ggui::codol_viewport_dialog); dlg && dlg->m->initialised)
		{
			dlg->m->renderer.on_device_lost();
		}
	}

	void codol_viewport_dialog::on_device_reset()
	{
		if (const auto dlg = GET_GUI(ggui::codol_viewport_dialog); dlg && dlg->m->initialised)
		{
			dlg->m->renderer.on_device_reset();
		}
	}

	bool codol_viewport_dialog::gui()
	{
		ImGui::SetNextWindowSizeConstraints(ImVec2(480.0f, 320.0f), ImVec2(FLT_MAX, FLT_MAX));
		ImGui::SetNextWindowSize(ImVec2(1100.0f, 700.0f), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowPos(ggui::get_initial_window_pos(), ImGuiCond_FirstUseEver);

		if (!ImGui::Begin("CoDOL Viewport##window", this->get_p_open(), ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
		{
			ImGui::End();
			return false;
		}

		// ---- toolbar
		ImGui::SetNextItemWidth(200.0f);
		const bool enter = ImGui::InputTextWithHint("##vp_map", "map zone, eg. mp_rust", &m->map_input, ImGuiInputTextFlags_EnterReturnsTrue);
		ImGui::SameLine();

		if ((ImGui::Button("Load") || enter) && !m->map_input.empty())
		{
			load_map(m->map_input);
		}

		const bool loaded = m->renderer.loaded();

		ImGui::SameLine(0.0f, 16.0f);
		ImGui::Checkbox("World", &m->world);
		ImGui::SameLine();
		ImGui::Checkbox("Static models", &m->static_models);
		ImGui::SameLine();
		ImGui::Checkbox("Entity models", &m->entity_models);
		ImGui::SameLine();
		ImGui::Checkbox("Brush models", &m->brush_models);
		ImGui::SameLine();
		ImGui::Checkbox("Sun shadows", &m->sun_shadows);
		ImGui::SameLine();
		ImGui::Checkbox("LODs", &m->model_lods);
		ImGui::SameLine();
		ImGui::Checkbox("Wireframe", &m->wireframe);
		ImGui::SameLine(0.0f, 16.0f);
		ImGui::Checkbox("Follow radiant camera", &m->follow_radiant);

		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("On: this view shows what the radiant camera looks at.\nOff: fly here with the right mouse button held ( W A S D, Q / E down / up, Shift faster ).");
		}

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
			ImGui::TextDisabled(
				"The map as the game draws it: read from the 1.1.1.13 fastfiles and drawn with the game's own materials and shaders.\n\n"
				"Type the zone name of a map and press Load ( it takes a few seconds ), or import a map in CoDOL > Map Entities.");
			ImGui::PopTextWrapPos();
			ImGui::Unindent(16.0f);
			ImGui::End();
			return true;
		}

		// ---- camera
		auto& cam = m->cam;
		cam.fov_x = m->fov;

		if (m->follow_radiant && cmainframe::activewnd && cmainframe::activewnd->m_pCamWnd)
		{
			const auto& rc = cmainframe::activewnd->m_pCamWnd->camera;
			cam.origin = { rc.origin[0], rc.origin[1], rc.origin[2] };

			// radiant: pitch up is positive, the game: pitch down is positive
			cam.pitch = -rc.angles[0];
			cam.yaw = rc.angles[1];
			cam.roll = 0.0f;
		}

		// ---- draw
		codol::view::frame_options frame;
		frame.game_time = static_cast<float>(ImGui::GetTime());
		frame.draw_world = m->world;
		frame.draw_static_models = m->static_models;
		frame.draw_entity_models = m->entity_models;
		frame.draw_brush_models = m->brush_models;
		frame.sun_shadows = m->sun_shadows;
		frame.model_lods = m->model_lods;
		frame.wireframe = m->wireframe;
		frame.clear_color = 0xFF202020;

		IDirect3DTexture9* texture = m->renderer.render(cam, width, height, frame);
		const ImVec2 image_pos = ImGui::GetCursorScreenPos();

		if (texture)
		{
			// the alpha channel of the frame is whatever the game's shaders left there: draw it opaque
			auto* dl = ImGui::GetWindowDrawList();
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
			ImGui::SetCursorScreenPos(ImVec2(image_pos.x + 16.0f, image_pos.y + 16.0f));
			ImGui::TextColored(ImVec4(0.96f, 0.45f, 0.4f, 1.0f), "render failed: %s", m->renderer.last_error().c_str());
			ImGui::SetCursorScreenPos(ImVec2(image_pos.x, image_pos.y + static_cast<float>(height)));
		}

		// ---- free fly ( only when this view has its own camera )
		if (!m->follow_radiant && ImGui::IsItemHovered() && ImGui::IsMouseDown(ImGuiMouseButton_Right))
		{
			const auto& io = ImGui::GetIO();

			cam.yaw -= io.MouseDelta.x * 0.2f;
			cam.pitch = std::clamp(cam.pitch + io.MouseDelta.y * 0.2f, -89.0f, 89.0f);

			const auto axes = cam.axes();
			const float step = m->fly_speed * (io.KeyShift ? 4.0f : 1.0f) * io.DeltaTime;

			const auto move = [&](const codol::render::float3& dir, const float amount)
			{
				cam.origin.x += dir.x * amount;
				cam.origin.y += dir.y * amount;
				cam.origin.z += dir.z * amount;
			};

			if (ImGui::IsKeyDown(ImGuiKey_W)) { move(axes.forward, step); }
			if (ImGui::IsKeyDown(ImGuiKey_S)) { move(axes.forward, -step); }
			if (ImGui::IsKeyDown(ImGuiKey_A)) { move(axes.left, step); }
			if (ImGui::IsKeyDown(ImGuiKey_D)) { move(axes.left, -step); }
			if (ImGui::IsKeyDown(ImGuiKey_E)) { cam.origin.z += step; }
			if (ImGui::IsKeyDown(ImGuiKey_Q)) { cam.origin.z -= step; }
		}

		// ---- status
		const auto stats = m->renderer.stats();
		ImGui::TextDisabled("%s   %d x %d   %d draws   pos %.0f %.0f %.0f   pitch %.0f yaw %.0f",
			m->renderer.map_name().c_str(), width, height, static_cast<int>(stats.draw_calls), cam.origin.x, cam.origin.y, cam.origin.z, cam.pitch, cam.yaw);

		ImGui::End();
		return true;
	}

	REGISTER_GUI(codol_viewport_dialog);
}
