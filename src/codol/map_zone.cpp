#include "std_include.hpp"

// windows.h macros
#undef min
#undef max

#include "codol/map_zone.hpp"

#include <cmath>

#include <codol_zone/fastfile.hpp>
#include <codol_zone/zone.hpp>
#include <codol_zone/assets.hpp>

namespace codol::mapgeo
{
	namespace
	{
		// brushes of a sub model: its leaf points at a tree of leaf brush nodes, the leaves of which list brush indices
		void collect_leaf_brushes(const zone::Zone& z, const std::vector<c1::cLeafBrushNode_s>& nodes, const std::size_t index,
			std::vector<std::uint16_t>& out, const int depth = 0)
		{
			if (index >= nodes.size() || depth > 64)
			{
				return;
			}

			const auto& node = nodes[index];

			if (node.leafBrushCount > 0)
			{
				// leaf: data = address of leafBrushCount brush indices
				std::uint32_t address = 0;
				std::memcpy(&address, node.data, 4);

				const auto brushes = z.get_array<std::uint16_t>(address, static_cast<std::size_t>(node.leafBrushCount));
				out.insert(out.end(), brushes.begin(), brushes.end());
				return;
			}

			// a negative count: that many nodes directly after this one belong to it as well
			for (int k = 1; k <= -node.leafBrushCount; k++)
			{
				collect_leaf_brushes(z, nodes, index + static_cast<std::size_t>(k), out, depth + 1);
			}

			// node: float dist, float range, u16 childOffset[2] ( relative to this node )
			std::uint16_t child[2] = {};
			std::memcpy(child, node.data + 8, 4);

			for (const auto offset : child)
			{
				if (offset != 0)
				{
					collect_leaf_brushes(z, nodes, index + offset, out, depth + 1);
				}
			}
		}
	}

	bool read_zone_map(const std::filesystem::path& fastfile, const std::filesystem::path& client_exe, zone_map& out, std::string& error)
	{
		out = {};

		zone::ClientKey key;
		if (const auto status = key.load_from_exe(client_exe); !status)
		{
			error = "client key: " + status.message;
			return false;
		}

		zone::Zone z;
		if (const auto status = z.open(fastfile, key); !status)
		{
			error = status.message;
			return false;
		}

		// ---- entities
		zone::MapEntsView ents;
		if (const auto address = zone::find_map_ents(z); !address || !zone::read_map_ents(z, address, ents))
		{
			error = "the zone has no map entities ( not a map zone )";
			return false;
		}

		out.entity_text = std::string(ents.entityString);
		out.trigger_models = static_cast<int>(ents.triggerModelCount);

		// ---- collision brushes
		auto clip_address = zone::first_asset(z, c1::ASSET_TYPE_CLIPMAP_MP);
		if (!clip_address)
		{
			clip_address = zone::first_asset(z, c1::ASSET_TYPE_CLIPMAP_SP);
		}

		zone::ClipMapView clip;
		if (clip_address && zone::read_clip_map(z, clip_address, clip))
		{
			// which sub model owns which brush ( everything else is the world )
			std::vector<int> owner(clip.brushes.size(), 0);
			const auto nodes = z.get_array<c1::cLeafBrushNode_s>(clip.raw.leafbrushNodes, clip.raw.leafbrushNodesCount);

			for (std::size_t m = 1; m < clip.subModels.size(); m++)
			{
				const auto node = clip.subModels[m].leaf.leafBrushNode;
				if (node <= 0)
				{
					continue;
				}

				std::vector<std::uint16_t> indices;
				collect_leaf_brushes(z, nodes, static_cast<std::size_t>(node), indices);

				for (const auto index : indices)
				{
					if (index < owner.size())
					{
						owner[index] = static_cast<int>(m);
					}
				}
			}

			out.geometry.brushes.reserve(clip.brushes.size());

			for (std::size_t i = 0; i < clip.brushes.size(); i++)
			{
				const auto& src = clip.brushes[i];

				brush b;
				b.index = static_cast<int>(i);
				b.sub_model = owner[i];
				b.contents = static_cast<std::uint32_t>(src.contents);

				for (const auto& side : src.sides)
				{
					brush_side s;
					s.normal[0] = side.plane.normal[0];
					s.normal[1] = side.plane.normal[1];
					s.normal[2] = side.plane.normal[2];
					s.dist = side.plane.dist;
					s.material = side.materialNum < clip.materials.size() ? std::string(clip.materials[side.materialNum].name) : std::string();

					if (s.material.empty())
					{
						s.material = "caulk";
					}

					// the same plane twice ( an axial side that is also stored ) would only confuse the editor
					bool duplicate = false;
					for (const auto& have : b.sides)
					{
						if (std::fabs(have.dist - s.dist) < 0.01 && std::fabs(have.normal[0] - s.normal[0]) < 1e-4
							&& std::fabs(have.normal[1] - s.normal[1]) < 1e-4 && std::fabs(have.normal[2] - s.normal[2]) < 1e-4)
						{
							duplicate = true;
							break;
						}
					}

					if (!duplicate)
					{
						b.sides.push_back(std::move(s));
					}
				}

				if (b.sides.size() >= 4)
				{
					out.geometry.brushes.push_back(std::move(b));
				}
			}
		}

		// ---- static models of the render world
		zone::GfxWorldView world;
		if (const auto address = zone::first_asset(z, c1::ASSET_TYPE_GFXWORLD); address && zone::read_gfx_world(z, address, world))
		{
			out.world_name = std::string(world.name);
			out.geometry.models.reserve(world.staticModels.size());

			for (const auto& inst : world.staticModels)
			{
				if (inst.modelName.empty())
				{
					continue;
				}

				// rows: forward, left, up, translation
				double m[16] = {};
				for (int r = 0; r < 3; r++)
				{
					for (int c = 0; c < 3; c++)
					{
						m[r * 4 + c] = inst.raw.axis[r][c];
					}
				}

				m[12] = inst.raw.origin[0];
				m[13] = inst.raw.origin[1];
				m[14] = inst.raw.origin[2];
				m[15] = 1.0;

				static_model sm;
				sm.model = std::string(inst.modelName);
				sm.origin[0] = m[12];
				sm.origin[1] = m[13];
				sm.origin[2] = m[14];
				angles_from_axes(m, sm.angles);
				sm.scale = inst.raw.scale > 0.0f ? inst.raw.scale : 1.0;

				out.geometry.models.push_back(std::move(sm));
			}
		}

		return true;
	}
}
