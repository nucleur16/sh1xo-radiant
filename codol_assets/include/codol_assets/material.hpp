#pragma once

// Material definition -> native 1.1.1.13 Material.
//
// A Material is, for the most part, decided by its technique set: which samplers and constants it has to supply,
// which of the 48 technique slots have state bits and how the state of each slot follows from the material's own
// state. None of that is in a file of the install ( the state maps of the technique sources are gone ), so the
// compiler learns it from the shipped materials: MaterialLibrary scans the fast files of an install once and keeps,
// per technique set,
//
//   * the texture table shape: sampler name hash, first / last character, semantic, and the image shipped
//     materials bind when they have none of their own ( $identitynormalmap, $white .. );
//   * the constant table shape with the most common values;
//   * every distinct state ( stateBitsEntry[48] + state bits table + stateFlags ) that a shipped material of the
//     technique set has, with how many materials use it and which sort key / game flags / camera region /
//     go with it;
//   * where the technique set is defined ( an always loaded zone: a reference by name is enough; else a zone to
//     copy it from ).
//
// compile_material() then picks the shipped state that agrees with what the definition asks for ( blend, alpha
// test, cull, depth write .. as decoded from the slot the surface is drawn with ) and, when no shipped material has
// that combination, derives it from the nearest one field by field.

#include "codol_assets/bitmap.hpp"
#include "codol_zone/c1_layouts.hpp"
#include "codol_zone/zone_editor.hpp"

#include <array>
#include <map>
#include <optional>

namespace codol::assets
{
	// GfxStateBits pair as one value: loadBits[0] in the low half, loadBits[1] in the high half
	using StateBits = std::uint64_t;

	struct MaterialTextureRef
	{
		std::string sampler;				// "colorMap", "normalMap", "specularMap", "detailMap" ..
		std::string image;					// image name ( without path and extension )
		int samplerState = -1;				// MaterialTextureDef::samplerState; -1: what shipped materials of the technique set use
		int semantic = -1;					// -1: that of the technique set's slot
	};

	struct MaterialConstant
	{
		std::string name;					// up to 12 characters
		float value[4] = {};
	};

	struct MaterialDef
	{
		std::string name;
		std::string techset;				// explicit technique set; empty: from `type` and the maps present
		std::string type = "world phong";	// world phong | model phong | world unlit | model unlit | 2d | sky | effect
		// what the definition asks of the state the surface is drawn with; bits outside the mask follow the
		// technique set's most common shipped state
		StateBits stateMask = 0;
		StateBits stateValue = 0;
		std::string blendFunc;				// as written ( for the technique set name ): replace, blend, add, multiply, screen
		std::string alphaTest;
		int sortKey = -1;					// -1: most common of the chosen state
		int gameFlags = -1;
		int cameraRegion = -1;
		std::uint32_t surfaceTypeBits = 0;
		int atlasRows = 1;
		int atlasColumns = 1;
		std::vector<MaterialTextureRef> textures;
		std::vector<MaterialConstant> constants;
		// source files of the images, by sampler ( GDT entries name files, not images ); empty in a .mtl
		std::map<std::string, std::string> imageFiles;
	};

	// ---- definitions as text ------------------------------------------------------------------------------------
	//   material "mc/my_crate"
	//   {
	//       type "model phong"            // or: techset mc_l_sm_r0c0n0s0
	//       colorMap crate_col            // [ tile | clamp | clampu | clampv ] [ nomip | bilinear | trilinear | aniso ]
	//       normalMap crate_nml
	//       specularMap crate_spc
	//       blendFunc replace             // replace | blend | add | multiply | screen
	//       alphaTest none                // none | gt0 | lt128 | ge128
	//       cullFace back                 // back | front | none
	//       depthWrite on                 // on | off
	//       depthTest lessequal           // lessequal | less | equal | always | disable
	//       polygonOffset none            // none | decal | impact
	//       sort 1                        // number, or a name of the table in the README
	//       surfaceType wood              // name or 0x.. bits
	//       gameFlags 0x52
	//       constant envMapParms 0.8 2 1 0.6
	//   }
	// Comments: // to the end of the line. Several materials per file.
	Status parse_material_text(std::string_view text, std::vector<MaterialDef>& out);
	std::string write_material_text(const MaterialDef& def);

	// A CoD4 GDT file: every entry whose definition file is material.gdf. Keys read: materialType, colorMap,
	// normalMap, specularMap, cosinePowerMap ( ignored ), detailMap, blendFunc, alphaTest, cullFace, depthTest,
	// depthWrite, polygonOffset, sort, surfaceType, tileColor / tileNormal / tileSpecular, filterColor ..,
	// envMapMin / envMapMax / envMapExponent, colorTint, detailScaleX / Y.
	Status parse_gdt_materials(std::string_view text, std::vector<MaterialDef>& out);

