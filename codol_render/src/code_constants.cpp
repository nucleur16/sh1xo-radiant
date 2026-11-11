#include "codol_render/code_constants.hpp"

#include <algorithm>
#include <bit>
#include <cmath>

namespace codol::render
{
	namespace
	{
		constexpr float PI = 3.14159265358979323846f;
		constexpr float TAU = 6.28318530717958647692f;
	}

	// ---------------------------------------------------------------------------------------------------
	// math.hpp

	bool inverse(const mat4& a, mat4& out)
	{
		const auto& m = a.m;
		const float a0 = m[0][0] * m[1][1] - m[0][1] * m[1][0];
		const float a1 = m[0][0] * m[1][2] - m[0][2] * m[1][0];
		const float a2 = m[0][0] * m[1][3] - m[0][3] * m[1][0];
		const float a3 = m[0][1] * m[1][2] - m[0][2] * m[1][1];
		const float a4 = m[0][1] * m[1][3] - m[0][3] * m[1][1];
		const float a5 = m[0][2] * m[1][3] - m[0][3] * m[1][2];
		const float b0 = m[2][0] * m[3][1] - m[2][1] * m[3][0];
		const float b1 = m[2][0] * m[3][2] - m[2][2] * m[3][0];
		const float b2 = m[2][0] * m[3][3] - m[2][3] * m[3][0];
		const float b3 = m[2][1] * m[3][2] - m[2][2] * m[3][1];
		const float b4 = m[2][1] * m[3][3] - m[2][3] * m[3][1];
		const float b5 = m[2][2] * m[3][3] - m[2][3] * m[3][2];
		const float det = a0 * b5 - a1 * b4 + a2 * b3 + a3 * b2 - a4 * b1 + a5 * b0;
		if (!(std::fabs(det) >= 1.401298464e-45f) || !std::isfinite(det))	// System.Numerics: |det| < float.Epsilon
		{
			return false;
		}
		const float inv = 1.0f / det;
		mat4 r;
		r.m[0][0] = (+m[1][1] * b5 - m[1][2] * b4 + m[1][3] * b3) * inv;
		r.m[0][1] = (-m[0][1] * b5 + m[0][2] * b4 - m[0][3] * b3) * inv;
		r.m[0][2] = (+m[3][1] * a5 - m[3][2] * a4 + m[3][3] * a3) * inv;
		r.m[0][3] = (-m[2][1] * a5 + m[2][2] * a4 - m[2][3] * a3) * inv;
		r.m[1][0] = (-m[1][0] * b5 + m[1][2] * b2 - m[1][3] * b1) * inv;
		r.m[1][1] = (+m[0][0] * b5 - m[0][2] * b2 + m[0][3] * b1) * inv;
		r.m[1][2] = (-m[3][0] * a5 + m[3][2] * a2 - m[3][3] * a1) * inv;
		r.m[1][3] = (+m[2][0] * a5 - m[2][2] * a2 + m[2][3] * a1) * inv;
		r.m[2][0] = (+m[1][0] * b4 - m[1][1] * b2 + m[1][3] * b0) * inv;
		r.m[2][1] = (-m[0][0] * b4 + m[0][1] * b2 - m[0][3] * b0) * inv;
		r.m[2][2] = (+m[3][0] * a4 - m[3][1] * a2 + m[3][3] * a0) * inv;
		r.m[2][3] = (-m[2][0] * a4 + m[2][1] * a2 - m[2][3] * a0) * inv;
		r.m[3][0] = (-m[1][0] * b3 + m[1][1] * b1 - m[1][2] * b0) * inv;
		r.m[3][1] = (+m[0][0] * b3 - m[0][1] * b1 + m[0][2] * b0) * inv;
		r.m[3][2] = (-m[3][0] * a3 + m[3][1] * a1 - m[3][2] * a0) * inv;
		r.m[3][3] = (+m[2][0] * a3 - m[2][1] * a1 + m[2][2] * a0) * inv;
		out = r;
		return true;
	}

