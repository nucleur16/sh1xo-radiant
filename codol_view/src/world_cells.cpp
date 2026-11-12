#include "world_cells.hpp"

#include <cmath>
#include <cstddef>
#include <cstring>
#include <new>
#include <span>

namespace codol::view
{
	namespace
	{
		using codol::zone::zaddr;
		using codol::zone::Zone;
		namespace c1 = codol::c1;

		// ---- layout offsets, 1.1.1.13 ( Codol.Assets\Maps\MapLayouts.cs ) ---------------------------------
		// GfxWorldLayout ( MapLayouts.cs:23-65 )
		constexpr std::uint32_t WORLD_PLANE_COUNT = 8;
		constexpr std::uint32_t WORLD_NODE_COUNT = 12;
		constexpr std::uint32_t WORLD_CELL_COUNT = 52;			// CellCount( build )
		constexpr std::uint32_t WORLD_PLANES = WORLD_CELL_COUNT + 4;	// WorldCells.cs:116
		constexpr std::uint32_t WORLD_NODES = WORLD_CELL_COUNT + 8;		// WorldCells.cs:117
		constexpr std::uint32_t WORLD_CELLS = 76;				// Cells( build )
		constexpr std::uint32_t WORLD_TREE_COUNTS = WORLD_CELLS - 8;	// WorldCells.cs:135
		constexpr std::uint32_t WORLD_TREES = WORLD_CELLS - 4;			// WorldCells.cs:136
		constexpr std::uint32_t WORLD_DPVS = 456;				// Dpvs( build )
		// GfxWorldDpvsStaticLayout ( MapLayouts.cs:121-143 )
		constexpr std::uint32_t DPVS_STATIC_SURFACE_COUNT = 4;
		constexpr std::uint32_t DPVS_STATIC_SURFACE_COUNT_NO_DECAL = 8;
		constexpr std::uint32_t DPVS_SORTED_SURF_INDEX = 76;
		// GfxCellLayout ( MapLayouts.cs:179-188 )
		constexpr std::uint32_t CELL_SIZE = 40;
		constexpr std::uint32_t CELL_BOUNDS = 0;
		constexpr std::uint32_t CELL_PORTAL_COUNT = 24;
		constexpr std::uint32_t CELL_PORTALS = 28;
		// GfxPortalLayout ( MapLayouts.cs:190-196 )
		constexpr std::uint32_t PORTAL_SIZE = 60;
		constexpr std::uint32_t PORTAL_VERTICES = 28;
		constexpr std::uint32_t PORTAL_TARGET_CELL = 32;
		constexpr std::uint32_t PORTAL_VERTEX_COUNT = 34;
		// GfxAabbTreeLayout ( WorldCells.cs:8-20 )
		constexpr std::int32_t AABB_SIZE = 44;
		constexpr std::uint32_t AABB_CENTER = 0;
		constexpr std::uint32_t AABB_HALF = 12;
		constexpr std::uint32_t AABB_CHILD_COUNT = 24;
		constexpr std::uint32_t AABB_SURFACE_COUNT = 26;
		constexpr std::uint32_t AABB_START_SURF_INDEX = 28;
		constexpr std::uint32_t AABB_SMODEL_INDEX_COUNT = 34;
		constexpr std::uint32_t AABB_SMODEL_INDEXES = 36;
		constexpr std::uint32_t AABB_CHILDREN_OFFSET = 40;
		constexpr std::uint32_t PLANE_STRIDE = 20;				// WorldCells.cs:120-126: cplane_s

