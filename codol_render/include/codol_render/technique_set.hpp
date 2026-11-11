#pragma once

// MaterialTechniqueSet -> techniques -> passes -> shader arguments of a 1.1.1.13 zone.
// Port of Codol.Assets\Rendering\TechniqueSet.cs, RenderLayouts.cs and the readers of RenderAssetLibrary.cs.
//
// What is not ported from RenderAssetLibrary: the name keyed caches over several zones ( the draw loop / zone
// loader owns those ). A shader, declaration or techset whose stored name starts with ',' is a reference to an
// asset of another zone; the readers return it with `is_reference` set and the name without the comma, and the
// caller looks it up by name.

#include "build_1_1_1_13.hpp"
#include "common.hpp"

#include <array>
#include <optional>
#include <vector>

namespace codol::render
{
	// ---------------------------------------------------------------------------------------------------
	// technique types
	//
	// The 48 slots of MaterialTechniqueSet::techniques / Material::stateBitsEntry. 1.1.1.13 has the IW4 list
	// ( Codol.FastFiles\Porting\V3ToV1\MaterialTechniqueSetConverter.cs:7-12: "the IW4 MaterialTechniqueType
	// list: 0 depth prepass .. 9/10 lit/lit dfog .. 37-39 light spot/omni/spot shadow, 44 wireframe solid,
	// 46/47 debug bumpmap" ). The names are OpenAssetTools' IW4 MaterialTechniqueType.
	//   VERIFIED by the C# renderer on real maps: 1, 2, 3, 4, 5, 10, 12, 14, 16, 18, 20, 22 ( the constants below )
	//   and by that comment: 0, 9, 10, 37, 38, 39, 44, 46, 47.
	//   NOT individually verified ( they follow from the IW4 order between verified anchors ): every other slot.
	enum technique_type : int
	{
		TECHNIQUE_DEPTH_PREPASS = 0,
		TECHNIQUE_BUILD_FLOAT_Z = 1,
		TECHNIQUE_BUILD_SHADOWMAP_DEPTH = 2,
		TECHNIQUE_BUILD_SHADOWMAP_COLOR = 3,
		TECHNIQUE_UNLIT = 4,
		TECHNIQUE_EMISSIVE = 5,
		TECHNIQUE_EMISSIVE_DFOG = 6,
		TECHNIQUE_EMISSIVE_SHADOW = 7,
		TECHNIQUE_EMISSIVE_SHADOW_DFOG = 8,
		TECHNIQUE_LIT = 9,
		TECHNIQUE_LIT_DFOG = 10,
		TECHNIQUE_LIT_SUN = 11,
		TECHNIQUE_LIT_SUN_DFOG = 12,
		TECHNIQUE_LIT_SUN_SHADOW = 13,
		TECHNIQUE_LIT_SUN_SHADOW_DFOG = 14,
		TECHNIQUE_LIT_SPOT = 15,
		TECHNIQUE_LIT_SPOT_DFOG = 16,
		TECHNIQUE_LIT_SPOT_SHADOW = 17,
		TECHNIQUE_LIT_SPOT_SHADOW_DFOG = 18,
		TECHNIQUE_LIT_OMNI = 19,
		TECHNIQUE_LIT_OMNI_DFOG = 20,
		TECHNIQUE_LIT_OMNI_SHADOW = 21,
		TECHNIQUE_LIT_OMNI_SHADOW_DFOG = 22,
		TECHNIQUE_LIT_INSTANCED = 23,
		TECHNIQUE_LIT_INSTANCED_DFOG = 24,
		TECHNIQUE_LIT_INSTANCED_SUN = 25,
		TECHNIQUE_LIT_INSTANCED_SUN_DFOG = 26,
		TECHNIQUE_LIT_INSTANCED_SUN_SHADOW = 27,
		TECHNIQUE_LIT_INSTANCED_SUN_SHADOW_DFOG = 28,
		TECHNIQUE_LIT_INSTANCED_SPOT = 29,
		TECHNIQUE_LIT_INSTANCED_SPOT_DFOG = 30,
		TECHNIQUE_LIT_INSTANCED_SPOT_SHADOW = 31,
		TECHNIQUE_LIT_INSTANCED_SPOT_SHADOW_DFOG = 32,
		TECHNIQUE_LIT_INSTANCED_OMNI = 33,
		TECHNIQUE_LIT_INSTANCED_OMNI_DFOG = 34,
		TECHNIQUE_LIT_INSTANCED_OMNI_SHADOW = 35,
		TECHNIQUE_LIT_INSTANCED_OMNI_SHADOW_DFOG = 36,
		TECHNIQUE_LIGHT_SPOT = 37,
		TECHNIQUE_LIGHT_OMNI = 38,
		TECHNIQUE_LIGHT_SPOT_SHADOW = 39,
		TECHNIQUE_FAKELIGHT_NORMAL = 40,
		TECHNIQUE_FAKELIGHT_VIEW = 41,
		TECHNIQUE_SUNLIGHT_PREVIEW = 42,
		TECHNIQUE_CASE_TEXTURE = 43,
		TECHNIQUE_WIREFRAME_SOLID = 44,
		TECHNIQUE_WIREFRAME_SHADED = 45,
		TECHNIQUE_DEBUG_BUMPMAP = 46,
		TECHNIQUE_DEBUG_BUMPMAP_INSTANCED = 47,
		TECHNIQUE_COUNT = build::TECHNIQUE_COUNT,
	};

