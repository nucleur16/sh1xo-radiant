// Picking and the selection helpers: a ray against the scene as it is placed now. Not in the C# renderer.
//
// The static world is one BVH in world space; every brush model, every distinct XModel ( LOD 0 ) and every
// dynamic mesh has its own in its own space, and the ray is taken into that space through the placement of the
// instance. Instances are walked linearly with a sphere test first ( a few ten thousand at most ).

#include "renderer_impl.hpp"

#include <cfloat>

namespace codol::view
{
	namespace
	{
		float3 normalised(const float3& v)
		{
			const float len = length(v);
			return len > 0 ? v * (1.0f / len) : v;
		}

		// may the ray hit a sphere before `limit`
		bool ray_sphere(const float3& origin, const float3& direction, const float3& center, const float radius, const float limit)
		{
			const float3 oc = center - origin;
			const float along = dot(oc, direction);
			if (along + radius < 0 || along - radius > limit)
			{
				return false;
			}
			return dot(oc, oc) - along * along <= radius * radius;
		}

		// Is a triangle drawn towards the ray. `facing` = dot( ( b - a ) x ( c - a ), ray direction ) in game
		// ( right handed ) axes. The view transform mirrors, so what Direct3D sees clockwise - the side D3DCULL_CCW
		// keeps, the game's "cull back" - is the side this normal points away from.
		bool side_drawn(const int cull, const float facing)
		{
			return cull == 2 ? facing > 0 : cull == 3 ? facing < 0 : true;
		}

		float3 rows_transform(const float3& v, const mat4& m)
		{
			return { v.x * m.m[0][0] + v.y * m.m[1][0] + v.z * m.m[2][0], v.x * m.m[0][1] + v.y * m.m[1][1] + v.z * m.m[2][1],
				v.x * m.m[0][2] + v.y * m.m[1][2] + v.z * m.m[2][2] };
		}
	}

	int map_renderer::impl::cull_of(material_draw* material) const
	{
		if (!material)
		{
			return 1;
		}
		if (material->cull < 0)
		{
			material->cull = 1;
			if (const auto& state = material->asset().state)
			{
				namespace rt = render::renderer_technique;
				const render::material_state::bits* bits = nullptr;
				for (const int type : { rt::LIT, rt::LIT - 1, rt::LIT_SUN, rt::LIT_SUN - 1, rt::EMISSIVE, rt::UNLIT })
				{
					bits = bits ? bits : state->pass_bits(type, 0);
				}
				for (int type = 0; type < render::build::TECHNIQUE_COUNT && !bits; type++)
				{
					bits = state->pass_bits(type, 0);
				}
				if (bits)
				{
					material->cull = static_cast<int>((bits->bits0 >> 14) & 3);
				}
			}
			// the sky box: its technique set is "wc_sky" ( "wc_sky_.." )
			material->sky = to_lower(material->set().name).find("sky") != std::string::npos && material->asset().state
				&& !material->asset().state->blends(render::renderer_technique::LIT);
		}
		return material->cull;
	}

