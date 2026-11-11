#pragma once

// GfxStateBits / sampler state bytes -> Direct3D 9 render and sampler states, and the renderer settings.
// Port of Codol.Assets\Rendering\GfxStateTables.cs:87-342.

#include "d3d9_constants.hpp"

#include <array>
#include <vector>

namespace codol::render
{
	// GfxStateTables.cs:275-342 ( RendererSettings ). The defaults are the game's dvar defaults the C# renderer uses.
	struct renderer_settings
	{
		int texture_filter_anisotropy_max = 16;
		int texture_filter_anisotropy_min = 1;
		int texture_filter_mip_mode = 0;				// 0 as authored, 1 force trilinear, 2 force bilinear, 3 no mips
		bool texture_filter_disable = false;
		float polygon_offset_scale = -0.5f;				// r_polygonOffsetScale
		float polygon_offset_bias = -0.04f;				// r_polygonOffsetBias
		float shadow_map_polygon_offset_scale = 1.5f;	// sm_polygonOffsetScale
		float shadow_map_polygon_offset_bias = 4.0f;	// sm_polygonOffsetBias
		float diffuse_color_scale = 1.0f;
		float specular_color_scale = 2.5f;
		bool post_effects = true;
		bool film_quadratic_tint = true;
		float film_contrast_scale = 1.0f;
		float film_brightness_offset = 0.0f;
		float film_desaturation_scale = 1.0f;
		bool glow = true;
		bool sun_shadows = true;
		int sun_shadow_map_size = 1024;
		float sun_shadow_sample_scale = 1.0f;
		float sun_shadow_sample_size_near = 0.25f;
		bool spot_shadows = true;
		bool cell_culling = true;
		int spot_shadow_limit = 4;
		float spot_shadow_min_score = 0.5f;
		float spot_shadow_fade_time = 1.0f;
		float spot_shadow_eye_bias = 64.0f;
		float spot_shadow_cone_bias = 0.125f;
		int spot_shadow_map_size = 512;
		int spot_shadow_tiles = 4;
		int sun_shadow_partitions = 2;
		float outdoor_feather = 8.0f;
		bool effects = true;
	};

	// GfxStateTables.cs:87: one IDirect3DDevice9::SetRenderState call
	struct render_state_value
	{
		std::uint32_t state;		// D3DRENDERSTATETYPE
		std::uint32_t value;

		bool operator==(const render_state_value&) const = default;
	};

	// The bit fields of a GfxStateBits pair ( Material::stateBitsTable entry = { loadBits[0], loadBits[1] } ),
	// as GfxStateBits.Translate reads them.
	namespace state_bits
	{
		// bits0
		inline constexpr std::uint32_t SRC_BLEND_MASK = 0x0000000F;			// D3DBLEND
		inline constexpr std::uint32_t DST_BLEND_MASK = 0x000000F0;			// D3DBLEND << 4
		inline constexpr std::uint32_t BLEND_OP_MASK = 0x00000700;			// D3DBLENDOP << 8; 0: blending off
		inline constexpr std::uint32_t ALPHA_TEST_DISABLE = 0x00000800;
		inline constexpr std::uint32_t ALPHA_TEST_MASK = 0x00003000;		// 0x1000 GT 0, 0x2000 LT 128, else GE 128
		inline constexpr std::uint32_t CULL_MASK = 0x0000C000;				// >> 14: 0 -, 1 NONE, 2 back ( D3DCULL_CCW ), 3 front ( D3DCULL_CW )
		inline constexpr std::uint32_t COLOR_WRITE_RGB = 0x08000000;
		inline constexpr std::uint32_t COLOR_WRITE_ALPHA = 0x10000000;
		inline constexpr std::uint32_t SRGB_WRITE = 0x40000000;				// "gamma write"
		inline constexpr std::uint32_t POLYMODE_LINE = 0x80000000;
		// bits1
		inline constexpr std::uint32_t DEPTH_WRITE = 0x00000001;
		inline constexpr std::uint32_t DEPTH_TEST_DISABLE = 0x00000002;
		inline constexpr std::uint32_t DEPTH_FUNC_MASK = 0x0000000C;		// >> 2: ALWAYS, LESS, EQUAL, LESSEQUAL
		inline constexpr std::uint32_t POLYGON_OFFSET_MASK = 0x00000030;	// >> 4: 0, 1, 2 x r_polygonOffset, 3 shadow map offset
		inline constexpr std::uint32_t STENCIL_FRONT_ENABLE = 0x00000040;
		inline constexpr std::uint32_t STENCIL_BACK_ENABLE = 0x00000080;	// separate back face ops in bits 20 .. 31
	}

