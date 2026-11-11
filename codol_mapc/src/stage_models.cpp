// Stage 2: static models moved, deleted, added.
//
// What the zone holds for a static model ( established on the shipped maps with codol_mapc_probe, and from
// CODOL-AssetExplorer Porting/V3ToV1/MapEdits.cs, which drops static models from ported maps ):
//
//   GfxWorld.dpvs.smodelDrawInsts   placement, scale, model, cull distance, lighting ( 76 bytes )
//   GfxWorld.dpvs.smodelInsts       world bounds and lighting origin ( 36 bytes )
//   the cells' GfxAabbTree nodes    u16 lists of static model indices. A node's list is the lists of its children
//                                   back to back; the zone stores the root's list and lets every other node point
//                                   into it ( the client relies on that when it sorts the models at load )
//   GfxWorld shadow geometry        per primary light the static models whose primaryLightIndex it is
//   clipMap.staticModelList         the models that have collision: inverse scaled axis, bounds
//   clipMap.smodelNodes             AABB tree over that list; its leaves name runs of the list
//
// The XModel of a static model is *nested* in the gfx_map asset: the first instance of a model carries it
// ( its model field says "the data follows" ), every other user - later instances, the clip map's list, other
// assets - points at that field. The instances that carry a model therefore keep their order.

#include "mapc_impl.hpp"

#include "light_grid.hpp"
#include "sight_trace.hpp"

#include <functional>
#include <numeric>
#include <optional>
#include <set>

namespace codol::mapc
{
	namespace
	{
		constexpr std::uint32_t DRAW_SIZE = sizeof(c1::GfxStaticModelDrawInst);
		constexpr std::uint32_t DRAW_MODEL = offsetof(c1::GfxStaticModelDrawInst, model);
		constexpr std::uint32_t FOLLOWS = 0xFFFFFFFFu;
		constexpr std::uint8_t FLAG_GROUND_LIGHTING = 0x20;

		struct instance
		{
			c1::GfxStaticModelDrawInst draw{};
			c1::GfxStaticModelInst inst{};
			zaddr model = 0;				// image address of the XModel header ( 0: a model the zone did not have )
			std::string name;
			bool carries_model = false;		// the nested XModel follows this instance
			std::optional<ZoneTarget> pointer;	// model field when it is a zone pointer
			int old_index = -1;				// instance of the base zone this record is ( -1: added )
			bool hidden = false;			// stays in the arrays, listed nowhere
			bool relit = false;				// placement changed: cells, lighting
		};

		void axis_of(const edit::vec3& angles, float axis[3][3])
		{
			edit::vec3 axes[3];
			edit::angles_to_axes(angles, axes);
			for (int a = 0; a < 3; a++)
			{
				for (int k = 0; k < 3; k++)
				{
					axis[a][k] = static_cast<float>(axes[a][k]);
				}
			}
		}

		// world box of a model box placed with origin, axis ( rows forward, left, up ) and scale
		box placed_box(const model_box& local, const float origin[3], const float axis[3][3], const float scale)
		{
			box out;
			for (int k = 0; k < 3; k++)
			{
				float mid = origin[k], half = 0;
				for (int a = 0; a < 3; a++)
				{
					mid += axis[a][k] * scale * local.mid[a];
					half += std::fabs(axis[a][k]) * scale * local.half[a];
				}
				out.mins[k] = mid - half;
				out.maxs[k] = mid + half;
			}
			return out;
		}

		model_box xmodel_box(const Zone& zone, const zaddr model)
		{
			model_box out;
			const auto x = zone.bytes(model, sizeof(c1::XModel));
			if (model && x.size() == sizeof(c1::XModel) && !zone.reference(model))
			{
				out.radius = rd<float>(x, XMODEL_RADIUS);
				for (int k = 0; k < 3; k++)
				{
					out.mid[k] = rd<float>(x, XMODEL_BOUNDS + 4 * k);
					out.half[k] = rd<float>(x, XMODEL_BOUNDS + 12 + 4 * k);
				}
				out.known = true;
			}
			return out;
		}

		// does the XModel have collision surfaces ( the models of the shipped maps that do are the ones with an entry
		// in clipMap.staticModelList )
		bool xmodel_collides(const Zone& zone, const zaddr model)
		{
			return model && !zone.reference(model) && zone.get<c1::XModel>(model).numCollSurfs > 0;
		}

		// the vertices of the highest LOD, in model space; empty when the surfaces are not in the zone
		std::vector<std::array<float, 3>> xmodel_points(const Zone& zone, const zaddr model)
		{
			std::vector<std::array<float, 3>> points;
			zone::ModelView view;
			if (!model || zone.reference(model) || !zone::read_model(zone, model, view, true) || view.lods.empty())
			{
				return points;
			}
			for (const zone::SurfaceView& surface : view.lods[0].surfs.surfaces)
			{
				for (std::size_t v = 0; v + 32 <= surface.vertexBytes.size(); v += 32)
				{
					std::array<float, 3> p{};
					std::memcpy(p.data(), surface.vertexBytes.data() + v, 12);
					points.push_back(p);
				}
			}
			return points;
		}

		// world box of model space points placed with origin, axis and scale
		box placed_points(const std::vector<std::array<float, 3>>& points, const float origin[3], const float axis[3][3], const float scale)
		{
			box out;
			for (const auto& p : points)
			{
				float w[3];
				for (int k = 0; k < 3; k++)
				{
					w[k] = origin[k] + scale * (p[0] * axis[0][k] + p[1] * axis[1][k] + p[2] * axis[2][k]);
				}
				out.add(w);
			}
			return out;
		}

		// A box in the space of a placed model that holds the world box `world` ( the corners taken back through the
		// placement ). Placing it again gives a box that holds whatever `world` held: larger than a tight one, never smaller.
		model_box local_box_of(const box& world, const float origin[3], const float axis[3][3], const float scale)
		{
			box local;
			for (int corner = 0; corner < 8; corner++)
			{
				float d[3], p[3];
				for (int k = 0; k < 3; k++)
				{
					d[k] = ((corner >> k) & 1 ? world.maxs[k] : world.mins[k]) - origin[k];
				}
				for (int a = 0; a < 3; a++)
				{
					p[a] = (d[0] * axis[a][0] + d[1] * axis[a][1] + d[2] * axis[a][2]) / scale;
				}
				local.add(p);
			}
			model_box out;
			const c1::Bounds b = local.bounds();
			std::memcpy(out.mid, b.midPoint, 12);
			std::memcpy(out.half, b.halfSize, 12);
			out.known = true;
			return out;
		}

		float box_error(const box& a, const box& b)
		{
			float worst = 0;
			for (int k = 0; k < 3; k++)
			{
				worst = std::max({ worst, std::fabs(a.mins[k] - b.mins[k]), std::fabs(a.maxs[k] - b.maxs[k]) });
			}
			return worst;
		}

		// ---- lighting of a placed model, the way the renderer's load_map computes it -----------------------

		struct lighting
		{
			std::optional<view::light_grid> grid;
			std::unique_ptr<view::world_sight_trace> trace;
			std::vector<view::map_light> lights;
			std::vector<view::float3> probes;
			int last_sun_light = 0;

			explicit lighting(const context& ctx)
			{
				const Zone& zone = ctx.zone();
				last_sun_light = static_cast<int>(ctx.gfx.raw.lastSunPrimaryLightIndex);
				grid = view::light_grid::read(zone, ctx.gfx_address);
				trace = std::make_unique<view::world_sight_trace>(zone);
				// codol_view renderer_load.cpp: ComWorld primary lights, 68 byte records
				zone::ComWorldView com;
				if (const zaddr com_world = zone::first_asset(zone, c1::ASSET_TYPE_COMWORLD); com_world && zone::read_com_world(zone, com_world, com) && com.primaryLightStride >= 68)
				{
					for (std::uint32_t i = 0; i < com.raw.primaryLightCount; i++)
					{
						const auto l = com.primaryLightBytes.subspan(static_cast<std::size_t>(i) * com.primaryLightStride, 68);
						view::map_light light;
						light.index = static_cast<int>(i);
						light.type = l[0];
						light.can_use_shadow_map = l[1] != 0;
						light.exponent = l[2];
						light.color = { rd<float>(l, 4), rd<float>(l, 8), rd<float>(l, 12) };
						light.direction = { rd<float>(l, 16), rd<float>(l, 20), rd<float>(l, 24) };
						light.origin = { rd<float>(l, 28), rd<float>(l, 32), rd<float>(l, 36) };
						light.radius = rd<float>(l, 40);
						light.cos_half_fov_outer = rd<float>(l, 44);
						light.cos_half_fov_inner = rd<float>(l, 48);
						light.cos_half_fov_expanded = rd<float>(l, 52);
						light.rotation_limit = rd<float>(l, 56);
						lights.push_back(std::move(light));
					}
				}
				const std::size_t count = ctx.gfx.raw.draw.reflectionProbeCount;
				const auto origins = zone.get_array<float>(ctx.gfx.raw.draw.reflectionProbeOrigins, 3 * count);
				for (std::size_t i = 0; i < count && !origins.empty(); i++)
				{
					probes.push_back({ origins[3 * i], origins[3 * i + 1], origins[3 * i + 2] });
				}
			}

