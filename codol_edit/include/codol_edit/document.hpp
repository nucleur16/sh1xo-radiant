#pragma once

// codol_edit: a map document - model, .map reader / writer, queries, selection, editing operations, undo.
//
//   codol::edit::document doc;
//   std::string error;
//   doc.load_map_file( "mp_rust.map", error );            // or doc.import_fastfile( "mp_rust.ff", "C1.exe", error )
//   for ( auto id : doc.entities() ) { const auto* e = doc.find_entity( id ); for ( auto child : e->children ) .. }
//   pick_result hit; if ( doc.pick( ray, hit ) ) doc.select( hit.object );
//   doc.translate( { 16, 0, 0 } );                          // one undo step
//   doc.undo();
//   doc.save_map_file( "mp_rust.map", error );
//
// Threading: a document is not thread safe; use it from one thread. It holds no global state.
//
// Undo: every editing operation is one named step on the undo stack. To make several calls one step ( a mouse
// drag that calls translate() on every mouse move, a tool that deletes and creates ) wrap them:
//   doc.begin_transaction( "Move" ); .. any number of operations .. doc.end_transaction();
// or give consecutive calls the same non zero merge key: begin_transaction( "Nudge", key ) merges into the step
// on top of the stack when that step has the same key.
// The selection is part of an undo step: undo restores the selection as it was before the step, redo the one
// after it. Changing the selection alone is not a step. Hidden objects / hidden layers are view state: not undone.

#include "codol_edit/model.hpp"
#include "codol_edit/geometry.hpp"
#include "codol_edit/select_types.hpp"

#include <filesystem>
#include <functional>
#include <memory>
#include <span>

namespace codol::edit
{
	struct pick_result
	{
		object_id object = no_object;	// brush, patch or point entity
		object_kind kind = object_kind::none;
		object_id entity = no_object;	// the entity itself, or the owner of the brush / patch
		int face = -1;					// brush face that was hit
		double distance = 0.0;			// in units of |ray.direction|
		vec3 point;
	};

	struct pick_options
	{
		bool brushes = true;
		bool patches = true;
		bool entities = true;			// point entities ( boxes )
		bool include_hidden = false;	// also hit hidden objects / objects on hidden layers
		double max_distance = 1e30;
	};

	enum class clip_keep
	{
		front,	// the part in front of the plane ( where the normal points )
		back,
		both,	// split: both parts stay
	};

	enum class select_mode
	{
		replace,
		add,
		remove,
		toggle,
	};

	// result of export_entity_override
	struct override_export
	{
		std::string text;			// contents of <map>.d3dbsp.ents
		std::string map_name;
		std::uint32_t base_crc = 0;
		int exported = 0;			// entities written
		int static_models = 0;		// misc_models of the zone ( codol_static ), which are not part of the entity string
		std::vector<std::string> warnings;
	};

	class document
	{
	public:
		document();		// an empty map: "iwmap 4" and a worldspawn
		~document();
		document(document&&) noexcept;
		document& operator=(document&&) noexcept;
		document(const document&) = delete;
		document& operator=(const document&) = delete;

		// ---- files ----------------------------------------------------------------------------

		// Replaces the document ( undo stack and selection are cleared ). On failure the document is unchanged.
		bool load_map_text(std::string text, std::string& error);
		bool load_map_file(const std::filesystem::path& file, std::string& error);
		void new_map();

		// An unedited document is written back byte for byte; objects that were not edited are written exactly
		// as they were read. Edited and new objects are written the way CoD4Radiant writes them.
		[[nodiscard]] std::string save_map_text() const;
		bool save_map_file(const std::filesystem::path& file, std::string& error) const;
		// every object written new, as if all of them had been edited ( normalises a hand written file; also the
		// reference for how close the writer is to radiant's own output )
		[[nodiscard]] std::string save_map_text_canonical() const;

		// true after any edit since load / new / mark_saved
		[[nodiscard]] bool modified() const;
		void mark_saved();

		// line end of the file ( "\n" or "\r\n" ), used for everything that is written new
		[[nodiscard]] std::string_view line_end() const;
		// warnings of the last load ( blocks that could not be parsed and are carried verbatim .. )
		[[nodiscard]] const std::vector<std::string>& load_warnings() const;

		// ---- fastfile import / entity override export -------------------------------------------

