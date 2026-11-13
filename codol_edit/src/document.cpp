// document: state, undo stack, selection, visibility, picking and queries

#include "doc_impl.hpp"

namespace codol::edit
{
	// ---- impl: access -------------------------------------------------------------------------------

	object_id document::impl::owner_of(const object_id id) const
	{
		if (kind(id) == object_kind::entity)
		{
			return id;
		}

		const auto* h = get_header(id);
		return h ? h->owner : no_object;
	}

	const brush_geometry* document::impl::geometry(const object_id id) const
	{
		const brush* b = get_brush(id);
		if (!b)
		{
			return nullptr;
		}

		if (geo_cache.size() <= id)
		{
			geo_cache.resize(slots.size());
		}

		auto& entry = geo_cache[id];
		if (!entry.geo || entry.revision != b->revision)
		{
			entry.geo = std::make_shared<brush_geometry>();
			build_brush_geometry(*b, *entry.geo);
			entry.revision = b->revision;
		}

		return entry.geo.get();
	}

	bounds document::impl::entity_local_box(const entity& e) const
	{
		bounds box;
		if (entity_box && entity_box(e, box) && box.valid())
		{
			return box;
		}

		float mins[3], maxs[3];
		class_catalog::c1().box(e.classname(), mins, maxs);
		box.mins = { mins[0], mins[1], mins[2] };
		box.maxs = { maxs[0], maxs[1], maxs[2] };
		return box;
	}

	bounds document::impl::object_bounds(const object_id id) const
	{
		bounds box;

		switch (kind(id))
		{
		case object_kind::brush:
			box = geometry(id)->box;
			break;

		case object_kind::patch:
			for (const auto& v : get_patch(id)->vertices)
			{
				box.add(v.xyz);
			}
			break;

		case object_kind::entity:
		{
			const entity* e = get_entity(id);
			if (e->children.empty())
			{
				const bounds local = entity_local_box(*e);
				const vec3 origin = e->origin();
				box.mins = local.mins + origin;
				box.maxs = local.maxs + origin;
			}
			else
			{
				for (const object_id child : e->children)
				{
					box.add(object_bounds(child));
				}
			}
			break;
		}

		default:
			break;
		}

		return box;
	}

	std::string_view document::impl::layer_of(const object_id id) const
	{
		if (const auto* e = get_entity(id))
		{
			return e->layer;
		}

		const auto* h = get_header(id);
		if (!h)
		{
			return {};
		}

		if (!h->layer.empty())
		{
			return h->layer;
		}

		const auto* owner = get_entity(h->owner);
		return owner ? std::string_view(owner->layer) : std::string_view();
	}

	bool document::impl::layer_hidden(const std::string_view name) const
	{
		for (const layer& l : layers)
		{
			if (l.hidden && l.name == name)
			{
				return true;
			}
		}

		return false;
	}

	bool document::impl::is_hidden(const object_id id) const
	{
		if (hidden.empty())
		{
			return false;
		}

		return hidden.contains(id) || hidden.contains(owner_of(id));
	}

	bool document::impl::is_visible(const object_id id) const
	{
		if (kind(id) == object_kind::none || is_hidden(id) || is_filtered(id))
		{
			return false;
		}

		for (const layer& l : layers)
		{
			if (l.hidden)
			{
				return !layer_hidden(layer_of(id));
			}
		}

		return true;
	}

	bool document::impl::is_selected(const object_id id) const
	{
		if (!sel_index_valid)
		{
			sel_index.clear();
			sel_index.insert(sel.objects.begin(), sel.objects.end());
			sel_index_valid = true;
		}

		return sel_index.contains(id);
	}

	void document::impl::texture_dims(const std::string_view material, int& w, int& h) const
	{
		w = h = 256;
		if (texture_size && (!texture_size(material, w, h) || w <= 0 || h <= 0))
		{
			w = h = 256;
		}
	}

	// ---- impl: undo ---------------------------------------------------------------------------------

	void document::impl::set_slot(const object_id id, slot s)
	{
		if (id >= slots.size())
		{
			slots.resize(static_cast<std::size_t>(id) + 1);
		}

		const auto count = [&](const object_kind k, const int delta)
		{
			if (k == object_kind::brush) { n_brushes += delta; }
			else if (k == object_kind::patch) { n_patches += delta; }
		};

		count(slots[id].kind, -1);
		count(s.kind, 1);
		slots[id] = std::move(s);
	}