			// codol_view map_renderer::impl::nearest_probe
			int nearest_probe(const float p[3]) const
			{
				int best = 0;
				float best_distance = 3.4e38f;
				for (std::size_t i = probes.size() > 1 ? 1 : 0; i < probes.size(); i++)
				{
					const float dx = probes[i].x - p[0], dy = probes[i].y - p[1], dz = probes[i].z - p[2];
					const float distance = dx * dx + dy * dy + dz * dz;
					if (distance < best_distance)
					{
						best_distance = distance;
						best = static_cast<int>(i);
					}
				}
				return best;
			}

			// primary light, reflection probe and, for a model with baked ground lighting, its colour
			void apply(instance& i)
			{
				i.draw.reflectionProbeIndex = static_cast<std::uint8_t>(nearest_probe(i.inst.lightingOrigin));
				if (!grid)
				{
					return;
				}
				const view::float3 origin{ i.inst.lightingOrigin[0], i.inst.lightingOrigin[1], i.inst.lightingOrigin[2] };
				const auto sample = grid->sample(origin, i.draw.primaryLightIndex, lights, last_sun_light,
					[this](const view::float3 start, const view::float3 end) { return trace->blocked(start, end); });
				if (sample.primary_light >= 0 && sample.primary_light < 256)
				{
					i.draw.primaryLightIndex = static_cast<std::uint8_t>(sample.primary_light);
				}
				if ((i.draw.flags & FLAG_GROUND_LIGHTING) != 0 && !sample.is_fallback)
				{
					const std::uint32_t ground = view::light_grid::ground_lighting(grid->blend(sample), sample.sun_visibility);
					std::memcpy(i.draw.groundLighting, &ground, 4);
				}
			}
		};

		// ---- one cell's AABB tree with its static model lists ----------------------------------------------

		struct cell_tree
		{
			ZoneDataId nodes_id = 0;
			ZoneDataId list_id = 0;						// the root's list; 0: the cell lists no model
			std::vector<aabb_node> nodes;
			std::vector<std::vector<int>> items;		// per node without children: the models it lists ( new indices )
			std::vector<int> parent;
			bool changed = false;

			int first_child(const int n) const { return n + nodes[static_cast<std::size_t>(n)].childrenOffset / 44; }
		};

		cell_tree read_cell_tree(const context& ctx, const int cell)
		{
			const Zone& zone = ctx.zone();
			cell_tree tree;
			const zaddr counts = zone.get<zaddr>(ctx.gfx_address + WORLD_AABB_TREE_COUNTS), trees = zone.get<zaddr>(ctx.gfx_address + WORLD_AABB_TREES);
			const int count = zone.get<std::int32_t>(counts + 4u * static_cast<std::uint32_t>(cell));
			const zaddr address = zone.get<zaddr>(trees + 4u * static_cast<std::uint32_t>(cell));
			tree.nodes = zone.get_array<aabb_node>(address, static_cast<std::size_t>(std::max(count, 0)));
			if (tree.nodes.empty())
			{
				return tree;
			}
			tree.nodes_id = ctx.data(address, "cell AABB tree");
			const aabb_node& root = tree.nodes[0];
			const zaddr root_list = root.smodelIndexes;
			if (root.smodelIndexCount)
			{
				tree.list_id = ctx.data(root_list, "cell static model list");
			}
			tree.items.resize(tree.nodes.size());
			tree.parent.assign(tree.nodes.size(), -1);
			const int total = static_cast<int>(tree.nodes.size());
			for (int n = 0; n < total; n++)
			{
				const aabb_node& node = tree.nodes[static_cast<std::size_t>(n)];
				// the run of the root's list this node names
				int start = 0;
				if (node.smodelIndexCount)
				{
					if (node.smodelIndexes < root_list || (node.smodelIndexes - root_list) % 2 != 0
						|| (node.smodelIndexes - root_list) / 2 + node.smodelIndexCount > root.smodelIndexCount)
					{
						throw compile_error(format("cell %d: the static model list of AABB node %d is not a run of the root's list", cell, n));
					}
					start = static_cast<int>((node.smodelIndexes - root_list) / 2);
				}
				if (node.childCount == 0)
				{
					const auto list = zone.get_array<std::uint16_t>(node.smodelIndexes, node.smodelIndexCount);
					tree.items[static_cast<std::size_t>(n)].assign(list.begin(), list.end());
					continue;
				}
				if (node.childrenOffset % 44 != 0 || tree.first_child(n) <= n || tree.first_child(n) + node.childCount > total)
				{
					throw compile_error(format("cell %d: AABB node %d has children outside the tree", cell, n));
				}
				// the children's runs, back to back, are the node's run
				int at = start, listed = 0;
				for (int c = tree.first_child(n); c < tree.first_child(n) + node.childCount; c++)
				{
					const aabb_node& child = tree.nodes[static_cast<std::size_t>(c)];
					tree.parent[static_cast<std::size_t>(c)] = n;
					if (child.smodelIndexCount)
					{
						if (static_cast<int>((child.smodelIndexes - root_list) / 2) != at)
						{
							throw compile_error(format("cell %d: the static model lists of the children of AABB node %d do not follow each other", cell, n));
						}
						at += child.smodelIndexCount;
						listed += child.smodelIndexCount;
					}
				}
				if (listed != node.smodelIndexCount)
				{
					throw compile_error(format("cell %d: AABB node %d lists %u static models, its children %d", cell, n, node.smodelIndexCount, listed));
				}
			}
			return tree;
		}

		// puts model `index` with world box `bounds` into the leaf whose box has to grow least
		void add_to_tree(cell_tree& tree, const int index, const box& bounds)
		{
			int best = -1;
			float best_growth = 0, best_size = 0;
			for (std::size_t n = 0; n < tree.nodes.size(); n++)
			{
				if (tree.nodes[n].childCount != 0)
				{
					continue;
				}
				const box node(tree.nodes[n].bounds);
				if (!node.valid())
				{
					continue;
				}
				const float growth = node.growth(bounds);
				const float size = tree.nodes[n].bounds.halfSize[0] + tree.nodes[n].bounds.halfSize[1] + tree.nodes[n].bounds.halfSize[2];
				// a leaf that lists models already is preferred over one that only holds surfaces
				const float penalty = tree.items[n].empty() ? 64.0f : 0.0f;
				if (best < 0 || growth + penalty < best_growth || (growth + penalty == best_growth && size < best_size))
				{
					best = static_cast<int>(n);
					best_growth = growth + penalty;
					best_size = size;
				}
			}
			if (best < 0)
			{
				best = 0;	// a tree of one node without a usable box: the root
				for (std::size_t n = 0; n < tree.nodes.size(); n++)
				{
					if (tree.nodes[n].childCount == 0)
					{
						best = static_cast<int>(n);
						break;
					}
				}
			}
			tree.items[static_cast<std::size_t>(best)].push_back(index);
			for (int n = best; n >= 0; n = tree.parent[static_cast<std::size_t>(n)])
			{
				box grown(tree.nodes[static_cast<std::size_t>(n)].bounds);
				if (!grown.valid())
				{
					grown = bounds;
				}
				grown.add(bounds);
				tree.nodes[static_cast<std::size_t>(n)].bounds = grown.bounds();
			}
			tree.changed = true;
		}

