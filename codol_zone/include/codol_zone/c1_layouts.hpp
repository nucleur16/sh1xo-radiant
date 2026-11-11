#pragma once

// Asset layouts of Call of Duty Online 1.1.1.13 ( "C1", fastfile version 334 ) as they are stored in a zone.
//
// Source of truth: the offsets verified by CODOL-AssetExplorer ( Codol.Assets: GameStructures.cs, MapLayouts.cs,
// RenderLayouts.cs, Codol.FastFiles: XAssetType.cs / GameBuild.cs ) - its loaders round-trip all 90 stock
// 1.1.1.13 fastfiles byte for byte. Every size and every named offset below is checked at compile time against
// those numbers; `_unkNN` members are bytes whose meaning is not established and are never interpreted.
// Where C1 differs from IW4 it is noted ( XModel, XModelSurfs, XSurface, GfxImage, PhysPreset, GameWorldMp ).
//
// These are *zone* layouts: every pointer is a 32 bit zone reference ( zptr ), never a native pointer, so the
// structs have the same size in a 32 and a 64 bit build.
//
// No dependency on radiant or imgui, so it is also built into the unit tests. Part of the standalone codol_zone
// library; src/codol/c1_layouts.hpp forwards to this file.
//
// In a zone that codol_zone has loaded ( codol::zone::Zone ) every zptr field holds an *image address*: block b
// starts at ( b + 1 ) << 28, asset headers live in the pool region at 0x9001'0000. Zone::bytes() / Zone::get<T>()
// turn such an address into data.

#include <cstddef>
#include <cstdint>

namespace codol::c1
{
	inline constexpr int FASTFILE_VERSION = 334;

	// zone reference:
	//   0            null
	//   0xFFFFFFFF   the data follows inline in the stream
	//   0xFFFFFFFE   inline, and the loader stores the address ( pointer to pointer )
	//   otherwise    ( block << 28 | offset ) + 1 into the memory blocks the loader has built so far
	using zptr = std::uint32_t;

	inline constexpr zptr ZPTR_NULL = 0;
	inline constexpr zptr ZPTR_INLINE = 0xFFFFFFFFu;
	inline constexpr zptr ZPTR_INLINE_INSERT = 0xFFFFFFFEu;

	inline constexpr bool zptr_is_inline(const zptr p) { return p == ZPTR_INLINE || p == ZPTR_INLINE_INSERT; }
	inline constexpr std::uint32_t zptr_block(const zptr p) { return (p - 1) >> 28; }
	inline constexpr std::uint32_t zptr_offset(const zptr p) { return (p - 1) & 0x0FFFFFFFu; }

	// ------------------------------------------------------------------------------------------
	// asset types ( g_assetNames @ 0x00CD55B0, Load_XAsset @ 0x004C8ED0 )

	enum XAssetType : std::int32_t
	{
		ASSET_TYPE_PHYSPRESET = 0,
		ASSET_TYPE_PHYSCOLLMAP = 1,
		ASSET_TYPE_XANIMPARTS = 2,
		ASSET_TYPE_XMODEL_SURFS = 3,
		ASSET_TYPE_XMODEL = 4,
		ASSET_TYPE_MATERIAL = 5,
		ASSET_TYPE_PIXELSHADER = 6,
		ASSET_TYPE_VERTEXSHADER = 7,
		ASSET_TYPE_VERTEXDECL = 8,
		ASSET_TYPE_TECHNIQUE_SET = 9,
		ASSET_TYPE_IMAGE = 10,
		ASSET_TYPE_SOUND = 11,
		ASSET_TYPE_SOUND_CURVE = 12,
		ASSET_TYPE_LOADED_SOUND = 13,
		ASSET_TYPE_CLIPMAP_SP = 14,
		ASSET_TYPE_CLIPMAP_MP = 15,
		ASSET_TYPE_COMWORLD = 16,
		ASSET_TYPE_GAMEWORLD_SP = 17,
		ASSET_TYPE_GAMEWORLD_MP = 18,
		ASSET_TYPE_MAP_ENTS = 19,
		ASSET_TYPE_FXWORLD = 20,
		ASSET_TYPE_GFXWORLD = 21,
		ASSET_TYPE_LIGHT_DEF = 22,
		ASSET_TYPE_UI_MAP = 23,				// no loader
		ASSET_TYPE_FONT = 24,
		ASSET_TYPE_MENULIST = 25,
		ASSET_TYPE_MENU = 26,
		ASSET_TYPE_LOCALIZE_ENTRY = 27,
		ASSET_TYPE_WEAPON = 28,
		ASSET_TYPE_SNDDRIVER_GLOBALS = 29,	// no loader
		ASSET_TYPE_FX = 30,
		ASSET_TYPE_IMPACT_FX = 31,
		ASSET_TYPE_AITYPE = 32,				// no loader
		ASSET_TYPE_MPTYPE = 33,				// no loader
		ASSET_TYPE_CHARACTER = 34,			// no loader
		ASSET_TYPE_XMODELALIAS = 35,		// no loader
		ASSET_TYPE_RAWFILE = 36,
		ASSET_TYPE_STRINGTABLE = 37,
		ASSET_TYPE_LEADERBOARD = 38,
		ASSET_TYPE_STRUCTURED_DATA_DEF = 39,
		ASSET_TYPE_TRACER = 40,
		ASSET_TYPE_VEHICLE = 41,
		ASSET_TYPE_ADDON_MAP_ENTS = 42,
		ASSET_TYPE_WEAPON_ATTACHMENT = 43,
		ASSET_TYPE_COUNT = 44,
	};

