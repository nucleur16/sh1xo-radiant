#pragma once

// state of a document ( private )

#include "codol_edit/document.hpp"
#include "codol_edit/class_catalog.hpp"

#include <algorithm>
#include <optional>
#include <unordered_map>
#include <unordered_set>

namespace codol::edit
{
	struct slot
	{
		object_kind kind = object_kind::none;
		std::shared_ptr<const void> ptr;
	};

	struct selection_state
	{
		std::vector<object_id> objects;
		std::vector<face_ref> faces;
	};

	struct change
	{
		object_id id = no_object;
		slot before, after;
	};

	struct undo_step
	{
		std::string name;
		std::uint64_t merge_key = 0;
		std::vector<change> changes;
		bool order_changed = false;
		std::vector<object_id> order_before, order_after;
		bool layers_changed = false;
		std::vector<layer> layers_before, layers_after;
		bool header_dirty_before = false, header_dirty_after = false;
		selection_state sel_before, sel_after;
		std::uint64_t token_before = 0, token_after = 0;
	};

	struct document::impl
	{
		// ---- file
		std::shared_ptr<const std::string> source;
		std::string eol = "\n";
		int version = 4;
		source_text header_source;
		bool header_dirty = true;
		source_text trailer;
		std::vector<std::string> warnings;

		// ---- model
		std::vector<layer> layers;
		std::vector<object_id> order;
		std::vector<slot> slots{ 1 };	// index = object id; 0 is never used
		std::size_t n_brushes = 0, n_patches = 0;

		// ---- selection / view state
		selection_state sel;
		mutable std::unordered_set<object_id> sel_index;
		mutable bool sel_index_valid = false;
		std::unordered_set<object_id> hidden;

		// ---- undo
		std::vector<undo_step> undo_stack, redo_stack;
		std::optional<undo_step> open;
		std::unordered_map<object_id, std::size_t> open_index;	// id -> index in open->changes
		int depth = 0;
		bool merge_allowed = false;		// false after undo / redo: the next step never merges into an older one
		std::size_t undo_limit = 256;

		std::uint64_t next_revision = 0;
		std::uint64_t change_counter = 1;
		std::uint64_t token_gen = 1, current_token = 1, saved_token = 1;	// identity of the document state, for modified()
		std::uint64_t selection_counter = 1;
		std::uint64_t visibility_counter = 1;

		// ---- settings
		bool texture_lock = true;				// moves
		bool texture_lock_rotations = true;		// rotations, scales, flips
		// the lightmap layer: on, it goes with the geometry as the material does with the two locks above; off, it stays
		// where the world is ( faces keep their lightmap projection, patches get theirs from their positions again )
		bool texture_lock_lightmap = false;
		int edit_layer = 0;						// which layer the texture operations change: 0 material, 1 lightmap
		std::string default_material = "caulk";
		texture_size_fn texture_size;
		std::function<bool(const entity&, bounds&)> entity_box;
		std::string last_error;

		// ---- import
		std::shared_ptr<const std::string> baseline_map;
		std::string original_ents;

		// ---- geometry cache
		struct geo_entry
		{
			std::uint64_t revision = 0;
			std::shared_ptr<brush_geometry> geo;
		};
		mutable std::vector<geo_entry> geo_cache;

		// ---- view filter / region / last hidden ( select_ops.cpp )
		view_filter filter;
		select_filter no_select;
		bool region_on = false;
		bounds region_box;
		std::uint64_t filter_serial = 1;
		mutable std::uint64_t filter_built_serial = 0, filter_built_change = 0;
		mutable std::unordered_set<object_id> filtered, unselectable;
		std::vector<object_id> last_hidden;
		[[nodiscard]] bool filter_active() const { return region_on || filter.any() || no_select.any(); }
		void build_filter() const;
		[[nodiscard]] bool is_filtered(object_id id) const;
		[[nodiscard]] bool is_unselectable(object_id id) const;

		// ---- access
		[[nodiscard]] object_kind kind(const object_id id) const { return id < slots.size() ? slots[id].kind : object_kind::none; }
		[[nodiscard]] const entity* get_entity(const object_id id) const
		{
			return kind(id) == object_kind::entity ? static_cast<const entity*>(slots[id].ptr.get()) : nullptr;
		}
		[[nodiscard]] const brush* get_brush(const object_id id) const
		{
			return kind(id) == object_kind::brush ? static_cast<const brush*>(slots[id].ptr.get()) : nullptr;
		}
		[[nodiscard]] const patch* get_patch(const object_id id) const
		{
			return kind(id) == object_kind::patch ? static_cast<const patch*>(slots[id].ptr.get()) : nullptr;
		}
		[[nodiscard]] const object_header* get_header(const object_id id) const
		{
			if (const auto* b = get_brush(id)) { return b; }
			return get_patch(id);
		}
		[[nodiscard]] object_id owner_of(object_id id) const;
		[[nodiscard]] object_id world() const { return order.empty() ? no_object : order.front(); }

		[[nodiscard]] const brush_geometry* geometry(object_id id) const;
		[[nodiscard]] bounds entity_local_box(const entity& e) const;
		[[nodiscard]] bounds object_bounds(object_id id) const;
		[[nodiscard]] std::string_view layer_of(object_id id) const;
		[[nodiscard]] bool layer_hidden(std::string_view name) const;
		[[nodiscard]] bool is_hidden(object_id id) const;
		[[nodiscard]] bool is_visible(object_id id) const;
		[[nodiscard]] bool is_selected(object_id id) const;
		void texture_dims(std::string_view material, int& w, int& h) const;

		// ---- undo plumbing ( map_undo.cpp )
		void begin(std::string_view name, std::uint64_t merge_key = 0);
		bool end();
		void cancel();
		void apply_step(const undo_step& step, bool forward);
		void set_slot(object_id id, slot s);

		// all of these need an open transaction
		change& record(object_id id);
		entity& edit_entity(object_id id, bool keys_changed);
		brush& edit_brush(object_id id);
		patch& edit_patch(object_id id);
		// keep_source: the change does not alter the text of the object ( it moves to another entity )
		object_header& edit_header(object_id id, bool keep_source = false);
		object_id add_entity(entity e, bool at_end = true);
		object_id add_brush(brush b, object_id owner);		// appended to the owner's children
		object_id add_patch(patch p, object_id owner);
		void remove_object(object_id id);					// brush / patch: also from its owner; entity: with its children
		std::vector<object_id>& edit_order();
		std::vector<layer>& edit_layers();

		// ---- selection plumbing
		void selection_changed();
		void set_selection(std::vector<object_id> ids);
		void prune_selection();		// drop ids / faces that no longer exist

		bool fail(std::string message)
		{
			last_error = std::move(message);
			return false;
		}

		// ---- reader / writer ( map_io.cpp )
		static bool parse(std::shared_ptr<const std::string> text, impl& out, std::string& error);
		[[nodiscard]] std::string write(bool canonical) const;
		void reset_to_empty();
		// settings, callbacks and counters of the document that `fresh` replaces
		static void carry_over(const impl& old, impl& fresh);
	};

	// one transaction for the lifetime of the object ( nested inside an open one when there is one )
	struct transaction_scope
	{
		document::impl& d;
		bool done = false;

		transaction_scope(document::impl& impl, const std::string_view name) : d(impl) { d.begin(name); }
		~transaction_scope() { if (!done) { d.end(); } }
		bool commit() { done = true; return d.end(); }
		void cancel() { done = true; d.cancel(); }
	};
}
