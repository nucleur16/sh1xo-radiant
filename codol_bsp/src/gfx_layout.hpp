#pragma once

// GfxWorld pieces c1_layouts.hpp has no struct for. 1.1.1.13 keeps the IW4 layout of the render world byte for byte
// ( CODOL-AssetExplorer Iw4ToV1Converters.cs ); sizes and stream order were checked against the loaders
// ( codol_zone/src/loaders_1_1_1_13.inc, Load_GfxWorld 0x4C7FE0 ) and against the shipped zones with codol_bsp_probe.

#include "codol_zone/c1_layouts.hpp"

namespace codol::bsp::gfx
{
	namespace c1 = codol::c1;

#pragma pack(push, 1)
	struct GfxSky
	{
		std::int32_t skySurfCount;
		c1::zptr skyStartSurfs;			// int per sky surface: index into dpvs.surfaces
		c1::zptr skyImage;
		std::uint8_t skySamplerState;
		std::uint8_t pad[3];
	};

	struct GfxAabbTree
	{
		c1::Bounds bounds;
		std::uint16_t childCount;
		std::uint16_t surfaceCount;
		std::uint16_t startSurfIndex;			// into dpvs.sortedSurfIndex
		std::uint16_t surfaceCountNoDecal;
		std::uint16_t startSurfIndexNoDecal;
		std::uint16_t smodelIndexCount;
		c1::zptr smodelIndexes;
		std::int32_t childrenOffset;			// bytes from this node to its first child
	};

	struct GfxPortal
	{
		std::uint8_t writable[12];				// runtime
		float plane[4];
		c1::zptr vertices;
		std::uint16_t cellIndex;				// the cell the portal leads to
		std::uint8_t vertexCount;
		std::uint8_t pad;
		float hullAxis[2][3];
	};

	struct GfxLightmapArray
	{
		c1::zptr primary;
		c1::zptr secondary;
	};

	struct GfxLightGrid
	{
		std::uint8_t hasLightRegions;
		std::uint8_t pad[3];
		std::uint32_t lastSunPrimaryLightIndex;
		std::uint16_t mins[3];
		std::uint16_t maxs[3];
		std::uint32_t rowAxis;
		std::uint32_t colAxis;
		c1::zptr rowDataStart;
		std::uint32_t rawRowDataSize;
		c1::zptr rawRowData;
		std::uint32_t entryCount;
		c1::zptr entries;
		std::uint32_t colorCount;
		c1::zptr colors;
	};

	struct GfxLightGridEntry
	{
		std::uint16_t colorsIndex;
		std::uint8_t primaryLightIndex;
		std::uint8_t needsTrace;
	};

	struct GfxLightGridColors
	{
		std::uint8_t rgb[56][3];
	};

	struct GfxLightGridRow
	{
		std::uint16_t colStart;
		std::uint16_t colCount;
		std::uint16_t zStart;
		std::uint16_t zCount;
		std::uint32_t firstEntry;
	};

	struct GfxShadowGeometry
	{
		std::uint16_t surfaceCount;
		std::uint16_t smodelCount;
		c1::zptr sortedSurfIndex;
		c1::zptr smodelIndex;
	};

	struct GfxLightRegion
	{
		std::uint32_t hullCount;
		c1::zptr hulls;
	};

	struct MaterialMemory
	{
		c1::zptr material;
		std::int32_t memory;
	};

	struct ComPrimaryLight
	{
		std::uint8_t type;						// 0 none, 1 sun ( directional ), 2 spot, 3 omni
		std::uint8_t canUseShadowMap;
		std::uint8_t exponent;
		std::uint8_t unused;
		float color[3];
		float dir[3];
		float origin[3];
		float radius;
		float cosHalfFovOuter;
		float cosHalfFovInner;
		float cosHalfFovExpanded;
		float rotationLimit;
		float translationLimit;
		c1::zptr defName;
	};
#pragma pack(pop)

	static_assert(sizeof(GfxSky) == 16 && sizeof(GfxAabbTree) == 44 && sizeof(GfxPortal) == 60 && sizeof(GfxLightGrid) == 56);
	static_assert(sizeof(GfxLightGridEntry) == 4 && sizeof(GfxLightGridColors) == 168 && sizeof(GfxLightGridRow) == 12);
	static_assert(sizeof(GfxShadowGeometry) == 12 && sizeof(GfxLightRegion) == 8 && sizeof(ComPrimaryLight) == 68);

	// GfxWorld header fields without a name in c1::GfxWorld
	inline constexpr std::uint32_t W_SORT_KEYS = 36;				// lit decal, effect decal, effect auto, distortion
	inline constexpr std::uint32_t W_PLANES = 56, W_NODES = 60, W_SCENE_ENT_CELL_BITS = 64, W_AABB_TREE_COUNTS = 68, W_AABB_TREES = 72;
	inline constexpr std::uint32_t W_CELL_CASTER_BITS = 416, W_CELL_SUN_LIT_BITS = 420, W_SCENE_DYN_MODEL = 424, W_SCENE_DYN_BRUSH = 428;
	inline constexpr std::uint32_t W_LIGHT_ENT_SHADOW_VIS = 432, W_SHADOW_GEOM = 448, W_LIGHT_REGION = 452, W_FOG_TYPES = 624;
	inline constexpr std::uint32_t D_PROBE_TEXTURES = 12, D_LIGHTMAP_PRIMARY_TEX = 24, D_LIGHTMAP_SECONDARY_TEX = 28;	// in GfxWorldDraw
	inline constexpr std::uint32_t SUN_SPRITE_MATERIAL = 4, SUN_FLARE_MATERIAL = 8;		// in sunflare_t

	// sort keys the header of every shipped map names
	inline constexpr std::uint32_t SORT_KEY_LIT_DECAL = 6, SORT_KEY_EFFECT_DECAL = 0x28, SORT_KEY_EFFECT_AUTO = 0x31, SORT_KEY_DISTORTION = 0x2C;

	// lightmap atlas: the secondary ( baked light ) image holds two layers of LM_W x LM_H texels on top of each
	// other, the primary ( baked sun visibility ) one layer of twice that resolution
	inline constexpr int LM_W = 512, LM_H = 1024;

	// D3DFORMAT
	inline constexpr std::uint32_t FMT_A8R8G8B8 = 21, FMT_L8 = 50;
}
