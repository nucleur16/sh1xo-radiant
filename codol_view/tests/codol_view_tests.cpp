// Unit tests of the device independent parts of codol_view ( no game install, no Direct3D device needed ).

#include "renderer_impl.hpp"

#include <cstdio>

namespace
{
	int failures = 0;
	int checks = 0;

	void check(const bool ok, const char* expression, const int line)
	{
		checks++;
		if (!ok)
		{
			failures++;
			std::printf("FAILED line %d: %s\n", line, expression);
		}
	}
#define CHECK(x) check((x), #x, __LINE__)

	bool close_to(const float a, const float b, const float eps = 1e-4f) { return std::fabs(a - b) <= eps; }

	using namespace codol::view;

	void test_entities()
	{
		std::vector<std::string> warnings;
		const auto entities = parse_entities("{\n\"classname\" \"worldspawn\"\n}\n{\n\"classname\" \"script_model\"\n\"Origin\" \"1 2.5 -3\"\n\"model\" \"*4\"\n}\n", &warnings);
		CHECK(warnings.empty());
		CHECK(entities.size() == 2);
		CHECK(entities[1].index == 1);
		CHECK(entities[1].class_name() == "script_model");
		CHECK(entities[1].get("origin") != nullptr);		// keys compare without case
		CHECK(entities[1].get("missing") == nullptr);
		const auto origin = parse_vector(entities[1].get("origin"));
		CHECK(origin && origin->x == 1.0f && origin->y == 2.5f && origin->z == -3.0f);
		const std::string two = "1 2", four = "1 2 3 4", spaced = "  1   2  3 ", junk = "1 a 3";
		CHECK(!parse_vector(&two));
		CHECK(!parse_vector(&four));
		CHECK(parse_vector(&spaced).has_value());
		CHECK(!parse_vector(&junk));
		CHECK(!parse_vector(nullptr));
		CHECK(parse_float("1e2").value_or(0) == 100.0f);
		CHECK(parse_float("+2").value_or(0) == 2.0f);
		CHECK(!parse_float("").has_value());
		CHECK(!parse_float("2x").has_value());

		warnings.clear();
		const auto broken = parse_entities("{ \"a\" \"b\" \"c\" } \"x\" { \"k\" \"v\"", &warnings);
		CHECK(broken.size() == 1);
		CHECK(warnings.size() == 3);		// key without value, string outside, last entity not closed
		CHECK(equals_ignore_case("Script_Model", "script_model"));
		CHECK(!equals_ignore_case("a", "ab"));
	}

	void test_zone_names()
	{
		CHECK(shared_zone_names().size() == 9);
		CHECK(shared_zone_names().front() == "code_pre_gfx_mp");
		const auto mp = level_zone_names("mp_rust");
		CHECK(mp.size() == 6 && mp[3] == "common_gametype_mp" && mp[2] == "patch_common_gametype_mp" && mp[4] == "patch_mp_rust" && mp[5] == "mp_rust_const");
		CHECK(level_zone_names("ZM_asylum_sh")[3] == "common_gametype_zm");
		CHECK(level_zone_names("coop_x")[3] == "common_gametype_coop");
		CHECK(level_zone_names("za_x")[3] == "common_gametype_zombie_arena");
		name_map<int> map;
		map.emplace("Abc", 1);
		CHECK(map.count("aBC") == 1);
	}

	void test_frustum()
	{
		camera cam;
		cam.origin = { 100, 200, 300 };
		cam.yaw = 90;		// looking along +Y
		const auto planes = build_frustum(cam, 16.0f / 9.0f);
		// eye relative points
		CHECK(sphere_visible(planes, { 0, 100, 0 }, 1));		// straight ahead
		CHECK(!sphere_visible(planes, { 0, -100, 0 }, 1));		// behind
		CHECK(sphere_visible(planes, { 0, -100, 0 }, 200));		// .. unless it is big enough
		CHECK(!sphere_visible(planes, { 1000, 100, 0 }, 1));	// far to the side
		CHECK(!sphere_visible(planes, { 0, 2, 0 }, 1));			// in front of the near plane ( z_near 4 )
		for (const auto& plane : planes)
		{
			CHECK(close_to(std::sqrt(plane.x * plane.x + plane.y * plane.y + plane.z * plane.z), 1.0f));
		}
	}

	void put32(std::vector<std::uint8_t>& data, const std::size_t at, const std::uint32_t value)
	{
		for (int i = 0; i < 4; i++)
		{
			data[at + static_cast<std::size_t>(i)] = static_cast<std::uint8_t>(value >> (8 * i));
		}
	}