	bool map_renderer::impl::pick_ray(const float3& origin, const float3& ray_direction, pick_result& out, const pick_options& options) const
	{
		if (!map_loaded)
		{
			return false;
		}
		const float3 direction = normalised(ray_direction);
		if (!(dot(direction, direction) > 0))
		{
			return false;
		}
		float best = options.max_distance > 0 ? options.max_distance : FLT_MAX;
		bool found = false;
		pick_result result;
		// priorities: what is drawn later wins a tie. World surfaces by their index ( the draw order of the three
		// ranges ), then models, brush models and meshes over the world they stand on.
		constexpr int MODEL_PRIORITY = 0x1000000;
		const auto side_priority = [&](material_draw* material, const float facing, const int priority)
		{
			const int cull = cull_of(material);
			if (material && material->sky && !options.sky)
			{
				return -1;
			}
			return options.backfaces || side_drawn(cull, facing) ? priority : -1;
		};
		const auto first_triangle_of = [](const pick_mesh& mesh, std::uint32_t triangle)
		{
			// triangles of a surface are consecutive
			const std::uint32_t surface = mesh.surface_of(triangle);
			while (triangle > 0 && mesh.surface_of(triangle - 1) == surface)
			{
				triangle--;
			}
			return triangle;
		};
		const auto record = [&](const pick_mesh& mesh, const pick_hit& hit, const object_ref ref, const int entity, const float3& world_normal, const material_draw* material)
		{
			best = hit.t;
			found = true;
			result = {};
			result.object = ref;
			result.entity = entity;
			result.distance = hit.t;
			result.point = origin + direction * hit.t;
			const float3 n = normalised(world_normal);
			result.normal = dot(n, direction) > 0 ? -n : n;
			result.surface = static_cast<int>(mesh.surface_of(hit.triangle));
			result.triangle = static_cast<int>(hit.triangle - first_triangle_of(mesh, hit.triangle));
			if (material)
			{
				result.material = material->asset().name;
			}
		};

		// ---- the static world
		if (options.world && !world_pick.empty())
		{
			pick_hit hit;
			hit.priority = -1;
			if (world_pick.raycast(origin, direction, best, hit, [&](const std::uint32_t surface, const float facing)
				{
					const world_surface& s = surfaces[surface];
					return s.hidden ? -1 : side_priority(s.material, facing, s.index);
				}))
			{
				const world_surface& s = surfaces[world_pick.surface_of(hit.triangle)];
				record(world_pick, hit, { object_kind::world_surface, s.index }, -1, hit.normal, s.material);
			}
		}

		// ---- models
		const auto test_model = [&](const static_model_instance& m)
		{
			if (m.hidden || !m.draw || !m.draw->pick || !ray_sphere(origin, direction, m.center, m.radius, best))
			{
				return;
			}
			// the game does not draw a static model beyond its cull distance
			if (m.cull_distance > 0 && length(m.center - origin) - m.radius > m.cull_distance)
			{
				return;
			}
			const float3 local_origin = to_local(origin, m.placement, true);
			const float3 local_direction = to_local(direction, m.placement, false);
			const auto& model_surfaces = m.draw->surfaces();
			pick_hit hit;
			hit.priority = found ? MODEL_PRIORITY - 1 : -1;		// a model wins a tie with the world, not with another model
			if (m.draw->pick->raycast(local_origin, local_direction, best, hit, [&](const std::uint32_t surface, const float facing)
				{
					if (surface >= model_surfaces.size() || (!m.hidden_surfaces.empty() && m.hidden_surfaces[surface]))
					{
						return -1;
					}
					material_draw* material = model_surfaces[surface].material;
					return material && !material->not_drawn() ? side_priority(material, facing, MODEL_PRIORITY + static_cast<int>(surface)) : -1;
				}))
			{
				const object_ref ref{ m.handle > 0 ? object_kind::placed_model : m.is_entity ? object_kind::entity_model : object_kind::static_model,
					m.handle > 0 ? m.handle : m.is_entity ? m.entity_index : m.static_index };
				record(*m.draw->pick, hit, ref, m.entity_index, rows_transform(hit.normal, m.placement), model_surfaces[m.draw->pick->surface_of(hit.triangle)].material);
			}
		};
		if (options.static_models || options.entity_models)
		{
			for (const static_model_instance& m : static_models)
			{
				if (m.is_entity ? options.entity_models : options.static_models)
				{
					test_model(m);
				}
			}
		}
		if (options.placed_models)
		{
			for (const auto& placed : placed_models)
			{
				test_model(*placed.second);
			}
		}

		// ---- brush models
		if (options.brush_models)
		{
			for (std::size_t b = 0; b < brush_models.size(); b++)
			{
				const brush_model_instance& instance = brush_models[b];
				if (instance.hidden || (instance.special_ops && !options.special_ops_brush_models) || !ray_sphere(origin, direction, instance.center, instance.radius, best))
				{
					continue;
				}
				const auto mesh = brush_pick.find(static_cast<int>(b));
				if (mesh == brush_pick.end() || mesh->second->empty())
				{
					continue;
				}
				pick_hit hit;
				hit.priority = found ? MODEL_PRIORITY - 1 : -1;
				if (mesh->second->raycast(to_local(origin, instance.placement, true), to_local(direction, instance.placement, false), best, hit,
					[&](const std::uint32_t surface, const float facing) { return side_priority(surfaces[surface].material, facing, MODEL_PRIORITY + static_cast<int>(surface)); }))
				{
					const world_surface& s = surfaces[mesh->second->surface_of(hit.triangle)];
					record(*mesh->second, hit, { object_kind::brush_model, instance.entity_index }, instance.entity_index, rows_transform(hit.normal, instance.placement), s.material);
				}
			}
		}

		// ---- dynamic meshes ( world space, triangles wound like the game's: the normal side is the far side of ( b - a ) x ( c - a ) )
		if (options.meshes)
		{
			for (const auto& entry : meshes)
			{
				const dynamic_mesh& mesh = *entry.second;
				if (mesh.hidden || mesh.pick.empty() || !ray_sphere(origin, direction, mesh.center, mesh.radius, best))
				{
					continue;
				}
				pick_hit hit;
				hit.priority = found ? MODEL_PRIORITY - 1 : -1;
				if (mesh.pick.raycast(origin, direction, best, hit,
					[&](std::uint32_t, const float facing) { return options.backfaces || mesh.two_sided || facing > 0 ? MODEL_PRIORITY : -1; }))
				{
					record(mesh.pick, hit, { object_kind::dynamic_mesh, mesh.handle }, -1, hit.normal, nullptr);
					result.surface = 0;
					result.triangle = static_cast<int>(hit.triangle);
					result.material = mesh.material_name;
				}
			}
		}
		if (found)
		{
			out = std::move(result);
		}
		return found;
	}

