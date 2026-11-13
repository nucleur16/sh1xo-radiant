#pragma once

// CPU side triangles for picking: positions, indices and a bounding volume hierarchy over them. Not in the C#
// renderer ( the AssetExplorer has no picking ). One pick_mesh holds the triangles of one thing in its own space:
// the static world, one brush model, one XModel ( LOD 0 ), one dynamic mesh.

#include "map_types.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace codol::view
{
	struct pick_hit
	{
		float t = 0;					// along the ray, in units of its direction
		std::uint32_t triangle = 0;		// index into the mesh's triangles
		float3 normal;					// ( b - a ) x ( c - a ), not normalised
		int priority = 0;				// what `accept` returned for it
	};

	// two hits closer together than this are the same place ( a decal on its wall, two layers of terrain )
	inline float pick_tie_distance(const float t) { return 0.01f + 2.0e-5f * t; }

	class pick_mesh
	{
	public:
		pick_mesh() = default;
		pick_mesh(const pick_mesh&) = delete;
		pick_mesh& operator=(const pick_mesh&) = delete;

		// `positions` must outlive the mesh ( the world's vertices are shared by the world and its brush models );
		// own_positions() makes the mesh keep them itself
		void set_positions(std::span<const float3> positions) { positions_ = positions; }
		void own_positions(std::vector<float3> positions);
		// one triangle: three indices into the positions and the surface it belongs to
		void reserve(std::size_t triangles);
		void add_triangle(std::uint32_t a, std::uint32_t b, std::uint32_t c, std::uint32_t surface);
		void build();
		void clear();
		void swap(pick_mesh& other) noexcept;

		std::size_t triangle_count() const { return surfaces_.size(); }
		std::uint32_t surface_of(const std::uint32_t tri) const { return surfaces_[tri]; }
		void triangle(std::uint32_t tri, float3& a, float3& b, float3& c) const;
		std::uint64_t bytes() const;
		bool empty() const { return surfaces_.empty(); }

		// Nearest hit with t in ( 0, t_max ). `accept( surface, facing )` returns < 0 when a triangle of that
		// surface does not count, else its priority; facing is the sign of dot( normal, direction ): > 0 the ray
		// sees the side the normal points away from. Of hits at the same place ( pick_tie_distance ) the one with
		// the highest priority wins - the caller passes the draw order, so that what is drawn on top is picked.
		// `hit` is in / out: a hit of an earlier call ( t <= t_max ) takes part in the tie. Returns false and
		// leaves `hit` alone when nothing better is found.
		template <typename Accept>
		bool raycast(const float3& origin, const float3& direction, float t_max, pick_hit& hit, Accept&& accept) const;

	private:
		struct node
		{
			float3 min, max;
			std::uint32_t first = 0;		// leaf: first slot of order_; inner: index of the left child ( right = left + 1 )
			std::uint32_t count = 0;		// leaf: triangles; 0 = inner node
		};

		void split(std::uint32_t node_index, std::vector<float3>& centroids);
		static bool ray_box(const node& n, const float3& origin, const float3& inverse, float t_max);
		bool ray_triangle(std::uint32_t tri, const float3& origin, const float3& direction, float& t, float3& normal) const;

		std::span<const float3> positions_;
		std::vector<float3> own_;
		std::vector<std::uint32_t> indices_;		// 3 per triangle
		std::vector<std::uint32_t> surfaces_;		// per triangle
		std::vector<std::uint32_t> order_;			// triangles in leaf order
		std::vector<node> nodes_;
	};

	template <typename Accept>
	bool pick_mesh::raycast(const float3& origin, const float3& direction, float t_max, pick_hit& hit, Accept&& accept) const
	{
		if (nodes_.empty())
		{
			return false;
		}
		const auto reciprocal = [](const float v) { return std::fabs(v) > 1.0e-20f ? 1.0f / v : (v < 0 ? -1.0e20f : 1.0e20f); };
		const float3 inverse{ reciprocal(direction.x), reciprocal(direction.y), reciprocal(direction.z) };
		std::uint32_t stack[64];
		int depth = 0;
		stack[depth++] = 0;
		bool found = false;
		float nearest = t_max;			// the nearest accepted hit; candidates reach a tie's width beyond it
		int priority = hit.priority;
		t_max = nearest + pick_tie_distance(nearest);
		while (depth > 0)
		{
			const node& n = nodes_[stack[--depth]];
			if (!ray_box(n, origin, inverse, t_max))
			{
				continue;
			}
			if (n.count == 0)
			{
				if (depth + 2 <= 64)
				{
					// the child on the far side of the split first, so that the near one is popped first
					const node& left = nodes_[n.first];
					const float3 d = (left.min + left.max) * 0.5f - origin;
					const bool left_near = d.x * direction.x + d.y * direction.y + d.z * direction.z > 0;
					stack[depth++] = left_near ? n.first + 1 : n.first;
					stack[depth++] = left_near ? n.first : n.first + 1;
				}
				continue;
			}
			for (std::uint32_t i = 0; i < n.count; i++)
			{
				const std::uint32_t tri = order_[n.first + i];
				float t = 0;
				float3 normal;
				if (!ray_triangle(tri, origin, direction, t, normal) || t <= 0 || t >= t_max)
				{
					continue;
				}
				const float facing = normal.x * direction.x + normal.y * direction.y + normal.z * direction.z;
				const int candidate = accept(surfaces_[tri], facing);
				if (candidate < 0)
				{
					continue;
				}
				// clearly nearer: taken. At the same place: only what is drawn later
				const bool nearer = t < nearest - pick_tie_distance(nearest);
				if (!nearer && candidate <= priority)
				{
					continue;
				}
				nearest = nearer ? t : std::min(nearest, t);
				t_max = nearest + pick_tie_distance(nearest);
				priority = candidate;
				hit.t = t;
				hit.triangle = tri;
				hit.normal = normal;
				hit.priority = candidate;
				found = true;
			}
		}
		return found;
	}
}