	void document::impl::begin(const std::string_view name, const std::uint64_t merge_key)
	{
		if (depth++ > 0)
		{
			return;
		}

		open.emplace();
		open->name = std::string(name);
		open->merge_key = merge_key;
		open->sel_before = sel;
		open->header_dirty_before = header_dirty;
		open_index.clear();
	}

	bool document::impl::end()
	{
		if (depth == 0)
		{
			return false;
		}

		if (--depth > 0)
		{
			return true;
		}

		undo_step step = std::move(*open);
		open.reset();
		open_index.clear();

		for (auto& c : step.changes)
		{
			c.after = slots[c.id];
		}

		std::erase_if(step.changes, [](const change& c) { return c.before.ptr == c.after.ptr; });

		if (step.order_changed && step.order_before == order)
		{
			step.order_changed = false;
		}

		if (step.changes.empty() && !step.order_changed && !step.layers_changed)
		{
			return false;
		}

		step.sel_after = sel;
		step.header_dirty_after = header_dirty;

		if (step.order_changed)
		{
			step.order_after = order;
		}

		if (step.layers_changed)
		{
			step.layers_after = layers;
		}

		step.token_before = current_token;
		step.token_after = current_token = ++token_gen;

		if (merge_allowed && step.merge_key != 0 && !undo_stack.empty() && undo_stack.back().merge_key == step.merge_key)
		{
			undo_step& top = undo_stack.back();

			for (auto& c : step.changes)
			{
				const auto it = std::find_if(top.changes.begin(), top.changes.end(), [&](const change& have) { return have.id == c.id; });
				if (it != top.changes.end())
				{
					it->after = std::move(c.after);
				}
				else
				{
					top.changes.push_back(std::move(c));
				}
			}

			if (step.order_changed)
			{
				if (!top.order_changed)
				{
					top.order_changed = true;
					top.order_before = std::move(step.order_before);
				}

				top.order_after = std::move(step.order_after);
			}

			if (step.layers_changed)
			{
				if (!top.layers_changed)
				{
					top.layers_changed = true;
					top.layers_before = std::move(step.layers_before);
				}

				top.layers_after = std::move(step.layers_after);
			}

			top.header_dirty_after = step.header_dirty_after;
			top.sel_after = std::move(step.sel_after);
			top.token_after = step.token_after;
		}
		else
		{
			undo_stack.push_back(std::move(step));

			if (undo_limit != 0 && undo_stack.size() > undo_limit)
			{
				undo_stack.erase(undo_stack.begin(), undo_stack.begin() + static_cast<std::ptrdiff_t>(undo_stack.size() - undo_limit));
			}
		}

		redo_stack.clear();
		merge_allowed = true;
		change_counter++;
		return true;
	}

	void document::impl::apply_step(const undo_step& step, const bool forward)
	{
		for (const change& c : step.changes)
		{
			set_slot(c.id, forward ? c.after : c.before);
		}

		if (step.order_changed)
		{
			order = forward ? step.order_after : step.order_before;
		}

		if (step.layers_changed)
		{
			// hidden is view state: keep what the layers have now
			std::vector<layer> restored = forward ? step.layers_after : step.layers_before;
			for (layer& l : restored)
			{
				l.hidden = layer_hidden(l.name);
			}

			layers = std::move(restored);
		}

		header_dirty = forward ? step.header_dirty_after : step.header_dirty_before;
		sel = forward ? step.sel_after : step.sel_before;
		current_token = forward ? step.token_after : step.token_before;
		merge_allowed = false;
		selection_changed();
		change_counter++;
		visibility_counter++;
	}

	void document::impl::cancel()
	{
		if (depth == 0)
		{
			return;
		}

		depth = 0;

		undo_step step = std::move(*open);
		open.reset();
		open_index.clear();

		step.token_before = current_token;
		apply_step(step, false);
	}

	change& document::impl::record(const object_id id)
	{
		const auto it = open_index.find(id);
		if (it != open_index.end())
		{
			return open->changes[it->second];
		}

		change c;
		c.id = id;

		if (id < slots.size())
		{
			c.before = slots[id];
		}

		open_index.emplace(id, open->changes.size());
		open->changes.push_back(std::move(c));
		return open->changes.back();
	}

