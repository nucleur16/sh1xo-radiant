// document: editing operations. Each public operation is one transaction ( = one undo step, or part of the
// transaction the caller has open ). Operations compute and validate their result before they touch the document,
// so a failing operation leaves nothing behind.

#include "doc_impl.hpp"

namespace codol::edit
{
	namespace
	{
		using impl = document::impl;

		// ---- small helpers --------------------------------------------------------------------------

		tex_def lightmap_default()
		{
			tex_def t;
			t.material = "lightmap_gray";
			t.size_x = t.size_y = 16384.0;
			return t;
		}

		// a new face of that material at radiant's default scale ( 0.25: a 256 texture repeats every 64 units )
		tex_def texture_default(const impl& d, const std::string_view material)
		{
			tex_def t;
			t.material = std::string(material.empty() ? std::string_view(d.default_material) : material);

			int w, h;
			d.texture_dims(t.material, w, h);
			t.size_x = w * 0.25;
			t.size_y = h * 0.25;
			return t;
		}

		face face_from_plane(const plane& pl, const tex_def& tex, const tex_def& lightmap)
		{
			face f;
			points_for_plane(pl, f.points);
			f.texture = tex;
			f.lightmap = lightmap;
			return f;
		}

		// drops the faces that have no winding; false when what is left is not a valid brush
		bool finish_faces(std::vector<face>& faces)
		{
			brush tmp;
			tmp.faces = std::move(faces);

			brush_geometry geo;
			build_brush_geometry(tmp, geo);

			std::vector<face> kept;
			for (std::size_t i = 0; i < tmp.faces.size(); i++)
			{
				if (!geo.face_vertices[i].empty())
				{
					kept.push_back(std::move(tmp.faces[i]));
				}
			}

			faces = std::move(kept);
			return geo.valid;
		}

		bool faces_valid(const std::vector<face>& faces)
		{
			brush tmp;
			tmp.faces = faces;
			brush_geometry geo;
			return build_brush_geometry(tmp, geo);
		}

