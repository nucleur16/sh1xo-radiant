// What a map editor needs on top of drawing the shipped map: camera math, hiding and moving objects, placing new
// models, lighting them where they stand, the model / material catalogs and material thumbnails.
// Not in the C# renderer ( the AssetExplorer only views maps ).

#include "renderer_impl.hpp"

#include <algorithm>
#include <cfloat>

namespace codol::view
{
	namespace
	{
		constexpr float PI = 3.14159265358979323846f;
		// ground lighting of a model nothing is known about: mid grey, full sun ( LightGrid.cs:232-246 packing )
		constexpr std::uint32_t NEUTRAL_GROUND_LIGHTING = 0xFF606060;
		constexpr int ZONE_IDLE_FRAMES = 120;

		void copy_matrix(const mat4& m, float out[16])
		{
			std::memcpy(out, m.data(), 16 * sizeof(float));
		}

		float3 normalised(const float3& v)
		{
			const float len = length(v);
			return len > 0 ? v * (1.0f / len) : v;
		}

		// the colour map of a material: the texture with the colour semantic, else its first image
		const render::material_texture* color_map_of(const render::material& asset)
		{
			const render::material_texture* color = asset.find_by_semantic(render::texture_semantic::COLOR_MAP);
			if (color && color->has_image && !color->image_name.empty())
			{
				return color;
			}
			for (const auto& texture : asset.textures)
			{
				if (texture.has_image && !texture.image_name.empty() && texture.semantic != render::texture_semantic::WATER_MAP)
				{
					return &texture;
				}
			}
			return nullptr;
		}
	}

	// ---------------------------------------------------------------------------------------------------
	// camera math ( the public free functions )

	void view_matrix(const camera& cam, float out[16])
	{
		// render_frame: world = translate( -eye ), view = the rotation
		copy_matrix(render::mul(mat4::translation(-cam.origin), cam.view_rotation()), out);
	}

	void projection_matrix(const camera& cam, const int width, const int height, float out[16])
	{
		copy_matrix(cam.projection(static_cast<float>(width) / static_cast<float>(height > 0 ? height : 1)), out);
	}

	void screen_ray(const camera& cam, const int width, const int height, const float px, const float py, float3& origin, float3& direction)
	{
		// Direct3D 9: clip x = -1 is the centre of pixel 0, clip x = +1 the centre of pixel `width`
		const float aspect = static_cast<float>(width) / static_cast<float>(height > 0 ? height : 1);
		const float tan_x = std::tan(cam.fov_x * PI / 360.0f);
		const float tan_y = tan_x / aspect;
		const float x = (2.0f * px / static_cast<float>(width > 0 ? width : 1) - 1.0f) * tan_x;
		const float y = (1.0f - 2.0f * py / static_cast<float>(height > 0 ? height : 1)) * tan_y;
		const auto axes = cam.axes();
		origin = cam.origin;
		direction = normalised(axes.forward - axes.left * x + axes.up * y);
	}

	bool project(const camera& cam, const int width, const int height, const float3& world, float screen_xy[2], float& depth)
	{
		const mat4 view_projection = render::mul(cam.view_rotation(), cam.projection(static_cast<float>(width) / static_cast<float>(height > 0 ? height : 1)));
		const float4 clip = render::transform(float4(world - cam.origin, 1.0f), view_projection);
		if (clip.w <= cam.z_near)
		{
			return false;
		}
		screen_xy[0] = (clip.x / clip.w + 1.0f) * 0.5f * static_cast<float>(width);
		screen_xy[1] = (1.0f - clip.y / clip.w) * 0.5f * static_cast<float>(height);
		depth = clip.z / clip.w;
		return true;
	}

	// ---------------------------------------------------------------------------------------------------
	// placements

	mat4 map_renderer::impl::placement_of(const transform& t)
	{
		// GameMapRenderer.cs:367-435 ( ReadEntityModels ): rows = the entity's axes times the scale, then the origin
		camera orientation;
		orientation.pitch = t.angles.x;
		orientation.yaw = t.angles.y;
		orientation.roll = t.angles.z;
		const auto axes = orientation.axes();
		const float scale = t.scale;
		return mat4::from_rows(
			{ axes.forward.x * scale, axes.forward.y * scale, axes.forward.z * scale, 0 },
			{ axes.left.x * scale, axes.left.y * scale, axes.left.z * scale, 0 },
			{ axes.up.x * scale, axes.up.y * scale, axes.up.z * scale, 0 },
			{ t.origin.x, t.origin.y, t.origin.z, 1 });
	}

