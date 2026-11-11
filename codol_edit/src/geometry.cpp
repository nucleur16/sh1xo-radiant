#include "codol_edit/geometry.hpp"

#include <algorithm>

namespace codol::edit
{
	// ---- math.hpp ---------------------------------------------------------------------------------

	bool plane_from_points(const vec3& a, const vec3& b, const vec3& c, plane& out)
	{
		const vec3 n = cross(c - a, b - a);
		const double len = length(n);

		if (len < 1e-9)
		{
			out = {};
			return false;
		}

		out.normal = n * (1.0 / len);
		out.dist = dot(out.normal, a);
		return true;
	}

	bool transform::is_rotation(const double eps) const
	{
		for (int i = 0; i < 3; i++)
		{
			for (int j = 0; j < 3; j++)
			{
				double d = 0.0;
				for (int k = 0; k < 3; k++)
				{
					d += m[i][k] * m[j][k];
				}

				if (std::fabs(d - (i == j ? 1.0 : 0.0)) > eps)
				{
					return false;
				}
			}
		}

		return determinant() > 0.0;
	}

	bool transform::is_translation() const
	{
		for (int i = 0; i < 3; i++)
		{
			for (int j = 0; j < 3; j++)
			{
				if (m[i][j] != (i == j ? 1.0 : 0.0))
				{
					return false;
				}
			}
		}

		return true;
	}

	transform transform::translation(const vec3& delta)
	{
		transform r;
		r.t = delta;
		return r;
	}

	transform transform::rotation(const vec3& origin, const vec3& axis, const double degrees)
	{
		transform r;
		const vec3 a = normalize(axis);

		// multiples of 90 degrees stay exact
		double s, c;
		const double quarter = degrees / 90.0;
		if (quarter == std::floor(quarter))
		{
			static constexpr double sines[4] = { 0, 1, 0, -1 };
			static constexpr double cosines[4] = { 1, 0, -1, 0 };
			const int q = static_cast<int>(((static_cast<long long>(quarter) % 4) + 4) % 4);
			s = sines[q];
			c = cosines[q];
		}
		else
		{
			s = std::sin(degrees * PI / 180.0);
			c = std::cos(degrees * PI / 180.0);
		}

		const double k = 1.0 - c;
		r.m[0][0] = c + a.x * a.x * k;			r.m[0][1] = a.x * a.y * k - a.z * s;	r.m[0][2] = a.x * a.z * k + a.y * s;
		r.m[1][0] = a.y * a.x * k + a.z * s;	r.m[1][1] = c + a.y * a.y * k;			r.m[1][2] = a.y * a.z * k - a.x * s;
		r.m[2][0] = a.z * a.x * k - a.y * s;	r.m[2][1] = a.z * a.y * k + a.x * s;	r.m[2][2] = c + a.z * a.z * k;
		r.t = origin - r.apply_vector(origin);
		return r;
	}

	transform transform::rotation_angles(const vec3& origin, const vec3& angles)
	{
		vec3 axes[3];
		angles_to_axes(angles, axes);

		transform r;
		for (int i = 0; i < 3; i++)
		{
			r.m[i][0] = axes[0][i];
			r.m[i][1] = axes[1][i];
			r.m[i][2] = axes[2][i];
		}

		r.t = origin - r.apply_vector(origin);
		return r;
	}

	transform transform::scaling(const vec3& origin, const vec3& factors)
	{
		transform r;
		r.m[0][0] = factors.x;
		r.m[1][1] = factors.y;
		r.m[2][2] = factors.z;
		r.t = origin - r.apply_vector(origin);
		return r;
	}

	transform transform::flip(const vec3& origin, const int axis)
	{
		vec3 f{ 1, 1, 1 };
		f[axis] = -1.0;
		return scaling(origin, f);
	}

	transform transform::concat(const transform& a, const transform& b)
	{
		transform r;
		for (int i = 0; i < 3; i++)
		{
			for (int j = 0; j < 3; j++)
			{
				r.m[i][j] = b.m[i][0] * a.m[0][j] + b.m[i][1] * a.m[1][j] + b.m[i][2] * a.m[2][j];
			}
		}

		r.t = b.apply(a.t);
		return r;
	}