	inline constexpr const char* XASSET_NAMES[ASSET_TYPE_COUNT] =
	{
		"physpreset", "phys_collmap", "xanim", "xmodelsurfs", "xmodel", "material", "pixelshader", "vertexshader",
		"vertexdecl", "techset", "image", "sound", "sndcurve", "loaded_sound", "col_map_sp", "col_map_mp",
		"com_map", "game_map_sp", "game_map_mp", "map_ents", "fx_map", "gfx_map", "lightdef", "ui_map",
		"font", "menufile", "menu", "localize", "weapon", "snddriverglobals", "fx", "impactfx", "aitype",
		"mptype", "character", "xmodelalias", "rawfile", "stringtable", "leaderboarddef", "structureddatadef",
		"tracer", "vehicle", "addon_map_ents", "weaponattach",
	};

	// size of the asset header of every type ( 0: the type has no loader )
	inline constexpr std::uint32_t XASSET_HEADER_SIZE[ASSET_TYPE_COUNT] =
	{
		0x44, 0x48, 0x58, 0x30, 0x168, 0x60, 0x10, 0x10, 0x64, 0xCC, 0x24, 0x0C, 0x88, 0x30, 0x100, 0x100, 0x10, 0x54,
		0x54, 0x2C, 0x74, 0x274, 0x10, 0, 0x18, 0x0C, 0x190, 0x08, 0xBFC, 0x04, 0x20, 0x08, 0, 0, 0, 0, 0x10, 0x10,
		0x18, 0x0C, 0x70, 0x314, 0x24, 0x0C,
	};

#define C1_SIZE(type, size)				static_assert(sizeof(type) == (size), #type " has the wrong size")
#define C1_OFFSET(type, member, offset)	static_assert(offsetof(type, member) == (offset), #type "::" #member " is at the wrong offset")
#define C1_HEADER(type, asset)			static_assert(sizeof(type) == XASSET_HEADER_SIZE[asset], #type " does not match the header size of " #asset)

#pragma pack(push, 1)

	// centre + half extents, like IW4 ( CODOL-AssetExplorer MapExtractor.ReadBounds: MapBounds.FromMidHalf )
	struct Bounds
	{
		float midPoint[3];
		float halfSize[3];
	};
	C1_SIZE(Bounds, 24);

	// ------------------------------------------------------------------------------------------
	// xmodel

	struct XModelLodInfo
	{
		float dist;
		std::uint16_t numsurfs;
		std::uint16_t surfIndex;
		zptr modelSurfs;				// XModelSurfs
		zptr surfsName;					// C1: name of the surfaces asset ( not in IW4 )
		std::uint32_t partBits[8];		// IW4: [6]
		zptr surfs;						// XSurface
		std::uint8_t _unk52[4];
	};
	C1_SIZE(XModelLodInfo, 56);			// IW4: 44
	C1_OFFSET(XModelLodInfo, numsurfs, 4);
	C1_OFFSET(XModelLodInfo, surfIndex, 6);
	C1_OFFSET(XModelLodInfo, modelSurfs, 8);
	C1_OFFSET(XModelLodInfo, surfsName, 12);
	C1_OFFSET(XModelLodInfo, partBits, 16);
	C1_OFFSET(XModelLodInfo, surfs, 48);

	struct XModel
	{
		zptr name;
		std::uint8_t numBones;
		std::uint8_t numRootBones;
		std::uint8_t numsurfs;
		std::uint8_t lodRampType;
		float scale;
		std::uint32_t noScalePartBits[8];	// IW4: [6]
		zptr boneNames;						// u16 script string per bone
		zptr parentList;					// u8 per non-root bone
		zptr quats;							// s16[4] per non-root bone
		zptr trans;							// float[3] per non-root bone
		zptr partClassification;			// u8 per bone
		zptr baseMat;						// DObjAnimMat per bone
		zptr materialHandles;				// Material* per surface
		XModelLodInfo lodInfo[4];
		std::uint8_t _unk296;
		std::uint8_t numLods;
		std::uint8_t _unk298;
		std::uint8_t lodFlags;				// byte 0x12B
		zptr collSurfs;
		std::int32_t numCollSurfs;
		std::int32_t contents;
		zptr boneInfo;						// 28 bytes per bone
		std::uint8_t _unk316[33];
		std::uint8_t flags;
		std::uint8_t _unk350[2];
		zptr physPreset;
		zptr physCollmap;
	};
	C1_HEADER(XModel, ASSET_TYPE_XMODEL);	// 0x168, IW4: 0x130
	C1_OFFSET(XModel, numBones, 4);
	C1_OFFSET(XModel, numRootBones, 5);
	C1_OFFSET(XModel, numsurfs, 6);
	C1_OFFSET(XModel, boneNames, 44);
	C1_OFFSET(XModel, parentList, 48);
	C1_OFFSET(XModel, quats, 52);
	C1_OFFSET(XModel, trans, 56);
	C1_OFFSET(XModel, partClassification, 60);
	C1_OFFSET(XModel, baseMat, 64);
	C1_OFFSET(XModel, materialHandles, 68);
	C1_OFFSET(XModel, lodInfo, 72);
	C1_OFFSET(XModel, numLods, 297);
	C1_OFFSET(XModel, lodFlags, 0x12B);
	C1_OFFSET(XModel, collSurfs, 300);
	C1_OFFSET(XModel, numCollSurfs, 304);
	C1_OFFSET(XModel, boneInfo, 312);
	C1_OFFSET(XModel, flags, 349);
	C1_OFFSET(XModel, physPreset, 352);
	C1_OFFSET(XModel, physCollmap, 356);

	struct XModelSurfs
	{
		zptr name;
		zptr surfs;						// XSurface
		std::uint16_t numsurfs;
		std::uint8_t _unk10[2];
		std::uint32_t partBits[8];		// IW4: [6]
		std::uint8_t _unk44[4];
	};
	C1_HEADER(XModelSurfs, ASSET_TYPE_XMODEL_SURFS);	// 0x30, IW4: 0x24
	C1_OFFSET(XModelSurfs, surfs, 4);
	C1_OFFSET(XModelSurfs, numsurfs, 8);
	C1_OFFSET(XModelSurfs, partBits, 12);

