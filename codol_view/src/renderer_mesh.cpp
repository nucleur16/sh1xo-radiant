// Dynamic meshes ( the editor's brushes ), the fixed function fallback and the overlay hook. Not in the C# renderer.
//
// A mesh is drawn like a piece of the GfxWorld: its vertices are turned into GfxWorldVertex ( position, packed
// normal and tangent, colour, texture coordinates ) and go through draw_passes with the material's own technique
// set - the lit technique the game would pick for a surface whose primary light is the sun, so the sun, the sun
// shadow map, the reflection probe and normal / specular maps all work. What a mesh cannot have is a lightmap:
// where the pass reads the primary lightmap ( the baked sun shadow ) it gets white, and where it reads the
// secondary lightmap ( the baked ambient light ) it gets a 1 x 1 texture with the colour of the light grid at the
// middle of the mesh. A material whose technique set reads the model lighting volume instead gets a block of that
// volume, like a placed model.
// A material that cannot be drawn that way ( no technique for the vertex format, a tool texture the game never
// draws, no technique set ) is drawn through the fixed function pipeline: its colour map, sun N.L + ambient.
//
// MEASURED slot masks ( render_technique_set::slot_mask ) of the shipped technique sets: world sets ( wc_l_sm_.. )
// 0x50E0007FFE1F, model sets ( mc_l_sm_.. ) 0xD0FFFFFFFE1F - both hold "unlit" ( 4 ) and every "lit .." slot but not
// "fakelight normal" / "fakelight view" ( 40 / 41 ), so there is no technique of the game's that lights geometry
// without a lightmap; "unlit" is the bare colour map. Hence the lit technique with stand-in lightmaps.

#include "renderer_impl.hpp"

#include <algorithm>
#include <cfloat>

namespace codol::view
{
	namespace
	{
		constexpr DWORD MESH_FVF = D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_DIFFUSE | D3DFVF_TEX1;
		static_assert(sizeof(mesh_vertex) == 36, "mesh_vertex is D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_DIFFUSE | D3DFVF_TEX1");

		float3 normalised(const float3& v)
		{
			const float len = length(v);
			return len > 0 ? v * (1.0f / len) : v;
		}

		// PackedUnitVec ( math.hpp unpack_unit_vector ) with the scale byte that makes a component ( b - 127 ) / 127
		std::uint32_t pack_unit_vector(const float3& v)
		{
			const auto byte = [](const float c) { return static_cast<std::uint32_t>(std::clamp(static_cast<int>(std::lround(c * 127.0f + 127.0f)), 0, 255)); };
			return byte(v.x) | (byte(v.y) << 8) | (byte(v.z) << 16) | (63u << 24);
		}

		D3DMATRIX to_d3d(const mat4& m)
		{
			D3DMATRIX out;
			std::memcpy(&out, m.data(), sizeof out);
			return out;
		}

		// the game's ground lighting bytes -> a colour ( draw_model_surface: lightprobeAmbient )
		float4 ground_color(const std::uint32_t ground)
		{
			const auto channel = [&](const int shift) { return static_cast<float>(std::pow(static_cast<double>((ground >> shift) & 0xFF) * 0.0078431377, 2.0)); };
			return { channel(0), channel(8), channel(16), static_cast<float>(static_cast<double>(ground >> 24) * 0.00098039221) };
		}
	}

	// ---------------------------------------------------------------------------------------------------
	// create / update

