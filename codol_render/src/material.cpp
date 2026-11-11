#include "codol_render/material.hpp"

#include <algorithm>
#include <cstdio>

namespace codol::render
{
	namespace
	{
		// MaterialAsset.cs:103-116
		const char* const KNOWN_SAMPLERS[] =
		{
			"colorMap", "colorMap0", "colorMap1", "colorMap2", "colorMap3", "normalMap", "normalMap0", "normalMap1",
			"specularMap", "specularMap0", "specularMap1", "detailMap", "detailMap0", "detailMap1", "detailNormalMap",
			"alphaRevealMap", "glossMap", "cosinePowerMap", "occlusionMap", "reflectionProbe", "lightmapPrimary",
			"lightmapSecondary", "modelLightingLookup", "shadowmapSamplerSun", "shadowmapSamplerSpot", "attenuationMap",
			"dynamicShadowMap", "colorMapSampler", "normalMapSampler", "specularMapSampler", "detailMapSampler",
			"envMap", "cubeMap", "skyMap", "distortionMap", "flickerMap", "noiseMap", "maskMap", "heatMap",
			"revealMap", "burnMap", "fadeMap", "waterMap", "rainMap", "snowMap", "scrollMap", "emissiveMap",
			"selfIllumMap", "glowMap", "lookupMap", "rampMap", "sceneMap", "floatZSampler", "postEffect0",
			"displacementMap", "displacementMap0", "displacementMap1", "displacementMap2", "displacementMap3",
			"heightMap", "heightMap0", "parallaxMap", "tessellationMap", "dispMap",
			"postEffect1", "outdoorMap", "featherMap", "blurMap", "gradientMap", "decalMap", "overlayMap",
		};

		bool zone_string(const zone_view& zone, const std::uint32_t pointer, std::string& out)
		{
			out.clear();
			return pointer != 0 && zone.string && zone.string(pointer, out);
		}

		// MaterialExtractor.cs:117-136
		std::optional<embedded_image> read_embedded(const zone_view& zone, const std::uint32_t load_def, const int width, const int height,
			const int depth, const std::uint8_t map_type)
		{
			const byte_span header = load_def == 0 ? byte_span{} : zone.read(load_def, build::LOAD_DEF_HEADER_SIZE);
			if (header.empty())
			{
				return std::nullopt;
			}
			const std::uint32_t size = rd_u32(header.data() + build::LOAD_DEF_RESOURCE_SIZE);
			if (size == 0 || size > build::MAX_PRESERVED_IMAGE_BYTES)
			{
				return std::nullopt;
			}
			embedded_image image;
			image.d3d_format = rd_u32(header.data() + build::LOAD_DEF_FORMAT);
			image.level_count = header[build::LOAD_DEF_LEVEL_COUNT];
			image.flags = rd_u32(header.data() + build::LOAD_DEF_FLAGS);
			image.width = width;
			image.height = height;
			image.depth = std::max(depth, 1);
			image.map_type = map_type;
			image.data = load_def + build::LOAD_DEF_DATA;
			image.data_size = size;
			return image;
		}
	}

	std::string texture_semantic_name(const std::uint8_t semantic)
	{
		switch (semantic)
		{
		case texture_semantic::IDLE: return "2d";
		case texture_semantic::FUNCTION: return "function";
		case texture_semantic::COLOR_MAP: return "color";
		case texture_semantic::DETAIL_MAP: return "detail";
		case texture_semantic::NORMAL_MAP: return "normal";
		case texture_semantic::SPECULAR_MAP: return "specular";
		case texture_semantic::WATER_MAP: return "water";
		default: return "semantic" + std::to_string(semantic);
		}
	}

	std::uint32_t hash_sampler_name(const std::string_view name)
	{
		std::uint32_t hash = 0;
		for (const char c : name)
		{
			const auto lower = static_cast<std::uint32_t>(c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c) & 0xFF;
			hash = lower ^ (hash * 33);
		}
		return hash;
	}

