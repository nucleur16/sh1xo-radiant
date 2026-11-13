		// Member declarations of codol::edit::document, included inside the class ( document.hpp ). Types:
		// select_types.hpp. Implementation: src/select_ops.cpp, src/prefab.cpp.

		// ---- select -----------------------------------------------------------------------------
		// entities whose `key` matches `pattern` ( '*' = any text; "" = the key exists ). Returns how many matched.
		std::size_t select_by_key(std::string_view key, std::string_view pattern, select_mode mode = select_mode::replace);
		// adds every entity that is linked to a selected one through "target" / "targetname", over any number of
		// steps and in both directions. Returns how many were added.
		std::size_t select_connected();
		// Radiant's Select Touching / Inside / Complete Tall / Partial Tall: the selected brushes are the volume.
		// `view_axis` is the axis that does not count for the two "tall" kinds ( 2: the top view ).
		bool select_by_volume(volume_select how, int view_axis = 2);
		// with brushes of one brush entity selected: the next ( step 1 ) / previous ( -1 ) brush of that entity alone
		bool select_next_in_group(int step = 1);

		// ---- clipboard / files ------------------------------------------------------------------
		// The selection as a complete .map: brushes of the worldspawn in a worldspawn, selected brushes of a brush
		// entity under a copy of that entity, point entities. `world_keys`: the worldspawn keeps its keys and the
		// file its layer table ( File > Save Selected ); without, a bare worldspawn ( the clipboard ). "" when
		// nothing is selected.
		[[nodiscard]] std::string selection_map_text(bool world_keys = false) const;
		// Everything that touches the region ( or everything visible when there is none ), with the worldspawn's
		// keys. `walls`: six brushes of `wall_material` around it, so that the result is a closed map ( a region
		// build ).
		[[nodiscard]] std::string region_map_text(bool walls = false, std::string_view wall_material = "caulk") const;
		// Adds the contents of a .map text: brushes of its worldspawn go into this worldspawn, its other entities
		// are appended. The new objects become the selection. `move_to`: the centre of what was added is moved
		// there ( snapped to `grid` ); nullptr: where it was. One undo step.
		bool paste_map_text(std::string_view text, const vec3* move_to = nullptr, double grid = 0.0, std::string_view undo_name = "Paste");

		// ---- brushes ----------------------------------------------------------------------------
		// one word of the "contents ..;" line ( detail, nonColliding, weaponClip ) on / off for the selected brushes
		// and patches; the same for "toolFlags ..;" ( splitGeo )
		bool set_content_flag(std::string_view word, bool on);
		bool set_tool_flag(std::string_view word, bool on);
		// Radiant's "link": the selected brushes move into `entity`. no_object: the one brush entity that has brushes
		// in the selection.
		bool move_selection_to_entity(object_id entity = no_object);
		// one brush out of the selected ones when together they are convex ( and belong to one entity ). Faces keep
		// the plane points and textures they had.
		bool csg_merge();
		// Faces that no one can see - covered completely by other opaque world brushes - get `material`. Looked at:
		// the selected brushes, or with `selected_only` false every brush of the worldspawn ( a brush entity may
		// move and show what it covers: its brushes are only caulked when they are selected ). `opaque`: says whether a
		// material hides what is behind it ( default: every material that is no tool material, plus caulk and sky ).
		// Returns the number of faces changed, -1 when there was nothing to look at.
		int auto_caulk(bool selected_only, std::string_view material = "caulk",
			const std::function<bool(std::string_view)>& opaque = {});

		// ---- entities ---------------------------------------------------------------------------
		// Selected point entities fall onto the first brush / patch below them. Models end with their origin on the
		// surface, other classes with the bottom of their box. `orient`: pitch and roll follow the surface ( the yaw
		// stays ). Returns how many moved.
		int drop_to_floor(bool orient = false);
		// every entity ( or every selected one ) whose `key` is `from` gets `to`; '*' in `from` is allowed. Returns
		// the number of entities changed.
		int replace_key_value(std::string_view key, std::string_view from, std::string_view to, bool selected_only);

		[[nodiscard]] map_info info() const;
		// "entity N brush M" as build reports and radiant count them: position in the file. no_object when there is none.
		[[nodiscard]] object_id find_by_number(int entity_number, int brush_number = -1) const;

		// ---- filters / region -------------------------------------------------------------------
		void set_view_filter(const view_filter& filter);
		[[nodiscard]] const view_filter& current_view_filter() const;
		void set_select_filter(const select_filter& filter);
		[[nodiscard]] const select_filter& current_select_filter() const;
		// visible, but not to be picked
		[[nodiscard]] bool is_selectable(object_id id) const;

		// Only what touches the box is shown ( and edited, and written by region_map_text ). View state.
		void set_region(const bounds& box);
		void clear_region();
		[[nodiscard]] bool has_region() const;
		[[nodiscard]] bounds region() const;

		// what the last hide_selected / hide_unselected hid is shown again
		void show_last_hidden();

		// ---- prefabs ----------------------------------------------------------------------------
		[[nodiscard]] static bool is_prefab(const entity& e) { return e.classname() == "misc_prefab"; }
		// Replaces misc_prefab entities by what their file holds, placed by the entity's origin and angles: brushes
		// and patches of the prefab's worldspawn go into this worldspawn, its entities are appended.
		//   selected_only  only the selected prefab entities, one level deep ( Stamp Prefab: prefabs inside stay
		//                  prefabs ); otherwise every prefab of the map with everything nested in it ( the compilers )
		// A prefab that cannot be loaded, or that contains itself, stays an entity and is named in the warnings.
		// False when nothing was expanded.
		bool expand_prefabs(const prefab_loader& load, prefab_result& result, bool selected_only = false, int max_depth = 16);
		// The selection becomes the text of a prefab .map ( `prefab_text`, relative to `origin` ) and is replaced by
		// a misc_prefab entity at `origin` that names `model`. `origin` nullptr: the middle of the selection's
		// floor, on whole units. The caller writes the file. One undo step.
		object_id convert_selection_to_prefab(std::string_view model, std::string& prefab_text, const vec3* origin = nullptr);