	bool map_renderer::impl::fill_mesh(dynamic_mesh& mesh, const mesh_data& data, std::string* error)
	{
		const auto fail = [&](std::string text)
		{
			last_error = text;
			render::set_error(error, std::move(text));
			return false;
		};
		if (!data.vertices || data.vertex_count == 0)
		{
			return fail("a mesh needs vertices");
		}
		const std::size_t index_count = data.indices ? data.index_count : data.vertex_count;
		if (index_count == 0 || index_count % 3 != 0)
		{
			return fail("a mesh is a triangle list: the number of indices must be a multiple of 3");
		}
		std::vector<std::uint32_t> indices(index_count);
		for (std::size_t i = 0; i < index_count; i++)
		{
			indices[i] = data.indices ? data.indices[i] : static_cast<std::uint32_t>(i);
			if (indices[i] >= data.vertex_count)
			{
				return fail("mesh index " + std::to_string(i) + " is out of range");
			}
		}
		mesh.vertices.assign(data.vertices, data.vertices + data.vertex_count);
		// The game's surfaces are visible from the side ( b - a ) x ( c - a ) points away from ( see side_drawn in
		// renderer_pick.cpp ); every triangle is wound that way by its vertex normals
		float3 box_min{ FLT_MAX, FLT_MAX, FLT_MAX }, box_max{ -FLT_MAX, -FLT_MAX, -FLT_MAX };
		for (std::size_t t = 0; t + 2 < index_count; t += 3)
		{
			const mesh_vertex& a = mesh.vertices[indices[t]];
			const mesh_vertex& b = mesh.vertices[indices[t + 1]];
			const mesh_vertex& c = mesh.vertices[indices[t + 2]];
			if (dot(cross(b.position - a.position, c.position - a.position), a.normal + b.normal + c.normal) > 0)
			{
				std::swap(indices[t + 1], indices[t + 2]);
			}
		}
		for (const mesh_vertex& v : mesh.vertices)
		{
			box_min = { std::min(box_min.x, v.position.x), std::min(box_min.y, v.position.y), std::min(box_min.z, v.position.z) };
			box_max = { std::max(box_max.x, v.position.x), std::max(box_max.y, v.position.y), std::max(box_max.z, v.position.z) };
		}
		mesh.indices = std::move(indices);
		mesh.center = (box_min + box_max) * 0.5f;
		mesh.half = (box_max - box_min) * 0.5f;
		mesh.radius = length(mesh.half);
		mesh.material_name = data.material;
		mesh.fallback_color = data.fallback_color;
		mesh.two_sided = data.two_sided;

		// ---- how it is drawn
		mesh.material = nullptr;
		mesh.color_map = nullptr;
		mesh.shading = mesh_shading::flat_color;
		mesh.format = 0;
		const std::shared_ptr<const library_material> asset = data.material.empty() ? nullptr : scope.find_material(data.material);
		if (asset)
		{
			material_draw* material = make_material(asset, nullptr);
			const int sun_light = sun ? sun->index : 0;
			bool usable = material && !material->not_drawn();
			if (usable)
			{
				// both techniques the frame can ask for ( with and without the sun shadow map ) must be drawable
				// with world vertices
				const int format = render::vertex_format::WORLD + material->set().world_vert_format;
				for (const bool shadows : { false, true })
				{
					render::technique_choice choice;
					choice.primary_light_index = sun_light;
					choice.last_sun_primary_light_index = last_sun_light;
					choice.light_type = sun ? 1 : 0;
					choice.sun_shadows_active = shadows;
					choice.slots = material->slot_mask;
					const int type = render::choose_technique(choice);
					const render::render_technique* technique = material->technique(type);
					usable = usable && technique && !technique->passes.empty();
					for (std::size_t p = 0; usable && p < technique->passes.size(); p++)
					{
						pass_plan* plan = plan_of(*material, type, static_cast<int>(p));
						usable = plan->complete && declaration_of(*plan, format) != nullptr;
					}
					if (usable)
					{
						prepare_technique(*material, type, format);
					}
				}
				mesh.format = format;
			}
			if (usable)
			{
				mesh.material = material;
				mesh.shading = mesh_shading::game_material;
			}
			else
			{
				// the colour map through the fixed function pipeline
				const auto& defs = asset->asset.textures;
				const render::material_texture* color = asset->asset.find_by_semantic(render::texture_semantic::COLOR_MAP);
				if (!color || !color->has_image)
				{
					color = nullptr;
					for (const auto& texture : defs)
					{
						if (texture.has_image && !texture.image_name.empty() && texture.semantic != render::texture_semantic::WATER_MAP)
						{
							color = &texture;
							break;
						}
					}
				}
				gpu_texture* texture = nullptr;
				if (color && material)
				{
					texture = material_texture(*material, static_cast<std::size_t>(color - defs.data()));
				}
				else if (color && !color->embedded)
				{
					texture = get_streamed_texture(color->image_name);
				}
				if (texture && !texture->cube)
				{
					mesh.color_map = texture;
					mesh.shading = mesh_shading::color_map;
				}
			}
		}
		light_mesh(mesh);

		// ---- picking
		mesh.pick.clear();
		std::vector<float3> positions(mesh.vertices.size());
		for (std::size_t v = 0; v < positions.size(); v++)
		{
			positions[v] = mesh.vertices[v].position;
		}
		mesh.pick.own_positions(std::move(positions));
		mesh.pick.reserve(mesh.indices.size() / 3);
		for (std::size_t t = 0; t + 2 < mesh.indices.size(); t += 3)
		{
			mesh.pick.add_triangle(mesh.indices[t], mesh.indices[t + 1], mesh.indices[t + 2], 0);
		}
		mesh.pick.build();
		return upload_mesh(mesh) || fail("cannot create the vertex / index buffers of the mesh");
	}