	void angles_to_axes(const vec3& angles, vec3 axes[3])
	{
		const double p = angles.x * PI / 180.0, y = angles.y * PI / 180.0, r = angles.z * PI / 180.0;
		const double sp = std::sin(p), cp = std::cos(p);
		const double sy = std::sin(y), cy = std::cos(y);
		const double sr = std::sin(r), cr = std::cos(r);

		axes[0] = { cp * cy, cp * sy, -sp };
		axes[1] = { sr * sp * cy - cr * sy, sr * sp * sy + cr * cy, sr * cp };
		axes[2] = { cr * sp * cy + sr * sy, cr * sp * sy - sr * cy, cr * cp };
	}

	vec3 axes_to_angles(const vec3 axes[3])
	{
		const vec3 fwd = normalize(axes[0]);
		const vec3 left = normalize(axes[1]);
		const vec3 up = normalize(axes[2]);

		const double flat = std::sqrt(fwd.x * fwd.x + fwd.y * fwd.y);
		double pitch, yaw, roll;

		if (flat > 1e-6)
		{
			yaw = std::atan2(fwd.y, fwd.x) * 180.0 / PI;
			pitch = std::atan2(-fwd.z, flat) * 180.0 / PI;
			roll = std::atan2(left.z, up.z) * 180.0 / PI;
		}
		else
		{
			pitch = fwd.z > 0.0 ? -90.0 : 90.0;
			yaw = std::atan2(-left.x, left.y) * 180.0 / PI;
			roll = 0.0;
		}

		const auto wrap = [](double a)
		{
			a = std::fmod(a, 360.0);
			if (a < 0.0)
			{
				a += 360.0;
			}

			return (std::fabs(a - 360.0) < 1e-6 || std::fabs(a) < 1e-6) ? 0.0 : a;
		};

		return { wrap(pitch), wrap(yaw), wrap(roll) };
	}

	// ---- windings ---------------------------------------------------------------------------------

	winding base_winding(const plane& p, const double extent)
	{
		const vec3& n = p.normal;
		const double ax = std::fabs(n.x), ay = std::fabs(n.y), az = std::fabs(n.z);

		const vec3 axis = (az >= ax && az >= ay) ? vec3{ 1, 0, 0 } : vec3{ 0, 0, 1 };
		const vec3 u = normalize(cross(n, axis));
		const vec3 v = cross(n, u);		// u x v = n
		const vec3 org = n * p.dist;

		return { org - u * extent - v * extent, org + u * extent - v * extent, org + u * extent + v * extent, org - u * extent + v * extent };
	}

	void clip_winding(winding& w, const plane& p, const double eps)
	{
		if (w.empty())
		{
			return;
		}

		enum { front, back, on };

		const std::size_t count = w.size();
		double dists_small[64];
		char sides_small[64];
		std::vector<double> dists_big;
		std::vector<char> sides_big;

		double* dists = dists_small;
		char* sides = sides_small;

		if (count > 64)
		{
			dists_big.resize(count);
			sides_big.resize(count);
			dists = dists_big.data();
			sides = sides_big.data();
		}

		int counts[3] = {};

		for (std::size_t i = 0; i < count; i++)
		{
			dists[i] = p.distance_to(w[i]);
			sides[i] = static_cast<char>(dists[i] > eps ? front : (dists[i] < -eps ? back : on));
			counts[sides[i]]++;
		}

		if (!counts[back])
		{
			w.clear();
			return;
		}

		if (!counts[front])
		{
			return;
		}

		winding out;
		out.reserve(count + 4);

		for (std::size_t i = 0; i < count; i++)
		{
			const vec3& p1 = w[i];

			if (sides[i] == on)
			{
				out.push_back(p1);
				continue;
			}

			if (sides[i] == back)
			{
				out.push_back(p1);
			}

			const std::size_t next = (i + 1) % count;
			if (sides[next] == on || sides[next] == sides[i])
			{
				continue;
			}

			const vec3& p2 = w[next];
			const double frac = dists[i] / (dists[i] - dists[next]);
			vec3 mid;

			for (int k = 0; k < 3; k++)
			{
				// keep coordinates of axial planes exact
				if (p.normal[k] == 1.0)
				{
					mid[k] = p.dist;
				}
				else if (p.normal[k] == -1.0)
				{
					mid[k] = -p.dist;
				}
				else
				{
					mid[k] = p1[k] + frac * (p2[k] - p1[k]);
				}
			}

			out.push_back(mid);
		}

		w.swap(out);
	}

	double winding_area(const winding& w)
	{
		vec3 total;
		for (std::size_t i = 2; i < w.size(); i++)
		{
			total += cross(w[i - 1] - w[0], w[i] - w[0]);
		}

		return 0.5 * length(total);
	}

