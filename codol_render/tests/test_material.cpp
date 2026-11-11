#include "test.hpp"

#include "codol_render/material.hpp"

// The layouts codol_render reads by offset are those of this repository's src/codol/c1_layouts.hpp; check every
// offset of build_1_1_1_13.hpp against that header at compile time.
// ( Skipped when codol_render is built outside this repository. )
#include <cstddef>
#include <cstdint>
#if __has_include("../../src/codol/c1_layouts.hpp")
#include "../../src/codol/c1_layouts.hpp"
#define CODOL_RENDER_HAS_C1_LAYOUTS
#endif

using namespace codol::render;

#ifdef CODOL_RENDER_HAS_C1_LAYOUTS
namespace layout_check
{
	namespace c1 = codol::c1;
	namespace b = codol::render::build;

	static_assert(b::TECHNIQUE_COUNT == c1::MATERIAL_TECHNIQUE_COUNT);
	static_assert(sizeof(c1::Material) == b::MATERIAL_SIZE);
	static_assert(offsetof(c1::Material, name) == b::MATERIAL_NAME);
	static_assert(offsetof(c1::Material, gameFlags) == b::MATERIAL_GAME_FLAGS);
	static_assert(offsetof(c1::Material, sortKey) == b::MATERIAL_SORT_KEY);
	static_assert(offsetof(c1::Material, surfaceTypeBits) == b::MATERIAL_SURFACE_TYPE_BITS);
	static_assert(offsetof(c1::Material, stateBitsEntry) == b::MATERIAL_STATE_BITS_ENTRY);
	static_assert(offsetof(c1::Material, textureCount) == b::MATERIAL_TEXTURE_COUNT);
	static_assert(offsetof(c1::Material, constantCount) == b::MATERIAL_CONSTANT_COUNT);
	static_assert(offsetof(c1::Material, stateBitsCount) == b::MATERIAL_STATE_BITS_COUNT);
	static_assert(offsetof(c1::Material, cameraRegion) == b::MATERIAL_CAMERA_REGION);
	static_assert(offsetof(c1::Material, techniqueSet) == b::MATERIAL_TECHNIQUE_SET);
	static_assert(offsetof(c1::Material, textureTable) == b::MATERIAL_TEXTURE_TABLE);
	static_assert(offsetof(c1::Material, constantTable) == b::MATERIAL_CONSTANT_TABLE);
	static_assert(offsetof(c1::Material, stateBitsTable) == b::MATERIAL_STATE_BITS_TABLE);

	static_assert(sizeof(c1::MaterialTextureDef) == b::TEXTURE_DEF_SIZE);
	static_assert(offsetof(c1::MaterialTextureDef, nameHash) == b::TEXTURE_DEF_NAME_HASH);
	static_assert(offsetof(c1::MaterialTextureDef, samplerState) == b::TEXTURE_DEF_SAMPLER_STATE);
	static_assert(offsetof(c1::MaterialTextureDef, semantic) == b::TEXTURE_DEF_SEMANTIC);
	static_assert(offsetof(c1::MaterialTextureDef, image) == b::TEXTURE_DEF_IMAGE);
	static_assert(sizeof(c1::MaterialConstantDef) == b::CONSTANT_DEF_SIZE);
	static_assert(offsetof(c1::MaterialConstantDef, name) == b::CONSTANT_DEF_NAME);
	static_assert(offsetof(c1::MaterialConstantDef, literal) == b::CONSTANT_DEF_LITERAL);

