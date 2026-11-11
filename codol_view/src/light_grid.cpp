// Port of Codol.Assets\Rendering\LightGrid.cs ( see light_grid.hpp ).
//
// Float widths follow the C#: `float` stays float, every expression the C# evaluates in double ( a double
// literal or an explicit cast ) is written with double operands here. Unsigned wrap arounds ( uint column / z
// offsets ) are kept on std::uint32_t. Where the C# would throw on malformed data ( an index outside an array )
// this code reads zero / skips instead - nothing throws.

#include "light_grid.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>

namespace codol::view
{
	using codol::zone::zaddr;
	using codol::zone::Zone;

	namespace
	{
		// C# ( .NET 9 ) float -> int conversion: saturating, NaN -> 0
		int to_int(const float value)
		{
			if (std::isnan(value))
			{
				return 0;
			}
			if (value >= 2147483648.0f)
			{
				return INT32_MAX;
			}
			if (value <= -2147483648.0f)
			{
				return INT32_MIN;
			}
			return static_cast<int>(value);
		}

		int to_int(const double value)
		{
			if (std::isnan(value))
			{
				return 0;
			}
			if (value >= 2147483648.0)
			{
				return INT32_MAX;
			}
			if (value <= -2147483649.0)
			{
				return INT32_MIN;
			}
			return static_cast<int>(value);
		}

		// C# int addition ( unchecked: wraps )
		int wrap_add(const int a, const int b)
		{
			return static_cast<int>(static_cast<std::uint32_t>(a) + static_cast<std::uint32_t>(b));
		}

		// LightGrid.cs:506 Component
		float component(const float3& v, const int axis)
		{
			return axis == 0 ? v.x : axis == 1 ? v.y : v.z;
		}

		// LightGrid.cs:508-513 WithComponent
		float3 with_component(float3 v, const int axis, const float value)
		{
			(axis == 0 ? v.x : axis == 1 ? v.y : v.z) = value;
			return v;
		}

		// System.Numerics.Vector3.Dot
		float dot(const float3& a, const float3& b)
		{
			return a.x * b.x + a.y * b.y + a.z * b.z;
		}

		// GameMemory.cs:38 IsLoadedRange + ReadBytes: false when the range is not image memory. A zero length
		// range is loaded ( and empty ).
		bool read_range(const Zone& zone, const zaddr address, const std::uint64_t size, std::vector<std::uint8_t>& out)
		{
			out.clear();
			if (size == 0)
			{
				return true;
			}
			if (size > 0xFFFFFFFFull)
			{
				return false;
			}
			const auto data = zone.bytes(address, static_cast<std::size_t>(size));
			if (data.size() != size)
			{
				return false;
			}
			out.assign(data.begin(), data.end());
			return true;
		}

		// LightGrid.cs:215-221 BlockColor: which of the 56 colours each of the 64 texels of a block shows
		constexpr std::uint8_t BLOCK_COLOR[64] =
		{
			0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
			16, 17, 18, 19, 20, 0, 3, 21, 22, 12, 15, 23, 24, 25, 26, 27,
			28, 29, 30, 31, 32, 40, 43, 33, 34, 52, 55, 35, 36, 37, 38, 39,
			40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55,
		};

		// LightGrid.cs:173-188 FixedWeights
		void fixed_weights(const float* weights, const int count, const float inverse_total, std::uint16_t* result)
		{
			int sum = 0, largest = 0;
			for (int i = 0; i < count; i++)
			{
				result[i] = static_cast<std::uint16_t>(to_int(static_cast<double>(weights[i]) * (static_cast<double>(inverse_total) * 256.0) + 0.5));
				sum += result[i];
				if (result[largest] < result[i])
				{
					largest = i;
				}
			}
			result[largest] = static_cast<std::uint16_t>(result[largest] + 256 - sum);
		}
	}