	namespace
	{
		// the object of a slot, copied unless this transaction already owns it
		template <typename T>
		T& writable(document::impl& d, const object_id id, const object_kind kind)
		{
			const change& c = d.record(id);
			slot& sl = d.slots[id];

			if (sl.ptr == c.before.ptr)
			{
				auto copy = std::make_shared<T>(*static_cast<const T*>(sl.ptr.get()));
				T& ref = *copy;
				sl.ptr = std::move(copy);
				sl.kind = kind;
				ref.revision = ++d.next_revision;
				return ref;
			}

			T& ref = *const_cast<T*>(static_cast<const T*>(sl.ptr.get()));
			ref.revision = ++d.next_revision;
			return ref;
		}
	}

	entity& document::impl::edit_entity(const object_id id, const bool keys_changed)
	{
		entity& e = writable<entity>(*this, id, object_kind::entity);
		if (keys_changed)
		{
			e.source_head = {};
		}

		return e;
	}

	brush& document::impl::edit_brush(const object_id id)
	{
		brush& b = writable<brush>(*this, id, object_kind::brush);
		b.source_body = {};
		return b;
	}

	patch& document::impl::edit_patch(const object_id id)
	{
		patch& p = writable<patch>(*this, id, object_kind::patch);
		p.source_body = {};
		return p;
	}

	object_header& document::impl::edit_header(const object_id id, const bool keep_source)
	{
		if (kind(id) == object_kind::brush)
		{
			brush& b = writable<brush>(*this, id, object_kind::brush);
			if (!keep_source)
			{
				b.source_body = {};
			}

			return b;
		}

		patch& p = writable<patch>(*this, id, object_kind::patch);
		if (!keep_source)
		{
			p.source_body = {};
		}

		return p;
	}

	object_id document::impl::add_entity(entity e, const bool at_end)
	{
		const auto id = static_cast<object_id>(slots.size());
		record(id);

		e.id = id;
		e.revision = ++next_revision;
		e.source_lead = {};
		e.source_head = {};
		e.source_tail = {};
		e.source_number = -1;
		e.source_late_pairs = false;
		e.children.clear();

		set_slot(id, { object_kind::entity, std::make_shared<entity>(std::move(e)) });

		auto& o = edit_order();
		if (at_end || o.empty())
		{
			o.push_back(id);
		}
		else
		{
			o.insert(o.begin() + 1, id);
		}

		return id;
	}

	object_id document::impl::add_brush(brush b, const object_id owner)
	{
		const auto id = static_cast<object_id>(slots.size());
		record(id);

		b.id = id;
		b.owner = owner;
		b.revision = ++next_revision;
		b.source_lead = {};
		b.source_body = {};
		b.source_number = -1;

		set_slot(id, { object_kind::brush, std::make_shared<brush>(std::move(b)) });
		edit_entity(owner, false).children.push_back(id);
		return id;
	}

	object_id document::impl::add_patch(patch p, const object_id owner)
	{
		const auto id = static_cast<object_id>(slots.size());
		record(id);

		p.id = id;
		p.owner = owner;
		p.revision = ++next_revision;
		p.source_lead = {};
		p.source_body = {};
		p.source_number = -1;

		set_slot(id, { object_kind::patch, std::make_shared<patch>(std::move(p)) });
		edit_entity(owner, false).children.push_back(id);
		return id;
	}

	void document::impl::remove_object(const object_id id)
	{
		const object_kind k = kind(id);

		if (k == object_kind::entity)
		{
			const std::vector<object_id> children = get_entity(id)->children;
			for (const object_id child : children)
			{
				record(child);
				set_slot(child, {});
			}

			record(id);
			set_slot(id, {});
			std::erase(edit_order(), id);
		}
		else if (k == object_kind::brush || k == object_kind::patch)
		{
			const object_id owner = get_header(id)->owner;
			if (kind(owner) == object_kind::entity)
			{
				std::erase(edit_entity(owner, false).children, id);
			}

			record(id);
			set_slot(id, {});
		}
	}

	std::vector<object_id>& document::impl::edit_order()
	{
		if (!open->order_changed)
		{
			open->order_changed = true;
			open->order_before = order;
		}

		return order;
	}

	std::vector<layer>& document::impl::edit_layers()
	{
		if (!open->layers_changed)
		{
			open->layers_changed = true;
			open->layers_before = layers;
		}

		header_dirty = true;
		return layers;
	}

