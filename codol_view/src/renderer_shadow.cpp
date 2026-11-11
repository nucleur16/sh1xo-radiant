// The dynamic sun shadow map. Port of Codol.Rendering\GameSunShadow.cs and GameMapRenderer.SunShadow.cs
// ( plus DrawCasterSurface / DrawCasterModel of GameMapRenderer.SpotShadow.cs:240-281 ).

#include "renderer_impl.hpp"

#include <algorithm>

namespace codol::view
{
	sun_shadow compute_sun_shadow(const camera& cam, const float aspect, const float3& sun_direction, const float3& world_mid, const float3& world_half,
		const render::renderer_settings& settings)
	{
		// GameSunShadow.cs:24-152 ( Compute )
		const int size = settings.sun_shadow_map_size;
		const int partitions = 2;
		const float sample_scale = settings.sun_shadow_sample_scale;
		const int draw_size = 2 * static_cast<int>(std::ceil(static_cast<float>(size) * sample_scale * 0.5f));
		const int margin = static_cast<int>(static_cast<std::uint32_t>(size - draw_size) >> 1);
		const float sample_size_near = settings.sun_shadow_sample_size_near;

		const float3 toward = -sun_direction;
		const float centre = dot(world_mid, toward);
		const float extent = dot(world_half, float3{ std::fabs(toward.x), std::fabs(toward.y), std::fabs(toward.z) });
		const float min_depth = centre - extent, max_depth = centre + extent;

		const float3 s = sun_direction;
		const float3 up = std::fabs(s.z) <= 0.9f ? float3{ 0, 0, 1 } : float3{ 1, 0, 0 };
		float3 axis0 = cross(up, s);
		axis0 = axis0 * (1.0f / length(axis0));
		const float3 axis1 = cross(s, axis0);

		const auto axes = cam.axes();
		const float tan_x = std::tan(cam.fov_x * 3.14159265358979323846f / 360.0f);
		const float tan_y = tan_x / aspect;
		float3 rays[4];
		float min[2] = { 0, 0 }, max[2] = { 0, 0 };
		for (int i = 0; i < 4; i++)
		{
			const float x = ((i - 1) & 2) != 0 ? 1.0f : 0.0f;
			const float y = (i & 2) != 0 ? 1.0f : 0.0f;
			const float lx = -((x * 2.0f - 1.0f) * tan_x);
			const float ly = -((2.0f * y - 1.0f) * tan_y);
			float3 ray = axes.up * ly + axes.left * lx + axes.forward;
			ray = ray * (1.0f / length(ray));
			rays[i] = ray;
			const float p[2] = { dot(ray, axis0), dot(ray, axis1) };
			for (int k = 0; k < 2; k++)
			{
				min[k] = std::min(min[k], p[k]);
				max[k] = std::max(max[k], p[k]);
			}
		}

		float offset[2], scales[2];
		for (int axis = 0; axis < 2; axis++)
		{
			const float lo = min[axis], hi = max[axis];
			float scale = (static_cast<float>(draw_size) - 2.0f) / (hi - lo);
			const float first = std::ceil(0.5f - scale * lo) + 0.5f;
			const float last = std::floor(static_cast<float>(draw_size) - 0.5f - hi * scale) - 0.5f;
			float position = first;
			if (last != first)
			{
				if (-lo >= hi)
				{
					position = first - 1.0f;
					scale = (1.0f - position) / lo;
				}
				else
				{
					position = last + 1.0f;
					scale = (static_cast<float>(draw_size) - 1.0f - position) / hi;
				}
			}
			offset[axis] = position;
			scales[axis] = scale;
		}
		const float sample0 = sample_size_near / sample_scale;
		const float sample1 = 4.0f * sample0;
		const float origin_x = dot(axis0, cam.origin), origin_y = dot(axis1, cam.origin);
		float min_cos = FLT_MAX;
		for (const float3& ray : rays)
		{
			min_cos = std::min(min_cos, dot(ray, axes.forward));
		}
		const float texel_scale = sample0 * std::min(scales[0], scales[1]);
		const float distance_scale = texel_scale * min_cos;
		const float3 depth_xyz = axes.forward * (1.0f / distance_scale);
		// C#: forward / distanceScale ( a division per component )
		const float4 depth_row{ axes.forward.x / distance_scale, axes.forward.y / distance_scale, axes.forward.z / distance_scale,
			-dot(cam.origin, float3{ axes.forward.x / distance_scale, axes.forward.y / distance_scale, axes.forward.z / distance_scale }) };
		(void)depth_xyz;

		const float samples[2] = { sample0, sample1 };
		float centres[2][2];
		for (int i = 0; i < 2; i++)
		{
			centres[i][0] = (std::floor(origin_x / samples[i]) + 0.5f) * samples[i];
			centres[i][1] = (std::floor(origin_y / samples[i]) + 0.5f) * samples[i];
		}

		const float fsize = static_cast<float>(size), fmargin = static_cast<float>(margin);
		const float switch_x = ((offset[0] + fmargin) * 0.75f + (centres[0][0] - centres[1][0]) / sample1) / fsize;
		const float v14 = ((fmargin + offset[1]) * 0.75f + (centres[0][1] - centres[1][1]) / sample1) / fsize;

		sun_shadow result;
		result.map_size = size;
		result.partition_count = partitions;
		result.switch_partition = { switch_x, (1.75f - v14) / static_cast<float>(partitions), 1.0f, 0.25f };
		result.scale = { 16.0f / sample_scale, static_cast<float>(partitions) * 16.0f / sample_scale, -8.0f / sample_scale, -24.0f / sample_scale };

		const float depth_scale = 1.0f / (max_depth - min_depth + 2.0f);
		for (int i = 0; i < 2; i++)
		{
			sun_shadow_partition& partition = result.partitions[i];
			partition.view = mat4::from_rows(
				{ axis0.x, axis1.x, -s.x, 0 },
				{ axis0.y, axis1.y, -s.y, 0 },
				{ axis0.z, axis1.z, -s.z, 0 },
				{ -centres[i][0], -centres[i][1], 0, 1 });
			const float m = 2.0f / (samples[i] * fsize);
			partition.projection = mat4::from_rows(
				{ m, 0, 0, 0 },
				{ 0, m, 0, 0 },
				{ 0, 0, depth_scale, 0 },
				{ (offset[0] + fmargin) / fsize * 2.0f - 1.0f, 2.0f * ((fmargin + offset[1]) / fsize) - 1.0f, -((min_depth - 1.0f) * depth_scale), 1 });
			partition.view_projection = render::mul(partition.view, partition.projection);
			partition.depth_scale = depth_scale;
		}

		const float h = 0.5f / fsize;
		const float u0 = h, u1 = h + 1.0f;
		const float hv = 0.5f / (static_cast<float>(partitions) * fsize);
		const float v0 = 1.0f / static_cast<float>(partitions) + hv, v1 = hv;
		const float u_scale = (u1 - u0) * 0.5f, u_offset = (u1 + u0) * 0.5f;
		const float v_scale = (v1 - v0) * 0.5f, v_offset = 0.5f * (v1 + v0);
		const mat4& vp = result.partitions[0].view_projection;
		const float depth[4] = { depth_row.x, depth_row.y, depth_row.z, depth_row.w };
		for (int r = 0; r < 4; r++)
		{
			result.lookup.m[r][0] = vp.m[r][0] * u_scale + vp.m[r][3] * u_offset;
			result.lookup.m[r][1] = vp.m[r][1] * v_scale + vp.m[r][3] * v_offset;
			result.lookup.m[r][2] = vp.m[r][2];
			result.lookup.m[r][3] = depth[r];
		}
		result.pixel_adjust = { 0.25f / fsize, 0.5f / (static_cast<float>(partitions) * fsize), 0.5f / fsize, -0.25f / (static_cast<float>(partitions) * fsize) };
		return result;
	}