		// layer for new objects: the active layer, or none when that is the root ( first ) layer
		std::string new_object_layer(const impl& d)
		{
			for (std::size_t i = 0; i < d.layers.size(); i++)
			{
				if (d.layers[i].active)
				{
					return i == 0 ? std::string() : d.layers[i].name;
				}
			}

			return {};
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

		bool editable_brush(const impl& d, const object_id id)
		{
			const brush* b = d.get_brush(id);
			return b && !b->opaque;
		}

		double tidy(const double v)
		{
			const double r = std::round(v * 1e6) / 1e6;
			return std::fabs(r - v) < 1e-9 ? r : v;
		}

		// ---- texture lock ---------------------------------------------------------------------------

		// New alignment for a face that is about to be transformed by `t`, such that every point of the surface
		// keeps its texture coordinate. Shear can not be expressed by the format and is dropped.
		void lock_texture(tex_def& tex, const face& before, const transform& t, const int tw, const int th)
		{
			plane old_plane;
			if (!plane_from_points(before.points[0], before.points[1], before.points[2], old_plane))
			{
				return;
			}

			vec3 u0, v0;
			face_texture_axes(old_plane.normal, tex.rotation, u0, v0);

			const double sx = tex.size_x != 0.0 ? tex.size_x : 1.0;
			const double sy = tex.size_y != 0.0 ? tex.size_y : 1.0;

			if (t.is_translation())
			{
				tex.shift_x = tidy(std::fmod(tex.shift_x - dot(t.t, u0) / sx * tw, static_cast<double>(tw)));
				tex.shift_y = tidy(std::fmod(tex.shift_y - dot(t.t, v0) / sy * th, static_cast<double>(th)));
				return;
			}

			vec3 moved[3];
			double uv[3][2];

			for (int i = 0; i < 3; i++)
			{
				uv[i][0] = dot(before.points[i], u0) / sx + tex.shift_x / tw;
				uv[i][1] = dot(before.points[i], v0) / sy + tex.shift_y / th;
				moved[i] = t.apply(before.points[i]);
			}

			plane new_plane;
			const bool mirrored = t.determinant() < 0.0;
			if (!plane_from_points(moved[0], mirrored ? moved[2] : moved[1], mirrored ? moved[1] : moved[2], new_plane))
			{
				return;
			}

			vec3 ub, vb;
			face_texture_axes(new_plane.normal, 0.0, ub, vb);

			double xy[3][2];
			for (int i = 0; i < 3; i++)
			{
				xy[i][0] = dot(moved[i], ub);
				xy[i][1] = dot(moved[i], vb);
			}

			// uv = N * xy + c
			const double m00 = xy[1][0] - xy[0][0], m01 = xy[2][0] - xy[0][0];
			const double m10 = xy[1][1] - xy[0][1], m11 = xy[2][1] - xy[0][1];
			const double det = m00 * m11 - m01 * m10;

			if (std::fabs(det) < 1e-12)
			{
				return;
			}

			const double i00 = m11 / det, i01 = -m01 / det, i10 = -m10 / det, i11 = m00 / det;

			double n[2][2];
			for (int k = 0; k < 2; k++)
			{
				const double d1 = uv[1][k] - uv[0][k], d2 = uv[2][k] - uv[0][k];
				n[k][0] = d1 * i00 + d2 * i10;
				n[k][1] = d1 * i01 + d2 * i11;
			}

			// gradients of u and v in the 2d frame the rotation works in
			const int sv = ub.x != 0.0 ? 0 : (ub.y != 0.0 ? 1 : 2);
			const int tv = vb.x != 0.0 ? 0 : (vb.y != 0.0 ? 1 : 2);
			const double su = ub[sv], sw = vb[tv];	// +-1

			const double gux = n[0][0] * su, guy = n[0][1] * sw;	// u gradient in ( sv, tv )
			const double gvx = n[1][0] * su, gvy = n[1][1] * sw;

			const double gu_len = std::sqrt(gux * gux + guy * guy);
			if (gu_len < 1e-12)
			{
				return;
			}

			// rotated u axis = R( r ) * ( su, 0 ) = ( c * su, s * su )
			const double r = std::atan2(guy * su, gux * su);
			const double c = std::cos(r), s = std::sin(r);

			// rotated v axis = R( r ) * ( 0, sw ) = ( -s * sw, c * sw )
			const double gv_along = gvx * (-s * sw) + gvy * (c * sw);
			if (std::fabs(gv_along) < 1e-12)
			{
				return;
			}

			double rotation = r * 180.0 / PI;
			rotation = std::fmod(rotation, 360.0);
			if (rotation < 0.0)
			{
				rotation += 360.0;
			}

			rotation = tidy(rotation);
			if (rotation == 360.0)
			{
				rotation = 0.0;
			}

			tex.rotation = rotation;
			tex.size_x = tidy(1.0 / gu_len);
			tex.size_y = tidy(1.0 / gv_along);

			const double cu = uv[0][0] - (n[0][0] * xy[0][0] + n[0][1] * xy[0][1]);
			const double cv = uv[0][1] - (n[1][0] * xy[0][0] + n[1][1] * xy[0][1]);
			tex.shift_x = tidy(std::fmod(cu * tw, static_cast<double>(tw)));
			tex.shift_y = tidy(std::fmod(cv * th, static_cast<double>(th)));
		}

		// ---- transforms -----------------------------------------------------------------------------

		// the two layers of a face before it is transformed: the material with the lock of moves / rotations, the
		// lightmap with Texture Lock > Lightmaps ( off: its projection stays in the world )
		void lock_layers(const impl& d, face& f, const transform& t, const bool lock_material)
		{
			int w, h;
			if (lock_material)
			{
				d.texture_dims(f.texture.material, w, h);
				lock_texture(f.texture, f, t, w, h);
			}

			if (d.texture_lock_lightmap)
			{
				d.texture_dims(f.lightmap.material, w, h);
				lock_texture(f.lightmap, f, t, w, h);
			}
		}

		void transform_faces(const impl& d, std::vector<face>& faces, const transform& t, const bool lock)
		{
			const bool mirrored = t.determinant() < 0.0;

			for (face& f : faces)
			{
				lock_layers(d, f, t, lock);

				for (vec3& p : f.points)
				{
					p = t.apply(p);
				}

				if (mirrored)
				{
					std::swap(f.points[1], f.points[2]);
				}
			}
		}

		// the lightmap coordinates of a patch from where its vertices are, as radiant makes them: along the axis the patch
		// faces most, one luxel per lightmap sample size ( the third number of the size line; 16 units without one )
		void world_lightmap(patch& p)
		{
			if (p.width < 2 || p.height < 2 || p.vertices.size() < static_cast<std::size_t>(p.width) * p.height)
			{
				return;
			}

			const vec3 a = p.at(p.width - 1, p.height - 1).xyz - p.at(0, 0).xyz;
			const vec3 b = p.at(0, p.height - 1).xyz - p.at(p.width - 1, 0).xyz;
			const vec3 n = cross(a, b);
			vec3 u, v;
			face_texture_axes(length(n) > 1e-9 ? normalize(n) : vec3{ 0, 0, 1 }, 0.0, u, v);
			const double per_unit = 1.0 / (p.has_lightmap_sample_size && p.lightmap_sample_size > 0.0 ? p.lightmap_sample_size : 16.0);

			for (auto& vertex : p.vertices)
			{
				// only coordinates that are there: a patch without lightmap coordinates stays without
				if (vertex.tex.size() >= 4)
				{
					vertex.tex[2] = tidy(dot(vertex.xyz, u) * per_unit);
					vertex.tex[3] = tidy(dot(vertex.xyz, v) * per_unit);
				}
			}
		}

		void transform_patch(const impl& d, patch& p, const transform& t)
		{
			for (auto& v : p.vertices)
			{
				v.xyz = t.apply(v.xyz);
			}

			if (t.determinant() < 0.0 && p.width > 1)
			{
				// keep the surface facing the same way: reverse the columns
				for (int c = 0; c < p.width / 2; c++)
				{
					for (int r = 0; r < p.height; r++)
					{
						std::swap(p.at(c, r), p.at(p.width - 1 - c, r));
					}
				}
			}

			// the lightmap coordinates of the vertices went with them ( the lock ); without it they are made again
			// from the world
			if (!d.texture_lock_lightmap && !(t.is_translation() && length(t.t) == 0.0))
			{
				world_lightmap(p);
			}
		}

		void transform_entity_keys(entity& e, const transform& t, const bool force_origin)
		{
			if (force_origin || e.find("origin"))
			{
				set_pair(e, "origin", format_key_vec3(t.apply(e.origin())));
			}

			if (t.is_translation() || !t.is_rotation(1e-6))
			{
				return;
			}

			vec3 axes[3];
			angles_to_axes(e.angles(), axes);

			for (vec3& axis : axes)
			{
				axis = t.apply_vector(axis);
			}

			vec3 angles = axes_to_angles(axes);
			for (int i = 0; i < 3; i++)
			{
				angles[i] = std::round(angles[i] * 1e4) / 1e4;
				if (angles[i] >= 360.0)
				{
					angles[i] = 0.0;
				}
			}

			if (!e.find("angles") && e.find("angle") && angles.x == 0.0 && angles.z == 0.0)
			{
				set_pair(e, "angle", format_key_number(angles.y));
			}
			else
			{
				set_pair(e, "angles", format_key_vec3(angles));
				std::erase_if(e.pairs, [](const auto& pair) { return pair.first == "angle"; });
			}
		}

		// one object; brush entities are handled by the caller
		void transform_object(impl& d, const object_id id, const transform& t)
		{
			switch (d.kind(id))
			{
			case object_kind::brush:
				if (editable_brush(d, id))
				{
					// moves and everything else ( rotations, scales, flips ) have a lock each
					transform_faces(d, d.edit_brush(id).faces, t, t.is_translation() ? d.texture_lock : d.texture_lock_rotations);
				}
				break;

			case object_kind::patch:
				transform_patch(d, d.edit_patch(id), t);
				break;

			case object_kind::entity:
				transform_entity_keys(d.edit_entity(id, true), t, true);
				break;

			default:
				break;
			}
		}

		// brush entities all of whose children are in `ids`
		std::vector<object_id> complete_entities(const impl& d, const std::vector<object_id>& ids)
		{
			const std::unordered_set<object_id> set(ids.begin(), ids.end());
			std::vector<object_id> out;
			std::unordered_set<object_id> seen;

			for (const object_id id : ids)
			{
				const object_id owner = d.owner_of(id);
				if (owner == id || owner == d.world() || !seen.insert(owner).second)
				{
					continue;
				}

				const entity* e = d.get_entity(owner);
				if (e && std::all_of(e->children.begin(), e->children.end(), [&](const object_id c) { return set.contains(c); }))
				{
					out.push_back(owner);
				}
			}

			return out;
		}

		// ---- brush building -------------------------------------------------------------------------

		bool faces_from_hull(const std::span<const vec3> points, const tex_def& tex, std::vector<face>& faces,
			const std::vector<face>* reuse = nullptr)
		{
			std::vector<plane> planes;
			std::vector<std::vector<int>> hull;

			if (!convex_hull(points, planes, hull))
			{
				return false;
			}

			// planes of the faces that may be reused
			std::vector<plane> old_planes;
			if (reuse)
			{
				old_planes.resize(reuse->size());
				for (std::size_t i = 0; i < reuse->size(); i++)
				{
					plane_from_points((*reuse)[i].points[0], (*reuse)[i].points[1], (*reuse)[i].points[2], old_planes[i]);
				}
			}

			faces.clear();

			for (std::size_t i = 0; i < planes.size(); i++)
			{
				const auto& on = hull[i];
				if (on.size() < 3)
				{
					continue;
				}

				int same = -1, closest = -1;
				double best = -2.0;

				for (std::size_t k = 0; k < old_planes.size(); k++)
				{
					const double dn = dot(old_planes[k].normal, planes[i].normal);
					if (dn > 1.0 - 1e-7 && std::fabs(old_planes[k].dist - planes[i].dist) < ON_EPSILON)
					{
						same = static_cast<int>(k);
						break;
					}

					if (dn > best)
					{
						best = dn;
						closest = static_cast<int>(k);
					}
				}

				if (same >= 0)
				{
					faces.push_back((*reuse)[same]);
					continue;
				}

				// p0, p2 neighbours on the outline, p1 the corner that spans the largest triangle with them
				const vec3& q0 = points[on[0]];
				const vec3& q1 = points[on[1]];
				int far = 2;
				double area = -1.0;

				for (std::size_t k = 2; k < on.size(); k++)
				{
					const double a = length(cross(q1 - q0, points[on[k]] - q0));
					if (a > area)
					{
						area = a;
						far = static_cast<int>(k);
					}
				}

				face f;
				f.points[0] = q0;
				f.points[1] = points[on[far]];
				f.points[2] = q1;

				if (closest >= 0)
				{
					f.texture = (*reuse)[closest].texture;
					f.lightmap = (*reuse)[closest].lightmap;
					f.trailing = (*reuse)[closest].trailing;
				}
				else
				{
					f.texture = tex;
					f.lightmap = lightmap_default();
				}

				faces.push_back(std::move(f));
			}

			return finish_faces(faces);
		}

		object_id add_world_brush(impl& d, std::vector<face> faces)
		{
			brush b;
			b.faces = std::move(faces);
			b.layer = new_object_layer(d);

			const object_id id = d.add_brush(std::move(b), d.world());
			d.sel.faces.clear();
			d.set_selection({ id });
			return id;
		}

		object_id create_from_points(impl& d, const std::span<const vec3> points, const std::string_view material, const std::string_view name)
		{
			std::vector<face> faces;
			if (!faces_from_hull(points, texture_default(d, material), faces))
			{
				d.fail(std::string(name) + ": the shape has no volume");
				return no_object;
			}

			transaction_scope tx(d, name);
			return add_world_brush(d, std::move(faces));
		}

		bool box_ok(impl& d, const vec3& mins, const vec3& maxs, const std::string_view what)
		{
			for (int i = 0; i < 3; i++)
			{
				if (!(maxs[i] - mins[i] > ON_EPSILON * 2))
				{
					return d.fail(std::string(what) + ": the box has no volume");
				}

				if (std::fabs(mins[i]) > WORLD_EXTENT || std::fabs(maxs[i]) > WORLD_EXTENT)
				{
					return d.fail(std::string(what) + ": outside the world");
				}
			}

			return true;
		}

		// ---- splitting ------------------------------------------------------------------------------

		enum class side { front, back, both };

		side classify(const std::vector<face>& faces, const plane& pl)
		{
			brush tmp;
			tmp.faces = faces;
			brush_geometry geo;
			build_brush_geometry(tmp, geo);

			double lo = 1e300, hi = -1e300;
			for (const vec3& v : geo.vertices)
			{
				const double dist = pl.distance_to(v);
				lo = std::min(lo, dist);
				hi = std::max(hi, dist);
			}

			if (hi <= ON_EPSILON)
			{
				return side::back;
			}

			if (lo >= -ON_EPSILON)
			{
				return side::front;
			}

			return side::both;
		}

		// front / back are left empty when that part does not exist
		void split_faces(const std::vector<face>& faces, const plane& pl, const face& cut_back, std::vector<face>& front, std::vector<face>& back)
		{
			front.clear();
			back.clear();

			switch (classify(faces, pl))
			{
			case side::back:
				back = faces;
				return;

			case side::front:
				front = faces;
				return;

			default:
				break;
			}

			back = faces;
			back.push_back(cut_back);
			if (!finish_faces(back))
			{
				back.clear();
			}

			front = faces;
			face cut_front = cut_back;
			std::swap(cut_front.points[1], cut_front.points[2]);
			front.push_back(std::move(cut_front));
			if (!finish_faces(front))
			{
				front.clear();
			}

			// a sliver on one side: the whole brush is the other part
			if (front.empty() && !back.empty())
			{
				back = faces;
			}
			else if (back.empty() && !front.empty())
			{
				front = faces;
			}
		}

		object_id add_sibling(impl& d, const brush& like, std::vector<face> faces)
		{
			brush b;
			b.layer = like.layer;
			b.contents = like.contents;
			b.tool_flags = like.tool_flags;
			b.faces = std::move(faces);
			return d.add_brush(std::move(b), like.owner);
		}

		// ---- texture targets ------------------------------------------------------------------------

		std::vector<face_ref> target_faces(const impl& d)
		{
			std::vector<face_ref> out = d.sel.faces;

			for (const object_id id : d.sel.objects)
			{
				const brush* b = d.get_brush(id);
				if (!b || b->opaque)
				{
					continue;
				}

				for (int f = 0; f < static_cast<int>(b->faces.size()); f++)
				{
					const face_ref ref{ id, f };
					if (std::find(d.sel.faces.begin(), d.sel.faces.end(), ref) == d.sel.faces.end())
					{
						out.push_back(ref);
					}
				}
			}

			std::erase_if(out, [&](const face_ref& f) { return !editable_brush(d, f.brush) || f.face >= static_cast<int>(d.get_brush(f.brush)->faces.size()); });
			return out;
		}

		template <typename Fn>
		bool for_target_faces(impl& d, const std::string_view name, Fn&& fn)
		{
			const auto targets = target_faces(d);
			if (targets.empty())
			{
				return d.fail(std::string(name) + ": no faces selected");
			}

			transaction_scope tx(d, name);
			for (const face_ref& ref : targets)
			{
				fn(d.edit_brush(ref.brush).faces[ref.face], ref);
			}

			return true;
		}

		// ---- layers ---------------------------------------------------------------------------------

		void set_layer_active(layer& l, const bool active)
		{
			std::string words;
			std::size_t pos = 0;

			while (pos < l.flags.size())
			{
				while (pos < l.flags.size() && l.flags[pos] == ' ')
				{
					pos++;
				}

				std::size_t end = pos;
				while (end < l.flags.size() && l.flags[end] != ' ')
				{
					end++;
				}

				if (end > pos && l.flags.compare(pos, end - pos, "active") != 0)
				{
					if (!words.empty())
					{
						words += ' ';
					}

					words.append(l.flags, pos, end - pos);
				}

				pos = end;
			}

			l.active = active;
			l.flags = " " + words + (active ? " active" : "");
		}

		bool layer_exists(const impl& d, const std::string_view name)
		{
			return std::any_of(d.layers.begin(), d.layers.end(), [&](const layer& l) { return l.name == name; });
		}

		// calls fn( current layer ) -> new layer or nullopt for every entity, brush and patch
		template <typename Fn>
		void relayer_objects(impl& d, Fn&& fn)
		{
			for (std::size_t id = 1; id < d.slots.size(); id++)
			{
				const auto oid = static_cast<object_id>(id);

				if (const entity* e = d.get_entity(oid))
				{
					if (const std::optional<std::string> to = fn(std::string_view(e->layer)))
					{
						d.edit_entity(oid, true).layer = *to;
					}
				}
				else if (const object_header* h = d.get_header(oid))
				{
					if (d.kind(oid) == object_kind::brush && d.get_brush(oid)->opaque)
					{
						continue;
					}

					if (const std::optional<std::string> to = fn(std::string_view(h->layer)))
					{
						d.edit_header(oid).layer = *to;
					}
				}
			}
		}
	}