	// ---- impl: selection ----------------------------------------------------------------------------

	void document::impl::selection_changed()
	{
		sel_index_valid = false;
		selection_counter++;
	}

	void document::impl::set_selection(std::vector<object_id> ids)
	{
		std::vector<object_id> out;
		std::unordered_set<object_id> seen;

		const auto add = [&](const object_id id)
		{
			if (seen.insert(id).second)
			{
				out.push_back(id);
			}
		};

		for (const object_id id : ids)
		{
			switch (kind(id))
			{
			case object_kind::brush:
			case object_kind::patch:
				add(id);
				break;

			case object_kind::entity:
			{
				const entity* e = get_entity(id);
				if (e->children.empty())
				{
					add(id);
				}
				else if (id != world())
				{
					for (const object_id child : e->children)
					{
						add(child);
					}
				}
				break;
			}

			default:
				break;
			}
		}

		sel.objects = std::move(out);
		selection_changed();
	}

	void document::impl::prune_selection()
	{
		const auto before = sel.objects.size() + sel.faces.size();

		std::erase_if(sel.objects, [&](const object_id id)
		{
			const object_kind k = kind(id);
			return k == object_kind::none || (k == object_kind::entity && !get_entity(id)->children.empty());
		});

		std::erase_if(sel.faces, [&](const face_ref& f)
		{
			const brush* b = get_brush(f.brush);
			return !b || f.face < 0 || f.face >= static_cast<int>(b->faces.size());
		});

		if (before != sel.objects.size() + sel.faces.size())
		{
			selection_changed();
		}
	}

	void document::impl::reset_to_empty()
	{
		entity world;
		world.id = 1;
		world.revision = ++next_revision;
		world.pairs.emplace_back("classname", "worldspawn");

		slots.assign(2, {});
		slots[1] = { object_kind::entity, std::make_shared<entity>(std::move(world)) };
		order = { 1 };
		header_dirty = true;
	}

	void document::impl::carry_over(const impl& old, impl& fresh)
	{
		fresh.texture_lock = old.texture_lock;
		fresh.default_material = old.default_material;
		fresh.texture_size = old.texture_size;
		fresh.entity_box = old.entity_box;
		fresh.undo_limit = old.undo_limit;
		fresh.change_counter = old.change_counter + 1;
		fresh.selection_counter = old.selection_counter + 1;
		fresh.visibility_counter = old.visibility_counter + 1;
		fresh.token_gen = fresh.current_token = fresh.saved_token = old.token_gen + 1;

		// revisions never repeat across loads
		for (auto& sl : fresh.slots)
		{
			if (sl.kind == object_kind::entity)
			{
				const_cast<entity*>(static_cast<const entity*>(sl.ptr.get()))->revision += old.next_revision;
			}
			else if (sl.kind == object_kind::brush)
			{
				const_cast<brush*>(static_cast<const brush*>(sl.ptr.get()))->revision += old.next_revision;
			}
			else if (sl.kind == object_kind::patch)
			{
				const_cast<patch*>(static_cast<const patch*>(sl.ptr.get()))->revision += old.next_revision;
			}
		}

		fresh.next_revision += old.next_revision;
	}

	// ---- document -----------------------------------------------------------------------------------

	document::document() : d(std::make_unique<impl>())
	{
		d->reset_to_empty();
	}

	document::~document() = default;
	document::document(document&&) noexcept = default;
	document& document::operator=(document&&) noexcept = default;

	void document::new_map()
	{
		auto fresh = std::make_unique<impl>();
		fresh->reset_to_empty();
		impl::carry_over(*d, *fresh);
		d = std::move(fresh);
	}

	bool document::modified() const { return d->current_token != d->saved_token; }
	void document::mark_saved() { d->saved_token = d->current_token; }
	std::string_view document::line_end() const { return d->eol; }
	const std::vector<std::string>& document::load_warnings() const { return d->warnings; }

	std::span<const object_id> document::entities() const { return d->order; }
	object_id document::worldspawn() const { return d->world(); }
	object_kind document::kind(const object_id id) const { return d->kind(id); }
	const entity* document::find_entity(const object_id id) const { return d->get_entity(id); }
	const brush* document::find_brush(const object_id id) const { return d->get_brush(id); }
	const patch* document::find_patch(const object_id id) const { return d->get_patch(id); }
	object_id document::owner_of(const object_id id) const { return d->owner_of(id); }

