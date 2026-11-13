#include "world_visibility.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <new>
#include <utility>

// the arithmetic has to match the C# ( System.Numerics, single precision, no fused multiply-add )
#pragma fp_contract(off)

namespace codol::view
{
	namespace
	{
		// System.Numerics.Vector3 as the C# uses it
		float dot(const float3& a, const float3& b)
		{
			return a.x * b.x + a.y * b.y + a.z * b.z;
		}

		float length_squared(const float3& a)
		{
			return dot(a, a);
		}

		float3 cross(const float3& a, const float3& b)
		{
			return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x };
		}

		// Vector3.Lerp: a * ( 1 - t ) + b * t
		float3 lerp(const float3& a, const float3& b, const float t)
		{
			return a * (1.0f - t) + b * t;
		}

		float3 divide(const float3& a, const float d)
		{
			return { a.x / d, a.y / d, a.z / d };
		}
	}

	// GameWorldVisibility.cs:25-64
	world_visibility::world_visibility(world_cell_data data, const int surface_count, const int static_model_count)
	{
		try
		{
			const std::size_t surfaces = static_cast<std::size_t>(std::max(surface_count, 0));
			const std::size_t models = static_cast<std::size_t>(std::max(static_model_count, 0));
			std::vector<std::uint8_t> surface_reset(surfaces, std::uint8_t{ 1 }), model_reset(models, std::uint8_t{ 1 });
			std::vector<std::uint8_t> surface_visible(surfaces), model_visible(models);
			std::vector<std::uint8_t> on_path(data.cells.size()), reached(data.cells.size());
			std::vector<float4> portal_planes(static_cast<std::size_t>(MAX_DEPTH) * MAX_PORTAL_PLANES);
			std::vector<int> node_stack;
			node_stack.reserve(256);

			// GameWorldVisibility.cs:37-63: what a cell lists is hidden until a cell shows it ( reset = !inCell )
			const std::vector<std::uint16_t>& sorted = data.sorted_surfaces;
			for (const world_cell& cell : data.cells)
			{
				for (const world_cell_node& node : cell.nodes)
				{
					for (std::int64_t s = node.first_surface; s < static_cast<std::int64_t>(node.first_surface) + node.surface_count && s < static_cast<std::int64_t>(sorted.size()); s++)
					{
						// s >= 0 and the index checks never fail for read_world_cells data
						if (s >= 0 && sorted[static_cast<std::size_t>(s)] < surfaces)
						{
							surface_reset[sorted[static_cast<std::size_t>(s)]] = 0;
						}
					}
					for (const int model : node.static_models)
					{
						if (model >= 0 && static_cast<std::size_t>(model) < models)
						{
							model_reset[static_cast<std::size_t>(model)] = 0;
						}
					}
				}
			}

			// nothing below throws
			data_ = std::move(data);
			surface_visible_ = std::move(surface_visible);
			model_visible_ = std::move(model_visible);
			surface_reset_ = std::move(surface_reset);
			model_reset_ = std::move(model_reset);
			on_path_ = std::move(on_path);
			reached_ = std::move(reached);
			portal_planes_ = std::move(portal_planes);
			node_stack_ = std::move(node_stack);
		}
		catch (const std::bad_alloc&)
		{
			// stays empty: no cells, no surfaces, no models
		}
	}

	// GameWorldVisibility.cs:78-111
	void world_visibility::update(const float3& eye, const std::span<const float4> frustum)
	{
		std::copy(surface_reset_.begin(), surface_reset_.end(), surface_visible_.begin());
		std::copy(model_reset_.begin(), model_reset_.end(), model_visible_.begin());
		std::fill(on_path_.begin(), on_path_.end(), std::uint8_t{ 0 });
		std::fill(reached_.begin(), reached_.end(), std::uint8_t{ 0 });
		visible_cells_ = 0;
		reached_cells_ = 0;
		visits_ = 0;
		whole_frustum_ = frustum;
		camera_cell_ = data_.cell_of_point(eye);
		const int count = cell_count();
		// CODOL_VIEW_STRICT_CELLS=1: only what the portals reach from the cell the camera is in is drawn - no start
		// in cells whose box merely holds the eye, no fallback to the frustum for unreached cells. That is what the
		// game does, and what a map compiler's cells and portals are judged by ( codol_bsp ).
		static const bool strict = std::getenv("CODOL_VIEW_STRICT_CELLS") != nullptr;
		const bool strict_now = strict && camera_cell_ >= 0;
		int starts = 0;
		for (int i = 0; i < count; i++)
		{
			if (i != camera_cell_ && (strict_now || !data_.cells[static_cast<std::size_t>(i)].contains(eye)))
			{
				continue;
			}
			starts++;
			visit(i, eye, frustum, 0);
		}
		if (starts == 0)
		{
			camera_cell_ = -1;
		}
		// GameWorldVisibility.cs:103-110: what the portals did not reach falls back to the whole frustum
		for (int i = 0; i < count && !strict_now; i++)
		{
			const world_cell& cell = data_.cells[static_cast<std::size_t>(i)];
			if (!reached_[static_cast<std::size_t>(i)] && box_visible(frustum, cell.center - eye, cell.half))
			{
				collect_cell(cell, eye, frustum);
				visible_cells_++;
			}
		}
		whole_frustum_ = {};
	}

	// GameWorldVisibility.cs:113-148
	void world_visibility::visit(const int index, const float3& eye, const std::span<const float4> frustum, const int depth)
	{
		const world_cell& cell = data_.cells[static_cast<std::size_t>(index)];
		if (!reached_[static_cast<std::size_t>(index)])
		{
			reached_[static_cast<std::size_t>(index)] = 1;
			reached_cells_++;
		}
		visible_cells_++;
		if (++visits_ > VISIT_BUDGET)
		{
			collect_cell(cell, eye, whole_frustum_);
			return;
		}
		if (box_visible(frustum, cell.center - eye, cell.half))
		{
			collect_cell(cell, eye, frustum);
		}
		if (depth >= MAX_DEPTH)
		{
			return;
		}
		on_path_[static_cast<std::size_t>(index)] = 1;
		// the frustums of this level's portals; deeper levels use the following blocks, so `frustum` ( which may be
		// the previous level's block ) stays intact
		float4* const storage = portal_planes_.data() + static_cast<std::size_t>(depth) * MAX_PORTAL_PLANES;
		for (const world_portal& portal : cell.portals)
		{
			// the range check never fails for read_world_cells data ( WorldCells.cs:160 )
			if (portal.target_cell < 0 || portal.target_cell >= cell_count() || on_path_[static_cast<std::size_t>(portal.target_cell)])
			{
				continue;
			}
			std::span<const float4> next;
			if (portal_frustum(portal.points, eye, frustum, storage, next))
			{
				visit(portal.target_cell, eye, next, depth + 1);
			}
		}
		on_path_[static_cast<std::size_t>(index)] = 0;
	}

	// GameWorldVisibility.cs:150-226
	bool world_visibility::portal_frustum(const std::span<const float3> points, const float3& eye, const std::span<const float4> frustum,
		float4* const storage, std::span<const float4>& next)
	{
		next = frustum;
		if (points.size() > static_cast<std::size_t>(MAX_PORTAL_POINTS / 2))
		{
			return true;
		}
		float3 buffer_a[MAX_PORTAL_POINTS];
		float3 buffer_b[MAX_PORTAL_POINTS];
		float3* polygon = buffer_a;
		float3* clipped = buffer_b;
		int count = static_cast<int>(points.size());
		for (int i = 0; i < count; i++)
		{
			polygon[i] = points[static_cast<std::size_t>(i)] - eye;
			if (length_squared(polygon[i]) < 1.0f)
			{
				return true;		// the eye is on the portal: no narrowing
			}
		}
		// clip the portal by every plane of the incoming frustum ( Sutherland - Hodgman )
		for (const float4& plane : frustum)
		{
			const float3 normal{ plane.x, plane.y, plane.z };
			int written = 0;
			for (int i = 0; i < count && written + 2 <= MAX_PORTAL_POINTS; i++)
			{
				const float3 current = polygon[i];
				const float3 previous = polygon[(i + count - 1) % count];
				const float distance = dot(normal, current) + plane.w;
				const float previous_distance = dot(normal, previous) + plane.w;
				if ((previous_distance < 0) != (distance < 0))
				{
					clipped[written++] = lerp(previous, current, previous_distance / (previous_distance - distance));
				}
				if (distance >= 0)
				{
					clipped[written++] = current;
				}
			}
			if (written < 3)
			{
				return false;
			}
			std::swap(polygon, clipped);		// C#: clipped[ ..written ].CopyTo( polygon )
			count = written;
		}
		if (count == 0)
		{
			return true;	// not in the C# ( a portal has >= 3 points ); avoids the division below
		}
		float3 centroid;
		for (int i = 0; i < count; i++)
		{
			centroid = centroid + polygon[i];
		}
		centroid = divide(centroid, static_cast<float>(count));
		if (length_squared(centroid) < 1.0f)
		{
			return true;
		}
		// one plane through the eye per edge of the clipped portal, facing its centroid
		int planes = 0;
		for (int i = 0; i < count; i++)
		{
			float3 normal = cross(polygon[i], polygon[(i + 1) % count]);
			const float length = std::sqrt(length_squared(normal));
			if (length < 1e-3f)
			{
				continue;
			}
			normal = divide(normal, length);
			if (dot(normal, centroid) < 0.0f)
			{
				normal = -normal;
			}
			storage[planes++] = float4(normal, 0.0f);
		}
		if (planes < 3)
		{
			return true;
		}
		if (!frustum.empty())		// always true for a real frustum; the C# would throw on an empty one
		{
			storage[planes++] = frustum.back();		// near
		}
		next = std::span<const float4>(storage, static_cast<std::size_t>(planes));
		return true;
	}

	// GameWorldVisibility.cs:228-264
	void world_visibility::collect_cell(const world_cell& cell, const float3& eye, const std::span<const float4> frustum)
	{
		if (cell.nodes.empty())
		{
			return;
		}
		const int node_count = static_cast<int>(cell.nodes.size());
		const std::size_t sorted_count = data_.sorted_surfaces.size();
		node_stack_.clear();
		node_stack_.push_back(0);
		while (!node_stack_.empty())
		{
			const world_cell_node& node = cell.nodes[static_cast<std::size_t>(node_stack_.back())];
			node_stack_.pop_back();
			if (!box_visible(frustum, node.center - eye, node.half))
			{
				continue;
			}
			// the index checks below never fail for read_world_cells data; the C# has none
			for (const int model : node.static_models)
			{
				if (model >= 0 && static_cast<std::size_t>(model) < model_visible_.size())
				{
					model_visible_[static_cast<std::size_t>(model)] = 1;
				}
			}
			if (node.child_count == 0)
			{
				for (int s = std::max(node.first_surface, 0); s < node.first_surface + node.surface_count && static_cast<std::size_t>(s) < sorted_count; s++)
				{
					const std::size_t surface = data_.sorted_surfaces[static_cast<std::size_t>(s)];
					if (surface < surface_visible_.size())
					{
						surface_visible_[surface] = 1;
					}
				}
				continue;
			}
			// pushed in order, so the last child is looked at first ( Stack<int> )
			for (int child = 0; child < node.child_count; child++)
			{
				const int at = node.first_child + child;
				if (at > 0 && at < node_count)
				{
					try
					{
						node_stack_.push_back(at);
					}
					catch (const std::bad_alloc&)
					{
						return;
					}
				}
			}
		}
	}

	// GameWorldVisibility.cs:266-278
	bool world_visibility::box_visible(const std::span<const float4> frustum, const float3& center, const float3& half)
	{
		for (const float4& plane : frustum)
		{
			const float distance = plane.x * center.x + plane.y * center.y + plane.z * center.z + plane.w;
			const float radius = std::fabs(plane.x) * half.x + std::fabs(plane.y) * half.y + std::fabs(plane.z) * half.z;
			if (distance + radius < 0.0f)
			{
				return false;
			}
		}
		return true;
	}
}
