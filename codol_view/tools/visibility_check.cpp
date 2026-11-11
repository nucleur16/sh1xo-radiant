// visibility_check <game root> <map name> <x> <y> <z> <pitch> <yaw> [fov=80] [aspect=1.7777778]
//
// Runs the cell / portal visibility ( world_cells + world_visibility ) for one camera on a real map and prints
// what it found, for comparing with the C# renderer ( CodolSoundDumper render-map prints
// "cells: camera in cell C of N, R reached through portals (V cell visits, cell culling on)" ).

#include "world_cells.hpp"
#include "world_visibility.hpp"

#include "codol_render/code_constants.hpp"
#include "codol_zone/zone.hpp"

#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

namespace
{
	using namespace codol;
	using view::float3;
	using view::float4;

	// GameMapRenderer.cs:1363-1377: left, right, bottom, top, near of the eye relative view * projection, normalised
	std::array<float4, 5> build_frustum(const render::camera& cam, const float aspect)
	{
		const render::mat4 m = render::mul(cam.view_rotation(), cam.projection(aspect));
		const float4 c1{ m.m[0][0], m.m[1][0], m.m[2][0], m.m[3][0] };
		const float4 c2{ m.m[0][1], m.m[1][1], m.m[2][1], m.m[3][1] };
		const float4 c3{ m.m[0][2], m.m[1][2], m.m[2][2], m.m[3][2] };
		const float4 c4{ m.m[0][3], m.m[1][3], m.m[2][3], m.m[3][3] };
		const auto add = [](const float4& a, const float4& b) { return float4{ a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w }; };
		const auto sub = [](const float4& a, const float4& b) { return float4{ a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w }; };
		std::array<float4, 5> planes{ add(c4, c1), sub(c4, c1), add(c4, c2), sub(c4, c2), c3 };
		for (float4& p : planes)
		{
			const float length = std::sqrt(p.x * p.x + p.y * p.y + p.z * p.z);
			const float d = length > 0 ? length : 1.0f;
			p = float4{ p.x / d, p.y / d, p.z / d, p.w / d };
		}
		return planes;
	}

	std::uint64_t fnv1a(std::uint64_t hash, const std::vector<std::uint8_t>& bytes)
	{
		for (const std::uint8_t b : bytes)
		{
			hash = (hash ^ b) * 0x100000001B3ull;
		}
		return hash;
	}

	bool parse_float(const char* text, float& value)
	{
		char* end = nullptr;
		value = std::strtof(text, &end);
		return end != text && *end == 0;
	}