		// the same offsets as codol_zone knows them
		static_assert(offsetof(c1::GfxWorld, planeCount) == WORLD_PLANE_COUNT);
		static_assert(offsetof(c1::GfxWorld, nodeCount) == WORLD_NODE_COUNT);
		static_assert(offsetof(c1::GfxWorld, cellCount) == WORLD_CELL_COUNT);
		static_assert(offsetof(c1::GfxWorld, cells) == WORLD_CELLS);
		static_assert(offsetof(c1::GfxWorld, dpvs) == WORLD_DPVS);
		static_assert(offsetof(c1::GfxWorldDpvsStatic, staticSurfaceCount) == DPVS_STATIC_SURFACE_COUNT);
		static_assert(offsetof(c1::GfxWorldDpvsStatic, staticSurfaceCountNoDecal) == DPVS_STATIC_SURFACE_COUNT_NO_DECAL);
		static_assert(offsetof(c1::GfxWorldDpvsStatic, sortedSurfIndex) == DPVS_SORTED_SURF_INDEX);
		static_assert(sizeof(c1::GfxCell) == CELL_SIZE && offsetof(c1::GfxCell, portalCount) == CELL_PORTAL_COUNT && offsetof(c1::GfxCell, portals) == CELL_PORTALS);
		static_assert(sizeof(c1::GfxPortal) == PORTAL_SIZE && offsetof(c1::GfxPortal, vertices) == PORTAL_VERTICES
			&& offsetof(c1::GfxPortal, cellIndex) == PORTAL_TARGET_CELL && offsetof(c1::GfxPortal, vertexCount) == PORTAL_VERTEX_COUNT);

		// ---- little endian reads out of a byte range ------------------------------------------------------
		template <typename T>
		T rd(const std::uint8_t* p)
		{
			T v;
			std::memcpy(&v, p, sizeof(T));
			return v;
		}

		float3 rd_float3(const std::uint8_t* p)
		{
			return { rd<float>(p), rd<float>(p + 4), rd<float>(p + 8) };
		}

		// VirtualMemory.U32: an unmapped address reads as 0
		std::uint32_t u32(const Zone& zone, const zaddr address)
		{
			return zone.get<std::uint32_t>(address);
		}

		// GameMemory.IsLoadedRange ( GameMemory.cs:38-57 ) + the bytes: true with an empty span for count 0, false
		// when count * stride bytes at `address` are not all inside the image.
		bool range(const Zone& zone, const zaddr address, const std::int64_t count, const std::int64_t stride, std::span<const std::uint8_t>& out)
		{
			out = {};
			const std::int64_t length = count * stride;
			if (length <= 0)
			{
				return length == 0;
			}
			if (static_cast<std::uint64_t>(address) + static_cast<std::uint64_t>(length) - 1 > 0xFFFFFFFFull)
			{
				return false;
			}
			out = zone.bytes(address, static_cast<std::size_t>(length));
			return out.size() == static_cast<std::size_t>(length);
		}

