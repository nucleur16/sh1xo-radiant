// document: what the Selection, Region and Misc menus and the Filter window of the editor need - selecting by
// key / links / volume, the clipboard as .map text, content flags, link, CSG merge, auto caulk, drop to floor,
// the view filter and the region. Every editing operation is one undo step, like those of edit_ops.cpp.

#include "doc_impl.hpp"

#include <cctype>
#include <map>

namespace codol::edit
{
	namespace
	{
		using impl = document::impl;

		std::string_view bare_material(std::string_view name)
		{
			if (name.rfind("wc/", 0) == 0)
			{
				name.remove_prefix(3);
			}

			return name;
		}

		bool starts(const std::string_view text, const std::string_view prefix)
		{
			return text.rfind(prefix, 0) == 0;
		}

		char lower(const char c)
		{
			return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
		}

		void set_pair(entity& e, const std::string_view key, const std::string_view value)
		{
			for (auto& [k, v] : e.pairs)
			{
				if (k == key)
				{
					v = std::string(value);
					return;
				}
			}

			e.pairs.emplace_back(std::string(key), std::string(value));
		}

		bool is_xmodel(const std::string_view model)
		{
			return !model.empty() && model[0] != '*' && model[0] != '?' && model.find(".map") == std::string_view::npos;
		}

		// ---- what a filter says about an object -------------------------------------------------------

		struct brush_traits
		{
			bool clip = false, weapon_clip = false, caulk = false, trigger = false, portal = false, hint = false, sky = false;
			bool volume = false;	// a tool volume: clip, trigger, portal, hint, nodraw .. - not caulk ( a hidden face of a wall ), not sky
		};

		brush_traits traits_of(const brush& b)
		{
			brush_traits t;
			bool all_caulk = !b.faces.empty();

			for (const face& f : b.faces)
			{
				const material_class c = classify_material(f.texture.material);
				t.clip |= c == material_class::clip;
				t.weapon_clip |= starts(bare_material(f.texture.material), "clip_weap");
				t.trigger |= c == material_class::trigger;
				t.portal |= c == material_class::portal;
				t.hint |= c == material_class::hint;
				t.sky |= c == material_class::sky;
				t.volume |= c == material_class::clip || c == material_class::trigger || c == material_class::portal || c == material_class::hint
					|| c == material_class::nodraw;
				all_caulk &= c == material_class::caulk;
			}

			t.caulk = all_caulk;
			t.weapon_clip |= b.has_content("weaponClip");
			return t;
		}

		bool brush_filtered(const brush& b, const bool in_world, const view_filter& f)
		{
			const brush_traits t = traits_of(b);

			return (f.clip && t.clip) || (f.weapon_clip && t.weapon_clip) || (f.caulk && t.caulk) || (f.triggers && t.trigger) || (f.portals && t.portal)
				|| (f.hints && t.hint) || (f.sky && t.sky) || (f.detail && b.is_detail()) || (f.non_colliding && b.has_content("nonColliding"))
				|| (f.structural && in_world && !b.is_detail() && !t.volume);
		}

		bool entity_filtered(const entity& e, const view_filter& f)
		{
			if (f.entities)
			{
				return true;
			}

			const std::string_view cls = e.classname();

			for (const std::string& pattern : f.classes)
			{
				if (!pattern.empty() && wildcard_match(pattern, cls))
				{
					return true;
				}
			}

			const bool has_brushes = !e.children.empty();
			const bool model = is_xmodel(e.value("model")) && !document::is_prefab(e);
			const bool light = starts(cls, "light");
			const bool node = starts(cls, "node_");
			const bool spawn = cls.find("spawn") != std::string_view::npos || cls == "mp_global_intermission" || cls == "info_player_start";
			const bool trigger = starts(cls, "trigger");
			const bool script = starts(cls, "script_") && !model && !has_brushes;
			const bool info = starts(cls, "info_") && !spawn;
			const bool prefab = document::is_prefab(e);

			if ((f.brush_entities && has_brushes) || (f.models && model) || (f.lights && light) || (f.pathnodes && node) || (f.spawns && spawn)
				|| (f.triggers && trigger) || (f.script && script) || (f.info && info) || (f.prefabs && prefab))
			{
				return true;
			}

			return f.other && !has_brushes && !model && !light && !node && !spawn && !trigger && !script && !info && !prefab;
		}

		// ---- copies between documents -----------------------------------------------------------------

		std::string known_layer(const impl& d, const std::string& name)
		{
			for (const layer& l : d.layers)
			{
				if (l.name == name)
				{
					return name;
				}
			}

			return {};
		}

