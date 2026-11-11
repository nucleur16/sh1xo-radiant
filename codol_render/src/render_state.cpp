#include "codol_render/render_state.hpp"

#include "codol_render/common.hpp"

#include <algorithm>

namespace codol::render
{
	namespace
	{
		constexpr std::uint32_t CULL_MODES[4] = { 0, 1, 3, 2 };			// GfxStateTables.cs:122
		constexpr std::uint32_t DEPTH_FUNCTIONS[4] = { 8, 2, 3, 4 };	// GfxStateTables.cs:123
		constexpr std::uint32_t MIP_FILTERS[12] = { 0, 1, 2, 0, 2, 2, 0, 1, 1, 0, 0, 0 };	// GfxStateTables.cs:240

		// GfxStateTables.cs:217-224: the alpha channel blends with the alpha variant of a colour factor
		std::uint32_t alpha_factor(const std::uint32_t factor)
		{
			switch (factor)
			{
			case 3: return 5;		// SRCCOLOR -> SRCALPHA
			case 4: return 6;		// INVSRCCOLOR -> INVSRCALPHA
			case 9: return 7;		// DESTCOLOR -> DESTALPHA
			case 10: return 8;		// INVDESTCOLOR -> INVDESTALPHA
			default: return factor;
			}
		}
	}

	std::vector<render_state_value> translate_state_bits(const std::uint32_t bits0, const std::uint32_t bits1, const renderer_settings& settings,
		const bool separate_alpha_blend, const bool slope_scale_depth_bias)
	{
		// GfxStateTables.cs:125-215
		std::vector<render_state_value> states;
		try
		{
			states.reserve(32);
			const auto set = [&](const std::uint32_t state, const std::uint32_t value) { states.push_back({ state, value }); };

			set(d3d::RS_COLORWRITEENABLE, ((bits0 & 0x08000000) != 0 ? 7u : 0u) | ((bits0 >> 25) & 8));
			const bool alpha_test = (bits0 & 0x800) == 0;
			set(d3d::RS_ALPHATESTENABLE, alpha_test ? 1u : 0u);
			if (alpha_test)
			{
				switch (bits0 & 0x3000)
				{
				case 0x1000:
					set(d3d::RS_ALPHAFUNC, 5);
					set(d3d::RS_ALPHAREF, 0);
					break;
				case 0x2000:
					set(d3d::RS_ALPHAFUNC, 2);
					set(d3d::RS_ALPHAREF, 128);
					break;
				default:
					set(d3d::RS_ALPHAFUNC, 7);
					set(d3d::RS_ALPHAREF, 128);
					break;
				}
			}
			set(d3d::RS_CULLMODE, CULL_MODES[(bits0 >> 14) & 3]);
			set(d3d::RS_FILLMODE, (bits0 & 0x80000000) != 0 ? 2u : 3u);
			set(d3d::RS_SRGBWRITEENABLE, (bits0 >> 30) & 1);
			if ((bits0 & 0x700) != 0)
			{
				const std::uint32_t src = bits0 & 0xF, dst = (bits0 >> 4) & 0xF, op = (bits0 >> 8) & 7;
				set(d3d::RS_ALPHABLENDENABLE, 1);
				set(d3d::RS_BLENDOP, op);
				set(d3d::RS_SRCBLEND, src);
				set(d3d::RS_DESTBLEND, dst);
				set(d3d::RS_SEPARATEALPHABLENDENABLE, 1);
				set(d3d::RS_BLENDOPALPHA, op);
				set(d3d::RS_SRCBLENDALPHA, separate_alpha_blend ? alpha_factor(src) : src);
				set(d3d::RS_DESTBLENDALPHA, separate_alpha_blend ? alpha_factor(dst) : dst);
			}
			else
			{
				set(d3d::RS_ALPHABLENDENABLE, 0);
			}

			set(d3d::RS_ZWRITEENABLE, bits1 & 1);
			const bool depth_test = (bits1 & 2) == 0;
			set(d3d::RS_ZENABLE, depth_test ? 1u : 0u);
			if (depth_test)
			{
				set(d3d::RS_ZFUNC, DEPTH_FUNCTIONS[(bits1 >> 2) & 3]);
			}
			const std::uint32_t offset = (bits1 >> 4) & 3;
			float bias, scale;
			if (offset == 3)
			{
				bias = settings.shadow_map_polygon_offset_bias / 65536.0f;
				scale = settings.shadow_map_polygon_offset_scale;
			}
			else
			{
				bias = settings.polygon_offset_bias * static_cast<float>(offset) / 65536.0f;
				scale = static_cast<float>(offset) * settings.polygon_offset_scale;
			}
			if (slope_scale_depth_bias)
			{
				set(d3d::RS_SLOPESCALEDEPTHBIAS, float_bits(scale));
			}
			else
			{
				bias *= 2;
			}
			set(d3d::RS_DEPTHBIAS, float_bits(bias));
			const bool stencil = (bits1 & 0x40) != 0;
			set(d3d::RS_STENCILENABLE, stencil ? 1u : 0u);
			if (stencil)
			{
				// without a separate back face ( 0x80 ) the front ops ( bits 8 .. 19 ) are used for both faces
				const std::uint32_t back = (bits1 & 0x80) != 0 ? bits1 : ((bits1 & 0xFFF00) << 12) | (bits1 & 0xFFFFF);
				set(d3d::RS_STENCILFAIL, ((bits1 >> 8) & 7) + 1);
				set(d3d::RS_STENCILZFAIL, ((bits1 >> 11) & 7) + 1);
				set(d3d::RS_STENCILPASS, ((bits1 >> 14) & 7) + 1);
				set(d3d::RS_STENCILFUNC, ((bits1 >> 17) & 7) + 1);
				set(d3d::RS_TWOSIDEDSTENCILMODE, 1);
				set(d3d::RS_CCW_STENCILFAIL, ((back >> 20) & 7) + 1);
				set(d3d::RS_CCW_STENCILZFAIL, ((back >> 23) & 7) + 1);
				set(d3d::RS_CCW_STENCILPASS, ((back >> 26) & 7) + 1);
				set(d3d::RS_CCW_STENCILFUNC, (back >> 29) + 1);
			}
		}
		catch (...)
		{
			states.clear();
		}
		return states;
	}