	static_assert(sizeof(c1::MaterialTechniqueSet) == b::TECHSET_SIZE);
	static_assert(offsetof(c1::MaterialTechniqueSet, worldVertFormat) == b::TECHSET_WORLD_VERT_FORMAT);
	static_assert(offsetof(c1::MaterialTechniqueSet, techniques) == b::TECHSET_TECHNIQUES);
	static_assert(offsetof(c1::MaterialTechnique, flags) == b::TECHNIQUE_FLAGS);
	static_assert(offsetof(c1::MaterialTechnique, passCount) == b::TECHNIQUE_PASS_COUNT);
	static_assert(offsetof(c1::MaterialTechnique, passArray) == b::TECHNIQUE_PASSES);
	static_assert(sizeof(c1::MaterialPass) == b::PASS_SIZE);
	static_assert(offsetof(c1::MaterialPass, vertexDecl) == b::PASS_VERTEX_DECL);
	static_assert(offsetof(c1::MaterialPass, vertexShader) == b::PASS_VERTEX_SHADER);
	static_assert(offsetof(c1::MaterialPass, pixelShader) == b::PASS_PIXEL_SHADER);
	static_assert(offsetof(c1::MaterialPass, perPrimArgCount) == b::PASS_PER_PRIM_ARG_COUNT);
	static_assert(offsetof(c1::MaterialPass, perObjArgCount) == b::PASS_PER_OBJ_ARG_COUNT);
	static_assert(offsetof(c1::MaterialPass, stableArgCount) == b::PASS_STABLE_ARG_COUNT);
	static_assert(offsetof(c1::MaterialPass, customSamplerFlags) == b::PASS_CUSTOM_SAMPLER_FLAGS);
	static_assert(offsetof(c1::MaterialPass, args) == b::PASS_ARGS);
	static_assert(sizeof(c1::MaterialShaderArgument) == b::ARG_SIZE);
	static_assert(offsetof(c1::MaterialShaderArgument, dest) == b::ARG_DEST);
	static_assert(offsetof(c1::MaterialShaderArgument, value) == b::ARG_VALUE);
	static_assert(sizeof(c1::MaterialShader) == b::SHADER_SIZE);
	static_assert(offsetof(c1::MaterialShader, program) == b::SHADER_PROGRAM);
	static_assert(offsetof(c1::MaterialShader, programSize) == b::SHADER_PROGRAM_SIZE);
	static_assert(offsetof(c1::MaterialShader, loadForRenderer) == b::SHADER_LOAD_FOR_RENDERER);
	static_assert(sizeof(c1::MaterialVertexDeclaration) == b::VERTEX_DECL_SIZE);
	static_assert(offsetof(c1::MaterialVertexDeclaration, streamCount) == b::VERTEX_DECL_STREAM_COUNT);
	static_assert(offsetof(c1::MaterialVertexDeclaration, hasOptionalSource) == b::VERTEX_DECL_HAS_OPTIONAL_SOURCE);
	static_assert(offsetof(c1::MaterialVertexDeclaration, routing) == b::VERTEX_DECL_ROUTING);
	static_assert(c1::MATERIAL_VERTEX_STREAM_MAX == b::VERTEX_DECL_MAX_STREAMS);

	static_assert(sizeof(c1::GfxImage) == b::IMAGE_SIZE);
	static_assert(offsetof(c1::GfxImage, loadDef) == b::IMAGE_LOAD_DEF);
	static_assert(offsetof(c1::GfxImage, mapType) == b::IMAGE_MAP_TYPE);
	static_assert(offsetof(c1::GfxImage, width) == b::IMAGE_WIDTH);
	static_assert(offsetof(c1::GfxImage, height) == b::IMAGE_HEIGHT);
	static_assert(offsetof(c1::GfxImage, depth) == b::IMAGE_DEPTH);
	static_assert(offsetof(c1::GfxImage, name) == b::IMAGE_NAME);
	static_assert(sizeof(c1::GfxImageLoadDef) == b::LOAD_DEF_HEADER_SIZE);
	static_assert(offsetof(c1::GfxImageLoadDef, flags) == b::LOAD_DEF_FLAGS);
	static_assert(offsetof(c1::GfxImageLoadDef, format) == b::LOAD_DEF_FORMAT);
	static_assert(offsetof(c1::GfxImageLoadDef, resourceSize) == b::LOAD_DEF_RESOURCE_SIZE);
}
#endif

namespace
{
	constexpr std::uint32_t BASE = 0x40000000;

	struct image
	{
		test::bytes b;

		std::uint32_t here() const { return BASE + static_cast<std::uint32_t>(b.size()); }
		std::uint32_t str(const char* text) { const auto p = here(); b.str(text); return p; }
	};
}

