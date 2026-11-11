#include "std_include.hpp"

// windows.h macros
#undef min
#undef max

#include "host.hpp"
#include "ui.hpp"

#include <commdlg.h>

// Editing actions, shortcuts, the File / Edit menus and the console commands. Every action is one function that
// the menu, the key and the command call - so anything the mouse can do can be driven ( and tested ) by command.

namespace editor::ui
{
	state& get()
	{
		static state s;
		return s;
	}

	namespace
	{
		// an action that waits for the answer to "unsaved changes"
		std::function<void()> g_pending;
		bool g_ask = false;

		void guarded(const bool force, std::function<void()> action)
		{
			auto& s = get_scene();

			if (force || !s.open || !s.doc.modified())
			{
				action();
				return;
			}

			g_pending = std::move(action);
			g_ask = true;
		}

		double number(const std::vector<std::string>& args, const std::size_t index, const double fallback = 0.0)
		{
			return index < args.size() ? std::atof(args[index].c_str()) : fallback;
		}

		edit::vec3 vector(const std::vector<std::string>& args, const std::size_t first)
		{
			return { number(args, first), number(args, first + 1), number(args, first + 2) };
		}

		bool need_map()
		{
			if (!get_scene().open)
			{
				game::printf_to_console("^1no map is open\n");
				return false;
			}

			return true;
		}

		void print_result(const bool ok, const char* what)
		{
			auto& doc = get_scene().doc;

			if (ok)
			{
				game::printf_to_console("%s: ok ( %d selected, undo %d / redo %d )\n", what, static_cast<int>(doc.selection().size()),
					static_cast<int>(doc.undo_count()), static_cast<int>(doc.redo_count()));
			}
			else
			{
				game::printf_to_console("^1%s: %s\n", what, doc.last_error().c_str());
			}
		}

		std::string describe(const object_id id)
		{
			auto& doc = get_scene().doc;
			char text[512];

			if (const auto* e = doc.find_entity(id))
			{
				const auto o = e->origin();
				std::snprintf(text, sizeof(text), "#%u entity %s  origin %.6g %.6g %.6g  angles %s  model %s", id, std::string(e->classname()).c_str(), o.x, o.y, o.z,
					std::string(e->value("angles", "-")).c_str(), std::string(e->value("model", "-")).c_str());
			}
			else if (const auto* b = doc.find_brush(id))
			{
				const auto box = doc.object_bounds(id);
				const auto* owner = doc.find_entity(b->owner);
				std::snprintf(text, sizeof(text), "#%u brush of %s ( #%u )  %d faces  %s  mins %.6g %.6g %.6g  maxs %.6g %.6g %.6g  layer \"%s\"%s  entity origin %s", id,
					owner ? std::string(owner->classname()).c_str() : "?", b->owner, static_cast<int>(b->faces.size()),
					b->faces.empty() ? "" : b->faces[0].texture.material.c_str(), box.mins.x, box.mins.y, box.mins.z, box.maxs.x, box.maxs.y, box.maxs.z,
					std::string(doc.layer_of(id)).c_str(), b->is_detail() ? "  detail" : "",
					owner ? std::string(owner->value("origin", "-")).c_str() : "-");
			}
			else if (const auto* p = doc.find_patch(id))
			{
				std::snprintf(text, sizeof(text), "#%u patch %d x %d  %s", id, p->width, p->height, p->material.c_str());
			}
			else
			{
				std::snprintf(text, sizeof(text), "#%u ( gone )", id);
			}

			return text;
		}

		std::filesystem::path file_dialog(const bool save, const std::filesystem::path& initial)
		{
			wchar_t file[MAX_PATH * 4] = {};
			const std::wstring start = initial.filename().wstring();
			wcsncpy_s(file, start.c_str(), _TRUNCATE);

			const std::wstring dir = initial.parent_path().wstring();
			std::error_code ec;
			std::filesystem::create_directories(initial.parent_path(), ec);

			OPENFILENAMEW ofn = {};
			ofn.lStructSize = sizeof(ofn);
			ofn.hwndOwner = main_window();
			ofn.lpstrFilter = L"Map source ( *.map )\0*.map\0All files\0*.*\0";
			ofn.lpstrFile = file;
			ofn.nMaxFile = static_cast<DWORD>(std::size(file));
			ofn.lpstrInitialDir = dir.c_str();
			ofn.lpstrDefExt = L"map";
			ofn.Flags = OFN_NOCHANGEDIR | OFN_PATHMUSTEXIST | (save ? OFN_OVERWRITEPROMPT : OFN_FILEMUSTEXIST);

			const BOOL ok = save ? GetSaveFileNameW(&ofn) : GetOpenFileNameW(&ofn);
			return ok ? std::filesystem::path(file) : std::filesystem::path();
		}

		std::filesystem::path map_source_dir()
		{
			return std::filesystem::u8path(utils::fs::get_basepath()) / "map_source";
		}
	}

	bool report(const bool ok, const char* action)
	{
		if (!ok)
		{
			const auto& error = get_scene().doc.last_error();
			ImGui::Toast(ImGuiToastType_Warning, action, error.empty() ? "Nothing to do." : error.c_str(), 2500);
			game::printf_to_console("%s: %s\n", action, error.empty() ? "nothing to do" : error.c_str());
		}

		return ok;
	}

	// ---- actions ---------------------------------------------------------------------------------------------

	void undo()
	{
		auto& doc = get_scene().doc;
		const std::string name(doc.undo_name());

		if (doc.undo())
		{
			game::printf_to_console("undo: %s\n", name.c_str());
		}
	}

	void redo()
	{
		auto& doc = get_scene().doc;
		const std::string name(doc.redo_name());

		if (doc.redo())
		{
			game::printf_to_console("redo: %s\n", name.c_str());
		}
	}

	void delete_selection() { report(get_scene().doc.delete_selection(), "Delete"); }
	void clone_selection() { report(get_scene().clone_selection(), "Clone"); }

	void hide_selected()
	{
		auto& doc = get_scene().doc;
		if (!doc.selection().empty())
		{
			doc.hide_selected();
		}
	}

	void show_all() { get_scene().doc.show_all(); }
	void select_all() { get_scene().doc.select_all(); }
	void invert_selection() { get_scene().doc.invert_selection(); }

