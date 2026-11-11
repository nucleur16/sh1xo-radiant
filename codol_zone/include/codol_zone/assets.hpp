#pragma once

// Typed, read only views of the assets of a loaded Zone.
//
// Every read_* function takes the image address of an asset header ( XAssetEntry::header, AssetRecord::header,
// Zone::find_asset or a pointer field of another asset ) and fills a view: the raw packed header ( c1:: layouts ),
// names, and spans / decoded copies of the arrays behind its pointers. Spans and string_views point into the
// Zone and stay valid as long as it lives. A function returns false when the address is not a loaded header of
// that type; it never throws and never reads outside the image.
//
// Asset pointers can lead out of the zone ( the asset is defined in another fast file ): such a pointer is a
// placeholder, Zone::reference() gives its type and name and the view marks it ( isReference / external ).

#include "codol_zone/zone.hpp"

#include <array>

namespace codol::zone
{
	// ---- small decoders -----------------------------------------------------------------------

	// PackedUnitVec ( GfxPackedVertex / GfxWorldVertex normal and tangent )
	void unpack_unit_vector(std::uint32_t packed, float out[3]);
	float half_to_float(std::uint16_t half);

	// ---- xmodel -------------------------------------------------------------------------------

	struct ModelBone
	{
		std::string_view name;
		int parent = -1;					// index, -1 for root bones
		float localQuat[4] = { 0, 0, 0, 1 };	// x y z w, relative to the parent ( roots: the base pose )
		float localTrans[3] = {};
		float baseQuat[4] = { 0, 0, 0, 1 };	// DObjAnimMat: model space bind pose
		float baseTrans[3] = {};
		float transWeight = 0;
		std::uint8_t partClassification = 0;
	};

	struct SurfaceVertex
	{
		float position[3];
		float normal[3];
		float tangent[3];
		float binormalSign;
		float uv[2];
		std::uint8_t color[4];				// r g b a
	};

	struct RigidVertList
	{
		std::uint16_t bone = 0;				// bone index ( boneOffset / 64 )
		std::uint16_t vertCount = 0;
		std::uint16_t triOffset = 0;
		std::uint16_t triCount = 0;
	};

	// up to four bone influences of one vertex of a blended ( skinned ) surface
	struct VertexWeights
	{
		std::uint8_t count = 0;
		std::uint16_t bone[4] = {};
		float weight[4] = {};
	};

	struct SurfaceView
	{
		zaddr address = 0;
		c1::XSurface raw{};
		std::uint32_t vertCount = 0;
		std::uint32_t triCount = 0;
		std::span<const std::uint8_t> vertexBytes;	// vertCount * 32: c1::GfxPackedVertex
		std::span<const std::uint8_t> indexBytes;	// triCount * 3 * 2: u16 vertex indices
		std::span<const std::uint8_t> rigidBytes;	// vertListCount * 12: c1::XRigidVertList

		std::vector<SurfaceVertex> vertices() const;			// decoded position, normal, tangent, uv, colour
		std::vector<std::uint16_t> indices() const;
		std::vector<RigidVertList> rigid_lists() const;
		// one entry per vertex: blended weights, the rigid lists' bones, or bone 0 for a static surface
		std::vector<VertexWeights> weights(const Zone& zone) const;
	};

	bool read_surface(const Zone& zone, zaddr surface, SurfaceView& view);

	struct ModelSurfsView
	{
		zaddr address = 0;
		c1::XModelSurfs raw{};
		std::string_view name;
		bool external = false;				// defined in another fast file or streamed from models/<name> in an .iwd
		std::vector<SurfaceView> surfaces;	// empty when external
	};

	bool read_model_surfs(const Zone& zone, zaddr modelSurfs, ModelSurfsView& view);

	struct ModelLod
	{
		float dist = 0;
		std::uint16_t numSurfs = 0;
		std::uint16_t surfIndex = 0;		// first entry of ModelView::materials used by this LOD
		std::string_view surfsName;
		ModelSurfsView surfs;
	};

