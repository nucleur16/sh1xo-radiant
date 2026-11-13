// lighting_check <game root> <map name>
//
// Verifies the model lighting port ( light_grid / world_sight_trace ) on a real map: loads
// <game root>\client_shipRetail\chinese\<map>.ff, samples the light grid at the lighting origin of every static
// model of the GfxWorld that is not flagged 0x20 ( the same set Codol.Rendering\GameMapRenderer.cs:1057-1126
// BuildModelLighting samples, without the script_model entities ) and prints counts, a hash over all results
// and the first samples. The output must be identical for a Win32 and an x64 build.
//
// lighting_check <game root> <map name> <x> <y> <z> [<radius> <step>]
//
// Samples around one point instead, printing what `CodolSoundDumper lightgrid-sample --game .. --map .. --pos x,y,z
// [--radius r --step s]` prints for its samples ( the lines from "  at .." to the end of the first table ), so
// the two can be diffed.

#include "light_grid.hpp"
#include "sight_trace.hpp"

#include "codol_zone/assets.hpp"

#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

namespace
{
	using namespace codol;
	using zone::zaddr;

	view::float3 read_vector(const zone::Zone& z, const zaddr address)
	{
		return { z.get<float>(address), z.get<float>(address + 4), z.get<float>(address + 8) };
	}

	// MapExtractor.cs:852-883 ReadLights
	std::vector<view::map_light> read_lights(const zone::Zone& z)
	{
		std::vector<view::map_light> lights;
		const zaddr com_world = zone::first_asset(z, c1::ASSET_TYPE_COMWORLD);
		if (com_world == 0)
		{
			return lights;
		}
		const std::uint32_t count = z.get<std::uint32_t>(com_world + 8);		// ComWorldLayout.PrimaryLightCount
		const zaddr array = z.get<zaddr>(com_world + 12);						// ComWorldLayout.PrimaryLights
		if (count == 0 || count > (1u << 24) || z.bytes(array, static_cast<std::size_t>(count) * 68).empty())
		{
			return lights;
		}
		for (std::uint32_t i = 0; i < count; i++)
		{
			const zaddr l = array + 68 * i;
			view::map_light light;
			light.index = static_cast<int>(i);
			light.type = z.get<std::uint8_t>(l);
			light.color = read_vector(z, l + 4);
			light.direction = read_vector(z, l + 16);
			light.origin = read_vector(z, l + 28);
			light.radius = z.get<float>(l + 40);
			light.cos_half_fov_outer = z.get<float>(l + 44);
			light.cos_half_fov_inner = z.get<float>(l + 48);
			light.can_use_shadow_map = z.get<std::uint8_t>(l + 1) != 0;
			light.exponent = z.get<std::uint8_t>(l + 2);
			light.cos_half_fov_expanded = z.get<float>(l + 52);
			light.rotation_limit = z.get<float>(l + 56);
			light.def_name = std::string(z.c_string(z.get<zaddr>(l + 64)));
			lights.push_back(std::move(light));
		}
		return lights;
	}

	struct fnv1a
	{
		std::uint64_t value = 0xCBF29CE484222325ull;

		void add(const std::uint32_t v)
		{
			for (int i = 0; i < 4; i++)
			{
				value ^= (v >> (8 * i)) & 0xFF;
				value *= 0x100000001B3ull;
			}
		}
	};

