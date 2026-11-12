#pragma once

// Test maps, written with codol_edit ( nothing here comes from a shipped map but material names and the worldspawn
// keys the shipped maps carry ).

#include "codol_edit/document.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>
#include <vector>

namespace codol::bsp::test
{
	using edit::vec3;

	struct materials
	{
		std::string floor = "kh_concrete_02";
		std::string wall = "me_cinderblock_wall2_sh";
		std::string ceiling = "me_concrete_riverwall";
		std::string crate = "ch_woodplank01_sh";
		std::string other = "me_cargocontainer_usmc_ext_wall";
		std::string sky = "sky_killhouse_sh";
	};

	inline void set_key(edit::document& doc, const edit::object_id entity, const char* key, const std::string& value)
	{
		doc.set_key(entity, key, value);
	}

	// a caulk box whose faces that look along `inward` ( pointing into the room ) carry `material`
	inline edit::object_id wall(edit::document& doc, const vec3& mins, const vec3& maxs, const vec3& inward, const std::string& material)
	{
		const edit::object_id id = doc.create_box(mins, maxs, "caulk");
		const edit::brush_geometry* geo = doc.geometry(id);
		if (!geo)
		{
			return id;
		}
		for (int f = 0; f < static_cast<int>(geo->planes.size()); f++)
		{
			if (dot(geo->planes[static_cast<std::size_t>(f)].normal, inward) > 0.9)
			{
				doc.deselect_all();
				doc.select_face({ id, f });
				doc.set_material(material);
			}
		}
		doc.deselect_all();
		return id;
	}

	inline edit::object_id spawn(edit::document& doc, const char* classname, const vec3& origin, const double yaw)
	{
		const std::vector<std::pair<std::string, std::string>> pairs = { { "angles", "0 " + edit::format_key_number(yaw) + " 0" } };
		return doc.create_point_entity(classname, origin, pairs);
	}

	inline void world_keys(edit::document& doc)
	{
		const edit::object_id ws = doc.worldspawn();
		// the keys ( not the values ) of the shipped maps' worldspawn
		set_key(doc, ws, "sunlight", "1.4");
		set_key(doc, ws, "suncolor", "1 0.93 0.82");
		set_key(doc, ws, "sundirection", "-50 35 0");
		set_key(doc, ws, "ambient", ".05");
		set_key(doc, ws, "_color", "1 0.93 0.82");
		set_key(doc, ws, "skylight", ".5");
		set_key(doc, ws, "skycolor", "0.8 0.9 1");
	}

	inline void game_entities(edit::document& doc, const vec3& centre, const double reach, const double floor)
	{
		// spawn points for the gametypes the server's scripts ask for; a ring around the centre
		const char* ring[] = { "mp_dm_spawn", "mp_tdm_spawn", "mp_dm_spawn", "mp_tdm_spawn", "mp_dm_spawn", "mp_tdm_spawn", "mp_dm_spawn", "mp_tdm_spawn" };
		for (int k = 0; k < 8; k++)
		{
			const double a = k * 3.14159265358979 / 4.0;
			spawn(doc, ring[k], { centre.x + std::cos(a) * reach, centre.y + std::sin(a) * reach, floor + 8 }, std::fmod(k * 45.0 + 180.0, 360.0));
		}
		spawn(doc, "mp_tdm_spawn_allies_start", { centre.x - reach, centre.y - reach * 0.5, floor + 8 }, 0);
		spawn(doc, "mp_tdm_spawn_allies_start", { centre.x - reach, centre.y + reach * 0.5, floor + 8 }, 0);
		spawn(doc, "mp_tdm_spawn_axis_start", { centre.x + reach, centre.y - reach * 0.5, floor + 8 }, 180);
		spawn(doc, "mp_tdm_spawn_axis_start", { centre.x + reach, centre.y + reach * 0.5, floor + 8 }, 180);
		spawn(doc, "mp_global_intermission", { centre.x, centre.y - reach * 0.8, floor + 120 }, 90);
	}

	// path nodes on a grid: what the server's bots walk on ( it links node_pathnode entities itself )
	inline void path_nodes(edit::document& doc, const vec3& mins, const vec3& maxs, const double step, const double z)
	{
		for (double x = mins.x; x <= maxs.x; x += step)
		{
			for (double y = mins.y; y <= maxs.y; y += step)
			{
				doc.create_point_entity("node_pathnode", { x, y, z });
			}
		}
	}