		// The objects `ids` of `from` in file order into `to` ( which has an open transaction ): children of the
		// worldspawn under to's worldspawn, children of another entity under a copy of that entity.
		void copy_subset(const impl& from, impl& to, const std::unordered_set<object_id>& ids, std::vector<object_id>* created)
		{
			for (const object_id ent : from.order)
			{
				const entity* e = from.get_entity(ent);

				if (e->children.empty())
				{
					if (ent != from.world() && ids.contains(ent))
					{
						entity copy = *e;
						copy.layer = known_layer(to, copy.layer);
						const object_id made = to.add_entity(std::move(copy));
						if (created) { created->push_back(made); }
					}

					continue;
				}

				object_id target = no_object;

				for (const object_id child : e->children)
				{
					if (!ids.contains(child))
					{
						continue;
					}

					const brush* b = from.get_brush(child);
					if (b && b->opaque)
					{
						continue;	// a block the editor does not understand has no text of its own once it is copied
					}

					if (target == no_object)
					{
						if (ent == from.world())
						{
							target = to.world();
						}
						else
						{
							entity copy = *e;
							copy.layer = known_layer(to, copy.layer);
							target = to.add_entity(std::move(copy));
						}
					}

					object_id made = no_object;

					if (b)
					{
						brush copy = *b;
						copy.layer = known_layer(to, copy.layer);
						made = to.add_brush(std::move(copy), target);
					}
					else if (const patch* p = from.get_patch(child))
					{
						patch copy = *p;
						copy.layer = known_layer(to, copy.layer);
						made = to.add_patch(std::move(copy), target);
					}

					if (created && made != no_object) { created->push_back(made); }
				}
			}
		}

		std::unordered_set<object_id> all_objects(const impl& d)
		{
			std::unordered_set<object_id> ids;

			for (const object_id ent : d.order)
			{
				const entity* e = d.get_entity(ent);
				if (e->children.empty())
				{
					ids.insert(ent);
				}

				ids.insert(e->children.begin(), e->children.end());
			}

			return ids;
		}

		// ---- convex pieces ( csg merge, auto caulk ) --------------------------------------------------

		double volume_of(const brush_geometry& geo)
		{
			double v = 0.0;
			for (const auto& indices : geo.face_vertices)
			{
				for (std::size_t k = 2; k < indices.size(); k++)
				{
					v += dot(geo.vertices[static_cast<std::size_t>(indices[0])],
						cross(geo.vertices[static_cast<std::size_t>(indices[k - 1])], geo.vertices[static_cast<std::size_t>(indices[k])]));
				}
			}

			return std::fabs(v) / 6.0;
		}

		// volume of the convex region behind all planes; 0 when it is empty or flat
		double volume_behind(const std::vector<plane>& planes)
		{
			brush tmp;
			tmp.faces.reserve(planes.size());

			for (const plane& p : planes)
			{
				face f;
				points_for_plane(p, f.points);
				tmp.faces.push_back(std::move(f));
			}

			brush_geometry geo;
			if (!build_brush_geometry(tmp, geo))
			{
				return 0.0;
			}

			return volume_of(geo);
		}

		bool same_plane(const plane& a, const plane& b)
		{
			return dot(a.normal, b.normal) > 1.0 - 1e-6 && std::fabs(a.dist - b.dist) < 0.01;
		}

		bool default_opaque(const std::string_view material)
		{
			const material_class c = classify_material(material);
			return c == material_class::normal || c == material_class::caulk || c == material_class::sky;
		}
	}

	// ---- materials / patterns -----------------------------------------------------------------------

	material_class classify_material(const std::string_view material)
	{
		const std::string_view m = bare_material(material);

		if (starts(m, "caulk") || m == "sky_caulk" || m == "lightdef_caulk" || m == "common/caulk") { return material_class::caulk; }
		if (starts(m, "clip")) { return material_class::clip; }
		if (starts(m, "trigger") || m == "aitrig") { return material_class::trigger; }
		if (starts(m, "portal")) { return material_class::portal; }
		if (starts(m, "hint") || m == "skip") { return material_class::hint; }
		if (m == "sky" || starts(m, "sky_")) { return material_class::sky; }
		if (starts(m, "nodraw") || starts(m, "lightgrid") || m == "ladder" || starts(m, "mantle") || starts(m, "tools/") || m == "noimpact"
			|| m == "shadowcaster" || m == "occluder")
		{
			return material_class::nodraw;
		}

		return material_class::normal;
	}

	bool is_tool_material(const std::string_view material)
	{
		return classify_material(material) != material_class::normal;
	}

	bool wildcard_match(const std::string_view pattern, const std::string_view text)
	{
		if (pattern.empty())
		{
			return true;
		}

		// iterative glob with backtracking to the last '*'
		std::size_t p = 0, t = 0, star = std::string_view::npos, mark = 0;

		while (t < text.size())
		{
			if (p < pattern.size() && pattern[p] == '*')
			{
				star = p++;
				mark = t;
			}
			else if (p < pattern.size() && lower(pattern[p]) == lower(text[t]))
			{
				p++;
				t++;
			}
			else if (star != std::string_view::npos)
			{
				p = star + 1;
				t = ++mark;
			}
			else
			{
				return false;
			}
		}

		while (p < pattern.size() && pattern[p] == '*')
		{
			p++;
		}

		return p == pattern.size();
	}

	// ---- filter / region ----------------------------------------------------------------------------

	void document::impl::build_filter() const
	{
		const std::uint64_t key = change_counter * 0x9E3779B97F4A7C15ull + next_revision;
		if (filter_built_serial == filter_serial && filter_built_change == key)
		{
			return;
		}

		filter_built_serial = filter_serial;
		filter_built_change = key;
		filtered.clear();

		const bool by_filter = filter.any();

		for (const object_id ent : order)
		{
			const entity* e = get_entity(ent);
			if (!e)
			{
				continue;
			}

			const bool in_world = ent == world();

			if (!in_world && by_filter && entity_filtered(*e, filter))
			{
				filtered.insert(ent);	// its children are found through their owner
				continue;
			}

			if (e->children.empty())
			{
				if (!in_world && region_on && !region_box.intersects(object_bounds(ent)))
				{
					filtered.insert(ent);
				}

				continue;
			}

			for (const object_id child : e->children)
			{
				bool out = false;

				if (by_filter)
				{
					if (const brush* b = get_brush(child))
					{
						out = (in_world && filter.world) || brush_filtered(*b, in_world, filter);
					}
					else if (const patch* p = get_patch(child))
					{
						out = (in_world && filter.world) || (p->type == patch_type::curve ? filter.curves : filter.terrain);
					}
				}

				if (!out && region_on)
				{
					out = !region_box.intersects(object_bounds(child));
				}

				if (out)
				{
					filtered.insert(child);
				}
			}
		}
	}

