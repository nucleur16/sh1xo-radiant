// Port of Codol.Assets\Rendering\WorldSightTrace.cs and of the clipMap reading it depends on
// ( Codol.Assets\Maps\MapExtractor.cs:441-797, offsets from Maps\MapLayouts.cs, 1.1.1.13 branch ).
//
// The clipMap is read straight from the zone image at the C# offsets. All arithmetic is single precision float,
// in the order of the C#. Where the C# would throw on malformed data ( an index outside an array ) the item is
// skipped - nothing throws.

#include "sight_trace.hpp"

#include "codol_zone/assets.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <span>

namespace codol::view
{
	using codol::zone::zaddr;
	using codol::zone::Zone;

	namespace
	{
		// MapLayouts.cs:198-247 ClipMapLayout ( not V3 )
		namespace clip
		{
			constexpr std::uint32_t PLANE_COUNT = 8, PLANES = 12;
			constexpr std::uint32_t NUM_MATERIALS = 24, MATERIALS = 28;
			constexpr std::uint32_t NUM_LEAFS = 56, LEAFS = 60;
			constexpr std::uint32_t LEAF_BRUSH_NODES_COUNT = 64, LEAF_BRUSH_NODES = 68;
			constexpr std::uint32_t VERT_COUNT = 88, VERTS = 92;
			constexpr std::uint32_t TRI_COUNT = 96, TRI_INDICES = 100;
			constexpr std::uint32_t PARTITION_COUNT = 116, PARTITIONS = 120;
			constexpr std::uint32_t AABB_TREE_COUNT = 124, AABB_TREES = 128;
			constexpr std::uint32_t NUM_SUB_MODELS = 132, CMODELS = 136;
			constexpr std::uint32_t NUM_BRUSHES = 140, BRUSHES = 144, BRUSH_BOUNDS = 148, BRUSH_CONTENTS = 152;
		}

		// MapLayouts.cs:249-342
		constexpr std::uint32_t CPLANE_SIZE = 20, CPLANE_NORMAL = 0, CPLANE_DIST = 12;
		constexpr std::uint32_t CBRUSHSIDE_SIZE = 8, CBRUSHSIDE_PLANE = 0;
		constexpr std::uint32_t CBRUSH_SIZE = 36, CBRUSH_NUM_SIDES = 0, CBRUSH_SIDES = 4;
		constexpr std::uint32_t CLIP_MATERIAL_SIZE = 12, CLIP_MATERIAL_CONTENTS = 8;
		constexpr std::uint32_t CMODEL_SIZE = 68, CMODEL_LEAF = 28;
		constexpr std::uint32_t CLEAF_SIZE = 40, CLEAF_FIRST_COLL_AABB_INDEX = 0, CLEAF_COLL_AABB_COUNT = 2, CLEAF_BRUSH_CONTENTS = 4,
			CLEAF_LEAF_BRUSH_NODE = 36;
		constexpr std::uint32_t CLEAF_BRUSH_NODE_SIZE = 20, CLEAF_BRUSH_NODE_LEAF_BRUSH_COUNT = 2, CLEAF_BRUSH_NODE_DATA = 8;
		constexpr std::uint32_t PARTITION_SIZE = 12, PARTITION_TRI_COUNT = 0, PARTITION_FIRST_TRI = 4;
		constexpr std::uint32_t AABB_TREE_SIZE = 32, AABB_TREE_MATERIAL_INDEX = 12, AABB_TREE_CHILD_COUNT = 14, AABB_TREE_U = 28;

		constexpr std::int64_t MAX_ELEMENTS = 1 << 24;		// MapExtractor.cs:915

		constexpr float CELL_SIZE = 256.0f;					// WorldSightTrace.cs:10
		constexpr std::int64_t MAX_CELLS_PER_ITEM = 4096;	// WorldSightTrace.cs:11

		template <typename T>
		T rd(const std::span<const std::uint8_t> data, const std::size_t offset)
		{
			T value{};
			if (offset + sizeof(T) <= data.size())
			{
				std::memcpy(&value, data.data() + offset, sizeof(T));
			}
			return value;
		}