		// Reads a shipped map straight from its fastfile: the entities, the collision brushes of the world and of
		// every brush model ( "model" "*N" ), and the static models as misc_model with "codol_static" "1". The same
		// result as codol_mapconv --ff. `map_name`: name of the map ( default: file name without extension ).
		// The import is remembered as the baseline for export_entity_override.
		bool import_fastfile(const std::filesystem::path& fastfile, const std::filesystem::path& client_exe,
			std::string& error, std::string_view map_name = {});

		// <map>.d3dbsp.ents for the dedicated server: the entities of the document, with every key that was not
		// edited taken from the original entity string. For an unedited import, the text after the three comment
		// lines is the original entity string ( + one line end ).
		// Needs the baseline: present after import_fastfile, or set with set_import_baseline after loading a .map
		// that was saved from an import. Without one, all entities are exported as they are in the document.
		bool export_entity_override(override_export& out, std::string& error) const;

		// baseline of an import, to store next to a saved map and give back after loading it again
		[[nodiscard]] bool has_import_baseline() const;
		[[nodiscard]] std::string_view import_baseline_map() const;		// the .map text right after the import
		[[nodiscard]] std::string_view import_original_ents() const;		// the entity string of the zone
		void set_import_baseline(std::string baseline_map, std::string original_ents);

		// ---- reading the model ------------------------------------------------------------------

		[[nodiscard]] std::span<const object_id> entities() const;		// file order, worldspawn first
		[[nodiscard]] object_id worldspawn() const;
		[[nodiscard]] object_kind kind(object_id id) const;				// none: no such object ( deleted .. )
		[[nodiscard]] const entity* find_entity(object_id id) const;
		[[nodiscard]] const brush* find_brush(object_id id) const;
		[[nodiscard]] const patch* find_patch(object_id id) const;
		// entity that owns a brush / patch; the id itself for an entity
		[[nodiscard]] object_id owner_of(object_id id) const;
		// revision of any object, 0 when it does not exist
		[[nodiscard]] std::uint64_t revision(object_id id) const;
		// changes whenever anything in the document changes ( edits, undo, redo, load ); not for selection / view state
		[[nodiscard]] std::uint64_t change_counter() const;

		[[nodiscard]] std::size_t brush_count() const;
		[[nodiscard]] std::size_t patch_count() const;

		// ---- geometry ---------------------------------------------------------------------------

		// texture sizes for uv computation and texture lock. Not set: every texture is 256 x 256.
		void set_texture_size_callback(texture_size_fn fn);
		// box of a point entity relative to its origin ( model bounds .. ). Return false to fall back to the
		// class catalog. Not set: class catalog only.
		void set_entity_box_callback(std::function<bool(const entity&, bounds& local_box)> fn);

		// windings / corners / validity of a brush, cached per revision. nullptr when `id` is not a brush.
		[[nodiscard]] const brush_geometry* geometry(object_id brush) const;
		// world bounds of a brush, patch, point entity ( its box ) or brush entity ( all children )
		[[nodiscard]] bounds object_bounds(object_id id) const;
		// triangle mesh of a brush or patch ( `out` is cleared first ). out.revision = revision of the object.
		bool build_mesh(object_id id, mesh& out, int patch_subdivisions = 4) const;

		// ---- visibility -------------------------------------------------------------------------

		void set_hidden(object_id id, bool hidden);			// entity: also hides its children
		[[nodiscard]] bool is_hidden(object_id id) const;	// hidden itself or through its entity
		void hide_selected();								// and deselects
		void hide_unselected();
		void show_all();
		// not hidden and not on a hidden layer
		[[nodiscard]] bool is_visible(object_id id) const;
		// changes whenever hidden objects / layers change
		[[nodiscard]] std::uint64_t visibility_counter() const;

		// ---- layers -----------------------------------------------------------------------------

		[[nodiscard]] std::span<const layer> layers() const;
		[[nodiscard]] std::string_view active_layer() const;	// "" when the file has none
		// layer an object is on: its own, else the one of its entity, else ""
		[[nodiscard]] std::string_view layer_of(object_id id) const;
		void set_layer_hidden(std::string_view name, bool hidden, bool with_children = true);	// view state
		bool add_layer(std::string_view name);
		// objects of the layer ( and of its children, which are removed too ) move to the parent layer
		bool remove_layer(std::string_view name);
		bool rename_layer(std::string_view name, std::string_view new_name);
		bool set_active_layer(std::string_view name);
		bool move_selection_to_layer(std::string_view name);

		// ---- selection --------------------------------------------------------------------------