	std::uint64_t document::revision(const object_id id) const
	{
		if (const auto* e = d->get_entity(id))
		{
			return e->revision;
		}

		const auto* h = d->get_header(id);
		return h ? h->revision : 0;
	}

	std::uint64_t document::change_counter() const { return d->change_counter; }
	std::size_t document::brush_count() const { return d->n_brushes; }
	std::size_t document::patch_count() const { return d->n_patches; }

	// ---- geometry

	void document::set_texture_size_callback(texture_size_fn fn)
	{
		d->texture_size = std::move(fn);
	}

	void document::set_entity_box_callback(std::function<bool(const entity&, bounds&)> fn)
	{
		d->entity_box = std::move(fn);
	}

	const brush_geometry* document::geometry(const object_id brush) const { return d->geometry(brush); }
	bounds document::object_bounds(const object_id id) const { return d->object_bounds(id); }

	bool document::build_mesh(const object_id id, mesh& out, const int patch_subdivisions) const
	{
		out.clear();

		if (const brush* b = d->get_brush(id))
		{
			build_brush_mesh(*b, *d->geometry(id), d->texture_size, out);
			return true;
		}

		if (const patch* p = d->get_patch(id))
		{
			build_patch_mesh(*p, patch_subdivisions, d->texture_size, out);
			return true;
		}

		return false;
	}

	// ---- visibility

	void document::set_hidden(const object_id id, const bool hidden)
	{
		if (d->kind(id) == object_kind::none)
		{
			return;
		}

		if (hidden)
		{
			d->hidden.insert(id);
		}
		else
		{
			d->hidden.erase(id);

			if (const auto* e = d->get_entity(id))
			{
				for (const object_id child : e->children)
				{
					d->hidden.erase(child);
				}
			}
		}

		d->visibility_counter++;
	}

	bool document::is_hidden(const object_id id) const { return d->is_hidden(id); }
	bool document::is_visible(const object_id id) const { return d->is_visible(id); }
	std::uint64_t document::visibility_counter() const { return d->visibility_counter; }

	void document::hide_selected()
	{
		d->last_hidden.clear();

		for (const object_id id : d->sel.objects)
		{
			if (d->hidden.insert(id).second) { d->last_hidden.push_back(id); }
		}

		d->visibility_counter++;
		deselect_all();
	}

	void document::hide_unselected()
	{
		d->last_hidden.clear();

		for (const object_id ent : d->order)
		{
			const entity* e = d->get_entity(ent);

			if (e->children.empty())
			{
				if (ent != d->world() && !d->is_selected(ent))
				{
					if (d->hidden.insert(ent).second) { d->last_hidden.push_back(ent); }
				}

				continue;
			}

			for (const object_id child : e->children)
			{
				if (!d->is_selected(child))
				{
					if (d->hidden.insert(child).second) { d->last_hidden.push_back(child); }
				}
			}
		}

		d->visibility_counter++;
	}

	void document::show_all()
	{
		d->hidden.clear();
		d->visibility_counter++;
	}

	// ---- layers ( read side; the operations are in edit_ops.cpp )

	std::span<const layer> document::layers() const { return d->layers; }

	std::string_view document::active_layer() const
	{
		for (const layer& l : d->layers)
		{
			if (l.active)
			{
				return l.name;
			}
		}

		return {};
	}

	std::string_view document::layer_of(const object_id id) const { return d->layer_of(id); }

	void document::set_layer_hidden(const std::string_view name, const bool hidden, const bool with_children)
	{
		const std::string prefix = std::string(name) + "/";

		for (layer& l : d->layers)
		{
			if (l.name == name || (with_children && l.name.starts_with(prefix)))
			{
				l.hidden = hidden;
			}
		}

		d->visibility_counter++;
	}

	// ---- selection

	std::span<const object_id> document::selection() const { return d->sel.objects; }
	std::span<const face_ref> document::selected_faces() const { return d->sel.faces; }
	bool document::is_selected(const object_id id) const { return d->is_selected(id); }

	bool document::is_face_selected(const face_ref f) const
	{
		return std::find(d->sel.faces.begin(), d->sel.faces.end(), f) != d->sel.faces.end();
	}

	void document::select(const object_id id, const select_mode mode)
	{
		const object_id ids[1] = { id };
		select(std::span<const object_id>(ids), mode);
	}