	struct ModelView
	{
		zaddr address = 0;
		c1::XModel raw{};
		std::string_view name;
		bool isReference = false;			// only the name is in this zone
		std::vector<ModelBone> bones;
		std::vector<ModelLod> lods;
		std::vector<zaddr> materials;		// one per surface over all LODs ( raw.numsurfs )
		std::vector<std::string_view> materialNames;
	};

	// withGeometry = false skips the surfaces ( bones, LOD table and materials only )
	bool read_model(const Zone& zone, zaddr model, ModelView& view, bool withGeometry = true);

	// ---- material / techset / image ---------------------------------------------------------------

	struct MaterialTexture
	{
		c1::MaterialTextureDef raw{};
		zaddr image = 0;					// GfxImage header ( 0 for water maps, semantic 11 )
		std::string_view imageName;
	};

	struct MaterialView
	{
		zaddr address = 0;
		c1::Material raw{};
		std::string_view name;
		bool isReference = false;
		std::string_view techsetName;
		std::vector<MaterialTexture> textures;
		std::vector<c1::MaterialConstantDef> constants;
		std::vector<std::array<std::uint32_t, 2>> stateBits;	// GfxStateBits loadBits
	};

	bool read_material(const Zone& zone, zaddr material, MaterialView& view);

	struct TechniqueSetView
	{
		zaddr address = 0;
		c1::MaterialTechniqueSet raw{};
		std::string_view name;
		bool isReference = false;
	};

	bool read_techset(const Zone& zone, zaddr techset, TechniqueSetView& view);

	struct ImageView
	{
		zaddr address = 0;
		c1::GfxImage raw{};
		std::string_view name;
		bool isReference = false;
		bool streamed = true;				// no pixels in the zone: they come from images/<name>.iwi
		c1::GfxImageLoadDef loadDef{};		// valid when !streamed
		std::span<const std::uint8_t> pixels;	// loadDef.resourceSize bytes in loadDef.format ( D3DFORMAT )
	};

	bool read_image(const Zone& zone, zaddr image, ImageView& view);

	// ---- small assets ---------------------------------------------------------------------------

	struct RawFileView
	{
		zaddr address = 0;
		c1::RawFile raw{};
		std::string_view name;
		std::span<const std::uint8_t> stored;	// the buffer as stored ( zlib when raw.compressedLen != 0 )
	};

	bool read_rawfile(const Zone& zone, zaddr rawfile, RawFileView& view);
	// the file's contents ( inflated when it is stored compressed )
	Status rawfile_contents(const RawFileView& view, std::vector<std::uint8_t>& contents);

	struct StringTableView
	{
		zaddr address = 0;
		c1::StringTable raw{};
		std::string_view name;
		std::vector<std::string_view> cells;	// rowCount * columnCount, row major
		std::string_view cell(const int row, const int column) const
		{
			const std::size_t i = static_cast<std::size_t>(row) * static_cast<std::size_t>(raw.columnCount) + static_cast<std::size_t>(column);
			return row >= 0 && column >= 0 && column < raw.columnCount && i < cells.size() ? cells[i] : std::string_view();
		}
	};

	bool read_stringtable(const Zone& zone, zaddr table, StringTableView& view);

	struct LocalizeView
	{
		zaddr address = 0;
		std::string_view name;
		std::string_view value;
	};

	bool read_localize(const Zone& zone, zaddr entry, LocalizeView& view);

	struct AnimView
	{
		zaddr address = 0;
		c1::XAnimParts raw{};
		std::string_view name;
		bool isReference = false;
		std::uint32_t frameCount = 0;		// raw.numframes ( the last frame index ) + 1
		std::vector<std::string_view> boneNames;	// raw.boneCount[9] entries
		std::vector<std::pair<std::string_view, float>> notetracks;
	};

	bool read_anim(const Zone& zone, zaddr anim, AnimView& view);

	// ---- map ------------------------------------------------------------------------------------

