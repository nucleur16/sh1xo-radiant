// Tests of the 3D editing part of codol_edit: handles.hpp ( edges, drag faces, cut polygon, ray queries ) and
// document::move_faces / move_corners. Plain checks, no framework, no test data.

#include <codol_edit/codol_edit.hpp>
#include <codol_edit/handles.hpp>

#include <cstdio>
#include <string>

using namespace codol::edit;

static int g_failed = 0;
static int g_checks = 0;

#define CHECK(expr) do { g_checks++; if (!(expr)) { g_failed++; std::printf("FAILED %s:%d  %s\n", __FILE__, __LINE__, #expr); } } while (0)

namespace
{
	bool near(const double a, const double b, const double eps = 1e-6)
	{
		return std::fabs(a - b) <= eps;
	}

	bool near(const vec3& a, const vec3& b, const double eps = 1e-6)
	{
		return near(a.x, b.x, eps) && near(a.y, b.y, eps) && near(a.z, b.z, eps);
	}

	// the face of a brush whose normal is `normal`
	int face_with_normal(const brush_geometry& geo, const vec3& normal)
	{
		for (std::size_t f = 0; f < geo.planes.size(); f++)
		{
			if (dot(geo.planes[f].normal, normal) > 0.999)
			{
				return static_cast<int>(f);
			}
		}

		return -1;
	}

	bool has_corner(const brush_geometry& geo, const vec3& p)
	{
		for (const vec3& v : geo.vertices)
		{
			if (near(v, p, 1e-3))
			{
				return true;
			}
		}

		return false;
	}

	void test_queries()
	{
		document doc;
		const object_id box = doc.create_box({ 0, 0, 0 }, { 64, 64, 64 });
		CHECK(box != no_object);

		const brush_geometry* geo = doc.geometry(box);
		CHECK(geo && geo->valid);

		// ---- edges
		const auto edges = brush_edges(*geo);
		CHECK(edges.size() == 12);

		for (const auto& e : edges)
		{
			CHECK(e.a < e.b);
			CHECK(e.faces[0] >= 0 && e.faces[1] >= 0 && e.faces[0] != e.faces[1]);
			// an edge of an axial box runs along one axis and is 64 long
			CHECK(near(length(geo->vertices[e.a] - geo->vertices[e.b]), 64.0));
		}

		{
			document wedge_doc;
			const object_id wedge = wedge_doc.create_wedge({ 0, 0, 0 }, { 64, 64, 64 });
			CHECK(wedge != no_object);
			CHECK(brush_edges(*wedge_doc.geometry(wedge)).size() == 9);		// a prism: 5 faces, 6 corners
		}

		// ---- ray / plane
		{
			double t = 0.0;
			CHECK(ray_plane({ { 0, 0, 100 }, { 0, 0, -1 } }, { { 0, 0, 1 }, 64.0 }, t) && near(t, 36.0));
			CHECK(!ray_plane({ { 0, 0, 100 }, { 0, 0, 1 } }, { { 0, 0, 1 }, 64.0 }, t));		// behind
			CHECK(!ray_plane({ { 0, 0, 100 }, { 1, 0, 0 } }, { { 0, 0, 1 }, 64.0 }, t));		// parallel
			// distances are in units of the direction
			CHECK(ray_plane({ { 0, 0, 100 }, { 0, 0, -2 } }, { { 0, 0, 1 }, 64.0 }, t) && near(t, 18.0));
		}

		// ---- closest point of a line to a ray
		{
			double t = 0.0;
			// the z axis through ( 10, 0, 0 ) and a ray that crosses it at z = 25
			CHECK(closest_on_line({ { 10, -50, 25 }, { 0, 1, 0 } }, { 10, 0, 0 }, { 0, 0, 1 }, t) && near(t, 25.0));
			// a ray that passes the line at a distance: the foot of the common perpendicular
			CHECK(closest_on_line({ { 30, -50, 40 }, { 0, 1, 0 } }, { 10, 0, 0 }, { 0, 0, 1 }, t) && near(t, 40.0));
			CHECK(!closest_on_line({ { 30, -50, 40 }, { 0, 0, 3 } }, { 10, 0, 0 }, { 0, 0, 1 }, t));	// parallel
		}

		// ---- the faces a drag next to the brush takes
		{
			const int top = face_with_normal(*geo, { 0, 0, 1 });
			const int east = face_with_normal(*geo, { 1, 0, 0 });
			const int north = face_with_normal(*geo, { 0, 1, 0 });
			CHECK(top >= 0 && east >= 0 && north >= 0);

			// looking along +y from the south, the ray passes above the box: the top
			auto faces = drag_faces(*geo, { { 32, -200, 100 }, { 0, 1, 0 } });
			CHECK(faces.size() == 1 && faces[0] == top);

			// .. right of it: the east side
			faces = drag_faces(*geo, { { 100, -200, 32 }, { 0, 1, 0 } });
			CHECK(faces.size() == 1 && faces[0] == east);

			// past the upper right edge: no single side ( the ray is in front of two faces all the way )
			faces = drag_faces(*geo, { { 100, -200, 100 }, { 0, 1, 0 } });
			CHECK(faces.empty());

			// a ray that hits the brush drags nothing
			faces = drag_faces(*geo, { { 32, -200, 32 }, { 0, 1, 0 } });
			CHECK(faces.empty());

			// from above at a slant, the ray comes down beyond the north side: north, not the top it starts over
			faces = drag_faces(*geo, { { 32, 32, 300 }, { 0, 0.5, -1 } });
			CHECK(faces.size() == 1 && faces[0] == north);
		}

		// ---- the polygon a plane cuts out of the brush
		{
			winding w;
			CHECK(cut_winding(*geo, { { 0, 0, 1 }, 16.0 }, w));
			CHECK(w.size() == 4 && near(winding_area(w), 64.0 * 64.0, 1e-3));

			for (const vec3& p : w)
			{
				CHECK(near(p.z, 16.0, 1e-6));
			}

			// a diagonal cut through two opposite edges
			CHECK(cut_winding(*geo, { normalize({ 1, 1, 0 }), 64.0 / std::sqrt(2.0) }, w));
			CHECK(w.size() == 4 && near(winding_area(w), 64.0 * 64.0 * std::sqrt(2.0), 1e-2));

			CHECK(!cut_winding(*geo, { { 0, 0, 1 }, 100.0 }, w));		// above the brush
			CHECK(!cut_winding(*geo, { { 0, 0, 0 }, 0.0 }, w));		// no plane
		}
	}