	winding brush_geometry::face_winding(const int face) const
	{
		winding w;
		if (face >= 0 && face < static_cast<int>(face_vertices.size()))
		{
			for (const int index : face_vertices[face])
			{
				w.push_back(vertices[index]);
			}
		}

		return w;
	}

	bool build_brush_geometry(const brush& b, brush_geometry& out)
	{
		out = {};

		const std::size_t count = b.faces.size();
		out.planes.resize(count);
		out.face_vertices.resize(count);

		std::vector<char> usable(count, 0);

		for (std::size_t i = 0; i < count; i++)
		{
			usable[i] = plane_from_points(b.faces[i].points[0], b.faces[i].points[1], b.faces[i].points[2], out.planes[i]) ? 1 : 0;
		}

		winding w;
		std::vector<winding> windings(count);
		std::vector<std::size_t> parallel;

		for (std::size_t i = 0; i < count; i++)
		{
			if (!usable[i])
			{
				continue;
			}

			const plane& pi = out.planes[i];
			w = base_winding(pi);
			parallel.clear();

			// first everything that is not ( nearly ) the same plane: that bounds the winding to the size of the brush
			for (std::size_t j = 0; j < count && !w.empty(); j++)
			{
				if (j == i || !usable[j])
				{
					continue;
				}

				if (dot(pi.normal, out.planes[j].normal) > 0.9999)
				{
					parallel.push_back(j);
					continue;
				}

				clip_winding(w, out.planes[j], CLIP_EPSILON);
			}

			// then the planes that face the same way. One that does not leave the winding by more than
			// SAME_PLANE_EPSILON is the same plane a second time: the first of the two keeps the winding. Anything
			// else is clipped exactly, so two faces that are almost coplanar share the surface along a line.
			for (const std::size_t j : parallel)
			{
				if (w.empty())
				{
					break;
				}

				const plane& pj = out.planes[j];
				double deviation = 0.0;

				for (const vec3& p : w)
				{
					deviation = std::max(deviation, std::fabs(pj.distance_to(p)));
				}

				if (deviation <= SAME_PLANE_EPSILON)
				{
					if (j < i)
					{
						w.clear();
					}

					continue;
				}

				clip_winding(w, pj, CLIP_EPSILON);
			}

			if (w.size() < 3)
			{
				continue;
			}

			windings[i] = w;
		}

		const auto is_closed = [](const std::vector<std::vector<int>>& faces)
		{
			// every directed edge exactly once, and its reverse exactly once
			std::vector<std::pair<int, int>> edges;
			for (const auto& indices : faces)
			{
				for (std::size_t k = 0; k < indices.size(); k++)
				{
					edges.emplace_back(indices[k], indices[(k + 1) % indices.size()]);
				}
			}

			std::sort(edges.begin(), edges.end());

			if (edges.empty() || std::adjacent_find(edges.begin(), edges.end()) != edges.end())
			{
				return false;
			}

			for (const auto& e : edges)
			{
				if (!std::binary_search(edges.begin(), edges.end(), std::pair<int, int>(e.second, e.first)))
				{
					return false;
				}
			}

			return true;
		};

		// corners of all windings welded with `weld`; true when the result is a closed polyhedron
		const auto assemble = [&](const double weld)
		{
			out.vertices.clear();

			for (std::size_t i = 0; i < count; i++)
			{
				auto& indices = out.face_vertices[i];
				indices.clear();

				for (const vec3& p : windings[i])
				{
					int found = -1;
					for (std::size_t v = 0; v < out.vertices.size(); v++)
					{
						const vec3 d = out.vertices[v] - p;
						if (std::fabs(d.x) < weld && std::fabs(d.y) < weld && std::fabs(d.z) < weld)
						{
							found = static_cast<int>(v);
							break;
						}
					}

					if (found < 0)
					{
						found = static_cast<int>(out.vertices.size());
						out.vertices.push_back(p);
					}

					if (indices.empty() || indices.back() != found)
					{
						indices.push_back(found);
					}
				}

				while (indices.size() > 1 && indices.front() == indices.back())
				{
					indices.pop_back();
				}

				if (indices.size() < 3)
				{
					indices.clear();
				}
			}

			// A corner of a closed polyhedron belongs to at least three faces. One that is used by fewer is debris
			// of the clipping ( a point on an edge that the neighbouring face does not have ): take it out, and with
			// it the faces that are left with fewer than three corners. Only kept when it closes the brush.
			if (!is_closed(out.face_vertices))
			{
				auto cleaned = out.face_vertices;

				for (bool changed = true; changed;)
				{
					changed = false;

					std::vector<int> uses(out.vertices.size(), 0);
					for (const auto& indices : cleaned)
					{
						for (const int index : indices)
						{
							uses[index]++;
						}
					}

					for (auto& indices : cleaned)
					{
						const auto before = indices.size();
						std::erase_if(indices, [&](const int index) { return uses[index] < 3; });

						if (indices.size() != before)
						{
							changed = true;

							if (indices.size() < 3)
							{
								indices.clear();
							}
						}
					}
				}

				if (is_closed(cleaned))
				{
					out.face_vertices = std::move(cleaned);
				}
			}

			out.face_count_with_winding = 0;
			for (const auto& indices : out.face_vertices)
			{
				out.face_count_with_winding += indices.empty() ? 0 : 1;
			}

			return out.face_count_with_winding >= 4 && is_closed(out.face_vertices);
		};

		// Slivers of about the size of the weld epsilon can leave a brush open ( thin slabs with bevel planes at
		// almost no angle in shipped maps ): try again with a coarser weld before giving up.
		if (!assemble(WELD_EPSILON) && !assemble(WELD_EPSILON * 2.5) && !assemble(WELD_EPSILON * 6.0))
		{
			assemble(WELD_EPSILON);
		}

		// drop corners that are not used any more
		{
			std::vector<int> used(out.vertices.size(), 0);
			for (const auto& indices : out.face_vertices)
			{
				for (const int index : indices)
				{
					used[index] = 1;
				}
			}

			std::vector<int> remap(out.vertices.size(), -1);
			std::vector<vec3> kept;

			for (std::size_t v = 0; v < out.vertices.size(); v++)
			{
				if (used[v])
				{
					remap[v] = static_cast<int>(kept.size());
					kept.push_back(out.vertices[v]);
				}
			}

			if (kept.size() != out.vertices.size())
			{
				out.vertices.swap(kept);
				for (auto& indices : out.face_vertices)
				{
					for (int& index : indices)
					{
						index = remap[index];
					}
				}
			}
		}

		out.closed = is_closed(out.face_vertices);

		bool inside_world = true;
		for (const vec3& v : out.vertices)
		{
			out.box.add(v);
			if (std::fabs(v.x) > WORLD_EXTENT + 1.0 || std::fabs(v.y) > WORLD_EXTENT + 1.0 || std::fabs(v.z) > WORLD_EXTENT + 1.0)
			{
				inside_world = false;
			}
		}

		out.valid = out.face_count_with_winding >= 4 && out.closed && inside_world;
		return out.valid;
	}