		float3 rd_vector(const std::span<const std::uint8_t> data, const std::size_t offset)
		{
			return { rd<float>(data, offset), rd<float>(data, offset + 4), rd<float>(data, offset + 8) };
		}

		// MapExtractor.cs:927-939 ArrayCount + the array's bytes: empty unless 0 < count <= 2^24 and the whole
		// array is loaded zone data
		std::span<const std::uint8_t> array_bytes(const Zone& zone, const std::int64_t count, const zaddr pointer, const std::uint32_t element_size)
		{
			if (count <= 0 || count > MAX_ELEMENTS)
			{
				return {};
			}
			const std::size_t size = static_cast<std::size_t>(count) * element_size;
			const auto data = zone.bytes(pointer, size);
			return data.size() == size ? data : std::span<const std::uint8_t>();
		}

		// System.Numerics.Vector3.Dot
		float dot(const float3& a, const float3& b)
		{
			return a.x * b.x + a.y * b.y + a.z * b.z;
		}

		float3 vmin(const float3& a, const float3& b)
		{
			return { std::min(a.x, b.x), std::min(a.y, b.y), std::min(a.z, b.z) };
		}

		float3 vmax(const float3& a, const float3& b)
		{
			return { std::max(a.x, b.x), std::max(a.y, b.y), std::max(a.z, b.z) };
		}

		// WorldSightTrace.cs:334 Component
		float component(const float3& v, const int axis)
		{
			return axis == 0 ? v.x : axis == 1 ? v.y : v.z;
		}

		// WorldSightTrace.cs:330 Cell
		int cell_of(const float value)
		{
			if (std::isnan(value))
			{
				return 0;
			}
			return static_cast<int>(std::floor(std::clamp(value, -1e7f, 1e7f) / CELL_SIZE));
		}

		// WorldSightTrace.cs:332 Key
		std::int64_t key_of(const int x, const int y)
		{
			return static_cast<std::int64_t>((static_cast<std::uint64_t>(static_cast<std::uint32_t>(x)) << 32) | static_cast<std::uint32_t>(y));
		}

		// MapExtractor.cs:744-797 CollectLeafBrushes: marks every brush of the leaf brush tree below `root` in
		// `listed`. `visited[n] == generation` is the C#'s visited set.
		void collect_leaf_brushes(const Zone& zone, const std::span<const std::uint8_t> nodes, const int node_count, const int root,
			const int brush_count, std::vector<std::uint8_t>& listed, std::vector<int>& visited, const int generation, std::vector<int>& stack)
		{
			if (root < 0 || root >= node_count)
			{
				return;
			}
			stack.clear();
			stack.push_back(root);
			while (!stack.empty())
			{
				const int n = stack.back();
				stack.pop_back();
				if (n < 0 || n >= node_count || visited[static_cast<std::size_t>(n)] == generation)
				{
					continue;
				}
				visited[static_cast<std::size_t>(n)] = generation;
				const std::size_t node = static_cast<std::size_t>(n) * CLEAF_BRUSH_NODE_SIZE;
				const std::int16_t leaf_brush_count = rd<std::int16_t>(nodes, node + CLEAF_BRUSH_NODE_LEAF_BRUSH_COUNT);
				if (leaf_brush_count > 0)
				{
					const zaddr list = rd<zaddr>(nodes, node + CLEAF_BRUSH_NODE_DATA);
					const std::size_t size = 2u * static_cast<std::size_t>(leaf_brush_count);
					const auto brushes = zone.bytes(list, size);
					if (brushes.size() == size)
					{
						for (std::size_t k = 0; k < static_cast<std::size_t>(leaf_brush_count); k++)
						{
							const int brush = rd<std::uint16_t>(brushes, 2 * k);
							if (brush < brush_count)
							{
								listed[static_cast<std::size_t>(brush)] = 1;
							}
						}
					}
				}
				else
				{
					for (int k = 1; k <= -leaf_brush_count; k++)
					{
						stack.push_back(n + k);
					}
					const std::uint16_t left = rd<std::uint16_t>(nodes, node + CLEAF_BRUSH_NODE_DATA + 8);
					const std::uint16_t right = rd<std::uint16_t>(nodes, node + CLEAF_BRUSH_NODE_DATA + 10);
					if (left != 0)
					{
						stack.push_back(n + left);
					}
					if (right != 0)
					{
						stack.push_back(n + right);
					}
				}
			}
		}
	}

