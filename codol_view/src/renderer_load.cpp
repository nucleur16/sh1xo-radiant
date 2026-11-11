// load_map: zones -> GPU resources and the CPU side scene. Port of the constructor of GameMapRenderer
// ( BuildWorld, ReadStaticModels, ReadEntityModels ), GetMaterial / GetModel, Prepare, BuildModelLighting,
// BuildVisibility, GameTextureCache and the parts of MapExtractor the renderer reads.

#include "renderer_impl.hpp"

#include <algorithm>
#include <cfloat>
#include <chrono>
#include <cstdlib>

namespace codol::view
{
	namespace
	{
		using clock = std::chrono::steady_clock;

		double ms_since(const clock::time_point from)
		{
			return std::chrono::duration<double, std::milli>(clock::now() - from).count();
		}

		float3 vec3(const float* f) { return { f[0], f[1], f[2] }; }

		// what load_map needs only while it runs
		struct load_context
		{
			std::shared_ptr<const zone::Zone> map_zone;
			std::shared_ptr<const zone::Zone> foreign_zone;		// at most one other zone at a time
			std::uint64_t peak_bytes = 0;
		};

		std::uint64_t upload_bytes(const render::texture_upload& upload)
		{
			std::uint64_t bytes = 0;
			for (const auto& surface : upload.surfaces)
			{
				bytes += surface.data.size();
			}
			return bytes;
		}

		void drop_top_mips(render::texture_upload& upload, const int skip)
		{
			// r_picmip: leave out the largest levels of a texture that has more of them
			const int drop = std::min(skip, upload.levels - 1);
			if (drop <= 0)
			{
				return;
			}
			std::vector<render::texture_surface> kept;
			for (auto& surface : upload.surfaces)
			{
				if (surface.level >= drop)
				{
					surface.level -= drop;
					kept.push_back(std::move(surface));
				}
			}
			upload.surfaces = std::move(kept);
			upload.levels -= drop;
			if (const auto* top = upload.surface(0, 0))
			{
				upload.width = top->width;
				upload.height = top->height;
			}
		}

		template <typename Lock, typename Unlock>
		bool copy_surface(const render::texture_surface& surface, Lock&& lock, Unlock&& unlock)
		{
			D3DLOCKED_RECT rect{};
			if (FAILED(lock(rect)))
			{
				return false;
			}
			for (int row = 0; row < surface.rows; row++)
			{
				std::memcpy(static_cast<std::uint8_t*>(rect.pBits) + static_cast<std::size_t>(row) * static_cast<std::size_t>(rect.Pitch),
					surface.data.data() + static_cast<std::size_t>(row) * static_cast<std::size_t>(surface.row_bytes), static_cast<std::size_t>(surface.row_bytes));
			}
			unlock();
			return true;
		}
	}

	// ---------------------------------------------------------------------------------------------------
	// textures ( GameTextureCache.cs )

	bool map_renderer::impl::create_texture(const std::string& name, const render::texture_upload& upload, gpu_texture& out, std::string* error)
	{
		// GameTextureCache.cs:85-114 ( Create ). A managed texture is filled directly; a default pool texture
		// ( IDirect3DDevice9Ex has no managed pool ) through a system memory copy and UpdateTexture.
		const D3DFORMAT format = static_cast<D3DFORMAT>(upload.d3d_format);
		const D3DPOOL fill_pool = managed ? D3DPOOL_MANAGED : D3DPOOL_SYSTEMMEM;
		com_ptr<IDirect3DBaseTexture9> filled;
		HRESULT hr;
		if (upload.is_cube())
		{
			com_ptr<IDirect3DCubeTexture9> cube;
			hr = device->CreateCubeTexture(static_cast<UINT>(upload.width), static_cast<UINT>(upload.levels), 0, format, fill_pool, cube.put(), nullptr);
			if (SUCCEEDED(hr))
			{
				for (const auto& surface : upload.surfaces)
				{
					if (!copy_surface(surface,
						[&](D3DLOCKED_RECT& rect) { return cube->LockRect(static_cast<D3DCUBEMAP_FACES>(surface.face), static_cast<UINT>(surface.level), &rect, nullptr, 0); },
						[&] { cube->UnlockRect(static_cast<D3DCUBEMAP_FACES>(surface.face), static_cast<UINT>(surface.level)); }))
					{
						hr = E_FAIL;
						break;
					}
				}
				*filled.put() = cube.get();
				cube.get()->AddRef();
			}
		}
		else
		{
			com_ptr<IDirect3DTexture9> texture;
			hr = device->CreateTexture(static_cast<UINT>(upload.width), static_cast<UINT>(upload.height), static_cast<UINT>(upload.levels), 0, format, fill_pool,
				texture.put(), nullptr);
			if (SUCCEEDED(hr))
			{
				for (const auto& surface : upload.surfaces)
				{
					if (!copy_surface(surface,
						[&](D3DLOCKED_RECT& rect) { return texture->LockRect(static_cast<UINT>(surface.level), &rect, nullptr, 0); },
						[&] { texture->UnlockRect(static_cast<UINT>(surface.level)); }))
					{
						hr = E_FAIL;
						break;
					}
				}
				*filled.put() = texture.get();
				texture.get()->AddRef();
			}
		}
		if (FAILED(hr))
		{
			render::set_error(error, "Direct3D cannot create the texture ( " + render::hex32(static_cast<std::uint32_t>(hr)) + ", format "
				+ std::to_string(upload.d3d_format) + ", " + std::to_string(upload.width) + "x" + std::to_string(upload.height) + " )");
			return false;
		}
		if (!managed)
		{
			com_ptr<IDirect3DBaseTexture9> target;
			if (upload.is_cube())
			{
				com_ptr<IDirect3DCubeTexture9> cube;
				hr = device->CreateCubeTexture(static_cast<UINT>(upload.width), static_cast<UINT>(upload.levels), 0, format, D3DPOOL_DEFAULT, cube.put(), nullptr);
				if (SUCCEEDED(hr))
				{
					*target.put() = cube.get();
					cube.get()->AddRef();
				}
			}
			else
			{
				com_ptr<IDirect3DTexture9> texture;
				hr = device->CreateTexture(static_cast<UINT>(upload.width), static_cast<UINT>(upload.height), static_cast<UINT>(upload.levels), 0, format,
					D3DPOOL_DEFAULT, texture.put(), nullptr);
				if (SUCCEEDED(hr))
				{
					*target.put() = texture.get();
					texture.get()->AddRef();
				}
			}
			if (SUCCEEDED(hr))
			{
				hr = device->UpdateTexture(filled.get(), target.get());
			}
			if (FAILED(hr))
			{
				render::set_error(error, "Direct3D cannot create the default pool texture ( " + render::hex32(static_cast<std::uint32_t>(hr)) + " )");
				return false;
			}
			filled = target;
		}
		out.handle = filled;
		out.cube = upload.is_cube();
		out.name = name;
		out.width = upload.width;
		out.height = upload.height;
		out.bytes = upload_bytes(upload);
		stat.texture_bytes += out.bytes;
		return true;
	}

