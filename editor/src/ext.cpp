#include "std_include.hpp"

// windows.h macros
#undef min
#undef max

#include "host.hpp"
#include "ui.hpp"
#include "ext.hpp"
#include "edit3d.hpp"

// Where the parts of ext.hpp meet the host: one call to set them up, one per frame, and the menu entries.

namespace editor::ext
{
	void register_command_actions();		// action_table.cpp

	namespace
	{
		void register_window_actions()
		{
			using namespace actions;
			auto& st = ui::get();

			const auto window = [](const char* id, const char* label, const char* key, bool& flag, const char* title, const int where)
			{
				add(id, label, key, [&flag, title]
				{
					flag = !flag;
					if (flag) { ImGui::SetWindowFocus(title); }
				}, where, no_map).toggle([&flag] { return flag; });
			};

			window("window.entity", "Entity window", "N", st.show_entity, "Entity##window", view_2d);
			window("window.selection", "Selection window", "", st.show_selection, "Selection##window", anywhere);
			window("window.layers", "Layers window", "L", st.show_layers, "Layers##window", views);
			window("window.models", "Models window", "", st.show_models, "Models##window", anywhere);
			window("window.assets", "Assets window", "", st.show_assets, "Assets##window", anywhere);
			window("window.preview", "Preview window", "", st.show_preview, "Preview##window", anywhere);

			add("window.preferences", "Preferences ..", "P", [] { prefs::open_window(); }, views, no_map);
			add("window.hotkeys", "Edit Hotkeys ..", "", [] { open_hotkeys_window(); }, anywhere, no_map);
			add("window.colors", "Edit Colors ..", "", [] { colors::open_window(); }, anywhere, no_map);
			add("window.toolbar", "Edit Toolbar ..", "", [] { toolbar::open_edit_window(); }, anywhere, no_map);
			add("window.toolbar_toggle", "Show the toolbar", "", [] { prefs::set_bool("ed_toolbar", !prefs::get_bool("ed_toolbar")); }, anywhere, no_map)
				.toggle([] { return prefs::get_bool("ed_toolbar"); });
			add("help.window", "Help", "F1", [] { open_help_window(); }, anywhere, no_map);

			// the grid by number, as radiant has it on the digits
			static const std::pair<const char*, double> grids[] = { { "~", 0.5 }, { "1", 1.0 }, { "2", 2.0 }, { "3", 4.0 }, { "4", 8.0 }, { "5", 16.0 }, { "6", 32.0 }, { "7", 64.0 }, { "8", 256.0 }, { "9", 512.0 } };

			for (const auto& [key, size] : grids)
			{
				char id[32], label[32];
				std::snprintf(id, sizeof(id), "grid.set_%g", size);
				std::snprintf(label, sizeof(label), "Grid %g", size);
				const double value = size;
				add(id, label, key, [value] { get_scene().set_grid(value); }, views).toggle([value] { return get_scene().grid == value; });
			}

			add("grid.set_128", "Grid 128", "", [] { get_scene().set_grid(128.0); }, views).toggle([] { return get_scene().grid == 128.0; });
			add("file.autosave", "Autosave now", "", []
			{
				std::string written;
				const bool ok = prefs::autosave_now(&written);
				game::printf_to_console("%s[autosave] %s\n", ok ? "" : "^1", written.c_str());
				ImGui::Toast(ok ? ImGuiToastType_Success : ImGuiToastType_Warning, "Autosave", written.c_str(), 4000);
			});
		}
	}

	void init()
	{
		prefs::init();
		colors::get(colors::grid_background);		// registers the colours

		actions::register_defaults();
		register_window_actions();
		textures::register_actions();
		render_modes::register_actions();
		fx::register_actions();
		map_tools::register_actions();
		register_command_actions();		// Brush / Patch, Selection / Grid / Misc / Region, clipboard, filters, prefabs
		edit3d::register_actions();		// the tool keys of the 3D view

		// the keys of the user, over the defaults of everything that is registered now ( and kept for what comes later )
		actions::load_keys();
	}

	void frame()
	{
		prefs::frame();

		actions::draw_hotkeys_window();
		actions::draw_help_window();
		colors::draw_window();
		toolbar::draw_edit_window();
		textures::draw_windows();
		render_modes::draw_windows();
		fx::draw_windows();
		map_tools::draw_windows();
	}

	void draw_menus()
	{
		textures::draw_menu();
		render_modes::draw_menu();
		fx::draw_menu();
		map_tools::draw_menu();
	}

	void draw_edit_menu_items()
	{
		actions::menu_item("window.colors");
		actions::menu_item("window.toolbar");
		actions::menu_item("window.hotkeys");
		actions::menu_item("window.preferences", "Preferences ..");
	}

	void draw_help_menu_items()
	{
		actions::menu_item("help.window", "Help: controls and keys");
		actions::menu_item("window.hotkeys");
	}
}
