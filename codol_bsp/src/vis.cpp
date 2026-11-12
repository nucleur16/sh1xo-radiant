// Structural BSP, leak test, hidden face removal, cells and portals.
//
//   1. a BSP from the faces of the structural ( opaque, non detail ) brushes of the world, plus the mapper's portal
//      faces: every leaf is a convex piece of space that lies entirely inside or entirely outside each such brush
//   2. portals between the leafs ( the Quake map compilers' algorithm: start with a box around the map, cut the
//      portals of a node with its plane ), which also gives every leaf its shape
//   3. solid leafs: those whose centre is inside a structural brush
//   4. flood fill from the entities through the portals of the non solid leafs. Reaching the outside of the map is a
//      leak ( the path is reported ); leafs the fill does not reach are the void
//   5. hidden faces: a face is cut by the tree and is drawn when a piece of it borders a reached leaf
//   6. cells: the reached leafs, separated where a portal face of the mapper lies - or, without any, at the closed
//      openings the tree has ( the cross section of a door or window: the leaf portals in one plane that touch and are
//      surrounded by solid ) - and the portals between the cells
//   7. the node stream that puts a point into a cell ( GfxWorld.dpvsPlanes.nodes )

#include "vis.hpp"

#include <queue>

namespace codol::bsp
{
	namespace
	{
		constexpr double ON = 0.05;					// a point this close to a plane is on it
		constexpr double AUTO_PORTAL_AREA = 72000.0;	// an opening larger than this ( square units ) is not made a portal
		constexpr int MAX_CELLS = 1000;

		using winding = std::vector<vec3>;

		struct tnode
		{
			int plane = -1;
			int child[2] = { -1, -1 };
			bool leaf = false;
			std::vector<int> portals;
			bool solid = false, reached = false;
			int cell = -1;
			int from_portal = -1;		// flood fill: the portal the fill came through
			vec3 centre;
		};

		struct tportal
		{
			winding w;
			int node[2] = { -1, -1 };	// [0] is in front of `plane`
			edit::plane plane;
			int plane_index = -1;		// tree plane ( -1: the box around the map )
			int patch = -1;				// cell boundary it belongs to
		};

		struct bface
		{
			winding w;
			int plane = 0;
		};

		double area_of(const winding& w)
		{
			return w.size() < 3 ? 0.0 : edit::winding_area(w);
		}

		vec3 centre_of(const winding& w)
		{
			vec3 c{};
			for (const vec3& p : w)
			{
				c = c + p;
			}
			return w.empty() ? c : c * (1.0 / static_cast<double>(w.size()));
		}

		// 1: in front, 2: behind, 3: both, 0: on the plane
		int classify(const winding& w, const edit::plane& p)
		{
			int mask = 0;
			for (const vec3& v : w)
			{
				const double d = p.distance_to(v);
				mask |= d > ON ? 1 : d < -ON ? 2 : 0;
			}
			return mask;
		}

		winding front_of(winding w, const edit::plane& p)
		{
			edit::clip_winding(w, p.flipped());
			return w;
		}

		winding back_of(winding w, const edit::plane& p)
		{
			edit::clip_winding(w, p);
			return w;
		}

		struct tree
		{
			std::vector<edit::plane> planes;
			std::map<std::array<long long, 4>, int> plane_lookup;
			std::vector<tnode> nodes;
			std::vector<tportal> portals;
			int outside = -1;
			int head = -1;

			int find_plane(edit::plane p)
			{
				int major = 0;
				for (int k = 1; k < 3; k++)
				{
					if (std::fabs(p.normal[k]) > std::fabs(p.normal[major]))
					{
						major = k;
					}
				}
				if (p.normal[major] < 0)
				{
					p = p.flipped();
				}
				const std::array<long long, 4> key = { std::llround(p.normal.x * 8192.0), std::llround(p.normal.y * 8192.0), std::llround(p.normal.z * 8192.0),
					std::llround(p.dist * 32.0) };
				if (const auto it = plane_lookup.find(key); it != plane_lookup.end())
				{
					return it->second;
				}
				planes.push_back(p);
				plane_lookup.emplace(key, static_cast<int>(planes.size() - 1));
				return static_cast<int>(planes.size() - 1);
			}

