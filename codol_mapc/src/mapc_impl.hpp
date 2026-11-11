#pragma once

// Shared pieces of the compiler stages.

#include "codol_mapc/mapc.hpp"
#include "codol_zone/assets.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdarg>
#include <cstring>
#include <map>
#include <span>
#include <stdexcept>

namespace codol::mapc
{
	namespace c1 = codol::c1;
	using zone::zaddr;
	using zone::Zone;
	using zone::ZoneDataId;
	using zone::ZoneEditor;
	using zone::ZonePointer;
	using zone::ZoneTarget;

	inline std::string format(const char* fmt, ...)
	{
		char text[1024];
		va_list args;
		va_start(args, fmt);
		std::vsnprintf(text, sizeof(text), fmt, args);
		va_end(args);
		return text;
	}

	// a stage that cannot go on: caught by compile_zone and returned as its error
	struct compile_error : std::runtime_error
	{
		using std::runtime_error::runtime_error;
	};

	inline void check(const zone::Status& s, const char* what)
	{
		if (!s)
		{
			throw compile_error(std::string(what) + ": " + s.message);
		}
	}

	template <typename T>
	T rd(const std::span<const std::uint8_t> data, const std::size_t offset)
	{
		T value{};
		if (offset + sizeof(T) <= data.size())
		{
			std::memcpy(&value, data.data() + offset, sizeof(T));
		}
		return value;
	}

	template <typename T>
	void wr(std::vector<std::uint8_t>& data, const std::size_t offset, const T& value)
	{
		std::memcpy(data.data() + offset, &value, sizeof(T));
	}

	template <typename T>
	void append(std::vector<std::uint8_t>& data, const T& value)
	{
		const auto* p = reinterpret_cast<const std::uint8_t*>(&value);
		data.insert(data.end(), p, p + sizeof(T));
	}

	template <typename T>
	std::vector<T> array_of(const std::span<const std::uint8_t> data)
	{
		std::vector<T> out(data.size() / sizeof(T));
		if (!out.empty())
		{
			std::memcpy(out.data(), data.data(), out.size() * sizeof(T));
		}
		return out;
	}

	template <typename T>
	std::span<const std::uint8_t> bytes_of(const std::vector<T>& values)
	{
		return { reinterpret_cast<const std::uint8_t*>(values.data()), values.size() * sizeof(T) };
	}

	// axis aligned box as the zone stores it ( centre + half extents ), with min / max arithmetic
	struct box
	{
		float mins[3] = { 1e30f, 1e30f, 1e30f };
		float maxs[3] = { -1e30f, -1e30f, -1e30f };

		box() = default;
		explicit box(const c1::Bounds& b)
		{
			for (int k = 0; k < 3; k++)
			{
				mins[k] = b.midPoint[k] - b.halfSize[k];
				maxs[k] = b.midPoint[k] + b.halfSize[k];
			}
		}
		bool valid() const { return mins[0] <= maxs[0] && mins[1] <= maxs[1] && mins[2] <= maxs[2]; }
		void add(const float p[3])
		{
			for (int k = 0; k < 3; k++)
			{
				mins[k] = std::min(mins[k], p[k]);
				maxs[k] = std::max(maxs[k], p[k]);
			}
		}
		void add(const box& b)
		{
			if (b.valid())
			{
				add(b.mins);
				add(b.maxs);
			}
		}
		bool intersects(const box& b, const float eps = 0.0f) const
		{
			for (int k = 0; k < 3; k++)
			{
				if (b.mins[k] > maxs[k] + eps || b.maxs[k] < mins[k] - eps)
				{
					return false;
				}
			}
			return true;
		}
		bool contains(const box& b) const
		{
			for (int k = 0; k < 3; k++)
			{
				if (b.mins[k] < mins[k] || b.maxs[k] > maxs[k])
				{
					return false;
				}
			}
			return true;
		}
		// how much the box has to grow to hold `b` ( sum over the axes )
		float growth(const box& b) const
		{
			float total = 0;
			for (int k = 0; k < 3; k++)
			{
				total += std::max(0.0f, mins[k] - b.mins[k]) + std::max(0.0f, b.maxs[k] - maxs[k]);
			}
			return total;
		}
		c1::Bounds bounds() const
		{
			c1::Bounds b{};
			for (int k = 0; k < 3; k++)
			{
				b.midPoint[k] = (mins[k] + maxs[k]) * 0.5f;
				b.halfSize[k] = (maxs[k] - mins[k]) * 0.5f;
			}
			return b;
		}
	};

	// GfxAabbTree node of a cell ( 44 bytes; CODOL-AssetExplorer WorldCells.cs GfxAabbTreeLayout, MapEdits.cs )
#pragma pack(push, 1)
	struct aabb_node
	{
		c1::Bounds bounds;
		std::uint16_t childCount;
		std::uint16_t surfaceCount;
		std::uint16_t startSurfIndex;			// into dpvs.sortedSurfIndex
		std::uint16_t surfaceCountNoDecal;
		std::uint16_t startSurfIndexNoDecal;
		std::uint16_t smodelIndexCount;
		c1::zptr smodelIndexes;					// u16; the root's list is stored, the other nodes point into it
		std::int32_t childrenOffset;			// bytes from this node to its first child
	};
	// SModelAabbNode of the clip map ( 28 bytes; MapEdits.cs )
	struct smodel_node
	{
		c1::Bounds bounds;
		std::uint16_t firstChild;				// >= numStaticModels: node firstChild - numStaticModels, else a static model
		std::uint16_t childCount;
	};
	struct cnode
	{
		c1::zptr plane;
		std::int16_t children[2];				// < 0: leaf -1 - child
	};
	struct shadow_geometry
	{
		std::uint16_t surfaceCount;
		std::uint16_t smodelCount;
		c1::zptr sortedSurfIndex;
		c1::zptr smodelIndex;
	};
	struct material_memory
	{
		c1::zptr material;
		std::int32_t memory;
	};
#pragma pack(pop)
	static_assert(sizeof(aabb_node) == 44 && sizeof(smodel_node) == 28 && sizeof(cnode) == 8 && sizeof(shadow_geometry) == 12);

