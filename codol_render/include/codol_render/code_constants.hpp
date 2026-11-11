#pragma once

// The values behind the "code" shader arguments ( CodeVertexConst / CodePixelConst ): per frame, per light and
// per object float4 constants and the 14 code matrix groups. Port of Codol.Rendering\GameCodeConstants.cs and
// Codol.Rendering\GameCamera.cs.
//
// CONVENTIONS ( the shaders of the game were compiled against these, the C# renderer reproduces them ):
//   - Camera relative rendering. The "world" matrix of world geometry is translate( -eye ); view is the camera
//     ROTATION only; so world space positions never reach the shaders, only eye relative ones. Positions written
//     into constants ( lightPosition ) are eye relative as well. A model's world matrix is its placement with
//     the eye subtracted from the translation ( GameMapRenderer.cs:1222-1225 ).
//   - Game axes: X forward, Y left, Z up; angles in degrees ( pitch down positive, yaw counter clockwise ).
//     View space: +X right, +Y up, +Z forward ( left handed, D3D ).
//   - Projection: infinite far plane, z_near 4 by default: clip.z = view.z - z_near, clip.w = view.z.
//   - Matrices are row-major with row vectors ( math.hpp ); a matrix argument uploads `row_count` rows starting
//     at `first_row`, one float4 register per row.
//   - Code matrix index = FIRST_CODE_MATRIX + 4 * group + variant; variant 0 matrix, 1 inverse, 2 transpose,
//     3 inverse transpose.
//
// A `code_constants` object is plain mutable state owned by the caller ( one per renderer ); nothing is global.

#include "build_1_1_1_13.hpp"
#include "math.hpp"
#include "render_state.hpp"