	void test_move_faces()
	{
		document doc;
		const object_id a = doc.create_box({ 0, 0, 0 }, { 64, 64, 64 });
		const object_id b = doc.create_box({ 64, 0, 0 }, { 128, 64, 32 });
		CHECK(a != no_object && b != no_object);

		const int a_top = face_with_normal(*doc.geometry(a), { 0, 0, 1 });
		const int a_west = face_with_normal(*doc.geometry(a), { -1, 0, 0 });
		const int b_top = face_with_normal(*doc.geometry(b), { 0, 0, 1 });
		const std::size_t undo_before = doc.undo_count();

		// both tops up by 16: one step
		const face_ref tops[] = { { a, a_top }, { b, b_top } };
		CHECK(doc.move_faces(tops, { 0, 0, 16 }));
		CHECK(doc.undo_count() == undo_before + 1);
		CHECK(doc.undo_name() == "Move faces");
		CHECK(near(doc.object_bounds(a).maxs.z, 80.0) && near(doc.object_bounds(b).maxs.z, 48.0));
		CHECK(near(doc.object_bounds(a).mins.z, 0.0));

		// a face only takes the part of the movement along its normal: the west face moves out by 8, the top not at all
		const face_ref mixed[] = { { a, a_top }, { a, a_west } };
		CHECK(doc.move_faces(mixed, { -8, 100, 0 }));
		CHECK(near(doc.object_bounds(a).mins.x, -8.0) && near(doc.object_bounds(a).maxs.z, 80.0));
		CHECK(near(doc.object_bounds(a).mins.y, 0.0) && near(doc.object_bounds(a).maxs.y, 64.0));

		// a movement no face follows, and one that would make the brush vanish: nothing changes, no step
		const std::size_t steps = doc.undo_count();
		const std::uint64_t change = doc.change_counter();
		CHECK(!doc.move_faces(tops, { 5, 5, 0 }));
		const face_ref one[] = { { b, b_top } };
		CHECK(!doc.move_faces(one, { 0, 0, -48 }));
		CHECK(!doc.last_error().empty());
		CHECK(doc.undo_count() == steps && doc.change_counter() == change);

		// one brush would vanish, the other can move: the one that can does
		CHECK(doc.move_faces(tops, { 0, 0, -48 }));
		CHECK(near(doc.object_bounds(a).maxs.z, 32.0) && near(doc.object_bounds(b).maxs.z, 48.0));

		// undo takes each call back as a whole
		CHECK(doc.undo() && doc.undo() && doc.undo());
		CHECK(near(doc.object_bounds(a).maxs.z, 64.0) && near(doc.object_bounds(b).maxs.z, 32.0));
		CHECK(near(doc.object_bounds(a).mins.x, 0.0));

		// texture lock: the texture of a face that moves along its normal stays where it was on the surface
		{
			doc.set_texture_lock(true);
			const tex_def before = doc.find_brush(b)->faces[static_cast<std::size_t>(b_top)].texture;
			CHECK(doc.move_faces(one, { 0, 0, 8 }));
			const tex_def after = doc.find_brush(b)->faces[static_cast<std::size_t>(b_top)].texture;
			CHECK(after.material == before.material && near(after.shift_x, before.shift_x) && near(after.shift_y, before.shift_y)
				&& near(after.rotation, before.rotation) && near(after.size_x, before.size_x) && near(after.size_y, before.size_y));
		}

		CHECK(!doc.move_faces({}, { 0, 0, 8 }));
		const face_ref bad[] = { { a, 99 }, { 9999, 0 } };
		CHECK(!doc.move_faces(bad, { 0, 0, 8 }));
	}