	bool map_renderer::impl::create_solid(gpu_texture& out, const char* name, const std::uint32_t argb)
	{
		// GameTextureCache.cs:150-155 ( CreateSolid )
		render::texture_upload upload;
		upload.d3d_format = render::d3d::FMT_A8R8G8B8;
		upload.width = upload.height = upload.levels = 1;
		render::texture_surface surface;
		surface.width = surface.height = surface.rows = 1;
		surface.row_bytes = 4;
		surface.data = { static_cast<std::uint8_t>(argb), static_cast<std::uint8_t>(argb >> 8), static_cast<std::uint8_t>(argb >> 16), static_cast<std::uint8_t>(argb >> 24) };
		upload.surfaces.push_back(std::move(surface));
		return create_texture(name, upload, out, &last_error);
	}

	gpu_texture* map_renderer::impl::get_streamed_texture(const std::string& name)
	{
		// GameTextureCache.cs:49-83 ( Get / GetByName ) for an image whose pixels are images/<name>.iwi
		if (name.empty())
		{
			return nullptr;
		}
		if (const auto it = textures.find(name); it != textures.end())
		{
			return it->second.get();
		}
		if (texture_failed.count(name))
		{
			return nullptr;
		}
		std::string error;
		std::vector<std::uint8_t> file;
		render::iwi_file iwi;
		render::texture_upload upload;
		auto texture = std::make_unique<gpu_texture>();
		bool ok = images.read(name, file, &error) && render::parse_iwi(file, iwi, &error) && render::build_texture_upload(iwi, wavelet.get(), upload, &error);
		if (ok)
		{
			drop_top_mips(upload, opts.texture_skip_mips);
			ok = create_texture(name, upload, *texture, &error);
		}
		if (!ok)
		{
			texture_failed.emplace(name, true);
			texture_failures.push_back(name + ": " + error);
			return nullptr;
		}
		gpu_texture* result = texture.get();
		textures.emplace(name, std::move(texture));
		return result;
	}

	gpu_texture* map_renderer::impl::get_embedded_texture(const std::string& name, const render::embedded_image& image, const zone::Zone& zone)
	{
		// GameTextureCache.cs:63-83: inline pixels are cached as "name#dataSize"
		const std::string key = name + "#" + std::to_string(image.data_size);
		if (const auto it = textures.find(key); it != textures.end())
		{
			return it->second.get();
		}
		if (texture_failed.count(key))
		{
			return nullptr;
		}
		std::string error;
		render::texture_upload upload;
		auto texture = std::make_unique<gpu_texture>();
		const auto data = zone.bytes(image.data, image.data_size);
		if (!render::build_embedded_upload(image, data, upload, &error) || !create_texture(name, upload, *texture, &error))
		{
			texture_failed.emplace(key, true);
			texture_failures.push_back(name + ": " + error);
			return nullptr;
		}
		gpu_texture* result = texture.get();
		textures.emplace(key, std::move(texture));
		return result;
	}

	gpu_texture* map_renderer::impl::get_image(const zone::Zone& zone, const zaddr image)
	{
		// MapExtractor.cs:885-913 ( ReadImage ) + GameTextureCache.Get
		if (!image)
		{
			return nullptr;
		}
		if (const zone::AssetReference* reference = zone.reference(image))
		{
			return get_streamed_texture(reference->name);
		}
		const auto info = render::read_image(make_zone_view(zone), image);
		if (!info || info->name.empty())
		{
			return nullptr;
		}
		return info->embedded ? get_embedded_texture(info->name, *info->embedded, zone) : get_streamed_texture(info->name);
	}

	gpu_texture* map_renderer::impl::material_texture(material_draw& material, const std::size_t index)
	{
		if (!material.texture_resolved[index])
		{
			// inline pixels were uploaded when the material was made ( its zone is gone by now )
			const auto& def = material.asset().textures[index];
			material.textures[index] = def.has_image && !def.image_name.empty() && !def.embedded ? get_streamed_texture(def.image_name) : nullptr;
			material.texture_resolved[index] = 1;
		}
		return material.textures[index];
	}

	// ---------------------------------------------------------------------------------------------------
	// buffers

	com_ptr<IDirect3DVertexBuffer9> map_renderer::impl::create_vertex_buffer(const std::span<const std::uint8_t> data)
	{
		com_ptr<IDirect3DVertexBuffer9> buffer;
		void* bits = nullptr;
		if (data.empty() || FAILED(device->CreateVertexBuffer(static_cast<UINT>(data.size()), D3DUSAGE_WRITEONLY, 0, static_pool(), buffer.put(), nullptr))
			|| FAILED(buffer->Lock(0, 0, &bits, 0)))
		{
			buffer.reset();
			return buffer;
		}
		std::memcpy(bits, data.data(), data.size());
		buffer->Unlock();
		stat.geometry_bytes += data.size();
		return buffer;
	}

	com_ptr<IDirect3DIndexBuffer9> map_renderer::impl::create_index_buffer(const std::span<const std::uint8_t> data, const bool index32)
	{
		com_ptr<IDirect3DIndexBuffer9> buffer;
		void* bits = nullptr;
		if (data.empty() || FAILED(device->CreateIndexBuffer(static_cast<UINT>(data.size()), D3DUSAGE_WRITEONLY, index32 ? D3DFMT_INDEX32 : D3DFMT_INDEX16,
			static_pool(), buffer.put(), nullptr)) || FAILED(buffer->Lock(0, 0, &bits, 0)))
		{
			buffer.reset();
			return buffer;
		}
		std::memcpy(bits, data.data(), data.size());
		buffer->Unlock();
		stat.geometry_bytes += data.size();
		return buffer;
	}

	// ---------------------------------------------------------------------------------------------------
	// techniques, passes

	const map_light* map_renderer::impl::primary_light(const int index) const
	{
		// GameMapRenderer.cs:1429
		return index > 0 && index < static_cast<int>(lights.size()) ? &lights[static_cast<std::size_t>(index)] : nullptr;
	}

	int map_renderer::impl::choose_technique(const material_draw& material, const int primary_light_index, const bool emissive) const
	{
		// GameMapRenderer.cs:1391-1427
		render::technique_choice choice;
		choice.emissive = emissive;
		choice.primary_light_index = primary_light_index;
		choice.last_sun_primary_light_index = last_sun_light;
		const map_light* light = primary_light(primary_light_index);
		choice.light_type = light ? light->type : 0;
		choice.sun_shadows_active = sun_shadows_active;
		choice.light_shadowed = false;		// spot shadow maps are not ported
		choice.slots = material.slot_mask;
		return render::choose_technique(choice);
	}

	bool map_renderer::impl::blends(const material_draw& material, const int technique_type)
	{
		// GameMapRenderer.cs:1353-1361
		return material.asset().state && material.asset().state->blends(technique_type);
	}

