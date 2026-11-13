#pragma once

// codol_edit: what a perspective view needs to edit geometry with the mouse - the edges of a brush, the faces a
// drag next to a brush takes hold of ( Radiant's "drag outside the brush" ), the polygon a clip plane cuts out of
// a brush, and the two ray queries every drag is made of. Free functions over brush_geometry; no document state.

#include "codol_edit/geometry.hpp"

namespace codol::edit
{
	struct brush_edge
	{
		int a = 0, b = 0;		// indices into brush_geometry::vertices, a < b
		int faces[2] = { -1, -1 };	// the two faces that meet in the edge ( -1: the brush is not closed there )
	};

	// every edge of the brush once
	[[nodiscard]] std::vector<brush_edge> brush_edges(const brush_geometry& geo);

	// Where the ray meets the plane, in units of |ray.direction|. False when the ray is parallel to the plane or the
	// plane is behind the origin.
	bool ray_plane(const ray& r, const plane& p, double& distance);

	// The point of the line `line_origin + t * line_direction` that is closest to the ray: its `t`, in units of
	// |line_direction|. False when ray and line are parallel. ( A drag along an axis: the mouse ray never meets the
	// axis exactly. )
	bool closest_on_line(const ray& r, const vec3& line_origin, const vec3& line_direction, double& t);

	// The faces a drag that starts next to the brush moves, as CoD4 Radiant picks them: a face is taken when the ray
	// passes the brush on that face's side - through the space that is behind every other face and in front of this
	// one - and the ray does not start in that space. Empty when the ray hits the brush, or passes it on no side
	// ( it runs past a corner ).
	[[nodiscard]] std::vector<int> drag_faces(const brush_geometry& geo, const ray& r);

	// the polygon `p` cuts out of the brush, counter clockwise seen from the front of `p`; false when the plane
	// misses the brush
	bool cut_winding(const brush_geometry& geo, const plane& p, winding& out);
}
