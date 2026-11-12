#pragma once

// codol_bsp: map compiler for Call of Duty Online 1.1.1.13. A .map ( brushes, patches, entities, misc_models,
// lights ) in, one fast file with every asset a map zone of this game holds out. Nothing is taken from a shipped map
// zone: the map assets ( GfxWorld, clipMap, ComWorld, GameWorldMp, FxWorld, MapEnts ) are built from the .map; the
// assets a map only uses ( materials with their technique sets and images, xmodels ) are looked up in the install's
// zones and carried over.
//
// See codol_bsp/README.md for formats, algorithms, what is approximate and what is taken on trust.

#include "codol_edit/document.hpp"
#include "codol_zone/fastfile.hpp"

#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

namespace codol::bsp
{
	struct options
	{
		std::filesystem::path map;			// the .map
		std::filesystem::path exe;			// C1.exe / codoMP_client_shipRetail.exe: fast file key
		std::filesystem::path install;		// the game install: its zones are where materials and models come from
		std::filesystem::path out;			// <mapname>.ff
		std::string name;					// map name; default: the stem of `out`
		std::filesystem::path zone_out;		// optional: the inflated zone
		std::filesystem::path cache;		// folder for the install catalog; default: next to `out`
		std::filesystem::path load_out;		// optional: <mapname>_load.ff; default: next to `out` ( "" + no_load: none )
		std::vector<std::filesystem::path> donors;	// zones searched for materials / models ahead of the install
		std::vector<std::pair<std::string, std::filesystem::path>> rawfiles;	// extra rawfiles: name in the zone, file
		int level = 6;						// deflate level of the container
		std::uint64_t exact_size = 0;		// > 0: the file is made exactly this long ( a zone that takes the place of a stock zone: the client checks stock lengths )
		int quality = 1;					// light bake: 0 = --fast, higher = more samples
		bool light = true;					// false: --no-light, flat lighting
		bool vis = true;					// false: one cell, no portals
		bool load_zone = true;				// also write <mapname>_load.ff
		bool verbose = false;
	};

	struct leak_point
	{
		float xyz[3];
	};

	struct report
	{
		std::string map_name;
		// input
		std::size_t brushes = 0, structural_brushes = 0, detail_brushes = 0, patches = 0, entities = 0, static_models = 0, lights = 0;
		// collision
		std::size_t clip_planes = 0, clip_brushes = 0, clip_sides = 0, clip_nodes = 0, clip_leafs = 0, leaf_brush_nodes = 0, clip_materials = 0;
		std::size_t sub_models = 0;
		// render
		std::size_t surfaces = 0, vertices = 0, triangles = 0, world_materials = 0, cells = 0, portals = 0, faces_culled = 0;
		std::size_t lightmaps = 0, light_grid_points = 0, primary_lights = 0, reflection_probes = 0;
		// leak
		bool leaked = false;
		std::string leak_entity;
		std::vector<leak_point> leak_path;	// from the entity to the outside, through leaf centres and portals
		// output
		std::size_t zone_bytes = 0, file_bytes = 0, assets = 0;
		double seconds = 0;
		std::vector<std::pair<std::string, double>> stage_seconds;
		std::vector<std::string> notes, warnings;

		std::string text() const;
	};

	// files in, fast file out ( what the tool calls ). false + error: nothing was written.
	bool compile(const options& opts, report& rep, std::string& error);

	// the same from a document that is already loaded; gives the inflated zone
	bool compile_document(const edit::document& doc, const options& opts, const zone::ClientKey& key,
		std::vector<std::uint8_t>& zone_bytes, report& rep, std::string& error);

	// Offline validation of a map zone ( inflated: XFileHeader + data ), as far as the dedicated server and the
	// renderer read it: the server's collision decoder rules ( codol_mapc check_server_collision ) and the checks of
	// this library on GfxWorld, ComWorld, GameWorldMp, MapEnts. Run at the end of every compile. `summary` receives
	// the counts that were read back.
	bool validate_zone(std::span<const std::uint8_t> zone_bytes, std::vector<std::string>& problems, std::string* summary = nullptr);
}