	// ---- creating -----------------------------------------------------------------------------------

	object_id document::create_box(const vec3& mins, const vec3& maxs, const std::string_view material)
	{
		if (!box_ok(*d, mins, maxs, "Create box"))
		{
			return no_object;
		}

		vec3 points[8];
		for (int i = 0; i < 8; i++)
		{
			points[i] = { (i & 1) ? maxs.x : mins.x, (i & 2) ? maxs.y : mins.y, (i & 4) ? maxs.z : mins.z };
		}

		return create_from_points(*d, points, material, "Create box");
	}

	object_id document::create_cylinder(const vec3& mins, const vec3& maxs, const int sides, const int axis, const std::string_view material)
	{
		if (!box_ok(*d, mins, maxs, "Create cylinder") || sides < 3 || sides > 64 || axis < 0 || axis > 2)
		{
			if (sides < 3 || sides > 64)
			{
				d->fail("Create cylinder: 3 to 64 sides");
			}

			return no_object;
		}

		const int a = (axis + 1) % 3, b = (axis + 2) % 3;
		const vec3 center = (mins + maxs) * 0.5;
		const double ra = (maxs[a] - mins[a]) * 0.5, rb = (maxs[b] - mins[b]) * 0.5;

		std::vector<vec3> points;
		for (int i = 0; i < sides; i++)
		{
			const double angle = (i + 0.5) * 2.0 * PI / sides;
			vec3 p = center;
			p[a] += tidy(std::cos(angle) * ra);
			p[b] += tidy(std::sin(angle) * rb);

			p[axis] = mins[axis];
			points.push_back(p);
			p[axis] = maxs[axis];
			points.push_back(p);
		}

		return create_from_points(*d, points, material, "Create cylinder");
	}