	// ---- texture coordinates ------------------------------------------------------------------------

	void face_texture_axes(const vec3& normal, const double rotation, vec3& u_axis, vec3& v_axis)
	{
		static constexpr vec3 base[18] =
		{
			{ 0, 0, 1 }, { 1, 0, 0 }, { 0, -1, 0 },		// floor
			{ 0, 0, -1 }, { 1, 0, 0 }, { 0, -1, 0 },	// ceiling
			{ 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, -1 },		// west wall
			{ -1, 0, 0 }, { 0, 1, 0 }, { 0, 0, -1 },	// east wall
			{ 0, 1, 0 }, { 1, 0, 0 }, { 0, 0, -1 },		// south wall
			{ 0, -1, 0 }, { 1, 0, 0 }, { 0, 0, -1 },	// north wall
		};

		int best = 0;
		double best_dot = 0.0;

		for (int i = 0; i < 6; i++)
		{
			const double d = dot(normal, base[i * 3]);
			if (d > best_dot)
			{
				best_dot = d;
				best = i;
			}
		}

		u_axis = base[best * 3 + 1];
		v_axis = base[best * 3 + 2];

		if (rotation == 0.0)
		{
			return;
		}

		double s, c;
		if (rotation == 90.0) { s = 1; c = 0; }
		else if (rotation == 180.0) { s = 0; c = -1; }
		else if (rotation == 270.0 || rotation == -90.0) { s = -1; c = 0; }
		else
		{
			s = std::sin(rotation * PI / 180.0);
			c = std::cos(rotation * PI / 180.0);
		}

		const int sv = u_axis.x != 0.0 ? 0 : (u_axis.y != 0.0 ? 1 : 2);
		const int tv = v_axis.x != 0.0 ? 0 : (v_axis.y != 0.0 ? 1 : 2);

		for (vec3* axis : { &u_axis, &v_axis })
		{
			const double ns = c * (*axis)[sv] - s * (*axis)[tv];
			const double nt = s * (*axis)[sv] + c * (*axis)[tv];
			(*axis)[sv] = ns;
			(*axis)[tv] = nt;
		}
	}

