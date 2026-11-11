#include "codol_render/vertex_format.hpp"

#include <algorithm>

namespace codol::render
{
	namespace
	{
		// GfxStateTables.cs:11-29: [format][source] = { stream, offset, D3DDECLTYPE }
		constexpr std::uint8_t SOURCE_INFO[vertex_format::COUNT * vertex_format::SOURCE_COUNT * 3] =
		{
			0, 0, 3, 0, 16, 4, 0, 20, 1, 0, 28, 5, 255, 0, 0, 255, 0, 0, 255, 0, 0, 255, 0, 0, 255, 0, 0,
			0, 0, 3, 0, 16, 4, 0, 20, 5, 0, 24, 5, 0, 28, 5, 255, 0, 0, 255, 0, 0, 255, 0, 0, 255, 0, 0,
			0, 0, 3, 0, 16, 4, 0, 20, 3, 0, 36, 5, 0, 40, 5, 255, 0, 0, 255, 0, 0, 255, 0, 0, 255, 0, 0,
			0, 0, 3, 0, 16, 4, 0, 20, 3, 0, 36, 5, 0, 40, 5, 1, 0, 1, 255, 0, 0, 255, 0, 0, 255, 0, 0,
			0, 0, 3, 0, 16, 4, 0, 20, 3, 0, 36, 5, 0, 40, 5, 1, 0, 1, 255, 0, 0, 1, 8, 4, 255, 0, 0,
			0, 0, 3, 0, 16, 4, 0, 20, 3, 0, 36, 5, 0, 40, 5, 1, 0, 3, 255, 0, 0, 255, 0, 0, 255, 0, 0,
			0, 0, 3, 0, 16, 4, 0, 20, 3, 0, 36, 5, 0, 40, 5, 1, 0, 3, 255, 0, 0, 1, 16, 4, 255, 0, 0,
			0, 0, 3, 0, 16, 4, 0, 20, 3, 0, 36, 5, 0, 40, 5, 1, 0, 3, 255, 0, 0, 1, 16, 4, 1, 20, 4,
			0, 0, 3, 0, 16, 4, 0, 20, 3, 0, 36, 5, 0, 40, 5, 1, 0, 3, 1, 16, 1, 255, 0, 0, 255, 0, 0,
			0, 0, 3, 0, 16, 4, 0, 20, 3, 0, 36, 5, 0, 40, 5, 1, 0, 3, 1, 16, 1, 1, 24, 4, 255, 0, 0,
			0, 0, 3, 0, 16, 4, 0, 20, 3, 0, 36, 5, 0, 40, 5, 1, 0, 3, 1, 16, 1, 1, 24, 4, 1, 28, 4,
			0, 0, 3, 0, 16, 4, 0, 20, 3, 0, 36, 5, 0, 40, 5, 1, 0, 3, 1, 16, 3, 255, 0, 0, 255, 0, 0,
			0, 0, 3, 0, 16, 4, 0, 20, 3, 0, 36, 5, 0, 40, 5, 1, 0, 3, 1, 16, 3, 1, 32, 4, 255, 0, 0,
			0, 0, 3, 0, 16, 4, 0, 20, 3, 0, 36, 5, 0, 40, 5, 1, 0, 3, 1, 16, 3, 1, 32, 4, 1, 36, 4,
			0, 0, 2, 255, 0, 0, 0, 12, 1, 255, 0, 0, 255, 0, 0, 255, 0, 0, 255, 0, 0, 255, 0, 0, 255, 0, 0,
			0, 0, 2, 0, 12, 4, 0, 16, 5, 0, 20, 5, 0, 24, 5, 0, 28, 5, 255, 0, 0, 255, 0, 0, 255, 0, 0,
		};

		// GfxStateTables.cs:31: [dest] = { D3DDECLUSAGE, usage index }
		constexpr std::uint8_t DEST_INFO[vertex_format::DEST_COUNT * 2] =
		{
			0, 0, 3, 0, 10, 0, 10, 1, 12, 0, 5, 0, 5, 1, 5, 2, 5, 3, 5, 4, 5, 5, 5, 6, 5, 7, 0, 0,
		};
	}

	vertex_source_info vertex_source(const int format, const int source)
	{
		if (format < 0 || format >= vertex_format::COUNT || source < 0 || source >= vertex_format::SOURCE_COUNT)
		{
			return { 0xFF, 0, 0 };
		}
		const auto* s = SOURCE_INFO + format * 27 + source * 3;
		return { s[0], s[1], s[2] };
	}

	vertex_dest_info vertex_dest(const int dest)
	{
		if (dest < 0 || dest >= vertex_format::DEST_COUNT)
		{
			return { 0, 0 };
		}
		return { DEST_INFO[2 * dest], DEST_INFO[2 * dest + 1] };
	}

	std::optional<std::vector<d3d::vertex_element>> build_vertex_elements(const std::span<const vertex_routing> routing, const int format, const bool terminate)
	{
		try
		{
			if (format < 0 || format >= vertex_format::COUNT)
			{
				return std::nullopt;
			}
			// GfxStateTables.cs:33-52
			std::vector<d3d::vertex_element> elements;
			elements.reserve(routing.size() + 1);
			for (const auto& r : routing)
			{
				if (r.source >= vertex_format::SOURCE_COUNT || r.dest >= vertex_format::DEST_COUNT)
				{
					return std::nullopt;
				}
				const auto* s = SOURCE_INFO + format * 27 + r.source * 3;
				if (s[0] == 0xFF)
				{
					return std::nullopt;
				}
				const d3d::vertex_element element = { s[0], s[1], s[2], 0, DEST_INFO[2 * r.dest], DEST_INFO[2 * r.dest + 1] };
				std::size_t at = elements.size();
				while (at > 0 && elements[at - 1].stream > element.stream)
				{
					at--;
				}
				elements.insert(elements.begin() + static_cast<std::ptrdiff_t>(at), element);
			}
			if (terminate)
			{
				elements.push_back(d3d::DECL_END);
			}
			return elements;
		}
		catch (...)
		{
			return std::nullopt;
		}
	}

	int stream0_stride(const int format)
	{
		// GfxStateTables.cs:54-59
		if (format >= vertex_format::WORLD && format <= 13)
		{
			return 44;
		}
		return format == 14 ? 20 : 32;
	}

	int stream1_stride(const int format)
	{
		// GfxStateTables.cs:61-73
		if (format < 0 || format >= vertex_format::COUNT)
		{
			return 0;
		}
		int stride = 0;
		for (int source = 0; source < vertex_format::SOURCE_COUNT; source++)
		{
			const auto* s = SOURCE_INFO + format * 27 + source * 3;
			if (s[0] == 1)
			{
				stride = std::max(stride, s[1] + decl_type_size(s[2]));
			}
		}
		return stride;
	}

	int decl_type_size(const std::uint8_t type)
	{
		// GfxStateTables.cs:75-84
		switch (type)
		{
		case 0: return 4;
		case 1: return 8;
		case 2: return 12;
		case 3: return 16;
		case 4: case 5: case 6: case 8: case 9: case 11: case 13: case 14: case 15: return 4;
		case 7: case 10: case 12: case 16: return 8;
		default: return 0;
		}
	}
}