	object_id document::create_cone(const vec3& mins, const vec3& maxs, const int sides, const int axis, const std::string_view material)
	{
		if (!box_ok(*d, mins, maxs, "Create cone") || sides < 3 || sides > 64 || axis < 0 || axis > 2)
		{
			if (sides < 3 || sides > 64)
			{
				d->fail("Create cone: 3 to 64 sides");
			}

			return no_object;
		}

		const int a = (axis + 1) % 3, b = (axis + 2) % 3;
		const vec3 center = (mins + maxs) * 0.5;
		const double ra = (maxs[a] - mins[a]) * 0.5, rb = (maxs[b] - mins[b]) * 0.5;

		std::vector<vec3> points;
		for (int i = 0; i < sides; i++)
		{
			const double angle = (i + 0.5) * 2.0 * PI / sides;
			vec3 p = center;
			p[a] += tidy(std::cos(angle) * ra);
			p[b] += tidy(std::sin(angle) * rb);
			p[axis] = mins[axis];
			points.push_back(p);
		}

		vec3 apex = center;
		apex[axis] = maxs[axis];
		points.push_back(apex);

		return create_from_points(*d, points, material, "Create cone");
	}

	object_id document::create_wedge(const vec3& mins, const vec3& maxs, const int slope_axis, const std::string_view material)
	{
		if (!box_ok(*d, mins, maxs, "Create wedge") || slope_axis < 0 || slope_axis > 1)
		{
			return no_object;
		}

		const int other = 1 - slope_axis;
		std::vector<vec3> points;

		for (int i = 0; i < 2; i++)
		{
			vec3 p;
			p[other] = i ? maxs[other] : mins[other];

			p[slope_axis] = mins[slope_axis]; p.z = mins.z; points.push_back(p);
			p[slope_axis] = maxs[slope_axis]; p.z = mins.z; points.push_back(p);
			p[slope_axis] = mins[slope_axis]; p.z = maxs.z; points.push_back(p);
		}

		return create_from_points(*d, points, material, "Create wedge");
	}

	object_id document::create_brush_from_points(const std::span<const vec3> points, const std::string_view material)
	{
		if (points.size() < 4 || points.size() > 128)
		{
			d->fail("Create brush: 4 to 128 points");
			return no_object;
		}

		for (const vec3& p : points)
		{
			if (std::fabs(p.x) > WORLD_EXTENT || std::fabs(p.y) > WORLD_EXTENT || std::fabs(p.z) > WORLD_EXTENT)
			{
				d->fail("Create brush: outside the world");
				return no_object;
			}
		}

		return create_from_points(*d, points, material, "Create brush");
	}

	object_id document::create_terrain_patch(const vec3& mins, const vec3& maxs, const int columns, const int rows, const int axis, const std::string_view material)
	{
		if (columns < 2 || rows < 2 || columns > 16 || rows > 16 || axis < 0 || axis > 2)
		{
			d->fail("Create terrain: 2 to 16 columns and rows");
			return no_object;
		}

		const int a = axis == 0 ? 1 : 0;
		const int b = axis == 2 ? 1 : 2;

		if (!(maxs[a] - mins[a] > ON_EPSILON) || !(maxs[b] - mins[b] > ON_EPSILON))
		{
			d->fail("Create terrain: the rectangle has no area");
			return no_object;
		}

		patch p;
		p.type = patch_type::mesh;
		p.material = std::string(material.empty() ? std::string_view(d->default_material) : material);
		p.lightmap = "lightmap_gray";
		p.width = columns;
		p.height = rows;
		p.layer = new_object_layer(*d);

		for (int c = 0; c < columns; c++)
		{
			for (int r = 0; r < rows; r++)
			{
				const double da = (maxs[a] - mins[a]) * c / (columns - 1);
				const double db = (maxs[b] - mins[b]) * r / (rows - 1);

				patch_vertex v;
				v.xyz[axis] = mins[axis];
				v.xyz[a] = mins[a] + da;
				v.xyz[b] = mins[b] + db;
				v.tex = { da * 4.0, db * 4.0, da / 16.0, db / 16.0 };	// texels at scale 0.25, luxels at 16 units
				p.vertices.push_back(std::move(v));
			}
		}

		transaction_scope tx(*d, "Create terrain");
		const object_id id = d->add_patch(std::move(p), d->world());
		d->sel.faces.clear();
		d->set_selection({ id });
		return id;
	}

	// ---- delete / clone -----------------------------------------------------------------------------

	bool document::delete_selection()
	{
		const std::vector<object_id> ids = d->sel.objects;
		if (ids.empty())
		{
			return d->fail("Delete: nothing selected");
		}

		transaction_scope tx(*d, "Delete");
		std::vector<object_id> owners;

		for (const object_id id : ids)
		{
			const object_id owner = d->owner_of(id);
			if (owner != id && owner != d->world())
			{
				owners.push_back(owner);
			}

			if (id != d->world())
			{
				d->remove_object(id);
			}
		}

		for (const object_id owner : owners)
		{
			const entity* e = d->get_entity(owner);
			if (e && e->children.empty())
			{
				d->remove_object(owner);
			}
		}

		d->sel.objects.clear();
		d->sel.faces.clear();
		d->selection_changed();
		return true;
	}

	bool document::clone_selection(const vec3& offset)
	{
		const std::vector<object_id> ids = d->sel.objects;
		if (ids.empty())
		{
			return d->fail("Clone: nothing selected");
		}

		transaction_scope tx(*d, "Clone");

		const std::vector<object_id> whole = complete_entities(*d, ids);
		std::unordered_map<object_id, object_id> entity_copy;	// brush entity -> its clone
		std::vector<object_id> created;
		const transform move = transform::translation(offset);
		const bool moved = !(offset == vec3{});

		for (const object_id owner : whole)
		{
			entity copy = *d->get_entity(owner);
			const object_id id = d->add_entity(std::move(copy));
			entity_copy.emplace(owner, id);

			if (moved && d->get_entity(id)->find("origin"))
			{
				transform_entity_keys(d->edit_entity(id, true), move, false);
			}
		}

		for (const object_id id : ids)
		{
			object_id made = no_object;

			if (const entity* e = d->get_entity(id))
			{
				made = d->add_entity(*e);
			}
			else
			{
				const object_header* h = d->get_header(id);
				const auto it = entity_copy.find(h->owner);
				const object_id owner = it != entity_copy.end() ? it->second : h->owner;

				if (const brush* b = d->get_brush(id))
				{
					if (b->opaque)
					{
						continue;
					}

					made = d->add_brush(*b, owner);
				}
				else
				{
					made = d->add_patch(*d->get_patch(id), owner);
				}
			}

			if (moved)
			{
				transform_object(*d, made, move);
			}

			created.push_back(made);
		}

		d->sel.faces.clear();
		d->set_selection(std::move(created));
		return true;
	}

