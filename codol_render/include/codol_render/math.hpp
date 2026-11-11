#pragma once

// Small math for the code constants: float3 / float4 and a 4x4 matrix. No D3DX.
//
// Conventions ( those of System.Numerics, which the C# renderer uses, and of D3DXMATRIX ):
//   - row-major storage: m[row][col], 16 floats in memory in the order _11 _12 _13 _14 _21 .. _44;
//   - row vectors: v' = v * M, translation in the fourth row ( m[3][0..2] );
//   - a * b applies a first, then b ( world * view * projection ).
// A matrix is uploaded to a shader row by row ( register r = row r ), see code_constants::write.

#include <cmath>
#include <cstdint>

namespace codol::render
{
	struct float3
	{
		float x = 0, y = 0, z = 0;

		constexpr float3 operator+(const float3& o) const { return { x + o.x, y + o.y, z + o.z }; }
		constexpr float3 operator-(const float3& o) const { return { x - o.x, y - o.y, z - o.z }; }
		constexpr float3 operator-() const { return { -x, -y, -z }; }
		constexpr float3 operator*(const float s) const { return { x * s, y * s, z * s }; }
		constexpr bool operator==(const float3&) const = default;
	};

	struct float4
	{
		float x = 0, y = 0, z = 0, w = 0;

		constexpr float4() = default;
		constexpr float4(const float x_, const float y_, const float z_, const float w_) : x(x_), y(y_), z(z_), w(w_) {}
		constexpr float4(const float3& v, const float w_) : x(v.x), y(v.y), z(v.z), w(w_) {}
		constexpr explicit float4(const float all) : x(all), y(all), z(all), w(all) {}
		constexpr bool operator==(const float4&) const = default;
	};

	struct mat4
	{
		float m[4][4] = { { 1, 0, 0, 0 }, { 0, 1, 0, 0 }, { 0, 0, 1, 0 }, { 0, 0, 0, 1 } };

		constexpr bool operator==(const mat4&) const = default;
		constexpr float4 row(const int r) const { return { m[r][0], m[r][1], m[r][2], m[r][3] }; }
		const float* data() const { return &m[0][0]; }		// 16 floats, row-major ( = D3DXMATRIX )

		static constexpr mat4 identity() { return {}; }

		static constexpr mat4 from_rows(const float4& r0, const float4& r1, const float4& r2, const float4& r3)
		{
			mat4 r;
			const float4 rows[4] = { r0, r1, r2, r3 };
			for (int i = 0; i < 4; i++)
			{
				r.m[i][0] = rows[i].x; r.m[i][1] = rows[i].y; r.m[i][2] = rows[i].z; r.m[i][3] = rows[i].w;
			}
			return r;
		}

		// 16 floats, row-major ( e.g. GfxWorld's outdoor lookup matrix as stored in the zone )
		static mat4 from_floats(const float* f)
		{
			mat4 r;
			for (int i = 0; i < 16; i++)
			{
				r.m[i / 4][i % 4] = f[i];
			}
			return r;
		}

		static constexpr mat4 translation(const float3& t)
		{
			mat4 r;
			r.m[3][0] = t.x; r.m[3][1] = t.y; r.m[3][2] = t.z;
			return r;
		}
	};

	// a * b ( a first, then b )
	constexpr mat4 mul(const mat4& a, const mat4& b)
	{
		mat4 r;
		for (int i = 0; i < 4; i++)
		{
			for (int j = 0; j < 4; j++)
			{
				r.m[i][j] = a.m[i][0] * b.m[0][j] + a.m[i][1] * b.m[1][j] + a.m[i][2] * b.m[2][j] + a.m[i][3] * b.m[3][j];
			}
		}
		return r;
	}

	constexpr mat4 transpose(const mat4& a)
	{
		mat4 r;
		for (int i = 0; i < 4; i++)
		{
			for (int j = 0; j < 4; j++)
			{
				r.m[i][j] = a.m[j][i];
			}
		}
		return r;
	}

	// v * M ( row vector )
	constexpr float4 transform(const float4& v, const mat4& a)
	{
		return {
			v.x * a.m[0][0] + v.y * a.m[1][0] + v.z * a.m[2][0] + v.w * a.m[3][0],
			v.x * a.m[0][1] + v.y * a.m[1][1] + v.z * a.m[2][1] + v.w * a.m[3][1],
			v.x * a.m[0][2] + v.y * a.m[1][2] + v.z * a.m[2][2] + v.w * a.m[3][2],
			v.x * a.m[0][3] + v.y * a.m[1][3] + v.z * a.m[2][3] + v.w * a.m[3][3],
		};
	}

	// General inverse by cofactors. false ( and `out` untouched ) when the matrix is singular.
	bool inverse(const mat4& a, mat4& out);

	// GameCodeConstants.cs:263: IEC 61966-2-1 style decode with the game's constants
	inline float srgb_to_linear(const float c)
	{
		return c > 0.03928f ? std::pow((c + 0.055f) * 0.9478673f, 2.4f) : c * 0.07739938f;
	}

	// ---------------------------------------------------------------------------------------------------
	// packed vertex members ( Codol.Assets\Utilities\MathExtensions.cs:9-16 ), for picking

	// IEEE 754 binary16 -> float ( GfxPackedVertex / GfxWorldVertex texture coordinates )
	float half_to_float(std::uint16_t bits);

	// PackedUnitVec ( GfxPackedVertex normal / tangent ): three bytes around 127 and a scale byte
	inline float3 unpack_unit_vector(const std::uint32_t packed)
	{
		const float scale = (static_cast<float>(packed >> 24) + 192.0f) / 32385.0f;
		return {
			(static_cast<float>(packed & 0xFF) - 127.0f) * scale,
			(static_cast<float>((packed >> 8) & 0xFF) - 127.0f) * scale,
			(static_cast<float>((packed >> 16) & 0xFF) - 127.0f) * scale,
		};
	}
}
