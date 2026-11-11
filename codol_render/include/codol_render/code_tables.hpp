#pragma once

// Index -> name tables of the 1.1.1.13 code constants and code samplers, the fixed sampler slots and the
// sampler states the C# renderer binds code samplers with.
//
// Sources: Codol.FastFiles\Iw5\Porting\Iw5ShaderTables.cs ( 1.1.1.13 = the IW4 numbering of OpenAssetTools'
// IW4_Assets.h CONST_SRC_CODE_* / TEXTURE_SRC_CODE_* ), Codol.FastFiles\Porting\V3ToV1\CodeConstantMap.cs
// ( "1.1.1.13 is the IW4 numbering with ssaoParms at 8, renderSourceSize at 24 and overlayTexCoord at 56 added,
// 79 float4 constants, the first code matrix at 79" ), GameMapRenderer.cs:1689-1738.
//
// `hlsl` is what the game's shaders call the register a code argument feeds ( the name in the shader's constant
// table, CTAB ); the C# renderer keys its float4 slots by that name. `name` is the same identifier made unique per
// index ( the elements of the HLSL arrays filterTap[8], particleCloudMatrix, codeMeshArg[2] get a number ), so that
// a draw loop can resolve a code constant from its index alone: code_constants::resolve( index ).
// `observed` tells whether tools/codol_render_check ( "names" ) saw the pairing in stock 1.1.1.13 zones ( MEASURED );
// the names of unobserved entries are the IW4 ones ( INFERRED ).
//
// MATRICES. Index FIRST_CODE_MATRIX + 4 * group + variant with the variants in the enum order matrix, inverse,
// transpose, inverse transpose. An HLSL float4x4 occupies four registers holding its COLUMNS, i.e. the rows of
// its transpose, so the shaders' "worldMatrix" is fed by TRANSPOSE_WORLD_MATRIX ( index 101 ), "inverseWorldViewMatrix"
// by INVERSE_TRANSPOSE_WORLD_VIEW_MATRIX ( 106 ), and so on - measured for 85, 89, 90, 93, 97, 101, 104, 105, 106, 109,
// 117 and 129. The table's names follow the shaders; `source` is the enum. code_constants::write produces the rows
// from the index, so nothing has to be transposed by the caller.

#include "build_1_1_1_13.hpp"

#include <cstdint>
#include <string_view>

namespace codol::render
{
	struct code_name
	{
		const char* name;			// unique per index
		const char* hlsl;			// register name in the shaders' constant tables ( "" when none was seen )
		const char* source;			// the IW4 enum name without the CONST_SRC_CODE_ / TEXTURE_SRC_CODE_ prefix
		bool observed;				// pairing confirmed against shader constant tables of the game's zones
	};

	// Code constant `index` ( shader_argument::code_index ), 0 .. CODE_CONSTANT_COUNT - 1; nullptr when out of range.
	const code_name* code_constant(int index);
	inline const char* code_constant_name(const int index) { const auto* c = code_constant(index); return c ? c->name : ""; }
	// by the unique `name`; -1 when there is none
	int code_constant_index(std::string_view name);

	// Code sampler `index` ( the value of a CodePixelSampler argument ), 0 .. CODE_SAMPLER_COUNT - 1
	const code_name* code_sampler(int index);
	inline const char* code_sampler_name(const int index) { const auto* c = code_sampler(index); return c ? c->name : ""; }

	// TEXTURE_SRC_CODE_* indices the renderer treats specially
	namespace code_sampler_index
	{
		inline constexpr int BLACK = 0;
		inline constexpr int WHITE = 1;
		inline constexpr int IDENTITY_NORMAL_MAP = 2;
		inline constexpr int MODEL_LIGHTING = 3;
		inline constexpr int LIGHTMAP_PRIMARY = 4;
		inline constexpr int LIGHTMAP_SECONDARY = 5;
		inline constexpr int SHADOWMAP_SUN = 6;
		inline constexpr int SHADOWMAP_SPOT = 7;
		inline constexpr int FEEDBACK = 8;				// GameMapRenderer.PostEffects.cs:207-213: post effect input
		inline constexpr int RESOLVED_POST_SUN = 9;		//   "
		inline constexpr int RESOLVED_SCENE = 10;		//   "
		inline constexpr int POST_EFFECT_0 = 11;
		inline constexpr int POST_EFFECT_1 = 12;
		inline constexpr int LIGHT_ATTENUATION = 13;
		inline constexpr int OUTDOOR = 14;
		inline constexpr int FLOATZ = 15;
		inline constexpr int REFLECTION_PROBE = 25;
	}

	// MaterialPass::customSamplerFlags ( GameMapRenderer.cs:1723-1738 ): samplers the pass reads from fixed
	// registers without an argument. The draw loop binds them per surface after the arguments.
	struct custom_sampler
	{
		std::uint8_t flag;				// bit of customSamplerFlags
		std::uint8_t sampler_register;	// s#
		std::uint8_t sampler_state;		// see render_state.hpp sampler_bits
		const char* what;
		const char* fallback;			// the texture when the surface has none: "$black" / "$white"
	};
	inline constexpr custom_sampler CUSTOM_SAMPLERS[3] =
	{
		{ 1, 1, 0x12 | 0xE0, "reflection probe cube of the surface", "$black" },	// linear, mip linear, clamp uvw
		{ 2, 2, 0x02 | 0x60, "primary lightmap of the surface", "$white" },		// linear, no mips, clamp uv
		{ 4, 3, 0x02 | 0x60, "secondary lightmap of the surface", "$black" },
	};

	// The sampler state byte the C# renderer uses for a code sampler ( GameMapRenderer.cs:1689-1721 ), by the
	// name of the sampler register in the pixel shader's constant table:
	//   code index 8, 9, 10 ( post effect inputs )   0x02 | 0x60   linear, clamp uv
	//   "modelLightingSampler"                       0x02 | 0xE0   linear, clamp uvw ( volume texture )
	//   "attenuationSampler"                         the GfxLightDef's own sampler state byte
	//   "shadowmapSamplerSun" / "shadowmapSamplerSpot" / "floatZSampler"   0x01 | 0x60   point, clamp uv
	//   "outdoorMapSampler"                          0x02 | 0x60
	//   anything else: the white texture with 0x02 ( the C# renderer has no source for it )
	// Returns false for "anything else"; `light_def_sampler_state` is passed through for the attenuation sampler.
	bool code_sampler_state(int code_index, std::string_view ctab_name, std::uint8_t light_def_sampler_state, std::uint8_t& sampler_state);

	// A material sampler without a texture in the material is bound white with this state ( GameMapRenderer.cs:1586 )
	inline constexpr std::uint8_t DEFAULT_MATERIAL_SAMPLER_STATE = 0x12;

	// The 1x1 stand-in textures of GameTextureCache.cs:33-35, as D3DCOLOR ( A8R8G8B8 )
	inline constexpr std::uint32_t WHITE_ARGB = 0xFFFFFFFF;
	inline constexpr std::uint32_t BLACK_ARGB = 0xFF000000;
	inline constexpr std::uint32_t IDENTITY_NORMAL_ARGB = 0xFF7F7FFF;
}