	// ---- transforms ---------------------------------------------------------------------------------

	bool document::transform_selection(const transform& t, const std::string_view undo_name)
	{
		const std::vector<object_id> ids = d->sel.objects;
		if (ids.empty())
		{
			return d->fail(std::string(undo_name) + ": nothing selected");
		}

		if (std::fabs(t.determinant()) < 1e-12)
		{
			return d->fail(std::string(undo_name) + ": the transform is singular");
		}

		transaction_scope tx(*d, undo_name);

		for (const object_id id : ids)
		{
			transform_object(*d, id, t);
		}

		// a brush entity that moves as a whole takes its origin along
		for (const object_id owner : complete_entities(*d, ids))
		{
			if (d->get_entity(owner)->find("origin"))
			{
				transform_entity_keys(d->edit_entity(owner, true), t, false);
			}
		}

		return true;
	}

	bool document::translate(const vec3& delta)
	{
		return transform_selection(transform::translation(delta), "Move");
	}

	bool document::rotate(const vec3& origin, const vec3& axis, const double degrees)
	{
		if (length(axis) < 1e-12)
		{
			return d->fail("Rotate: no axis");
		}

		return transform_selection(transform::rotation(origin, axis, degrees), "Rotate");
	}

	bool document::scale(const vec3& origin, const vec3& factors)
	{
		return transform_selection(transform::scaling(origin, factors), "Scale");
	}

	bool document::flip(const vec3& origin, const int axis)
	{
		if (axis < 0 || axis > 2)
		{
			return d->fail("Flip: bad axis");
		}

		return transform_selection(transform::flip(origin, axis), "Flip");
	}

	bool document::snap_to_grid(const double grid)
	{
		const std::vector<object_id> ids = d->sel.objects;
		if (ids.empty() || !(grid > 0.0))
		{
			return d->fail("Snap: nothing selected");
		}

		transaction_scope tx(*d, "Snap to grid");

		for (const object_id id : ids)
		{
			if (const brush* b = d->get_brush(id))
			{
				if (b->opaque)
				{
					continue;
				}

				std::vector<face> faces = b->faces;
				bool changed = false;

				for (face& f : faces)
				{
					for (vec3& p : f.points)
					{
						const vec3 snapped = snap_point(p, grid);
						changed |= !(snapped == p);
						p = snapped;
					}
				}

				// a brush that would collapse stays as it is
				if (changed && faces_valid(faces))
				{
					d->edit_brush(id).faces = std::move(faces);
				}
			}
			else if (const patch* p = d->get_patch(id))
			{
				const bool changed = std::any_of(p->vertices.begin(), p->vertices.end(), [&](const patch_vertex& v) { return !(snap_point(v.xyz, grid) == v.xyz); });
				if (changed)
				{
					for (auto& v : d->edit_patch(id).vertices)
					{
						v.xyz = snap_point(v.xyz, grid);
					}
				}
			}
			else if (const entity* e = d->get_entity(id))
			{
				const vec3 snapped = snap_point(e->origin(), grid);
				if (!(snapped == e->origin()) || !e->find("origin"))
				{
					set_pair(d->edit_entity(id, true), "origin", format_key_vec3(snapped));
				}
			}
		}

		return true;
	}

	// ---- clip / csg ---------------------------------------------------------------------------------

	bool document::clip(const vec3& a, const vec3& b, const vec3& c, const clip_keep keep, const std::string_view material)
	{
		plane pl;
		if (!plane_from_points(a, b, c, pl))
		{
			return d->fail("Clip: the three points are on one line");
		}

		std::vector<object_id> ids;
		for (const object_id id : d->sel.objects)
		{
			if (editable_brush(*d, id))
			{
				ids.push_back(id);
			}
		}

		if (ids.empty())
		{
			return d->fail("Clip: no brushes selected");
		}

		transaction_scope tx(*d, "Clip");
		std::vector<object_id> selection = d->sel.objects;
		std::vector<object_id> emptied;

		for (const object_id id : ids)
		{
			const brush source = *d->get_brush(id);

			face cut;
			cut.points[0] = a;
			cut.points[1] = b;
			cut.points[2] = c;
			cut.texture = material.empty() ? source.faces.front().texture : texture_default(*d, material);
			cut.lightmap = material.empty() ? source.faces.front().lightmap : lightmap_default();

			std::vector<face> front, back;
			split_faces(source.faces, pl, cut, front, back);

			const bool want_front = keep != clip_keep::back;
			const bool want_back = keep != clip_keep::front;

			std::vector<face>* first = (want_back && !back.empty()) ? &back : ((want_front && !front.empty()) ? &front : nullptr);
			std::vector<face>* second = (first == &back && want_front && !front.empty()) ? &front : nullptr;

			if (!first)
			{
				if (source.owner != d->world())
				{
					emptied.push_back(source.owner);
				}

				d->remove_object(id);
				std::erase(selection, id);
				continue;
			}

			const bool unchanged = !second && first->size() == source.faces.size()
				&& std::equal(first->begin(), first->end(), source.faces.begin(), [](const face& x, const face& y)
					{ return x.points[0] == y.points[0] && x.points[1] == y.points[1] && x.points[2] == y.points[2]; });

			if (!unchanged)
			{
				d->edit_brush(id).faces = std::move(*first);
			}

			if (second)
			{
				selection.push_back(add_sibling(*d, source, std::move(*second)));
			}
		}

		for (const object_id owner : emptied)
		{
			const entity* e = d->get_entity(owner);
			if (e && e->children.empty())
			{
				d->remove_object(owner);
			}
		}

		d->set_selection(std::move(selection));
		d->prune_selection();
		return true;
	}

	bool document::clip(const plane& p, const clip_keep keep, const std::string_view material)
	{
		if (length(p.normal) < 1e-9)
		{
			return d->fail("Clip: no plane");
		}

		plane unit = p;
		const double len = length(p.normal);
		unit.normal = p.normal * (1.0 / len);
		unit.dist = p.dist / len;

		vec3 points[3];
		points_for_plane(unit, points);
		return clip(points[0], points[1], points[2], keep, material);
	}

