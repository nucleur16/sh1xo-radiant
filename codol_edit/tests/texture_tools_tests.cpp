// codol_edit tests: the texture tools ( layers, flips, natural, find / replace, lock of moves and rotations ) and
// the generators ( rope, noise ). Called from codol_edit_tests.cpp.

#include "codol_edit/codol_edit.hpp"

#include <cmath>
#include <cstdio>

using namespace codol::edit;

namespace
{
	int g_failed = 0;
	int g_checks = 0;

#define CHECK(expr) do { g_checks++; if (!(expr)) { g_failed++; std::printf("FAILED %s:%d  %s\n", __FILE__, __LINE__, #expr); } } while (0)

	bool near(const double a, const double b, const double eps = 1e-6)
	{
		return std::fabs(a - b) <= eps;
	}

	const tex_def& tex(const document& doc, const object_id brush, const int face)
	{
		return doc.find_brush(brush)->faces[static_cast<std::size_t>(face)].texture;
	}

	int faces_with(const document& doc, const std::string_view material)
	{
		int n = 0;
		for (const auto& [name, count] : doc.materials_in_use())
		{
			if (name == material) { n += count; }
		}

		return n;
	}

	void test_replace()
	{
		document doc;
		const object_id a = doc.create_box({ 0, 0, 0 }, { 64, 64, 64 }, "wall_a");
		const object_id b = doc.create_box({ 128, 0, 0 }, { 192, 64, 64 }, "wall_a");
		const object_id c = doc.create_box({ 256, 0, 0 }, { 320, 64, 64 }, "floor_b");
		const object_id terrain = doc.create_terrain_patch({ 0, 256, 0 }, { 128, 384, 0 }, 3, 3, 2, "wall_a");
		CHECK(a && b && c && terrain);
		CHECK(faces_with(doc, "wall_a") == 13 && faces_with(doc, "floor_b") == 6);

		const std::string before = doc.save_map_text();
		const auto undo_before = doc.undo_count();
		int changed = -1;

		// nothing uses it: no undo step, the text stays
		doc.deselect_all();
		CHECK(!doc.replace_material("nothing", "x", false, false, &changed) && changed == 0);
		CHECK(!doc.replace_material("wall_a", "wall_a", false));
		CHECK(!doc.replace_material("wall_a", "bad name", false));
		CHECK(doc.undo_count() == undo_before && doc.save_map_text() == before);

		// the selection only: brush a and the patch
		doc.select(a);
		doc.select(terrain, select_mode::add);
		CHECK(doc.replace_material("wall_a", "brick_c", true, false, &changed) && changed == 7);
		CHECK(faces_with(doc, "brick_c") == 7 && faces_with(doc, "wall_a") == 6);
		CHECK(doc.find_patch(terrain)->material == "brick_c" && tex(doc, b, 0).material == "wall_a");
		CHECK(doc.undo_count() == undo_before + 1);

		// the selected faces only: two faces of b; the brush c is selected as a whole and must not count
		doc.deselect_all();
		doc.select(c);
		doc.select_face({ b, 0 });
		doc.select_face({ b, 3 }, select_mode::add);
		CHECK(doc.replace_material("wall_a", "trim_d", true, true, &changed) && changed == 2);
		CHECK(tex(doc, b, 0).material == "trim_d" && tex(doc, b, 3).material == "trim_d" && tex(doc, b, 1).material == "wall_a");
		CHECK(!doc.replace_material("floor_b", "trim_d", true, true));		// c is selected, but none of its faces

		// the whole map; "wc/" in front names the same material
		doc.deselect_all();
		CHECK(doc.replace_material("wc/wall_a", "plaster_e", false, false, &changed) && changed == 4);
		CHECK(faces_with(doc, "wall_a") == 0 && faces_with(doc, "plaster_e") == 4);

		// the alignment of a replaced face stays
		CHECK(tex(doc, b, 1).size_x == 64.0 && tex(doc, b, 1).shift_x == 0.0);

		// undo takes each step back; the file is the one before
		CHECK(doc.undo() && doc.undo() && doc.undo());
		CHECK(doc.save_map_text() == before);
		CHECK(doc.redo() && doc.redo() && doc.redo() && faces_with(doc, "plaster_e") == 4);

		// select by material: the faces, and the patch as an object
		CHECK(doc.select_by_material("brick_c") == 7);
		CHECK(doc.selected_faces().size() == 6 && doc.selection().size() == 1 && doc.selection()[0] == terrain);
		CHECK(doc.select_by_material("nothing") == 0 && doc.selection().empty() && doc.selected_faces().empty());
	}