	struct XRigidVertList
	{
		std::uint16_t boneOffset;
		std::uint16_t vertCount;
		std::uint16_t triOffset;
		std::uint16_t triCount;
		zptr collisionTree;
	};
	C1_SIZE(XRigidVertList, 12);

	struct XSurface
	{
		std::uint8_t tileMode;
		std::uint8_t deformed;
		std::uint8_t flags;				// C1: 0x20 in every native surface ( not in IW4 )
		std::uint8_t _unk3;
		std::uint16_t vertCount;
		std::uint16_t triCount;
		std::uint32_t baseTriIndex;		// C1: 32 bit base indices
		std::uint32_t baseVertIndex;
		zptr triIndices;				// u16[3] per triangle
		std::int16_t vertInfoCount[4];	// vertices with 1 .. 4 bone weights
		zptr vertsBlend;				// u16 stream
		zptr verts0;					// GfxPackedVertex
		std::uint8_t _unk36[4];
		std::uint32_t vertListCount;
		zptr vertList;					// XRigidVertList
		std::uint8_t _unk48[4];
		std::uint32_t partBits[8];		// IW4: [6]
	};
	C1_SIZE(XSurface, 0x54);			// IW4: 0x40
	C1_OFFSET(XSurface, flags, 2);
	C1_OFFSET(XSurface, vertCount, 4);
	C1_OFFSET(XSurface, triCount, 6);
	C1_OFFSET(XSurface, baseTriIndex, 8);
	C1_OFFSET(XSurface, baseVertIndex, 12);
	C1_OFFSET(XSurface, triIndices, 16);
	C1_OFFSET(XSurface, vertInfoCount, 20);
	C1_OFFSET(XSurface, vertsBlend, 28);
	C1_OFFSET(XSurface, verts0, 32);
	C1_OFFSET(XSurface, vertListCount, 40);
	C1_OFFSET(XSurface, vertList, 44);
	C1_OFFSET(XSurface, partBits, 52);

	struct GfxPackedVertex
	{
		float xyz[3];
		float binormalSign;
		std::uint32_t color;			// rgba8
		std::uint16_t texCoordV;		// half floats, V first
		std::uint16_t texCoordU;
		std::uint32_t normal;			// packed unit vector
		std::uint32_t tangent;			// packed unit vector
	};
	C1_SIZE(GfxPackedVertex, 32);
	C1_OFFSET(GfxPackedVertex, binormalSign, 12);
	C1_OFFSET(GfxPackedVertex, color, 16);
	C1_OFFSET(GfxPackedVertex, texCoordV, 20);
	C1_OFFSET(GfxPackedVertex, texCoordU, 22);
	C1_OFFSET(GfxPackedVertex, normal, 24);
	C1_OFFSET(GfxPackedVertex, tangent, 28);

	// ------------------------------------------------------------------------------------------
	// material / techset / image

	inline constexpr int MATERIAL_TECHNIQUE_COUNT = 48;

	struct MaterialTextureDef
	{
		std::uint32_t nameHash;
		char nameStart;
		char nameEnd;
		std::uint8_t samplerState;
		std::uint8_t semantic;
		zptr image;						// GfxImage ( water_t for semantic 11 )
	};
	C1_SIZE(MaterialTextureDef, 12);
	C1_OFFSET(MaterialTextureDef, samplerState, 6);
	C1_OFFSET(MaterialTextureDef, semantic, 7);
	C1_OFFSET(MaterialTextureDef, image, 8);

	struct MaterialConstantDef
	{
		std::uint32_t nameHash;
		char name[12];
		float literal[4];
	};
	C1_SIZE(MaterialConstantDef, 32);
	C1_OFFSET(MaterialConstantDef, literal, 16);

	struct Material
	{
		zptr name;
		std::uint8_t gameFlags;
		std::uint8_t sortKey;
		std::uint8_t _unk6[10];
		std::uint32_t surfaceTypeBits;
		std::uint8_t _unk20[4];
		std::uint8_t stateBitsEntry[MATERIAL_TECHNIQUE_COUNT];
		std::uint8_t textureCount;
		std::uint8_t constantCount;
		std::uint8_t stateBitsCount;
		std::uint8_t _unk75;
		std::uint8_t cameraRegion;
		std::uint8_t _unk77[3];
		zptr techniqueSet;				// MaterialTechniqueSet
		zptr textureTable;				// MaterialTextureDef
		zptr constantTable;				// MaterialConstantDef
		zptr stateBitsTable;			// u32[2] per entry
	};
	C1_HEADER(Material, ASSET_TYPE_MATERIAL);	// 0x60, same layout as IW4
	C1_OFFSET(Material, sortKey, 5);
	C1_OFFSET(Material, surfaceTypeBits, 16);
	C1_OFFSET(Material, stateBitsEntry, 24);
	C1_OFFSET(Material, textureCount, 72);
	C1_OFFSET(Material, constantCount, 73);
	C1_OFFSET(Material, stateBitsCount, 74);
	C1_OFFSET(Material, cameraRegion, 76);
	C1_OFFSET(Material, techniqueSet, 80);
	C1_OFFSET(Material, textureTable, 84);
	C1_OFFSET(Material, constantTable, 88);
	C1_OFFSET(Material, stateBitsTable, 92);

	struct MaterialShaderArgument
	{
		std::uint16_t type;				// 0 material vertex const, 1 literal vertex const, 3 code vertex const,
										// 5 code pixel const, 6 material pixel const, 7 literal pixel const ..
		std::uint16_t dest;
		std::uint32_t value;			// literal pointer / code const / name hash
	};
	C1_SIZE(MaterialShaderArgument, 8);

