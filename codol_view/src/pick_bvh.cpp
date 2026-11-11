#include "pick_bvh.hpp"

#include <algorithm>
#include <cfloat>
#include <cmath>

namespace codol::view
{
	namespace
	{
		constexpr std::uint32_t LEAF_TRIANGLES = 4;

		float component(const float3& v, const int axis) { return axis == 0 ? v.x : axis == 1 ? v.y : v.z; }
		float3 min3(const float3& a, const float3& b) { return { std::min(a.x, b.x), std::min(a.y, b.y), std::min(a.z, b.z) }; }
		float3 max3(const float3& a, const float3& b) { return { std::max(a.x, b.x), std::max(a.y, b.y), std::max(a.z, b.z) }; }
	}

	void pick_mesh::own_positions(std::vector<float3> positions)
	{
		own_ = std::move(positions);
		positions_ = own_;
	}

	void pick_mesh::clear()
	{
		positions_ = {};
		own_ = {};
		indices_ = {};
		surfaces_ = {};
		order_ = {};
		nodes_ = {};
	}

	void pick_mesh::swap(pick_mesh& other) noexcept
	{
		// the spans point into the vectors' heap blocks, which move with them
		std::swap(positions_, other.positions_);
		own_.swap(other.own_);
		indices_.swap(other.indices_);
		surfaces_.swap(other.surfaces_);
		order_.swap(other.order_);
		nodes_.swap(other.nodes_);
	}

	void pick_mesh::reserve(const std::size_t triangles)
	{
		indices_.reserve(3 * triangles);
		surfaces_.reserve(triangles);
	}

	void pick_mesh::add_triangle(const std::uint32_t a, const std::uint32_t b, const std::uint32_t c, const std::uint32_t surface)
	{
		const std::size_t count = positions_.size();
		if (a >= count || b >= count || c >= count)
		{
			return;
		}
		indices_.push_back(a);
		indices_.push_back(b);
		indices_.push_back(c);
		surfaces_.push_back(surface);
	}

	void pick_mesh::triangle(const std::uint32_t tri, float3& a, float3& b, float3& c) const
	{
		a = positions_[indices_[3 * static_cast<std::size_t>(tri)]];
		b = positions_[indices_[3 * static_cast<std::size_t>(tri) + 1]];
		c = positions_[indices_[3 * static_cast<std::size_t>(tri) + 2]];
	}

	std::uint64_t pick_mesh::bytes() const
	{
		return own_.capacity() * sizeof(float3) + (indices_.capacity() + surfaces_.capacity() + order_.capacity()) * sizeof(std::uint32_t)
			+ nodes_.capacity() * sizeof(node);
	}

	void pick_mesh::build()
	{
		nodes_.clear();
		order_.clear();
		const std::size_t count = surfaces_.size();
		if (count == 0)
		{
			return;
		}
		indices_.shrink_to_fit();
		surfaces_.shrink_to_fit();
		order_.resize(count);
		std::vector<float3> centroids(count);
		for (std::size_t t = 0; t < count; t++)
		{
			order_[t] = static_cast<std::uint32_t>(t);
			float3 a, b, c;
			triangle(static_cast<std::uint32_t>(t), a, b, c);
			centroids[t] = (a + b + c) * (1.0f / 3.0f);
		}
		nodes_.reserve(count / 2 + 8);
		node root;
		root.first = 0;
		root.count = static_cast<std::uint32_t>(count);
		nodes_.push_back(root);
		// median split of the longest centroid axis, breadth first: a node's children are always adjacent
		for (std::uint32_t at = 0; at < nodes_.size(); at++)
		{
			split(at, centroids);
		}
		nodes_.shrink_to_fit();
	}