	// WorldSightTrace.cs:174-193 TraceWork
	struct world_sight_trace::trace_work
	{
		float3 start;
		float3 end;
		float3 delta;
		float3 midpoint;
		float3 half_delta_abs;
		float3 inverse_delta_abs;

		trace_work(const float3& s, const float3& e)
			: start(s), end(e)
		{
			delta = end - start;
			midpoint = (end + start) * 0.5f;
			const float3 half = delta * 0.5f;
			half_delta_abs = { std::fabs(half.x), std::fabs(half.y), std::fabs(half.z) };
			const float3 d{ std::fabs(delta.x), std::fabs(delta.y), std::fabs(delta.z) };
			inverse_delta_abs = { d.x >= 0.000001f ? 1.0f / d.x : 0.0f, d.y >= 0.000001f ? 1.0f / d.y : 0.0f, d.z >= 0.000001f ? 1.0f / d.z : 0.0f };
		}
	};

	world_sight_trace::world_sight_trace(const Zone& zone)
	{
		zaddr clip_map = codol::zone::first_asset(zone, codol::c1::ASSET_TYPE_CLIPMAP_MP);
		if (clip_map == 0)
		{
			clip_map = codol::zone::first_asset(zone, codol::c1::ASSET_TYPE_CLIPMAP_SP);
		}
		load(zone, clip_map);
	}

	world_sight_trace::world_sight_trace(const Zone& zone, const zaddr clip_map)
	{
		load(zone, clip_map);
	}

	// nothing throws: out of memory leaves an empty world ( nothing blocks )
	void world_sight_trace::load(const Zone& zone, const zaddr clip_map)
	{
		try
		{
			read_clip_map(zone, clip_map);
		}
		catch (...)
		{
			*this = world_sight_trace();
		}
	}