	struct MaterialPass
	{
		zptr vertexDecl;
		zptr vertexShader;
		zptr pixelShader;
		std::uint8_t perPrimArgCount;
		std::uint8_t perObjArgCount;
		std::uint8_t stableArgCount;
		std::uint8_t customSamplerFlags;
		zptr args;						// MaterialShaderArgument
	};
	C1_SIZE(MaterialPass, 20);
	C1_OFFSET(MaterialPass, perPrimArgCount, 12);
	C1_OFFSET(MaterialPass, args, 16);

	struct MaterialTechnique
	{
		zptr name;
		std::uint16_t flags;
		std::uint16_t passCount;
		MaterialPass passArray[1];		// passCount entries
	};
	C1_OFFSET(MaterialTechnique, passCount, 6);
	C1_OFFSET(MaterialTechnique, passArray, 8);

	struct MaterialTechniqueSet
	{
		zptr name;
		std::uint8_t worldVertFormat;
		std::uint8_t _unk5[7];
		zptr techniques[MATERIAL_TECHNIQUE_COUNT];
	};
	C1_HEADER(MaterialTechniqueSet, ASSET_TYPE_TECHNIQUE_SET);	// 0xCC
	C1_OFFSET(MaterialTechniqueSet, worldVertFormat, 4);
	C1_OFFSET(MaterialTechniqueSet, techniques, 12);

	// pixel and vertex shader share one layout
	struct MaterialShader
	{
		zptr name;
		std::uint8_t _unk4[4];
		zptr program;					// DWORD bytecode
		std::uint16_t programSize;		// in DWORDs
		std::uint16_t loadForRenderer;
	};
	C1_HEADER(MaterialShader, ASSET_TYPE_PIXELSHADER);
	C1_HEADER(MaterialShader, ASSET_TYPE_VERTEXSHADER);
	C1_OFFSET(MaterialShader, program, 8);
	C1_OFFSET(MaterialShader, programSize, 12);
	C1_OFFSET(MaterialShader, loadForRenderer, 14);

	inline constexpr int MATERIAL_VERTEX_STREAM_MAX = 13;

	struct MaterialVertexDeclaration
	{
		zptr name;
		std::uint8_t streamCount;
		std::uint8_t hasOptionalSource;
		std::uint8_t _unk6[2];
		struct { std::uint8_t source; std::uint8_t dest; } routing[MATERIAL_VERTEX_STREAM_MAX];
		std::uint8_t _unk34[2];
		zptr decl[16];
	};
	C1_HEADER(MaterialVertexDeclaration, ASSET_TYPE_VERTEXDECL);	// 0x64
	C1_OFFSET(MaterialVertexDeclaration, streamCount, 4);
	C1_OFFSET(MaterialVertexDeclaration, routing, 8);

	struct GfxImageLoadDef
	{
		std::uint8_t levelCount;
		std::uint8_t _unk1[3];
		std::uint32_t flags;
		std::uint32_t format;			// D3DFORMAT
		std::uint32_t resourceSize;
		// resourceSize bytes of pixel data follow
	};
	C1_SIZE(GfxImageLoadDef, 16);
	C1_OFFSET(GfxImageLoadDef, flags, 4);
	C1_OFFSET(GfxImageLoadDef, format, 8);
	C1_OFFSET(GfxImageLoadDef, resourceSize, 12);

	// Pixels are either inline ( loadDef ) or streamed from images/<name>.iwi ( IWI version 9, formats 1 - 13,
	// 6 - 10 are wavelet coded ) inside the encrypted main\iw_image_NN.iwd archives.
	struct GfxImage
	{
		zptr loadDef;					// GfxImageLoadDef
		std::uint8_t mapType;
		std::uint8_t semantic;
		std::uint8_t category;			// C1: 9 / 10 where IW4 has 1 / 2
		std::uint8_t _unk7[13];
		std::uint16_t width;
		std::uint16_t height;
		std::uint16_t depth;
		std::uint8_t _unk26;
		std::uint8_t delayLoad;			// C1: 2 for images that are streamed from an .iwi ( not in IW4 )
		std::uint8_t _unk28[4];
		zptr name;
	};
	C1_HEADER(GfxImage, ASSET_TYPE_IMAGE);	// 0x24, IW4: 0x20 with the name at 0x1C
	C1_OFFSET(GfxImage, mapType, 4);
	C1_OFFSET(GfxImage, semantic, 5);
	C1_OFFSET(GfxImage, category, 6);
	C1_OFFSET(GfxImage, width, 20);
	C1_OFFSET(GfxImage, height, 22);
	C1_OFFSET(GfxImage, depth, 24);
	C1_OFFSET(GfxImage, delayLoad, 0x1B);
	C1_OFFSET(GfxImage, name, 32);

	// ------------------------------------------------------------------------------------------
	// xanim