	bool document::impl::is_filtered(const object_id id) const
	{
		if (!region_on && !filter.any())
		{
			return false;
		}

		build_filter();
		return filtered.contains(id) || filtered.contains(owner_of(id));
	}

	bool document::impl::is_unselectable(const object_id id) const
	{
		if (!no_select.any())
		{
			return false;
		}

		const object_id owner = owner_of(id);
		const bool in_world = owner == world();

		if (const brush* b = get_brush(id))
		{
			if ((no_select.brushes && in_world) || (no_select.entities && !in_world))
			{
				return true;
			}

			return no_select.sky && traits_of(*b).sky;
		}

		if (get_patch(id))
		{
			return no_select.curves || (no_select.entities && !in_world);
		}

		if (const entity* e = get_entity(id))
		{
			return no_select.entities || (no_select.models && is_xmodel(e->value("model")));
		}

		return false;
	}

	void document::set_view_filter(const view_filter& filter)
	{
		d->filter = filter;
		d->filter_serial++;
		d->visibility_counter++;
	}

	const view_filter& document::current_view_filter() const { return d->filter; }

	void document::set_select_filter(const select_filter& filter)
	{
		d->no_select = filter;
	}

	const select_filter& document::current_select_filter() const { return d->no_select; }

	bool document::is_selectable(const object_id id) const
	{
		return d->is_visible(id) && !d->is_unselectable(id);
	}

	void document::set_region(const bounds& box)
	{
		if (!box.valid())
		{
			clear_region();
			return;
		}

		d->region_on = true;
		d->region_box = box;
		d->filter_serial++;
		d->visibility_counter++;
	}

	void document::clear_region()
	{
		d->region_on = false;
		d->filter_serial++;
		d->visibility_counter++;
	}

	bool document::has_region() const { return d->region_on; }
	bounds document::region() const { return d->region_on ? d->region_box : bounds{}; }

	void document::show_last_hidden()
	{
		for (const object_id id : d->last_hidden)
		{
			d->hidden.erase(id);
		}

		d->last_hidden.clear();
		d->visibility_counter++;
	}

	// ---- select -------------------------------------------------------------------------------------

	std::size_t document::select_by_key(const std::string_view key, const std::string_view pattern, const select_mode mode)
	{
		std::vector<object_id> ids;

		for (const object_id ent : d->order)
		{
			const entity* e = d->get_entity(ent);
			if (ent == d->world() || !e)
			{
				continue;
			}

			const std::string* value = e->find(key);
			if (!value || (!pattern.empty() && !wildcard_match(pattern, *value)))
			{
				continue;
			}

			// an entity all of whose parts are filtered away is not selected behind the user's back
			bool visible = e->children.empty() ? d->is_visible(ent) : false;
			for (const object_id child : e->children)
			{
				visible |= d->is_visible(child);
			}

			if (visible)
			{
				ids.push_back(ent);
			}
		}

		select(std::span<const object_id>(ids), mode);
		return ids.size();
	}

	std::size_t document::select_connected()
	{
		const std::vector<object_id> start = selected_entities();
		if (start.empty())
		{
			d->fail("Select connected: no entity selected");
			return 0;
		}

		std::unordered_multimap<std::string, object_id> by_name, by_target;
		for (const object_id ent : d->order)
		{
			const entity* e = d->get_entity(ent);
			if (const std::string* name = e->find("targetname"); name && !name->empty()) { by_name.emplace(*name, ent); }
			if (const std::string* target = e->find("target"); target && !target->empty()) { by_target.emplace(*target, ent); }
		}

		std::unordered_set<object_id> seen(start.begin(), start.end());
		std::vector<object_id> queue = start, found;

		while (!queue.empty())
		{
			const object_id at = queue.back();
			queue.pop_back();
			const entity* e = d->get_entity(at);

			const auto visit = [&](const std::unordered_multimap<std::string, object_id>& map, const std::string* key)
			{
				if (!key || key->empty())
				{
					return;
				}

				const auto range = map.equal_range(*key);
				for (auto it = range.first; it != range.second; ++it)
				{
					if (seen.insert(it->second).second)
					{
						queue.push_back(it->second);
						found.push_back(it->second);
					}
				}
			};

			visit(by_name, e->find("target"));		// what it targets
			visit(by_target, e->find("targetname"));	// what targets it
		}

		select(std::span<const object_id>(found), select_mode::add);
		return found.size();
	}

