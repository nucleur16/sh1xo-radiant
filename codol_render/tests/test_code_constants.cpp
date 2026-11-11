#include "test.hpp"

#include "codol_render/code_constants.hpp"
#include "codol_render/code_tables.hpp"

using namespace codol::render;

namespace
{
	bool near4(const float4& a, const float4& b, const float tolerance = 1e-5f)
	{
		const bool ok = std::fabs(a.x - b.x) <= tolerance && std::fabs(a.y - b.y) <= tolerance && std::fabs(a.z - b.z) <= tolerance
			&& std::fabs(a.w - b.w) <= tolerance;
		if (!ok)
		{
			std::printf("  got (%.7g, %.7g, %.7g, %.7g) expected (%.7g, %.7g, %.7g, %.7g)\n", a.x, a.y, a.z, a.w, b.x, b.y, b.z, b.w);
		}
		return ok;
	}

	bool near_matrix(const mat4& a, const mat4& b, const float tolerance = 1e-5f)
	{
		for (int r = 0; r < 4; r++)
		{
			if (!near4(a.row(r), b.row(r), tolerance))
			{
				return false;
			}
		}
		return true;
	}

	float4 named(const code_constants& c, const char* name)
	{
		float4 v{ -999, -999, -999, -999 };
		c.get(name, v);
		return v;
	}
}