	// MapExtractor.cs:441-624 ReadClipMap + 626-742 ReadSubModels ( only what the trace uses ), then
	// WorldSightTrace.cs:28-71 the constructor
	void world_sight_trace::read_clip_map(const Zone& zone, const zaddr clip_map)
	{
		const auto header = zone.bytes(clip_map, 0x100);
		brush_first_side_.assign(1, 0u);
		if (clip_map == 0 || header.size() != 0x100)
		{
			return;		// MapExtractor.cs:445: no clipMap, no collision
		}
		const auto u32 = [&](const std::uint32_t offset) { return rd<std::uint32_t>(header, offset); };

		// ---- clip materials ( MapExtractor.cs:473-485 ): contents only
		const auto materials = array_bytes(zone, u32(clip::NUM_MATERIALS), u32(clip::MATERIALS), CLIP_MATERIAL_SIZE);
		const int material_count = static_cast<int>(materials.size() / CLIP_MATERIAL_SIZE);

		// ---- brushes ( MapExtractor.cs:487-539 )
		const auto brushes = array_bytes(zone, rd<std::uint16_t>(header, clip::NUM_BRUSHES), u32(clip::BRUSHES), CBRUSH_SIZE);
		const int brush_count = static_cast<int>(brushes.size() / CBRUSH_SIZE);
		const std::size_t brush_total = static_cast<std::size_t>(brush_count);
		// IsLoadedRange( pointer, 0 ) is true, but then there is no brush to read either
		const auto brush_bounds = zone.bytes(u32(clip::BRUSH_BOUNDS), 24 * brush_total);
		const auto brush_contents = zone.bytes(u32(clip::BRUSH_CONTENTS), 4 * brush_total);
		const bool bounds_loaded = brush_bounds.size() == 24 * brush_total;
		const bool contents_loaded = brush_contents.size() == 4 * brush_total;

		brush_mid_.assign(brush_total, float3{});
		brush_half_.assign(brush_total, float3{});
		brush_contents_.assign(brush_total, 0);
		brush_stamp_.assign(brush_total, 0);
		brush_first_side_.assign(brush_total + 1, 0u);
		for (std::size_t i = 0; i < brush_total; i++)
		{
			const std::size_t b = i * CBRUSH_SIZE;
			const std::size_t num_sides = rd<std::uint16_t>(brushes, b + CBRUSH_NUM_SIDES);
			const zaddr side_pointer = rd<zaddr>(brushes, b + CBRUSH_SIDES);
			brush_first_side_[i] = static_cast<std::uint32_t>(sides_.size());
			const auto sides = zone.bytes(side_pointer, num_sides * CBRUSHSIDE_SIZE);
			if (num_sides > 0 && sides.size() == num_sides * CBRUSHSIDE_SIZE)
			{
				for (std::size_t k = 0; k < num_sides; k++)
				{
					// MapExtractor.cs:459-471 PlaneIndex: a plane of clipMap.planes, or any loaded cplane_s. Both
					// read the plane the pointer names ( WorldSightTrace.cs:39 only needs normal and distance ).
					const zaddr plane = rd<zaddr>(sides, k * CBRUSHSIDE_SIZE + CBRUSHSIDE_PLANE);
					const auto p = zone.bytes(plane, CPLANE_SIZE);
					if (plane == 0 || p.size() != CPLANE_SIZE)
					{
						continue;
					}
					sides_.push_back({ rd_vector(p, CPLANE_NORMAL), rd<float>(p, CPLANE_DIST) });
				}
			}
		}
		brush_first_side_[brush_total] = static_cast<std::uint32_t>(sides_.size());

		// ---- collision vertices and triangles ( MapExtractor.cs:549-574 )
		const auto verts = array_bytes(zone, u32(clip::VERT_COUNT), u32(clip::VERTS), 12);
		vertices_.resize(verts.size() / 12);
		for (std::size_t i = 0; i < vertices_.size(); i++)
		{
			vertices_[i] = rd_vector(verts, 12 * i);
		}
		const auto tris = array_bytes(zone, u32(clip::TRI_COUNT), u32(clip::TRI_INDICES), 6);
		triangles_.resize(tris.size() / 2);
		if (!tris.empty())
		{
			std::memcpy(triangles_.data(), tris.data(), tris.size());
		}

		// ---- partitions and the material of each ( MapExtractor.cs:576-602 )
		const auto partitions = array_bytes(zone, u32(clip::PARTITION_COUNT), u32(clip::PARTITIONS), PARTITION_SIZE);
		const int partition_count = static_cast<int>(partitions.size() / PARTITION_SIZE);
		std::vector<int> partition_material(static_cast<std::size_t>(partition_count), -1);
		const auto aabbs = array_bytes(zone, u32(clip::AABB_TREE_COUNT), u32(clip::AABB_TREES), AABB_TREE_SIZE);
		const int aabb_count = static_cast<int>(aabbs.size() / AABB_TREE_SIZE);
		for (int i = 0; i < aabb_count; i++)
		{
			const std::size_t a = static_cast<std::size_t>(i) * AABB_TREE_SIZE;
			if (rd<std::uint16_t>(aabbs, a + AABB_TREE_CHILD_COUNT) != 0)
			{
				continue;
			}
			const int partition = rd<std::int32_t>(aabbs, a + AABB_TREE_U);
			if (partition >= 0 && partition < partition_count)
			{
				partition_material[static_cast<std::size_t>(partition)] = rd<std::uint16_t>(aabbs, a + AABB_TREE_MATERIAL_INDEX);
			}
		}

		// ---- sub models ( MapExtractor.cs:626-742 ReadSubModels )
		// The C# gives every brush / partition the lowest sub model > 0 that lists it, else 0 when the world
		// ( sub model 0's leaf or any collision leaf ) lists it; a brush nobody lists becomes a worldspawn brush
		// that is not in the collision tree. The trace only asks "world and in the collision tree" for brushes
		// ( WorldSightTrace.cs:40 ) and "not of a sub model > 0" for partitions ( :57 ), so two flags per item
		// carry the same information.
		std::vector<std::uint8_t> brush_in_world(brush_total, 0), brush_in_sub_model(brush_total, 0);
		std::vector<std::uint8_t> partition_in_sub_model(static_cast<std::size_t>(partition_count), 0);
		{
			const auto cmodels = array_bytes(zone, u32(clip::NUM_SUB_MODELS), u32(clip::CMODELS), CMODEL_SIZE);
			const int sub_model_count = static_cast<int>(cmodels.size() / CMODEL_SIZE);
			const auto nodes = array_bytes(zone, u32(clip::LEAF_BRUSH_NODES_COUNT), u32(clip::LEAF_BRUSH_NODES), CLEAF_BRUSH_NODE_SIZE);
			const int node_count = static_cast<int>(nodes.size() / CLEAF_BRUSH_NODE_SIZE);
			std::vector<int> visited(static_cast<std::size_t>(node_count), 0);
			std::vector<int> stack;
			int generation = 0;
			for (int i = 0; i < sub_model_count; i++)
			{
				const std::size_t leaf = static_cast<std::size_t>(i) * CMODEL_SIZE + CMODEL_LEAF;
				if (rd<std::int32_t>(cmodels, leaf + CLEAF_BRUSH_CONTENTS) != 0)
				{
					collect_leaf_brushes(zone, nodes, node_count, rd<std::int32_t>(cmodels, leaf + CLEAF_LEAF_BRUSH_NODE), brush_count,
						i == 0 ? brush_in_world : brush_in_sub_model, visited, ++generation, stack);
				}

				const int first_aabb = rd<std::uint16_t>(cmodels, leaf + CLEAF_FIRST_COLL_AABB_INDEX);
				const int leaf_aabb_count = rd<std::uint16_t>(cmodels, leaf + CLEAF_COLL_AABB_COUNT);
				if (i > 0 && leaf_aabb_count > 0 && aabb_count > 0)
				{
					stack.clear();
					for (int k = 0; k < leaf_aabb_count; k++)
					{
						stack.push_back(first_aabb + k);
					}
					std::int64_t guard = 0;
					while (!stack.empty() && guard++ < 4ll * aabb_count + 16)
					{
						const int a = stack.back();
						stack.pop_back();
						if (a < 0 || a >= aabb_count)
						{
							continue;
						}
						const std::size_t tree = static_cast<std::size_t>(a) * AABB_TREE_SIZE;
						const int children = rd<std::uint16_t>(aabbs, tree + AABB_TREE_CHILD_COUNT);
						const int u = rd<std::int32_t>(aabbs, tree + AABB_TREE_U);
						if (children == 0)
						{
							if (u >= 0 && u < partition_count)
							{
								partition_in_sub_model[static_cast<std::size_t>(u)] = 1;
							}
						}
						else
						{
							for (int k = 0; k < children; k++)
							{
								stack.push_back(static_cast<int>(static_cast<std::uint32_t>(u) + static_cast<std::uint32_t>(k)));
							}
						}
					}
				}
			}

			// MapExtractor.cs:692-707: the world's brushes are those of every collision leaf
			const std::uint32_t num_leafs = u32(clip::NUM_LEAFS);
			const std::uint64_t leafs_size = static_cast<std::uint64_t>(num_leafs) * CLEAF_SIZE;
			const auto leafs = leafs_size <= 0xFFFFFFFFull ? zone.bytes(u32(clip::LEAFS), static_cast<std::size_t>(leafs_size)) : std::span<const std::uint8_t>();
			const std::size_t leaf_count = leafs.size() == leafs_size ? num_leafs : 0;
			if (sub_model_count > 0 && leaf_count > 0)
			{
				++generation;
				for (std::size_t i = 0; i < leaf_count; i++)
				{
					const std::size_t leaf = i * CLEAF_SIZE;
					if (rd<std::int32_t>(leafs, leaf + CLEAF_BRUSH_CONTENTS) != 0)
					{
						collect_leaf_brushes(zone, nodes, node_count, rd<std::int32_t>(leafs, leaf + CLEAF_LEAF_BRUSH_NODE), brush_count, brush_in_world,
							visited, generation, stack);
					}
				}
			}
		}

		// ---- WorldSightTrace.cs:36-48: world brushes into the cell hash
		for (std::size_t b = 0; b < brush_total; b++)
		{
			// SubModel != 0 || !InCollisionTree || Bounds.IsEmpty
			if (brush_in_sub_model[b] || !brush_in_world[b] || !bounds_loaded)
			{
				continue;
			}
			// MapExtractor.cs:947 ReadBounds = MapBounds.FromMidHalf( mid, half )
			const float3 mid = rd_vector(brush_bounds, 24 * b), half = rd_vector(brush_bounds, 24 * b + 12);
			const float3 min = mid - half, max = mid + half;
			if (min.x > max.x || min.y > max.y || min.z > max.z)
			{
				continue;
			}
			brush_mid_[b] = (min + max) * 0.5f;
			brush_half_[b] = (max - min) * 0.5f;
			brush_contents_[b] = contents_loaded ? rd<std::int32_t>(brush_contents, 4 * b) : 0;
			insert(brush_cells_, large_brushes_, static_cast<int>(b), min, max);
		}

		// ---- WorldSightTrace.cs:50-70: the triangles of the world's partitions
		const std::int64_t triangle_count = static_cast<std::int64_t>(triangles_.size() / 3);
		triangle_contents_.assign(static_cast<std::size_t>(triangle_count), 0);
		triangle_stamp_.assign(static_cast<std::size_t>(triangle_count), 0);
		for (int i = 0; i < partition_count; i++)
		{
			const std::size_t p = static_cast<std::size_t>(i) * PARTITION_SIZE;
			const int material = partition_material[static_cast<std::size_t>(i)];
			if (partition_in_sub_model[static_cast<std::size_t>(i)] || material < 0 || material >= material_count)
			{
				continue;
			}
			const int contents = rd<std::int32_t>(materials, static_cast<std::size_t>(material) * CLIP_MATERIAL_SIZE + CLIP_MATERIAL_CONTENTS);
			const std::int64_t first = rd<std::int32_t>(partitions, p + PARTITION_FIRST_TRI);
			const std::int64_t count = rd<std::uint8_t>(partitions, p + PARTITION_TRI_COUNT);
			for (std::int64_t t = std::max<std::int64_t>(first, 0); t < first + count && t < triangle_count; t++)
			{
				const std::size_t at = static_cast<std::size_t>(3 * t);
				const std::size_t i0 = triangles_[at], i1 = triangles_[at + 1], i2 = triangles_[at + 2];
				if (i0 >= vertices_.size() || i1 >= vertices_.size() || i2 >= vertices_.size())
				{
					continue;		// the C# would throw
				}
				const float3& a = vertices_[i0];
				const float3& b = vertices_[i1];
				const float3& c = vertices_[i2];
				triangle_contents_[static_cast<std::size_t>(t)] = contents;
				insert(triangle_cells_, large_triangles_, static_cast<int>(t), vmin(a, vmin(b, c)), vmax(a, vmax(b, c)));
			}
		}
	}