	void map_renderer::impl::release_shadow_targets()
	{
		shadow_surface.reset();
		shadow_depth.reset();
		shadow_texture.reset();
		shadow_map.handle.reset();
	}

	void map_renderer::impl::update_sun_shadow_state()
	{
		// GameMapRenderer.SunShadow.cs:43-76 ( UpdateSunShadowState / EnsureShadowTargets )
		sun_shadows_active = false;
		const bool wanted = opts.settings.sun_shadows && (!frame || frame->sun_shadows);
		if (!wanted || !sun)
		{
			return;
		}
		if (!shadow_surface && !shadow_unavailable)
		{
			const UINT size = static_cast<UINT>(opts.settings.sun_shadow_map_size);
			if (FAILED(device->CreateTexture(size, size * 2, 1, D3DUSAGE_RENDERTARGET, D3DFMT_R32F, D3DPOOL_DEFAULT, shadow_texture.put(), nullptr))
				|| FAILED(shadow_texture->GetSurfaceLevel(0, shadow_surface.put()))
				|| FAILED(device->CreateDepthStencilSurface(size, size * 2, D3DFMT_D24S8, D3DMULTISAMPLE_NONE, 0, TRUE, shadow_depth.put(), nullptr)))
			{
				shadow_unavailable = true;
				warn("no sun shadow map target; sun shadows come from the lightmaps only");
				release_shadow_targets();
			}
			else
			{
				*shadow_map.handle.put() = shadow_texture.get();
				shadow_texture.get()->AddRef();
				shadow_map.name = "$shadowmap_sun";
				shadow_map.width = static_cast<int>(size);
				shadow_map.height = static_cast<int>(size * 2);
			}
		}
		sun_shadows_active = static_cast<bool>(shadow_surface);
	}

