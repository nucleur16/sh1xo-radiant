// clip_trace: see trace.hpp. Own code over the zone data, not the game's.

#include "codol_mapc/trace.hpp"

#include "mapc_impl.hpp"

namespace codol::mapc
{
	std::vector<c1::cLeafBrushNode_s> stream_leaf_brush_nodes(const ZoneEditor& editor, const zone::ClipMapView& clip)
	{
		const ZoneDataId id = editor.data_at(clip.raw.leafbrushNodes);
		return id ? array_of<c1::cLeafBrushNode_s>(editor.data_bytes(id)) : std::vector<c1::cLeafBrushNode_s>();
	}

	clip_trace::clip_trace(const ZoneEditor& editor)
	{
		const Zone& zone = editor.zone();
		zone::ClipMapView clip;
		zaddr address = zone::first_asset(zone, c1::ASSET_TYPE_CLIPMAP_MP);
		if (!address)
		{
			address = zone::first_asset(zone, c1::ASSET_TYPE_CLIPMAP_SP);
		}
		if (!address || !zone::read_clip_map(zone, address, clip))
		{
			return;
		}
		brushes_.resize(clip.brushes.size());
		for (std::size_t i = 0; i < clip.brushes.size(); i++)
		{
			const zone::Brush& from = clip.brushes[i];
			brush& b = brushes_[i];
			for (int k = 0; k < 3; k++)
			{
				b.mins[k] = from.bounds.midPoint[k] - from.bounds.halfSize[k];
				b.maxs[k] = from.bounds.midPoint[k] + from.bounds.halfSize[k];
			}
			b.contents = from.contents;
			for (std::size_t s = 6; s < from.sides.size(); s++)
			{
				b.sides.push_back({ { from.sides[s].plane.normal[0], from.sides[s].plane.normal[1], from.sides[s].plane.normal[2] }, from.sides[s].plane.dist });
			}
		}
		nodes_ = stream_leaf_brush_nodes(editor, clip);
		const auto image = zone.get_array<c1::cLeafBrushNode_s>(clip.raw.leafbrushNodes, clip.raw.leafbrushNodesCount);
		if (nodes_.size() != image.size())
		{
			return;
		}
		lists_.resize(nodes_.size());
		for (std::size_t n = 0; n < nodes_.size(); n++)
		{
			if (nodes_[n].leafBrushCount > 0)
			{
				// the list pointer is a zone pointer: its address is in the image
				lists_[n] = zone.get_array<std::uint16_t>(rd<zaddr>({ image[n].data, 12 }, 0), static_cast<std::size_t>(nodes_[n].leafBrushCount));
			}
		}
		leafs_ = zone.get_array<c1::cLeaf_t>(clip.raw.leafs, clip.raw.numLeafs);
		models_ = clip.subModels;
		const auto raw_nodes = zone.get_array<cnode>(clip.raw.nodes, clip.raw.numNodes);
		bsp_.resize(raw_nodes.size());
		for (std::size_t n = 0; n < raw_nodes.size(); n++)
		{
			const c1::cplane_s p = zone.get<c1::cplane_s>(raw_nodes[n].plane);
			bsp_[n] = { { { p.normal[0], p.normal[1], p.normal[2] }, p.dist }, { raw_nodes[n].children[0], raw_nodes[n].children[1] } };
		}
		std::vector<std::uint8_t> listed(brushes_.size(), 0);
		for (const c1::cLeaf_t& leaf : leafs_)
		{
			if (leaf.leafBrushNode > 0)
			{
				std::vector<int> found;
				segment all{};
				trace_result unused;
				walk_tree(leaf.leafBrushNode, all, unused, &found);
				for (const int b : found)
				{
					listed[static_cast<std::size_t>(b)] = 1;
				}
			}
		}
		for (std::size_t b = 0; b < listed.size(); b++)
		{
			if (listed[b])
			{
				world_brushes_.push_back(static_cast<int>(b));
			}
		}
		valid_ = true;
	}