			int build(std::vector<bface> faces, const int depth)
			{
				const int index = static_cast<int>(nodes.size());
				nodes.push_back({});
				if (faces.empty() || depth > 200)
				{
					nodes[static_cast<std::size_t>(index)].leaf = true;
					return index;
				}
				std::map<int, int> uses;
				for (const bface& f : faces)
				{
					uses[f.plane]++;
				}
				std::vector<int> candidates;
				for (const auto& [plane, count] : uses)
				{
					candidates.push_back(plane);
				}
				if (candidates.size() > 40)
				{
					std::sort(candidates.begin(), candidates.end(), [&](const int a, const int b) { return uses[a] != uses[b] ? uses[a] > uses[b] : a < b; });
					candidates.resize(40);
				}
				int best = candidates[0];
				long long best_score = (1ll << 60);
				for (const int c : candidates)
				{
					const edit::plane& p = planes[static_cast<std::size_t>(c)];
					long long front = 0, back = 0, split = 0;
					for (const bface& f : faces)
					{
						if (f.plane == c)
						{
							continue;
						}
						const int m = classify(f.w, p);
						front += m == 1;
						back += m == 2;
						split += m == 3;
					}
					const bool axial = std::fabs(p.normal.x) > 0.9999 || std::fabs(p.normal.y) > 0.9999 || std::fabs(p.normal.z) > 0.9999;
					const long long score = split * 8 + std::llabs(front - back) + (axial ? 0 : 6);
					if (score < best_score)
					{
						best_score = score;
						best = c;
					}
				}
				const edit::plane p = planes[static_cast<std::size_t>(best)];
				std::vector<bface> front, back;
				for (bface& f : faces)
				{
					if (f.plane == best)
					{
						continue;
					}
					const int m = classify(f.w, p);
					if (m == 1)
					{
						front.push_back(std::move(f));
					}
					else if (m == 2)
					{
						back.push_back(std::move(f));
					}
					else if (m == 3)
					{
						bface a{ front_of(f.w, p), f.plane }, b{ back_of(f.w, p), f.plane };
						if (a.w.size() >= 3)
						{
							front.push_back(std::move(a));
						}
						if (b.w.size() >= 3)
						{
							back.push_back(std::move(b));
						}
					}
				}
				faces.clear();
				faces.shrink_to_fit();
				nodes[static_cast<std::size_t>(index)].plane = best;
				const int f = build(std::move(front), depth + 1);
				nodes[static_cast<std::size_t>(index)].child[0] = f;
				const int b = build(std::move(back), depth + 1);
				nodes[static_cast<std::size_t>(index)].child[1] = b;
				return index;
			}

			void attach(const int portal, const int front, const int back)
			{
				portals[static_cast<std::size_t>(portal)].node[0] = front;
				portals[static_cast<std::size_t>(portal)].node[1] = back;
				nodes[static_cast<std::size_t>(front)].portals.push_back(portal);
				nodes[static_cast<std::size_t>(back)].portals.push_back(portal);
			}

			void detach(const int portal, const int node)
			{
				std::vector<int>& list = nodes[static_cast<std::size_t>(node)].portals;
				list.erase(std::remove(list.begin(), list.end(), portal), list.end());
			}

			void head_portals(const box3& box)
			{
				outside = static_cast<int>(nodes.size());
				nodes.push_back({});
				nodes[static_cast<std::size_t>(outside)].leaf = true;
				edit::plane sides[6];
				for (int k = 0; k < 3; k++)
				{
					sides[2 * k].normal[k] = 1.0;
					sides[2 * k].dist = box.mins[k];
					sides[2 * k + 1].normal[k] = -1.0;
					sides[2 * k + 1].dist = -box.maxs[k];
				}
				for (int s = 0; s < 6; s++)
				{
					winding w = edit::base_winding(sides[s]);
					for (int o = 0; o < 6 && w.size() >= 3; o++)
					{
						if (o != s)
						{
							w = front_of(std::move(w), sides[o]);
						}
					}
					tportal p;
					p.w = std::move(w);
					p.plane = sides[s];
					portals.push_back(std::move(p));
					attach(static_cast<int>(portals.size() - 1), head, outside);
				}
			}