	// WorldSightTrace.cs:73 BrushCount
	int world_sight_trace::brush_count() const
	{
		return static_cast<int>(std::count_if(brush_contents_.begin(), brush_contents_.end(), [](const int c) { return c != 0; }));
	}

	// WorldSightTrace.cs:75 TriangleCount
	int world_sight_trace::triangle_count() const
	{
		return static_cast<int>(std::count_if(triangle_contents_.begin(), triangle_contents_.end(), [](const int c) { return c != 0; }));
	}

	// WorldSightTrace.cs:81-90 Blocked
	bool world_sight_trace::blocked(const float3 start, const float3 end, const int mask)
	{
		traces_++;
		if (trace(start, end, mask))
		{
			blocked_traces_++;
			return true;
		}
		return false;
	}

	// WorldSightTrace.cs:92-165 Trace
	bool world_sight_trace::trace(const float3 start, const float3 end, const int mask)
	{
		const trace_work work(start, end);
		const float3 min = vmin(start, end) - float3{ 0.25f, 0.25f, 0.25f };
		const float3 max = vmax(start, end) + float3{ 0.25f, 0.25f, 0.25f };
		if (++stamp_ == INT32_MAX)
		{
			std::fill(brush_stamp_.begin(), brush_stamp_.end(), 0);
			std::fill(triangle_stamp_.begin(), triangle_stamp_.end(), 0);
			stamp_ = 1;
		}
		for (const int b : large_brushes_)
		{
			if ((brush_contents_[static_cast<std::size_t>(b)] & mask) != 0 && brush_hit(work, b))
			{
				return true;
			}
		}
		float fraction = 1.0f;
		for (const int t : large_triangles_)
		{
			if ((triangle_contents_[static_cast<std::size_t>(t)] & mask) != 0)
			{
				triangle_hit(work, t, fraction);
			}
		}
		if (fraction < 1.0f)
		{
			return true;
		}
		const int x0 = cell_of(min.x), x1 = cell_of(max.x), y0 = cell_of(min.y), y1 = cell_of(max.y);
		for (int y = y0; y <= y1; y++)
		{
			for (int x = x0; x <= x1; x++)
			{
				const std::int64_t key = key_of(x, y);
				if (const auto found = brush_cells_.find(key); found != brush_cells_.end())
				{
					for (const int b : found->second)
					{
						const std::size_t at = static_cast<std::size_t>(b);
						if (brush_stamp_[at] == stamp_)
						{
							continue;
						}
						brush_stamp_[at] = stamp_;
						if ((brush_contents_[at] & mask) != 0 && overlaps(b, min, max) && brush_hit(work, b))
						{
							return true;
						}
					}
				}
				if (const auto found = triangle_cells_.find(key); found != triangle_cells_.end())
				{
					for (const int t : found->second)
					{
						const std::size_t at = static_cast<std::size_t>(t);
						if (triangle_stamp_[at] == stamp_)
						{
							continue;
						}
						triangle_stamp_[at] = stamp_;
						if ((triangle_contents_[at] & mask) != 0)
						{
							triangle_hit(work, t, fraction);
							if (fraction < 1.0f)
							{
								return true;
							}
						}
					}
				}
			}
		}
		return false;
	}

