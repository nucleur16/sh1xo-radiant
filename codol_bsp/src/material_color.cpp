// The colour a material throws light back with: the mean of its colour map.
//
// The game's lit shaders square the colour map ( `texld colorMap; mul r, r, r` in lm_dfog_.. ): its texels are the
// square roots of what a surface reflects. So the reflectance of a material is the mean of the squares of its colour
// map's texels, per channel - read from the picture itself:
//   * a streamed image ( nearly every material of the game ): images/<name>.iwi out of the install's image archives,
//     any of its formats, the wavelet coded ones included ( codol_render );
//   * an image whose pixels are in a zone ( a project's material, compiled by codol_assets ): from there.
// The means of streamed images are kept in codol_bsp_matcolor.txt next to the catalog; the archives take a moment to
// open, and a map uses the same materials at every compile.

#include "bsp_impl.hpp"
#include "render.hpp"

#include "codol_render/iwd.hpp"
#include "codol_render/iwi.hpp"
#include "codol_render/material.hpp"
#include "codol_render/texture.hpp"
#include "codol_zone/assets.hpp"

#include <cstdlib>
#include <fstream>
#include <map>
#include <set>
#include <sstream>

namespace codol::bsp
{
	namespace
	{
		// the mean of the squared colour of one level of a picture: the smallest that still has 8 texels a side ( a mean
		// of means; DXT blocks are whole there ), else the largest there is. `by_alpha`: texels count by their alpha
		// ( a fence's holes are not its colour ).
		bool mean_of_upload(const render::texture_upload& upload, const bool by_alpha, float out[3])
		{
			const render::texture_surface* pick = nullptr;
			const auto enough = [](const render::texture_surface& s) { return s.width >= 8 && s.height >= 8; };
			for (const render::texture_surface& s : upload.surfaces)
			{
				if (s.face != 0 || s.width < 1 || s.height < 1)
				{
					continue;
				}
				// of the levels with 8 texels a side the smallest; without one the largest level
				if (!pick || (enough(s) && (!enough(*pick) || s.level > pick->level)) || (!enough(s) && !enough(*pick) && s.level < pick->level))
				{
					pick = &s;
				}
			}
			render::rgba_image image;
			if (!pick || !render::upload_surface_to_rgba(upload, *pick, image) || image.pixels.size() < static_cast<std::size_t>(image.width) * image.height * 4)
			{
				return false;
			}
			double sum[3] = { 0, 0, 0 }, weight = 0;
			for (std::size_t k = 0; k < static_cast<std::size_t>(image.width) * image.height; k++)
			{
				const double a = by_alpha ? image.pixels[k * 4 + 3] / 255.0 : 1.0;
				for (int c = 0; c < 3; c++)
				{
					const double v = image.pixels[k * 4 + static_cast<std::size_t>(c)] / 255.0;
					sum[c] += v * v * a;
				}
				weight += a;
			}
			if (weight < 1e-6)
			{
				return false;
			}
			for (int c = 0; c < 3; c++)
			{
				out[c] = static_cast<float>(sum[c] / weight);
			}
			return true;
		}
	}

	bool albedo_from_zone_image(const zone::Zone& z, const zone::zaddr image, const bool by_alpha, float out[3])
	{
		zone::ImageView view;
		if (!zone::read_image(z, image, view) || view.isReference || view.streamed || view.pixels.empty())
		{
			return false;
		}
		render::embedded_image e;
		e.d3d_format = view.loadDef.format;
		e.level_count = view.loadDef.levelCount;
		e.flags = view.loadDef.flags;
		e.width = view.raw.width;
		e.height = view.raw.height;
		e.depth = std::max<int>(view.raw.depth, 1);
		e.map_type = view.raw.mapType;
		e.data_size = static_cast<std::uint32_t>(view.pixels.size());
		render::texture_upload upload;
		return render::build_embedded_upload(e, render::byte_span(view.pixels.data(), view.pixels.size()), upload) && mean_of_upload(upload, by_alpha, out);
	}