	// "depth prepass", "build floatz", .. ( the names the IW4 tools use ); "" for an index out of range
	const char* technique_type_name(int type);

	// The slots the C# renderer draws with. It names them without "dfog": its "lit" is the fog blended slot of
	// each pair, and choose_technique falls back to slot - 1 ( the plain one ) when a techset lacks it.
	namespace renderer_technique
	{
		inline constexpr int BUILD_FLOAT_Z = 1;				// GameMapRenderer.cs:39
		inline constexpr int BUILD_SHADOWMAP_DEPTH = 2;		// GameMapRenderer.SpotShadow.cs:23
		inline constexpr int BUILD_SHADOWMAP_COLOR = 3;		// GameMapRenderer.SunShadow.cs:12
		inline constexpr int UNLIT = 4;						// GameMapRenderer.cs:36
		inline constexpr int EMISSIVE = 5;					// GameMapRenderer.cs:37
		inline constexpr int LIT = 10;						// GameMapRenderer.cs:38
		inline constexpr int LIT_SUN = 12;					// GameMapRenderer.cs:40
		inline constexpr int LIT_SUN_SHADOW = 14;			// GameMapRenderer.cs:41
		inline constexpr int LIT_SPOT = 16;					// GameMapRenderer.cs:42
		inline constexpr int LIT_SPOT_SHADOW = 18;			// GameMapRenderer.SpotShadow.cs:11
		inline constexpr int LIT_OMNI = 20;					// GameMapRenderer.cs:43
		inline constexpr int LIT_OMNI_SHADOW = 22;			// GameMapRenderer.SpotShadow.cs:12
	}

	// ---------------------------------------------------------------------------------------------------
	// shader arguments

	// RenderLayouts.cs:60-70
	enum class shader_argument_type : std::uint16_t
	{
		material_vertex_const = 0,
		literal_vertex_const = 1,
		material_pixel_sampler = 2,
		code_vertex_const = 3,
		code_pixel_sampler = 4,
		code_pixel_const = 5,
		material_pixel_const = 6,
		literal_pixel_const = 7,
	};

	// TechniqueSet.cs:75-88
	struct shader_argument
	{
		shader_argument_type type = shader_argument_type::material_vertex_const;
		std::uint16_t dest = 0;				// register: c# of the stage, or s# for the sampler types
		std::uint32_t value = 0;			// literal pointer / code constant / name hash / code sampler index
		float literal[4] = {};				// the float4 behind `value` of the literal types ( 0 when not loaded )

		// code constants: index in the low word, first row and row count of a matrix in the high bytes
		int code_index() const { return static_cast<int>(value & 0xFFFF); }
		int first_row() const { return static_cast<int>((value >> 16) & 0xFF); }
		int row_count() const { const int rows = static_cast<int>(value >> 24); return rows > 0 ? rows : 1; }
		std::uint32_t name_hash() const { return value; }	// material constants and samplers

		bool is_vertex_stage() const
		{
			return type == shader_argument_type::material_vertex_const || type == shader_argument_type::literal_vertex_const
				|| type == shader_argument_type::code_vertex_const;
		}
		bool is_sampler() const
		{
			return type == shader_argument_type::material_pixel_sampler || type == shader_argument_type::code_pixel_sampler;
		}
		bool is_literal() const
		{
			return type == shader_argument_type::literal_vertex_const || type == shader_argument_type::literal_pixel_const;
		}
		bool is_code_const() const
		{
			return type == shader_argument_type::code_vertex_const || type == shader_argument_type::code_pixel_const;
		}
	};

	// ---------------------------------------------------------------------------------------------------
	// vertex declarations

	// TechniqueSet.cs:16: source = which vertex member, dest = which shader input ( see vertex_format.hpp )
	struct vertex_routing
	{
		std::uint8_t source = 0;
		std::uint8_t dest = 0;
		bool operator==(const vertex_routing&) const = default;
	};

	struct vertex_declaration
	{
		std::uint32_t pointer = 0;			// the MaterialVertexDeclaration, 0 when built from a name
		std::string name;					// without the reference comma
		bool is_reference = false;			// stored name was ",name": `routing` then comes from parse_vertex_declaration_name
		bool has_optional_source = false;
		std::vector<vertex_routing> routing;

