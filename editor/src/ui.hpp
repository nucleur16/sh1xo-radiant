#pragma once

// What the windows of the editor share: tool state, the editing actions ( menu, shortcuts and console commands
// all end up in the same functions ), file handling with the "unsaved changes" question.

#include "scene.hpp"

namespace editor::ui
{
	enum class gizmo_tool : int
	{
		move,
		rotate,
		scale,
	};

	// the clipper of the 2D views: two or three points; with two, the plane runs along the view axis
	struct clipper_state
	{
		bool on = false;
		std::vector<edit::vec3> points;
		int axis = 2;				// view axis the points were placed in
		int dragging = -1;
	};

	struct state
	{
		gizmo_tool gizmo = gizmo_tool::move;
		bool gizmo_world = true;

		bool show_selection = true;
		bool show_entity = true;
		bool show_layers = false;
		bool show_models = false;
		bool show_materials = false;
		bool show_surface = false;

		// which view the mouse was over in the last frame: decides what view relative shortcuts mean
		int hovered_2d = -1;		// index of the 2D view, -1: none
		bool hovered_3d = false;
		int next_hovered_2d = -1;
		bool next_hovered_3d = false;

		clipper_state clipper;
		bool vertex_mode = false;

		// "Create entity" popup: opened by a right click in a view
		bool open_create_popup = false;
		edit::vec3 create_position;
		std::string create_filter;

		// frame time of the editor's own work ( everything but Present ), milliseconds
		double frame_ms = 0.0;
		double frame_ms_max = 0.0;
	};

	state& get();

	// ---- actions ---------------------------------------------------------------------------------------------
	// Each does one thing to the document and tells the user when there was nothing to do.
	void undo();
	void redo();
	void delete_selection();
	void clone_selection();
	void hide_selected();
	void show_all();
	void select_all();
	void invert_selection();
	void deselect();
	void select_whole_entities();
	void snap_to_grid();
	void csg_subtract();
	void csg_hollow();
	void set_detail(bool detail);
	void toggle_texture_lock();
	void connect_selected();
	void move_to_worldspawn();
	void grid_smaller();
	void grid_larger();
	// one grid step along a world axis
	void nudge(int axis, int sign);
	// one grid step, relative to the view the mouse is over: dx right, dy up ( on screen ), dz towards / away
	void nudge_view(int dx, int dy);

	// clipper: keep 0 front, 1 back, 2 both ( split )
	bool apply_clip(int keep);
	void set_clipper(bool on);
	void set_vertex_mode(bool on);

	// toast + console line; returns ok
	bool report(bool ok, const char* action);

	// ---- files -----------------------------------------------------------------------------------------------
	// These ask before unsaved changes are thrown away. `force`: do not ask ( console commands ).
	void open_zone(const std::string& map, bool force = false);
	void open_map_file(const std::filesystem::path& file, bool force = false);
	void open_map_dialog();
	bool save();
	void save_as_dialog();
	void export_entities(bool reload);
	void close_map(bool force = false);
	void exit_editor();

	// ---- per frame -------------------------------------------------------------------------------------------
	void process_shortcuts();
	void draw_file_menu_items();
	void draw_edit_menu();
	void draw_modals();
	void draw_create_entity_popup();
	void draw_entity_window();
	void draw_layers_window();
	void draw_surface_window();

	// one row of filter + list of the classes of the catalog; returns the picked class
	const edit::class_info* class_picker(std::string& filter, float height, bool brush_classes_enabled);

	void register_commands();

	// Development aid ( like CODOL_STARTUP_CMDS ): the console commands ui_mouse / ui_key / ui_text feed mouse and
	// keyboard input to the gui, so that unattended runs go through the same code as a hand on the mouse. Called
	// between the platform backend's new frame and ImGui::NewFrame().
	void apply_injected_input();

	// the 2D views ( grid_view.cpp )
	void grid_view_command(const std::vector<std::string>& args);
	void draw_grid_views();
	void add_grid_view();
	void cycle_grid_axis();
	void draw_grid_view_menu_items();

	// view axis ( 0 x, 1 y, 2 z ) of a 2D view
	int grid_view_axis(int index);

	// the 3D view ( viewport.cpp ): a click along a ray / at a pixel of the view, as the mouse does it. The nearest of
	// what the renderer draws and what the document shows ( brushes, entity boxes ) is selected; `toggle` adds /
	// removes ( Ctrl or Shift click ), `face` selects the brush face ( Shift + Ctrl click ).
	void pick_ray(const float3& origin, const float3& direction, bool toggle, bool face);
	void viewport_click(float px, float py, bool toggle, bool face);
	void set_material_filter(const std::string& text, int group);
}