	// LightGrid.cs:49-88 Read
	std::optional<light_grid> light_grid::read(const Zone& zone, const zaddr gfx_world)
	try
	{
		namespace L = light_grid_layout;
		const zaddr g = gfx_world + L::WORLD_OFFSET;

		light_grid grid;
		grid.has_light_regions_ = zone.get<std::uint8_t>(g + L::HAS_LIGHT_REGIONS) != 0;
		grid.row_axis_ = static_cast<int>(zone.get<std::uint32_t>(g + L::ROW_AXIS));
		grid.col_axis_ = static_cast<int>(zone.get<std::uint32_t>(g + L::COL_AXIS));
		if (grid.row_axis_ < 0 || grid.row_axis_ > 2 || grid.col_axis_ < 0 || grid.col_axis_ > 2)
		{
			return std::nullopt;
		}
		for (std::uint32_t i = 0; i < 3; i++)
		{
			grid.mins_[i] = zone.get<std::uint16_t>(g + L::MINS + 2 * i);
			grid.maxs_[i] = zone.get<std::uint16_t>(g + L::MAXS + 2 * i);
		}
		const std::size_t ra = static_cast<std::size_t>(grid.row_axis_);
		const int row_count = grid.maxs_[ra] - grid.mins_[ra] + 1;
		const zaddr row_data_start = zone.get<zaddr>(g + L::ROW_DATA_START);
		const std::uint32_t raw_size = zone.get<std::uint32_t>(g + L::RAW_ROW_DATA_SIZE);
		const zaddr raw = zone.get<zaddr>(g + L::RAW_ROW_DATA);
		const std::uint32_t entry_count = zone.get<std::uint32_t>(g + L::ENTRY_COUNT);
		const zaddr entries = zone.get<zaddr>(g + L::ENTRIES);
		const std::uint32_t color_count = zone.get<std::uint32_t>(g + L::COLOR_COUNT);
		const zaddr colors = zone.get<zaddr>(g + L::COLORS);

		std::vector<std::uint8_t> starts;
		if (row_count <= 0
			|| !read_range(zone, row_data_start, 2ull * static_cast<std::uint64_t>(row_count), starts)
			|| !read_range(zone, raw, raw_size, grid.rows_)
			|| !read_range(zone, entries, 4ull * entry_count, grid.entries_)
			|| !read_range(zone, colors, static_cast<std::uint64_t>(L::COLORS_SIZE) * color_count, grid.colors_))
		{
			return std::nullopt;
		}
		grid.row_data_start_.resize(static_cast<std::size_t>(row_count));
		std::memcpy(grid.row_data_start_.data(), starts.data(), starts.size());
		return grid;
	}
	catch (...)
	{
		return std::nullopt;		// out of memory: nothing throws
	}

	// LightGrid.cs:90
	int light_grid::entry_colors(const int entry) const
	{
		const std::size_t at = 4 * static_cast<std::size_t>(entry);
		return entry >= 0 && at + 3 < entries_.size() ? entries_[at] | (entries_[at + 1] << 8) : 0;
	}

	// LightGrid.cs:92
	int light_grid::entry_light(const int entry) const
	{
		const std::size_t at = 4 * static_cast<std::size_t>(entry);
		return entry >= 0 && at + 3 < entries_.size() ? entries_[at + 2] : 0;
	}

	// LightGrid.cs:94
	int light_grid::entry_trace(const int entry) const
	{
		const std::size_t at = 4 * static_cast<std::size_t>(entry);
		return entry >= 0 && at + 3 < entries_.size() ? entries_[at + 3] : 0;
	}

	// _rows[at]; 0 outside the row data ( the C# would throw )
	std::uint32_t light_grid::row_byte(const std::int64_t at) const
	{
		return at >= 0 && static_cast<std::uint64_t>(at) < rows_.size() ? rows_[static_cast<std::size_t>(at)] : 0u;
	}

	// LightGrid.cs:504 U16
	std::uint16_t light_grid::row_u16(const std::int64_t at) const
	{
		return static_cast<std::uint16_t>(row_byte(at) | (row_byte(at + 1) << 8));
	}

