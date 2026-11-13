#pragma once

// What the top bar of CoD4 Radiant has beyond File / Edit / View, natively: the action registry with its
// rebindable keys ( hotkeys.cpp ), the preferences, colours, autosave and the list of recent maps
// ( preferences.cpp ), the global toolbar ( toolbar.cpp ), the Textures menu with find / replace and the
// material browser ( menu_textures.cpp ), the Renderer menu with the render modes and the post process
// ( menu_renderer.cpp ), the effects in the map view ( effects.cpp ) and the tools ( tools.cpp ).
//
// Every option is an action ( a menu entry, a key, a toolbar button, the console command "act <id>" ) and, where
// it is a setting, a value of codol_editor.cfg.

#include "scene.hpp"

struct ImDrawList;

namespace editor::actions
{
	// where the mouse has to be for the key of an action to count ( menu entries and buttons always work )
	enum scope : int
	{
		anywhere = 0,		// the whole window ( keys with Ctrl, function keys )
		views = 1,			// the 3D view or a 2D view: plain keys must not act while a list has the mouse
		view_3d = 2,
		view_2d = 3,
	};

	enum flags : int
	{
		none = 0,
		no_map = 1,			// works without an open map
		repeat = 2,			// the key repeats while it is held
		in_drag = 4,		// also while the right button flies the camera or a drag is in progress
	};

	struct chord
	{
		ImGuiKey key = ImGuiKey_None;
		bool ctrl = false, shift = false, alt = false;

		bool operator==(const chord&) const = default;
		[[nodiscard]] bool empty() const { return key == ImGuiKey_None; }
	};

	// "Ctrl+Shift+Z"; "" for no key. Names as the Edit Hotkeys window shows them.
	std::string chord_text(const chord& c);
	bool parse_chord(std::string_view text, chord& out);

	struct action
	{
		std::string id;				// "edit.clone": never changes, the hotkeys file and the toolbar name actions by it
		std::string label;
		std::string category;		// the part of the id before the first '.', as a title
		chord defaults[2];			// "Backspace|Delete": two keys for one action
		chord keys[2];
		std::function<void()> run;
		std::function<bool()> enabled;		// empty: always ( with a map, unless no_map )
		std::function<bool()> checked;		// empty: not a toggle
		int where = anywhere;
		int options = none;
		std::string tip;

		action& when(std::function<bool()> fn) { enabled = std::move(fn); return *this; }
		action& toggle(std::function<bool()> fn) { checked = std::move(fn); return *this; }
		action& help(std::string text) { tip = std::move(text); return *this; }
	};

	// THE ONE LINE: actions::add( "edit.clone", "Clone", "Space", [] { clone_selection(); }, actions::views );
	// `keys`: "" for none, "Ctrl+S", or two alternatives "Backspace|Delete". Registering an id again replaces the
	// action and keeps the key the user gave it.
	action& add(std::string id, std::string label, std::string_view keys, std::function<void()> run, int where = anywhere, int options = none);

	action* find(std::string_view id);
	const std::vector<std::unique_ptr<action>>& all();
	[[nodiscard]] bool usable(const action& a);		// enabled right now
	bool run(std::string_view id);						// false: no such action, or it is disabled
	// the key of an action for a menu entry ( "" when it has none or does not exist )
	const char* shortcut(std::string_view id);
	// ImGui::MenuItem with the label, key, state and check mark of the action; a missing action is a disabled entry
	bool menu_item(std::string_view id, const char* label = nullptr);

	// once per frame, where process_shortcuts() used to read the keys
	void process_keys();
	void set_key(action& a, int slot, const chord& c);
	void reset_keys();
	void load_keys();		// codol_editor_hotkeys.cfg next to the settings
	void save_keys();

	// the editor's own actions ( everything that had a hard coded key ) and the console commands act / bind / unbind
	void register_defaults();

	void draw_hotkeys_window();
	void draw_help_window();
	void open_hotkeys_window();
	void open_help_window();
}

// The hand written menus go through these instead of ImGui::BeginMenu / MenuItem: a key shown in a menu is
// always the action's key of the moment ( menu::key( "edit.clone" ) ), and actions_check can walk every menu
// ( audit: all menus are run once into a window off the screen ) and report entries whose key text is not the
// registry's.
namespace editor::menu
{
	bool begin(const char* label, bool enabled = true);
	void end();
	bool item(const char* label, const char* shortcut = nullptr, bool selected = false, bool enabled = true);
	bool item(const char* label, const char* shortcut, bool* p_selected, bool enabled = true);
	// the key of an action as the next item shows it; the item is then known to belong to the action
	const char* key(std::string_view action_id);

	[[nodiscard]] bool auditing();
	// the menu bar asks for this once per frame: true when actions_check wants every menu walked now
	[[nodiscard]] bool audit_requested();
	void audit_begin();
	void audit_end();		// prints the report of actions_check
	void register_commands();
}

namespace editor
{
	// the key of an action for a text ( a tooltip, the hint lines ): "-" when it has none
	inline const char* key_or_dash(const std::string_view id)
	{
		const char* k = actions::shortcut(id);
		return k ? k : "-";
	}
}

namespace editor::prefs
{
	// typed settings on top of the saved dvars
	bool get_bool(const char* name);
	int get_int(const char* name);
	float get_float(const char* name);
	std::string get_string(const char* name);
	void set_bool(const char* name, bool value);
	void set_int(const char* name, int value);
	void set_float(const char* name, float value);
	void set_string(const char* name, const std::string& value);