	// segment ( of a box ) against the planes of a convex brush: latest entry, earliest exit
	void clip_trace::test_brush(const int index, const segment& s, trace_result& out) const
	{
		if (index < 0 || index >= static_cast<int>(brushes_.size()))
		{
			return;
		}
		const brush& b = brushes_[static_cast<std::size_t>(index)];
		if ((b.contents & s.mask) == 0)
		{
			return;
		}
		out.brushes_tested++;
		float enter = -1.0f, leave = 1.0f;
		bool starts_out = false, ends_out = false;
		const auto side = [&](const float n[3], const float dist)
		{
			const float grow = std::fabs(n[0]) * s.half[0] + std::fabs(n[1]) * s.half[1] + std::fabs(n[2]) * s.half[2];
			const float d1 = n[0] * s.start[0] + n[1] * s.start[1] + n[2] * s.start[2] - dist - grow;
			const float d2 = n[0] * s.end[0] + n[1] * s.end[1] + n[2] * s.end[2] - dist - grow;
			starts_out = starts_out || d1 > 0;
			ends_out = ends_out || d2 > 0;
			if (d1 > 0 && d2 > 0)
			{
				return false;		// in front of the plane all the way
			}
			if (d1 <= 0 && d2 <= 0)
			{
				return true;
			}
			const float f = d1 / (d1 - d2);
			if (d1 > d2)
			{
				enter = std::max(enter, f);
			}
			else
			{
				leave = std::min(leave, f);
			}
			return true;
		};
		for (int k = 0; k < 3; k++)
		{
			float n[3] = {};
			n[k] = -1.0f;
			if (!side(n, -b.mins[k]))
			{
				return;
			}
			n[k] = 1.0f;
			if (!side(n, b.maxs[k]))
			{
				return;
			}
		}
		for (const plane& p : b.sides)
		{
			if (!side(p.normal, p.dist))
			{
				return;
			}
		}
		if (!starts_out)
		{
			out.start_solid = true;
			if (out.fraction > 0)
			{
				out.fraction = 0;
				out.brush = index;
			}
			return;
		}
		if (enter < leave && enter > -1.0f && enter < out.fraction)
		{
			out.fraction = std::max(enter, 0.0f);
			out.brush = index;
		}
	}

	// the leaf brush node tree: a leaf lists brushes; an inner node splits along an axis at `dist`, child 0 holds
	// what lies at dist + range and above, child 1 what lies at dist - range and below; a node with a negative count
	// is followed by a node of brushes that straddle the split
	void clip_trace::walk_tree(int node, const segment& s, trace_result& out, std::vector<int>* collect, const int depth) const
	{
		for (int guard = 0; guard < 100000 && depth < 256; guard++)
		{
			if (node < 0 || node >= static_cast<int>(nodes_.size()))
			{
				return;
			}
			const c1::cLeafBrushNode_s& n = nodes_[static_cast<std::size_t>(node)];
			if (!collect && (n.contents & s.mask) == 0)
			{
				return;
			}
			if (n.leafBrushCount > 0)
			{
				for (const std::uint16_t b : lists_[static_cast<std::size_t>(node)])
				{
					if (collect)
					{
						collect->push_back(b);
					}
					else
					{
						test_brush(b, s, out);
					}
				}
				return;
			}
			if (n.leafBrushCount < 0)
			{
				walk_tree(node + 1, s, out, collect, depth + 1);
			}
			const float dist = rd<float>({ n.data, 12 }, 0), range = rd<float>({ n.data, 12 }, 4);
			const std::uint16_t front = rd<std::uint16_t>({ n.data, 12 }, 8), back = rd<std::uint16_t>({ n.data, 12 }, 10);
			if (!front && !back)
			{
				return;
			}
			if (collect)
			{
				if (front)
				{
					walk_tree(node + front, s, out, collect, depth + 1);
				}
				if (!back || back == front)
				{
					return;
				}
				node += back;
				continue;
			}
			const int axis = n.axis < 3 ? n.axis : 0;
			const float t1 = s.start[axis] - dist, t2 = s.end[axis] - dist;
			const float offset = s.half[axis] + 0.125f - range;
			if (std::min(t1, t2) >= offset)
			{
				if (!front)
				{
					return;
				}
				node += front;
				continue;
			}
			if (std::max(t1, t2) <= -offset)
			{
				if (!back)
				{
					return;
				}
				node += back;
				continue;
			}
			// both sides ( the whole segment each: more brushes tested than needed, the same result )
			if (front)
			{
				walk_tree(node + front, s, out, collect, depth + 1);
			}
			if (!back || back == front)
			{
				return;
			}
			node += back;
		}
	}