	// LightGrid.cs:96 Colors
	std::span<const std::uint8_t> light_grid::colors(const int index) const
	{
		if (index < 0 || index >= color_count())
		{
			return {};
		}
		return std::span<const std::uint8_t>(colors_).subspan(static_cast<std::size_t>(index) * COLORS_SIZE, COLORS_SIZE);
	}

	// LightGrid.cs:98-171 Sample
	light_grid_sample light_grid::sample(const float3 origin, const int model_light, const std::span<const map_light> lights,
		const int last_sun_light, const blocked_fn& blocked) const
	{
		float weights[8] = {};
		int entries[8] = {};
		int cell[3] = {};
		bool inside = true;
		const int best = corners(origin, weights, entries, cell, inside, last_sun_light, blocked);

		int light = best;
		if (light < 256 - last_sun_light)
		{
			if (has_light_regions_ && (light == 0 || light > last_sun_light))
			{
				light = model_light;
			}
		}
		else
		{
			light = light + last_sun_light - 255;
		}
		float lit = 0, shadowed = 0, total = 0;
		std::uint16_t indices[8] = {};
		float summed[8] = {};
		int count = 0;
		for (int i = 0; i < 8; i++)
		{
			const int entry = entries[i];
			if (entry < 0)
			{
				continue;
			}
			const int entry_l = entry_light(entry);
			if (entry_l == light)
			{
				lit += weights[i];
			}
			else if (!(entry_l != 0 && (entry_l < 256 - last_sun_light || light == 0)) && influences_corner(i, origin, light, lights))
			{
				shadowed += weights[i];
			}
			total += weights[i];
			const std::uint16_t colors_index = static_cast<std::uint16_t>(entry_colors(entry));
			int at = -1;
			for (int k = 0; k < count; k++)
			{
				if (indices[k] == colors_index)
				{
					at = k;
					break;
				}
			}
			if (at >= 0)
			{
				summed[at] += weights[i];
			}
			else
			{
				indices[count] = colors_index;
				summed[count] = weights[i];
				count++;
			}
		}

		light_grid_sample result;
		if (count == 0)
		{
			int fallback = inside ? 1 : 0;
			if (color_count() <= fallback)
			{
				fallback = 0;
			}
			result.primary_light = 0;
			result.sun_visibility = 255;
			result.count = 1;
			result.color_indices[0] = static_cast<std::uint16_t>(fallback);
			result.weights[0] = 256;
			result.is_fallback = true;
			return result;
		}
		float sun = 0;
		if (light != 0)
		{
			sun = shadowed == 0 ? (lit == 0 ? 0.0f : 1.0f) : lit / (shadowed + lit);
		}
		result.primary_light = light;
		result.sun_visibility = static_cast<std::uint8_t>(to_int(static_cast<double>(sun) * 255.0 + 0.5));
		result.count = count;
		if (count == 1)
		{
			result.color_indices[0] = indices[0];
			result.weights[0] = 256;
			return result;
		}
		for (int k = 0; k < count; k++)
		{
			result.color_indices[static_cast<std::size_t>(k)] = indices[k];
		}
		fixed_weights(summed, count, 1.0f / total, result.weights.data());
		return result;
	}

	// LightGrid.cs:190-213 Blend
	std::array<std::uint8_t, 168> light_grid::blend(const light_grid_sample& sample) const
	{
		std::array<std::uint8_t, 168> result{};
		const int count = std::clamp(sample.count, 0, 8);
		if (count == 1)
		{
			const auto block = colors(sample.color_indices[0]);
			if (!block.empty())
			{
				std::memcpy(result.data(), block.data(), result.size());
			}
			return result;
		}
		std::uint16_t accumulator[COLORS_SIZE] = {};
		for (int k = 0; k < count; k++)
		{
			const auto block = colors(sample.color_indices[static_cast<std::size_t>(k)]);
			if (block.empty())
			{
				continue;
			}
			const std::uint16_t weight = sample.weights[static_cast<std::size_t>(k)];
			for (std::size_t i = 0; i < static_cast<std::size_t>(COLORS_SIZE); i++)
			{
				accumulator[i] = static_cast<std::uint16_t>(accumulator[i] + weight * block[i]);
			}
		}
		for (std::size_t i = 0; i < result.size(); i++)
		{
			result[i] = static_cast<std::uint8_t>(static_cast<std::uint16_t>(accumulator[i] + 127) >> 8);
		}
		return result;
	}

