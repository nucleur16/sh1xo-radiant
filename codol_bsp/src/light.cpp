// Lighting data of the GfxWorld: lightmaps and the surfaces' lightmap coordinates, the light grid, reflection probes.

#include "render.hpp"

namespace codol::bsp
{
	using namespace gfx;

	namespace
	{
		std::uint8_t to_byte(const float v)
		{
			return static_cast<std::uint8_t>(std::clamp(std::lround(v * 255.0f), 0l, 255l));
		}

		// 64 x 64 cube map, 6 faces, 7 mip levels, one colour
		std::vector<std::uint8_t> flat_probe(const float color[3])
		{
			std::vector<std::uint8_t> pixels(131064);
			for (std::size_t k = 0; k < pixels.size(); k += 4)
			{
				pixels[k] = to_byte(color[2]);
				pixels[k + 1] = to_byte(color[1]);
				pixels[k + 2] = to_byte(color[0]);
				pixels[k + 3] = 255;
			}
			return pixels;
		}

		// the grid cell of a world position: 32 units in x and y, 64 in z
		std::uint16_t grid_coord(const float v, const int axis)
		{
			const int i = static_cast<int>(std::floor(v)) + 0x20000;
			return static_cast<std::uint16_t>(axis == 2 ? i >> 6 : i >> 5);
		}

		// a light grid that holds one sample for every point of the box
		void uniform_grid(render_grid& grid, const box3& box, const float color[3], const std::uint8_t primary_light)
		{
			GfxLightGrid& h = grid.header;
			h.hasLightRegions = 0;
			h.lastSunPrimaryLightIndex = 1;
			for (int k = 0; k < 3; k++)
			{
				h.mins[k] = grid_coord(box.mins[k], k);
				h.maxs[k] = grid_coord(box.maxs[k], k);
			}
			h.maxs[2] = static_cast<std::uint16_t>(std::min<int>(h.maxs[2], h.mins[2] + 254));
			h.rowAxis = 0;
			h.colAxis = 1;
			const std::uint32_t rows = h.maxs[0] - h.mins[0] + 1u, cols = h.maxs[1] - h.mins[1] + 1u, layers = h.maxs[2] - h.mins[2] + 1u;
			std::uint32_t first = 0;
			for (std::uint32_t r = 0; r < rows; r++)
			{
				grid.row_starts.push_back(static_cast<std::uint16_t>(grid.raw_rows.size() / 4));
				GfxLightGridRow row{};
				row.colStart = h.mins[1];
				row.colCount = static_cast<std::uint16_t>(cols);
				row.zStart = h.mins[2];
				row.zCount = static_cast<std::uint16_t>(layers);
				row.firstEntry = first;
				const auto* p = reinterpret_cast<const std::uint8_t*>(&row);
				grid.raw_rows.insert(grid.raw_rows.end(), p, p + sizeof(row));
				for (std::uint32_t left = cols; left > 0;)
				{
					const std::uint32_t run = std::min<std::uint32_t>(left, 255);
					grid.raw_rows.push_back(static_cast<std::uint8_t>(run));
					grid.raw_rows.push_back(static_cast<std::uint8_t>(layers));
					grid.raw_rows.push_back(0);		// z start of the run, relative to the row's
					left -= run;
				}
				while (grid.raw_rows.size() % 4)
				{
					grid.raw_rows.push_back(0);
				}
				first += cols * layers;
			}
			GfxLightGridEntry entry{};
			entry.colorsIndex = 0;
			entry.primaryLightIndex = primary_light;
			grid.entries.assign(first, entry);
			GfxLightGridColors colors{};
			for (auto& rgb : colors.rgb)
			{
				for (int k = 0; k < 3; k++)
				{
					rgb[k] = to_byte(color[k]);
				}
			}
			grid.colors.push_back(colors);
		}
	}

	void bake_lighting(compile_context& ctx, render_world& rw)
	{
		const world& w = ctx.w;

		// flat lighting: every surface reads one texel that holds the ambient light, in full sun
		float ambient[3];
		for (int k = 0; k < 3; k++)
		{
			ambient[k] = std::clamp(w.ambient[k], 0.0f, 1.0f);
		}
		render_lightmap lightmap;
		lightmap.primary.assign(static_cast<std::size_t>(LM_W) * 2 * LM_H * 2, 255);
		lightmap.secondary.resize(static_cast<std::size_t>(LM_W) * LM_H * 2 * 4);
		for (std::size_t k = 0; k < lightmap.secondary.size(); k += 4)
		{
			lightmap.secondary[k] = to_byte(ambient[2]);
			lightmap.secondary[k + 1] = to_byte(ambient[1]);
			lightmap.secondary[k + 2] = to_byte(ambient[0]);
			lightmap.secondary[k + 3] = 255;
		}
		rw.lightmaps.push_back(std::move(lightmap));

		// reflection probes: [0] is the default every map has, [1] stands in the middle of the map
		const float sky[3] = { 0.35f, 0.4f, 0.45f };
		render_probe none, centre;
		none.pixels = flat_probe(sky);
		centre.pixels = flat_probe(sky);
		for (int k = 0; k < 3; k++)
		{
			centre.origin[k] = (rw.bounds.mins[k] + rw.bounds.maxs[k]) * 0.5f;
		}
		rw.probes.push_back(std::move(none));
		rw.probes.push_back(std::move(centre));
		for (render_cell& cell : rw.cells)
		{
			cell.probes = { 1 };
		}

		uniform_grid(rw.grid, rw.bounds, ambient, 1);
		rw.light_surfaces.assign(w.lights.size(), {});
	}
}