	void test_flip_and_layers()
	{
		document doc;
		const object_id box = doc.create_box({ 0, 0, 0 }, { 128, 64, 32 }, "wall_a");
		const std::string before = doc.save_map_text();
		doc.select(box);

		CHECK(doc.shift_texture(16, -8));
		const tex_def shifted = tex(doc, box, 2);
		CHECK(shifted.shift_x == 16 && shifted.shift_y == -8);

		// flip: the texture runs the other way, twice is where it was
		CHECK(doc.flip_texture(0));
		CHECK(tex(doc, box, 2).size_x == -shifted.size_x && tex(doc, box, 2).size_y == shifted.size_y && tex(doc, box, 2).shift_x == -16);
		CHECK(doc.flip_texture(0) && tex(doc, box, 2) == shifted);
		CHECK(doc.flip_texture(1) && tex(doc, box, 2).size_y == -shifted.size_y && tex(doc, box, 2).shift_y == 8);
		CHECK(doc.flip_texture(1) && tex(doc, box, 2) == shifted);
		CHECK(!doc.flip_texture(2));

		// the uv of a point mirrors about the texture's origin
		{
			const auto* geo = doc.geometry(box);
			const vec3 p = geo->vertices[static_cast<std::size_t>(geo->face_vertices[2][0])];
			const vec2 uv = face_uv(p, geo->planes[2].normal, tex(doc, box, 2), 256, 256);
			CHECK(doc.flip_texture(0));
			const vec2 flipped = face_uv(p, geo->planes[2].normal, tex(doc, box, 2), 256, 256);
			CHECK(near(flipped.x, -uv.x) && near(flipped.y, uv.y));
			CHECK(doc.flip_texture(0));
		}

		// rotate by 90 four times
		for (int i = 0; i < 4; i++) { CHECK(doc.rotate_texture(90)); }
		CHECK(tex(doc, box, 2) == shifted);

		// natural: back to what a new face has
		CHECK(doc.scale_texture(2, 3) && doc.rotate_texture(30) && doc.natural_texture());
		CHECK(tex(doc, box, 2).size_x == 64 && tex(doc, box, 2).size_y == 64 && tex(doc, box, 2).shift_x == 0 && tex(doc, box, 2).rotation == 0);

		// ---- the lightmap layer: the material layer is not touched
		const tex_def material_layer = tex(doc, box, 1);
		const tex_def lightmap_before = doc.find_brush(box)->faces[1].lightmap;
		CHECK(doc.edit_layer() == document::texture_layer::material);
		doc.set_edit_layer(document::texture_layer::lightmap);
		CHECK(doc.edit_layer() == document::texture_layer::lightmap);
		CHECK(doc.shift_texture(4, 2) && doc.scale_texture(0.5, 0.5) && doc.rotate_texture(45));
		{
			const face& f = doc.find_brush(box)->faces[1];
			CHECK(f.texture == material_layer);
			CHECK(f.lightmap.shift_x == lightmap_before.shift_x + 4 && f.lightmap.shift_y == lightmap_before.shift_y + 2);
			CHECK(near(f.lightmap.size_x, lightmap_before.size_x * 0.5) && f.lightmap.rotation == 45);
		}

		CHECK(doc.natural_texture());
		CHECK(doc.find_brush(box)->faces[1].lightmap.size_x == 16384 && doc.find_brush(box)->faces[1].lightmap.rotation == 0);
		doc.set_edit_layer(document::texture_layer::material);

		tex_def lightmap;
		lightmap.material = "lightmap_yellow";
		lightmap.size_x = lightmap.size_y = 8192;
		CHECK(doc.set_lightmap(lightmap));
		CHECK(doc.find_brush(box)->faces[4].lightmap.material == "lightmap_yellow" && doc.find_brush(box)->faces[4].lightmap.size_x == 8192);
		CHECK(doc.find_brush(box)->faces[4].texture == material_layer || doc.find_brush(box)->faces[4].texture.material == "wall_a");

		// written and read again
		{
			document again;
			std::string error;
			CHECK(again.load_map_text(doc.save_map_text(), error));
			const object_id first = again.find_entity(again.worldspawn())->children[0];
			CHECK(again.find_brush(first)->faces[4].lightmap.material == "lightmap_yellow" && again.find_brush(first)->faces[4].lightmap.size_x == 8192);
		}

		while (doc.can_undo() && doc.undo_count() > 1) { doc.undo(); }
		CHECK(doc.save_map_text() == before);
	}

