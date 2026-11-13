// codol_edit: texture operations - the alignment of brush faces and the texture coordinates of patches, on the
// material layer or the lightmap layer; flips, the natural alignment, find / replace, materials in use.

#include "doc_impl.hpp"

#include <cmath>
#include <map>

namespace codol::edit
{
	using impl = document::impl;

	namespace
	{
		double tidy(const double v)
		{
			const double r = std::round(v * 1e6) / 1e6;
			return std::fabs(r - v) < 1e-9 ? r : v;
		}

		bool editable_brush(const impl& d, const object_id id)
		{
			const brush* b = d.get_brush(id);
			return b && !b->opaque;
		}

		// the selected faces, plus every face of the selected brushes
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

			std::erase_if(out, [&](const face_ref& f) { return !editable_brush(d, f.brush) || f.face < 0 || f.face >= static_cast<int>(d.get_brush(f.brush)->faces.size()); });
			return out;
		}

		std::vector<object_id> target_patches(const impl& d)
		{
			std::vector<object_id> out;

			for (const object_id id : d.sel.objects)
			{
				if (d.get_patch(id))
				{
					out.push_back(id);
				}
			}

			return out;
		}

		tex_def& layer_tex(const impl& d, face& f)
		{
			return d.edit_layer == 1 ? f.lightmap : f.texture;
		}

		// the two numbers of a patch vertex that are the coordinates of the edited layer ( added when the vertex has
		// fewer: a patch without lightmap coordinates gets them once the lightmap layer is edited )
		double* layer_uv(const impl& d, patch_vertex& v)
		{
			const std::size_t first = d.edit_layer == 1 ? 2 : 0;
			if (v.tex.size() < first + 2)
			{
				v.tex.resize(first + 2, 0.0);
			}

			return v.tex.data() + first;
		}

		// faces and patches of the selection: `face_fn( tex_def&, face_ref )`, `patch_fn( patch& )`
		template <typename FaceFn, typename PatchFn>
		bool for_targets(impl& d, const std::string_view name, FaceFn&& face_fn, PatchFn&& patch_fn)
		{
			const auto faces = target_faces(d);
			const auto patches = target_patches(d);

			if (faces.empty() && patches.empty())
			{
				return d.fail(std::string(name) + ": no faces selected");
			}

			transaction_scope tx(d, name);

			for (const face_ref& ref : faces)
			{
				face_fn(layer_tex(d, d.edit_brush(ref.brush).faces[ref.face]), ref);
			}

			for (const object_id id : patches)
			{
				patch_fn(d.edit_patch(id));
			}

			return true;
		}

		struct uv_box
		{
			double lo[2] = { 1e300, 1e300 }, hi[2] = { -1e300, -1e300 };

			[[nodiscard]] double mid(const int i) const { return (lo[i] + hi[i]) * 0.5; }
		};

		uv_box uv_bounds(const impl& d, patch& p)
		{
			uv_box box;

			for (patch_vertex& v : p.vertices)
			{
				const double* uv = layer_uv(d, v);
				for (int i = 0; i < 2; i++)
				{
					box.lo[i] = std::min(box.lo[i], uv[i]);
					box.hi[i] = std::max(box.hi[i], uv[i]);
				}
			}

			return box;
		}

		// a material name without the "wc/" in front: the browser lists world materials with it, .map files of
		// radiant name them without
		std::string_view bare(const std::string_view name)
		{
			return name.rfind("wc/", 0) == 0 ? name.substr(3) : name;
		}

		bool same_material(const std::string_view a, const std::string_view b)
		{
			return bare(a) == bare(b);
		}

		bool valid_material(const std::string_view material)
		{
			return !material.empty() && material.find_first_of(" \t\r\n\"") == std::string_view::npos;
		}