	bool document::select_by_volume(const volume_select how, const int view_axis)
	{
		std::vector<object_id> markers;
		bounds box;

		for (const object_id id : d->sel.objects)
		{
			const brush* b = d->get_brush(id);
			const brush_geometry* geo = b && !b->opaque ? d->geometry(id) : nullptr;

			if (geo && geo->valid)
			{
				markers.push_back(id);
				box.add(geo->box);
			}
		}

		if (markers.empty())
		{
			return d->fail("Select: select the brush that marks the volume first");
		}

		const bool tall = how == volume_select::complete_tall || how == volume_select::partial_tall;
		if (tall)
		{
			const int axis = std::clamp(view_axis, 0, 2);
			box.mins[axis] = -1e30;
			box.maxs[axis] = 1e30;
		}

		const std::unordered_set<object_id> marker_set(markers.begin(), markers.end());
		std::vector<object_id> found;

		const auto corners_of = [&](const object_id id, std::vector<vec3>& points)
		{
			points.clear();

			if (const brush_geometry* geo = d->geometry(id))
			{
				points = geo->vertices;
				return;
			}

			if (const patch* p = d->get_patch(id))
			{
				for (const auto& v : p->vertices)
				{
					points.push_back(v.xyz);
				}

				return;
			}

			const bounds b = d->object_bounds(id);
			for (int i = 0; i < 8; i++)
			{
				points.push_back({ (i & 1) ? b.maxs.x : b.mins.x, (i & 2) ? b.maxs.y : b.mins.y, (i & 4) ? b.maxs.z : b.mins.z });
			}
		};

		std::vector<vec3> points;

		const auto test = [&](const object_id id)
		{
			if (marker_set.contains(id) || !d->is_visible(id) || d->is_unselectable(id))
			{
				return;
			}

			const bounds b = d->object_bounds(id);
			if (!b.valid())
			{
				return;
			}

			bool hit = false;

			switch (how)
			{
			case volume_select::inside:
			case volume_select::complete_tall:
				hit = true;
				for (int i = 0; i < 3; i++)
				{
					hit &= b.mins[i] >= box.mins[i] - ON_EPSILON && b.maxs[i] <= box.maxs[i] + ON_EPSILON;
				}
				break;

			case volume_select::partial_tall:
				hit = box.intersects(b, -ON_EPSILON);
				break;

			case volume_select::touching:
				corners_of(id, points);

				for (const object_id marker : markers)
				{
					const brush_geometry* geo = d->geometry(marker);
					if (!geo->box.intersects(b, ON_EPSILON))
					{
						continue;
					}

					// no plane of the marker has the whole object in front of it ..
					bool separated = false;
					for (const plane& pl : geo->planes)
					{
						if (pl.normal == vec3{})
						{
							continue;
						}

						separated |= std::all_of(points.begin(), points.end(), [&](const vec3& p) { return pl.distance_to(p) > ON_EPSILON; });
					}

					// .. and no plane of a brush has the whole marker in front of it
					if (const brush_geometry* other = separated ? nullptr : d->geometry(id))
					{
						for (const plane& pl : other->planes)
						{
							if (pl.normal == vec3{})
							{
								continue;
							}

							separated |= std::all_of(geo->vertices.begin(), geo->vertices.end(), [&](const vec3& p) { return pl.distance_to(p) > ON_EPSILON; });
						}
					}

					if (!separated)
					{
						hit = true;
						break;
					}
				}
				break;
			}

			if (hit)
			{
				found.push_back(id);
			}
		};

		for (const object_id ent : d->order)
		{
			const entity* e = d->get_entity(ent);
			if (e->children.empty())
			{
				if (ent != d->world())
				{
					test(ent);
				}

				continue;
			}

			for (const object_id child : e->children)
			{
				test(child);
			}
		}

		if (how == volume_select::touching)
		{
			select(std::span<const object_id>(found), select_mode::add);
			return true;
		}

		if (found.empty())
		{
			return d->fail(tall ? "Select tall: nothing in the column of the selected brush" : "Select inside: nothing inside the selected brush");
		}

		// as in radiant the brush that marked the volume goes
		transaction_scope tx(*d, how == volume_select::inside ? "Select inside" : (how == volume_select::complete_tall ? "Select complete tall" : "Select partial tall"));

		for (const object_id marker : markers)
		{
			const object_id owner = d->owner_of(marker);
			d->remove_object(marker);

			if (const entity* e = d->get_entity(owner); e && owner != d->world() && e->children.empty())
			{
				d->remove_object(owner);
			}
		}

		d->sel.faces.clear();
		d->set_selection(std::move(found));
		return true;
	}

	bool document::select_next_in_group(const int step)
	{
		object_id owner = no_object;
		object_id first = no_object;

		for (const object_id id : d->sel.objects)
		{
			const object_id o = d->owner_of(id);
			if (o != id && o != d->world())
			{
				owner = o;
				first = id;
				break;
			}
		}

		const entity* e = d->get_entity(owner);
		if (!e || e->children.empty())
		{
			return d->fail("Cycle group: select a brush of a brush entity");
		}

		const auto count = static_cast<int>(e->children.size());
		int selected_of_group = 0;
		for (const object_id child : e->children)
		{
			selected_of_group += d->is_selected(child) ? 1 : 0;
		}

		int index = 0;
		if (selected_of_group == 1)
		{
			const auto at = std::find(e->children.begin(), e->children.end(), first);
			index = (static_cast<int>(at - e->children.begin()) + (step >= 0 ? 1 : count - 1)) % count;
		}
		else if (step < 0)
		{
			index = count - 1;
		}

		d->sel.faces.clear();
		d->sel.objects = { e->children[static_cast<std::size_t>(index)] };
		d->selection_changed();
		return true;
	}