	int run(const int argc, char** argv)
	{
		if (argc < 8)
		{
			std::fprintf(stderr, "usage: visibility_check <game root> <map name> <x> <y> <z> <pitch> <yaw> [fov=80] [aspect=1.7777778]\n");
			return 2;
		}
		float v[7] = { 0, 0, 0, 0, 0, 80.0f, 1.7777778f };
		for (int i = 0; i < 7 && 3 + i < argc; i++)
		{
			if (!parse_float(argv[3 + i], v[i]))
			{
				std::fprintf(stderr, "error: '%s' is not a number\n", argv[3 + i]);
				return 2;
			}
		}
		const std::filesystem::path root = argv[1];
		const std::filesystem::path fastfile = root / "client_shipRetail" / "chinese" / (std::string(argv[2]) + ".ff");

		zone::ClientKey key;
		if (const zone::Status s = key.load_from_exe(root / "C1.exe"); !s)
		{
			std::fprintf(stderr, "error: %s\n", s.message.c_str());
			return 1;
		}
		zone::Zone z;
		if (const zone::Status s = z.open(fastfile, key); !s)
		{
			std::fprintf(stderr, "error: %s\n", s.message.c_str());
			return 1;
		}
		zone::zaddr world = 0;
		for (const zone::AssetRecord& asset : z.all_assets())
		{
			if (asset.type == c1::ASSET_TYPE_GFXWORLD && !z.reference(asset.header))
			{
				world = asset.header;
				break;
			}
		}
		c1::GfxWorld raw{};
		if (!world || !z.get(world, raw))
		{
			std::fprintf(stderr, "error: the zone has no GfxWorld\n");
			return 1;
		}

		// GameMapRenderer.cs:266 ( every surface ) and :325-336, 1024 ( the static models, when both arrays are there )
		const int surface_count = static_cast<int>(raw.surfaceCount);
		int static_count = static_cast<int>(raw.dpvs.smodelCount);
		if (static_count <= 0
			|| z.bytes(raw.dpvs.smodelDrawInsts, static_cast<std::size_t>(static_count) * sizeof(c1::GfxStaticModelDrawInst)).empty()
			|| z.bytes(raw.dpvs.smodelInsts, static_cast<std::size_t>(static_count) * sizeof(c1::GfxStaticModelInst)).empty())
		{
			static_count = 0;
		}

		std::vector<std::string> warnings;
		auto cells = view::read_world_cells(z, world, surface_count, static_count, warnings);
		for (const std::string& w : warnings)
		{
			std::printf("warning: %s\n", w.c_str());
		}
		if (!cells || cells->cells.empty())
		{
			std::printf("no visibility cells\n");
			return 1;
		}
		std::size_t portals = 0, nodes = 0;
		for (const view::world_cell& cell : cells->cells)
		{
			portals += cell.portals.size();
			nodes += cell.nodes.size();
		}
		const std::size_t sorted = cells->sorted_surfaces.size(), bsp_planes = cells->planes.size(), bsp_nodes = cells->nodes.size();

		render::camera cam;
		cam.origin = { v[0], v[1], v[2] };
		cam.pitch = v[3];
		cam.yaw = v[4];
		cam.fov_x = v[5];
		const auto frustum = build_frustum(cam, v[6]);

		view::world_visibility visibility(std::move(*cells), surface_count, static_count);
		visibility.update(cam.origin, frustum);

		std::size_t visible_surfaces = 0, visible_models = 0;
		for (const std::uint8_t b : visibility.surface_visible())
		{
			visible_surfaces += b;
		}
		for (const std::uint8_t b : visibility.model_visible())
		{
			visible_models += b;
		}
		const std::uint64_t hash = fnv1a(fnv1a(0xCBF29CE484222325ull, visibility.surface_visible()), visibility.model_visible());

		std::printf("map: %s  surfaces %d, static models %d, sorted surfaces %zu, portals %zu, aabb nodes %zu, bsp planes %zu, bsp nodes %zu\n",
			argv[2], surface_count, static_count, sorted, portals, nodes, bsp_planes, bsp_nodes);
		std::printf("camera: origin %g %g %g  pitch %g yaw %g  fov %g aspect %g\n", v[0], v[1], v[2], v[3], v[4], v[5], v[6]);
		std::printf("cell count: %d\n", visibility.cell_count());
		std::printf("camera cell: %d\n", visibility.camera_cell());
		std::printf("reached cells: %d\n", visibility.reached_cells());
		std::printf("visible cell visits: %d\n", visibility.visible_cells());
		std::printf("visible surfaces: %zu of %d\n", visible_surfaces, surface_count);
		std::printf("visible static models: %zu of %d\n", visible_models, static_count);
		std::printf("visibility hash: %016llx\n", static_cast<unsigned long long>(hash));
		std::printf("cells: camera in cell %d of %d, %d reached through portals (%d cell visits)\n",
			visibility.camera_cell(), visibility.cell_count(), visibility.reached_cells(), visibility.visible_cells());
		return 0;
	}
}

int main(const int argc, char** argv)
{
	try
	{
		return run(argc, argv);
	}
	catch (const std::exception& e)
	{
		std::fprintf(stderr, "error: %s\n", e.what());
		return 1;
	}
}
