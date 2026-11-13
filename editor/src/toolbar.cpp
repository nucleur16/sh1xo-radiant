#include "std_include.hpp"

// windows.h macros
#undef min
#undef max

#include "host.hpp"
#include "ui.hpp"
#include "ext.hpp"

// The global toolbar under the menu bar: buttons for actions of the registry ( hotkeys.cpp ). The list is the
// user's ( Edit > Edit Toolbar: add, remove, reorder ) and is saved to codol_editor_toolbar.cfg as action ids, one
// per line. An id whose action is not registered ( it comes with another part of the editor ) keeps its place and
// shows up as soon as the action exists.
//
// The pictures are drawn here from lines, boxes and circles on a 16 x 16 raster: no image files.

namespace editor::toolbar
{
	namespace
	{
		constexpr const char* SEPARATOR = "|";

		// what a fresh install shows. Ids of actions other parts of the editor register are in here as well.
		constexpr const char* DEFAULT_LAYOUT[] = {
			"file.open", "file.save", "|",
			"edit.undo", "edit.redo", "edit.copy", "edit.paste", "|",
			"gizmo.move", "gizmo.rotate", "gizmo.scale", "gizmo.world", "grid.snap", "grid.smaller", "grid.larger", "|",
			"select.flip_x", "select.flip_y", "select.flip_z", "select.rotate_x", "select.rotate_y", "select.rotate_z", "|",
			"select.complete_tall", "select.touching", "select.partial_tall", "select.inside", "|",
			"tool.clipper", "tool.vertex", "csg.hollow", "csg.subtract", "csg.merge", "|",
			"texture.flip_x", "texture.flip_y", "texture.rotate_90", "texture.fit", "texture.lock_toggle", "texture.cycle_layer", "view.surface", "|",
			"view.hide_selected", "view.show_all", "camera.focus", "view.cycle_axis", "render.cubic_clip", "filter.window", "|",
			"render.wireframe", "render.fullbright", "render.fake_normal", "render.patches_cycle", "render.entities_cycle", "render.light_preview", "render.filmtweaks", "|",
			"patch.redisperse", "patch.cap", "patch.thicken", "|",
			"fx.play", "fx.pause", "fx.stop", "fx.repeat", "fx.edit", "|",
			"tool.mesh_painter", "tool.rope", "tool.noise", "|",
			"file.build", "texture.reload" };

		// L line, R box, F filled box, C circle, D disc, P open line through n points, G closed outline, T filled
		// shape ( convex ), A arc ( centre, radius, from, to in degrees, clockwise from the right ), S text at x y
		struct icon
		{
			const char* id;
			const char* shape;
		};