	// WorldSightTrace.cs:167-172 Overlaps
	bool world_sight_trace::overlaps(const int b, const float3& min, const float3& max) const
	{
		const float3 lo = brush_mid_[static_cast<std::size_t>(b)] - brush_half_[static_cast<std::size_t>(b)];
		const float3 hi = brush_mid_[static_cast<std::size_t>(b)] + brush_half_[static_cast<std::size_t>(b)];
		return lo.x <= max.x && hi.x >= min.x && lo.y <= max.y && hi.y >= min.y && lo.z <= max.z && hi.z >= min.z;
	}

	// WorldSightTrace.cs:195-273 BrushHit: the segment against the brush's box ( the six axial sides, entering
	// with an eighth of a unit of slack ), then against its stored side planes
	bool world_sight_trace::brush_hit(const trace_work& work, const int b) const
	{
		float enter = 0.0f, leave = 1.0f;
		const float3& mid = brush_mid_[static_cast<std::size_t>(b)];
		const float3& half = brush_half_[static_cast<std::size_t>(b)];
		for (int k = 0; k < 3; k++)
		{
			const float delta = component(work.delta, k);
			const float sign = 0.0f > delta ? -1.0f : 1.0f;
			const float offset = (component(work.midpoint, k) - component(mid, k)) * sign;
			const float half_delta = component(work.half_delta_abs, k);
			const float inverse = component(work.inverse_delta_abs, k);
			const float outside = half_delta - component(half, k);
			const float past_far = outside + offset;
			if (past_far > 0.0f)
			{
				const float leave_here = (half_delta * 2.0f - past_far) * inverse;
				if (leave_here <= enter)
				{
					return false;
				}
				if (leave_here < leave)
				{
					leave = leave_here;
				}
			}
			const float before_near = outside - offset;
			if (before_near > 0.0f)
			{
				const float nudged = before_near - 0.125f;
				if (half_delta * 2.0f * leave <= nudged)
				{
					return false;
				}
				const float enter_here = nudged * inverse;
				if (enter_here > enter)
				{
					enter = enter_here;
				}
			}
		}
		const std::size_t first = brush_first_side_[static_cast<std::size_t>(b)], last = brush_first_side_[static_cast<std::size_t>(b) + 1];
		for (std::size_t s = first; s < last; s++)
		{
			const float3& normal = sides_[s].normal;
			const float distance = sides_[s].distance;
			const float d1 = dot(work.start, normal) - distance;
			const float d2 = dot(work.end, normal) - distance;
			if (d1 <= 0.0f)
			{
				if (d2 > 0.0f)
				{
					const float span = d1 - d2;
					if (span * leave < d1)
					{
						leave = d1 / span;
						if (leave <= enter)
						{
							return false;
						}
					}
				}
			}
			else
			{
				if (d2 > 0.0f)
				{
					return false;
				}
				const float span = d1 - d2;
				if (span * enter < d1)
				{
					enter = d1 / span;
					if (leave <= enter)
					{
						return false;
					}
				}
			}
		}
		return true;
	}

