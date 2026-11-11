// The collision data of a zone as a dedicated server reads it: see server_check.hpp.
//
// The order of the checks and the offsets follow the server's readers ( CoD4x_Server src/codol/collision:
// codol_collision_stream.c, codol_world_planes.c, codol_collision_brushes.c, codol_collision_brush_tree.c,
// codol_collision_leaves.c, codol_collision_static_models.c, codol_collision_native.c ). Nothing here uses the
// loaders of codol_zone: the stream is walked by hand, as the server does.

#include "codol_mapc/server_check.hpp"

#include <cmath>
#include <cstdarg>
#include <cstring>
#include <vector>

namespace codol::mapc
{
	namespace
	{
		constexpr std::uint32_t NONE = 0xFFFFFFFFu;		// "the data follows"

		struct failure
		{
			std::string text;
		};

		[[noreturn]] void fail(const char* fmt, ...)
		{
			char text[512];
			va_list args;
			va_start(args, fmt);
			std::vsnprintf(text, sizeof(text), fmt, args);
			va_end(args);
			throw failure{ text };
		}

		struct reader
		{
			const std::uint8_t* b = nullptr;
			std::size_t size = 0;

			std::uint32_t u32(const std::size_t at) const
			{
				if (at + 4 > size)
				{
					fail("read past the end of the asset ( offset %zu of %zu )", at, size);
				}
				std::uint32_t v;
				std::memcpy(&v, b + at, 4);
				return v;
			}
			std::uint32_t u16(const std::size_t at) const
			{
				if (at + 2 > size)
				{
					fail("read past the end of the asset ( offset %zu of %zu )", at, size);
				}
				std::uint16_t v;
				std::memcpy(&v, b + at, 2);
				return v;
			}
			std::uint8_t u8(const std::size_t at) const
			{
				if (at >= size)
				{
					fail("read past the end of the asset ( offset %zu of %zu )", at, size);
				}
				return b[at];
			}
			float f32(const std::size_t at) const
			{
				const std::uint32_t bits = u32(at);
				float v;
				std::memcpy(&v, &bits, 4);
				return v;
			}
			void take(std::size_t& p, const std::uint64_t count, const unsigned stride, const char* what) const
			{
				if (p > size || count > (size - p) / stride)
				{
					fail("%s: %llu x %u bytes do not fit the asset", what, static_cast<unsigned long long>(count), stride);
				}
				p += static_cast<std::size_t>(count) * stride;
			}
			void string(std::size_t& p, const char* what) const
			{
				if (p >= size)
				{
					fail("%s: no string", what);
				}
				const void* end = std::memchr(b + p, 0, size - p);
				if (!end)
				{
					fail("%s: string without an end", what);
				}
				p = static_cast<std::size_t>(static_cast<const std::uint8_t*>(end) - b) + 1;
			}
		};

		struct span
		{
			std::size_t offset = 0;
			std::uint32_t count = 0, reference = 0;
		};

		struct layout
		{
			std::uint32_t name_reference = 0;
			span planes, static_models, materials, sides, edges, nodes, leafs, leaf_brushes, leaf_brush_nodes, sub_models, brushes, brush_bounds, brush_contents, aabbs;
		};

		// array of { plane pointer, .. } records of 8 bytes; a plane may be stored behind the array
		void plane_array(const reader& r, std::size_t& p, const std::uint32_t count, const char* what)
		{
			const std::size_t start = p;
			r.take(p, count, 8, what);
			for (std::uint32_t i = 0; i < count; i++)
			{
				if (r.u32(start + 8 * static_cast<std::size_t>(i)) == NONE)
				{
					r.take(p, 1, 20, what);
				}
			}
		}

