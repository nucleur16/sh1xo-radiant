// The install catalog: which zone holds which material / xmodel / inline image, and the clip material table.

#include "bsp_impl.hpp"

#include <fstream>
#include <sstream>

namespace codol::bsp
{
	namespace
	{
		namespace fs = std::filesystem;

		std::vector<fs::path> zone_files(const fs::path& install)
		{
			std::vector<fs::path> files;
			for (const char* folder : { "client_shipRetail/chinese", "client_shipRetail/patch" })
			{
				std::error_code ec;
				for (fs::directory_iterator it(install / folder, ec), end; !ec && it != end; it.increment(ec))
				{
					if (it->is_regular_file(ec) && it->path().extension() == ".ff")
					{
						files.push_back(it->path());
					}
				}
			}
			std::sort(files.begin(), files.end());
			return files;
		}

		// changes when a fast file of the install is added, removed or replaced
		std::string signature(const fs::path& install)
		{
			std::uint64_t hash = 0xCBF29CE484222325ull;
			std::size_t count = 0;
			for (const fs::path& file : zone_files(install))
			{
				std::error_code ec;
				const std::string text = file.filename().string() + ":" + std::to_string(fs::file_size(file, ec)) + ":"
					+ std::to_string(fs::last_write_time(file, ec).time_since_epoch().count());
				for (const char c : text)
				{
					hash = (hash ^ static_cast<std::uint8_t>(c)) * 0x100000001B3ull;
				}
				count++;
			}
			return format("codol_bsp catalog 4 %zu %016llx", count, static_cast<unsigned long long>(hash));
		}
	}

	bool is_tool_material(const std::string_view name)
	{
		static constexpr std::string_view exact[] = { "caulk", "caulk_shadow", "hint", "skip", "portal", "portal_nodraw", "nodraw", "trigger",
			"lightgrid_volume", "lightgrid_sky", "ladder", "mantle_on", "mantle_over", "aitrig", "common/caulk", "sky_caulk", "lightdef_caulk",
			"nodraw_decal", "nodraw_notsolid", "portal_blocker", "noimpact", "shadowcaster", "occluder" };
		std::string_view bare = name;
		if (bare.rfind("wc/", 0) == 0)
		{
			bare.remove_prefix(3);
		}
		for (const std::string_view e : exact)
		{
			if (bare == e)
			{
				return true;
			}
		}
		return bare.rfind("clip", 0) == 0 || bare.rfind("trigger", 0) == 0 || bare.rfind("nodraw", 0) == 0 || bare.rfind("portal", 0) == 0
			|| bare.rfind("hint", 0) == 0 || bare.rfind("tools/", 0) == 0;
	}

	bool catalog::load(const std::filesystem::path& file, const std::filesystem::path& install)
	{
		std::ifstream in(file);
		std::string line;
		if (!in || !std::getline(in, line) || line != signature(install))
		{
			return false;
		}
		while (std::getline(in, line))
		{
			std::vector<std::string> f;
			std::size_t at = 0;
			while (true)
			{
				const std::size_t tab = line.find('\t', at);
				f.push_back(line.substr(at, tab == std::string::npos ? std::string::npos : tab - at));
				if (tab == std::string::npos)
				{
					break;
				}
				at = tab + 1;
			}
			if (f.size() >= 3 && f[0] == "m")
			{
				material_zone[f[1]] = f[2];
			}
			else if (f.size() >= 3 && f[0] == "x")
			{
				model_zone[f[1]] = f[2];
			}
			else if (f.size() >= 3 && f[0] == "i")
			{
				image_zone[f[1]] = f[2];
			}
			else if (f.size() >= 4 && f[0] == "c")
			{
				clip_materials[f[1]] = { static_cast<std::int32_t>(std::stoul(f[2], nullptr, 16)), static_cast<std::int32_t>(std::stoul(f[3], nullptr, 16)) };
			}
		}
		return true;
	}

	bool catalog::save(const std::filesystem::path& file, const std::filesystem::path& install) const
	{
		std::error_code ec;
		fs::create_directories(file.parent_path(), ec);
		std::ofstream out(file, std::ios::trunc);
		if (!out)
		{
			return false;
		}
		out << signature(install) << "\n";
		for (const auto& [name, z] : material_zone)
		{
			out << "m\t" << name << "\t" << z << "\n";
		}
		for (const auto& [name, z] : model_zone)
		{
			out << "x\t" << name << "\t" << z << "\n";
		}
		for (const auto& [name, z] : image_zone)
		{
			out << "i\t" << name << "\t" << z << "\n";
		}
		for (const auto& [name, info] : clip_materials)
		{
			out << "c\t" << name << "\t" << format("%08X\t%08X", static_cast<unsigned>(info.surface_flags), static_cast<unsigned>(info.contents)) << "\n";
		}
		return static_cast<bool>(out);
	}

	void catalog::scan(const std::filesystem::path& install, const zone::ClientKey& key, std::vector<std::string>& warnings)
	{
		const std::vector<fs::path> files = zone_files(install);
		// map zones last: a material that a common zone has is taken from there
		std::vector<fs::path> order;
		for (int pass = 0; pass < 2; pass++)
		{
			for (const fs::path& file : files)
			{
				const std::string stem = file.stem().string();
				const bool map = stem.rfind("mp_", 0) == 0 || stem.rfind("zm_", 0) == 0 || stem.rfind("so_", 0) == 0;
				if (map == (pass == 1))
				{
					order.push_back(file);
				}
			}
		}
		for (const fs::path& file : order)
		{
			Zone z;
			if (const zone::Status s = z.open(file, key); !s)
			{
				warnings.push_back("catalog: " + file.filename().string() + ": " + s.message);
				continue;
			}
			const std::string relative = fs::relative(file, install).generic_string();
			std::set<zaddr> top;
			for (const zone::XAssetEntry& e : z.assets())
			{
				top.insert(e.header);
			}
			for (const zone::AssetRecord& r : z.all_assets())
			{
				if (z.reference(r.header))
				{
					continue;
				}
				const std::string name(z.asset_name(r.type, r.header));
				if (name.empty() || name[0] == ',')
				{
					continue;
				}
				if (r.type == c1::ASSET_TYPE_MATERIAL)
				{
					material_zone.emplace(name, relative);
				}
				else if (r.type == c1::ASSET_TYPE_XMODEL && top.count(r.header))
				{
					model_zone.emplace(name, relative);
				}
				else if (r.type == c1::ASSET_TYPE_IMAGE)
				{
					zone::ImageView image;
					if (zone::read_image(z, r.header, image) && !image.streamed)
					{
						image_zone.emplace(name, relative);
					}
				}
				else if (r.type == c1::ASSET_TYPE_CLIPMAP_MP || r.type == c1::ASSET_TYPE_CLIPMAP_SP)
				{
					zone::ClipMapView clip;
					if (zone::read_clip_map(z, r.header, clip))
					{
						for (const zone::ClipMaterialView& m : clip.materials)
						{
							// a name can occur with several contents ( the same material on a non colliding brush ):
							// the entry with contents wins
							const auto [it, added] = clip_materials.emplace(std::string(m.name), clip_material_info{ m.surfaceFlags, m.contents });
							if (!added && it->second.contents == 0 && m.contents != 0)
							{
								it->second = clip_material_info{ m.surfaceFlags, m.contents };
							}
						}
					}
				}
			}
		}
	}
}