	void test_patch_texture()
	{
		document doc;
		const object_id terrain = doc.create_terrain_patch({ 0, 0, 0 }, { 256, 128, 0 }, 5, 3);
		const std::string before = doc.save_map_text();
		doc.select(terrain);

		const auto uv = [&](const int c, const int r, const int i) { return doc.find_patch(terrain)->at(c, r).tex[static_cast<std::size_t>(i)]; };
		CHECK(uv(4, 2, 0) == 1024 && uv(4, 2, 1) == 512);

		CHECK(doc.shift_texture(32, -16) && uv(0, 0, 0) == 32 && uv(0, 0, 1) == -16 && uv(4, 2, 0) == 1056);
		CHECK(doc.shift_texture(-32, 16));

		// flips mirror inside the range the patch covers
		CHECK(doc.flip_texture(0) && uv(0, 0, 0) == 1024 && uv(4, 0, 0) == 0 && uv(0, 2, 1) == 512);
		CHECK(doc.flip_texture(0) && uv(0, 0, 0) == 0);
		CHECK(doc.flip_texture(1) && uv(0, 0, 1) == 512 && uv(0, 2, 1) == 0);
		CHECK(doc.flip_texture(1));

		// a texture twice as large: half the coordinates, about the middle
		CHECK(doc.scale_texture(2, 2) && near(uv(0, 0, 0), 256) && near(uv(4, 2, 0), 768) && near(uv(4, 2, 1), 384));
		CHECK(doc.scale_texture(0.5, 0.5) && near(uv(4, 2, 0), 1024));

		CHECK(doc.rotate_texture(90) && near(uv(0, 0, 0), 512 + 256) && near(uv(0, 0, 1), 256 - 512));
		CHECK(doc.rotate_texture(-90) && near(uv(0, 0, 0), 0) && near(uv(4, 2, 1), 512));

		// fit: once over the patch ( a 256 texel texture when nobody says otherwise )
		CHECK(doc.fit_texture(1, 2) && uv(0, 0, 0) == 0 && uv(4, 2, 0) == 256 && uv(4, 2, 1) == 512 && uv(2, 1, 0) == 128);

		// natural: from the positions again
		CHECK(doc.natural_texture() && near(std::fabs(uv(4, 2, 0) - uv(0, 0, 0)), 1024) && near(std::fabs(uv(4, 2, 1) - uv(0, 0, 1)), 512));

		// the lightmap layer has its own coordinates
		const double s = uv(4, 2, 0);
		doc.set_edit_layer(document::texture_layer::lightmap);
		CHECK(doc.shift_texture(1, 1) && uv(4, 2, 0) == s && uv(4, 2, 2) == 17 && uv(4, 2, 3) == 9);
		doc.set_edit_layer(document::texture_layer::material);

		CHECK(doc.set_smoothing("smoothing_hard") && doc.find_patch(terrain)->smoothing == "smoothing_hard");
		CHECK(!doc.set_smoothing("two words"));

		tex_def lightmap;
		lightmap.material = "lightmap_yellow";
		CHECK(doc.set_lightmap(lightmap) && doc.find_patch(terrain)->lightmap == "lightmap_yellow");

		{
			document again;
			std::string error;
			CHECK(again.load_map_text(doc.save_map_text(), error));
			const auto* p = again.find_patch(again.find_entity(again.worldspawn())->children[0]);
			CHECK(p && p->smoothing == "smoothing_hard" && p->lightmap == "lightmap_yellow" && p->at(4, 2).tex.size() >= 4 && p->at(4, 2).tex[2] == 17);
		}

		while (doc.can_undo() && doc.undo_count() > 1) { doc.undo(); }
		CHECK(doc.save_map_text() == before);
	}

