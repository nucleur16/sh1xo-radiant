#pragma once

// codol_edit: geometry of brushes and patches. Free functions over the model objects; document caches the
// brush_geometry of every brush per revision ( document::geometry ), so the UI normally only calls
// document::build_mesh.

#include "codol_edit/model.hpp"

#include <functional>
#include <span>

namespace codol::edit
{
	inline constexpr double ON_EPSILON = 0.01;			// a point this close to a plane is on it ( classification: clip, csg, queries )
	inline constexpr double CLIP_EPSILON = 1e-6;		// the same when windings are built: near exact, so that faces meet along the same lines
	inline constexpr double SAME_PLANE_EPSILON = 1e-3;	// two faces that deviate less than this over the brush are one plane
	inline constexpr double WELD_EPSILON = 0.02;		// corners closer than this are one vertex ( removes slivers thinner than that )

	// convex polygon, counter clockwise when seen from the front of its plane ( from outside the brush )
	using winding = std::vector<vec3>;

	// a square of +-`extent` on the plane
	winding base_winding(const plane& p, double extent = 1048576.0);
	// keeps the part behind `p` ( dist <= eps ). Empty result when nothing is strictly behind the plane.
	void clip_winding(winding& w, const plane& p, double eps = CLIP_EPSILON);
	double winding_area(const winding& w);

	struct brush_geometry
	{
		std::vector<plane> planes;						// per face; normal 0 0 0 for a face with collinear points
		std::vector<vec3> vertices;						// corners, welded
		std::vector<std::vector<int>> face_vertices;	// per face: indices into `vertices`, counter clockwise from outside; empty: the face has no winding
		bounds box;
		int face_count_with_winding = 0;
		bool closed = false;	// every edge is shared by exactly two faces, in opposite directions
		bool valid = false;		// >= 4 faces with a winding, closed, inside the world

		[[nodiscard]] winding face_winding(int face) const;
	};

	// windings by clipping each face plane against all others. Returns out.valid.
	bool build_brush_geometry(const brush& b, brush_geometry& out);

	// ---- texture coordinates ------------------------------------------------------------------

	// material name -> size of its colour map in texels; false / not set: 256 x 256
	using texture_size_fn = std::function<bool(std::string_view material, int& width, int& height)>;

	// The two texture axes of a face: the base axes of the plane ( the world axis pair the normal is most
	// perpendicular to, quake style ) rotated by tex.rotation. Unit length.
	void face_texture_axes(const vec3& normal, double rotation, vec3& u_axis, vec3& v_axis);

	// uv = ( p . u_axis / size_x + shift_x / tex_width, p . v_axis / size_y + shift_y / tex_height )
	vec2 face_uv(const vec3& point, const vec3& normal, const tex_def& tex, int tex_width, int tex_height);

	// ---- render meshes ------------------------------------------------------------------------

	struct mesh_vertex
	{
		float position[3];
		float normal[3];
		float uv[2];
		float lightmap_uv[2];
		std::uint8_t color[4];	// r g b a ( patch vertex colour, 255 for brushes )
	};

	// consecutive triangles with one material: one face of a brush, or a whole patch
	struct mesh_part
	{
		std::string material;
		int face = -1;				// brush face index, -1 for a patch
		std::uint32_t first_index = 0;
		std::uint32_t index_count = 0;
	};

	struct mesh
	{
		std::vector<mesh_vertex> vertices;
		std::vector<std::uint32_t> indices;		// triangle list, counter clockwise from the visible side
		std::vector<mesh_part> parts;
		bounds box;
		std::uint64_t revision = 0;				// revision of the object it was built from

		void clear();
	};

	// one fan per face with a winding; appends to `out` ( call out.clear() first for a fresh mesh )
	void build_brush_mesh(const brush& b, const brush_geometry& geo, const texture_size_fn& texture_size, mesh& out);

	// `subdivisions`: quads per bezier segment of a curve ( 1 .. 32 ); a terrain mesh is one quad per cell whatever
	// the value. Normals are computed from the tessellated surface ( cross( along columns, along rows ) ).
	// Patch vertices carry their texture coordinates in texels, the mesh gets them divided by the texture size;
	// the lightmap pair is passed through as it is. Appends to `out`.
	void build_patch_mesh(const patch& p, int subdivisions, const texture_size_fn& texture_size, mesh& out);

	// tessellated points only ( picking, bounds ): `columns` x `rows` grid, points[ column * rows + row ]
	void tessellate_patch(const patch& p, int subdivisions, std::vector<vec3>& points, int& columns, int& rows);

	// ---- intersection -------------------------------------------------------------------------

	// ray against the convex volume. `distance` in units of |ray.direction|. A ray starting inside does not hit.
	bool ray_brush(const ray& r, const brush_geometry& geo, double& distance, int& face);
	bool ray_box(const ray& r, const bounds& box, double& distance);
	bool ray_triangle(const ray& r, const vec3& a, const vec3& b, const vec3& c, double& distance, bool two_sided = true);

	// ---- construction helpers -----------------------------------------------------------------

	// three points for a plane, laid out like radiant does for a new face; integers when the plane is axial and
	// on the grid
	void points_for_plane(const plane& p, vec3 out[3]);

	// planes of the convex hull of a point set ( brute force, meant for the <= 128 corners of a brush );
	// false when the points are coplanar. hull_faces[i] lists the points on plane i, counter clockwise from outside.
	bool convex_hull(std::span<const vec3> points, std::vector<plane>& planes, std::vector<std::vector<int>>& hull_faces);
}