	pass_plan* map_renderer::impl::plan_of(const material_draw& material, const int technique_type, const int pass_index)
	{
		// GameMapRenderer.cs:1660-1679 ( PlanOf ), :1492-1508 ( ArgumentNames ), RenderAssetLibrary.cs:513-550
		// ( ResolveShader / ResolveDeclaration ), GameMapRenderer.cs:1773-1781 ( GetShader )
		const render::render_technique_set& set = material.set();
		const render::render_technique* technique = set.technique(technique_type);
		const render::render_pass& pass = technique->passes[static_cast<std::size_t>(pass_index)];
		auto& slot = pass_plans[&pass];
		if (!slot)
		{
			slot = std::make_unique<pass_plan>();
			pass_plan& plan = *slot;
			const library_pass& own = material.techset->passes[static_cast<std::size_t>(set.slots[static_cast<std::size_t>(technique_type)])][static_cast<std::size_t>(pass_index)];
			std::shared_ptr<const shader_blob> vs = own.vertex_shader, ps = own.pixel_shader;
			if (!vs && pass.vertex_shader)
			{
				vs = scope.find_shader(pass.vertex_shader->name, false);
			}
			if (!ps && pass.pixel_shader)
			{
				ps = scope.find_shader(pass.pixel_shader->name, true);
			}
			plan.vertex_shader_name = pass.vertex_shader ? pass.vertex_shader->name : std::string();
			plan.pixel_shader_name = pass.pixel_shader ? pass.pixel_shader->name : std::string();
			if (pass.declaration)
			{
				const render::vertex_declaration* known = pass.declaration->is_reference ? scope.find_declaration(pass.declaration->name) : nullptr;
				plan.own_declaration = known ? *known : *pass.declaration;
				plan.declaration = &plan.own_declaration;
			}
			const render::shader_program* vertex_program = nullptr;
			const render::shader_program* pixel_program = nullptr;
			const auto program_of = [&](const std::shared_ptr<const shader_blob>& blob) -> const render::shader_program*
			{
				auto it = programs.find(blob.get());
				if (it == programs.end())
				{
					render::shader_program program;
					render::parse_shader_program(blob->code, program);
					it = programs.emplace(blob.get(), std::move(program)).first;
				}
				return &it->second;
			};
			if (vs)
			{
				auto& shader = vertex_shaders[vs.get()];
				if (!shader && FAILED(device->CreateVertexShader(reinterpret_cast<const DWORD*>(vs->code.data()), shader.put())))
				{
					warn("vertex shader " + vs->name + " is rejected by Direct3D");
				}
				plan.vertex_shader = shader.get();
				vertex_program = program_of(vs);
			}
			if (ps)
			{
				auto& shader = pixel_shaders[ps.get()];
				if (!shader && FAILED(device->CreatePixelShader(reinterpret_cast<const DWORD*>(ps->code.data()), shader.put())))
				{
					warn("pixel shader " + ps->name + " is rejected by Direct3D");
				}
				plan.pixel_shader = shader.get();
				pixel_program = program_of(ps);
			}
			plan.complete = plan.vertex_shader && plan.pixel_shader && plan.declaration;
			plan.names.resize(pass.arguments.size());
			for (std::size_t a = 0; a < pass.arguments.size(); a++)
			{
				const auto& arg = pass.arguments[a];
				const render::shader_program* program = arg.is_vertex_stage() ? vertex_program : pixel_program;
				const auto set_kind = arg.type == render::shader_argument_type::code_pixel_sampler ? render::register_set::sampler : render::register_set::float4;
				if (const render::shader_constant* constant = program ? program->find_constant(set_kind, arg.dest) : nullptr)
				{
					plan.names[a] = constant->name;
				}
			}
		}
		pass_plan& plan = *slot;
		if (plan.by_index_generation != constants->by_index_generation())
		{
			plan.handles.assign(pass.arguments.size(), {});
			for (std::size_t a = 0; a < pass.arguments.size(); a++)
			{
				const auto& arg = pass.arguments[a];
				if (arg.is_code_const())
				{
					plan.handles[a] = constants->resolve(arg.code_index(), plan.names[a].empty() ? "code" + std::to_string(arg.code_index()) : plan.names[a]);
				}
			}
			plan.by_index_generation = constants->by_index_generation();
		}
		return &plan;
	}

	IDirect3DVertexDeclaration9* map_renderer::impl::declaration_of(pass_plan& plan, const int format)
	{
		// GameMapRenderer.cs:1783-1793 ( GetDeclaration )
		if (!plan.declaration || format < 0 || format >= render::vertex_format::COUNT)
		{
			return nullptr;
		}
		const auto f = static_cast<std::size_t>(format);
		if (!plan.declaration_resolved[f])
		{
			plan.declaration_resolved[f] = 1;
			auto& cached = declarations[{ plan.declaration->key(), format }];
			if (!cached)
			{
				if (const auto elements = render::build_vertex_elements(plan.declaration->routing, format))
				{
					device->CreateVertexDeclaration(reinterpret_cast<const D3DVERTEXELEMENT9*>(elements->data()), cached.put());
				}
			}
			plan.declarations[f] = cached.get();
		}
		return plan.declarations[f];
	}

	void map_renderer::impl::prepare_technique(material_draw& material, const int technique_type, const int format)
	{
		// GameMapRenderer.cs:1031-1055 ( PrepareTechnique )
		const render::render_technique* technique = material.technique(technique_type);
		if (!technique)
		{
			return;
		}
		for (std::size_t p = 0; p < technique->passes.size(); p++)
		{
			pass_plan* plan = plan_of(material, technique_type, static_cast<int>(p));
			if (!plan->complete)
			{
				continue;
			}
			declaration_of(*plan, format);
			for (const auto& arg : technique->passes[p].arguments)
			{
				if (arg.type != render::shader_argument_type::material_pixel_sampler)
				{
					continue;
				}
				const auto& defs = material.asset().textures;
				for (std::size_t t = 0; t < defs.size(); t++)
				{
					if (defs[t].name_hash == arg.name_hash())
					{
						material_texture(material, t);
						break;
					}
				}
			}
		}
	}

	// ---------------------------------------------------------------------------------------------------
	// load_map

