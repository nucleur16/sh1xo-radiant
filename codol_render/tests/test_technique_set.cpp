#include "test.hpp"

#include "codol_render/technique_set.hpp"

using namespace codol::render;

namespace
{
	constexpr std::uint32_t BASE = 0x10000;		// zone pointer of the first byte of the hand made image

	struct image
	{
		test::bytes b;

		std::uint32_t here() const { return BASE + static_cast<std::uint32_t>(b.size()); }
		std::uint32_t str(const char* text) { const auto p = here(); b.str(text); return p; }
	};
}

void test_technique_set()
{
	// ---- argument decoding ( TechniqueSet.cs:75-88 ) ------------------------------------------------------
	{
		shader_argument a;
		a.type = shader_argument_type::code_vertex_const;
		a.value = (2u << 24) | (1u << 16) | 107;
		CHECK_EQ(a.code_index(), 107);
		CHECK_EQ(a.first_row(), 1);
		CHECK_EQ(a.row_count(), 2);
		CHECK(a.is_vertex_stage() && a.is_code_const() && !a.is_sampler() && !a.is_literal());
		a.value = 7;
		CHECK_EQ(a.row_count(), 1);		// a row count of 0 means one row
		a.type = shader_argument_type::code_pixel_sampler;
		CHECK(!a.is_vertex_stage() && a.is_sampler());
		a.type = shader_argument_type::literal_pixel_const;
		CHECK(!a.is_vertex_stage() && a.is_literal());
		a.type = shader_argument_type::material_pixel_sampler;
		a.value = 0xA0AB1041;
		CHECK_EQ(a.name_hash(), 0xA0AB1041u);
	}

	// ---- technique types -------------------------------------------------------------------------------------
	CHECK_EQ(static_cast<int>(TECHNIQUE_COUNT), 48);
	CHECK_EQ(static_cast<int>(TECHNIQUE_LIT_DFOG), renderer_technique::LIT);
	CHECK_EQ(static_cast<int>(TECHNIQUE_LIT_SUN_DFOG), renderer_technique::LIT_SUN);
	CHECK_EQ(static_cast<int>(TECHNIQUE_LIT_SUN_SHADOW_DFOG), renderer_technique::LIT_SUN_SHADOW);
	CHECK_EQ(static_cast<int>(TECHNIQUE_LIT_SPOT_DFOG), renderer_technique::LIT_SPOT);
	CHECK_EQ(static_cast<int>(TECHNIQUE_LIT_SPOT_SHADOW_DFOG), renderer_technique::LIT_SPOT_SHADOW);
	CHECK_EQ(static_cast<int>(TECHNIQUE_LIT_OMNI_DFOG), renderer_technique::LIT_OMNI);
	CHECK_EQ(static_cast<int>(TECHNIQUE_LIT_OMNI_SHADOW_DFOG), renderer_technique::LIT_OMNI_SHADOW);
	CHECK_EQ(static_cast<int>(TECHNIQUE_BUILD_SHADOWMAP_COLOR), renderer_technique::BUILD_SHADOWMAP_COLOR);
	CHECK_EQ(static_cast<int>(TECHNIQUE_LIGHT_SPOT), 37);
	CHECK_EQ(static_cast<int>(TECHNIQUE_WIREFRAME_SOLID), 44);
	CHECK_EQ(static_cast<int>(TECHNIQUE_DEBUG_BUMPMAP), 46);
	CHECK(std::strcmp(technique_type_name(0), "depth prepass") == 0);
	CHECK(std::strcmp(technique_type_name(47), "debug bumpmap instanced") == 0);
	CHECK(std::strcmp(technique_type_name(48), "") == 0);

	// ---- vertex declaration names ( TechniqueSet.cs:31-73 ) ------------------------------------------------
	{
		const auto routing = parse_vertex_declaration_name("ppcc0t0t0nntt2n0t4n1t5");
		CHECK(routing.has_value());
		if (routing)
		{
			const std::vector<vertex_routing> expected = { { 0, 0 }, { 1, 2 }, { 2, 5 }, { 3, 1 }, { 4, 7 }, { 7, 9 }, { 8, 10 } };
			CHECK(*routing == expected);
		}
		const auto tangent = parse_vertex_declaration_name(",tt1t1t1t2t7pd");		// tangent->t1, texcoord1->t1, texcoord2->t7, position->depth
		CHECK(tangent.has_value());
		if (tangent)
		{
			const std::vector<vertex_routing> expected = { { 4, 6 }, { 5, 6 }, { 6, 12 }, { 0, 4 } };
			CHECK(*tangent == expected);
		}
		CHECK(!parse_vertex_declaration_name("").has_value());
		CHECK(!parse_vertex_declaration_name("xyz").has_value());
		CHECK(!parse_vertex_declaration_name("pp0").has_value());		// "p" "p" then a dangling "0"
		vertex_declaration decl;
		decl.routing = { { 0, 0 }, { 1, 2 }, { 2, 5 } };
		CHECK(decl.key() == "0:0,1:2,2:5");
	}

	// ---- a technique set in a flat image ------------------------------------------------------------------
	image z;
	z.b.zeros(16);		// keep offset 0 unused

	// vertex declaration ( 0x64 bytes ): name, streamCount 3, hasOptionalSource 1, routing p->p, c->c0, t0->t0
	const std::uint32_t decl_name = z.str("pp0cc0t0t0_test");
	const std::uint32_t decl = z.here();
	z.b.u32(decl_name).u8(3).u8(1).u16(0).u8(0).u8(0).u8(1).u8(2).u8(2).u8(5).zeros(0x64 - 14);

	// vertex shader with a 3 DWORD program, pixel shader that is a reference into another zone
	const std::uint32_t vs_name = z.str("vs_test.hlsl");
	const std::uint32_t vs_code = z.here();
	z.b.u32(0xFFFE0300).u32(0x0000FFFF).u32(0);
	const std::uint32_t vs = z.here();
	z.b.u32(vs_name).u32(0).u32(vs_code).u16(3).u16(1);
	const std::uint32_t ps_name = z.str(",ps_shared.hlsl");
	const std::uint32_t ps = z.here();
	z.b.u32(ps_name).u32(0).u32(0).u16(0).u16(0);

	// arguments of pass 0: one per primitive, one per object, three stable
	const std::uint32_t literal = z.here();
	z.b.f32(0.5f).f32(-1.0f).f32(2.0f).f32(4.0f);
	const std::uint32_t args = z.here();
	z.b.u16(3).u16(4).u32((4u << 24) | 107);			// code vertex const: worldViewProjection rows 0-3 -> c4
	z.b.u16(3).u16(8).u32((3u << 24) | (0u << 16) | 99);	// code vertex const: world rows 0-2 -> c8
	z.b.u16(2).u16(0).u32(0xA0AB1041);					// material pixel sampler colorMap -> s0
	z.b.u16(5).u16(1).u32(39);							// code pixel const 39 -> c1
	z.b.u16(7).u16(2).u32(literal);						// literal pixel const -> c2

	const std::uint32_t tech_a_name = z.str("lit_test");
	const std::uint32_t tech_a = z.here();
	z.b.u32(tech_a_name).u16(0x0004).u16(2);
	z.b.u32(decl).u32(vs).u32(ps).u8(1).u8(1).u8(3).u8(0x06).u32(args);		// pass 0
	z.b.u32(0).u32(vs).u32(0).u8(0).u8(0).u8(0).u8(0).u32(0);				// pass 1: no declaration, no pixel shader, no arguments

	const std::uint32_t tech_b = z.here();									// a technique without a name and without passes
	z.b.u32(0).u16(0).u16(0);

	const std::uint32_t set_name = z.str("wc_l_test");
	const std::uint32_t set = z.here();
	z.b.u32(set_name).u8(3).zeros(7);
	const std::size_t slots = z.b.size();
	z.b.zeros(48 * 4);
	z.b.put_u32(slots + 4 * 9, tech_a);
	z.b.put_u32(slots + 4 * 10, tech_a);			// same technique in two slots
	z.b.put_u32(slots + 4 * 4, tech_b);
	z.b.put_u32(slots + 4 * 20, 0xFFFFFFFF);		// a pointer the loader never resolved: slot stays empty
	CHECK_EQ(z.b.size() - (set - BASE), 0xCCu);

	const std::uint32_t ref_name = z.str(",mc_l_shared");
	const std::uint32_t ref_set = z.here();
	z.b.u32(ref_name).zeros(0xCC - 4);

	const zone_view zone = flat_zone_view(z.b.data, BASE);

	render_technique_set ts;
	std::string error;
	CHECK(read_technique_set(zone, set, ts, &error));
	CHECK(ts.name == "wc_l_test");
	CHECK_EQ(ts.world_vert_format, 3);
	CHECK_EQ(ts.techniques.size(), 2u);
	CHECK(ts.has(9) && ts.has(10) && ts.has(4) && !ts.has(20) && !ts.has(0) && !ts.has(48) && !ts.has(-1));
	CHECK(ts.technique(9) == ts.technique(10));
	CHECK_EQ(ts.slot_mask(), (1ull << 4) | (1ull << 9) | (1ull << 10));

	const render_technique* lit = ts.technique(10);
	CHECK(lit != nullptr);
	if (lit)
	{
		CHECK(lit->name == "lit_test");
		CHECK_EQ(lit->flags, 4);
		CHECK_EQ(lit->passes.size(), 2u);
		const render_pass& p0 = lit->passes[0];
		CHECK_EQ(p0.per_prim_arg_count, 1);
		CHECK_EQ(p0.per_obj_arg_count, 1);
		CHECK_EQ(p0.stable_arg_count, 3);
		CHECK_EQ(p0.custom_sampler_flags, 6);
		CHECK_EQ(p0.arguments.size(), 5u);
		if (p0.arguments.size() == 5)
		{
			CHECK(p0.arguments[0].type == shader_argument_type::code_vertex_const);
			CHECK_EQ(p0.arguments[0].dest, 4);
			CHECK_EQ(p0.arguments[0].code_index(), 107);
			CHECK_EQ(p0.arguments[0].row_count(), 4);
			CHECK_EQ(p0.arguments[1].code_index(), 99);
			CHECK_EQ(p0.arguments[1].row_count(), 3);
			CHECK(p0.arguments[2].type == shader_argument_type::material_pixel_sampler);
			CHECK_EQ(p0.arguments[2].name_hash(), 0xA0AB1041u);
			CHECK(p0.arguments[3].type == shader_argument_type::code_pixel_const);
			CHECK_EQ(p0.arguments[3].code_index(), 39);
			CHECK(p0.arguments[4].type == shader_argument_type::literal_pixel_const);
			CHECK_EQ(p0.arguments[4].dest, 2);
			CHECK(p0.arguments[4].literal[0] == 0.5f && p0.arguments[4].literal[1] == -1.0f && p0.arguments[4].literal[2] == 2.0f
				&& p0.arguments[4].literal[3] == 4.0f);
			CHECK(p0.arguments[0].literal[0] == 0.0f);
		}
		CHECK(p0.declaration.has_value());
		if (p0.declaration)
		{
			CHECK(p0.declaration->name == "pp0cc0t0t0_test");
			CHECK(!p0.declaration->is_reference);
			CHECK(p0.declaration->has_optional_source);
			const std::vector<vertex_routing> expected = { { 0, 0 }, { 1, 2 }, { 2, 5 } };
			CHECK(p0.declaration->routing == expected);
		}
		CHECK(p0.vertex_shader.has_value());
		if (p0.vertex_shader)
		{
			CHECK(p0.vertex_shader->name == "vs_test.hlsl");
			CHECK(p0.vertex_shader->has_program());
			CHECK_EQ(p0.vertex_shader->program, vs_code);
			CHECK_EQ(p0.vertex_shader->program_size, 12u);
			CHECK_EQ(shader_code(zone, *p0.vertex_shader).size(), 12u);
			CHECK(!p0.vertex_shader->is_pixel_shader);
		}
		CHECK(p0.pixel_shader.has_value());
		if (p0.pixel_shader)
		{
			CHECK(p0.pixel_shader->name == "ps_shared.hlsl");
			CHECK(p0.pixel_shader->is_reference);
			CHECK(!p0.pixel_shader->has_program());
			CHECK(p0.pixel_shader->is_pixel_shader);
		}
		const render_pass& p1 = lit->passes[1];
		CHECK(!p1.declaration.has_value());
		CHECK(p1.vertex_shader.has_value());
		CHECK(!p1.pixel_shader.has_value());
		CHECK(p1.arguments.empty());
	}
	const render_technique* unlit = ts.technique(4);
	CHECK(unlit != nullptr);
	if (unlit)
	{
		char expected[32];
		std::snprintf(expected, sizeof expected, "technique_%08X", tech_b);
		CHECK(unlit->name == expected);
		CHECK(unlit->passes.empty());
	}

	// a techset stored as a reference keeps its name with the comma and has no techniques
	render_technique_set reference;
	CHECK(read_technique_set(zone, ref_set, reference, &error));
	CHECK(reference.name == ",mc_l_shared");
	CHECK(reference.techniques.empty());
	CHECK_EQ(reference.slot_mask(), 0ull);

	// not loaded
	CHECK(!read_technique_set(zone, 0, ts, &error));
	CHECK(!read_technique_set(zone, 0x7FFFFFFF, ts, &error));
	CHECK(!read_technique_set(zone, BASE + static_cast<std::uint32_t>(z.b.size()) - 8, ts, &error));	// header runs off the image

	// a referenced declaration takes its routing from its name ( RenderAssetLibrary.cs:536-549 )
	{
		image y;
		y.b.zeros(8);
		const std::uint32_t name = y.str(",ppnnt0t0");
		const std::uint32_t d = y.here();
		y.b.u32(name).zeros(0x64 - 4);
		const std::uint32_t bad_name = y.str(",nonsense");
		const std::uint32_t bad = y.here();
		y.b.u32(bad_name).zeros(0x64 - 4);
		const zone_view view = flat_zone_view(y.b.data, BASE);
		const auto parsed = read_vertex_declaration(view, d);
		CHECK(parsed.has_value());
		if (parsed)
		{
			CHECK(parsed->is_reference);
			CHECK(parsed->name == "ppnnt0t0");
			const std::vector<vertex_routing> expected = { { 0, 0 }, { 3, 1 }, { 2, 5 } };
			CHECK(parsed->routing == expected);
		}
		CHECK(!read_vertex_declaration(view, bad).has_value());
		CHECK(!read_vertex_declaration(view, 0).has_value());
	}
}