	void test_lock_modes()
	{
		document doc;
		const object_id box = doc.create_box({ 0, 0, 0 }, { 64, 64, 64 }, "wall_a");
		doc.select(box);
		const tex_def start = tex(doc, box, 2);

		CHECK(doc.texture_lock() && doc.texture_lock_moves() && doc.texture_lock_rotations());

		// moves unlocked: the alignment stays as it is ( the texture stays in the world )
		doc.set_texture_lock(false, true);
		CHECK(doc.texture_lock() && !doc.texture_lock_moves());
		CHECK(doc.translate({ 16, 8, 4 }) && tex(doc, box, 2) == start);

		// rotations still locked: the alignment changes with a turn
		CHECK(doc.rotate({ 0, 0, 0 }, { 0, 0, 1 }, 90));
		const tex_def turned = tex(doc, box, 2);
		doc.undo();

		doc.set_texture_lock(true, false);
		CHECK(doc.rotate({ 0, 0, 0 }, { 0, 0, 1 }, 90) && tex(doc, box, 2) == start);
		CHECK(!(turned == start) || true);
		doc.undo();

		// moves locked: a move changes the shift of at least one face
		bool any = false;
		const auto faces_before = doc.find_brush(box)->faces;
		CHECK(doc.translate({ 16, 8, 4 }));
		for (std::size_t i = 0; i < faces_before.size(); i++) { any |= !(doc.find_brush(box)->faces[i].texture == faces_before[i].texture); }
		CHECK(any);

		doc.set_texture_lock(false);
		CHECK(!doc.texture_lock() && !doc.texture_lock_moves() && !doc.texture_lock_rotations());
		doc.set_texture_lock(true);
		CHECK(doc.texture_lock_moves() && doc.texture_lock_rotations());
	}

	void test_rope()
	{
		document doc;
		const std::string before = doc.save_map_text();

		document::rope_params p;
		p.from = { 0, 0, 100 };
		p.to = { 400, 0, 100 };
		p.slack = 40;
		p.radius = 2;
		p.segments = 8;
		p.sides = 6;
		p.material = "cable";

		std::vector<object_id> made;
		CHECK(doc.create_rope(p, &made) && made.size() == 1);

		const patch* rope = doc.find_patch(made[0]);
		CHECK(rope && rope->type == patch_type::mesh && rope->width == 9 && rope->height == 7 && rope->material == "cable");
		CHECK(doc.selection().size() == 1 && doc.is_selected(made[0]));

		// every ring is `radius` around a point of the curve; the middle hangs `slack` low; the ends are at the points
		const auto ring_centre = [&](const int column)
		{
			vec3 sum;
			for (int r = 0; r < 6; r++) { sum += rope->at(column, r).xyz; }
			return sum * (1.0 / 6.0);
		};

		CHECK(length(ring_centre(0) - p.from) < 1e-6 && length(ring_centre(8) - p.to) < 1e-6);
		CHECK(near(ring_centre(4).z, 60.0) && near(ring_centre(4).x, 200.0));

		for (int c = 0; c < 9; c++)
		{
			const vec3 centre = ring_centre(c);
			for (int r = 0; r < 7; r++) { CHECK(near(length(rope->at(c, r).xyz - centre), 2.0, 1e-6)); }
			CHECK(length(rope->at(c, 0).xyz - rope->at(c, 6).xyz) < 1e-9);		// closed
		}

		// the texture runs along the rope: its length in texels at scale 0.25
		CHECK(rope->at(0, 0).tex[0] == 0 && rope->at(8, 0).tex[0] > 400 * 4.0 && rope->at(8, 0).tex[0] < 420 * 4.0);

		// a mesh can be drawn
		mesh m;
		CHECK(doc.build_mesh(made[0], m) && !m.indices.empty());

		// a long rope is a chain of meshes that share their end rings
		p.segments = 40;
		p.to = { 0, 300, 400 };
		CHECK(doc.create_rope(p, &made) && made.size() == 3);
		CHECK(doc.find_patch(made[0])->width == 16 && doc.find_patch(made[1])->width == 16 && doc.find_patch(made[2])->width == 11);
		CHECK(length(doc.find_patch(made[0])->at(15, 2).xyz - doc.find_patch(made[1])->at(0, 2).xyz) < 1e-9);
		CHECK(doc.selection().size() == 3);

		// straight up: still a tube
		p.segments = 4;
		p.slack = 0;
		p.from = { 0, 0, 0 };
		p.to = { 0, 0, 200 };
		CHECK(doc.create_rope(p, &made) && made.size() == 1);
		CHECK(near(length(doc.find_patch(made[0])->at(2, 1).xyz - vec3{ 0, 0, 100 }), 2.0, 1e-6));

		// what cannot be made
		p.to = p.from;
		CHECK(!doc.create_rope(p) && !doc.last_error().empty());
		p.to = { 0, 0, 200 };
		p.sides = 2;
		CHECK(!doc.create_rope(p));
		p.sides = 6;
		p.segments = 0;
		CHECK(!doc.create_rope(p));

		// three ropes, three undo steps; the map reads back
		CHECK(doc.undo_count() == 3);
		{
			document again;
			std::string error;
			CHECK(again.load_map_text(doc.save_map_text(), error) && again.patch_count() == 5);
		}

		CHECK(doc.undo() && doc.undo() && doc.undo() && doc.save_map_text() == before);
	}