	void deselect()
	{
		auto& s = get_scene();
		s.doc.deselect_all();
		s.inspecting = false;
	}

	void select_whole_entities() { get_scene().doc.select_whole_entities(); }
	void snap_to_grid() { report(get_scene().doc.snap_to_grid(get_scene().grid), "Snap to grid"); }
	void csg_subtract() { report(get_scene().doc.csg_subtract(), "CSG subtract"); }
	void csg_hollow() { report(get_scene().doc.csg_hollow(std::max(1.0, get_scene().grid)), "CSG hollow"); }
	void set_detail(const bool detail) { report(get_scene().doc.set_detail(detail), detail ? "Make detail" : "Make structural"); }
	void move_to_worldspawn() { report(get_scene().doc.move_selection_to_worldspawn(), "Move to worldspawn"); }

	void toggle_texture_lock()
	{
		auto& doc = get_scene().doc;
		doc.set_texture_lock(!doc.texture_lock());
		game::printf_to_console("texture lock: %s\n", doc.texture_lock() ? "on" : "off");
	}

	void connect_selected()
	{
		auto& doc = get_scene().doc;
		const auto ents = doc.selected_entities();

		if (ents.size() != 2)
		{
			ImGui::Toast(ImGuiToastType_Warning, "Connect", "Select exactly two entities: the first one will target the second.", 3000);
			game::printf_to_console("Connect: select exactly two entities\n");
			return;
		}

		if (report(doc.connect_entities(ents[0], ents[1]), "Connect"))
		{
			game::printf_to_console("connected #%u -> #%u ( target \"%s\" )\n", ents[0], ents[1], std::string(doc.find_entity(ents[0])->value("target")).c_str());
		}
	}

	void grid_smaller() { auto& s = get_scene(); s.set_grid(s.grid / 2.0); }
	void grid_larger() { auto& s = get_scene(); s.set_grid(s.grid * 2.0); }

	void nudge(const int axis, const int sign)
	{
		auto& s = get_scene();
		if (s.doc.selection().empty())
		{
			return;
		}

		edit::vec3 delta;
		delta[axis] = s.grid * sign;

		// keys pressed in a row are one undo step
		s.doc.begin_transaction("Nudge", 0x6e756467ull + s.doc.selection_counter());
		s.translate(delta);
		s.doc.end_transaction();
	}

	void nudge_view(const int dx, const int dy)
	{
		auto& s = get_scene();
		const auto& st = get();

		if (st.hovered_2d >= 0)
		{
			const int axis = grid_view_axis(st.hovered_2d);
			const int h = axis == 0 ? 1 : 0, v = axis == 2 ? 1 : 2;

			if (dx) { nudge(h, dx); }
			if (dy) { nudge(v, dy); }
			return;
		}

		// 3D: the world axis closest to "right" / "ahead" on the ground
		const auto axes = s.cam.axes();
		const auto dominant = [](const float x, const float y, int& axis, int& sign)
		{
			axis = std::fabs(x) >= std::fabs(y) ? 0 : 1;
			sign = (axis == 0 ? x : y) >= 0.0f ? 1 : -1;
		};

		int axis = 0, sign = 1;

		if (dx)
		{
			dominant(-axes.left.x, -axes.left.y, axis, sign);
			nudge(axis, sign * dx);
		}

		if (dy)
		{
			dominant(axes.forward.x, axes.forward.y, axis, sign);
			nudge(axis, sign * dy);
		}
	}

	void set_clipper(const bool on)
	{
		auto& st = get();
		st.clipper.on = on;
		st.clipper.points.clear();
		st.clipper.dragging = -1;

		if (on)
		{
			st.vertex_mode = false;
		}
	}

	void set_vertex_mode(const bool on)
	{
		auto& st = get();
		st.vertex_mode = on;

		if (on)
		{
			st.clipper.on = false;
			st.clipper.points.clear();
		}
	}

	bool apply_clip(const int keep)
	{
		auto& st = get();
		auto& s = get_scene();
		auto& points = st.clipper.points;

		if (points.size() < 2)
		{
			game::printf_to_console("Clip: place two or three points first\n");
			return false;
		}

		// two points: the plane runs along the view axis of the 2D view they were placed in
		edit::vec3 third;
		if (points.size() >= 3)
		{
			third = points[2];
		}
		else
		{
			third = points[0];
			third[st.clipper.axis] += 64.0;
		}

		const auto mode = keep == 2 ? edit::clip_keep::both : (keep == 1 ? edit::clip_keep::back : edit::clip_keep::front);
		const bool ok = report(s.doc.clip(points[0], points[1], third, mode), keep == 2 ? "Split" : "Clip");

		if (ok)
		{
			points.clear();
		}

		return ok;
	}

	// ---- files -----------------------------------------------------------------------------------------------

	void open_zone(const std::string& map, const bool force)
	{
		guarded(force, [map]
		{
			std::string error;
			if (!get_scene().open_zone(map, error))
			{
				game::printf_to_console("^1[CoDOL] %s\n", error.c_str());
				ImGui::Toast(ImGuiToastType_Error, "Open map", error.c_str(), 6000);
			}
		});
	}

	void open_map_file(const std::filesystem::path& file, const bool force)
	{
		guarded(force, [file]
		{
			std::string error;
			if (!get_scene().open_map_file(file, error))
			{
				game::printf_to_console("^1[CoDOL] %s\n", error.c_str());
				ImGui::Toast(ImGuiToastType_Error, "Open .map", error.c_str(), 6000);
			}
		});
	}

	void open_map_dialog()
	{
		guarded(false, []
		{
			if (const auto file = file_dialog(false, map_source_dir() / ""); !file.empty())
			{
				open_map_file(file, true);
			}
		});
	}

	bool save()
	{
		auto& s = get_scene();
		std::string error;

		if (!s.save(error))
		{
			game::printf_to_console("^1[CoDOL] %s\n", error.c_str());
			ImGui::Toast(ImGuiToastType_Error, "Save", error.c_str(), 6000);
			return false;
		}

		game::printf_to_console("[CoDOL] saved %s\n", to_utf8(s.map_file).c_str());
		ImGui::Toast(ImGuiToastType_Success, "Saved", to_utf8(s.map_file).c_str(), 2500);
		return true;
	}