	bool document::csg_subtract()
	{
		std::vector<object_id> cutters;
		for (const object_id id : d->sel.objects)
		{
			if (editable_brush(*d, id) && d->geometry(id)->valid)
			{
				cutters.push_back(id);
			}
		}

		if (cutters.empty())
		{
			return d->fail("CSG subtract: no brushes selected");
		}

		// every visible brush that is not selected and touches a cutter
		std::vector<object_id> targets;
		for (const object_id ent : d->order)
		{
			for (const object_id child : d->get_entity(ent)->children)
			{
				if (!editable_brush(*d, child) || d->is_selected(child) || !d->is_visible(child) || !d->geometry(child)->valid)
				{
					continue;
				}

				const bounds& box = d->geometry(child)->box;
				if (std::any_of(cutters.begin(), cutters.end(), [&](const object_id c) { return d->geometry(c)->box.intersects(box, -ON_EPSILON); }))
				{
					targets.push_back(child);
				}
			}
		}

		transaction_scope tx(*d, "CSG subtract");
		bool any = false;

		for (const object_id target : targets)
		{
			const brush source = *d->get_brush(target);
			std::vector<std::vector<face>> pieces{ source.faces };
			bool cut = false;

			for (const object_id cutter : cutters)
			{
				const brush* cb = d->get_brush(cutter);
				const brush_geometry* cg = d->geometry(cutter);
				std::vector<std::vector<face>> next;

				for (auto& piece : pieces)
				{
					std::vector<std::vector<face>> outside;
					std::vector<face> rest = piece;
					bool intersects = true;

					for (std::size_t f = 0; f < cb->faces.size() && intersects; f++)
					{
						if (cg->face_vertices[f].empty())
						{
							continue;
						}

						face cut_face = cb->faces[f];
						cut_face.texture = source.faces.front().texture;
						cut_face.lightmap = source.faces.front().lightmap;
						cut_face.trailing.clear();

						std::vector<face> front, back;
						split_faces(rest, cg->planes[f], cut_face, front, back);

						if (!front.empty())
						{
							outside.push_back(std::move(front));
						}

						if (back.empty())
						{
							intersects = false;		// the rest is outside the cutter: nothing to remove from this piece
						}
						else
						{
							rest = std::move(back);
						}
					}

					if (!intersects)
					{
						next.push_back(std::move(piece));
						continue;
					}

					cut = true;
					for (auto& o : outside)
					{
						next.push_back(std::move(o));
					}
				}

				pieces = std::move(next);
			}

			if (!cut)
			{
				continue;
			}

			any = true;

			if (pieces.empty())
			{
				d->remove_object(target);

				const entity* owner = d->get_entity(source.owner);
				if (owner && source.owner != d->world() && owner->children.empty())
				{
					d->remove_object(source.owner);
				}

				continue;
			}

			d->edit_brush(target).faces = std::move(pieces[0]);
			for (std::size_t i = 1; i < pieces.size(); i++)
			{
				add_sibling(*d, source, std::move(pieces[i]));
			}
		}

		if (!any)
		{
			return d->fail("CSG subtract: the selection does not intersect another brush");
		}

		return true;
	}

	bool document::csg_hollow(const double thickness)
	{
		if (!(thickness > ON_EPSILON))
		{
			return d->fail("Hollow: no wall thickness");
		}

		std::vector<object_id> ids;
		for (const object_id id : d->sel.objects)
		{
			if (editable_brush(*d, id) && d->geometry(id)->valid)
			{
				ids.push_back(id);
			}
		}

		if (ids.empty())
		{
			return d->fail("Hollow: no brushes selected");
		}

		transaction_scope tx(*d, "Hollow");
		std::vector<object_id> selection = d->sel.objects;
		bool any = false;

		for (const object_id id : ids)
		{
			const brush source = *d->get_brush(id);
			const brush_geometry geo = *d->geometry(id);
			std::vector<std::vector<face>> walls;

			for (std::size_t f = 0; f < source.faces.size(); f++)
			{
				if (geo.face_vertices[f].empty())
				{
					continue;
				}

				// the slab between the face and a parallel plane `thickness` inside
				plane inner = geo.planes[f];
				inner.dist -= thickness;

				if (classify(source.faces, inner) != side::both)
				{
					continue;	// the wall would be the whole brush
				}

				std::vector<face> wall = source.faces;
				wall.push_back(face_from_plane(inner.flipped(), source.faces[f].texture, source.faces[f].lightmap));

				if (finish_faces(wall))
				{
					walls.push_back(std::move(wall));
				}
			}

			if (walls.size() < 2)
			{
				continue;
			}

			any = true;
			d->edit_brush(id).faces = std::move(walls[0]);

			for (std::size_t i = 1; i < walls.size(); i++)
			{
				selection.push_back(add_sibling(*d, source, std::move(walls[i])));
			}
		}

		if (!any)
		{
			return d->fail("Hollow: the walls are thicker than the brush");
		}

		d->set_selection(std::move(selection));
		return true;
	}

	bool document::set_contents(const std::span<const std::string> words)
	{
		std::vector<object_id> ids;
		for (const object_id id : d->sel.objects)
		{
			if (editable_brush(*d, id) || d->get_patch(id))
			{
				ids.push_back(id);
			}
		}

		if (ids.empty())
		{
			return d->fail("Contents: no brushes selected");
		}

		transaction_scope tx(*d, "Contents");
		for (const object_id id : ids)
		{
			const object_header* h = d->get_header(id);
			if (!std::equal(h->contents.begin(), h->contents.end(), words.begin(), words.end()))
			{
				d->edit_header(id).contents.assign(words.begin(), words.end());
			}
		}

		return true;
	}

	bool document::set_detail(const bool detail)
	{
		std::vector<object_id> ids;
		for (const object_id id : d->sel.objects)
		{
			if (editable_brush(*d, id) || d->get_patch(id))
			{
				ids.push_back(id);
			}
		}

		if (ids.empty())
		{
			return d->fail("Detail: no brushes selected");
		}

		transaction_scope tx(*d, detail ? "Make detail" : "Make structural");
		for (const object_id id : ids)
		{
			if (d->get_header(id)->is_detail() == detail)
			{
				continue;
			}

			auto& contents = d->edit_header(id).contents;
			if (detail)
			{
				contents.insert(contents.begin(), "detail");
			}
			else
			{
				std::erase(contents, "detail");
			}
		}

		return true;
	}

	// ---- brush shape --------------------------------------------------------------------------------

	bool document::move_face(const face_ref f, const double distance)
	{
		if (!editable_brush(*d, f.brush) || f.face < 0 || f.face >= static_cast<int>(d->get_brush(f.brush)->faces.size()))
		{
			return d->fail("Move face: no such face");
		}

		const brush_geometry* geo = d->geometry(f.brush);
		const vec3 normal = geo->planes[f.face].normal;

		if (length(normal) < 0.5)
		{
			return d->fail("Move face: the face is degenerate");
		}

		std::vector<face> faces = d->get_brush(f.brush)->faces;
		face& target = faces[f.face];
		const transform move = transform::translation(normal * distance);

		lock_layers(*d, target, move, d->texture_lock);

		for (vec3& p : target.points)
		{
			p = move.apply(p);
		}

		brush tmp;
		tmp.faces = faces;
		brush_geometry moved;

		if (!build_brush_geometry(tmp, moved) || moved.face_vertices[f.face].empty())
		{
			return d->fail("Move face: the brush would vanish");
		}

		transaction_scope tx(*d, "Move face");
		d->edit_brush(f.brush).faces = std::move(faces);
		return true;
	}

	bool document::resize_selection(const vec3& direction)
	{
		const double len = length(direction);
		if (len < 1e-12)
		{
			return d->fail("Resize: no direction");
		}

		const vec3 dir = direction * (1.0 / len);
		const transform move = transform::translation(direction);

		transaction_scope tx(*d, "Resize");
		bool any = false;

		for (const object_id id : std::vector<object_id>(d->sel.objects))
		{
			if (!editable_brush(*d, id))
			{
				continue;
			}

			const brush_geometry* geo = d->geometry(id);
			std::vector<face> faces = d->get_brush(id)->faces;
			bool changed = false;

			for (std::size_t f = 0; f < faces.size(); f++)
			{
				if (geo->face_vertices[f].empty() || dot(geo->planes[f].normal, dir) < 0.7)
				{
					continue;
				}

				lock_layers(*d, faces[f], move, d->texture_lock);

				for (vec3& p : faces[f].points)
				{
					p = move.apply(p);
				}

				changed = true;
			}

			if (changed && faces_valid(faces))
			{
				d->edit_brush(id).faces = std::move(faces);
				any = true;
			}
		}

		if (!any)
		{
			return d->fail("Resize: no face of the selection points that way, or the brush would vanish");
		}

		return true;
	}