	void map_renderer::impl::light_mesh(dynamic_mesh& mesh)
	{
		// where the mesh is: the sun as its primary light ( the sun shadow map then shades it ), the ambient light
		// of the light grid, the nearest reflection probe
		int sampled_light = 0;
		std::array<std::uint32_t, 64> block{};
		mesh.ground_lighting = sample_ground_lighting(mesh.center, 0, &sampled_light, &block);
		mesh.primary_light = sun ? sun->index : sampled_light;
		mesh.reflection_probe = nearest_probe(mesh.center);
		if (mesh.shading != mesh_shading::game_material)
		{
			return;
		}
		if (reads_model_lighting(*mesh.material))
		{
			if (mesh.lighting_handle == 0)
			{
				mesh.lighting_handle = allocate_lighting_handle();
			}
			set_lighting_block(mesh.lighting_handle, block);
		}
		// The stand-in for the secondary lightmap: the ambient colour, as the bytes of the ground lighting.
		// MEASURED on mp_killhouse_sh ( a box of wc/com_plastic_wall, the 1 x 1 textures swapped for solid colours ):
		// a black secondary lightmap leaves only the sun patch, a black primary lightmap removes the sun and keeps the
		// ambient, and the output follows the secondary's rgb like the model lighting volume's ( 128 = full ).
		const std::uint32_t g = mesh.ground_lighting;
		const std::uint32_t argb = 0xFF000000u | ((g & 0xFF) << 16) | (g & 0xFF00) | ((g >> 16) & 0xFF);
		if (!mesh.ambient.handle || mesh.ambient_argb != argb)
		{
			mesh.ambient = {};
			mesh.ambient_argb = argb;
			if (!create_solid(mesh.ambient, "$mesh_ambient", argb))
			{
				warn("the ambient texture of a mesh could not be created");
			}
		}
	}