	// ---- clipboard / files --------------------------------------------------------------------------

	std::string document::selection_map_text(const bool world_keys) const
	{
		if (d->sel.objects.empty())
		{
			return {};
		}

		document out;
		impl& to = *out.d;
		to.eol = d->eol;
		to.begin("copy");

		if (world_keys)
		{
			to.edit_entity(to.world(), true).pairs = d->get_entity(d->world())->pairs;
			auto& layers = to.edit_layers();
			layers = d->layers;
			for (layer& l : layers) { l.hidden = false; }
		}
		else
		{
			// names of layers travel with the objects; the table they are checked against is this document's
			to.layers = d->layers;
		}

		copy_subset(*d, to, std::unordered_set<object_id>(d->sel.objects.begin(), d->sel.objects.end()), nullptr);

		if (!world_keys)
		{
			to.layers.clear();
		}

		to.end();
		return out.save_map_text();
	}

	std::string document::region_map_text(const bool walls, const std::string_view wall_material) const
	{
		std::unordered_set<object_id> ids;
		bounds content;

		for (const object_id id : all_objects(*d))
		{
			const bounds b = d->object_bounds(id);

			if (!d->region_on || (b.valid() && d->region_box.intersects(b)))
			{
				ids.insert(id);
				content.add(b);
			}
		}

		document out;
		impl& to = *out.d;
		to.eol = d->eol;
		to.begin("region");
		to.edit_entity(to.world(), true).pairs = d->get_entity(d->world())->pairs;
		{
			auto& layers = to.edit_layers();
			layers = d->layers;
			for (layer& l : layers) { l.hidden = false; }
		}
		copy_subset(*d, to, ids, nullptr);
		to.end();

		if (walls && content.valid())
		{
			// a box around everything that was written: whatever sticks out of the region is inside it too
			const double gap = 8.0, thick = 16.0;
			const vec3 lo = content.mins - vec3{ gap, gap, gap }, hi = content.maxs + vec3{ gap, gap, gap };
			const vec3 olo = lo - vec3{ thick, thick, thick }, ohi = hi + vec3{ thick, thick, thick };

			out.begin_transaction("region walls");
			for (int axis = 0; axis < 3; axis++)
			{
				vec3 a = olo, b = ohi;
				b[axis] = lo[axis];
				out.create_box(a, b, wall_material);

				a = olo;
				b = ohi;
				a[axis] = hi[axis];
				out.create_box(a, b, wall_material);
			}
			out.end_transaction();
		}

		return out.save_map_text();
	}

	bool document::paste_map_text(const std::string_view text, const vec3* move_to, const double grid, const std::string_view undo_name)
	{
		document source;
		std::string error;

		if (!source.load_map_text(std::string(text), error))
		{
			return d->fail(std::string(undo_name) + ": " + error);
		}

		transaction_scope tx(*d, undo_name);
		std::vector<object_id> created;
		copy_subset(*source.d, *d, all_objects(*source.d), &created);

		if (created.empty())
		{
			tx.cancel();
			return d->fail(std::string(undo_name) + ": the text holds no brushes or entities");
		}

		d->sel.faces.clear();
		d->set_selection(std::move(created));

		if (move_to)
		{
			const bounds box = selection_bounds();
			if (box.valid())
			{
				vec3 delta = *move_to - box.center();
				if (grid > 0.0)
				{
					delta = snap_point(delta, grid);
				}

				if (!(delta == vec3{}))
				{
					translate(delta);
				}
			}
		}

		return true;
	}

	// ---- brushes ------------------------------------------------------------------------------------

	namespace
	{
		bool set_flag(impl& d, const std::string_view word, const bool on, const bool tool, const char* name)
		{
			std::vector<object_id> ids;
			for (const object_id id : d.sel.objects)
			{
				const brush* b = d.get_brush(id);
				if ((b && !b->opaque) || d.get_patch(id))
				{
					ids.push_back(id);
				}
			}

			if (ids.empty() || word.empty())
			{
				return d.fail(std::string(name) + ": no brushes selected");
			}

			transaction_scope tx(d, name);

			for (const object_id id : ids)
			{
				const object_header* h = d.get_header(id);
				const auto& now = tool ? h->tool_flags : h->contents;

				if ((std::find(now.begin(), now.end(), word) != now.end()) == on)
				{
					continue;
				}

				object_header& edited = d.edit_header(id);
				auto& words = tool ? edited.tool_flags : edited.contents;

				if (on)
				{
					words.emplace_back(word);
				}
				else
				{
					std::erase(words, word);
				}
			}

			return true;
		}
	}

	bool document::set_content_flag(const std::string_view word, const bool on)
	{
		return set_flag(*d, word, on, false, "Contents");
	}

	bool document::set_tool_flag(const std::string_view word, const bool on)
	{
		return set_flag(*d, word, on, true, "Tool flags");
	}