	void save_as_dialog()
	{
		auto& s = get_scene();
		if (!s.open)
		{
			return;
		}

		const auto file = file_dialog(true, s.map_file.empty() ? s.default_map_file() : s.map_file);
		if (file.empty())
		{
			return;
		}

		std::string error;
		if (!s.save_as(file, error))
		{
			game::printf_to_console("^1[CoDOL] %s\n", error.c_str());
			ImGui::Toast(ImGuiToastType_Error, "Save as", error.c_str(), 6000);
			return;
		}

		game::printf_to_console("[CoDOL] saved %s\n", to_utf8(s.map_file).c_str());
	}

	void export_entities(const bool reload)
	{
		auto& s = get_scene();
		std::filesystem::path written;
		std::string summary;

		if (!s.export_entities(written, summary))
		{
			game::printf_to_console("^1[CoDOL] %s\n", summary.c_str());
			ImGui::Toast(ImGuiToastType_Error, "Export", summary.c_str(), 6000);
			return;
		}

		game::printf_to_console("[CoDOL] %s\n", summary.c_str());
		ImGui::Toast(ImGuiToastType_Success, "Export", summary.c_str(), 5000);

		if (reload)
		{
			// rcon to the local server; it reports by itself when nobody answers
			components::codol_game::load_map(s.map_name);
		}
	}

	void close_map(const bool force)
	{
		guarded(force, [] { get_scene().close(); });
	}

	void exit_editor()
	{
		guarded(false, [] { request_exit(); });
	}