		void write_cell_tree(context& ctx, cell_tree& tree)
		{
			// the root's list: the leaves' lists in tree order; every node names its run of it
			std::vector<std::uint16_t> list;
			std::vector<std::pair<int, int>> run(tree.nodes.size(), { 0, 0 });
			const std::function<void(int)> walk = [&](const int n)
			{
				const aabb_node& node = tree.nodes[static_cast<std::size_t>(n)];
				const int start = static_cast<int>(list.size());
				if (node.childCount == 0)
				{
					for (const int index : tree.items[static_cast<std::size_t>(n)])
					{
						list.push_back(static_cast<std::uint16_t>(index));
					}
				}
				else
				{
					for (int c = tree.first_child(n); c < tree.first_child(n) + node.childCount; c++)
					{
						walk(c);
					}
				}
				run[static_cast<std::size_t>(n)] = { start, static_cast<int>(list.size()) - start };
			};
			walk(0);

			std::vector<aabb_node> nodes = tree.nodes;
			for (std::size_t n = 0; n < nodes.size(); n++)
			{
				nodes[n].smodelIndexCount = static_cast<std::uint16_t>(run[n].second);
				nodes[n].smodelIndexes = n == 0 && !list.empty() ? FOLLOWS : 0;
			}
			// the nodes without pointers first: the list they point into is about to change its length
			ctx.set(tree.nodes_id, bytes_of(nodes), {}, "cell AABB tree");
			if (list.empty())
			{
				if (tree.list_id)
				{
					check(ctx.editor.remove_data(tree.list_id), "cell static model list");
					tree.list_id = 0;
				}
				return;
			}
			if (tree.list_id)
			{
				ctx.set(tree.list_id, bytes_of(list), {}, "cell static model list");
			}
			else
			{
				check(ctx.editor.insert_data(tree.nodes_id, true, bytes_of(list), {}, tree.list_id), "cell static model list");
			}
			std::vector<ZonePointer> pointers;
			for (std::size_t n = 1; n < nodes.size(); n++)
			{
				if (run[n].second)
				{
					pointers.push_back({ static_cast<std::uint32_t>(44 * n + offsetof(aabb_node, smodelIndexes)), ZoneTarget::to_data(tree.list_id, 2u * static_cast<std::uint32_t>(run[n].first)) });
				}
			}
			ctx.set(tree.nodes_id, bytes_of(nodes), pointers, "cell AABB tree");
		}

		// ---- the clip map's static model AABB tree ( MapEdits.cs RebuildSmodelTree ) ------------------------

		struct clip_tree_node
		{
			box bounds;
			std::vector<int> children;
			std::vector<int> models;		// leaf: item ids
			bool leaf = false;
			bool touched = false;
			int parent = -1;
		};
	}

