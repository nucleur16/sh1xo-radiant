// One frame. Port of GameMapRenderer.cs: RenderFrame, CullSurfaces, CollectModels, DrawWorldSurfaces, DrawModelSurfaces,
// DrawPasses, ApplyStateBits, BindArguments, CodeSampler, BindCustomSamplers, BindSampler, SetPrimaryLight.

#include "renderer_impl.hpp"

#include <algorithm>
#include <chrono>

namespace codol::view
{
	std::array<float4, 5> build_frustum(const camera& cam, const float aspect)
	{
		// GameMapRenderer.cs:1363-1377
		const mat4 m = render::mul(cam.view_rotation(), cam.projection(aspect));
		const auto column = [&](const int c) { return float4{ m.m[0][c], m.m[1][c], m.m[2][c], m.m[3][c] }; };
		const float4 c1 = column(0), c2 = column(1), c3 = column(2), c4 = column(3);
		const auto add = [](const float4& a, const float4& b) { return float4{ a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w }; };
		const auto sub = [](const float4& a, const float4& b) { return float4{ a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w }; };
		std::array<float4, 5> planes = { add(c4, c1), sub(c4, c1), add(c4, c2), sub(c4, c2), c3 };
		for (float4& plane : planes)
		{
			const float len = std::sqrt(plane.x * plane.x + plane.y * plane.y + plane.z * plane.z);
			const float divisor = len > 0 ? len : 1.0f;
			plane = { plane.x / divisor, plane.y / divisor, plane.z / divisor, plane.w / divisor };
		}
		return planes;
	}

	bool sphere_visible(const std::span<const float4> planes, const float3& center, const float radius)
	{
		// GameMapRenderer.cs:1379-1389
		for (const float4& plane : planes)
		{
			if (plane.x * center.x + plane.y * center.y + plane.z * center.z + plane.w < -radius)
			{
				return false;
			}
		}
		return true;
	}

	namespace
	{
		// render states the game's state bits never set: the host may have left them in any state
		constexpr render::render_state_value BASELINE_STATES[] =
		{
			{ D3DRS_SCISSORTESTENABLE, FALSE },
			{ D3DRS_FOGENABLE, FALSE },
			{ D3DRS_CLIPPLANEENABLE, 0 },
			{ D3DRS_CLIPPING, TRUE },
			{ D3DRS_SHADEMODE, D3DSHADE_GOURAUD },
			{ D3DRS_STENCILREF, 0 },
			{ D3DRS_STENCILMASK, 0xFFFFFFFF },
			{ D3DRS_STENCILWRITEMASK, 0xFFFFFFFF },
			{ D3DRS_MULTISAMPLEANTIALIAS, TRUE },
			{ D3DRS_MULTISAMPLEMASK, 0xFFFFFFFF },
			{ D3DRS_ANTIALIASEDLINEENABLE, FALSE },
			{ D3DRS_POINTSPRITEENABLE, FALSE },
			{ D3DRS_POINTSCALEENABLE, FALSE },
			{ D3DRS_LASTPIXEL, TRUE },
			{ D3DRS_DITHERENABLE, FALSE },
			{ D3DRS_SPECULARENABLE, FALSE },
			{ D3DRS_COLORWRITEENABLE1, 0xF },
			{ D3DRS_COLORWRITEENABLE2, 0xF },
			{ D3DRS_COLORWRITEENABLE3, 0xF },
			{ D3DRS_BLENDFACTOR, 0xFFFFFFFF },
			{ D3DRS_INDEXEDVERTEXBLENDENABLE, FALSE },
			{ D3DRS_VERTEXBLEND, D3DVBF_DISABLE },
			{ D3DRS_ENABLEADAPTIVETESSELLATION, FALSE },
			{ D3DRS_FILLMODE, D3DFILL_SOLID },
			{ D3DRS_ZENABLE, D3DZB_TRUE },
			{ D3DRS_ZWRITEENABLE, TRUE },
			{ D3DRS_ZFUNC, D3DCMP_LESSEQUAL },
			{ D3DRS_ALPHABLENDENABLE, FALSE },
			{ D3DRS_ALPHATESTENABLE, FALSE },
			{ D3DRS_STENCILENABLE, FALSE },
			{ D3DRS_CULLMODE, D3DCULL_CCW },
			{ D3DRS_COLORWRITEENABLE, 0xF },
			{ D3DRS_SRGBWRITEENABLE, FALSE },
			{ D3DRS_DEPTHBIAS, 0 },
			{ D3DRS_SLOPESCALEDEPTHBIAS, 0 },
		};

