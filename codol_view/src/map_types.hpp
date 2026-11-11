#pragma once

// Small map data types shared by the pieces of codol_view.

#include "codol_render/math.hpp"

#include <cstdint>
#include <string>

namespace codol::view
{
	using render::float3;
	using render::float4;
	using render::mat4;

	// A ComWorld primary light ( Codol.Assets\Maps\MapData.cs:315-340 MapLight, MapExtractor.cs:852-883 ReadLights:
	// 68 byte records: type u8 @0, canUseShadowMap u8 @1, exponent u8 @2, color @4, dir @16, origin @28, radius @40,
	// cosHalfFovOuter @44, cosHalfFovInner @48, cosHalfFovExpanded @52, rotationLimit @56, defName ptr @64 )
	struct map_light
	{
		int index = 0;
		std::uint8_t type = 0;				// 1 sun ( directional ), 2 spot, 3 omni
		float3 color;
		float3 direction;
		float3 origin;
		float radius = 0;
		float cos_half_fov_outer = 0;
		float cos_half_fov_inner = 0;
		bool can_use_shadow_map = false;
		std::uint8_t exponent = 0;
		float cos_half_fov_expanded = 0;
		float rotation_limit = 0;
		std::string def_name;				// empty when the light has none
	};
}