	struct XAnimParts
	{
		zptr name;
		std::uint16_t dataByteCount;
		std::uint16_t dataShortCount;
		std::uint16_t dataIntCount;
		std::uint16_t randomDataByteCount;
		std::uint16_t randomDataIntCount;
		std::uint16_t numframes;
		std::uint8_t flags;
		std::uint8_t boneCount[10];
		std::uint8_t notifyCount;
		std::uint8_t assetType;
		std::uint8_t isDefault;
		std::uint8_t _unk30[2];
		std::uint32_t randomDataShortCount;
		std::uint32_t indexCount;
		float framerate;
		float frequency;
		zptr names;						// u16 script string per bone
		zptr dataByte;
		zptr dataShort;
		zptr dataInt;
		zptr randomDataShort;
		zptr randomDataByte;
		zptr randomDataInt;
		zptr indices;
		zptr notify;					// XAnimNotifyInfo
		zptr deltaPart;
	};
	C1_HEADER(XAnimParts, ASSET_TYPE_XANIMPARTS);	// 0x58, same layout as IW4
	C1_OFFSET(XAnimParts, numframes, 14);
	C1_OFFSET(XAnimParts, flags, 16);
	C1_OFFSET(XAnimParts, boneCount, 17);
	C1_OFFSET(XAnimParts, notifyCount, 27);
	C1_OFFSET(XAnimParts, assetType, 28);
	C1_OFFSET(XAnimParts, isDefault, 29);
	C1_OFFSET(XAnimParts, randomDataShortCount, 32);
	C1_OFFSET(XAnimParts, indexCount, 36);
	C1_OFFSET(XAnimParts, framerate, 40);
	C1_OFFSET(XAnimParts, frequency, 44);
	C1_OFFSET(XAnimParts, names, 48);
	C1_OFFSET(XAnimParts, indices, 76);
	C1_OFFSET(XAnimParts, notify, 80);
	C1_OFFSET(XAnimParts, deltaPart, 84);

	struct XAnimNotifyInfo
	{
		std::uint16_t name;				// script string
		std::uint8_t _unk2[2];
		float time;
	};
	C1_SIZE(XAnimNotifyInfo, 8);

	// ------------------------------------------------------------------------------------------
	// small assets

	struct LocalizeEntry
	{
		zptr value;
		zptr name;
	};
	C1_HEADER(LocalizeEntry, ASSET_TYPE_LOCALIZE_ENTRY);

	struct RawFile
	{
		zptr name;
		std::int32_t compressedLen;		// 0: stored
		std::int32_t len;
		zptr buffer;
	};
	C1_HEADER(RawFile, ASSET_TYPE_RAWFILE);

	struct StringTableCell
	{
		zptr string;
		std::int32_t hash;
	};
	C1_SIZE(StringTableCell, 8);

	struct StringTable
	{
		zptr name;
		std::int32_t columnCount;
		std::int32_t rowCount;
		zptr values;					// StringTableCell
	};
	C1_HEADER(StringTable, ASSET_TYPE_STRINGTABLE);

	// ------------------------------------------------------------------------------------------
	// map: entities

	struct MapTriggers
	{
		std::uint32_t count;
		zptr models;					// TriggerModel, referenced from entities as "model" "?N"
		std::uint32_t hullCount;
		zptr hulls;
		std::uint32_t slabCount;
		zptr slabs;
	};
	C1_SIZE(MapTriggers, 24);

	struct MapEnts
	{
		zptr name;
		zptr entityString;
		std::int32_t numEntityChars;	// length of the entity string including its terminator
		MapTriggers trigger;
		zptr stages;
		std::uint8_t stageCount;
		std::uint8_t _unk41[3];
	};
	C1_HEADER(MapEnts, ASSET_TYPE_MAP_ENTS);	// 0x2C, same layout as IW4
	C1_OFFSET(MapEnts, entityString, 4);
	C1_OFFSET(MapEnts, numEntityChars, 8);
	C1_OFFSET(MapEnts, trigger, 12);
	C1_OFFSET(MapEnts, stages, 36);
	C1_OFFSET(MapEnts, stageCount, 40);

	struct ComWorld
	{
		zptr name;
		std::int32_t isInUse;
		std::uint32_t primaryLightCount;
		zptr primaryLights;
	};
	C1_HEADER(ComWorld, ASSET_TYPE_COMWORLD);

	// ------------------------------------------------------------------------------------------
	// map: collision ( clipMap_t ), same layout as IW4

	struct cplane_s
	{
		float normal[3];
		float dist;
		std::uint8_t type;
		std::uint8_t signbits;
		std::uint8_t _unk18[2];
	};
	C1_SIZE(cplane_s, 20);

	struct cbrushside_t
	{
		zptr plane;
		std::uint16_t materialNum;
		std::uint8_t firstAdjacentSideOffset;
		std::uint8_t edgeCount;
	};
	C1_SIZE(cbrushside_t, 8);

	struct cbrush_t
	{
		std::uint16_t numsides;
		std::uint16_t glassPieceIndex;
		zptr sides;						// cbrushside_t, the non axial sides
		zptr baseAdjacentSide;
		std::int16_t axialMaterialNum[2][3];
		std::uint8_t firstAdjacentSideOffsets[2][3];
		std::uint8_t edgeCount[2][3];
	};
	C1_SIZE(cbrush_t, 36);
	C1_OFFSET(cbrush_t, sides, 4);
	C1_OFFSET(cbrush_t, baseAdjacentSide, 8);
	C1_OFFSET(cbrush_t, axialMaterialNum, 12);
	C1_OFFSET(cbrush_t, firstAdjacentSideOffsets, 24);
	C1_OFFSET(cbrush_t, edgeCount, 30);

	struct ClipMaterial
	{
		zptr name;
		std::int32_t surfaceFlags;
		std::int32_t contents;
	};
	C1_SIZE(ClipMaterial, 12);

	struct cStaticModel_s
	{
		zptr xmodel;
		float origin[3];
		float invScaledAxis[3][3];
		Bounds absBounds;
	};
	C1_SIZE(cStaticModel_s, 76);
	C1_OFFSET(cStaticModel_s, invScaledAxis, 16);
	C1_OFFSET(cStaticModel_s, absBounds, 52);

	struct cLeaf_t
	{
		std::uint16_t firstCollAabbIndex;
		std::uint16_t collAabbCount;
		std::int32_t brushContents;
		std::int32_t terrainContents;
		Bounds bounds;
		std::int32_t leafBrushNode;
	};
	C1_SIZE(cLeaf_t, 40);
	C1_OFFSET(cLeaf_t, bounds, 12);
	C1_OFFSET(cLeaf_t, leafBrushNode, 36);