		constexpr icon ICONS[] = {
			{ "file.open", "P 5 1 13 1 5 6 5 7 6 15 6 G 4 1 13 3 8 15 8 13 13" },
			{ "file.save", "G 5 2 2 12 2 14 4 14 14 2 14 R 5 2 11 6 F 5 9 11 14" },
			{ "file.build", "G 4 2 14 2 8 8 5 14 8 L 8 5 8 14 L 2 8 8 5 L 14 8 8 5 L 8 2 8 5" },
			{ "edit.undo", "A 9 9 5 180 360 L 14 9 14 12 T 3 1 9 7 9 4 12" },
			{ "edit.redo", "A 7 9 5 180 360 L 2 9 2 12 T 3 15 9 9 9 12 12" },
			{ "edit.copy", "R 2 2 10 11 R 6 5 14 14" },
			{ "edit.paste", "R 3 3 13 14 F 6 1 10 4 L 6 8 10 8 L 6 11 10 11" },
			{ "edit.delete", "L 3 3 13 13 L 13 3 3 13" },
			{ "edit.clone", "R 2 5 9 12 R 7 3 14 10" },
			{ "gizmo.move", "L 8 1 8 15 L 1 8 15 8 T 3 8 0 6 3 10 3 T 3 8 16 6 13 10 13 T 3 0 8 3 6 3 10 T 3 16 8 13 6 13 10" },
			{ "gizmo.rotate", "A 8 8 6 40 320 T 3 15 8 10 7 13 3" },
			{ "gizmo.scale", "R 2 8 8 14 R 2 2 14 14 L 8 8 13 3 T 3 14 2 10 3 13 6" },
			{ "gizmo.world", "C 8 8 6 L 2 8 14 8 A 8 8 3 0 360 L 8 2 8 14" },
			{ "grid.snap", "L 2 5 14 5 L 2 11 14 11 L 5 2 5 14 L 11 2 11 14 D 11 11 2.5" },
			{ "grid.smaller", "R 2 2 14 14 L 8 2 8 14 L 2 8 14 8 L 5 2 5 14 L 11 2 11 14 L 2 5 14 5 L 2 11 14 11" },
			{ "grid.larger", "R 2 2 14 14 L 8 2 8 14 L 2 8 14 8" },
			{ "tool.clipper", "R 2 4 14 12 L 5 1 11 15 D 5 1 1.5 D 11 15 1.5" },
			{ "tool.vertex", "G 4 3 3 13 5 12 13 5 12 D 3 3 2 D 13 5 2 D 12 13 2 D 5 12 2" },
			{ "csg.hollow", "R 2 2 14 14 R 5 5 11 11" },
			{ "csg.subtract", "R 2 2 11 11 F 8 8 14 14" },
			{ "csg.merge", "G 8 2 2 9 2 9 5 14 5 14 14 5 14 5 9 2 9" },
			{ "select.flip_x", "L 8 1 8 15 T 3 1 8 6 4 6 12 G 3 15 8 10 4 10 12 S 1 0 X" },
			{ "select.flip_y", "L 8 1 8 15 T 3 1 8 6 4 6 12 G 3 15 8 10 4 10 12 S 11 0 Y" },
			{ "select.flip_z", "L 1 8 15 8 T 3 8 1 4 6 12 6 G 3 8 15 4 10 12 10 S 12 9 Z" },
			{ "select.rotate_x", "A 8 9 5 30 330 T 3 14 9 10 8 12 5 S 6 4 X" },
			{ "select.rotate_y", "A 8 9 5 30 330 T 3 14 9 10 8 12 5 S 6 4 Y" },
			{ "select.rotate_z", "A 8 9 5 30 330 T 3 14 9 10 8 12 5 S 6 4 Z" },
			{ "select.complete_tall", "R 5 1 11 15 F 6 6 10 10" },
			{ "select.partial_tall", "R 5 1 11 15 F 3 6 9 10" },
			{ "select.touching", "R 2 2 9 9 F 7 7 14 14" },
			{ "select.inside", "R 2 2 14 14 F 6 6 10 10" },
			{ "select.all", "R 2 2 14 14 F 5 5 11 11" },
			{ "select.invert", "F 2 2 8 14 R 8 2 14 14" },
			{ "texture.flip_x", "R 2 3 14 13 L 8 3 8 13 T 3 4 8 7 5 7 11 T 3 12 8 9 5 9 11" },
			{ "texture.flip_y", "R 2 3 14 13 L 2 8 14 8 T 3 8 4 5 7 11 7 T 3 8 12 5 9 11 9" },
			{ "texture.rotate_90", "R 2 6 10 14 A 9 7 5 200 350 T 3 14 9 11 6 15 5" },
			{ "texture.fit", "R 2 2 14 14 L 5 8 11 8 T 3 3 8 6 6 6 10 T 3 13 8 10 6 10 10" },
			{ "texture.natural", "R 2 2 14 14 L 2 8 14 8 L 8 2 8 14" },
			{ "texture.lock_toggle", "R 3 7 13 14 A 8 7 3.5 180 360 D 8 10.5 1.3" },
			{ "texture.lock_moves", "R 3 7 13 14 A 8 7 3.5 180 360 L 5 10.5 11 10.5" },
			{ "texture.lock_rotations", "R 3 7 13 14 A 8 7 3.5 180 360 C 8 10.5 2" },
			{ "texture.cycle_layer", "G 4 8 2 14 5 8 8 2 5 P 3 2 8 8 11 14 8 P 3 2 11 8 14 14 11" },
			{ "texture.find_replace", "C 6 6 4 L 9 9 14 14 L 11 3 15 3 T 3 15 3 13 1 13 5" },
			{ "texture.reload", "A 8 8 5.5 30 300 T 3 14 3 14 8 9 7" },
			{ "view.surface", "R 2 2 14 14 L 2 6 14 6 L 6 6 6 14 L 10 6 10 14 L 2 10 14 10" },
			{ "view.hide_selected", "A 8 9 6 200 340 L 3 3 13 13" },
			{ "view.show_all", "A 8 10 6 200 340 A 8 5 6 20 160 D 8 8 2" },
			{ "view.cycle_axis", "L 3 13 3 3 L 3 13 13 13 L 3 13 10 6 S 0 0 z S 12 9 x" },
			{ "camera.focus", "C 8 8 5 D 8 8 1.5 L 8 1 8 4 L 8 12 8 15 L 1 8 4 8 L 12 8 15 8" },
			{ "camera.overview", "R 2 2 14 14 T 3 8 5 5 10 11 10" },
			{ "camera.next_spawn", "D 8 4 2.5 L 8 7 8 12 L 8 12 5 15 L 8 12 11 15 L 4 9 12 9" },
			{ "render.cubic_clip", "R 2 5 10 14 G 4 2 5 6 2 14 2 10 5 L 14 2 14 10 L 10 14 14 10" },
			{ "render.wireframe", "G 4 8 1 15 5 8 15 1 5 L 1 5 8 9 L 15 5 8 9 L 8 9 8 15 L 8 1 8 9" },
			{ "render.fullbright", "D 8 8 3 L 8 1 8 3 L 8 13 8 15 L 1 8 3 8 L 13 8 15 8 L 3 3 4.5 4.5 L 11.5 11.5 13 13 L 13 3 11.5 4.5 L 3 13 4.5 11.5" },
			{ "render.fake_normal", "C 8 8 6 T 4 8 2 14 8 8 14 8 8" },
			{ "render.fake_view", "C 8 8 6 D 8 8 3" },
			{ "render.patches_cycle", "P 5 2 12 5 6 8 8 11 6 14 12 P 5 2 6 5 3 8 4 11 3 14 6 L 2 12 2 6 L 14 12 14 6" },
			{ "render.entities_cycle", "R 3 3 13 13 L 3 3 13 13 L 13 3 3 13" },
			{ "render.light_preview", "C 8 6 4 L 6 11 10 11 L 6 13 10 13 L 7 15 9 15" },
			{ "render.sun_shadows", "D 5 5 3 T 4 6 14 15 14 13 9 9 9" },
			{ "render.filmtweaks", "R 2 3 14 13 L 5 3 5 13 L 11 3 11 13 L 2 6 5 6 L 2 10 5 10 L 11 6 14 6 L 11 10 14 10" },
			{ "render.outline", "R 2 2 14 14 R 5 5 11 11 R 4 4 12 12" },
			{ "render.water", "P 5 1 6 4 4 8 6 12 4 15 6 P 5 1 11 4 9 8 11 12 9 15 11" },
			{ "filter.window", "G 6 1 2 15 2 10 8 10 14 6 12 6 8" },
			{ "patch.redisperse", "D 3 8 1.5 D 8 8 1.5 D 13 8 1.5 L 3 4 13 4 L 3 12 13 12" },
			{ "patch.cap", "A 8 13 6 180 360 L 2 13 14 13" },
			{ "patch.thicken", "A 8 14 7 200 340 A 8 14 4 200 340" },
			{ "fx.play", "T 3 4 2 4 14 14 8" },
			{ "fx.pause", "F 3 2 7 14 F 9 2 13 14" },
			{ "fx.stop", "F 3 3 13 13" },
			{ "fx.repeat", "A 8 8 5.5 200 520 T 3 3 3 2 9 7 7" },
			{ "fx.edit", "G 4 2 14 10 4 12 6 4 14 L 9 5 11 7" },
			{ "fx.browser", "D 8 8 2 L 8 1 8 4 L 8 12 8 15 L 1 8 4 8 L 12 8 15 8 L 3 3 5 5 L 11 11 13 13 L 13 3 11 5 L 3 13 5 11" },
			{ "tool.mesh_painter", "G 4 9 2 14 7 7 14 2 9 L 2 14 5 14 L 5 11 2 14" },
			{ "tool.rope", "P 6 1 3 3 9 6 12 10 12 13 9 15 3" },
			{ "tool.noise", "P 7 1 12 4 5 6 9 9 4 11 10 15 12 L 1 14 15 14" },
			{ "window.preferences", "C 8 8 3 C 8 8 6 L 8 1 8 3 L 8 13 8 15 L 1 8 3 8 L 13 8 15 8" },
			{ "window.hotkeys", "R 1 4 15 12 L 4 7 5 7 L 7 7 8 7 L 10 7 12 7 L 4 10 12 10" },
			{ "window.colors", "C 8 8 6 D 6 6 1.3 D 10 6 1.3 D 8 10 1.3" },
			{ "entity.connect", "D 4 4 2.5 D 12 12 2.5 L 6 6 10 10" },
			{ "brush.make_detail", "R 2 2 14 14 L 2 2 14 14 L 8 2 14 8 L 2 8 8 14" },
			{ "brush.make_structural", "R 2 2 14 14 R 4 4 12 12" } };