	void document::select(const std::span<const object_id> ids, const select_mode mode)
	{
		// what the ids stand for ( an entity with brushes: its brushes )
		const selection_state keep = d->sel;
		d->set_selection(std::vector<object_id>(ids.begin(), ids.end()));
		std::vector<object_id> expanded = std::move(d->sel.objects);
		d->sel = keep;

		std::vector<object_id> result;

		switch (mode)
		{
		case select_mode::replace:
			result = std::move(expanded);
			d->sel.faces.clear();
			break;

		case select_mode::add:
			result = d->sel.objects;
			result.insert(result.end(), expanded.begin(), expanded.end());
			break;

		case select_mode::remove:
		{
			const std::unordered_set<object_id> gone(expanded.begin(), expanded.end());
			for (const object_id id : d->sel.objects)
			{
				if (!gone.contains(id))
				{
					result.push_back(id);
				}
			}
			break;
		}

		case select_mode::toggle:
		{
			const std::unordered_set<object_id> flip(expanded.begin(), expanded.end());
			for (const object_id id : d->sel.objects)
			{
				if (!flip.contains(id))
				{
					result.push_back(id);
				}
			}

			for (const object_id id : expanded)
			{
				if (!d->is_selected(id))
				{
					result.push_back(id);
				}
			}
			break;
		}
		}

		d->set_selection(std::move(result));
	}

	void document::select_face(const face_ref f, const select_mode mode)
	{
		const brush* b = d->get_brush(f.brush);
		const bool exists = b && f.face >= 0 && f.face < static_cast<int>(b->faces.size());
		const auto it = std::find(d->sel.faces.begin(), d->sel.faces.end(), f);
		const bool selected = it != d->sel.faces.end();

		switch (mode)
		{
		case select_mode::replace:
			d->sel.faces.clear();
			d->sel.objects.clear();
			if (exists) { d->sel.faces.push_back(f); }
			break;

		case select_mode::add:
			if (exists && !selected) { d->sel.faces.push_back(f); }
			break;

		case select_mode::remove:
			if (selected) { d->sel.faces.erase(it); }
			break;

		case select_mode::toggle:
			if (selected) { d->sel.faces.erase(it); }
			else if (exists) { d->sel.faces.push_back(f); }
			break;
		}

		d->selection_changed();
	}

	void document::select_all()
	{
		std::vector<object_id> ids;

		for (const object_id ent : d->order)
		{
			const entity* e = d->get_entity(ent);

			if (e->children.empty())
			{
				if (ent != d->world() && d->is_visible(ent))
				{
					ids.push_back(ent);
				}

				continue;
			}

			for (const object_id child : e->children)
			{
				if (d->is_visible(child))
				{
					ids.push_back(child);
				}
			}
		}

		d->sel.faces.clear();
		d->set_selection(std::move(ids));
	}

	void document::deselect_all()
	{
		if (d->sel.objects.empty() && d->sel.faces.empty())
		{
			return;
		}

		d->sel.objects.clear();
		d->sel.faces.clear();
		d->selection_changed();
	}

	void document::invert_selection()
	{
		std::vector<object_id> ids;

		for (const object_id ent : d->order)
		{
			const entity* e = d->get_entity(ent);

			if (e->children.empty())
			{
				if (ent != d->world() && d->is_visible(ent) && !d->is_selected(ent))
				{
					ids.push_back(ent);
				}

				continue;
			}

			for (const object_id child : e->children)
			{
				if (d->is_visible(child) && !d->is_selected(child))
				{
					ids.push_back(child);
				}
			}
		}

		d->sel.faces.clear();
		d->set_selection(std::move(ids));
	}

	void document::select_whole_entities()
	{
		std::vector<object_id> ids = d->sel.objects;

		for (const object_id id : d->sel.objects)
		{
			const object_id owner = d->owner_of(id);
			if (owner != id && owner != d->world())
			{
				ids.push_back(owner);	// expands to all of its brushes
			}
		}

		d->set_selection(std::move(ids));
	}

	std::vector<object_id> document::selected_entities() const
	{
		std::vector<object_id> out;
		std::unordered_set<object_id> seen;

		for (const object_id id : d->sel.objects)
		{
			const object_id owner = d->owner_of(id);
			if (owner != no_object && owner != d->world() && seen.insert(owner).second)
			{
				out.push_back(owner);
			}
		}

		return out;
	}