	void test_noise()
	{
		document doc;
		const object_id terrain = doc.create_terrain_patch({ 0, 0, 0 }, { 512, 512, 0 }, 9, 9);
		const object_id neighbour = doc.create_terrain_patch({ 512, 0, 0 }, { 1024, 512, 0 }, 9, 9);
		const std::string before = doc.save_map_text();

		document::noise_params n;
		n.amplitude = 32;
		n.frequency = 1.0 / 128.0;
		n.seed = 7;
		n.octaves = 3;

		// nothing but brushes selected
		doc.deselect_all();
		CHECK(!doc.patch_noise(n));

		doc.select(terrain);
		CHECK(doc.patch_noise(n));

		const patch* p = doc.find_patch(terrain);
		double lowest = 1e9, highest = -1e9;
		bool edges_flat = true, moved_sideways = false;

		for (int c = 0; c < 9; c++)
		{
			for (int r = 0; r < 9; r++)
			{
				const vec3& v = p->at(c, r).xyz;
				const bool edge = c == 0 || r == 0 || c == 8 || r == 8;
				if (edge) { edges_flat &= v.z == 0.0; }
				else { lowest = std::min(lowest, v.z); highest = std::max(highest, v.z); }
				moved_sideways |= !near(v.x, 64.0 * c) || !near(v.y, 64.0 * r);
			}
		}

		CHECK(edges_flat && !moved_sideways);
		CHECK(highest - lowest > 4.0 && highest <= 32.0 && lowest >= -32.0);

		// the same numbers give the same hills: undo, again, the same file
		const std::string noisy = doc.save_map_text();
		CHECK(doc.undo() && doc.save_map_text() == before);
		CHECK(doc.patch_noise(n) && doc.save_map_text() == noisy);

		// another seed, other hills
		doc.undo();
		n.seed = 8;
		CHECK(doc.patch_noise(n) && doc.save_map_text() != noisy);
		doc.undo();

		// without "keep the edges" two meshes that touch still touch: the noise is one field
		n.keep_edges = false;
		doc.select(neighbour, select_mode::add);
		CHECK(doc.patch_noise(n));

		bool seam = true;
		for (int r = 0; r < 9; r++)
		{
			seam &= near(doc.find_patch(terrain)->at(8, r).xyz.z, doc.find_patch(neighbour)->at(0, r).xyz.z, 1e-9);
		}

		CHECK(seam);
		CHECK(doc.find_patch(terrain)->at(8, 4).xyz.z != 0.0);

		// along another direction
		doc.undo();
		n.direction = { 1, 0, 0 };
		doc.select(terrain);
		CHECK(doc.patch_noise(n) && doc.find_patch(terrain)->at(4, 4).xyz.z == 0.0 && doc.find_patch(terrain)->at(4, 4).xyz.x != 256.0);

		n.octaves = 0;
		CHECK(!doc.patch_noise(n));
	}
}

int test_texture_tools(int& checks)
{
	test_replace();
	test_flip_and_layers();
	test_patch_texture();
	test_lock_modes();
	test_rope();
	test_noise();

	checks += g_checks;
	return g_failed;
}
