#pragma once

// Test maps, written with codol_edit ( nothing here comes from a shipped map but material names and the worldspawn
// keys the shipped maps carry ).

#include "codol_edit/document.hpp"

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
}