	bool document::move_brush_corners(const object_id brush_id, const std::span<const vec3> corners, const vec3& delta)
	{
		if (!editable_brush(*d, brush_id))
		{
			return d->fail("Move corners: not a brush");
		}

		const brush* b = d->get_brush(brush_id);
		const brush_geometry* geo = d->geometry(brush_id);

		std::vector<vec3> points = geo->vertices;
		bool any = false;

		for (vec3& p : points)
		{
			for (const vec3& c : corners)
			{
				if (length(p - c) < 0.05)
				{
					p += delta;
					any = true;
					break;
				}
			}
		}

		if (!any)
		{
			return d->fail("Move corners: none of the positions is a corner of the brush");
		}

		if (points.size() > 128)
		{
			return d->fail("Move corners: the brush has too many corners");
		}

		for (const vec3& p : points)
		{
			if (std::fabs(p.x) > WORLD_EXTENT || std::fabs(p.y) > WORLD_EXTENT || std::fabs(p.z) > WORLD_EXTENT)
			{
				return d->fail("Move corners: outside the world");
			}
		}

		std::vector<face> faces;
		if (!faces_from_hull(points, b->faces.front().texture, faces, &b->faces))
		{
			return d->fail("Move corners: the brush would be flat");
		}

		transaction_scope tx(*d, "Move corners");
		d->edit_brush(brush_id).faces = std::move(faces);
		d->prune_selection();
		return true;
	}

	bool document::move_patch_vertices(const object_id patch_id, const std::span<const int> vertex_indices, const vec3& delta)
	{
		const patch* p = d->get_patch(patch_id);
		if (!p || vertex_indices.empty())
		{
			return d->fail("Move vertices: not a patch");
		}

		for (const int index : vertex_indices)
		{
			if (index < 0 || index >= static_cast<int>(p->vertices.size()))
			{
				return d->fail("Move vertices: bad vertex index");
			}
		}

		transaction_scope tx(*d, "Move vertices");
		patch& edit = d->edit_patch(patch_id);

		for (const int index : vertex_indices)
		{
			edit.vertices[index].xyz += delta;
		}

		return true;
	}

	// ---- textures -----------------------------------------------------------------------------------

	bool document::set_material(const std::string_view material)
	{
		if (material.empty() || material.find_first_of(" \t\r\n\"") != std::string_view::npos)
		{
			return d->fail("Set material: bad material name");
		}

		const auto targets = target_faces(*d);
		std::vector<object_id> patches;

		for (const object_id id : d->sel.objects)
		{
			if (d->get_patch(id))
			{
				patches.push_back(id);
			}
		}

		if (targets.empty() && patches.empty())
		{
			return d->fail("Set material: nothing selected");
		}

		transaction_scope tx(*d, "Set material");

		for (const face_ref& ref : targets)
		{
			if (d->get_brush(ref.brush)->faces[ref.face].texture.material != material)
			{
				d->edit_brush(ref.brush).faces[ref.face].texture.material = std::string(material);
			}
		}

		for (const object_id id : patches)
		{
			if (d->get_patch(id)->material != material)
			{
				d->edit_patch(id).material = std::string(material);
			}
		}

		return true;
	}

	// shift_texture, scale_texture, rotate_texture, set_texture, fit_texture and the other texture tools: texture_ops.cpp

	// ---- entities -----------------------------------------------------------------------------------

	namespace
	{
		bool valid_key_text(const std::string_view text)
		{
			return text.find_first_of("\"\r\n") == std::string_view::npos;
		}
	}

	object_id document::create_point_entity(const std::string_view classname, const vec3& origin,
		const std::span<const std::pair<std::string, std::string>> pairs)
	{
		if (classname.empty() || !valid_key_text(classname))
		{
			d->fail("Create entity: no classname");
			return no_object;
		}

		entity e;
		e.pairs.emplace_back("classname", std::string(classname));
		e.pairs.emplace_back("origin", format_key_vec3(origin));

		for (const auto& [k, v] : pairs)
		{
			if (k.empty() || !valid_key_text(k) || !valid_key_text(v))
			{
				d->fail("Create entity: bad key or value");
				return no_object;
			}

			set_pair(e, k, v);
		}

		transaction_scope tx(*d, "Create entity");
		const object_id id = d->add_entity(std::move(e));
		d->sel.faces.clear();
		d->set_selection({ id });
		return id;
	}

	object_id document::create_brush_entity(const std::string_view classname, const std::span<const std::pair<std::string, std::string>> pairs)
	{
		if (classname.empty() || !valid_key_text(classname))
		{
			d->fail("Create entity: no classname");
			return no_object;
		}

		std::vector<object_id> children;
		for (const object_id id : d->sel.objects)
		{
			if (d->get_header(id))
			{
				children.push_back(id);
			}
		}

		if (children.empty())
		{
			d->fail("Create entity: no brushes selected");
			return no_object;
		}

		entity e;
		e.pairs.emplace_back("classname", std::string(classname));

		for (const auto& [k, v] : pairs)
		{
			if (k.empty() || !valid_key_text(k) || !valid_key_text(v))
			{
				d->fail("Create entity: bad key or value");
				return no_object;
			}

			set_pair(e, k, v);
		}

		transaction_scope tx(*d, "Create entity");
		const object_id id = d->add_entity(std::move(e));
		std::vector<object_id> owners;

		for (const object_id child : children)
		{
			const object_id owner = d->get_header(child)->owner;
			std::erase(d->edit_entity(owner, false).children, child);
			owners.push_back(owner);

			d->edit_header(child, true).owner = id;
			d->edit_entity(id, false).children.push_back(child);
		}

		for (const object_id owner : owners)
		{
			const entity* old = d->get_entity(owner);
			if (old && owner != d->world() && old->children.empty())
			{
				d->remove_object(owner);
			}
		}

		d->set_selection(std::move(children));
		return id;
	}

	bool document::move_selection_to_worldspawn()
	{
		std::vector<object_id> children;
		for (const object_id id : d->sel.objects)
		{
			const object_header* h = d->get_header(id);
			if (h && h->owner != d->world())
			{
				children.push_back(id);
			}
		}

		if (children.empty())
		{
			return d->fail("Ungroup: no brushes of an entity selected");
		}

		transaction_scope tx(*d, "Move to worldspawn");
		std::vector<object_id> owners;

		for (const object_id child : children)
		{
			const object_id owner = d->get_header(child)->owner;
			std::erase(d->edit_entity(owner, false).children, child);
			owners.push_back(owner);

			d->edit_header(child, true).owner = d->world();
			d->edit_entity(d->world(), false).children.push_back(child);
		}

		for (const object_id owner : owners)
		{
			const entity* old = d->get_entity(owner);
			if (old && old->children.empty())
			{
				d->remove_object(owner);
			}
		}

		d->prune_selection();
		return true;
	}

	bool document::set_key(const object_id entity_id, const std::string_view key, const std::string_view value)
	{
		const entity* e = d->get_entity(entity_id);
		if (!e || key.empty() || !valid_key_text(key) || !valid_key_text(value))
		{
			return d->fail("Set key: bad entity, key or value");
		}

		if (const auto* have = e->find(key); have && *have == value)
		{
			return true;
		}

		transaction_scope tx(*d, "Set key");
		set_pair(d->edit_entity(entity_id, true), key, value);
		return true;
	}

	bool document::remove_key(const object_id entity_id, const std::string_view key)
	{
		const entity* e = d->get_entity(entity_id);
		if (!e || !e->find(key))
		{
			return d->fail("Remove key: the entity has no such key");
		}

		if (key == "classname")
		{
			return d->fail("Remove key: an entity needs its classname");
		}

		transaction_scope tx(*d, "Remove key");
		std::erase_if(d->edit_entity(entity_id, true).pairs, [&](const auto& pair) { return pair.first == key; });
		return true;
	}

