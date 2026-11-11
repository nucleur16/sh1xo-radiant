#pragma once

// Every number of codol_render that is specific to Call of Duty Online 1.1.1.13 ( "C1", fastfile version 334 ).
// Only this build is ported; the C# sources switch on GameBuild, the values below are the 1.1.1.13 branch of
// each switch. Paths are relative to CODOL-AssetExplorer\src\ .
//
// The struct layouts are the ones of src/codol/c1_layouts.hpp of this repository ( codol::c1::Material,
// MaterialTechniqueSet, MaterialPass, MaterialShaderArgument, MaterialShader, MaterialVertexDeclaration,
// MaterialTextureDef, MaterialConstantDef, GfxImage, GfxImageLoadDef ); codol_render reads them through byte
// offsets so that it does not depend on that header. tests/test_material.cpp checks the offsets below against
// c1_layouts.hpp at compile time.

#include <cstddef>
#include <cstdint>

namespace codol::render::build
{
	// ---- MaterialTechniqueSet ( Codol.Assets\Rendering\RenderLayouts.cs:5-11 ) ---------------------------
	inline constexpr int TECHNIQUE_COUNT = 48;						// Codol.Assets\Game\GameStructures.cs:142
	inline constexpr std::uint32_t TECHSET_SIZE = 0xCC;
	inline constexpr std::uint32_t TECHSET_NAME = 0;
	inline constexpr std::uint32_t TECHSET_WORLD_VERT_FORMAT = 4;
	inline constexpr std::uint32_t TECHSET_TECHNIQUES = 12;			// 16 for 1.2.x / 2.6.9.7 / 3.x

	// ---- MaterialTechnique ( RenderLayouts.cs:13-19 ) -----------------------------------------------------
	inline constexpr std::uint32_t TECHNIQUE_NAME = 0;
	inline constexpr std::uint32_t TECHNIQUE_FLAGS = 4;
	inline constexpr std::uint32_t TECHNIQUE_PASS_COUNT = 6;
	inline constexpr std::uint32_t TECHNIQUE_PASSES = 8;

	// ---- MaterialPass ( RenderLayouts.cs:21-32 ) ----------------------------------------------------------
	inline constexpr std::uint32_t PASS_SIZE = 20;
	inline constexpr std::uint32_t PASS_VERTEX_DECL = 0;
	inline constexpr std::uint32_t PASS_VERTEX_SHADER = 4;
	inline constexpr std::uint32_t PASS_PIXEL_SHADER = 8;
	inline constexpr std::uint32_t PASS_PER_PRIM_ARG_COUNT = 12;
	inline constexpr std::uint32_t PASS_PER_OBJ_ARG_COUNT = 13;
	inline constexpr std::uint32_t PASS_STABLE_ARG_COUNT = 14;
	inline constexpr std::uint32_t PASS_CUSTOM_SAMPLER_FLAGS = 15;
	inline constexpr std::uint32_t PASS_ARGS = 16;

	// ---- MaterialShaderArgument ( RenderLayouts.cs:34-40 ) ------------------------------------------------
	inline constexpr std::uint32_t ARG_SIZE = 8;
	inline constexpr std::uint32_t ARG_TYPE = 0;
	inline constexpr std::uint32_t ARG_DEST = 2;
	inline constexpr std::uint32_t ARG_VALUE = 4;

	// ---- MaterialPixelShader / MaterialVertexShader ( RenderLayouts.cs:42-49 ) ----------------------------
	inline constexpr std::uint32_t SHADER_SIZE = 16;
	inline constexpr std::uint32_t SHADER_NAME = 0;
	inline constexpr std::uint32_t SHADER_PROGRAM = 8;
	inline constexpr std::uint32_t SHADER_PROGRAM_SIZE = 12;		// u16, in DWORDs
	inline constexpr std::uint32_t SHADER_LOAD_FOR_RENDERER = 14;

	// ---- MaterialVertexDeclaration ( RenderLayouts.cs:51-58 ) ---------------------------------------------
	inline constexpr std::uint32_t VERTEX_DECL_SIZE = 0x64;
	inline constexpr std::uint32_t VERTEX_DECL_NAME = 0;
	inline constexpr std::uint32_t VERTEX_DECL_STREAM_COUNT = 4;
	inline constexpr std::uint32_t VERTEX_DECL_HAS_OPTIONAL_SOURCE = 5;
	inline constexpr std::uint32_t VERTEX_DECL_ROUTING = 8;
	inline constexpr int VERTEX_DECL_MAX_STREAMS = 13;

	// ---- Material ( Codol.Assets\Game\GameStructures.cs:112-147, Shift 0 / CountShift 0 ) -----------------
	inline constexpr std::uint32_t MATERIAL_SIZE = 0x60;
	inline constexpr std::uint32_t MATERIAL_NAME = 0;
	inline constexpr std::uint32_t MATERIAL_GAME_FLAGS = 4;
	inline constexpr std::uint32_t MATERIAL_SORT_KEY = 5;
	inline constexpr std::uint32_t MATERIAL_SURFACE_TYPE_BITS = 16;
	inline constexpr std::uint32_t MATERIAL_STATE_BITS_ENTRY = 24;	// u8[TECHNIQUE_COUNT]
	inline constexpr std::uint32_t MATERIAL_TEXTURE_COUNT = 72;
	inline constexpr std::uint32_t MATERIAL_CONSTANT_COUNT = 73;
	inline constexpr std::uint32_t MATERIAL_STATE_BITS_COUNT = 74;
	inline constexpr std::uint32_t MATERIAL_STATE_FLAGS = 75;
	inline constexpr std::uint32_t MATERIAL_CAMERA_REGION = 76;
	inline constexpr std::uint32_t MATERIAL_TECHNIQUE_SET = 80;
	inline constexpr std::uint32_t MATERIAL_TEXTURE_TABLE = 84;
	inline constexpr std::uint32_t MATERIAL_CONSTANT_TABLE = 88;
	inline constexpr std::uint32_t MATERIAL_STATE_BITS_TABLE = 92;
	inline constexpr std::uint8_t CAMERA_REGION_COUNT = 4;			// GameStructures.cs:146 ( 5 only for 3.19.26.22 )