	vec2 face_uv(const vec3& point, const vec3& normal, const tex_def& tex, const int tex_width, const int tex_height)
	{
		vec3 u, v;
		face_texture_axes(normal, tex.rotation, u, v);

		const double sx = tex.size_x != 0.0 ? tex.size_x : 1.0;
		const double sy = tex.size_y != 0.0 ? tex.size_y : 1.0;

		return { dot(point, u) / sx + tex.shift_x / (tex_width > 0 ? tex_width : 256),
			dot(point, v) / sy + tex.shift_y / (tex_height > 0 ? tex_height : 256) };
	}

	// ---- meshes -------------------------------------------------------------------------------------

	void mesh::clear()
	{
		vertices.clear();
		indices.clear();
		parts.clear();
		box = {};
		revision = 0;
	}

	void build_brush_mesh(const brush& b, const brush_geometry& geo, const texture_size_fn& texture_size, mesh& out)
	{
		out.revision = b.revision;

		for (std::size_t f = 0; f < geo.face_vertices.size() && f < b.faces.size(); f++)
		{
			const auto& indices = geo.face_vertices[f];
			if (indices.size() < 3)
			{
				continue;
			}

			const face& fc = b.faces[f];
			const plane& pl = geo.planes[f];

			int tw = 256, th = 256;
			if (texture_size && (!texture_size(fc.texture.material, tw, th) || tw <= 0 || th <= 0))
			{
				tw = th = 256;
			}

			vec3 u, v, lu, lv;
			face_texture_axes(pl.normal, fc.texture.rotation, u, v);
			face_texture_axes(pl.normal, fc.lightmap.rotation, lu, lv);

			const double sx = fc.texture.size_x != 0.0 ? fc.texture.size_x : 1.0;
			const double sy = fc.texture.size_y != 0.0 ? fc.texture.size_y : 1.0;
			const double lx = fc.lightmap.size_x != 0.0 ? fc.lightmap.size_x : 1.0;
			const double ly = fc.lightmap.size_y != 0.0 ? fc.lightmap.size_y : 1.0;

			mesh_part part;
			part.material = fc.texture.material;
			part.face = static_cast<int>(f);
			part.first_index = static_cast<std::uint32_t>(out.indices.size());

			const auto first = static_cast<std::uint32_t>(out.vertices.size());

			for (const int index : indices)
			{
				const vec3& p = geo.vertices[index];
				out.box.add(p);

				mesh_vertex mv{};
				mv.position[0] = static_cast<float>(p.x);
				mv.position[1] = static_cast<float>(p.y);
				mv.position[2] = static_cast<float>(p.z);
				mv.normal[0] = static_cast<float>(pl.normal.x);
				mv.normal[1] = static_cast<float>(pl.normal.y);
				mv.normal[2] = static_cast<float>(pl.normal.z);
				mv.uv[0] = static_cast<float>(dot(p, u) / sx + fc.texture.shift_x / tw);
				mv.uv[1] = static_cast<float>(dot(p, v) / sy + fc.texture.shift_y / th);
				mv.lightmap_uv[0] = static_cast<float>(dot(p, lu) / lx + fc.lightmap.shift_x / 1024.0);
				mv.lightmap_uv[1] = static_cast<float>(dot(p, lv) / ly + fc.lightmap.shift_y / 1024.0);
				mv.color[0] = mv.color[1] = mv.color[2] = mv.color[3] = 255;
				out.vertices.push_back(mv);
			}

			for (std::size_t k = 2; k < indices.size(); k++)
			{
				out.indices.push_back(first);
				out.indices.push_back(first + static_cast<std::uint32_t>(k - 1));
				out.indices.push_back(first + static_cast<std::uint32_t>(k));
			}

			part.index_count = static_cast<std::uint32_t>(out.indices.size()) - part.first_index;
			out.parts.push_back(std::move(part));
		}
	}

	namespace
	{
		struct tess_vertex
		{
			vec3 xyz;
			double tex[4] = {};
			double color[4] = { 255, 255, 255, 255 };
		};

		tess_vertex from_control(const patch_vertex& v)
		{
			tess_vertex t;
			t.xyz = v.xyz;

			for (std::size_t i = 0; i < 4 && i < v.tex.size(); i++)
			{
				t.tex[i] = v.tex[i];
			}

			for (int i = 0; i < 4; i++)
			{
				t.color[i] = v.color[i];
			}

			return t;
		}