	bool document::move_selection_to_entity(object_id target)
	{
		std::vector<object_id> children;
		for (const object_id id : d->sel.objects)
		{
			if (d->get_header(id))
			{
				children.push_back(id);
			}
		}

		if (target == no_object)
		{
			for (const object_id id : children)
			{
				const object_id owner = d->get_header(id)->owner;
				if (owner == d->world())
				{
					continue;
				}

				if (target != no_object && target != owner)
				{
					return d->fail("Link: the selection has brushes of more than one entity");
				}

				target = owner;
			}
		}

		const entity* e = d->get_entity(target);
		if (!e || target == d->world() || e->children.empty())
		{
			return d->fail("Link: select brushes together with a brush of the entity they should join");
		}

		std::erase_if(children, [&](const object_id id) { return d->get_header(id)->owner == target; });
		if (children.empty())
		{
			return d->fail("Link: nothing to move into the entity");
		}

		transaction_scope tx(*d, "Link entities");
		std::vector<object_id> owners;

		for (const object_id child : children)
		{
			const object_id owner = d->get_header(child)->owner;
			std::erase(d->edit_entity(owner, false).children, child);
			owners.push_back(owner);

			d->edit_header(child, true).owner = target;
			d->edit_entity(target, false).children.push_back(child);
		}

		for (const object_id owner : owners)
		{
			const entity* old = d->get_entity(owner);
			if (old && owner != d->world() && old->children.empty())
			{
				d->remove_object(owner);
			}
		}

		d->prune_selection();
		return true;
	}

	bool document::csg_merge()
	{
		std::vector<object_id> ids;
		for (const object_id id : d->sel.objects)
		{
			const brush* b = d->get_brush(id);
			if (b && !b->opaque)
			{
				ids.push_back(id);
			}
		}

		if (ids.size() < 2)
		{
			return d->fail("CSG merge: select two or more brushes");
		}

		const object_id owner = d->get_brush(ids[0])->owner;
		std::vector<vec3> points;
		std::vector<const brush_geometry*> geos;

		for (const object_id id : ids)
		{
			const brush_geometry* geo = d->geometry(id);
			if (d->get_brush(id)->owner != owner)
			{
				return d->fail("CSG merge: the brushes belong to different entities");
			}

			if (!geo || !geo->valid)
			{
				return d->fail("CSG merge: a selected brush is not a closed convex volume");
			}

			geos.push_back(geo);
			points.insert(points.end(), geo->vertices.begin(), geo->vertices.end());
		}

		std::vector<plane> hull;
		std::vector<std::vector<int>> hull_faces;
		if (!convex_hull(points, hull, hull_faces))
		{
			return d->fail("CSG merge: the brushes are flat");
		}

		// convex together = nothing of the hull is outside all of them: cut every brush out of the hull
		const double hull_volume = volume_behind(hull);
		std::vector<std::vector<plane>> pieces{ hull };

		for (const brush_geometry* geo : geos)
		{
			std::vector<std::vector<plane>> next;

			for (std::vector<plane>& piece : pieces)
			{
				for (const plane& pl : geo->planes)
				{
					if (pl.normal == vec3{})
					{
						continue;
					}

					std::vector<plane> front = piece;
					front.push_back(pl.flipped());
					if (volume_behind(front) > 1e-3)
					{
						next.push_back(std::move(front));
					}

					piece.push_back(pl);
					if (volume_behind(piece) <= 1e-3)
					{
						break;
					}
				}
			}

			pieces = std::move(next);
			if (pieces.size() > 4096)
			{
				return d->fail("CSG merge: too many brushes");
			}
		}

		double left = 0.0;
		for (const auto& piece : pieces)
		{
			left += volume_behind(piece);
		}

		if (hull_volume <= 0.0 || left > std::max(0.01, hull_volume * 1e-5))
		{
			return d->fail("CSG merge: the selected brushes are not convex together");
		}

		// faces: the one a brush has on that plane, with its points and texture; else a new one
		brush merged = *d->get_brush(ids[0]);
		const face fallback = merged.faces.empty() ? face{} : merged.faces[0];
		merged.faces.clear();
		merged.extra.clear();

		for (const plane& pl : hull)
		{
			const face* source = nullptr;

			for (std::size_t n = 0; n < ids.size() && !source; n++)
			{
				const brush* b = d->get_brush(ids[n]);
				for (std::size_t f = 0; f < b->faces.size() && f < geos[n]->planes.size(); f++)
				{
					if (!geos[n]->face_vertices[f].empty() && same_plane(geos[n]->planes[f], pl))
					{
						source = &b->faces[f];
						break;
					}
				}
			}

			if (source)
			{
				merged.faces.push_back(*source);
			}
			else
			{
				face f = fallback;
				points_for_plane(pl, f.points);
				merged.faces.push_back(std::move(f));
			}
		}

		brush_geometry check;
		if (!build_brush_geometry(merged, check))
		{
			return d->fail("CSG merge: the merged brush is not valid");
		}

		transaction_scope tx(*d, "CSG merge");

		for (const object_id id : ids)
		{
			d->remove_object(id);
		}

		const object_id made = d->add_brush(std::move(merged), owner);
		d->sel.faces.clear();
		d->prune_selection();
		d->sel.objects.push_back(made);
		d->selection_changed();
		return true;
	}

