#pragma once

#include "codol_assets/material.hpp"

namespace codol::assets::state
{
	// fields of a StateBits value ( loadBits[0] low, loadBits[1] high )
	inline constexpr StateBits BLEND = 0x07FF07FFull;			// colour and alpha: source, destination, operation
	inline constexpr StateBits BLEND_COLOR = 0x7FFull;
	inline constexpr StateBits ALPHA_TEST = 0x3800ull;
	inline constexpr StateBits CULL = 0xC000ull;
	inline constexpr StateBits COLOR_WRITE = 0x18000000ull;
	inline constexpr StateBits GAMMA_WRITE = 0x40000000ull;
	inline constexpr StateBits DEPTH_WRITE = 0x1ull << 32;
	inline constexpr StateBits DEPTH_TEST = 0xEull << 32;
	inline constexpr StateBits POLYGON_OFFSET = 0x30ull << 32;

	// the fields a state is derived by, one at a time
	inline constexpr StateBits FIELDS[] = { 0x0000000Full, 0x000000F0ull, 0x00000700ull, 0x07FF0000ull, ALPHA_TEST, CULL, COLOR_WRITE, GAMMA_WRITE,
		DEPTH_WRITE, DEPTH_TEST, POLYGON_OFFSET };
	inline constexpr StateBits ALL_FIELDS = BLEND | ALPHA_TEST | CULL | COLOR_WRITE | GAMMA_WRITE | DEPTH_WRITE | DEPTH_TEST | POLYGON_OFFSET;
}