	bool map_renderer::impl::object_box(const object_ref& ref, oriented_box& out) const
	{
		if (!map_loaded)
		{
			return false;
		}
		out = {};
		const auto placed = [&](const float3& min, const float3& max, const mat4& placement)
		{
			out.center = transform_point((min + max) * 0.5f, placement);
			for (int axis = 0; axis < 3; axis++)
			{
				const float3 row{ placement.m[axis][0], placement.m[axis][1], placement.m[axis][2] };
				const float len = length(row);
				out.axis[axis] = len > 0 ? row * (1.0f / len) : out.axis[axis];
				const float extent = (axis == 0 ? max.x - min.x : axis == 1 ? max.y - min.y : max.z - min.z) * 0.5f * len;
				(axis == 0 ? out.half_size.x : axis == 1 ? out.half_size.y : out.half_size.z) = extent;
			}
		};
		if (const brush_model_instance* brush = find_brush(ref))
		{
			placed(brush->local_min, brush->local_max, brush->placement);
			return true;
		}
		if (const static_model_instance* model = find_model(ref))
		{
			if (model->draw && !model->draw->surfaces().empty())
			{
				placed(model->draw->min, model->draw->max, model->placement);
			}
			else
			{
				out.center = model->center;
				out.half_size = model->half;
			}
			return true;
		}
		scene_object info;
		if (!object(ref, info))
		{
			return false;
		}
		out.center = info.center;
		out.half_size = info.half_size;
		return true;
	}

	bool map_renderer::impl::object_triangles(const object_ref& ref, std::vector<float3>& out) const
	{
		if (!map_loaded)
		{
			return false;
		}
		const auto append = [&](const pick_mesh& mesh, const mat4* placement, const auto& wanted)
		{
			for (std::uint32_t t = 0; t < mesh.triangle_count(); t++)
			{
				if (!wanted(mesh.surface_of(t)))
				{
					continue;
				}
				float3 a, b, c;
				mesh.triangle(t, a, b, c);
				out.push_back(placement ? transform_point(a, *placement) : a);
				out.push_back(placement ? transform_point(b, *placement) : b);
				out.push_back(placement ? transform_point(c, *placement) : c);
			}
		};
		switch (ref.kind)
		{
		case object_kind::world_surface:
		{
			if (ref.id < 0 || ref.id >= static_cast<int>(surfaces.size()) || surfaces[static_cast<std::size_t>(ref.id)].brush_model || world_pick.empty())
			{
				return false;
			}
			const std::uint32_t id = static_cast<std::uint32_t>(ref.id);
			append(world_pick, nullptr, [&](const std::uint32_t surface) { return surface == id; });
			return true;
		}
		case object_kind::brush_model:
		{
			const brush_model_instance* brush = find_brush(ref);
			const auto mesh = brush ? brush_pick.find(static_cast<int>(brush - brush_models.data())) : brush_pick.end();
			if (mesh == brush_pick.end())
			{
				return false;
			}
			append(*mesh->second, &brush->placement, [](std::uint32_t) { return true; });
			return true;
		}
		case object_kind::dynamic_mesh:
		{
			const dynamic_mesh* mesh = find_mesh(ref.id);
			if (!mesh)
			{
				return false;
			}
			for (const std::uint32_t index : mesh->indices)
			{
				out.push_back(mesh->vertices[index].position);
			}
			return true;
		}
		default:
		{
			const static_model_instance* model = find_model(ref);
			if (!model || !model->draw || !model->draw->pick)
			{
				return false;
			}
			append(*model->draw->pick, &model->placement, [&](const std::uint32_t surface) { return model->hidden_surfaces.empty() || !model->hidden_surfaces[surface]; });
			return true;
		}
		}
	}

	// ---------------------------------------------------------------------------------------------------

	bool map_renderer::pick(const camera& cam, const int width, const int height, const float px, const float py, pick_result& out, const pick_options& options) const
	{
		float3 origin, direction;
		screen_ray(cam, width, height, px, py, origin, direction);
		return impl_->pick_ray(origin, direction, out, options);
	}

	bool map_renderer::pick_ray(const float3& origin, const float3& direction, pick_result& out, const pick_options& options) const
	{
		return impl_->pick_ray(origin, direction, out, options);
	}

	bool map_renderer::object_box(const object_ref& ref, oriented_box& out) const { return impl_->object_box(ref, out); }
	bool map_renderer::object_triangles(const object_ref& ref, std::vector<float3>& out) const { return impl_->object_triangles(ref, out); }
}