			void make_portals(const int node)
			{
				if (nodes[static_cast<std::size_t>(node)].leaf)
				{
					return;
				}
				const edit::plane plane = planes[static_cast<std::size_t>(nodes[static_cast<std::size_t>(node)].plane)];
				const int c0 = nodes[static_cast<std::size_t>(node)].child[0], c1 = nodes[static_cast<std::size_t>(node)].child[1];
				// the portal of the node itself: its plane, cut by everything that bounds the node
				winding w = edit::base_winding(plane);
				for (const int p : nodes[static_cast<std::size_t>(node)].portals)
				{
					const tportal& portal = portals[static_cast<std::size_t>(p)];
					w = front_of(std::move(w), portal.node[0] == node ? portal.plane : portal.plane.flipped());
					if (w.size() < 3)
					{
						break;
					}
				}
				const std::vector<int> old = nodes[static_cast<std::size_t>(node)].portals;
				nodes[static_cast<std::size_t>(node)].portals.clear();
				if (w.size() >= 3 && area_of(w) > 1e-3)
				{
					tportal np;
					np.w = std::move(w);
					np.plane = plane;
					np.plane_index = nodes[static_cast<std::size_t>(node)].plane;
					portals.push_back(std::move(np));
					attach(static_cast<int>(portals.size() - 1), c0, c1);
				}
				// the portals the node had go to its children
				for (const int p : old)
				{
					const int side = portals[static_cast<std::size_t>(p)].node[0] == node ? 0 : 1;
					const int other = portals[static_cast<std::size_t>(p)].node[1 - side];
					detach(p, other);
					winding f = front_of(portals[static_cast<std::size_t>(p)].w, plane), b = back_of(portals[static_cast<std::size_t>(p)].w, plane);
					const bool has_f = f.size() >= 3 && area_of(f) > 1e-3, has_b = b.size() >= 3 && area_of(b) > 1e-3;
					const auto place = [&](const int portal, const int child)
					{
						if (side == 0)
						{
							attach(portal, child, other);
						}
						else
						{
							attach(portal, other, child);
						}
					};
					if (has_f && has_b)
					{
						tportal q = portals[static_cast<std::size_t>(p)];
						q.w = std::move(b);
						portals[static_cast<std::size_t>(p)].w = std::move(f);
						portals.push_back(std::move(q));
						place(p, c0);
						place(static_cast<int>(portals.size() - 1), c1);
					}
					else if (has_f)
					{
						place(p, c0);
					}
					else if (has_b)
					{
						place(p, c1);
					}
					else
					{
						portals[static_cast<std::size_t>(p)].node[0] = portals[static_cast<std::size_t>(p)].node[1] = -1;
					}
				}
				make_portals(c0);
				make_portals(c1);
			}

			int leaf_of(const vec3& point) const
			{
				int at = head;
				while (!nodes[static_cast<std::size_t>(at)].leaf)
				{
					const tnode& n = nodes[static_cast<std::size_t>(at)];
					at = n.child[planes[static_cast<std::size_t>(n.plane)].distance_to(point) > 0 ? 0 : 1];
				}
				return at;
			}

			// the leafs a face borders: the face is cut by the tree; a piece in a node's plane goes to the side it faces
			void face_leafs(const winding& w, const vec3& normal, const int node, std::vector<int>& out) const
			{
				const tnode& n = nodes[static_cast<std::size_t>(node)];
				if (n.leaf)
				{
					if (area_of(w) > 0.01)
					{
						out.push_back(node);
					}
					return;
				}
				const edit::plane& p = planes[static_cast<std::size_t>(n.plane)];
				const int m = classify(w, p);
				if (m == 0)
				{
					face_leafs(w, normal, n.child[dot(normal, p.normal) > 0 ? 0 : 1], out);
				}
				else if (m == 1)
				{
					face_leafs(w, normal, n.child[0], out);
				}
				else if (m == 2)
				{
					face_leafs(w, normal, n.child[1], out);
				}
				else
				{
					const winding f = front_of(w, p), b = back_of(w, p);
					if (f.size() >= 3)
					{
						face_leafs(f, normal, n.child[0], out);
					}
					if (b.size() >= 3)
					{
						face_leafs(b, normal, n.child[1], out);
					}
				}
			}
		};

		bool inside_brush(const map_brush& b, const vec3& p, const double eps)
		{
			const float f[3] = { static_cast<float>(p.x), static_cast<float>(p.y), static_cast<float>(p.z) };
			if (!b.box.contains(f, 0.1f))
			{
				return false;
			}
			for (const map_face& face : b.faces)
			{
				if (face.plane.distance_to(p) > eps)
				{
					return false;
				}
			}
			return true;
		}