		// TechniqueSet.cs:28: "source:dest,source:dest,.." - a key for caching IDirect3DVertexDeclaration9
		std::string key() const;
	};

	// TechniqueSet.cs:31-73 ( VertexDeclarationNames.TryParse ): the routing a declaration name spells, e.g.
	// "pp0nn0t0t0" -> position->position, normal->normal, texcoord0->texcoord0. A leading ',' is ignored.
	// std::nullopt when the name does not parse.
	std::optional<std::vector<vertex_routing>> parse_vertex_declaration_name(std::string_view name);

	// RenderAssetLibrary.cs:424-435 ( ReadDeclaration ) + :524-550 ( ResolveDeclaration ). For a ",name" reference
	// the routing is parsed from the name; std::nullopt when the pointer is not loaded or such a name does not parse.
	std::optional<vertex_declaration> read_vertex_declaration(const zone_view& zone, std::uint32_t pointer);

	// ---------------------------------------------------------------------------------------------------
	// shaders

	// A MaterialVertexShader / MaterialPixelShader header ( RenderAssetLibrary.cs:371-398, :513-522 )
	struct shader_ref
	{
		std::uint32_t pointer = 0;
		std::string name;					// without the reference comma; "shader_XXXXXXXX" when it has none
		bool has_name = false;				// false: the name pointer is null or not loaded ( `name` is the stand-in )
		bool is_reference = false;			// stored name was ",name": the program is in another zone
		bool is_pixel_shader = false;
		std::uint32_t program = 0;			// zone pointer to the byte code, 0 when absent
		std::uint32_t program_size = 0;		// in bytes ( 4 * MaterialShader::programSize )

		bool has_program() const { return !is_reference && program != 0 && program_size != 0; }
	};

	std::optional<shader_ref> read_shader(const zone_view& zone, std::uint32_t pointer, bool pixel_shader);

	// The byte code of a shader ( zone.read of its program ); empty when it is not loaded
	byte_span shader_code(const zone_view& zone, const shader_ref& shader);

	// ---------------------------------------------------------------------------------------------------
	// passes, techniques, technique sets

	// TechniqueSet.cs:90-109
	struct render_pass
	{
		std::optional<vertex_declaration> declaration;	// nullopt: null / not loaded / unparsable reference name
		std::optional<shader_ref> vertex_shader;
		std::optional<shader_ref> pixel_shader;
		std::uint8_t per_prim_arg_count = 0;
		std::uint8_t per_obj_arg_count = 0;
		std::uint8_t stable_arg_count = 0;
		// bit 0: s1 = reflection probe, bit 1: s2 = primary lightmap, bit 2: s3 = secondary lightmap
		// ( GameMapRenderer.cs:1723-1738, see code_tables.hpp custom_sampler )
		std::uint8_t custom_sampler_flags = 0;
		std::vector<shader_argument> arguments;		// per primitive, then per object, then stable
	};

	// TechniqueSet.cs:111-118
	struct render_technique
	{
		std::uint32_t pointer = 0;
		std::string name;					// "technique_XXXXXXXX" when it has none
		std::uint16_t flags = 0;
		std::vector<render_pass> passes;
	};

	// TechniqueSet.cs:120-129
	struct render_technique_set
	{
		std::uint32_t pointer = 0;
		std::string name;					// as stored ( with the comma of a reference ); "techset_XXXXXXXX" when it has none
		std::uint8_t world_vert_format = 0;
		// Distinct techniques; several slots can share one ( RenderAssetLibrary.cs:449-456 )
		std::vector<render_technique> techniques;
		// slot -> index into `techniques`, -1 when the slot is empty or its pointer is not loaded
		std::array<std::int8_t, build::TECHNIQUE_COUNT> slots{};

		render_technique_set() { slots.fill(-1); }

		const render_technique* technique(const int type) const
		{
			return type >= 0 && type < build::TECHNIQUE_COUNT && slots[type] >= 0 ? &techniques[slots[type]] : nullptr;
		}
		bool has(const int type) const { return technique(type) != nullptr; }
		// bit t set when slot t holds a technique ( the input of choose_technique )
		std::uint64_t slot_mask() const;
	};

	// RenderAssetLibrary.cs:467-511 ( ReadTechnique )
	bool read_technique(const zone_view& zone, std::uint32_t pointer, render_technique& out, std::string* error = nullptr);

	// RenderAssetLibrary.cs:437-465 ( ReadSet ). A techset stored as a ",name" reference has no techniques
	// of its own: the caller resolves `name` ( trim_reference ) in the zone that defines it.
	bool read_technique_set(const zone_view& zone, std::uint32_t pointer, render_technique_set& out, std::string* error = nullptr);
}
