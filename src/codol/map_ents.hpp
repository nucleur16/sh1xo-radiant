#pragma once

// CoDOL map entities <-> radiant .map
//
// There is no bsp compiler for CoDOL, but the entities of a shipped map can still be edited:
// the dedicated server replaces the entity string of a map zone with
// <scripts>/maps/mp/<map>.d3dbsp.ents when that file names the CRC-32 of the entity string the
// zone carries ( "// base_crc32 xxxxxxxx" ). So the loop is
//
//   zone entity string  ->  radiant .map ( point entities )  ->  edit  ->  <map>.d3dbsp.ents  ->  "map <map>"
//
// Entities that reference a compiled brush model ( "model" "*N" for brush models, "?N" for
// trigger hulls ) keep that reference; they are shown as a codol_bmodel point entity because the
// hull only exists inside the zone. Moving one moves its hull ( bounds are relative to the origin ).
//
// No dependency on radiant or imgui, so it is also built into the unit tests.

namespace codol::mapents
{
	inline constexpr const char* KEY_BASE_CRC = "codol_base_crc32";
	inline constexpr const char* KEY_MAP_NAME = "codol_map";
	inline constexpr const char* KEY_ORIG_CLASS = "codol_classname";
	inline constexpr const char* KEY_TRIGGER_MODELS = "codol_trigger_models";	// number of trigger hulls ( "?N" ) the zone has
	inline constexpr const char* KEY_ID = "codol_id";			// index of the entity in the entity string it was imported from
	inline constexpr const char* CLASS_BMODEL = "codol_bmodel";

	struct entity
	{
		std::vector<std::pair<std::string, std::string>> pairs;
		bool has_brushes = false;	// only set when read from a .map
		int line = 0;

		[[nodiscard]] const std::string* find(std::string_view key) const;
		void set(std::string_view key, std::string_view value);
		void erase(std::string_view key);
	};

	struct entity_set
	{
		std::vector<entity> ents;
		bool has_crc = false;
		std::uint32_t base_crc = 0;
		int trigger_models = -1;	// MapEnts::trigger.count when the entities came out of a zone, else -1
	};

	std::uint32_t crc32(std::string_view data);

	// "{\n"k" "v"\n}\n{ .." exactly like the game stores it ( no trailing newline )
	std::string serialize_entity_string(const std::vector<entity>& ents);

	// entity string as stored in a zone, or the contents of a .d3dbsp.ents override ( "//" comment lines,
	// one of them "// base_crc32 xxxxxxxx" ). For a plain entity string, base_crc is the crc of the text itself.
	bool parse_entity_string(std::string_view text, entity_set& out, std::string& error);

	// the MapEnts asset of a decrypted / inflated zone
	struct zone_map_ents
	{
		std::size_t header_offset = 0;	// of the c1::MapEnts header
		std::size_t text_offset = 0;	// of the entity string
		std::size_t text_length = 0;	// without the terminator
		c1::MapEnts header = {};
	};

	// locate the entity string of a zone and the MapEnts header in front of it. The header is validated
	// ( inline string reference, numEntityChars == length + 1 ), so a hit is the real asset and not a look-alike.
	bool find_map_ents(std::string_view zone_bytes, zone_map_ents& out);

	// find the entity string inside a decrypted / inflated zone. `trigger_models` receives the number of trigger
	// hulls of the map when the MapEnts header was found ( -1 otherwise ).
	bool extract_from_zone(std::string_view zone_bytes, std::string& ent_text, int* trigger_models = nullptr);

	// radiant .map with one point entity per game entity
	std::string to_radiant_map(const entity_set& set, std::string_view map_name);

	struct export_result
	{
		std::string text;			// contents of <map>.d3dbsp.ents
		std::string map_name;
		std::uint32_t base_crc = 0;
		int exported = 0;
		std::vector<std::string> warnings;
	};

	// read a radiant .map that was created by to_radiant_map() ( and edited ) and build the override.
	// Every value is taken from the map as radiant saved it - see merge_radiant_map() for the precise way.
	bool from_radiant_map(std::string_view map_text, export_result& out, std::string& error);

	// Radiant rewrites entities when it saves a map, even untouched ones: origins are rounded to one decimal,
	// classes it knows from its own game get their "model" replaced, point entities gain an "origin" ..
	// To export only what the user changed, the saved map is compared with a baseline ( the same map saved by
	// radiant right after the import, before any edit ):
	//   - a key that still has its baseline value is exported with the value of the original entity string
	//     ( or not at all if the original entity did not have it )
	//   - a key that differs from the baseline, or is new, is an edit and is exported as it is
	//   - entities without a valid "codol_id" are new and exported as they are
	// `original_ents` is the entity string the map was imported from ( optionally with its "// base_crc32" line ).
	bool merge_radiant_map(std::string_view current_map, std::string_view baseline_map, std::string_view original_ents,
		export_result& out, std::string& error);
}