	// What the game's killstreak script asks every map for ( maps/mp/killstreaks/_airdrop.gsc ): a care package model
	// that targets its collision, a brush model. The script takes both away at the start of a match.
	inline void airdrop_prefab(edit::document& doc, const vec3& at)
	{
		const edit::object_id box = doc.create_box({ at.x - 29, at.y - 15, at.z }, { at.x + 29, at.y + 15, at.z + 30 }, "clip");
		doc.deselect_all();
		doc.select(box);
		const std::vector<std::pair<std::string, std::string>> collision = { { "targetname", "airdrop_collision" }, { "script_gameobjectname", "airdrop_pallet" } };
		doc.create_brush_entity("script_brushmodel", collision);
		doc.deselect_all();
		const std::vector<std::pair<std::string, std::string>> crate = { { "model", "com_plasticcase_green_big_us_dirt" }, { "targetname", "care_package" },
			{ "target", "airdrop_collision" } };
		doc.create_point_entity("script_model", { at.x, at.y, at.z }, crate);
	}

	// M1: a hollow room, floor 1024 x 1024, 320 high, a few crates, spawn points, a light
	// skylight: the half of the ceiling over x > 0 is sky, so the sun shines in
	inline void make_box_room(edit::document& doc, const materials& m = {}, const bool skylight = false)
	{
		doc.new_map();
		world_keys(doc);
		const double h = 512, t = 16, top = 320;
		wall(doc, { -h - t, -h - t, -t }, { h + t, h + t, 0 }, { 0, 0, 1 }, m.floor);
		if (skylight)
		{
			wall(doc, { -h - t, -h - t, top }, { 0, h + t, top + t }, { 0, 0, -1 }, m.ceiling);
			wall(doc, { 0, -h - t, top }, { h + t, h + t, top + t }, { 0, 0, -1 }, m.sky);
		}
		else
		{
			wall(doc, { -h - t, -h - t, top }, { h + t, h + t, top + t }, { 0, 0, -1 }, m.ceiling);
		}
		wall(doc, { -h - t, -h - t, 0 }, { -h, h + t, top }, { 1, 0, 0 }, m.wall);
		wall(doc, { h, -h - t, 0 }, { h + t, h + t, top }, { -1, 0, 0 }, m.wall);
		wall(doc, { -h, -h - t, 0 }, { h, -h, top }, { 0, 1, 0 }, m.other);
		wall(doc, { -h, h, 0 }, { h, h + t, top }, { 0, -1, 0 }, m.other);
		// crates: one alone, two stacked, a low one
		std::vector<edit::object_id> crates;
		crates.push_back(doc.create_box({ 128, 128, 0 }, { 192, 192, 64 }, m.crate));
		crates.push_back(doc.create_box({ -256, 64, 0 }, { -192, 128, 64 }, m.crate));
		crates.push_back(doc.create_box({ -256, 64, 64 }, { -192, 128, 128 }, m.crate));
		crates.push_back(doc.create_box({ -64, -320, 0 }, { 64, -256, 32 }, m.crate));
		doc.select(crates);
		doc.set_detail(true);
		doc.deselect_all();
		game_entities(doc, { 0, 0, 0 }, 384, 0);
		airdrop_prefab(doc, { -440, -440, 0 });
		path_nodes(doc, { -416, -416, 0 }, { 416, 416, 0 }, 104, 16);
		const std::vector<std::pair<std::string, std::string>> light = { { "_color", "1 0.85 0.6" }, { "radius", "400" }, { "intensity", "1.5" } };
		doc.create_point_entity("light", { 300, -300, 200 }, light);
	}