	void clip_trace::walk_bsp(int node, const segment& s, trace_result& out, const int depth) const
	{
		for (int guard = 0; guard < 100000 && depth < 512; guard++)
		{
			if (node < 0)
			{
				const std::size_t leaf = static_cast<std::size_t>(-1 - node);
				if (leaf < leafs_.size() && (leafs_[leaf].brushContents & s.mask) != 0 && leafs_[leaf].leafBrushNode > 0)
				{
					// the leaf's box is the first test
					const c1::cLeaf_t& l = leafs_[leaf];
					bool apart = false;
					for (int k = 0; k < 3; k++)
					{
						const float lo = std::min(s.start[k], s.end[k]) - s.half[k] - 0.125f, hi = std::max(s.start[k], s.end[k]) + s.half[k] + 0.125f;
						apart = apart || lo > l.bounds.midPoint[k] + l.bounds.halfSize[k] || hi < l.bounds.midPoint[k] - l.bounds.halfSize[k];
					}
					if (!apart)
					{
						walk_tree(l.leafBrushNode, s, out, nullptr);
					}
				}
				return;
			}
			if (node >= static_cast<int>(bsp_.size()))
			{
				return;
			}
			const bsp_node& n = bsp_[static_cast<std::size_t>(node)];
			const float grow = std::fabs(n.p.normal[0]) * s.half[0] + std::fabs(n.p.normal[1]) * s.half[1] + std::fabs(n.p.normal[2]) * s.half[2] + 0.125f;
			const float d1 = n.p.normal[0] * s.start[0] + n.p.normal[1] * s.start[1] + n.p.normal[2] * s.start[2] - n.p.dist;
			const float d2 = n.p.normal[0] * s.end[0] + n.p.normal[1] * s.end[1] + n.p.normal[2] * s.end[2] - n.p.dist;
			if (d1 >= grow && d2 >= grow)
			{
				node = n.children[0];
				continue;
			}
			if (d1 < -grow && d2 < -grow)
			{
				node = n.children[1];
				continue;
			}
			walk_bsp(n.children[0], s, out, depth + 1);
			node = n.children[1];
		}
	}

	namespace
	{
		template <typename S>
		S make_segment(const float start[3], const float end[3], const int mask, const float half[3])
		{
			S s{};
			for (int k = 0; k < 3; k++)
			{
				s.start[k] = start[k];
				s.end[k] = end[k];
				s.half[k] = half ? half[k] : 0.0f;
			}
			s.mask = mask;
			return s;
		}
	}

	trace_result clip_trace::trace(const float start[3], const float end[3], const int mask, const float half[3]) const
	{
		trace_result out;
		if (valid_ && !bsp_.empty())
		{
			walk_bsp(0, make_segment<segment>(start, end, mask, half), out);
		}
		return out;
	}

	trace_result clip_trace::trace_all(const float start[3], const float end[3], const int mask, const float half[3]) const
	{
		trace_result out;
		const segment s = make_segment<segment>(start, end, mask, half);
		for (const int b : world_brushes_)
		{
			test_brush(b, s, out);
		}
		return out;
	}

	std::vector<int> clip_trace::model_brushes(const int sub_model) const
	{
		std::vector<int> found;
		if (sub_model >= 0 && sub_model < static_cast<int>(models_.size()) && models_[static_cast<std::size_t>(sub_model)].leaf.leafBrushNode > 0)
		{
			segment all{};
			trace_result unused;
			walk_tree(models_[static_cast<std::size_t>(sub_model)].leaf.leafBrushNode, all, unused, &found);
			std::sort(found.begin(), found.end());
			found.erase(std::unique(found.begin(), found.end()), found.end());
		}
		return found;
	}

	trace_result clip_trace::trace_model(const int sub_model, const float start[3], const float end[3], const int mask, const bool through_tree) const
	{
		trace_result out;
		if (sub_model < 0 || sub_model >= static_cast<int>(models_.size()))
		{
			return out;
		}
		const segment s = make_segment<segment>(start, end, mask, nullptr);
		if (through_tree)
		{
			const c1::cLeaf_t& leaf = models_[static_cast<std::size_t>(sub_model)].leaf;
			if ((leaf.brushContents & mask) != 0 && leaf.leafBrushNode > 0)
			{
				walk_tree(leaf.leafBrushNode, s, out, nullptr);
			}
			return out;
		}
		for (const int b : model_brushes(sub_model))
		{
			test_brush(b, s, out);
		}
		return out;
	}

	std::vector<int> clip_trace::leafs_of(const int brush) const
	{
		std::vector<int> out;
		for (std::size_t l = 0; l < leafs_.size(); l++)
		{
			if (leafs_[l].leafBrushNode > 0)
			{
				std::vector<int> found;
				segment all{};
				trace_result unused;
				walk_tree(leafs_[l].leafBrushNode, all, unused, &found);
				if (std::find(found.begin(), found.end(), brush) != found.end())
				{
					out.push_back(static_cast<int>(l));
				}
			}
		}
		return out;
	}
}