	struct cmodel_t
	{
		Bounds bounds;
		float radius;
		cLeaf_t leaf;
	};
	C1_SIZE(cmodel_t, 68);
	C1_OFFSET(cmodel_t, leaf, 28);

	struct cLeafBrushNode_s
	{
		std::uint8_t axis;
		std::uint8_t _unk1;
		std::int16_t leafBrushCount;
		std::int32_t contents;
		std::uint8_t data[12];			// leaf: zptr brushes / node: float dist, float range, u16 childOffset[2]
	};
	C1_SIZE(cLeafBrushNode_s, 20);

	struct CollisionPartition
	{
		std::uint8_t triCount;
		std::uint8_t borderCount;
		std::uint8_t firstVertSegment;
		std::uint8_t _unk3;
		std::int32_t firstTri;
		zptr borders;
	};
	C1_SIZE(CollisionPartition, 12);

	struct CollisionAabbTree
	{
		float midPoint[3];
		std::uint16_t materialIndex;
		std::uint16_t childCount;
		float halfSize[3];
		std::int32_t u;					// firstChildIndex / partitionIndex
	};
	C1_SIZE(CollisionAabbTree, 32);
	C1_OFFSET(CollisionAabbTree, materialIndex, 12);
	C1_OFFSET(CollisionAabbTree, halfSize, 16);

	struct clipMap_t
	{
		zptr name;
		std::int32_t isInUse;
		std::int32_t planeCount;
		zptr planes;					// cplane_s
		std::uint32_t numStaticModels;
		zptr staticModelList;			// cStaticModel_s
		std::uint32_t numMaterials;
		zptr materials;					// ClipMaterial
		std::uint32_t numBrushSides;
		zptr brushsides;				// cbrushside_t
		std::uint32_t numBrushEdges;
		zptr brushEdges;				// u8
		std::uint32_t numNodes;
		zptr nodes;
		std::uint32_t numLeafs;
		zptr leafs;						// cLeaf_t
		std::uint32_t leafbrushNodesCount;
		zptr leafbrushNodes;			// cLeafBrushNode_s
		std::uint32_t numLeafBrushes;
		zptr leafbrushes;				// u16
		std::uint8_t _unk80[8];
		std::uint32_t vertCount;
		zptr verts;						// float[3]
		std::int32_t triCount;
		zptr triIndices;				// u16, relative to a 1024 vertex block per partition
		zptr triEdgeIsWalkable;
		std::int32_t borderCount;
		zptr borders;
		std::int32_t partitionCount;
		zptr partitions;				// CollisionPartition
		std::int32_t aabbTreeCount;
		zptr aabbTrees;					// CollisionAabbTree
		std::uint32_t numSubModels;
		zptr cmodels;					// cmodel_t
		std::uint16_t numBrushes;
		std::uint8_t _unk142[2];
		zptr brushes;					// cbrush_t
		zptr brushBounds;				// Bounds
		zptr brushContents;				// int
		zptr mapEnts;					// MapEnts
		std::uint32_t smodelNodeCount;
		zptr smodelNodes;
		std::uint16_t dynEntCount[2];
		zptr dynEntDefList[2];
		std::uint8_t _unk180[24];
		std::uint32_t checksum;
		std::uint8_t _unk208[48];
	};
	C1_HEADER(clipMap_t, ASSET_TYPE_CLIPMAP_MP);	// 0x100
	C1_HEADER(clipMap_t, ASSET_TYPE_CLIPMAP_SP);
	C1_OFFSET(clipMap_t, planeCount, 8);
	C1_OFFSET(clipMap_t, planes, 12);
	C1_OFFSET(clipMap_t, numStaticModels, 16);
	C1_OFFSET(clipMap_t, staticModelList, 20);
	C1_OFFSET(clipMap_t, numMaterials, 24);
	C1_OFFSET(clipMap_t, materials, 28);
	C1_OFFSET(clipMap_t, numBrushSides, 32);
	C1_OFFSET(clipMap_t, brushsides, 36);
	C1_OFFSET(clipMap_t, numBrushEdges, 40);
	C1_OFFSET(clipMap_t, brushEdges, 44);
	C1_OFFSET(clipMap_t, numNodes, 48);
	C1_OFFSET(clipMap_t, nodes, 52);
	C1_OFFSET(clipMap_t, numLeafs, 56);
	C1_OFFSET(clipMap_t, leafs, 60);
	C1_OFFSET(clipMap_t, leafbrushNodesCount, 64);
	C1_OFFSET(clipMap_t, leafbrushNodes, 68);
	C1_OFFSET(clipMap_t, numLeafBrushes, 72);
	C1_OFFSET(clipMap_t, leafbrushes, 76);
	C1_OFFSET(clipMap_t, vertCount, 88);
	C1_OFFSET(clipMap_t, verts, 92);
	C1_OFFSET(clipMap_t, triCount, 96);
	C1_OFFSET(clipMap_t, triIndices, 100);
	C1_OFFSET(clipMap_t, triEdgeIsWalkable, 104);
	C1_OFFSET(clipMap_t, borderCount, 108);
	C1_OFFSET(clipMap_t, borders, 112);
	C1_OFFSET(clipMap_t, partitionCount, 116);
	C1_OFFSET(clipMap_t, partitions, 120);
	C1_OFFSET(clipMap_t, aabbTreeCount, 124);
	C1_OFFSET(clipMap_t, aabbTrees, 128);
	C1_OFFSET(clipMap_t, numSubModels, 132);
	C1_OFFSET(clipMap_t, cmodels, 136);
	C1_OFFSET(clipMap_t, numBrushes, 140);
	C1_OFFSET(clipMap_t, brushes, 144);
	C1_OFFSET(clipMap_t, brushBounds, 148);
	C1_OFFSET(clipMap_t, brushContents, 152);
	C1_OFFSET(clipMap_t, mapEnts, 156);
	C1_OFFSET(clipMap_t, smodelNodeCount, 160);
	C1_OFFSET(clipMap_t, smodelNodes, 164);
	C1_OFFSET(clipMap_t, dynEntCount, 168);
	C1_OFFSET(clipMap_t, dynEntDefList, 172);
	C1_OFFSET(clipMap_t, checksum, 204);