	bounds document::selection_bounds() const
	{
		bounds box;
		for (const object_id id : d->sel.objects)
		{
			box.add(d->object_bounds(id));
		}

		return box;
	}

	std::uint64_t document::selection_counter() const { return d->selection_counter; }

	// ---- picking

	namespace
	{
		template <typename Fn>
		void for_each_candidate(const document::impl& d, const pick_options& options, Fn&& fn)
		{
			for (const object_id ent : d.order)
			{
				const entity* e = d.get_entity(ent);

				if (e->children.empty())
				{
					if (options.entities && ent != d.world() && (options.include_hidden || d.is_visible(ent)) && !d.is_unselectable(ent))
					{
						fn(ent, object_kind::entity, ent);
					}

					continue;
				}

				if (!options.include_hidden && d.hidden.contains(ent))
				{
					continue;
				}

				for (const object_id child : e->children)
				{
					const object_kind k = d.kind(child);
					if ((k == object_kind::brush && !options.brushes) || (k == object_kind::patch && !options.patches))
					{
						continue;
					}

					if ((options.include_hidden || d.is_visible(child)) && !d.is_unselectable(child))
					{
						fn(child, k, ent);
					}
				}
			}
		}

		bool pick_object(const document::impl& d, const ray& r, const object_id id, const object_kind k, pick_result& hit)
		{
			double distance = 0.0;

			if (k == object_kind::brush)
			{
				const brush_geometry* geo = d.geometry(id);
				double box_distance;

				if (!geo->valid && geo->face_count_with_winding < 4)
				{
					return false;
				}

				if (!ray_box(r, geo->box, box_distance))
				{
					return false;
				}

				int face = -1;
				if (!ray_brush(r, *geo, distance, face))
				{
					return false;
				}

				hit.face = face;
			}
			else if (k == object_kind::patch)
			{
				const patch* p = d.get_patch(id);
				bounds box;
				for (const auto& v : p->vertices)
				{
					box.add(v.xyz);
				}

				double box_distance;
				if (!box.valid() || !ray_box(r, box, box_distance))
				{
					return false;
				}

				std::vector<vec3> points;
				int columns = 0, rows = 0;
				tessellate_patch(*p, 4, points, columns, rows);

				bool any = false;
				for (int c = 0; c + 1 < columns; c++)
				{
					for (int row = 0; row + 1 < rows; row++)
					{
						const vec3& v00 = points[static_cast<std::size_t>(c) * rows + row];
						const vec3& v10 = points[static_cast<std::size_t>(c + 1) * rows + row];
						const vec3& v11 = points[static_cast<std::size_t>(c + 1) * rows + row + 1];
						const vec3& v01 = points[static_cast<std::size_t>(c) * rows + row + 1];

						// the same two triangles the mesh has
						const bool turned = columns == p->width && rows == p->height && p->at(c, row).has_flag && p->at(c, row).flag != 0;

						double t;
						const bool hit_quad = turned
							? (ray_triangle(r, v00, v10, v01, t) || ray_triangle(r, v10, v11, v01, t))
							: (ray_triangle(r, v00, v10, v11, t) || ray_triangle(r, v00, v11, v01, t));

						if (hit_quad && (!any || t < distance))
						{
							any = true;
							distance = t;
						}
					}
				}

				if (!any)
				{
					return false;
				}
			}
			else
			{
				if (!ray_box(r, d.object_bounds(id), distance))
				{
					return false;
				}
			}

			hit.object = id;
			hit.kind = k;
			hit.distance = distance;
			hit.point = r.origin + r.direction * distance;
			return true;
		}
	}

	bool document::pick(const ray& r, pick_result& out, const pick_options& options) const
	{
		bool any = false;

		for_each_candidate(*d, options, [&](const object_id id, const object_kind k, const object_id owner)
		{
			pick_result hit;
			if (pick_object(*d, r, id, k, hit) && hit.distance <= options.max_distance && (!any || hit.distance < out.distance))
			{
				hit.entity = owner;
				out = hit;
				any = true;
			}
		});

		return any;
	}

	std::vector<pick_result> document::pick_all(const ray& r, const pick_options& options) const
	{
		std::vector<pick_result> hits;

		for_each_candidate(*d, options, [&](const object_id id, const object_kind k, const object_id owner)
		{
			pick_result hit;
			if (pick_object(*d, r, id, k, hit) && hit.distance <= options.max_distance)
			{
				hit.entity = owner;
				hits.push_back(hit);
			}
		});

		std::stable_sort(hits.begin(), hits.end(), [](const pick_result& a, const pick_result& b) { return a.distance < b.distance; });
		return hits;
	}