	bool map_renderer::impl::upload_mesh(dynamic_mesh& mesh)
	{
		const auto drop = [&](const std::size_t bytes) { stat.geometry_bytes -= std::min<std::uint64_t>(stat.geometry_bytes, bytes); };
		drop(static_cast<std::size_t>(mesh.vertex_capacity) + static_cast<std::size_t>(mesh.index_capacity) + static_cast<std::size_t>(mesh.layer_capacity));
		mesh.vertex_buffer.reset();
		mesh.index_buffer.reset();
		mesh.layer_buffer.reset();
		mesh.vertex_capacity = mesh.index_capacity = mesh.layer_capacity = 0;
		const std::size_t count = mesh.vertices.size();
		if (mesh.shading == mesh_shading::game_material)
		{
			// tangents from the texture coordinates ( per triangle, summed per vertex )
			std::vector<float3> tangents(count), bitangents(count);
			for (std::size_t t = 0; t + 2 < mesh.indices.size(); t += 3)
			{
				const mesh_vertex& a = mesh.vertices[mesh.indices[t]];
				const mesh_vertex& b = mesh.vertices[mesh.indices[t + 1]];
				const mesh_vertex& c = mesh.vertices[mesh.indices[t + 2]];
				const float3 e1 = b.position - a.position, e2 = c.position - a.position;
				const float du1 = b.uv[0] - a.uv[0], dv1 = b.uv[1] - a.uv[1], du2 = c.uv[0] - a.uv[0], dv2 = c.uv[1] - a.uv[1];
				const float det = du1 * dv2 - du2 * dv1;
				if (std::fabs(det) < 1.0e-12f)
				{
					continue;
				}
				const float3 tangent = (e1 * dv2 - e2 * dv1) * (1.0f / det), bitangent = (e2 * du1 - e1 * du2) * (1.0f / det);
				for (int k = 0; k < 3; k++)
				{
					tangents[mesh.indices[t + static_cast<std::size_t>(k)]] = tangents[mesh.indices[t + static_cast<std::size_t>(k)]] + tangent;
					bitangents[mesh.indices[t + static_cast<std::size_t>(k)]] = bitangents[mesh.indices[t + static_cast<std::size_t>(k)]] + bitangent;
				}
			}
			std::vector<std::uint8_t> bytes(count * sizeof(c1::GfxWorldVertex));
			for (std::size_t v = 0; v < count; v++)
			{
				const mesh_vertex& in = mesh.vertices[v];
				const float3 normal = normalised(in.normal);
				float3 tangent = tangents[v] - normal * dot(normal, tangents[v]);
				if (dot(tangent, tangent) < 1.0e-12f)
				{
					// no usable texture coordinates: any direction in the plane
					tangent = cross(normal, std::fabs(normal.z) < 0.9f ? float3{ 0, 0, 1 } : float3{ 1, 0, 0 });
				}
				tangent = normalised(tangent);
				c1::GfxWorldVertex out{};
				out.xyz[0] = in.position.x;
				out.xyz[1] = in.position.y;
				out.xyz[2] = in.position.z;
				out.binormalSign = dot(cross(normal, tangent), bitangents[v]) < 0 ? -1.0f : 1.0f;
				out.color = in.color;
				out.texCoord[0] = in.uv[0];
				out.texCoord[1] = in.uv[1];
				out.lmapCoord[0] = out.lmapCoord[1] = 0.5f;
				out.normal = pack_unit_vector(normal);
				out.tangent = pack_unit_vector(tangent);
				std::memcpy(bytes.data() + v * sizeof out, &out, sizeof out);
			}
			mesh.vertex_buffer = create_vertex_buffer(bytes);
			mesh.vertex_stride = sizeof(c1::GfxWorldVertex);
			mesh.vertex_capacity = static_cast<int>(bytes.size());
			if (const int layer_stride = render::stream1_stride(mesh.format); layer_stride > 0)
			{
				// more texture layers than one: the mesh has none, their blend data is zero
				const std::vector<std::uint8_t> layer(count * static_cast<std::size_t>(layer_stride), 0);
				mesh.layer_buffer = create_vertex_buffer(layer);
				mesh.layer_capacity = static_cast<int>(layer.size());
			}
		}
		else
		{
			mesh.vertex_buffer = create_vertex_buffer({ reinterpret_cast<const std::uint8_t*>(mesh.vertices.data()), count * sizeof(mesh_vertex) });
			mesh.vertex_stride = sizeof(mesh_vertex);
			mesh.vertex_capacity = static_cast<int>(count * sizeof(mesh_vertex));
		}
		mesh.index_buffer = create_index_buffer({ reinterpret_cast<const std::uint8_t*>(mesh.indices.data()), mesh.indices.size() * 4 }, true);
		mesh.index_capacity = static_cast<int>(mesh.indices.size() * 4);
		return mesh.vertex_buffer && mesh.index_buffer;
	}

	int map_renderer::impl::create_mesh(const mesh_data& data, const int handle, std::string* error)
	{
		if (!map_loaded || device_lost)
		{
			last_error = "no map is loaded";
			render::set_error(error, last_error);
			return 0;
		}
		auto mesh = std::make_unique<dynamic_mesh>();
		if (!fill_mesh(*mesh, data, error))
		{
			release_lighting_handle(mesh->lighting_handle);
			return 0;
		}
		mesh->handle = handle > 0 ? handle : ++next_handle;
		next_handle = std::max(next_handle, mesh->handle);
		const int result = mesh->handle;
		meshes[result] = std::move(mesh);
		return result;
	}