		// codol_collision_stream.c CODOL_ReadCollisionLayout
		layout read_layout(const reader& r)
		{
			layout l;
			if (r.size < 256)
			{
				fail("col_map: shorter than its header");
			}
			std::uint32_t h[64];
			for (std::size_t i = 0; i < 64; i++)
			{
				h[i] = r.u32(4 * i);
			}
			std::size_t p = 256;
			if (h[0] == NONE)
			{
				r.string(p, "col_map name");
			}
			l.name_reference = h[0];
			l.planes = { h[3] == NONE ? p : static_cast<std::size_t>(-1), h[2], h[3] };
			if (h[3] == NONE)
			{
				r.take(p, h[2], 20, "col_map planes");
			}
			l.static_models = { p, h[4], h[5] };
			r.take(p, h[4], 76, "staticModelList");
			for (std::uint32_t i = 0; i < h[4]; i++)
			{
				const std::uint32_t model = r.u32(l.static_models.offset + 76 * static_cast<std::size_t>(i));
				if (model == NONE || model == NONE - 1)
				{
					fail("staticModelList[%u]: the model is stored behind the entry; the server wants a pointer to a model the zone already has", i);
				}
			}
			l.materials = { p, h[6], h[7] };
			r.take(p, h[6], 12, "materials");
			for (std::uint32_t i = 0; i < h[6]; i++)
			{
				if (r.u32(l.materials.offset + 12 * static_cast<std::size_t>(i)) == NONE)
				{
					r.string(p, "material name");
				}
			}
			l.sides = { p, h[8], h[9] };
			plane_array(r, p, h[8], "brushsides");
			l.edges = { p, h[10], h[11] };
			r.take(p, h[10], 1, "brushEdges");
			l.nodes = { p, h[12], h[13] };
			plane_array(r, p, h[12], "nodes");
			l.leafs = { p, h[14], h[15] };
			r.take(p, h[14], 40, "leafs");
			l.leaf_brushes = { p, h[18], h[19] };
			r.take(p, h[18], 2, "leafbrushes");
			l.leaf_brush_nodes = { p, h[16], h[17] };
			r.take(p, h[16], 20, "leafbrushNodes");
			for (std::uint32_t i = 0; i < h[16]; i++)
			{
				const std::size_t node = l.leaf_brush_nodes.offset + 20 * static_cast<std::size_t>(i);
				const std::uint32_t count = r.u16(node + 2);
				if (count && count < 0x8000 && r.u32(node + 8) == NONE)
				{
					r.take(p, count, 2, "leaf brush list behind the nodes");
				}
			}
			r.take(p, h[20], 4, "leafsurfaces");
			r.take(p, h[22], 12, "verts");
			r.take(p, h[24], 6, "triIndices");
			r.take(p, (static_cast<std::uint64_t>(h[24]) * 3 + 31) / 32, 4, "triEdgeIsWalkable");
			r.take(p, h[27], 28, "borders");
			{
				const std::size_t start = p;
				r.take(p, h[29], 12, "partitions");
				for (std::uint32_t i = 0; i < h[29]; i++)
				{
					if (r.u32(start + 12 * static_cast<std::size_t>(i) + 8) == NONE)
					{
						r.take(p, 1, 28, "partition border");
					}
				}
			}
			l.aabbs = { p, h[31], h[32] };
			r.take(p, h[31], 32, "aabbTrees");
			l.sub_models = { p, h[33], h[34] };
			r.take(p, h[33], 68, "cmodels");
			const std::uint32_t brush_count = h[35] & 0xFFFF;
			l.brushes = { p, brush_count, h[36] };
			r.take(p, brush_count, 36, "brushes");
			for (std::uint32_t i = 0; i < brush_count; i++)
			{
				const std::size_t brush = l.brushes.offset + 36 * static_cast<std::size_t>(i);
				if (r.u32(brush + 4) == NONE)
				{
					plane_array(r, p, r.u16(brush), "brush sides behind a brush");
				}
				if (r.u32(brush + 8) == NONE)
				{
					unsigned edges = 0;
					for (std::size_t j = 0; j < 6; j++)
					{
						edges += r.u8(brush + 30 + j);
					}
					r.take(p, edges, 1, "brush edges behind a brush");
				}
			}
			l.brush_bounds = { p, brush_count, h[37] };
			r.take(p, brush_count, 24, "brushBounds");
			l.brush_contents = { p, brush_count, h[38] };
			r.take(p, brush_count, 4, "brushContents");
			r.take(p, h[40], 28, "smodelNodes");
			if (r.size - p < 44 || (h[39] != NONE && h[39] != NONE - 1))
			{
				fail("col_map: the MapEnts are not stored behind the static model nodes");
			}
			if (r.u32(p) != h[0])
			{
				fail("col_map: the MapEnts do not carry the clip map's name ( the stream is not laid out as the header says )");
			}
			return l;
		}

