#include "std_include.hpp"

// windows.h macros
#undef min
#undef max

#include "host.hpp"
#include "menu_ext.hpp"
#include "ext.hpp"

// The actions of the Brush / Patch menus ( menu_brush_patch.cpp ), of the Selection / Grid / Misc / Region menus,
// the clipboard, the filters and the prefabs ( menu_selection.cpp, filters.cpp, prefabs.cpp ): each is the console
// command of that part with an id, a label and a default key - so its key can be changed in Edit > Edit Hotkeys,
// the Help window lists it and the toolbar can show it. The keys are the ones those parts had hard coded; where
// Radiant has another key that is free here it is the second key.
//
// A new action of this kind is one line:   command_action( "patch.invert", "Invert patch", "Ctrl+I", "patch_invert" );

namespace editor::ext
{
	namespace
	{
		actions::action& command_action(const char* id, const char* label, const char* keys, const char* line, const int where = actions::anywhere, const int options = actions::none)
		{
			const std::string text(line);
			return actions::add(id, label, keys, [text] { command::execute(text); }, where, options);
		}
	}

	void register_command_actions()
	{
		using namespace actions;
		const auto any_selected = [] { return !get_scene().doc.selection().empty(); };
		auto& st = ui::ext::get();

		// ---- clipboard, files
		command_action("edit.copy", "Copy", "Ctrl+C", "ed_copy").when(any_selected);
		command_action("edit.cut", "Cut", "Ctrl+X", "ed_cut").when(any_selected);
		command_action("edit.paste", "Paste", "Ctrl+V", "ed_paste");
		command_action("edit.paste_here", "Paste at the cursor / camera", "Ctrl+Shift+V", "ed_paste here");
		add("window.map_info", "Map Info", "Ctrl+M", [&st] { st.show_map_info = !st.show_map_info; }).toggle([&st] { return st.show_map_info; });
		add("window.entity_info", "Entity Info", "", [&st] { st.show_entity_info = !st.show_entity_info; }).toggle([&st] { return st.show_entity_info; });
		add("window.prefabs", "Prefab Browser", "", [&st] { st.show_prefab_browser = !st.show_prefab_browser; }, anywhere, no_map).toggle([&st] { return st.show_prefab_browser; });
		add("filter.window", "Filter window", "F", [&st] { st.show_filter = !st.show_filter; }, view_2d, no_map).toggle([&st] { return st.show_filter; });
		command_action("prefab.enter", "Enter prefab", "", "prefab_enter");
		command_action("prefab.leave", "Leave prefab", "", "prefab_leave");

		// ---- selection
		command_action("select.connected", "Select connected entities", "Ctrl+Shift+C", "sel_connected");
		add("select.key_value", "Select by key / value ..", "Ctrl+Shift+F", [&st] { st.open_key_value = true; });
		command_action("select.complete_tall", "Select complete tall", "", "sel_tall").when(any_selected);
		command_action("select.partial_tall", "Select partial tall", "", "sel_partial_tall").when(any_selected);
		command_action("select.touching", "Select touching", "", "sel_touching").when(any_selected);
		command_action("select.inside", "Select inside", "", "sel_inside").when(any_selected);
		command_action("select.next_in_group", "Next brush of the entity", "Tab", "sel_group", views).when(any_selected);
		command_action("select.previous_in_group", "Previous brush of the entity", "Shift+Tab", "sel_group prev", views).when(any_selected);
		command_action("select.flip_x", "Flip X", "", "ed_flip x").when(any_selected);
		command_action("select.flip_y", "Flip Y", "", "ed_flip y").when(any_selected);
		command_action("select.flip_z", "Flip Z", "", "ed_flip z").when(any_selected);
		command_action("select.rotate_x", "Rotate X 90", "", "ed_rotate3 90 0 0").when(any_selected);
		command_action("select.rotate_y", "Rotate Y 90", "", "ed_rotate3 0 90 0").when(any_selected);
		command_action("select.rotate_z", "Rotate Z 90", "Shift+D", "ed_rotate3 0 0 90", views).when(any_selected);
		add("select.rotate_dialog", "Arbitrary rotation ..", "", [&st] { st.open_rotate = true; }).when(any_selected);
		add("select.scale_dialog", "Arbitrary scale ..", "", [&st] { st.open_scale = true; }).when(any_selected);
		add("gizmo.rotate_toggle", "Gizmo: rotate on / off", "R", [] { auto& u = ui::get(); u.gizmo = u.gizmo == ui::gizmo_tool::rotate ? ui::gizmo_tool::move : ui::gizmo_tool::rotate; }, view_2d);
		command_action("select.drop_to_floor", "Drop to the floor", "", "ed_drop").when(any_selected);
		add("select.plant_models", "Plant new models on the floor", "", [&st] { st.plant_models = !st.plant_models; }, anywhere, no_map).toggle([&st] { return st.plant_models; });
		add("select.orient_to_floor", "Dropped models turn to the floor", "", [&st] { st.orient_to_floor = !st.orient_to_floor; }, anywhere, no_map).toggle([&st] { return st.orient_to_floor; });

		// ---- view
		command_action("view.hide_unselected", "Hide unselected", "Ctrl+H|Alt+H", "ed_hide_unselected").when(any_selected);
		command_action("view.show_last_hidden", "Show last hidden", "Ctrl+Shift+H", "ed_show_last");
		command_action("view.floor_up", "Up floor", "Ctrl+PageUp", "ed_floor up");
		command_action("view.floor_down", "Down floor", "Ctrl+PageDown", "ed_floor down");
		command_action("view.center_2d_on_camera", "Center the 2D views on the camera", "", "view2d_camera");

		// ---- brushes
		command_action("csg.merge", "CSG merge", "Ctrl+U", "ed_merge").when(any_selected);
		command_action("csg.auto_caulk", "Auto caulk", "Alt+C", "ed_autocaulk", views).when(any_selected);
		command_action("brush.weapon_clip", "Make weapon clip", "Ctrl+Shift+W", "ed_contents weaponClip").when(any_selected);
		command_action("brush.non_colliding", "Make non-colliding", "Ctrl+Shift+Minus", "ed_contents nonColliding").when(any_selected);
		command_action("entity.link", "Link the brushes to the entity", "", "ed_link").when(any_selected);
		add("entity.color", "Entity colour ..", "K", [&st] { st.open_color = true; }, views).when(any_selected);

		// ---- misc, region
		command_action("misc.next_leak", "Next leak spot / error", "Ctrl+Alt+K|Ctrl+Shift+K", "leak_next");
		command_action("misc.previous_leak", "Previous leak spot / error", "Ctrl+Alt+L|Ctrl+Shift+L", "leak_prev");
		add("misc.find_brush", "Find brush ..", "", [&st] { st.open_find = true; });
		add("misc.go_to", "Go to position ..", "", [&st] { st.open_goto = true; });
		command_action("region.off", "Region off", "", "region off");
		command_action("region.xy", "Region: the 2D view", "", "region xy");
		command_action("region.tall", "Region: tall brush", "", "region tall").when(any_selected);
		command_action("region.brush", "Region: brush", "", "region brush").when(any_selected);
		command_action("region.selected", "Region: selected brushes", "", "region selected").when(any_selected);

		// ---- filters: what a click does not select
		command_action("filter.noselect_curves", "Don't select curves", "", "ed_noselect curves", anywhere, no_map);
		command_action("filter.noselect_entities", "Don't select entities", "", "ed_noselect entities", anywhere, no_map);
		command_action("filter.noselect_sky", "Don't select sky", "", "ed_noselect sky", anywhere, no_map);
		command_action("filter.noselect_models", "Don't select models", "", "ed_noselect models", anywhere, no_map);
		command_action("filter.cycle_models", "Cycle preview models", "", "ed_models_cycle");

		// ---- patches
		command_action("patch.invert", "Invert patch", "Ctrl+I", "patch_invert").when(any_selected);
		command_action("patch.thicken", "Thicken patch ..", "Ctrl+T", "patch_dialog thicken");
		command_action("patch.transpose", "Transpose patch", "Ctrl+Shift+M", "patch_transpose").when(any_selected);
		command_action("patch.cycle_cap_texture", "Cycle cap texture", "Ctrl+Shift+P", "patch_cycle_cap").when(any_selected);
		command_action("patch.add_rows", "Add 2 rows", "Ctrl+KeypadAdd", "patch_add rows").when(any_selected);
		command_action("patch.add_columns", "Add 2 columns", "Ctrl+Shift+KeypadAdd", "patch_add columns").when(any_selected);
		command_action("patch.delete_rows", "Delete the last 2 rows", "Ctrl+KeypadSubtract", "patch_delete last rows").when(any_selected);
		command_action("patch.delete_columns", "Delete the last 2 columns", "Ctrl+Shift+KeypadSubtract", "patch_delete last columns").when(any_selected);
		command_action("patch.simple_curve", "Simple curve patch ..", "Shift+P", "patch_dialog simple", views);
		command_action("patch.simple_terrain", "Simple terrain patch ..", "", "patch_dialog terrain");
		command_action("patch.cap", "Cap ..", "Shift+C", "patch_dialog cap", views);
		command_action("patch.advanced_edit", "Advanced edit dialog", "Shift+S", "patch_dialog advanced", views);
		command_action("patch.vertex_edit", "Vert edit dialog", "", "patch_dialog vertex");
		command_action("patch.redisperse", "Re-disperse vertices", "", "patch_redisperse").when(any_selected);
		command_action("patch.redisperse_rows", "Re-disperse rows", "", "patch_redisperse rows").when(any_selected);
		command_action("patch.redisperse_columns", "Re-disperse columns", "", "patch_redisperse columns").when(any_selected);
		command_action("patch.weld", "Weld patches", "", "patch_weld").when(any_selected);
		command_action("patch.weld_brushes", "Weld to brush vertices", "", "patch_weld_brushes").when(any_selected);
		command_action("patch.subdivide_more", "Inc subdivision", "", "patch_subdiv 1").when(any_selected);
		command_action("patch.subdivide_less", "Dec subdivision", "", "patch_subdiv -1").when(any_selected);
		command_action("patch.split", "Split patch", "", "patch_split").when(any_selected);
		command_action("patch.turn_edges", "Turn terrain edges", "", "patch_turn_edges").when(any_selected);
		command_action("patch.lock_vertices", "Lock vertices", "", "patch_lock").when(any_selected);
		command_action("patch.unlock_vertices", "Unlock vertices", "", "patch_unlock").when(any_selected);
		command_action("patch.curve_to_terrain", "Curve to terrain", "", "patch_curve_to_terrain").when(any_selected);

		// ---- the tools of the 3D view ( edit3d.cpp; its keys E, B and Esc are registered there )
		command_action("tool.object", "3D tool: objects", "", "ed3d tool object");
		command_action("tool.vertex_3d", "3D tool: vertices", "", "ed3d tool vertex");
		command_action("tool.edge", "3D tool: edges", "", "ed3d tool edge");
		command_action("tool.clip_3d", "3D tool: clipper", "", "ed3d tool clip");
		command_action("tool.create", "3D tool: create brushes", "", "ed3d tool create");
	}
}