	int document::auto_caulk(const bool selected_only, const std::string_view material, const std::function<bool(std::string_view)>& opaque)
	{
		const auto hides = [&](const std::string_view name) { return opaque ? opaque(name) : default_opaque(name); };

		// what can hide a face: brushes of the worldspawn whose every face is opaque
		struct occluder
		{
			object_id id;
			const brush_geometry* geo;
		};

		std::vector<occluder> occluders;
		const entity* world = d->get_entity(d->world());

		for (const object_id id : world->children)
		{
			const brush* b = d->get_brush(id);
			const brush_geometry* geo = b && !b->opaque ? d->geometry(id) : nullptr;

			if (geo && geo->valid && std::all_of(b->faces.begin(), b->faces.end(), [&](const face& f) { return hides(f.texture.material); }))
			{
				occluders.push_back({ id, geo });
			}
		}

		std::vector<object_id> targets;
		if (selected_only)
		{
			for (const object_id id : d->sel.objects)
			{
				if (const brush* b = d->get_brush(id); b && !b->opaque)
				{
					targets.push_back(id);
				}
			}
		}
		else
		{
			// the world only: a brush entity may be a door or a lift, and what it covers now can be seen later
			for (const object_id id : world->children)
			{
				if (const brush* b = d->get_brush(id); b && !b->opaque)
				{
					targets.push_back(id);
				}
			}
		}

		if (targets.empty())
		{
			d->fail(selected_only ? "Auto caulk: no brushes selected" : "Auto caulk: the map has no brushes");
			return -1;
		}

		std::vector<std::pair<object_id, int>> hidden_faces;

		for (const object_id id : targets)
		{
			const brush* b = d->get_brush(id);
			const brush_geometry* geo = d->geometry(id);
			if (!geo || !geo->valid)
			{
				continue;
			}

			for (std::size_t f = 0; f < b->faces.size() && f < geo->planes.size(); f++)
			{
				if (geo->face_vertices[f].size() < 3 || is_tool_material(b->faces[f].texture.material))
				{
					continue;
				}

				const plane& face_plane = geo->planes[f];
				std::vector<winding> pieces{ geo->face_winding(static_cast<int>(f)) };
				const double area = winding_area(pieces[0]);
				bounds face_box;
				for (const vec3& p : pieces[0]) { face_box.add(p); }

				for (const occluder& o : occluders)
				{
					if (pieces.empty())
					{
						break;
					}

					if (o.id == id || !o.geo->box.intersects(face_box, ON_EPSILON))
					{
						continue;
					}

					// a brush with a face in the same plane looking the same way lies beside this one, not in front of it
					if (std::any_of(o.geo->planes.begin(), o.geo->planes.end(), [&](const plane& pl) { return same_plane(pl, face_plane); }))
					{
						continue;
					}

					std::vector<winding> next;

					for (winding& piece : pieces)
					{
						// what is in front of a plane of the occluder stays visible; the rest goes on to the next plane
						for (const plane& pl : o.geo->planes)
						{
							if (pl.normal == vec3{})
							{
								continue;
							}

							double lo = 1e30, hi = -1e30;
							for (const vec3& p : piece)
							{
								const double dist = pl.distance_to(p);
								lo = std::min(lo, dist);
								hi = std::max(hi, dist);
							}

							if (hi <= ON_EPSILON)
							{
								continue;	// behind the plane or on it ( a face that touches the occluder )
							}

							if (lo >= -ON_EPSILON)
							{
								next.push_back(std::move(piece));	// all of it outside
								piece.clear();
								break;
							}

							winding front = piece;
							clip_winding(front, pl.flipped(), ON_EPSILON);
							if (front.size() >= 3 && winding_area(front) > 1e-3)
							{
								next.push_back(std::move(front));
							}

							clip_winding(piece, pl, ON_EPSILON);
							if (piece.size() < 3)
							{
								break;
							}
						}
					}

					pieces = std::move(next);
				}

				double left = 0.0;
				for (const winding& piece : pieces)
				{
					left += winding_area(piece);
				}

				if (left <= std::max(1e-3, area * 1e-4))
				{
					hidden_faces.emplace_back(id, static_cast<int>(f));
				}
			}
		}

		if (hidden_faces.empty())
		{
			d->fail("Auto caulk: no hidden faces found");
			return 0;
		}

		transaction_scope tx(*d, "Auto caulk");
		int changed = 0;

		for (const auto& [id, f] : hidden_faces)
		{
			tex_def& tex = d->edit_brush(id).faces[static_cast<std::size_t>(f)].texture;
			if (tex.material != material)
			{
				tex.material = std::string(material);
				changed++;
			}
		}

		return changed;
	}

	// ---- entities -----------------------------------------------------------------------------------