	// GfxStateTables.cs:125-225 ( GfxStateBits.Translate ). The states in the order the C# renderer applies them.
	// `separate_alpha_blend` / `slope_scale_depth_bias` are the device capabilities ( D3DPMISCCAPS_SEPARATEALPHABLEND,
	// D3DPRASTERCAPS_SLOPESCALEDEPTHBIAS ); both true on any SM3 card.
	// Float states ( D3DRS_DEPTHBIAS, D3DRS_SLOPESCALEDEPTHBIAS ) carry the float's bits, as SetRenderState expects.
	std::vector<render_state_value> translate_state_bits(std::uint32_t bits0, std::uint32_t bits1, const renderer_settings& settings = {},
		bool separate_alpha_blend = true, bool slope_scale_depth_bias = true);

	// GfxStateTables.cs ( GameMapRenderer.cs:1353-1361 Blends ): the pass blends, so it is drawn in the transparent range
	inline bool state_bits_blend(const std::uint32_t bits0) { return (bits0 & state_bits::BLEND_OP_MASK) != 0; }

	// The states GameMapRenderer.cs:1135-1140 ( ApplyInitialStates ) sets once per frame before any material
	inline constexpr std::array<render_state_value, 3> INITIAL_RENDER_STATES =
	{ {
		{ d3d::RS_SEPARATEALPHABLENDENABLE, 1 },
		{ d3d::RS_TWOSIDEDSTENCILMODE, 1 },
		{ d3d::RS_LIGHTING, 0 },
	} };

	// GfxStateTables.cs:227
	struct sampler_state_values
	{
		std::uint32_t min_filter = 0;		// D3DTEXTUREFILTERTYPE
		std::uint32_t mag_filter = 0;
		std::uint32_t mip_filter = 0;
		std::uint32_t max_anisotropy = 0;
		std::uint32_t address_u = 0;		// D3DTEXTUREADDRESS
		std::uint32_t address_v = 0;
		std::uint32_t address_w = 0;
		bool srgb = false;					// D3DSAMP_SRGBTEXTURE

		bool operator==(const sampler_state_values&) const = default;

		struct pair
		{
			std::uint32_t state;	// D3DSAMPLERSTATETYPE
			std::uint32_t value;
		};
		// The eight SetSamplerState calls in the order of GameMapRenderer.cs:1758-1765
		std::array<pair, 8> states() const;
	};

	// The sampler state byte of a MaterialTextureDef / a code sampler
	namespace sampler_bits
	{
		inline constexpr std::uint8_t FILTER_MASK = 0x07;		// 1 nearest, 2 linear, 3 aniso 2x, 4 aniso 4x
		inline constexpr std::uint8_t MIPMAP_MASK = 0x18;		// >> 3: 0 disabled, 1 nearest, 2 linear
		inline constexpr std::uint8_t CLAMP_U = 0x20;
		inline constexpr std::uint8_t CLAMP_V = 0x40;
		inline constexpr std::uint8_t CLAMP_W = 0x80;
	}

	// GfxStateTables.cs:242-272 ( GfxSamplerState.Translate ). `srgb` is passed through ( the renderer decides
	// per texture: GameMapRenderer.cs:1743 ). The device parameters are D3DCAPS9::MaxAnisotropy ( at least 1 ) and
	// D3DPTFILTERCAPS_MINFANISOTROPIC / MAGFANISOTROPIC.
	sampler_state_values translate_sampler_state(std::uint8_t sampler_state, bool srgb, const renderer_settings& settings = {},
		int device_max_anisotropy = 16, bool anisotropic_min = true, bool anisotropic_mag = true);
}