		std::vector<std::string> g_layout;
		bool g_loaded = false;
		bool g_show_edit = false;
		int g_picked = -1;
		std::string g_filter;

		std::filesystem::path layout_file()
		{
			return std::filesystem::u8path(utils::fs::get_basepath()) / "codol_editor_toolbar.cfg";
		}

		void reset_layout()
		{
			g_layout.assign(std::begin(DEFAULT_LAYOUT), std::end(DEFAULT_LAYOUT));
		}

		void save_layout()
		{
			std::string out = "// CoDOL Radiant toolbar: one action per line, | is a gap. Edit > Edit Toolbar writes this file.\n";
			for (const auto& id : g_layout) { out += id + "\n"; }

			std::ofstream file(layout_file(), std::ios::binary | std::ios::trunc);
			file << out;
		}

		void ensure_loaded()
		{
			if (g_loaded) { return; }

			g_loaded = true;
			std::ifstream file(layout_file(), std::ios::binary);

			if (!file.is_open())
			{
				reset_layout();
				return;
			}

			std::string line;
			while (std::getline(file, line))
			{
				while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) { line.pop_back(); }
				if (!line.empty() && line[0] != '/') { g_layout.push_back(line); }
			}
		}

		const char* shape_of(const std::string_view id)
		{
			for (const auto& i : ICONS)
			{
				if (id == i.id) { return i.shape; }
			}

			return nullptr;
		}

