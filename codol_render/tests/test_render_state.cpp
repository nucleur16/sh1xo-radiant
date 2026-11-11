#include "test.hpp"

#include "codol_render/common.hpp"
#include "codol_render/render_state.hpp"

using namespace codol::render;

namespace
{
	constexpr render_state_value rs(const std::uint32_t state, const std::uint32_t value) { return { state, value }; }

	bool same(const std::vector<render_state_value>& got, const std::vector<render_state_value>& expected)
	{
		if (got == expected)
		{
			return true;
		}
		std::printf("  got %zu states:", got.size());
		for (const auto& s : got)
		{
			std::printf(" (%u,0x%X)", s.state, s.value);
		}
		std::printf("\n");
		return false;
	}
}

void test_render_state()
{
	// Every expectation below is GfxStateBits.Translate ( GfxStateTables.cs:125-215 ) evaluated by hand.
	const renderer_settings defaults;
	CHECK(defaults.polygon_offset_scale == -0.5f && defaults.polygon_offset_bias == -0.04f);
	CHECK(defaults.shadow_map_polygon_offset_scale == 1.5f && defaults.shadow_map_polygon_offset_bias == 4.0f);
	CHECK_EQ(defaults.texture_filter_anisotropy_max, 16);
	CHECK_EQ(defaults.texture_filter_anisotropy_min, 1);
	CHECK(defaults.specular_color_scale == 2.5f && defaults.outdoor_feather == 8.0f);

	// opaque: colour + alpha write, no alpha test, no culling, depth write + test ALWAYS, no offset, no stencil.
	// offset 0: bias = -0.04 * 0 / 65536 = -0.0 and scale = 0 * -0.5 = -0.0 ( negative zero bits )
	{
		const auto states = translate_state_bits(0x18000000 | 0x800 | 0x4000, 0x1);
		const std::vector<render_state_value> expected =
		{
			{ 168, 15 }, { 15, 0 }, { 22, 1 }, { 8, 3 }, { 194, 0 }, { 27, 0 },
			{ 14, 1 }, { 7, 1 }, { 23, 8 }, { 175, 0x80000000 }, { 195, 0x80000000 }, { 52, 0 },
		};
		CHECK(same(states, expected));
		CHECK(!state_bits_blend(0x18000000 | 0x800 | 0x4000));
	}

	// blended: SRCALPHA / INVSRCALPHA / ADD, alpha test GE 128, cull back ( CCW ), sRGB write, colour write only;
	// depth test off, polygon offset 2, stencil front ops fail 1, zfail 2, pass 3, func 7 mirrored to the back face
	{
		const std::uint32_t bits0 = 0x08000000 | 0x40000000 | 0x8000 | 0x100 | 0x60 | 0x5;
		const std::uint32_t bits1 = 0x2 | 0x20 | 0x40 | 0x100 | 0x1000 | 0xC000 | 0xE0000;
		CHECK_EQ(bits0, 0x48008165u);
		CHECK_EQ(bits1, 0xED162u);
		const auto states = translate_state_bits(bits0, bits1);
		const std::vector<render_state_value> expected =
		{
			{ 168, 7 }, { 15, 1 }, { 25, 7 }, { 24, 128 }, { 22, 3 }, { 8, 3 }, { 194, 1 },
			{ 27, 1 }, { 171, 1 }, { 19, 5 }, { 20, 6 }, { 206, 1 }, { 209, 1 }, { 207, 5 }, { 208, 6 },
			{ 14, 0 }, { 7, 0 },
			{ 175, float_bits(2.0f * -0.5f) }, { 195, float_bits(-0.04f * 2.0f / 65536.0f) },
			{ 52, 1 }, { 53, 2 }, { 54, 3 }, { 55, 4 }, { 56, 8 }, { 185, 1 }, { 186, 2 }, { 187, 3 }, { 188, 4 }, { 189, 8 },
		};
		CHECK(same(states, expected));
		CHECK(state_bits_blend(bits0));
		CHECK_EQ(float_bits(2.0f * -0.5f), 0xBF800000u);
	}

	// alpha test variants, wireframe, cull front, depth functions, separate back stencil
	{
		auto states = translate_state_bits(0x1000 | 0xC000 | 0x80000000, 0x4);		// GT 0, cull front, wireframe; depth LESS
		std::vector<render_state_value> expected =
		{
			{ 168, 0 }, { 15, 1 }, { 25, 5 }, { 24, 0 }, { 22, 2 }, { 8, 2 }, { 194, 0 }, { 27, 0 },
			{ 14, 0 }, { 7, 1 }, { 23, 2 }, { 175, 0x80000000 }, { 195, 0x80000000 }, { 52, 0 },
		};
		CHECK(same(states, expected));

		states = translate_state_bits(0x2000, 0x8 | 0x1);		// LT 128; cull index 0; depth EQUAL
		expected =
		{
			{ 168, 0 }, { 15, 1 }, { 25, 2 }, { 24, 128 }, { 22, 0 }, { 8, 3 }, { 194, 0 }, { 27, 0 },
			{ 14, 1 }, { 7, 1 }, { 23, 3 }, { 175, 0x80000000 }, { 195, 0x80000000 }, { 52, 0 },
		};
		CHECK(same(states, expected));

		// 0x3000 falls into the default branch ( GE 128 ); depth LESSEQUAL; alpha write only
		states = translate_state_bits(0x3000 | 0x10000000, 0xC);
		CHECK(states.size() == 14 && states[0] == rs(168, 8) && states[2] == rs(25, 7)
			&& states[10] == rs(23, 4));

		// separate back face stencil ( 0x80 ): front 0/0/0/0 -> 1, back fail 2 ( bits 20-22 ), zfail 3, pass 4, func 5 ( bits 29-31 )
		const std::uint32_t bits1 = 0x40 | 0x80 | (2u << 20) | (3u << 23) | (4u << 26) | (5u << 29);
		states = translate_state_bits(0x800, bits1);
		expected =
		{
			{ 168, 0 }, { 15, 0 }, { 22, 0 }, { 8, 3 }, { 194, 0 }, { 27, 0 },
			{ 14, 0 }, { 7, 1 }, { 23, 8 }, { 175, 0x80000000 }, { 195, 0x80000000 },
			{ 52, 1 }, { 53, 1 }, { 54, 1 }, { 55, 1 }, { 56, 1 }, { 185, 1 }, { 186, 3 }, { 187, 4 }, { 188, 5 }, { 189, 6 },
		};
		CHECK(same(states, expected));
	}

	// the alpha channel factors of colour factors ( GfxStateTables.cs:217-224 ), with and without separate alpha blend
	{
		auto states = translate_state_bits(0x800 | 0x100 | 3 | (10 << 4), 0);		// SRCCOLOR, INVDESTCOLOR
		CHECK(states.size() >= 12 && states[7] == rs(19, 3) && states[8] == rs(20, 10)
			&& states[11] == rs(207, 5) && states[12] == rs(208, 8));
		states = translate_state_bits(0x800 | 0x100 | 3 | (10 << 4), 0, defaults, false);
		CHECK(states.size() >= 12 && states[11] == rs(207, 3) && states[12] == rs(208, 10));
		states = translate_state_bits(0x800 | 0x500 | 4 | (9 << 4), 0);			// INVSRCCOLOR, DESTCOLOR, op 5 ( MAX )
		CHECK(states.size() >= 12 && states[6] == rs(171, 5) && states[10] == rs(209, 5)
			&& states[11] == rs(207, 6) && states[12] == rs(208, 7));
	}

	// polygon offset: 1 x, shadow map ( 3 ), and without slope scale support ( bias doubled, no D3DRS_SLOPESCALEDEPTHBIAS )
	{
		auto states = translate_state_bits(0x800, 0x10);
		CHECK(states.size() == 12 && states[9] == rs(175, float_bits(-0.5f))
			&& states[10] == rs(195, float_bits(-0.04f * 1.0f / 65536.0f)));
		states = translate_state_bits(0x800, 0x30);
		CHECK(states.size() == 12 && states[9] == rs(175, float_bits(1.5f))
			&& states[10] == rs(195, float_bits(4.0f / 65536.0f)));
		states = translate_state_bits(0x800, 0x30, defaults, true, false);
		CHECK(states.size() == 11 && states[9] == rs(195, float_bits(4.0f / 65536.0f * 2.0f)));
		renderer_settings custom;
		custom.polygon_offset_bias = -1.0f;
		custom.polygon_offset_scale = -2.0f;
		states = translate_state_bits(0x800, 0x20, custom);
		CHECK(states.size() == 12 && states[9] == rs(175, float_bits(-4.0f))
			&& states[10] == rs(195, float_bits(-2.0f / 65536.0f)));
	}

	CHECK(INITIAL_RENDER_STATES[0] == (rs(206, 1)));
	CHECK(INITIAL_RENDER_STATES[1] == (rs(185, 1)));
	CHECK(INITIAL_RENDER_STATES[2] == (rs(137, 0)));

	// ---- sampler states ( GfxStateTables.cs:242-272 ) -------------------------------------------------------
	{
		// 0x12 | 0xE0: linear, mip linear, clamp uvw ( the reflection probe sampler ); anisotropy min 1 -> plain linear
		auto s = translate_sampler_state(0xF2, false);
		CHECK(s == (sampler_state_values{ 2, 2, 2, 1, 3, 3, 3, false }));
		// 0x02 | 0x60: linear, no mips, clamp uv ( lightmaps )
		s = translate_sampler_state(0x62, true);
		CHECK(s == (sampler_state_values{ 2, 2, 0, 1, 3, 3, 1, true }));
		// 0x01 | 0x60: point ( shadow maps )
		s = translate_sampler_state(0x61, false);
		CHECK(s == (sampler_state_values{ 1, 1, 0, 1, 3, 3, 1, false }));
		// filter 0 behaves like point
		s = translate_sampler_state(0x00, false);
		CHECK(s == (sampler_state_values{ 1, 1, 0, 1, 1, 1, 1, false }));
		// anisotropic 2x / 4x with mip nearest / linear
		s = translate_sampler_state(0x0B, false);
		CHECK(s == (sampler_state_values{ 3, 3, 1, 2, 1, 1, 1, false }));
		s = translate_sampler_state(0x14, false);
		CHECK(s == (sampler_state_values{ 3, 3, 2, 4, 1, 1, 1, false }));
		// a device without anisotropy: linear, 1
		s = translate_sampler_state(0x14, false, defaults, 1);
		CHECK(s == (sampler_state_values{ 2, 2, 2, 1, 1, 1, 1, false }));
		// device limit 2 caps the 4x filter; anisotropic only for minification
		s = translate_sampler_state(0x14, false, defaults, 2, true, false);
		CHECK(s == (sampler_state_values{ 3, 2, 2, 2, 1, 1, 1, false }));

		// r_texFilterAnisoMin 8: linear filtered mipped textures become anisotropic 8, 2x / 4x are raised to 8
		renderer_settings aniso;
		aniso.texture_filter_anisotropy_min = 8;
		s = translate_sampler_state(0x12, false, aniso);
		CHECK(s == (sampler_state_values{ 3, 3, 2, 8, 1, 1, 1, false }));
		s = translate_sampler_state(0x13, false, aniso);
		CHECK(s == (sampler_state_values{ 3, 3, 2, 8, 1, 1, 1, false }));
		s = translate_sampler_state(0x02, false, aniso);		// no mips: stays linear, 1
		CHECK(s == (sampler_state_values{ 2, 2, 0, 1, 1, 1, 1, false }));

		// mip modes: 1 forces linear between mips, 2 forces nearest, 3 and "filter disable" turn mips off
		renderer_settings mode;
		mode.texture_filter_mip_mode = 1;
		CHECK_EQ(translate_sampler_state(0x0A, false, mode).mip_filter, 2u);
		mode.texture_filter_mip_mode = 2;
		CHECK_EQ(translate_sampler_state(0x12, false, mode).mip_filter, 1u);
		mode.texture_filter_mip_mode = 3;
		CHECK_EQ(translate_sampler_state(0x12, false, mode).mip_filter, 0u);
		mode.texture_filter_mip_mode = 0;
		mode.texture_filter_disable = true;
		s = translate_sampler_state(0x14, false, mode);
		CHECK(s == (sampler_state_values{ 1, 1, 0, 1, 1, 1, 1, false }));

		const auto pairs = translate_sampler_state(0xF2, true).states();
		CHECK(pairs[0].state == 6 && pairs[0].value == 2);
		CHECK(pairs[1].state == 5 && pairs[2].state == 7 && pairs[3].state == 10);
		CHECK(pairs[4].state == 1 && pairs[4].value == 3 && pairs[5].state == 2 && pairs[6].state == 3);
		CHECK(pairs[7].state == 11 && pairs[7].value == 1);
	}
}