		// Selected objects are brushes, patches and point entities. Selecting an entity that has brushes selects
		// its brushes. Faces are selected separately ( texture tools ).
		[[nodiscard]] std::span<const object_id> selection() const;
		[[nodiscard]] std::span<const face_ref> selected_faces() const;
		[[nodiscard]] bool is_selected(object_id id) const;
		[[nodiscard]] bool is_face_selected(face_ref f) const;
		void select(object_id id, select_mode mode = select_mode::replace);
		void select(std::span<const object_id> ids, select_mode mode = select_mode::replace);
		void select_face(face_ref f, select_mode mode = select_mode::replace);
		void select_all();				// everything visible
		void deselect_all();			// objects and faces
		void invert_selection();
		// all brushes of the entities the selected brushes belong to ( not for the worldspawn )
		void select_whole_entities();
		// entities touched by the selection: selected point entities and owners of selected brushes ( no worldspawn )
		[[nodiscard]] std::vector<object_id> selected_entities() const;
		[[nodiscard]] bounds selection_bounds() const;
		// changes whenever the selection changes
		[[nodiscard]] std::uint64_t selection_counter() const;

		// ---- picking / queries ------------------------------------------------------------------

		// closest hit
		bool pick(const ray& r, pick_result& out, const pick_options& options = {}) const;
		// every hit, closest first ( to cycle through objects behind each other )
		[[nodiscard]] std::vector<pick_result> pick_all(const ray& r, const pick_options& options = {}) const;

		// Objects inside a convex volume given by planes that point out of it ( a view frustum: 4 - 6 planes ).
		// fully_inside: the whole object has to be inside; otherwise touching is enough.
		[[nodiscard]] std::vector<object_id> query_volume(std::span<const plane> planes, bool fully_inside,
			const pick_options& options = {}) const;
		// Rectangle in a 2d view that looks along `axis` ( 0 x, 1 y, 2 z ). The rectangle is given in the two other
		// world axes in ascending order ( axis 2: x y, axis 1: x z, axis 0: y z ).
		[[nodiscard]] std::vector<object_id> query_rect(int axis, vec2 lo, vec2 hi, bool fully_inside,
			const pick_options& options = {}) const;

		// ---- selection menu / clipboard / filters / regions / prefabs ( src/select_ops.cpp, src/prefab.cpp ) ----
#include "codol_edit/document_select.inc"

		// ---- undo -------------------------------------------------------------------------------

		void begin_transaction(std::string_view name, std::uint64_t merge_key = 0);
		// closes the transaction; true when it changed something ( and therefore is an undo step ). Nested
		// begin / end pairs are counted, the outermost one decides.
		bool end_transaction();
		// undoes everything since the outermost begin_transaction and closes it
		void cancel_transaction();
		[[nodiscard]] bool in_transaction() const;

		[[nodiscard]] bool can_undo() const;
		[[nodiscard]] bool can_redo() const;
		bool undo();
		bool redo();
		[[nodiscard]] std::string_view undo_name() const;	// name of the step undo() would take back
		[[nodiscard]] std::string_view redo_name() const;
		[[nodiscard]] std::size_t undo_count() const;
		[[nodiscard]] std::size_t redo_count() const;
		void clear_undo();
		void set_undo_limit(std::size_t steps);				// default 256, 0: unlimited

		// message of the last operation that returned false / no_object
		[[nodiscard]] const std::string& last_error() const;

		// ---- editing settings -------------------------------------------------------------------

		// keep textures in place on the surface when brushes move / rotate / scale ( default on )
		void set_texture_lock(bool on);
		[[nodiscard]] bool texture_lock() const;
		// the lock of moves and the lock of everything else ( rotations, scales, flips ) apart; set_texture_lock( on )
		// sets both, texture_lock() is true while either is on
		void set_texture_lock(bool moves, bool rotations);
		[[nodiscard]] bool texture_lock_moves() const;
		[[nodiscard]] bool texture_lock_rotations() const;
		// material of new brushes when none is given ( default "caulk" )
		void set_default_material(std::string_view material);
		[[nodiscard]] std::string_view default_material() const;

		// ---- creating ---------------------------------------------------------------------------
		// New brushes go to the worldspawn, on the active layer, and become the selection. `material` empty: the
		// default material. no_object + last_error() when the shape is degenerate.