	// ---- MaterialTextureDef / MaterialConstantDef ( GameStructures.cs:149-166 ) ---------------------------
	inline constexpr std::uint32_t TEXTURE_DEF_SIZE = 12;
	inline constexpr std::uint32_t TEXTURE_DEF_NAME_HASH = 0;
	inline constexpr std::uint32_t TEXTURE_DEF_NAME_START = 4;
	inline constexpr std::uint32_t TEXTURE_DEF_NAME_END = 5;
	inline constexpr std::uint32_t TEXTURE_DEF_SAMPLER_STATE = 6;
	inline constexpr std::uint32_t TEXTURE_DEF_SEMANTIC = 7;
	inline constexpr std::uint32_t TEXTURE_DEF_IMAGE = 8;

	inline constexpr std::uint32_t CONSTANT_DEF_SIZE = 32;
	inline constexpr std::uint32_t CONSTANT_DEF_NAME_HASH = 0;
	inline constexpr std::uint32_t CONSTANT_DEF_NAME = 4;			// char[12], not always terminated
	inline constexpr std::uint32_t CONSTANT_DEF_LITERAL = 16;

	// ---- GfxImage ( GameStructures.cs:168-182, HasImageNameAt32 ) / GfxImageLoadDef ( :184-192 ) ----------
	inline constexpr std::uint32_t IMAGE_SIZE = 0x24;
	inline constexpr std::uint32_t IMAGE_LOAD_DEF = 0;
	inline constexpr std::uint32_t IMAGE_MAP_TYPE = 4;
	inline constexpr std::uint32_t IMAGE_SEMANTIC = 5;
	inline constexpr std::uint32_t IMAGE_CATEGORY = 6;
	inline constexpr std::uint32_t IMAGE_WIDTH = 20;
	inline constexpr std::uint32_t IMAGE_HEIGHT = 22;
	inline constexpr std::uint32_t IMAGE_DEPTH = 24;
	inline constexpr std::uint32_t IMAGE_NAME = 32;
	inline constexpr std::uint8_t MAPTYPE_CUBE = 5;					// TextureExtractor.cs:326

	inline constexpr std::uint32_t LOAD_DEF_HEADER_SIZE = 16;
	inline constexpr std::uint32_t LOAD_DEF_LEVEL_COUNT = 0;
	inline constexpr std::uint32_t LOAD_DEF_FLAGS = 4;
	inline constexpr std::uint32_t LOAD_DEF_FORMAT = 8;
	inline constexpr std::uint32_t LOAD_DEF_RESOURCE_SIZE = 12;
	inline constexpr std::uint32_t LOAD_DEF_DATA = 16;
	inline constexpr std::uint32_t MAX_PRESERVED_IMAGE_BYTES = 64u << 20;	// Codol.FastFiles\Emulation\LoaderNatives.cs:94

	// ---- code constants ( RenderLayouts.cs:79-80, Codol.FastFiles\Porting\V3ToV1\CodeConstantMap.cs ) -----
	inline constexpr int FIRST_CODE_MATRIX = 79;					// 86 for 1.2.x, 247 / 249 for 3.x
	inline constexpr int CODE_MATRIX_GROUPS = 14;
	inline constexpr int CODE_CONSTANT_COUNT = FIRST_CODE_MATRIX + 4 * CODE_MATRIX_GROUPS;	// 135
	inline constexpr int CODE_SAMPLER_COUNT = 26;

	// ---- client executable ( C1.exe ), file offsets -------------------------------------------------------
	// Codol.Assets\Textures\WaveletDecoder.cs:11-13: three Huffman tables of 4096 { s16 value, u16 bits }
	inline constexpr std::uint64_t EXE_WAVELET_PRIMARY = 0x7F5E60;
	inline constexpr std::uint64_t EXE_WAVELET_SECONDARY = 0x7F9E60;
	inline constexpr std::uint64_t EXE_WAVELET_ALPHA = 0x7FDE60;
	// Codol.FastFiles\Crypto\FastFileKeyProvider.cs:9-11 ( g_ffRsaPrivateKeyDer 0x00C3D248 ), :45 ( g_iwdAesKeyRsaBlob 0x00C39B20 )
	inline constexpr std::uint64_t EXE_RSA_PRIVATE_KEY_DER = 0x83C248;
	inline constexpr std::uint32_t EXE_RSA_PRIVATE_KEY_DER_SIZE = 1191;
	inline constexpr std::uint64_t EXE_IWD_KEY_BLOB = 0x838B20;	// 256 bytes, RSA-OAEP( SHA-256 ) of the 24 byte AES key

	// ---- IWI ( Codol.Assets\Textures\IwiFile.cs:52-53 ) ---------------------------------------------------
	inline constexpr int IWI_HEADER_SIZE = 32;
	inline constexpr std::uint8_t IWI_VERSION = 9;					// version 8 ( IW4 ) has the same header and data
}