	float half_to_float(const std::uint16_t bits)
	{
		// MathExtensions.cs:9 ( BitConverter.UInt16BitsToHalf ): IEEE 754 binary16
		const std::uint32_t sign = static_cast<std::uint32_t>(bits & 0x8000) << 16;
		const std::uint32_t exponent = (bits >> 10) & 0x1F;
		std::uint32_t mantissa = bits & 0x3FF;
		std::uint32_t result;
		if (exponent == 0)
		{
			if (mantissa == 0)
			{
				result = sign;
			}
			else
			{
				// subnormal: normalise
				int e = -1;
				do
				{
					e++;
					mantissa <<= 1;
				} while ((mantissa & 0x400) == 0);
				result = sign | (static_cast<std::uint32_t>(127 - 15 - e) << 23) | ((mantissa & 0x3FF) << 13);
			}
		}
		else if (exponent == 31)
		{
			result = sign | 0x7F800000u | (mantissa << 13);
		}
		else
		{
			result = sign | ((exponent + (127 - 15)) << 23) | (mantissa << 13);
		}
		return std::bit_cast<float>(result);
	}

	// ---------------------------------------------------------------------------------------------------
	// camera ( GameCamera.cs )

	camera::axes_t camera::axes() const
	{
		const float p = pitch * PI / 180.0f, y = yaw * PI / 180.0f, r = roll * PI / 180.0f;
		const float sp = std::sin(p), cp = std::cos(p), sy = std::sin(y), cy = std::cos(y), sr = std::sin(r), cr = std::cos(r);
		const float3 forward{ cp * cy, cp * sy, -sp };
		const float3 right{ -sr * sp * cy + cr * sy, -sr * sp * sy - cr * cy, -sr * cp };
		const float3 up{ cr * sp * cy + sr * sy, cr * sp * sy - sr * cy, cr * cp };
		return { forward, -right, up };
	}

	mat4 camera::view_rotation() const
	{
		const auto a = axes();
		const float3 right = -a.left;
		return mat4::from_rows(
			{ right.x, a.up.x, a.forward.x, 0 },
			{ right.y, a.up.y, a.forward.y, 0 },
			{ right.z, a.up.z, a.forward.z, 0 },
			{ 0, 0, 0, 1 });
	}

	mat4 camera::projection(const float aspect) const
	{
		const float tan_x = std::tan(fov_x * PI / 360.0f);
		const float tan_y = tan_x / aspect;
		return mat4::from_rows(
			{ 1.0f / tan_x, 0, 0, 0 },
			{ 0, 1.0f / tan_y, 0, 0 },
			{ 0, 0, 1, 1 },
			{ 0, 0, -z_near, 0 });
	}

	// ---------------------------------------------------------------------------------------------------
	// code_constants ( GameCodeConstants.cs )

	code_constants::code_constants(const renderer_settings& settings) : settings_(settings)
	{
		// GameCodeConstants.cs:22-31: GameFog.None ( the member defaults ) and identity matrices, each with a version
		for (int i = 0; i < code_matrix::GROUP_COUNT; i++)
		{
			set_matrix(i, mat4::identity());
		}
	}

	void code_constants::begin_frame(const camera& cam, const int width, const int height, const float game_time)
	{
		// GameCodeConstants.cs:45-64
		eye_offset_ = cam.origin;
		view_forward_ = cam.axes().forward;
		const mat4 view = cam.view_rotation();
		const mat4 projection = cam.projection(static_cast<float>(width) / static_cast<float>(height));
		set_world(mat4::translation(-eye_offset_));
		set_matrix(code_matrix::VIEW, view);
		set_matrix(code_matrix::PROJECTION, projection);
		set_matrix(code_matrix::VIEW_PROJECTION, mul(view, projection));
		update_object_matrices();

		const float frac = game_time - std::floor(game_time);
		set("gameTime", { std::sin(frac * TAU), std::cos(frac * TAU), frac, std::fmod(game_time, 43200.0f) });
		set("materialColor", { 1, 1, 1, 0 });
		set_viewport(width, height, 0, 0, width, height);
		set("glowColorParams0", {});
		set("outdoorFeatherParms", float4(settings_.outdoor_feather));
		// GameCodeConstants.cs:324-334 ( GameFog.Write ): the gamma variants get the linear colours
		set("fogConsts", fog.fog_consts);
		set("fogColorLinear", fog.color_linear);
		set("fogColorGamma", fog.color_linear);
		set("fogSunConsts", fog.sun_consts);
		set("fogSunColorLinear", fog.sun_color_linear);
		set("fogSunColorGamma", fog.sun_color_linear);
		set("fogSunDir", fog.sun_dir);
		set("fogHeight", fog.height);
	}