		object_id create_box(const vec3& mins, const vec3& maxs, std::string_view material = {});
		// prism with `sides` ( 3 .. 64 ) sides whose axis is world axis `axis`, filling the box
		object_id create_cylinder(const vec3& mins, const vec3& maxs, int sides, int axis = 2, std::string_view material = {});
		// like the cylinder, tapering to a point at the maxs end of the axis
		object_id create_cone(const vec3& mins, const vec3& maxs, int sides, int axis = 2, std::string_view material = {});
		// box cut along the diagonal: the full height at mins of world axis `slope_axis` ( 0 or 1 ), zero at maxs
		object_id create_wedge(const vec3& mins, const vec3& maxs, int slope_axis = 0, std::string_view material = {});
		// brush from the convex hull of points ( >= 4, not coplanar )
		object_id create_brush_from_points(std::span<const vec3> points, std::string_view material = {});
		// flat terrain mesh in the plane perpendicular to `axis`, `columns` x `rows` vertices ( 2 .. 16 )
		object_id create_terrain_patch(const vec3& mins, const vec3& maxs, int columns, int rows, int axis = 2, std::string_view material = {});

		// ---- selection operations ---------------------------------------------------------------
		// All of them work on the selection and return false ( last_error() ) when there was nothing to do.

		bool delete_selection();
		// copies become the selection. Brushes of a brush entity that is selected completely are cloned as a new
		// entity; single brushes are cloned into the entity they are in.
		bool clone_selection(const vec3& offset = {});

		bool translate(const vec3& delta);
		// right handed rotation about the line through `origin` along `axis`. Entities get their "angles" updated.
		bool rotate(const vec3& origin, const vec3& axis, double degrees);
		// per axis scale about `origin` ( a negative factor mirrors )
		bool scale(const vec3& origin, const vec3& factors);
		bool flip(const vec3& origin, int axis);
		// any affine transform
		bool transform_selection(const transform& t, std::string_view undo_name = "Transform");

		// snap the plane points of brushes, patch vertices and entity origins to multiples of `grid`
		bool snap_to_grid(double grid);

		// keep the part of each selected brush on one side of the plane ( or split ). Brushes entirely on the
		// removed side are deleted. The new face gets `material`, or the alignment of the brush's first face.
		bool clip(const plane& p, clip_keep keep, std::string_view material = {});
		bool clip(const vec3& a, const vec3& b, const vec3& c, clip_keep keep, std::string_view material = {});

		// cut the selected brushes out of every other visible brush they intersect ( the selection stays )
		bool csg_subtract();
		// replace each selected brush by one wall brush per face, `thickness` units thick, inside the old volume
		bool csg_hollow(double thickness);

		// "contents detail;" on / off for the selected brushes and patches
		bool set_detail(bool detail);
		bool set_contents(std::span<const std::string> words);

		// ---- brush shape ------------------------------------------------------------------------

		// move one face along its normal ( positive: outwards ). Fails when the brush would vanish.
		bool move_face(face_ref f, double distance);
		// move the faces of the selected brushes whose normal points along `direction` ( within ~45 degrees ) by
		// `direction` - the "drag outside the brush" resize of radiant
		bool resize_selection(const vec3& direction);

		// Move corners of a brush ( an edge is two corners ). `corners` are positions of brush_geometry::vertices
		// ( matched within 0.05 ); the brush is rebuilt as the convex hull of the moved corner set. Faces whose
		// plane does not change keep their plane points and alignment exactly. Fails when the result is flat.
		bool move_brush_corners(object_id brush, std::span<const vec3> corners, const vec3& delta);
		bool move_patch_vertices(object_id patch, std::span<const int> vertex_indices, const vec3& delta);

		// ---- brush shape, several at once ( edit_ops_3d.cpp; the handles of a perspective view ) -----

		// Each face moves along its normal by the part of `delta` that points that way ( one step ). Faces that
		// would make their brush vanish stay; false when none moved.
		bool move_faces(std::span<const face_ref> faces, const vec3& delta);
		// move_brush_corners for every selected brush that has corners at `corners` ( one step ): a corner shared
		// by brushes that touch moves in all of them. Brushes that would be flat stay; false when none moved.
		bool move_corners(std::span<const vec3> corners, const vec3& delta);

		// ---- textures ---------------------------------------------------------------------------
		// Target: the selected faces, plus every face of the selected brushes and the selected patches.

		bool set_material(std::string_view material);
		bool shift_texture(double dx, double dy);			// texels
		bool scale_texture(double fx, double fy);			// multiplies size_x / size_y
		bool rotate_texture(double degrees);
		// absolute alignment; the material of `t` is ignored when empty
		bool set_texture(const tex_def& t);
		// stretch the texture so that it repeats `repeat_x` x `repeat_y` times over each face
		bool fit_texture(double repeat_x = 1.0, double repeat_y = 1.0);