		// codol_world_planes.c: name and plane array of the GfxWorld
		void read_world(const reader& r, std::uint32_t& name_reference, std::string& base_name, std::size_t& planes, std::uint32_t& plane_count)
		{
			std::size_t p = 0x274;
			if (r.size < p)
			{
				fail("gfx_map: shorter than its header");
			}
			name_reference = r.u32(0);
			if (name_reference == NONE)
			{
				r.string(p, "gfx_map name");
			}
			if (r.u32(4) == NONE)
			{
				const std::size_t start = p;
				r.string(p, "gfx_map base name");
				base_name = reinterpret_cast<const char*>(r.b + start);
			}
			const std::uint32_t count = r.u32(0x14);
			const std::size_t skies = p;
			if (r.u32(0x18))
			{
				r.take(p, count, 16, "gfx_map skies");
				for (std::uint32_t i = 0; i < count; i++)
				{
					const std::size_t sky = skies + 16 * static_cast<std::size_t>(i);
					const std::uint32_t ref = r.u32(sky + 8);
					if (r.u32(sky + 4))
					{
						r.take(p, r.u32(sky), 4, "sky surfaces");
					}
					if (ref == NONE || ref == NONE - 1)
					{
						const std::size_t header = p;
						r.take(p, 1, 36, "sky image");
						if (r.u32(header + 32) == NONE)
						{
							r.string(p, "sky image name");
						}
						if (r.u32(header) == NONE)
						{
							const std::size_t def = p;
							r.take(p, 1, 16, "sky image data");
							r.take(p, r.u32(def + 12), 1, "sky image data");
						}
					}
				}
			}
			else if (count)
			{
				fail("gfx_map: skies without data");
			}
			if (r.u32(0x38) != NONE)
			{
				fail("gfx_map: the plane array is not stored in the GfxWorld");
			}
			planes = p;
			plane_count = r.u32(8);
			r.take(p, plane_count, 20, "gfx_map planes");
		}

		void finite_box(const reader& r, const std::size_t at, const char* what, const std::uint32_t index)
		{
			for (std::size_t j = 0; j < 3; j++)
			{
				const float mid = r.f32(at + 4 * j), half = r.f32(at + 12 + 4 * j);
				if (!std::isfinite(mid) || !std::isfinite(half) || half < 0)
				{
					fail("%s[%u]: box with a half size below zero or a number that is not finite", what, index);
				}
			}
		}

		void check_leaf(const reader& r, const std::size_t at, const layout& l, const char* what, const std::uint32_t index, std::vector<server_leaf>& out)
		{
			const std::uint32_t first = r.u16(at), count = r.u16(at + 2), node = r.u32(at + 36);
			if (first > l.aabbs.count || count > l.aabbs.count - first)
			{
				fail("%s[%u]: collision AABB trees %u + %u of %u", what, index, first, count, l.aabbs.count);
			}
			if (l.leaf_brush_nodes.count ? node >= l.leaf_brush_nodes.count : node != 0)
			{
				fail("%s[%u]: leafBrushNode %u of %u", what, index, node, l.leaf_brush_nodes.count);
			}
			finite_box(r, at + 12, what, index);
			server_leaf leaf;
			leaf.brush_contents = static_cast<std::int32_t>(r.u32(at + 4));
			leaf.node = node;
			for (std::size_t j = 0; j < 3; j++)
			{
				leaf.mins[j] = r.f32(at + 12 + 4 * j) - r.f32(at + 24 + 4 * j);
				leaf.maxs[j] = r.f32(at + 12 + 4 * j) + r.f32(at + 24 + 4 * j);
			}
			out.push_back(leaf);
		}