	// LightGrid.cs:223-230 WriteBlock
	void light_grid::write_block(const std::span<const std::uint8_t> colors, const std::uint8_t sun_visibility,
		const std::span<std::uint32_t, 64> texels)
	{
		if (colors.size() < static_cast<std::size_t>(COLORS_SIZE))
		{
			std::fill(texels.begin(), texels.end(), 0u);
			return;
		}
		for (std::size_t t = 0; t < 64; t++)
		{
			const std::size_t c = 3u * BLOCK_COLOR[t];
			texels[t] = (static_cast<std::uint32_t>(sun_visibility) << 24) | (static_cast<std::uint32_t>(colors[c]) << 16)
				| (static_cast<std::uint32_t>(colors[c + 1]) << 8) | colors[c + 2];
		}
	}

	// LightGrid.cs:232-246 GroundLighting
	std::uint32_t light_grid::ground_lighting(const std::span<const std::uint8_t> colors, const std::uint8_t sun_visibility)
	{
		static constexpr std::size_t CORNERS[8] = { 0, 3, 12, 15, 40, 43, 52, 55 };
		std::uint32_t packed = static_cast<std::uint32_t>(sun_visibility) << 24;
		if (colors.size() < static_cast<std::size_t>(COLORS_SIZE))
		{
			return packed;
		}
		for (std::size_t channel = 0; channel < 3; channel++)
		{
			int sum = 4;
			for (const std::size_t c : CORNERS)
			{
				sum += colors[3 * c + channel];
			}
			packed |= static_cast<std::uint32_t>(sum >> 3) << (8 * channel);
		}
		return packed;
	}

	// LightGrid.cs:248-319 Corners: the trilinear weights of the eight cell corners around `origin`, their grid
	// entries ( -1 = none / dropped ) and the primary light the corners agree on.
	//   corner i: bit 2 = +1 along the row axis, bit 1 = +1 along the column axis, bit 0 = +1 in z
	int light_grid::corners(const float3 origin, float weights[8], int entries[8], int cell[3], bool& inside,
		const int last_sun_light, const blocked_fn& blocked) const
	{
		cell[0] = wrap_add(to_int(std::floor(origin.x)), 0x20000) >> 5;
		cell[1] = wrap_add(to_int(std::floor(origin.y)), 0x20000) >> 5;
		cell[2] = wrap_add(to_int(std::floor(origin.z)), 0x20000) >> 6;
		const int ra = row_axis_, ca = col_axis_;
		const float fr = static_cast<float>((static_cast<double>(component(origin, ra)) - -131072.0) * 0.03125
			- static_cast<double>(static_cast<std::uint32_t>(cell[ra])));
		const float fc = static_cast<float>(0.03125 * (static_cast<double>(component(origin, ca)) - -131072.0)
			- static_cast<double>(static_cast<std::uint32_t>(cell[ca])));
		const float fz = static_cast<float>((static_cast<double>(origin.z) - -131072.0) * 0.015625
			- static_cast<double>(static_cast<std::uint32_t>(cell[2])));
		const double dr = fr, dc = fc, dz = fz;
		const float v35 = static_cast<float>((1.0 - dc) * (1.0 - dz));
		weights[0] = static_cast<float>(static_cast<double>(v35) * (1.0 - dr));
		weights[4] = static_cast<float>(static_cast<double>(v35) * dr);
		const float v36 = static_cast<float>((1.0 - dc) * dz);
		weights[1] = static_cast<float>(static_cast<double>(v36) * (1.0 - dr));
		weights[5] = static_cast<float>(static_cast<double>(v36) * dr);
		const float v37 = static_cast<float>((1.0 - dz) * dc);
		weights[2] = static_cast<float>(static_cast<double>(v37) * (1.0 - dr));
		weights[6] = static_cast<float>(static_cast<double>(v37) * dr);
		const float v38 = static_cast<float>(dz * dc);
		weights[3] = static_cast<float>((1.0 - dr) * static_cast<double>(v38));
		weights[7] = static_cast<float>(static_cast<double>(v38) * dr);

		inside = true;
		row_entries(cell, entries, inside);
		cell[ra] = wrap_add(cell[ra], 1);
		row_entries(cell, entries + 4, inside);
		cell[ra] = wrap_add(cell[ra], -1);

		float best_weight = 0;
		int best = 0;
		bool found_visible = false;
		for (int i = 0; i < 8; i++)
		{
			const int entry = entries[i];
			if (entry < 0)
			{
				continue;
			}
			if (weights[i] < 0.001f)
			{
				entries[i] = -1;
				continue;
			}
			const bool is_blocked = (entry_trace(entry) & (1 << i)) != 0 && blocked && blocked(origin, trace_point(origin, cell, i));
			if (is_blocked)
			{
				if (found_visible)
				{
					entries[i] = -1;
					continue;
				}
			}
			else if (!found_visible)
			{
				best_weight = weights[i];
				found_visible = true;
				best = entry_light(entry);
				for (int k = 0; k < i; k++)
				{
					entries[k] = -1;
				}
				continue;
			}
			const int light = entry_light(entry);
			if (best == 0 || (light != 0 && (best >= 256 - last_sun_light || (light < 256 - last_sun_light && weights[i] > best_weight))))
			{
				best = light;
				best_weight = weights[i];
			}
		}
		return best;
	}