	std::string sampler_name(const std::uint32_t hash)
	{
		for (const char* name : KNOWN_SAMPLERS)
		{
			if (hash_sampler_name(name) == hash)
			{
				return name;
			}
		}
		char text[24];
		std::snprintf(text, sizeof text, "hash_%08X", hash);
		return text;
	}

	const material_state::bits* material_state::pass_bits(const int type, const int pass) const
	{
		// GameMapRenderer.cs:1462-1474
		if (type < 0 || type >= build::TECHNIQUE_COUNT || pass < 0 || entries[static_cast<std::size_t>(type)] == 0xFF)
		{
			return nullptr;
		}
		const std::size_t entry = static_cast<std::size_t>(entries[static_cast<std::size_t>(type)]) + static_cast<std::size_t>(pass);
		return entry < table.size() ? &table[entry] : nullptr;
	}

	bool material_state::blends(const int type) const
	{
		const bits* b = pass_bits(type, 0);
		return b && (b->bits0 & 0x700) != 0;
	}

	const material_texture* material::find_texture(const std::uint32_t name_hash) const
	{
		for (const auto& t : textures)
		{
			if (t.name_hash == name_hash)
			{
				return &t;
			}
		}
		return nullptr;
	}

	const material_constant* material::find_constant(const std::uint32_t name_hash) const
	{
		for (const auto& c : constants)
		{
			if (c.name_hash == name_hash)
			{
				return &c;
			}
		}
		return nullptr;
	}

	const material_texture* material::find_by_semantic(const std::uint8_t semantic) const
	{
		for (const auto& t : textures)
		{
			if (t.semantic == semantic)
			{
				return &t;
			}
		}
		return nullptr;
	}

	std::optional<image_info> read_image(const zone_view& zone, const std::uint32_t pointer)
	{
		try
		{
			const byte_span data = pointer == 0 ? byte_span{} : zone.read(pointer, build::IMAGE_SIZE);
			if (data.empty())
			{
				return std::nullopt;
			}
			// MaterialExtractor.cs:97-102
			image_info info;
			std::string name;
			if (zone_string(zone, rd_u32(data.data() + build::IMAGE_NAME), name))
			{
				info.is_reference = is_reference_name(name);
				info.name = std::string(trim_reference(name));
			}
			info.width = rd_u16(data.data() + build::IMAGE_WIDTH);
			info.height = rd_u16(data.data() + build::IMAGE_HEIGHT);
			info.depth = rd_u16(data.data() + build::IMAGE_DEPTH);
			info.map_type = data[build::IMAGE_MAP_TYPE];
			info.embedded = read_embedded(zone, rd_u32(data.data() + build::IMAGE_LOAD_DEF), info.width, info.height, info.depth, info.map_type);
			return info;
		}
		catch (...)
		{
			return std::nullopt;
		}
	}