	void code_constants::set_viewport(const int target_width, const int target_height, const int x, const int y, const int width, const int height)
	{
		// GameCodeConstants.cs:66-73
		const float inverse_width = 1.0f / static_cast<float>(target_width), inverse_height = 1.0f / static_cast<float>(target_height);
		const float half_width = static_cast<float>(width) * inverse_width * 0.5f, half_height = static_cast<float>(height) * inverse_height * 0.5f;
		set("renderTargetSize", { static_cast<float>(target_width), static_cast<float>(target_height), inverse_width, inverse_height });
		set("clipSpaceLookupScale", { half_width, -half_height, 0, 1 });
		set("clipSpaceLookupOffset", { inverse_width * 0.5f + half_width + static_cast<float>(x) * inverse_width,
			inverse_height * 0.5f + half_height + static_cast<float>(y) * inverse_height, 0, 0 });
	}

	void code_constants::begin_shadow_pass(const mat4& view, const mat4& projection)
	{
		// GameCodeConstants.cs:75-82: shadow maps are rendered in world space ( no eye offset )
		eye_offset_ = {};
		set_matrix(code_matrix::VIEW, view);
		set_matrix(code_matrix::PROJECTION, projection);
		set_matrix(code_matrix::VIEW_PROJECTION, mul(view, projection));
		set_world(mat4::identity());
	}

	void code_constants::begin_2d(const int width, const int height)
	{
		// GameCodeConstants.cs:247-256: pixel coordinates -> clip space, with the D3D9 half pixel offset
		const float iw = 1.0f / static_cast<float>(width), ih = 1.0f / static_cast<float>(height);
		set_matrix(code_matrix::VIEW, mat4::identity());
		set_matrix(code_matrix::PROJECTION, mat4::from_rows({ iw + iw, 0, 0, 0 }, { 0, -2.0f * ih, 0, 0 }, { 0, 0, 0, 0 }, { -1.0f - iw, ih + 1.0f, 1, 1 }));
		set_matrix(code_matrix::VIEW_PROJECTION, matrices_[code_matrix::PROJECTION]);
		set_matrix(code_matrix::WORLD, mat4::identity());
		update_object_matrices();
		set_viewport(width, height, 0, 0, width, height);
	}

	void code_constants::set_world(const mat4& world)
	{
		// GameCodeConstants.cs:151-155
		set_matrix(code_matrix::WORLD, world);
		update_object_matrices();
	}

	void code_constants::update_object_matrices()
	{
		// GameCodeConstants.cs:157-171
		set_matrix(code_matrix::WORLD_VIEW, mul(matrices_[code_matrix::WORLD], matrices_[code_matrix::VIEW]));
		mat4 world = matrices_[code_matrix::WORLD];
		world.m[3][0] += eye_offset_.x;
		world.m[3][1] += eye_offset_.y;
		world.m[3][2] += eye_offset_.z;
		mat4 outdoor = mul(world, outdoor_lookup);
		const float4 toward = transform(float4(view_forward_ * -32.0f, 0.0f), outdoor_lookup);
		outdoor.m[3][0] += toward.x;
		outdoor.m[3][1] += toward.y;
		outdoor.m[3][2] += toward.z;
		outdoor.m[3][3] += toward.w;
		set_matrix(code_matrix::WORLD_OUTDOOR_LOOKUP, outdoor);
		set_matrix(code_matrix::WORLD_VIEW_PROJECTION, mul(matrices_[code_matrix::WORLD], matrices_[code_matrix::VIEW_PROJECTION]));
	}

	void code_constants::set_sun_shadow(const sun_shadow_info& shadow)
	{
		// GameCodeConstants.cs:86-92
		set_shadow_lookup(shadow.lookup);
		set("shadowmapSwitchPartition", shadow.switch_partition);
		set("shadowmapScale", shadow.scale);
		set("sunShadowmapPixelAdjust", shadow.pixel_adjust);
	}

	void code_constants::set_shadow_lookup(const mat4& lookup)
	{
		// GameCodeConstants.cs:94: the lookup takes world positions, the shaders have eye relative ones
		set_matrix(code_matrix::SHADOW_LOOKUP, mul(mat4::translation(eye_offset_), lookup));
	}