	transform map_renderer::impl::transform_of(const mat4& placement, const float scale)
	{
		// the inverse of camera::axes(): forward = ( cp cy, cp sy, -sp ), left.z = sr cp, up.z = cr cp
		transform t;
		t.origin = { placement.m[3][0], placement.m[3][1], placement.m[3][2] };
		const float3 row0{ placement.m[0][0], placement.m[0][1], placement.m[0][2] };
		const float3 row1{ placement.m[1][0], placement.m[1][1], placement.m[1][2] };
		const float3 row2{ placement.m[2][0], placement.m[2][1], placement.m[2][2] };
		t.scale = scale > 0 ? scale : length(row0);
		const float3 forward = normalised(row0), left = normalised(row1), up = normalised(row2);
		const float flat = std::sqrt(forward.x * forward.x + forward.y * forward.y);
		const float degrees = 180.0f / PI;
		t.angles.x = std::atan2(-forward.z, flat) * degrees;
		if (flat > 1.0e-5f)
		{
			t.angles.y = std::atan2(forward.y, forward.x) * degrees;
			t.angles.z = std::atan2(left.z, up.z) * degrees;
		}
		else
		{
			// looking straight up or down: yaw and roll turn about the same axis, all of it goes to the yaw
			t.angles.y = std::atan2(-left.x, left.y) * degrees;
			t.angles.z = 0;
		}
		return t;
	}

