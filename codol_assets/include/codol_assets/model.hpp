#pragma once

// xmodel_export -> native 1.1.1.13 XModel with its surfaces.
//
// Source: the text format the CoD exporters write and CODOL-AssetExplorer writes back ( MODEL / VERSION 6: bones
// with world offset and axes, vertices with bone weights, triangles with per corner normal, colour and uv, objects,
// materials ). One file per LOD.
//
// What the compiler makes of it, as the shipped models have it ( survey of 13024 models / 28506 surfaces ):
//   * one XSurface per material of a LOD; vertices are the distinct ( position, normal, colour, uv ) corners,
//     GfxPackedVertex: position as floats in model space, uv as half floats, normal and tangent as packed unit
//     vectors, the sign of the binormal;
//   * a surface whose vertices all follow one bone each is rigid: vertices and triangles sorted by bone, one
//     XRigidVertList per bone; a surface with weighted vertices is "deformed": vertices sorted by their number of
//     influences ( vertInfoCount[4] ), weights in the vertsBlend stream, no rigid lists;
//   * bones: roots first, the local rotation as four s16, the local translation, the model space bind pose
//     ( DObjAnimMat with transWeight 2 ), part classification 0;
//   * per bone bounds ( XBoneInfo: centre, half size, squared radius of the vertices that follow the bone ), model
//     bounds and radius;
//   * LOD distances, the collision LOD with its collision surfaces ( 48 byte triangle records ), contents.
// Not written: XSurfaceCollisionTree of the rigid lists ( 44 % of the shipped static surfaces have one; it serves
// decal projection ), physics preset / collision map.

#include "codol_assets/bitmap.hpp"
#include "codol_zone/assets.hpp"
#include "codol_zone/zone_editor.hpp"

#include <map>

namespace codol::assets
{
	struct ExportBone
	{
		std::string name;
		int parent = -1;
		float offset[3] = {};								// model space
		float axis[3][3] = { { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 } };	// rows X Y Z: the bone's axes in model space
	};

	struct ExportVertex
	{
		float position[3] = {};
		std::vector<std::pair<int, float>> weights;		// bone, weight; in file order
	};

	struct ExportCorner
	{
		int vertex = 0;
		float normal[3] = { 0, 0, 1 };
		float color[4] = { 1, 1, 1, 1 };
		float uv[2] = {};
	};

	struct ExportFace
	{
		int object = 0;
		int material = 0;
		ExportCorner corners[3];
	};

	struct ExportModel
	{
		std::vector<ExportBone> bones;
		std::vector<ExportVertex> vertices;
		std::vector<ExportFace> faces;
		std::vector<std::string> objects;
		std::vector<std::string> materials;
	};

	Status parse_xmodel_export(std::string_view text, ExportModel& out);
	Status load_xmodel_export(const std::filesystem::path& file, ExportModel& out);
	std::string write_xmodel_export(const ExportModel& model, std::string_view comment = {});

	// One LOD of a shipped model as an export: bones in the bind pose, the vertices of all surfaces, one material
	// per surface ( the real material names ). The inverse of compile_model.
	bool extract_model_lod(const zone::Zone& zone, const zone::ModelView& model, std::size_t lod, ExportModel& out);

	struct ModelLodSource
	{
		ExportModel mesh;
		float distance = 1000000.0f;		// the LOD is drawn up to this distance; the last LOD of shipped models has 1000000 or its own
	};

	struct ModelOptions
	{
		std::string name;
		std::vector<ModelLodSource> lods;	// 1 .. 4, nearest first
		// export material name -> material asset name; names that are not listed are used as they are
		std::map<std::string, std::string> materialNames;
		int collisionLod = -1;				// LOD whose triangles become the collision surfaces; -1: no collision ( contents 0 )
		std::int32_t contents = 1;			// of the collision surfaces: 1 solid ( the shipped props )
		std::int32_t collisionSurfaceFlags = 0;
		// -1: decide from the mesh. 0 / 1: corner order as in the file / reversed.
		int reverseWinding = -1;
	};

	struct CompiledSurface
	{
		std::string material;
		c1::XSurface header{};						// pointers unset
		std::vector<c1::GfxPackedVertex> vertices;
		std::vector<std::uint16_t> indices;
		std::vector<c1::XRigidVertList> rigid;
		std::vector<std::uint16_t> blend;			// vertsBlend stream
	};

	struct CompiledLod
	{
		float distance = 0;
		std::string surfsName;
		std::uint32_t partBits[8] = {};
		std::vector<CompiledSurface> surfaces;
	};

	struct CompiledCollisionSurface
	{
		c1::Bounds bounds{};
		std::int32_t bone = 0;
		std::int32_t contents = 0;
		std::int32_t surfaceFlags = 0;
		std::vector<std::array<float, 12>> triangles;	// plane, s vector, t vector
	};

	struct CompiledModel
	{
		std::string name;
		std::vector<std::string> boneNames;
		std::uint8_t rootBones = 0;
		std::vector<std::uint8_t> parentList;			// per non root bone: index - parent
		std::vector<std::int16_t> quats;				// 4 per non root bone
		std::vector<float> trans;						// 3 per non root bone
		std::vector<std::uint8_t> partClassification;
		std::vector<float> baseMat;						// 8 per bone: quat, trans, transWeight
		std::vector<float> boneInfo;					// 7 per bone: centre, half size, squared radius
		std::vector<CompiledLod> lods;
		std::vector<CompiledCollisionSurface> collision;
		int collisionLod = -1;
		std::int32_t contents = 0;
		c1::Bounds bounds{};
		float radius = 0;
		std::vector<std::string> notes;

		std::vector<std::string> materials() const;		// one per surface over all LODs
	};

	Status compile_model(const ModelOptions& options, CompiledModel& out);

	// ---- packing ( inverse of codol_zone's decoders ) -------------------------------------------------------
	std::uint32_t pack_unit_vector(const float v[3]);
	std::uint16_t float_to_half(float value);
}