	bool read_material(const zone_view& zone, const std::uint32_t pointer, material& out, std::string* error)
	{
		try
		{
			// MaterialExtractor.cs:10-53
			out = {};
			const byte_span header = pointer == 0 ? byte_span{} : zone.read(pointer, build::MATERIAL_SIZE);
			if (header.empty())
			{
				set_error(error, "material " + hex32(pointer) + " is not loaded");
				return false;
			}
			const auto* m = header.data();
			out.pointer = pointer;
			std::string text;
			if (zone_string(zone, rd_u32(m + build::MATERIAL_NAME), text))
			{
				out.is_reference = is_reference_name(text);
				out.name = std::string(trim_reference(text));
			}
			else
			{
				char fallback[32];
				std::snprintf(fallback, sizeof fallback, "material_%08X", pointer);
				out.name = fallback;
			}
			out.game_flags = m[build::MATERIAL_GAME_FLAGS];
			out.sort_key = m[build::MATERIAL_SORT_KEY];
			out.camera_region = m[build::MATERIAL_CAMERA_REGION];
			out.state_flags = m[build::MATERIAL_STATE_FLAGS];
			out.surface_type_bits = rd_u32(m + build::MATERIAL_SURFACE_TYPE_BITS);

			const std::uint32_t techset = rd_u32(m + build::MATERIAL_TECHNIQUE_SET);
			if (techset != 0)
			{
				const byte_span techset_header = zone.read(techset, 4);
				if (!techset_header.empty())
				{
					out.technique_set = techset;
					if (zone_string(zone, rd_u32(techset_header.data()), text))
					{
						out.technique_set_name = std::string(trim_reference(text));
					}
				}
			}

			// MaterialExtractor.cs:17-27
			const std::uint32_t texture_count = m[build::MATERIAL_TEXTURE_COUNT];
			const std::uint32_t texture_table = rd_u32(m + build::MATERIAL_TEXTURE_TABLE);
			if (texture_count > 0 && texture_table != 0)
			{
				const byte_span defs = zone.read(texture_table, texture_count * build::TEXTURE_DEF_SIZE);
				if (!defs.empty())
				{
					out.textures.reserve(texture_count);
					for (std::uint32_t i = 0; i < texture_count; i++)
					{
						// MaterialExtractor.cs:78-115 ( ReadTextureDef )
						const auto* def = defs.data() + i * build::TEXTURE_DEF_SIZE;
						material_texture texture;
						texture.name_hash = rd_u32(def + build::TEXTURE_DEF_NAME_HASH);
						texture.semantic = def[build::TEXTURE_DEF_SEMANTIC];
						texture.sampler_state = def[build::TEXTURE_DEF_SAMPLER_STATE];
						texture.image = rd_u32(def + build::TEXTURE_DEF_IMAGE);
						if (texture.semantic != texture_semantic::WATER_MAP)
						{
							if (auto image = read_image(zone, texture.image))
							{
								texture.has_image = true;
								texture.image_name = std::move(image->name);
								texture.width = image->width;
								texture.height = image->height;
								texture.map_type = image->map_type;
								texture.embedded = image->embedded;
							}
						}
						out.textures.push_back(std::move(texture));
					}
				}
			}

			// MaterialExtractor.cs:29-42
			const std::uint32_t constant_count = m[build::MATERIAL_CONSTANT_COUNT];
			const std::uint32_t constant_table = rd_u32(m + build::MATERIAL_CONSTANT_TABLE);
			if (constant_count > 0 && constant_table != 0)
			{
				const byte_span defs = zone.read(constant_table, constant_count * build::CONSTANT_DEF_SIZE);
				if (!defs.empty())
				{
					out.constants.reserve(constant_count);
					for (std::uint32_t i = 0; i < constant_count; i++)
					{
						const auto* c = defs.data() + i * build::CONSTANT_DEF_SIZE;
						material_constant constant;
						const auto* name = reinterpret_cast<const char*>(c + build::CONSTANT_DEF_NAME);
						constant.name.assign(name, strnlen(name, 12));
						constant.name_hash = rd_u32(c + build::CONSTANT_DEF_NAME_HASH);
						for (int k = 0; k < 4; k++)
						{
							constant.value[k] = rd_f32(c + build::CONSTANT_DEF_LITERAL + 4 * k);
						}
						out.constants.push_back(std::move(constant));
					}
				}
			}

			// MaterialExtractor.cs:55-76 ( ReadState )
			const std::uint32_t state_count = m[build::MATERIAL_STATE_BITS_COUNT];
			const std::uint32_t state_table = rd_u32(m + build::MATERIAL_STATE_BITS_TABLE);
			if (state_count != 0 && state_table != 0)
			{
				const byte_span table = zone.read(state_table, 8 * state_count);
				if (!table.empty())
				{
					material_state state;
					state.sort_key = m[build::MATERIAL_SORT_KEY];
					state.camera_region = m[build::MATERIAL_CAMERA_REGION];
					std::memcpy(state.entries.data(), m + build::MATERIAL_STATE_BITS_ENTRY, build::TECHNIQUE_COUNT);
					state.table.resize(state_count);
					for (std::uint32_t i = 0; i < state_count; i++)
					{
						state.table[i] = { rd_u32(table.data() + 8 * i), rd_u32(table.data() + 8 * i + 4) };
					}
					out.state = std::move(state);
				}
			}
			return true;
		}
		catch (...)
		{
			set_error(error, "out of memory");
			return false;
		}
	}
}
