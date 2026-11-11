#pragma once

// Minimal test harness of codol_render ( no third party framework ).

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace test
{
	struct counters
	{
		int checks = 0;
		int failures = 0;
	};
	counters& state();

	inline void check(const bool ok, const char* expression, const char* file, const int line)
	{
		state().checks++;
		if (!ok)
		{
			state().failures++;
			std::printf("  FAILED %s:%d  %s\n", file, line, expression);
		}
	}

	template <typename A, typename B>
	void check_eq(const A& a, const B& b, const char* expression, const char* file, const int line)
	{
		state().checks++;
		if (!(a == b))
		{
			state().failures++;
			std::printf("  FAILED %s:%d  %s  ( %lld vs %lld )\n", file, line, expression, static_cast<long long>(a), static_cast<long long>(b));
		}
	}

	inline void check_near(const double a, const double b, const double tolerance, const char* expression, const char* file, const int line)
	{
		state().checks++;
		if (!(std::fabs(a - b) <= tolerance))
		{
			state().failures++;
			std::printf("  FAILED %s:%d  %s  ( %.9g vs %.9g )\n", file, line, expression, a, b);
		}
	}

	// little endian byte buffer builder for hand made zone data
	struct bytes
	{
		std::vector<std::uint8_t> data;

		std::size_t size() const { return data.size(); }
		bytes& u8(const std::uint8_t v) { data.push_back(v); return *this; }
		bytes& u16(const std::uint16_t v) { data.push_back(static_cast<std::uint8_t>(v)); data.push_back(static_cast<std::uint8_t>(v >> 8)); return *this; }
		bytes& u32(const std::uint32_t v) { u16(static_cast<std::uint16_t>(v)); u16(static_cast<std::uint16_t>(v >> 16)); return *this; }
		bytes& f32(const float v) { std::uint32_t b; std::memcpy(&b, &v, 4); return u32(b); }
		bytes& zeros(const std::size_t n) { data.insert(data.end(), n, 0); return *this; }
		bytes& str(const char* s) { data.insert(data.end(), s, s + std::strlen(s) + 1); return *this; }
		bytes& raw(const void* p, const std::size_t n) { const auto* b = static_cast<const std::uint8_t*>(p); data.insert(data.end(), b, b + n); return *this; }
		bytes& pad_to(const std::size_t n) { if (data.size() < n) { data.resize(n, 0); } return *this; }
		void put_u32(const std::size_t at, const std::uint32_t v) { for (int i = 0; i < 4; i++) { data[at + i] = static_cast<std::uint8_t>(v >> (8 * i)); } }
		void put_u16(const std::size_t at, const std::uint16_t v) { data[at] = static_cast<std::uint8_t>(v); data[at + 1] = static_cast<std::uint8_t>(v >> 8); }
		void put_u8(const std::size_t at, const std::uint8_t v) { data[at] = v; }
	};
}

#define CHECK(expression) ::test::check((expression), #expression, __FILE__, __LINE__)
#define CHECK_EQ(a, b) ::test::check_eq((a), (b), #a " == " #b, __FILE__, __LINE__)
#define CHECK_NEAR(a, b, tolerance) ::test::check_near((a), (b), (tolerance), #a " ~ " #b, __FILE__, __LINE__)

void test_shader_program();
void test_technique_set();
void test_vertex_format();
void test_render_state();
void test_material();
void test_code_constants();
void test_technique_choice();
void test_textures();
void test_iwd();