	// WorldSightTrace.cs:275-306 TriangleHit: front faces only; lowers `fraction` when the segment crosses the
	// triangle before the current fraction
	void world_sight_trace::triangle_hit(const trace_work& work, const int t, float& fraction) const
	{
		const std::size_t at = 3 * static_cast<std::size_t>(t);
		const float3& v0 = vertices_[triangles_[at]];
		const float3& v1 = vertices_[triangles_[at + 1]];
		const float3& v2 = vertices_[triangles_[at + 2]];
		const float3 e1 = v0 - v1;
		const float3 e2 = v0 - v2;
		const float3 normal{ e2.y * e1.z - e1.y * e2.z, e2.z * e1.x - e2.x * e1.z, e2.x * e1.y - e2.y * e1.x };
		const float3& delta = work.delta;
		const float facing = dot(normal, delta);
		if (facing >= 0.0f)
		{
			return;
		}
		const float3 w = v0 - work.start;
		const float distance = dot(normal, w);
		if (distance > 0.0f || fraction * facing >= distance)
		{
			return;
		}
		const float3 c{ w.z * delta.y - delta.z * w.y, delta.z * w.x - w.z * delta.x, w.y * delta.x - w.x * delta.y };
		const float u = dot(e1, c);
		if (u > 0.0f || facing > u)
		{
			return;
		}
		const float v = dot(e2, c);
		if (v >= 0.0f && u - v >= facing)
		{
			fraction = distance / facing;
		}
	}

	// WorldSightTrace.cs:308-328 Insert
	void world_sight_trace::insert(cell_map& cells, std::vector<int>& large, const int item, const float3& min, const float3& max)
	{
		const int x0 = cell_of(min.x), x1 = cell_of(max.x), y0 = cell_of(min.y), y1 = cell_of(max.y);
		if (static_cast<std::int64_t>(x1 - x0 + 1) * (y1 - y0 + 1) > MAX_CELLS_PER_ITEM)
		{
			large.push_back(item);
			return;
		}
		for (int y = y0; y <= y1; y++)
		{
			for (int x = x0; x <= x1; x++)
			{
				cells[key_of(x, y)].push_back(item);
			}
		}
	}
}