		// the normal a patch has on the whole: across its two diagonals
		vec3 patch_normal(const patch& p)
		{
			if (p.width < 2 || p.height < 2 || p.vertices.size() < static_cast<std::size_t>(p.width) * p.height)
			{
				return { 0, 0, 1 };
			}

			const vec3 a = p.at(p.width - 1, p.height - 1).xyz - p.at(0, 0).xyz;
			const vec3 b = p.at(0, p.height - 1).xyz - p.at(p.width - 1, 0).xyz;
			const vec3 n = cross(a, b);
			return length(n) > 1e-9 ? normalize(n) : vec3{ 0, 0, 1 };
		}
	}

	// ---- settings -----------------------------------------------------------------------------------

	void document::set_texture_lock(const bool moves, const bool rotations)
	{
		d->texture_lock = moves;
		d->texture_lock_rotations = rotations;
	}

	bool document::texture_lock_moves() const { return d->texture_lock; }
	bool document::texture_lock_rotations() const { return d->texture_lock_rotations; }
	void document::set_texture_lock_lightmap(const bool on) { d->texture_lock_lightmap = on; }
	bool document::texture_lock_lightmap() const { return d->texture_lock_lightmap; }
	void document::set_edit_layer(const texture_layer layer) { d->edit_layer = layer == texture_layer::lightmap ? 1 : 0; }
	document::texture_layer document::edit_layer() const { return d->edit_layer == 1 ? texture_layer::lightmap : texture_layer::material; }

	// ---- alignment ----------------------------------------------------------------------------------

	bool document::shift_texture(const double dx, const double dy)
	{
		return for_targets(*d, "Shift texture", [&](tex_def& t, const face_ref&)
		{
			t.shift_x += dx;
			t.shift_y += dy;
		}, [&](patch& p)
		{
			for (patch_vertex& v : p.vertices)
			{
				double* uv = layer_uv(*d, v);
				uv[0] += dx;
				uv[1] += dy;
			}
		});
	}

	bool document::scale_texture(const double fx, const double fy)
	{
		if (fx == 0.0 || fy == 0.0)
		{
			return d->fail("Scale texture: factor 0");
		}

		return for_targets(*d, "Scale texture", [&](tex_def& t, const face_ref&)
		{
			t.size_x *= fx;
			t.size_y *= fy;
		}, [&](patch& p)
		{
			// a texture twice as large has half the coordinates; about the middle, so that it stays in place
			const uv_box box = uv_bounds(*d, p);

			for (patch_vertex& v : p.vertices)
			{
				double* uv = layer_uv(*d, v);
				uv[0] = tidy(box.mid(0) + (uv[0] - box.mid(0)) / fx);
				uv[1] = tidy(box.mid(1) + (uv[1] - box.mid(1)) / fy);
			}
		});
	}

	bool document::rotate_texture(const double degrees)
	{
		const double radians = degrees * PI / 180.0;
		const double c = std::cos(radians), s = std::sin(radians);

		return for_targets(*d, "Rotate texture", [&](tex_def& t, const face_ref&)
		{
			double r = std::fmod(t.rotation + degrees, 360.0);
			if (r < 0.0)
			{
				r += 360.0;
			}

			t.rotation = r;
		}, [&](patch& p)
		{
			const uv_box box = uv_bounds(*d, p);

			for (patch_vertex& v : p.vertices)
			{
				double* uv = layer_uv(*d, v);
				const double x = uv[0] - box.mid(0), y = uv[1] - box.mid(1);
				uv[0] = tidy(box.mid(0) + x * c - y * s);
				uv[1] = tidy(box.mid(1) + x * s + y * c);
			}
		});
	}

	bool document::set_texture(const tex_def& t)
	{
		if (t.size_x == 0.0 || t.size_y == 0.0)
		{
			return d->fail("Set texture: size 0");
		}

		const auto faces = target_faces(*d);
		if (faces.empty())
		{
			return d->fail("Set texture: no faces selected");
		}

		transaction_scope tx(*d, "Set texture");

		for (const face_ref& ref : faces)
		{
			tex_def& target = layer_tex(*d, d->edit_brush(ref.brush).faces[ref.face]);
			const std::string material = t.material.empty() ? target.material : t.material;
			target = t;
			target.material = material;
		}

		return true;
	}

	bool document::fit_texture(const double repeat_x, const double repeat_y)
	{
		if (!(repeat_x > 0.0) || !(repeat_y > 0.0))
		{
			return d->fail("Fit texture: bad repeat count");
		}

		const auto targets = target_faces(*d);
		const auto patches = target_patches(*d);

		if (targets.empty() && patches.empty())
		{
			return d->fail("Fit texture: no faces selected");
		}

		// the windings as they are now ( the geometry cache is replaced while the brushes are edited )
		struct fit
		{
			face_ref ref;
			double min_u, max_u, min_v, max_v;
		};

		std::vector<fit> fits;

		for (const face_ref& ref : targets)
		{
			const brush_geometry* geo = d->geometry(ref.brush);
			const auto& indices = geo->face_vertices[ref.face];
			if (indices.empty())
			{
				continue;
			}

			const face& source = d->get_brush(ref.brush)->faces[ref.face];
			vec3 u, v;
			face_texture_axes(geo->planes[ref.face].normal, (d->edit_layer == 1 ? source.lightmap : source.texture).rotation, u, v);

			fit f{ ref, 1e300, -1e300, 1e300, -1e300 };
			for (const int index : indices)
			{
				const vec3& p = geo->vertices[index];
				f.min_u = std::min(f.min_u, dot(p, u));
				f.max_u = std::max(f.max_u, dot(p, u));
				f.min_v = std::min(f.min_v, dot(p, v));
				f.max_v = std::max(f.max_v, dot(p, v));
			}

			if (f.max_u - f.min_u > 1e-6 && f.max_v - f.min_v > 1e-6)
			{
				fits.push_back(f);
			}
		}

		if (fits.empty() && patches.empty())
		{
			return d->fail("Fit texture: the faces have no area");
		}

		transaction_scope tx(*d, "Fit texture");

		for (const fit& f : fits)
		{
			tex_def& tex = layer_tex(*d, d->edit_brush(f.ref.brush).faces[f.ref.face]);

			int w, h;
			d->texture_dims(tex.material, w, h);

			tex.size_x = (f.max_u - f.min_u) / repeat_x;
			tex.size_y = (f.max_v - f.min_v) / repeat_y;
			tex.shift_x = tidy(std::fmod(-f.min_u / tex.size_x * w, static_cast<double>(w)));
			tex.shift_y = tidy(std::fmod(-f.min_v / tex.size_y * h, static_cast<double>(h)));
		}

		for (const object_id id : patches)
		{
			patch& p = d->edit_patch(id);
			if (p.width < 2 || p.height < 2)
			{
				continue;
			}

			// material: texels; lightmap: the numbers are the coordinates themselves
			int w = 1, h = 1;
			if (d->edit_layer == 0)
			{
				d->texture_dims(p.material, w, h);
			}

			for (int c = 0; c < p.width; c++)
			{
				for (int r = 0; r < p.height; r++)
				{
					double* uv = layer_uv(*d, p.at(c, r));
					uv[0] = tidy(repeat_x * w * c / (p.width - 1));
					uv[1] = tidy(repeat_y * h * r / (p.height - 1));
				}
			}
		}

		return true;
	}

	bool document::flip_texture(const int axis)
	{
		if (axis != 0 && axis != 1)
		{
			return d->fail("Flip texture: axis 0 or 1");
		}

		return for_targets(*d, axis == 0 ? "Flip texture X" : "Flip texture Y", [&](tex_def& t, const face_ref&)
		{
			// a negative size runs the texture the other way; the shift goes with it so that the seam stays
			if (axis == 0)
			{
				t.size_x = -t.size_x;
				t.shift_x = t.shift_x == 0.0 ? 0.0 : -t.shift_x;
			}
			else
			{
				t.size_y = -t.size_y;
				t.shift_y = t.shift_y == 0.0 ? 0.0 : -t.shift_y;
			}
		}, [&](patch& p)
		{
			const uv_box box = uv_bounds(*d, p);

			for (patch_vertex& v : p.vertices)
			{
				double* uv = layer_uv(*d, v);
				uv[axis] = tidy(box.lo[axis] + box.hi[axis] - uv[axis]);
			}
		});
	}

	bool document::natural_texture()
	{
		return for_targets(*d, "Natural texture", [&](tex_def& t, const face_ref&)
		{
			if (d->edit_layer == 1)
			{
				// the lightmap as radiant writes it
				t.size_x = t.size_y = 16384.0;
			}
			else
			{
				int w, h;
				d->texture_dims(t.material, w, h);
				t.size_x = w * 0.25;
				t.size_y = h * 0.25;
			}

			t.shift_x = t.shift_y = t.rotation = 0.0;
		}, [&](patch& p)
		{
			// as a face that looks the way the patch does on the whole would be textured
			vec3 u, v;
			face_texture_axes(patch_normal(p), 0.0, u, v);
			const double per_unit = d->edit_layer == 1 ? 1.0 / 16.0 : 4.0;		// luxels at 16 units, texels at scale 0.25

			for (patch_vertex& vertex : p.vertices)
			{
				double* uv = layer_uv(*d, vertex);
				uv[0] = tidy(dot(vertex.xyz, u) * per_unit);
				uv[1] = tidy(dot(vertex.xyz, v) * per_unit);
			}
		});
	}

	bool document::set_lightmap(const tex_def& t)
	{
		if (t.size_x == 0.0 || t.size_y == 0.0)
		{
			return d->fail("Set lightmap: size 0");
		}

		if (!t.material.empty() && !valid_material(t.material))
		{
			return d->fail("Set lightmap: bad material name");
		}

		const auto faces = target_faces(*d);
		const auto patches = target_patches(*d);

		if (faces.empty() && patches.empty())
		{
			return d->fail("Set lightmap: no faces selected");
		}

		transaction_scope tx(*d, "Set lightmap");

		for (const face_ref& ref : faces)
		{
			tex_def& target = d->edit_brush(ref.brush).faces[ref.face].lightmap;
			const std::string material = t.material.empty() ? target.material : t.material;
			target = t;
			target.material = material;
		}

		for (const object_id id : patches)
		{
			if (!t.material.empty() && d->get_patch(id)->lightmap != t.material)
			{
				d->edit_patch(id).lightmap = t.material;
			}
		}

		return true;
	}

	bool document::set_smoothing(const std::string_view word)
	{
		if (word.find_first_of(" \t\r\n\"") != std::string_view::npos)
		{
			return d->fail("Set smoothing: one word");
		}

		const auto patches = target_patches(*d);
		if (patches.empty())
		{
			return d->fail("Set smoothing: no patch selected ( brush faces have no smoothing of their own )");
		}

		transaction_scope tx(*d, "Set smoothing");

		for (const object_id id : patches)
		{
			if (d->get_patch(id)->smoothing != word)
			{
				d->edit_patch(id).smoothing = std::string(word);
			}
		}

		return true;
	}

	// ---- find / replace -----------------------------------------------------------------------------

	bool document::replace_material(const std::string_view from, const std::string_view to, const bool selection_only, const bool faces_only, int* changed)
	{
		if (changed)
		{
			*changed = 0;
		}

		if (!valid_material(from) || !valid_material(to))
		{
			return d->fail("Replace material: bad material name");
		}

		if (from == to)
		{
			return d->fail("Replace material: both names are the same");
		}

		std::vector<face_ref> faces;
		std::vector<object_id> patches;

		if (faces_only)
		{
			faces = d->sel.faces;
			std::erase_if(faces, [&](const face_ref& f) { return !editable_brush(*d, f.brush) || f.face < 0 || f.face >= static_cast<int>(d->get_brush(f.brush)->faces.size()); });
		}
		else if (selection_only)
		{
			faces = target_faces(*d);
			patches = target_patches(*d);
		}
		else
		{
			for (const object_id entity_id : d->order)
			{
				const entity* e = d->get_entity(entity_id);
				if (!e)
				{
					continue;
				}

				for (const object_id child : e->children)
				{
					if (const brush* b = d->get_brush(child); b && !b->opaque)
					{
						for (int f = 0; f < static_cast<int>(b->faces.size()); f++)
						{
							faces.push_back({ child, f });
						}
					}
					else if (d->get_patch(child))
					{
						patches.push_back(child);
					}
				}
			}
		}

		std::erase_if(faces, [&](const face_ref& f) { return !same_material(d->get_brush(f.brush)->faces[f.face].texture.material, from); });
		std::erase_if(patches, [&](const object_id id) { return !same_material(d->get_patch(id)->material, from); });

		if (faces.empty() && patches.empty())
		{
			return d->fail("Replace material: nothing uses " + std::string(from) + (faces_only ? " among the selected faces" : selection_only ? " in the selection" : ""));
		}

		transaction_scope tx(*d, "Replace material");

		for (const face_ref& ref : faces)
		{
			d->edit_brush(ref.brush).faces[ref.face].texture.material = std::string(to);
		}

		for (const object_id id : patches)
		{
			d->edit_patch(id).material = std::string(to);
		}

		if (changed)
		{
			*changed = static_cast<int>(faces.size() + patches.size());
		}

		return true;
	}

	int document::select_by_material(const std::string_view material)
	{
		std::vector<face_ref> faces;
		std::vector<object_id> patches;

		for (const object_id entity_id : d->order)
		{
			const entity* e = d->get_entity(entity_id);
			if (!e)
			{
				continue;
			}

			for (const object_id child : e->children)
			{
				if (!d->is_visible(child))
				{
					continue;
				}

				if (const brush* b = d->get_brush(child); b && !b->opaque)
				{
					for (int f = 0; f < static_cast<int>(b->faces.size()); f++)
					{
						if (same_material(b->faces[f].texture.material, material))
						{
							faces.push_back({ child, f });
						}
					}
				}
				else if (const patch* p = d->get_patch(child); p && same_material(p->material, material))
				{
					patches.push_back(child);
				}
			}
		}

		const int found = static_cast<int>(faces.size() + patches.size());
		d->sel.faces = std::move(faces);
		d->set_selection(std::move(patches));
		d->selection_changed();
		return found;
	}

	std::vector<std::pair<std::string, int>> document::materials_in_use() const
	{
		std::map<std::string, int> counts;

		for (const object_id entity_id : d->order)
		{
			const entity* e = d->get_entity(entity_id);
			if (!e)
			{
				continue;
			}

			for (const object_id child : e->children)
			{
				if (const brush* b = d->get_brush(child); b && !b->opaque)
				{
					for (const face& f : b->faces)
					{
						counts[f.texture.material]++;
					}
				}
				else if (const patch* p = d->get_patch(child))
				{
					counts[p->material]++;
				}
			}
		}

		return { counts.begin(), counts.end() };
	}
}