	void test_move_corners()
	{
		document doc;
		const object_id a = doc.create_box({ 0, 0, 0 }, { 64, 64, 64 });
		const object_id b = doc.create_box({ 64, 0, 0 }, { 128, 64, 64 });
		const object_id c = doc.create_box({ 0, 0, 128 }, { 64, 64, 192 });
		CHECK(a != no_object && b != no_object && c != no_object);

		const object_id both[] = { a, b };
		doc.select(both);
		const std::size_t undo_before = doc.undo_count();

		// the upper edge the two brushes share ( x = 64, z = 64 ): up by 32, in both, in one step
		const vec3 edge[] = { { 64, 0, 64 }, { 64, 64, 64 } };
		CHECK(doc.move_corners(edge, { 0, 0, 32 }));
		CHECK(doc.undo_count() == undo_before + 1);
		CHECK(doc.undo_name() == "Move corners");

		for (const object_id id : both)
		{
			const brush_geometry* geo = doc.geometry(id);
			CHECK(geo && geo->valid);
			CHECK(has_corner(*geo, { 64, 0, 96 }) && has_corner(*geo, { 64, 64, 96 }));
			CHECK(!has_corner(*geo, { 64, 0, 64 }));
			CHECK(near(doc.object_bounds(id).maxs.z, 96.0));
			// the top is one slanted plane now: still 6 faces, 8 corners
			CHECK(geo->vertices.size() == 8 && geo->face_count_with_winding == 6);
		}

		// the brush that is not selected stays, even though nothing else is at those positions
		CHECK(near(doc.object_bounds(c).mins.z, 128.0));

		// one corner alone: the sides it was on are no longer planar, the hull splits them into triangles
		const vec3 corner[] = { { 0, 0, 64 } };
		CHECK(doc.move_corners(corner, { 0, 0, 16 }));
		{
			const brush_geometry* geo = doc.geometry(a);
			CHECK(geo && geo->valid && has_corner(*geo, { 0, 0, 80 }));
			CHECK(geo->face_count_with_winding > 6);
			CHECK(doc.geometry(b)->face_count_with_winding == 6);	// b has no corner there
		}

		// positions that are no corner of a selected brush; a movement that flattens the brush
		const std::size_t steps = doc.undo_count();
		const vec3 nowhere[] = { { 1, 2, 3 } };
		CHECK(!doc.move_corners(nowhere, { 0, 0, 8 }));
		const vec3 c_corner[] = { { 0, 0, 192 } };
		CHECK(!doc.move_corners(c_corner, { 0, 0, 8 }));		// c is not selected
		CHECK(!doc.move_corners(edge, { 0, 0, 0 }));
		CHECK(doc.undo_count() == steps);

		CHECK(doc.undo() && doc.undo());
		CHECK(near(doc.object_bounds(a).maxs.z, 64.0) && near(doc.object_bounds(b).maxs.z, 64.0));
		CHECK(doc.is_selected(a) && doc.is_selected(b));

		// all four upper corners of a box down onto the floor: the brush would be flat, it stays as it is
		doc.select(c);
		const vec3 top[] = { { 0, 0, 192 }, { 64, 0, 192 }, { 0, 64, 192 }, { 64, 64, 192 } };
		CHECK(!doc.move_corners(top, { 0, 0, -64 }));
		CHECK(near(doc.object_bounds(c).maxs.z, 192.0));
		CHECK(doc.move_corners(top, { 0, 0, -32 }));
		CHECK(near(doc.object_bounds(c).maxs.z, 160.0));
	}
}

int main()
{
	test_queries();
	test_move_faces();
	test_move_corners();

	std::printf("\ncodol_edit_3d_tests: %d checks, %d failed\n", g_checks, g_failed);
	return g_failed == 0 ? 0 : 1;
}