	void init();			// registers the settings; before dvars::load_config()
	void frame();			// keeps the settings and what they stand for in line; autosave; recent maps
	void open_window();
	void draw_window(bool* open);		// File > Settings, Edit > Preferences
	void draw_recent_menu();			// File > Open Recent
	bool autosave_now(std::string* written = nullptr);

	// the camera of the 3D view: its values live in the view, the settings follow ( and lead after a change here )
	void sync_camera(float& fov, float& fly_speed, float& wire_distance);
	[[nodiscard]] float look_sign_y();		// -1 with "invert mouse Y"
}

namespace editor::colors
{
	enum id : int
	{
		grid_background,
		grid_minor,
		grid_major,
		grid_axis,
		grid_text,
		grid_block,
		selection,			// selected brushes, both views
		selection_model,	// outline of a selected model
		selection_face,
		brush,				// a new / edited brush in the 2D views
		brush_zone,			// a collision brush of the zone
		brush_detail,
		brush_entity,
		patch,
		entity,				// classes without a colour of their own
		connection,
		camera_icon,
		clipper,
		view_background,	// clear colour of the 3D view
		count,
	};

	ImU32 get(id which);
	DWORD d3d(id which);
	void draw_window();
	void open_window();
	// a number that changes when a colour changes: caches of coloured lines are made again
	std::uint64_t serial();
}

namespace editor::toolbar
{
	void draw();				// right under the menu bar, before the dock space
	void draw_edit_window();
	void open_edit_window();
	// the picture of an action, drawn into a square; false: the action has none ( its letters are shown )
	bool draw_icon(ImDrawList* dl, std::string_view action_id, const ImVec2& at, float size, ImU32 color);
}

namespace editor::textures
{
	void register_actions();
	void draw_menu();
	void draw_windows();											// find / replace
	void draw_surface_extras();										// the lower part of the Surface window
	void draw_materials_window(std::string& filter, int& group);	// the Materials browser
	void note_material_used(const std::string& name);
}

namespace editor::render_modes
{
	enum class shading : int { game, fullbright, fake_normal, fake_view, depth };
	enum class patches : int { textured, wireframe, textured_wire };
	// CoD4 radiant's Show Entities As: skinned, bounding box, skinned and boxed, wireframe, selected wireframe, selected skinned
	enum class entities : int { models, boxes, models_boxes, wireframe, selected_wireframe, selected_models, count };

	struct state
	{
		bool outline_selected = true;
		bool tint_selected = false;
		bool show_angles = false;
		bool model_origins = false;
		bool grid_names = true;
		bool grid_coordinates = true;
		bool grid_crosshair = false;
		shading mode = shading::game;
		bool wireframe = false;
		bool draw_water = true;
		int texture_filter = 0;		// 0 as the material says, 1 point, 2 bilinear, 3 trilinear, 4 anisotropic
		int texture_resolution = 0;	// 0 full, 1 .. 3: mip levels dropped
		bool specular = true;
		bool normal_maps = true;
		patches show_patches = patches::textured;
		entities show_entities = entities::models;
		bool cubic_clip = false;
		float cubic_distance = 4096.0f;
		bool sun_shadows = true;
		bool light_preview = true;
		bool light_max_intensity = false;
		bool filmtweaks = false;
		std::string vision;			// name of the vision file whose film values are shown
	};

	state& get();
	void register_actions();
	void draw_menu();
	void draw_windows();											// vision picker, film values
	// what the 3D view asks before it draws: the options of the frame that are render modes
	void apply(view::frame_options& frame);
	// the frame as the view shows it: the post process ( fake light, light preview, film ) over the renderer's picture
	IDirect3DTexture9* post_process(IDirect3DTexture9* frame, int width, int height);
	void draw_overlay(const view::overlay_context& ctx);			// lines: clip cube, patch wires, angles, origins
	void device_lost();
	[[nodiscard]] bool entity_boxes_always();
	[[nodiscard]] bool hide_patch_meshes();
	// the 2D views: blocks, coordinates, the cross under the mouse, the camera
	struct grid_context
	{
		ImDrawList* dl = nullptr;
		ImVec2 pos, size;			// the canvas
		int axis = 2;				// the axis the view looks along
		double center[2] = {};		// world position in the middle of the canvas
		double scale = 1.0;			// pixels per unit
		bool hovered = false;
	};
	void draw_grid_extras(const grid_context& ctx);
}

namespace editor::fx
{
	void register_actions();
	void draw_menu();
	void draw_windows();											// Effects Browser, Effect Settings
	// the effects of the map ( entities with an "fx" key, createfx positions ) as the Preview window draws one
	void draw_in_view(ImDrawList* dl, const ImVec2& origin, int width, int height);
}

namespace editor::map_tools
{
	void register_actions();
	void draw_menu();
	void draw_windows();											// rope, noise, mesh painter
	// before the 3D view takes the mouse: the painter's strokes ( it puts its own button over the view while it paints )
	void viewport_input(const ImVec2& origin, int width, int height);
}

namespace editor::ext
{
	void init();					// once, before the settings are read: registers settings, actions, commands
	void frame();					// once per frame, after the views
	void draw_menus();				// Textures, Renderer, Effects, Tools - between View and Game
	void draw_edit_menu_items();	// Edit Colors / Edit Toolbar / Edit Hotkeys / Preferences
	void draw_help_menu_items();
}