		void check_pair(const reader& world, const reader& col, server_collision_info& info)
		{
			const layout l = read_layout(col);
			std::uint32_t world_name = 0, plane_count = 0;
			std::size_t plane_offset = 0;
			read_world(world, world_name, info.map, plane_offset, plane_count);
			if (!world_name || world_name != l.name_reference)
			{
				fail("gfx_map and col_map do not share their name ( 0x%08X / 0x%08X )", world_name, l.name_reference);
			}
			if (plane_count != l.planes.count)
			{
				fail("planeCount: GfxWorld %u, clip map %u", plane_count, l.planes.count);
			}
			if (plane_count && (!l.planes.reference || l.planes.reference >= NONE - 1))
			{
				fail("clipMap.planes does not point at the GfxWorld's plane array");
			}
			info.planes = plane_count;
			info.static_models = l.static_models.count;
			info.materials = l.materials.count;
			info.brush_sides = l.sides.count;
			info.brush_edges = l.edges.count;
			info.brushes = l.brushes.count;
			info.leafs = l.leafs.count;
			info.sub_models = l.sub_models.count;
			info.leaf_brush_nodes = l.leaf_brush_nodes.count;
			info.leaf_brushes = l.leaf_brushes.count;

			// ---- static models ( codol_collision_static_models.c, codol_collision_native.c )
			for (std::uint32_t i = 0; i < l.static_models.count; i++)
			{
				const std::size_t at = l.static_models.offset + 76 * static_cast<std::size_t>(i);
				const std::uint32_t ref = col.u32(at);
				if (!ref || ref >= NONE - 1)
				{
					fail("staticModelList[%u]: no model pointer", i);
				}
				for (std::size_t j = 0; j < 12; j++)
				{
					if (!std::isfinite(col.f32(at + 4 + 4 * j)))
					{
						fail("staticModelList[%u]: placement that is not finite", i);
					}
				}
				finite_box(col, at + 52, "staticModelList", i);
			}

			// ---- planes ( codol_collision_brushes.c )
			for (std::uint32_t i = 0; i < plane_count; i++)
			{
				const std::size_t at = plane_offset + 20 * static_cast<std::size_t>(i);
				float length = 0;
				for (std::size_t j = 0; j < 3; j++)
				{
					const float n = world.f32(at + 4 * j);
					if (!std::isfinite(n))
					{
						fail("plane %u: normal that is not finite", i);
					}
					length += n * n;
				}
				if (!std::isfinite(world.f32(at + 12)) || std::fabs(length - 1) > 0.001f)
				{
					fail("plane %u: not a unit normal ( length^2 %.6f ) or a distance that is not finite", i, length);
				}
			}
			const auto plane_reference = [&](const std::uint32_t ref, const char* what, const std::uint32_t index)
			{
				const std::uint32_t base = l.planes.reference;
				if (ref < base || (ref - base) % 20 || (ref - base) / 20 >= plane_count)
				{
					fail("%s[%u]: the plane pointer 0x%08X does not lead into the plane array ( 0x%08X, %u planes )", what, index, ref, base, plane_count);
				}
			};

			// ---- BSP nodes
			if (l.nodes.count > 32768 || l.leafs.count > 32768)
			{
				fail("more than 32768 BSP nodes or leafs");
			}
			for (std::uint32_t i = 0; i < l.nodes.count; i++)
			{
				const std::size_t at = l.nodes.offset + 8 * static_cast<std::size_t>(i);
				plane_reference(col.u32(at), "nodes", i);
				for (std::size_t j = 0; j < 2; j++)
				{
					const std::uint32_t raw = col.u16(at + 4 + 2 * j);
					const int child = raw < 32768 ? static_cast<int>(raw) : static_cast<int>(raw) - 65536;
					if (child >= 0 ? (static_cast<std::uint32_t>(child) <= i || static_cast<std::uint32_t>(child) >= l.nodes.count)
						: static_cast<std::uint32_t>(-1 - child) >= l.leafs.count)
					{
						fail("nodes[%u]: child %d does not lead forward or to a leaf", i, child);
					}
				}
			}

			// ---- brush sides
			for (std::uint32_t i = 0; i < l.sides.count; i++)
			{
				const std::size_t at = l.sides.offset + 8 * static_cast<std::size_t>(i);
				plane_reference(col.u32(at), "brushsides", i);
				if (col.u16(at + 4) >= l.materials.count)
				{
					fail("brushsides[%u]: material %u of %u", i, col.u16(at + 4), l.materials.count);
				}
			}

			// ---- brushes: their sides and edges are runs of the two arrays, back to back in brush order
			std::uint32_t side = 0, edge = 0, side_base = 0, edge_base = 0;
			bool have_side = false, have_edge = false;
			const auto run_reference = [](const std::uint32_t ref, const std::uint32_t offset, std::uint32_t& base, bool& seen)
			{
				if (!ref || ref >= NONE - 1 || ref < offset)
				{
					return false;
				}
				if (!seen)
				{
					base = ref - offset;
					seen = true;
				}
				return static_cast<std::uint64_t>(base) + offset == ref;
			};
			for (std::uint32_t i = 0; i < l.brushes.count; i++)
			{
				const std::size_t raw = l.brushes.offset + 36 * static_cast<std::size_t>(i);
				const std::uint32_t numsides = col.u16(raw);
				if (col.u16(raw + 2) > 1024)
				{
					fail("brushes[%u]: glass piece index %u", i, col.u16(raw + 2));
				}
				if (numsides > l.sides.count - side)
				{
					fail("brushes[%u]: %u sides, but only %u of %u brush sides are left", i, numsides, l.sides.count - side, l.sides.count);
				}
				if (numsides && !run_reference(col.u32(raw + 4), side * 8, side_base, have_side))
				{
					fail("brushes[%u]: its %u sides are not the next run of clipMap.brushsides ( expected entry %u; pointer 0x%08X, array at 0x%08X ). "
						"The sides of the brushes have to lie back to back in brush order", i, numsides, side, col.u32(raw + 4), side_base);
				}
				std::uint32_t edges = 0;
				for (std::size_t j = 0; j < 6; j++)
				{
					const std::uint32_t material = col.u16(raw + 12 + 2 * j);
					if (material >= l.materials.count || material > 32767)
					{
						fail("brushes[%u]: axial material %u of %u", i, material, l.materials.count);
					}
					edges += col.u8(raw + 30 + j);
				}
				for (std::uint32_t j = 0; j < numsides; j++)
				{
					edges += col.u8(l.sides.offset + 8 * static_cast<std::size_t>(side + j) + 7);
				}
				if (edges > l.edges.count - edge)
				{
					fail("brushes[%u]: %u edges, but only %u of %u brush edges are left", i, edges, l.edges.count - edge, l.edges.count);
				}
				if (edges && !run_reference(col.u32(raw + 8), edge, edge_base, have_edge))
				{
					fail("brushes[%u]: its %u edges are not the next run of clipMap.brushEdges ( expected byte %u; pointer 0x%08X, array at 0x%08X ). "
						"The edge lists of the brushes have to lie back to back in brush order", i, edges, edge, col.u32(raw + 8), edge_base);
				}
				for (std::size_t j = 0; j < 6; j++)
				{
					if (static_cast<std::uint32_t>(col.u8(raw + 24 + j)) + col.u8(raw + 30 + j) > edges)
					{
						fail("brushes[%u]: the edge list of an axial side reaches beyond the brush's edges", i);
					}
				}
				for (std::uint32_t j = 0; j < numsides; j++)
				{
					const std::size_t s = l.sides.offset + 8 * static_cast<std::size_t>(side + j);
					if (static_cast<std::uint32_t>(col.u8(s + 6)) + col.u8(s + 7) > edges)
					{
						fail("brushes[%u]: the edge list of side %u reaches beyond the brush's edges", i, j);
					}
				}
				finite_box(col, l.brush_bounds.offset + 24 * static_cast<std::size_t>(i), "brushBounds", i);
				{
					server_brush brush;
					const std::size_t box = l.brush_bounds.offset + 24 * static_cast<std::size_t>(i);
					brush.contents = static_cast<std::int32_t>(col.u32(l.brush_contents.offset + 4 * static_cast<std::size_t>(i)));
					for (std::size_t j = 0; j < 3; j++)
					{
						brush.mins[j] = col.f32(box + 4 * j) - col.f32(box + 12 + 4 * j);
						brush.maxs[j] = col.f32(box + 4 * j) + col.f32(box + 12 + 4 * j);
					}
					brush.sides = numsides;
					brush.edges = edges;
					info.brush_list.push_back(brush);
				}
				side += numsides;
				edge += edges;
			}
			if (side != l.sides.count)
			{
				fail("clipMap.brushsides has %u entries, the brushes use %u: entries no brush names are not allowed", l.sides.count, side);
			}
			if (edge != l.edges.count)
			{
				fail("clipMap.brushEdges has %u bytes, the brushes use %u: bytes no brush names are not allowed", l.edges.count, edge);
			}

			// ---- leafs and brush models ( codol_collision_leaves.c )
			for (std::uint32_t i = 0; i < l.leafs.count; i++)
			{
				check_leaf(col, l.leafs.offset + 40 * static_cast<std::size_t>(i), l, "leafs", i, info.leaf_list);
			}
			if (!l.nodes.count || !l.leafs.count || !l.sub_models.count)
			{
				fail("a clip map without BSP nodes, leafs or brush models");
			}
			for (std::uint32_t i = 0; i < l.sub_models.count; i++)
			{
				const std::size_t at = l.sub_models.offset + 68 * static_cast<std::size_t>(i);
				const float radius = col.f32(at + 24);
				if (!std::isfinite(radius) || radius < 0)
				{
					fail("cmodels[%u]: radius", i);
				}
				finite_box(col, at, "cmodels", i);
				check_leaf(col, at + 28, l, "cmodels", i, info.model_list);
			}

			// ---- leaf brush nodes ( codol_collision_brush_tree.c )
			{
				const std::uint32_t count = l.leaf_brush_nodes.count, pooled = l.leaf_brushes.count;
				for (std::uint32_t i = 0; i < pooled; i++)
				{
					if (col.u16(l.leaf_brushes.offset + 2 * static_cast<std::size_t>(i)) >= l.brushes.count)
					{
						fail("leafbrushes[%u]: brush %u of %u", i, col.u16(l.leaf_brushes.offset + 2 * static_cast<std::size_t>(i)), l.brushes.count);
					}
				}
				std::size_t inline_at = l.leaf_brush_nodes.offset + 20 * static_cast<std::size_t>(count);
				std::uint32_t used = 0, base = 0;
				bool have_base = false;
				for (std::uint32_t i = 0; i < count; i++)
				{
					const std::size_t p = l.leaf_brush_nodes.offset + 20 * static_cast<std::size_t>(i);
					const std::uint32_t raw_count = col.u16(p + 2);
					const int list = raw_count < 32768 ? static_cast<int>(raw_count) : static_cast<int>(raw_count) - 65536;
					server_leaf_brush_node& out = info.nodes.emplace_back();
					out.axis = col.u8(p);
					out.count = list;
					out.contents = static_cast<std::int32_t>(col.u32(p + 4));
					if (list > 0)
					{
						const std::uint32_t ref = col.u32(p + 8);
						if (ref == NONE)
						{
							for (int j = 0; j < list; j++)
							{
								if (col.u16(inline_at + 2 * static_cast<std::size_t>(j)) >= l.brushes.count)
								{
									fail("leafbrushNodes[%u]: its list ( stored behind the nodes ) names brush %u of %u", i, col.u16(inline_at + 2 * static_cast<std::size_t>(j)), l.brushes.count);
								}
								out.brushes.push_back(static_cast<std::uint16_t>(col.u16(inline_at + 2 * static_cast<std::size_t>(j))));
							}
							inline_at += 2 * static_cast<std::size_t>(list);
							info.inline_leaf_brushes += static_cast<std::uint32_t>(list);
							continue;
						}
						if (static_cast<std::uint32_t>(list) > pooled - used || !ref || ref == NONE - 1 || ref < static_cast<std::uint64_t>(used) * 2)
						{
							fail("leafbrushNodes[%u]: a list of %d brushes, but only %u of %u leaf brushes are left ( pointer 0x%08X )", i, list, pooled - used, pooled, ref);
						}
						if (!have_base)
						{
							base = ref - used * 2;
							have_base = true;
						}
						if (static_cast<std::uint64_t>(base) + static_cast<std::uint64_t>(used) * 2 != ref)
						{
							fail("leafbrushNodes[%u]: its list is not the next run of clipMap.leafbrushes ( expected entry %u, the pointer leads to entry %d ). "
								"The lists have to lie back to back in node order", i, used, static_cast<int>((static_cast<std::int64_t>(ref) - base) / 2));
						}
						for (int j = 0; j < list; j++)
						{
							out.brushes.push_back(static_cast<std::uint16_t>(col.u16(l.leaf_brushes.offset + 2 * (static_cast<std::size_t>(used) + static_cast<std::size_t>(j)))));
						}
						used += static_cast<std::uint32_t>(list);
						continue;
					}
					bool zero = i == 0;
					for (std::size_t j = 0; j < 20 && zero; j++)
					{
						zero = col.u8(p + j) == 0;
					}
					if (zero)
					{
						continue;		// the reserved node 0
					}
					if (list < -1 || col.u8(p) > 2 || (list < 0 && i + 1 >= count))
					{
						fail("leafbrushNodes[%u]: leafBrushCount %d, axis %u", i, list, col.u8(p));
					}
					const float dist = col.f32(p + 8), range = col.f32(p + 12);
					if (!std::isfinite(dist) || !std::isfinite(range) || range < 0)
					{
						fail("leafbrushNodes[%u]: dist %g, range %g", i, dist, range);
					}
					for (std::size_t j = 0; j < 2; j++)
					{
						const std::uint32_t offset = col.u16(p + 16 + 2 * j);
						if (!offset || offset >= count - i)
						{
							fail("leafbrushNodes[%u]: a node that lists no brushes has to split, but child offset %zu is %u ( %u nodes ). There is no empty node", i, j, offset, count);
						}
					}
					out.dist = dist;
					out.range = range;
					out.child[0] = i + col.u16(p + 16);
					out.child[1] = i + col.u16(p + 18);
				}
				if (used != pooled)
				{
					fail("clipMap.leafbrushes has %u entries, the lists of the nodes use %u: entries no node names are not allowed", pooled, used);
				}
			}
		}
	}