		// ---- texture tools ( src/texture_ops.cpp ) ------------------------------------------------
		// A face line of a .map has two layers: the material with its alignment and the lightmap with its own
		// ( "lightmap_gray 16384 16384 0 0 0 0" ); a patch has a material, a lightmap material and two sets of
		// texture coordinates per vertex. shift / scale / rotate / set / fit / flip / natural change the layer that
		// is being edited ( default: the material ). Patches are targets as well: their texture coordinates move.
		enum class texture_layer : int { material = 0, lightmap = 1 };
		void set_edit_layer(texture_layer layer);
		[[nodiscard]] texture_layer edit_layer() const;
		// mirror the texture on the surface: axis 0 along its s direction ( left / right ), 1 along t
		bool flip_texture(int axis);
		// the alignment a new face gets: one repeat per 256 texels at scale 0.25, no shift, no rotation - projected
		// along the world axis closest to the face normal ( the only projection the format has ). Patches get
		// coordinates from their vertex positions the same way.
		bool natural_texture();
		// the lightmap layer of the targets: its material ( empty: keep ) and, for faces, its alignment
		bool set_lightmap(const tex_def& t);
		// the smoothing of the selected patches ( the "smoothing <word>" line; empty: none )
		bool set_smoothing(std::string_view word);
		// Material `from` becomes `to` on brush faces and patches. selection_only: of the selected objects and faces;
		// faces_only: of the selected faces alone ( not the brushes ). Returns the number of faces and patches that
		// changed through `changed`; false ( last_error ) when none did.
		bool replace_material(std::string_view from, std::string_view to, bool selection_only, bool faces_only = false, int* changed = nullptr);
		// every face / patch that uses the material becomes the selection ( faces as selected faces )
		int select_by_material(std::string_view material);
		// materials in use with the number of faces and patches, sorted by name
		[[nodiscard]] std::vector<std::pair<std::string, int>> materials_in_use() const;

		// ---- tools ( src/tool_ops.cpp ) -----------------------------------------------------------
		struct rope_params
		{
			vec3 from, to;
			double slack = 32.0;		// how far the middle hangs below the straight line
			double radius = 2.0;
			int segments = 12;			// along the rope, 1 .. 256 ( 15 per mesh: a long rope is several meshes )
			int sides = 6;				// around it, 3 .. 15
			std::string material;		// empty: the default material
		};
		// A cable between two points as terrain meshes ( tubes ). The new meshes become the selection; their ids go
		// to `out` when given. One undo step.
		bool create_rope(const rope_params& p, std::vector<object_id>* out = nullptr);

		struct noise_params
		{
			double amplitude = 16.0;	// units, peak
			double frequency = 0.01;	// cycles per unit
			std::uint32_t seed = 1;
			int octaves = 3;			// 1 .. 8, each half as strong and twice as fine
			vec3 direction{ 0, 0, 1 };	// the way vertices move
			bool keep_edges = true;		// the border vertices stay ( the mesh keeps fitting to what is next to it )
		};
		// moves the vertices of the selected terrain meshes by a noise of their position ( the same noise for all,
		// so that meshes that touch keep touching )
		bool patch_noise(const noise_params& p);

		// ---- entities ---------------------------------------------------------------------------

		// appended to the map, selected. Extra pairs are added after classname and origin.
		object_id create_point_entity(std::string_view classname, const vec3& origin,
			std::span<const std::pair<std::string, std::string>> pairs = {});
		// new entity that takes the selected brushes / patches out of the entities they are in
		object_id create_brush_entity(std::string_view classname,
			std::span<const std::pair<std::string, std::string>> pairs = {});
		// selected brushes go back to the worldspawn; brush entities left without brushes are deleted
		bool move_selection_to_worldspawn();

		bool set_key(object_id entity, std::string_view key, std::string_view value);	// adds or replaces
		bool remove_key(object_id entity, std::string_view key);
		bool rename_key(object_id entity, std::string_view key, std::string_view new_key);
		// "target" of `from` = "targetname" of `to`. An existing targetname of `to` is used, else an existing
		// target of `from`, else a new unique "auto<N>".
		bool connect_entities(object_id from, object_id to);
		// entities whose targetname equals the "target" of `entity`, and the ones that target it
		[[nodiscard]] std::vector<object_id> targets_of(object_id entity) const;
		[[nodiscard]] std::vector<object_id> targeting(object_id entity) const;

		struct impl;	// private state ( src/doc_impl.hpp )

	private:
		friend struct ops_access;	// patch_ops.hpp: brush primitives and patch operations ( src/patch_ops.cpp )
		std::unique_ptr<impl> d;
	};
}
