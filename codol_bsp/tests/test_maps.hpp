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

	// What the helicopter script asks a map for ( maps/mp/killstreaks/_helicopter.gsc init ): script_origins named
	// heli_start ( targets the way in ), heli_loop_start ( targets a closed loop ), heli_leave and heli_crash_start.
	// Without heli_start / heli_loop_start the script sets nothing up, and a harrier - which uses its effects - runs
	// into script errors ( seen with bots on a map without them ). All nodes lie inside the map: `reach` around
	// `centre` at height `z`.
	inline void heli_prefab(edit::document& doc, const vec3& centre, const double reach, const double z)
	{
		const auto node = [&](const vec3& at, const char* name, const char* target)
		{
			std::vector<std::pair<std::string, std::string>> keys = { { "targetname", name } };
			if (target)
			{
				keys.emplace_back("target", target);
			}
			doc.create_point_entity("script_origin", at, keys);
		};
		node({ centre.x - reach, centre.y - reach, z }, "heli_start", "heli_dest_1");
		node({ centre.x, centre.y, z }, "heli_dest_1", nullptr);
		node({ centre.x + reach, centre.y - reach, z }, "heli_loop_start", "heli_loop_1");
		node({ centre.x + reach * 0.6, centre.y - reach * 0.6, z }, "heli_loop_1", "heli_loop_2");
		node({ centre.x + reach * 0.6, centre.y + reach * 0.6, z }, "heli_loop_2", "heli_loop_3");
		node({ centre.x - reach * 0.6, centre.y + reach * 0.6, z }, "heli_loop_3", "heli_loop_4");
		node({ centre.x - reach * 0.6, centre.y - reach * 0.6, z }, "heli_loop_4", "heli_loop_1");
		node({ centre.x - reach, centre.y + reach, z }, "heli_leave", nullptr);
		node({ centre.x + reach, centre.y + reach, z }, "heli_crash_start", "heli_crash_1");
		node({ centre.x + reach * 0.5, centre.y + reach * 0.3, z * 0.25 }, "heli_crash_1", nullptr);
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

	// The yard: a small multiplayer layout with everything the compiler handles. An open yard of 2048 x 1536 under
	// sky and sun, walled in; two buildings ( west and east ) with doorways and windows, lit by a light each; a mound
	// of terrain in the middle; crates as brushes, static models ( from the common zones, from inside mp_rust's
	// gfx_map, from a codol_assets project ) with clip brushes, a fence and a decal of a modder's material; spawn
	// points for dm and tdm, path nodes, an effect, a sound, reflection probes.
	// Needs the zones of tests/make_modder_assets.py ( bsp_test_* materials ) and of codol_assets' test sources
	// ( codol_test_crate ) as --assets; without them those faces and models are reported and left out.
	inline double yard_mound(const double x, const double y)
	{
		const double r2 = x * x + y * y;
		return std::fabs(x) >= 255 || std::fabs(y) >= 255 ? 0.0 : 72.0 * std::exp(-r2 / (150.0 * 150.0));
	}

	inline void make_yard(edit::document& doc)
	{
		doc.new_map();
		world_keys(doc);
		const std::string ground = "kh_concrete_02", outer = "me_cargocontainer_usmc_ext_wall", brick = "bsp_test_wall", roof = "me_concrete_riverwall",
			wood = "ch_woodplank01", sky = "sky_killhouse_sh";
		struct solid_box
		{
			vec3 mins, maxs;
		};
		std::vector<solid_box> solids;
		const auto box = [&](const vec3& mins, const vec3& maxs, const std::string& material)
		{
			solids.push_back({ mins, maxs });
			return doc.create_box(mins, maxs, material);
		};
		const double X = 1024, Y = 768, T = 16, WALL = 256, SKY = 512;
		// ground, the wall around the yard, sky above the wall and as the lid
		wall(doc, { -X - T, -Y - T, -T }, { X + T, Y + T, 0 }, { 0, 0, 1 }, ground);
		wall(doc, { -X - T, -Y - T, 0 }, { -X, Y + T, WALL }, { 1, 0, 0 }, outer);
		wall(doc, { X, -Y - T, 0 }, { X + T, Y + T, WALL }, { -1, 0, 0 }, outer);
		wall(doc, { -X, -Y - T, 0 }, { X, -Y, WALL }, { 0, 1, 0 }, outer);
		wall(doc, { -X, Y, 0 }, { X, Y + T, WALL }, { 0, -1, 0 }, outer);
		wall(doc, { -X - T, -Y - T, WALL }, { -X, Y + T, SKY }, { 1, 0, 0 }, sky);
		wall(doc, { X, -Y - T, WALL }, { X + T, Y + T, SKY }, { -1, 0, 0 }, sky);
		wall(doc, { -X, -Y - T, WALL }, { X, -Y, SKY }, { 0, 1, 0 }, sky);
		wall(doc, { -X, Y, WALL }, { X, Y + T, SKY }, { 0, -1, 0 }, sky);
		wall(doc, { -X - T, -Y - T, SKY }, { X + T, Y + T, SKY + T }, { 0, 0, -1 }, sky);

		// a wall of a building along x ( y0 .. y1 is its thickness ) or along y, with openings ( from, to, bottom, top )
		struct opening
		{
			double from, to, bottom, top;
		};
		const double H = 160;
		const auto building_wall = [&](const bool along_x, const double a0, const double a1, const double b0, const double b1, const std::vector<opening>& openings)
		{
			const auto piece = [&](const double from, const double to, const double z0, const double z1)
			{
				if (to - from < 1 || z1 - z0 < 1)
				{
					return;
				}
				if (along_x)
				{
					box({ from, b0, z0 }, { to, b1, z1 }, brick);
				}
				else
				{
					box({ b0, from, z0 }, { b1, to, z1 }, brick);
				}
			};
			double at = a0;
			for (const opening& o : openings)
			{
				piece(at, o.from, 0, H);
				piece(o.from, o.to, 0, o.bottom);
				piece(o.from, o.to, o.top, H);
				at = o.to;
			}
			piece(at, a1, 0, H);
		};
		// west building: x -896 .. -448, y -320 .. 320; a door in the east wall and one in the south wall, windows north and east
		building_wall(false, -320, 320, -896, -880, {});
		building_wall(false, -320, 320, -464, -448, { { -240, -144, 48, 104 }, { -48, 48, 0, 112 }, { 144, 240, 48, 104 } });
		building_wall(true, -880, -464, -320, -304, { { -720, -624, 0, 112 } });
		building_wall(true, -880, -464, 304, 320, { { -720, -624, 48, 104 } });
		box({ -896, -320, H }, { -448, 320, H + T }, roof);
		// east building, the mirror image with the second door to the north
		building_wall(false, -320, 320, 880, 896, {});
		building_wall(false, -320, 320, 448, 464, { { -240, -144, 48, 104 }, { -48, 48, 0, 112 }, { 144, 240, 48, 104 } });
		building_wall(true, 464, 880, 304, 320, { { 624, 720, 0, 112 } });
		building_wall(true, 464, 880, -320, -304, { { 624, 720, 48, 104 } });
		box({ 448, -320, H }, { 896, 320, H + T }, roof);

		// crates of brushes: cover in the yard and inside the buildings
		std::vector<edit::object_id> detail;
		const auto crate = [&](const double x, const double y, const double size, const double height)
		{
			detail.push_back(box({ x - size / 2, y - size / 2, 0 }, { x + size / 2, y + size / 2, height }, wood));
		};
		crate(-200, 420, 64, 64);
		crate(-136, 420, 64, 64);
		crate(-168, 420, 64, 128);
		crate(260, -440, 96, 48);
		crate(0, -560, 64, 64);
		crate(-640, 120, 64, 64);
		crate(700, -100, 64, 64);
		crate(-360, -520, 128, 40);
		crate(380, 500, 64, 96);
		doc.deselect_all();
		doc.select(detail);
		doc.set_detail(true);
		doc.deselect_all();

		// the mound: a terrain mesh over the middle of the yard
		{
			const edit::object_id id = doc.create_terrain_patch({ -256, -256, 0 }, { 256, 256, 0 }, 9, 9, 2, ground);
			doc.deselect_all();
			if (const edit::patch* p = doc.find_patch(id))
			{
				const int count = p->width * p->height;
				for (int v = 0; v < count; v++)
				{
					const vec3 at = doc.find_patch(id)->vertices[static_cast<std::size_t>(v)].xyz;
					const double z = yard_mound(at.x, at.y);
					if (z > 0)
					{
						const int index[1] = { v };
						doc.move_patch_vertices(id, index, { 0, 0, z });
					}
				}
			}
			doc.deselect_all();
		}

		// static models, each with the clip brush a mapper puts around it ( players only collide with brushes and terrain )
		const auto model = [&](const char* name, const vec3& at, const double yaw, const vec3& clip_half, const double clip_height, const char* scale = nullptr)
		{
			std::vector<std::pair<std::string, std::string>> keys = { { "model", name }, { "angles", "0 " + edit::format_key_number(yaw) + " 0" } };
			if (scale)
			{
				keys.emplace_back("modelscale", scale);
			}
			doc.create_point_entity("misc_model", at, keys);
			if (clip_height > 0)
			{
				box({ at.x - clip_half.x, at.y - clip_half.y, at.z }, { at.x + clip_half.x, at.y + clip_half.y, at.z + clip_height }, "clip");
			}
		};
		model("com_plasticcase_green_big_us_dirt", { 300, 200, 0 }, 0, { 29, 15, 0 }, 30);
		model("com_plasticcase_green_big_us_dirt", { -300, -200, 0 }, 90, { 15, 29, 0 }, 30);
		model("codol_test_crate", { 120, 560, 0 }, 0, { 24, 24, 0 }, 48);
		model("codol_test_crate", { -700, -520, 0 }, 0, { 48, 48, 0 }, 96, "2");
		model("ch_gas_pump", { 600, 420, 0 }, 180, { 20, 14, 0 }, 56);
		model("concrete_barrier_damaged_2", { -100, -400, 0 }, 0, { 40, 14, 0 }, 34);
		model("concrete_barrier_damaged_2", { 60, 380, 0 }, 90, { 14, 40, 0 }, 34);
		model("foliage_tree_palm_tall_2_static", { 620, -560, 0 }, 0, { 8, 8, 0 }, 200);
		model("foliage_tree_palm_tall_2_static", { -560, 560, 0 }, 40, { 8, 8, 0 }, 200);

		// a fence of the modder's alpha tested material and a decal of the blended one
		{
			const std::vector<edit::object_id> fence = { box({ 200, -200, 0 }, { 208, 120, 112 }, "bsp_test_fence") };
			doc.deselect_all();
			doc.select(fence);
			doc.set_detail(true);
			doc.deselect_all();
			doc.create_terrain_patch({ -380, 200, 1 }, { -252, 328, 1 }, 3, 3, 2, "bsp_test_decal");
			doc.deselect_all();
		}

		// the game's entities
		const auto dm = [&](const double x, const double y, const double yaw) { spawn(doc, "mp_dm_spawn", { x, y, 8 }, yaw); };
		const auto tdm = [&](const double x, const double y, const double yaw) { spawn(doc, "mp_tdm_spawn", { x, y, 8 }, yaw); };
		dm(-940, -700, 45); dm(-940, 700, 315); dm(940, -700, 135); dm(940, 700, 225); dm(0, 680, 270); dm(0, -680, 90);
		dm(-672, 0, 0); dm(672, 0, 180); dm(-320, 320, 315); dm(320, -320, 135); dm(-672, -480, 60); dm(672, 480, 240);
		for (int k = 0; k < 4; k++)
		{
			spawn(doc, "mp_tdm_spawn_allies_start", { -760, -180 + 120.0 * k, 8 }, 0);
			spawn(doc, "mp_tdm_spawn_axis_start", { 760, -180 + 120.0 * k, 8 }, 180);
			tdm(-540 - 60.0 * k, 520, 300);
			tdm(-540 - 60.0 * k, -620, 60);
			tdm(540 + 60.0 * k, 620, 240);
			tdm(540 + 60.0 * k, -520, 120);
		}
		spawn(doc, "mp_global_intermission", { -900, -700, 220 }, 40);
		airdrop_prefab(doc, { 900, 0, 0 });
		solids.push_back({ { 871, -15, 0 }, { 929, 15, 30 } });
		heli_prefab(doc, { 0, 0, 0 }, 700, 440);
		// path nodes: a grid over the yard and through the buildings, left out where something stands
		for (double x = -960; x <= 960; x += 96)
		{
			for (double y = -704; y <= 704; y += 96)
			{
				bool blocked = false;
				for (const solid_box& s : solids)
				{
					blocked = blocked || (x > s.mins.x - 24 && x < s.maxs.x + 24 && y > s.mins.y - 24 && y < s.maxs.y + 24 && s.mins.z < 60 && s.maxs.z > 1);
				}
				if (!blocked)
				{
					doc.create_point_entity("node_pathnode", { x, y, yard_mound(x, y) + 16 });
				}
			}
		}
		// lights in the buildings, reflection probes, an effect and a sound
		const std::vector<std::pair<std::string, std::string>> warm = { { "_color", "1 0.85 0.6" }, { "radius", "420" }, { "intensity", "1.6" } };
		doc.create_point_entity("light", { -672, 0, 130 }, warm);
		const std::vector<std::pair<std::string, std::string>> cold = { { "_color", "0.7 0.85 1" }, { "radius", "420" }, { "intensity", "1.6" } };
		doc.create_point_entity("light", { 672, 0, 130 }, cold);
		doc.create_point_entity("reflection_probe", { 0, 0, 160 });
		doc.create_point_entity("reflection_probe", { -672, 0, 80 });
		doc.create_point_entity("reflection_probe", { 672, 0, 80 });
		const std::vector<std::pair<std::string, std::string>> fire = { { "fx", "fire/firelp_small_pm" }, { "angles", "270 0 0" } };
		doc.create_point_entity("script_fx", { 260, -440, 50 }, fire);
		const std::vector<std::pair<std::string, std::string>> smoke = { { "fx", "smoke/thin_black_smoke_m" }, { "angles", "270 0 0" }, { "delay", "4" } };
		doc.create_point_entity("script_fx", { 260, -440, 60 }, smoke);
		const std::vector<std::pair<std::string, std::string>> rattle = { { "soundalias", "emt_mtl_chainlink_rattle" } };
		doc.create_point_entity("script_sound", { 204, -40, 60 }, rattle);
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