	void test_streamed_surfaces()
	{
		// header 88 bytes; block 0: 16 bytes at 88 with one pointer into block 1; block 1: 8 bytes; block 2: 4 bytes; fixups: 1 entry
		std::vector<std::uint8_t> file(88 + 16 + 8 + 4 + 4);
		put32(file, 0, 1);
		const std::uint32_t sizes[4] = { 16, 8, 4, 4 };
		const std::uint32_t offsets[4] = { 88, 104, 112, 116 };
		for (std::size_t i = 0; i < 4; i++)
		{
			put32(file, 8 + 20 * i, sizes[i]);
			put32(file, 8 + 20 * i + 4, offsets[i]);
		}
		put32(file, 8 + 12, 1);				// block 0: fixup count 1 ( start 0 )
		put32(file, 88 + 4, 4);				// the pointer: offset 4 of its target block
		put32(file, 104 + 4, 0xCAFEBABE);
		put32(file, 116, (4u << 3) | 1u);	// fixup: location offset 4, target block 1
		streamed_surfaces mapped;
		std::string error;
		CHECK(mapped.map("test", file, &error));
		const auto root = mapped.bytes(mapped.root(), 16);
		CHECK(root.size() == 16);
		const std::uint32_t pointer = codol::render::rd_u32(root.data() + 4);
		const auto target = mapped.bytes(pointer, 4);
		CHECK(target.size() == 4 && codol::render::rd_u32(target.data()) == 0xCAFEBABE);
		CHECK(mapped.bytes(pointer, 8).empty());		// past the end of block 1
		CHECK(mapped.bytes(0, 4).empty());

		std::vector<std::uint8_t> bad = file;
		put32(bad, 0, 2);
		CHECK(!streamed_surfaces().map("version", bad, &error));
		bad = file;
		put32(bad, 116, (400u << 3) | 1u);		// fixup outside its block
		CHECK(!streamed_surfaces().map("fixup", bad, &error));
		bad.resize(40);
		CHECK(!streamed_surfaces().map("short", bad, &error));
	}

	void test_sun_shadow()
	{
		camera cam;
		cam.origin = { 10, 20, 30 };
		cam.yaw = 30;
		codol::render::renderer_settings settings;
		const float3 sun{ 0.48f, 0.6f, -0.64f };
		const sun_shadow shadow = compute_sun_shadow(cam, 16.0f / 9.0f, sun, { 0, 0, 0 }, { 4000, 4000, 1000 }, settings);
		CHECK(shadow.map_size == 1024 && shadow.partition_count == 2);
		// GameSunShadow.cs:147: PixelAdjust
		CHECK(close_to(shadow.pixel_adjust.x, 0.25f / 1024) && close_to(shadow.pixel_adjust.y, 0.5f / 2048) && close_to(shadow.pixel_adjust.w, -0.25f / 2048));
		CHECK(close_to(shadow.scale.x, 16) && close_to(shadow.scale.y, 32) && close_to(shadow.scale.z, -8) && close_to(shadow.scale.w, -24));
		// the far partition covers four times the near one
		CHECK(close_to(shadow.partitions[0].projection.m[0][0], 4 * shadow.partitions[1].projection.m[0][0], 1e-6f));
		// a point at the eye lands inside the near partition's clip square and between the depth planes
		const float4 clip = codol::render::transform({ cam.origin.x, cam.origin.y, cam.origin.z, 1 }, shadow.partitions[0].view_projection);
		CHECK(std::fabs(clip.x) <= 1 && std::fabs(clip.y) <= 1 && clip.z > 0 && clip.z < 1 && close_to(clip.w, 1));
		// depth grows along the light direction
		const float4 further = codol::render::transform({ cam.origin.x + sun.x * -100, cam.origin.y + sun.y * -100, cam.origin.z + sun.z * -100, 1 },
			shadow.partitions[0].view_projection);
		CHECK(further.z != clip.z);
	}

	void test_math()
	{
		mat4 m = mat4::translation({ 1, 2, 3 });
		const float3 p = transform_point({ 10, 20, 30 }, m);
		CHECK(p.x == 11 && p.y == 22 && p.z == 33);
		CHECK(close_to(length(cross({ 1, 0, 0 }, { 0, 1, 0 }) - float3{ 0, 0, 1 }), 0));
	}
}

int main()
{
	test_entities();
	test_zone_names();
	test_frustum();
	test_streamed_surfaces();
	test_sun_shadow();
	test_math();
	std::printf("%d checks, %d failures\n", checks, failures);
	return failures == 0 ? 0 : 1;
}