	// LightGrid.cs:321-334 TracePoint: the corner's position, pulled 0.01 units towards the origin
	float3 light_grid::trace_point(const float3 origin, const int cell[3], const int corner) const
	{
		float3 point{
			static_cast<float>(cell[0] * 32.0 - 131072.0),
			static_cast<float>(cell[1] * 32.0 - 131072.0),
			static_cast<float>(cell[2] * 64.0 - 131072.0) };
		point = with_component(point, row_axis_, component(point, row_axis_) + static_cast<float>(8 * (corner & 4)));
		point = with_component(point, col_axis_, component(point, col_axis_) + static_cast<float>(16 * (corner & 2)));
		point.z += static_cast<float>((corner & 1) << 6);
		float3 direction = origin - point;
		const float length = std::sqrt(dot(direction, direction));
		if (length > 0)
		{
			direction = { direction.x / length, direction.y / length, direction.z / length };
		}
		return point + direction * 0.01f;
	}

	// LightGrid.cs:336-352 InfluencesCorner. The z of the corner is derived from origin.X in the C# ( line 347 );
	// kept as it is, the port has to give the same samples.
	bool light_grid::influences_corner(const int corner, const float3 origin, const int light, const std::span<const map_light> lights) const
	{
		if (light <= 0 || static_cast<std::size_t>(light) >= lights.size())
		{
			return false;
		}
		const map_light& l = lights[static_cast<std::size_t>(light)];
		if (l.type == 1)
		{
			return true;
		}
		float3 point{
			std::floor(origin.x * 0.03125f) * 32.0f,
			std::floor(origin.y * 0.03125f) * 32.0f,
			std::floor(origin.x * 0.015625f) * 64.0f };
		point = with_component(point, row_axis_, component(point, row_axis_) + static_cast<float>(8 * (corner & 4)));
		point = with_component(point, col_axis_, component(point, col_axis_) + static_cast<float>(16 * (corner & 2)));
		point.z += static_cast<float>((corner & 1) << 6);
		return influences_point(l, point);
	}