	void map_renderer::impl::placed_box(const float3& min, const float3& max, const mat4& placement, float3& center, float3& half)
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
	}

	float3 map_renderer::impl::to_local(const float3& v, const mat4& placement, const bool point)
	{
		// the inverse of a placement whose rows are orthogonal axes of one length ( rotation and uniform scale )
		const float3 d = point ? v - float3{ placement.m[3][0], placement.m[3][1], placement.m[3][2] } : v;
		float out[3];
		for (int r = 0; r < 3; r++)
		{
			const float3 row{ placement.m[r][0], placement.m[r][1], placement.m[r][2] };
			const float squared = dot(row, row);
			out[r] = squared > 0 ? dot(d, row) / squared : 0.0f;
		}
		return { out[0], out[1], out[2] };
	}

	// ---------------------------------------------------------------------------------------------------
	// objects

	static_model_instance* map_renderer::impl::find_model(const object_ref& ref)
	{
		switch (ref.kind)
		{
		case object_kind::static_model:
			return ref.id >= 0 && ref.id < static_count && ref.id < static_cast<int>(static_models.size()) ? &static_models[static_cast<std::size_t>(ref.id)] : nullptr;
		case object_kind::entity_model:
			if (const auto it = entity_models.find(ref.id); it != entity_models.end())
			{
				return &static_models[static_cast<std::size_t>(it->second)];
			}
			return nullptr;
		case object_kind::placed_model:
			if (const auto it = placed_models.find(ref.id); it != placed_models.end())
			{
				return it->second.get();
			}
			return nullptr;
		default:
			return nullptr;
		}
	}

	brush_model_instance* map_renderer::impl::find_brush(const object_ref& ref)
	{
		if (ref.kind != object_kind::brush_model)
		{
			return nullptr;
		}
		const auto it = entity_brushes.find(ref.id);
		return it == entity_brushes.end() ? nullptr : &brush_models[static_cast<std::size_t>(it->second)];
	}

	dynamic_mesh* map_renderer::impl::find_mesh(const int handle)
	{
		const auto it = meshes.find(handle);
		return it == meshes.end() ? nullptr : it->second.get();
	}

	bool* map_renderer::impl::hidden_flag(const object_ref& ref)
	{
		switch (ref.kind)
		{
		case object_kind::world_surface:
			if (ref.id >= 0 && ref.id < static_cast<int>(surfaces.size()) && !surfaces[static_cast<std::size_t>(ref.id)].brush_model)
			{
				return &surfaces[static_cast<std::size_t>(ref.id)].hidden;
			}
			return nullptr;
		case object_kind::brush_model:
			if (brush_model_instance* brush = find_brush(ref))
			{
				return &brush->hidden;
			}
			return nullptr;
		case object_kind::dynamic_mesh:
			if (dynamic_mesh* mesh = find_mesh(ref.id))
			{
				return &mesh->hidden;
			}
			return nullptr;
		default:
			if (static_model_instance* model = find_model(ref))
			{
				return &model->hidden;
			}
			return nullptr;
		}
	}

	bool map_renderer::impl::set_hidden(const object_ref& ref, const bool hidden)
	{
		bool* flag = map_loaded ? hidden_flag(ref) : nullptr;
		if (!flag)
		{
			return false;
		}
		if (*flag != hidden)
		{
			hidden_count += hidden ? 1 : -1;
			*flag = hidden;
		}
		return true;
	}

	void map_renderer::impl::fill_object(const world_surface& s, scene_object& o) const
	{
		o.kind = object_kind::world_surface;
		o.id = s.index;
		o.center = s.center;
		o.half_size = s.half;
		o.name = s.material ? s.material->asset().name : std::string();
		o.drawn = static_cast<std::size_t>(s.index) < surface_visible.size() && surface_visible[static_cast<std::size_t>(s.index)] != 0;
		o.hidden = s.hidden;
	}

	void map_renderer::impl::fill_object(const static_model_instance& m, scene_object& o) const
	{
		o.kind = m.handle > 0 ? object_kind::placed_model : m.is_entity ? object_kind::entity_model : object_kind::static_model;
		o.id = m.handle > 0 ? m.handle : m.is_entity ? m.entity_index : m.static_index;
		o.entity = m.entity_index;
		o.center = m.center;
		o.half_size = m.half;
		o.name = m.draw ? m.draw->name : std::string();
		o.drawn = m.drawn;
		o.hidden = m.hidden;
		o.moved = m.moved;
	}

	void map_renderer::impl::fill_object(const brush_model_instance& b, scene_object& o) const
	{
		o.kind = object_kind::brush_model;
		o.id = b.entity_index;
		o.entity = b.entity_index;
		o.center = b.center;
		o.half_size = b.half;
		o.name = "*" + std::to_string(b.model);
		o.drawn = b.drawn;
		o.hidden = b.hidden;
		o.moved = b.moved;
	}

	void map_renderer::impl::fill_object(const dynamic_mesh& m, scene_object& o) const
	{
		o.kind = object_kind::dynamic_mesh;
		o.id = m.handle;
		o.center = m.center;
		o.half_size = m.half;
		o.name = m.material_name;
		o.drawn = m.drawn;
		o.hidden = m.hidden;
	}

	bool map_renderer::impl::object(const object_ref& ref, scene_object& out) const
	{
		if (!map_loaded)
		{
			return false;
		}
		out = {};
		switch (ref.kind)
		{
		case object_kind::world_surface:
			if (ref.id < 0 || ref.id >= static_cast<int>(surfaces.size()) || surfaces[static_cast<std::size_t>(ref.id)].brush_model)
			{
				return false;
			}
			fill_object(surfaces[static_cast<std::size_t>(ref.id)], out);
			return true;
		case object_kind::brush_model:
			if (const brush_model_instance* brush = find_brush(ref))
			{
				fill_object(*brush, out);
				return true;
			}
			return false;
		case object_kind::dynamic_mesh:
			if (const dynamic_mesh* mesh = find_mesh(ref.id))
			{
				fill_object(*mesh, out);
				return true;
			}
			return false;
		default:
			if (const static_model_instance* model = find_model(ref))
			{
				fill_object(*model, out);
				return true;
			}
			return false;
		}
	}

	// ---------------------------------------------------------------------------------------------------
	// lighting of a model where it stands ( GameMapRenderer.cs:1057-1126 BuildModelLighting, for one instance )

	int map_renderer::impl::allocate_lighting_handle()
	{
		int handle;
		if (!free_lighting_handles.empty())
		{
			handle = free_lighting_handles.back();
			free_lighting_handles.pop_back();
		}
		else
		{
			handle = ++lighting_handles;
		}
		if (!lighting_volume)
		{
			lighting_volume = std::make_unique<model_lighting_volume>(std::max(handle, 64));
		}
		else if (handle > lighting_volume->capacity())
		{
			lighting_volume->grow(std::max(handle, 2 * lighting_volume->capacity()));
		}
		return handle;
	}

	void map_renderer::impl::release_lighting_handle(const int handle)
	{
		if (handle > 0)
		{
			free_lighting_handles.push_back(handle);
		}
	}

	void map_renderer::impl::set_lighting_block(const int handle, const std::span<const std::uint32_t> block)
	{
		if (!lighting_volume || handle <= 0)
		{
			return;
		}
		lighting_volume->set_block(handle, block);
		if (!upload_model_lighting())
		{
			warn("the model lighting volume texture could not be updated");
		}
	}

	std::uint32_t map_renderer::impl::sample_ground_lighting(const float3& point, const int model_light, int* light, std::array<std::uint32_t, 64>* block)
	{
		if (grid)
		{
			light_grid::blocked_fn blocked;
			if (trace)
			{
				blocked = [this](const float3 start, const float3 end) { return trace->blocked(start, end); };
			}
			const auto sample = grid->sample(point, model_light, lights, last_sun_light, blocked);
			const auto colors = grid->blend(sample);
			if (block)
			{
				light_grid::write_block(colors, sample.sun_visibility, *block);
			}
			if (light)
			{
				*light = sample.primary_light;
			}
			return light_grid::ground_lighting(colors, sample.sun_visibility);
		}
		// no light grid kept ( options::keep_light_grid ): a neutral ambient under the sun
		const std::uint32_t c = NEUTRAL_GROUND_LIGHTING;
		if (block)
		{
			block->fill(((c >> 24) << 24) | ((c & 0xFF) << 16) | (((c >> 8) & 0xFF) << 8) | ((c >> 16) & 0xFF));
		}
		if (light)
		{
			*light = sun ? sun->index : model_light;
		}
		return c;
	}

	void map_renderer::impl::light_instance(static_model_instance& instance, const int model_light)
	{
		// as load_map lights an entity model: nearest reflection probe, light grid at the lighting origin
		instance.reflection_probe = nearest_probe(instance.lighting_origin);
		const bool volume = instance.draw && instance.draw->uses_lighting_volume;
		std::array<std::uint32_t, 64> block{};
		int light = model_light;
		const std::uint32_t ground = sample_ground_lighting(instance.lighting_origin, model_light, &light, &block);
		instance.ground_lighting = ground;
		instance.primary_light = light;
		if (volume)
		{
			if (instance.lighting_handle == 0)
			{
				instance.lighting_handle = allocate_lighting_handle();
			}
			set_lighting_block(instance.lighting_handle, block);
		}
	}

	void map_renderer::impl::place_model(static_model_instance& instance, const mat4& placement, const float scale)
	{
		instance.placement = placement;
		instance.scale = scale;
		if (instance.draw)
		{
			// as for a script_model entity ( GameMapRenderer.cs:367-435 )
			instance.center = transform_point((instance.draw->min + instance.draw->max) * 0.5f, placement);
			float3 unused;
			placed_box(instance.draw->min, instance.draw->max, placement, unused, instance.half);
			instance.radius = length(instance.draw->max - instance.draw->min) * 0.5f * scale;
		}
		else
		{
			instance.center = { placement.m[3][0], placement.m[3][1], placement.m[3][2] };
		}
	}

	// ---------------------------------------------------------------------------------------------------
	// transform overrides

	bool map_renderer::impl::set_transform(const object_ref& ref, const transform& t)
	{
		if (!map_loaded || !(t.scale > 0))
		{
			return false;
		}
		const mat4 placement = placement_of(t);
		if (brush_model_instance* brush = find_brush(ref))
		{
			const int index = static_cast<int>(brush - brush_models.data());
			if (!brush_backups.count(index))
			{
				instance_backup backup;
				backup.placement = brush->placement;
				backup.center = brush->center;
				backup.half = brush->half;
				backup.radius = brush->radius;
				backup.scale = brush->scale;
				brush_backups.emplace(index, backup);
			}
			brush->placement = placement;
			brush->scale = t.scale;
			placed_box(brush->local_min, brush->local_max, placement, brush->center, brush->half);
			brush->radius = length(brush->half);
			brush->moved = true;
			return true;
		}
		static_model_instance* model = find_model(ref);
		if (!model)
		{
			return false;
		}
		if (model->handle == 0)
		{
			const int index = static_cast<int>(model - static_models.data());
			if (!model_backups.count(index))
			{
				instance_backup backup;
				backup.placement = model->placement;
				backup.center = model->center;
				backup.half = model->half;
				backup.radius = model->radius;
				backup.scale = model->scale;
				backup.lighting_origin = model->lighting_origin;
				backup.reflection_probe = model->reflection_probe;
				backup.primary_light = model->primary_light;
				backup.ground_lighting = model->ground_lighting;
				if (lighting_volume && model->lighting_handle > 0)
				{
					lighting_volume->get_block(model->lighting_handle, backup.block);
				}
				model_backups.emplace(index, backup);
			}
			model->moved = true;
		}
		// the lighting origin keeps its place relative to the model
		const float3 local_lighting = to_local(model->lighting_origin, model->placement, true);
		place_model(*model, placement, t.scale);
		model->lighting_origin = transform_point(local_lighting, placement);
		if (grid)
		{
			light_instance(*model, 0);
		}
		else
		{
			model->reflection_probe = nearest_probe(model->lighting_origin);
		}
		return true;
	}

	bool map_renderer::impl::get_transform(const object_ref& ref, transform& out) const
	{
		if (!map_loaded)
		{
			return false;
		}
		if (const brush_model_instance* brush = find_brush(ref))
		{
			out = transform_of(brush->placement, brush->scale);
			return true;
		}
		if (const static_model_instance* model = find_model(ref))
		{
			out = transform_of(model->placement, model->scale);
			return true;
		}
		return false;
	}

	bool map_renderer::impl::reset_transform(const object_ref& ref)
	{
		if (!map_loaded)
		{
			return false;
		}
		if (brush_model_instance* brush = find_brush(ref))
		{
			const auto it = brush_backups.find(static_cast<int>(brush - brush_models.data()));
			if (it != brush_backups.end())
			{
				brush->placement = it->second.placement;
				brush->center = it->second.center;
				brush->half = it->second.half;
				brush->radius = it->second.radius;
				brush->scale = it->second.scale;
				brush->moved = false;
				brush_backups.erase(it);
			}
			return true;
		}
		static_model_instance* model = find_model(ref);
		if (!model || model->handle > 0)
		{
			return false;
		}
		const auto it = model_backups.find(static_cast<int>(model - static_models.data()));
		if (it != model_backups.end())
		{
			const instance_backup& backup = it->second;
			model->placement = backup.placement;
			model->center = backup.center;
			model->half = backup.half;
			model->radius = backup.radius;
			model->scale = backup.scale;
			model->lighting_origin = backup.lighting_origin;
			model->reflection_probe = backup.reflection_probe;
			model->primary_light = backup.primary_light;
			model->ground_lighting = backup.ground_lighting;
			if (model->lighting_handle > 0)
			{
				set_lighting_block(model->lighting_handle, backup.block);
			}
			model->moved = false;
			model_backups.erase(it);
		}
		return true;
	}

	// ---------------------------------------------------------------------------------------------------
	// new model instances

	int map_renderer::impl::add_model(const std::string& name, const transform& t, const int handle, std::string* error)
	{
		const auto fail = [&](std::string text)
		{
			last_error = text;
			render::set_error(error, std::move(text));
			return 0;
		};
		if (!map_loaded || device_lost)
		{
			return fail("no map is loaded");
		}
		if (!(t.scale > 0))
		{
			return fail("the scale of a model must be greater than 0");
		}
		model_draw* draw = resolve_model(name);
		if (!draw)
		{
			return fail("model " + name + " is not in the map, level, shared or extra zones");
		}
		if (draw->surfaces().empty())
		{
			return fail("model " + name + " has no surfaces ( LOD 0 )");
		}
		auto instance = std::make_unique<static_model_instance>();
		instance->draw = draw;
		instance->handle = handle > 0 ? handle : ++next_handle;
		next_handle = std::max(next_handle, instance->handle);
		place_model(*instance, placement_of(t), t.scale);
		instance->lighting_origin = instance->center;
		light_instance(*instance, 0);
		for (const auto& lod : draw->lods)
		{
			for (const auto& surface : lod.surfaces)
			{
				if (surface.material)
				{
					prepare_technique(*surface.material, choose_technique(*surface.material, instance->primary_light, false), render::vertex_format::PACKED);
				}
			}
		}
		const int result = instance->handle;
		placed_models[result] = std::move(instance);
		return result;
	}

	bool map_renderer::impl::remove_model(const int handle)
	{
		const auto it = placed_models.find(handle);
		if (it == placed_models.end())
		{
			return false;
		}
		if (it->second->hidden)
		{
			hidden_count--;
		}
		release_lighting_handle(it->second->lighting_handle);
		opaque_models.clear();			// they point at instances
		transparent_models.clear();
		placed_models.erase(it);
		return true;
	}

	void map_renderer::impl::release_idle_zone()
	{
		if (held_zone && ++zone_idle_frames > ZONE_IDLE_FRAMES)
		{
			held_zone.reset();
		}
	}

	// ---------------------------------------------------------------------------------------------------
	// catalogs

	void map_renderer::impl::build_name_lists()
	{
		if (model_names_built && material_names_built)
		{
			return;
		}
		name_set seen_models, seen_materials;
		model_name_list.clear();
		material_name_list.clear();
		for (const auto& zone : scope.zones)
		{
			for (const auto& model : zone->models)
			{
				if (seen_models.insert(model.first).second)
				{
					model_name_list.push_back(model.second);
				}
			}
			for (const auto& material : zone->materials)
			{
				if (seen_materials.insert(material.first).second)
				{
					material_name_list.push_back(material.second->asset.name);
				}
			}
		}
		std::sort(model_name_list.begin(), model_name_list.end());
		std::sort(material_name_list.begin(), material_name_list.end());
		model_names_built = material_names_built = true;
	}

	bool map_renderer::impl::material_details(const std::string& name, material_info& out)
	{
		const std::shared_ptr<const library_material> found = map_loaded ? scope.find_material(name) : nullptr;
		if (!found)
		{
			return false;
		}
		const render::material& asset = found->asset;
		out = {};
		out.name = asset.name;
		out.technique_set = asset.technique_set_name;
		out.surface_type_bits = asset.surface_type_bits;
		out.game_flags = asset.game_flags;
		out.sort_key = asset.sort_key;
		out.camera_region = asset.camera_region;
		out.zone = found->zone_name;
		std::shared_ptr<const library_techset> techset = found->techset;
		if (!techset && !asset.technique_set_name.empty())
		{
			techset = scope.find_techset(asset.technique_set_name);
		}
		if (techset)
		{
			namespace rt = render::renderer_technique;
			for (const int type : { rt::LIT, rt::LIT - 1, rt::LIT_SUN, rt::LIT_SUN - 1, rt::EMISSIVE, rt::UNLIT })
			{
				const render::render_technique* technique = techset->set.technique(type);
				if (!technique)
				{
					continue;
				}
				for (const auto& pass : technique->passes)
				{
					out.world_material = out.world_material || (pass.custom_sampler_flags & 6) != 0;
				}
				out.blends = asset.state && asset.state->blends(type);
				if (const render::material_state::bits* bits = asset.state ? asset.state->pass_bits(type, 0) : nullptr)
				{
					// render_state.cpp translate_state_bits
					out.alpha_tested = (bits->bits0 & 0x800) == 0;
					out.writes_depth = (bits->bits1 & 1) != 0;
				}
				break;
			}
		}
		if (const render::material_texture* color = color_map_of(asset))
		{
			out.color_map = color->image_name;
			out.width = color->width;
			out.height = color->height;
			if (out.width <= 0 || out.height <= 0)
			{
				// a streamed image: the zone only has its name, the size is in the IWI file
				auto it = image_sizes.find(color->image_name);
				if (it == image_sizes.end())
				{
					std::pair<int, int> size{ 0, 0 };
					std::vector<std::uint8_t> file;
					render::iwi_header header;
					if (images.read(color->image_name, file, nullptr) && render::parse_iwi_header(file, header, nullptr))
					{
						size = { header.width, header.height };
					}
					it = image_sizes.emplace(color->image_name, size).first;
				}
				out.width = it->second.first;
				out.height = it->second.second;
			}
		}
		return true;
	}

	IDirect3DTexture9* map_renderer::impl::material_thumbnail(const std::string& name, const int max_size)
	{
		if (!map_loaded || device_lost)
		{
			return nullptr;
		}
		if (const auto it = thumbnails.find(name); it != thumbnails.end())
		{
			return it->second.get();
		}
		com_ptr<IDirect3DTexture9>& slot = thumbnails[name];		// stays null when anything below fails
		const std::shared_ptr<const library_material> found = scope.find_material(name);
		const render::material_texture* color = found ? color_map_of(found->asset) : nullptr;
		if (!color)
		{
			return nullptr;
		}
		std::string error;
		render::texture_upload upload;
		std::vector<std::uint8_t> file;
		render::iwi_file iwi;
		bool ok;
		if (color->embedded)
		{
			const auto holder = acquire_zone(found->zone_name);
			ok = holder && render::build_embedded_upload(*color->embedded, holder->bytes(color->embedded->data, color->embedded->data_size), upload, &error);
		}
		else
		{
			ok = images.read(color->image_name, file, &error) && render::parse_iwi(file, iwi, &error) && render::build_texture_upload(iwi, wavelet.get(), upload, &error);
		}
		if (!ok || upload.is_cube())
		{
			return nullptr;
		}
		int skip = 0;
		while (skip + 1 < upload.levels && (std::max(upload.width, upload.height) >> skip) > std::max(max_size, 1))
		{
			skip++;
		}
		drop_top_mips(upload, skip);
		gpu_texture texture;
		if (!create_texture(color->image_name + "#thumbnail", upload, texture, &error))
		{
			return nullptr;
		}
		IDirect3DTexture9* flat = nullptr;
		if (FAILED(texture.handle->QueryInterface(__uuidof(IDirect3DTexture9), reinterpret_cast<void**>(&flat))) || !flat)
		{
			return nullptr;
		}
		*slot.put() = flat;		// QueryInterface added the reference the slot now owns
		return flat;
	}

	// ---------------------------------------------------------------------------------------------------
	// the edits as data: a default pool renderer loads the map again after a device reset

	edit_snapshot map_renderer::impl::capture_edits() const
	{
		edit_snapshot edits;
		edits.valid = true;
		edits.next_handle = next_handle;
		for (const world_surface& s : surfaces)
		{
			if (s.hidden)
			{
				edits.hidden.push_back({ object_kind::world_surface, s.index });
			}
		}
		const auto ref_of = [](const static_model_instance& m) -> object_ref
		{
			return { m.handle > 0 ? object_kind::placed_model : m.is_entity ? object_kind::entity_model : object_kind::static_model,
				m.handle > 0 ? m.handle : m.is_entity ? m.entity_index : m.static_index };
		};
		for (const static_model_instance& m : static_models)
		{
			if (m.hidden)
			{
				edits.hidden.push_back(ref_of(m));
			}
			if (m.moved)
			{
				edits.moved.emplace_back(ref_of(m), transform_of(m.placement, m.scale));
			}
		}
		for (const brush_model_instance& b : brush_models)
		{
			if (b.hidden)
			{
				edits.hidden.push_back({ object_kind::brush_model, b.entity_index });
			}
			if (b.moved)
			{
				edits.moved.emplace_back(object_ref{ object_kind::brush_model, b.entity_index }, transform_of(b.placement, b.scale));
			}
		}
		for (const auto& placed : placed_models)
		{
			const static_model_instance& m = *placed.second;
			edits.models.push_back({ m.handle, m.draw ? m.draw->name : std::string(), transform_of(m.placement, m.scale) });
			if (m.hidden)
			{
				edits.hidden.push_back(ref_of(m));
			}
		}
		for (const auto& entry : meshes)
		{
			const dynamic_mesh& m = *entry.second;
			edits.meshes.push_back({ m.handle, m.vertices, m.indices, m.material_name, m.fallback_color, m.two_sided });
			if (m.hidden)
			{
				edits.hidden.push_back({ object_kind::dynamic_mesh, m.handle });
			}
		}
		return edits;
	}

	void map_renderer::impl::apply_edits(const edit_snapshot& edits)
	{
		if (!edits.valid || !map_loaded)
		{
			return;
		}
		for (const auto& model : edits.models)
		{
			add_model(model.name, model.placement, model.handle, nullptr);
		}
		for (const auto& mesh : edits.meshes)
		{
			mesh_data data;
			data.vertices = mesh.vertices.data();
			data.vertex_count = mesh.vertices.size();
			data.indices = mesh.indices.data();
			data.index_count = mesh.indices.size();
			data.material = mesh.material;
			data.fallback_color = mesh.color;
			data.two_sided = mesh.two_sided;
			create_mesh(data, mesh.handle, nullptr);
		}
		for (const auto& moved : edits.moved)
		{
			set_transform(moved.first, moved.second);
		}
		for (const object_ref& ref : edits.hidden)
		{
			set_hidden(ref, true);
		}
		next_handle = std::max(next_handle, edits.next_handle);
		copy_edit_stats(stat);
	}

	void map_renderer::impl::copy_edit_stats(statistics& to) const
	{
		to.placed_model_count = static_cast<int>(placed_models.size());
		to.mesh_count = static_cast<int>(meshes.size());
		to.hidden_objects = hidden_count;
		to.moved_objects = static_cast<int>(model_backups.size() + brush_backups.size());
		to.pick_bytes = pick_bytes;
		to.pick_triangles = pick_triangles;
		to.pick_build_ms = pick_build_ms;
		to.light_grid_bytes = light_grid_bytes;
		// models and materials the editor made the renderer load after load_map
		to.distinct_models = static_cast<int>(models.size());
		to.materials = static_cast<int>(materials.size());
	}

	// ---------------------------------------------------------------------------------------------------
	// the public face

	bool map_renderer::object(const object_ref& ref, scene_object& out) const { return impl_->object(ref, out); }
	bool map_renderer::set_hidden(const object_ref& ref, const bool hidden) { return impl_->set_hidden(ref, hidden); }

	bool map_renderer::is_hidden(const object_ref& ref) const
	{
		const bool* flag = impl_->map_loaded ? impl_->hidden_flag(ref) : nullptr;
		return flag && *flag;
	}

	void map_renderer::show_all()
	{
		impl& i = *impl_;
		for (world_surface& s : i.surfaces)
		{
			s.hidden = false;
		}
		for (static_model_instance& m : i.static_models)
		{
			m.hidden = false;
		}
		for (brush_model_instance& b : i.brush_models)
		{
			b.hidden = false;
		}
		for (auto& placed : i.placed_models)
		{
			placed.second->hidden = false;
		}
		for (auto& mesh : i.meshes)
		{
			mesh.second->hidden = false;
		}
		i.hidden_count = 0;
	}

	bool map_renderer::set_transform(const object_ref& ref, const transform& placement)
	{
		try
		{
			return impl_->set_transform(ref, placement);
		}
		catch (const std::exception& e)
		{
			impl_->last_error = std::string("set_transform: ") + e.what();
			return false;
		}
	}

	bool map_renderer::get_transform(const object_ref& ref, transform& out) const { return impl_->get_transform(ref, out); }

	bool map_renderer::reset_transform(const object_ref& ref)
	{
		try
		{
			return impl_->reset_transform(ref);
		}
		catch (const std::exception& e)
		{
			impl_->last_error = std::string("reset_transform: ") + e.what();
			return false;
		}
	}

	int map_renderer::add_model(const std::string& xmodel_name, const transform& placement, std::string* error)
	{
		try
		{
			return impl_->add_model(xmodel_name, placement, 0, error);
		}
		catch (const std::exception& e)
		{
			impl_->last_error = std::string("add_model: ") + e.what();
			render::set_error(error, impl_->last_error);
			return 0;
		}
	}

	bool map_renderer::remove_model(const int handle) { return impl_->remove_model(handle); }

	bool map_renderer::model_bounds(const std::string& xmodel_name, float3& mins, float3& maxs)
	{
		try
		{
			impl& i = *impl_;
			const model_draw* draw = i.map_loaded && !i.device_lost ? i.resolve_model(xmodel_name) : nullptr;
			if (!draw || draw->surfaces().empty())
			{
				return false;
			}
			mins = draw->min;
			maxs = draw->max;
			return true;
		}
		catch (const std::exception& e)
		{
			impl_->last_error = std::string("model_bounds: ") + e.what();
			return false;
		}
	}

	const std::vector<std::string>& map_renderer::model_names() const
	{
		impl_->build_name_lists();
		return impl_->model_name_list;
	}

	const std::vector<std::string>& map_renderer::material_names() const
	{
		impl_->build_name_lists();
		return impl_->material_name_list;
	}

	void map_renderer::release_zone_cache() { impl_->held_zone.reset(); }

	bool map_renderer::material_details(const std::string& name, material_info& out)
	{
		try
		{
			return impl_->material_details(name, out);
		}
		catch (const std::exception& e)
		{
			impl_->last_error = std::string("material_details: ") + e.what();
			return false;
		}
	}

	IDirect3DTexture9* map_renderer::material_thumbnail(const std::string& name, const int max_size)
	{
		try
		{
			return impl_->material_thumbnail(name, max_size);
		}
		catch (const std::exception& e)
		{
			impl_->last_error = std::string("material_thumbnail: ") + e.what();
			return nullptr;
		}
	}
}