		tess_vertex blend3(const tess_vertex& a, const tess_vertex& b, const tess_vertex& c, const double t)
		{
			const double w0 = (1.0 - t) * (1.0 - t), w1 = 2.0 * t * (1.0 - t), w2 = t * t;

			tess_vertex r;
			r.xyz = a.xyz * w0 + b.xyz * w1 + c.xyz * w2;

			for (int i = 0; i < 4; i++)
			{
				r.tex[i] = a.tex[i] * w0 + b.tex[i] * w1 + c.tex[i] * w2;
				r.color[i] = a.color[i] * w0 + b.color[i] * w1 + c.color[i] * w2;
			}

			return r;
		}

		bool is_bezier(const patch& p)
		{
			return p.type == patch_type::curve && p.width >= 3 && p.height >= 3 && (p.width & 1) && (p.height & 1);
		}

		void tessellate(const patch& p, int subdivisions, std::vector<tess_vertex>& grid, int& columns, int& rows)
		{
			grid.clear();
			columns = rows = 0;

			if (p.width < 2 || p.height < 2 || p.vertices.size() < static_cast<std::size_t>(p.width) * p.height)
			{
				return;
			}

			if (!is_bezier(p))
			{
				columns = p.width;
				rows = p.height;
				grid.reserve(p.vertices.size());

				for (int c = 0; c < columns; c++)
				{
					for (int r = 0; r < rows; r++)
					{
						grid.push_back(from_control(p.at(c, r)));
					}
				}

				return;
			}

			subdivisions = std::clamp(subdivisions, 1, 32);

			const int segs_c = (p.width - 1) / 2, segs_r = (p.height - 1) / 2;
			columns = segs_c * subdivisions + 1;
			rows = segs_r * subdivisions + 1;
			grid.resize(static_cast<std::size_t>(columns) * rows);

			// first along the rows of every control column, then across the columns
			std::vector<tess_vertex> along(static_cast<std::size_t>(p.width) * rows);

			for (int c = 0; c < p.width; c++)
			{
				for (int r = 0; r < rows; r++)
				{
					const int seg = std::min(r / subdivisions, segs_r - 1);
					const double t = static_cast<double>(r - seg * subdivisions) / subdivisions;
					along[static_cast<std::size_t>(c) * rows + r] = blend3(from_control(p.at(c, seg * 2)), from_control(p.at(c, seg * 2 + 1)),
						from_control(p.at(c, seg * 2 + 2)), t);
				}
			}

			for (int c = 0; c < columns; c++)
			{
				const int seg = std::min(c / subdivisions, segs_c - 1);
				const double t = static_cast<double>(c - seg * subdivisions) / subdivisions;

				for (int r = 0; r < rows; r++)
				{
					grid[static_cast<std::size_t>(c) * rows + r] = blend3(along[static_cast<std::size_t>(seg * 2) * rows + r],
						along[static_cast<std::size_t>(seg * 2 + 1) * rows + r], along[static_cast<std::size_t>(seg * 2 + 2) * rows + r], t);
				}
			}
		}
	}

	void tessellate_patch(const patch& p, const int subdivisions, std::vector<vec3>& points, int& columns, int& rows)
	{
		std::vector<tess_vertex> grid;
		tessellate(p, subdivisions, grid, columns, rows);

		points.clear();
		points.reserve(grid.size());

		for (const auto& v : grid)
		{
			points.push_back(v.xyz);
		}
	}