	struct MapEntsView
	{
		zaddr address = 0;
		c1::MapEnts raw{};
		std::string_view name;
		std::string_view entityString;		// without the terminator ( raw.numEntityChars - 1 characters )
		std::uint32_t triggerModelCount = 0;
		std::uint32_t triggerHullCount = 0;
		std::uint32_t triggerSlabCount = 0;
	};

	bool read_map_ents(const Zone& zone, zaddr mapEnts, MapEntsView& view);
	// first MapEnts of the zone ( top level or nested in the clipMap ); 0 when the zone has none
	zaddr find_map_ents(const Zone& zone);
	// number of "{" blocks of an entity string
	std::uint32_t count_entities(std::string_view entityString);

	struct ComWorldView
	{
		zaddr address = 0;
		c1::ComWorld raw{};
		std::string_view name;
		std::span<const std::uint8_t> primaryLightBytes;	// raw.primaryLightCount * primaryLightStride
		std::uint32_t primaryLightStride = 0;
	};

	bool read_com_world(const Zone& zone, zaddr comWorld, ComWorldView& view);

	struct BrushSide
	{
		c1::cplane_s plane{};
		std::int32_t planeIndex = -1;		// index into ClipMapView::planes, -1 for the six axial sides
		std::uint16_t materialNum = 0;		// index into ClipMapView::materials
	};

	struct Brush
	{
		c1::cbrush_t raw{};
		c1::Bounds bounds{};
		std::int32_t contents = 0;
		// the six axial sides ( -x +x -y +y -z +z from the bounds ) followed by raw.numsides stored sides
		std::vector<BrushSide> sides;
	};

	struct ClipStaticModel
	{
		c1::cStaticModel_s raw{};
		std::string_view modelName;
	};

	struct ClipMaterialView
	{
		std::string_view name;
		std::int32_t surfaceFlags = 0;
		std::int32_t contents = 0;
	};

	struct ClipMapView
	{
		zaddr address = 0;
		c1::clipMap_t raw{};
		std::string_view name;
		std::vector<c1::cplane_s> planes;
		std::vector<ClipMaterialView> materials;
		std::vector<Brush> brushes;
		std::vector<c1::cmodel_t> subModels;
		std::vector<ClipStaticModel> staticModels;
		zaddr mapEnts = 0;
	};

	bool read_clip_map(const Zone& zone, zaddr clipMap, ClipMapView& view);

	struct WorldSurface
	{
		c1::GfxSurface raw{};
		std::string_view materialName;
	};

	struct WorldStaticModel
	{
		c1::GfxStaticModelDrawInst raw{};	// origin, axis[3][3], scale, model
		std::string_view modelName;
	};

	struct WorldCell
	{
		c1::GfxCell raw{};
		std::vector<c1::GfxPortal> portals;
	};

	struct GfxWorldView
	{
		zaddr address = 0;
		c1::GfxWorld raw{};
		std::string_view name;
		std::string_view baseName;
		std::span<const std::uint8_t> vertexBytes;	// raw.draw.vertexCount * 44: c1::GfxWorldVertex
		std::span<const std::uint8_t> indexBytes;	// raw.draw.indexCount * 2: u16
		std::vector<WorldSurface> surfaces;			// raw.surfaceCount
		std::vector<WorldStaticModel> staticModels;	// raw.dpvs.smodelCount
		std::vector<c1::GfxBrushModel> brushModels;	// raw.modelCount
		std::vector<WorldCell> cells;				// raw.cellCount

		std::vector<c1::GfxWorldVertex> vertices() const;
		std::vector<std::uint16_t> indices() const;
	};

	bool read_gfx_world(const Zone& zone, zaddr gfxWorld, GfxWorldView& view);

	// ---- lookup helpers -------------------------------------------------------------------------

	// header of the first asset of that type registered by the zone ( top level or nested ), 0 when none
	zaddr first_asset(const Zone& zone, std::int32_t type);
}