	void map_renderer::impl::draw_sun_shadows(const camera& cam, const float aspect)
	{
		// GameMapRenderer.SunShadow.cs:90-136 ( DrawSunShadows )
		current_sun_shadow.reset();
		if (!sun_shadows_active || !sun)
		{
			return;
		}
		namespace rt = render::renderer_technique;
		sun_shadow shadow = compute_sun_shadow(cam, aspect, sun->direction, world_mid, world_half, opts.settings);
		const statistics before = frame_stat;
		device->SetRenderTarget(0, shadow_surface.get());
		device->SetDepthStencilSurface(shadow_depth.get());
		D3DVIEWPORT9 viewport{ 0, 0, static_cast<DWORD>(shadow.map_size), static_cast<DWORD>(shadow.map_size * shadow.partition_count), 0.0f, 1.0f };
		device->SetViewport(&viewport);
		device->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL, 0xFFFFFFFF, 1.0f, 0);
		for (int i = 0; i < shadow.partition_count; i++)
		{
			const sun_shadow_partition& partition = shadow.partitions[i];
			viewport = { 0, static_cast<DWORD>(i * shadow.map_size), static_cast<DWORD>(shadow.map_size), static_cast<DWORD>(shadow.map_size), 0.0f, 1.0f };
			device->SetViewport(&viewport);
			constants->begin_shadow_pass(partition.view, partition.projection);
			constants->set("shadowmapPolygonOffset", render::sun_shadow_polygon_offset(opts.settings, partition.depth_scale));
			constants_light = -2;
			constants_instance = nullptr;
			// GameMapRenderer.SunShadow.cs:163-177 ( SidePlanes ): the four sides of the partition, world space
			const mat4& m = partition.view_projection;
			const auto column = [&](const int c) { return float4{ m.m[0][c], m.m[1][c], m.m[2][c], m.m[3][c] }; };
			const float4 c1 = column(0), c2 = column(1), c4 = column(3);
			std::array<float4, 4> planes = { float4{ c4.x + c1.x, c4.y + c1.y, c4.z + c1.z, c4.w + c1.w }, float4{ c4.x - c1.x, c4.y - c1.y, c4.z - c1.z, c4.w - c1.w },
				float4{ c4.x + c2.x, c4.y + c2.y, c4.z + c2.z, c4.w + c2.w }, float4{ c4.x - c2.x, c4.y - c2.y, c4.z - c2.z, c4.w - c2.w } };
			for (float4& plane : planes)
			{
				const float len = std::sqrt(plane.x * plane.x + plane.y * plane.y + plane.z * plane.z);
				const float divisor = len > 0 ? len : 1.0f;
				plane = { plane.x / divisor, plane.y / divisor, plane.z / divisor, plane.w / divisor };
			}
			if (frame->draw_world)
			{
				for (const range& r : ranges)
				{
					for (int index = r.begin; index < r.end; index++)
					{
						const world_surface& surface = surfaces[static_cast<std::size_t>(index)];
						if (surface.brush_model || surface.hidden || (surface.radius > 0.0f && !sphere_visible(planes, surface.center, surface.radius)))
						{
							continue;
						}
						// SpotShadow.cs:240-246 ( DrawCasterSurface ) -> DrawSurface with the forced technique
						material_draw* material = nullptr;
						int technique_type = 0;
						if (surface.material && surface.material->technique(rt::BUILD_SHADOWMAP_COLOR)
							&& resolve_surface(surface, surface.emissive, rt::BUILD_SHADOWMAP_COLOR, material, technique_type))
						{
							draw_resolved_surface(surface, *material, technique_type, render::vertex_format::WORLD + material->set().world_vert_format);
						}
					}
				}
			}
			const auto draw_caster = [&](const static_model_instance& instance)
			{
				if (!instance.draw || instance.hidden
					|| (instance.handle > 0 ? !frame->draw_placed_models : instance.is_entity ? !frame->draw_entity_models : !frame->draw_static_models)
					|| !sphere_visible(planes, instance.center, instance.radius))
				{
					return;
				}
				// SpotShadow.cs:248-279 ( DrawCasterModel )
				const model_lod_draw& lod = instance.draw->lods[0];
				for (std::size_t k = 0; k < lod.surfaces.size(); k++)
				{
					const model_surface_draw& surface = lod.surfaces[k];
					material_draw* material = surface.material;
					if ((!instance.hidden_surfaces.empty() && instance.hidden_surfaces[k]) || !material || material->not_drawn()
						|| !material->technique(rt::BUILD_SHADOWMAP_COLOR) || blends(*material, choose_technique(*material, instance.primary_light, false)))
					{
						continue;
					}
					int triangles = surface.triangle_count;
					int span = surface.vertex_count;
					while (k + 1 < lod.surfaces.size() && lod.surfaces[k + 1].material == material
						&& (instance.hidden_surfaces.empty() || !instance.hidden_surfaces[k + 1])
						&& lod.surfaces[k + 1].base_index == surface.base_index + 3 * triangles)
					{
						triangles += lod.surfaces[k + 1].triangle_count;
						span += lod.surfaces[k + 1].vertex_count;
						k++;
					}
					draw_model_surface({}, instance, lod, surface, *material, rt::BUILD_SHADOWMAP_COLOR, triangles, span);
				}
			};
			for (const static_model_instance& instance : static_models)
			{
				draw_caster(instance);
			}
			for (const auto& placed : placed_models)
			{
				draw_caster(*placed.second);
			}
			if (frame->draw_meshes && !meshes.empty())
			{
				draw_mesh_shadow_casters(planes);
			}
		}
		const int draws = frame_stat.draw_calls - before.draw_calls;
		frame_stat = before;
		frame_stat.sun_shadow_draws = draws;
		current_sun_shadow = std::make_unique<sun_shadow>(shadow);
	}
}