	bool map_renderer::impl::destroy_mesh(const int handle)
	{
		const auto it = meshes.find(handle);
		if (it == meshes.end())
		{
			return false;
		}
		dynamic_mesh& mesh = *it->second;
		if (mesh.hidden)
		{
			hidden_count--;
		}
		release_lighting_handle(mesh.lighting_handle);
		stat.geometry_bytes -= std::min<std::uint64_t>(stat.geometry_bytes, static_cast<std::uint64_t>(mesh.vertex_capacity) + static_cast<std::uint64_t>(mesh.index_capacity)
			+ static_cast<std::uint64_t>(mesh.layer_capacity));
		opaque_meshes.clear();
		transparent_meshes.clear();
		fixed_meshes.clear();
		meshes.erase(it);
		return true;
	}

	// ---------------------------------------------------------------------------------------------------
	// drawing

	void map_renderer::impl::collect_meshes(const camera& cam, const std::span<const float4> frustum)
	{
		for (const auto& entry : meshes)
		{
			dynamic_mesh& mesh = *entry.second;
			mesh.drawn = false;
			if (mesh.hidden || !mesh.vertex_buffer || !mesh.index_buffer || !sphere_visible(frustum, mesh.center - cam.origin, mesh.radius))
			{
				continue;
			}
			if (mesh.shading != mesh_shading::game_material)
			{
				mesh.drawn = true;
				frame_stat.meshes++;
				fixed_meshes.push_back(&mesh);
				continue;
			}
			const int technique_type = choose_technique(*mesh.material, mesh.primary_light, false);
			if (!mesh.material->technique(technique_type))
			{
				frame_stat.skipped_no_technique++;
				continue;
			}
			mesh.drawn = true;
			frame_stat.meshes++;
			(blends(*mesh.material, technique_type) ? transparent_meshes : opaque_meshes).push_back({ &mesh, technique_type });
		}
	}

	void map_renderer::impl::draw_meshes(const float3& eye, const std::vector<mesh_draw_instance>& draws, const int forced_technique)
	{
		constants_instance = nullptr;
		for (const mesh_draw_instance& entry : draws)
		{
			dynamic_mesh& mesh = *entry.mesh;
			const int technique = forced_technique >= 0 ? forced_technique : entry.technique;
			if (!mesh.material->technique(technique))
			{
				continue;
			}
			constants->set_world(mat4::translation(-eye));
			set_primary_light(mesh.primary_light);
			if (lighting_volume && mesh.lighting_handle > 0)
			{
				constants->set("baseLightingCoords", lighting_volume->base_coords(mesh.lighting_handle));
			}
			constants->set("lightprobeAmbient", ground_color(mesh.ground_lighting));
			surface_draw draw;
			draw.vertex_buffer = mesh.vertex_buffer.get();
			draw.vertex_stride = mesh.vertex_stride;
			draw.index_buffer = mesh.index_buffer.get();
			draw.vertex_count = static_cast<int>(mesh.vertices.size());
			draw.triangle_count = static_cast<int>(mesh.indices.size() / 3);
			if (mesh.layer_buffer)
			{
				draw.layer_buffer = mesh.layer_buffer.get();
				draw.layer_stride = render::stream1_stride(mesh.format);
			}
			draw.two_sided = mesh.two_sided;
			// no lightmap: full sun where the baked sun shadow would be, the light grid's colour for the baked ambient
			const lightmap_pair lightmap{ &white, mesh.ambient.handle ? &mesh.ambient : &black };
			draw_passes(*mesh.material, technique, mesh.format, mesh.reflection_probe, &lightmap, draw);
		}
		constants_instance = nullptr;
	}

	void map_renderer::impl::draw_mesh_shadow_casters(const std::span<const float4> planes)
	{
		namespace rt = render::renderer_technique;
		std::vector<mesh_draw_instance> casters;
		for (const auto& entry : meshes)
		{
			dynamic_mesh& mesh = *entry.second;
			if (mesh.hidden || mesh.shading != mesh_shading::game_material || !mesh.vertex_buffer || !mesh.index_buffer
				|| !mesh.material->technique(rt::BUILD_SHADOWMAP_COLOR) || blends(*mesh.material, choose_technique(*mesh.material, mesh.primary_light, false))
				|| !sphere_visible(planes, mesh.center, mesh.radius))
			{
				continue;
			}
			casters.push_back({ &mesh, rt::BUILD_SHADOWMAP_COLOR });
		}
		if (!casters.empty())
		{
			draw_meshes({}, casters, rt::BUILD_SHADOWMAP_COLOR);
		}
	}