	void build_patch_mesh(const patch& p, const int subdivisions, const texture_size_fn& texture_size, mesh& out)
	{
		out.revision = p.revision;

		int tw = 256, th = 256;
		if (texture_size && (!texture_size(p.material, tw, th) || tw <= 0 || th <= 0))
		{
			tw = th = 256;
		}

		std::vector<tess_vertex> grid;
		int columns = 0, rows = 0;
		tessellate(p, subdivisions, grid, columns, rows);

		if (columns < 2 || rows < 2)
		{
			return;
		}

		const auto at = [&](const int c, const int r) -> const tess_vertex& { return grid[static_cast<std::size_t>(c) * rows + r]; };
		const auto first = static_cast<std::uint32_t>(out.vertices.size());

		for (int c = 0; c < columns; c++)
		{
			for (int r = 0; r < rows; r++)
			{
				const tess_vertex& v = at(c, r);
				const vec3 dc = at(std::min(c + 1, columns - 1), r).xyz - at(std::max(c - 1, 0), r).xyz;
				const vec3 dr = at(c, std::min(r + 1, rows - 1)).xyz - at(c, std::max(r - 1, 0)).xyz;
				const vec3 n = normalize(cross(dc, dr));

				out.box.add(v.xyz);

				mesh_vertex mv{};
				mv.position[0] = static_cast<float>(v.xyz.x);
				mv.position[1] = static_cast<float>(v.xyz.y);
				mv.position[2] = static_cast<float>(v.xyz.z);
				mv.normal[0] = static_cast<float>(n.x);
				mv.normal[1] = static_cast<float>(n.y);
				mv.normal[2] = static_cast<float>(n.z);
				mv.uv[0] = static_cast<float>(v.tex[0] / tw);
				mv.uv[1] = static_cast<float>(v.tex[1] / th);
				mv.lightmap_uv[0] = static_cast<float>(v.tex[2]);
				mv.lightmap_uv[1] = static_cast<float>(v.tex[3]);

				for (int i = 0; i < 4; i++)
				{
					mv.color[i] = static_cast<std::uint8_t>(std::clamp(v.color[i] + 0.5, 0.0, 255.0));
				}

				out.vertices.push_back(mv);
			}
		}

		mesh_part part;
		part.material = p.material;
		part.first_index = static_cast<std::uint32_t>(out.indices.size());

		const bool control_grid = !is_bezier(p);

		for (int c = 0; c + 1 < columns; c++)
		{
			for (int r = 0; r + 1 < rows; r++)
			{
				const std::uint32_t v00 = first + static_cast<std::uint32_t>(c * rows + r);
				const std::uint32_t v10 = first + static_cast<std::uint32_t>((c + 1) * rows + r);
				const std::uint32_t v11 = v10 + 1;
				const std::uint32_t v01 = v00 + 1;

				// "f" on the first vertex of a terrain quad turns its diagonal
				const bool turned = control_grid && p.at(c, r).has_flag && p.at(c, r).flag != 0;

				if (turned)
				{
					out.indices.insert(out.indices.end(), { v00, v10, v01, v10, v11, v01 });
				}
				else
				{
					out.indices.insert(out.indices.end(), { v00, v10, v11, v00, v11, v01 });
				}
			}
		}

		part.index_count = static_cast<std::uint32_t>(out.indices.size()) - part.first_index;
		out.parts.push_back(std::move(part));
	}

	// ---- intersection -------------------------------------------------------------------------------

	bool ray_brush(const ray& r, const brush_geometry& geo, double& distance, int& face)
	{
		double enter = -1e300, leave = 1e300;
		int enter_face = -1;

		for (std::size_t i = 0; i < geo.planes.size(); i++)
		{
			const plane& p = geo.planes[i];
			if (p.normal.x == 0.0 && p.normal.y == 0.0 && p.normal.z == 0.0)
			{
				continue;
			}

			const double d0 = p.distance_to(r.origin);
			const double dn = dot(p.normal, r.direction);

			if (std::fabs(dn) < 1e-12)
			{
				if (d0 > 0.0)
				{
					return false;
				}

				continue;
			}

			const double t = -d0 / dn;

			if (dn < 0.0)
			{
				// prefer a face that has a winding when two planes are entered at the same distance
				if (t > enter + 1e-9 || (t > enter - 1e-9 && enter_face >= 0 && geo.face_vertices[enter_face].empty() && !geo.face_vertices[i].empty()))
				{
					enter = std::max(enter, t);
					enter_face = static_cast<int>(i);
				}
			}
			else if (t < leave)
			{
				leave = t;
			}

			if (enter > leave)
			{
				return false;
			}
		}

		if (enter_face < 0 || enter < 0.0 || enter > leave)
		{
			return false;
		}

		distance = enter;
		face = enter_face;
		return true;
	}

	bool ray_box(const ray& r, const bounds& box, double& distance)
	{
		double enter = -1e300, leave = 1e300;

		for (int i = 0; i < 3; i++)
		{
			if (std::fabs(r.direction[i]) < 1e-12)
			{
				if (r.origin[i] < box.mins[i] || r.origin[i] > box.maxs[i])
				{
					return false;
				}

				continue;
			}

			double t0 = (box.mins[i] - r.origin[i]) / r.direction[i];
			double t1 = (box.maxs[i] - r.origin[i]) / r.direction[i];

			if (t0 > t1)
			{
				std::swap(t0, t1);
			}

			enter = std::max(enter, t0);
			leave = std::min(leave, t1);

			if (enter > leave)
			{
				return false;
			}
		}

		if (leave < 0.0)
		{
			return false;
		}

		distance = std::max(enter, 0.0);
		return true;
	}