	int document::drop_to_floor(const bool orient)
	{
		std::vector<object_id> ids;
		for (const object_id id : d->sel.objects)
		{
			if (d->get_entity(id))
			{
				ids.push_back(id);
			}
		}

		if (ids.empty())
		{
			d->fail("Drop to floor: no point entities selected");
			return 0;
		}

		pick_options options;
		options.entities = false;

		const auto cast = [&](const vec3& from, pick_result& hit)
		{
			// from a little above: something that stands on the floor already finds it
			return pick(ray{ from + vec3{ 0.0, 0.0, 1.0 }, { 0.0, 0.0, -1.0 } }, hit, options);
		};

		struct move
		{
			object_id id;
			vec3 origin;
			bool turn = false;
			vec3 angles;
		};

		std::vector<move> moves;

		for (const object_id id : ids)
		{
			const entity* e = d->get_entity(id);
			const vec3 origin = e->origin();
			pick_result hit;

			if (!cast(origin, hit))
			{
				continue;
			}

			move m;
			m.id = id;
			m.origin = origin;
			m.origin.z = hit.point.z;

			if (!is_xmodel(e->value("model")))
			{
				// the bottom of the box on the floor
				m.origin.z -= std::min(0.0, d->entity_local_box(*e).mins.z);
			}

			if (orient)
			{
				vec3 normal{ 0.0, 0.0, 1.0 };

				if (const brush_geometry* geo = d->geometry(hit.object); geo && hit.face >= 0 && hit.face < static_cast<int>(geo->planes.size()))
				{
					normal = geo->planes[static_cast<std::size_t>(hit.face)].normal;
				}
				else
				{
					// a patch: the slope between three casts
					pick_result hx, hy;
					if (cast(origin + vec3{ 2.0, 0.0, 0.0 }, hx) && cast(origin + vec3{ 0.0, 2.0, 0.0 }, hy))
					{
						normal = normalize(cross(hx.point - hit.point, hy.point - hit.point));
					}
				}

				if (normal.z > 0.1)
				{
					const double yaw = e->angles().y * PI / 180.0;
					const vec3 ahead{ std::cos(yaw), std::sin(yaw), 0.0 };
					vec3 axes[3];
					axes[2] = normal;
					axes[1] = normalize(cross(normal, ahead));	// left
					axes[0] = cross(axes[1], normal);			// forward, on the surface
					m.angles = axes_to_angles(axes);

					for (int i = 0; i < 3; i++)
					{
						m.angles[i] = std::round(m.angles[i] * 1e3) / 1e3;
						if (m.angles[i] >= 360.0) { m.angles[i] = 0.0; }
					}

					m.turn = true;
				}
			}

			if (!(m.origin == origin) || m.turn)
			{
				moves.push_back(m);
			}
		}

		if (moves.empty())
		{
			d->fail("Drop to floor: nothing below the selected entities ( or they stand on it already )");
			return 0;
		}

		transaction_scope tx(*d, "Drop to floor");

		for (const move& m : moves)
		{
			entity& e = d->edit_entity(m.id, true);
			set_pair(e, "origin", format_key_vec3(m.origin));

			if (m.turn)
			{
				set_pair(e, "angles", format_key_vec3(m.angles));
				std::erase_if(e.pairs, [](const auto& pair) { return pair.first == "angle"; });
			}
		}

		return static_cast<int>(moves.size());
	}

	int document::replace_key_value(const std::string_view key, const std::string_view from, const std::string_view to, const bool selected_only)
	{
		std::vector<object_id> ids;

		if (selected_only)
		{
			ids = selected_entities();
		}
		else
		{
			ids.assign(d->order.begin(), d->order.end());
		}

		std::erase_if(ids, [&](const object_id id)
		{
			const entity* e = d->get_entity(id);
			const std::string* value = e ? e->find(key) : nullptr;
			return !value || *value == to || !wildcard_match(from, *value);
		});

		if (ids.empty() || key.empty())
		{
			d->fail("Replace: no entity has \"" + std::string(key) + "\" \"" + std::string(from) + "\"");
			return 0;
		}

		transaction_scope tx(*d, "Replace " + std::string(key));
		for (const object_id id : ids)
		{
			set_pair(d->edit_entity(id, true), key, to);
		}

		return static_cast<int>(ids.size());
	}

	map_info document::info() const
	{
		map_info out;
		std::map<std::string, std::size_t> classes, materials;

		for (const object_id ent : d->order)
		{
			const entity* e = d->get_entity(ent);
			out.entities++;
			classes[std::string(e->classname())]++;

			if (ent != d->world())
			{
				(e->children.empty() ? out.point_entities : out.brush_entities)++;
				out.models += is_xmodel(e->value("model")) && !is_prefab(*e) ? 1 : 0;
				out.lights += starts(e->classname(), "light") ? 1 : 0;
				out.prefabs += is_prefab(*e) ? 1 : 0;

				if (e->children.empty())
				{
					out.box.add(e->origin());
				}
			}

			for (const object_id child : e->children)
			{
				if (const brush* b = d->get_brush(child))
				{
					out.brushes++;
					out.world_brushes += ent == d->world() ? 1 : 0;
					out.detail_brushes += b->is_detail() ? 1 : 0;
					out.faces += b->faces.size();

					for (const face& f : b->faces)
					{
						materials[f.texture.material]++;
					}
				}
				else if (const patch* p = d->get_patch(child))
				{
					out.patches++;
					(p->type == patch_type::curve ? out.curves : out.terrain)++;
					materials[p->material]++;
				}

				out.box.add(d->object_bounds(child));
			}
		}

		out.classes.assign(classes.begin(), classes.end());
		out.materials.assign(materials.begin(), materials.end());
		std::stable_sort(out.materials.begin(), out.materials.end(), [](const auto& a, const auto& b) { return a.second > b.second; });
		return out;
	}

	object_id document::find_by_number(const int entity_number, const int brush_number) const
	{
		if (entity_number < 0 || static_cast<std::size_t>(entity_number) >= d->order.size())
		{
			return no_object;
		}

		const object_id ent = d->order[static_cast<std::size_t>(entity_number)];
		if (brush_number < 0)
		{
			return ent;
		}

		const entity* e = d->get_entity(ent);
		return static_cast<std::size_t>(brush_number) < e->children.size() ? e->children[static_cast<std::size_t>(brush_number)] : no_object;
	}
}
