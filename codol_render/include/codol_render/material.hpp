#pragma once

// Material of a 1.1.1.13 zone: textures, constants, state bits.
// Port of Codol.Assets\Models\MaterialExtractor.cs and Codol.Assets\Assets\MaterialAsset.cs.

#include "build_1_1_1_13.hpp"
#include "common.hpp"

#include <array>
#include <optional>
#include <vector>

namespace codol::render
{
	// MaterialAsset.cs:64-121 ( TextureSemantics ): MaterialTextureDef::semantic
	namespace texture_semantic
	{
		inline constexpr std::uint8_t IDLE = 0;			// "2d"
		inline constexpr std::uint8_t FUNCTION = 1;
		inline constexpr std::uint8_t COLOR_MAP = 2;
		inline constexpr std::uint8_t DETAIL_MAP = 3;
		inline constexpr std::uint8_t NORMAL_MAP = 5;
		inline constexpr std::uint8_t SPECULAR_MAP = 8;
		inline constexpr std::uint8_t WATER_MAP = 11;	// the def's pointer is a water_t, not a GfxImage
	}

	// "2d", "function", "color", "detail", "normal", "specular", "water", else "semanticN" ( MaterialAsset.cs:76-86 )
	std::string texture_semantic_name(std::uint8_t semantic);

	// MaterialAsset.cs:88-96: the hash MaterialTextureDef::nameHash / MaterialConstantDef::nameHash /
	// material arguments use: hash = lower( c ) ^ ( hash * 33 ), starting at 0
	std::uint32_t hash_sampler_name(std::string_view name);

	// MaterialAsset.cs:98-121: the sampler name of a hash for the names the C# tool knows, else "hash_XXXXXXXX"
	std::string sampler_name(std::uint32_t hash);

	// MaterialExtractor.cs:117-136 ( ReadEmbedded ): pixels a zone carries in a GfxImageLoadDef
	struct embedded_image
	{
		std::uint32_t d3d_format = 0;		// D3DFORMAT
		int level_count = 0;
		std::uint32_t flags = 0;			// GfxImageLoadDef::flags ( IWI flag bits )
		int width = 0;
		int height = 0;
		int depth = 1;
		std::uint8_t map_type = 0;
		std::uint32_t data = 0;				// zone pointer to the pixels
		std::uint32_t data_size = 0;
	};

	// MaterialAsset.cs:5-26 ( MaterialTextureAsset )
	struct material_texture
	{
		std::uint32_t name_hash = 0;
		std::uint8_t semantic = 0;
		std::uint8_t sampler_state = 0;		// see render_state.hpp sampler_bits
		std::uint32_t image = 0;			// MaterialTextureDef::image as stored
		// Empty `image_name` + has_image false: a water map ( semantic 11 ) or an image that is not loaded; the
		// C# renderer then binds its white texture ( GameMapRenderer.cs:1581-1586 ).
		bool has_image = false;
		std::string image_name;				// without the reference comma: the streamed file is images/<name>.iwi
		int width = 0;
		int height = 0;
		std::uint8_t map_type = 0;
		std::optional<embedded_image> embedded;
	};

	// MaterialAsset.cs:31
	struct material_constant
	{
		std::string name;					// up to 12 characters, as stored
		std::uint32_t name_hash = 0;
		float value[4] = {};
	};

	// MaterialAsset.cs:33-36 ( MaterialStateBits )
	struct material_state
	{
		std::uint8_t sort_key = 0;
		std::uint8_t camera_region = 0;
		// per technique type the index of its first pass in `table`, 0xFF: none. Pass p of a technique uses
		// table[ entries[type] + p ] ( GameMapRenderer.cs:1462-1474 ).
		std::array<std::uint8_t, build::TECHNIQUE_COUNT> entries{};
		struct bits
		{
			std::uint32_t bits0;
			std::uint32_t bits1;
		};
		std::vector<bits> table;

		// MaterialAsset.cs:35: a camera region >= 4 ( CAMERA_REGION_NONE ) is never drawn
		bool not_drawn() const { return camera_region >= build::CAMERA_REGION_COUNT; }

		// The state bits of pass `pass` of technique `type`, nullptr when the material has none ( the renderer
		// then leaves the render states as they are: GameMapRenderer.cs:1465-1473 )
		const bits* pass_bits(int type, int pass) const;

		// GameMapRenderer.cs:1353-1361: the first pass of the technique blends ( transparent draw range )
		bool blends(int type) const;
	};

	// MaterialAsset.cs:38-62 ( MaterialAsset )
	struct material
	{
		std::uint32_t pointer = 0;
		std::string name;					// without the reference comma; "material_XXXXXXXX" when it has none
		bool is_reference = false;			// stored as ",name": defined in another zone, nothing else is valid
		std::uint8_t game_flags = 0;
		std::uint8_t state_flags = 0;
		std::uint32_t surface_type_bits = 0;
		std::uint32_t technique_set = 0;	// zone pointer for read_technique_set, 0 when not loaded
		std::string technique_set_name;		// without the reference comma, empty when not loaded
		std::vector<material_texture> textures;
		std::vector<material_constant> constants;
		std::optional<material_state> state;	// nullopt: no state bits table

		const material_texture* find_texture(std::uint32_t name_hash) const;	// GameMapRenderer.cs:1572-1580
		const material_constant* find_constant(std::uint32_t name_hash) const;	// GameMapRenderer.cs:1533-1541
		const material_texture* find_by_semantic(std::uint8_t semantic) const;	// MaterialAsset.cs:55
	};

	// MaterialExtractor.cs:10-53 ( Read ). Fails only when the Material itself is not loaded.
	bool read_material(const zone_view& zone, std::uint32_t pointer, material& out, std::string* error = nullptr);

	// MaterialExtractor.cs:138-145 ( ReadEmbeddedImage ) / the image part of ReadTextureDef, for a GfxImage that is
	// not reached through a material ( lightmaps, reflection probes, the outdoor image ).
	struct image_info
	{
		std::string name;					// without the reference comma
		bool is_reference = false;
		int width = 0;
		int height = 0;
		int depth = 0;
		std::uint8_t map_type = 0;
		std::optional<embedded_image> embedded;
	};
	std::optional<image_info> read_image(const zone_view& zone, std::uint32_t pointer);
}
