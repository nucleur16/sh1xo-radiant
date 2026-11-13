#pragma once

// The GfxWorld light grid ( model lighting ): a faithful port of Codol.Assets\Rendering\LightGrid.cs.
//
//   light_grid             the grid itself: rows of run-length packed columns of entries, each entry naming a block
//                          of 56 colours ( 168 bytes ), a primary light and the corners that need a sight trace;
//   light_grid_sample      what one lighting origin resolves to: primary light, sun visibility and up to eight
//                          weighted colour blocks;
//   model_lighting_volume  the 256 x H x 4 A8R8G8B8 volume the static model shaders sample ( $modellighting ).
//
// Arithmetic, operation order and float / double widths are those of the C#; every function names its source.

#include "map_types.hpp"

#include "codol_zone/zone.hpp"

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <vector>

namespace codol::view
{
	// LightGrid.cs:8-26 GfxLightGridLayout ( 1.1.1.13: the grid is embedded in the GfxWorld at +152 )
	namespace light_grid_layout
	{
		inline constexpr std::uint32_t WORLD_OFFSET = 152;			// MapLayouts.cs:36 GfxWorldLayout.LightGrid
		inline constexpr std::uint32_t HAS_LIGHT_REGIONS = 0;
		inline constexpr std::uint32_t SUN_PRIMARY_LIGHT_INDEX = 4;
		inline constexpr std::uint32_t MINS = 8;
		inline constexpr std::uint32_t MAXS = 14;
		inline constexpr std::uint32_t ROW_AXIS = 20;
		inline constexpr std::uint32_t COL_AXIS = 24;
		inline constexpr std::uint32_t ROW_DATA_START = 28;
		inline constexpr std::uint32_t RAW_ROW_DATA_SIZE = 32;
		inline constexpr std::uint32_t RAW_ROW_DATA = 36;
		inline constexpr std::uint32_t ENTRY_COUNT = 40;
		inline constexpr std::uint32_t ENTRIES = 44;
		inline constexpr std::uint32_t COLOR_COUNT = 48;
		inline constexpr std::uint32_t COLORS = 52;
		inline constexpr int COLORS_SIZE = 168;
	}

	// LightGrid.cs:28 LightGridSample. The C# arrays have 1 .. 8 elements ( one per distinct colour block of the
	// eight corners ); here they are fixed arrays with `count` used elements. weights are 8.8 fixed point and sum
	// to 256.
	struct light_grid_sample
	{
		int primary_light = 0;
		std::uint8_t sun_visibility = 0;
		int count = 0;											// used elements of color_indices / weights
		std::array<std::uint16_t, 8> color_indices{};
		std::array<std::uint16_t, 8> weights{};
		bool is_fallback = false;
	};

	class light_grid
	{
	public:
		static constexpr int COLORS_SIZE = light_grid_layout::COLORS_SIZE;

		// start, end -> true when the world blocks the line ( world_sight_trace::blocked ). An empty function
		// means "nothing is blocked" ( the C#'s null ).
		using blocked_fn = std::function<bool(float3 start, float3 end)>;

		// LightGrid.cs:49-88 Read. nullopt when the axes are out of range or an array is not loaded zone data.
		static std::optional<light_grid> read(const codol::zone::Zone& zone, codol::zone::zaddr gfx_world);

		bool has_light_regions() const { return has_light_regions_; }
		int row_axis() const { return row_axis_; }
		int col_axis() const { return col_axis_; }
		const std::array<std::uint16_t, 3>& mins() const { return mins_; }
		const std::array<std::uint16_t, 3>& maxs() const { return maxs_; }
		int entry_count() const { return static_cast<int>(entries_.size() / 4); }				// LightGrid.cs:45
		int color_count() const { return static_cast<int>(colors_.size() / COLORS_SIZE); }		// LightGrid.cs:47

		// LightGrid.cs:96 Colors: the 168 bytes ( 56 rgb colours ) of one block; empty when index is out of range
		std::span<const std::uint8_t> colors(int index) const;