	void stage_models(context& ctx)
	{
		const Zone& zone = ctx.zone();
		ZoneEditor& editor = ctx.editor;
		const auto& dpvs = ctx.gfx.raw.dpvs;
		const map_diff& diff = ctx.rep.diff;
		const std::uint32_t old_count = dpvs.smodelCount;

		const ZoneDataId draw_id = ctx.data(dpvs.smodelDrawInsts, "smodelDrawInsts");
		const ZoneDataId inst_id = ctx.data(dpvs.smodelInsts, "smodelInsts");
		const auto draw_stream = array_of<c1::GfxStaticModelDrawInst>(editor.data_bytes(draw_id));
		const auto inst_stream = array_of<c1::GfxStaticModelInst>(editor.data_bytes(inst_id));
		if (draw_stream.size() != old_count || inst_stream.size() != old_count || ctx.gfx.staticModels.size() != old_count)
		{
			throw compile_error("static models: the instance arrays do not have smodelCount elements");
		}
		std::map<std::uint32_t, ZoneTarget> draw_pointers;
		for (const ZonePointer& p : editor.data_pointers(draw_id))
		{
			draw_pointers.emplace(p.offset, p.target);
		}

		// ---- the instances of the base zone
		std::vector<instance> slots(old_count);
		std::map<zaddr, int> carrier;				// model -> the instance that carries its XModel
		std::map<std::string, int> by_name;			// model name -> an instance of it
		for (std::uint32_t i = 0; i < old_count; i++)
		{
			instance& s = slots[i];
			s.draw = draw_stream[i];
			s.inst = inst_stream[i];
			s.model = ctx.gfx.staticModels[i].raw.model;
			s.name = std::string(ctx.gfx.staticModels[i].modelName);
			s.old_index = static_cast<int>(i);
			const auto pointer = draw_pointers.find(DRAW_SIZE * i + DRAW_MODEL);
			if (pointer != draw_pointers.end())
			{
				s.pointer = pointer->second;
			}
			else if (s.draw.model == FOLLOWS)
			{
				s.carries_model = true;
				carrier[s.model] = static_cast<int>(i);
			}
			by_name.emplace(s.name, static_cast<int>(i));
		}

		// ---- the edits, by instance of the base zone
		std::vector<std::uint8_t> deleted(old_count, 0);
		std::vector<const model_placement*> moved(old_count, nullptr);
		for (const int index : diff.models_deleted)
		{
			deleted[static_cast<std::size_t>(index)] = 1;
		}
		for (const model_move& move : diff.models_moved)
		{
			moved[static_cast<std::size_t>(move.zone_index)] = &move.to;
		}

		lighting light(ctx);
		const cell_finder cells(ctx);
		std::map<zaddr, model_box> boxes;
		const auto box_of = [&](const zaddr model) -> const model_box&
		{
			auto it = boxes.find(model);
			if (it == boxes.end())
			{
				it = boxes.emplace(model, xmodel_box(zone, model)).first;
			}
			return it->second;
		};

		std::map<zaddr, std::vector<std::array<float, 3>>> point_cache;
		const auto points_of = [&](const zaddr model) -> const std::vector<std::array<float, 3>>&
		{
			auto it = point_cache.find(model);
			if (it == point_cache.end())
			{
				it = point_cache.emplace(model, xmodel_points(zone, model)).first;
			}
			return it->second;
		};

		// How the stored instance bounds follow from the model: checked on the base zone and reported, because the
		// bounds of a moved or new instance are computed the same way.
		{
			int checked = 0, tight = 0, inside_box = 0, with_points = 0;
			const std::uint32_t step = std::max(1u, old_count / 1500);
			for (std::uint32_t i = 0; i < old_count; i += step)
			{
				const model_box& local = box_of(slots[i].model);
				if (!local.known)
				{
					continue;
				}
				checked++;
				const box stored(slots[i].inst.bounds);
				box grown = placed_box(local, slots[i].draw.origin, slots[i].draw.axis, slots[i].draw.scale);
				for (int k = 0; k < 3; k++)
				{
					grown.mins[k] -= 0.5f;
					grown.maxs[k] += 0.5f;
				}
				inside_box += grown.contains(stored);
				const auto& points = points_of(slots[i].model);
				if (!points.empty())
				{
					with_points++;
					tight += box_error(placed_points(points, slots[i].draw.origin, slots[i].draw.axis, slots[i].draw.scale), stored) < 0.5f;
				}
			}
			ctx.note(format("static models: of %d sampled instances of the base zone, the stored bounds are the box of the placed LOD 0 vertices ( within 0.5 units ) for %d of the %d "
				"whose vertices are in the zone, and lie inside the placed XModel box for %d", checked, tight, with_points, inside_box));
		}

		// places `s` anew: origin, axis, scale, bounds, lighting origin, cull distance
		const auto place = [&](instance& s, const model_placement& to, const model_box& local, const bool added)
		{
			static const std::vector<std::array<float, 3>> no_points;
			const auto& points = s.model ? points_of(s.model) : no_points;
			const c1::GfxStaticModelDrawInst before = s.draw;
			const box old_box(s.inst.bounds);
			for (int k = 0; k < 3; k++)
			{
				s.draw.origin[k] = static_cast<float>(to.origin[k]);
			}
			if (added || to.angles_changed)
			{
				axis_of(to.angles, s.draw.axis);
			}
			if (added || to.scale_changed)
			{
				s.draw.scale = static_cast<float>(to.scale);
			}
			float offset[3] = {};		// lighting origin relative to the centre of the bounds
			if (!added)
			{
				for (int k = 0; k < 3; k++)
				{
					offset[k] = s.inst.lightingOrigin[k] - s.inst.bounds.midPoint[k];
				}
			}
			box now;
			if (!points.empty())
			{
				now = placed_points(points, s.draw.origin, s.draw.axis, s.draw.scale);
			}
			else if (local.known)
			{
				now = placed_box(local, s.draw.origin, s.draw.axis, s.draw.scale);
			}
			else if (!added && !to.angles_changed && !to.scale_changed)
			{
				now = old_box;
				for (int k = 0; k < 3; k++)
				{
					now.mins[k] += s.draw.origin[k] - before.origin[k];
					now.maxs[k] += s.draw.origin[k] - before.origin[k];
				}
			}
			else
			{
				// the model is in another zone and no box is known: a cube around the origin that holds the old box in any rotation
				float reach = 64.0f;
				if (!added)
				{
					reach = 0;
					for (int k = 0; k < 3; k++)
					{
						reach = std::max({ reach, std::fabs(old_box.mins[k] - before.origin[k]), std::fabs(old_box.maxs[k] - before.origin[k]) });
					}
					reach *= 1.7321f * (before.scale > 0 ? s.draw.scale / before.scale : 1.0f);
				}
				for (int k = 0; k < 3; k++)
				{
					now.mins[k] = s.draw.origin[k] - reach;
					now.maxs[k] = s.draw.origin[k] + reach;
				}
				ctx.warn("static model '" + s.name + "': its XModel is defined in another zone, the bounds of the rotated / scaled / new instance are a cube around its origin");
			}
			s.inst.bounds = now.bounds();
			for (int k = 0; k < 3; k++)
			{
				s.inst.lightingOrigin[k] = s.inst.bounds.midPoint[k] + offset[k];
			}
			if (!added && before.scale > 0 && s.draw.scale != before.scale)
			{
				s.draw.cullDist = static_cast<std::uint16_t>(std::clamp(std::lround(before.cullDist * static_cast<double>(s.draw.scale) / before.scale), 0l, 65535l));
			}
			s.draw.lightingHandle = 0;
			light.apply(s);
			s.relit = true;
		};

		// ---- deleted instances. One that carries its model hands the slot to another instance of that model.
		std::vector<int> slot_of(old_count);			// where instance i of the base zone is now ( slot ), -1: gone
		std::iota(slot_of.begin(), slot_of.end(), 0);
		std::vector<std::uint8_t> slot_gone(old_count, 0);
		std::vector<int> emptied;						// slots whose model has no instance left
		for (std::uint32_t i = 0; i < old_count; i++)
		{
			if (!deleted[i])
			{
				continue;
			}
			slot_of[i] = -1;
			if (!slots[i].carries_model)
			{
				slot_gone[i] = 1;
				continue;
			}
			int heir = -1;
			for (std::uint32_t j = 0; j < old_count && heir < 0; j++)
			{
				if (j != i && !deleted[j] && slots[j].model == slots[i].model && slot_of[j] == static_cast<int>(j) && !slots[j].carries_model)
				{
					heir = static_cast<int>(j);
				}
			}
			if (heir < 0)
			{
				emptied.push_back(static_cast<int>(i));
				continue;
			}
			instance taken = slots[static_cast<std::size_t>(heir)];
			taken.carries_model = true;
			taken.pointer.reset();
			slots[i] = std::move(taken);
			slot_of[static_cast<std::size_t>(heir)] = static_cast<int>(i);
			slot_gone[static_cast<std::size_t>(heir)] = 1;
		}

		// ---- a model without instances: its nested XModel leaves the zone when nothing else uses it
		const auto inbound = editor.pointers_to(draw_id);
		ZoneDataId clip_list_id = 0;
		if (ctx.clip.raw.numStaticModels)
		{
			clip_list_id = ctx.data(ctx.clip.raw.staticModelList, "clipMap.staticModelList");
		}
		for (const int slot : emptied)
		{
			instance& s = slots[static_cast<std::size_t>(slot)];
			const std::uint32_t field = DRAW_SIZE * static_cast<std::uint32_t>(slot) + DRAW_MODEL;
			bool used_elsewhere = false;
			for (const zone::ZoneInbound& in : inbound)
			{
				used_elsewhere = used_elsewhere || (in.targetOffset == field && in.from != draw_id && in.from != clip_list_id);
			}
			bool removed = false;
			zone::ZoneDataInfo header;
			const ZoneDataId header_id = editor.header_data(s.model);
			if (!used_elsewhere && header_id && editor.data_info(header_id, header) && header.asset == ctx.gfx_asset)
			{
				// the XModel's data ends where the next carried model begins, or with the last piece before the hero lights
				std::uint32_t end = editor.asset(ctx.gfx_asset).chunks;
				for (const auto& [model, at] : carrier)
				{
					zone::ZoneDataInfo other;
					if (model != s.model && editor.data_info(editor.header_data(model), other) && other.asset == ctx.gfx_asset && other.index > header.index)
					{
						end = std::min(end, other.index);
					}
				}
				if (const zaddr hero = ctx.gfx.raw.heroOnlyLights)
				{
					zone::ZoneDataInfo other;
					if (editor.data_info(editor.data_at(hero), other) && other.asset == ctx.gfx_asset && other.index > header.index)
					{
						end = std::min(end, other.index);
					}
				}
				const ZoneDataId last = editor.asset_data(ctx.gfx_asset, end - 1);
				const zone::Status s2 = editor.remove_data_range(header_id, last);
				removed = static_cast<bool>(s2);
				if (!removed)
				{
					ctx.warn("static model '" + s.name + "': " + s2.message);
				}
			}
			if (removed)
			{
				slot_gone[static_cast<std::size_t>(slot)] = 1;
				carrier.erase(s.model);
				ctx.rep.model_assets_removed++;
				ctx.note("static models: the last instance of '" + s.name + "' is gone, its XModel left the gfx_map");
			}
			else
			{
				s.hidden = true;
				ctx.rep.models_hidden++;
				ctx.warn("static model '" + s.name + "': the last instance was deleted but other data of the zone uses the XModel it carries. The instance stays in the arrays as a holder "
					"that no cell, shadow list or collision list names ( it is never drawn ); a new import of the compiled zone shows it again");
			}
		}

		// ---- moved instances
		for (std::uint32_t i = 0; i < old_count; i++)
		{
			if (moved[i] && slot_of[i] >= 0)
			{
				instance& s = slots[static_cast<std::size_t>(slot_of[i])];
				place(s, *moved[i], box_of(s.model), false);
				ctx.rep.models_moved++;
			}
		}
		ctx.rep.models_deleted = static_cast<int>(diff.models_deleted.size()) - ctx.rep.models_hidden;

		// ---- the new arrays. A hole is filled with the last instance that carries no model, so that the instances
		// that do carry one ( and the fields other assets point at ) stay where they are; added instances follow.
		std::vector<int> content(old_count);			// position -> slot whose record it holds, -1: free
		std::iota(content.begin(), content.end(), 0);
		{
			// an instance has to stay behind the one that carries its model: the loader resolves "the same model as
			// that field" in array order
			const auto can_fill = [&](const int position, const std::uint32_t hole)
			{
				const int slot = content[static_cast<std::size_t>(position)];
				if (slot < 0)
				{
					return false;
				}
				const instance& s = slots[static_cast<std::size_t>(slot)];
				if (s.carries_model || !s.pointer)
				{
					return false;
				}
				return s.pointer->kind != ZoneTarget::Kind::data || s.pointer->data != draw_id || s.pointer->offset / DRAW_SIZE < hole;
			};
			for (std::uint32_t hole = 0; hole < old_count; hole++)
			{
				if (slot_gone[hole])
				{
					content[hole] = -1;
				}
			}
			// the place an instance leaves is a hole again, filled the same way
			for (std::uint32_t hole = 0; hole < old_count; hole++)
			{
				if (content[hole] >= 0)
				{
					continue;
				}
				for (int tail = static_cast<int>(old_count) - 1; tail > static_cast<int>(hole); tail--)
				{
					if (can_fill(tail, hole))
					{
						content[hole] = content[static_cast<std::size_t>(tail)];
						content[static_cast<std::size_t>(tail)] = -1;
						break;
					}
				}
			}
		}
		std::vector<instance> out;
		std::vector<int> new_of_slot(old_count, -1);
		for (std::uint32_t at = 0; at < old_count; at++)
		{
			if (content[at] >= 0)
			{
				new_of_slot[static_cast<std::size_t>(content[at])] = static_cast<int>(out.size());
				out.push_back(slots[static_cast<std::size_t>(content[at])]);
			}
		}
		const auto new_index = [&](const int old_instance) { return slot_of[static_cast<std::size_t>(old_instance)] < 0 ? -1 : new_of_slot[static_cast<std::size_t>(slot_of[static_cast<std::size_t>(old_instance)])]; };

		// a model the gfx_map does not carry: a top level xmodel in front of the gfx_map, or one taken from a donor zone
		std::vector<std::unique_ptr<ZoneEditor>> donors;
		struct outside_model
		{
			ZoneTarget pointer;
			model_box bounds;
			bool collides = false;
			model_box collision;
		};
		std::map<std::string, outside_model> outside;
		const auto find_outside = [&](const std::string& name) -> const outside_model*
		{
			if (const auto it = outside.find(name); it != outside.end())
			{
				return it->second.pointer.kind == ZoneTarget::Kind::none ? nullptr : &it->second;
			}
			outside_model found;
			const std::size_t gfx_now = editor.find_asset(c1::ASSET_TYPE_GFXWORLD, editor.asset(ctx.gfx_asset).name);
			if (const std::size_t at = editor.find_asset(c1::ASSET_TYPE_XMODEL, name); at != ZoneEditor::npos && at < gfx_now)
			{
				found.pointer = ZoneTarget::to_asset(at);
				const zaddr header = zone.find_asset(c1::ASSET_TYPE_XMODEL, name);
				found.bounds = xmodel_box(zone, header);
				found.collision = found.bounds;		// the model's box holds its collision surfaces
				found.collides = xmodel_collides(zone, header) && found.bounds.known;
			}
			else
			{
				if (donors.empty())
				{
					for (const auto& file : ctx.opts.donors)
					{
						auto donor = std::make_unique<ZoneEditor>();
						if (const zone::Status s = donor->open(file, ctx.key); !s)
						{
							ctx.warn("donor zone: " + s.message);
							continue;
						}
						donors.push_back(std::move(donor));
					}
				}
				for (const auto& donor : donors)
				{
					const std::size_t in_donor = donor->find_asset(c1::ASSET_TYPE_XMODEL, name);
					if (in_donor == ZoneEditor::npos)
					{
						continue;
					}
					const std::size_t before = editor.asset_count();
					if (const zone::Status s = editor.add_asset(*donor, in_donor, gfx_now, true); !s)
					{
						ctx.warn("static model '" + name + "' could not be taken from zone '" + donor->name() + "': " + s.message);
						continue;
					}
					found.pointer = ZoneTarget::to_asset(editor.find_asset(c1::ASSET_TYPE_XMODEL, name));
					const zaddr header = donor->zone().find_asset(c1::ASSET_TYPE_XMODEL, name);
					found.bounds = xmodel_box(donor->zone(), header);
					found.collision = found.bounds;
					found.collides = xmodel_collides(donor->zone(), header) && found.bounds.known;
					ctx.rep.model_assets_added += static_cast<int>(editor.asset_count() - before);
					ctx.note(format("static models: xmodel '%s' taken from zone '%s' with %zu assets it depends on", name.c_str(), donor->name().c_str(),
						editor.asset_count() - before - 1));
					break;
				}
			}
			const auto it = outside.emplace(name, found).first;
			return found.pointer.kind == ZoneTarget::Kind::none ? nullptr : &it->second;
		};

		// cull distance of a new instance: what the other instances of the model have per unit of scale, else the map's median
		const auto cull_distance = [&](const std::string& name, const float scale)
		{
			std::vector<float> per_scale, all;
			for (const instance& s : slots)
			{
				if (s.draw.scale > 0)
				{
					(s.name == name ? per_scale : all).push_back(s.draw.cullDist / s.draw.scale);
				}
			}
			auto& from = per_scale.empty() ? all : per_scale;
			if (from.empty())
			{
				return std::uint16_t{ 0 };
			}
			std::nth_element(from.begin(), from.begin() + static_cast<std::ptrdiff_t>(from.size() / 2), from.end());
			return static_cast<std::uint16_t>(std::clamp(std::lround(from[from.size() / 2] * scale), 0l, 65535l));
		};

		struct added_clip
		{
			int instance;				// new index
			ZoneTarget pointer;
			model_box collision;
		};
		std::vector<added_clip> clip_added;
		for (const model_placement& to : diff.models_added)
		{
			instance s;
			s.name = to.model;
			model_box local;
			if (const auto known = by_name.find(to.model); known != by_name.end() && !slot_gone[static_cast<std::size_t>(known->second)]
				&& slots[static_cast<std::size_t>(known->second)].name == to.model)
			{
				// the zone has the model: point at the field of the instance that carries it, as every other instance does
				const instance& like = slots[static_cast<std::size_t>(known->second)];
				s.model = like.model;
				// the slot that carries a model never moves, whoever fills it now
				const auto at = carrier.find(like.model);
				if (at != carrier.end() && new_of_slot[static_cast<std::size_t>(at->second)] >= 0)
				{
					s.pointer = ZoneTarget::to_data(draw_id, DRAW_SIZE * static_cast<std::uint32_t>(new_of_slot[static_cast<std::size_t>(at->second)]) + DRAW_MODEL);
				}
				else if (like.pointer)
				{
					s.pointer = like.pointer;
				}
				local = box_of(like.model);
			}
			if (!s.pointer)
			{
				const outside_model* found = find_outside(to.model);
				if (!found)
				{
					ctx.warn("static model '" + to.model + "' is neither a static model of the map, a top level xmodel in front of its gfx_map, nor in a donor zone: the new instance is left out");
					continue;
				}
				s.pointer = found->pointer;
				local = found->bounds;
				if (found->collides)
				{
					clip_added.push_back({ static_cast<int>(out.size()), found->pointer, found->collision });
				}
			}
			s.draw.cullDist = cull_distance(to.model, static_cast<float>(to.scale));
			place(s, to, local, true);
			out.push_back(std::move(s));
			ctx.rep.models_added++;
		}
		if (out.size() > 0xFFFF)
		{
			throw compile_error("static models: more than 65535 instances ( the lists hold 16 bit indices )");
		}

		// ---- write the instance arrays. Pointers of other data to a model field move with the field.
		const std::uint32_t new_count = static_cast<std::uint32_t>(out.size());
		const auto retarget = [&](const ZoneTarget& t) -> std::optional<ZoneTarget>
		{
			if (t.kind != ZoneTarget::Kind::data || t.data != draw_id)
			{
				return t;
			}
			const std::uint32_t slot = t.offset / DRAW_SIZE;
			if (t.offset % DRAW_SIZE != DRAW_MODEL || slot >= old_count || new_of_slot[slot] < 0)
			{
				return std::nullopt;
			}
			return ZoneTarget::to_data(draw_id, DRAW_SIZE * static_cast<std::uint32_t>(new_of_slot[slot]) + DRAW_MODEL);
		};
		std::vector<c1::GfxStaticModelDrawInst> draws(new_count);
		std::vector<c1::GfxStaticModelInst> insts(new_count);
		std::vector<ZonePointer> pointers;
		for (std::uint32_t n = 0; n < new_count; n++)
		{
			const instance& s = out[n];
			draws[n] = s.draw;
			insts[n] = s.inst;
			if (s.carries_model)
			{
				draws[n].model = FOLLOWS;
				continue;
			}
			draws[n].model = 0;
			if (!s.pointer)
			{
				throw compile_error("static models: an instance has neither a model nor a pointer to one");
			}
			// an added instance already names the new place of the field it points at
			const std::optional<ZoneTarget> target = s.old_index < 0 ? s.pointer : retarget(*s.pointer);
			if (!target)
			{
				throw compile_error("static model '" + s.name + "': the instance that carries its XModel is gone");
			}
			pointers.push_back({ DRAW_SIZE * n + DRAW_MODEL, *target });
		}
		// park the pointers of other data at a field that exists in the old and in the new array, then set them right
		struct outer
		{
			zone::ZoneInbound in;
			std::optional<ZoneTarget> target;
		};
		std::vector<outer> outers;
		for (const zone::ZoneInbound& in : editor.pointers_to(draw_id))		// asked again: a removed XModel took its pointers along
		{
			if (in.from != draw_id && in.from != clip_list_id)
			{
				outers.push_back({ in, retarget(ZoneTarget::to_data(draw_id, in.targetOffset)) });
				if (!outers.back().target)
				{
					throw compile_error("static models: other data of the zone points at an instance that is gone");
				}
			}
		}
		if (new_count == 0)
		{
			throw compile_error("static models: a map without any static model is not supported");
		}
		const std::uint32_t park = DRAW_MODEL;
		for (const outer& o : outers)
		{
			check(editor.set_pointer(o.in.from, o.in.offset, ZoneTarget::to_data(draw_id, park)), "static models");
		}
		// the clip map's list points into the draw insts too: it is rebuilt below, park it as well
		std::vector<c1::cStaticModel_s> clip_models;
		std::vector<ZoneTarget> clip_targets;
		if (clip_list_id)
		{
			clip_models = array_of<c1::cStaticModel_s>(editor.data_bytes(clip_list_id));
			std::map<std::uint32_t, ZoneTarget> at;
			for (const ZonePointer& p : editor.data_pointers(clip_list_id))
			{
				at.emplace(p.offset, p.target);
			}
			clip_targets.resize(clip_models.size());
			std::vector<ZonePointer> parked;
			for (std::size_t i = 0; i < clip_models.size(); i++)
			{
				const auto p = at.find(static_cast<std::uint32_t>(76 * i));
				if (p == at.end())
				{
					throw compile_error("clipMap.staticModelList: an entry carries its XModel itself, which the compiler does not expect");
				}
				clip_targets[i] = p->second;
				const bool into_draw = p->second.kind == ZoneTarget::Kind::data && p->second.data == draw_id;
				parked.push_back({ static_cast<std::uint32_t>(76 * i), into_draw ? ZoneTarget::to_data(draw_id, park) : p->second });
			}
			ctx.set(clip_list_id, bytes_of(clip_models), parked, "clipMap.staticModelList");
		}
		// pointers of the array into itself are dropped before it changes its length
		ctx.set(draw_id, bytes_of(draws), {}, "smodelDrawInsts");
		ctx.set(draw_id, bytes_of(draws), pointers, "smodelDrawInsts");
		ctx.set(inst_id, bytes_of(insts), {}, "smodelInsts");
		for (const outer& o : outers)
		{
			check(editor.set_pointer(o.in.from, o.in.offset, *o.target), "static models");
		}
		const std::uint32_t dpvs_offset = offsetof(c1::GfxWorld, dpvs);
		ctx.patch(ctx.gfx_header, dpvs_offset + offsetof(c1::GfxWorldDpvsStatic, smodelCount), new_count, "dpvs.smodelCount");
		ctx.patch(ctx.gfx_header, dpvs_offset + offsetof(c1::GfxWorldDpvsStatic, smodelVisDataCount), vis_data_count(dpvs.smodelVisDataCount, old_count, new_count),
			"dpvs.smodelVisDataCount");

		// ---- the cells' lists
		{
			std::vector<cell_tree> trees;
			for (int c = 0; c < ctx.gfx.raw.cellCount; c++)
			{
				trees.push_back(read_cell_tree(ctx, c));
			}
			// instances that left a list: deleted, hidden, or placed anew
			std::vector<int> mapped(old_count, -1);
			for (std::uint32_t i = 0; i < old_count; i++)
			{
				const int n = new_index(static_cast<int>(i));
				mapped[i] = n >= 0 && !out[static_cast<std::size_t>(n)].hidden && !out[static_cast<std::size_t>(n)].relit ? n : -1;
			}
			bool renumbered = false;
			for (std::uint32_t i = 0; i < old_count && !renumbered; i++)
			{
				renumbered = mapped[i] != static_cast<int>(i);
			}
			for (cell_tree& tree : trees)
			{
				for (auto& list : tree.items)
				{
					std::vector<int> kept;
					for (const int index : list)
					{
						if (index >= 0 && index < static_cast<int>(old_count) && mapped[static_cast<std::size_t>(index)] >= 0)
						{
							kept.push_back(mapped[static_cast<std::size_t>(index)]);
						}
					}
					tree.changed = tree.changed || kept != list;
					list = std::move(kept);
				}
			}
			int fallback_cells = 0;
			for (std::uint32_t n = 0; n < new_count; n++)
			{
				const instance& s = out[n];
				if (!s.relit || s.hidden)
				{
					continue;
				}
				const box bounds(s.inst.bounds);
				int cell = cells.find(s.inst.bounds.midPoint);
				if (cell < 0 || cell >= static_cast<int>(trees.size()) || trees[static_cast<std::size_t>(cell)].nodes.empty())
				{
					// outside the BSP: the cell whose tree has to grow least
					float best = 0;
					cell = -1;
					for (std::size_t c = 0; c < trees.size(); c++)
					{
						if (trees[c].nodes.empty() || !box(trees[c].nodes[0].bounds).valid())
						{
							continue;
						}
						const float growth = box(trees[c].nodes[0].bounds).growth(bounds);
						if (cell < 0 || growth < best)
						{
							cell = static_cast<int>(c);
							best = growth;
						}
					}
					fallback_cells++;
				}
				if (cell < 0)
				{
					throw compile_error("static models: the map has no cell with an AABB tree");
				}
				add_to_tree(trees[static_cast<std::size_t>(cell)], static_cast<int>(n), bounds);
			}
			int written = 0;
			for (cell_tree& tree : trees)
			{
				if (tree.changed && tree.nodes_id)
				{
					write_cell_tree(ctx, tree);
					written++;
				}
			}
			ctx.note(format("static models: %d of %d cell AABB trees rewritten%s", written, ctx.gfx.raw.cellCount,
				fallback_cells ? format( "; %d instances lie in no cell of the BSP and went to the nearest tree", fallback_cells).c_str() : ""));
			(void)renumbered;
		}

		// ---- shadow geometry: per primary light the models lit by it
		if (const zaddr shadow_address = zone.get<zaddr>(ctx.gfx_address + WORLD_SHADOW_GEOM); shadow_address && ctx.gfx.raw.primaryLightCount)
		{
			const ZoneDataId shadow_id = ctx.data(shadow_address, "shadow geometry");
			auto shadows = array_of<shadow_geometry>(editor.data_bytes(shadow_id));
			const auto image = zone.get_array<shadow_geometry>(shadow_address, shadows.size());
			std::vector<std::vector<std::uint16_t>> lists(shadows.size());
			for (std::uint32_t n = 0; n < new_count; n++)
			{
				if (!out[n].hidden && out[n].draw.primaryLightIndex < lists.size())
				{
					lists[out[n].draw.primaryLightIndex].push_back(static_cast<std::uint16_t>(n));
				}
			}
			// base zone: is every model in the list of its light? If not, the lists are something else and are only renumbered
			bool by_light = true;
			std::vector<std::vector<std::uint16_t>> old_lists(shadows.size());
			for (std::size_t l = 0; l < shadows.size(); l++)
			{
				old_lists[l] = zone.get_array<std::uint16_t>(image[l].smodelIndex, image[l].smodelCount);
				for (const std::uint16_t m : old_lists[l])
				{
					by_light = by_light && m < old_count && draw_stream[m].primaryLightIndex == l;
				}
			}
			if (!by_light)
			{
				ctx.warn("shadow geometry: the static model lists of the base zone are not ordered by primary light; they are renumbered and new instances are not added to them");
				for (std::size_t l = 0; l < shadows.size(); l++)
				{
					lists[l].clear();
					for (const std::uint16_t m : old_lists[l])
					{
						const int n = m < old_count ? new_index(m) : -1;
						if (n >= 0 && !out[static_cast<std::size_t>(n)].hidden)
						{
							lists[l].push_back(static_cast<std::uint16_t>(n));
						}
					}
					std::sort(lists[l].begin(), lists[l].end());
				}
			}
			ZoneDataId anchor = shadow_id;		// the piece the next new list goes behind
			std::vector<ZonePointer> shadow_pointers = editor.data_pointers(shadow_id);
			if (!shadow_pointers.empty())
			{
				throw compile_error("shadow geometry: lists that are shared between lights are not expected");
			}
			for (std::size_t l = 0; l < shadows.size(); l++)
			{
				if (shadows[l].surfaceCount && image[l].sortedSurfIndex)
				{
					anchor = ctx.data(image[l].sortedSurfIndex, "shadow geometry surface list");
				}
				ZoneDataId list_id = shadows[l].smodelCount ? ctx.data(image[l].smodelIndex, "shadow geometry model list") : 0;
				if (lists[l].empty())
				{
					if (list_id)
					{
						check(editor.remove_data(list_id), "shadow geometry model list");
					}
					shadows[l].smodelIndex = 0;
				}
				else if (list_id)
				{
					if (lists[l] != old_lists[l])
					{
						ctx.set(list_id, bytes_of(lists[l]), {}, "shadow geometry model list");
					}
					anchor = list_id;
				}
				else
				{
					check(editor.insert_data(anchor, true, bytes_of(lists[l]), {}, list_id), "shadow geometry model list");
					shadows[l].smodelIndex = FOLLOWS;
					anchor = list_id;
				}
				shadows[l].smodelCount = static_cast<std::uint16_t>(lists[l].size());
			}
			ctx.set(shadow_id, bytes_of(shadows), {}, "shadow geometry");
		}

		// ---- collision: clipMap.staticModelList and its AABB tree
		if (clip_list_id || !clip_added.empty())
		{
			if (!clip_list_id)
			{
				ctx.warn("clip map: the base zone has no static model collision list; the new models do not collide");
				return;
			}
			const std::uint32_t clip_count = static_cast<std::uint32_t>(clip_models.size());
			// which gfx instance of the base zone a collision entry belongs to: same model, same origin
			std::vector<zaddr> clip_model(clip_count);
			for (std::uint32_t c = 0; c < clip_count; c++)
			{
				clip_model[c] = ctx.clip.staticModels[c].raw.xmodel;
			}
			std::vector<int> entry_state(clip_count, 0);		// 0 keep, 1 moved, 2 deleted
			std::vector<std::uint8_t> claimed(clip_count, 0);
			const auto entry_of = [&](const std::uint32_t i) -> int
			{
				for (std::uint32_t c = 0; c < clip_count; c++)
				{
					if (!claimed[c] && clip_model[c] == ctx.gfx.staticModels[i].raw.model
						&& std::fabs(clip_models[c].origin[0] - draw_stream[i].origin[0]) < 0.01f && std::fabs(clip_models[c].origin[1] - draw_stream[i].origin[1]) < 0.01f
						&& std::fabs(clip_models[c].origin[2] - draw_stream[i].origin[2]) < 0.01f)
					{
						claimed[c] = 1;
						return static_cast<int>(c);
					}
				}
				return -1;
			};
			// Bounds of a collision entry placed anew. The base zone's bounds are tight around the model's collision
			// triangles; what is known here is the old world box, so a turned or scaled entry gets the box that holds
			// the old one taken through both placements: never smaller than the tight box, usually somewhat larger.
			int loose_bounds = 0;
			const auto entry_bounds = [&](const c1::cStaticModel_s& like, const instance& s)
			{
				float scale = 1.0f, axis[3][3];
				const float length = std::sqrt(like.invScaledAxis[0][0] * like.invScaledAxis[0][0] + like.invScaledAxis[0][1] * like.invScaledAxis[0][1]
					+ like.invScaledAxis[0][2] * like.invScaledAxis[0][2]);
				scale = length > 1e-6f ? 1.0f / length : 1.0f;
				bool turned = false;
				for (int a = 0; a < 3; a++)
				{
					for (int k = 0; k < 3; k++)
					{
						axis[a][k] = like.invScaledAxis[a][k] * scale;
						turned = turned || std::fabs(like.invScaledAxis[a][k] - s.draw.axis[a][k] / s.draw.scale) > 1e-4f;
					}
				}
				box bounds(like.absBounds);
				if (!turned)
				{
					for (int k = 0; k < 3; k++)
					{
						bounds.mins[k] += s.draw.origin[k] - like.origin[k];
						bounds.maxs[k] += s.draw.origin[k] - like.origin[k];
					}
					return bounds;
				}
				loose_bounds++;
				return placed_box(local_box_of(bounds, like.origin, axis, scale), s.draw.origin, s.draw.axis, s.draw.scale);
			};
			const auto set_entry = [&](c1::cStaticModel_s& entry, const instance& s, const box& bounds)
			{
				for (int k = 0; k < 3; k++)
				{
					entry.origin[k] = s.draw.origin[k];
					for (int a = 0; a < 3; a++)
					{
						entry.invScaledAxis[a][k] = s.draw.axis[a][k] / s.draw.scale;
					}
				}
				entry.absBounds = bounds.bounds();
			};

			struct clip_item
			{
				c1::cStaticModel_s entry{};
				ZoneTarget pointer;
			};
			std::vector<clip_item> items(clip_count);		// item c = entry c of the base zone; added ones follow
			for (std::uint32_t c = 0; c < clip_count; c++)
			{
				items[c].entry = clip_models[c];
				const std::optional<ZoneTarget> target = retarget(clip_targets[c]);
				items[c].pointer = target ? *target : ZoneTarget();
				if (!target)
				{
					entry_state[c] = 2;		// its model left the zone
					claimed[c] = 1;
					ctx.rep.clip_models_deleted++;
				}
			}
			for (std::uint32_t i = 0; i < old_count; i++)
			{
				if (!deleted[i] && !moved[i])
				{
					continue;
				}
				// a model can have several collision entries at one place
				for (int c = entry_of(i); c >= 0; c = entry_of(i))
				{
					if (deleted[i])
					{
						entry_state[static_cast<std::size_t>(c)] = 2;
						ctx.rep.clip_models_deleted++;
						continue;
					}
					const instance& s = out[static_cast<std::size_t>(new_index(static_cast<int>(i)))];
					const c1::cStaticModel_s before = clip_models[static_cast<std::size_t>(c)];
					set_entry(items[static_cast<std::size_t>(c)].entry, s, entry_bounds(before, s));
					entry_state[static_cast<std::size_t>(c)] = 1;
					ctx.rep.clip_models_moved++;
				}
			}
			// added instances: of a model that has collision entries, or of a model from outside that has collision surfaces
			std::vector<int> added_items;
			for (std::uint32_t n = 0; n < new_count; n++)
			{
				const instance& s = out[n];
				if (s.old_index >= 0)
				{
					continue;
				}
				const auto from_outside = std::find_if(clip_added.begin(), clip_added.end(), [&](const added_clip& a) { return a.instance == static_cast<int>(n); });
				int like = -1;
				for (std::uint32_t c = 0; c < clip_count && like < 0 && s.model; c++)
				{
					like = clip_model[c] == s.model ? static_cast<int>(c) : -1;
				}
				if (like < 0 && from_outside == clip_added.end())
				{
					continue;		// a model without collision
				}
				clip_item item;
				if (like >= 0)
				{
					set_entry(item.entry, s, entry_bounds(clip_models[static_cast<std::size_t>(like)], s));
					item.pointer = *s.pointer;
				}
				else
				{
					set_entry(item.entry, s, placed_box(from_outside->collision, s.draw.origin, s.draw.axis, s.draw.scale));
					item.pointer = from_outside->pointer;
				}
				added_items.push_back(static_cast<int>(items.size()));
				items.push_back(item);
				ctx.rep.clip_models_added++;
			}

			// ---- the AABB tree: node 0 is the root; firstChild >= numStaticModels names nodes, below it a run of models
			const std::uint32_t node_count = ctx.clip.raw.smodelNodeCount & 0xFFFFu;
			const auto old_nodes = zone.get_array<smodel_node>(ctx.clip.raw.smodelNodes, node_count);
			std::vector<clip_tree_node> tree(old_nodes.size());
			std::vector<int> named(clip_count, 0);
			for (std::size_t n = 0; n < tree.size(); n++)
			{
				tree[n].bounds = box(old_nodes[n].bounds);
				if (old_nodes[n].firstChild >= clip_count)
				{
					const std::size_t first = old_nodes[n].firstChild - clip_count;
					if (first + old_nodes[n].childCount > tree.size() || first <= n)
					{
						throw compile_error("clip map: the static model AABB tree is not shaped as expected ( children )");
					}
					for (std::size_t c = first; c < first + old_nodes[n].childCount; c++)
					{
						tree[n].children.push_back(static_cast<int>(c));
						tree[c].parent = static_cast<int>(n);
					}
				}
				else
				{
					tree[n].leaf = true;
					if (static_cast<std::uint32_t>(old_nodes[n].firstChild) + old_nodes[n].childCount > clip_count)
					{
						throw compile_error("clip map: the static model AABB tree is not shaped as expected ( models )");
					}
					for (std::uint32_t m = old_nodes[n].firstChild; m < static_cast<std::uint32_t>(old_nodes[n].firstChild) + old_nodes[n].childCount; m++)
					{
						tree[n].models.push_back(static_cast<int>(m));
						named[m]++;
					}
				}
			}
			if (tree.empty() || std::any_of(named.begin(), named.end(), [](const int n) { return n != 1; }))
			{
				throw compile_error("clip map: the static model AABB tree does not name every static model once");
			}
			const auto item_box = [&](const int item) { return box(items[static_cast<std::size_t>(item)].entry.absBounds); };
			const auto touch = [&](int n)
			{
				for (; n >= 0; n = tree[static_cast<std::size_t>(n)].parent)
				{
					tree[static_cast<std::size_t>(n)].touched = true;
				}
			};
			for (std::size_t n = 0; n < tree.size(); n++)
			{
				if (!tree[n].leaf)
				{
					continue;
				}
				std::vector<int> kept;
				for (const int m : tree[n].models)
				{
					if (entry_state[static_cast<std::size_t>(m)] != 2)
					{
						kept.push_back(m);
					}
					if (entry_state[static_cast<std::size_t>(m)] != 0)
					{
						touch(static_cast<int>(n));
					}
				}
				tree[n].models = std::move(kept);
			}
			for (const int item : added_items)
			{
				int best = -1;
				float best_growth = 0;
				for (std::size_t n = 0; n < tree.size(); n++)
				{
					if (tree[n].leaf && !tree[n].models.empty())
					{
						const float growth = tree[n].bounds.growth(item_box(item));
						if (best < 0 || growth < best_growth)
						{
							best = static_cast<int>(n);
							best_growth = growth;
						}
					}
				}
				if (best < 0)
				{
					best = 0;
					if (!tree[0].leaf)
					{
						throw compile_error("clip map: the static model AABB tree has no leaf to put a new model in");
					}
				}
				tree[static_cast<std::size_t>(best)].models.push_back(item);
				touch(best);
			}
			// bounds of what changed, bottom up; nodes that hold nothing any more go
			const std::function<bool(int)> settle = [&](const int n)
			{
				clip_tree_node& node = tree[static_cast<std::size_t>(n)];
				box all;
				if (node.leaf)
				{
					for (const int m : node.models)
					{
						all.add(item_box(m));
					}
					if (node.models.empty())
					{
						return false;
					}
				}
				else
				{
					std::vector<int> kept;
					for (const int c : node.children)
					{
						if (settle(c))
						{
							kept.push_back(c);
							all.add(tree[static_cast<std::size_t>(c)].bounds);
						}
					}
					node.children = std::move(kept);
					if (node.children.empty())
					{
						return false;
					}
				}
				if (node.touched)
				{
					node.bounds = all;
				}
				return true;
			};
			const bool any = settle(0);

			// the list in the order of the old one ( leaves name runs ), nodes breadth first ( children stay together )
			std::vector<int> leaf_order;
			if (any)
			{
				std::vector<int> stack{ 0 };
				while (!stack.empty())
				{
					const int n = stack.back();
					stack.pop_back();
					if (tree[static_cast<std::size_t>(n)].leaf)
					{
						leaf_order.push_back(n);
					}
					for (const int c : tree[static_cast<std::size_t>(n)].children)
					{
						stack.push_back(c);
					}
				}
				// by the first model each leaf named in the base zone
				std::sort(leaf_order.begin(), leaf_order.end(), [&](const int a, const int b) { return old_nodes[static_cast<std::size_t>(a)].firstChild < old_nodes[static_cast<std::size_t>(b)].firstChild; });
			}
			std::vector<c1::cStaticModel_s> new_list;
			std::vector<ZonePointer> list_pointers;
			std::vector<std::pair<int, int>> leaf_run(tree.size(), { 0, 0 });
			for (const int n : leaf_order)
			{
				leaf_run[static_cast<std::size_t>(n)] = { static_cast<int>(new_list.size()), static_cast<int>(tree[static_cast<std::size_t>(n)].models.size()) };
				for (const int m : tree[static_cast<std::size_t>(n)].models)
				{
					const clip_item& item = items[static_cast<std::size_t>(m)];
					if (item.pointer.kind == ZoneTarget::Kind::none)
					{
						throw compile_error("clip map: a collision entry points at a static model instance that is gone");
					}
					list_pointers.push_back({ static_cast<std::uint32_t>(76 * new_list.size()), item.pointer });
					new_list.push_back(item.entry);
					new_list.back().xmodel = 0;
				}
			}
			const std::uint32_t new_clip_count = static_cast<std::uint32_t>(new_list.size());
			std::vector<smodel_node> new_nodes;
			if (any)
			{
				std::vector<int> order{ 0 };
				std::vector<int> position(tree.size(), -1);
				for (std::size_t i = 0; i < order.size(); i++)
				{
					position[static_cast<std::size_t>(order[i])] = static_cast<int>(i);
					for (const int c : tree[static_cast<std::size_t>(order[i])].children)
					{
						order.push_back(c);
					}
				}
				// children were appended in order, so the first child's position is where the run starts
				std::vector<int> first_child(tree.size(), 0);
				{
					std::size_t next = 1;
					for (const int n : order)
					{
						first_child[static_cast<std::size_t>(n)] = static_cast<int>(next);
						next += tree[static_cast<std::size_t>(n)].children.size();
					}
				}
				if (new_clip_count + order.size() > 0xFFFF)
				{
					throw compile_error("clip map: the static model AABB tree does not fit its 16 bit indices");
				}
				for (const int n : order)
				{
					const clip_tree_node& node = tree[static_cast<std::size_t>(n)];
					smodel_node packed{};
					packed.bounds = node.bounds.bounds();
					if (node.leaf)
					{
						packed.firstChild = static_cast<std::uint16_t>(leaf_run[static_cast<std::size_t>(n)].first);
						packed.childCount = static_cast<std::uint16_t>(leaf_run[static_cast<std::size_t>(n)].second);
					}
					else
					{
						packed.firstChild = static_cast<std::uint16_t>(new_clip_count + static_cast<std::uint32_t>(first_child[static_cast<std::size_t>(n)]));
						packed.childCount = static_cast<std::uint16_t>(node.children.size());
					}
					new_nodes.push_back(packed);
				}
			}
			if (new_list.empty() || new_nodes.empty())
			{
				throw compile_error("clip map: removing every static model with collision is not supported");
			}
			const bool list_changed = ctx.rep.clip_models_moved + ctx.rep.clip_models_deleted + ctx.rep.clip_models_added > 0;
			// the list was parked above: it is written in any case, with the pointers at their final place
			ctx.set(clip_list_id, bytes_of(new_list), list_pointers, "clipMap.staticModelList");
			if (list_changed)
			{
				ctx.set(ctx.data(ctx.clip.raw.smodelNodes, "clipMap.smodelNodes"), bytes_of(new_nodes), {}, "clipMap.smodelNodes");
				ctx.patch(ctx.clip_header, offsetof(c1::clipMap_t, numStaticModels), new_clip_count, "clipMap.numStaticModels");
				ctx.patch(ctx.clip_header, offsetof(c1::clipMap_t, smodelNodeCount), static_cast<std::uint16_t>(new_nodes.size()), "clipMap.smodelNodeCount");
				ctx.note(format("static models: collision list %u -> %u entries, AABB tree %zu -> %zu nodes", clip_count, new_clip_count, old_nodes.size(), new_nodes.size()));
				if (loose_bounds)
				{
					ctx.warn(format("static models: %d collision entries were turned or scaled; their bounds hold the old bounds taken through both placements, which is larger than "
						"the tight box the map compiler of the game would have computed ( a trace tests the model a little more often, the result is the same )", loose_bounds));
				}
			}
		}
		ctx.note(format("static models: %u -> %u instances", old_count, new_count));
	}
}