	void map_renderer::impl::set_fixed_function_state(const camera& cam, const int width, const int height)
	{
		// the state frame_options::overlay documents
		device->SetVertexShader(nullptr);
		device->SetPixelShader(nullptr);
		device->SetFVF(D3DFVF_XYZ | D3DFVF_DIFFUSE);
		device->SetTexture(0, nullptr);
		device->SetTexture(1, nullptr);
		device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
		device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
		device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
		device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
		device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);
		device->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
		device->SetTextureStageState(0, D3DTSS_TEXCOORDINDEX, 0);
		device->SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
		device->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
		device->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
		static constexpr render::render_state_value STATES[] =
		{
			{ D3DRS_LIGHTING, FALSE },
			{ D3DRS_FOGENABLE, FALSE },
			{ D3DRS_SPECULARENABLE, FALSE },
			{ D3DRS_COLORVERTEX, TRUE },
			{ D3DRS_SHADEMODE, D3DSHADE_GOURAUD },
			{ D3DRS_ZENABLE, D3DZB_TRUE },
			{ D3DRS_ZWRITEENABLE, TRUE },
			{ D3DRS_ZFUNC, D3DCMP_LESSEQUAL },
			{ D3DRS_CULLMODE, D3DCULL_NONE },
			{ D3DRS_FILLMODE, D3DFILL_SOLID },
			{ D3DRS_ALPHABLENDENABLE, FALSE },
			{ D3DRS_SEPARATEALPHABLENDENABLE, FALSE },
			{ D3DRS_ALPHATESTENABLE, FALSE },
			{ D3DRS_STENCILENABLE, FALSE },
			{ D3DRS_COLORWRITEENABLE, 0xF },
			{ D3DRS_SRGBWRITEENABLE, FALSE },
			{ D3DRS_DEPTHBIAS, 0 },
			{ D3DRS_SLOPESCALEDEPTHBIAS, 0 },
			{ D3DRS_SCISSORTESTENABLE, FALSE },
			{ D3DRS_CLIPPLANEENABLE, 0 },
			{ D3DRS_CLIPPING, TRUE },
			{ D3DRS_VERTEXBLEND, D3DVBF_DISABLE },
			{ D3DRS_POINTSPRITEENABLE, FALSE },
		};
		for (const auto& state : STATES)
		{
			device->SetRenderState(static_cast<D3DRENDERSTATETYPE>(state.state), state.value);
		}
		const D3DMATRIX world = to_d3d(mat4::identity());
		const D3DMATRIX view = to_d3d(render::mul(mat4::translation(-cam.origin), cam.view_rotation()));
		const D3DMATRIX projection = to_d3d(cam.projection(static_cast<float>(width) / static_cast<float>(height)));
		device->SetTransform(D3DTS_WORLD, &world);
		device->SetTransform(D3DTS_VIEW, &view);
		device->SetTransform(D3DTS_PROJECTION, &projection);
		// nothing the shader path cached is true any more
		reset_frame_caches();
	}

	void map_renderer::impl::draw_fixed_meshes(const camera& cam, const int width, const int height)
	{
		// fixed function: colour map ( or the flat colour ) x ( ambient + sun N.L ) x vertex colour
		set_fixed_function_state(cam, width, height);
		device->SetFVF(MESH_FVF);
		device->SetRenderState(D3DRS_LIGHTING, TRUE);
		device->SetRenderState(D3DRS_NORMALIZENORMALS, TRUE);
		device->SetRenderState(D3DRS_DIFFUSEMATERIALSOURCE, D3DMCS_COLOR1);
		device->SetRenderState(D3DRS_AMBIENTMATERIALSOURCE, D3DMCS_COLOR1);
		device->SetRenderState(D3DRS_SPECULARMATERIALSOURCE, D3DMCS_MATERIAL);
		device->SetRenderState(D3DRS_EMISSIVEMATERIALSOURCE, D3DMCS_MATERIAL);
		device->SetRenderState(D3DRS_LOCALVIEWER, FALSE);
		if (frame->wireframe)
		{
			device->SetRenderState(D3DRS_FILLMODE, D3DFILL_WIREFRAME);
		}
		D3DMATERIAL9 surface{};
		surface.Diffuse = surface.Ambient = { 1, 1, 1, 1 };
		device->SetMaterial(&surface);
		D3DLIGHT9 light{};
		light.Type = D3DLIGHT_DIRECTIONAL;
		// the sun's direction points at the sun; a light without one shines straight down
		float3 to_sun = sun ? normalised(sun->direction) : float3{ 0, 0, 1 };
		if (to_sun.z < 0)
		{
			to_sun = -to_sun;
		}
		light.Direction = { -to_sun.x, -to_sun.y, -to_sun.z };
		const float3 sun_color = sun ? sun->color : float3{ 1, 1, 1 };
		const float strongest = std::max({ sun_color.x, sun_color.y, sun_color.z, 1.0e-3f });
		light.Diffuse = { 0.75f * sun_color.x / strongest, 0.75f * sun_color.y / strongest, 0.75f * sun_color.z / strongest, 1.0f };
		device->SetLight(0, &light);
		device->LightEnable(0, TRUE);
		for (DWORD other = 1; other < 8; other++)
		{
			device->LightEnable(other, FALSE);
		}
		device->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
		device->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);
		device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
		device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
		device->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR);
		device->SetSamplerState(0, D3DSAMP_SRGBTEXTURE, FALSE);
		for (dynamic_mesh* mesh : fixed_meshes)
		{
			const float4 ambient = ground_color(mesh->ground_lighting);
			const auto byte = [](const float c) { return static_cast<DWORD>(std::clamp(static_cast<int>(std::lround((0.25f + 0.5f * std::sqrt(std::max(c, 0.0f))) * 255.0f)), 0, 255)); };
			device->SetRenderState(D3DRS_AMBIENT, 0xFF000000 | (byte(ambient.x) << 16) | (byte(ambient.y) << 8) | byte(ambient.z));
			device->SetRenderState(D3DRS_CULLMODE, mesh->two_sided ? D3DCULL_NONE : D3DCULL_CCW);
			const bool textured = mesh->shading == mesh_shading::color_map && mesh->color_map && mesh->color_map->handle;
			device->SetTexture(0, textured ? mesh->color_map->handle.get() : nullptr);
			device->SetTextureStageState(0, D3DTSS_COLOROP, textured ? D3DTOP_MODULATE : D3DTOP_SELECTARG2);
			device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
			device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
			if (!textured)
			{
				// the flat colour replaces the vertex colour's rgb
				device->SetRenderState(D3DRS_DIFFUSEMATERIALSOURCE, D3DMCS_MATERIAL);
				device->SetRenderState(D3DRS_AMBIENTMATERIALSOURCE, D3DMCS_MATERIAL);
				D3DMATERIAL9 flat{};
				const std::uint32_t c = mesh->fallback_color;
				flat.Diffuse = flat.Ambient = { static_cast<float>((c >> 16) & 0xFF) / 255.0f, static_cast<float>((c >> 8) & 0xFF) / 255.0f,
					static_cast<float>(c & 0xFF) / 255.0f, static_cast<float>(c >> 24) / 255.0f };
				device->SetMaterial(&flat);
			}
			else
			{
				device->SetRenderState(D3DRS_DIFFUSEMATERIALSOURCE, D3DMCS_COLOR1);
				device->SetRenderState(D3DRS_AMBIENTMATERIALSOURCE, D3DMCS_COLOR1);
				device->SetMaterial(&surface);
			}
			device->SetStreamSource(0, mesh->vertex_buffer.get(), 0, sizeof(mesh_vertex));
			device->SetIndices(mesh->index_buffer.get());
			const UINT triangles = static_cast<UINT>(mesh->indices.size() / 3);
			device->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, static_cast<UINT>(mesh->vertices.size()), 0, triangles);
			frame_stat.triangles += triangles;
			frame_stat.draw_calls++;
		}
		device->SetRenderState(D3DRS_LIGHTING, FALSE);
		device->LightEnable(0, FALSE);
		device->SetTexture(0, nullptr);
		reset_frame_caches();
	}

	void map_renderer::impl::run_overlay(const camera& cam, const int width, const int height)
	{
		set_fixed_function_state(cam, width, height);
		overlay_context context;
		context.device = device.get();
		context.cam = cam;
		context.width = width;
		context.height = height;
		view_matrix(cam, context.view);
		projection_matrix(cam, width, height, context.projection);
		try
		{
			frame->overlay(context);
		}
		catch (const std::exception& e)
		{
			// the host's state still has to be restored
			last_error = std::string("overlay: ") + e.what();
		}
		reset_frame_caches();
	}

	// ---------------------------------------------------------------------------------------------------

	int map_renderer::create_mesh(const mesh_data& data, std::string* error)
	{
		try
		{
			return impl_->create_mesh(data, 0, error);
		}
		catch (const std::exception& e)
		{
			impl_->last_error = std::string("create_mesh: ") + e.what();
			render::set_error(error, impl_->last_error);
			return 0;
		}
	}

	bool map_renderer::update_mesh(const int handle, const mesh_data& data, std::string* error)
	{
		try
		{
			impl& i = *impl_;
			dynamic_mesh* mesh = i.map_loaded && !i.device_lost ? i.find_mesh(handle) : nullptr;
			if (!mesh)
			{
				render::set_error(error, "no such mesh");
				return false;
			}
			// on failure the mesh keeps what it had
			dynamic_mesh updated;
			updated.handle = mesh->handle;
			updated.hidden = mesh->hidden;
			updated.lighting_handle = mesh->lighting_handle;
			updated.vertex_capacity = mesh->vertex_capacity;
			updated.index_capacity = mesh->index_capacity;
			updated.layer_capacity = mesh->layer_capacity;
			if (!i.fill_mesh(updated, data, error))
			{
				if (updated.lighting_handle != mesh->lighting_handle)
				{
					i.release_lighting_handle(updated.lighting_handle);
				}
				return false;
			}
			if (updated.shading != mesh_shading::game_material || !i.reads_model_lighting(*updated.material))
			{
				i.release_lighting_handle(updated.lighting_handle);
				updated.lighting_handle = 0;
			}
			i.opaque_meshes.clear();
			i.transparent_meshes.clear();
			i.fixed_meshes.clear();
			mesh->vertices = std::move(updated.vertices);
			mesh->indices = std::move(updated.indices);
			mesh->material_name = std::move(updated.material_name);
			mesh->fallback_color = updated.fallback_color;
			mesh->two_sided = updated.two_sided;
			mesh->shading = updated.shading;
			mesh->material = updated.material;
			mesh->color_map = updated.color_map;
			mesh->format = updated.format;
			mesh->vertex_buffer = std::move(updated.vertex_buffer);
			mesh->layer_buffer = std::move(updated.layer_buffer);
			mesh->index_buffer = std::move(updated.index_buffer);
			mesh->vertex_capacity = updated.vertex_capacity;
			mesh->index_capacity = updated.index_capacity;
			mesh->layer_capacity = updated.layer_capacity;
			mesh->vertex_stride = updated.vertex_stride;
			mesh->center = updated.center;
			mesh->half = updated.half;
			mesh->radius = updated.radius;
			mesh->primary_light = updated.primary_light;
			mesh->reflection_probe = updated.reflection_probe;
			mesh->lighting_handle = updated.lighting_handle;
			mesh->ground_lighting = updated.ground_lighting;
			mesh->ambient = std::move(updated.ambient);
			mesh->ambient_argb = updated.ambient_argb;
			mesh->pick.swap(updated.pick);
			return true;
		}
		catch (const std::exception& e)
		{
			impl_->last_error = std::string("update_mesh: ") + e.what();
			render::set_error(error, impl_->last_error);
			return false;
		}
	}

	bool map_renderer::destroy_mesh(const int handle) { return impl_->destroy_mesh(handle); }
	bool map_renderer::set_mesh_visible(const int handle, const bool visible) { return impl_->set_hidden({ object_kind::dynamic_mesh, handle }, !visible); }

	mesh_shading map_renderer::mesh_shading_of(const int handle) const
	{
		const dynamic_mesh* mesh = impl_->find_mesh(handle);
		return mesh ? mesh->shading : mesh_shading::none;
	}
}