		// LightGrid.cs:98-171 Sample
		light_grid_sample sample(float3 origin, int model_light, std::span<const map_light> lights, int last_sun_light,
			const blocked_fn& blocked) const;

		// LightGrid.cs:190-213 Blend
		std::array<std::uint8_t, 168> blend(const light_grid_sample& sample) const;

		// LightGrid.cs:223-230 WriteBlock: the 4 x 4 x 4 block of a model ( x + 4 * y + 16 * z ), A = sun
		// visibility, R G B = the colour. `colors` must hold 168 bytes ( else the block is cleared ).
		static void write_block(std::span<const std::uint8_t> colors, std::uint8_t sun_visibility, std::span<std::uint32_t, 64> texels);

		// LightGrid.cs:232-246 GroundLighting: average of the eight corner colours, packed sun << 24 | b << 16 |
		// g << 8 | r. `colors` must hold 168 bytes ( else only the sun visibility is returned ).
		static std::uint32_t ground_lighting(std::span<const std::uint8_t> colors, std::uint8_t sun_visibility);

		// LightGrid.cs:354-382 InfluencesPoint
		static bool influences_point(const map_light& light, float3 point);

		// system memory of the grid ( not in the C#: the renderer reports what it keeps for the editor )
		std::size_t bytes() const { return row_data_start_.capacity() * 2 + rows_.capacity() + entries_.capacity() + colors_.capacity(); }

	private:
		light_grid() = default;

		int entry_colors(int entry) const;			// LightGrid.cs:90
		int entry_light(int entry) const;			// LightGrid.cs:92
		int entry_trace(int entry) const;			// LightGrid.cs:94
		std::uint32_t row_byte(std::int64_t at) const;
		std::uint16_t row_u16(std::int64_t at) const;	// LightGrid.cs:504

		int corners(float3 origin, float weights[8], int entries[8], int cell[3], bool& inside, int last_sun_light,
			const blocked_fn& blocked) const;
		float3 trace_point(float3 origin, const int cell[3], int corner) const;
		bool influences_corner(int corner, float3 origin, int light, std::span<const map_light> lights) const;
		void row_entries(const int cell[3], int result[4], bool& inside) const;

		bool has_light_regions_ = false;
		int row_axis_ = 0;
		int col_axis_ = 0;
		std::array<std::uint16_t, 3> mins_{};
		std::array<std::uint16_t, 3> maxs_{};
		std::vector<std::uint16_t> row_data_start_;
		std::vector<std::uint8_t> rows_;
		std::vector<std::uint8_t> entries_;
		std::vector<std::uint8_t> colors_;
	};

	// LightGrid.cs:516-555 ModelLightingVolume: one 4 x 4 x 4 block per lit model, 64 blocks per row of blocks.
	// Handles are 1 based ( 0 = no lighting handle ).
	class model_lighting_volume
	{
	public:
		static constexpr int WIDTH = 256;
		static constexpr int DEPTH = 4;

		explicit model_lighting_volume(int handles);

		int height() const { return height_; }
		const std::vector<std::uint32_t>& texels() const { return texels_; }	// WIDTH * height() * DEPTH, A8R8G8B8, z major

		float4 lookup_scale() const;				// lightingLookupScale
		float4 base_coords(int handle) const;		// baseLightingCoords of a model

		// block: 64 texels as light_grid::write_block writes them. A handle outside the volume is ignored.
		void set_block(int handle, std::span<const std::uint32_t> block);

		// ---- not in the C#: the editor places models after the volume was made --------------------------
		int capacity() const { return 64 * (height_ / 4); }			// handles the volume has room for
		// makes room for `handles` handles; the blocks stay. True when the height changed ( new texture, new scales ).
		bool grow(int handles);
		// the 64 texels of a handle, in the order of set_block; zeros for a handle outside the volume
		void get_block(int handle, std::span<std::uint32_t, 64> block) const;

	private:
		int height_ = 4;
		std::vector<std::uint32_t> texels_;
	};
}