	bool check_server_collision(const std::span<const std::uint8_t> zone, std::string& error, server_collision_info* info)
	try
	{
		server_collision_info local;
		const reader z{ zone.data(), zone.size() };
		if (zone.size() < 56)
		{
			fail("not a zone");
		}
		// XFile header ( 40 bytes ), XAssetList: script string count and pointer, asset count and pointer
		const std::uint32_t string_count = z.u32(40), asset_count = z.u32(48);
		std::size_t p = 56;
		z.take(p, string_count, 4, "script strings");
		for (std::uint32_t i = 0; i < string_count; i++)
		{
			if (z.u32(56 + 4 * static_cast<std::size_t>(i)) == NONE)
			{
				z.string(p, "script string");
			}
		}
		const std::size_t table = p;
		z.take(p, asset_count, 16, "asset table");
		reader world, col;
		int worlds = 0, cols = 0;
		for (std::uint32_t i = 0; i < asset_count; i++)
		{
			const std::size_t entry = table + 16 * static_cast<std::size_t>(i);
			const std::uint32_t type = z.u32(entry), offset = z.u32(entry + 4), extent = z.u32(entry + 12);
			if (type != 21 && type != 14 && type != 15)
			{
				continue;
			}
			if (static_cast<std::size_t>(offset) + 40 > zone.size() || extent > zone.size() - offset - 40)
			{
				fail("asset %u: its stream range lies outside the zone", i);
			}
			const reader asset{ zone.data() + offset + 40, extent };
			if (type == 21)
			{
				world = asset;
				worlds++;
			}
			else
			{
				col = asset;
				cols++;
			}
		}
		if (worlds != 1 || cols != 1)
		{
			fail("the zone holds %d gfx_map and %d col_map assets ( one of each is a map )", worlds, cols);
		}
		check_pair(world, col, local);
		if (info)
		{
			*info = local;
		}
		return true;
	}
	catch (const failure& f)
	{
		error = f.text;
		return false;
	}
}
