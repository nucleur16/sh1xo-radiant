// codol_edit: the operations and queries of geometry editing in a perspective view ( handles.hpp, and the
// document operations that move several faces / the corners of several brushes in one step ).

#include "doc_impl.hpp"

#include "codol_edit/handles.hpp"

#include <algorithm>
#include <map>

namespace codol::edit
{
	std::vector<brush_edge> brush_edges(const brush_geometry& geo)
	{
		std::map<std::pair<int, int>, brush_edge> found;

		for (std::size_t f = 0; f < geo.face_vertices.size(); f++)
		{
			const auto& loop = geo.face_vertices[f];

			for (std::size_t i = 0; i < loop.size(); i++)
			{
				const int a = loop[i], b = loop[(i + 1) % loop.size()];
				if (a == b)
				{
					continue;
				}

				const std::pair<int, int> key(std::min(a, b), std::max(a, b));
				auto [it, fresh] = found.try_emplace(key);

				if (fresh)
				{
					it->second.a = key.first;
					it->second.b = key.second;
					it->second.faces[0] = static_cast<int>(f);
				}
				else if (it->second.faces[1] < 0)
				{
					it->second.faces[1] = static_cast<int>(f);
				}
			}
		}

		std::vector<brush_edge> edges;
		edges.reserve(found.size());

		for (const auto& [key, edge] : found)
		{
			edges.push_back(edge);
		}

		return edges;
	}

	bool ray_plane(const ray& r, const plane& p, double& distance)
	{
		const double denom = dot(p.normal, r.direction);
		if (std::fabs(denom) < 1e-12)
		{
			return false;
		}

		const double t = (p.dist - dot(p.normal, r.origin)) / denom;
		if (t < 0.0)
		{
			return false;
		}

		distance = t;
		return true;
	}

	bool closest_on_line(const ray& r, const vec3& line_origin, const vec3& line_direction, double& t)
	{
		// minimise | ( line_origin + t d ) - ( r.origin + s e ) |
		const vec3& d = line_direction;
		const vec3& e = r.direction;
		const vec3 w = line_origin - r.origin;

		const double dd = dot(d, d), de = dot(d, e), ee = dot(e, e);
		const double denom = dd * ee - de * de;

		if (dd < 1e-24 || ee < 1e-24 || denom < 1e-12 * dd * ee)
		{
			return false;
		}

		t = (de * dot(e, w) - ee * dot(d, w)) / denom;
		return true;
	}

	std::vector<int> drag_faces(const brush_geometry& geo, const ray& r)
	{
		std::vector<int> faces;

		{
			double distance = 0.0;
			int face = -1;

			if (ray_brush(r, geo, distance, face))
			{
				return faces;
			}
		}

		const double reach = WORLD_EXTENT * 4.0 / std::max(1e-12, length(r.direction));

		for (std::size_t f = 0; f < geo.planes.size(); f++)
		{
			if (geo.face_vertices[f].empty())
			{
				continue;
			}

			// the part of the ray behind every other face
			double t0 = 0.0, t1 = reach;
			bool starts_inside = true;

			for (std::size_t k = 0; k < geo.planes.size() && t0 <= t1; k++)
			{
				if (k == f || geo.face_vertices[k].empty())
				{
					continue;
				}

				const plane& pl = geo.planes[k];
				const double at = pl.distance_to(r.origin);
				const double slope = dot(pl.normal, r.direction);

				if (at > ON_EPSILON)
				{
					starts_inside = false;
				}

				if (std::fabs(slope) < 1e-12)
				{
					if (at > 0.0)
					{
						t1 = -1.0;	// parallel and in front: never behind this face
					}
				}
				else if (slope > 0.0)
				{
					t1 = std::min(t1, -at / slope);	// leaves the back of the face
				}
				else
				{
					t0 = std::max(t0, -at / slope);	// enters it
				}
			}

			// Radiant does not take the face the camera itself looks along ( the ray starts in that face's space ):
			// that one could never be told apart from the sides.
			if (t0 > t1 || starts_inside)
			{
				continue;
			}

			// the ray missed the brush, so what is left of it is in front of this face
			const vec3 mid = r.origin + r.direction * ((t0 + t1) * 0.5);
			if (geo.planes[f].distance_to(mid) > 0.0)
			{
				faces.push_back(static_cast<int>(f));
			}
		}

		return faces;
	}

	bool cut_winding(const brush_geometry& geo, const plane& p, winding& out)
	{
		if (length(p.normal) < 1e-9)
		{
			return false;
		}

		out = base_winding(p);

		for (std::size_t f = 0; f < geo.planes.size() && !out.empty(); f++)
		{
			if (length(geo.planes[f].normal) > 0.5)
			{
				clip_winding(out, geo.planes[f]);
			}
		}

		return out.size() >= 3 && winding_area(out) > 1e-6;
	}

	// ---- document -----------------------------------------------------------------------------------

	bool document::move_faces(const std::span<const face_ref> faces, const vec3& delta)
	{
		if (faces.empty())
		{
			return d->fail("Move faces: no face");
		}

		// the distances first: moving one face of a brush does not change the normals of its other faces, but it
		// replaces the brush
		std::vector<std::pair<face_ref, double>> moves;

		for (const face_ref& f : faces)
		{
			const brush_geometry* geo = d->geometry(f.brush);
			if (!geo || f.face < 0 || f.face >= static_cast<int>(geo->planes.size()))
			{
				continue;
			}

			const double distance = dot(geo->planes[f.face].normal, delta);
			if (std::fabs(distance) > 1e-9)
			{
				moves.emplace_back(f, distance);
			}
		}

		transaction_scope tx(*d, "Move faces");
		bool any = false;
		std::string first_error;

		for (const auto& [f, distance] : moves)
		{
			if (move_face(f, distance))
			{
				any = true;
			}
			else if (first_error.empty())
			{
				first_error = d->last_error;
			}
		}

		if (!any)
		{
			return d->fail(first_error.empty() ? "Move faces: the faces do not move that way" : first_error);
		}

		return true;
	}

	bool document::move_corners(const std::span<const vec3> corners, const vec3& delta)
	{
		if (corners.empty() || length(delta) < 1e-12)
		{
			return d->fail("Move corners: nothing to move");
		}

		transaction_scope tx(*d, "Move corners");
		bool any = false;
		std::string first_error;

		for (const object_id id : std::vector<object_id>(d->sel.objects))
		{
			const brush_geometry* geo = d->geometry(id);
			if (!geo)
			{
				continue;
			}

			// the corners of this brush among the positions
			std::vector<vec3> own;

			for (const vec3& p : geo->vertices)
			{
				for (const vec3& c : corners)
				{
					if (length(p - c) < 0.05)
					{
						own.push_back(p);
						break;
					}
				}
			}

			if (own.empty())
			{
				continue;
			}

			if (move_brush_corners(id, own, delta))
			{
				any = true;
			}
			else if (first_error.empty())
			{
				first_error = d->last_error;
			}
		}

		if (!any)
		{
			return d->fail(first_error.empty() ? "Move corners: none of the positions is a corner of a selected brush" : first_error);
		}

		return true;
	}
}