	// LightGrid.cs:354-382 InfluencesPoint
	bool light_grid::influences_point(const map_light& light, const float3 point)
	{
		const float3 d = light.origin - point;
		const float distance2 = dot(d, d);
		if (light.radius * light.radius <= distance2)
		{
			return false;
		}
		if (light.type == 3 || -light.cos_half_fov_outer >= light.rotation_limit)
		{
			return true;
		}
		const float dp = dot(d, light.direction);
		float cos;
		if (light.rotation_limit == 1.0f)
		{
			cos = light.cos_half_fov_outer;
		}
		else
		{
			const float a = light.cos_half_fov_outer, b = light.rotation_limit;
			cos = static_cast<float>(static_cast<double>(a * b) - std::sqrt(static_cast<double>((1.0f - b * b) * (1.0f - a * a))));
			if (cos <= 0)
			{
				return dp <= cos * light.radius;
			}
		}
		return dp > 0 && dp * dp >= cos * cos * distance2;
	}

	// LightGrid.cs:384-502 RowEntries: the entries of the four corners ( column, z ), ( column, z + 1 ),
	// ( column + 1, z ), ( column + 1, z + 1 ) of one row; -1 where the grid has none. `inside` is cleared when
	// the cell lies below the stored z range.
	//
	//   row data ( at 4 * rowDataStart[row] ): u16 colStart, colCount, zStart, zCount, u32 firstEntry, then runs
	//   of columns: u8 colCount, u8 zCount [, u8 / u16 zStart when zCount != 0 ] ( u16 when the row's zCount > 255 )
	void light_grid::row_entries(const int cell[3], int result[4], bool& inside) const
	{
		result[0] = result[1] = result[2] = result[3] = -1;
		const std::size_t ra = static_cast<std::size_t>(row_axis_), ca = static_cast<std::size_t>(col_axis_);
		const std::uint32_t row = static_cast<std::uint32_t>(cell[ra] - mins_[ra]);
		if (row >= static_cast<std::uint32_t>(maxs_[ra] - mins_[ra] + 1) || row >= row_data_start_.size() || row_data_start_[row] == 0xFFFF)
		{
			return;
		}
		const std::int64_t r = 4 * static_cast<std::int64_t>(row_data_start_[row]);
		const std::uint16_t col_start = row_u16(r), col_count = row_u16(r + 2), z_start = row_u16(r + 4), z_count = row_u16(r + 6);
		std::uint32_t col = static_cast<std::uint32_t>(cell[ca] - col_start);
		const std::uint32_t z = static_cast<std::uint32_t>(cell[2] - z_start);
		const std::uint32_t z_from_row = z;
		if (col + 1 > col_count)
		{
			return;
		}
		if (z + 1 > z_count)
		{
			if (static_cast<std::uint32_t>(cell[2]) < z_start)
			{
				inside = false;
			}
			return;
		}
		int first = static_cast<int>(row_byte(r + 8) | (row_byte(r + 9) << 8) | (row_byte(r + 10) << 16) | (row_byte(r + 11) << 24));
		std::int64_t p = r + 12;
		const bool wide = z_count > 0xFF;
		const int run_size = wide ? 4 : 3;
		const auto z_start_at = [&](const std::int64_t at) -> std::uint32_t
		{
			return row_byte(at) | (wide ? row_byte(at + 1) << 8 : 0u);
		};
		if (col == UINT32_MAX)
		{
			const std::uint32_t run_z = z_start_at(p + 2);
			const int e = wrap_add(static_cast<int>(z - run_z), first);
			if (z - run_z < row_byte(p + 1))
			{
				result[2] = e;
			}
			if (z - run_z + 1 < row_byte(p + 1))
			{
				result[3] = wrap_add(e, 1);
			}
			if (z < run_z)
			{
				inside = false;
			}
			return;
		}
		std::uint32_t cols = row_byte(p);
		while (col >= cols)
		{
			const int zc = static_cast<int>(row_byte(p + 1));
			first = wrap_add(first, static_cast<int>(static_cast<std::int64_t>(cols) * zc));
			col -= cols;
			p += zc != 0 ? run_size : 2;
			if (static_cast<std::uint64_t>(p) >= rows_.size())
			{
				return;		// malformed row ( the C# would throw )
			}
			cols = row_byte(p);
		}
		if (row_byte(p + 1) != 0)
		{
			const std::uint32_t run_z = z_start_at(p + 2);
			if (z < run_z)
			{
				inside = false;
			}
			const std::uint32_t dz = z - run_z;
			const int zc = static_cast<int>(row_byte(p + 1));
			const int e = static_cast<int>(static_cast<std::uint32_t>(first) + dz + col * static_cast<std::uint32_t>(zc));
			if (dz < static_cast<std::uint32_t>(zc))
			{
				result[0] = e;
			}
			if (dz + 1 < static_cast<std::uint32_t>(zc))
			{
				result[1] = wrap_add(e, 1);
			}
			if (col + 1 < row_byte(p))
			{
				if (dz < static_cast<std::uint32_t>(zc))
				{
					result[2] = wrap_add(e, zc);
				}
				if (dz + 1 < static_cast<std::uint32_t>(zc))
				{
					result[3] = wrap_add(e, zc + 1);
				}
				return;
			}
		}
		else
		{
			const std::uint32_t next_count = row_byte(p + 3);
			if (next_count != 0 && z < z_start_at(p + 4) + next_count)
			{
				inside = false;
			}
			if (col + 1 < row_byte(p))
			{
				return;
			}
		}
		if (cell[ca] + 1 == col_count + col_start)
		{
			return;
		}
		const int run_cols = static_cast<int>(row_byte(p));
		const int run_z_count = static_cast<int>(row_byte(p + 1));
		const int next = wrap_add(run_cols * run_z_count, first);
		const std::int64_t next_run = p + (run_z_count != 0 ? run_size : 2);
		const std::uint32_t next_z = z_from_row - z_start_at(next_run + 2);
		const int next_entry = static_cast<int>(next_z + static_cast<std::uint32_t>(next));
		if (next_z < row_byte(next_run + 1))
		{
			result[2] = next_entry;
		}
		if (next_z + 1 < row_byte(next_run + 1))
		{
			result[3] = wrap_add(next_entry, 1);
		}
	}

