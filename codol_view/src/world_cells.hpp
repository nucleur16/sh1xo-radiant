#pragma once

// The visibility cells of a GfxWorld: cells, their portals and their AABB trees, the BSP that finds the cell of a
// point, and dpvs.sortedSurfIndex ( the surface order the AABB tree leaves index ).
//
// Port of CODOL-AssetExplorer src\Codol.Assets\Rendering\WorldCells.cs, 1.1.1.13 layouts only
// ( Codol.Assets\Maps\MapLayouts.cs ). Nothing here throws; no global state.

#include "codol_render/math.hpp"
#include "codol_zone/zone.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace codol::view
{
	using render::float3;
	using render::float4;

	// WorldCells.cs:22-27
	struct world_portal
	{
		int target_cell = 0;
		std::vector<float3> points;			// the portal polygon, world space, >= 3 points
	};

	// WorldCells.cs:29-44: one GfxAabbTree node of a cell
	struct world_cell_node
	{
		float3 center;
		float3 half;
		int first_surface = 0;				// into world_cell_data::sorted_surfaces
		int surface_count = 0;
		int first_child = 0;				// index into the cell's nodes
		int child_count = 0;
		std::vector<int> static_models;		// static model indices ( dpvs.smodelDrawInsts )
	};

	// WorldCells.cs:78-92
	struct world_cell
	{
		int index = 0;
		float3 center;
		float3 half;
		std::vector<world_portal> portals;
		std::vector<world_cell_node> nodes;	// nodes[ 0 ] is the root

		bool contains(const float3& point) const;		// WorldCells.cs:90-91
	};

	// WorldCells.cs:46-76
	struct world_cell_data
	{
		std::vector<world_cell> cells;
		std::vector<std::uint16_t> sorted_surfaces;		// dpvs.sortedSurfIndex: surface index per sorted slot
		std::vector<float4> planes;						// GfxWorld dpvsPlanes: normal xyz, dist w
		std::vector<std::uint16_t> nodes;				// GfxWorld BSP nodes

		// the cell the BSP puts `point` in; -1 when there is no tree or the walk leaves it ( WorldCells.cs:48-75 )
		int cell_of_point(const float3& point) const;
	};

	// WorldCells.cs:96-217. nullopt ( and a line in `warnings` ) when the zone has no usable cells: the caller then
	// culls by the frustum only. `surface_count` is GfxWorld.surfaceCount, `static_model_count` the number of static
	// model instances ( dpvs.smodelCount ) the caller indexes its models with.
	std::optional<world_cell_data> read_world_cells(const codol::zone::Zone& zone, codol::zone::zaddr gfx_world,
		int surface_count, int static_model_count, std::vector<std::string>& warnings);
}
