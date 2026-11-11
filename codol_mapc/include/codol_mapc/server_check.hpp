#pragma once

// What a dedicated server ( CoD4x_Server, src/codol/collision ) requires from the collision data of a zone.
//
// The server does not run the game's loaders: it decodes the clip map from the zone *stream* with a reader of its
// own, and that reader accepts the shape the game's map compiler gives the data, not everything the loaders would
// take. A zone that breaks one of these rules is refused ( "CoDOL collision <map>: source decode failed (-1)" ):
//
//   brushes        every brush with sides points into clipMap.brushsides, every brush with edges into
//                  clipMap.brushEdges; in brush order the runs lie back to back and together they are the whole
//                  array ( no gaps, no unreferenced tail, nothing stored behind the brush )
//   brush sides    plane pointers lead into the plane array of the GfxWorld ( which stores it; the clip map points
//                  at it, both plane counts are equal ), material indices are clip materials
//   planes         unit normals ( length^2 within 0.001 of 1 ), finite distances
//   leaf brushes   in node order the lists of the leaf brush nodes that point into clipMap.leafbrushes lie back
//                  to back and together they are the whole array; a list may be stored behind the node array instead
//   leaf brush     a node lists brushes ( leafBrushCount > 0 ) or splits ( 0, or -1 with the node behind it ): axis
//   nodes          0 .. 2, finite dist, range >= 0, both child offsets > 0 and inside the array. Only node 0 may be
//                  all zero. There is no "empty" node.
//   leafs, models  leafBrushNode inside the node array, boxes with half sizes >= 0, radius >= 0
//   BSP            children lead forward ( child node > node ) or to a leaf
//   static models  the model pointer is a zone pointer ( never "follows" ), finite placement, half sizes >= 0
//
// check_server_collision reads the inflated zone ( XFileHeader + data ) the same way: straight from the stream, without
// codol_zone's loaders - so it is also a second reading of what the compiler wrote.

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace codol::mapc
{
	// a leaf brush node as the stream has it
	struct server_leaf_brush_node
	{
		std::uint8_t axis = 0;
		int count = 0;							// > 0: lists `brushes`; 0: splits; -1: splits, and the node behind it is the tree of the brushes on the split
		std::int32_t contents = 0;
		float dist = 0, range = 0;				// of a split
		std::uint32_t child[2] = {};			// of a split: node indices ( 0 for the reserved node 0 )
		std::vector<std::uint16_t> brushes;
	};

	// a collision leaf of the BSP, or the leaf of a brush model
	struct server_leaf
	{
		std::int32_t brush_contents = 0;
		std::uint32_t node = 0;					// root of its leaf brush tree
		float mins[3] = {}, maxs[3] = {};
	};

	struct server_brush
	{
		std::int32_t contents = 0;
		float mins[3] = {}, maxs[3] = {};
		std::uint32_t sides = 0, edges = 0;
	};

	struct server_collision_info
	{
		std::string map;						// base name of the GfxWorld ( "mp_rust" )
		std::uint32_t planes = 0, brushes = 0, brush_sides = 0, brush_edges = 0;
		std::uint32_t leaf_brush_nodes = 0, leaf_brushes = 0, inline_leaf_brushes = 0;
		std::uint32_t leafs = 0, sub_models = 0, static_models = 0, materials = 0;
		std::vector<server_leaf_brush_node> nodes;
		std::vector<server_leaf> leaf_list, model_list;
		std::vector<server_brush> brush_list;
	};

	// false + what is wrong ( the first rule that does not hold ) when the server's decoder would refuse the zone
	bool check_server_collision(std::span<const std::uint8_t> zone, std::string& error, server_collision_info* info = nullptr);
}