void test_material()
{
	// ---- name hashes ( MaterialAsset.cs:88-96 ); the values are the well known IW sampler hashes ---------
	CHECK_EQ(hash_sampler_name("colorMap"), 0xA0AB1041u);
	CHECK_EQ(hash_sampler_name("normalMap"), 0x59D30D0Fu);
	CHECK_EQ(hash_sampler_name("specularMap"), 0x34ECCCB3u);
	CHECK_EQ(hash_sampler_name("detailMap"), 0xEB529B4Du);
	CHECK_EQ(hash_sampler_name("COLORMAP"), 0xA0AB1041u);		// case insensitive
	CHECK_EQ(hash_sampler_name(""), 0u);
	CHECK_EQ(hash_sampler_name("a"), 0x61u);
	CHECK_EQ(hash_sampler_name("ab"), 0x62u ^ (0x61u * 33));
	CHECK(sampler_name(0xA0AB1041) == "colorMap");
	CHECK(sampler_name(0x12345678) == "hash_12345678");
	CHECK(texture_semantic_name(2) == "color");
	CHECK(texture_semantic_name(11) == "water");
	CHECK(texture_semantic_name(4) == "semantic4");

	// ---- a material in a flat image -------------------------------------------------------------------------
	image z;
	z.b.zeros(4);

	// images: one streamed ( no load def ), one with 16 bytes of embedded pixels, one that is a reference
	const std::uint32_t img0_name = z.str("me_wall_col");
	const std::uint32_t img0 = z.here();
	z.b.u32(0).u8(3).u8(2).u8(9).zeros(13).u16(512).u16(256).u16(1).u8(0).u8(2).zeros(4).u32(img0_name);
	CHECK_EQ(z.here() - img0, 0x24u);

	const std::uint32_t load_def = z.here();
	z.b.u8(1).zeros(3).u32(0x3).u32(21).u32(16);						// levelCount 1, flags, D3DFMT_A8R8G8B8, 16 bytes
	for (int i = 0; i < 16; i++)
	{
		z.b.u8(static_cast<std::uint8_t>(i));
	}
	const std::uint32_t img1_name = z.str("$identitynormalmap");
	const std::uint32_t img1 = z.here();
	z.b.u32(load_def).u8(3).u8(5).u8(1).zeros(13).u16(2).u16(2).u16(0).u8(0).u8(0).zeros(4).u32(img1_name);

	const std::uint32_t img2_name = z.str(",me_shared_spc");
	const std::uint32_t img2 = z.here();
	z.b.u32(0).u8(3).u8(8).u8(9).zeros(13).u16(0).u16(0).u16(0).u8(0).u8(0).zeros(4).u32(img2_name);

	// texture table: colorMap, normalMap, specularMap, a water map ( pointer is not a GfxImage ), a null image
	const std::uint32_t textures = z.here();
	z.b.u32(0xA0AB1041).u8('c').u8('p').u8(0x12).u8(2).u32(img0);
	z.b.u32(0x59D30D0F).u8('n').u8('p').u8(0x62).u8(5).u32(img1);
	z.b.u32(0x34ECCCB3).u8('s').u8('p').u8(0x0B).u8(8).u32(img2);
	z.b.u32(0x11111111).u8('w').u8('p').u8(0x01).u8(11).u32(img0);
	z.b.u32(0x22222222).u8('x').u8('y').u8(0x02).u8(0).u32(0);

	// constant table: a 12 character name without terminator and a short one
	const std::uint32_t constants = z.here();
	z.b.u32(0xDEAD0001).raw("envMapParmsX", 12).f32(1.0f).f32(2.0f).f32(3.0f).f32(4.0f);
	z.b.u32(0xDEAD0002).raw("colorTint\0\0\0", 12).f32(0.25f).f32(0.5f).f32(0.75f).f32(1.0f);

	// state bits table: 3 entries
	const std::uint32_t state_bits = z.here();
	z.b.u32(0x18000865).u32(0x00000001);		// opaque
	z.b.u32(0x18000965).u32(0x00000000);		// blended ( op 1 )
	z.b.u32(0x00000800).u32(0x00000033);

	const std::uint32_t techset_name = z.str(",wc_l_sm_r0c0n0s0");
	const std::uint32_t techset = z.here();
	z.b.u32(techset_name).zeros(0xCC - 4);

	const std::uint32_t name = z.str("me_wall_plaster");
	const std::uint32_t material_ptr = z.here();
	z.b.u32(name).u8(0x5A).u8(4).zeros(10).u32(0x00100000).zeros(4);		// name, gameFlags, sortKey, .., surfaceTypeBits
	const std::size_t entries = z.b.size();
	for (int i = 0; i < 48; i++)
	{
		z.b.u8(0xFF);
	}
	z.b.put_u8(entries + 4, 0);		// unlit: entry 0
	z.b.put_u8(entries + 10, 1);	// lit dfog: entries 1 ( pass 0 ) and 2 ( pass 1 )
	z.b.put_u8(entries + 20, 7);	// an entry past the table
	z.b.u8(5).u8(2).u8(3).u8(0x21).u8(0).zeros(3);							// textureCount, constantCount, stateBitsCount, stateFlags, cameraRegion
	z.b.u32(techset).u32(textures).u32(constants).u32(state_bits);
	CHECK_EQ(z.here() - material_ptr, 0x60u);

	const zone_view zone = flat_zone_view(z.b.data, BASE);
	material m;
	std::string error;
	CHECK(read_material(zone, material_ptr, m, &error));
	CHECK(m.name == "me_wall_plaster");
	CHECK(!m.is_reference);
	CHECK_EQ(m.game_flags, 0x5A);
	CHECK_EQ(m.sort_key, 4);
	CHECK_EQ(m.camera_region, 0);
	CHECK_EQ(m.state_flags, 0x21);
	CHECK_EQ(m.surface_type_bits, 0x00100000u);
	CHECK_EQ(m.technique_set, techset);
	CHECK(m.technique_set_name == "wc_l_sm_r0c0n0s0");		// MaterialExtractor.cs:15 trims the reference comma

	CHECK_EQ(m.textures.size(), 5u);
	if (m.textures.size() == 5)
	{
		const auto& color = m.textures[0];
		CHECK_EQ(color.name_hash, 0xA0AB1041u);
		CHECK_EQ(color.semantic, 2);
		CHECK_EQ(color.sampler_state, 0x12);
		CHECK(color.has_image && color.image_name == "me_wall_col");
		CHECK(color.width == 512 && color.height == 256 && color.map_type == 3);
		CHECK(!color.embedded.has_value());

		const auto& normal = m.textures[1];
		CHECK(normal.has_image && normal.image_name == "$identitynormalmap");
		CHECK(normal.embedded.has_value());
		if (normal.embedded)
		{
			CHECK_EQ(normal.embedded->d3d_format, 21u);
			CHECK_EQ(normal.embedded->level_count, 1);
			CHECK_EQ(normal.embedded->flags, 3u);
			CHECK(normal.embedded->width == 2 && normal.embedded->height == 2);
			CHECK_EQ(normal.embedded->depth, 1);		// a stored depth of 0 counts as 1
			CHECK_EQ(normal.embedded->data, load_def + 16);
			CHECK_EQ(normal.embedded->data_size, 16u);
			const byte_span pixels = zone.read(normal.embedded->data, normal.embedded->data_size);
			CHECK(pixels.size() == 16 && pixels[0] == 0 && pixels[15] == 15);
		}

		CHECK(m.textures[2].has_image && m.textures[2].image_name == "me_shared_spc");
		CHECK_EQ(m.textures[2].sampler_state, 0x0B);

		// MaterialExtractor.cs:84: a water map has no image even though its pointer is set
		CHECK(m.textures[3].semantic == texture_semantic::WATER_MAP && !m.textures[3].has_image && m.textures[3].image_name.empty());
		CHECK_EQ(m.textures[3].image, img0);
		CHECK(!m.textures[4].has_image);
	}
	CHECK(m.find_texture(0x59D30D0F) == &m.textures[1]);
	CHECK(m.find_texture(0x99999999) == nullptr);
	CHECK(m.find_by_semantic(texture_semantic::SPECULAR_MAP) == &m.textures[2]);

	CHECK_EQ(m.constants.size(), 2u);
	if (m.constants.size() == 2)
	{
		CHECK(m.constants[0].name == "envMapParmsX");		// 12 characters, no terminator
		CHECK_EQ(m.constants[0].name_hash, 0xDEAD0001u);
		CHECK(m.constants[0].value[0] == 1.0f && m.constants[0].value[3] == 4.0f);
		CHECK(m.constants[1].name == "colorTint");
		CHECK(m.constants[1].value[1] == 0.5f && m.constants[1].value[2] == 0.75f);
		CHECK(m.find_constant(0xDEAD0002) == &m.constants[1]);
		CHECK(m.find_constant(1) == nullptr);
	}

	CHECK(m.state.has_value());
	if (m.state)
	{
		const material_state& s = *m.state;
		CHECK_EQ(s.sort_key, 4);
		CHECK_EQ(s.camera_region, 0);
		CHECK(!s.not_drawn());
		CHECK_EQ(s.table.size(), 3u);
		CHECK(s.entries[4] == 0 && s.entries[10] == 1 && s.entries[0] == 0xFF);
		CHECK(s.table[1].bits0 == 0x18000965 && s.table[2].bits1 == 0x33);
		// GameMapRenderer.cs:1462-1474: pass p of a technique uses table[ entry + p ]
		CHECK(s.pass_bits(4, 0) == &s.table[0]);
		CHECK(s.pass_bits(10, 0) == &s.table[1]);
		CHECK(s.pass_bits(10, 1) == &s.table[2]);
		CHECK(s.pass_bits(10, 2) == nullptr);
		CHECK(s.pass_bits(0, 0) == nullptr);		// 0xFF
		CHECK(s.pass_bits(20, 0) == nullptr);		// entry past the table
		CHECK(s.pass_bits(48, 0) == nullptr);
		// GameMapRenderer.cs:1353-1361
		CHECK(!s.blends(4));
		CHECK(s.blends(10));
		CHECK(!s.blends(0));
	}

	// the rule that a camera region >= 4 is not drawn ( MaterialAsset.cs:35 )
	for (int region = 0; region < 8; region++)
	{
		auto copy = z.b;
		copy.put_u8(material_ptr - BASE + 76, static_cast<std::uint8_t>(region));
		material r;
		CHECK(read_material(flat_zone_view(copy.data, BASE), material_ptr, r));
		CHECK(r.state.has_value() && r.state->camera_region == region && r.state->not_drawn() == (region >= 4));
	}

	// no state bits table -> no state ( MaterialExtractor.cs:64-67 ); unloaded tables -> empty lists
	{
		auto copy = z.b;
		copy.put_u8(material_ptr - BASE + 74, 0);
		copy.put_u32(material_ptr - BASE + 84, 0x7FFFFFF0);		// texture table not loaded
		copy.put_u32(material_ptr - BASE + 88, 0);				// no constant table
		copy.put_u32(material_ptr - BASE + 80, 0);				// no techset
		material r;
		CHECK(read_material(flat_zone_view(copy.data, BASE), material_ptr, r));
		CHECK(!r.state.has_value() && r.textures.empty() && r.constants.empty());
		CHECK(r.technique_set == 0 && r.technique_set_name.empty());
	}

	// a material that is a reference into another zone
	{
		image y;
		y.b.zeros(4);
		const std::uint32_t ref_name = y.str(",mc/mtl_shared");
		const std::uint32_t ref = y.here();
		y.b.u32(ref_name).zeros(0x60 - 4);
		material r;
		CHECK(read_material(flat_zone_view(y.b.data, BASE), ref, r));
		CHECK(r.is_reference && r.name == "mc/mtl_shared");
		CHECK(!read_material(flat_zone_view(y.b.data, BASE), 0, r, &error));
		CHECK(!read_material(flat_zone_view(y.b.data, BASE), 0x12345678, r, &error));
	}

	// read_image on its own
	{
		const auto info = read_image(zone, img1);
		CHECK(info.has_value());
		if (info)
		{
			CHECK(info->name == "$identitynormalmap" && info->width == 2 && info->height == 2 && info->embedded.has_value());
		}
		const auto shared = read_image(zone, img2);
		CHECK(shared && shared->is_reference && shared->name == "me_shared_spc");
		CHECK(!read_image(zone, 0).has_value());
	}
}