		std::optional<world_cell_data> read(const Zone& zone, const zaddr world, const int surface_count, const int static_model_count, std::vector<std::string>& warnings)
		{
			std::span<const std::uint8_t> bytes;

			// WorldCells.cs:100-113: dpvs.sortedSurfIndex
			const zaddr dpvs = world + WORLD_DPVS;
			const int sorted_count = static_cast<int>(u32(zone, dpvs + DPVS_STATIC_SURFACE_COUNT) + u32(zone, dpvs + DPVS_STATIC_SURFACE_COUNT_NO_DECAL));
			const zaddr sorted_list = u32(zone, dpvs + DPVS_SORTED_SURF_INDEX);
			if (sorted_count <= 0 || !range(zone, sorted_list, sorted_count, 2, bytes))
			{
				warnings.emplace_back("dpvs.sortedSurfIndex is not in the zone; the view is culled by the frustum only");
				return std::nullopt;
			}
			world_cell_data data;
			data.sorted_surfaces.resize(static_cast<std::size_t>(sorted_count));
			for (int i = 0; i < sorted_count; i++)
			{
				const std::uint16_t index = rd<std::uint16_t>(bytes.data() + 2 * static_cast<std::size_t>(i));
				data.sorted_surfaces[static_cast<std::size_t>(i)] = index < surface_count ? index : std::uint16_t{ 0 };
			}

			// WorldCells.cs:114-133: the BSP ( planes are 20 byte cplane_s: normal, dist, type / signbits )
			const int plane_count = static_cast<int>(u32(zone, world + WORLD_PLANE_COUNT));
			const int tree_node_count = static_cast<int>(u32(zone, world + WORLD_NODE_COUNT));
			const zaddr planes_ptr = u32(zone, world + WORLD_PLANES);
			const zaddr nodes_ptr = u32(zone, world + WORLD_NODES);
			std::span<const std::uint8_t> plane_bytes, node_bytes;
			if (plane_count > 0 && tree_node_count > 0 && range(zone, planes_ptr, plane_count, PLANE_STRIDE, plane_bytes) && range(zone, nodes_ptr, tree_node_count, 2, node_bytes))
			{
				data.planes.resize(static_cast<std::size_t>(plane_count));
				for (std::size_t i = 0; i < data.planes.size(); i++)
				{
					const std::uint8_t* plane = plane_bytes.data() + PLANE_STRIDE * i;
					data.planes[i] = float4(rd<float>(plane), rd<float>(plane + 4), rd<float>(plane + 8), rd<float>(plane + 12));
				}
				data.nodes.resize(static_cast<std::size_t>(tree_node_count));
				std::memcpy(data.nodes.data(), node_bytes.data(), node_bytes.size());
			}

			// WorldCells.cs:134-144
			const zaddr cells = u32(zone, world + WORLD_CELLS);
			const zaddr tree_counts = u32(zone, world + WORLD_TREE_COUNTS);
			const zaddr trees = u32(zone, world + WORLD_TREES);
			const int cell_count = static_cast<int>(u32(zone, world + WORLD_CELL_COUNT));
			std::span<const std::uint8_t> cell_bytes, tree_count_bytes, tree_bytes;
			if (cell_count <= 0 || cell_count > 4096 || !range(zone, cells, cell_count, CELL_SIZE, cell_bytes)
				|| !range(zone, tree_counts, cell_count, 4, tree_count_bytes) || !range(zone, trees, cell_count, 4, tree_bytes))
			{
				warnings.emplace_back("the visibility cells are not in the zone; the view is culled by the frustum only");
				return std::nullopt;
			}

			// WorldCells.cs:145-215
			data.cells.resize(static_cast<std::size_t>(cell_count));
			for (int i = 0; i < cell_count; i++)
			{
				const std::uint8_t* cell = cell_bytes.data() + static_cast<std::size_t>(i) * CELL_SIZE;
				world_cell& out = data.cells[static_cast<std::size_t>(i)];

				// WorldCells.cs:149-171: portals
				const int portal_count = static_cast<int>(rd<std::uint32_t>(cell + CELL_PORTAL_COUNT));
				const zaddr portal_array = rd<std::uint32_t>(cell + CELL_PORTALS);
				std::span<const std::uint8_t> portal_bytes;
				if (portal_count > 0 && range(zone, portal_array, portal_count, PORTAL_SIZE, portal_bytes))
				{
					out.portals.reserve(static_cast<std::size_t>(portal_count));
					for (int k = 0; k < portal_count; k++)
					{
						const std::uint8_t* portal = portal_bytes.data() + static_cast<std::size_t>(k) * PORTAL_SIZE;
						const int target = rd<std::uint16_t>(portal + PORTAL_TARGET_CELL);
						const int points = rd<std::uint8_t>(portal + PORTAL_VERTEX_COUNT);
						const zaddr vertices = rd<std::uint32_t>(portal + PORTAL_VERTICES);
						std::span<const std::uint8_t> vertex_bytes;
						if (target >= cell_count || points < 3 || !range(zone, vertices, points, 12, vertex_bytes))
						{
							continue;
						}
						world_portal& p = out.portals.emplace_back();
						p.target_cell = target;
						p.points.resize(static_cast<std::size_t>(points));
						for (std::size_t v = 0; v < p.points.size(); v++)
						{
							p.points[v] = rd_float3(vertex_bytes.data() + 12 * v);
						}
					}
				}

				// WorldCells.cs:172-206: the cell's AABB tree
				const int node_count = static_cast<int>(rd<std::uint32_t>(tree_count_bytes.data() + 4 * static_cast<std::size_t>(i)));
				const zaddr tree = rd<std::uint32_t>(tree_bytes.data() + 4 * static_cast<std::size_t>(i));
				std::span<const std::uint8_t> aabb_bytes;
				if (node_count > 0 && range(zone, tree, node_count, AABB_SIZE, aabb_bytes))
				{
					out.nodes.resize(static_cast<std::size_t>(node_count));
					for (int k = 0; k < node_count; k++)
					{
						const std::uint8_t* node = aabb_bytes.data() + static_cast<std::size_t>(k) * static_cast<std::size_t>(AABB_SIZE);
						const int first = rd<std::uint16_t>(node + AABB_START_SURF_INDEX);
						const int count = rd<std::uint16_t>(node + AABB_SURFACE_COUNT);
						const int children = rd<std::uint16_t>(node + AABB_CHILD_COUNT);
						const std::int32_t offset = rd<std::int32_t>(node + AABB_CHILDREN_OFFSET);
						const int models = rd<std::uint16_t>(node + AABB_SMODEL_INDEX_COUNT);
						const zaddr list = rd<std::uint32_t>(node + AABB_SMODEL_INDEXES);

						world_cell_node& n = out.nodes[static_cast<std::size_t>(k)];
						std::span<const std::uint8_t> model_bytes;
						if (models > 0 && range(zone, list, models, 2, model_bytes))
						{
							// WorldCells.cs:203: all of them or none
							bool all_valid = true;
							n.static_models.resize(static_cast<std::size_t>(models));
							for (std::size_t x = 0; x < n.static_models.size(); x++)
							{
								const int model = rd<std::uint16_t>(model_bytes.data() + 2 * x);
								n.static_models[x] = model;
								all_valid = all_valid && model < static_model_count;
							}
							if (!all_valid)
							{
								n.static_models.clear();
							}
						}
						n.center = rd_float3(node + AABB_CENTER);
						n.half = rd_float3(node + AABB_HALF);
						n.first_surface = first + count <= sorted_count ? first : 0;
						n.surface_count = first + count <= sorted_count ? count : 0;
						// childrenOffset is a byte offset from the node itself; C# int % and / truncate as C++ does
						n.first_child = offset % AABB_SIZE == 0 ? k + offset / AABB_SIZE : 0;
						n.child_count = children;
					}
				}

				out.index = i;
				out.center = rd_float3(cell + CELL_BOUNDS);
				out.half = rd_float3(cell + CELL_BOUNDS + 12);
			}
			return data;
		}
	}

