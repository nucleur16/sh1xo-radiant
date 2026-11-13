#include "std_include.hpp"

// windows.h macros
#undef min
#undef max

#include "host.hpp"
#include "ui.hpp"
#include "ext.hpp"

// The action registry: everything the editor can be told to do has an id, a label, a default key and a function.
// Menus, the toolbar, the keys and the console ( act <id> ) all go through it, so a key can be given to anything
// and the Help window lists what there is. The keys are saved to codol_editor_hotkeys.cfg next to the settings.

namespace editor::actions
{
	namespace
	{
		std::vector<std::unique_ptr<action>> g_actions;
		std::unordered_map<std::string, action*> g_by_id;
		// keys read from the file for actions that are not registered ( yet ): applied when they are, written back
		std::map<std::string, std::array<std::string, 2>> g_pending;
		bool g_loaded = false;

		bool g_show_hotkeys = false;
		bool g_show_help = false;
		std::string g_filter;
		action* g_capture = nullptr;		// the action whose key is being typed in the Edit Hotkeys window
		int g_capture_slot = 0;
		std::unordered_map<std::string, std::string> g_shortcut_text;

		struct key_name
		{
			const char* name;
			ImGuiKey key;
		};

		// the first name of a key is the one that is shown and written
		constexpr key_name KEY_NAMES[] = {
			{ "Space", ImGuiKey_Space }, { "Enter", ImGuiKey_Enter }, { "Return", ImGuiKey_Enter }, { "Escape", ImGuiKey_Escape }, { "Esc", ImGuiKey_Escape },
			{ "Tab", ImGuiKey_Tab }, { "Backspace", ImGuiKey_Backspace }, { "Delete", ImGuiKey_Delete }, { "Insert", ImGuiKey_Insert },
			{ "Home", ImGuiKey_Home }, { "End", ImGuiKey_End }, { "PageUp", ImGuiKey_PageUp }, { "PageDown", ImGuiKey_PageDown },
			{ "Left", ImGuiKey_LeftArrow }, { "Right", ImGuiKey_RightArrow }, { "Up", ImGuiKey_UpArrow }, { "Down", ImGuiKey_DownArrow },
			{ "[", ImGuiKey_LeftBracket }, { "]", ImGuiKey_RightBracket }, { "Comma", ImGuiKey_Comma }, { "Period", ImGuiKey_Period },
			{ "Minus", ImGuiKey_Minus }, { "Plus", ImGuiKey_Equal }, { "Equal", ImGuiKey_Equal }, { "Semicolon", ImGuiKey_Semicolon },
			{ "Apostrophe", ImGuiKey_Apostrophe }, { "Slash", ImGuiKey_Slash }, { "Backslash", ImGuiKey_Backslash }, { "~", ImGuiKey_GraveAccent },
			{ "KeypadEnter", ImGuiKey_KeypadEnter }, { "KeypadAdd", ImGuiKey_KeypadAdd }, { "KeypadSubtract", ImGuiKey_KeypadSubtract },
			{ "KeypadMultiply", ImGuiKey_KeypadMultiply }, { "KeypadDivide", ImGuiKey_KeypadDivide }, { "KeypadDecimal", ImGuiKey_KeypadDecimal },
			{ "Pause", ImGuiKey_Pause }, { "CapsLock", ImGuiKey_CapsLock }, { "PrintScreen", ImGuiKey_PrintScreen } };

		std::string lower(std::string_view text)
		{
			std::string out(text);
			for (char& c : out) { c = static_cast<char>(std::tolower(static_cast<unsigned char>(c))); }
			return out;
		}

		std::string key_text(const ImGuiKey key)
		{
			if (key >= ImGuiKey_A && key <= ImGuiKey_Z) { return std::string(1, static_cast<char>('A' + (key - ImGuiKey_A))); }
			if (key >= ImGuiKey_0 && key <= ImGuiKey_9) { return std::string(1, static_cast<char>('0' + (key - ImGuiKey_0))); }
			if (key >= ImGuiKey_F1 && key <= ImGuiKey_F12) { return "F" + std::to_string(key - ImGuiKey_F1 + 1); }
			if (key >= ImGuiKey_Keypad0 && key <= ImGuiKey_Keypad9) { return "Keypad" + std::to_string(key - ImGuiKey_Keypad0); }

			for (const auto& k : KEY_NAMES)
			{
				if (k.key == key) { return k.name; }
			}

			return {};
		}

		ImGuiKey key_of(const std::string_view text)
		{
			const std::string name = lower(text);

			if (name.size() == 1 && name[0] >= 'a' && name[0] <= 'z') { return static_cast<ImGuiKey>(ImGuiKey_A + (name[0] - 'a')); }
			if (name.size() == 1 && name[0] >= '0' && name[0] <= '9') { return static_cast<ImGuiKey>(ImGuiKey_0 + (name[0] - '0')); }

			if (name.size() >= 2 && name[0] == 'f' && std::isdigit(static_cast<unsigned char>(name[1])))
			{
				const int n = std::atoi(name.c_str() + 1);
				if (n >= 1 && n <= 12) { return static_cast<ImGuiKey>(ImGuiKey_F1 + n - 1); }
			}

			if (name.size() == 7 && name.rfind("keypad", 0) == 0 && std::isdigit(static_cast<unsigned char>(name[6])))
			{
				return static_cast<ImGuiKey>(ImGuiKey_Keypad0 + (name[6] - '0'));
			}

			for (const auto& k : KEY_NAMES)
			{
				if (lower(k.name) == name) { return k.key; }
			}

			return ImGuiKey_None;
		}