	void code_constants::set_primary_light(const primary_light* light, const light_def_info* definition, const spot_shadow_info* spot_shadow)
	{
		// GameCodeConstants.cs:173-204
		if (!light)
		{
			set("lightPosition", {});
			set("lightDiffuse", {});
			set("lightSpecular", {});
			return;
		}
		if (light->type == 1)
		{
			set("lightPosition", float4(light->direction, 0.0f));
		}
		else
		{
			set("lightPosition", float4(light->origin - eye_offset_, light->radius > 0 ? 1.0f / light->radius : 0.0f));
			set("lightSpotDir", float4(light->direction, 0.0f));
			if (light->type == 2 || spot_shadow)
			{
				const float scale = 1.0f / (light->cos_half_fov_inner - light->cos_half_fov_outer);
				set("lightSpotFactors", { scale, -scale * light->cos_half_fov_outer, static_cast<float>(light->exponent), spot_shadow ? spot_shadow->fade : 0.0f });
			}
			if (spot_shadow)
			{
				set_shadow_lookup(spot_shadow->lookup);
				set("spotShadowmapPixelAdjust", spot_shadow->pixel_adjust);
			}
			set("lightFalloffPlacement", !definition ? float4{}
				: float4{ static_cast<float>(definition->attenuation_width) * 0.001953125f, 0, static_cast<float>(definition->lookup_start) * 0.001953125f, 0 });
		}
		const auto linear = [](const float3& c, const float scale) -> float4
		{
			return { srgb_to_linear(c.x * scale), srgb_to_linear(c.y * scale), srgb_to_linear(c.z * scale), 1.0f };
		};
		set("lightDiffuse", linear(light->color, settings_.diffuse_color_scale));
		set("lightSpecular", linear(light->color, settings_.specular_color_scale));
	}

	void code_constants::set_glow(const vision_set* vision)
	{
		// GameCodeConstants.cs:108-118
		if (!vision || vision->glow_bloom_intensity0 == 0.0f)
		{
			return;
		}
		const float cutoff = srgb_to_linear(vision->glow_bloom_cutoff);
		set("glowSetup", { cutoff, 1.0f / (1.0f - cutoff), 0, vision->glow_bloom_desaturation });
		set("glowApply", { 0, 0, 0, vision->glow_bloom_intensity0 });
	}

	void code_constants::set_film(const vision_set* vision)
	{
		// GameCodeConstants.cs:206-245
		const bool enabled = vision && vision->film_enable;
		float brightness = 0, contrast = 1, desaturation = 0, desaturation_dark = 0;
		bool invert = false;
		float3 dark{ 1, 1, 1 }, medium{ 1, 1, 1 }, light{ 1, 1, 1 };
		if (enabled)
		{
			brightness = vision->film_brightness + settings_.film_brightness_offset;
			contrast = vision->film_contrast * settings_.film_contrast_scale;
			desaturation = vision->film_desaturation;
			desaturation *= (1.0f - desaturation) * settings_.film_desaturation_scale + desaturation;
			desaturation_dark = vision->film_desaturation_dark;
			invert = vision->film_invert;
			dark = vision->film_dark_tint;
			medium = vision->film_medium_tint;
			light = vision->film_light_tint;
		}
		const float d = std::max(desaturation, 0.000244140625f);
		float c = settings_.film_quadratic_tint ? contrast : d * contrast;
		float b = brightness + 0.5f - 0.5f * contrast;
		if (invert)
		{
			c = -c;
			b += 1.0f;
		}
		set("colorBias", { b, b, b, 1.0f / d - 1.0f });
		if (settings_.film_quadratic_tint)
		{
			set("colorTintBase", float4(dark * c, desaturation_dark));
			set("colorTintDelta", float4((medium * 2.0f - dark * 2.0f) * c, desaturation - desaturation_dark));
			set("colorTintQuadraticDelta", float4((dark - medium * 2.0f + light) * c, 0.0f));
		}
		else
		{
			set("colorTintBase", float4(dark * c, 0.0f));
			set("colorTintDelta", float4((light - dark) * c, 0.0f));
			set("colorTintQuadraticDelta", {});
		}
	}

	int code_constants::name_slot(const std::string_view name)
	{
		// GameCodeConstants.cs:122-129
		const auto found = by_name_.find(std::string(name));
		if (found != by_name_.end())
		{
			return found->second;
		}
		const int index = static_cast<int>(slots_.size());
		slots_.push_back({});
		slots_.back().id = ++next_slot_id_;
		by_name_.emplace(std::string(name), index);
		return index;
	}

	void code_constants::update(const int slot_index, const float4& value)
	{
		// GameCodeConstants.cs:133-141
		slot& s = slots_[static_cast<std::size_t>(slot_index)];
		if (!s.known || !(s.value == value))
		{
			s.value = value;
			s.known = true;
			s.version = ++next_version_;
		}
	}

	void code_constants::set(const std::string_view name, const float4& value)
	{
		update(name_slot(name), value);
	}