	bool document::rename_key(const object_id entity_id, const std::string_view key, const std::string_view new_key)
	{
		const entity* e = d->get_entity(entity_id);
		if (!e || !e->find(key) || new_key.empty() || !valid_key_text(new_key))
		{
			return d->fail("Rename key: the entity has no such key");
		}

		if (key == new_key)
		{
			return true;
		}

		if (e->find(new_key))
		{
			return d->fail("Rename key: the entity already has that key");
		}

		if (key == "classname")
		{
			return d->fail("Rename key: an entity needs its classname");
		}

		transaction_scope tx(*d, "Rename key");
		for (auto& [k, v] : d->edit_entity(entity_id, true).pairs)
		{
			if (k == key)
			{
				k = std::string(new_key);
			}
		}

		return true;
	}

	bool document::connect_entities(const object_id from, const object_id to)
	{
		const entity* a = d->get_entity(from);
		const entity* b = d->get_entity(to);

		if (!a || !b || from == to)
		{
			return d->fail("Connect: two different entities are needed");
		}

		if (from == d->world() || to == d->world())
		{
			return d->fail("Connect: the worldspawn can not be connected");
		}

		std::string name;

		if (const auto* targetname = b->find("targetname"); targetname && !targetname->empty())
		{
			name = *targetname;
		}
		else if (const auto* target = a->find("target"); target && !target->empty())
		{
			name = *target;
		}
		else
		{
			std::unordered_set<std::string_view> used;
			for (const object_id id : d->order)
			{
				const entity* e = d->get_entity(id);
				used.insert(e->value("targetname"));
				used.insert(e->value("target"));
			}

			for (int n = 1;; n++)
			{
				name = "auto" + std::to_string(n);
				if (!used.contains(name))
				{
					break;
				}
			}
		}

		transaction_scope tx(*d, "Connect entities");

		if (a->value("target") != name)
		{
			set_pair(d->edit_entity(from, true), "target", name);
		}

		if (d->get_entity(to)->value("targetname") != name)
		{
			set_pair(d->edit_entity(to, true), "targetname", name);
		}

		return true;
	}

	std::vector<object_id> document::targets_of(const object_id entity_id) const
	{
		std::vector<object_id> out;
		const entity* e = d->get_entity(entity_id);
		const std::string_view target = e ? e->value("target") : std::string_view();

		if (target.empty())
		{
			return out;
		}

		for (const object_id id : d->order)
		{
			if (d->get_entity(id)->value("targetname") == target)
			{
				out.push_back(id);
			}
		}

		return out;
	}

	std::vector<object_id> document::targeting(const object_id entity_id) const
	{
		std::vector<object_id> out;
		const entity* e = d->get_entity(entity_id);
		const std::string_view name = e ? e->value("targetname") : std::string_view();

		if (name.empty())
		{
			return out;
		}

		for (const object_id id : d->order)
		{
			if (d->get_entity(id)->value("target") == name)
			{
				out.push_back(id);
			}
		}

		return out;
	}

	// ---- layers -------------------------------------------------------------------------------------

	bool document::add_layer(const std::string_view name)
	{
		if (name.empty() || !valid_key_text(name) || name.front() == '/' || name.back() == '/')
		{
			return d->fail("Add layer: bad name");
		}

		if (layer_exists(*d, name))
		{
			return d->fail("Add layer: the layer exists");
		}

		transaction_scope tx(*d, "Add layer");

		// parents first
		for (std::size_t slash = name.find('/'); slash != std::string_view::npos; slash = name.find('/', slash + 1))
		{
			const auto parent = name.substr(0, slash);
			if (!layer_exists(*d, parent))
			{
				layer l;
				l.name = std::string(parent);
				l.flags = " ";
				d->edit_layers().push_back(std::move(l));
			}
		}

		layer l;
		l.name = std::string(name);
		l.flags = " ";
		d->edit_layers().push_back(std::move(l));
		return true;
	}

	bool document::remove_layer(const std::string_view name)
	{
		if (!layer_exists(*d, name))
		{
			return d->fail("Remove layer: no such layer");
		}

		const std::string removed(name);
		const std::string prefix = removed + "/";
		const auto slash = removed.rfind('/');
		const std::string parent = slash == std::string::npos ? std::string() : removed.substr(0, slash);

		transaction_scope tx(*d, "Remove layer");

		relayer_objects(*d, [&](const std::string_view current) -> std::optional<std::string>
		{
			if (current == removed || current.starts_with(prefix))
			{
				return parent;
			}

			return std::nullopt;
		});

		auto& layers = d->edit_layers();
		const bool was_active = std::any_of(layers.begin(), layers.end(), [&](const layer& l) { return l.active && (l.name == removed || l.name.starts_with(prefix)); });
		std::erase_if(layers, [&](const layer& l) { return l.name == removed || l.name.starts_with(prefix); });

		if (was_active && !layers.empty())
		{
			const auto it = std::find_if(layers.begin(), layers.end(), [&](const layer& l) { return l.name == parent; });
			set_layer_active(it != layers.end() ? *it : layers.front(), true);
		}

		return true;
	}

	bool document::rename_layer(const std::string_view name, const std::string_view new_name)
	{
		if (!layer_exists(*d, name) || new_name.empty() || !valid_key_text(new_name))
		{
			return d->fail("Rename layer: no such layer, or a bad name");
		}

		if (name == new_name)
		{
			return true;
		}

		if (layer_exists(*d, new_name))
		{
			return d->fail("Rename layer: the name is taken");
		}

		const std::string from(name), to(new_name);
		const std::string prefix = from + "/";

		const auto renamed = [&](const std::string_view current) -> std::optional<std::string>
		{
			if (current == from)
			{
				return to;
			}

			if (current.starts_with(prefix))
			{
				return to + "/" + std::string(current.substr(prefix.size()));
			}

			return std::nullopt;
		};

		transaction_scope tx(*d, "Rename layer");
		relayer_objects(*d, renamed);

		for (layer& l : d->edit_layers())
		{
			if (const auto n = renamed(l.name))
			{
				l.name = *n;
			}
		}

		return true;
	}

	bool document::set_active_layer(const std::string_view name)
	{
		if (!layer_exists(*d, name))
		{
			return d->fail("Set active layer: no such layer");
		}

		if (active_layer() == name)
		{
			return true;
		}

		transaction_scope tx(*d, "Set active layer");
		for (layer& l : d->edit_layers())
		{
			if (l.active != (l.name == name))
			{
				set_layer_active(l, l.name == name);
			}
		}

		return true;
	}

	bool document::move_selection_to_layer(const std::string_view name)
	{
		if (!name.empty() && !layer_exists(*d, name))
		{
			return d->fail("Move to layer: no such layer");
		}

		if (d->sel.objects.empty())
		{
			return d->fail("Move to layer: nothing selected");
		}

		transaction_scope tx(*d, "Move to layer");

		for (const object_id id : std::vector<object_id>(d->sel.objects))
		{
			if (const entity* e = d->get_entity(id))
			{
				if (e->layer != name)
				{
					d->edit_entity(id, true).layer = std::string(name);
				}
			}
			else if (const object_header* h = d->get_header(id))
			{
				if (h->layer != name && (d->kind(id) != object_kind::brush || !d->get_brush(id)->opaque))
				{
					d->edit_header(id).layer = std::string(name);
				}
			}
		}

		return true;
	}
}