		std::string category_of(const std::string& id)
		{
			static const std::pair<const char*, const char*> titles[] = {
				{ "file", "File" }, { "edit", "Edit" }, { "select", "Selection" }, { "view", "View" }, { "camera", "Camera" }, { "grid", "Grid" },
				{ "gizmo", "Gizmo" }, { "tool", "Tools" }, { "clip", "Clipper" }, { "brush", "Brush" }, { "patch", "Patch" }, { "entity", "Entity" },
				{ "texture", "Textures" }, { "render", "Renderer" }, { "fx", "Effects" }, { "window", "Windows" }, { "region", "Region" },
				{ "filter", "Filter" }, { "misc", "Misc" }, { "csg", "CSG" }, { "help", "Help" } };

			const std::string head = id.substr(0, id.find('.'));

			for (const auto& [prefix, title] : titles)
			{
				if (head == prefix) { return title; }
			}

			std::string out = head;
			if (!out.empty()) { out[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(out[0]))); }
			return out;
		}

		std::filesystem::path keys_file()
		{
			return std::filesystem::u8path(utils::fs::get_basepath()) / "codol_editor_hotkeys.cfg";
		}

		// two actions answer to the same key when the mouse can be somewhere both look at
		bool scopes_meet(const int a, const int b)
		{
			return !((a == view_3d && b == view_2d) || (a == view_2d && b == view_3d));
		}

		void refresh_shortcut_text()
		{
			g_shortcut_text.clear();
		}

		void apply_pending(action& a)
		{
			if (const auto it = g_pending.find(a.id); it != g_pending.end())
			{
				for (int slot = 0; slot < 2; slot++)
				{
					chord c;
					parse_chord(it->second[static_cast<std::size_t>(slot)], c);
					a.keys[slot] = c;
				}

				g_pending.erase(it);
			}
		}