	bool map_renderer::impl::load_map(const std::string& name, std::string* error)
	{
		const auto started = clock::now();
		unload();
		if (!device)
		{
			render::set_error(error, "the renderer is not initialised");
			return false;
		}
		if (!opts.keep_library)
		{
			library.clear();
		}
		stat = {};
		load_context ctx;
		const auto fail = [&](std::string text)
		{
			last_error = text;
			render::set_error(error, std::move(text));
			unload();
			return false;
		};

		// ---- the library: level zones, then the shared zones ( RenderLayouts.cs:98-128 ). Each is loaded, frozen
		// and released before the next one, and before the map zone is read.
		std::vector<std::shared_ptr<const library_zone>> layers;
		const std::vector<std::string> level_names = level_zone_names(name);
		for (const auto& names : { level_names, shared_zone_names() })
		{
			for (const std::string& zone_name : names)
			{
				std::string zone_error;
				if (auto frozen = library.frozen(zone_name, &zone_error))
				{
					layers.push_back(std::move(frozen));
				}
				else if (!zone_error.empty())
				{
					warn("zone " + zone_name + ": " + zone_error);
				}
			}
		}

		// ---- the map zone
		std::string zone_error;
		ctx.map_zone = library.load_zone(name, &zone_error);
		if (!ctx.map_zone)
		{
			return fail(zone_error);
		}
		const zone::Zone& zone = *ctx.map_zone;
		ctx.peak_bytes = zone.stream_size();
		for (const std::string& warning : zone.warnings())
		{
			warn(name + ": " + warning);
		}
		const zaddr world_address = zone::first_asset(zone, c1::ASSET_TYPE_GFXWORLD);
		zone::GfxWorldView world;
		if (!world_address || !zone::read_gfx_world(zone, world_address, world))
		{
			return fail("zone " + name + " has no GfxWorld");
		}
		{
			std::vector<std::string> freeze_warnings;
			scope.zones.push_back(zone_library::freeze(zone, &freeze_warnings));
			for (auto& warning : freeze_warnings)
			{
				warn(std::move(warning));
			}
			scope.zones.insert(scope.zones.end(), layers.begin(), layers.end());
		}
		map_name = name;

		// ---- constants ( GameMapRenderer.cs:94-105 )
		constants = std::make_unique<render::code_constants>(opts.settings);
		last_sun_light = static_cast<int>(world.raw.lastSunPrimaryLightIndex);
		constants->outdoor_lookup = mat4::from_floats(&world.raw.outdoorLookupMatrix[0][0]);
		world_mid = vec3(world.raw.bounds.midPoint);
		world_half = vec3(world.raw.bounds.halfSize);

		if (!create_solid(white, "$white", render::WHITE_ARGB) || !create_solid(black, "$black", render::BLACK_ARGB)
			|| !create_solid(flat_normal, "$identitynormalmap", render::IDENTITY_NORMAL_ARGB))
		{
			return fail("cannot create the stand-in textures: " + last_error);
		}

		// zones other than the map zone, one at a time ( RenderAssetLibrary.FindModelFile + GameReader.Load )
		const auto acquire_zone = [&](const std::string& zone_name) -> std::shared_ptr<const zone::Zone>
		{
			if (equals_ignore_case(zone_name, zone.name()))
			{
				return ctx.map_zone;
			}
			if (!ctx.foreign_zone || !equals_ignore_case(ctx.foreign_zone->name(), zone_name))
			{
				ctx.foreign_zone.reset();
				std::string load_error;
				ctx.foreign_zone = library.load_zone(zone_name, &load_error);
				if (!ctx.foreign_zone)
				{
					warn("zone " + zone_name + ": " + load_error);
					return nullptr;
				}
				ctx.peak_bytes = std::max<std::uint64_t>(ctx.peak_bytes, zone.stream_size() + ctx.foreign_zone->stream_size());
			}
			return ctx.foreign_zone;
		};

		// ---- materials ( GameMapRenderer.cs:492-531 GetMaterial )
		std::map<const library_material*, material_draw*> material_by_asset;
		const auto make_material = [&](const std::shared_ptr<const library_material>& asset, const zone::Zone* defining_zone) -> material_draw*
		{
			if (const auto it = material_by_asset.find(asset.get()); it != material_by_asset.end())
			{
				return it->second;
			}
			material_draw* result = nullptr;
			std::shared_ptr<const library_techset> techset = asset->techset;
			if (!techset && !asset->asset.technique_set_name.empty())
			{
				techset = scope.find_techset(asset->asset.technique_set_name);
			}
			if (!techset)
			{
				warn("material " + asset->asset.name + ": technique set " + asset->asset.technique_set_name + " not found");
			}
			else
			{
				auto draw = std::make_unique<material_draw>();
				draw->material = asset;
				draw->techset = techset;
				draw->id = ++next_material_id;
				draw->slot_mask = techset->set.slot_mask();
				draw->textures.assign(asset->asset.textures.size(), nullptr);
				draw->texture_resolved.assign(asset->asset.textures.size(), 0);
				// inline pixels can only be read while the defining zone is in memory
				std::shared_ptr<const zone::Zone> holder;
				for (std::size_t t = 0; t < asset->asset.textures.size(); t++)
				{
					const auto& def = asset->asset.textures[t];
					if (!def.embedded || !def.has_image)
					{
						continue;
					}
					if (!defining_zone)
					{
						holder = acquire_zone(asset->zone_name);
						defining_zone = holder.get();
					}
					draw->textures[t] = defining_zone ? get_embedded_texture(def.image_name, *def.embedded, *defining_zone) : nullptr;
					draw->texture_resolved[t] = 1;
				}
				result = draw.get();
				materials.emplace(std::to_string(draw->id), std::move(draw));
			}
			material_by_asset.emplace(asset.get(), result);
			return result;
		};
		const auto get_material = [&](const zone::Zone& from, const zaddr address) -> material_draw*
		{
			if (!address || from.bytes(address, 4).empty())
			{
				return nullptr;
			}
			const std::string material_name(from.asset_name(c1::ASSET_TYPE_MATERIAL, address));
			const std::string_view stored = from.c_string(from.get<zaddr>(address));
			const bool reference = from.reference(address) != nullptr || (!stored.empty() && stored[0] == ',');
			std::shared_ptr<const library_material> asset;
			const zone::Zone* defining = nullptr;
			if (!reference)
			{
				// defined in the zone the pointer is from
				std::shared_ptr<const library_zone> own = &from == &zone ? scope.zones[0] : library.frozen(from.name(), nullptr);
				if (own)
				{
					if (const auto it = own->materials.find(material_name); it != own->materials.end())
					{
						asset = it->second;
						defining = &from;
					}
				}
			}
			if (!asset)
			{
				asset = scope.find_material(material_name);
				if (asset && equals_ignore_case(asset->zone_name, from.name()))
				{
					defining = &from;
				}
			}
			if (!asset)
			{
				warn("material " + material_name + " is defined in a zone that is not loaded");
				return nullptr;
			}
			return make_material(asset, defining);
		};

		// ---- world geometry ( GameMapRenderer.cs:244-323 BuildWorld / BuildWorldIndices )
		vertex_buffer = create_vertex_buffer(world.vertexBytes);
		index_buffer = create_index_buffer(world.indexBytes, false);
		if (!vertex_buffer || !index_buffer)
		{
			return fail("cannot create the world vertex / index buffers");
		}
		if (world.raw.draw.vertexLayerDataSize > 0)
		{
			const auto layer = zone.bytes(world.raw.draw.vertexLayerData, world.raw.draw.vertexLayerDataSize);
			if (!layer.empty())
			{
				layer_buffer = create_vertex_buffer(layer);
			}
		}
		const int surface_count = static_cast<int>(world.surfaces.size());
		const auto bounds = zone.get_array<c1::Bounds>(world.raw.dpvs.surfacesBounds, static_cast<std::size_t>(surface_count));
		const bool has_bounds = !bounds.empty();
		surfaces.reserve(static_cast<std::size_t>(surface_count));
		for (int i = 0; i < surface_count; i++)
		{
			const c1::GfxSurface& raw = world.surfaces[static_cast<std::size_t>(i)].raw;
			world_surface s;
			s.index = i;
			s.material = get_material(zone, raw.material);
			s.first_vertex = static_cast<int>(raw.firstVertex);
			s.vertex_count = raw.vertexCount;
			s.triangle_count = raw.triCount;
			s.base_index = static_cast<int>(raw.baseIndex);
			s.layer_data_offset = raw.vertexLayerData;
			s.lightmap_index = raw.lightmapIndex;
			s.reflection_probe_index = raw.reflectionProbeIndex;
			s.primary_light_index = raw.primaryLightIndex;
			if (has_bounds)
			{
				s.center = vec3(bounds[static_cast<std::size_t>(i)].midPoint);
				s.half = vec3(bounds[static_cast<std::size_t>(i)].halfSize);
				s.radius = length(s.half);
			}
			surfaces.push_back(s);
		}
		const auto clamp_range = [&](const std::uint32_t value) { return static_cast<int>(std::min<std::uint32_t>(value, static_cast<std::uint32_t>(surface_count))); };
		const auto& dpvs = world.raw.dpvs;
		ranges.push_back({ clamp_range(dpvs.litOpaqueSurfsBegin), clamp_range(dpvs.litOpaqueSurfsEnd), false });
		ranges.push_back({ clamp_range(dpvs.emissiveSurfsBegin), clamp_range(dpvs.emissiveSurfsEnd), true });
		ranges.push_back({ clamp_range(dpvs.litTransSurfsBegin), clamp_range(dpvs.litTransSurfsEnd), false });
		for (int i = ranges[1].begin; i < ranges[1].end; i++)
		{
			surfaces[static_cast<std::size_t>(i)].emissive = true;
		}
		{
			const auto indices = world.indexBytes;
			std::size_t total = 0;
			for (const auto& s : surfaces)
			{
				total += 3u * static_cast<std::size_t>(s.triangle_count);
			}
			std::vector<std::uint32_t> absolute(total);
			std::size_t at = 0;
			for (auto& s : surfaces)
			{
				s.absolute_index = static_cast<int>(at);
				const std::size_t count = 3u * static_cast<std::size_t>(s.triangle_count);
				for (std::size_t k = 0; k < count; k++)
				{
					const std::size_t source = 2 * (static_cast<std::size_t>(s.base_index) + k);
					absolute[at + k] = source + 1 < indices.size() ? static_cast<std::uint32_t>(s.first_vertex) + render::rd_u16(indices.data() + source)
						: static_cast<std::uint32_t>(s.first_vertex);
				}
				at += count;
			}
			if (total > 0)
			{
				world_indices = create_index_buffer({ reinterpret_cast<const std::uint8_t*>(absolute.data()), absolute.size() * 4 }, true);
			}
		}

		// ---- lightmaps, reflection probes ( MapExtractor.cs:207-234 ), lights ( :852-883 ), light defs ( RenderLightDef.cs )
		if (world.raw.draw.lightmapCount > 0)
		{
			const auto pointers = zone.get_array<zaddr>(world.raw.draw.lightmaps, 2u * static_cast<std::size_t>(world.raw.draw.lightmapCount));
			for (std::size_t i = 0; i + 1 < pointers.size(); i += 2)
			{
				lightmaps.push_back({ get_image(zone, pointers[i]), get_image(zone, pointers[i + 1]) });
			}
		}
		{
			const std::size_t count = world.raw.draw.reflectionProbeCount;
			const auto origins = zone.get_array<float>(world.raw.draw.reflectionProbeOrigins, 3 * count);
			const auto probe_images = zone.get_array<zaddr>(world.raw.draw.reflectionProbes, count);
			for (std::size_t i = 0; i < count && !origins.empty(); i++)
			{
				probes.push_back({ vec3(&origins[3 * i]), probe_images.empty() ? nullptr : get_image(zone, probe_images[i]) });
			}
		}
		if (const zaddr com_world = zone::first_asset(zone, c1::ASSET_TYPE_COMWORLD))
		{
			zone::ComWorldView com;
			if (zone::read_com_world(zone, com_world, com) && com.primaryLightStride >= 68)
			{
				for (std::uint32_t i = 0; i < com.raw.primaryLightCount; i++)
				{
					const std::uint8_t* l = com.primaryLightBytes.data() + static_cast<std::size_t>(i) * com.primaryLightStride;
					map_light light;
					light.index = static_cast<int>(i);
					light.type = l[0];
					light.can_use_shadow_map = l[1] != 0;
					light.exponent = l[2];
					light.color = { render::rd_f32(l + 4), render::rd_f32(l + 8), render::rd_f32(l + 12) };
					light.direction = { render::rd_f32(l + 16), render::rd_f32(l + 20), render::rd_f32(l + 24) };
					light.origin = { render::rd_f32(l + 28), render::rd_f32(l + 32), render::rd_f32(l + 36) };
					light.radius = render::rd_f32(l + 40);
					light.cos_half_fov_outer = render::rd_f32(l + 44);
					light.cos_half_fov_inner = render::rd_f32(l + 48);
					light.cos_half_fov_expanded = render::rd_f32(l + 52);
					light.rotation_limit = render::rd_f32(l + 56);
					light.def_name = std::string(zone.c_string(render::rd_u32(l + 64)));
					lights.push_back(std::move(light));
				}
			}
		}
		for (const zone::AssetRecord& record : zone.all_assets())
		{
			if (record.type != c1::ASSET_TYPE_LIGHT_DEF)
			{
				continue;
			}
			const std::string_view def_name = zone.c_string(zone.get<zaddr>(record.header));
			if (def_name.empty() || def_name[0] == ',')
			{
				continue;
			}
			light_def def;
			def.name = std::string(def_name);
			def.attenuation = get_image(zone, zone.get<zaddr>(record.header + 4));
			def.sampler_state = zone.get<std::uint8_t>(record.header + 8);
			def.lookup_start = zone.get<std::int32_t>(record.header + 12);
			def.attenuation_width = def.attenuation ? def.attenuation->width : 0;
			light_defs.emplace(def.name, std::move(def));
		}
		outdoor = get_image(zone, world.raw.outdoorImage);		// GameMapRenderer.cs:117-133
		for (const map_light& light : lights)
		{
			// GameMapRenderer.SunShadow.cs:30-41 ( Sun )
			if (light.type == 1 && light.index <= last_sun_light && !(light.direction == float3{}))
			{
				sun = &light;
				break;
			}
		}

		// ---- models ( GameMapRenderer.cs:533-678 ResolveModel / FindForeignModel / GetModel )
		const auto reads_model_lighting = [&](const material_draw& material)
		{
			// GameMapRenderer.cs:680-682: a vertex shader of the set reads "baseLightingCoords"
			const auto& set = material.set();
			for (std::size_t t = 0; t < set.techniques.size(); t++)
			{
				for (std::size_t p = 0; p < set.techniques[t].passes.size(); p++)
				{
					const auto& pass = set.techniques[t].passes[p];
					std::shared_ptr<const shader_blob> vs = material.techset->passes[t][p].vertex_shader;
					if (!vs && pass.vertex_shader)
					{
						vs = scope.find_shader(pass.vertex_shader->name, false);
					}
					if (!vs)
					{
						continue;
					}
					auto it = programs.find(vs.get());
					if (it == programs.end())
					{
						render::shader_program program;
						render::parse_shader_program(vs->code, program);
						it = programs.emplace(vs.get(), std::move(program)).first;
					}
					for (const auto& arg : pass.arguments)
					{
						if (arg.type != render::shader_argument_type::code_vertex_const)
						{
							continue;
						}
						const render::shader_constant* constant = it->second.find_constant(render::register_set::float4, arg.dest);
						if (constant && constant->name == "baseLightingCoords")
						{
							return true;
						}
					}
				}
			}
			return false;
		};
		std::map<const material_draw*, bool> lighting_by_material;
		name_map<bool> streamed_names;

		const auto build_model = [&](const zone::Zone& from, const zaddr header, const std::string& model_name) -> model_draw*
		{
			auto draw = std::make_unique<model_draw>();
			draw->name = model_name;
			c1::XModel raw;
			from.get(header, raw);
			const int lod_count = std::clamp(static_cast<int>(static_cast<std::int8_t>(raw.numLods)), 0, 4);
			float3 min{ FLT_MAX, FLT_MAX, FLT_MAX }, max{ -FLT_MAX, -FLT_MAX, -FLT_MAX };
			for (int lod = 0; lod < (opts.load_model_lods ? std::max(lod_count, 1) : 1); lod++)
			{
				std::vector<std::string> model_warnings;
				bool streamed = false;
				const auto read = read_render_surfaces(from, header, lod, model_archives.get(), model_warnings, &streamed);
				for (const std::string& warning : model_warnings)
				{
					warn(model_name + ": " + warning);
				}
				if (lod == 0 && streamed)
				{
					stat.streamed_models++;
				}
				if (lod > 0 && read.empty())
				{
					break;
				}
				model_lod_draw out;
				out.dist = lod < 4 ? raw.lodInfo[lod].dist : 0;
				std::size_t vertices = 0, index_count = 0;
				for (const auto& surface : read)
				{
					vertices += static_cast<std::size_t>(surface.vertex_count);
					index_count += 3u * static_cast<std::size_t>(surface.triangle_count);
				}
				std::vector<std::uint8_t> vertex_data(32 * vertices);
				std::vector<std::uint32_t> index_data(index_count);
				std::size_t vertex_at = 0, index_at = 0;
				for (const auto& surface : read)
				{
					std::memcpy(vertex_data.data() + 32 * vertex_at, surface.vertices.data(), 32u * static_cast<std::size_t>(surface.vertex_count));
					for (std::size_t k = 0; k < 3u * static_cast<std::size_t>(surface.triangle_count); k++)
					{
						index_data[index_at + k] = static_cast<std::uint32_t>(vertex_at) + render::rd_u16(surface.indices.data() + 2 * k);
					}
					model_surface_draw s;
					s.material = get_material(from, surface.material);
					s.rigid_bones = surface.rigid_bones;
					s.vertex_count = surface.vertex_count;
					s.triangle_count = surface.triangle_count;
					s.first_vertex = static_cast<int>(vertex_at);
					s.base_index = static_cast<int>(index_at);
					if (lod == 0)
					{
						for (int v = 0; v < surface.vertex_count; v++)
						{
							const std::uint8_t* p = surface.vertices.data() + 32 * static_cast<std::size_t>(v);
							const float3 position{ render::rd_f32(p), render::rd_f32(p + 4), render::rd_f32(p + 8) };
							min = { std::min(min.x, position.x), std::min(min.y, position.y), std::min(min.z, position.z) };
							max = { std::max(max.x, position.x), std::max(max.y, position.y), std::max(max.z, position.z) };
						}
						if (s.material)
						{
							auto it = lighting_by_material.find(s.material);
							if (it == lighting_by_material.end())
							{
								it = lighting_by_material.emplace(s.material, reads_model_lighting(*s.material)).first;
							}
							draw->uses_lighting_volume |= it->second;
						}
					}
					out.surfaces.push_back(std::move(s));
					vertex_at += static_cast<std::size_t>(surface.vertex_count);
					index_at += 3u * static_cast<std::size_t>(surface.triangle_count);
				}
				if (vertices > 0)
				{
					out.vertex_buffer = create_vertex_buffer(vertex_data);
					out.index_buffer = create_index_buffer({ reinterpret_cast<const std::uint8_t*>(index_data.data()), index_data.size() * 4 }, true);
				}
				draw->lods.push_back(std::move(out));
			}
			if (draw->lods.empty())
			{
				draw->lods.emplace_back();
			}
			if (!draw->lods[0].surfaces.empty())
			{
				draw->min = min;
				draw->max = max;
			}
			zone::ModelView view;
			if (zone::read_model(from, header, view, false))
			{
				for (const auto& bone : view.bones)
				{
					draw->bone_names.emplace_back(bone.name);
				}
			}
			model_draw* result = draw.get();
			models.emplace(model_name, std::move(draw));
			return result;
		};
		name_map<model_draw*> resolved_models;		// by name: nullptr = not found
		const auto resolve_model = [&](const std::string& model_name) -> model_draw*
		{
			if (const auto it = resolved_models.find(model_name); it != resolved_models.end())
			{
				return it->second;
			}
			model_draw* result = nullptr;
			std::string stored_name = model_name;		// Zone::find_asset compares exactly, entity keys do not
			const std::string zone_name = scope.find_model_zone(model_name, &stored_name);
			if (!zone_name.empty())
			{
				if (const auto holder = acquire_zone(zone_name))
				{
					if (const zaddr header = holder->find_asset(c1::ASSET_TYPE_XMODEL, stored_name))
					{
						result = build_model(*holder, header, model_name);
						if (holder != ctx.map_zone)
						{
							stat.foreign_models++;
						}
					}
				}
			}
			resolved_models.emplace(model_name, result);
			return result;
		};
		const auto get_model = [&](const zaddr header) -> model_draw*
		{
			// a model pointer of the map zone
			if (!header || zone.bytes(header, 4).empty())
			{
				return nullptr;
			}
			const std::string model_name(zone.asset_name(c1::ASSET_TYPE_XMODEL, header));
			if (model_name.empty())
			{
				warn("model " + render::hex32(header) + " has no name");
				return nullptr;
			}
			model_draw* result = resolve_model(model_name);
			if (!result)
			{
				warn("model " + model_name + " is not in the map, level or shared zones");
			}
			return result;
		};

		// ---- static models ( GameMapRenderer.cs:325-365 ReadStaticModels ). Models of other zones are resolved
		// zone by zone so that only one of those zones is in memory at a time.
		const auto insts = zone.get_array<c1::GfxStaticModelInst>(dpvs.smodelInsts, world.staticModels.size());
		int static_count = 0;
		if (!world.staticModels.empty() && insts.size() == world.staticModels.size())
		{
			static_count = static_cast<int>(world.staticModels.size());
			std::vector<std::pair<std::string, std::string>> order;		// ( zone, model ) of every distinct model
			{
				name_set seen;
				for (const auto& sm : world.staticModels)
				{
					const std::string model_name(zone.asset_name(c1::ASSET_TYPE_XMODEL, sm.raw.model));
					if (!model_name.empty() && seen.insert(model_name).second)
					{
						order.emplace_back(to_lower(scope.find_model_zone(model_name)), model_name);
					}
				}
				std::stable_sort(order.begin(), order.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
				for (const auto& [zone_name, model_name] : order)
				{
					resolve_model(model_name);
				}
			}
			static_models.reserve(world.staticModels.size());
			for (std::size_t i = 0; i < world.staticModels.size(); i++)
			{
				const c1::GfxStaticModelDrawInst& d = world.staticModels[i].raw;
				const float scale = d.scale;
				const float3 x = vec3(d.axis[0]) * scale, y = vec3(d.axis[1]) * scale, z = vec3(d.axis[2]) * scale;
				static_model_instance instance;
				instance.draw = get_model(d.model);
				instance.placement = mat4::from_rows({ x.x, x.y, x.z, 0 }, { y.x, y.y, y.z, 0 }, { z.x, z.y, z.z, 0 }, { d.origin[0], d.origin[1], d.origin[2], 1 });
				instance.center = vec3(insts[i].bounds.midPoint);
				instance.half = vec3(insts[i].bounds.halfSize);
				instance.radius = length(instance.half);
				instance.static_index = static_cast<int>(i);
				instance.lighting_origin = vec3(insts[i].lightingOrigin);
				instance.cull_distance = d.cullDist;
				instance.reflection_probe = d.reflectionProbeIndex;
				instance.primary_light = d.primaryLightIndex;
				instance.flags = d.flags;
				instance.ground_lighting = render::rd_u32(d.groundLighting);
				instance.scale = scale;
				static_models.push_back(std::move(instance));
			}
		}
		stat.static_model_count = static_cast<int>(static_models.size());

		// ---- entities ( MapEnts ), script_model entities ( GameMapRenderer.cs:367-435 ReadEntityModels )
		if (const zaddr ents = zone::find_map_ents(zone))
		{
			zone::MapEntsView view;
			if (zone::read_map_ents(zone, ents, view))
			{
				entity_string = std::string(view.entityString);
			}
		}
		entities = parse_entities(entity_string);
		const auto nearest_probe = [&](const float3& point)
		{
			// GameMapRenderer.cs:459-473
			int best = 0;
			float best_distance = FLT_MAX;
			for (std::size_t i = probes.size() > 1 ? 1 : 0; i < probes.size(); i++)
			{
				const float3 d = probes[i].origin - point;
				const float distance = dot(d, d);
				if (distance < best_distance)
				{
					best_distance = distance;
					best = static_cast<int>(i);
				}
			}
			return best;
		};
		const auto entity_placement = [](const float3& origin, const float3& angles, const float scale)
		{
			camera orientation;
			orientation.pitch = angles.x;
			orientation.yaw = angles.y;
			orientation.roll = angles.z;
			const auto axes = orientation.axes();
			return mat4::from_rows(
				{ axes.forward.x * scale, axes.forward.y * scale, axes.forward.z * scale, 0 },
				{ axes.left.x * scale, axes.left.y * scale, axes.left.z * scale, 0 },
				{ axes.up.x * scale, axes.up.y * scale, axes.up.z * scale, 0 },
				{ origin.x, origin.y, origin.z, 1 });
		};
		const auto placed_box = [](const float3& min, const float3& max, const mat4& placement, float3& center, float3& half)
		{
			float3 box_min{ FLT_MAX, FLT_MAX, FLT_MAX }, box_max{ -FLT_MAX, -FLT_MAX, -FLT_MAX };
			for (int corner = 0; corner < 8; corner++)
			{
				const float3 p = transform_point({ (corner & 1) ? max.x : min.x, (corner & 2) ? max.y : min.y, (corner & 4) ? max.z : min.z }, placement);
				box_min = { std::min(box_min.x, p.x), std::min(box_min.y, p.y), std::min(box_min.z, p.z) };
				box_max = { std::max(box_max.x, p.x), std::max(box_max.y, p.y), std::max(box_max.z, p.z) };
			}
			center = (box_min + box_max) * 0.5f;
			half = (box_max - box_min) * 0.5f;
		};
		for (const map_entity& entity : entities)
		{
			const std::string* model_key = entity.get("model");
			if (!model_key || model_key->empty())
			{
				continue;
			}
			const float3 origin = parse_vector(entity.get("origin")).value_or(float3{});
			const float3 angles = parse_vector(entity.get("angles")).value_or(float3{});
			if ((*model_key)[0] == '*')
			{
				// A brush model: the surfaces of GfxWorld::models[ N ] at the entity's origin and angles. Not in the
				// C# renderer ( it draws the three static ranges only ).
				const int index = std::atoi(model_key->c_str() + 1);
				if (index <= 0 || index >= static_cast<int>(world.brushModels.size()))
				{
					continue;
				}
				const c1::GfxBrushModel& brush = world.brushModels[static_cast<std::size_t>(index)];
				if (brush.surfaceCount == 0 || static_cast<int>(brush.startSurfIndex) + brush.surfaceCount > surface_count)
				{
					continue;
				}
				brush_model_instance instance;
				instance.model = index;
				instance.entity_index = entity.index;
				instance.placement = entity_placement(origin, angles, 1.0f);
				const float3 mid = vec3(brush.bounds.midPoint), half = vec3(brush.bounds.halfSize);
				placed_box(mid - half, mid + half, instance.placement, instance.center, instance.half);
				instance.radius = length(instance.half);
				instance.first_surface = brush.startSurfIndex;
				instance.surface_count = brush.surfaceCount;
				const std::string* special = entity.get("script_specialops");
				instance.special_ops = special && *special == "1";
				for (int s = 0; s < instance.surface_count; s++)
				{
					surfaces[static_cast<std::size_t>(instance.first_surface + s)].brush_model = true;
				}
				brush_models.push_back(instance);
				continue;
			}
			if (!equals_ignore_case(entity.class_name(), "script_model") || entity.get("script_gameobjectname"))
			{
				continue;
			}
			model_draw* draw = resolve_model(*model_key);
			if (!draw)
			{
				warn("entity model " + *model_key + " is not in the map, level or shared zones");
				continue;
			}
			if (draw->surfaces().empty())
			{
				continue;
			}
			float scale = 1.0f;
			if (const std::string* text = entity.get("modelscale"))
			{
				if (const auto value = parse_float(*text); value && *value > 0)
				{
					scale = *value;
				}
			}
			static_model_instance instance;
			instance.draw = draw;
			instance.placement = entity_placement(origin, angles, scale);
			instance.center = transform_point((draw->min + draw->max) * 0.5f, instance.placement);
			float3 unused;
			placed_box(draw->min, draw->max, instance.placement, unused, instance.half);
			instance.radius = length(draw->max - draw->min) * 0.5f * scale;
			instance.lighting_origin = parse_vector(entity.get("ltOrigin")).value_or(instance.center);
			instance.reflection_probe = nearest_probe(instance.lighting_origin);
			if (const std::string* ground = entity.get("gndLt"); ground && ground->size() == 10)
			{
				// GameMapRenderer.cs:456-457 ( GroundLightingLight ): the last two hex digits
				char* end = nullptr;
				const long value = std::strtol(ground->c_str() + 8, &end, 16);
				instance.primary_light = end == ground->c_str() + 10 ? static_cast<int>(value) : 0;
			}
			instance.is_entity = true;
			instance.entity_index = entity.index;
			instance.scale = scale;
			// GameMapRenderer.cs:439-454: the damaged parts of a destructible ( bones ending in "_d" ) are hidden
			const std::string* target_name = entity.get("targetname");
			if (entity.get("destructible_type") || entity.get("destructibledef") || (target_name && to_lower(*target_name).rfind("destructible", 0) == 0))
			{
				std::vector<std::uint8_t> hidden(draw->surfaces().size(), 0);
				bool any = false;
				for (std::size_t s = 0; s < hidden.size(); s++)
				{
					const auto& bones = draw->surfaces()[s].rigid_bones;
					bool all = !bones.empty();
					for (const int bone : bones)
					{
						const bool damaged = bone >= 0 && bone < static_cast<int>(draw->bone_names.size())
							&& draw->bone_names[static_cast<std::size_t>(bone)].size() >= 2
							&& to_lower(draw->bone_names[static_cast<std::size_t>(bone)]).ends_with("_d");
						all = all && damaged;
					}
					hidden[s] = all ? 1 : 0;
					any = any || all;
				}
				if (any)
				{
					instance.hidden_surfaces = std::move(hidden);
				}
			}
			static_models.push_back(std::move(instance));
			stat.entity_model_count++;
		}
		stat.brush_model_count = static_cast<int>(brush_models.size());
		ctx.foreign_zone.reset();

		// ---- Prepare ( GameMapRenderer.cs:964-1020 ): shaders, declarations and textures of what will be drawn
		update_sun_shadow_state();
		for (const range& r : ranges)
		{
			for (int i = r.begin; i < r.end; i++)
			{
				const world_surface& s = surfaces[static_cast<std::size_t>(i)];
				if (s.material)
				{
					prepare_technique(*s.material, choose_technique(*s.material, s.primary_light_index, r.emissive),
						render::vertex_format::WORLD + s.material->set().world_vert_format);
				}
			}
		}
		for (const brush_model_instance& instance : brush_models)
		{
			for (int i = 0; i < instance.surface_count; i++)
			{
				const world_surface& s = surfaces[static_cast<std::size_t>(instance.first_surface + i)];
				if (s.material)
				{
					prepare_technique(*s.material, choose_technique(*s.material, s.primary_light_index, false),
						render::vertex_format::WORLD + s.material->set().world_vert_format);
				}
			}
		}
		for (const static_model_instance& instance : static_models)
		{
			if (!instance.draw)
			{
				continue;
			}
			for (const auto& lod : instance.draw->lods)
			{
				for (const auto& surface : lod.surfaces)
				{
					if (surface.material)
					{
						prepare_technique(*surface.material, choose_technique(*surface.material, instance.primary_light, false), render::vertex_format::PACKED);
					}
				}
			}
		}

		// ---- model lighting ( GameMapRenderer.cs:1057-1126 BuildModelLighting )
		if (!static_models.empty())
		{
			auto grid = light_grid::read(zone, world_address);
			if (!grid)
			{
				warn("the light grid could not be read; static models use their stored ground lighting");
			}
			std::unique_ptr<world_sight_trace> trace = grid ? std::make_unique<world_sight_trace>(zone) : nullptr;
			light_grid::blocked_fn blocked;
			if (trace)
			{
				blocked = [&trace](const float3 start, const float3 end) { return trace->blocked(start, end); };
			}
			std::vector<static_model_instance*> lit;
			for (auto& instance : static_models)
			{
				if (instance.draw && instance.draw->uses_lighting_volume)
				{
					lit.push_back(&instance);
					continue;
				}
				if ((instance.flags & 0x20) == 0 && grid)
				{
					const auto sample = grid->sample(instance.lighting_origin, instance.primary_light, lights, last_sun_light, blocked);
					if (!sample.is_fallback)
					{
						instance.ground_lighting = light_grid::ground_lighting(grid->blend(sample), sample.sun_visibility);
					}
					if (instance.is_entity)
					{
						instance.primary_light = sample.primary_light;
					}
				}
			}
			if (!lit.empty())
			{
				lighting_volume = std::make_unique<model_lighting_volume>(static_cast<int>(lit.size()));
				std::array<std::uint32_t, 64> block{};
				int handle = 0;
				for (static_model_instance* instance : lit)
				{
					instance->lighting_handle = ++handle;
					if ((instance->flags & 0x20) != 0 || !grid)
					{
						const std::uint32_t c = instance->ground_lighting;
						block.fill(((c >> 24) << 24) | ((c & 0xFF) << 16) | (((c >> 8) & 0xFF) << 8) | ((c >> 16) & 0xFF));
					}
					else
					{
						const auto sample = grid->sample(instance->lighting_origin, instance->primary_light, lights, last_sun_light, blocked);
						const auto colors = grid->blend(sample);
						light_grid::write_block(colors, sample.sun_visibility, block);
						if (!sample.is_fallback)
						{
							instance->ground_lighting = light_grid::ground_lighting(colors, sample.sun_visibility);
						}
						if (instance->is_entity)
						{
							instance->primary_light = sample.primary_light;
						}
					}
					lighting_volume->set_block(instance->lighting_handle, block);
				}
				// GameMapRenderer.cs:1120-1124: the $modellighting volume texture
				const int height = lighting_volume->height();
				com_ptr<IDirect3DVolumeTexture9> filled;
				const D3DPOOL fill_pool = managed ? D3DPOOL_MANAGED : D3DPOOL_SYSTEMMEM;
				D3DLOCKED_BOX box{};
				if (SUCCEEDED(device->CreateVolumeTexture(model_lighting_volume::WIDTH, static_cast<UINT>(height), model_lighting_volume::DEPTH, 1, 0, D3DFMT_A8R8G8B8,
					fill_pool, filled.put(), nullptr)) && SUCCEEDED(filled->LockBox(0, &box, nullptr, 0)))
				{
					const auto& texels = lighting_volume->texels();
					for (int z = 0; z < model_lighting_volume::DEPTH; z++)
					{
						for (int y = 0; y < height; y++)
						{
							std::memcpy(static_cast<std::uint8_t*>(box.pBits) + static_cast<std::size_t>(z) * static_cast<std::size_t>(box.SlicePitch)
								+ static_cast<std::size_t>(y) * static_cast<std::size_t>(box.RowPitch),
								texels.data() + (static_cast<std::size_t>(z) * static_cast<std::size_t>(height) + static_cast<std::size_t>(y)) * model_lighting_volume::WIDTH,
								4u * model_lighting_volume::WIDTH);
						}
					}
					filled->UnlockBox(0);
					com_ptr<IDirect3DVolumeTexture9> final_texture = filled;
					if (!managed)
					{
						com_ptr<IDirect3DVolumeTexture9> target;
						if (SUCCEEDED(device->CreateVolumeTexture(model_lighting_volume::WIDTH, static_cast<UINT>(height), model_lighting_volume::DEPTH, 1, 0,
							D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, target.put(), nullptr)) && SUCCEEDED(device->UpdateTexture(filled.get(), target.get())))
						{
							final_texture = target;
						}
						else
						{
							final_texture.reset();
						}
					}
					if (final_texture)
					{
						*model_lighting.handle.put() = final_texture.get();
						final_texture.get()->AddRef();
						model_lighting.name = "$modellighting";
						constants->set("lightingLookupScale", lighting_volume->lookup_scale());
					}
				}
				if (!model_lighting.handle)
				{
					warn("the model lighting volume texture could not be created");
					lighting_volume.reset();
				}
			}
			if (trace)
			{
				stat.light_grid_traces = trace->traces();
				stat.light_grid_blocked = trace->blocked_traces();
			}
		}

		// ---- visibility ( GameMapRenderer.cs:1022-1029 BuildVisibility )
		{
			std::vector<std::string> cell_warnings;
			if (auto cells = read_world_cells(zone, world_address, surface_count, static_count, cell_warnings); cells && !cells->cells.empty())
			{
				stat.cell_count = static_cast<int>(cells->cells.size());
				visibility = std::make_unique<world_visibility>(std::move(*cells), surface_count, static_count);
			}
			for (auto& warning : cell_warnings)
			{
				warn(std::move(warning));
			}
		}

		// ---- fog. The C# renderer never sets fog ( GameFog.None ). In this engine family the fog of a map is not
		// part of its fastfile assets: the level script calls setExpFog / the vision set file sets it at run time.
		fog_source = "none: 1.1.1.13 maps carry no fog in GfxWorld / ComWorld; the game sets it from the level script ( setExpFog ) at run time";
		has_fog = false;

		stat.world_surface_count = surface_count;
		stat.distinct_models = static_cast<int>(models.size());
		stat.light_count = static_cast<int>(lights.size());
		stat.lightmap_count = static_cast<int>(lightmaps.size());
		stat.reflection_probe_count = static_cast<int>(probes.size());
		stat.materials = static_cast<int>(materials.size());
		stat.vertex_shaders = static_cast<int>(vertex_shaders.size());
		stat.pixel_shaders = static_cast<int>(pixel_shaders.size());
		stat.textures = static_cast<int>(textures.size());
		stat.missing_textures = static_cast<int>(texture_failures.size());
		stat.zone_bytes_peak = ctx.peak_bytes;
		stat.load_ms = ms_since(started);
		map_loaded = true;
		return true;		// the zones are released here: ctx goes out of scope
	}
}
