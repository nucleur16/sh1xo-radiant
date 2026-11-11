#pragma once

// The Direct3D 9 enum values codol_render produces, so that neither the library nor its callers need <d3d9.h>
// for them. The values are those of d3d9types.h; names keep the SDK spelling behind the d3d:: namespace.
// `vertex_element` has the layout of D3DVERTEXELEMENT9 and can be passed to CreateVertexDeclaration as is.

#include <cstdint>

namespace codol::render::d3d
{
	// D3DFORMAT ( Codol.Assets\Textures\TextureDecoder.cs:26-36 )
	inline constexpr std::uint32_t FMT_A8R8G8B8 = 21;
	inline constexpr std::uint32_t FMT_X8R8G8B8 = 22;
	inline constexpr std::uint32_t FMT_A8 = 28;
	inline constexpr std::uint32_t FMT_L8 = 50;
	inline constexpr std::uint32_t FMT_A8L8 = 51;
	inline constexpr std::uint32_t FMT_DXT1 = 0x31545844;
	inline constexpr std::uint32_t FMT_DXT3 = 0x33545844;
	inline constexpr std::uint32_t FMT_DXT5 = 0x35545844;

	// D3DRENDERSTATETYPE ( Codol.Assets\Rendering\GfxStateTables.cs:91-120 )
	inline constexpr std::uint32_t RS_ZENABLE = 7;
	inline constexpr std::uint32_t RS_FILLMODE = 8;
	inline constexpr std::uint32_t RS_ZWRITEENABLE = 14;
	inline constexpr std::uint32_t RS_ALPHATESTENABLE = 15;
	inline constexpr std::uint32_t RS_SRCBLEND = 19;
	inline constexpr std::uint32_t RS_DESTBLEND = 20;
	inline constexpr std::uint32_t RS_CULLMODE = 22;
	inline constexpr std::uint32_t RS_ZFUNC = 23;
	inline constexpr std::uint32_t RS_ALPHAREF = 24;
	inline constexpr std::uint32_t RS_ALPHAFUNC = 25;
	inline constexpr std::uint32_t RS_ALPHABLENDENABLE = 27;
	inline constexpr std::uint32_t RS_STENCILENABLE = 52;
	inline constexpr std::uint32_t RS_STENCILFAIL = 53;
	inline constexpr std::uint32_t RS_STENCILZFAIL = 54;
	inline constexpr std::uint32_t RS_STENCILPASS = 55;
	inline constexpr std::uint32_t RS_STENCILFUNC = 56;
	inline constexpr std::uint32_t RS_LIGHTING = 137;				// GameMapRenderer.cs:1139 sets it to 0 once per frame
	inline constexpr std::uint32_t RS_COLORWRITEENABLE = 168;
	inline constexpr std::uint32_t RS_BLENDOP = 171;
	inline constexpr std::uint32_t RS_SLOPESCALEDEPTHBIAS = 175;
	inline constexpr std::uint32_t RS_TWOSIDEDSTENCILMODE = 185;
	inline constexpr std::uint32_t RS_CCW_STENCILFAIL = 186;
	inline constexpr std::uint32_t RS_CCW_STENCILZFAIL = 187;
	inline constexpr std::uint32_t RS_CCW_STENCILPASS = 188;
	inline constexpr std::uint32_t RS_CCW_STENCILFUNC = 189;
	inline constexpr std::uint32_t RS_SRGBWRITEENABLE = 194;
	inline constexpr std::uint32_t RS_DEPTHBIAS = 195;
	inline constexpr std::uint32_t RS_SEPARATEALPHABLENDENABLE = 206;
	inline constexpr std::uint32_t RS_SRCBLENDALPHA = 207;
	inline constexpr std::uint32_t RS_DESTBLENDALPHA = 208;
	inline constexpr std::uint32_t RS_BLENDOPALPHA = 209;

	// D3DSAMPLERSTATETYPE ( GfxStateTables.cs:231-238 )
	inline constexpr std::uint32_t SAMP_ADDRESSU = 1;
	inline constexpr std::uint32_t SAMP_ADDRESSV = 2;
	inline constexpr std::uint32_t SAMP_ADDRESSW = 3;
	inline constexpr std::uint32_t SAMP_MAGFILTER = 5;
	inline constexpr std::uint32_t SAMP_MINFILTER = 6;
	inline constexpr std::uint32_t SAMP_MIPFILTER = 7;
	inline constexpr std::uint32_t SAMP_MAXANISOTROPY = 10;
	inline constexpr std::uint32_t SAMP_SRGBTEXTURE = 11;

	// D3DTEXTUREFILTERTYPE / D3DTEXTUREADDRESS
	inline constexpr std::uint32_t TEXF_NONE = 0;
	inline constexpr std::uint32_t TEXF_POINT = 1;
	inline constexpr std::uint32_t TEXF_LINEAR = 2;
	inline constexpr std::uint32_t TEXF_ANISOTROPIC = 3;
	inline constexpr std::uint32_t TADDRESS_WRAP = 1;
	inline constexpr std::uint32_t TADDRESS_CLAMP = 3;

	// D3DDECLTYPE
	inline constexpr std::uint8_t DECLTYPE_FLOAT1 = 0;
	inline constexpr std::uint8_t DECLTYPE_FLOAT2 = 1;
	inline constexpr std::uint8_t DECLTYPE_FLOAT3 = 2;
	inline constexpr std::uint8_t DECLTYPE_FLOAT4 = 3;
	inline constexpr std::uint8_t DECLTYPE_D3DCOLOR = 4;
	inline constexpr std::uint8_t DECLTYPE_UBYTE4 = 5;
	inline constexpr std::uint8_t DECLTYPE_UNUSED = 17;

	// D3DDECLUSAGE
	inline constexpr std::uint8_t DECLUSAGE_POSITION = 0;
	inline constexpr std::uint8_t DECLUSAGE_NORMAL = 3;
	inline constexpr std::uint8_t DECLUSAGE_TEXCOORD = 5;
	inline constexpr std::uint8_t DECLUSAGE_COLOR = 10;
	inline constexpr std::uint8_t DECLUSAGE_DEPTH = 12;

	// D3DVERTEXELEMENT9
	struct vertex_element
	{
		std::uint16_t stream;
		std::uint16_t offset;
		std::uint8_t type;			// D3DDECLTYPE
		std::uint8_t method;		// D3DDECLMETHOD, always 0 ( DEFAULT )
		std::uint8_t usage;			// D3DDECLUSAGE
		std::uint8_t usage_index;

		bool operator==(const vertex_element&) const = default;
	};
	static_assert(sizeof(vertex_element) == 8, "vertex_element must match D3DVERTEXELEMENT9");

	// D3DDECL_END()
	inline constexpr vertex_element DECL_END = { 0xFF, 0, DECLTYPE_UNUSED, 0, 0, 0 };
}