		// convex hull of coplanar points, counter clockwise seen from the front of `plane`
		winding hull_of(const std::vector<vec3>& points, const edit::plane& plane)
		{
			vec3 u_axis, v_axis;
			edit::face_texture_axes(plane.normal, 0.0, u_axis, v_axis);
			// an orthonormal pair in the plane
			vec3 a = u_axis - plane.normal * dot(u_axis, plane.normal);
			a = a * (1.0 / std::max(length(a), 1e-9));
			const vec3 b = cross(plane.normal, a);
			struct p2
			{
				double x, y;
				vec3 source;
			};
			std::vector<p2> pts;
			for (const vec3& p : points)
			{
				pts.push_back({ dot(p, a), dot(p, b), p });
			}
			std::sort(pts.begin(), pts.end(), [](const p2& l, const p2& r) { return l.x != r.x ? l.x < r.x : l.y < r.y; });
			const auto turn = [](const p2& o, const p2& s, const p2& t) { return (s.x - o.x) * (t.y - o.y) - (s.y - o.y) * (t.x - o.x); };
			std::vector<p2> hull;
			for (int pass = 0; pass < 2; pass++)
			{
				const std::size_t start = hull.size();
				for (const p2& p : pts)
				{
					while (hull.size() >= start + 2 && turn(hull[hull.size() - 2], hull.back(), p) <= 0.01)
					{
						hull.pop_back();
					}
					hull.push_back(p);
				}
				hull.pop_back();
				std::reverse(pts.begin(), pts.end());
			}
			winding out;
			for (const p2& p : hull)
			{
				out.push_back(p.source);
			}
			return out;
		}

		bool point_in_winding(const vec3& p, const winding& w, const vec3& normal)
		{
			for (std::size_t k = 0; k < w.size(); k++)
			{
				const vec3 edge = w[(k + 1) % w.size()] - w[k];
				if (dot(cross(normal, edge), p - w[k]) < -0.1)
				{
					return false;
				}
			}
			return true;
		}
	}