	// ------------------------------------------------------------------------------------------
	// map: render world ( GfxWorld ), same layout as IW4

	struct GfxWorldVertex
	{
		float xyz[3];
		float binormalSign;
		std::uint32_t color;
		float texCoord[2];
		float lmapCoord[2];
		std::uint32_t normal;			// packed unit vector
		std::uint32_t tangent;			// packed unit vector
	};
	C1_SIZE(GfxWorldVertex, 44);
	C1_OFFSET(GfxWorldVertex, texCoord, 20);
	C1_OFFSET(GfxWorldVertex, lmapCoord, 28);
	C1_OFFSET(GfxWorldVertex, normal, 36);
	C1_OFFSET(GfxWorldVertex, tangent, 40);

	struct GfxSurface
	{
		std::uint32_t vertexLayerData;
		std::uint32_t firstVertex;
		std::uint16_t vertexCount;
		std::uint16_t triCount;
		std::uint32_t baseIndex;
		zptr material;
		std::uint8_t lightmapIndex;
		std::uint8_t reflectionProbeIndex;
		std::uint8_t primaryLightIndex;
		std::uint8_t flags;
	};
	C1_SIZE(GfxSurface, 24);
	C1_OFFSET(GfxSurface, vertexCount, 8);
	C1_OFFSET(GfxSurface, triCount, 10);
	C1_OFFSET(GfxSurface, baseIndex, 12);
	C1_OFFSET(GfxSurface, material, 16);
	C1_OFFSET(GfxSurface, lightmapIndex, 20);

	struct GfxStaticModelDrawInst
	{
		float origin[3];
		float axis[3][3];
		float scale;
		zptr model;						// XModel
		std::uint16_t cullDist;
		std::uint16_t lightingHandle;
		std::uint8_t reflectionProbeIndex;
		std::uint8_t primaryLightIndex;
		std::uint8_t flags;
		std::uint8_t firstMtlSkinIndex;
		std::uint8_t groundLighting[12];
	};
	C1_SIZE(GfxStaticModelDrawInst, 76);
	C1_OFFSET(GfxStaticModelDrawInst, axis, 12);
	C1_OFFSET(GfxStaticModelDrawInst, scale, 48);
	C1_OFFSET(GfxStaticModelDrawInst, model, 52);
	C1_OFFSET(GfxStaticModelDrawInst, cullDist, 56);
	C1_OFFSET(GfxStaticModelDrawInst, flags, 62);
	C1_OFFSET(GfxStaticModelDrawInst, groundLighting, 64);

	struct GfxStaticModelInst
	{
		Bounds bounds;
		float lightingOrigin[3];
	};
	C1_SIZE(GfxStaticModelInst, 36);

	struct GfxBrushModel
	{
		Bounds writableBounds;
		Bounds bounds;
		float radius;
		std::uint16_t surfaceCount;
		std::uint16_t startSurfIndex;
		std::uint16_t surfaceCountNoDecal;
		std::uint8_t _unk58[2];
	};
	C1_SIZE(GfxBrushModel, 60);
	C1_OFFSET(GfxBrushModel, radius, 48);
	C1_OFFSET(GfxBrushModel, surfaceCount, 52);
	C1_OFFSET(GfxBrushModel, startSurfIndex, 54);
	C1_OFFSET(GfxBrushModel, surfaceCountNoDecal, 56);

	struct GfxCell
	{
		Bounds bounds;
		std::int32_t portalCount;
		zptr portals;					// GfxPortal
		std::uint8_t reflectionProbeCount;
		std::uint8_t _unk33[3];
		zptr reflectionProbes;
	};
	C1_SIZE(GfxCell, 40);
	C1_OFFSET(GfxCell, portalCount, 24);
	C1_OFFSET(GfxCell, portals, 28);
	C1_OFFSET(GfxCell, reflectionProbeCount, 32);
	C1_OFFSET(GfxCell, reflectionProbes, 36);

	struct GfxPortal
	{
		std::uint8_t _unk0[28];
		zptr vertices;					// float[3]
		std::uint16_t cellIndex;
		std::uint8_t vertexCount;
		std::uint8_t _unk35[25];
	};
	C1_SIZE(GfxPortal, 60);
	C1_OFFSET(GfxPortal, vertices, 28);
	C1_OFFSET(GfxPortal, cellIndex, 32);
	C1_OFFSET(GfxPortal, vertexCount, 34);

	struct GfxWorldDraw
	{
		std::uint32_t reflectionProbeCount;
		zptr reflectionProbes;			// 12 bytes each
		zptr reflectionProbeOrigins;
		std::uint8_t _unk12[4];
		std::int32_t lightmapCount;
		zptr lightmaps;
		std::uint8_t _unk24[16];
		std::uint32_t vertexCount;
		zptr vertices;					// GfxWorldVertex
		std::uint8_t _unk48[4];
		std::uint32_t vertexLayerDataSize;
		zptr vertexLayerData;
		std::uint8_t _unk60[4];
		std::uint32_t indexCount;
		zptr indices;					// u16
	};
	C1_SIZE(GfxWorldDraw, 72);
	C1_OFFSET(GfxWorldDraw, lightmapCount, 16);
	C1_OFFSET(GfxWorldDraw, lightmaps, 20);
	C1_OFFSET(GfxWorldDraw, vertexCount, 40);
	C1_OFFSET(GfxWorldDraw, vertices, 44);
	C1_OFFSET(GfxWorldDraw, vertexLayerDataSize, 52);
	C1_OFFSET(GfxWorldDraw, vertexLayerData, 56);
	C1_OFFSET(GfxWorldDraw, indexCount, 64);
	C1_OFFSET(GfxWorldDraw, indices, 68);

