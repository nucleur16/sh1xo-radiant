#pragma once

#include "render.hpp"

namespace codol::bsp
{
	struct vis_result
	{
		bool built = false;							// false: no structural brushes; the map is one cell
		std::vector<std::uint16_t> cell_nodes;		// GfxWorld.dpvsPlanes.nodes
		std::vector<render_cell> cells;				// box and portals
		std::size_t tree_nodes = 0, leafs = 0, solid_leafs = 0, empty_leafs = 0, void_leafs = 0, tree_portals = 0, portals = 0;
		std::string invariant_error;				// a self check of the tree that failed
	};

	// Sets map_face::visible and map_face::cells of the world's brushes, reports a leak in ctx.rep.
	void build_vis(compile_context& ctx, vis_result& out);
}