	void build_vis(compile_context& ctx, vis_result& out)
	{
		world& w = ctx.w;
		report& rep = ctx.rep;

		// ---- the tree
		tree t;
		std::vector<bface> faces;
		std::vector<const map_brush*> opaque;
		for (const map_brush& b : w.brushes)
		{
			if (!b.opaque)
			{
				continue;
			}
			opaque.push_back(&b);
			for (const map_face& f : b.faces)
			{
				if (f.winding.size() >= 3)
				{
					faces.push_back({ f.winding, t.find_plane(f.plane) });
				}
			}
		}
		if (opaque.empty())
		{
			return;
		}
		std::vector<int> portal_face_plane;
		for (const map_face& f : w.portal_faces)
		{
			portal_face_plane.push_back(t.find_plane(f.plane));
			faces.push_back({ f.winding, portal_face_plane.back() });
		}
		t.head = t.build(std::move(faces), 0);
		box3 outer = w.bounds;
		outer.expand(256.0f);
		t.head_portals(outer);
		t.make_portals(t.head);

		// ---- leafs: centre, solid
		for (std::size_t n = 0; n < t.nodes.size(); n++)
		{
			tnode& node = t.nodes[n];
			if (!node.leaf || static_cast<int>(n) == t.outside)
			{
				continue;
			}
			out.leafs++;
			vec3 sum{};
			std::size_t count = 0;
			for (const int p : node.portals)
			{
				for (const vec3& v : t.portals[static_cast<std::size_t>(p)].w)
				{
					sum = sum + v;
					count++;
				}
			}
			if (node.portals.size() < 4 || count == 0)
			{
				node.solid = true;		// no volume
				out.solid_leafs++;
				continue;
			}
			node.centre = sum * (1.0 / static_cast<double>(count));
			for (const map_brush* b : opaque)
			{
				if (inside_brush(*b, node.centre, 0.02))
				{
					node.solid = true;
					break;
				}
			}
			out.solid_leafs += node.solid;
		}
		out.tree_nodes = t.nodes.size() - 1 - out.leafs;
		out.tree_portals = t.portals.size();

		// self check: the centre of a leaf lies behind / in front of every portal that bounds it
		for (std::size_t n = 0; n < t.nodes.size() && out.invariant_error.empty(); n++)
		{
			const tnode& node = t.nodes[n];
			if (!node.leaf || node.solid || static_cast<int>(n) == t.outside)
			{
				continue;
			}
			for (const int p : node.portals)
			{
				const tportal& portal = t.portals[static_cast<std::size_t>(p)];
				const double d = portal.plane.distance_to(node.centre);
				if ((portal.node[0] == static_cast<int>(n)) != (d > -0.01) && std::fabs(d) > 0.01)
				{
					out.invariant_error = format("leaf %zu: a portal is on the wrong side of the leaf centre ( %.3f )", n, d);
					break;
				}
			}
		}

		// ---- flood fill from the entities
		std::queue<int> queue;
		std::vector<int> start_entity(t.nodes.size(), -1);
		for (std::size_t e = 1; e < w.entities.size(); e++)
		{
			const map_entity& entity = w.entities[e];
			if (!entity.brushes.empty() || entity.value("origin").empty())
			{
				continue;
			}
			const int leaf = t.leaf_of(entity.origin + vec3{ 0, 0, 1 });
			tnode& node = t.nodes[static_cast<std::size_t>(leaf)];
			if (node.solid)
			{
				continue;
			}
			if (!node.reached)
			{
				node.reached = true;
				start_entity[static_cast<std::size_t>(leaf)] = static_cast<int>(e);
				queue.push(leaf);
			}
		}
		const bool seeded = !queue.empty();
		int leak_leaf = -1, leak_portal = -1;
		while (!queue.empty())
		{
			const int leaf = queue.front();
			queue.pop();
			for (const int p : t.nodes[static_cast<std::size_t>(leaf)].portals)
			{
				const tportal& portal = t.portals[static_cast<std::size_t>(p)];
				const int other = portal.node[0] == leaf ? portal.node[1] : portal.node[0];
				if (other == t.outside)
				{
					if (leak_leaf < 0)
					{
						leak_leaf = leaf;
						leak_portal = p;
					}
					continue;
				}
				tnode& next = t.nodes[static_cast<std::size_t>(other)];
				if (next.solid || next.reached)
				{
					continue;
				}
				next.reached = true;
				next.from_portal = p;
				queue.push(other);
			}
		}
		if (leak_leaf >= 0)
		{
			rep.leaked = true;
			std::vector<vec3> path;
			path.push_back(centre_of(t.portals[static_cast<std::size_t>(leak_portal)].w));
			int at = leak_leaf;
			while (true)
			{
				path.push_back(t.nodes[static_cast<std::size_t>(at)].centre);
				const int p = t.nodes[static_cast<std::size_t>(at)].from_portal;
				if (p < 0)
				{
					break;
				}
				path.push_back(centre_of(t.portals[static_cast<std::size_t>(p)].w));
				at = t.portals[static_cast<std::size_t>(p)].node[0] == at ? t.portals[static_cast<std::size_t>(p)].node[1] : t.portals[static_cast<std::size_t>(p)].node[0];
			}
			const map_entity& entity = w.entities[static_cast<std::size_t>(std::max(start_entity[static_cast<std::size_t>(at)], 0))];
			rep.leak_entity = format("%s at ( %.0f %.0f %.0f )", entity.classname.c_str(), entity.origin.x, entity.origin.y, entity.origin.z);
			path.push_back(entity.origin);
			std::reverse(path.begin(), path.end());
			for (const vec3& p : path)
			{
				rep.leak_path.push_back({ { static_cast<float>(p.x), static_cast<float>(p.y), static_cast<float>(p.z) } });
			}
			rep.warnings.push_back("LEAK: " + rep.leak_entity + " reaches the outside of the map; nothing is treated as hidden and the map is one cell");
		}
		if (!seeded)
		{
			rep.warnings.push_back("vis: no point entity lies in open space, so inside and outside of the map cannot be told apart; nothing is treated as hidden");
		}
		const bool sealed = seeded && leak_leaf < 0;
		for (tnode& node : t.nodes)
		{
			if (node.leaf && !node.solid && !sealed)
			{
				node.reached = true;
			}
		}
		t.nodes[static_cast<std::size_t>(t.outside)].reached = false;
		for (const tnode& node : t.nodes)
		{
			if (node.leaf && !node.solid)
			{
				(node.reached ? out.empty_leafs : out.void_leafs)++;
			}
		}
		out.void_leafs -= 1;		// the outside node

		// ---- cell boundaries
		struct patch
		{
			std::vector<int> portals;
			winding polygon;
			edit::plane plane;
		};
		std::vector<patch> patches;
		const auto open_portal = [&](const tportal& p)
		{
			return p.node[0] >= 0 && p.node[1] >= 0 && p.plane_index >= 0 && t.nodes[static_cast<std::size_t>(p.node[0])].reached
				&& t.nodes[static_cast<std::size_t>(p.node[1])].reached;
		};
		if (sealed && ctx.opts.vis && !w.portal_faces.empty())
		{
			patches.resize(w.portal_faces.size());
			for (std::size_t f = 0; f < w.portal_faces.size(); f++)
			{
				patches[f].polygon = w.portal_faces[f].winding;
				patches[f].plane = t.planes[static_cast<std::size_t>(portal_face_plane[f])];
			}
			for (std::size_t p = 0; p < t.portals.size(); p++)
			{
				tportal& portal = t.portals[p];
				if (!open_portal(portal))
				{
					continue;
				}
				for (std::size_t f = 0; f < w.portal_faces.size(); f++)
				{
					if (portal_face_plane[f] == portal.plane_index && point_in_winding(centre_of(portal.w), w.portal_faces[f].winding, w.portal_faces[f].plane.normal))
					{
						portal.patch = static_cast<int>(f);
						patches[f].portals.push_back(static_cast<int>(p));
						break;
					}
				}
			}
		}
		else if (sealed && ctx.opts.vis)
		{
			// closed openings: per plane, the open portals that touch form a patch; a small convex patch is a boundary
			std::map<int, std::vector<int>> by_plane;
			for (std::size_t p = 0; p < t.portals.size(); p++)
			{
				if (open_portal(t.portals[p]))
				{
					by_plane[t.portals[p].plane_index].push_back(static_cast<int>(p));
				}
			}
			for (const auto& [plane_index, list] : by_plane)
			{
				std::vector<int> group(list.size());
				for (std::size_t k = 0; k < list.size(); k++)
				{
					group[k] = static_cast<int>(k);
				}
				const auto root = [&](int k)
				{
					while (group[static_cast<std::size_t>(k)] != k)
					{
						k = group[static_cast<std::size_t>(k)] = group[static_cast<std::size_t>(group[static_cast<std::size_t>(k)])];
					}
					return k;
				};
				for (std::size_t a = 0; a < list.size(); a++)
				{
					for (std::size_t b = a + 1; b < list.size(); b++)
					{
						int shared = 0;
						for (const vec3& p : t.portals[static_cast<std::size_t>(list[a])].w)
						{
							for (const vec3& q : t.portals[static_cast<std::size_t>(list[b])].w)
							{
								shared += length(p - q) < 0.1;
							}
						}
						if (shared >= 2)
						{
							group[static_cast<std::size_t>(root(static_cast<int>(a)))] = root(static_cast<int>(b));
						}
					}
				}
				std::map<int, std::vector<int>> members;
				for (std::size_t k = 0; k < list.size(); k++)
				{
					members[root(static_cast<int>(k))].push_back(list[k]);
				}
				for (const auto& [r, portals] : members)
				{
					double area = 0;
					std::vector<vec3> points;
					for (const int p : portals)
					{
						area += area_of(t.portals[static_cast<std::size_t>(p)].w);
						points.insert(points.end(), t.portals[static_cast<std::size_t>(p)].w.begin(), t.portals[static_cast<std::size_t>(p)].w.end());
					}
					if (area > AUTO_PORTAL_AREA || area < 64.0)
					{
						continue;
					}
					const edit::plane& plane = t.planes[static_cast<std::size_t>(plane_index)];
					winding hull = hull_of(points, plane);
					if (hull.size() < 3 || hull.size() > 32 || std::fabs(area_of(hull) - area) > area * 0.01 + 1.0)
					{
						continue;		// not convex
					}
					patch pt;
					pt.portals = portals;
					pt.polygon = std::move(hull);
					pt.plane = plane;
					for (const int p : portals)
					{
						t.portals[static_cast<std::size_t>(p)].patch = static_cast<int>(patches.size());
					}
					patches.push_back(std::move(pt));
				}
			}
		}

		// ---- cells: reached leafs connected without crossing a boundary
		int cell_count = 0;
		const auto flood_cells = [&]
		{
			cell_count = 0;
			for (tnode& node : t.nodes)
			{
				node.cell = -1;
			}
			for (std::size_t n = 0; n < t.nodes.size(); n++)
			{
				if (!t.nodes[n].leaf || !t.nodes[n].reached || t.nodes[n].cell >= 0)
				{
					continue;
				}
				std::vector<int> stack{ static_cast<int>(n) };
				t.nodes[n].cell = cell_count;
				while (!stack.empty())
				{
					const int leaf = stack.back();
					stack.pop_back();
					for (const int p : t.nodes[static_cast<std::size_t>(leaf)].portals)
					{
						const tportal& portal = t.portals[static_cast<std::size_t>(p)];
						if (portal.patch >= 0)
						{
							continue;
						}
						const int other = portal.node[0] == leaf ? portal.node[1] : portal.node[0];
						if (other < 0 || !t.nodes[static_cast<std::size_t>(other)].reached || t.nodes[static_cast<std::size_t>(other)].cell >= 0)
						{
							continue;
						}
						t.nodes[static_cast<std::size_t>(other)].cell = cell_count;
						stack.push_back(other);
					}
				}
				cell_count++;
			}
		};
		flood_cells();
		// a boundary with the same cell on both sides separates nothing: it is dropped, so that it does not stop the fill
		for (bool changed = true; changed;)
		{
			changed = false;
			for (std::size_t k = 0; k < patches.size(); k++)
			{
				patch& pt = patches[k];
				if (pt.portals.empty())
				{
					continue;
				}
				const tportal& first = t.portals[static_cast<std::size_t>(pt.portals[0])];
				if (t.nodes[static_cast<std::size_t>(first.node[0])].cell == t.nodes[static_cast<std::size_t>(first.node[1])].cell)
				{
					for (const int p : pt.portals)
					{
						t.portals[static_cast<std::size_t>(p)].patch = -1;
					}
					pt.portals.clear();
				}
			}
		}
		if (cell_count > MAX_CELLS)
		{
			rep.warnings.push_back(format("vis: %d cells are more than the %d this compiler writes; the map is one cell", cell_count, MAX_CELLS));
			for (tportal& p : t.portals)
			{
				p.patch = -1;
			}
			patches.clear();
			flood_cells();
		}
		if (cell_count == 0)
		{
			return;
		}

		out.cells.assign(static_cast<std::size_t>(cell_count), {});
		for (const tnode& node : t.nodes)
		{
			if (!node.leaf || node.cell < 0)
			{
				continue;
			}
			for (const int p : node.portals)
			{
				for (const vec3& v : t.portals[static_cast<std::size_t>(p)].w)
				{
					out.cells[static_cast<std::size_t>(node.cell)].box.add(v);
				}
			}
		}
		for (const patch& pt : patches)
		{
			if (pt.portals.empty() || pt.polygon.size() < 3)
			{
				continue;
			}
			const tportal& first = t.portals[static_cast<std::size_t>(pt.portals[0])];
			const int front_cell = t.nodes[static_cast<std::size_t>(first.node[0])].cell, back_cell = t.nodes[static_cast<std::size_t>(first.node[1])].cell;
			if (front_cell < 0 || back_cell < 0 || front_cell == back_cell)
			{
				continue;
			}
			// two portals: one in each cell, its normal pointing into the other cell, its corners clockwise seen from
			// that side - as the portals of the shipped maps are
			for (int side = 0; side < 2; side++)
			{
				const int own = side == 0 ? front_cell : back_cell, target = side == 0 ? back_cell : front_cell;
				const vec3 normal = side == 0 ? pt.plane.normal * -1.0 : pt.plane.normal;
				winding polygon = pt.polygon;
				const vec3 facing = cross(polygon[1] - polygon[0], polygon[2] - polygon[0]);
				if (dot(facing, normal) > 0)
				{
					std::reverse(polygon.begin(), polygon.end());
				}
				render_portal rp;
				rp.cell = target;
				rp.plane[0] = static_cast<float>(normal.x);
				rp.plane[1] = static_cast<float>(normal.y);
				rp.plane[2] = static_cast<float>(normal.z);
				rp.plane[3] = static_cast<float>(-dot(normal, polygon[0]));
				for (const vec3& v : polygon)
				{
					rp.vertices.push_back(static_cast<float>(v.x));
					rp.vertices.push_back(static_cast<float>(v.y));
					rp.vertices.push_back(static_cast<float>(v.z));
				}
				vec3 axis0 = std::fabs(normal.z) < 0.99 ? vec3{ 0, 0, 1 } : vec3{ 1, 0, 0 };
				axis0 = axis0 - normal * dot(axis0, normal);
				axis0 = axis0 * (1.0 / length(axis0));
				const vec3 axis1 = cross(normal, axis0);
				for (int k = 0; k < 3; k++)
				{
					rp.hull_axis[0][k] = static_cast<float>(axis0[k]);
					rp.hull_axis[1][k] = static_cast<float>(axis1[k]);
				}
				out.cells[static_cast<std::size_t>(own)].portals.push_back(std::move(rp));
				out.portals++;
			}
		}

		// ---- faces: hidden ones, and the cells the others are seen from
		for (map_brush& b : w.brushes)
		{
			if (b.model != 0)
			{
				continue;
			}
			for (map_face& f : b.faces)
			{
				if (f.winding.size() < 3 || is_tool_material(f.material))
				{
					continue;
				}
				std::vector<int> leafs;
				t.face_leafs(f.winding, f.plane.normal, t.head, leafs);
				std::set<int> cells;
				for (const int leaf : leafs)
				{
					if (t.nodes[static_cast<std::size_t>(leaf)].cell >= 0)
					{
						cells.insert(t.nodes[static_cast<std::size_t>(leaf)].cell);
					}
				}
				f.cells.assign(cells.begin(), cells.end());
				bool hidden = cells.empty();
				if (!hidden && (b.contents & 1))
				{
					// covered by another solid brush ( a crate on a crate ): the tree only knows the structural ones
					const vec3 centre = centre_of(f.winding);
					for (const map_brush& other : w.brushes)
					{
						if (&other == &b || other.model != 0 || !(other.contents & 1) || !other.box.intersects(b.box, 0.1f))
						{
							continue;
						}
						bool covered = true;
						for (const vec3& p : f.winding)
						{
							const vec3 probe = p + (centre - p) * 0.01 + f.plane.normal * 0.05;
							covered = covered && inside_brush(other, probe, 0.01);
						}
						if (covered)
						{
							hidden = true;
							break;
						}
					}
				}
				if (hidden)
				{
					f.visible = false;
					rep.faces_culled++;
				}
			}
		}

		// ---- the node stream: a leaf is its cell + 1 ( 0: none ); a split is plane + cell count + 1, then the offset to
		// its back child; sub trees that are one cell throughout are a single leaf
		struct emit
		{
			const tree& t;
			world& w;
			int cells;
			std::vector<std::uint16_t> run(const int node) const
			{
				const tnode& n = t.nodes[static_cast<std::size_t>(node)];
				if (n.leaf)
				{
					return { static_cast<std::uint16_t>(n.cell + 1) };
				}
				std::vector<std::uint16_t> front = run(n.child[0]);
				const std::vector<std::uint16_t> back = run(n.child[1]);
				if (front.size() == 1 && back.size() == 1 && front[0] == back[0])
				{
					return front;
				}
				const edit::plane& p = t.planes[static_cast<std::size_t>(n.plane)];
				const double normal[3] = { p.normal.x, p.normal.y, p.normal.z };
				const std::uint32_t plane = w.planes.add(normal, p.dist);
				if (plane + static_cast<std::uint32_t>(cells) + 1 > 0xFFFF || front.size() + 2 > 0xFFFF)
				{
					throw compile_error("vis: the cell tree does not fit 16 bit node values");
				}
				std::vector<std::uint16_t> all{ static_cast<std::uint16_t>(plane + static_cast<std::uint32_t>(cells) + 1), static_cast<std::uint16_t>(front.size() + 2) };
				all.insert(all.end(), front.begin(), front.end());
				all.insert(all.end(), back.begin(), back.end());
				return all;
			}
		};
		out.cell_nodes = emit{ t, w, cell_count }.run(t.head);
		out.built = true;
		rep.cells = static_cast<std::size_t>(cell_count);
		rep.portals = out.portals;
		rep.notes.push_back(format("vis: %zu tree nodes, %zu leafs ( %zu solid, %zu open, %zu void ), %zu leaf portals; %d cells, %zu portals; %zu faces hidden%s",
			out.tree_nodes, out.leafs, out.solid_leafs, out.empty_leafs, out.void_leafs, out.tree_portals, cell_count, out.portals, rep.faces_culled,
			out.invariant_error.empty() ? "" : ( "; SELF CHECK FAILED: " + out.invariant_error ).c_str()));
	}
}