		std::vector<const action*> conflicts_of(const action& a, const chord& c)
		{
			std::vector<const action*> out;
			if (c.empty()) { return out; }

			for (const auto& other : g_actions)
			{
				if (other.get() != &a && scopes_meet(a.where, other->where) && (other->keys[0] == c || other->keys[1] == c))
				{
					out.push_back(other.get());
				}
			}

			return out;
		}
	}

	std::string chord_text(const chord& c)
	{
		if (c.empty()) { return {}; }

		std::string out;
		if (c.ctrl) { out += "Ctrl+"; }
		if (c.shift) { out += "Shift+"; }
		if (c.alt) { out += "Alt+"; }
		return out + key_text(c.key);
	}

	bool parse_chord(const std::string_view text, chord& out)
	{
		out = {};
		std::size_t start = 0;

		while (start < text.size())
		{
			// "+" separates; a trailing "+" would be the key itself ( written as Plus )
			auto end = text.find('+', start);
			if (end == std::string_view::npos) { end = text.size(); }

			std::string part = lower(text.substr(start, end - start));
			while (!part.empty() && part.back() == ' ') { part.pop_back(); }
			while (!part.empty() && part.front() == ' ') { part.erase(part.begin()); }

			if (part == "ctrl" || part == "control") { out.ctrl = true; }
			else if (part == "shift") { out.shift = true; }
			else if (part == "alt") { out.alt = true; }
			else if (!part.empty())
			{
				out.key = key_of(part);
				if (out.key == ImGuiKey_None) { out = {}; return false; }
			}

			start = end + 1;
		}

		if (out.key == ImGuiKey_None) { out = {}; return text.empty(); }
		return true;
	}

	action& add(std::string id, std::string label, const std::string_view keys, std::function<void()> run_fn, const int where, const int options)
	{
		action* a = find(id);

		if (!a)
		{
			g_actions.push_back(std::make_unique<action>());
			a = g_actions.back().get();
			a->id = std::move(id);
			g_by_id[a->id] = a;
		}

		const chord kept[2] = { a->keys[0], a->keys[1] };
		const bool had_keys = a->run != nullptr && (a->keys[0] != a->defaults[0] || a->keys[1] != a->defaults[1]);

		a->label = std::move(label);
		a->category = category_of(a->id);
		a->run = std::move(run_fn);
		a->enabled = nullptr;
		a->checked = nullptr;
		a->where = where;
		a->options = options;
		a->defaults[0] = a->defaults[1] = {};

		const auto bar = keys.find('|');
		parse_chord(keys.substr(0, bar), a->defaults[0]);
		if (bar != std::string_view::npos) { parse_chord(keys.substr(bar + 1), a->defaults[1]); }

		a->keys[0] = had_keys ? kept[0] : a->defaults[0];
		a->keys[1] = had_keys ? kept[1] : a->defaults[1];
		apply_pending(*a);
		refresh_shortcut_text();
		return *a;
	}

	action* find(const std::string_view id)
	{
		const auto it = g_by_id.find(std::string(id));
		return it == g_by_id.end() ? nullptr : it->second;
	}

	const std::vector<std::unique_ptr<action>>& all()
	{
		return g_actions;
	}

	bool usable(const action& a)
	{
		if (!a.run) { return false; }
		if (!(a.options & no_map) && !get_scene().open) { return false; }
		return !a.enabled || a.enabled();
	}

	bool run(const std::string_view id)
	{
		action* a = find(id);
		if (!a || !usable(*a)) { return false; }
		a->run();
		return true;
	}

	const char* shortcut(const std::string_view id)
	{
		const std::string key(id);
		auto it = g_shortcut_text.find(key);

		if (it == g_shortcut_text.end())
		{
			const action* a = find(id);
			it = g_shortcut_text.emplace(key, a ? chord_text(a->keys[0].empty() ? a->keys[1] : a->keys[0]) : std::string()).first;
		}

		return it->second.empty() ? nullptr : it->second.c_str();
	}

	bool menu_item(const std::string_view id, const char* label)
	{
		action* a = find(id);

		if (!a)
		{
			// the action comes with another part of the editor: the entry is there, greyed out
			ImGui::MenuItem(label ? label : std::string(id).c_str(), nullptr, false, false);
			return false;
		}

		const bool on = a->checked && a->checked();

		if (ImGui::MenuItem(label ? label : a->label.c_str(), shortcut(id), on, usable(*a)))
		{
			a->run();
			return true;
		}

		if (!a->tip.empty() && ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("%s", a->tip.c_str());
		}

		return false;
	}

	void set_key(action& a, const int slot, const chord& c)
	{
		a.keys[slot & 1] = c;
		refresh_shortcut_text();
		save_keys();
	}

	void reset_keys()
	{
		for (const auto& a : g_actions)
		{
			a->keys[0] = a->defaults[0];
			a->keys[1] = a->defaults[1];
		}

		g_pending.clear();
		refresh_shortcut_text();
		save_keys();
	}

	void load_keys()
	{
		g_loaded = true;
		std::ifstream file(keys_file(), std::ios::binary);
		std::string line;

		while (file.is_open() && std::getline(file, line))
		{
			if (!line.empty() && line.back() == '\r') { line.pop_back(); }
			if (line.empty() || line[0] == '/') { continue; }

			// <id> "<key>" "<second key>"
			const auto space = line.find(' ');
			if (space == std::string::npos) { continue; }

			std::array<std::string, 2> texts;
			std::size_t at = space;

			for (auto& text : texts)
			{
				const auto open = line.find('"', at);
				const auto close = open == std::string::npos ? open : line.find('"', open + 1);
				if (close == std::string::npos) { break; }
				text = line.substr(open + 1, close - open - 1);
				at = close + 1;
			}

			g_pending[line.substr(0, space)] = texts;
		}

		for (const auto& a : g_actions)
		{
			apply_pending(*a);
		}

		refresh_shortcut_text();
	}

	void save_keys()
	{
		if (!g_loaded) { return; }

		std::string out = "// CoDOL Radiant keys: <action> \"<key>\" \"<second key>\". Edit > Edit Hotkeys writes this file.\n";

		for (const auto& a : g_actions)
		{
			out += a->id + " \"" + chord_text(a->keys[0]) + "\" \"" + chord_text(a->keys[1]) + "\"\n";
		}

		for (const auto& [id, texts] : g_pending)
		{
			out += id + " \"" + texts[0] + "\" \"" + texts[1] + "\"\n";
		}

		std::ofstream file(keys_file(), std::ios::binary | std::ios::trunc);
		file << out;
	}

	// ---- keys --------------------------------------------------------------------------------------------------

	void process_keys()
	{
		const auto& io = ImGui::GetIO();
		const auto& st = ui::get();
		auto& s = get_scene();

		// a key that is being given to an action in the Edit Hotkeys window
		if (g_capture)
		{
			if (ImGui::IsKeyPressed(ImGuiKey_Escape, false))
			{
				g_capture = nullptr;
				return;
			}

			for (int k = ImGuiKey_NamedKey_BEGIN; k < ImGuiKey_NamedKey_END; k++)
			{
				const auto key = static_cast<ImGuiKey>(k);

				if (key_text(key).empty() || !ImGui::IsKeyPressed(key, false))
				{
					continue;
				}

				set_key(*g_capture, g_capture_slot, { key, io.KeyCtrl, io.KeyShift, io.KeyAlt });
				game::printf_to_console("[keys] %s: %s\n", g_capture->id.c_str(), chord_text(g_capture->keys[g_capture_slot]).c_str());
				g_capture = nullptr;
				break;
			}

			return;
		}

		// never while a text field has the keyboard, a menu or a dialog is open
		if (io.WantTextInput || ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel))
		{
			return;
		}

		// the script workbench and the game window have their own keys
		if (const auto* focused = ImGui::GetCurrentContext()->NavWindow; focused && std::strstr(focused->Name, "##codol_window"))
		{
			return;
		}

		// while the right button flies the camera the keys move it; a drag owns the document until it ends
		const bool busy = ImGui::IsMouseDown(ImGuiMouseButton_Right) || s.dragging();
		const bool over_3d = st.hovered_3d, over_2d = st.hovered_2d >= 0;

		// Of the actions a key means, the one that looks at the smallest place wins ( the 3D view before "a view"
		// before anywhere ); the others do not run.
		action* chosen[ImGuiKey_NamedKey_COUNT] = {};
		bool any = false;

		for (const auto& a : g_actions)
		{
			for (const chord& c : a->keys)
			{
				if (c.empty() || c.ctrl != io.KeyCtrl || c.shift != io.KeyShift || c.alt != io.KeyAlt || !ImGui::IsKeyPressed(c.key, (a->options & repeat) != 0))
				{
					continue;
				}

				if ((busy && !(a->options & in_drag)) || !usable(*a))
				{
					continue;
				}

				if ((a->where == views && !over_3d && !over_2d) || (a->where == view_3d && !over_3d) || (a->where == view_2d && !over_2d))
				{
					continue;
				}

				action*& slot = chosen[c.key - ImGuiKey_NamedKey_BEGIN];

				if (!slot || a->where > slot->where)
				{
					slot = a.get();
					any = true;
				}

				break;
			}
		}

		if (any)
		{
			for (action* a : chosen)
			{
				if (a) { a->run(); }
			}
		}
	}

	// ---- the editor's own actions ------------------------------------------------------------------------------

	void register_defaults()
	{
		using namespace ui;
		const auto any_selected = [] { return !get_scene().doc.selection().empty(); };
		const auto clipping = [] { return get().clipper.on; };

		// ---- files
		add("file.open", "Open map ..", "Ctrl+O", []
		{
			GET_GUI(ggui::codol_map_ents_dialog)->open();
			ImGui::SetWindowFocus("Maps##window");
		}, anywhere, no_map);
		add("file.open_map_file", "Open .map ..", "", [] { open_map_dialog(); }, anywhere, no_map);
		add("file.save", "Save", "Ctrl+S", [] { save(); });
		add("file.save_as", "Save as ..", "", [] { save_as_dialog(); });
		add("file.export", "Export entities to the server", "Ctrl+E", [] { export_entities(GET_GUI(ggui::codol_map_ents_dialog)->m_reload_after_export); });
		add("file.build", "Build map", "", [] { build_map(false); });
		add("file.build_fast", "Build map, fast", "", [] { build_map(true); });
		add("file.compile", "Compile map to a fastfile", "", [] { compile_map(); });
		add("file.install", "Install the built map ..", "", [] { open_install_window(); });

		// ---- edit
		add("edit.undo", "Undo", "Ctrl+Z", [] { undo(); }, anywhere, repeat).when([] { return get_scene().doc.can_undo(); });
		add("edit.redo", "Redo", "Ctrl+Y|Ctrl+Shift+Z", [] { redo(); }, anywhere, repeat).when([] { return get_scene().doc.can_redo(); });
		add("edit.delete", "Delete", "Backspace|Delete", [] { delete_selection(); }, views).when(any_selected);
		add("edit.clone", "Clone", "Space", [] { clone_selection(); }, views).when(any_selected);
		add("edit.snap_to_grid", "Snap to grid", "Ctrl+G", [] { snap_to_grid(); }).when(any_selected);
		add("edit.escape", "Deselect / leave the tool", "Escape", []
		{
			// one thing at a time: clip points, then the tool, then the selection
			auto& st = get();
			if (st.clipper.on && !st.clipper.points.empty()) { st.clipper.points.clear(); }
			else if (st.clipper.on) { set_clipper(false); }
			else if (st.vertex_mode) { set_vertex_mode(false); }
			else { deselect(); }
		}, views);
		add("edit.nudge_left", "Nudge left", "Left", [] { nudge_view(-1, 0); }, views, repeat);
		add("edit.nudge_right", "Nudge right", "Right", [] { nudge_view(1, 0); }, views, repeat);
		add("edit.nudge_up", "Nudge up", "Up", [] { nudge_view(0, 1); }, views, repeat);
		add("edit.nudge_down", "Nudge down", "Down", [] { nudge_view(0, -1); }, views, repeat);
		// along the axis the view looks ( up / down in 3D and in the top view )
		add("edit.nudge_in", "Nudge along the view axis +", "PageUp", [] { nudge(get().hovered_2d >= 0 ? grid_view_axis(get().hovered_2d) : 2, 1); }, views, repeat);
		add("edit.nudge_out", "Nudge along the view axis -", "PageDown", [] { nudge(get().hovered_2d >= 0 ? grid_view_axis(get().hovered_2d) : 2, -1); }, views, repeat);
		add("edit.gameplay_entities", "Add gameplay entities", "", [] { add_gameplay_entities(); });

		// ---- selection
		add("select.all", "Select all", "Ctrl+A", [] { select_all(); });
		add("select.invert", "Invert selection", "I", [] { invert_selection(); }, views);
		add("select.whole_entities", "Select whole entities", "Ctrl+Alt+E", [] { select_whole_entities(); }).when(any_selected);
		add("select.none", "Deselect", "", [] { deselect(); });

		// ---- view
		add("view.hide_selected", "Hide selected", "H", [] { hide_selected(); }, views).when(any_selected);
		add("view.show_all", "Show all", "Shift+H", [] { show_all(); }, views);
		add("view.cycle_axis", "Next 2D view axis ( XY, YZ, XZ )", "Ctrl+Tab", [] { cycle_grid_axis(); });
		add("view.surface", "Surface inspector", "S", [] { get().show_surface = !get().show_surface; }, views, no_map).toggle([] { return get().show_surface; });

		// ---- grid
		add("grid.smaller", "Grid smaller", "[", [] { grid_smaller(); }, views);
		add("grid.larger", "Grid larger", "]", [] { grid_larger(); }, views);
		add("grid.snap", "Snap to the grid while dragging", "", [] { get_scene().snap = !get_scene().snap; }).toggle([] { return get_scene().snap; });

		// ---- brushes, entities
		add("csg.subtract", "CSG subtract", "", [] { csg_subtract(); }).when(any_selected);
		add("csg.hollow", "CSG hollow", "", [] { csg_hollow(); }).when(any_selected);
		add("brush.make_detail", "Make detail", "Ctrl+D", [] { set_detail(true); }).when(any_selected);
		add("brush.make_structural", "Make structural", "Ctrl+Shift+D", [] { set_detail(false); }).when(any_selected);
		add("brush.to_worldspawn", "Move brushes to the worldspawn", "", [] { move_to_worldspawn(); }).when(any_selected);
		add("entity.connect", "Connect entities", "Ctrl+K", [] { connect_selected(); }).when(any_selected);

		// ---- tools
		add("tool.clipper", "Clipper", "C", [] { set_clipper(!get().clipper.on); }, views).toggle(clipping);
		add("tool.vertex", "Vertex / edge mode", "V", [] { set_vertex_mode(!get().vertex_mode); }, views).toggle([] { return get().vertex_mode; });
		add("clip.apply", "Clip: keep the front", "Enter|KeypadEnter", [] { apply_clip(0); }, views).when(clipping);
		add("clip.split", "Clip: split", "Shift+Enter|Shift+KeypadEnter", [] { apply_clip(2); }, views).when(clipping);
		add("clip.apply_back", "Clip: keep the back", "Ctrl+Enter|Ctrl+KeypadEnter", [] { apply_clip(1); }).when(clipping);

		// ---- gizmo and camera of the 3D view
		add("gizmo.move", "Gizmo: move", "W", [] { get().gizmo = gizmo_tool::move; }, view_3d).toggle([] { return get().gizmo == gizmo_tool::move; });
		// while the handles of the 3D view are shown E is its edge mode ( edit3d.cpp ): nothing is rotated there
		add("gizmo.rotate", "Gizmo: rotate", "E", [] { get().gizmo = gizmo_tool::rotate; }, view_3d).when([] { return !get().vertex_mode; }).toggle([] { return get().gizmo == gizmo_tool::rotate; });
		add("gizmo.scale", "Gizmo: scale", "R", [] { get().gizmo = gizmo_tool::scale; }, view_3d).toggle([] { return get().gizmo == gizmo_tool::scale; });
		add("gizmo.world", "Gizmo: world / local axes", "", [] { get().gizmo_world = !get().gizmo_world; }).toggle([] { return get().gizmo_world; });
		add("camera.next_spawn", "Camera: next spawn point", "N", [] { viewport_request("next_spawn"); }, view_3d);
		add("camera.overview", "Camera: whole map from above", "Home", [] { viewport_request("overview"); }, view_3d);
		add("camera.focus", "Camera: go to the selection", "F", [] { viewport_request("focus"); }, view_3d);

		// ---- console
		using args_t = const std::vector<std::string>&;

		// act <id>: what the key, the menu entry and the toolbar button of an action do
		command::register_command("act", [](args_t args)
		{
			if (args.size() < 2)
			{
				game::printf_to_console("usage: act <action id>   ( actions [text] lists them )\n");
				return;
			}

			const action* a = find(args[1]);
			if (!a) { game::printf_to_console("^1no action %s\n", args[1].c_str()); return; }
			if (!run(args[1])) { game::printf_to_console("%s: not available right now\n", args[1].c_str()); return; }

			if (a->checked) { game::printf_to_console("%s: %s\n", a->id.c_str(), a->checked() ? "on" : "off"); }
		});

		// actions [text]: ids, keys and labels
		command::register_command("actions", [](args_t args)
		{
			const std::string needle = args.size() > 1 ? lower(args[1]) : std::string();
			int shown = 0;

			for (const auto& a : all())
			{
				if (!needle.empty() && lower(a->id).find(needle) == std::string::npos && lower(a->label).find(needle) == std::string::npos) { continue; }

				std::string keys = chord_text(a->keys[0]);
				if (!a->keys[1].empty()) { keys += (keys.empty() ? "" : " | ") + chord_text(a->keys[1]); }
				game::printf_to_console("  %-28s %-18s %s\n", a->id.c_str(), keys.c_str(), a->label.c_str());
				shown++;
			}

			game::printf_to_console("%d actions\n", shown);
		});

		// bind <id> <key> [2]: eg. bind edit.clone Ctrl+Shift+D;  unbind <id>;  bind_reset
		command::register_command("bind", [](args_t args)
		{
			action* a = args.size() > 2 ? find(args[1]) : nullptr;
			chord c;

			if (!a || !parse_chord(args[2], c))
			{
				game::printf_to_console(a ? "^1bind: no key \"%s\"\n" : "usage: bind <action id> <key> [2]   eg. bind edit.clone Ctrl+Shift+D\n", args.size() > 2 ? args[2].c_str() : "");
				return;
			}

			const int slot = args.size() > 3 && args[3] == "2" ? 1 : 0;

			for (const action* other : conflicts_of(*a, c))
			{
				game::printf_to_console("^3[keys] %s is also the key of %s ( %s )\n", chord_text(c).c_str(), other->id.c_str(), other->label.c_str());
			}

			set_key(*a, slot, c);
			game::printf_to_console("[keys] %s: %s\n", a->id.c_str(), chord_text(c).c_str());
		});

		command::register_command("unbind", [](args_t args)
		{
			if (action* a = args.size() > 1 ? find(args[1]) : nullptr)
			{
				a->keys[1] = {};
				set_key(*a, 0, {});
				game::printf_to_console("[keys] %s: no key\n", a->id.c_str());
			}
		});

		command::register_command("bind_reset", [](args_t)
		{
			reset_keys();
			game::printf_to_console("[keys] every action has its default key again\n");
		});

		// ed_hotkeys [filter] | capture <id> [2]: the Edit Hotkeys window; "capture" waits for the next key like a click on a key field
		command::register_command("ed_hotkeys", [](args_t args)
		{
			g_show_hotkeys = true;

			if (args.size() > 2 && args[1] == "capture")
			{
				g_capture = find(args[2]);
				g_capture_slot = args.size() > 3 && args[3] == "2" ? 1 : 0;
			}
			else
			{
				g_filter = args.size() > 1 ? args[1] : std::string();
			}
		});

		command::register_command("ed_help", [](args_t) { g_show_help = true; });
	}

	// ---- Edit Hotkeys ------------------------------------------------------------------------------------------

	void open_hotkeys_window() { g_show_hotkeys = true; ImGui::SetWindowFocus("Edit Hotkeys##window"); }
	void open_help_window() { g_show_help = true; ImGui::SetWindowFocus("Help##window"); }

	void draw_hotkeys_window()
	{
		if (!g_show_hotkeys)
		{
			g_capture = nullptr;
			return;
		}

		ImGui::SetNextWindowSize(ImVec2(760.0f, 620.0f), ImGuiCond_FirstUseEver);

		if (!ImGui::Begin("Edit Hotkeys##window", &g_show_hotkeys, ImGuiWindowFlags_NoCollapse))
		{
			ImGui::End();
			return;
		}

		ImGui::SetNextItemWidth(260.0f);
		ImGui::InputTextWithHint("##hotkey_filter", "search: action or key", &g_filter);
		ImGui::SameLine();

		if (ImGui::Button("Reset all to defaults"))
		{
			ImGui::OpenPopup("##reset_keys");
		}

		if (ImGui::BeginPopup("##reset_keys"))
		{
			ImGui::TextUnformatted("Give every action its default key again?");

			if (ImGui::Button("Reset", ImVec2(100.0f, 0.0f)))
			{
				reset_keys();
				ImGui::CloseCurrentPopup();
			}

			ImGui::SameLine();
			if (ImGui::Button("Cancel", ImVec2(100.0f, 0.0f))) { ImGui::CloseCurrentPopup(); }
			ImGui::EndPopup();
		}

		// every key that two actions share where both can hear it
		int conflicts = 0;
		for (const auto& a : g_actions)
		{
			for (const chord& c : a->keys) { conflicts += !conflicts_of(*a, c).empty(); }
		}

		ImGui::SameLine();

		if (conflicts)
		{
			ImGui::TextColored(ImVec4(0.96f, 0.55f, 0.35f, 1.0f), "%d keys are used twice: the action of the smaller place wins", conflicts);
		}
		else
		{
			ImGui::TextDisabled("%d actions. Click a key to change it; Esc cancels.", static_cast<int>(g_actions.size()));
		}

		ImGui::TextDisabled("Saved to codol_editor_hotkeys.cfg next to the editor. Where: the place the mouse has to be over for the key to count.");

		const std::string needle = lower(g_filter);

		if (ImGui::BeginTable("##hotkeys", 5, ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_Resizable))
		{
			ImGui::TableSetupScrollFreeze(0, 1);
			ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthStretch, 2.4f);
			ImGui::TableSetupColumn("Key", ImGuiTableColumnFlags_WidthStretch, 1.2f);
			ImGui::TableSetupColumn("Second key", ImGuiTableColumnFlags_WidthStretch, 1.2f);
			ImGui::TableSetupColumn("Where", ImGuiTableColumnFlags_WidthStretch, 0.8f);
			ImGui::TableSetupColumn("##reset", ImGuiTableColumnFlags_WidthFixed, 64.0f);
			ImGui::TableHeadersRow();

			// by category, in the order the actions were registered
			std::vector<std::string> categories;
			for (const auto& a : g_actions)
			{
				if (std::find(categories.begin(), categories.end(), a->category) == categories.end()) { categories.push_back(a->category); }
			}

			static const char* places[] = { "anywhere", "a view", "3D view", "2D view" };

			for (const auto& category : categories)
			{
				bool header = false;

				for (const auto& a : g_actions)
				{
					if (a->category != category)
					{
						continue;
					}

					if (!needle.empty() && lower(a->label).find(needle) == std::string::npos && lower(a->id).find(needle) == std::string::npos
						&& lower(chord_text(a->keys[0])).find(needle) == std::string::npos && lower(chord_text(a->keys[1])).find(needle) == std::string::npos)
					{
						continue;
					}

					if (!header)
					{
						header = true;
						ImGui::TableNextRow(ImGuiTableRowFlags_Headers);
						ImGui::TableNextColumn();
						ImGui::TextColored(ImVec4(1.0f, 0.72f, 0.25f, 1.0f), "%s", category.c_str());
					}

					ImGui::PushID(a.get());
					ImGui::TableNextRow();
					ImGui::TableNextColumn();
					ImGui::AlignTextToFramePadding();
					ImGui::TextUnformatted(a->label.c_str());

					if (ImGui::IsItemHovered())
					{
						ImGui::SetTooltip("%s%s%s", a->id.c_str(), a->tip.empty() ? "" : "\n", a->tip.c_str());
					}

					for (int slot = 0; slot < 2; slot++)
					{
						ImGui::TableNextColumn();
						ImGui::PushID(slot);

						const bool capturing = g_capture == a.get() && g_capture_slot == slot;
						const auto clash = conflicts_of(*a, a->keys[slot]);
						const std::string text = capturing ? "press a key .." : (a->keys[slot].empty() ? "-" : chord_text(a->keys[slot]));

						if (!clash.empty()) { ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.96f, 0.55f, 0.35f, 1.0f)); }
						if (capturing) { ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive)); }

						if (ImGui::Button(text.c_str(), ImVec2(-28.0f, 0.0f)))
						{
							g_capture = capturing ? nullptr : a.get();
							g_capture_slot = slot;
						}

						if (capturing) { ImGui::PopStyleColor(); }
						if (!clash.empty()) { ImGui::PopStyleColor(); }

						if (!clash.empty() && ImGui::IsItemHovered())
						{
							std::string list = "Also the key of:";
							for (const action* other : clash) { list += "\n  " + other->label + "  ( " + other->id + " )"; }
							ImGui::SetTooltip("%s", list.c_str());
						}

						ImGui::SameLine(0.0f, 2.0f);

						if (ImGui::Button("x", ImVec2(24.0f, 0.0f)))
						{
							set_key(*a, slot, {});
						}

						if (ImGui::IsItemHovered()) { ImGui::SetTooltip("No key"); }
						ImGui::PopID();
					}

					ImGui::TableNextColumn();
					ImGui::AlignTextToFramePadding();
					ImGui::TextDisabled("%s", places[std::clamp(a->where, 0, 3)]);

					ImGui::TableNextColumn();
					const bool changed = a->keys[0] != a->defaults[0] || a->keys[1] != a->defaults[1];

					if (changed && ImGui::Button("default"))
					{
						a->keys[1] = a->defaults[1];
						set_key(*a, 0, a->defaults[0]);
					}

					ImGui::PopID();
				}
			}

			ImGui::EndTable();
		}

		ImGui::End();
	}

	// ---- Help --------------------------------------------------------------------------------------------------

	void draw_help_window()
	{
		if (!g_show_help)
		{
			return;
		}

		ImGui::SetNextWindowSize(ImVec2(720.0f, 640.0f), ImGuiCond_FirstUseEver);

		if (!ImGui::Begin("Help##window", &g_show_help, ImGuiWindowFlags_NoCollapse))
		{
			ImGui::End();
			return;
		}

		const ImVec4 title(1.0f, 0.72f, 0.25f, 1.0f);

		const auto rows = [](const char* id, const std::initializer_list<std::pair<const char*, const char*>> lines)
		{
			if (ImGui::BeginTable(id, 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV))
			{
				ImGui::TableSetupColumn("##what", ImGuiTableColumnFlags_WidthFixed, 250.0f);
				ImGui::TableSetupColumn("##does", ImGuiTableColumnFlags_WidthStretch);

				for (const auto& [what, does] : lines)
				{
					ImGui::TableNextRow();
					ImGui::TableNextColumn();
					ImGui::TextUnformatted(what);
					ImGui::TableNextColumn();
					ImGui::TextWrapped("%s", does);
				}

				ImGui::EndTable();
			}
		};

		if (ImGui::BeginTabBar("##help_tabs"))
		{
			if (ImGui::BeginTabItem("3D view"))
			{
				ImGui::TextColored(title, "Mouse");
				rows("##help_3d", {
					{ "Click", "select what is under the mouse ( a model, a brush, the box of an entity )" },
					{ "Ctrl / Shift + click", "add to the selection, or take out of it" },
					{ "Ctrl + Shift + click", "select the face of a brush ( texture tools, Surface inspector )" },
					{ "Right click", "create an entity where the mouse points" },
					{ "Right drag", "look around; with W A S D fly, E / Space up, Q / C down, wheel changes the speed" },
					{ "Left drag", "drive: up / down moves along the ground, left / right turns" },
					{ "Middle drag, or left + right", "pan" },
					{ "Wheel", "forward / back" },
					{ "Shift while moving", "four times the speed" },
					{ "Drag the gizmo", "move, rotate or scale the selection; Ctrl while dragging: the opposite of the snap setting" },
					{ "Mesh painter ( Tools )", "left drag paints models onto what the mouse is over" } });
				ImGui::Spacing();
				ImGui::TextDisabled("The keys of the view are actions: see the Keys tab, and Edit > Edit Hotkeys to change them.");
				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("2D views"))
			{
				ImGui::TextColored(title, "Mouse");
				rows("##help_2d", {
					{ "Click", "select; another click at the same place takes the next object behind" },
					{ "Shift + click", "add to the selection, or take out of it" },
					{ "Shift + drag", "select everything the rectangle touches" },
					{ "Drag inside the selection's box", "move the selection, in steps of the grid" },
					{ "Drag next to a selected brush", "move its faces on that side ( resize )" },
					{ "Drag with nothing selected", "a new brush with the material of the Materials window, as deep as the last selection" },
					{ "Drag the camera", "moves the camera of the 3D view" },
					{ "Right or middle drag", "move the view" },
					{ "Wheel", "zoom at the mouse" },
					{ "Right click", "create an entity there" },
					{ "Clipper", "click two or three points, then clip: keep the front, split, or keep the back ( see the Keys tab )" },
					{ "Vertex / edge mode", "drag the corners of the selected brushes; an edge that points at you moves as a whole" } });
				ImGui::Spacing();
				ImGui::TextDisabled("Ctrl + Tab cycles the axis of a view; View > 2D View adds another one.");
				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("Keys"))
			{
				static std::string filter;
				ImGui::SetNextItemWidth(260.0f);
				ImGui::InputTextWithHint("##help_filter", "search", &filter);
				ImGui::SameLine();
				if (ImGui::Button("Edit Hotkeys ..")) { g_show_hotkeys = true; }

				const std::string needle = lower(filter);

				if (ImGui::BeginTable("##help_keys", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_BordersInnerV))
				{
					ImGui::TableSetupScrollFreeze(0, 1);
					ImGui::TableSetupColumn("Key", ImGuiTableColumnFlags_WidthFixed, 200.0f);
					ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthStretch);
					ImGui::TableSetupColumn("Console", ImGuiTableColumnFlags_WidthFixed, 230.0f);
					ImGui::TableHeadersRow();

					std::string category;

					std::vector<const action*> sorted;
					for (const auto& a : g_actions) { sorted.push_back(a.get()); }
					std::stable_sort(sorted.begin(), sorted.end(), [](const action* a, const action* b) { return a->category < b->category; });

					for (const action* a : sorted)
					{
						std::string keys = chord_text(a->keys[0]);
						if (!a->keys[1].empty()) { keys += (keys.empty() ? "" : "  /  ") + chord_text(a->keys[1]); }

						if (!needle.empty() && lower(a->label).find(needle) == std::string::npos && lower(keys).find(needle) == std::string::npos && lower(a->id).find(needle) == std::string::npos)
						{
							continue;
						}

						if (a->category != category)
						{
							category = a->category;
							ImGui::TableNextRow(ImGuiTableRowFlags_Headers);
							ImGui::TableNextColumn();
							ImGui::TextColored(title, "%s", category.c_str());
						}

						ImGui::TableNextRow();
						ImGui::TableNextColumn();
						ImGui::TextUnformatted(keys.empty() ? "-" : keys.c_str());
						ImGui::TableNextColumn();
						ImGui::TextUnformatted(a->label.c_str());
						ImGui::TableNextColumn();
						ImGui::TextDisabled("act %s", a->id.c_str());
					}

					ImGui::EndTable();
				}

				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("Console"))
			{
				ImGui::PushTextWrapPos(0.0f);
				ImGui::TextUnformatted(
					"Everything the mouse can do is a console command as well ( View > Console; \"help\" lists them ).\n\n"
					"act <id>            what the key, the menu entry and the toolbar button of an action do\n"
					"actions [text]      the actions with their keys\n"
					"bind <id> <key>     give an action a key: bind edit.clone Ctrl+Shift+D\n"
					"unbind <id>, bind_reset\n"
					"<setting> [value]   a setting of Edit > Preferences by its console name: ed_cam_fov 90\n\n"
					"A list of commands can be run at start: set CODOL_STARTUP_CMDS=\"map_load my.map;wait 5;ed_shot shot.png;quit\".");
				ImGui::PopTextWrapPos();
				ImGui::EndTabItem();
			}

			ImGui::EndTabBar();
		}

		ImGui::End();
	}
}