	std::vector<object_id> document::query_volume(const std::span<const plane> planes, const bool fully_inside, const pick_options& options) const
	{
		std::vector<object_id> out;
		std::vector<vec3> points;

		for_each_candidate(*d, options, [&](const object_id id, const object_kind k, object_id)
		{
			points.clear();

			if (k == object_kind::brush)
			{
				points = d->geometry(id)->vertices;
			}
			else if (k == object_kind::patch)
			{
				for (const auto& v : d->get_patch(id)->vertices)
				{
					points.push_back(v.xyz);
				}
			}
			else
			{
				const bounds box = d->object_bounds(id);
				for (int i = 0; i < 8; i++)
				{
					points.push_back({ (i & 1) ? box.maxs.x : box.mins.x, (i & 2) ? box.maxs.y : box.mins.y, (i & 4) ? box.maxs.z : box.mins.z });
				}
			}

			if (points.empty())
			{
				return;
			}

			for (const plane& pl : planes)
			{
				bool all_front = true, any_front = false;

				for (const vec3& p : points)
				{
					if (pl.distance_to(p) > ON_EPSILON)
					{
						any_front = true;
					}
					else
					{
						all_front = false;
					}
				}

				if (fully_inside ? any_front : all_front)
				{
					return;
				}
			}

			out.push_back(id);
		});

		return out;
	}

	std::vector<object_id> document::query_rect(const int axis, const vec2 lo, const vec2 hi, const bool fully_inside, const pick_options& options) const
	{
		const int a = axis == 0 ? 1 : 0;
		const int b = axis == 2 ? 1 : 2;

		plane planes[4];
		planes[0].normal[a] = -1.0; planes[0].dist = -std::min(lo.x, hi.x);
		planes[1].normal[a] = 1.0; planes[1].dist = std::max(lo.x, hi.x);
		planes[2].normal[b] = -1.0; planes[2].dist = -std::min(lo.y, hi.y);
		planes[3].normal[b] = 1.0; planes[3].dist = std::max(lo.y, hi.y);

		return query_volume(planes, fully_inside, options);
	}

	// ---- undo

	void document::begin_transaction(const std::string_view name, const std::uint64_t merge_key) { d->begin(name, merge_key); }
	bool document::end_transaction() { return d->end(); }
	void document::cancel_transaction() { d->cancel(); }
	bool document::in_transaction() const { return d->depth > 0; }
	bool document::can_undo() const { return d->depth == 0 && !d->undo_stack.empty(); }
	bool document::can_redo() const { return d->depth == 0 && !d->redo_stack.empty(); }

	bool document::undo()
	{
		if (!can_undo())
		{
			return false;
		}

		undo_step step = std::move(d->undo_stack.back());
		d->undo_stack.pop_back();
		d->apply_step(step, false);
		d->redo_stack.push_back(std::move(step));
		return true;
	}

	bool document::redo()
	{
		if (!can_redo())
		{
			return false;
		}

		undo_step step = std::move(d->redo_stack.back());
		d->redo_stack.pop_back();
		d->apply_step(step, true);
		d->undo_stack.push_back(std::move(step));
		return true;
	}

	std::string_view document::undo_name() const { return d->undo_stack.empty() ? std::string_view() : std::string_view(d->undo_stack.back().name); }
	std::string_view document::redo_name() const { return d->redo_stack.empty() ? std::string_view() : std::string_view(d->redo_stack.back().name); }
	std::size_t document::undo_count() const { return d->undo_stack.size(); }
	std::size_t document::redo_count() const { return d->redo_stack.size(); }

	void document::clear_undo()
	{
		d->undo_stack.clear();
		d->redo_stack.clear();
	}

	void document::set_undo_limit(const std::size_t steps) { d->undo_limit = steps; }
	const std::string& document::last_error() const { return d->last_error; }

	void document::set_texture_lock(const bool on) { d->texture_lock = on; }
	bool document::texture_lock() const { return d->texture_lock; }
	void document::set_default_material(const std::string_view material) { d->default_material = std::string(material); }
	std::string_view document::default_material() const { return d->default_material; }
}