void test_code_constants()
{
	// ---- math ------------------------------------------------------------------------------------------------
	{
		const mat4 t = mat4::translation({ 1, 2, 3 });
		CHECK(t.m[3][0] == 1 && t.m[3][1] == 2 && t.m[3][2] == 3 && t.m[3][3] == 1);
		CHECK(t.data()[12] == 1 && t.data()[13] == 2 && t.data()[14] == 3);		// row-major: translation in floats 12 .. 14
		// row vectors: a point transforms as p * M, and a * b applies a first
		CHECK(near4(transform({ 1, 1, 1, 1 }, t), { 2, 3, 4, 1 }));
		const mat4 scale = mat4::from_rows({ 2, 0, 0, 0 }, { 0, 2, 0, 0 }, { 0, 0, 2, 0 }, { 0, 0, 0, 1 });
		CHECK(near4(transform({ 1, 1, 1, 1 }, mul(t, scale)), { 4, 6, 8, 1 }));		// translate, then scale
		CHECK(near4(transform({ 1, 1, 1, 1 }, mul(scale, t)), { 3, 4, 5, 1 }));		// scale, then translate
		CHECK(transpose(t).m[0][3] == 1 && transpose(t).m[3][0] == 0);

		const mat4 general = mat4::from_rows({ 2, 1, 0, 3 }, { -1, 4, 2, 0 }, { 0.5f, -2, 3, 1 }, { 7, 0, -3, 5 });
		mat4 inv;
		CHECK(inverse(general, inv));
		CHECK(near_matrix(mul(general, inv), mat4::identity(), 1e-4f));
		CHECK(near_matrix(mul(inv, general), mat4::identity(), 1e-4f));
		mat4 inv_t;
		CHECK(inverse(t, inv_t));
		CHECK(near_matrix(inv_t, mat4::translation({ -1, -2, -3 })));
		const mat4 singular = mat4::from_rows({ 1, 2, 3, 4 }, { 2, 4, 6, 8 }, { 0, 0, 1, 0 }, { 0, 0, 0, 1 });
		mat4 untouched = t;
		CHECK(!inverse(singular, untouched));
		CHECK(untouched == t);

		// GameCodeConstants.cs:263
		CHECK_NEAR(srgb_to_linear(0.02f), 0.02f * 0.07739938f, 1e-9);
		CHECK_NEAR(srgb_to_linear(0.5f), std::pow((0.5f + 0.055f) * 0.9478673f, 2.4f), 1e-7);
		CHECK_NEAR(srgb_to_linear(0.5f), 0.2140411, 1e-5);
		CHECK_NEAR(srgb_to_linear(1.0f), 1.0, 1e-5);

		// MathExtensions.cs:9-16
		CHECK(half_to_float(0x3C00) == 1.0f);
		CHECK(half_to_float(0xC000) == -2.0f);
		CHECK(half_to_float(0x0000) == 0.0f);
		CHECK(half_to_float(0x3800) == 0.5f);
		CHECK(half_to_float(0x7BFF) == 65504.0f);
		CHECK(half_to_float(0x0001) == 5.9604644775390625e-8f);		// smallest subnormal, 2^-24
		CHECK(half_to_float(0x0400) == 6.103515625e-5f);			// smallest normal, 2^-14
		CHECK(std::isinf(half_to_float(0x7C00)) && half_to_float(0xFC00) < 0);
		CHECK(std::isnan(half_to_float(0x7E00)));
		CHECK_NEAR(half_to_float(0x3555), 0.33325195, 1e-8);
		// packed unit vector: scale byte 63 -> ( 63 + 192 ) / 32385 = 1 / 127
		CHECK(near4(float4(unpack_unit_vector((63u << 24) | (127u << 16) | (127u << 8) | 254u), 0), { 1, 0, 0, 0 }, 1e-6f));
		CHECK(near4(float4(unpack_unit_vector((63u << 24) | (0u << 16) | (254u << 8) | 127u), 0), { 0, 1, -1, 0 }, 1e-6f));
		CHECK(near4(float4(unpack_unit_vector(0xFF7F7FFFu), 0), { 128.0f * 447.0f / 32385.0f, 0, 0, 0 }, 1e-6f));
	}

	// ---- camera ( GameCamera.cs ) ---------------------------------------------------------------------------
	{
		camera cam;
		CHECK(cam.fov_x == 80.0f && cam.z_near == 4.0f);
		auto a = cam.axes();
		CHECK(near4(float4(a.forward, 0), { 1, 0, 0, 0 }) && near4(float4(a.left, 0), { 0, 1, 0, 0 }) && near4(float4(a.up, 0), { 0, 0, 1, 0 }));
		// view space: x right, y up, z forward
		CHECK(near_matrix(cam.view_rotation(), mat4::from_rows({ 0, 0, 1, 0 }, { -1, 0, 0, 0 }, { 0, 1, 0, 0 }, { 0, 0, 0, 1 })));
		cam.yaw = 90;
		a = cam.axes();
		CHECK(near4(float4(a.forward, 0), { 0, 1, 0, 0 }) && near4(float4(a.left, 0), { -1, 0, 0, 0 }));
		cam.yaw = 0;
		cam.pitch = 90;		// positive pitch looks down
		a = cam.axes();
		CHECK(near4(float4(a.forward, 0), { 0, 0, -1, 0 }) && near4(float4(a.up, 0), { 1, 0, 0, 0 }));
		cam.pitch = 0;
		cam.roll = 90;
		a = cam.axes();
		CHECK(near4(float4(a.left, 0), { 0, 0, 1, 0 }) && near4(float4(a.up, 0), { 0, -1, 0, 0 }));

		camera wide;
		wide.fov_x = 90;
		// infinite far plane: clip.z = view.z - z_near, clip.w = view.z
		CHECK(near_matrix(wide.projection(2.0f), mat4::from_rows({ 1, 0, 0, 0 }, { 0, 2, 0, 0 }, { 0, 0, 1, 1 }, { 0, 0, -4, 0 })));
		const float4 clip = transform({ 3, 5, 100, 1 }, wide.projection(2.0f));
		CHECK(near4(clip, { 3, 10, 96, 100 }));
	}

	// ---- begin_frame ( GameCodeConstants.cs:45-64 ) ---------------------------------------------------------
	renderer_settings settings;
	code_constants constants(settings);
	camera cam;
	cam.origin = { 10, 20, 30 };
	cam.fov_x = 90;
	constants.begin_frame(cam, 640, 320, 1.25f);

	// camera relative: world = translate( -eye ), view = rotation only
	CHECK(near_matrix(constants.matrix(code_matrix::WORLD), mat4::translation({ -10, -20, -30 })));
	CHECK(near_matrix(constants.matrix(code_matrix::VIEW), cam.view_rotation()));
	CHECK(near_matrix(constants.matrix(code_matrix::PROJECTION), mat4::from_rows({ 1, 0, 0, 0 }, { 0, 2, 0, 0 }, { 0, 0, 1, 1 }, { 0, 0, -4, 0 })));
	CHECK(near_matrix(constants.matrix(code_matrix::VIEW_PROJECTION), mul(cam.view_rotation(), cam.projection(2.0f))));
	CHECK(near_matrix(constants.matrix(code_matrix::WORLD_VIEW), mul(mat4::translation({ -10, -20, -30 }), cam.view_rotation())));
	CHECK(near_matrix(constants.matrix(code_matrix::WORLD_VIEW_PROJECTION),
		mul(mat4::translation({ -10, -20, -30 }), mul(cam.view_rotation(), cam.projection(2.0f)))));
	// a world point 100 units in front of the eye ends up at clip ( 0, 0, 96, 100 )
	CHECK(near4(transform({ 110, 20, 30, 1 }, constants.matrix(code_matrix::WORLD_VIEW_PROJECTION)), { 0, 0, 96, 100 }, 1e-4f));
	// outdoor lookup: world space again ( eye added back ), pushed 32 units against the view direction
	CHECK(near_matrix(constants.matrix(code_matrix::WORLD_OUTDOOR_LOOKUP), mat4::translation({ -32, 0, 0 })));
	CHECK(near_matrix(constants.matrix(code_matrix::SHADOW_LOOKUP), mat4::identity()));
	CHECK(constants.eye_offset() == (float3{ 10, 20, 30 }));

	// gameTime 1.25: ( sin( 2 pi 0.25 ), cos( 2 pi 0.25 ), 0.25, 1.25 mod 43200 )
	CHECK(near4(named(constants, "gameTime"), { 1, 0, 0.25f, 1.25f }, 1e-6f));
	CHECK(near4(named(constants, "materialColor"), { 1, 1, 1, 0 }));
	CHECK(near4(named(constants, "renderTargetSize"), { 640, 320, 1.0f / 640, 1.0f / 320 }, 1e-9f));
	CHECK(near4(named(constants, "clipSpaceLookupScale"), { 0.5f, -0.5f, 0, 1 }));
	CHECK(near4(named(constants, "clipSpaceLookupOffset"), { 0.5f / 640 + 0.5f, 0.5f / 320 + 0.5f, 0, 0 }, 1e-7f));
	CHECK(near4(named(constants, "outdoorFeatherParms"), { 8, 8, 8, 8 }));
	CHECK(near4(named(constants, "glowColorParams0"), { 0, 0, 0, 0 }));
	// GameFog.None
	CHECK(near4(named(constants, "fogConsts"), { 0, 0, 0, 0 }));
	CHECK(near4(named(constants, "fogSunConsts"), { 0, 2, 0, 0 }));
	CHECK(near4(named(constants, "fogSunDir"), { 0, 0, 1, 0 }));
	CHECK(near4(named(constants, "fogColorGamma"), { 0, 0, 0, 0 }));
	float4 unset;
	CHECK(!constants.get("lightPosition", unset));

	// a sub viewport ( GameCodeConstants.cs:66-73 ): target 1024 x 512, viewport 256 x 128 at ( 512, 64 )
	constants.set_viewport(1024, 512, 512, 64, 256, 128);
	CHECK(near4(named(constants, "renderTargetSize"), { 1024, 512, 1.0f / 1024, 1.0f / 512 }, 1e-9f));
	CHECK(near4(named(constants, "clipSpaceLookupScale"), { 0.125f, -0.125f, 0, 1 }));
	CHECK(near4(named(constants, "clipSpaceLookupOffset"), { 0.5f / 1024 + 0.125f + 0.5f, 0.5f / 512 + 0.125f + 0.125f, 0, 0 }, 1e-7f));

	// ---- resolve / write ( GameCodeConstants.cs:265-317 ) ---------------------------------------------------
	{
		float out[16];
		bool known = false;

		// index 99 = FIRST_CODE_MATRIX + 4 * WORLD: three rows from row 1
		const constant_handle world = constants.resolve(build::FIRST_CODE_MATRIX + 4 * code_matrix::WORLD, "worldMatrix");
		CHECK(world.is_matrix() && world.matrix_group == code_matrix::WORLD && world.variant == 0);
		CHECK_EQ(code_constants::rows_of(world, 1, 3), 3);
		CHECK_EQ(constants.write(world, 1, 3, out, known), 3);
		CHECK(known);
		CHECK(out[0] == 0 && out[1] == 1 && out[2] == 0 && out[3] == 0);			// row 1
		CHECK(out[8] == -10 && out[9] == -20 && out[10] == -30 && out[11] == 1);	// row 3: the translation
		// asking for more rows than are left clamps
		CHECK_EQ(code_constants::rows_of(world, 3, 4), 1);
		CHECK_EQ(constants.write(world, 3, 4, out, known), 1);
		CHECK(out[0] == -10 && out[3] == 1);

		// variants: 100 inverse, 101 transpose, 102 inverse transpose
		const constant_handle inv = constants.resolve(100, "inverseWorldMatrix");
		CHECK(inv.matrix_group == code_matrix::WORLD && inv.variant == code_matrix::VARIANT_INVERSE);
		CHECK_EQ(constants.write(inv, 0, 4, out, known), 4);
		CHECK(out[12] == 10 && out[13] == 20 && out[14] == 30 && out[15] == 1);
		CHECK_EQ(constants.write(constants.resolve(101, ""), 0, 4, out, known), 4);
		CHECK(out[3] == -10 && out[7] == -20 && out[11] == -30 && out[12] == 0);
		CHECK_EQ(constants.write(constants.resolve(102, ""), 0, 4, out, known), 4);
		CHECK(out[3] == 10 && out[7] == 20 && out[11] == 30 && out[15] == 1);
		// index 107: worldViewProjection
		const constant_handle wvp = constants.resolve(107, "worldViewProjectionMatrix");
		CHECK(wvp.matrix_group == code_matrix::WORLD_VIEW_PROJECTION && wvp.variant == 0);
		// the last group is 13 ( worldViewProjection2 ); anything past it is not known and writes identity
		const constant_handle last = constants.resolve(build::CODE_CONSTANT_COUNT - 1, "");
		CHECK(last.matrix_group == 13 && last.variant == 3);
		constants.write(last, 0, 4, out, known);
		CHECK(known && out[0] == 1 && out[5] == 1);
		constants.write(constants.resolve(build::CODE_CONSTANT_COUNT, ""), 0, 1, out, known);
		CHECK(!known && out[0] == 1 && out[1] == 0);
		CHECK_EQ(constants.version_of(constants.resolve(build::CODE_CONSTANT_COUNT, "")), 0);

		// float4 constants are slots keyed by the name the shader gives the register
		const constant_handle time = constants.resolve(7, "gameTime");
		CHECK(!time.is_matrix());
		CHECK_EQ(constants.write(time, 0, 1, out, known), 1);
		CHECK(known && out[2] == 0.25f && out[3] == 1.25f);
		CHECK_EQ(code_constants::rows_of(time, 0, 4), 1);
		const constant_handle never = constants.resolve(60, "framebufferRead");
		constants.write(never, 0, 1, out, known);
		CHECK(!known && out[0] == 0 && out[3] == 0);
		CHECK(code_constants::key_of(time) > 0 && code_constants::key_of(world) < 0 && code_constants::key_of(time) != code_constants::key_of(never));
		CHECK(code_constants::key_of(world) != code_constants::key_of(inv));

		// versions change only when the value changes ( GameCodeConstants.cs:133-150 )
		const int v0 = constants.version_of(time);
		constants.set("gameTime", named(constants, "gameTime"));
		CHECK_EQ(constants.version_of(time), v0);
		constants.set("gameTime", { 0, 1, 0, 2 });
		CHECK(constants.version_of(time) > v0);
		const int w0 = constants.version_of(world);
		const int wvp0 = constants.version_of(wvp);
		constants.set_world(mat4::translation({ -10, -20, -30 }));
		CHECK_EQ(constants.version_of(world), w0);
		CHECK_EQ(constants.version_of(wvp), wvp0);
		constants.set_world(mat4::translation({ 5, 0, 0 }));
		CHECK(constants.version_of(world) > w0 && constants.version_of(wvp) > wvp0);
		// the inverse of a matrix shares the group's version
		CHECK_EQ(constants.version_of(inv), constants.version_of(world));

		// set_by_index ( filter taps: GameMapRenderer.PostEffects.cs:169 ) takes an index away from its name
		const int generation = constants.by_index_generation();
		constants.set("filterTap0", { 9, 9, 9, 9 });
		constants.set_by_index(11, { 1, 2, 3, 4 });
		CHECK_EQ(constants.by_index_generation(), generation + 1);
		constants.write(11, "filterTap0", 0, 1, out, known);
		CHECK(known && out[0] == 1 && out[3] == 4);
		constants.set_by_index(11, { 5, 6, 7, 8 });
		CHECK_EQ(constants.by_index_generation(), generation + 1);
		constants.write(11, "filterTap0", 0, 1, out, known);
		CHECK(out[0] == 5);
		constants.write(12, "filterTap0", 0, 1, out, known);		// another index with that name still reads the named slot
		CHECK(out[0] == 9);
	}

	// ---- lights ( GameCodeConstants.cs:173-204 ) ------------------------------------------------------------
	{
		code_constants c(settings);
		camera eye;
		eye.origin = { 100, 0, 0 };
		c.begin_frame(eye, 100, 100, 0);

		primary_light sun;
		sun.type = 1;
		sun.direction = { 0, 0.6f, 0.8f };
		sun.color = { 1.0f, 0.5f, 0.02f };
		c.set_primary_light(&sun);
		CHECK(near4(named(c, "lightPosition"), { 0, 0.6f, 0.8f, 0 }));
		// diffuse scale 1, specular scale 2.5, both through the gamma decode
		CHECK(near4(named(c, "lightDiffuse"), { srgb_to_linear(1.0f), srgb_to_linear(0.5f), 0.02f * 0.07739938f, 1 }));
		CHECK(near4(named(c, "lightSpecular"), { srgb_to_linear(2.5f), srgb_to_linear(1.25f), srgb_to_linear(0.05f), 1 }));
		float4 none;
		CHECK(!c.get("lightSpotDir", none));

		primary_light spot;
		spot.type = 2;
		spot.origin = { 150, 20, 30 };
		spot.direction = { 0, 0, -1 };
		spot.radius = 200;
		spot.cos_half_fov_inner = 0.9f;
		spot.cos_half_fov_outer = 0.7f;
		spot.exponent = 3;
		spot.color = { 1, 1, 1 };
		const light_def_info def{ 256, 3 };
		c.set_primary_light(&spot, &def);
		CHECK(near4(named(c, "lightPosition"), { 50, 20, 30, 1.0f / 200 }));		// eye relative, 1 / radius
		CHECK(near4(named(c, "lightSpotDir"), { 0, 0, -1, 0 }));
		const float scale = 1.0f / (0.9f - 0.7f);
		CHECK(near4(named(c, "lightSpotFactors"), { scale, -scale * 0.7f, 3, 0 }, 1e-4f));
		CHECK(near4(named(c, "lightFalloffPlacement"), { 256 * 0.001953125f, 0, 3 * 0.001953125f, 0 }));

		// a shadowed omni light: spot factors are written too, with the shadow's fade; the lookup is made eye relative
		primary_light omni = spot;
		omni.type = 3;
		spot_shadow_info shadow;
		shadow.lookup = mat4::from_rows({ 2, 0, 0, 0 }, { 0, 2, 0, 0 }, { 0, 0, 2, 0 }, { 1, 1, 1, 1 });
		shadow.pixel_adjust = { 0.25f, 0.0625f, 4, 16 };
		shadow.fade = 0.5f;
		c.set_primary_light(&omni, nullptr, &shadow);
		CHECK(near4(named(c, "lightSpotFactors"), { scale, -scale * 0.7f, 3, 0.5f }, 1e-4f));
		CHECK(near4(named(c, "spotShadowmapPixelAdjust"), { 0.25f, 0.0625f, 4, 16 }));
		CHECK(near4(named(c, "lightFalloffPlacement"), { 0, 0, 0, 0 }));
		CHECK(near_matrix(c.matrix(code_matrix::SHADOW_LOOKUP), mul(mat4::translation({ 100, 0, 0 }), shadow.lookup)));
		CHECK(near4(c.matrix(code_matrix::SHADOW_LOOKUP).row(3), { 201, 1, 1, 1 }));

		c.set_primary_light(nullptr);
		CHECK(near4(named(c, "lightPosition"), { 0, 0, 0, 0 }) && near4(named(c, "lightDiffuse"), { 0, 0, 0, 0 })
			&& near4(named(c, "lightSpecular"), { 0, 0, 0, 0 }));

		sun_shadow_info sun_shadow;
		sun_shadow.lookup = mat4::identity();
		sun_shadow.switch_partition = { 1, 2, 3, 4 };
		sun_shadow.scale = { 5, 6, 7, 8 };
		sun_shadow.pixel_adjust = { 9, 10, 11, 12 };
		c.set_sun_shadow(sun_shadow);
		CHECK(near4(named(c, "shadowmapSwitchPartition"), { 1, 2, 3, 4 }) && near4(named(c, "shadowmapScale"), { 5, 6, 7, 8 })
			&& near4(named(c, "sunShadowmapPixelAdjust"), { 9, 10, 11, 12 }));
		CHECK(near_matrix(c.matrix(code_matrix::SHADOW_LOOKUP), mat4::translation({ 100, 0, 0 })));

		// shadow passes are drawn in world space ( GameCodeConstants.cs:75-82 )
		const mat4 view = mat4::translation({ 1, 2, 3 });
		const mat4 projection = mat4::from_rows({ 0.5f, 0, 0, 0 }, { 0, 0.5f, 0, 0 }, { 0, 0, 0.25f, 0 }, { 0, 0, 0, 1 });
		c.begin_shadow_pass(view, projection);
		CHECK(c.eye_offset() == float3{});
		CHECK(near_matrix(c.matrix(code_matrix::WORLD), mat4::identity()));
		CHECK(near_matrix(c.matrix(code_matrix::WORLD_VIEW_PROJECTION), mul(view, projection)));

		CHECK(near4(spot_shadow_polygon_offset(settings), { 1, 1.5f, 0, 0 }));
		CHECK(near4(sun_shadow_polygon_offset(settings, 2.0f), { 4 * 0.03f * 2, 1.5f * 0.65f, 0, 0 }));
	}

	// ---- film / glow / 2D ( GameCodeConstants.cs:108-118, 206-256 ) -----------------------------------------
	{
		code_constants c(settings);
		c.set_film(nullptr);
		// no vision set: d = 2^-12, contrast 1, brightness 0 -> bias 0, 1 / d - 1 = 4095
		CHECK(near4(named(c, "colorBias"), { 0, 0, 0, 4095 }));
		CHECK(near4(named(c, "colorTintBase"), { 1, 1, 1, 0 }));
		CHECK(near4(named(c, "colorTintDelta"), { 0, 0, 0, 0 }));
		CHECK(near4(named(c, "colorTintQuadraticDelta"), { 0, 0, 0, 0 }));

		vision_set vision;
		vision.film_enable = true;
		vision.film_brightness = 0.1f;
		vision.film_contrast = 1.5f;
		vision.film_desaturation = 0.5f;
		vision.film_desaturation_dark = 0.25f;
		vision.film_dark_tint = { 1, 0.5f, 0.25f };
		vision.film_medium_tint = { 1, 1, 1 };
		vision.film_light_tint = { 2, 1, 1 };
		c.set_film(&vision);
		// desaturation 0.5 * ( ( 1 - 0.5 ) * 1 + 0.5 ) = 0.5; b = 0.1 + 0.5 - 0.75 = -0.15; 1 / 0.5 - 1 = 1
		CHECK(near4(named(c, "colorBias"), { -0.15f, -0.15f, -0.15f, 1 }, 1e-6f));
		CHECK(near4(named(c, "colorTintBase"), { 1.5f, 0.75f, 0.375f, 0.25f }));
		CHECK(near4(named(c, "colorTintDelta"), { 0, 1.5f, 2.25f, 0.25f }));					// ( medium * 2 - dark * 2 ) * c
		CHECK(near4(named(c, "colorTintQuadraticDelta"), { 1.5f, -0.75f, -1.125f, 0 }));	// ( dark - 2 medium + light ) * c
		vision.film_invert = true;
		c.set_film(&vision);
		CHECK(near4(named(c, "colorBias"), { 0.85f, 0.85f, 0.85f, 1 }, 1e-6f));
		CHECK(near4(named(c, "colorTintBase"), { -1.5f, -0.75f, -0.375f, 0.25f }));

		renderer_settings linear = settings;
		linear.film_quadratic_tint = false;
		code_constants l(linear);
		vision.film_invert = false;
		l.set_film(&vision);
		// c = d * contrast = 0.75
		CHECK(near4(named(l, "colorTintBase"), { 0.75f, 0.375f, 0.1875f, 0 }));
		CHECK(near4(named(l, "colorTintDelta"), { 0.75f, 0.375f, 0.5625f, 0 }));			// ( light - dark ) * c
		CHECK(near4(named(l, "colorTintQuadraticDelta"), { 0, 0, 0, 0 }));

		float4 v;
		c.set_glow(nullptr);
		CHECK(!c.get("glowSetup", v));
		vision.glow_bloom_intensity0 = 0;
		c.set_glow(&vision);
		CHECK(!c.get("glowSetup", v));
		vision.glow_bloom_intensity0 = 0.75f;
		vision.glow_bloom_cutoff = 0.5f;
		vision.glow_bloom_desaturation = 0.3f;
		c.set_glow(&vision);
		const float cutoff = srgb_to_linear(0.5f);
		CHECK(near4(named(c, "glowSetup"), { cutoff, 1.0f / (1.0f - cutoff), 0, 0.3f }));
		CHECK(near4(named(c, "glowApply"), { 0, 0, 0, 0.75f }));

		c.begin_2d(200, 100);
		// pixel ( 0, 0 ) -> clip ( -1 - 1/200, 1 + 1/100 ), pixel ( 200, 100 ) -> ( 1 - 1/200, -1 + 1/100 ), z 0, w 1
		const mat4& wvp = c.matrix(code_matrix::WORLD_VIEW_PROJECTION);
		CHECK(near4(transform({ 0, 0, 0, 1 }, wvp), { -1.005f, 1.01f, 1, 1 }, 1e-6f));
		CHECK(near4(transform({ 200, 100, 0, 1 }, wvp), { 0.995f, -0.99f, 1, 1 }, 1e-6f));
		CHECK(near4(named(c, "renderTargetSize"), { 200, 100, 0.005f, 0.01f }, 1e-8f));
	}

	// ---- name tables ----------------------------------------------------------------------------------------
	{
		for (int i = 0; i < build::CODE_CONSTANT_COUNT; i++)
		{
			const code_name* c = code_constant(i);
			CHECK(c && c->name && c->name[0] && c->source && c->source[0]);
			CHECK_EQ(code_constant_index(c ? c->name : ""), i);		// every name is unique
		}
		CHECK(code_constant(-1) == nullptr && code_constant(build::CODE_CONSTANT_COUNT) == nullptr);
		CHECK_EQ(build::CODE_CONSTANT_COUNT, 135);
		// anchors of Iw5ShaderTables.cs / CodeConstantMap.cs
		CHECK_EQ(code_constant_index("lightPosition"), 0);
		CHECK_EQ(code_constant_index("gameTime"), 7);
		CHECK_EQ(code_constant_index("ssaoParms"), 8);
		CHECK_EQ(code_constant_index("filterTap0"), 11);
		CHECK_EQ(code_constant_index("renderTargetSize"), 23);
		CHECK_EQ(code_constant_index("renderSourceSize"), 24);
		CHECK_EQ(code_constant_index("materialColor"), 38);
		CHECK_EQ(code_constant_index("fogSunDir"), 45);
		CHECK_EQ(code_constant_index("glowSetup"), 46);
		CHECK_EQ(code_constant_index("colorBias"), 48);
		CHECK_EQ(code_constant_index("spotShadowmapPixelAdjust"), 55);
		CHECK_EQ(code_constant_index("overlayTexCoord"), 56);
		CHECK_EQ(code_constant_index("viewportDimensions"), 59);
		CHECK_EQ(code_constant_index("baseLightingCoords"), 61);
		CHECK_EQ(code_constant_index("clipSpaceLookupOffset"), 67);
		CHECK_EQ(code_constant_index("depthFromClip"), 76);
		CHECK_EQ(code_constant_index("codeMeshArg1"), 78);
		CHECK_EQ(code_constant_index("viewMatrix"), build::FIRST_CODE_MATRIX);
		CHECK_EQ(code_constant_index("worldMatrix"), build::FIRST_CODE_MATRIX + 4 * code_matrix::WORLD);
		CHECK_EQ(code_constant_index("inverseTransposeWorldViewProjectionMatrix"), build::FIRST_CODE_MATRIX + 4 * code_matrix::WORLD_VIEW_PROJECTION + 3);
		CHECK_EQ(code_constant_index("nope"), -1);

		for (int i = 0; i < build::CODE_SAMPLER_COUNT; i++)
		{
			const code_name* s = code_sampler(i);
			CHECK(s && s->name && s->name[0]);
		}
		CHECK(code_sampler(build::CODE_SAMPLER_COUNT) == nullptr);
		CHECK(std::strcmp(code_sampler_name(code_sampler_index::SHADOWMAP_SUN), "shadowmapSamplerSun") == 0);
		CHECK(std::strcmp(code_sampler_name(code_sampler_index::LIGHT_ATTENUATION), "attenuationSampler") == 0);
		CHECK(std::strcmp(code_sampler_name(code_sampler_index::FLOATZ), "floatZSampler") == 0);

		// GameMapRenderer.cs:1689-1721
		std::uint8_t state = 0;
		CHECK(code_sampler_state(10, "whatever", 0, state) && state == 0x62);
		CHECK(code_sampler_state(3, "modelLightingSampler", 0, state) && state == 0xE2);
		CHECK(code_sampler_state(13, "attenuationSampler", 0x7A, state) && state == 0x7A);
		CHECK(code_sampler_state(6, "shadowmapSamplerSun", 0, state) && state == 0x61);
		CHECK(code_sampler_state(7, "shadowmapSamplerSpot", 0, state) && state == 0x61);
		CHECK(code_sampler_state(14, "outdoorMapSampler", 0, state) && state == 0x62);
		CHECK(code_sampler_state(15, "floatZSampler", 0, state) && state == 0x61);
		CHECK(!code_sampler_state(20, "caseTexture", 0, state) && state == 0x02);

		// GameMapRenderer.cs:1723-1738
		CHECK(CUSTOM_SAMPLERS[0].flag == 1 && CUSTOM_SAMPLERS[0].sampler_register == 1 && CUSTOM_SAMPLERS[0].sampler_state == 0xF2);
		CHECK(CUSTOM_SAMPLERS[1].flag == 2 && CUSTOM_SAMPLERS[1].sampler_register == 2 && CUSTOM_SAMPLERS[1].sampler_state == 0x62);
		CHECK(CUSTOM_SAMPLERS[2].flag == 4 && CUSTOM_SAMPLERS[2].sampler_register == 3 && CUSTOM_SAMPLERS[2].sampler_state == 0x62);
	}
}
