#pragma once

// codol_edit: small double precision math used by the document model and the editing operations.
// Map coordinates go up to +-131072 with fractions, so everything is double; meshes are converted to float
// only when they are handed to a renderer.

#include <cmath>
#include <cstdint>

namespace codol::edit
{
	inline constexpr double PI = 3.14159265358979323846;
	inline constexpr double WORLD_EXTENT = 131072.0;	// |coordinate| limit of the engine

	struct vec2
	{
		double x = 0.0, y = 0.0;
	};

	struct vec3
	{
		double x = 0.0, y = 0.0, z = 0.0;

		constexpr vec3() = default;
		constexpr vec3(const double x_, const double y_, const double z_) : x(x_), y(y_), z(z_) {}

		constexpr double& operator[](const int i) { return i == 0 ? x : (i == 1 ? y : z); }
		constexpr const double& operator[](const int i) const { return i == 0 ? x : (i == 1 ? y : z); }

		constexpr vec3 operator+(const vec3& o) const { return { x + o.x, y + o.y, z + o.z }; }
		constexpr vec3 operator-(const vec3& o) const { return { x - o.x, y - o.y, z - o.z }; }
		constexpr vec3 operator*(const double s) const { return { x * s, y * s, z * s }; }
		constexpr vec3 operator-() const { return { -x, -y, -z }; }
		constexpr vec3& operator+=(const vec3& o) { x += o.x; y += o.y; z += o.z; return *this; }
		constexpr vec3& operator-=(const vec3& o) { x -= o.x; y -= o.y; z -= o.z; return *this; }
		constexpr bool operator==(const vec3& o) const { return x == o.x && y == o.y && z == o.z; }
	};

	constexpr double dot(const vec3& a, const vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
	constexpr vec3 cross(const vec3& a, const vec3& b)
	{
		return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x };
	}
	inline double length(const vec3& a) { return std::sqrt(dot(a, a)); }
	inline vec3 normalize(const vec3& a)
	{
		const double len = length(a);
		return len > 1e-300 ? a * (1.0 / len) : vec3{};
	}

	// normal . p = dist; the front of a plane is where normal . p - dist > 0. Brush faces point out of the brush.
	struct plane
	{
		vec3 normal;
		double dist = 0.0;

		[[nodiscard]] constexpr double distance_to(const vec3& p) const { return dot(normal, p) - dist; }
		[[nodiscard]] constexpr plane flipped() const { return { -normal, -dist }; }
	};

	// the plane of a brush face as the map format defines it: three points with cross( c - a, b - a ) pointing out
	// of the brush. false if the points are collinear.
	bool plane_from_points(const vec3& a, const vec3& b, const vec3& c, plane& out);

	struct bounds
	{
		vec3 mins{ 1e30, 1e30, 1e30 };
		vec3 maxs{ -1e30, -1e30, -1e30 };

		[[nodiscard]] constexpr bool valid() const { return mins.x <= maxs.x && mins.y <= maxs.y && mins.z <= maxs.z; }
		constexpr void add(const vec3& p)
		{
			for (int i = 0; i < 3; i++)
			{
				if (p[i] < mins[i]) { mins[i] = p[i]; }
				if (p[i] > maxs[i]) { maxs[i] = p[i]; }
			}
		}
		constexpr void add(const bounds& b)
		{
			if (b.valid()) { add(b.mins); add(b.maxs); }
		}
		[[nodiscard]] constexpr vec3 center() const { return (mins + maxs) * 0.5; }
		[[nodiscard]] constexpr vec3 size() const { return maxs - mins; }
		[[nodiscard]] constexpr bool intersects(const bounds& b, const double eps = 0.0) const
		{
			for (int i = 0; i < 3; i++)
			{
				if (b.mins[i] > maxs[i] + eps || b.maxs[i] < mins[i] - eps) { return false; }
			}
			return true;
		}
		[[nodiscard]] constexpr bool contains(const vec3& p, const double eps = 0.0) const
		{
			for (int i = 0; i < 3; i++)
			{
				if (p[i] > maxs[i] + eps || p[i] < mins[i] - eps) { return false; }
			}
			return true;
		}
	};

	struct ray
	{
		vec3 origin;
		vec3 direction;		// does not have to be normalised; distances are reported in units of its length
	};

	// affine transform p' = m * p + t ( m[row][column] )
	struct transform
	{
		double m[3][3] = { { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 } };
		vec3 t;

		[[nodiscard]] constexpr vec3 apply(const vec3& p) const
		{
			return {
				m[0][0] * p.x + m[0][1] * p.y + m[0][2] * p.z + t.x,
				m[1][0] * p.x + m[1][1] * p.y + m[1][2] * p.z + t.y,
				m[2][0] * p.x + m[2][1] * p.y + m[2][2] * p.z + t.z };
		}
		[[nodiscard]] constexpr vec3 apply_vector(const vec3& v) const
		{
			return {
				m[0][0] * v.x + m[0][1] * v.y + m[0][2] * v.z,
				m[1][0] * v.x + m[1][1] * v.y + m[1][2] * v.z,
				m[2][0] * v.x + m[2][1] * v.y + m[2][2] * v.z };
		}
		[[nodiscard]] constexpr double determinant() const
		{
			return m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1])
				- m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0])
				+ m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]);
		}
		// true when the linear part is a pure rotation ( no scale, no mirror )
		[[nodiscard]] bool is_rotation(double eps = 1e-9) const;
		[[nodiscard]] bool is_translation() const;

		static transform translation(const vec3& delta);
		// right handed rotation of `degrees` about the line through `origin` along `axis`
		static transform rotation(const vec3& origin, const vec3& axis, double degrees);
		// rotation by game angles ( pitch yaw roll, degrees ) about `origin`
		static transform rotation_angles(const vec3& origin, const vec3& angles);
		static transform scaling(const vec3& origin, const vec3& factors);
		// mirror along a world axis ( 0 x, 1 y, 2 z ) through `origin`
		static transform flip(const vec3& origin, int axis);
		// a then b
		static transform concat(const transform& a, const transform& b);
	};

	// game angles ( pitch yaw roll in degrees ) <-> axes ( forward, left, up ), the engine's AnglesToAxis / AxisToAngles
	void angles_to_axes(const vec3& angles, vec3 axes[3]);
	vec3 axes_to_angles(const vec3 axes[3]);

	inline double snap_value(const double v, const double grid)
	{
		return grid > 0.0 ? std::floor(v / grid + 0.5) * grid : v;
	}
	inline vec3 snap_point(const vec3& p, const double grid)
	{
		return { snap_value(p.x, grid), snap_value(p.y, grid), snap_value(p.z, grid) };
	}
}
