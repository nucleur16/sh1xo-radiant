#pragma once

// The open map as the editor sees it.
//
// The document ( codol_edit ) is the single source of truth: entities, brushes, patches, selection, undo. The
// native renderer ( codol_view ) is a view of it. scene::sync() runs once per frame and brings the renderer in
// line with whatever the document holds now - it compares state ( revisions, visibility ), it does not listen to
// operations, so undo / redo / load need no code of their own.
//
// Nothing here keeps pointers into the document: ids and revisions only.

#include <codol_view/map_renderer.hpp>
#include <codol_edit/codol_edit.hpp>

namespace editor
{
	namespace view = codol::view;
	namespace edit = codol::edit;
	using float3 = codol::view::float3;
	using edit::object_id;

	struct line_vertex
	{
		float x, y, z;
		DWORD color;
	};

	// how the collision brushes that came with the zone are shown in the 3D view ( new and edited brushes are
	// always drawn, solid )
	enum class brush_display : int
	{
		off,
		selected,
		all,
	};

	// a brush or patch, ready to draw as lines in any view
	struct cached_object
	{
		object_id id = edit::no_object;
		object_id owner = edit::no_object;
		float mins[3] = {};
		float maxs[3] = {};
		std::uint32_t first = 0;	// object_cache::points, two per edge
		std::uint32_t count = 0;
		ImU32 color = 0;
		bool imported = false;		// an unedited collision brush of the zone
		bool compiled = false;		// belongs to an entity whose brush model the renderer draws
		bool patch = false;
	};

	struct cached_entity
	{
		object_id id = edit::no_object;
		float mins[3] = {};
		float maxs[3] = {};
		float origin[3] = {};
		float yaw = 0.0f;
		ImU32 color = 0;
		std::string classname;
		bool has_model = false;		// the renderer draws something for it
	};

	struct object_cache
	{
		std::vector<cached_object> objects;
		std::vector<float3> points;
		std::vector<cached_entity> entities;

		void clear()
		{
			objects.clear();
			points.clear();
			entities.clear();
		}
	};

	struct connection
	{
		float3 from, to;
	};

	inline DWORD to_d3d_color(const ImU32 c)
	{
		return (c & 0xFF00FF00u) | ((c & 0xFFu) << 16) | ((c >> 16) & 0xFFu);
	}

	struct scene
	{
		scene();

		view::map_renderer renderer;
		edit::document doc;

		std::string map_name;				// zone the renderer shows; name of the export
		std::filesystem::path map_file;		// the .map the document was loaded from / saved to, empty for a fresh import
		bool open = false;					// a document is open
		std::uint64_t open_serial = 0;		// counts maps opened / closed ( views frame the new map )
		double load_seconds = 0.0;

		view::camera cam;
		int spawn_index = 0;

		// ---- editing settings ( shared by all views ) ------------------------------------------------------
		double grid = 8.0;					// 1 .. 512, powers of two
		bool snap = true;
		float snap_angle = 15.0f;
		brush_display show_brushes = brush_display::off;
		bool show_entity_boxes = true;
		bool show_connections = true;
		bool show_labels = true;
		std::string model_class = "script_model";	// class of entities made from the Models browser

		// a world surface that was clicked: shown in the Selection window, not part of the document
		bool inspecting = false;
		view::pick_result inspected;

		// bounds of the last non empty selection ( the third axis of a brush drawn in a 2D view )
		edit::bounds last_selection_box;

		// ---- files -----------------------------------------------------------------------------------------
		// import <map>.ff of the install and show it
		bool open_zone(const std::string& map, std::string& error);
		// a .map; with its sidecars the import baseline comes back, and the zone named by "codol_map" is shown
		bool open_map_file(const std::filesystem::path& file, std::string& error);
		bool save(std::string& error);										// to map_file, or default_map_file()
		bool save_as(const std::filesystem::path& file, std::string& error);
		[[nodiscard]] std::filesystem::path default_map_file() const;		// <editor>\map_source\<map>.map
		void close();

		// <script root>\maps\mp\<map>.d3dbsp.ents. `summary` is a line for the user either way.
		bool export_entities(std::filesystem::path& written, std::string& summary);

		[[nodiscard]] std::string title() const;

		// ---- sync ------------------------------------------------------------------------------------------
		// once per frame, before anything is drawn
		void sync();

		// the document object a renderer object stands for ( an entity, or the brush of a mesh ); no_object for
		// the world and for anything the document does not know
		[[nodiscard]] object_id object_of(const view::object_ref& ref) const;
		// renderer objects that show an entity right now
		void refs_of(object_id entity, std::vector<view::object_ref>& out) const;
		[[nodiscard]] bool has_model(object_id entity) const;
		// a brush / patch exactly as the zone has it ( not new, not edited )
		[[nodiscard]] bool imported(object_id id) const;