	std::array<sampler_state_values::pair, 8> sampler_state_values::states() const
	{
		// GameMapRenderer.cs:1758-1765
		return
		{ {
			{ d3d::SAMP_MINFILTER, min_filter },
			{ d3d::SAMP_MAGFILTER, mag_filter },
			{ d3d::SAMP_MIPFILTER, mip_filter },
			{ d3d::SAMP_MAXANISOTROPY, max_anisotropy },
			{ d3d::SAMP_ADDRESSU, address_u },
			{ d3d::SAMP_ADDRESSV, address_v },
			{ d3d::SAMP_ADDRESSW, address_w },
			{ d3d::SAMP_SRGBTEXTURE, srgb ? 1u : 0u },
		} };
	}

	sampler_state_values translate_sampler_state(const std::uint8_t sampler_state, const bool srgb, const renderer_settings& settings,
		const int device_max_anisotropy, const bool anisotropic_min, const bool anisotropic_mag)
	{
		// GfxStateTables.cs:242-272
		constexpr std::uint32_t point = d3d::TEXF_POINT, linear = d3d::TEXF_LINEAR, anisotropic = d3d::TEXF_ANISOTROPIC;
		int max = std::min(settings.texture_filter_anisotropy_max, device_max_anisotropy);
		std::uint32_t aniso_min = linear, aniso_mag = linear;
		if (max > 1)
		{
			aniso_min = anisotropic_min ? anisotropic : linear;
			aniso_mag = anisotropic_mag ? anisotropic : linear;
		}
		else
		{
			max = 1;
		}
		const int min = std::min(settings.texture_filter_anisotropy_min, max);
		const int mode = settings.texture_filter_disable ? 3 : std::clamp(settings.texture_filter_mip_mode, 0, 3);
		// mode 3 with mip bits 3 indexes past the C# table ( it would throw there ); no state byte of the game has mip bits 3
		const int mip_index = 3 * mode + ((sampler_state >> 3) & 3);
		const std::uint32_t mip = mip_index < 12 ? MIP_FILTERS[mip_index] : 0;
		const int filter = settings.texture_filter_disable ? 0 : sampler_state & 7;

		sampler_state_values result;
		int aniso;
		switch (filter)
		{
		case 2:
			if (mip == 0)
			{
				result.min_filter = linear; result.mag_filter = linear; aniso = 1;
			}
			else if (min != 1)
			{
				result.min_filter = aniso_min; result.mag_filter = aniso_mag; aniso = min;
			}
			else
			{
				result.min_filter = linear; result.mag_filter = linear; aniso = min;
			}
			break;
		case 3:
			result.min_filter = aniso_min; result.mag_filter = aniso_mag; aniso = std::max(min, std::min(max, 2));
			break;
		case 4:
			result.min_filter = aniso_min; result.mag_filter = aniso_mag; aniso = std::max(min, std::min(max, 4));
			break;
		default:
			result.min_filter = point; result.mag_filter = point; aniso = 1;
			break;
		}
		result.mip_filter = mip;
		result.max_anisotropy = static_cast<std::uint32_t>(aniso);
		result.address_u = (sampler_state & 0x20) != 0 ? d3d::TADDRESS_CLAMP : d3d::TADDRESS_WRAP;
		result.address_v = (sampler_state & 0x40) != 0 ? d3d::TADDRESS_CLAMP : d3d::TADDRESS_WRAP;
		result.address_w = (sampler_state & 0x80) != 0 ? d3d::TADDRESS_CLAMP : d3d::TADDRESS_WRAP;
		result.srgb = srgb;
		return result;
	}
}
