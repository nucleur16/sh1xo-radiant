#include "zone_library.hpp"

#include "entities.hpp"

namespace codol::view
{
	namespace
	{
		char lower(const char c) { return c >= 'A' && c <= 'Z' ? static_cast<char>(c + 32) : c; }

		template <typename Map>
		auto find_in(const Map& map, const std::string& name) -> const typename Map::mapped_type*
		{
			const auto it = map.find(name);
			return it == map.end() ? nullptr : &it->second;
		}
	}

	std::size_t name_hash::operator()(const std::string& s) const noexcept
	{
		std::size_t h = 1469598103934665603ull & static_cast<std::size_t>(-1);
		for (const char c : s)
		{
			h ^= static_cast<unsigned char>(lower(c));
			h *= static_cast<std::size_t>(1099511628211ull);
		}
		return h;
	}

	bool name_equal::operator()(const std::string& a, const std::string& b) const noexcept
	{
		return equals_ignore_case(a, b);
	}

	render::zone_view make_zone_view(const zone::Zone& zone)
	{
		render::zone_view view;
		const zone::Zone* z = &zone;
		view.read = [z](const std::uint32_t ptr, const std::uint32_t size) -> render::byte_span
		{
			return ptr && size ? z->bytes(ptr, size) : render::byte_span{};
		};
		view.string = [z](const std::uint32_t ptr, std::string& out) -> bool
		{
			if (!ptr || z->bytes(ptr, 1).empty())
			{
				return false;
			}
			out = std::string(z->c_string(ptr));
			return true;
		};
		return view;
	}

	const std::vector<std::string>& shared_zone_names()
	{
		// RenderLayouts.cs:85-87
		static const std::vector<std::string> names = { "code_pre_gfx_mp", "patch_code_post_gfx_mp", "code_post_gfx_mp", "code_post_gfx_default_mp",
			"patch_ui_mp", "ui_mp", "common_marketplace_lite", "patch_common_mp", "common_mp" };
		return names;
	}

	std::vector<std::string> level_zone_names(const std::string& map_name)
	{
		// RenderLayouts.cs:89-96
		const std::string lower_name = to_lower(map_name);
		const auto starts = [&](const char* prefix) { return lower_name.rfind(prefix, 0) == 0; };
		const std::string mode = starts("zm_") ? "common_gametype_zm" : starts("za_") ? "common_gametype_zombie_arena"
			: starts("coop_") ? "common_gametype_coop" : "common_gametype_mp";
		return { "common_mp_reload", "reloadable_common_mp", "patch_" + mode, mode, "patch_" + map_name, map_name + "_const" };
	}

	// ---------------------------------------------------------------------------------------------------

	std::shared_ptr<const library_material> library_scope::find_material(const std::string& name) const
	{
		for (const auto& zone : zones)
		{
			if (const auto* hit = find_in(zone->materials, name))
			{
				return *hit;
			}
		}
		return nullptr;
	}

	std::shared_ptr<const library_techset> library_scope::find_techset(const std::string& name) const
	{
		for (const auto& zone : zones)
		{
			if (const auto* hit = find_in(zone->techsets, name))
			{
				return *hit;
			}
		}
		return nullptr;
	}

	std::shared_ptr<const shader_blob> library_scope::find_shader(const std::string& name, const bool pixel) const
	{
		for (const auto& zone : zones)
		{
			if (const auto* hit = find_in(pixel ? zone->pixel_shaders : zone->vertex_shaders, name))
			{
				return *hit;
			}
		}
		return nullptr;
	}

	const render::vertex_declaration* library_scope::find_declaration(const std::string& name) const
	{
		for (const auto& zone : zones)
		{
			if (const auto* hit = find_in(zone->declarations, name))
			{
				return hit;
			}
		}
		return nullptr;
	}

	std::string library_scope::find_model_zone(const std::string& name, std::string* stored_name) const
	{
		for (const auto& zone : zones)
		{
			if (const auto it = zone->models.find(name); it != zone->models.end())
			{
				if (stored_name)
				{
					*stored_name = it->second;
				}
				return zone->name;
			}
		}
		return {};
	}

	// ---------------------------------------------------------------------------------------------------

	bool zone_library::init(const std::filesystem::path& client_exe, std::vector<std::filesystem::path> zone_directories, std::string* error)
	{
		directories_ = std::move(zone_directories);
		const zone::Status status = key_.load_from_exe(client_exe);
		if (!status)
		{
			render::set_error(error, "fast file key: " + status.message);
			return false;
		}
		return true;
	}

	std::filesystem::path zone_library::find_zone_file(const std::string& name) const
	{
		std::error_code ec;
		if (const auto it = files_.find(name); it != files_.end())
		{
			return std::filesystem::is_regular_file(it->second, ec) ? it->second : std::filesystem::path();
		}
		for (const auto& directory : directories_)
		{
			std::filesystem::path path = directory / (name + ".ff");
			if (std::filesystem::is_regular_file(path, ec))
			{
				return path;
			}
		}
		return {};
	}

	std::shared_ptr<const zone::Zone> zone_library::load_zone(const std::string& name, std::string* error) const
	{
		const std::filesystem::path path = find_zone_file(name);
		if (path.empty())
		{
			render::set_error(error, "no zone named " + name);
			return nullptr;
		}
		auto loaded = std::make_shared<zone::Zone>();
		const zone::Status status = loaded->open(path, key_);
		if (!status)
		{
			render::set_error(error, status.message);
			return nullptr;
		}
		return loaded;
	}