	// The sample part of `CodolSoundDumper lightgrid-sample --pos x,y,z` ( Codol.SoundDumper\LightGridCommands.cs:82-108 ),
	// in the same text format, to diff the port against the C# at arbitrary points ( model light 0 ).
	void probe_point(const view::light_grid& grid, const std::vector<view::map_light>& lights, const int last_sun_light,
		const view::light_grid::blocked_fn& blocked, const view::float3 pos, const int radius, const int step)
	{
		for (const float dz : { 8.0f, 36.0f, 60.0f })
		{
			const view::float3 at = pos + view::float3{ 0, 0, dz };
			const view::light_grid_sample s = grid.sample(at, 0, lights, last_sun_light, blocked);
			const std::uint32_t c = view::light_grid::ground_lighting(grid.blend(s), s.sun_visibility);
			std::printf("  at %.0f,%.0f,%.0f (+%.0f): sun visibility %3u, primary light %d, ambient rgb %u,%u,%u%s\n", static_cast<double>(at.x),
				static_cast<double>(at.y), static_cast<double>(at.z), static_cast<double>(dz), s.sun_visibility, s.primary_light, c & 0xFF, (c >> 8) & 0xFF,
				(c >> 16) & 0xFF, s.is_fallback ? " (fallback: no grid entries here)" : "");
		}
		std::printf("\n");
		std::printf("sun visibility around the point at chest height (+36), %d-unit steps; rows +Y at top, columns +X to the right, [ ] = the point\n", step);
		std::printf("        ");
		for (int dx = -radius; dx <= radius; dx += step)
		{
			std::printf("%5d", dx);
		}
		std::printf("\n");
		for (int dy = radius; dy >= -radius; dy -= step)
		{
			std::printf("%6d  ", dy);
			for (int dx = -radius; dx <= radius; dx += step)
			{
				const view::light_grid_sample s = grid.sample(pos + view::float3{ static_cast<float>(dx), static_cast<float>(dy), 36.0f }, 0, lights,
					last_sun_light, blocked);
				const std::string v = s.is_fallback ? "-" : std::to_string(s.sun_visibility);
				std::printf(dx == 0 && dy == 0 ? "[%3s]" : "%5s", v.c_str());
			}
			std::printf("\n");
		}
	}