	void code_constants::set_by_index(const int index, const float4& value)
	{
		// GameCodeConstants.cs:96-104
		auto found = by_index_.find(index);
		if (found == by_index_.end())
		{
			const int slot_index = static_cast<int>(slots_.size());
			slots_.push_back({});
			slots_.back().id = ++next_slot_id_;
			found = by_index_.emplace(index, slot_index).first;
			by_index_generation_++;
		}
		update(found->second, value);
	}

	bool code_constants::get(const std::string_view name, float4& value) const
	{
		const auto found = by_name_.find(std::string(name));
		if (found == by_name_.end() || !slots_[static_cast<std::size_t>(found->second)].known)
		{
			return false;
		}
		value = slots_[static_cast<std::size_t>(found->second)].value;
		return true;
	}

	void code_constants::set_matrix(const int group, const mat4& matrix)
	{
		// GameCodeConstants.cs:143-150
		const auto g = static_cast<std::size_t>(group);
		if (matrix_versions_[g] == 0 || !(matrices_[g] == matrix))
		{
			matrices_[g] = matrix;
			matrix_versions_[g] = ++next_version_;
		}
	}

	constant_handle code_constants::resolve(const int index, const std::string_view name)
	{
		// GameCodeConstants.cs:265-272
		if (index >= build::FIRST_CODE_MATRIX)
		{
			return { -1, (index - build::FIRST_CODE_MATRIX) / 4, (index - build::FIRST_CODE_MATRIX) % 4 };
		}
		const auto found = by_index_.find(index);
		return { found != by_index_.end() ? found->second : name_slot(name), -1, 0 };
	}

	int code_constants::version_of(const constant_handle& handle) const
	{
		// GameCodeConstants.cs:274-275
		if (!handle.is_matrix())
		{
			return slots_[static_cast<std::size_t>(handle.slot)].version;
		}
		return handle.matrix_group >= 0 && handle.matrix_group < code_matrix::GROUP_COUNT ? matrix_versions_[static_cast<std::size_t>(handle.matrix_group)] : 0;
	}

	std::int64_t code_constants::key_of(const constant_handle& handle)
	{
		// GameCodeConstants.cs:277: slot ids are positive, matrices negative. ( The slot index + 1 is the slot id. )
		return !handle.is_matrix() ? static_cast<std::int64_t>(handle.slot) + 1 : -(static_cast<std::int64_t>(handle.matrix_group) * 4 + handle.variant + 1);
	}

	int code_constants::rows_of(const constant_handle& handle, const int first_row, const int row_count)
	{
		// GameCodeConstants.cs:279-280
		return handle.is_matrix() ? std::max(std::min(row_count, 4 - first_row), 1) : 1;
	}

	mat4 code_constants::variant(const mat4& m, const int variant)
	{
		// GameCodeConstants.cs:302-308
		mat4 inv;
		switch (variant)
		{
		case 1: return inverse(m, inv) ? inv : mat4::identity();
		case 2: return transpose(m);
		case 3: return inverse(m, inv) ? transpose(inv) : mat4::identity();
		default: return m;
		}
	}

	int code_constants::write(const constant_handle& handle, const int first_row, const int row_count, float* output, bool& known) const
	{
		// GameCodeConstants.cs:282-300
		if (handle.is_matrix())
		{
			known = handle.matrix_group >= 0 && handle.matrix_group < code_matrix::GROUP_COUNT;
			const mat4 matrix = known ? variant(matrices_[static_cast<std::size_t>(handle.matrix_group)], handle.variant) : mat4::identity();
			const int rows = std::min(row_count, 4 - first_row);
			for (int r = 0; r < rows; r++)
			{
				// GameCodeConstants.cs:310-316 ( Row ): any row index past 2 reads the last row
				const float4 row = matrix.row(std::clamp(first_row + r, 0, 3));
				output[4 * r] = row.x;
				output[4 * r + 1] = row.y;
				output[4 * r + 2] = row.z;
				output[4 * r + 3] = row.w;
			}
			return std::max(rows, 1);
		}
		const slot& s = slots_[static_cast<std::size_t>(handle.slot)];
		known = s.known;
		output[0] = s.value.x;
		output[1] = s.value.y;
		output[2] = s.value.z;
		output[3] = s.value.w;
		return 1;
	}

	int code_constants::write(const int index, const std::string_view name, const int first_row, const int row_count, float* output, bool& known)
	{
		return write(resolve(index, name), first_row, row_count, output, known);
	}
}
