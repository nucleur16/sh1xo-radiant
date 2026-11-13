#pragma once

// What menu_selection.cpp ( File / Edit additions, Selection, Grid, Misc and Region menus ), filters.cpp ( the
// Filter window ) and prefabs.cpp ( misc_prefab: instances, browser, enter / leave ) share. The entry points the
// rest of the editor calls are declared in ui.hpp.

#include "ui.hpp"
#include "ext.hpp"

namespace editor::ui::ext
{
	using args_t = const std::vector<std::string>&;

	inline double number(args_t args, const std::size_t index, const double fallback = 0.0)
	{
		return index < args.size() ? std::atof(args[index].c_str()) : fallback;
	}

	inline edit::vec3 vector(args_t args, const std::size_t first)
	{
		return { number(args, first), number(args, first + 1), number(args, first + 2) };
	}

	inline int axis_of(const std::string& text)
	{
		return text == "x" || text == "0" ? 0 : (text == "y" || text == "1" ? 1 : 2);
	}

	// console line "no map is open" when there is none
	bool need_map();
	// "<what>: ok ( n selected .. )" or the document's error, on the console
	void print_result(bool ok, const char* what);

	std::filesystem::path map_source_dir();
	std::filesystem::path file_dialog(bool save, const std::filesystem::path& initial);
	bool read_text(const std::filesystem::path& file, std::string& out);
	bool write_text(const std::filesystem::path& file, const std::string& text, std::string& error);

	// a 2D view as far as other code has to know it ( grid_view.cpp )
	struct view2d_info
	{
		int axis = 2;
		double center[2] = {};
		double zoom = 1.0;
		ImVec2 canvas_pos, canvas_size;
		bool open = false;

		[[nodiscard]] int h() const { return axis == 0 ? 1 : 0; }
		[[nodiscard]] int v() const { return axis == 2 ? 1 : 2; }
	};

	struct state
	{
		// windows
		bool show_map_info = false;
		bool show_entity_info = false;
		bool show_filter = false;
		bool show_prefab_browser = false;

		// dialogs ( opened by menu entries, drawn by draw_extra_windows )
		bool open_key_value = false;
		bool open_rotate = false;
		bool open_scale = false;
		bool open_find = false;
		bool open_goto = false;
		bool open_color = false;
		bool open_model_replace = false;
		bool open_convert_prefab = false;

		std::string key = "classname", pattern;
		float rotate_angles[3] = {};
		float scale_factors[3] = { 1.0f, 1.0f, 1.0f };
		bool scale_lock[3] = {};
		int find_entity = 0, find_brush = 0;
		float goto_position[3] = {};
		float color[3] = { 1.0f, 1.0f, 1.0f };
		std::string replace_from, replace_to;
		bool replace_selected_only = false;
		std::string prefab_name = "my_prefab";
		std::string entity_filter;

		// misc
		bool orient_to_floor = false;		// "drop to floor" also turns models to the surface
		bool plant_models = false;			// new models of the Models browser / ed_place are dropped to the floor
		bool show_grid_blocks = false;		// 2D views: the 1024 unit blocks
		bool show_prefab_contents = true;

		// the leak of the last build ( File > Generate File > Pointfile ) and the brushes that are no closed convex
		// volume ( Errorfile ): Misc > next / previous walks them
		std::vector<edit::vec3> leak;
		std::string leak_text;
		std::vector<object_id> errors;
		int spot = -1;

		// the next File > Build map compiles the region only
		bool region_build = false;
	};

	state& get();

	// ---- filters.cpp
	void draw_filter_window();
	void register_filter_commands();
	// once per frame: the filter of the window goes to the document ( also after a map was opened )
	void apply_filters();
	// Misc > Cycle Preview Models
	void cycle_model_filter();

	// ---- prefabs.cpp
	void prefab_frame();
	void append_prefab_lines(std::vector<line_vertex>& out);
	void draw_prefab_browser();
	void draw_prefab_menu_items();
	void draw_prefab_breadcrumb();
	void register_prefab_commands();
	bool enter_prefab();
	bool leave_prefab();
	bool stamp_prefab();
	bool convert_to_prefab(const std::string& name, bool overwrite);
	[[nodiscard]] int prefab_depth();
	[[nodiscard]] bool prefab_selected();

	// ---- menu_selection.cpp
	void focus_selection();		// camera and 2D views on the selection
	void go_to(const edit::vec3& position);
}

namespace editor::ui
{
	// grid_view.cpp: false when there is no view `index`
	bool grid_view_info(int index, ext::view2d_info& out);
	void grid_view_center(int index, double h, double v);
}