	// ---- what the install teaches -------------------------------------------------------------------------------

	struct LibrarySampler
	{
		std::uint32_t nameHash = 0;
		char nameStart = 0;
		char nameEnd = 0;
		std::uint8_t semantic = 0;
		std::uint8_t samplerState = 0;		// most common
		std::string defaultImage;			// most common image of materials that bind no image of the slot's semantic
	};

	struct LibraryConstant
	{
		std::uint32_t nameHash = 0;
		char name[12] = {};
		float value[4] = {};				// most common
	};

	struct LibraryState
	{
		std::array<std::uint8_t, 48> entries{};
		std::vector<StateBits> table;
		std::uint8_t stateFlags = 0;
		std::uint32_t count = 0;				// shipped materials with this state
		std::map<std::uint8_t, std::uint32_t> sortKeys, gameFlags, cameraRegions;
		std::map<std::uint16_t, std::uint32_t> sortToRegion;	// ( sort key << 8 | camera region ) -> count

		int ref_slot() const;					// the slot whose bits are the material's own state; -1: none
		StateBits ref() const;
	};

	struct LibraryTechset
	{
		std::string name;
		std::string commonZone;				// file name of an always loaded zone that defines it; empty: none
		std::string donorZone;				// path ( relative to the install ) of the smallest zone that defines it
		std::uint64_t donorSize = 0;
		std::uint32_t materials = 0;
		std::vector<LibrarySampler> samplers;	// in table order ( ascending name hash )
		std::vector<LibraryConstant> constants;
		std::vector<LibraryState> states;		// most common first
	};

	class MaterialLibrary
	{
	public:
		// loads every fast file below <install>/client_shipRetail ( chinese, patch ); takes a quarter of a minute
		Status scan(const std::filesystem::path& install, const zone::ClientKey& key);
		// the scan as a text file, so that it is done once
		Status save(const std::filesystem::path& file) const;
		Status load(const std::filesystem::path& file);
		// load( cache ) when it exists and was made from this install, else scan + save
		Status open(const std::filesystem::path& install, const zone::ClientKey& key, const std::filesystem::path& cache);

		const LibraryTechset* find(std::string_view techset) const;
		const std::map<std::string, LibraryTechset, std::less<>>& techsets() const { return techsets_; }
		// camera region shipped materials of any technique set have with this sort key ( most common )
		int region_of_sort(int sortKey) const;
		// where an image / technique set that is not part of the output zone lives ( always loaded zone ), for names
		// like $identitynormalmap; empty when unknown
		std::string image_zone(std::string_view image) const;

		// one shipped material, added to the statistics ( scan() calls this; tests build small libraries with it )
		void add(const zone::Zone& zone, zone::zaddr material, bool commonZone);
		void finish();

	private:
		struct Raw;
		std::map<std::string, LibraryTechset, std::less<>> techsets_;
		std::map<int, std::map<int, std::uint32_t>> sortRegions_;
		std::map<std::string, std::string, std::less<>> commonImages_;
		std::shared_ptr<Raw> raw_;
		std::string install_;
	};

	// ---- compiling --------------------------------------------------------------------------------------------

	struct CompiledMaterial
	{
		std::string name;
		std::string techset;
		c1::Material header{};				// pointers unset
		std::vector<c1::MaterialTextureDef> textures;		// image pointers unset
		std::vector<std::string> images;					// per texture: image name
		std::vector<c1::MaterialConstantDef> constants;
		std::vector<StateBits> stateBits;
		bool stateShipped = false;			// the state is one a shipped material of the technique set has
		std::vector<std::string> notes;		// what was assumed
	};

	// technique set name for a definition without an explicit one
	std::string techset_for(const MaterialDef& def);

	// excludeOne: a state of the library that is not to be used when only one shipped material has it - the
	// material under test itself ( leave one out, for the verification )
	Status compile_material(const MaterialDef& def, const MaterialLibrary& library, CompiledMaterial& out, const LibraryState* excludeOne = nullptr);

	// The definition a shipped material would have been made from: technique set, the state of its own slot, sort,
	// flags, textures, constants.
	bool extract_material(const zone::Zone& zone, zone::zaddr material, MaterialDef& out);

	// MaterialTextureDef / MaterialConstantDef name hash
	std::uint32_t material_name_hash(std::string_view name);

	// surface type name -> bit ( "wood", "metal" .. the 1.1.1.13 table ); 0 when unknown
	std::uint32_t surface_type_bits(std::string_view name);
}
