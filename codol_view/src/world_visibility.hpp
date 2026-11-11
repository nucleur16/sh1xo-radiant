#pragma once

// Cell / portal visibility of a GfxWorld.
//
// Port of CODOL-AssetExplorer src\Codol.Rendering\GameWorldVisibility.cs. Per frame, update() finds the camera's
// cell, walks the portals from it while narrowing the frustum to each portal's outline, and marks the surfaces and
// static models of every AABB tree node it can see. Surfaces and static models that no cell lists are always
// visible. Cells the walk did not reach are still collected with the whole frustum ( GameWorldVisibility.cs:103-110 ),
// so the result is never less than plain frustum culling of the cell trees.
//
// The frustum is a list of planes ( xyz normal, w distance ) in EYE RELATIVE space, a point p is inside when
// dot( n, p - eye ) + w >= 0 for all of them; the LAST plane must be the near plane ( it is kept through every
// portal ). GameMapRenderer.cs:1363-1377 builds left, right, bottom, top, near.
//
// An object holds no global state; update() does not allocate once the object is built ( apart from the node
// stack growing on the first frames ). Nothing throws: if the constructor runs out of memory the object is empty
// ( cell_count() == 0, both arrays empty ).

#include "world_cells.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace codol::view
{
	class world_visibility
	{
	public:
		static constexpr int MAX_DEPTH = 24;			// GameWorldVisibility.cs:8
		static constexpr int VISIT_BUDGET = 256;		// GameWorldVisibility.cs:10

		// GameWorldVisibility.cs:25-64
		world_visibility(world_cell_data data, int surface_count, int static_model_count);

		// GameWorldVisibility.cs:78-111. `eye` in world space, `frustum` eye relative ( see above ); the span only
		// has to live for the call.
		void update(const float3& eye, std::span<const float4> frustum);

		// 0 / 1 per surface index ( GfxWorld.dpvs.surfaces ) and per static model index, as of the last update()
		const std::vector<std::uint8_t>& surface_visible() const { return surface_visible_; }
		const std::vector<std::uint8_t>& model_visible() const { return model_visible_; }

		int camera_cell() const { return camera_cell_; }		// -1 when the eye is in no cell
		int visible_cells() const { return visible_cells_; }	// cell visits: portal walk + frustum fallback
		int reached_cells() const { return reached_cells_; }	// distinct cells the portal walk reached
		int cell_count() const { return static_cast<int>(data_.cells.size()); }

		// GameWorldVisibility.cs:266-278: `center` eye relative
		static bool box_visible(std::span<const float4> frustum, const float3& center, const float3& half);

	private:
		// most planes a portal frustum can have: a clipped portal has at most MAX_PORTAL_POINTS points, + near
		static constexpr int MAX_PORTAL_POINTS = 64;	// GameWorldVisibility.cs:152
		static constexpr int MAX_PORTAL_PLANES = MAX_PORTAL_POINTS + 1;

		void visit(int index, const float3& eye, std::span<const float4> frustum, int depth);
		void collect_cell(const world_cell& cell, const float3& eye, std::span<const float4> frustum);
		// false: the portal is outside `frustum`. Otherwise `next` is the frustum through the portal: either
		// `frustum` itself or planes written into `storage` ( MAX_PORTAL_PLANES entries ).
		static bool portal_frustum(std::span<const float3> points, const float3& eye, std::span<const float4> frustum,
			float4* storage, std::span<const float4>& next);

		world_cell_data data_;
		std::vector<std::uint8_t> surface_visible_, model_visible_;
		std::vector<std::uint8_t> surface_reset_, model_reset_;		// 1 for what no cell lists
		std::vector<std::uint8_t> on_path_, reached_;				// per cell
		std::vector<int> node_stack_;
		std::vector<float4> portal_planes_;							// MAX_DEPTH levels of MAX_PORTAL_PLANES
		std::span<const float4> whole_frustum_;						// only valid inside update()
		int visits_ = 0;
		int camera_cell_ = -1;
		int visible_cells_ = 0;
		int reached_cells_ = 0;
	};
}
