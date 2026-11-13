#pragma once

// The render world between the stages: surfaces ( render_build ), visibility ( vis ), lighting ( light ), and the
// writer ( gfx_build ).

#include "world.hpp"
#include "gfx_layout.hpp"

namespace codol::bsp
{
	// one brush face ( or patch quad ) inside a surface: what the light bake works on
	struct render_face
	{
		std::uint32_t first_vertex = 0, vertex_count = 0;	// inside its surface; a convex polygon, in winding order
		float normal[3] = {};
		int brush = -1;					// world::brushes index, -1: not from a brush
		int face = -1;					// face of that brush
	};

	struct render_surface
	{
		int material = 0;				// render_world::materials
		int cell = 0;
		int model = 0;
		int lightmap = 0;				// 31: none
		int reflection_probe = 1;
		int primary_light = 1;
		int light_hint = -1;			// the light stage's cut: the primary light entity the surface's faces were cut out for, 0: none of them, -1: not decided
		bool casts_sun_shadow = true;
		bool sky = false;
		std::vector<c1::GfxWorldVertex> vertices;
		std::vector<std::uint16_t> indices;		// relative to the surface's first vertex; clockwise seen from the front
		std::vector<render_face> faces;
		box3 box;
	};

	struct render_material
	{
		std::string name;				// "wc/<map material>"
		zone_ref ref;
		const material_desc* desc = nullptr;
		int draw_class = 0;				// 0 lit opaque, 1 lit transparent / decal
		// the mean of its colour map's squared texels: what it reflects ( read_material_colours ). Not known: a mid grey.
		float albedo[3] = { 0.25f, 0.25f, 0.25f };
		bool albedo_known = false;
	};

	struct render_portal
	{
		float plane[4] = {};			// normal + dist, facing into the cell that owns the portal
		int cell = 0;					// the cell it leads to
		std::vector<float> vertices;	// xyz per corner
		float hull_axis[2][3] = {};
	};

	struct render_cell
	{
		box3 box;
		std::vector<render_portal> portals;
		std::vector<std::uint8_t> probes;	// reflection probe indices
	};

	struct render_probe
	{
		float origin[3] = {};
		std::vector<std::uint8_t> pixels;	// 64 x 64 cube, A8R8G8B8, all mip levels: 131064 bytes
	};

	struct render_lightmap
	{
		std::vector<std::uint8_t> primary;		// L8, ( 2 LM_W ) x ( 2 LM_H ): baked sun visibility
		std::vector<std::uint8_t> secondary;	// A8R8G8B8, LM_W x ( 2 LM_H ): two layers of baked light
	};

	struct render_grid
	{
		gfx::GfxLightGrid header{};
		std::vector<std::uint16_t> row_starts;
		std::vector<std::uint8_t> raw_rows;
		std::vector<gfx::GfxLightGridEntry> entries;
		std::vector<gfx::GfxLightGridColors> colors;
	};

	// a static model of the world ( world::models[ model ] )
	struct render_smodel
	{
		int model = 0;
		int cell = 0;
		int primary_light = 1;
		int reflection_probe = 1;
	};

	// a convex piece of a primary light's region: the ranges of what it lights along nine axes ( x, y, z, x + y, x - y,
	// x + z, x - z, y + z, y - z ), relative to the light's origin
	struct render_light_hull
	{
		struct axis
		{
			float dir[3] = {}, mid = 0, half = 0;
		};
		float mid[9] = {}, half[9] = {};
		std::vector<axis> axes;			// further planes: the ranges along other directions ( at most 8 )
	};

	struct render_model
	{
		box3 box;
		std::uint32_t first_surface = 0, surface_count = 0;
	};

	struct render_world
	{
		std::vector<render_surface> surfaces;		// static surfaces sorted by draw class and material, then the brush models'
		std::vector<render_material> materials;
		std::vector<render_cell> cells;
		std::vector<std::uint16_t> cell_nodes;		// the BSP that puts a point into a cell
		std::vector<render_probe> probes;			// [0]: the default probe
		std::vector<render_lightmap> lightmaps;
		render_grid grid;
		std::vector<render_model> models;
		box3 bounds;
		std::vector<std::int32_t> sky_surfaces;
		zone_ref sky_image;
		std::vector<render_smodel> smodels;
		std::vector<std::vector<std::uint16_t>> light_surfaces;	// per primary light: the surfaces in its shadow geometry
		std::vector<std::vector<render_light_hull>> light_regions;	// per primary light: the hulls of its region
		std::uint32_t lit_opaque_end = 0, lit_trans_end = 0;
	};

	// brush faces -> surfaces; one cell around everything
	void build_render_surfaces(compile_context& ctx, render_world& rw);
	// lightmaps, lightmap coordinates, light grid, reflection probes
	void bake_lighting(compile_context& ctx, render_world& rw);
	// the colour every material throws light back with: the mean of its colour map ( material_color.cpp )
	void read_material_colours(compile_context& ctx, render_world& rw);
	// vertices of the world that lie on an edge of a face become vertices of that face ( tjunction.cpp )
	void repair_t_junctions(compile_context& ctx, render_world& rw);
}