	// M3: two rooms joined by a corridor. Every brush is textured on all sides, so the compiler has faces to remove.
	// Room A: x -1024 .. -256, room B: x 256 .. 1024, both y -384 .. 384 and 256 high; the corridor between them is
	// 128 wide and 128 high. leak: a gap of 200 units in room B's south wall.
	inline void make_two_rooms(edit::document& doc, const materials& m = {}, const bool leak = false)
	{
		doc.new_map();
		world_keys(doc);
		const auto box = [&](const vec3& mins, const vec3& maxs, const std::string& material) { return doc.create_box(mins, maxs, material); };
		box({ -1040, -400, -16 }, { 1040, 400, 0 }, m.floor);
		for (const double side : { -1.0, 1.0 })
		{
			const double x0 = side < 0 ? -1040 : 240, x1 = side < 0 ? -240 : 1040;
			box({ x0, -400, 256 }, { x1, 400, 272 }, m.ceiling);
			box({ x0, 384, 0 }, { x1, 400, 256 }, m.wall);
			box({ x0, -400, 0 }, { leak && side > 0 ? x1 - 200 : x1, -384, 256 }, m.wall);
			const double outer0 = side < 0 ? -1040 : 1024, outer1 = side < 0 ? -1024 : 1040;
			box({ outer0, -384, 0 }, { outer1, 384, 256 }, m.wall);
			// the wall towards the corridor, with the doorway
			const double inner0 = side < 0 ? -256 : 240, inner1 = side < 0 ? -240 : 256;
			box({ inner0, -384, 0 }, { inner1, -64, 256 }, m.other);
			box({ inner0, 64, 0 }, { inner1, 384, 256 }, m.other);
			box({ inner0, -64, 128 }, { inner1, 64, 256 }, m.other);
		}
		box({ -240, -80, 0 }, { 240, -64, 128 }, m.wall);
		box({ -240, 64, 0 }, { 240, 80, 128 }, m.wall);
		box({ -240, -80, 128 }, { 240, 80, 144 }, m.ceiling);
		std::vector<edit::object_id> crates;
		crates.push_back(box({ 600, -200, 0 }, { 664, -136, 64 }, m.crate));
		crates.push_back(box({ 600, -200, 64 }, { 664, -136, 128 }, m.crate));
		crates.push_back(box({ -700, 200, 0 }, { -636, 264, 64 }, m.crate));
		doc.select(crates);
		doc.set_detail(true);
		doc.deselect_all();
		game_entities(doc, { -640, 0, 0 }, 240, 0);
		game_entities(doc, { 640, 0, 0 }, 240, 0);
		airdrop_prefab(doc, { -960, -340, 0 });
		path_nodes(doc, { -960, -320, 0 }, { -320, 320, 0 }, 128, 16);
		path_nodes(doc, { 320, -320, 0 }, { 960, 320, 0 }, 128, 16);
		path_nodes(doc, { -256, 0, 0 }, { 256, 0, 0 }, 128, 16);
	}

	// M5: two rooms with one wall between them. The doorway holds a portal brush; a door ( script_brushmodel ) stands
	// open beside it, a trigger_multiple fills a corner, a terrain patch is a platform in room 1, the ceiling of room 2
	// is sky, and there are a misc_model, a spot light and a reflection probe.
	inline void make_content(edit::document& doc, const materials& m = {})
	{
		doc.new_map();
		world_keys(doc);
		const auto box = [&](const vec3& mins, const vec3& maxs, const std::string& material) { return doc.create_box(mins, maxs, material); };
		box({ -528, -272, -16 }, { 544, 272, 0 }, m.floor);
		box({ -528, -272, 192 }, { 16, 272, 208 }, m.ceiling);
		wall(doc, { 16, -272, 192 }, { 544, 272, 208 }, { 0, 0, -1 }, m.sky);
		box({ -528, -272, 0 }, { -512, 272, 192 }, m.wall);
		box({ 528, -272, 0 }, { 544, 272, 192 }, m.wall);
		box({ -512, 256, 0 }, { 528, 272, 192 }, m.wall);
		box({ -512, -272, 0 }, { 528, -256, 192 }, m.wall);
		// the wall between the rooms, with a doorway 96 wide and 112 high
		box({ 0, -256, 0 }, { 16, -48, 192 }, m.other);
		box({ 0, 48, 0 }, { 16, 256, 192 }, m.other);
		box({ 0, -48, 112 }, { 16, 48, 192 }, m.other);
		// the portal brush in the doorway: one face is the portal
		wall(doc, { 4, -48, 0 }, { 12, 48, 112 }, { 1, 0, 0 }, "portal");
		// a door, open
		{
			const edit::object_id door = box({ 20, 52, 0 }, { 28, 148, 112 }, m.crate);
			doc.deselect_all();
			doc.select(door);
			const std::vector<std::pair<std::string, std::string>> keys = { { "targetname", "test_door" } };
			doc.create_brush_entity("script_brushmodel", keys);
			doc.deselect_all();
		}
		// a trigger
		{
			const edit::object_id trigger = box({ 400, -240, 0 }, { 512, -128, 96 }, "trigger");
			doc.deselect_all();
			doc.select(trigger);
			const std::vector<std::pair<std::string, std::string>> keys = { { "targetname", "test_trigger" } };
			doc.create_brush_entity("trigger_multiple", keys);
			doc.deselect_all();
		}
		// a terrain patch: a platform 24 above the floor of room 1
		doc.create_terrain_patch({ -400, -150, 24 }, { -208, 42, 24 }, 5, 5, 2, m.crate);
		doc.deselect_all();
		const std::vector<edit::object_id> crate = { box({ 200, 100, 0 }, { 264, 164, 64 }, m.crate) };
		doc.select(crate);
		doc.set_detail(true);
		doc.deselect_all();
		game_entities(doc, { -256, 0, 0 }, 180, 0);
		game_entities(doc, { 272, 0, 0 }, 180, 0);
		airdrop_prefab(doc, { -470, -230, 0 });
		path_nodes(doc, { -448, -192, 0 }, { -64, 192, 0 }, 128, 16);
		path_nodes(doc, { 80, -192, 0 }, { 464, 192, 0 }, 128, 16);
		const std::vector<std::pair<std::string, std::string>> model = { { "model", "com_plasticcase_green_big_us_dirt" }, { "angles", "0 30 0" } };
		doc.create_point_entity("misc_model", { -100, 150, 0 }, model);
		const std::vector<std::pair<std::string, std::string>> spot = { { "_color", "0.6 0.8 1" }, { "radius", "500" }, { "intensity", "2" }, { "fov_outer", "70" },
			{ "fov_inner", "40" }, { "angles", "80 0 0" } };
		doc.create_point_entity("light", { -256, 0, 180 }, spot);
		doc.create_point_entity("reflection_probe", { -256, 0, 96 });
		doc.create_point_entity("reflection_probe", { 272, 0, 96 });
	}

