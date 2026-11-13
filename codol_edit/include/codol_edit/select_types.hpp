#pragma once

// codol_edit: types of the Selection / Region / Filter / Prefab operations of a document ( document_select.inc ).

#include "codol_edit/model.hpp"

#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace codol::edit
{
	// What a material name says about a brush face. The names are those of the 1.1.1.13 zones: every shipped map
	// carries "caulk" ( contents solid, not drawn ), the clip family "clip", "clip_player", "clip_weap" .., "trigger",
	// "portal" / "portal_nodraw", "hint", "skip", "sky*".
	enum class material_class : std::uint8_t
	{
		normal,
		caulk,		// caulk, caulk_shadow, sky_caulk ..: solid, never drawn
		clip,		// clip*: collision only
		trigger,	// trigger*, aitrig
		portal,		// portal*
		hint,		// hint*, skip
		sky,		// sky*
		nodraw,		// nodraw*, lightgrid*, ladder, mantle*: tool volumes
	};

	// "wc/" in front of a name is ignored
	material_class classify_material(std::string_view material);
	// anything but material_class::normal
	bool is_tool_material(std::string_view material);
	// the material hidden faces get ( Auto Caulk ) and new brushes start with
	inline constexpr std::string_view CAULK_MATERIAL = "caulk";

	// '*' stands for any text, everything else has to match ( case insensitive ); "" matches everything
	bool wildcard_match(std::string_view pattern, std::string_view text);

	// What is not shown ( and therefore not picked, not selected by "select all", not drawn by any view ). View
	// state like hidden objects: not saved, not part of undo.
	struct view_filter
	{
		// brushes, by the materials of their faces and their contents line
		bool clip = false;				// a face with a clip* material
		bool weapon_clip = false;		// "contents weaponClip;" or clip_weap*
		bool caulk = false;				// all faces caulk
		bool detail = false;			// "contents detail;"
		bool structural = false;		// world brushes that are not detail and not a tool volume
		bool non_colliding = false;		// "contents nonColliding;"
		bool triggers = false;			// a trigger* face, and trigger_* entities with their brushes
		bool portals = false;
		bool hints = false;				// hint*, skip
		bool sky = false;
		// patches
		bool curves = false;
		bool terrain = false;
		// entities
		bool entities = false;			// everything but the worldspawn
		bool world = false;				// brushes and patches of the worldspawn
		bool brush_entities = false;	// entities that own brushes ( script_brushmodel .. ), with their brushes
		bool models = false;			// entities with an xmodel in "model" ( misc_model, script_model )
		bool lights = false;			// light*
		bool pathnodes = false;			// node_*
		bool spawns = false;			// *spawn*, mp_global_intermission, info_player_start
		bool script = false;			// script_origin, script_struct .. ( not script_model, not script_brushmodel )
		bool info = false;				// info_*
		bool prefabs = false;			// misc_prefab
		bool other = false;				// point entities of no group above
		std::vector<std::string> classes;	// further classnames, '*' allowed

		[[nodiscard]] bool any() const
		{
			return clip || weapon_clip || caulk || detail || structural || non_colliding || triggers || portals || hints || sky || curves || terrain
				|| entities || world || brush_entities || models || lights || pathnodes || spawns || script || info || prefabs || other || !classes.empty();
		}
	};

	// what stays visible but is not picked ( the toolbar toggles "don't select .." of radiant )
	struct select_filter
	{
		bool curves = false;		// patches
		bool entities = false;		// point entities and brushes of brush entities
		bool sky = false;
		bool models = false;
		bool brushes = false;		// world brushes

		[[nodiscard]] bool any() const { return curves || entities || sky || models || brushes; }
	};

	enum class volume_select
	{
		touching,		// what touches the selected brushes; they stay selected
		inside,			// what lies completely inside the box of the selection; the selecting brushes are deleted
		complete_tall,	// like inside, ignoring the view axis
		partial_tall,	// what touches the box, ignoring the view axis
	};

	// counts of a map ( Edit > Map Info )
	struct map_info
	{
		std::size_t entities = 0, point_entities = 0, brush_entities = 0;
		std::size_t brushes = 0, world_brushes = 0, detail_brushes = 0, patches = 0, curves = 0, terrain = 0;
		std::size_t models = 0, lights = 0, prefabs = 0, faces = 0;
		bounds box;
		std::vector<std::pair<std::string, std::size_t>> classes;		// by name
		std::vector<std::pair<std::string, std::size_t>> materials;		// faces / patches per material, most used first
	};

	// ---- prefabs ------------------------------------------------------------------------------------------------
	// A misc_prefab entity stands for the contents of another .map: "model" names it relative to map_source
	// ( "prefabs/crate.map" ), "origin" and "angles" place it.

	// gives the text of a prefab .map; false + error when it cannot be read
	using prefab_loader = std::function<bool(std::string_view model, std::string& text, std::string& error)>;

	struct prefab_result
	{
		int expanded = 0;						// misc_prefab entities replaced ( nested ones count )
		int brushes = 0, patches = 0, entities = 0;	// objects that came out of them
		std::vector<std::string> warnings;		// a file that is missing, a prefab that contains itself ..
	};
}
