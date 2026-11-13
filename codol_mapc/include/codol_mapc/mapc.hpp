#pragma once

// codol_mapc: incremental map compiler for Call of Duty Online 1.1.1.13.
//
// A map that was imported from a fast file with codol_edit ( document::import_fastfile ) and edited is compiled
// by applying the *difference* between the import and the edited document to the zone the import came from:
//
//   1. entities        the entity string of the MapEnts
//   2. static models   misc_model entities with "codol_static": GfxWorld draw insts, insts, cell AABB tree lists,
//                      shadow geometry lists, the clip map's static model list and its AABB tree
//   3. brushes         the clip map: brushes, sides, planes, edge lists, bounds, contents, leaf brush lists and
//                      the leaf brush node trees of the world leafs and of the brush models
//   4. surfaces        render geometry of new / edited world brushes: vertices, indices, GfxSurfaces, the sorted
//                      surface lists of the cell AABB trees
//
// Everything that is not touched by a difference stays as the zone has it; a map without differences compiles to
// the zone it was imported from, byte for byte. See README.md for what is exact, what is approximated and what
// has to be tested in the game.

#include "codol_edit/document.hpp"
#include "codol_zone/zone_editor.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace codol::mapc
{
	// ---- the difference between an import and the edited map ------------------------------------------------

	struct model_placement
	{
		std::string model;
		edit::vec3 origin;
		edit::vec3 angles;			// pitch yaw roll
		double scale = 1.0;
		bool angles_changed = true;	// moved models: false when the "angles" text is the one of the baseline
		bool scale_changed = true;
	};

	struct model_move
	{
		int zone_index = 0;			// GfxWorld static model ( dpvs.smodelDrawInsts )
		model_placement to;
	};

	// a brush of the edited document that has to be compiled
	struct brush_edit
	{
		edit::object_id brush = edit::no_object;	// in the edited document
		int sub_model = 0;							// 0: world, N: the brush model "*N"
		int replaces = -1;							// cbrush_t index it takes the place of; -1: a new brush
	};

	struct map_diff
	{
		std::vector<model_move> models_moved;
		std::vector<int> models_deleted;			// GfxWorld static model indices
		std::vector<model_placement> models_added;

		std::vector<brush_edit> brushes;			// edited ( replaces >= 0 ) and added
		std::vector<int> brushes_deleted;			// cbrush_t indices

		std::vector<std::string> not_compiled;		// things of the edited map the compiler has no stage for

		[[nodiscard]] bool empty() const
		{
			return models_moved.empty() && models_deleted.empty() && models_added.empty() && brushes.empty() && brushes_deleted.empty();
		}
	};

	// Compares the edited document with the baseline ( the document right after import_fastfile of `zone` ). The
	// zone is needed to map the objects of the baseline to zone indices; false when the baseline does not belong to it.
	bool diff_documents(const zone::Zone& zone, const edit::document& baseline, const edit::document& edited, map_diff& out, std::string& error);

	// ---- compiling ------------------------------------------------------------------------------------------

	struct options
	{
		std::filesystem::path base;				// the fast file the map was imported from
		std::filesystem::path exe;				// C1.exe ( the fast file key )
		std::filesystem::path map;				// the edited .map
		std::filesystem::path baseline;			// <map>.codol_base.map; empty: next to `map`, else a fresh import of `base`
		std::filesystem::path out;				// fast file to write
		std::filesystem::path zone_out;			// optional: the inflated zone
		std::vector<std::filesystem::path> donors;	// fast files a static model that the map zone does not have may be taken from
		// A project's own assets ( a codol_assetc zone ): every top level asset of the types named - none named: xanim,
		// sndcurve, loaded_sound, sound, fx, xmodel, material - is carried into the map zone with what it points at,
		// as codol_assetc --carry does for a map compiled from scratch. Such a zone is a donor as well.
		struct asset_source
		{
			std::filesystem::path zone;
			std::vector<std::string> types;		// names as codol_linker --list prints them
		};
		std::vector<asset_source> assets;
		// rawfiles: ( name, file ) pairs. `rawfiles` replaces or adds a rawfile; `rawfiles_appended` puts the file's text
		// behind what the zone's rawfile of that name holds ( animlists/<map>.animlist of a shipped map, to which a
		// project adds its animations ), or adds it when the zone has none
		std::vector<std::pair<std::string, std::filesystem::path>> rawfiles, rawfiles_appended;
		int level = 6;							// deflate level of the fast file
		bool entities = true, models = true, brushes = true, surfaces = true;		// stages to run
	};

	struct report
	{
		bool identical = false;					// no difference: the output zone is the base zone
		map_diff diff;
		bool entities_changed = false;
		int entities_before = 0, entities_after = 0;
		int models_moved = 0, models_deleted = 0, models_added = 0, models_hidden = 0, model_assets_removed = 0, model_assets_added = 0;
		int clip_models_moved = 0, clip_models_deleted = 0, clip_models_added = 0;
		int brushes_replaced = 0, brushes_deleted = 0, brushes_added = 0, brush_sides_added = 0, planes_added = 0, leafs_touched = 0;
		int surfaces_added = 0, vertices_added = 0, triangles_added = 0, faces_not_drawn = 0;
		int assets_carried = 0, asset_dependencies = 0, rawfiles_set = 0;
		std::uint64_t zone_bytes_before = 0, zone_bytes_after = 0;
		std::vector<std::string> notes;			// what was done, per stage
		std::vector<std::string> warnings;		// what was approximated or left out

		[[nodiscard]] std::string text() const;
	};

	// Applies the difference between `baseline` and `edited` to the zone in `editor` ( loaded from the base fast
	// file ) and returns the inflated zone. `editor` is left holding the compiled zone.
	bool compile_zone(zone::ZoneEditor& editor, const edit::document& baseline, const edit::document& edited, const options& opts,
		const zone::ClientKey& key, std::vector<std::uint8_t>& zone_bytes, report& rep, std::string& error);

	// The whole tool: reads the files named in `opts`, compiles and writes opts.out ( and opts.zone_out ).
	bool compile(const options& opts, report& rep, std::string& error);

	// the entity string a document compiles to: what document::export_entity_override gives, without its comment
	// header. `original_ents`: the entity string of the base zone.
	bool entity_string_of(const edit::document& baseline, const edit::document& edited, std::string_view original_ents, std::string& out,
		std::vector<std::string>& warnings, std::string& error);
}
