#pragma once

// Which technique slot a surface is drawn with. Port of GameMapRenderer.cs:1391-1427 ( ChooseTechnique ) as a
// pure function.

#include "technique_set.hpp"

namespace codol::render
{
	struct technique_choice
	{
		// The surface belongs to the emissive draw range ( GameMapRenderer.cs _ranges: sort keys the game draws
		// after the lit geometry; effects are always drawn emissive: GameMapRenderer.Effects.cs:247 )
		bool emissive = false;
		// GfxSurface / static model primary light index; 0 = none
		int primary_light_index = 0;
		// GfxWorld::lastSunPrimaryLightIndex ( GameMapRenderer.cs:97 ): lights 1 .. this are the sun
		int last_sun_primary_light_index = 0;
		// ComWorld primary light type of `primary_light_index`: 1 directional, 2 spot, 3 omni;
		// 0 when the index is not a light of the map ( GameMapRenderer.cs:1429 PrimaryLight returns null )
		int light_type = 0;
		// a sun shadow map was rendered this frame
		bool sun_shadows_active = false;
		// this light has a spot shadow map this frame ( _spotShadowByLight.ContainsKey )
		bool light_shadowed = false;
		// bit t set: the technique set has a technique in slot t ( render_technique_set::slot_mask )
		std::uint64_t slots = 0;
	};

	constexpr int choose_technique(const technique_choice& c)
	{
		namespace rt = renderer_technique;
		const auto has = [&](const int type) { return type >= 0 && type < build::TECHNIQUE_COUNT && ((c.slots >> type) & 1) != 0; };
		if (!c.emissive)
		{
			int lit = rt::LIT;
			if (c.primary_light_index > 0)
			{
				if (c.primary_light_index <= c.last_sun_primary_light_index)
				{
					lit = c.sun_shadows_active && has(rt::LIT_SUN_SHADOW) ? rt::LIT_SUN_SHADOW : rt::LIT_SUN;
				}
				else if (c.light_type != 0)
				{
					const bool shadowed = c.light_shadowed;
					lit = c.light_type == 2 ? (shadowed ? rt::LIT_SPOT_SHADOW : rt::LIT_SPOT)
						: c.light_type == 3 ? (shadowed ? rt::LIT_OMNI_SHADOW : rt::LIT_OMNI) : rt::LIT;
					if (shadowed && !has(lit))
					{
						lit = c.light_type == 2 ? rt::LIT_SPOT : rt::LIT_OMNI;
					}
				}
			}
			if (has(lit))
			{
				return lit;
			}
			if (has(lit - 1))		// the variant without distance fog of the same pair
			{
				return lit - 1;
			}
			if (has(rt::LIT))
			{
				return rt::LIT;
			}
		}
		return has(rt::EMISSIVE) ? rt::EMISSIVE : rt::UNLIT;	// the caller still checks that the slot exists
	}
}