	bool ray_triangle(const ray& r, const vec3& a, const vec3& b, const vec3& c, double& distance, const bool two_sided)
	{
		const vec3 e1 = b - a, e2 = c - a;
		const vec3 pv = cross(r.direction, e2);
		const double det = dot(e1, pv);

		if (two_sided ? std::fabs(det) < 1e-14 : det < 1e-14)
		{
			return false;
		}

		const double inv = 1.0 / det;
		const vec3 tv = r.origin - a;
		const double u = dot(tv, pv) * inv;

		if (u < -1e-9 || u > 1.0 + 1e-9)
		{
			return false;
		}

		const vec3 qv = cross(tv, e1);
		const double v = dot(r.direction, qv) * inv;

		if (v < -1e-9 || u + v > 1.0 + 1e-9)
		{
			return false;
		}

		const double t = dot(e2, qv) * inv;
		if (t < 0.0)
		{
			return false;
		}

		distance = t;
		return true;
	}

	// ---- construction -------------------------------------------------------------------------------

	void points_for_plane(const plane& p, vec3 out[3])
	{
		const vec3& n = p.normal;
		const double ax = std::fabs(n.x), ay = std::fabs(n.y), az = std::fabs(n.z);

		const vec3 axis = (az >= ax && az >= ay) ? vec3{ 1, 0, 0 } : vec3{ 0, 0, 1 };
		const vec3 u = normalize(cross(n, axis));
		const vec3 v = cross(n, u);

		constexpr double SPAN = 64.0;
		out[0] = n * p.dist;
		out[1] = out[0] + v * SPAN;
		out[2] = out[0] + u * SPAN;

		// an axial plane: get rid of -0 and of noise in the two free coordinates
		for (int k = 0; k < 3; k++)
		{
			for (int i = 0; i < 3; i++)
			{
				if (std::fabs(out[k][i]) < 1e-9)
				{
					out[k][i] = 0.0;
				}
			}
		}
	}

	bool convex_hull(const std::span<const vec3> points, std::vector<plane>& planes, std::vector<std::vector<int>>& hull_faces)
	{
		planes.clear();
		hull_faces.clear();

		const int n = static_cast<int>(points.size());
		if (n < 4)
		{
			return false;
		}

		constexpr double EPS = 0.005;

		for (int i = 0; i < n; i++)
		{
			for (int j = i + 1; j < n; j++)
			{
				for (int k = j + 1; k < n; k++)
				{
					plane pl;
					if (!plane_from_points(points[i], points[j], points[k], pl))
					{
						continue;
					}

					int front = 0, back = 0;
					for (int m = 0; m < n && !(front && back); m++)
					{
						const double d = pl.distance_to(points[m]);
						if (d > EPS) { front++; }
						else if (d < -EPS) { back++; }
					}

					if (front && back)
					{
						continue;
					}

					if (!front && !back)
					{
						return false;	// everything on one plane
					}

					if (front)
					{
						pl = pl.flipped();
					}

					bool known = false;
					for (const plane& have : planes)
					{
						if (dot(have.normal, pl.normal) > 1.0 - 1e-7 && std::fabs(have.dist - pl.dist) < EPS * 2)
						{
							known = true;
							break;
						}
					}

					if (!known)
					{
						planes.push_back(pl);
					}
				}
			}
		}

		if (planes.size() < 4)
		{
			planes.clear();
			return false;
		}

		for (const plane& pl : planes)
		{
			std::vector<int> on;
			vec3 center;

			for (int m = 0; m < n; m++)
			{
				if (std::fabs(pl.distance_to(points[m])) <= EPS * 2)
				{
					// the same position twice counts once
					bool twice = false;
					for (const int have : on)
					{
						if (length(points[have] - points[m]) < EPS)
						{
							twice = true;
							break;
						}
					}

					if (!twice)
					{
						on.push_back(m);
						center += points[m];
					}
				}
			}

			center = center * (1.0 / static_cast<double>(on.size()));

			// counter clockwise from the front: ( u, v, n ) right handed
			const winding base = base_winding(pl, 1.0);
			const vec3 u = normalize(base[1] - base[0]);
			const vec3 v = normalize(base[3] - base[0]);

			std::sort(on.begin(), on.end(), [&](const int a, const int b)
			{
				const vec3 da = points[a] - center, db = points[b] - center;
				return std::atan2(dot(da, v), dot(da, u)) < std::atan2(dot(db, v), dot(db, u));
			});

			hull_faces.push_back(std::move(on));
		}

		return true;
	}
}