	// Modder materials: the sky room with walls, a fence and a decal whose materials exist in no zone of the install
	// ( tests/make_modder_assets.py + codol_assetc make them from PNGs; the zone is given with --assets ). The fence
	// is a structural brush on purpose: it must not hide the wall behind it. The decal is a flat patch on the floor.
	inline void make_modder(edit::document& doc)
	{
		materials m;
		m.wall = "bsp_test_wall";
		make_box_room(doc, m, true);
		doc.create_box({ -320, 192, 0 }, { 320, 200, 160 }, "bsp_test_fence");
		doc.deselect_all();
		doc.create_terrain_patch({ 200, -120, 1 }, { 456, 136, 1 }, 3, 3, 2, "bsp_test_decal");
		doc.deselect_all();
	}

	// Terrain: the sky room with a terrain mesh that is a ramp ( x -448 .. -192: z = ( x + 448 ) / 2, a slope of 26.6
	// degrees ) and a plateau at z 128 ( x -192 .. 64 ), y -256 .. 256. The plateau ends in the air at x 64.
	inline double terrain_height(const double x)
	{
		return std::clamp((x + 448.0) * 0.5, 0.0, 128.0);
	}

	inline void make_terrain(edit::document& doc, const materials& m = {})
	{
		make_box_room(doc, m, true);
		const edit::object_id id = doc.create_terrain_patch({ -448, -256, 0 }, { 64, 256, 0 }, 9, 9, 2, m.floor);
		doc.deselect_all();
		if (const edit::patch* p = doc.find_patch(id))
		{
			const int count = p->width * p->height;
			for (int v = 0; v < count; v++)
			{
				const edit::patch* now = doc.find_patch(id);
				const double z = terrain_height(now->vertices[static_cast<std::size_t>(v)].xyz.x);
				if (z > 0)
				{
					const int index[1] = { v };
					doc.move_patch_vertices(id, index, { 0, 0, z });
				}
			}
		}
		doc.deselect_all();
		// a ledge: a mesh folded into a wall ( x 200, z 0 .. 96, facing -x ) and a top ( x 200 .. 328, z 96 ). The edge
		// where they meet is a border of the clip map's terrain.
		const edit::object_id ledge = doc.create_terrain_patch({ 200, -256, 0 }, { 328, 256, 0 }, 3, 5, 2, m.crate);
		doc.deselect_all();
		if (const edit::patch* p = doc.find_patch(ledge))
		{
			const int count = p->width * p->height;
			for (int v = 0; v < count; v++)
			{
				const double x = doc.find_patch(ledge)->vertices[static_cast<std::size_t>(v)].xyz.x;
				const int index[1] = { v };
				if (x > 300)
				{
					doc.move_patch_vertices(ledge, index, { 0, 0, 96 });
				}
				else if (x > 230)
				{
					doc.move_patch_vertices(ledge, index, { 200 - x, 0, 96 });
				}
			}
		}
		doc.deselect_all();
	}

