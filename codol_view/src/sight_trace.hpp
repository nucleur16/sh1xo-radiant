#pragma once

// Line of sight through the collision world of a map: a faithful port of
// Codol.Assets\Rendering\WorldSightTrace.cs, with the parts of Maps\MapExtractor.cs ( ReadClipMap, ReadSubModels,
// CollectLeafBrushes ) that feed it. The light grid asks it whether a cell corner is visible from a model's
// lighting origin ( light_grid::sample's `blocked` ).
//
// It tests the world's own brushes ( those listed by the collision leaves and by no brush sub model ) and the
// collision triangles of the terrain partitions, both hashed into 256 unit xy cells. blocked() keeps per trace
// stamps, so one instance must not be used from two threads at once.

#include "map_types.hpp"

#include "codol_zone/zone.hpp"

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace codol::view
{
	class world_sight_trace
	{
	public:
		static constexpr int LIGHT_GRID_MASK = 0x2001;		// WorldSightTrace.cs:8

		// reads the zone's clipMap ( first clipMap_t asset, mp before sp ); empty ( nothing blocks ) without one
		explicit world_sight_trace(const codol::zone::Zone& zone);
		// same for a given clipMap_t header
		world_sight_trace(const codol::zone::Zone& zone, codol::zone::zaddr clip_map);

		// WorldSightTrace.cs:81-90 Blocked: true when something with ( contents & mask ) lies between start and end
		bool blocked(float3 start, float3 end, int mask = LIGHT_GRID_MASK);

		int traces() const { return traces_; }					// WorldSightTrace.cs:77
		int blocked_traces() const { return blocked_traces_; }	// WorldSightTrace.cs:79
		int brush_count() const;								// WorldSightTrace.cs:73: brushes that can block
		int triangle_count() const;								// WorldSightTrace.cs:75: triangles that can block

	private:
		struct side_plane
		{
			float3 normal;
			float distance = 0;
		};

		struct trace_work;
		using cell_map = std::unordered_map<std::int64_t, std::vector<int>>;

		world_sight_trace() = default;
		void load(const codol::zone::Zone& zone, codol::zone::zaddr clip_map);
		void read_clip_map(const codol::zone::Zone& zone, codol::zone::zaddr clip_map);
		bool trace(float3 start, float3 end, int mask);
		bool overlaps(int b, const float3& min, const float3& max) const;
		bool brush_hit(const trace_work& work, int b) const;
		void triangle_hit(const trace_work& work, int t, float& fraction) const;
		static void insert(cell_map& cells, std::vector<int>& large, int item, const float3& min, const float3& max);

		std::vector<float3> brush_mid_;
		std::vector<float3> brush_half_;
		std::vector<int> brush_contents_;
		std::vector<std::uint32_t> brush_first_side_{ 0u };	// brush b: sides_[ brush_first_side_[b] .. brush_first_side_[b + 1] )
		std::vector<side_plane> sides_;
		std::vector<float3> vertices_;
		std::vector<std::uint16_t> triangles_;
		std::vector<int> triangle_contents_;
		cell_map brush_cells_;
		cell_map triangle_cells_;
		std::vector<int> large_brushes_;
		std::vector<int> large_triangles_;
		std::vector<int> brush_stamp_;
		std::vector<int> triangle_stamp_;
		int stamp_ = 0;
		int traces_ = 0;
		int blocked_traces_ = 0;
	};
}