		// the letters of an action without a picture: the first of each word of the part after the '.'
		std::string letters_of(const std::string_view id)
		{
			const auto dot = id.find('.');
			const std::string_view name = dot == std::string_view::npos ? id : id.substr(dot + 1);
			std::string out;
			bool start = true;

			for (const char c : name)
			{
				if (c == '_') { start = true; continue; }
				if (start && out.size() < 3) { out.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c)))); }
				start = false;
			}

			if (out.size() == 1 && name.size() > 1) { out.push_back(name[1]); }
			return out;
		}

		// one button; true when it was clicked
		bool button(const actions::action& a, const float size)
		{
			const bool usable = actions::usable(a);
			const bool on = a.checked && a.checked();
			auto* dl = ImGui::GetWindowDrawList();
			const ImVec2 at = ImGui::GetCursorScreenPos();

			ImGui::PushID(a.id.c_str());
			const bool clicked = ImGui::InvisibleButton("##tb", ImVec2(size, size)) && usable;
			const bool hovered = ImGui::IsItemHovered();
			const bool held = ImGui::IsItemActive();
			ImGui::PopID();

			const ImVec2 end(at.x + size, at.y + size);

			if (on)
			{
				dl->AddRectFilled(at, end, ImGui::GetColorU32(ImGuiCol_Header), 3.0f);
				dl->AddRect(at, end, ImGui::GetColorU32(ImGuiCol_CheckMark, 0.7f), 3.0f);
			}

			if (hovered && usable)
			{
				dl->AddRectFilled(at, end, ImGui::GetColorU32(held ? ImGuiCol_ButtonActive : ImGuiCol_ButtonHovered, on ? 0.6f : 1.0f), 3.0f);
			}

			const ImU32 color = ImGui::GetColorU32(ImGuiCol_Text, usable ? (on ? 1.0f : 0.86f) : 0.28f);
			const float pad = std::floor(size * 0.16f);

			if (!draw_icon(dl, a.id, ImVec2(at.x + pad, at.y + pad), size - pad * 2.0f, color))
			{
				const std::string text = letters_of(a.id);
				auto* font = ImGui::GetIO().Fonts->Fonts.Size > 3 ? ImGui::GetIO().Fonts->Fonts[3] : ImGui::GetFont();
				const float font_size = std::min(font->FontSize, size * 0.5f);
				const ImVec2 extent = font->CalcTextSizeA(font_size, FLT_MAX, 0.0f, text.c_str());
				dl->AddRect(ImVec2(at.x + 2.0f, at.y + 2.0f), ImVec2(end.x - 2.0f, end.y - 2.0f), ImGui::GetColorU32(ImGuiCol_Text, usable ? 0.25f : 0.1f), 3.0f);
				dl->AddText(font, font_size, ImVec2(at.x + (size - extent.x) * 0.5f, at.y + (size - extent.y) * 0.5f), color, text.c_str());
			}

			if (hovered)
			{
				const char* key = actions::shortcut(a.id);
				std::string tip = a.label;
				if (key) { tip += std::string("   ( ") + key + " )"; }
				if (!a.tip.empty()) { tip += "\n" + a.tip; }
				ImGui::SetTooltip("%s", tip.c_str());
			}

			return clicked;
		}
	}

	bool draw_icon(ImDrawList* dl, const std::string_view action_id, const ImVec2& at, const float size, const ImU32 color)
	{
		const char* shape = shape_of(action_id);
		if (!shape)
		{
			return false;
		}

		const float unit = size / 16.0f;
		const float thickness = std::max(1.0f, size / 14.0f);
		const auto point = [&](const float x, const float y) { return ImVec2(at.x + x * unit, at.y + y * unit); };

		std::istringstream in(shape);
		std::string command;

		while (in >> command)
		{
			float v[8] = {};
			const auto read = [&](const int n) { for (int i = 0; i < n; i++) { in >> v[i]; } };

			switch (command[0])
			{
			case 'L':
				read(4);
				dl->AddLine(point(v[0], v[1]), point(v[2], v[3]), color, thickness);
				break;

			case 'R':
				read(4);
				dl->AddRect(point(v[0], v[1]), point(v[2], v[3]), color, 0.0f, 0, thickness);
				break;

			case 'F':
				read(4);
				dl->AddRectFilled(point(v[0], v[1]), point(v[2], v[3]), color);
				break;

			case 'C':
				read(3);
				dl->AddCircle(point(v[0], v[1]), v[2] * unit, color, 20, thickness);
				break;

			case 'D':
				read(3);
				dl->AddCircleFilled(point(v[0], v[1]), v[2] * unit, color, 16);
				break;

			case 'A':
				read(5);
				dl->PathArcTo(point(v[0], v[1]), v[2] * unit, v[3] * 0.0174532925f, v[4] * 0.0174532925f, 20);
				dl->PathStroke(color, 0, thickness);
				break;

			case 'P':
			case 'G':
			case 'T':
			{
				int n = 0;
				in >> n;
				ImVec2 points[16];
				n = std::clamp(n, 0, 16);

				for (int i = 0; i < n; i++)
				{
					float x = 0.0f, y = 0.0f;
					in >> x >> y;
					points[i] = point(x, y);
				}

				if (command[0] == 'T') { dl->AddConvexPolyFilled(points, n, color); }
				else { dl->AddPolyline(points, n, color, command[0] == 'G' ? ImDrawFlags_Closed : 0, thickness); }
				break;
			}

			case 'S':
			{
				read(2);
				std::string text;
				in >> text;
				auto* font = ImGui::GetIO().Fonts->Fonts.Size > 2 ? ImGui::GetIO().Fonts->Fonts[2] : ImGui::GetFont();
				dl->AddText(font, std::max(8.0f, size * 0.5f), point(v[0], v[1]), color, text.c_str());
				break;
			}

			default:
				break;
			}
		}

		return true;
	}

	void draw()
	{
		ensure_loaded();

		// toolbar_add <id> [position] | toolbar_remove <id> | toolbar_reset | ed_toolbar
		static bool registered = false;
		if (!registered)
		{
			registered = true;
			using args_t = const std::vector<std::string>&;

			command::register_command("toolbar_add", [](args_t args)
			{
				if (args.size() < 2) { game::printf_to_console("usage: toolbar_add <action id or |> [position]\n"); return; }
				const std::size_t at = args.size() > 2 ? std::min(static_cast<std::size_t>(std::max(0, std::atoi(args[2].c_str()))), g_layout.size()) : g_layout.size();
				g_layout.insert(g_layout.begin() + static_cast<std::ptrdiff_t>(at), args[1]);
				save_layout();
			});

			command::register_command("toolbar_remove", [](args_t args)
			{
				if (args.size() < 2) { return; }
				std::erase(g_layout, args[1]);
				save_layout();
			});

			command::register_command("toolbar_reset", [](args_t)
			{
				reset_layout();
				save_layout();
			});

			command::register_command("toolbar_list", [](args_t)
			{
				std::string line;
				for (const auto& id : g_layout) { line += (actions::find(id) || id == SEPARATOR ? id : "( " + id + " )") + " "; }
				game::printf_to_console("%s\n", line.c_str());
			});

			command::register_command("ed_toolbar", [](args_t) { g_show_edit = true; });
		}

		if (!prefs::get_bool("ed_toolbar"))
		{
			return;
		}

		const float size = static_cast<float>(std::clamp(prefs::get_int("ed_toolbar_size"), 18, 48));
		const float height = size + 8.0f;

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6.0f, 4.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2.0f, 0.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
		ImGui::PushStyleColor(ImGuiCol_WindowBg, ImGui::GetStyleColorVec4(ImGuiCol_MenuBarBg));

		const ImGuiWindowFlags flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollWithMouse
			| ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoBringToFrontOnFocus;

		if (ImGui::BeginViewportSideBar("##global_toolbar", ImGui::GetMainViewport(), ImGuiDir_Up, height, flags))
		{
			bool gap = false, first = true;
			const actions::action* clicked = nullptr;

			for (const auto& id : g_layout)
			{
				if (id == SEPARATOR)
				{
					gap = !first;
					continue;
				}

				const actions::action* a = actions::find(id);
				if (!a)
				{
					continue;	// a slot of an action that does not exist ( yet )
				}

				if (gap)
				{
					// a thin line between two groups; never two in a row, none at the ends
					const ImVec2 at = ImGui::GetCursorScreenPos();
					ImGui::GetWindowDrawList()->AddLine(ImVec2(at.x + 4.0f, at.y + 3.0f), ImVec2(at.x + 4.0f, at.y + size - 3.0f), ImGui::GetColorU32(ImGuiCol_Separator));
					ImGui::Dummy(ImVec2(9.0f, size));
					ImGui::SameLine();
					gap = false;
				}

				if (button(*a, size)) { clicked = a; }
				ImGui::SameLine();
				first = false;
			}

			if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
			{
				ImGui::OpenPopup("##toolbar_menu");
			}

			if (ImGui::BeginPopup("##toolbar_menu"))
			{
				if (ImGui::MenuItem("Edit Toolbar ..")) { g_show_edit = true; }
				if (ImGui::MenuItem("Hide the toolbar")) { prefs::set_bool("ed_toolbar", false); }
				ImGui::EndPopup();
			}

			if (clicked)
			{
				clicked->run();
			}
		}

		ImGui::End();
		ImGui::PopStyleColor();
		ImGui::PopStyleVar(3);
	}

	void open_edit_window()
	{
		g_show_edit = true;
		ImGui::SetWindowFocus("Edit Toolbar##window");
	}

	void draw_edit_window()
	{
		if (!g_show_edit)
		{
			return;
		}

		ensure_loaded();
		ImGui::SetNextWindowSize(ImVec2(760.0f, 560.0f), ImGuiCond_FirstUseEver);

		if (!ImGui::Begin("Edit Toolbar##window", &g_show_edit, ImGuiWindowFlags_NoCollapse))
		{
			ImGui::End();
			return;
		}

		bool changed = false;
		bool show = prefs::get_bool("ed_toolbar");
		if (ImGui::Checkbox("Show the toolbar", &show)) { prefs::set_bool("ed_toolbar", show); }
		ImGui::SameLine();
		int size = prefs::get_int("ed_toolbar_size");
		ImGui::SetNextItemWidth(160.0f);
		if (ImGui::SliderInt("button size", &size, 18, 48, "%d px") && size != prefs::get_int("ed_toolbar_size")) { prefs::set_int("ed_toolbar_size", size); }
		ImGui::SameLine();

		if (ImGui::Button("Reset to the default"))
		{
			reset_layout();
			g_picked = -1;
			changed = true;
		}

		ImGui::TextDisabled("Left: the toolbar, in order ( drag a row to move it ). Right: every action - double click to add. Saved to codol_editor_toolbar.cfg.");

		const float half = (ImGui::GetContentRegionAvail().x - 110.0f) * 0.5f;

		// ---- the toolbar
		ImGui::BeginChild("##tb_layout", ImVec2(half, 0.0f), true);

		for (int i = 0; i < static_cast<int>(g_layout.size()); i++)
		{
			const std::string& id = g_layout[static_cast<std::size_t>(i)];
			const actions::action* a = actions::find(id);
			const bool separator = id == SEPARATOR;
			ImGui::PushID(i);

			const ImVec2 at = ImGui::GetCursorScreenPos();
			const float line = ImGui::GetTextLineHeightWithSpacing();
			const std::string label = separator ? "         ---- gap ----" : "         " + (a ? a->label : id + "   ( not available yet )");

			if (!a && !separator) { ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled)); }
			if (ImGui::Selectable(label.c_str(), g_picked == i)) { g_picked = i; }
			if (!a && !separator) { ImGui::PopStyleColor(); }

			if (a) { draw_icon(ImGui::GetWindowDrawList(), id, ImVec2(at.x + 2.0f, at.y + 1.0f), line - 4.0f, ImGui::GetColorU32(ImGuiCol_Text)); }
			if (ImGui::IsItemHovered() && !separator) { ImGui::SetTooltip("%s", id.c_str()); }

			if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceNoHoldToOpenOthers))
			{
				ImGui::SetDragDropPayload("CODOL_TB_ROW", &i, sizeof(i));
				ImGui::TextUnformatted(label.c_str() + 9);
				ImGui::EndDragDropSource();
			}

			if (ImGui::BeginDragDropTarget())
			{
				if (const auto* payload = ImGui::AcceptDragDropPayload("CODOL_TB_ROW"))
				{
					const int from = *static_cast<const int*>(payload->Data);

					if (from != i && from >= 0 && from < static_cast<int>(g_layout.size()))
					{
						std::string moved = g_layout[static_cast<std::size_t>(from)];
						g_layout.erase(g_layout.begin() + from);
						g_layout.insert(g_layout.begin() + i, std::move(moved));
						g_picked = i;
						changed = true;
					}
				}

				ImGui::EndDragDropTarget();
			}

			ImGui::PopID();
		}

		ImGui::EndChild();
		ImGui::SameLine();

		// ---- between the lists
		ImGui::BeginGroup();
		const bool picked = g_picked >= 0 && g_picked < static_cast<int>(g_layout.size());
		const ImVec2 wide(100.0f, 0.0f);

		ImGui::Dummy(ImVec2(0.0f, 40.0f));

		if (ImGui::Button("Up", wide) && picked && g_picked > 0)
		{
			std::swap(g_layout[static_cast<std::size_t>(g_picked)], g_layout[static_cast<std::size_t>(g_picked) - 1]);
			g_picked--;
			changed = true;
		}

		if (ImGui::Button("Down", wide) && picked && g_picked + 1 < static_cast<int>(g_layout.size()))
		{
			std::swap(g_layout[static_cast<std::size_t>(g_picked)], g_layout[static_cast<std::size_t>(g_picked) + 1]);
			g_picked++;
			changed = true;
		}

		if (ImGui::Button("Remove", wide) && picked)
		{
			g_layout.erase(g_layout.begin() + g_picked);
			g_picked = std::min(g_picked, static_cast<int>(g_layout.size()) - 1);
			changed = true;
		}

		if (ImGui::Button("Add a gap", wide))
		{
			const int at = picked ? g_picked + 1 : static_cast<int>(g_layout.size());
			g_layout.insert(g_layout.begin() + at, SEPARATOR);
			g_picked = at;
			changed = true;
		}

		ImGui::EndGroup();
		ImGui::SameLine();

		// ---- every action
		ImGui::BeginGroup();
		ImGui::SetNextItemWidth(-1.0f);
		ImGui::InputTextWithHint("##tb_filter", "search", &g_filter);
		ImGui::BeginChild("##tb_actions", ImVec2(0.0f, 0.0f), true);

		std::string needle = g_filter;
		for (char& c : needle) { c = static_cast<char>(std::tolower(static_cast<unsigned char>(c))); }
		std::string category;

		for (const auto& a : actions::all())
		{
			std::string hay = a->label + " " + a->id;
			for (char& c : hay) { c = static_cast<char>(std::tolower(static_cast<unsigned char>(c))); }
			if (!needle.empty() && hay.find(needle) == std::string::npos) { continue; }

			if (a->category != category)
			{
				category = a->category;
				ImGui::TextColored(ImVec4(1.0f, 0.72f, 0.25f, 1.0f), "%s", category.c_str());
			}

			const bool used = std::find(g_layout.begin(), g_layout.end(), a->id) != g_layout.end();
			const ImVec2 at = ImGui::GetCursorScreenPos();
			const float line = ImGui::GetTextLineHeightWithSpacing();

			ImGui::PushID(a.get());
			if (used) { ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled)); }

			if (ImGui::Selectable(("         " + a->label).c_str(), false, ImGuiSelectableFlags_AllowDoubleClick) && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && !used)
			{
				const int where = picked ? g_picked + 1 : static_cast<int>(g_layout.size());
				g_layout.insert(g_layout.begin() + where, a->id);
				g_picked = where;
				changed = true;
			}

			if (used) { ImGui::PopStyleColor(); }
			ImGui::PopID();

			draw_icon(ImGui::GetWindowDrawList(), a->id, ImVec2(at.x + 2.0f, at.y + 1.0f), line - 4.0f, ImGui::GetColorU32(used ? ImGuiCol_TextDisabled : ImGuiCol_Text));
			if (ImGui::IsItemHovered()) { ImGui::SetTooltip("%s%s", a->id.c_str(), used ? "\nin the toolbar" : "\ndouble click: add after the picked row"); }
		}

		ImGui::EndChild();
		ImGui::EndGroup();

		if (changed)
		{
			save_layout();
		}

		ImGui::End();
	}
}