	// ---------------------------------------------------------------------------------------------------
	// LightGrid.cs:516-555 ModelLightingVolume

	// LightGrid.cs:521-526
	model_lighting_volume::model_lighting_volume(const int handles)
	{
		const int rows = std::max(1, static_cast<int>((static_cast<std::int64_t>(handles) + 63) / 64));
		height_ = static_cast<int>(std::max<std::uint32_t>(4u, std::bit_ceil(static_cast<std::uint32_t>(4 * rows))));
		texels_.assign(static_cast<std::size_t>(WIDTH) * static_cast<std::size_t>(height_) * DEPTH, 0u);
	}

	// LightGrid.cs:532 LookupScale
	float4 model_lighting_volume::lookup_scale() const
	{
		return { 1.5f / WIDTH, 1.5f / static_cast<float>(height_), 1.5f / DEPTH, 0.0f };
	}

	// LightGrid.cs:534-538 BaseCoords
	float4 model_lighting_volume::base_coords(const int handle) const
	{
		const int slot = handle - 1;
		return { static_cast<float>(4 * (slot & 63) + 2) / 256.0f, static_cast<float>(4 * (slot >> 6) + 2) / static_cast<float>(height_), 0.5f, 1.0f };
	}

	// LightGrid.cs:540-554 SetBlock
	void model_lighting_volume::set_block(const int handle, const std::span<const std::uint32_t> block)
	{
		const int slot = handle - 1;
		if (slot < 0 || 4 * (slot >> 6) + 4 > height_ || block.size() < 64)
		{
			return;
		}
		const int x0 = 4 * (slot & 63), y0 = 4 * (slot >> 6);
		for (int z = 0; z < 4; z++)
		{
			for (int y = 0; y < 4; y++)
			{
				for (int x = 0; x < 4; x++)
				{
					texels_[static_cast<std::size_t>((z * height_ + y0 + y) * WIDTH + x0 + x)] = block[static_cast<std::size_t>(x + 4 * y + 16 * z)];
				}
			}
		}
	}
}