		// lines of everything visible, split in "not selected" ( rebuilt when the document changes, but not while a
		// drag is in progress - a drag only changes the selection ) and "selected" ( rebuilt on every change )
		object_cache statics, selected;
		std::vector<connection> connections;		// target -> targetname
		std::uint64_t statics_serial = 0;			// change when the cache was rebuilt
		std::uint64_t selected_serial = 0;

		// ---- editing ( what takes more than one document call ) -------------------------------------------
		// Drags apply their whole movement again on every mouse move: restart_drag() takes the document back to
		// where the drag began, then the caller applies the total. One undo step, no accumulated error.
		void begin_drag(std::string_view name);
		void restart_drag();
		bool end_drag();
		void cancel_drag();
		[[nodiscard]] bool dragging() const { return drag_active_; }

		bool translate(const edit::vec3& delta);
		bool rotate(const edit::vec3& origin, const edit::vec3& axis, double degrees);
		bool scale(const edit::vec3& origin, const edit::vec3& factors);
		bool clone_selection();
		// move the faces of the selected brushes that look along world axis `axis` ( side +1 / -1 ) by `distance`
		// along that axis
		bool resize_selection(int axis, int side, double distance);
		object_id create_model_entity(std::string_view model, const edit::vec3& origin, double yaw);
		object_id create_entity(std::string_view classname, const edit::vec3& origin);

		[[nodiscard]] edit::vec3 selection_pivot() const;
		[[nodiscard]] double snapped(double v) const { return snap ? edit::snap_value(v, grid) : v; }
		void set_grid(double g);

		// first position a ray hits: the map, else a point `fallback` units along it
		[[nodiscard]] edit::vec3 ray_target(const float3& origin, const float3& direction, float fallback = 300.0f) const;
		void look_at_selection();

		[[nodiscard]] static double model_scale(const edit::entity& e);
		[[nodiscard]] ImU32 class_color(std::string_view classname) const;

	private:
		struct entity_link
		{
			// objects of the shipped map that show this entity, with their placement in the zone
			std::vector<view::object_ref> refs;
			std::vector<view::transform> base;
			bool brush_model = false;
			bool is_static = false;
			std::string base_model;
			edit::vec3 base_origin, base_angles;
			double base_scale = 1.0;
			edit::bounds local_box;			// of the map object, relative to base_origin
			bool has_box = false;

			// a model the zone has no object for ( new entity, changed "model" )
			int placed = 0;
			std::string placed_model;
			std::string failed_model;

			// what was pushed last
			std::uint64_t revision = 0;
			bool visible = false;
			bool pushed = false;
			bool moved = false;
			bool shows_map_objects = false;
			std::uint64_t stamp = 0;
		};

		struct brush_mesh
		{
			std::uint64_t revision = 0;
			std::vector<int> handles;		// one per material
			std::uint64_t stamp = 0;
		};

		bool load_renderer(const std::string& map, std::string& error);
		void clear_view_state();
		void place_brush_models(const edit::document* baseline);
		void build_links(const edit::document& baseline, bool fresh_import);
		void sync_renderer();
		void push_entity(const edit::entity& e, entity_link& link, bool visible);
		void drop_placed(entity_link& link);
		void rebuild_mesh(object_id id, brush_mesh& m);
		void destroy_mesh(brush_mesh& m);
		void rebuild_cache(object_cache& cache, bool want_selected);
		void rebuild_connections();
		void ensure_origin_keys();
		bool model_box(std::string_view model, float3& mins, float3& maxs);

		std::unordered_map<object_id, entity_link> links_;
		std::unordered_map<std::uint64_t, object_id> ref_owner_;			// renderer object -> document object
		std::unordered_map<object_id, std::uint64_t> import_revision_;		// brushes / patches as they came from the zone
		std::unordered_map<object_id, brush_mesh> meshes_;
		std::unordered_map<std::string, std::pair<int, int>> texture_sizes_;
		std::unordered_map<std::string, std::pair<float3, float3>> model_boxes_;
		std::unordered_set<std::string> no_model_box_;
		std::map<std::string, int> missing_models_;		// models of entities that could not be loaded, reported once per sync

		std::uint64_t stamp_ = 0;
		std::uint64_t seen_change_ = ~0ull;
		std::uint64_t seen_visibility_ = ~0ull;
		std::uint64_t seen_selection_ = ~0ull;
		std::uint64_t selection_hash_ = 0;
		std::uint64_t statics_change_ = ~0ull;
		bool caches_dirty_ = true;

		bool drag_active_ = false;
		std::string drag_name_;
	};

	scene& get_scene();
}