	struct GfxWorldDpvsStatic
	{
		std::uint32_t smodelCount;
		std::uint32_t staticSurfaceCount;
		std::uint32_t staticSurfaceCountNoDecal;
		std::uint32_t litOpaqueSurfsBegin;
		std::uint32_t litOpaqueSurfsEnd;
		std::uint32_t litTransSurfsBegin;
		std::uint32_t litTransSurfsEnd;
		std::uint32_t shadowCasterSurfsBegin;
		std::uint32_t shadowCasterSurfsEnd;
		std::uint32_t emissiveSurfsBegin;
		std::uint32_t emissiveSurfsEnd;
		std::uint32_t smodelVisDataCount;
		std::uint32_t surfaceVisDataCount;
		zptr smodelVisData[3];
		zptr surfaceVisData[3];
		zptr sortedSurfIndex;			// u16
		zptr smodelInsts;				// GfxStaticModelInst
		zptr surfaces;					// GfxSurface
		zptr surfacesBounds;			// Bounds
		zptr smodelDrawInsts;			// GfxStaticModelDrawInst
		zptr surfaceMaterials;
		zptr surfaceCastsSunShadow;
		std::uint8_t _unk104[4];
	};
	C1_SIZE(GfxWorldDpvsStatic, 108);
	C1_OFFSET(GfxWorldDpvsStatic, smodelVisDataCount, 44);
	C1_OFFSET(GfxWorldDpvsStatic, surfaceVisDataCount, 48);
	C1_OFFSET(GfxWorldDpvsStatic, smodelVisData, 52);
	C1_OFFSET(GfxWorldDpvsStatic, sortedSurfIndex, 76);
	C1_OFFSET(GfxWorldDpvsStatic, smodelInsts, 80);
	C1_OFFSET(GfxWorldDpvsStatic, surfaces, 84);
	C1_OFFSET(GfxWorldDpvsStatic, surfacesBounds, 88);
	C1_OFFSET(GfxWorldDpvsStatic, smodelDrawInsts, 92);
	C1_OFFSET(GfxWorldDpvsStatic, surfaceMaterials, 96);
	C1_OFFSET(GfxWorldDpvsStatic, surfaceCastsSunShadow, 100);

	struct GfxWorld
	{
		zptr name;						// "maps/mp/<map>.d3dbsp"
		zptr baseName;
		std::int32_t planeCount;
		std::int32_t nodeCount;
		std::uint32_t surfaceCount;
		std::int32_t skyCount;
		zptr skies;
		std::uint32_t lastSunPrimaryLightIndex;
		std::uint32_t primaryLightCount;
		std::uint8_t _unk36[16];
		std::int32_t cellCount;
		std::uint8_t _unk56[20];
		zptr cells;						// GfxCell
		GfxWorldDraw draw;
		std::uint8_t lightGrid[56];
		std::int32_t modelCount;
		zptr models;					// GfxBrushModel
		Bounds bounds;
		std::uint32_t checksum;
		std::int32_t materialMemoryCount;
		zptr materialMemory;
		std::uint8_t sun[96];
		float outdoorLookupMatrix[4][4];
		zptr outdoorImage;
		std::uint8_t _unk416[40];
		GfxWorldDpvsStatic dpvs;
		std::uint8_t dpvsDyn[52];
		std::uint32_t heroOnlyLightCount;
		zptr heroOnlyLights;
		std::uint8_t _unk624[4];
	};
	C1_HEADER(GfxWorld, ASSET_TYPE_GFXWORLD);	// 0x274
	C1_OFFSET(GfxWorld, planeCount, 8);
	C1_OFFSET(GfxWorld, nodeCount, 12);
	C1_OFFSET(GfxWorld, surfaceCount, 16);
	C1_OFFSET(GfxWorld, skyCount, 20);
	C1_OFFSET(GfxWorld, skies, 24);
	C1_OFFSET(GfxWorld, lastSunPrimaryLightIndex, 28);
	C1_OFFSET(GfxWorld, primaryLightCount, 32);
	C1_OFFSET(GfxWorld, cellCount, 52);
	C1_OFFSET(GfxWorld, cells, 76);
	C1_OFFSET(GfxWorld, draw, 80);
	C1_OFFSET(GfxWorld, lightGrid, 152);
	C1_OFFSET(GfxWorld, modelCount, 208);
	C1_OFFSET(GfxWorld, models, 212);
	C1_OFFSET(GfxWorld, bounds, 216);
	C1_OFFSET(GfxWorld, checksum, 240);
	C1_OFFSET(GfxWorld, materialMemoryCount, 244);
	C1_OFFSET(GfxWorld, materialMemory, 248);
	C1_OFFSET(GfxWorld, sun, 252);
	C1_OFFSET(GfxWorld, outdoorLookupMatrix, 348);
	C1_OFFSET(GfxWorld, outdoorImage, 412);
	C1_OFFSET(GfxWorld, dpvs, 456);
	C1_OFFSET(GfxWorld, dpvsDyn, 564);
	C1_OFFSET(GfxWorld, heroOnlyLightCount, 616);
	C1_OFFSET(GfxWorld, heroOnlyLights, 620);

#pragma pack(pop)

#undef C1_SIZE
#undef C1_OFFSET
#undef C1_HEADER
}