	int run(const std::filesystem::path& root, const std::string& map, const view::float3* probe, const int radius, const int step)
	{
		zone::ClientKey key;
		if (const zone::Status s = key.load_from_exe(root / "C1.exe"); !s)
		{
			std::fprintf(stderr, "error: %s\n", s.message.c_str());
			return 1;
		}
		zone::Zone z;
		if (const zone::Status s = z.open(root / "client_shipRetail" / "chinese" / (map + ".ff"), key); !s)
		{
			std::fprintf(stderr, "error: %s\n", s.message.c_str());
			return 1;
		}
		const zaddr world = zone::first_asset(z, c1::ASSET_TYPE_GFXWORLD);
		if (world == 0)
		{
			std::fprintf(stderr, "error: the zone has no GfxWorld\n");
			return 1;
		}
		const auto grid = view::light_grid::read(z, world);
		if (!grid)
		{
			std::fprintf(stderr, "error: the light grid could not be read\n");
			return 1;
		}
		const std::vector<view::map_light> lights = read_lights(z);
		view::world_sight_trace trace(z);
		const int last_sun_light = static_cast<int>(z.get<std::uint32_t>(world + 28));	// GfxWorldLayout.LastSunPrimaryLightIndex

		std::printf("map: %s\n", map.c_str());
		std::printf("light grid: rowAxis %d colAxis %d mins %u %u %u maxs %u %u %u lightRegions %d\n", grid->row_axis(), grid->col_axis(),
			grid->mins()[0], grid->mins()[1], grid->mins()[2], grid->maxs()[0], grid->maxs()[1], grid->maxs()[2], grid->has_light_regions() ? 1 : 0);
		std::printf("light grid: %d entries, %d colour blocks\n", grid->entry_count(), grid->color_count());
		std::printf("primary lights: %zu, lastSunPrimaryLightIndex %d\n", lights.size(), last_sun_light);
		std::printf("sight trace: %d brushes, %d triangles\n", trace.brush_count(), trace.triangle_count());

		// MapLayouts.cs:64 GfxWorldLayout.Dpvs = 456, :121-143 GfxWorldDpvsStaticLayout
		const zaddr dpvs = world + 456;
		const std::uint32_t smodel_count = z.get<std::uint32_t>(dpvs + 0);
		const zaddr insts = z.get<zaddr>(dpvs + 80);			// GfxStaticModelInst, 36 bytes
		const zaddr draw_insts = z.get<zaddr>(dpvs + 92);		// GfxStaticModelDrawInst, 76 bytes
		const auto inst_bytes = z.bytes(insts, static_cast<std::size_t>(smodel_count) * 36);
		const auto draw_bytes = z.bytes(draw_insts, static_cast<std::size_t>(smodel_count) * 76);
		if (smodel_count != 0 && (inst_bytes.empty() || draw_bytes.empty()))
		{
			std::fprintf(stderr, "error: the static model arrays are not loaded zone data\n");
			return 1;
		}

		const view::light_grid::blocked_fn blocked = [&trace](const view::float3 start, const view::float3 end) { return trace.blocked(start, end); };
		if (probe)
		{
			probe_point(*grid, lights, last_sun_light, blocked, *probe, radius, step);
			return 0;
		}
		fnv1a hash;
		int samples = 0, fallbacks = 0;
		std::vector<std::string> first;
		for (std::uint32_t i = 0; i < smodel_count; i++)
		{
			const std::uint8_t model_light = draw_bytes[static_cast<std::size_t>(i) * 76 + 61];
			const std::uint8_t flags = draw_bytes[static_cast<std::size_t>(i) * 76 + 62];
			if ((flags & 0x20) != 0)
			{
				continue;
			}
			view::float3 origin;
			std::memcpy(&origin, inst_bytes.data() + static_cast<std::size_t>(i) * 36 + 24, 12);
			const view::light_grid_sample s = grid->sample(origin, model_light, lights, last_sun_light, blocked);
			const auto colors = grid->blend(s);
			const std::uint32_t ground = view::light_grid::ground_lighting(colors, s.sun_visibility);
			hash.add(ground);
			hash.add(static_cast<std::uint32_t>(s.primary_light));
			samples++;
			fallbacks += s.is_fallback ? 1 : 0;
			if (first.size() < 10)
			{
				char text[512];
				int n = std::snprintf(text, sizeof(text), "  smodel %u origin (%.3f %.3f %.3f) modelLight %u -> primaryLight %d sun %u fallback %d ground %08X colours",
					i, static_cast<double>(origin.x), static_cast<double>(origin.y), static_cast<double>(origin.z), model_light, s.primary_light,
					s.sun_visibility, s.is_fallback ? 1 : 0, ground);
				for (int k = 0; k < s.count && n > 0 && n < static_cast<int>(sizeof(text)); k++)
				{
					n += std::snprintf(text + n, sizeof(text) - static_cast<std::size_t>(n), " %u*%u", s.color_indices[static_cast<std::size_t>(k)],
						s.weights[static_cast<std::size_t>(k)]);
				}
				first.emplace_back(text);
			}
		}

		std::printf("static models: %u, sampled ( flags & 0x20 == 0 ): %d, fallback samples: %d\n", smodel_count, samples, fallbacks);
		std::printf("traces: %d, blocked: %d\n", trace.traces(), trace.blocked_traces());
		std::printf("hash: %016" PRIX64 "\n", hash.value);
		std::printf("first samples:\n");
		for (const std::string& line : first)
		{
			std::printf("%s\n", line.c_str());
		}
		return 0;
	}
}

int main(const int argc, char** argv)
{
	if (argc != 3 && argc != 6 && argc != 8)
	{
		std::fprintf(stderr, "usage: lighting_check <game root> <map name> [<x> <y> <z> [<radius> <step>]]\n"
			"  without a point: samples every static model of the map\n"
			"  with a point: the samples of `CodolSoundDumper lightgrid-sample --pos x,y,z --radius r --step s`\n");
		return 2;
	}
	try
	{
		if (argc == 3)
		{
			return run(std::filesystem::path(argv[1]), argv[2], nullptr, 0, 0);
		}
		const view::float3 pos{ std::strtof(argv[3], nullptr), std::strtof(argv[4], nullptr), std::strtof(argv[5], nullptr) };
		const int radius = argc == 8 ? std::atoi(argv[6]) : 96;
		const int step = argc == 8 ? std::atoi(argv[7]) : 16;
		if (radius < 0 || step <= 0)
		{
			std::fprintf(stderr, "error: radius must be >= 0 and step > 0\n");
			return 2;
		}
		return run(std::filesystem::path(argv[1]), argv[2], &pos, radius, step);
	}
	catch (const std::exception& e)
	{
		std::fprintf(stderr, "error: %s\n", e.what());
		return 1;
	}
}