	std::shared_ptr<library_zone> zone_library::freeze(const zone::Zone& zone, std::vector<std::string>* warnings)
	{
		// RenderAssetLibrary.cs:74-112 ( AddLoadedZone ): every registered asset, nested ones included; a name is
		// taken by the first asset that has it ( TryAdd ); ",name" references are skipped. :114-154 ( Freeze ).
		auto result = std::make_shared<library_zone>();
		result->name = zone.name();
		const render::zone_view view = make_zone_view(zone);

		const auto add_shader = [&](const std::optional<render::shader_ref>& shader) -> std::shared_ptr<const shader_blob>
		{
			// RenderAssetLibrary.cs:371-398 ( AddShader )
			if (!shader || !shader->has_name || !shader->has_program())
			{
				return nullptr;
			}
			auto& table = shader->is_pixel_shader ? result->pixel_shaders : result->vertex_shaders;
			if (const auto it = table.find(shader->name); it != table.end())
			{
				return it->second;
			}
			const render::byte_span code = render::shader_code(view, *shader);
			if (code.empty())
			{
				return nullptr;
			}
			auto blob = std::make_shared<shader_blob>();
			blob->name = shader->name;
			blob->pixel = shader->is_pixel_shader;
			blob->code.assign(code.begin(), code.end());
			result->shader_bytes += blob->code.size();
			table.emplace(shader->name, blob);
			return blob;
		};

		for (const zone::AssetRecord& record : zone.all_assets())
		{
			switch (record.type)
			{
			case c1::ASSET_TYPE_PIXELSHADER:
				add_shader(render::read_shader(view, record.header, true));
				break;
			case c1::ASSET_TYPE_VERTEXSHADER:
				add_shader(render::read_shader(view, record.header, false));
				break;
			case c1::ASSET_TYPE_VERTEXDECL:
				// RenderAssetLibrary.cs:400-416 ( AddDeclaration )
				if (auto declaration = render::read_vertex_declaration(view, record.header); declaration && !declaration->is_reference && !declaration->name.empty())
				{
					result->declarations.emplace(declaration->name, std::move(*declaration));
				}
				break;
			case c1::ASSET_TYPE_XMODEL:
			{
				const std::string_view raw = zone.c_string(zone.get<zaddr>(record.header));
				if (!raw.empty() && raw[0] != ',')
				{
					result->models.emplace(std::string(raw), std::string(raw));
				}
				break;
			}
			default:
				break;
			}
		}

		// technique sets ( RenderAssetLibrary.cs:338-369 AddTechniqueSet, :437-465 ReadSet )
		std::unordered_map<zaddr, std::shared_ptr<const library_techset>> by_header;
		for (const zone::AssetRecord& record : zone.all_assets())
		{
			if (record.type != c1::ASSET_TYPE_TECHNIQUE_SET)
			{
				continue;
			}
			auto techset = std::make_shared<library_techset>();
			std::string error;
			if (!render::read_technique_set(view, record.header, techset->set, &error))
			{
				if (warnings)
				{
					warnings->push_back(result->name + ": " + error);
				}
				continue;
			}
			if (render::is_reference_name(techset->set.name) || techset->set.name.empty())
			{
				continue;
			}
			techset->passes.resize(techset->set.techniques.size());
			for (std::size_t t = 0; t < techset->set.techniques.size(); t++)
			{
				const auto& technique = techset->set.techniques[t];
				techset->passes[t].resize(technique.passes.size());
				for (std::size_t p = 0; p < technique.passes.size(); p++)
				{
					const auto& pass = technique.passes[p];
					techset->passes[t][p].vertex_shader = add_shader(pass.vertex_shader);
					techset->passes[t][p].pixel_shader = add_shader(pass.pixel_shader);
					if (pass.declaration && !pass.declaration->is_reference && !pass.declaration->name.empty())
					{
						result->declarations.emplace(pass.declaration->name, *pass.declaration);
					}
				}
			}
			by_header.emplace(record.header, techset);
			result->techsets.emplace(techset->set.name, techset);
		}

		// materials ( RenderAssetLibrary.cs:205-249 ReadMaterial )
		for (const zone::AssetRecord& record : zone.all_assets())
		{
			if (record.type != c1::ASSET_TYPE_MATERIAL)
			{
				continue;
			}
			auto material = std::make_shared<library_material>();
			if (!render::read_material(view, record.header, material->asset) || material->asset.is_reference || material->asset.name.empty())
			{
				continue;
			}
			if (result->materials.count(material->asset.name))
			{
				continue;
			}
			material->zone_name = result->name;
			if (material->asset.technique_set)
			{
				// RenderAssetLibrary.cs:251-255, 257-293: the set at this pointer, unless it is a ",name" reference
				if (const auto it = by_header.find(material->asset.technique_set); it != by_header.end())
				{
					material->techset = it->second;
				}
			}
			result->materials.emplace(material->asset.name, std::move(material));
		}
		return result;
	}

	std::shared_ptr<const library_zone> zone_library::frozen(const std::string& name, std::string* error)
	{
		if (const auto it = frozen_.find(name); it != frozen_.end())
		{
			return it->second;
		}
		if (find_zone_file(name).empty())
		{
			return nullptr;		// RenderLayouts.cs:104-108: zones an install does not have are skipped
		}
		const auto loaded = load_zone(name, error);
		if (!loaded)
		{
			return nullptr;
		}
		std::shared_ptr<const library_zone> result = freeze(*loaded);
		frozen_.emplace(name, result);
		return result;
	}

	void zone_library::set_zone_file(const std::string& name, const std::filesystem::path& file)
	{
		frozen_.erase(name);
		if (file.empty())
		{
			files_.erase(name);
		}
		else
		{
			files_[name] = file;
		}
	}

	void zone_library::clear()
	{
		frozen_.clear();
	}
}