	// GfxWorld fields the layout header has no name for ( MapEdits.cs, WorldCells.cs )
	inline constexpr std::uint32_t WORLD_DPVS_PLANES = 56;
	inline constexpr std::uint32_t WORLD_NODES = 60;
	inline constexpr std::uint32_t WORLD_AABB_TREE_COUNTS = 68;
	inline constexpr std::uint32_t WORLD_AABB_TREES = 72;
	inline constexpr std::uint32_t WORLD_SHADOW_GEOM = 0x1C0;
	inline constexpr std::uint32_t XMODEL_RADIUS = 316;			// float radius, then Bounds ( centre, half extents )
	inline constexpr std::uint32_t XMODEL_BOUNDS = 320;

	// the words of the "contents" a tool brush has no render face for ( codol_edit puts such brushes on their own layer )
	bool is_tool_material(std::string_view name);

	// number of 32 bit words of a visibility array for `count` items, in the relation the stored value has to the
	// old count ( MapEdits.cs VisDataCount )
	std::uint32_t vis_data_count(std::uint32_t stored, std::uint32_t count, std::uint32_t new_count);

	// state of one compile
	struct context
	{
		ZoneEditor& editor;
		const zone::ClientKey& key;
		const options& opts;
		report& rep;
		const edit::document& baseline;
		const edit::document& edited;

		// the map assets of the zone as the editor has it loaded ( refreshed by reload )
		zaddr gfx_address = 0, clip_address = 0;
		std::size_t gfx_asset = ZoneEditor::npos, clip_asset = ZoneEditor::npos;
		zone::GfxWorldView gfx;
		zone::ClipMapView clip;
		ZoneDataId gfx_header = 0, clip_header = 0;

		const Zone& zone() const { return editor.zone(); }
		void refresh();		// reads the views from editor.zone()
		void reload();		// writes the edited graph and loads it again: the next stage sees the result of the last

		void note(std::string text) { rep.notes.push_back(std::move(text)); }
		void warn(std::string text) { rep.warnings.push_back(std::move(text)); }

		// the stream data behind a pointer field of the loaded zone; throws when there is none
		ZoneDataId data(zaddr address, const char* what) const;
		// replaces an array and the count that goes with it stays the caller's business
		void set(ZoneDataId id, std::span<const std::uint8_t> bytes, std::span<const ZonePointer> pointers, const char* what);
		template <typename T>
		void patch(const ZoneDataId id, const std::uint32_t offset, const T& value, const char* what)
		{
			check(editor.patch_value(id, offset, value), what);
		}
	};

	// ---- the cell the BSP of the GfxWorld puts a point in ( codol_view world_cells.cpp cell_of_point ) ----

	struct cell_finder
	{
		std::vector<c1::cplane_s> planes;
		std::vector<std::uint16_t> nodes;
		int cell_count = 0;

		explicit cell_finder(const context& ctx)
		{
			const Zone& zone = ctx.zone();
			planes = zone.get_array<c1::cplane_s>(zone.get<zaddr>(ctx.gfx_address + WORLD_DPVS_PLANES), static_cast<std::size_t>(std::max(ctx.gfx.raw.planeCount, 0)));
			nodes = zone.get_array<std::uint16_t>(zone.get<zaddr>(ctx.gfx_address + WORLD_NODES), static_cast<std::size_t>(std::max(ctx.gfx.raw.nodeCount, 0)));
			cell_count = ctx.gfx.raw.cellCount;
		}

		int find(const float p[3]) const
		{
			if (nodes.empty() || planes.empty())
			{
				return -1;
			}
			int at = 0;
			for (int step = 0; step < 256; step++)
			{
				const int value = nodes[static_cast<std::size_t>(at)];
				if (value < cell_count)
				{
					return value;
				}
				if (value >= static_cast<int>(planes.size()) || at + 1 >= static_cast<int>(nodes.size()))
				{
					return -1;
				}
				const c1::cplane_s& plane = planes[static_cast<std::size_t>(value)];
				const float side = plane.normal[0] * p[0] + plane.normal[1] * p[1] + plane.normal[2] * p[2] - plane.dist;
				at = side >= 0 ? at + 2 : at + nodes[static_cast<std::size_t>(at) + 1];
				if (at < 0 || at >= static_cast<int>(nodes.size()))
				{
					return -1;
				}
			}
			return -1;
		}
	};

	// placement of an XModel instance: bounds from the model's own box
	struct model_box
	{
		float mid[3] = {};
		float half[3] = {};
		float radius = 0;
		bool known = false;
	};

	void stage_entities(context& ctx);
	void stage_models(context& ctx);
	void stage_brushes(context& ctx);
	void stage_surfaces(context& ctx);

	// brushes of the zone's clip map in the order document::import_fastfile emits them: per sub model ( 0: world )
	// the cbrush_t indices
	std::map<int, std::vector<int>> imported_brush_order(const Zone& zone, const zone::ClipMapView& clip);

	// leaf brush nodes as the zone *stream* has them ( the loader overwrites the float of some nodes in the image )
	std::vector<c1::cLeafBrushNode_s> stream_leaf_brush_nodes(const ZoneEditor& editor, const zone::ClipMapView& clip);
}
