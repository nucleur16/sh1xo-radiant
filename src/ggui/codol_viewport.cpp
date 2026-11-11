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

		// one shot requests ( toolbar or keys ), handled where the camera is set up
		bool next_spawn = false;
		bool overview = false;
		bool goto_radiant = false;
		int spawn_index = 0;

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
		m->spawn_index = 0;
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
			ImGui::Separator();
			ImGui::Checkbox("Sun shadows", &m->sun_shadows);
			ImGui::Checkbox("Model LODs", &m->model_lods);
			ImGui::Checkbox("Wireframe", &m->wireframe);
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
			ImGui::Separator();

			if (ImGui::MenuItem("Next spawn point", "N", false, loaded))
			{
				m->next_spawn = true;
			}

			if (ImGui::MenuItem("Whole map from above", "Home", false, loaded))
			{
				m->overview = true;
			}

			if (ImGui::MenuItem("Go to the radiant camera", "G", false, loaded))
			{
				m->goto_radiant = true;
			}

			ImGui::Separator();
			ImGui::Checkbox("Follow the radiant camera", &m->follow_radiant);
			ImGui::EndPopup();
		}

		ImGui::SameLine();
		ImGui::Checkbox("Follow radiant", &m->follow_radiant);

		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("This view shows what the radiant camera looks at.\nMoving in this view turns it off again.");
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

		if (m->next_spawn)
		{
			m->next_spawn = false;
			m->follow_radiant = false;

			if (!m->renderer.spawn_camera(++m->spawn_index, cam))
			{
				m->spawn_index = 0;
				m->renderer.spawn_camera(0, cam);
			}
		}

		if (m->overview)
		{
			m->overview = false;
			m->follow_radiant = false;

			codol::render::float3 center, half;
			if (m->renderer.world_bounds(center, half))
			{
				const float reach = std::max(half.x, half.y) / std::tan(m->fov * 0.5f * 0.0174532925f);
				cam.origin = { center.x, center.y, center.z + half.z + reach };
				cam.pitch = 89.0f;
				cam.yaw = 90.0f;
				cam.roll = 0.0f;
			}
		}

#ifndef CODOL_STANDALONE
		if ((m->follow_radiant || m->goto_radiant) && cmainframe::activewnd && cmainframe::activewnd->m_pCamWnd)
		{
			const auto& rc = cmainframe::activewnd->m_pCamWnd->camera;
			cam.origin = { rc.origin[0], rc.origin[1], rc.origin[2] };

			// radiant: pitch up is positive, the game: pitch down is positive
			cam.pitch = -rc.angles[0];
			cam.yaw = rc.angles[1];
			cam.roll = 0.0f;
		}