		double ms_since(const std::chrono::steady_clock::time_point from)
		{
			return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - from).count();
		}
	}

	// ---------------------------------------------------------------------------------------------------

	void map_renderer::impl::reset_frame_caches()
	{
		// the caches of GameMapRenderer assume that nobody else touches the device; here the host does between
		// frames, so every frame starts from "nothing is known"
		bound_material = nullptr;
		bound_vertex_shader = nullptr;
		bound_pixel_shader = nullptr;
		bound_vertex.fill({});
		bound_pixel.fill({});
		applied_samplers.fill({});
		applied_state_bits = nullptr;
		current_light_def = nullptr;
		constants_light = -2;
		constants_instance = nullptr;
	}

	IDirect3DTexture9* map_renderer::impl::render(const camera& cam, const int width, const int height, const frame_options& options)
	{
		if (!map_loaded || device_lost || width <= 0 || height <= 0)
		{
			return nullptr;
		}
		const auto started = std::chrono::steady_clock::now();
		release_idle_zone();
		if (!ensure_targets(width, height, options.target == nullptr, options.depth_stencil == nullptr, options.float_z))
		{
			return nullptr;
		}
		IDirect3DSurface9* color = options.target ? options.target : target_surface.get();
		IDirect3DSurface9* depth = options.depth_stencil ? options.depth_stencil : depth_surface.get();

		// ---- save the host's state: everything a state block holds, and the targets it does not
		if (!saved_state && FAILED(device->CreateStateBlock(D3DSBT_ALL, saved_state.put())))
		{
			last_error = "IDirect3DDevice9::CreateStateBlock failed";
			return nullptr;
		}
		saved_state->Capture();
		com_ptr<IDirect3DSurface9> host_target, host_depth;
		device->GetRenderTarget(0, host_target.put());
		device->GetDepthStencilSurface(host_depth.put());
		// MEASURED with codol_viewtool statetest: IDirect3DStateBlock9::Apply restores the buffer and stride of a
		// stream but not its offset, so the two streams the renderer uses are saved by hand
		com_ptr<IDirect3DVertexBuffer9> host_streams[2];
		UINT host_stream_offsets[2] = {}, host_stream_strides[2] = {};
		bool host_stream_known[2] = {};
		for (UINT i = 0; i < 2; i++)
		{
			host_stream_known[i] = SUCCEEDED(device->GetStreamSource(i, host_streams[i].put(), &host_stream_offsets[i], &host_stream_strides[i]));
		}

		bool end_scene = false;
		if (options.scene != scene_mode::inside_scene)
		{
			// automatic: BeginScene fails with D3DERR_INVALIDCALL inside a scene, which is then the host's
			end_scene = SUCCEEDED(device->BeginScene());
			if (!end_scene && options.scene == scene_mode::begin_end)
			{
				last_error = "IDirect3DDevice9::BeginScene failed";
			}
		}

		frame = &options;
		frame_stat = {};
		reset_frame_caches();
		for (int s = 0; s < 16; s++)
		{
			device->SetSamplerState(static_cast<DWORD>(s), D3DSAMP_MIPMAPLODBIAS, 0);
			device->SetSamplerState(static_cast<DWORD>(s), D3DSAMP_MAXMIPLEVEL, 0);
			device->SetSamplerState(static_cast<DWORD>(s), D3DSAMP_BORDERCOLOR, 0);
		}
		for (const auto& state : BASELINE_STATES)
		{
			device->SetRenderState(static_cast<D3DRENDERSTATETYPE>(state.state), state.value);
		}
		device->SetStreamSourceFreq(0, 1);
		device->SetStreamSourceFreq(1, 1);
		for (DWORD target = 1; target < caps.NumSimultaneousRTs; target++)
		{
			device->SetRenderTarget(target, nullptr);
		}

		render_frame(cam, width, height, color, depth);
		if (options.overlay)
		{
			run_overlay(cam, width, height);
		}

		if (end_scene)
		{
			device->EndScene();
		}

		// ---- restore. The targets first: SetRenderTarget resets the viewport, which the state block then restores.
		if (host_target)
		{
			device->SetRenderTarget(0, host_target.get());
		}
		device->SetDepthStencilSurface(host_depth.get());
		saved_state->Apply();
		for (UINT i = 0; i < 2; i++)
		{
			if (host_stream_known[i])
			{
				device->SetStreamSource(i, host_streams[i].get(), host_stream_offsets[i], host_stream_strides[i]);
			}
		}
		frame = nullptr;

		// keep the load numbers, replace the frame numbers
		statistics merged = frame_stat;
		merged.world_surface_count = stat.world_surface_count;
		merged.static_model_count = stat.static_model_count;
		merged.entity_model_count = stat.entity_model_count;
		merged.brush_model_count = stat.brush_model_count;
		merged.distinct_models = stat.distinct_models;
		merged.streamed_models = stat.streamed_models;
		merged.foreign_models = stat.foreign_models;
		merged.cell_count = stat.cell_count;
		merged.light_count = stat.light_count;
		merged.lightmap_count = stat.lightmap_count;
		merged.reflection_probe_count = stat.reflection_probe_count;
		merged.materials = stat.materials;
		merged.vertex_shaders = static_cast<int>(vertex_shaders.size());
		merged.pixel_shaders = static_cast<int>(pixel_shaders.size());
		merged.textures = static_cast<int>(textures.size());
		merged.missing_textures = static_cast<int>(texture_failures.size());
		merged.unbound_constants = static_cast<int>(unbound_constants.size());
		merged.light_grid_traces = stat.light_grid_traces;
		merged.light_grid_blocked = stat.light_grid_blocked;
		merged.texture_bytes = stat.texture_bytes;
		merged.geometry_bytes = stat.geometry_bytes;
		merged.zone_bytes_peak = stat.zone_bytes_peak;
		merged.load_ms = stat.load_ms;
		copy_edit_stats(merged);
		merged.cpu_ms = ms_since(started);
		stat = merged;
		return options.target ? nullptr : target_texture.get();
	}

	void map_renderer::impl::render_frame(const camera& cam, const int width, const int height, IDirect3DSurface9* color, IDirect3DSurface9* depth)
	{
		// GameMapRenderer.cs:742-836 ( RenderFrame ) without the float z pre-pass, effects and post effects
		const float aspect = static_cast<float>(width) / static_cast<float>(height);
		constants->fog = frame->fog && has_fog ? fog : render::game_fog{};
		constants->begin_frame(cam, width, height, frame->game_time);
		const mat4 world_matrix = mat4::translation(-cam.origin);
		const auto frustum = build_frustum(cam, aspect);
		const bool cells = frame->cell_culling && visibility;
		if (cells)
		{
			visibility->update(cam.origin, frustum);
			frame_stat.camera_cell = visibility->camera_cell();
			frame_stat.reached_cells = visibility->reached_cells();
			frame_stat.cell_visits = visibility->visible_cells();
		}
		opaque_models.clear();
		transparent_models.clear();
		opaque_brushes.clear();
		transparent_brushes.clear();
		opaque_meshes.clear();
		transparent_meshes.clear();
		fixed_meshes.clear();

		// GameMapRenderer.cs:1138-1143 ( ApplyInitialStates )
		for (const auto& state : render::INITIAL_RENDER_STATES)
		{
			device->SetRenderState(static_cast<D3DRENDERSTATETYPE>(state.state), state.value);
		}
		update_sun_shadow_state();
		cull_surfaces(cam, frustum);
		if (frame->draw_static_models || frame->draw_entity_models || (frame->draw_placed_models && !placed_models.empty()))
		{
			collect_models(cam, frustum);
		}
		if (frame->draw_brush_models)
		{
			collect_brush_models(cam, frustum);
		}
		if (frame->draw_meshes && !meshes.empty())
		{
			collect_meshes(cam, frustum);
		}
		draw_sun_shadows(cam, aspect);
		if (current_sun_shadow)
		{
			// GameMapRenderer.cs:789-798
			constants->begin_frame(cam, width, height, frame->game_time);
			render::sun_shadow_info info;
			info.lookup = current_sun_shadow->lookup;
			info.switch_partition = current_sun_shadow->switch_partition;
			info.scale = current_sun_shadow->scale;
			info.pixel_adjust = current_sun_shadow->pixel_adjust;
			constants->set_sun_shadow(info);
		}
		constants_light = -2;
		constants_instance = nullptr;
		device->SetDepthStencilSurface(depth);
		const D3DVIEWPORT9 viewport{ 0, 0, static_cast<DWORD>(width), static_cast<DWORD>(height), 0.0f, 1.0f };
		if (frame->float_z && float_z_surface && float_z_width == width && float_z_height == height)
		{
			device->SetRenderTarget(0, float_z_surface.get());
			device->SetViewport(&viewport);
			draw_float_z(cam, world_matrix);
		}
		device->SetRenderTarget(0, color);
		device->SetViewport(&viewport);
		if (frame->clear)
		{
			// the state block restore does not matter here: Clear ignores everything but the viewport and scissor
			device->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL, frame->clear_color, 1.0f, 0);
		}
		applied_state_bits = nullptr;

		// GameMapRenderer.cs:808-819: lit opaque world, opaque models, emissive world, lit transparent world,
		// transparent models. Brush models go with the models of their kind.
		if (frame->draw_world)
		{
			draw_world_surfaces(ranges[0].begin, ranges[0].end, ranges[0].emissive);
		}
		draw_model_surfaces(cam.origin, opaque_models);
		draw_brush_surfaces(cam.origin, opaque_brushes);
		if (!opaque_meshes.empty())
		{
			draw_meshes(cam.origin, opaque_meshes);
		}
		if (!fixed_meshes.empty())
		{
			draw_fixed_meshes(cam, width, height);
		}
		constants->set_world(world_matrix);
		if (frame->draw_world)
		{
			draw_world_surfaces(ranges[1].begin, ranges[1].end, ranges[1].emissive);
			draw_world_surfaces(ranges[2].begin, ranges[2].end, ranges[2].emissive);
		}
		draw_model_surfaces(cam.origin, transparent_models);
		draw_brush_surfaces(cam.origin, transparent_brushes);
		if (!transparent_meshes.empty())
		{
			draw_meshes(cam.origin, transparent_meshes);
		}
		constants->set_world(world_matrix);
	}

	void map_renderer::impl::draw_float_z(const camera& cam, const mat4& world_matrix)
	{
		// GameMapRenderer.cs:838-849 ( DrawFloatZ ): the opaque world and models with the "build floatz" technique
		const statistics before = frame_stat;
		device->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL, 0, 1.0f, 0);
		constants->set("depthFromClip", { 0, 0, 0, 1 });
		constants->set_world(world_matrix);
		if (frame->draw_world)
		{
			draw_world_surfaces(ranges[0].begin, ranges[0].end, false, render::renderer_technique::BUILD_FLOAT_Z);
		}
		draw_model_surfaces(cam.origin, opaque_models, render::renderer_technique::BUILD_FLOAT_Z);
		if (!opaque_meshes.empty())
		{
			draw_meshes(cam.origin, opaque_meshes, render::renderer_technique::BUILD_FLOAT_Z);
		}
		constants->set_world(world_matrix);
		const int draws = frame_stat.draw_calls - before.draw_calls;
		const int shadow_draws = frame_stat.sun_shadow_draws;
		frame_stat = before;
		frame_stat.sun_shadow_draws = shadow_draws;
		frame_stat.float_z_draws = draws;
	}

	void map_renderer::impl::cull_surfaces(const camera& cam, const std::span<const float4> frustum)
	{
		// GameMapRenderer.cs:949-962
		surface_visible.resize(surfaces.size());
		const std::vector<std::uint8_t>* cells = frame->cell_culling && visibility ? &visibility->surface_visible() : nullptr;
		for (std::size_t i = 0; i < surfaces.size(); i++)
		{
			const world_surface& s = surfaces[i];
			surface_visible[i] = !s.brush_model && !s.hidden && (!cells || i >= cells->size() || (*cells)[i])
				&& (s.radius <= 0.0f || sphere_visible(frustum, s.center - cam.origin, s.radius));
		}
	}

	const model_lod_draw& map_renderer::impl::choose_lod(const static_model_instance& instance, const camera& cam) const
	{
		// Not in the C# renderer ( it draws LOD 0 ). R_GetLodForDist: the first LOD whose distance the model has not
		// passed; a model beyond its last LOD keeps that one here ( the game culls it ).
		const auto& lods = instance.draw->lods;
		if (!frame->model_lods || lods.size() < 2)
		{
			return lods[0];
		}
		const float3 origin{ instance.placement.m[3][0], instance.placement.m[3][1], instance.placement.m[3][2] };
		const float scale = instance.scale > 0 ? instance.scale : 1.0f;
		const float distance = length(origin - cam.origin) * (frame->lod_scale > 0 ? frame->lod_scale : 1.0f) / scale;
		for (std::size_t i = 0; i < lods.size(); i++)
		{
			if (!lods[i].surfaces.empty() && (lods[i].dist <= 0 || distance < lods[i].dist))
			{
				return lods[i];
			}
		}
		for (std::size_t i = lods.size(); i-- > 0;)
		{
			if (!lods[i].surfaces.empty())
			{
				return lods[i];
			}
		}
		return lods[0];
	}

	void map_renderer::impl::collect_models(const camera& cam, const std::span<const float4> frustum)
	{
		// GameMapRenderer.cs:1173-1215
		const std::vector<std::uint8_t>* cells = frame->cell_culling && visibility ? &visibility->model_visible() : nullptr;
		for (static_model_instance& instance : static_models)
		{
			collect_model(instance, cam, frustum, cells);
		}
		// models the editor placed: after the map's, culled by the frustum only
		for (const auto& placed : placed_models)
		{
			collect_model(*placed.second, cam, frustum, nullptr);
		}
	}

	void map_renderer::impl::collect_model(static_model_instance& instance, const camera& cam, const std::span<const float4> frustum,
		const std::vector<std::uint8_t>* cells)
	{
		instance.drawn = false;
		if (instance.handle > 0 ? !frame->draw_placed_models : instance.is_entity ? !frame->draw_entity_models : !frame->draw_static_models)
		{
			return;
		}
		if (instance.hidden)
		{
			return;
		}
		// a model the editor moved is no longer where the cells list it
		if (cells && !instance.moved && instance.static_index >= 0 && instance.static_index < static_cast<int>(cells->size())
			&& !(*cells)[static_cast<std::size_t>(instance.static_index)])
		{
			frame_stat.culled_models++;
			return;
		}
		if (!sphere_visible(frustum, instance.center - cam.origin, instance.radius)
			|| (instance.cull_distance > 0 && length(instance.center - cam.origin) - instance.radius > instance.cull_distance))
		{
			frame_stat.culled_models++;
			return;
		}
		if (!instance.draw)
		{
			return;
		}
		frame_stat.model_instances++;
		if (instance.handle > 0)
		{
			frame_stat.placed_models++;
		}
		instance.drawn = true;
		const model_lod_draw& lod = choose_lod(instance, cam);
		const bool first_lod = &lod == &instance.draw->lods[0];
		for (std::size_t index = 0; index < lod.surfaces.size(); index++)
		{
			const model_surface_draw& surface = lod.surfaces[index];
			if (first_lod && !instance.hidden_surfaces.empty() && instance.hidden_surfaces[index])
			{
				continue;
			}
			material_draw* material = surface.material;
			if (!material || material->not_drawn())
			{
				continue;
			}
			const int technique_type = choose_technique(*material, instance.primary_light, false);
			if (!material->technique(technique_type))
			{
				frame_stat.skipped_no_technique++;
				continue;
			}
			(blends(*material, technique_type) ? transparent_models : opaque_models).push_back({ &instance, &lod, &surface, material, technique_type });
		}
	}

	void map_renderer::impl::collect_brush_models(const camera& cam, const std::span<const float4> frustum)
	{
		// Not in the C# renderer. A brush model is a run of GfxWorld surfaces outside the static draw ranges; its
		// surfaces are drawn like world surfaces ( lightmap, primary light ) under the entity's placement.
		for (brush_model_instance& instance : brush_models)
		{
			instance.drawn = false;
			if (instance.hidden || (instance.special_ops && !frame->draw_special_ops_brush_models) || !sphere_visible(frustum, instance.center - cam.origin, instance.radius))
			{
				continue;
			}
			instance.drawn = true;
			frame_stat.brush_models++;
			for (int i = 0; i < instance.surface_count; i++)
			{
				world_surface& surface = surfaces[static_cast<std::size_t>(instance.first_surface + i)];
				material_draw* material = surface.material;
				if (!material)
				{
					frame_stat.skipped_no_technique++;
					continue;
				}
				if (material->not_drawn())
				{
					frame_stat.skipped_hidden++;
					continue;
				}
				const int technique_type = choose_technique(*material, surface.primary_light_index, false);
				if (!material->technique(technique_type))
				{
					frame_stat.skipped_no_technique++;
					continue;
				}
				(blends(*material, technique_type) ? transparent_brushes : opaque_brushes).push_back({ &instance, &surface, material, technique_type });
			}
		}
	}

	// ---------------------------------------------------------------------------------------------------
	// world

	bool map_renderer::impl::resolve_surface(const world_surface& surface, const bool emissive, const int forced_technique, material_draw*& material, int& technique_type)
	{
		// GameMapRenderer.cs:912-945
		material = surface.material;
		if (!material)
		{
			frame_stat.skipped_no_technique++;
			return false;
		}
		if (material->not_drawn())
		{
			frame_stat.skipped_hidden++;
			return false;
		}
		technique_type = choose_technique(*material, surface.primary_light_index, emissive);
		if (forced_technique >= 0)
		{
			if (blends(*material, technique_type))
			{
				return false;
			}
			technique_type = forced_technique;
		}
		if (!material->technique(technique_type))
		{
			frame_stat.skipped_no_technique++;
			return false;
		}
		return true;
	}

	void map_renderer::impl::draw_world_surfaces(const int begin, const int end, const bool emissive, const int forced_technique)
	{
		// GameMapRenderer.cs:858-906
		int i = begin;
		while (i < end)
		{
			if (!surface_visible[static_cast<std::size_t>(i)])
			{
				frame_stat.culled_surfaces++;
				i++;
				continue;
			}
			const world_surface& first = surfaces[static_cast<std::size_t>(i)];
			material_draw* material = nullptr;
			int technique_type = 0;
			if (!resolve_surface(first, emissive, forced_technique, material, technique_type))
			{
				i++;
				continue;
			}
			const int format = render::vertex_format::WORLD + material->set().world_vert_format;
			if (!world_indices || render::stream1_stride(format) > 0)
			{
				draw_resolved_surface(first, *material, technique_type, format);
				i++;
				continue;
			}
			int j = i + 1;
			int triangles = first.triangle_count;
			int min_vertex = first.first_vertex, max_vertex = first.first_vertex + first.vertex_count;
			while (j < end && surface_visible[static_cast<std::size_t>(j)])
			{
				// GameMapRenderer.cs:908-910 ( SameBatch )
				const world_surface& next = surfaces[static_cast<std::size_t>(j)];
				if (next.material != first.material || next.lightmap_index != first.lightmap_index || next.reflection_probe_index != first.reflection_probe_index
					|| next.primary_light_index != first.primary_light_index || next.emissive != first.emissive)
				{
					break;
				}
				triangles += next.triangle_count;
				min_vertex = std::min(min_vertex, next.first_vertex);
				max_vertex = std::max(max_vertex, next.first_vertex + next.vertex_count);
				j++;
			}
			frame_stat.surfaces += j - i;
			set_primary_light(first.primary_light_index);
			const lightmap_pair* lightmap = first.lightmap_index < static_cast<int>(lightmaps.size()) ? &lightmaps[static_cast<std::size_t>(first.lightmap_index)] : nullptr;
			surface_draw draw;
			draw.vertex_buffer = vertex_buffer.get();
			draw.vertex_stride = sizeof(c1::GfxWorldVertex);
			draw.index_buffer = world_indices.get();
			draw.min_vertex = min_vertex;
			draw.vertex_count = max_vertex - min_vertex;
			draw.start_index = first.absolute_index;
			draw.triangle_count = triangles;
			draw_passes(*material, technique_type, format, first.reflection_probe_index, lightmap, draw);
			i = j;
		}
	}

	void map_renderer::impl::draw_resolved_surface(const world_surface& surface, material_draw& material, const int technique_type, const int format)
	{
		// GameMapRenderer.cs:1153-1166
		frame_stat.surfaces++;
		set_primary_light(surface.primary_light_index);
		const lightmap_pair* lightmap = surface.lightmap_index < static_cast<int>(lightmaps.size()) ? &lightmaps[static_cast<std::size_t>(surface.lightmap_index)] : nullptr;
		const int layer_stride = render::stream1_stride(format);
		surface_draw draw;
		draw.vertex_buffer = vertex_buffer.get();
		draw.vertex_stride = sizeof(c1::GfxWorldVertex);
		draw.index_buffer = index_buffer.get();
		draw.base_vertex = surface.first_vertex;
		draw.vertex_count = surface.vertex_count;
		draw.start_index = surface.base_index;
		draw.triangle_count = surface.triangle_count;
		if (layer_stride > 0 && layer_buffer)
		{
			draw.layer_buffer = layer_buffer.get();
			draw.layer_offset = static_cast<int>(surface.layer_data_offset);
			draw.layer_stride = layer_stride;
		}
		draw_passes(material, technique_type, format, surface.reflection_probe_index, lightmap, draw);
	}

	void map_renderer::impl::draw_brush_surfaces(const float3& eye, const std::vector<brush_surface_instance>& draws)
	{
		constants_instance = nullptr;
		for (const brush_surface_instance& draw : draws)
		{
			mat4 world = draw.instance->placement;
			world.m[3][0] -= eye.x;
			world.m[3][1] -= eye.y;
			world.m[3][2] -= eye.z;
			constants->set_world(world);
			frame_stat.brush_model_surfaces++;
			draw_resolved_surface(*draw.surface, *draw.material, draw.technique, render::vertex_format::WORLD + draw.material->set().world_vert_format);
			frame_stat.surfaces--;		// counted as a brush model surface
		}
	}

	// ---------------------------------------------------------------------------------------------------
	// models

	void map_renderer::impl::draw_model_surfaces(const float3& eye, const std::vector<model_surface_instance>& draws, const int forced_technique)
	{
		// GameMapRenderer.cs:1244-1270
		constants_instance = nullptr;
		for (std::size_t i = 0; i < draws.size();)
		{
			const model_surface_instance& draw = draws[i];
			const int technique = forced_technique >= 0 ? forced_technique : draw.technique;
			if (forced_technique >= 0 && !draw.material->technique(forced_technique))
			{
				i++;
				continue;
			}
			int triangles = draw.surface->triangle_count;
			int span = draw.surface->vertex_count;
			std::size_t j = i + 1;
			while (j < draws.size() && draws[j].instance == draw.instance && draws[j].material == draw.material && draws[j].technique == draw.technique
				&& draws[j].lod == draw.lod && draws[j].surface->base_index == draw.surface->base_index + 3 * triangles)
			{
				triangles += draws[j].surface->triangle_count;
				span += draws[j].surface->vertex_count;
				j++;
			}
			draw_model_surface(eye, *draw.instance, *draw.lod, *draw.surface, *draw.material, technique, triangles, span);
			i = j;
		}
	}

	void map_renderer::impl::draw_model_surface(const float3& eye, const static_model_instance& instance, const model_lod_draw& lod, const model_surface_draw& surface,
		material_draw& material, const int technique_type, const int triangles, const int vertex_span)
	{
		// GameMapRenderer.cs:1217-1240
		if (constants_instance != &instance)
		{
			// the same values again for the next surface of an instance would only cost time
			constants_instance = &instance;
			mat4 world = instance.placement;
			world.m[3][0] -= eye.x;
			world.m[3][1] -= eye.y;
			world.m[3][2] -= eye.z;
			constants->set_world(world);
			set_primary_light(instance.primary_light);
			if (lighting_volume && instance.lighting_handle > 0)
			{
				constants->set("baseLightingCoords", lighting_volume->base_coords(instance.lighting_handle));
			}
			const std::uint32_t ground = instance.ground_lighting;
			const auto channel = [&](const int shift) { return static_cast<float>(std::pow(static_cast<double>((ground >> shift) & 0xFF) * 0.0078431377, 2.0)); };
			constants->set("lightprobeAmbient", { channel(0), channel(8), channel(16), static_cast<float>(static_cast<double>(ground >> 24) * 0.00098039221) });
		}
		frame_stat.model_surfaces++;
		surface_draw draw;
		draw.vertex_buffer = lod.vertex_buffer.get();
		draw.vertex_stride = 32;
		draw.index_buffer = lod.index_buffer.get();
		draw.min_vertex = surface.first_vertex;
		draw.vertex_count = vertex_span;
		draw.start_index = surface.base_index;
		draw.triangle_count = triangles;
		draw_passes(material, technique_type, render::vertex_format::PACKED, instance.reflection_probe, nullptr, draw);
	}

	// ---------------------------------------------------------------------------------------------------
	// passes

	void map_renderer::impl::draw_passes(material_draw& material, const int technique_type, const int format, const int reflection_probe,
		const lightmap_pair* lightmap, const surface_draw& draw)
	{
		// GameMapRenderer.cs:1294-1341 ( DrawPasses ), :1282-1292 ( Submit )
		const render::render_technique* technique = material.technique(technique_type);
		if (!draw.vertex_buffer || !draw.index_buffer)
		{
			return;
		}
		for (std::size_t p = 0; p < technique->passes.size(); p++)
		{
			const render::render_pass& pass = technique->passes[p];
			pass_plan* plan = plan_of(material, technique_type, static_cast<int>(p));
			if (!plan->complete)
			{
				frame_stat.skipped_incomplete_pass++;
				warn(technique->name + " pass " + std::to_string(p) + ": vs " + plan->vertex_shader_name + " / ps " + plan->pixel_shader_name + " / declaration "
					+ (plan->declaration ? plan->declaration->name : std::string("-")) + " not loaded");
				continue;
			}
			IDirect3DVertexDeclaration9* declaration = declaration_of(*plan, format);
			if (!declaration)
			{
				frame_stat.skipped_incomplete_pass++;
				warn(technique->name + ": declaration " + plan->declaration->name + " has no vertex format " + std::to_string(format));
				continue;
			}
			apply_state_bits(material, technique_type, static_cast<int>(p));
			if (frame && frame->wireframe)
			{
				device->SetRenderState(D3DRS_FILLMODE, D3DFILL_WIREFRAME);
				applied_state_bits = nullptr;
			}
			if (draw.two_sided)
			{
				device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
				applied_state_bits = nullptr;
			}
			device->SetVertexDeclaration(declaration);
			if (plan->vertex_shader != bound_vertex_shader || plan->pixel_shader != bound_pixel_shader)
			{
				frame_stat.shader_changes++;
				bound_vertex_shader = plan->vertex_shader;
				bound_pixel_shader = plan->pixel_shader;
				device->SetVertexShader(plan->vertex_shader);
				device->SetPixelShader(plan->pixel_shader);
			}
			if (&material != bound_material)
			{
				frame_stat.material_changes++;
				bound_material = &material;
			}
			bind_arguments(pass, *plan, material);
			bind_custom_samplers(pass, reflection_probe, lightmap);

			device->SetStreamSource(0, draw.vertex_buffer, 0, static_cast<UINT>(draw.vertex_stride));
			if (draw.layer_stride > 0)
			{
				device->SetStreamSource(1, draw.layer_buffer, static_cast<UINT>(draw.layer_offset), static_cast<UINT>(draw.layer_stride));
			}
			device->SetIndices(draw.index_buffer);
			device->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, draw.base_vertex, static_cast<UINT>(draw.min_vertex), static_cast<UINT>(draw.vertex_count),
				static_cast<UINT>(draw.start_index), static_cast<UINT>(draw.triangle_count));
			frame_stat.triangles += draw.triangle_count;
			frame_stat.draw_calls++;
		}
	}

	void map_renderer::impl::set_primary_light(const int index)
	{
		// GameMapRenderer.cs:1431-1452
		if (index == constants_light)
		{
			return;
		}
		constants_light = index;
		const map_light* light = primary_light(index);
		current_light_def = nullptr;
		if (light && light->type != 1 && !light->def_name.empty())
		{
			const auto it = light_defs.find(light->def_name);
			if (it == light_defs.end())
			{
				warn("light definition " + light->def_name + " is not in the map zone");
			}
			else
			{
				current_light_def = &it->second;
			}
		}
		if (!light)
		{
			constants->set_primary_light(nullptr);
			return;
		}
		render::primary_light value;
		value.type = light->type;
		value.color = light->color;
		value.direction = light->direction;
		value.origin = light->origin;
		value.radius = light->radius;
		value.cos_half_fov_outer = light->cos_half_fov_outer;
		value.cos_half_fov_inner = light->cos_half_fov_inner;
		value.exponent = light->exponent;
		render::light_def_info info;
		if (current_light_def)
		{
			info.attenuation_width = current_light_def->attenuation_width;
			info.lookup_start = current_light_def->lookup_start;
		}
		constants->set_primary_light(&value, current_light_def ? &info : nullptr, nullptr);
		if (light->type == 1 && current_sun_shadow)
		{
			constants->set_shadow_lookup(current_sun_shadow->lookup);
		}
	}

	void map_renderer::impl::apply_state_bits(const material_draw& material, const int technique_type, const int pass)
	{
		// GameMapRenderer.cs:1462-1490
		const auto& state = material.asset().state;
		const render::material_state::bits* bits = state ? state->pass_bits(technique_type, pass) : nullptr;
		if (!bits)
		{
			return;
		}
		auto it = state_bits.find({ bits->bits0, bits->bits1 });
		if (it == state_bits.end())
		{
			it = state_bits.emplace(std::make_pair(bits->bits0, bits->bits1), render::translate_state_bits(bits->bits0, bits->bits1, opts.settings,
				(caps.PrimitiveMiscCaps & D3DPMISCCAPS_SEPARATEALPHABLEND) != 0, (caps.RasterCaps & D3DPRASTERCAPS_SLOPESCALEDEPTHBIAS) != 0)).first;
		}
		if (&it->second == applied_state_bits)
		{
			return;
		}
		for (const auto& s : it->second)
		{
			device->SetRenderState(static_cast<D3DRENDERSTATETYPE>(s.state), s.value);
		}
		applied_state_bits = &it->second;
	}

	bool map_renderer::impl::already_bound(const bool vertex, const int reg, const int rows, const std::int64_t key, const int version) const
	{
		// GameMapRenderer.cs:1625-1641
		const int limit = vertex ? static_cast<int>(bound_vertex.size()) : static_cast<int>(bound_pixel.size());
		if (reg < 0 || reg + rows > limit)
		{
			return false;
		}
		for (int r = 0; r < rows; r++)
		{
			const bound_register& b = vertex ? bound_vertex[static_cast<std::size_t>(reg + r)] : bound_pixel[static_cast<std::size_t>(reg + r)];
			if (b.key != key + r || b.version != version)
			{
				return false;
			}
		}
		return true;
	}

	void map_renderer::impl::set_constants(const render::shader_argument& arg, const float* values, const int rows, const std::int64_t key, const int version)
	{
		// GameMapRenderer.cs:1600-1616
		const bool vertex = arg.is_vertex_stage();
		if (vertex)
		{
			device->SetVertexShaderConstantF(arg.dest, values, static_cast<UINT>(rows));
		}
		else
		{
			device->SetPixelShaderConstantF(arg.dest, values, static_cast<UINT>(rows));
		}
		const int limit = vertex ? static_cast<int>(bound_vertex.size()) : static_cast<int>(bound_pixel.size());
		for (int r = 0; r < rows && arg.dest + r < limit; r++)
		{
			bound_register& b = vertex ? bound_vertex[static_cast<std::size_t>(arg.dest + r)] : bound_pixel[static_cast<std::size_t>(arg.dest + r)];
			b = key == 0 ? bound_register{} : bound_register{ key + r, version };
		}
	}

	void map_renderer::impl::bind_arguments(const render::render_pass& pass, pass_plan& plan, material_draw& material)
	{
		// GameMapRenderer.cs:1510-1598
		using type = render::shader_argument_type;
		float buffer[16];
		for (std::size_t a = 0; a < pass.arguments.size(); a++)
		{
			const render::shader_argument& arg = pass.arguments[a];
			switch (arg.type)
			{
			case type::literal_vertex_const:
			case type::literal_pixel_const:
				set_constants(arg, arg.literal, 1);
				break;
			case type::material_vertex_const:
			case type::material_pixel_const:
			{
				// GameMapRenderer.cs:1646-1647 ( MaterialKey )
				const std::int64_t key = (2ll << 60) | (static_cast<std::int64_t>(material.id & 0xFFFFFF) << 36) | (static_cast<std::int64_t>(arg.name_hash()) << 4);
				if (already_bound(arg.is_vertex_stage(), arg.dest, 1, key, 0))
				{
					break;
				}
				const render::material_constant* constant = material.asset().find_constant(arg.name_hash());
				if (!constant)
				{
					warn(material.asset().name + ": no material constant " + render::hex32(arg.name_hash()));
				}
				const float zero[4] = {};
				set_constants(arg, constant ? constant->value : zero, 1, key, 0);
				break;
			}
			case type::code_vertex_const:
			case type::code_pixel_const:
			{
				const render::constant_handle& handle = plan.handles[a];
				const int version = constants->version_of(handle);
				// GameMapRenderer.cs:1643-1644 ( CodeKey )
				const std::int64_t key = (1ll << 60) | ((render::code_constants::key_of(handle) & 0xFFFFFFFFll) << 12) | (static_cast<std::int64_t>(arg.first_row()) << 4);
				if (already_bound(arg.is_vertex_stage(), arg.dest, render::code_constants::rows_of(handle, arg.first_row(), arg.row_count()), key, version))
				{
					break;
				}
				bool known = false;
				const int rows = constants->write(handle, arg.first_row(), arg.row_count(), buffer, known);
				if (!known)
				{
					const std::string name = plan.names[a].empty() ? "code" + std::to_string(arg.code_index()) : plan.names[a];
					if (unbound_constants.insert(name).second)
					{
						warn("code constant " + std::to_string(arg.code_index()) + " (" + name + ") is not computed (read by "
							+ (arg.is_vertex_stage() ? plan.vertex_shader_name : plan.pixel_shader_name) + ")");
					}
				}
				set_constants(arg, buffer, rows, key, version);
				break;
			}
			case type::material_pixel_sampler:
			{
				const auto& defs = material.asset().textures;
				const render::material_texture* def = nullptr;
				gpu_texture* texture = nullptr;
				for (std::size_t t = 0; t < defs.size(); t++)
				{
					if (defs[t].name_hash == arg.name_hash())
					{
						def = &defs[t];
						texture = material_texture(material, t);
						break;
					}
				}
				if (!def)
				{
					warn(material.asset().name + ": no texture for sampler " + render::hex32(arg.name_hash()));
				}
				bind_sampler(arg.dest, texture ? *texture : white, def ? def->sampler_state : render::DEFAULT_MATERIAL_SAMPLER_STATE);
				break;
			}
			case type::code_pixel_sampler:
			{
				// GameMapRenderer.cs:1689-1721 ( CodeSampler )
				const std::string& name = plan.names[a];
				const gpu_texture* texture = nullptr;
				std::uint8_t sampler_state = 0x02;
				if (name == "modelLightingSampler" && model_lighting.handle)
				{
					texture = &model_lighting;
					sampler_state = 0x02 | 0xE0;
				}
				else if (name == "attenuationSampler" && current_light_def && current_light_def->attenuation)
				{
					texture = current_light_def->attenuation;
					sampler_state = current_light_def->sampler_state;
				}
				else if (name == "shadowmapSamplerSun" && current_sun_shadow && shadow_map.handle)
				{
					texture = &shadow_map;
					sampler_state = 0x01 | 0x60;
				}
				else if (name == "floatZSampler" && float_z.handle && frame && frame->float_z)
				{
					texture = &float_z;
					sampler_state = 0x01 | 0x60;
				}
				else if (name == "outdoorMapSampler" && outdoor)
				{
					texture = outdoor;
					sampler_state = 0x02 | 0x60;
				}
				else
				{
					// the stand-ins of the game for its constant images; everything else is white as in the C#
					const int code = arg.code_index();
					texture = code == render::code_sampler_index::BLACK ? &black : code == render::code_sampler_index::IDENTITY_NORMAL_MAP ? &flat_normal : &white;
					if (code > render::code_sampler_index::IDENTITY_NORMAL_MAP)
					{
						warn("code sampler " + (name.empty() ? "codeSampler" + std::to_string(code) : name) + " is a white stand-in (read by " + plan.pixel_shader_name + ")");
					}
				}
				bind_sampler(arg.dest, *texture, sampler_state);
				break;
			}
			}
		}
	}

	void map_renderer::impl::bind_custom_samplers(const render::render_pass& pass, const int reflection_probe, const lightmap_pair* lightmap)
	{
		// GameMapRenderer.cs:1723-1738
		if (pass.custom_sampler_flags & 1)
		{
			const gpu_texture* probe = reflection_probe >= 0 && reflection_probe < static_cast<int>(probes.size()) ? probes[static_cast<std::size_t>(reflection_probe)].image : nullptr;
			bind_sampler(1, probe ? *probe : black, 0x12 | 0xE0);
		}
		if (pass.custom_sampler_flags & 2)
		{
			bind_sampler(2, lightmap && lightmap->primary ? *lightmap->primary : white, 0x02 | 0x60);
		}
		if (pass.custom_sampler_flags & 4)
		{
			bind_sampler(3, lightmap && lightmap->secondary ? *lightmap->secondary : black, 0x02 | 0x60);
		}
	}

	void map_renderer::impl::bind_sampler(const int sampler, const gpu_texture& texture, const std::uint8_t sampler_state)
	{
		// GameMapRenderer.cs:1740-1771. sRGB reads only happen in the post effects, which are not ported.
		frame_stat.texture_binds++;
		const bool cached = sampler >= 0 && sampler < 16;
		if (cached)
		{
			applied_sampler& applied = applied_samplers[static_cast<std::size_t>(sampler)];
			if (applied.valid && applied.texture == texture.handle.get() && applied.state == sampler_state)
			{
				return;
			}
			applied = { texture.handle.get(), sampler_state, true };
		}
		device->SetTexture(static_cast<DWORD>(sampler), texture.handle.get());
		auto it = sampler_states.find(sampler_state);
		if (it == sampler_states.end())
		{
			it = sampler_states.emplace(sampler_state, render::translate_sampler_state(sampler_state, false, opts.settings,
				std::max(static_cast<int>(caps.MaxAnisotropy), 1), (caps.TextureFilterCaps & D3DPTFILTERCAPS_MINFANISOTROPIC) != 0,
				(caps.TextureFilterCaps & D3DPTFILTERCAPS_MAGFANISOTROPIC) != 0)).first;
		}
		for (const auto& pair : it->second.states())
		{
			device->SetSamplerState(static_cast<DWORD>(sampler), static_cast<D3DSAMPLERSTATETYPE>(pair.state), pair.value);
		}
	}
}
