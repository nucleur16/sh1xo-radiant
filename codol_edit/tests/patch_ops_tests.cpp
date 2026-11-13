// Tests of patch_ops.hpp: brush primitives and the operations of the Patch menu. Plain checks, no framework, no
// test data outside this file.

#include <codol_edit/codol_edit.hpp>
#include <codol_edit/patch_ops.hpp>

#include <cstdio>

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

	// a map in the formatting CoD4 radiant writes: a terrain mesh with colours, lightmap coordinates and turned
	// edges, a curve with contents / tool flags / smoothing, a curve without any of it
	std::string sample_map()
	{
		const char* lines[] =
		{
			"iwmap 4",
			"\"000_Global\" flags  active",
			"\"The Map\" flags ",
			"// entity 0",
			"{",
			"\"classname\" \"worldspawn\"",
			"// brush 0",
			" {",
			"  mesh",
			"  {",
			"  contents nonColliding;",
			"  toolFlags splitGeo;",
			"   ch_grass_01",
			"   lightmap_gray",
			"   smoothing smoothing_hard",
			"   3 3 16 8",
			"   (",
			"\tv 0 0 0 c 255 255 255 255 t 0 512 0 0 f 1",
			"\tv 0 64 4 c 255 128 0 255 t 0 256 0 4",
			"\tv 0 128 0 c 255 255 255 255 t 0 0 0 8",
			"   )",
			"   (",
			"\tv 64 0 2.5 c 255 255 255 128 t 256 512 4 0",
			"\tv 64 64 16 c 255 255 255 255 t 256 256 4 4 f 1",
			"\tv 64 128 -1.25 c 255 255 255 255 t 256 0 4 8",
			"   )",
			"   (",
			"\tv 128 0 0 c 255 255 255 255 t 512 512 8 0",
			"\tv 128 64 0 c 0 0 0 255 t 512 256 8 4",
			"\tv 128 128 0 c 255 255 255 255 t 512 0 8 8",
			"   )",
			"  }",
			" }",
			"// brush 1",
			" {",
			"  curve",
			"  {",
			"  contents detail;",
			"  toolFlags splitGeo;",
			"   ch_concrete_01",
			"   lightmap_gray",
			"   smoothing smoothing_soft",
			"   5 3 16 8",
			"   (",
			"\tv 256 0 0 c 255 255 255 255 t 0 256 0 0",
			"\tv 256 0 32 c 255 255 255 255 t 0 128 0 2",
			"\tv 256 0 64 c 255 255 255 255 t 0 0 0 4",
			"   )",
			"   (",
			"\tv 288 -32 0 c 255 255 255 255 t 128 256 2 0",
			"\tv 288 -32 32 c 255 255 255 255 t 128 128 2 2",
			"\tv 288 -32 64 c 255 255 255 255 t 128 0 2 4",
			"   )",
			"   (",
			"\tv 320 0 0 c 255 255 255 255 t 256 256 4 0",
			"\tv 320 0 32 c 255 255 255 255 t 256 128 4 2",
			"\tv 320 0 64 c 255 255 255 255 t 256 0 4 4",
			"   )",
			"   (",
			"\tv 352 32 0 c 255 255 255 255 t 384 256 6 0",
			"\tv 352 32 32 c 255 255 255 255 t 384 128 6 2",
			"\tv 352 32 64 c 255 255 255 255 t 384 0 6 4",
			"   )",
			"   (",
			"\tv 384 0 0 c 255 255 255 255 t 512 256 8 0",
			"\tv 384 0 32 c 255 255 255 255 t 512 128 8 2",
			"\tv 384 0 64 c 255 255 255 255 t 512 0 8 4",
			"   )",
			"  }",
			" }",
			"// brush 2",
			" {",
			"  curve",
			"  {",
			"   ch_metal_01",
			"   lightmap_gray",
			"   3 3 16 12",
			"   (",
			"\tv 0 256 0 t 0 0 0 0",
			"\tv 0 256 32 t 0 128 0 2",
			"\tv 0 256 64 t 0 256 0 4",
			"   )",
			"   (",
			"\tv 32 288 0 t 128 0 2 0",
			"\tv 32 288 32 t 128 128 2 2",
			"\tv 32 288 64 t 128 256 2 4",
			"   )",
			"   (",
			"\tv 64 256 0 t 256 0 4 0",
			"\tv 64 256 32 t 256 128 4 2",
			"\tv 64 256 64 t 256 256 4 4",
			"   )",
			"  }",
			" }",
			"}",
		};

		std::string out;
		for (const char* line : lines)
		{
			out += line;
			out += "\n";
		}

		return out;
	}

	const patch* only_selected_patch(const document& doc)
	{
		return doc.selection().size() == 1 ? doc.find_patch(doc.selection()[0]) : nullptr;
	}

	// the tessellated surface as a list of points, to compare shapes before and after an operation
	std::vector<vec3> surface(const document& doc, const object_id id, const int subdivisions)
	{
		std::vector<vec3> points;
		int columns = 0, rows = 0;
		tessellate_patch(*doc.find_patch(id), subdivisions, points, columns, rows);
		return points;
	}

	// the largest distance of a point of `a` to the nearest point of `b`
	double deviation(const std::vector<vec3>& a, const std::vector<vec3>& b)
	{
		double worst = 0.0;
		for (const vec3& p : a)
		{
			double best = 1e30;
			for (const vec3& q : b)
			{
				best = std::min(best, length(p - q));
			}

			worst = std::max(worst, best);
		}

		return worst;
	}

	vec3 mesh_normal(const document& doc, const object_id id)
	{
		mesh m;
		doc.build_mesh(id, m, 4);

		vec3 n;
		for (std::size_t i = 0; i + 2 < m.indices.size(); i += 3)
		{
			const auto pos = [&](const std::size_t k)
			{
				const mesh_vertex& v = m.vertices[m.indices[k]];
				return vec3{ v.position[0], v.position[1], v.position[2] };
			};

			n += cross(pos(i + 1) - pos(i), pos(i + 2) - pos(i));
		}

		return n;
	}

	// ---- .map round trip ------------------------------------------------------------------------

	void test_round_trip()
	{
		const std::string text = sample_map();

		document doc;
		std::string error;
		CHECK(doc.load_map_text(text, error));
		CHECK(doc.load_warnings().empty());
		CHECK(doc.save_map_text() == text);
		CHECK(doc.save_map_text_canonical() == text);
		CHECK(doc.patch_count() == 3);

		const entity* world = doc.find_entity(doc.worldspawn());
		const patch* terrain = doc.find_patch(world->children[0]);
		const patch* curve = doc.find_patch(world->children[1]);
		const patch* plain = doc.find_patch(world->children[2]);

		CHECK(terrain && terrain->type == patch_type::mesh && terrain->width == 3 && terrain->height == 3);
		CHECK(terrain && terrain->at(0, 0).has_flag && terrain->at(0, 0).flag == 1 && terrain->at(1, 1).flag == 1 && !terrain->at(1, 0).has_flag);
		CHECK(terrain && terrain->at(1, 0).color[3] == 128 && terrain->at(2, 1).color[0] == 0 && near(terrain->at(1, 2).xyz.z, -1.25));
		CHECK(terrain && terrain->at(2, 2).tex.size() == 4 && terrain->at(2, 2).tex[2] == 8 && terrain->smoothing == "smoothing_hard");
		CHECK(curve && curve->type == patch_type::curve && curve->width == 5 && curve->height == 3 && curve->smoothing == "smoothing_soft");
		CHECK(curve && curve->is_detail() && curve->tool_flags.size() == 1 && patch_subdivisions(*curve) == 2);
		// the size line: columns rows, lightmap sample size, subdivision ( an error bound in units: | P0 - 2 P1 + P2 | is 64 for
		// both curves, so 8 and 12 give 2 quads per segment )
		CHECK(curve && curve->has_lightmap_sample_size && curve->lightmap_sample_size == 16.0 && curve->subdivision == 8);
		CHECK(plain && !plain->at(0, 0).has_color && plain->subdivision == 12 && patch_subdivisions(*plain) == 2 && patch_subdivisions(*terrain) == 1);

		// an edit and its undo: byte identical again; every operation of this file writes a file that reads back the same
		doc.select(world->children[1]);
		CHECK(patch_invert(doc));
		CHECK(doc.save_map_text() != text);
		CHECK(doc.undo());
		CHECK(doc.save_map_text() == text);

		doc.select_all();
		CHECK(patch_insert(doc, patch_line::column, true));
		CHECK(patch_thicken(doc, 8.0, true));
		CHECK(patch_cap(doc));

		const std::string edited = doc.save_map_text();
		document again;
		CHECK(again.load_map_text(edited, error));
		CHECK(again.load_warnings().empty());
		CHECK(again.save_map_text() == edited);
		CHECK(again.save_map_text_canonical() == edited);
		CHECK(again.patch_count() == doc.patch_count());

		while (doc.can_undo())
		{
			doc.undo();
		}

		CHECK(doc.save_map_text() == text);
	}

	// ---- brush primitives -----------------------------------------------------------------------

	void test_brushes()
	{
		document doc;
		const vec3 mins{ -64, -32, 0 }, maxs{ 64, 32, 128 };

		const auto check_shape = [&](const brush_shape shape, const int sides, const int axis, const std::size_t faces)
		{
			const object_id id = create_brush_shape(doc, shape, mins, maxs, sides, axis);
			CHECK(id != no_object);
			if (!id)
			{
				std::printf("   %s\n", doc.last_error().c_str());
				return id;
			}

			const brush* b = doc.find_brush(id);
			const brush_geometry* geo = doc.geometry(id);
			CHECK(b && geo && geo->valid && geo->closed);
			CHECK(b && b->faces.size() == faces);
			CHECK(b && b->faces[0].texture.material == "caulk" && b->faces[0].lightmap.material == "lightmap_gray");
			CHECK(doc.selection().size() == 1 && doc.selection()[0] == id);

			// inside the box along the axis, and touching both ends of it
			const bounds box = doc.object_bounds(id);
			CHECK(near(box.mins[axis], mins[axis], 1e-3) && near(box.maxs[axis], maxs[axis], 1e-3));
			return id;
		};

		check_shape(brush_shape::box, 0, 2, 6);
		check_shape(brush_shape::wedge, 0, 2, 5);

		// radiant's sided brushes: 4 sides is the box itself, 8 sides touch the box with every second face
		{
			const object_id four = check_shape(brush_shape::prism, 4, 2, 6);
			const bounds box = doc.object_bounds(four);
			CHECK(near(box.mins, mins, 1e-3) && near(box.maxs, maxs, 1e-3));

			const object_id eight = check_shape(brush_shape::prism, 8, 2, 10);
			const bounds box8 = doc.object_bounds(eight);
			CHECK(near(box8.mins, mins, 1e-3) && near(box8.maxs, maxs, 1e-3));

			const object_id along_x = check_shape(brush_shape::prism, 6, 0, 8);
			CHECK(near(doc.object_bounds(along_x).mins.x, mins.x, 1e-3));
		}

		check_shape(brush_shape::cone, 8, 2, 9);
		check_shape(brush_shape::cone, 5, 1, 6);

		// sphere: 8 slices, 4 bands -> 32 faces, 26 corners, inside the box and touching it at the poles and the equator
		{
			const object_id sphere = check_shape(brush_shape::sphere, 8, 2, 32);
			const brush_geometry* geo = doc.geometry(sphere);
			CHECK(geo && geo->vertices.size() == 26);

			const bounds box = doc.object_bounds(sphere);
			CHECK(near(box.mins, mins, 1e-3) && near(box.maxs, maxs, 1e-3));

			// every corner lies on the ellipsoid
			bool on = true;
			for (const vec3& v : geo->vertices)
			{
				const double e = (v.x / 64.0) * (v.x / 64.0) + (v.y / 32.0) * (v.y / 32.0) + ((v.z - 64.0) / 64.0) * ((v.z - 64.0) / 64.0);
				on = on && near(e, 1.0, 1e-3);
			}

			CHECK(on);
			check_shape(brush_shape::sphere, 16, 0, 16 * 8);
		}

		CHECK(!create_brush_shape(doc, brush_shape::sphere, mins, maxs, 3, 2) && !doc.last_error().empty());
		CHECK(!create_brush_shape(doc, brush_shape::prism, mins, maxs, 2, 2));
		CHECK(!create_brush_shape(doc, brush_shape::prism, mins, mins, 8, 2));

		// one undo step per shape
		{
			document d2;
			CHECK(create_brush_shape(d2, brush_shape::sphere, mins, maxs, 8, 2, "ch_metal_01"));
			CHECK(d2.undo_count() == 1 && d2.undo_name() == "Create sphere");
			CHECK(d2.find_brush(d2.selection()[0])->faces[3].texture.material == "ch_metal_01");
			CHECK(d2.undo() && d2.brush_count() == 0);
		}

		// the selected brush becomes the shape: same id, entity, layer and material; the others go
		{
			document d2;
			const object_id a = d2.create_box({ 0, 0, 0 }, { 64, 64, 64 }, "wall_a");
			const object_id b = d2.create_box({ 64, 0, 0 }, { 128, 64, 32 }, "wall_b");
			const std::pair<std::string, std::string> pairs[] = { { "targetname", "t" } };

			d2.select(a);
			const object_id ent = d2.create_brush_entity("script_brushmodel", pairs);
			CHECK(ent != no_object);

			const object_id both[] = { a, b };
			d2.select(both);
			const std::size_t undo = d2.undo_count();

			const object_id made = brush_shape_from_selection(d2, brush_shape::prism, 8, 2);
			CHECK(made == a && d2.undo_count() == undo + 1);
			CHECK(d2.find_brush(a) && d2.find_brush(a)->faces.size() == 10 && d2.find_brush(a)->owner == ent);
			CHECK(d2.find_brush(a)->faces[0].texture.material == "wall_a");
			CHECK(!d2.find_brush(b) && d2.brush_count() == 1);
			CHECK(d2.selection().size() == 1 && d2.selection()[0] == a);

			const bounds box = d2.object_bounds(a);
			CHECK(near(box.mins, { 0, 0, 0 }, 1e-3) && near(box.maxs, { 128, 64, 64 }, 1e-3));

			CHECK(d2.undo());
			CHECK(d2.find_brush(a)->faces.size() == 6 && d2.find_brush(b) && d2.selection().size() == 2);

			d2.deselect_all();
			CHECK(!brush_shape_from_selection(d2, brush_shape::sphere, 8, 2));
		}
	}

	// ---- creating patches -----------------------------------------------------------------------

	void test_create()
	{
		document doc;
		const vec3 mins{ 0, 0, 0 }, maxs{ 128, 64, 32 };

		// flat curve: 5 x 3 control points in the plane, facing up, 4 texels per unit
		{
			const object_id id = create_curve_patch(doc, mins, maxs, 5, 3, 2, "ch_grass_01");
			const patch* p = doc.find_patch(id);
			CHECK(p && p->type == patch_type::curve && p->width == 5 && p->height == 3 && p->vertices.size() == 15);
			CHECK(p && p->material == "ch_grass_01" && p->lightmap == "lightmap_gray");
			CHECK(p && near(p->at(0, 0).xyz, { 0, 0, 0 }) && near(p->at(4, 2).xyz, { 128, 64, 0 }) && near(p->at(2, 1).xyz, { 64, 32, 0 }));
			CHECK(p && near(p->at(4, 0).tex[0], 512) && near(p->at(0, 0).tex[1], 256) && near(p->at(0, 2).tex[1], 0) && near(p->at(4, 2).tex[2], 8));
			CHECK(mesh_normal(doc, id).z > 0.0);
			CHECK(doc.undo_name() == "Create curve" && doc.selection().size() == 1);

			CHECK(!create_curve_patch(doc, mins, maxs, 4, 3));
			CHECK(!create_curve_patch(doc, mins, maxs, 3, 33));
			CHECK(!create_curve_patch(doc, mins, { 0, 64, 32 }, 3, 3));
			CHECK(create_curve_patch(doc, mins, maxs, 3, 3, 0) && near(only_selected_patch(doc)->at(2, 2).xyz, { 0, 64, 32 }));
		}

		struct expect
		{
			patch_shape shape;
			int width, height;
			bool closed;
		};

		const expect shapes[] = {
			{ patch_shape::bevel, 3, 3, false }, { patch_shape::square_bevel, 5, 3, false }, { patch_shape::endcap, 5, 3, false },
			{ patch_shape::square_endcap, 7, 3, false }, { patch_shape::cylinder, 9, 3, true }, { patch_shape::dense_cylinder, 9, 5, true },
			{ patch_shape::very_dense_cylinder, 9, 9, true }, { patch_shape::square_cylinder, 9, 3, true }, { patch_shape::cone, 9, 3, true } };

		for (const expect& e : shapes)
		{
			const object_id id = create_patch_shape(doc, e.shape, mins, maxs, 2);
			const patch* p = doc.find_patch(id);
			CHECK(p && p->type == patch_type::curve && p->width == e.width && p->height == e.height);
			if (!p)
			{
				continue;
			}

			CHECK(near(p->at(0, 0).xyz, p->at(p->width - 1, 0).xyz) == e.closed);

			// inside the box, from its bottom to its top
			const bounds box = doc.object_bounds(id);
			CHECK(box.mins.x >= -1e-6 && box.mins.y >= -1e-6 && near(box.mins.z, 0) && box.maxs.x <= 128 + 1e-6 && box.maxs.y <= 64 + 1e-6 && near(box.maxs.z, 32));

			// it faces away from the axis of the box
			mesh m;
			CHECK(doc.build_mesh(id, m, 4));
			int outward = 0, inward = 0;
			for (std::size_t i = 0; i + 2 < m.indices.size(); i += 3)
			{
				const auto pos = [&](const std::size_t k)
				{
					const mesh_vertex& v = m.vertices[m.indices[k]];
					return vec3{ v.position[0], v.position[1], v.position[2] };
				};

				const vec3 n = cross(pos(i + 1) - pos(i), pos(i + 2) - pos(i));
				const vec3 mid = (pos(i) + pos(i + 1) + pos(i + 2)) * (1.0 / 3.0);
				const vec3 from_axis{ (mid.x - 64.0) / 64.0, (mid.y - 32.0) / 32.0, 0.0 };
				if (length(n) > 1e-6 && length(from_axis) > 0.05)
				{
					(dot(n, from_axis) > 0.0 ? outward : inward)++;
				}
			}

			CHECK(outward > 0 && inward == 0);
		}

		// the cylinder: the on-curve points lie on the middle of the box sides, the corners of the box are control points
		{
			const object_id id = create_patch_shape(doc, patch_shape::cylinder, mins, maxs, 2);
			const patch* p = doc.find_patch(id);
			CHECK(near(p->at(0, 0).xyz, { 0, 32, 0 }) && near(p->at(1, 0).xyz, { 0, 0, 0 }) && near(p->at(2, 0).xyz, { 64, 0, 0 }) && near(p->at(4, 2).xyz, { 128, 32, 32 }));
			// texels follow the outline: a quarter of it is 64 + 32 units of control polygon
			CHECK(near(p->at(2, 0).tex[0], (32 + 64) * 4.0) && near(p->at(0, 2).tex[1], 0) && near(p->at(0, 0).tex[1], 32 * 4.0));

			// the cone ends in one point
			const object_id cone = create_patch_shape(doc, patch_shape::cone, mins, maxs, 2);
			const patch* c = doc.find_patch(cone);
			CHECK(near(c->at(0, 2).xyz, { 64, 32, 32 }) && near(c->at(5, 2).xyz, { 64, 32, 32 }) && near(c->at(3, 1).xyz, { 96, 16, 16 }));

			// along another axis
			const object_id lying = create_patch_shape(doc, patch_shape::cylinder, mins, maxs, 0);
			const bounds box = doc.object_bounds(lying);
			CHECK(near(box.mins.x, 0) && near(box.maxs.x, 128) && doc.find_patch(lying)->at(0, 0).xyz.x == 0 && doc.find_patch(lying)->at(0, 2).xyz.x == 128);
		}

		CHECK(!create_patch_shape(doc, patch_shape::cylinder, mins, { 128, 64, 0 }, 2));
	}

	// ---- rows and columns -----------------------------------------------------------------------

	void test_lines()
	{
		document doc;
		std::string error;
		CHECK(doc.load_map_text(sample_map(), error));

		const entity* world = doc.find_entity(doc.worldspawn());
		const object_id terrain = world->children[0], curve = world->children[1];

		// curve: two more columns, the surface is the same one
		{
			doc.select(curve);
			const std::vector<vec3> before = surface(doc, curve, 16);

			CHECK(patch_insert(doc, patch_line::column, false));
			const patch* p = doc.find_patch(curve);
			CHECK(p->width == 7 && p->height == 3 && doc.undo_name() == "Insert columns");
			CHECK(near(p->at(0, 0).xyz, { 256, 0, 0 }) && near(p->at(1, 0).xyz, { 272, -16, 0 }) && near(p->at(2, 0).xyz, { 288, -16, 0 }) && near(p->at(4, 0).xyz, { 320, 0, 0 }));
			CHECK(near(p->at(2, 0).tex[0], 128) && near(p->at(1, 0).tex[0], 64));
			CHECK(deviation(surface(doc, curve, 32), before) < 4.0 && deviation(before, surface(doc, curve, 32)) < 1e-6);

			CHECK(patch_insert(doc, patch_line::column, true));
			CHECK(doc.find_patch(curve)->width == 9 && near(doc.find_patch(curve)->at(7, 0).xyz, { 368, 16, 0 }));
			CHECK(deviation(before, surface(doc, curve, 32)) < 1e-6);

			CHECK(patch_insert(doc, patch_line::row, false));
			CHECK(doc.find_patch(curve)->height == 5 && doc.find_patch(curve)->width == 9 && near(doc.find_patch(curve)->at(0, 1).xyz, { 256, 0, 16 }));
			CHECK(deviation(before, surface(doc, curve, 32)) < 1e-6);

			// and off again
			CHECK(patch_delete(doc, patch_line::row, true) && doc.find_patch(curve)->height == 3);
			CHECK(!patch_delete(doc, patch_line::row, true));
			CHECK(patch_delete(doc, patch_line::column, false) && doc.find_patch(curve)->width == 7 && near(doc.find_patch(curve)->at(0, 0).xyz, { 288, -16, 0 }));
			CHECK(patch_delete(doc, patch_line::column, true) && doc.find_patch(curve)->width == 5);

			for (int i = 0; i < 6; i++)
			{
				CHECK(doc.undo());
			}

			CHECK(doc.save_map_text() == sample_map());
		}

		// terrain
		{
			doc.select(terrain);
			CHECK(patch_insert(doc, patch_line::column, false));
			const patch* p = doc.find_patch(terrain);
			CHECK(p->width == 5 && near(p->at(1, 1).xyz.x, 64.0 / 3.0) && near(p->at(1, 1).xyz.z, 4 + 12.0 / 3.0) && p->at(1, 0).has_flag && p->at(2, 0).flag == 1);
			CHECK(doc.undo());

			CHECK(patch_insert_terrain(doc, patch_line::column));
			p = doc.find_patch(terrain);
			CHECK(p->width == 4 && near(p->at(2, 1).xyz, { 96, 64, 8 }) && near(p->at(2, 1).tex[0], 384) && p->at(2, 1).has_flag);
			CHECK(patch_insert_terrain(doc, patch_line::row, 0));
			p = doc.find_patch(terrain);
			CHECK(p->height == 4 && near(p->at(0, 1).xyz, { 0, 32, 2 }) && p->at(0, 1).color[1] == 192);

			CHECK(patch_extrude_terrain(doc, patch_line::column, true));
			p = doc.find_patch(terrain);
			CHECK(p->width == 5 && near(p->at(4, 0).xyz, { 160, 0, -1.25 }) && near(p->at(4, 0).tex[0], 640));
			CHECK(patch_extrude_terrain(doc, patch_line::row, false));
			p = doc.find_patch(terrain);
			CHECK(p->height == 5 && near(p->at(0, 0).xyz, { 0, -32, -2 }));

			CHECK(patch_delete_terrain(doc, patch_line::row, 0) && doc.find_patch(terrain)->height == 4 && near(doc.find_patch(terrain)->at(0, 0).xyz, { 0, 0, 0 }));
			CHECK(patch_delete_terrain(doc, patch_line::column) && doc.find_patch(terrain)->width == 4);

			// not for curves
			doc.select(curve);
			CHECK(!patch_insert_terrain(doc, patch_line::column) && !patch_extrude_terrain(doc, patch_line::row) && !patch_delete_terrain(doc, patch_line::row));
			CHECK(doc.last_error().find("terrain") != std::string::npos);
		}

		// the limit
		{
			document d2;
			CHECK(create_curve_patch(d2, { 0, 0, 0 }, { 64, 64, 0 }, 31, 3));
			CHECK(!patch_insert(d2, patch_line::column, true));
			CHECK(patch_insert(d2, patch_line::row, true));
			d2.deselect_all();
			CHECK(!patch_insert(d2, patch_line::row, true) && d2.last_error().find("no patch") != std::string::npos);
		}
	}

	// ---- matrix: transpose, invert, redisperse, split ---------------------------------------------

	void test_matrix()
	{
		document doc;
		std::string error;
		CHECK(doc.load_map_text(sample_map(), error));

		const entity* world = doc.find_entity(doc.worldspawn());
		const object_id terrain = world->children[0], curve = world->children[1];

		// invert: the other side, the same triangles ( also of a terrain with turned edges )
		for (const object_id id : { terrain, curve })
		{
			doc.select(id);
			const vec3 n = mesh_normal(doc, id);
			const std::vector<vec3> before = surface(doc, id, 4);

			mesh m0;
			doc.build_mesh(id, m0, 4);

			CHECK(patch_invert(doc));
			CHECK(dot(mesh_normal(doc, id), n) < 0.0 && near(length(mesh_normal(doc, id)), length(n), 1e-3 * length(n)));
			CHECK(deviation(before, surface(doc, id, 4)) < 1e-9);

			// the centres of the triangles are the same set
			mesh m1;
			doc.build_mesh(id, m1, 4);
			const auto centres = [](const mesh& m)
			{
				std::vector<vec3> out;
				for (std::size_t i = 0; i + 2 < m.indices.size(); i += 3)
				{
					vec3 c;
					for (int k = 0; k < 3; k++)
					{
						const mesh_vertex& v = m.vertices[m.indices[i + static_cast<std::size_t>(k)]];
						c += vec3{ v.position[0], v.position[1], v.position[2] } * (1.0 / 3.0);
					}

					out.push_back(c);
				}

				return out;
			};

			// ( a curve is cut into triangles along the other diagonal: the same surface within the tessellation )
			CHECK(m0.indices.size() == m1.indices.size());
			CHECK(id != terrain || (deviation(centres(m0), centres(m1)) < 1e-4 && deviation(centres(m1), centres(m0)) < 1e-4));

			CHECK(patch_invert(doc));
			CHECK(dot(mesh_normal(doc, id), n) > 0.0);
			CHECK(doc.undo() && doc.undo());
		}

		// transpose
		{
			doc.select(curve);
			const vec3 n = mesh_normal(doc, curve);
			CHECK(patch_transpose(doc));
			const patch* p = doc.find_patch(curve);
			CHECK(p->width == 3 && p->height == 5 && near(p->at(2, 4).xyz, { 384, 0, 64 }) && near(p->at(1, 0).xyz, { 256, 0, 32 }));
			CHECK(dot(mesh_normal(doc, curve), n) < 0.0);
			CHECK(patch_transpose(doc) && doc.undo() && doc.undo());
			CHECK(doc.save_map_text() == sample_map());
		}

		// redisperse
		{
			doc.select(terrain);
			CHECK(patch_redisperse(doc, patch_disperse::columns));
			const patch* p = doc.find_patch(terrain);
			CHECK(near(p->at(1, 1).xyz, { 64, 64, 2 }) && near(p->at(1, 0).xyz, { 64, 0, 0 }) && near(p->at(0, 1).xyz, { 0, 64, 4 }));
			CHECK(!patch_redisperse(doc, patch_disperse::columns));
			CHECK(patch_redisperse(doc, patch_disperse::rows));
			CHECK(near(doc.find_patch(terrain)->at(0, 1).xyz, { 0, 64, 0 }));
			CHECK(doc.undo() && doc.undo());

			// along the lines the points form: uneven columns become even again, the ends stay
			document d2;
			const object_id flat = d2.create_terrain_patch({ 0, 0, 0 }, { 96, 64, 0 }, 4, 2);
			const int column1[] = { 2, 3 };
			CHECK(d2.move_patch_vertices(flat, column1, { -24, 0, 0 }));
			CHECK(near(d2.find_patch(flat)->at(1, 0).xyz, { 8, 0, 0 }));
			CHECK(patch_redisperse(d2, patch_disperse::vertices));
			CHECK(near(d2.find_patch(flat)->at(1, 0).xyz, { 32, 0, 0 }) && near(d2.find_patch(flat)->at(2, 1).xyz, { 64, 64, 0 }) && near(d2.find_patch(flat)->at(3, 1).xyz, { 96, 64, 0 }));
			CHECK(!patch_redisperse(d2, patch_disperse::vertices));
		}

		// split
		{
			doc.select(curve);
			const std::vector<vec3> before = surface(doc, curve, 8);
			CHECK(patch_split(doc, patch_line::column));
			CHECK(doc.selection().size() == 2 && doc.patch_count() == 4 && doc.undo_name() == "Split patch");

			const patch* a = doc.find_patch(doc.selection()[0]);
			const patch* b = doc.find_patch(doc.selection()[1]);
			CHECK(a && b && a->id == curve && a->width == 3 && b->width == 3 && a->height == 3 && b->height == 3);
			CHECK(a && b && near(a->at(2, 0).xyz, { 320, 0, 0 }) && near(b->at(0, 0).xyz, { 320, 0, 0 }) && near(b->at(2, 2).xyz, { 384, 0, 64 }));
			CHECK(b && b->is_detail() && b->owner == a->owner && b->material == "ch_concrete_01" && b->smoothing == "smoothing_soft");

			std::vector<vec3> after = surface(doc, a->id, 8);
			const std::vector<vec3> second = surface(doc, b->id, 8);
			after.insert(after.end(), second.begin(), second.end());
			CHECK(deviation(before, after) < 1e-9 && deviation(after, before) < 1e-9);

			CHECK(!patch_split(doc, patch_line::column));		// 3 wide curves do not split
			CHECK(doc.undo() && doc.patch_count() == 3);

			doc.select(terrain);
			CHECK(patch_split(doc, patch_line::row));
			a = doc.find_patch(doc.selection()[0]);
			b = doc.find_patch(doc.selection()[1]);
			CHECK(a->height == 2 && b->height == 2 && a->width == 3 && near(a->at(1, 1).xyz, { 64, 64, 16 }) && near(b->at(1, 0).xyz, { 64, 64, 16 }));
			CHECK(!a->at(1, 1).has_flag && b->at(1, 0).has_flag && a->at(0, 0).has_flag);
			CHECK(doc.undo());
			CHECK(doc.save_map_text() == sample_map());
		}

		// subdivision
		{
			doc.select(curve);
			mesh m;
			doc.build_mesh(curve, m, patch_subdivisions(*doc.find_patch(curve)));
			CHECK(m.vertices.size() == 5 * 3);

			CHECK(patch_change_subdivision(doc, 1));
			// Inc: the number goes down until the tessellation changes: 3 is the first with 3 quads per segment
			CHECK(doc.find_patch(curve)->subdivision == 3 && patch_subdivisions(*doc.find_patch(curve)) == 3);
			CHECK(doc.save_map_text().find("   5 3 16 3\n") != std::string::npos);
			doc.build_mesh(curve, m, patch_subdivisions(*doc.find_patch(curve)));
			CHECK(m.vertices.size() == 7 * 4);

			CHECK(patch_change_subdivision(doc, 1) && doc.find_patch(curve)->subdivision == 1 && patch_subdivisions(*doc.find_patch(curve)) == 4);
			CHECK(!patch_change_subdivision(doc, 1) && doc.find_patch(curve)->subdivision == 1);		// 1 is the finest
			CHECK(patch_change_subdivision(doc, -1) && doc.find_patch(curve)->subdivision == 2 && patch_subdivisions(*doc.find_patch(curve)) == 3);
			CHECK(patch_change_subdivision(doc, -2) && doc.find_patch(curve)->subdivision == 16 && patch_subdivisions(*doc.find_patch(curve)) == 1);
			CHECK(!patch_change_subdivision(doc, -1) && doc.find_patch(curve)->subdivision == 16);	// one quad is the coarsest
			doc.select(terrain);
			CHECK(!patch_change_subdivision(doc, 1));
		}
	}

	// ---- conversions ----------------------------------------------------------------------------

	void test_conversions()
	{
		document doc;
		std::string error;
		CHECK(doc.load_map_text(sample_map(), error));

		const entity* world = doc.find_entity(doc.worldspawn());
		const object_id terrain = world->children[0], curve = world->children[1], plain = world->children[2];

		// curve -> terrain: the vertices of the tessellation, every attribute interpolated
		{
			// at the subdivision of the file: 8 ( an error bound ) is 2 quads per segment for this curve
			const std::vector<vec3> before = surface(doc, curve, 2);
			const object_id both[] = { curve, plain, terrain };
			doc.select(both);
			CHECK(curve_to_terrain(doc));

			const patch* p = doc.find_patch(curve);
			CHECK(p->type == patch_type::mesh && p->width == 5 && p->height == 3 && p->vertices.size() == 15);
			CHECK(near(p->at(1, 0).xyz, { 288, -16, 0 }) && near(p->at(1, 0).tex[0], 128) && near(p->at(2, 1).xyz, { 320, 0, 32 }) && p->at(1, 1).has_color);
			CHECK(deviation(before, surface(doc, curve, 1)) < 1e-9 && deviation(surface(doc, curve, 1), before) < 1e-9);
			CHECK(p->is_detail() && p->smoothing == "smoothing_soft");

			// one segment each way at 2 quads: 3 x 3 vertices, now a terrain mesh
			CHECK(doc.find_patch(plain)->type == patch_type::mesh && doc.find_patch(plain)->width == 3 && doc.find_patch(plain)->height == 3 && !doc.find_patch(plain)->at(1, 1).has_color);
			CHECK(doc.find_patch(terrain)->width == 3);		// was one already
			CHECK(doc.undo_count() == 1 && !curve_to_terrain(doc));
			CHECK(doc.undo() && doc.save_map_text() == sample_map());
		}

		// faces -> terrain
		{
			document d2;
			const object_id box = d2.create_box({ 0, 0, 0 }, { 64, 64, 64 }, "wall_a");
			CHECK(faces_to_terrain(d2));
			CHECK(d2.patch_count() == 6 && d2.selection().size() == 6 && d2.brush_count() == 1 && d2.undo_name() == "Faces to terrain");

			for (const object_id id : d2.selection())
			{
				const patch* p = d2.find_patch(id);
				CHECK(p && p->type == patch_type::mesh && p->width == 2 && p->height == 2 && p->material == "wall_a");

				vec3 centre;
				for (const patch_vertex& v : p->vertices)
				{
					centre += v.xyz * 0.25;
				}

				CHECK(dot(mesh_normal(d2, id), centre - vec3{ 32, 32, 32 }) > 0.0);

				// 64 units are one repeat of a 256 texel texture at radiant's default scale
				double smin = 1e30, smax = -1e30;
				for (const patch_vertex& v : p->vertices)
				{
					smin = std::min(smin, v.tex[0]);
					smax = std::max(smax, v.tex[0]);
				}

				CHECK(near(smax - smin, 256.0, 1e-6));
			}

			CHECK(d2.undo());

			// one face, and the brush goes
			d2.deselect_all();
			d2.select_face({ box, 4 });
			CHECK(faces_to_terrain(d2, true));
			CHECK(d2.patch_count() == 1 && d2.brush_count() == 0 && d2.selection().size() == 1);
			CHECK(d2.undo() && d2.brush_count() == 1 && d2.patch_count() == 0);

			// a face with more corners is left alone
			CHECK(create_brush_shape(d2, brush_shape::prism, { 0, 0, 0 }, { 64, 64, 64 }, 8));
			CHECK(faces_to_terrain(d2) && d2.patch_count() == 8);
			d2.deselect_all();
			CHECK(!faces_to_terrain(d2));
		}
	}

	// ---- welding --------------------------------------------------------------------------------

	void test_weld()
	{
		{
			document doc;
			const object_id a = doc.create_terrain_patch({ 0, 0, 0 }, { 64, 64, 0 }, 2, 2);
			const object_id b = doc.create_terrain_patch({ 64.05, 0, 0 }, { 128, 64, 0 }, 2, 2);
			const object_id both[] = { a, b };
			doc.select(both);

			CHECK(!patch_weld(doc, 0.01));
			CHECK(patch_weld(doc, 0.1));
			CHECK(doc.find_patch(b)->at(0, 0).xyz == vec3(64, 0, 0) && doc.find_patch(b)->at(0, 1).xyz == vec3(64, 64, 0) && near(doc.find_patch(b)->at(1, 0).xyz, { 128, 0, 0 }));
			CHECK(doc.find_patch(a)->at(1, 0).xyz == vec3(64, 0, 0));
			CHECK(!patch_weld(doc, 0.1) && doc.undo_name() == "Weld patches");

			// to brush corners
			const object_id box = doc.create_box({ 0, 0, -16 }, { 64, 64, 0 });
			CHECK(box != no_object);
			const object_id c = doc.create_terrain_patch({ 0.04, 0.03, 0 }, { 64, 64.02, 0 }, 2, 2);
			CHECK(!patch_weld_to_brushes(doc, 0.01));
			CHECK(patch_weld_to_brushes(doc, 0.1));
			CHECK(doc.find_patch(c)->at(0, 0).xyz == vec3(0, 0, 0) && doc.find_patch(c)->at(1, 1).xyz == vec3(64, 64, 0));
		}

		// moving welded vertices: the seam of a tube stays closed
		{
			document doc;
			const object_id tube = create_patch_shape(doc, patch_shape::cylinder, { 0, 0, 0 }, { 64, 64, 64 });
			const int first[] = { 0 };

			CHECK(move_patch_vertices_welded(doc, tube, first, { -8, 0, 0 }, 0.01));
			CHECK(near(doc.find_patch(tube)->at(0, 0).xyz, { -8, 32, 0 }) && near(doc.find_patch(tube)->at(8, 0).xyz, { -8, 32, 0 }) && near(doc.find_patch(tube)->at(0, 1).xyz, { 0, 32, 32 }));
			CHECK(doc.undo());

			CHECK(move_patch_vertices_welded(doc, tube, first, { -8, 0, 0 }, -1.0));
			CHECK(near(doc.find_patch(tube)->at(0, 0).xyz, { -8, 32, 0 }) && near(doc.find_patch(tube)->at(8, 0).xyz, { 0, 32, 0 }));
			CHECK(doc.undo());

			// locked vertices stay, also the welded ones
			const int locked[] = { 0 };
			CHECK(!move_patch_vertices_welded(doc, tube, first, { -8, 0, 0 }, 0.01, locked));
			const int seam[] = { 24 };
			CHECK(move_patch_vertices_welded(doc, tube, seam, { -8, 0, 0 }, 0.01, locked));
			CHECK(near(doc.find_patch(tube)->at(0, 0).xyz, { 0, 32, 0 }) && near(doc.find_patch(tube)->at(8, 0).xyz, { -8, 32, 0 }));

			// a second selected patch that shares the corner moves along
			document d2;
			const object_id a = d2.create_terrain_patch({ 0, 0, 0 }, { 64, 64, 0 }, 2, 2);
			const object_id b = d2.create_terrain_patch({ 64, 0, 0 }, { 128, 64, 0 }, 2, 2);
			const object_id both[] = { a, b };
			d2.select(both);
			const int corner[] = { 2 };		// column 1, row 0 of a
			CHECK(move_patch_vertices_welded(d2, a, corner, { 0, 0, 16 }, 0.01));
			CHECK(near(d2.find_patch(b)->at(0, 0).xyz, { 64, 0, 16 }) && near(d2.find_patch(b)->at(0, 1).xyz, { 64, 64, 0 }) && d2.undo_count() == 3);
		}

		// the rotate / scale gizmo of the 3D view on a set of vertices: a turn about their centre, welded and locked
		{
			document doc;
			const object_id a = doc.create_terrain_patch({ 0, 0, 0 }, { 128, 128, 0 }, 3, 3);
			const object_id b = doc.create_terrain_patch({ 128, 0, 0 }, { 256, 128, 0 }, 3, 3);
			const object_id both[] = { a, b };
			doc.select(both);
			// the column x 128 of a ( 6 7 8 ), turned 90 degrees about Z through its centre 128 64: ( x y ) -> ( 128 - ( y - 64 ), 64 + ( x - 128 ) )
			transform turn;
			turn.m[0][0] = 0; turn.m[0][1] = -1; turn.m[1][0] = 1; turn.m[1][1] = 0;
			turn.t = vec3(128, 64, 0) - turn.apply_vector({ 128, 64, 0 });
			const int column[] = { 6, 7, 8 };
			const int locked[] = { 8 };
			const auto undo_before = doc.undo_count();
			CHECK(transform_patch_vertices(doc, a, column, turn, 0.01, locked));
			CHECK(doc.undo_count() == undo_before + 1 && doc.undo_name() == "Transform vertices");
			CHECK(near(doc.find_patch(a)->at(2, 0).xyz, { 192, 64, 0 }) && near(doc.find_patch(a)->at(2, 1).xyz, { 128, 64, 0 }) && near(doc.find_patch(a)->at(2, 2).xyz, { 128, 128, 0 }));
			// welded: column 0 of b sat on 6 7 8 and went with them ( 8 is locked: its partner stays too )
			CHECK(near(doc.find_patch(b)->at(0, 0).xyz, { 192, 64, 0 }) && near(doc.find_patch(b)->at(0, 2).xyz, { 128, 128, 0 }));
			CHECK(!transform_patch_vertices(doc, a, std::span<const int>(locked), turn, 0.01, locked));
			CHECK(doc.undo());

			// a scale by 2 about the centre, no weld
			transform twice;
			twice.m[0][0] = twice.m[1][1] = twice.m[2][2] = 2.0;
			twice.t = vec3(128, 64, 0) - twice.apply_vector({ 128, 64, 0 });
			CHECK(transform_patch_vertices(doc, a, column, twice, -1.0));
			CHECK(near(doc.find_patch(a)->at(2, 0).xyz, { 128, -64, 0 }) && near(doc.find_patch(b)->at(0, 0).xyz, { 128, 0, 0 }));

			// the nearest snap point within reach
			const vec3 points[] = { { 10, 0, 0 }, { 0, 3, 0 }, { 0, 0, 30 } };
			vec3 onto;
			CHECK(snap_to_nearest({ 0, 0, 0 }, points, 8.0, onto) && onto == vec3(0, 3, 0));
			CHECK(!snap_to_nearest({ 0, 0, 0 }, points, 2.0, onto));
		}
	}

	// ---- thicken / cap --------------------------------------------------------------------------

	void test_thicken_cap()
	{
		// a flat curve facing up: a second one 8 below facing down, four seams facing out
		{
			document doc;
			const object_id top = create_curve_patch(doc, { 0, 0, 0 }, { 64, 64, 0 }, 3, 3);
			CHECK(patch_thicken(doc, 8.0, true));
			CHECK(doc.patch_count() == 6 && doc.selection().size() == 6 && doc.undo_name() == "Thicken patch");

			int down = 0, sides = 0;
			for (const object_id id : doc.selection())
			{
				if (id == top)
				{
					continue;
				}

				const bounds box = doc.object_bounds(id);
				const vec3 n = normalize(mesh_normal(doc, id));
				const patch* p = doc.find_patch(id);
				CHECK(p->type == patch_type::curve && p->material == doc.find_patch(top)->material);

				if (near(box.maxs.z, -8))
				{
					down += n.z < -0.99;
					CHECK(p->width == 3 && p->height == 3);
				}
				else
				{
					CHECK(near(box.mins.z, -8) && near(box.maxs.z, 0) && near(n.z, 0, 1e-6));
					sides += dot(n, box.center() - vec3{ 32, 32, -4 }) > 0.0;
				}
			}

			CHECK(down == 1 && sides == 4);
			CHECK(doc.undo() && doc.patch_count() == 1);

			// in front, without seams: one more, facing the same way
			CHECK(patch_thicken(doc, -4.0, false));
			CHECK(doc.patch_count() == 2 && near(doc.object_bounds(doc.selection()[1]).mins.z, 4) && mesh_normal(doc, doc.selection()[1]).z > 0.0);
			CHECK(!patch_thicken(doc, 0.0, true));
		}

		// a tube: the inner wall and two rings
		{
			document doc;
			const object_id tube = create_patch_shape(doc, patch_shape::cylinder, { 0, 0, 0 }, { 64, 64, 64 });
			CHECK(patch_thicken(doc, 8.0, true));
			CHECK(doc.patch_count() == 4);

			int inner = 0, rings = 0;
			for (const object_id id : doc.selection())
			{
				if (id == tube)
				{
					continue;
				}

				const bounds box = doc.object_bounds(id);
				const patch* p = doc.find_patch(id);

				if (p->height == 3 && near(box.maxs.z - box.mins.z, 64))
				{
					// 8 inside the tube, closed, and it faces the axis
					CHECK(near(p->at(0, 0).xyz, p->at(8, 0).xyz) && near(p->at(0, 0).xyz, { 8, 32, 0 }) && near(p->at(4, 2).xyz, { 56, 32, 64 }));

					mesh m;
					doc.build_mesh(id, m, 4);
					bool inward = true;
					for (std::size_t i = 0; i + 2 < m.indices.size(); i += 3)
					{
						const auto pos = [&](const std::size_t k)
						{
							const mesh_vertex& v = m.vertices[m.indices[k]];
							return vec3{ v.position[0], v.position[1], v.position[2] };
						};

						const vec3 n = cross(pos(i + 1) - pos(i), pos(i + 2) - pos(i));
						const vec3 mid = (pos(i) + pos(i + 1) + pos(i + 2)) * (1.0 / 3.0);
						inward = inward && dot(n, vec3{ mid.x - 32, mid.y - 32, 0 }) < 0.0;
					}

					inner += inward;
				}
				else
				{
					const vec3 n = normalize(mesh_normal(doc, id));
					CHECK(near(box.maxs.z, box.mins.z) && p->width == 3 && p->height == 9);
					rings += (near(box.mins.z, 0) && n.z < -0.99) || (near(box.mins.z, 64) && n.z > 0.99);
				}
			}

			CHECK(inner == 1 && rings == 2);
		}

		// caps
		{
			document doc;
			const object_id tube = create_patch_shape(doc, patch_shape::cylinder, { 0, 0, 0 }, { 64, 64, 64 }, 2, "ch_metal_01");
			CHECK(patch_cap(doc));
			CHECK(doc.patch_count() == 3 && doc.selection().size() == 3 && doc.undo_name() == "Cap patch");

			int bottom = 0, top = 0;
			for (const object_id id : doc.selection())
			{
				if (id == tube)
				{
					continue;
				}

				const patch* p = doc.find_patch(id);
				const bounds box = doc.object_bounds(id);
				const vec3 n = normalize(mesh_normal(doc, id));
				CHECK(p->type == patch_type::curve && p->width == 9 && p->height == 3 && p->material == "ch_metal_01");
				CHECK(near(box.mins.z, box.maxs.z) && near(box.mins.x, 0) && near(box.maxs.x, 64));

				// one edge is the outline of the tube, the other its centre; the texture lies flat on it
				bool centre = false, outline = false;
				for (const int row : { 0, 2 })
				{
					centre = centre || (near(p->at(0, row).xyz.x, 32) && near(p->at(3, row).xyz.y, 32));
					outline = outline || (near(std::fabs(p->at(1, row).xyz.x - 32), 32) && near(std::fabs(p->at(1, row).xyz.y - 32), 32));
				}

				CHECK(centre && outline);
				CHECK(near(p->at(1, 0).tex[0], p->at(1, 0).xyz.x * 4) && near(p->at(1, 0).tex[1], -p->at(1, 0).xyz.y * 4));

				bottom += near(box.mins.z, 0) && n.z < -0.99;
				top += near(box.mins.z, 64) && n.z > 0.99;
			}

			CHECK(bottom == 1 && top == 1);
			CHECK(doc.undo());

			// one end only
			doc.select(tube);
			CHECK(patch_cap(doc, patch_cap_type::cylinder, false, true) && doc.patch_count() == 2);
			CHECK(near(doc.object_bounds(doc.selection()[1]).mins.z, 64));
		}

		{
			document doc;

			// a cone has one open end
			CHECK(create_patch_shape(doc, patch_shape::cone, { 0, 0, 0 }, { 64, 64, 64 }));
			CHECK(patch_cap(doc) && doc.patch_count() == 2);

			// bevel: between the round and the straight line / the corner
			const object_id bevel = create_patch_shape(doc, patch_shape::bevel, { 100, 0, 0 }, { 164, 64, 64 });
			CHECK(patch_cap(doc) && doc.selection().size() == 3);
			const patch* cap = doc.find_patch(doc.selection()[2]);
			CHECK(cap->width == 3 && cap->height == 3);
			// the far line of the cap runs from the first to the last point of the outline
			bool chord = false;
			for (const int row : { 0, 2 })
			{
				chord = chord || near(cap->at(1, row).xyz, { 132, 32, cap->at(1, row).xyz.z });
			}
			CHECK(chord);
			CHECK(doc.undo());

			doc.select(bevel);
			CHECK(patch_cap(doc, patch_cap_type::inverted_bevel));
			cap = doc.find_patch(doc.selection()[2]);
			bool corner = false;
			for (const int row : { 0, 2 })
			{
				corner = corner || (near(cap->at(0, row).xyz.x, 164) && near(cap->at(0, row).xyz.y, 0) && near(cap->at(2, row).xyz.x, 164) && near(cap->at(2, row).xyz.y, 0));
			}
			CHECK(corner);

			// end caps
			const object_id endcap = create_patch_shape(doc, patch_shape::endcap, { 200, 0, 0 }, { 264, 64, 64 });
			CHECK(endcap && patch_cap(doc, patch_cap_type::endcap) && doc.selection().size() == 3);
			CHECK(doc.find_patch(doc.selection()[1])->width == 5 && doc.find_patch(doc.selection()[1])->height == 3);
			CHECK(doc.undo() && patch_cap(doc, patch_cap_type::inverted_endcap) && doc.selection().size() == 3);

			// not for terrain
			CHECK(doc.create_terrain_patch({ 0, 100, 0 }, { 64, 164, 0 }, 3, 3));
			CHECK(!patch_cap(doc));
		}
	}

	// ---- terrain edges, textures, vertices --------------------------------------------------------

	void test_vertices()
	{
		document doc;
		std::string error;
		CHECK(doc.load_map_text(sample_map(), error));

		const entity* world = doc.find_entity(doc.worldspawn());
		const object_id terrain = world->children[0], curve = world->children[1];

		// edges
		{
			doc.select(terrain);
			CHECK(patch_turn_edges(doc));
			const patch* p = doc.find_patch(terrain);
			CHECK(!p->at(0, 0).has_flag && p->at(1, 0).flag == 1 && p->at(0, 1).flag == 1 && !p->at(1, 1).has_flag && !p->at(2, 2).has_flag && !p->at(2, 0).has_flag);
			CHECK(doc.undo());

			CHECK(patch_turn_edge(doc, terrain, 1, 0) && doc.find_patch(terrain)->at(1, 0).flag == 1);
			CHECK(patch_turn_edge(doc, terrain, 1, 0) && !doc.find_patch(terrain)->at(1, 0).has_flag);
			CHECK(!patch_turn_edge(doc, terrain, 2, 0) && !patch_turn_edge(doc, curve, 0, 0));

			int column = -1, row = -1;
			double distance = 0.0;
			CHECK(patch_pick_quad(doc, terrain, { { 96, 96, 100 }, { 0, 0, -1 } }, column, row, distance) && column == 1 && row == 1 && distance > 80 && distance < 101);
			CHECK(patch_pick_quad(doc, terrain, { { 10, 20, 100 }, { 0, 0, -1 } }, column, row, distance) && column == 0 && row == 0);
			CHECK(!patch_pick_quad(doc, terrain, { { 500, 20, 100 }, { 0, 0, -1 } }, column, row, distance));
			CHECK(!patch_pick_quad(doc, curve, { { 300, 100, 32 }, { 0, -1, 0 } }, column, row, distance));

			doc.select(curve);
			CHECK(!patch_turn_edges(doc));
		}

		// textures
		{
			document d2;
			const object_id flat = create_curve_patch(d2, { 16, 32, 8 }, { 80, 96, 8 }, 3, 3);
			CHECK(patch_cap_texture(d2));
			const patch* p = d2.find_patch(flat);
			CHECK(near(p->at(0, 0).tex[0], 64) && near(p->at(0, 0).tex[1], -128) && near(p->at(2, 2).tex[0], 320) && near(p->at(2, 2).tex[1], -384) && near(p->at(2, 2).tex[2], 5));

			CHECK(patch_cycle_cap_texture(d2));		// z -> x: y and z
			p = d2.find_patch(flat);
			CHECK(near(p->at(2, 2).tex[0], 96 * 4) && near(p->at(2, 2).tex[1], -32));
			CHECK(patch_cycle_cap_texture(d2));		// x -> y: x and z
			CHECK(near(d2.find_patch(flat)->at(2, 2).tex[0], 320) && near(d2.find_patch(flat)->at(2, 2).tex[1], -32));
			CHECK(patch_cycle_cap_texture(d2) && near(d2.find_patch(flat)->at(2, 2).tex[1], -384));
			CHECK(patch_cap_texture(d2, 0) && near(d2.find_patch(flat)->at(2, 2).tex[0], 384));

			CHECK(patch_natural_texture(d2, 0.5));
			p = d2.find_patch(flat);
			CHECK(near(p->at(0, 0).tex[0], 0) && near(p->at(2, 0).tex[0], 128) && near(p->at(0, 0).tex[1], 128) && near(p->at(0, 2).tex[1], 0));
			CHECK(!patch_natural_texture(d2, 0.0));

			CHECK(patch_fit_texture(d2, 2.0, 1.0));
			p = d2.find_patch(flat);
			CHECK(near(p->at(2, 0).tex[0], 512) && near(p->at(1, 0).tex[0], 256) && near(p->at(0, 0).tex[1], 256) && near(p->at(0, 2).tex[1], 0));

			d2.set_texture_size_callback([](std::string_view, int& w, int& h) { w = 512; h = 128; return true; });
			CHECK(patch_fit_texture(d2) && near(d2.find_patch(flat)->at(2, 0).tex[0], 512) && near(d2.find_patch(flat)->at(0, 0).tex[1], 128));
		}

		// vertices
		{
			const std::uint8_t red[4] = { 255, 0, 0, 128 };
			const int some[] = { 0, 4 };
			CHECK(patch_set_color(doc, curve, some, red));
			const patch* p = doc.find_patch(curve);
			CHECK(p->vertices[0].color[1] == 0 && p->vertices[4].color[3] == 128 && p->vertices[1].color[1] == 255);
			CHECK(!patch_set_color(doc, curve, some, red));
			CHECK(patch_set_color(doc, curve, {}, red) && doc.find_patch(curve)->vertices[14].color[2] == 0);
			CHECK(!patch_set_color(doc, 9999, some, red));

			// noise: repeatable, bounded, and a seam stays closed
			document d2;
			const object_id tube = create_patch_shape(d2, patch_shape::cylinder, { 0, 0, 0 }, { 64, 64, 64 });
			const patch before = *d2.find_patch(tube);
			CHECK(patch_add_noise(d2, tube, {}, { 2, 2, 4 }, 7));
			const patch noisy = *d2.find_patch(tube);

			bool bounded = true, moved = false;
			for (std::size_t i = 0; i < noisy.vertices.size(); i++)
			{
				const vec3 delta = noisy.vertices[i].xyz - before.vertices[i].xyz;
				bounded = bounded && std::fabs(delta.x) <= 2 && std::fabs(delta.y) <= 2 && std::fabs(delta.z) <= 4;
				moved = moved || length(delta) > 0.01;
			}

			CHECK(bounded && moved && near(noisy.at(0, 1).xyz, noisy.at(8, 1).xyz));
			CHECK(d2.undo() && patch_add_noise(d2, tube, {}, { 2, 2, 4 }, 7));
			CHECK(near(d2.find_patch(tube)->at(3, 1).xyz, noisy.at(3, 1).xyz));
			CHECK(d2.undo() && patch_add_noise(d2, tube, {}, { 2, 2, 4 }, 8));
			CHECK(!near(d2.find_patch(tube)->at(3, 1).xyz, noisy.at(3, 1).xyz));

			patch_vertex v = d2.find_patch(tube)->vertices[5];
			v.xyz = { 1, 2, 3 };
			v.tex = { 10, 20, 1, 2 };
			CHECK(patch_set_vertex(d2, tube, 5, v) && near(d2.find_patch(tube)->vertices[5].xyz, { 1, 2, 3 }) && d2.find_patch(tube)->vertices[5].tex[1] == 20);
			CHECK(!patch_set_vertex(d2, tube, 500, v));

			// the generic door
			CHECK(edit_patch(d2, tube, "Rename", [](patch& q) { q.material = "other"; q.smoothing = "smoothing_hard"; return true; }));
			CHECK(d2.find_patch(tube)->material == "other" && d2.undo_name() == "Rename");
			CHECK(!edit_patch(d2, tube, "Break", [](patch& q) { q.width = 4; return true; }));
			CHECK(!edit_patch(d2, tube, "Nothing", [](patch&) { return false; }));
			CHECK(d2.find_patch(tube)->width == 9);
		}
	}
}

int main()
{
	std::setvbuf(stdout, nullptr, _IONBF, 0);
	std::printf("round_trip\n");
	test_round_trip();
	std::printf("brushes\n");
	test_brushes();
	std::printf("create\n");
	test_create();
	std::printf("lines\n");
	test_lines();
	std::printf("matrix\n");
	test_matrix();
	std::printf("conversions\n");
	test_conversions();
	std::printf("weld\n");
	test_weld();
	std::printf("thicken_cap\n");
	test_thicken_cap();
	std::printf("vertices\n");
	test_vertices();

	std::printf("\ncodol_patch_ops_tests: %d checks, %d failed\n", g_checks, g_failed);
	return g_failed == 0 ? 0 : 1;
}