	// M6: n x n rooms of 512 x 512, every room joined to its neighbours by doorways, crates in each. All brushes are
	// textured on all sides.
	inline void make_big(edit::document& doc, const int n, const materials& m = {})
	{
		doc.new_map();
		world_keys(doc);
		const auto box = [&](const vec3& mins, const vec3& maxs, const std::string& material) { return doc.create_box(mins, maxs, material); };
		const double size = 512, t = 16, height = 192, half = n * size * 0.5;
		std::vector<edit::object_id> crates;
		for (int i = 0; i <= n; i++)
		{
			for (int j = 0; j < n; j++)
			{
				// walls along y at x = i ( between room i - 1 and i ), and along x at y = i: with a doorway unless on the outside
				const double at = -half + i * size, lo = -half + j * size;
				const bool outer = i == 0 || i == n;
				if (outer)
				{
					box({ at - t / 2, lo, 0 }, { at + t / 2, lo + size, height }, m.wall);
					box({ lo, at - t / 2, 0 }, { lo + size, at + t / 2, height }, m.wall);
					continue;
				}
				box({ at - t / 2, lo, 0 }, { at + t / 2, lo + 192, height }, m.other);
				box({ at - t / 2, lo + 320, 0 }, { at + t / 2, lo + size, height }, m.other);
				box({ at - t / 2, lo + 192, 112 }, { at + t / 2, lo + 320, height }, m.other);
				box({ lo, at - t / 2, 0 }, { lo + 192, at + t / 2, height }, m.wall);
				box({ lo + 320, at - t / 2, 0 }, { lo + size, at + t / 2, height }, m.wall);
				box({ lo + 192, at - t / 2, 112 }, { lo + 320, at + t / 2, height }, m.wall);
			}
		}
		for (int i = 0; i < n; i++)
		{
			for (int j = 0; j < n; j++)
			{
				const double x = -half + i * size, y = -half + j * size;
				box({ x, y, -t }, { x + size, y + size, 0 }, m.floor);
				if ((i + j) % 3 == 0)
				{
					wall(doc, { x, y, height }, { x + size, y + size, height + t }, { 0, 0, -1 }, m.sky);
				}
				else
				{
					box({ x, y, height }, { x + size, y + size, height + t }, m.ceiling);
				}
				crates.push_back(box({ x + 96, y + 96, 0 }, { x + 160, y + 160, 64 }, m.crate));
				crates.push_back(box({ x + 352, y + 380, 0 }, { x + 416, y + 444, 48 }, m.crate));
				if ((i * 7 + j * 3) % 5 == 0)
				{
					spawn(doc, "mp_dm_spawn", { x + 256, y + 256, 8 }, (i * 90 + j * 45) % 360);
					spawn(doc, "mp_tdm_spawn", { x + 256, y + 200, 8 }, (i * 45 + j * 90) % 360);
				}
				doc.create_point_entity("node_pathnode", { x + 256, y + 256, 16 });
				doc.create_point_entity("node_pathnode", { x + 256, y + 16, 16 });
				doc.create_point_entity("node_pathnode", { x + 16, y + 256, 16 });
			}
		}
		// the outside of the grid: the outer walls are half inside it, close the gap around the floors and ceilings
		box({ -half - t, -half - t, -t }, { half + t, -half, height + t }, m.wall);
		box({ -half - t, half, -t }, { half + t, half + t, height + t }, m.wall);
		box({ -half - t, -half, -t }, { -half, half, height + t }, m.wall);
		box({ half, -half, -t }, { half + t, half, height + t }, m.wall);
		doc.select(crates);
		doc.set_detail(true);
		doc.deselect_all();
		spawn(doc, "mp_tdm_spawn_allies_start", { -half + 200, -half + 256, 8 }, 0);
		spawn(doc, "mp_tdm_spawn_axis_start", { half - 200, half - 256, 8 }, 180);
		spawn(doc, "mp_global_intermission", { 0, 40, 120 }, 90);
		airdrop_prefab(doc, { -half + 60, -half + 40, 0 });
	}
}