	void draw_modals()
	{
		if (g_ask)
		{
			g_ask = false;
			ImGui::OpenPopup("Unsaved changes##modal");
		}

		ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

		if (ImGui::BeginPopupModal("Unsaved changes##modal", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		{
			auto& s = get_scene();
			ImGui::Text("%s has changes that are not saved.", s.map_name.c_str());
			ImGui::Spacing();

			const auto finish = [](const bool run)
			{
				auto action = std::move(g_pending);
				g_pending = nullptr;
				ImGui::CloseCurrentPopup();

				if (run && action)
				{
					action();
				}
			};

			if (ImGui::Button("Save", ImVec2(110.0f, 0.0f)))
			{
				finish(save());
			}

			ImGui::SameLine();
			if (ImGui::Button("Discard", ImVec2(110.0f, 0.0f)))
			{
				finish(true);
			}

			ImGui::SameLine();
			if (ImGui::Button("Cancel", ImVec2(110.0f, 0.0f)) || ImGui::IsKeyPressed(ImGuiKey_Escape, false))
			{
				finish(false);
			}

			ImGui::EndPopup();
		}
	}

	// ---- menus -----------------------------------------------------------------------------------------------

	void draw_file_menu_items()
	{
		auto& s = get_scene();

		if (ImGui::MenuItem("Open map ..", "Ctrl+O"))
		{
			GET_GUI(ggui::codol_map_ents_dialog)->open();
			ImGui::SetWindowFocus("Maps##window");
		}

		if (ImGui::MenuItem("Open .map .."))
		{
			open_map_dialog();
		}

		if (ImGui::MenuItem("Save", "Ctrl+S", false, s.open))
		{
			save();
		}

		if (ImGui::MenuItem("Save as ..", nullptr, false, s.open))
		{
			save_as_dialog();
		}

		if (ImGui::MenuItem("Close map", nullptr, false, s.open))
		{
			close_map();
		}

		ImGui::Separator();

		auto* maps = GET_GUI(ggui::codol_map_ents_dialog);

		if (ImGui::MenuItem("Export entities to the server", "Ctrl+E", false, s.open))
		{
			export_entities(maps->m_reload_after_export);
		}

		ImGui::MenuItem("Load the map on the server after the export", nullptr, &maps->m_reload_after_export);
	}

	void draw_edit_menu()
	{
		auto& s = get_scene();
		auto& doc = s.doc;
		auto& st = get();
		const bool open = s.open;
		const bool any = open && !doc.selection().empty();

		const std::string undo_label = open && doc.can_undo() ? "Undo " + std::string(doc.undo_name()) : "Undo";
		const std::string redo_label = open && doc.can_redo() ? "Redo " + std::string(doc.redo_name()) : "Redo";

		if (ImGui::MenuItem(undo_label.c_str(), "Ctrl+Z", false, open && doc.can_undo())) { undo(); }
		if (ImGui::MenuItem(redo_label.c_str(), "Ctrl+Y", false, open && doc.can_redo())) { redo(); }
		ImGui::Separator();

		if (ImGui::MenuItem("Delete", "Backspace", false, any)) { delete_selection(); }
		if (ImGui::MenuItem("Clone", "Space", false, any)) { clone_selection(); }
		if (ImGui::MenuItem("Hide selected", "H", false, any)) { hide_selected(); }
		if (ImGui::MenuItem("Show all", "Shift+H", false, open)) { show_all(); }
		ImGui::Separator();

		if (ImGui::MenuItem("Select all", "Ctrl+A", false, open)) { select_all(); }
		if (ImGui::MenuItem("Invert selection", "I", false, open)) { invert_selection(); }
		if (ImGui::MenuItem("Select whole entities", "Ctrl+Alt+E", false, any)) { select_whole_entities(); }
		if (ImGui::MenuItem("Deselect", "Esc", false, any || s.inspecting)) { deselect(); }
		ImGui::Separator();

		if (ImGui::MenuItem("Snap to grid", "Ctrl+G", false, any)) { snap_to_grid(); }
		if (ImGui::MenuItem("CSG subtract", nullptr, false, any)) { csg_subtract(); }
		if (ImGui::MenuItem("CSG hollow", nullptr, false, any)) { csg_hollow(); }
		if (ImGui::MenuItem("Make detail", "Ctrl+D", false, any)) { set_detail(true); }
		if (ImGui::MenuItem("Make structural", "Ctrl+Shift+D", false, any)) { set_detail(false); }
		if (ImGui::MenuItem("Connect entities", "Ctrl+K", false, any)) { connect_selected(); }
		if (ImGui::MenuItem("Move brushes to the worldspawn", nullptr, false, any)) { move_to_worldspawn(); }
		ImGui::Separator();

		bool lock = doc.texture_lock();
		if (ImGui::MenuItem("Texture lock", "Shift+T", &lock, open)) { toggle_texture_lock(); }

		bool clipper = st.clipper.on;
		if (ImGui::MenuItem("Clipper ( 2D views )", "C", &clipper, open)) { set_clipper(clipper); }

		bool vertex = st.vertex_mode;
		if (ImGui::MenuItem("Vertex / edge mode ( 2D views )", "V", &vertex, open)) { set_vertex_mode(vertex); }

		if (ImGui::BeginMenu("Grid"))
		{
			for (int g = 1; g <= 512; g *= 2)
			{
				const std::string label = std::to_string(g);
				if (ImGui::MenuItem(label.c_str(), g == 1 ? "[ smaller" : (g == 512 ? "] larger" : nullptr), s.grid == g))
				{
					s.set_grid(g);
				}
			}

			ImGui::Separator();
			ImGui::MenuItem("Snap to the grid", nullptr, &s.snap);
			ImGui::EndMenu();
		}
	}

	// ---- shortcuts -------------------------------------------------------------------------------------------

	void process_shortcuts()
	{
		auto& st = get();
		auto& s = get_scene();
		const auto& io = ImGui::GetIO();

		st.hovered_2d = st.next_hovered_2d;
		st.hovered_3d = st.next_hovered_3d;
		st.next_hovered_2d = -1;
		st.next_hovered_3d = false;

		// never while a text field has the keyboard, a menu or a dialog is open
		if (io.WantTextInput || ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel))
		{
			return;
		}

		const bool ctrl = io.KeyCtrl, shift = io.KeyShift, alt = io.KeyAlt;
		const auto pressed = [](const ImGuiKey key, const bool repeat = false) { return ImGui::IsKeyPressed(key, repeat); };

		if (ctrl && pressed(ImGuiKey_O))
		{
			GET_GUI(ggui::codol_map_ents_dialog)->open();
			ImGui::SetWindowFocus("Maps##window");
		}

		// while the right button flies the camera the keys move it; a drag owns the document until it ends
		if (!s.open || ImGui::IsMouseDown(ImGuiMouseButton_Right) || s.dragging())
		{
			return;
		}

		if (ctrl && !alt)
		{
			if (pressed(ImGuiKey_Z, true)) { shift ? redo() : undo(); }
			if (pressed(ImGuiKey_Y, true)) { redo(); }
			if (pressed(ImGuiKey_S)) { save(); }
			if (pressed(ImGuiKey_E)) { export_entities(GET_GUI(ggui::codol_map_ents_dialog)->m_reload_after_export); }
			if (pressed(ImGuiKey_A)) { select_all(); }
			if (pressed(ImGuiKey_G)) { snap_to_grid(); }
			if (pressed(ImGuiKey_K)) { connect_selected(); }
			if (pressed(ImGuiKey_D)) { set_detail(!shift); }
			if (pressed(ImGuiKey_Tab)) { cycle_grid_axis(); }

			if (st.clipper.on && (pressed(ImGuiKey_Enter) || pressed(ImGuiKey_KeypadEnter))) { apply_clip(1); }
			return;
		}

		if (ctrl && alt)
		{
			if (pressed(ImGuiKey_E)) { select_whole_entities(); }
			return;
		}

		if (pressed(ImGuiKey_Delete) || pressed(ImGuiKey_Backspace)) { delete_selection(); }
		if (pressed(ImGuiKey_Space)) { clone_selection(); }
		if (pressed(ImGuiKey_H)) { (shift || alt) ? show_all() : hide_selected(); }
		if (pressed(ImGuiKey_I)) { invert_selection(); }
		if (pressed(ImGuiKey_LeftBracket)) { grid_smaller(); }
		if (pressed(ImGuiKey_RightBracket)) { grid_larger(); }
		if (shift && pressed(ImGuiKey_T)) { toggle_texture_lock(); }
		if (!shift && pressed(ImGuiKey_S)) { st.show_surface = !st.show_surface; }
		if (pressed(ImGuiKey_C)) { set_clipper(!st.clipper.on); }
		if (pressed(ImGuiKey_V)) { set_vertex_mode(!st.vertex_mode); }

		if (pressed(ImGuiKey_Escape))
		{
			// one thing at a time: clip points, then the tool, then the selection
			if (st.clipper.on && !st.clipper.points.empty()) { st.clipper.points.clear(); }
			else if (st.clipper.on) { set_clipper(false); }
			else if (st.vertex_mode) { set_vertex_mode(false); }
			else { deselect(); }
		}

		if (st.clipper.on && (pressed(ImGuiKey_Enter) || pressed(ImGuiKey_KeypadEnter)))
		{
			apply_clip(shift ? 2 : 0);
		}

		if (pressed(ImGuiKey_LeftArrow, true)) { nudge_view(-1, 0); }
		if (pressed(ImGuiKey_RightArrow, true)) { nudge_view(1, 0); }
		if (pressed(ImGuiKey_UpArrow, true)) { nudge_view(0, 1); }
		if (pressed(ImGuiKey_DownArrow, true)) { nudge_view(0, -1); }

		// along the axis the view looks ( up / down in 3D and in the top view )
		const int depth = st.hovered_2d >= 0 ? grid_view_axis(st.hovered_2d) : 2;
		if (pressed(ImGuiKey_PageUp, true)) { nudge(depth, 1); }
		if (pressed(ImGuiKey_PageDown, true)) { nudge(depth, -1); }
	}

	// ---- console commands ------------------------------------------------------------------------------------

	void register_commands()
	{
		using args_t = const std::vector<std::string>&;

		// ---- files
		command::register_command("map_open", [](args_t args)
		{
			if (args.size() < 2) { game::printf_to_console("usage: map_open <zone>   eg. map_open mp_rust\n"); return; }
			open_zone(args[1], true);
		});

		command::register_command("map_load", [](args_t args)
		{
			if (args.size() < 2) { game::printf_to_console("usage: map_load <file.map>\n"); return; }
			open_map_file(std::filesystem::u8path(args[1]), true);
		});

		command::register_command("map_save", [](args_t args)
		{
			if (!need_map()) { return; }

			if (args.size() > 1)
			{
				std::string error;
				if (!get_scene().save_as(std::filesystem::u8path(args[1]), error)) { game::printf_to_console("^1%s\n", error.c_str()); return; }
				game::printf_to_console("[CoDOL] saved %s\n", args[1].c_str());
			}
			else
			{
				save();
			}
		});

		command::register_command("map_export", [](args_t args)
		{
			if (need_map()) { export_entities(args.size() > 1 && args[1] == "reload"); }
		});

		command::register_command("map_close", [](args_t) { close_map(true); });
		command::register_command("quit", [](args_t) { request_exit(); });

		// ---- selection
		command::register_command("sel_id", [](args_t args)
		{
			if (!need_map() || args.size() < 2) { return; }
			auto& doc = get_scene().doc;
			const auto id = static_cast<object_id>(std::atoi(args[1].c_str()));

			if (doc.kind(id) == edit::object_kind::none) { game::printf_to_console("^1no object #%u\n", id); return; }
			doc.select(id, args.size() > 2 ? edit::select_mode::add : edit::select_mode::replace);
			game::printf_to_console("%s\n", describe(id).c_str());
		});

		// sel_class <classname> [n]: every entity of the class, or the n-th
		command::register_command("sel_class", [](args_t args)
		{
			if (!need_map() || args.size() < 2) { return; }
			auto& doc = get_scene().doc;
			std::vector<object_id> ids;

			for (const object_id id : doc.entities())
			{
				if (doc.find_entity(id)->classname() == args[1]) { ids.push_back(id); }
			}

			if (args.size() > 2)
			{
				const auto n = static_cast<std::size_t>(std::atoi(args[2].c_str()));
				ids = n < ids.size() ? std::vector<object_id>{ ids[n] } : std::vector<object_id>{};
			}

			doc.select(std::span<const object_id>(ids), args.size() > 3 ? edit::select_mode::add : edit::select_mode::replace);
			game::printf_to_console("%d entities of class %s, %d objects selected\n", static_cast<int>(ids.size()), args[1].c_str(), static_cast<int>(doc.selection().size()));
		});

		// sel_codol <codol_id>: the entity with that index in the entity string of the zone
		command::register_command("sel_codol", [](args_t args)
		{
			if (!need_map() || args.size() < 2) { return; }
			auto& doc = get_scene().doc;

			for (const object_id id : doc.entities())
			{
				if (doc.find_entity(id)->value("codol_id") == args[1])
				{
					doc.select(id, args.size() > 2 ? edit::select_mode::add : edit::select_mode::replace);
					game::printf_to_console("%s\n", describe(id).c_str());
					return;
				}
			}

			game::printf_to_console("^1no entity with codol_id %s\n", args[1].c_str());
		});

		// sel_brush <n> [entity id]: the n-th brush of the worldspawn ( or of an entity )
		command::register_command("sel_brush", [](args_t args)
		{
			if (!need_map() || args.size() < 2) { return; }
			auto& doc = get_scene().doc;
			const auto* e = doc.find_entity(args.size() > 2 ? static_cast<object_id>(std::atoi(args[2].c_str())) : doc.worldspawn());
			const auto n = static_cast<std::size_t>(std::atoi(args[1].c_str()));

			if (!e || n >= e->children.size()) { game::printf_to_console("^1no such brush\n"); return; }
			doc.select(e->children[n], args.size() > 3 ? edit::select_mode::add : edit::select_mode::replace);
			game::printf_to_console("%s\n", describe(e->children[n]).c_str());
		});

		command::register_command("sel_face", [](args_t args)
		{
			if (!need_map() || args.size() < 3) { return; }
			get_scene().doc.select_face({ static_cast<object_id>(std::atoi(args[1].c_str())), std::atoi(args[2].c_str()) },
				args.size() > 3 ? edit::select_mode::add : edit::select_mode::replace);
		});

		// sel_rect <axis x|y|z> <lo a> <lo b> <hi a> <hi b> [inside]: the drag rectangle of a 2D view
		command::register_command("sel_rect", [](args_t args)
		{
			if (!need_map() || args.size() < 6) { return; }
			auto& doc = get_scene().doc;
			const int axis = args[1] == "x" ? 0 : (args[1] == "y" ? 1 : 2);
			const auto ids = doc.query_rect(axis, { number(args, 2), number(args, 3) }, { number(args, 4), number(args, 5) }, args.size() > 6);
			doc.select(std::span<const object_id>(ids), edit::select_mode::replace);
			game::printf_to_console("%d objects in the rectangle\n", static_cast<int>(ids.size()));
		});

		command::register_command("sel_all", [](args_t) { if (need_map()) { select_all(); print_result(true, "select all"); } });
		command::register_command("sel_none", [](args_t) { if (need_map()) { deselect(); } });
		command::register_command("sel_invert", [](args_t) { if (need_map()) { invert_selection(); print_result(true, "invert"); } });
		command::register_command("sel_entity", [](args_t) { if (need_map()) { select_whole_entities(); print_result(true, "whole entities"); } });

		command::register_command("sel_info", [](args_t)
		{
			if (!need_map()) { return; }
			auto& s = get_scene();
			auto& doc = s.doc;
			const auto sel = doc.selection();

			game::printf_to_console("%s: %d entities, %d brushes, %d patches; %d selected, %d faces; modified %d; undo %d ( %s ), redo %d ( %s ); grid %g\n",
				s.map_name.c_str(), static_cast<int>(doc.entities().size()), static_cast<int>(doc.brush_count()), static_cast<int>(doc.patch_count()),
				static_cast<int>(sel.size()), static_cast<int>(doc.selected_faces().size()), doc.modified() ? 1 : 0,
				static_cast<int>(doc.undo_count()), std::string(doc.undo_name()).c_str(), static_cast<int>(doc.redo_count()), std::string(doc.redo_name()).c_str(), s.grid);

			for (std::size_t i = 0; i < sel.size() && i < 8; i++)
			{
				game::printf_to_console("  %s\n", describe(sel[i]).c_str());
			}

			if (const auto box = doc.selection_bounds(); box.valid())
			{
				game::printf_to_console("  bounds %.6g %.6g %.6g .. %.6g %.6g %.6g\n", box.mins.x, box.mins.y, box.mins.z, box.maxs.x, box.maxs.y, box.maxs.z);
			}

			const auto stats = s.renderer.stats();
			game::printf_to_console("  renderer: %d hidden, %d moved, %d placed models, %d meshes\n", stats.hidden_objects, stats.moved_objects, stats.placed_model_count, stats.mesh_count);
		});

		// ed_pick [yaw offset] [pitch offset] [toggle] [face]: click on what the camera looks at
		command::register_command("ed_pick", [](args_t args)
		{
			if (!need_map()) { return; }
			auto& s = get_scene();
			auto c = s.cam;
			c.yaw += static_cast<float>(number(args, 1));
			c.pitch += static_cast<float>(number(args, 2));
			pick_ray(c.origin, c.axes().forward, args.size() > 3 && args[3] != "0", args.size() > 4 && args[4] != "0");

			for (const object_id id : s.doc.selection())
			{
				game::printf_to_console("picked %s\n", describe(id).c_str());
			}

			if (s.inspecting)
			{
				game::printf_to_console("looking at a world surface ( %s ), not a document object\n", s.inspected.material.c_str());
			}
		});

		// ed_click <px> <py> [toggle] [face]: a left click at a pixel of the 3D view
		command::register_command("ed_click", [](args_t args)
		{
			if (need_map() && args.size() > 2)
			{
				viewport_click(static_cast<float>(number(args, 1)), static_cast<float>(number(args, 2)), args.size() > 3 && args[3] != "0", args.size() > 4 && args[4] != "0");
			}
		});

		// ---- editing
		command::register_command("ed_undo", [](args_t args)
		{
			if (!need_map()) { return; }
			for (int i = std::max(1, static_cast<int>(number(args, 1, 1))); i > 0 && get_scene().doc.can_undo(); i--) { undo(); }
		});

		command::register_command("ed_redo", [](args_t args)
		{
			if (!need_map()) { return; }
			for (int i = std::max(1, static_cast<int>(number(args, 1, 1))); i > 0 && get_scene().doc.can_redo(); i--) { redo(); }
		});

		command::register_command("ed_move", [](args_t args)
		{
			if (need_map() && args.size() > 3) { print_result(get_scene().translate(vector(args, 1)), "move"); }
		});

		// ed_rotate <x|y|z> <degrees>: about the centre of the selection
		command::register_command("ed_rotate", [](args_t args)
		{
			if (!need_map() || args.size() < 3) { return; }
			auto& s = get_scene();
			edit::vec3 axis;
			axis[args[1] == "x" ? 0 : (args[1] == "y" ? 1 : 2)] = 1.0;
			print_result(s.rotate(s.selection_pivot(), axis, number(args, 2)), "rotate");
		});

		command::register_command("ed_scale", [](args_t args)
		{
			if (!need_map() || args.size() < 2) { return; }
			auto& s = get_scene();
			const double f = number(args, 1, 1.0);
			print_result(s.scale(s.selection_pivot(), { f, number(args, 2, f), number(args, 3, f) }), "scale");
		});

		command::register_command("ed_delete", [](args_t) { if (need_map()) { print_result(get_scene().doc.delete_selection(), "delete"); } });
		command::register_command("ed_clone", [](args_t) { if (need_map()) { print_result(get_scene().clone_selection(), "clone"); } });
		command::register_command("ed_hide", [](args_t) { if (need_map()) { hide_selected(); } });
		command::register_command("ed_showall", [](args_t) { if (need_map()) { show_all(); } });
		command::register_command("ed_snap", [](args_t) { if (need_map()) { print_result(get_scene().doc.snap_to_grid(get_scene().grid), "snap"); } });
		command::register_command("ed_subtract", [](args_t) { if (need_map()) { print_result(get_scene().doc.csg_subtract(), "csg subtract"); } });
		command::register_command("ed_worldspawn", [](args_t) { if (need_map()) { print_result(get_scene().doc.move_selection_to_worldspawn(), "to worldspawn"); } });
		command::register_command("ed_connect", [](args_t) { if (need_map()) { connect_selected(); } });

		command::register_command("ed_hollow", [](args_t args)
		{
			if (need_map()) { print_result(get_scene().doc.csg_hollow(number(args, 1, std::max(1.0, get_scene().grid))), "csg hollow"); }
		});

		command::register_command("ed_detail", [](args_t args)
		{
			if (need_map()) { print_result(get_scene().doc.set_detail(number(args, 1, 1.0) != 0.0), "detail"); }
		});

		command::register_command("ed_texlock", [](args_t args)
		{
			auto& doc = get_scene().doc;
			doc.set_texture_lock(args.size() > 1 ? number(args, 1) != 0.0 : !doc.texture_lock());
			game::printf_to_console("texture lock: %s\n", doc.texture_lock() ? "on" : "off");
		});

		command::register_command("ed_grid", [](args_t args)
		{
			auto& s = get_scene();
			if (args.size() > 1)
			{
				if (args[1] == "+") { grid_larger(); }
				else if (args[1] == "-") { grid_smaller(); }
				else { s.set_grid(number(args, 1, 8.0)); }
			}

			if (args.size() > 2) { s.snap = number(args, 2) != 0.0; }
			game::printf_to_console("grid %g, snap %s\n", s.grid, s.snap ? "on" : "off");
		});

		// ed_nudge <left|right|up|down|in|out>: what the arrow keys do in the 3D view
		command::register_command("ed_nudge", [](args_t args)
		{
			if (!need_map() || args.size() < 2) { return; }
			if (args[1] == "left") { nudge_view(-1, 0); }
			else if (args[1] == "right") { nudge_view(1, 0); }
			else if (args[1] == "up") { nudge_view(0, 1); }
			else if (args[1] == "down") { nudge_view(0, -1); }
			else if (args[1] == "in") { nudge(2, 1); }
			else if (args[1] == "out") { nudge(2, -1); }
			print_result(true, "nudge");
		});

		// ed_box x1 y1 z1 x2 y2 z2 [material]
		command::register_command("ed_box", [](args_t args)
		{
			if (!need_map() || args.size() < 7) { game::printf_to_console("usage: ed_box x1 y1 z1 x2 y2 z2 [material]\n"); return; }
			auto& doc = get_scene().doc;
			const auto id = doc.create_box(vector(args, 1), vector(args, 4), args.size() > 7 ? std::string_view(args[7]) : std::string_view());

			if (id) { game::printf_to_console("created %s\n", describe(id).c_str()); }
			else { game::printf_to_console("^1%s\n", doc.last_error().c_str()); }
		});

		// ed_entity <classname> [x y z]: a point entity ( at the position, else where the camera looks ), or a
		// brush entity around the selected brushes when the class is one
		command::register_command("ed_entity", [](args_t args)
		{
			if (!need_map() || args.size() < 2) { return; }
			auto& s = get_scene();
			const auto pos = args.size() > 4 ? vector(args, 2) : edit::snap_point(s.ray_target(s.cam.origin, s.cam.axes().forward), s.snap ? s.grid : 0.0);
			const auto id = s.create_entity(args[1], pos);

			if (id) { game::printf_to_console("created %s\n", describe(id).c_str()); }
			else { game::printf_to_console("^1%s\n", s.doc.last_error().c_str()); }
		});

		// ed_place <xmodel> [x y z]: a model entity ( class of the Models window ) where the camera looks
		command::register_command("ed_place", [](args_t args)
		{
			if (!need_map() || args.size() < 2) { game::printf_to_console("usage: ed_place <xmodel> [x y z]\n"); return; }
			auto& s = get_scene();
			const auto pos = args.size() > 4 ? vector(args, 2) : s.ray_target(s.cam.origin, s.cam.axes().forward);
			const auto id = s.create_model_entity(args[1], pos, 0.0);

			if (id) { game::printf_to_console("created %s\n", describe(id).c_str()); }
			else { game::printf_to_console("^1%s\n", s.doc.last_error().c_str()); }
		});

		// ed_key <key> <value>: on every selected entity
		command::register_command("ed_key", [](args_t args)
		{
			if (!need_map() || args.size() < 3) { return; }
			auto& doc = get_scene().doc;
			bool ok = false;

			doc.begin_transaction("Set key");
			for (const object_id id : doc.selected_entities()) { ok |= doc.set_key(id, args[1], args[2]); }
			doc.end_transaction();
			print_result(ok, "set key");
		});

		command::register_command("ed_delkey", [](args_t args)
		{
			if (!need_map() || args.size() < 2) { return; }
			auto& doc = get_scene().doc;
			bool ok = false;

			doc.begin_transaction("Remove key");
			for (const object_id id : doc.selected_entities()) { ok |= doc.remove_key(id, args[1]); }
			doc.end_transaction();
			print_result(ok, "remove key");
		});

		// ed_clip ax ay az bx by bz cx cy cz [front|back|both]
		command::register_command("ed_clip", [](args_t args)
		{
			if (!need_map() || args.size() < 10) { return; }
			const auto keep = args.size() > 10 && args[10] == "both" ? edit::clip_keep::both : (args.size() > 10 && args[10] == "back" ? edit::clip_keep::back : edit::clip_keep::front);
			print_result(get_scene().doc.clip(vector(args, 1), vector(args, 4), vector(args, 7), keep), "clip");
		});

		// ed_clip_apply [front|back|both]: what Enter / Ctrl+Enter / Shift+Enter do with the points of the clipper
		command::register_command("ed_clip_apply", [](args_t args)
		{
			if (need_map()) { print_result(apply_clip(args.size() > 1 && args[1] == "both" ? 2 : (args.size() > 1 && args[1] == "back" ? 1 : 0)), "clipper"); }
		});

		// ed_resize <x|y|z> <+|-> <distance>: move the faces of the selected brushes on that side
		command::register_command("ed_resize", [](args_t args)
		{
			if (!need_map() || args.size() < 4) { return; }
			const int axis = args[1] == "x" ? 0 : (args[1] == "y" ? 1 : 2);
			print_result(get_scene().resize_selection(axis, args[2] == "-" ? -1 : 1, number(args, 3)), "resize");
		});

		// ed_corner cx cy cz dx dy dz: move the corner at c of the selected brushes by d
		command::register_command("ed_corner", [](args_t args)
		{
			if (!need_map() || args.size() < 7) { return; }
			auto& doc = get_scene().doc;
			const edit::vec3 corner = vector(args, 1);
			bool ok = false;

			doc.begin_transaction("Move corner");
			for (const object_id id : std::vector<object_id>(doc.selection().begin(), doc.selection().end()))
			{
				if (doc.kind(id) == edit::object_kind::brush) { ok |= doc.move_brush_corners(id, std::span<const edit::vec3>(&corner, 1), vector(args, 4)); }
			}

			doc.end_transaction();
			print_result(ok, "move corner");
		});

		// ---- textures
		command::register_command("tex_default", [](args_t args)
		{
			auto& doc = get_scene().doc;
			if (args.size() > 1) { doc.set_default_material(args[1]); }
			game::printf_to_console("material of new brushes: %s\n", std::string(doc.default_material()).c_str());
		});

		command::register_command("tex_set", [](args_t args)
		{
			if (need_map()) { print_result(get_scene().doc.set_material(args.size() > 1 ? std::string_view(args[1]) : get_scene().doc.default_material()), "set material"); }
		});

		command::register_command("tex_shift", [](args_t args) { if (need_map()) { print_result(get_scene().doc.shift_texture(number(args, 1), number(args, 2)), "shift texture"); } });
		command::register_command("tex_scale", [](args_t args) { if (need_map()) { print_result(get_scene().doc.scale_texture(number(args, 1, 1.0), number(args, 2, number(args, 1, 1.0))), "scale texture"); } });
		command::register_command("tex_rotate", [](args_t args) { if (need_map()) { print_result(get_scene().doc.rotate_texture(number(args, 1)), "rotate texture"); } });
		command::register_command("tex_fit", [](args_t args) { if (need_map()) { print_result(get_scene().doc.fit_texture(number(args, 1, 1.0), number(args, 2, 1.0)), "fit texture"); } });

		// ---- layers
		command::register_command("layer_add", [](args_t args) { if (need_map() && args.size() > 1) { print_result(get_scene().doc.add_layer(args[1]), "add layer"); } });
		command::register_command("layer_remove", [](args_t args) { if (need_map() && args.size() > 1) { print_result(get_scene().doc.remove_layer(args[1]), "remove layer"); } });
		command::register_command("layer_rename", [](args_t args) { if (need_map() && args.size() > 2) { print_result(get_scene().doc.rename_layer(args[1], args[2]), "rename layer"); } });
		command::register_command("layer_active", [](args_t args) { if (need_map() && args.size() > 1) { print_result(get_scene().doc.set_active_layer(args[1]), "active layer"); } });
		command::register_command("layer_move", [](args_t args) { if (need_map() && args.size() > 1) { print_result(get_scene().doc.move_selection_to_layer(args[1]), "move to layer"); } });

		command::register_command("layer_hide", [](args_t args)
		{
			if (need_map() && args.size() > 1) { get_scene().doc.set_layer_hidden(args[1], number(args, 2, 1.0) != 0.0); }
		});

		command::register_command("layer_list", [](args_t)
		{
			for (const auto& l : get_scene().doc.layers())
			{
				game::printf_to_console("  \"%s\"%s%s\n", l.name.c_str(), l.active ? "  active" : "", l.hidden ? "  hidden" : "");
			}
		});

		// ---- view
		command::register_command("ed_camera", [](args_t args)
		{
			auto& s = get_scene();
			if (args.size() > 3) { s.cam.origin = { static_cast<float>(number(args, 1)), static_cast<float>(number(args, 2)), static_cast<float>(number(args, 3)) }; }
			if (args.size() > 5) { s.cam.pitch = static_cast<float>(number(args, 4)); s.cam.yaw = static_cast<float>(number(args, 5)); }
			game::printf_to_console("camera %.0f %.0f %.0f  pitch %.1f yaw %.1f\n", s.cam.origin.x, s.cam.origin.y, s.cam.origin.z, s.cam.pitch, s.cam.yaw);
		});

		command::register_command("ed_focus", [](args_t) { get_scene().look_at_selection(); });

		// ed_brushes <off|selected|all>: the collision brushes of the zone in the 3D view
		command::register_command("ed_brushes", [](args_t args)
		{
			auto& s = get_scene();
			if (args.size() > 1)
			{
				s.show_brushes = args[1] == "all" ? brush_display::all : (args[1] == "selected" ? brush_display::selected : brush_display::off);
			}
		});

		// ed_windows <letters>: s selection, e entity, l layers, m models, t materials, u surface
		command::register_command("ed_windows", [](args_t args)
		{
			auto& st = get();
			const std::string which = args.size() > 1 ? args[1] : "se";
			st.show_selection = which.find('s') != std::string::npos;
			st.show_entity = which.find('e') != std::string::npos;
			st.show_layers = which.find('l') != std::string::npos;
			st.show_models = which.find('m') != std::string::npos;
			st.show_materials = which.find('t') != std::string::npos;
			st.show_surface = which.find('u') != std::string::npos;
		});

		// ed_focus_window <title>: bring a docked window to the front, eg. ed_focus_window Materials##window
		command::register_command("ed_focus_window", [](args_t args)
		{
			if (args.size() > 1) { ImGui::SetWindowFocus(args[1].c_str()); }
		});

		command::register_command("ed_gizmo", [](args_t args)
		{
			if (args.size() > 1)
			{
				get().gizmo = args[1] == "rotate" ? gizmo_tool::rotate : (args[1] == "scale" ? gizmo_tool::scale : gizmo_tool::move);
			}
		});

		// ed_tool <clip|vertex|none>
		command::register_command("ed_tool", [](args_t args)
		{
			const std::string tool = args.size() > 1 ? args[1] : "none";
			set_clipper(tool == "clip");
			set_vertex_mode(tool == "vertex");
		});

		// ed_material_filter <text> [group 0|1|2]
		command::register_command("ed_material_filter", [](args_t args)
		{
			set_material_filter(args.size() > 1 ? args[1] : "", args.size() > 2 ? std::atoi(args[2].c_str()) : -1);
		});

		// ed_materials <text> / ed_models <text>: names that contain the text ( the first 40 )
		const auto list_names = [](const std::vector<std::string>& names, const std::string& needle)
		{
			int shown = 0, total = 0;

			for (const auto& name : names)
			{
				if (name.find(needle) == std::string::npos)
				{
					continue;
				}

				if (++total <= 40)
				{
					game::printf_to_console("  %s\n", name.c_str());
					shown++;
				}
			}

			game::printf_to_console("%d of %d names with \"%s\"\n", shown, total, needle.c_str());
		};

		command::register_command("ed_materials", [list_names](args_t args)
		{
			if (need_map()) { list_names(get_scene().renderer.material_names(), args.size() > 1 ? args[1] : ""); }
		});

		command::register_command("ed_models", [list_names](args_t args)
		{
			if (need_map()) { list_names(get_scene().renderer.model_names(), args.size() > 1 ? args[1] : ""); }
		});

		command::register_command("ed_stats", [](args_t)
		{
			auto& st = get();
			auto& s = get_scene();
			const auto stats = s.renderer.stats();

			game::printf_to_console("frame: %.2f ms editor work ( max %.2f since the last ed_stats ), %.1f fps shown; renderer %.2f ms, %d draws, %lld triangles; cache: %d + %d objects, %d + %d entities, %d target lines\n",
				st.frame_ms, st.frame_ms_max, ImGui::GetIO().Framerate, stats.cpu_ms, stats.draw_calls, static_cast<long long>(stats.triangles),
				static_cast<int>(s.statics.objects.size()), static_cast<int>(s.selected.objects.size()), static_cast<int>(s.statics.entities.size()), static_cast<int>(s.selected.entities.size()), static_cast<int>(s.connections.size()));
			st.frame_ms_max = 0.0;
		});

		// view2d <axis|center|zoom|fit|new|click|drag|rect> ..: the 2D views ( grid_view.cpp )
		command::register_command("view2d", [](args_t args) { grid_view_command(args); });

		command::register_command("help", [](args_t)
		{
			std::string line;
			for (const auto& name : command::names())
			{
				line += name + "  ";
				if (line.size() > 110) { game::printf_to_console("%s\n", line.c_str()); line.clear(); }
			}

			if (!line.empty()) { game::printf_to_console("%s\n", line.c_str()); }
		});
	}
}