	void pick_mesh::split(const std::uint32_t node_index, std::vector<float3>& centroids)
	{
		const std::uint32_t first = nodes_[node_index].first, count = nodes_[node_index].count;
		float3 box_min{ FLT_MAX, FLT_MAX, FLT_MAX }, box_max{ -FLT_MAX, -FLT_MAX, -FLT_MAX };
		float3 centre_min = box_min, centre_max = box_max;
		for (std::uint32_t i = 0; i < count; i++)
		{
			const std::uint32_t t = order_[first + i];
			float3 a, b, c;
			triangle(t, a, b, c);
			box_min = min3(box_min, min3(a, min3(b, c)));
			box_max = max3(box_max, max3(a, max3(b, c)));
			centre_min = min3(centre_min, centroids[t]);
			centre_max = max3(centre_max, centroids[t]);
		}
		nodes_[node_index].min = box_min;
		nodes_[node_index].max = box_max;
		if (count <= LEAF_TRIANGLES)
		{
			return;
		}
		const float3 extent = centre_max - centre_min;
		const int axis = extent.x >= extent.y && extent.x >= extent.z ? 0 : extent.y >= extent.z ? 1 : 2;
		if (component(extent, axis) <= 0)
		{
			return;		// all centroids in one point: a ( large ) leaf
		}
		const auto begin = order_.begin() + first, middle = begin + count / 2, end = begin + count;
		std::nth_element(begin, middle, end, [&](const std::uint32_t l, const std::uint32_t r) { return component(centroids[l], axis) < component(centroids[r], axis); });
		node left, right;
		left.first = first;
		left.count = count / 2;
		right.first = first + count / 2;
		right.count = count - count / 2;
		const std::uint32_t child = static_cast<std::uint32_t>(nodes_.size());
		nodes_.push_back(left);
		nodes_.push_back(right);
		nodes_[node_index].first = child;
		nodes_[node_index].count = 0;
	}

	bool pick_mesh::ray_box(const node& n, const float3& origin, const float3& inverse, const float t_max)
	{
		float t0 = 0, t1 = t_max;
		const float lo[3] = { (n.min.x - origin.x) * inverse.x, (n.min.y - origin.y) * inverse.y, (n.min.z - origin.z) * inverse.z };
		const float hi[3] = { (n.max.x - origin.x) * inverse.x, (n.max.y - origin.y) * inverse.y, (n.max.z - origin.z) * inverse.z };
		for (int axis = 0; axis < 3; axis++)
		{
			const float near_t = std::min(lo[axis], hi[axis]), far_t = std::max(lo[axis], hi[axis]);
			t0 = std::max(t0, near_t);
			t1 = std::min(t1, far_t);
		}
		// a little slack: the slabs of a flat box and the rounding of the reciprocal
		return t0 <= t1 * 1.00001f + 1.0e-4f;
	}

	bool pick_mesh::ray_triangle(const std::uint32_t tri, const float3& origin, const float3& direction, float& t, float3& normal) const
	{
		// Moeller - Trumbore, in double: world coordinates of a map reach five digits
		float3 a, b, c;
		triangle(tri, a, b, c);
		const double e1[3] = { static_cast<double>(b.x) - a.x, static_cast<double>(b.y) - a.y, static_cast<double>(b.z) - a.z };
		const double e2[3] = { static_cast<double>(c.x) - a.x, static_cast<double>(c.y) - a.y, static_cast<double>(c.z) - a.z };
		const double d[3] = { direction.x, direction.y, direction.z };
		const double p[3] = { d[1] * e2[2] - d[2] * e2[1], d[2] * e2[0] - d[0] * e2[2], d[0] * e2[1] - d[1] * e2[0] };
		const double det = e1[0] * p[0] + e1[1] * p[1] + e1[2] * p[2];
		if (det > -1.0e-12 && det < 1.0e-12)
		{
			return false;
		}
		const double inv = 1.0 / det;
		const double s[3] = { static_cast<double>(origin.x) - a.x, static_cast<double>(origin.y) - a.y, static_cast<double>(origin.z) - a.z };
		const double u = (s[0] * p[0] + s[1] * p[1] + s[2] * p[2]) * inv;
		if (u < 0.0 || u > 1.0)
		{
			return false;
		}
		const double q[3] = { s[1] * e1[2] - s[2] * e1[1], s[2] * e1[0] - s[0] * e1[2], s[0] * e1[1] - s[1] * e1[0] };
		const double v = (d[0] * q[0] + d[1] * q[1] + d[2] * q[2]) * inv;
		if (v < 0.0 || u + v > 1.0)
		{
			return false;
		}
		t = static_cast<float>((e2[0] * q[0] + e2[1] * q[1] + e2[2] * q[2]) * inv);
		normal = { static_cast<float>(e1[1] * e2[2] - e1[2] * e2[1]), static_cast<float>(e1[2] * e2[0] - e1[0] * e2[2]), static_cast<float>(e1[0] * e2[1] - e1[1] * e2[0]) };
		return true;
	}
}
