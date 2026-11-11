#pragma once

// Geometry of a shipped CoDOL map, read from the metadata CODOL-AssetExplorer writes next to a map export
// ( `CodolSoundDumper export-map --scope brushes --layout separate` -> <name>.map.json and
//   `... --scope static-models --layout instanced` -> <name>.map.json ).
//
// Together with the entity string this is enough to rebuild an editable radiant map: every collision brush
// with the material of each side, the brushes of every brush entity, and every static model placement.
//
// No dependency on radiant or imgui, so it is also built into the unit tests and the codol_mapconv tool.

namespace codol::mapgeo
{
	struct brush_side
	{
		double normal[3];	// world space, pointing out of the brush
		double dist;		// normal . p = dist
		std::string material;
	};

	struct brush
	{
		int index = 0;			// cbrush_t index in the clipmap
		int sub_model = 0;		// 0: world, N: brush entity with "model" "*N"
		std::uint32_t contents = 0;
		std::vector<brush_side> sides;
	};

	struct static_model
	{
		std::string model;
		double origin[3];
		double angles[3];		// pitch yaw roll, as radiant / the game write them
		double scale = 1.0;
	};

	struct map_geometry
	{
		std::string map;
		std::vector<brush> brushes;
		std::vector<static_model> models;

		[[nodiscard]] bool empty() const { return brushes.empty() && models.empty(); }
	};

	// row major 4x4 as the exporter writes it: rows 0-2 are the axes ( forward, left, up ), row 3 the translation
	void angles_from_axes(const double m[16], double angles[3], double* uniform_scale = nullptr);

	// both functions add to `out`; false + error if the text is not such a file
	bool read_brushes_json(const std::string& text, map_geometry& out, std::string& error);
	bool read_static_models_json(const std::string& text, map_geometry& out, std::string& error);

	// " ( x y z ) ( x y z ) ( x y z ) material 64 64 0 0 0 0 lightmap_gray 16384 16384 0 0 0 0" for one side
	std::string side_to_map_line(const brush_side& side);

	// "// brush N\n{\n ...sides... }\n"
	void append_brush(std::string& out, const brush& b, int number);

	// format a coordinate the way radiant does: no exponent, no trailing zeros
	std::string format_number(double value);
}