#endif

		m->goto_radiant = false;

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

		// ---- navigation
		// An invisible button over the image owns the mouse: a drag that starts on the view keeps steering it when the
		// cursor leaves the view, and never moves the window.
		ImGui::SetCursorScreenPos(image_pos);
		ImGui::InvisibleButton("##vp_nav", ImVec2(static_cast<float>(width), static_cast<float>(height)),
			ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight | ImGuiButtonFlags_MouseButtonMiddle);

		const bool hovered = ImGui::IsItemHovered();
		const bool active = ImGui::IsItemActive();

		{
			const auto& io = ImGui::GetIO();
			const auto axes = cam.axes();
			const float boost = io.KeyShift ? 4.0f : 1.0f;
			bool moved = false;

			const auto move = [&](const codol::render::float3& dir, const float amount)
			{
				cam.origin.x += dir.x * amount;
				cam.origin.y += dir.y * amount;
				cam.origin.z += dir.z * amount;
				moved = true;
			};

			const bool lmb = active && ImGui::IsMouseDown(ImGuiMouseButton_Left);
			const bool rmb = active && ImGui::IsMouseDown(ImGuiMouseButton_Right);
			const bool mmb = active && ImGui::IsMouseDown(ImGuiMouseButton_Middle);
			const float dx = io.MouseDelta.x;
			const float dy = io.MouseDelta.y;

			// units per pixel of mouse travel, scaled with the fly speed
			const float drag = m->fly_speed * boost / 200.0f;

			if (mmb || (lmb && rmb))
			{
				// pan: the scene follows the cursor
				move(axes.left, dx * drag);
				move(axes.up, dy * drag);
				ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
			}
			else if (rmb)
			{
				// look around, fly with the keys
				if (dx != 0.0f || dy != 0.0f)
				{
					cam.yaw -= dx * 0.15f;
					cam.pitch = std::clamp(cam.pitch + dy * 0.15f, -89.0f, 89.0f);
					moved = true;
				}

				const float step = m->fly_speed * boost * io.DeltaTime;

				if (ImGui::IsKeyDown(ImGuiKey_W)) { move(axes.forward, step); }
				if (ImGui::IsKeyDown(ImGuiKey_S)) { move(axes.forward, -step); }
				if (ImGui::IsKeyDown(ImGuiKey_A)) { move(axes.left, step); }
				if (ImGui::IsKeyDown(ImGuiKey_D)) { move(axes.left, -step); }
				if (ImGui::IsKeyDown(ImGuiKey_E) || ImGui::IsKeyDown(ImGuiKey_Space)) { move({ 0.0f, 0.0f, 1.0f }, step); }
				if (ImGui::IsKeyDown(ImGuiKey_Q) || ImGui::IsKeyDown(ImGuiKey_C)) { move({ 0.0f, 0.0f, 1.0f }, -step); }

				// the wheel changes the fly speed while flying
				if (io.MouseWheel != 0.0f)
				{
					m->fly_speed = std::clamp(m->fly_speed * std::pow(1.25f, io.MouseWheel), 50.0f, 20000.0f);
				}

				ImGui::SetMouseCursor(ImGuiMouseCursor_None);
			}
			else if (lmb)
			{
				// drive: up / down moves along the ground, left / right turns
				if (dx != 0.0f || dy != 0.0f)
				{
					const float yaw = cam.yaw * 0.0174532925f;
					move({ std::cos(yaw), std::sin(yaw), 0.0f }, -dy * drag);
					cam.yaw -= dx * 0.15f;
				}
			}
			else if (hovered)
			{
				// the wheel moves along the view direction
				if (io.MouseWheel != 0.0f)
				{
					move(axes.forward, io.MouseWheel * m->fly_speed * boost * 0.25f);
				}

				if (ImGui::IsKeyPressed(ImGuiKey_N, false)) { m->next_spawn = true; }
				if (ImGui::IsKeyPressed(ImGuiKey_Home, false)) { m->overview = true; }
				if (ImGui::IsKeyPressed(ImGuiKey_G, false)) { m->goto_radiant = true; }
			}

			// taking the controls ends following the radiant camera
			if (moved)
			{
				m->follow_radiant = false;
			}
		}

		// ---- hint
		if (hovered && !active)
		{
			const char* hint = "Right drag: look  +  W A S D fly, Q / E down / up, wheel = speed     Left drag: drive     Middle drag: pan     Wheel: forward / back     Shift: faster";
			auto* dl = ImGui::GetWindowDrawList();
			const ImVec2 size = ImGui::CalcTextSize(hint);
			const ImVec2 at(image_pos.x + 8.0f, image_pos.y + static_cast<float>(height) - size.y - 10.0f);

			dl->AddRectFilled(ImVec2(at.x - 4.0f, at.y - 3.0f), ImVec2(at.x + size.x + 4.0f, at.y + size.y + 3.0f), IM_COL32(0, 0, 0, 150), 3.0f);
			dl->AddText(at, IM_COL32(230, 230, 230, 255), hint);
		}

		// ---- status
		const auto stats = m->renderer.stats();
		ImGui::TextDisabled("%s   %d x %d   %d draws   pos %.0f %.0f %.0f   pitch %.0f yaw %.0f   speed %.0f",
			m->renderer.map_name().c_str(), width, height, static_cast<int>(stats.draw_calls), cam.origin.x, cam.origin.y, cam.origin.z, cam.pitch, cam.yaw, m->fly_speed);

		ImGui::End();
		return true;
	}

	REGISTER_GUI(codol_viewport_dialog);
}