	// WorldCells.cs:90-91
	bool world_cell::contains(const float3& point) const
	{
		return std::fabs(point.x - center.x) <= half.x && std::fabs(point.y - center.y) <= half.y && std::fabs(point.z - center.z) <= half.z;
	}

	// WorldCells.cs:48-75
	int world_cell_data::cell_of_point(const float3& point) const
	{
		if (nodes.empty() || planes.empty())
		{
			return -1;
		}
		const int node_count = static_cast<int>(nodes.size());
		int at = 0;
		for (int step = 0; step < 256; step++)
		{
			// The engine's encoding ( IW R_CellForPoint; the node stream of every shipped map is consumed exactly by
			// it, see codol_bsp/README.md ): a value up to the cell count is a leaf - 0: no cell, else cell + 1;
			// anything above is a split by plane value - cellCount - 1, whose front child follows and whose back
			// child is `nodes[ at + 1 ]` entries on.
			const int value = nodes[static_cast<std::size_t>(at)];
			const int cell_count = static_cast<int>(cells.size());
			if (value <= cell_count)
			{
				return value - 1;
			}
			const int plane_index = value - cell_count - 1;
			if (plane_index >= static_cast<int>(planes.size()) || at + 1 >= node_count)
			{
				return -1;
			}
			const float4& plane = planes[static_cast<std::size_t>(plane_index)];
			const float side = plane.x * point.x + plane.y * point.y + plane.z * point.z - plane.w;
			at = side > 0 ? at + 2 : at + nodes[static_cast<std::size_t>(at) + 1];
			if (at < 0 || at >= node_count)
			{
				return -1;
			}
		}
		return -1;
	}

	// WorldCells.cs:96-217
	std::optional<world_cell_data> read_world_cells(const codol::zone::Zone& zone, const codol::zone::zaddr gfx_world,
		const int surface_count, const int static_model_count, std::vector<std::string>& warnings)
	{
		try
		{
			return read(zone, gfx_world, surface_count, static_model_count, warnings);
		}
		catch (const std::bad_alloc&)
		{
		}
		try
		{
			warnings.emplace_back("out of memory reading the visibility cells; the view is culled by the frustum only");
		}
		catch (const std::bad_alloc&)
		{
		}
		return std::nullopt;
	}
}