#include <array>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace codol::render
{
	// GameCamera.cs:5-37
	struct camera
	{
		float3 origin;
		float pitch = 0;		// degrees
		float yaw = 0;
		float roll = 0;
		float fov_x = 80.0f;	// horizontal field of view, degrees
		float z_near = 4.0f;

		struct axes_t
		{
			float3 forward, left, up;
		};
		axes_t axes() const;						// GameCamera.cs:7-15
		mat4 view_rotation() const;					// GameCamera.cs:17-26: columns right, up, forward
		mat4 projection(float aspect) const;		// GameCamera.cs:28-37: aspect = width / height
	};

	// GameCodeConstants.cs:320-335
	struct game_fog
	{
		float4 fog_consts;
		float4 color_linear;
		float4 sun_consts{ 0, 2, 0, 0 };
		float4 sun_color_linear;
		float4 sun_dir{ 0, 0, 1, 0 };
		float4 height;
	};

	// The fields of a ComWorld primary light the constants need ( Codol.Assets\Maps\MapData.cs:315-340 MapLight )
	struct primary_light
	{
		int type = 0;				// 1 directional ( sun ), 2 spot, 3 omni
		float3 color;				// as stored ( gamma space )
		float3 direction;
		float3 origin;				// world space
		float radius = 0;
		float cos_half_fov_outer = 0;
		float cos_half_fov_inner = 0;
		std::uint8_t exponent = 0;
	};

	// Codol.Assets\Rendering\RenderLightDef.cs:7 ( GfxLightDef ): what lightFalloffPlacement needs
	struct light_def_info
	{
		int attenuation_width = 0;	// width of the attenuation image
		int lookup_start = 0;		// GfxLightDef::lmapLookupStart
	};

	// Codol.Rendering\GameSpotShadow.cs:7-25
	struct spot_shadow_info
	{
		mat4 lookup;
		float4 pixel_adjust;		// ( 1 / tile, 1 / ( tile * count ), tile, tile * count )
		float fade = 1.0f;
	};

	// Codol.Rendering\GameSunShadow.cs:9-19
	struct sun_shadow_info
	{
		mat4 lookup;
		float4 switch_partition;
		float4 scale;
		float4 pixel_adjust;
	};

	// Codol.Assets\Rendering\VisionSet.cs:11-27, the fields SetFilm / SetGlow read
	struct vision_set
	{
		float glow_bloom_cutoff = 0;
		float glow_bloom_desaturation = 0;
		float glow_bloom_intensity0 = 0;
		bool film_enable = false;
		float film_brightness = 0;
		float film_contrast = 0;
		float film_desaturation = 0;
		float film_desaturation_dark = 0;
		bool film_invert = false;
		float3 film_light_tint{ 1, 1, 1 };
		float3 film_medium_tint{ 1, 1, 1 };
		float3 film_dark_tint{ 1, 1, 1 };
	};

	// GameSpotShadow.cs:27-28 / GameSunShadow.cs:21-22: the shadowmapPolygonOffset constant of the shadow passes
	inline float4 spot_shadow_polygon_offset(const renderer_settings& s)
	{
		return { s.shadow_map_polygon_offset_bias * 0.25f, s.shadow_map_polygon_offset_scale, 0, 0 };
	}
	inline float4 sun_shadow_polygon_offset(const renderer_settings& s, const float partition_depth_scale)
	{
		return { s.shadow_map_polygon_offset_bias * 0.03f * partition_depth_scale, s.shadow_map_polygon_offset_scale * 0.65f, 0, 0 };
	}

	// GameCodeConstants.cs:36-43: the matrix groups the renderer fills. The other six ( world / worldView /
	// worldViewProjection 1 and 2 ) stay identity.
	namespace code_matrix
	{
		inline constexpr int VIEW = 0;
		inline constexpr int PROJECTION = 1;
		inline constexpr int VIEW_PROJECTION = 2;
		inline constexpr int SHADOW_LOOKUP = 3;
		inline constexpr int WORLD_OUTDOOR_LOOKUP = 4;
		inline constexpr int WORLD = 5;
		inline constexpr int WORLD_VIEW = 6;
		inline constexpr int WORLD_VIEW_PROJECTION = 7;
		inline constexpr int GROUP_COUNT = build::CODE_MATRIX_GROUPS;

		inline constexpr int VARIANT_MATRIX = 0;
		inline constexpr int VARIANT_INVERSE = 1;
		inline constexpr int VARIANT_TRANSPOSE = 2;
		inline constexpr int VARIANT_INVERSE_TRANSPOSE = 3;
	}

	// GameCodeConstants.cs:351 ( ConstantHandle ): what a code constant argument resolved to
	struct constant_handle
	{
		int slot = -1;				// index of a float4 slot, -1 for a matrix
		int matrix_group = -1;
		int variant = 0;

		bool is_matrix() const { return slot < 0; }
	};

	class code_constants
	{
	public:
		explicit code_constants(const renderer_settings& settings = {});	// GameCodeConstants.cs:22-31

		// GfxWorld's outdoor lookup matrix ( GameMapRenderer.cs:99-105 ); identity until set
		mat4 outdoor_lookup;
		game_fog fog;

		// ---- per frame / per pass ( GameCodeConstants.cs:45-82, 247-256 ) -------------------------------
		void begin_frame(const camera& cam, int width, int height, float game_time);
		void set_viewport(int target_width, int target_height, int x, int y, int width, int height);
		void begin_shadow_pass(const mat4& view, const mat4& projection);
		void begin_2d(int width, int height);

		// ---- per object ( GameCodeConstants.cs:151-171 ) -------------------------------------------------
		void set_world(const mat4& world);

		// ---- lights and shadows ( GameCodeConstants.cs:84-94, 173-204 ) ----------------------------------
		void set_primary_light(const primary_light* light, const light_def_info* definition = nullptr, const spot_shadow_info* spot_shadow = nullptr);
		void set_sun_shadow(const sun_shadow_info& shadow);
		void set_shadow_lookup(const mat4& lookup);

		// ---- post effects ( GameCodeConstants.cs:108-118, 206-245 ) --------------------------------------
		void set_glow(const vision_set* vision);
		void set_film(const vision_set* vision);

		// ---- direct access --------------------------------------------------------------------------------
		void set(std::string_view name, const float4& value);		// GameCodeConstants.cs:120
		void set_by_index(int index, const float4& value);			// GameCodeConstants.cs:96-104
		int by_index_generation() const { return by_index_generation_; }

		const float3& eye_offset() const { return eye_offset_; }
		const mat4& matrix(const int group) const { return matrices_[static_cast<std::size_t>(group)]; }
		// the value of a named slot; false when it was never set
		bool get(std::string_view name, float4& value) const;

		// ---- what the draw loop binds ( GameCodeConstants.cs:265-317 ) -----------------------------------
		// `name` is the name the shader's constant table gives the register: float4 constants are slots keyed by
		// name unless set_by_index gave the index a value of its own ( the C# renderer's way ).
		constant_handle resolve(int index, std::string_view name);
		// The same with the name of code_tables.hpp ( code_constant_name( index ) ), which needs no constant table.
		// For every float4 constant the renderer fills the two agree ( measured on the stock zones ); they differ only
		// for the HLSL arrays, whose elements share one name in the shaders: resolve( 11 .. 18 ) are the slots
		// "filterTap0" .. "filterTap7", which set( "filterTap3", .. ) fills without set_by_index.
		constant_handle resolve(int index);
		// changes whenever the value behind the handle changes ( for skipping redundant SetShaderConstantF )
		int version_of(const constant_handle& handle) const;
		// identifies the value independently of the register it goes to
		static std::int64_t key_of(const constant_handle& handle);
		// number of registers write() fills
		static int rows_of(const constant_handle& handle, int first_row, int row_count);
		// Writes the value(s) into `output` ( room for 16 floats ); returns the number of float4 rows written.
		// `known` is false for a slot nothing has set yet ( zeros are written ) and for a matrix group out of range.
		int write(const constant_handle& handle, int first_row, int row_count, float* output, bool& known) const;
		int write(int index, std::string_view name, int first_row, int row_count, float* output, bool& known);

		// GameCodeConstants.cs:302-308
		static mat4 variant(const mat4& m, int variant);

	private:
		struct slot
		{
			float4 value;
			bool known = false;
			int version = 0;
			int id = 0;
		};

		int name_slot(std::string_view name);
		void update(int slot_index, const float4& value);
		void set_matrix(int group, const mat4& matrix);
		void update_object_matrices();

		renderer_settings settings_;
		std::vector<slot> slots_;
		std::unordered_map<std::string, int> by_name_;
		std::unordered_map<int, int> by_index_;
		std::array<mat4, code_matrix::GROUP_COUNT> matrices_{};
		std::array<int, code_matrix::GROUP_COUNT> matrix_versions_{};
		int next_version_ = 0;
		int next_slot_id_ = 0;
		int by_index_generation_ = 0;
		float3 eye_offset_;
		float3 view_forward_;
	};
}
