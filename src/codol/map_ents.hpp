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
	};

	std::uint32_t crc32(std::string_view data);

	// "{\n"k" "v"\n}\n{ .." exactly like the game stores it ( no trailing newline )
	std::string serialize_entity_string(const std::vector<entity>& ents);

	// entity string as stored in a zone, or the contents of a .d3dbsp.ents override ( "//" comment lines,
	// one of them "// base_crc32 xxxxxxxx" ). For a plain entity string, base_crc is the crc of the text itself.
	bool parse_entity_string(std::string_view text, entity_set& out, std::string& error);

	// find the entity string inside a decrypted / inflated zone
	bool extract_from_zone(std::string_view zone_bytes, std::string& ent_text);

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

	// read a radiant .map that was created by to_radiant_map() ( and edited ) and build the override
	bool from_radiant_map(std::string_view map_text, export_result& out, std::string& error);
}
