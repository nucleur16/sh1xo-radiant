#pragma once

// A trace through the brushes of a zone's clip map, to check what codol_mapc writes.
//
// This is this library's own walk of the zone data, written from the layouts and from how the Quake 3 / IW
// engines are known to trace ( BSP nodes -> leafs -> leaf brush node tree -> brush planes ). It is NOT the game's
// code and proves nothing about what the game does with a zone - it proves that the structures are consistent:
// that a brush can be reached through the trees from every place a segment passes, and that its planes enclose
// the volume they should. Only brushes are tested ( no terrain triangles, no static models ).

#include "codol_zone/zone_editor.hpp"

#include <vector>

namespace codol::mapc
{
	struct trace_result
	{
		float fraction = 1.0f;		// of the segment at which the first brush is entered; 1: nothing hit
		int brush = -1;				// cbrush_t index of that brush
		bool start_solid = false;	// the start lies inside a brush
		int brushes_tested = 0;
	};

	class clip_trace
	{
	public:
		// reads the clip map of editor.zone(). The leaf brush nodes are taken from the zone stream: the loader
		// overwrites the split distance of some nodes in the loaded image ( see README ).
		explicit clip_trace(const zone::ZoneEditor& editor);

		bool valid() const { return valid_; }
		int brush_count() const { return static_cast<int>(brushes_.size()); }

		// the world: down the BSP to the leafs the segment ( grown by `half`, a box, or a point when null ) touches,
		// then through each leaf's brush tree. `mask`: contents a brush needs to count.
		trace_result trace(const float start[3], const float end[3], int mask, const float half[3] = nullptr) const;
		// the same segment against every brush any world leaf lists, one by one, without the trees
		trace_result trace_all(const float start[3], const float end[3], int mask, const float half[3] = nullptr) const;
		// brush model `sub_model` ( in its own space ): through its tree, and against every brush it lists
		trace_result trace_model(int sub_model, const float start[3], const float end[3], int mask, bool through_tree = true) const;

		// brushes the world leafs list / a brush model lists ( sorted, unique )
		const std::vector<int>& world_brushes() const { return world_brushes_; }
		std::vector<int> model_brushes(int sub_model) const;
		// world leafs whose tree lists `brush`
		std::vector<int> leafs_of(int brush) const;

	private:
		struct plane
		{
			float normal[3];
			float dist;
		};
		struct brush
		{
			float mins[3], maxs[3];
			int contents = 0;
			std::vector<plane> sides;		// the stored ( non axial ) sides
		};
		struct segment
		{
			float start[3], end[3], half[3];
			int mask;
		};

		void test_brush(int index, const segment& s, trace_result& out) const;
		void walk_tree(int node, const segment& s, trace_result& out, std::vector<int>* collect, int depth = 0) const;
		void walk_bsp(int node, const segment& s, trace_result& out, int depth = 0) const;

		bool valid_ = false;
		std::vector<brush> brushes_;
		std::vector<c1::cLeafBrushNode_s> nodes_;			// stream values
		std::vector<std::vector<std::uint16_t>> lists_;		// per leaf node: its brushes
		std::vector<c1::cLeaf_t> leafs_;
		std::vector<c1::cmodel_t> models_;
		struct bsp_node
		{
			plane p;
			int children[2];
		};
		std::vector<bsp_node> bsp_;
		std::vector<int> world_brushes_;
	};
}
