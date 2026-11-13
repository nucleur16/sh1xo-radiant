// Tests of codol_edit: what the Selection / Region / Misc menus, the Filter window and prefabs use
// ( src/select_ops.cpp, src/prefab.cpp ). Plain checks and counters, no framework, no test data.

#include <codol_edit/codol_edit.hpp>

#include <cstdio>
#include <map>

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

	using pairs = std::vector<std::pair<std::string, std::string>>;

	object_id point(document& doc, const char* cls, const vec3& at, const pairs& keys = {})
	{
		return doc.create_point_entity(cls, at, keys);
	}

	std::size_t count_faces_with(const document& doc, const object_id id, const std::string_view material)
	{
		std::size_t n = 0;
		for (const face& f : doc.find_brush(id)->faces)
		{
			n += f.texture.material == material ? 1 : 0;
		}

		return n;
	}

	std::size_t entity_count(const document& doc, const std::string_view cls)
	{
		std::size_t n = 0;
		for (const object_id id : doc.entities())
		{
			n += doc.find_entity(id)->classname() == cls ? 1 : 0;
		}

		return n;
	}

	bounds world_bounds(const document& doc)
	{
		bounds b;
		for (const object_id id : doc.find_entity(doc.worldspawn())->children)
		{
			b.add(doc.object_bounds(id));
		}

		return b;
	}

	// ---------------------------------------------------------------------------------------------

	void test_names()
	{
		CHECK(classify_material("caulk") == material_class::caulk);
		CHECK(classify_material("wc/caulk") == material_class::caulk);
		CHECK(classify_material("clip_weap") == material_class::clip);
		CHECK(classify_material("trigger") == material_class::trigger);
		CHECK(classify_material("portal_nodraw") == material_class::portal);
		CHECK(classify_material("skip") == material_class::hint);
		CHECK(classify_material("sky_mp_rust") == material_class::sky);
		CHECK(classify_material("skyscraper_wall") == material_class::normal);
		CHECK(classify_material("ch_concrete_wall_01") == material_class::normal);
		CHECK(is_tool_material("hint") && !is_tool_material("me_brick"));

		CHECK(wildcard_match("", "anything"));
		CHECK(wildcard_match("*", ""));
		CHECK(wildcard_match("mp_*_spawn", "mp_dm_spawn"));
		CHECK(wildcard_match("MP_DM_SPAWN", "mp_dm_spawn"));
		CHECK(!wildcard_match("mp_*_spawn", "mp_dm_spawn_start"));
		CHECK(wildcard_match("*door*", "auto_door_3"));
		CHECK(!wildcard_match("door", "auto_door_3"));
	}

	void test_content_flags()
	{
		document doc;
		const object_id a = doc.create_box({ 0, 0, 0 }, { 64, 64, 64 }, "me_brick");
		CHECK(a != no_object);

		CHECK(doc.set_content_flag("weaponClip", true));
		CHECK(doc.find_brush(a)->has_content("weaponClip"));
		CHECK(doc.set_content_flag("nonColliding", true));
		CHECK(doc.set_detail(true));
		CHECK(doc.set_tool_flag("splitGeo", true));

		const std::string text = doc.save_map_text();
		CHECK(text.find("contents detail weaponClip nonColliding;") != std::string::npos);
		CHECK(text.find("toolFlags splitGeo;") != std::string::npos);

		// what radiant writes is read back
		document again;
		std::string error;
		CHECK(again.load_map_text(text, error));
		const object_id b = again.find_entity(again.worldspawn())->children.at(0);
		CHECK(again.find_brush(b)->has_content("weaponClip") && again.find_brush(b)->has_content("nonColliding") && again.find_brush(b)->is_detail());
		CHECK(again.find_brush(b)->tool_flags.size() == 1 && again.find_brush(b)->tool_flags[0] == "splitGeo");
		CHECK(again.save_map_text() == text);

		// off again, one word at a time, and undo
		CHECK(doc.set_content_flag("weaponClip", false));
		CHECK(!doc.find_brush(a)->has_content("weaponClip") && doc.find_brush(a)->has_content("nonColliding"));
		CHECK(doc.set_tool_flag("splitGeo", false));
		CHECK(doc.find_brush(a)->tool_flags.empty());
		CHECK(doc.undo() && doc.undo());
		CHECK(doc.find_brush(a)->has_content("weaponClip") && doc.find_brush(a)->tool_flags.size() == 1);

		doc.deselect_all();
		CHECK(!doc.set_content_flag("weaponClip", true));
	}

	void test_select()
	{
		document doc;
		const object_id door = point(doc, "script_origin", { 0, 0, 0 }, { { "targetname", "door_1" }, { "target", "hinge" } });
		const object_id hinge = point(doc, "script_origin", { 64, 0, 0 }, { { "targetname", "hinge" }, { "target", "stop" } });
		const object_id stop = point(doc, "script_struct", { 128, 0, 0 }, { { "targetname", "stop" } });
		const object_id button = point(doc, "script_model", { -64, 0, 0 }, { { "target", "door_1" }, { "model", "com_barrel" } });
		const object_id alone = point(doc, "mp_dm_spawn", { 0, 512, 0 });

		CHECK(doc.select_by_key("classname", "script_origin") == 2);
		CHECK(doc.selection().size() == 2 && doc.is_selected(door) && doc.is_selected(hinge));
		CHECK(doc.select_by_key("targetname", "") == 3);
		CHECK(doc.select_by_key("targetname", "door*") == 1 && doc.is_selected(door));
		CHECK(doc.select_by_key("classname", "mp_*", select_mode::add) == 1 && doc.selection().size() == 2);
		CHECK(doc.select_by_key("nokey", "") == 0 && doc.selection().empty());

		// links in both directions, over several steps
		doc.select(hinge);
		CHECK(doc.select_connected() == 3);
		CHECK(doc.is_selected(door) && doc.is_selected(stop) && doc.is_selected(button) && !doc.is_selected(alone));
		doc.select(alone);
		CHECK(doc.select_connected() == 0 && doc.selection().size() == 1);

		// replace a key over the selection / the map
		doc.select(button);
		CHECK(doc.replace_key_value("model", "com_barrel", "com_crate", true) == 1);
		CHECK(doc.find_entity(button)->value("model") == "com_crate");
		CHECK(doc.replace_key_value("model", "com_barrel", "x", false) == 0);
		CHECK(doc.undo() && doc.find_entity(button)->value("model") == "com_barrel");

		// numbers as build reports print them
		CHECK(doc.find_by_number(0) == doc.worldspawn());
		CHECK(doc.find_by_number(1) == door && doc.find_by_number(5) == alone && doc.find_by_number(6) == no_object);
	}

	void test_volume_select()
	{
		document doc;
		const object_id inside = doc.create_box({ 16, 16, 16 }, { 32, 32, 32 });
		const object_id crossing = doc.create_box({ 48, 16, 16 }, { 96, 32, 32 });
		const object_id above = doc.create_box({ 16, 16, 200 }, { 32, 32, 216 });
		const object_id away = doc.create_box({ 300, 300, 0 }, { 316, 316, 16 });
		const object_id ent = point(doc, "script_origin", { 24, 40, 24 });

		const auto marker = [&] { return doc.create_box({ 0, 0, 0 }, { 64, 64, 64 }); };

		// touching: the marker stays selected, what it touches is added
		object_id m = marker();
		CHECK(doc.select_by_volume(volume_select::touching));
		CHECK(doc.is_selected(m) && doc.is_selected(inside) && doc.is_selected(crossing) && doc.is_selected(ent));
		CHECK(!doc.is_selected(above) && !doc.is_selected(away));
		CHECK(doc.kind(m) == object_kind::brush);

		// inside: only what is completely inside; the marker goes ( one undo step brings it back )
		doc.select(m);
		CHECK(doc.select_by_volume(volume_select::inside));
		CHECK(doc.kind(m) == object_kind::none);
		CHECK(doc.selection().size() == 2 && doc.is_selected(inside) && doc.is_selected(ent));
		CHECK(doc.undo() && doc.kind(m) == object_kind::brush);

		// complete tall: the height does not count
		doc.select(m);
		CHECK(doc.select_by_volume(volume_select::complete_tall, 2));
		CHECK(doc.selection().size() == 3 && doc.is_selected(above) && !doc.is_selected(crossing));
		CHECK(doc.undo());

		// partial tall: whatever reaches into the column
		doc.select(m);
		CHECK(doc.select_by_volume(volume_select::partial_tall, 2));
		CHECK(doc.selection().size() == 4 && doc.is_selected(crossing) && !doc.is_selected(away));
		CHECK(doc.undo());

		// nothing inside: the marker stays
		doc.select(away);
		CHECK(!doc.select_by_volume(volume_select::inside));
		CHECK(doc.kind(away) == object_kind::brush);

		doc.deselect_all();
		CHECK(!doc.select_by_volume(volume_select::touching));
	}

	void test_clipboard()
	{
		document doc;
		const object_id a = doc.create_box({ 0, 0, 0 }, { 64, 64, 64 }, "me_brick");
		const object_id b = doc.create_box({ 128, 0, 0 }, { 192, 64, 64 }, "me_brick");
		const object_id c = doc.create_box({ 256, 0, 0 }, { 320, 64, 64 }, "clip");
		doc.select(c);
		const object_id mover = doc.create_brush_entity("script_brushmodel", pairs{ { "targetname", "mover" } });
		const object_id spawn = point(doc, "mp_dm_spawn", { 32, 32, 96 }, { { "angles", "0 90 0" } });
		CHECK(mover != no_object);

		doc.deselect_all();
		CHECK(doc.selection_map_text().empty());

		const object_id ids[] = { a, c, spawn };
		doc.select(std::span<const object_id>(ids));
		const std::string text = doc.selection_map_text();
		CHECK(text.find("iwmap 4") == 0);
		CHECK(text.find("script_brushmodel") != std::string::npos && text.find("mp_dm_spawn") != std::string::npos);

		// into another map ( another editor ): in place
		document other;
		other.create_box({ -64, -64, -64 }, { 0, 0, 0 });
		CHECK(other.paste_map_text(text));
		CHECK(other.brush_count() == 3 && other.entities().size() == 3);
		CHECK(other.selection().size() == 3);
		CHECK(near(other.selection_bounds().mins, { 0, 0, 0 }) && near(other.selection_bounds().maxs, { 320, 64, 112 }, 64.0));
		CHECK(entity_count(other, "script_brushmodel") == 1 && entity_count(other, "worldspawn") == 1);
		CHECK(other.undo());
		CHECK(other.brush_count() == 1 && other.entities().size() == 1);

		// at a place: the centre of what was pasted goes there, on the grid
		const vec3 target{ 1000, 1000, 500 };
		CHECK(other.paste_map_text(text, &target, 8.0));
		const vec3 centre = other.selection_bounds().center();
		CHECK(near(centre.x, 1000, 8.0) && near(centre.y, 1000, 8.0) && near(centre.z, 500, 8.0));
		CHECK(other.undo_count() == 2);		// the box and one step for the paste
		for (const object_id id : other.selection())
		{
			if (const entity* e = other.find_entity(id))
			{
				CHECK(e->value("angles") == "0 90 0");
			}
		}

		// the same map: a second copy, ids are new
		CHECK(doc.paste_map_text(text));
		CHECK(doc.brush_count() == 5 && !doc.is_selected(a) && !doc.is_selected(b));

		CHECK(!doc.paste_map_text("this is not a map"));
		CHECK(!doc.paste_map_text("iwmap 4\n// entity 0\n{\n\"classname\" \"worldspawn\"\n}\n"));

		// File > Save Selected: the worldspawn keeps its keys
		doc.set_key(doc.worldspawn(), "sunlight", "1.5");
		doc.select(a);
		const std::string saved = doc.selection_map_text(true);
		CHECK(saved.find("\"sunlight\" \"1.5\"") != std::string::npos);
		document loaded;
		std::string error;
		CHECK(loaded.load_map_text(saved, error) && loaded.brush_count() == 1);
	}

	void test_link_and_group()
	{
		document doc;
		const object_id a = doc.create_box({ 0, 0, 0 }, { 64, 64, 64 });
		const object_id b = doc.create_box({ 128, 0, 0 }, { 192, 64, 64 });
		const object_id c = doc.create_box({ 256, 0, 0 }, { 320, 64, 64 });
		doc.select(c);
		const object_id group = doc.create_brush_entity("script_brushmodel");

		// link: world brushes join the entity that has a brush in the selection
		const object_id ids[] = { a, b, c };
		doc.select(std::span<const object_id>(ids));
		CHECK(doc.move_selection_to_entity());
		CHECK(doc.find_entity(group)->children.size() == 3);
		CHECK(doc.find_entity(doc.worldspawn())->children.empty());
		CHECK(doc.owner_of(a) == group);

		// cycle through the brushes of the group
		CHECK(doc.select_next_in_group());
		CHECK(doc.selection().size() == 1);
		const object_id first = doc.selection()[0];
		CHECK(doc.select_next_in_group() && doc.selection()[0] != first);
		CHECK(doc.select_next_in_group() && doc.select_next_in_group() && doc.selection()[0] == first);
		CHECK(doc.select_next_in_group(-1) && doc.selection()[0] != first);

		CHECK(doc.undo());
		CHECK(doc.find_entity(group)->children.size() == 1 && doc.owner_of(a) == doc.worldspawn());

		doc.select(a);
		CHECK(!doc.move_selection_to_entity());
		CHECK(!doc.select_next_in_group());
	}

	void test_csg_merge()
	{
		document doc;
		const object_id a = doc.create_box({ 0, 0, 0 }, { 64, 64, 64 }, "me_brick");
		const object_id b = doc.create_box({ 64, 0, 0 }, { 128, 64, 64 }, "me_concrete");
		const object_id c = doc.create_box({ 0, 64, 0 }, { 64, 128, 64 }, "me_brick");

		// two boxes side by side: one box, with the textures the faces had
		const object_id ab[] = { a, b };
		doc.select(std::span<const object_id>(ab));
		CHECK(doc.csg_merge());
		CHECK(doc.brush_count() == 2 && doc.selection().size() == 1);
		const object_id merged = doc.selection()[0];
		const brush_geometry* geo = doc.geometry(merged);
		CHECK(geo && geo->valid && doc.find_brush(merged)->faces.size() == 6);
		CHECK(near(geo->box.mins, { 0, 0, 0 }) && near(geo->box.maxs, { 128, 64, 64 }));
		CHECK(count_faces_with(doc, merged, "me_concrete") >= 1 && count_faces_with(doc, merged, "me_brick") >= 1);

		// an L is not convex: nothing changes
		const object_id l[] = { merged, c };
		doc.select(std::span<const object_id>(l));
		CHECK(!doc.csg_merge());
		CHECK(doc.brush_count() == 2);

		CHECK(doc.undo());
		CHECK(doc.brush_count() == 3 && doc.kind(a) == object_kind::brush && doc.kind(merged) == object_kind::none);

		// boxes that overlap and are convex together
		const object_id d = doc.create_box({ 200, 0, 0 }, { 264, 64, 64 });
		const object_id e = doc.create_box({ 232, 0, 0 }, { 300, 64, 64 });
		const object_id de[] = { d, e };
		doc.select(std::span<const object_id>(de));
		CHECK(doc.csg_merge());
		CHECK(near(doc.selection_bounds().maxs.x, 300.0));

		// a gap between them: not convex together
		const object_id f = doc.create_box({ 400, 0, 0 }, { 464, 64, 64 });
		const object_id g = doc.create_box({ 480, 0, 0 }, { 544, 64, 64 });
		const object_id fg[] = { f, g };
		doc.select(std::span<const object_id>(fg));
		CHECK(!doc.csg_merge());

		doc.select(a);
		CHECK(!doc.csg_merge());
	}

	void test_auto_caulk()
	{
		document doc;
		// a floor, a wall standing on it, a small crate against the wall
		const object_id floor = doc.create_box({ 0, 0, -16 }, { 256, 256, 0 }, "me_floor");
		const object_id wall = doc.create_box({ 0, 0, 0 }, { 16, 256, 128 }, "me_wall");
		const object_id crate = doc.create_box({ 16, 100, 0 }, { 48, 132, 32 }, "me_crate");
		const object_id glass = doc.create_box({ 100, 100, 0 }, { 132, 132, 64 }, "clip");

		CHECK(doc.auto_caulk(false) == 3);
		// the wall's bottom is on the floor; the crate's bottom is on the floor and one side is on the wall
		CHECK(count_faces_with(doc, wall, "caulk") == 1);
		CHECK(count_faces_with(doc, crate, "caulk") == 2);
		// the floor's top is only partly covered, the wall's side only partly by the crate
		CHECK(count_faces_with(doc, floor, "caulk") == 0);
		// a clip brush hides nothing and is not caulked
		CHECK(count_faces_with(doc, glass, "clip") == 6);

		// which face: the one that looks down
		const brush_geometry* geo = doc.geometry(wall);
		for (std::size_t f = 0; f < doc.find_brush(wall)->faces.size(); f++)
		{
			if (doc.find_brush(wall)->faces[f].texture.material == "caulk")
			{
				CHECK(near(geo->planes[f].normal, { 0, 0, -1 }));
			}
		}

		CHECK(doc.auto_caulk(false) == 0);		// nothing left
		CHECK(doc.undo());
		CHECK(count_faces_with(doc, wall, "caulk") == 0);

		// the selection only
		doc.select(crate);
		CHECK(doc.auto_caulk(true) == 2);
		CHECK(count_faces_with(doc, wall, "caulk") == 0);

		// "the whole map" leaves brush entities alone: a door may move away from what it covers
		document door;
		door.create_box({ 0, 0, -16 }, { 256, 256, 0 }, "me_floor");
		const object_id leaf = door.create_box({ 100, 100, 0 }, { 116, 164, 96 }, "me_door");
		door.create_brush_entity("script_brushmodel");
		CHECK(door.auto_caulk(false) == 0);
		CHECK(count_faces_with(door, leaf, "caulk") == 0);
		door.select(leaf);
		CHECK(door.auto_caulk(true) == 1);

		// noselect: "select all" leaves out what is not to be picked
		select_filter no_brushes;
		no_brushes.brushes = true;
		door.set_select_filter(no_brushes);
		door.select_all();
		CHECK(door.selection().size() == 1 && door.is_selected(leaf));
		door.set_select_filter({});

		// a face covered by two brushes together
		document two;
		const object_id top = two.create_box({ 0, 0, 0 }, { 128, 64, 64 }, "me_wall");
		two.create_box({ 0, 0, -64 }, { 64, 64, 0 }, "me_wall");
		two.create_box({ 64, 0, -64 }, { 128, 64, 0 }, "me_wall");
		two.select(top);
		CHECK(two.auto_caulk(true) == 1);

		// a material that lets one see through hides nothing
		document see;
		const object_id base = see.create_box({ 0, 0, 0 }, { 64, 64, 64 }, "me_wall");
		see.create_box({ 0, 0, 64 }, { 64, 64, 128 }, "glass_clear");
		see.select(base);
		CHECK(see.auto_caulk(true, "caulk", [](const std::string_view m) { return m != "glass_clear"; }) == 0);
		CHECK(see.auto_caulk(true) == 1);

		see.deselect_all();
		CHECK(see.auto_caulk(true) == -1);
	}

	void test_drop_to_floor()
	{
		document doc;
		doc.create_box({ -128, -128, -16 }, { 128, 128, 0 });
		doc.create_box({ 0, 0, 0 }, { 64, 64, 48 });
		const object_id on_box = point(doc, "script_model", { 32, 32, 300 }, { { "model", "com_barrel" } });
		const object_id on_floor = point(doc, "script_model", { -64, -64, 300 }, { { "model", "com_barrel" } });
		const object_id nothing = point(doc, "script_model", { 1000, 0, 300 }, { { "model", "com_barrel" } });

		const object_id ids[] = { on_box, on_floor, nothing };
		doc.select(std::span<const object_id>(ids));
		CHECK(doc.drop_to_floor() == 2);
		CHECK(near(doc.find_entity(on_box)->origin(), { 32, 32, 48 }));
		CHECK(near(doc.find_entity(on_floor)->origin(), { -64, -64, 0 }));
		CHECK(near(doc.find_entity(nothing)->origin(), { 1000, 0, 300 }));
		CHECK(doc.drop_to_floor() == 0);		// they stand on it
		CHECK(doc.undo() && near(doc.find_entity(on_box)->origin().z, 300.0));

		// a class with a box: its bottom ends on the floor
		const object_id node = point(doc, "script_origin", { -32, -32, 100 });
		const double bottom = doc.object_bounds(node).mins.z - 100.0;
		CHECK(doc.drop_to_floor() == 1);
		CHECK(near(doc.find_entity(node)->origin().z, -bottom));

		// on a slope, turned to the surface: a ramp that rises along +x by 45 degrees
		document ramp;
		const vec3 corners[] = { { 0, 0, 0 }, { 128, 0, 0 }, { 128, 0, 128 }, { 0, 128, 0 }, { 128, 128, 0 }, { 128, 128, 128 } };
		CHECK(ramp.create_brush_from_points(corners) != no_object);
		const object_id model = point(ramp, "misc_model", { 64, 64, 400 }, { { "model", "com_barrel" }, { "angles", "0 0 0" } });
		CHECK(ramp.drop_to_floor(true) == 1);
		CHECK(near(ramp.find_entity(model)->origin(), { 64, 64, 64 }, 1e-3));
		const vec3 angles = ramp.find_entity(model)->angles();
		// facing up the slope: the nose is lifted by 45 degrees ( pitch -45 = 315 )
		CHECK(near(angles.x, 315.0, 0.01) || near(angles.x, -45.0, 0.01));
		CHECK(near(angles.y, 0.0, 0.01) && near(angles.z, 0.0, 0.01));
	}

	void test_filters()
	{
		document doc;
		const object_id wall = doc.create_box({ 0, 0, 0 }, { 64, 64, 64 }, "me_wall");
		const object_id detail = doc.create_box({ 100, 0, 0 }, { 164, 64, 64 }, "me_wall");
		CHECK(doc.set_detail(true));
		const object_id clip = doc.create_box({ 200, 0, 0 }, { 264, 64, 64 }, "clip");
		const object_id caulk = doc.create_box({ 300, 0, 0 }, { 364, 64, 64 }, "caulk");
		const object_id sky = doc.create_box({ 400, 0, 0 }, { 464, 64, 64 }, "sky_test");
		const object_id trig_brush = doc.create_box({ 500, 0, 0 }, { 564, 64, 64 }, "trigger");
		const object_id trigger = doc.create_brush_entity("trigger_multiple");
		const object_id terrain = doc.create_terrain_patch({ 0, 200, 0 }, { 128, 328, 0 }, 3, 3);
		const object_id light = point(doc, "light", { 0, 0, 200 });
		const object_id model = point(doc, "misc_model", { 100, 0, 200 }, { { "model", "com_barrel" } });
		const object_id node = point(doc, "node_pathnode", { 200, 0, 200 });
		const object_id spawn = point(doc, "mp_dm_spawn", { 300, 0, 200 });
		const object_id prefab = point(doc, "misc_prefab", { 400, 0, 200 }, { { "model", "prefabs/x.map" } });
		doc.deselect_all();
		CHECK(trigger != no_object && terrain != no_object);

		const auto visible = [&](const object_id id) { return doc.is_visible(id); };
		const auto all_visible = [&] { return visible(wall) && visible(detail) && visible(clip) && visible(caulk) && visible(sky) && visible(trig_brush)
			&& visible(terrain) && visible(light) && visible(model) && visible(node) && visible(spawn) && visible(prefab); };
		CHECK(all_visible());

		const auto only = [&](const auto set)
		{
			view_filter f;
			set(f);
			const auto before = doc.visibility_counter();
			doc.set_view_filter(f);
			CHECK(doc.visibility_counter() != before);
		};

		only([](view_filter& f) { f.detail = true; });
		CHECK(!visible(detail) && visible(wall) && visible(clip));
		only([](view_filter& f) { f.structural = true; });
		// structural: what is not detail and no tool volume - a caulk or sky brush is, a clip brush is not
		CHECK(!visible(wall) && visible(detail) && visible(clip) && !visible(caulk) && !visible(sky) && visible(trig_brush));
		// a wall with a caulked face is still a wall
		doc.select_face({ wall, 0 });
		doc.set_material("caulk");
		doc.deselect_all();
		CHECK(!visible(wall));
		only([](view_filter& f) { f.caulk = true; });
		CHECK(visible(wall) && !visible(caulk));
		CHECK(doc.undo());
		only([](view_filter& f) { f.clip = true; f.caulk = true; f.sky = true; });
		CHECK(!visible(clip) && !visible(caulk) && !visible(sky) && visible(wall));
		only([](view_filter& f) { f.triggers = true; });
		CHECK(!visible(trig_brush) && visible(wall));
		only([](view_filter& f) { f.terrain = true; });
		CHECK(!visible(terrain) && visible(wall));
		only([](view_filter& f) { f.curves = true; });
		CHECK(visible(terrain));
		only([](view_filter& f) { f.lights = true; f.models = true; });
		CHECK(!visible(light) && !visible(model) && visible(node) && visible(prefab));
		only([](view_filter& f) { f.pathnodes = true; f.spawns = true; f.prefabs = true; });
		CHECK(!visible(node) && !visible(spawn) && !visible(prefab) && visible(light));
		only([](view_filter& f) { f.entities = true; });
		CHECK(!visible(light) && !visible(trig_brush) && visible(wall) && visible(terrain));
		only([](view_filter& f) { f.world = true; });
		CHECK(!visible(wall) && !visible(terrain) && visible(trig_brush) && visible(light));
		only([](view_filter& f) { f.classes = { "node_*", "light" }; });
		CHECK(!visible(node) && !visible(light) && visible(spawn));

		// what is filtered is not picked and not selected by "select all"
		only([](view_filter& f) { f.detail = true; });
		pick_result hit;
		CHECK(!doc.pick(ray{ { 132, 32, 500 }, { 0, 0, -1 } }, hit));
		CHECK(doc.pick(ray{ { 32, 32, 500 }, { 0, 0, -1 } }, hit) && hit.object == wall);
		doc.select_all();
		CHECK(!doc.is_selected(detail) && doc.is_selected(wall));
		doc.deselect_all();

		// a brush that becomes detail while the filter is on goes too, and comes back with undo
		doc.select(wall);
		CHECK(doc.set_detail(true) && !visible(wall));
		CHECK(doc.undo() && visible(wall));
		doc.deselect_all();

		doc.set_view_filter({});
		CHECK(all_visible());

		// visible but not to be picked
		select_filter no;
		no.entities = true;
		no.curves = true;
		no.sky = true;
		doc.set_select_filter(no);
		CHECK(visible(light) && !doc.is_selectable(light) && !doc.is_selectable(terrain) && !doc.is_selectable(sky) && doc.is_selectable(wall));
		CHECK(!doc.is_selectable(trig_brush));
		CHECK(!doc.pick(ray{ { 0, 0, 500 }, { 0, 0, -1 } }, hit) || hit.object != light);
		CHECK(doc.query_rect(2, { -1000, -1000 }, { 1000, 1000 }, false).size() == 4);		// wall, detail, clip, caulk
		no = {};
		no.models = true;
		no.brushes = true;
		doc.set_select_filter(no);
		CHECK(!doc.is_selectable(model) && doc.is_selectable(light) && !doc.is_selectable(wall) && doc.is_selectable(trig_brush));
		doc.set_select_filter({});

		// region: only what touches the box
		bounds box;
		box.mins = { -8, -8, -1e30 };
		box.maxs = { 180, 72, 1e30 };
		doc.set_region(box);
		CHECK(doc.has_region());
		CHECK(visible(wall) && visible(detail) && !visible(clip) && !visible(terrain) && visible(light) && visible(model) && !visible(node));
		doc.select_all();
		CHECK(doc.selection().size() == 4);
		doc.deselect_all();

		// Save Region: what touches it, with the worldspawn's keys; the build variant is closed by six walls
		doc.set_key(doc.worldspawn(), "ambient", "0.2");
		document region;
		std::string error;
		CHECK(region.load_map_text(doc.region_map_text(), error));
		CHECK(region.brush_count() == 2 && region.entities().size() == 3);
		CHECK(region.find_entity(region.worldspawn())->value("ambient") == "0.2");
		CHECK(region.load_map_text(doc.region_map_text(true, "caulk"), error));
		CHECK(region.brush_count() == 8);
		const bounds walls = world_bounds(region);
		CHECK(walls.mins.x < -8 && walls.maxs.z > 200 && walls.maxs.x > 164);

		doc.clear_region();
		CHECK(!doc.has_region() && all_visible());

		// hide, hide the rest, show what was hidden last
		doc.select(wall);
		doc.hide_selected();
		CHECK(!visible(wall));
		doc.select(detail);
		doc.hide_unselected();
		CHECK(visible(detail) && !visible(clip) && !visible(light));
		doc.show_last_hidden();
		CHECK(visible(clip) && visible(light) && !visible(wall));
		doc.show_all();
		CHECK(all_visible());

		const map_info info = doc.info();
		CHECK(info.brushes == 6 && info.world_brushes == 5 && info.detail_brushes == 1 && info.patches == 1 && info.terrain == 1);
		CHECK(info.point_entities == 5 && info.brush_entities == 1 && info.models == 1 && info.lights == 1 && info.prefabs == 1);
		CHECK(info.box.valid() && info.box.maxs.x >= 564);
		CHECK(!info.materials.empty() && info.materials[0].first == "me_wall" && info.materials[0].second == 12);
	}

	// ---- prefabs --------------------------------------------------------------------------------

	struct prefab_files
	{
		std::map<std::string, std::string> files;

		[[nodiscard]] prefab_loader loader() const
		{
			return [this](const std::string_view model, std::string& text, std::string& error)
			{
				const auto it = files.find(std::string(model));
				if (it == files.end())
				{
					error = "no such prefab";
					return false;
				}

				text = it->second;
				return true;
			};
		}
	};

	void test_prefabs()
	{
		prefab_files disk;

		// a crate: a box of 32 around its origin's x / y, standing on z 0, with a model on top
		{
			document crate;
			crate.create_box({ -16, -16, 0 }, { 16, 16, 32 }, "me_crate");
			point(crate, "misc_model", { 0, 0, 32 }, { { "model", "com_barrel" }, { "angles", "0 0 0" } });
			disk.files["prefabs/crate.map"] = crate.save_map_text();
		}

		// a stack: two crates, one of them turned and lifted, and a brush of its own
		{
			document stack;
			stack.create_box({ 64, -8, 0 }, { 80, 8, 16 }, "me_wall");
			point(stack, "misc_prefab", { 0, 0, 0 }, { { "model", "prefabs/crate.map" } });
			point(stack, "misc_prefab", { 0, 0, 32 }, { { "model", "prefabs/crate.map" }, { "angles", "0 90 0" } });
			disk.files["prefabs/stack.map"] = stack.save_map_text();
		}

		// one that contains itself
		{
			document loop;
			loop.create_box({ 0, 0, 0 }, { 8, 8, 8 });
			point(loop, "misc_prefab", { 16, 0, 0 }, { { "model", "prefabs/loop.map" } });
			disk.files["prefabs/loop.map"] = loop.save_map_text();
		}

		// ---- a prefab, moved and turned by 90 degrees
		{
			document doc;
			doc.create_box({ -512, -512, -16 }, { 512, 512, 0 });
			const object_id p = point(doc, "misc_prefab", { 100, 200, 0 }, { { "model", "prefabs/crate.map" }, { "angles", "0 90 0" } });
			CHECK(document::is_prefab(*doc.find_entity(p)));

			prefab_result result;
			CHECK(doc.expand_prefabs(disk.loader(), result));
			CHECK(result.expanded == 1 && result.brushes == 1 && result.entities == 1 && result.warnings.empty());
			CHECK(doc.kind(p) == object_kind::none && entity_count(doc, "misc_prefab") == 0);
			CHECK(doc.brush_count() == 2 && entity_count(doc, "misc_model") == 1);

			const object_id crate = doc.find_entity(doc.worldspawn())->children.at(1);
			CHECK(near(doc.object_bounds(crate).mins, { 84, 184, 0 }, 1e-6) && near(doc.object_bounds(crate).maxs, { 116, 216, 32 }, 1e-6));

			for (const object_id id : doc.entities())
			{
				if (const entity* e = doc.find_entity(id); e->classname() == "misc_model")
				{
					CHECK(near(e->origin(), { 100, 200, 32 }));
					CHECK(near(e->angles(), { 0, 90, 0 }));
				}
			}

			CHECK(doc.undo());		// one step
			CHECK(doc.kind(p) == object_kind::entity && doc.brush_count() == 1);
		}

		// ---- a turned prefab that is not symmetric: the brush of the stack at +x ends at +y
		{
			document doc;
			point(doc, "misc_prefab", { 1000, 0, 0 }, { { "model", "prefabs/stack.map" }, { "angles", "0 90 0" } });
			prefab_result result;
			CHECK(doc.expand_prefabs(disk.loader(), result));
			CHECK(result.expanded == 3);		// the stack and the two crates inside it
			CHECK(doc.brush_count() == 3 && entity_count(doc, "misc_model") == 2 && entity_count(doc, "misc_prefab") == 0);

			bool found_wall = false, found_upper = false;
			for (const object_id id : doc.find_entity(doc.worldspawn())->children)
			{
				const bounds b = doc.object_bounds(id);
				found_wall |= near(b.mins, { 992, 64, 0 }, 1e-6) && near(b.maxs, { 1008, 80, 16 }, 1e-6);
				found_upper |= near(b.mins, { 984, -16, 32 }, 1e-6) && near(b.maxs, { 1016, 16, 64 }, 1e-6);
			}
			CHECK(found_wall && found_upper);

			// the model of the upper, turned crate: 90 degrees of the crate + 90 of the stack
			bool turned = false;
			for (const object_id id : doc.entities())
			{
				if (const entity* e = doc.find_entity(id); e->classname() == "misc_model" && near(e->origin().z, 64.0))
				{
					turned = near(e->angles(), { 0, 180, 0 }, 1e-3);
				}
			}
			CHECK(turned);
		}

		// ---- stamp: the selected prefab only, one level deep
		{
			document doc;
			const object_id a = point(doc, "misc_prefab", { 0, 0, 0 }, { { "model", "prefabs/stack.map" } });
			const object_id b = point(doc, "misc_prefab", { 500, 0, 0 }, { { "model", "prefabs/crate.map" } });
			doc.select(a);

			prefab_result result;
			CHECK(doc.expand_prefabs(disk.loader(), result, true));
			CHECK(result.expanded == 1 && doc.kind(a) == object_kind::none && doc.kind(b) == object_kind::entity);
			CHECK(doc.brush_count() == 1 && entity_count(doc, "misc_prefab") == 3);		// b and the two crates of the stack
			CHECK(doc.selection().size() == 3);
			CHECK(doc.undo_count() == 3);

			doc.deselect_all();
			prefab_result none;
			CHECK(!doc.expand_prefabs(disk.loader(), none, true));
		}

		// ---- a missing file and a prefab that contains itself stay entities, with a warning
		{
			document doc;
			doc.create_box({ 0, 0, 0 }, { 8, 8, 8 });
			point(doc, "misc_prefab", { 0, 0, 0 }, { { "model", "prefabs/missing.map" } });
			point(doc, "misc_prefab", { 100, 0, 0 }, { { "model", "prefabs/loop.map" } });

			prefab_result result;
			CHECK(doc.expand_prefabs(disk.loader(), result));
			CHECK(result.expanded == 1);
			CHECK(result.warnings.size() == 2);
			CHECK(doc.brush_count() == 2);
			CHECK(entity_count(doc, "misc_prefab") == 2);		// the missing one and the inner loop
		}

		// ---- convert a selection into a prefab, and get the same map back by expanding it
		{
			document doc;
			const object_id a = doc.create_box({ 100, 100, 0 }, { 164, 164, 64 }, "me_wall");
			const object_id b = doc.create_box({ 164, 100, 0 }, { 196, 164, 32 }, "me_wall");
			const object_id s = point(doc, "mp_dm_spawn", { 132, 132, 64 }, { { "angles", "0 45 0" } });
			const object_id keep = doc.create_box({ -64, -64, 0 }, { 0, 0, 8 });
			const object_id ids[] = { a, b, s };
			doc.select(std::span<const object_id>(ids));

			std::string text;
			const object_id made = doc.convert_selection_to_prefab("prefabs/room.map", text);
			CHECK(made != no_object && !text.empty());
			CHECK(doc.brush_count() == 1 && doc.kind(keep) == object_kind::brush && entity_count(doc, "misc_prefab") == 1);
			CHECK(near(doc.find_entity(made)->origin(), { 148, 132, 0 }));
			CHECK(doc.find_entity(made)->value("model") == "prefabs/room.map");
			CHECK(doc.undo_count() == 5 && doc.is_selected(made));

			// the prefab itself is around its origin
			document room;
			std::string error;
			CHECK(room.load_map_text(text, error) && room.brush_count() == 2 && room.entities().size() == 2);
			CHECK(near(world_bounds(room).mins, { -48, -32, 0 }) && near(world_bounds(room).maxs, { 48, 32, 64 }));

			disk.files["prefabs/room.map"] = text;
			prefab_result result;
			CHECK(doc.expand_prefabs(disk.loader(), result));
			CHECK(doc.brush_count() == 3 && entity_count(doc, "mp_dm_spawn") == 1);
			bounds all;
			for (const object_id id : doc.find_entity(doc.worldspawn())->children)
			{
				if (id != keep) { all.add(doc.object_bounds(id)); }
			}
			CHECK(near(all.mins, { 100, 100, 0 }) && near(all.maxs, { 196, 164, 64 }));

			doc.deselect_all();
			CHECK(doc.convert_selection_to_prefab("prefabs/none.map", text) == no_object);
		}

		// ---- files: a map below map_source finds map_source\prefabs
		{
			const auto roots = prefab_roots_for("C:/editor/map_source/mp_test.map");
			CHECK(!roots.empty() && roots[0].filename() == "map_source");
			CHECK(resolve_prefab("prefabs/none.map", roots).empty());
			CHECK(resolve_prefab("", roots).empty());
		}
	}
}

int main()
{
	test_names();
	test_content_flags();
	test_select();
	test_volume_select();
	test_clipboard();
	test_link_and_group();
	test_csg_merge();
	test_auto_caulk();
	test_drop_to_floor();
	test_filters();
	test_prefabs();

	std::printf("\ncodol_edit_select_tests: %d checks, %d failed\n", g_checks, g_failed);
	return g_failed == 0 ? 0 : 1;
}