	void read_material_colours(compile_context& ctx, render_world& rw)
	{
		struct colour
		{
			float c[3] = { -1, -1, -1 };		// below 0: the picture could not be read
		};
		// CODOL_BSP_GREY_BOUNCE=1: the grey world of the rounds before, to compare with
		if (std::getenv("CODOL_BSP_GREY_BOUNCE"))
		{
			ctx.rep.notes.push_back("light: bounced light without the colour of the materials ( CODOL_BSP_GREY_BOUNCE )");
			return;
		}
		std::set<std::string> wanted;
		for (render_material& m : rw.materials)
		{
			if (!m.desc || m.desc->sky)
			{
				continue;
			}
			if (m.desc->albedo_known)
			{
				std::copy(m.desc->albedo, m.desc->albedo + 3, m.albedo);
				m.albedo_known = true;
			}
			else if (!m.desc->color_map.empty())
			{
				wanted.insert((m.desc->see_through ? "a:" : "c:") + m.desc->color_map);
			}
		}
		std::filesystem::path folder = ctx.opts.cache;
		if (folder.empty())
		{
			folder = ctx.opts.out.empty() ? std::filesystem::current_path() : std::filesystem::absolute(ctx.opts.out).parent_path();
		}
		const std::filesystem::path cache = folder / "codol_bsp_matcolor.txt";
		const std::string tag = "material colours 1 " + ctx.opts.install.generic_string();
		std::map<std::string, colour> known;
		{
			std::ifstream in(cache);
			std::string line;
			if (in && std::getline(in, line) && line == tag)
			{
				while (std::getline(in, line))
				{
					std::istringstream text(line);
					std::string name;
					colour c;
					if (text >> name >> c.c[0] >> c.c[1] >> c.c[2])
					{
						known[name] = c;
					}
				}
			}
		}
		std::vector<std::string> missing;
		for (const std::string& name : wanted)
		{
			if (!known.count(name))
			{
				missing.push_back(name);
			}
		}
		if (!missing.empty())
		{
			render::iwd_key key{};
			render::image_archive_sets archives;
			std::string error;
			if (render::derive_iwd_key(ctx.opts.exe, key, &error) && render::open_image_archives(ctx.opts.install, key, archives, &error))
			{
				std::shared_ptr<const render::wavelet_tables> tables;
				for (const std::string& name : missing)
				{
					colour c;
					std::vector<std::uint8_t> file;
					render::iwi_file image;
					if (archives.read(name.substr(2), file, &error) && render::parse_iwi(file, image, &error))
					{
						if (image.header.is_wavelet() && !tables)
						{
							tables = render::wavelet_tables::load_from_executable(ctx.opts.exe, &error);
						}
						render::texture_upload upload;
						if (render::build_texture_upload(image, tables.get(), upload, &error))
						{
							float mean[3];
							if (mean_of_upload(upload, name[0] == 'a', mean))
							{
								std::copy(mean, mean + 3, c.c);
							}
						}
					}
					known[name] = c;
				}
				std::ofstream out(cache, std::ios::trunc);
				out << tag << "\n";
				for (const auto& [name, c] : known)
				{
					out << name << " " << c.c[0] << " " << c.c[1] << " " << c.c[2] << "\n";
				}
			}
			else
			{
				ctx.rep.warnings.push_back("light: the image archives of the install were not opened ( " + error + " ): bounced light does not take the colour of the materials");
			}
		}
		std::size_t read = 0, grey = 0;
		for (render_material& m : rw.materials)
		{
			if (!m.desc || m.desc->sky)
			{
				continue;
			}
			if (!m.albedo_known && !m.desc->color_map.empty())
			{
				if (const auto it = known.find((m.desc->see_through ? "a:" : "c:") + m.desc->color_map); it != known.end() && it->second.c[0] >= 0)
				{
					std::copy(it->second.c, it->second.c + 3, m.albedo);
					m.albedo_known = true;
				}
			}
			(m.albedo_known ? read : grey)++;
		}
		ctx.rep.notes.push_back(format("light: bounced light takes the colour of %zu of %zu materials ( the mean of their colour maps%s )%s", read, read + grey,
			missing.empty() ? ", codol_bsp_matcolor.txt" : "", grey ? "; the others have no colour map that could be read and count as grey" : ""));
	}
}
