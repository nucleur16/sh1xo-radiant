// T-junction repair of the world's render geometry.
//
// Every face is drawn as its own polygon: where the corner of one face lies in the middle of an edge of another - a
// wall that ends halfway along a floor, a crate on the ground, a face the light stage cut, a terrain mesh along a
// brush - the two edges are the same line for the eye and different lines for the rasteriser, which leaves single
// pixels of the background along the joint ( "sparkles" ). The repair is the one of the Quake map compilers: every
// vertex of the world that lies on an edge of a face becomes a vertex of that face, and the face is cut into
// triangles again so that none of them has such a vertex in the middle of a side.
//
// The new vertices are points of the old edge: position, texture and lightmap coordinates are those of the edge at
// that point ( the position is the other face's vertex to the bit ), so nothing else changes.

#include "render.hpp"

namespace codol::bsp
{
	namespace
	{
		constexpr double CELL = 64.0;			// of the point hash
		constexpr double ON_EDGE = 0.02;		// a vertex this near an edge's line lies on it
		constexpr double APART = 0.05;			// .. and this far from the next vertex of the edge is a new one

		struct point
		{
			float xyz[3];
		};

		std::uint64_t cell_key(const int x, const int y, const int z)
		{
			return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(x) & 0x1FFFFF) << 42) | (static_cast<std::uint64_t>(static_cast<std::uint32_t>(y) & 0x1FFFFF) << 21)
				| (static_cast<std::uint32_t>(z) & 0x1FFFFF);
		}

		int cell_coord(const double v)
		{
			return static_cast<int>(std::floor(v / CELL));
		}

		c1::GfxWorldVertex on_edge(const c1::GfxWorldVertex& a, const c1::GfxWorldVertex& b, const double t, const point& p)
		{
			c1::GfxWorldVertex out = t < 0.5 ? a : b;		// normal, tangent, binormal sign of the nearer end
			const float f = static_cast<float>(t);
			std::memcpy(out.xyz, p.xyz, sizeof(out.xyz));
			for (int k = 0; k < 2; k++)
			{
				out.texCoord[k] = a.texCoord[k] + (b.texCoord[k] - a.texCoord[k]) * f;
				out.lmapCoord[k] = a.lmapCoord[k] + (b.lmapCoord[k] - a.lmapCoord[k]) * f;
			}
			std::uint32_t color = 0;
			for (int shift = 0; shift < 32; shift += 8)
			{
				const double ca = (a.color >> shift) & 0xFF, cb = (b.color >> shift) & 0xFF;
				color |= static_cast<std::uint32_t>(std::clamp(std::lround(ca + (cb - ca) * t), 0l, 255l)) << shift;
			}
			out.color = color;
			return out;
		}

		vec3 position(const c1::GfxWorldVertex& v)
		{
			return { v.xyz[0], v.xyz[1], v.xyz[2] };
		}

		// Triangles of a convex polygon that may have vertices in the middle of its sides: ears are cut off that have
		// an area and leave a rest with an area, so every vertex ends up as a corner of a triangle and no triangle has
		// a vertex inside a side. `out`: indices into the polygon, three per triangle, in the polygon's order.
		void triangulate(const std::vector<vec3>& polygon, std::vector<std::uint32_t>& out)
		{
			std::vector<std::uint32_t> rest(polygon.size());
			for (std::uint32_t k = 0; k < rest.size(); k++)
			{
				rest[k] = k;
			}
			vec3 normal{ 0, 0, 0 };
			for (std::size_t k = 2; k < polygon.size(); k++)
			{
				normal = normal + cross(polygon[k - 1] - polygon[0], polygon[k] - polygon[0]);
			}
			const double area2 = length(normal);
			if (area2 < 1e-9)
			{
				return;
			}
			normal = normal * (1.0 / area2);
			const double least = std::max(1e-7, area2 * 1e-9);
			const auto rest_area = [&](const std::size_t without)
			{
				vec3 sum{ 0, 0, 0 };
				std::size_t first = npos, last = npos;
				for (std::size_t k = 0; k < rest.size(); k++)
				{
					if (k == without)
					{
						continue;
					}
					if (first == npos)
					{
						first = k;
					}
					else if (last != npos)
					{
						sum = sum + cross(polygon[rest[last]] - polygon[rest[first]], polygon[rest[k]] - polygon[rest[first]]);
					}
					last = k;
				}
				return dot(sum, normal);
			};
			while (rest.size() > 3)
			{
				std::size_t ear = npos;
				double best = 0;
				for (std::size_t k = 0; k < rest.size(); k++)
				{
					const vec3& a = polygon[rest[(k + rest.size() - 1) % rest.size()]];
					const vec3& b = polygon[rest[k]];
					const vec3& c = polygon[rest[(k + 1) % rest.size()]];
					const double area = dot(cross(b - a, c - b), normal);
					// the fattest ear: the one whose shortest side is longest would be better still, this is enough
					if (area > least && area > best && rest_area(k) > least)
					{
						best = area;
						ear = k;
					}
				}
				if (ear == npos)
				{
					break;
				}
				out.push_back(rest[(ear + rest.size() - 1) % rest.size()]);
				out.push_back(rest[ear]);
				out.push_back(rest[(ear + 1) % rest.size()]);
				rest.erase(rest.begin() + static_cast<std::ptrdiff_t>(ear));
			}
			// what is left: a triangle - or, when no ear was found ( numbers ), a fan
			for (std::size_t k = 2; k < rest.size(); k++)
			{
				out.push_back(rest[0]);
				out.push_back(rest[k - 1]);
				out.push_back(rest[k]);
			}
		}
	}

	void repair_t_junctions(compile_context& ctx, render_world& rw)
	{
		if (std::getenv("CODOL_BSP_NO_TJUNC"))
		{
			return;
		}
		// the vertices of every model's surfaces, by cell ( a brush model moves: its faces only meet each other )
		std::vector<std::unordered_map<std::uint64_t, std::vector<point>>> points(rw.models.size());
		for (const render_surface& surface : rw.surfaces)
		{
			if (surface.sky)
			{
				continue;
			}
			auto& hash = points[static_cast<std::size_t>(surface.model)];
			for (const c1::GfxWorldVertex& v : surface.vertices)
			{
				std::vector<point>& list = hash[cell_key(cell_coord(v.xyz[0]), cell_coord(v.xyz[1]), cell_coord(v.xyz[2]))];
				bool known = false;
				for (const point& p : list)
				{
					known = known || (p.xyz[0] == v.xyz[0] && p.xyz[1] == v.xyz[1] && p.xyz[2] == v.xyz[2]);
				}
				if (!known)
				{
					list.push_back({ { v.xyz[0], v.xyz[1], v.xyz[2] } });
				}
			}
		}
		std::size_t added = 0, faces_changed = 0, surfaces_full = 0;
		for (render_surface& surface : rw.surfaces)
		{
			if (surface.sky)
			{
				continue;
			}
			const auto& hash = points[static_cast<std::size_t>(surface.model)];
			std::vector<c1::GfxWorldVertex> vertices;
			std::vector<std::uint16_t> indices;
			std::vector<render_face> faces;
			bool changed = false, full = false;
			for (const render_face& rf : surface.faces)
			{
				std::vector<c1::GfxWorldVertex> polygon;
				bool grown = false;
				for (std::uint32_t k = 0; k < rf.vertex_count; k++)
				{
					const c1::GfxWorldVertex& a = surface.vertices[rf.first_vertex + k];
					const c1::GfxWorldVertex& b = surface.vertices[rf.first_vertex + (k + 1) % rf.vertex_count];
					polygon.push_back(a);
					const vec3 pa = position(a), pb = position(b), along = pb - pa;
					const double len = length(along);
					if (len < 2 * APART || full)
					{
						continue;
					}
					// the cells the edge runs through, and those around them
					std::set<std::uint64_t> cells;
					const int steps = static_cast<int>(std::ceil(len / (CELL * 0.5)));
					for (int s = 0; s <= steps; s++)
					{
						const vec3 p = pa + along * (static_cast<double>(s) / steps);
						const int cx = cell_coord(p.x), cy = cell_coord(p.y), cz = cell_coord(p.z);
						for (int dx = -1; dx <= 1; dx++)
						{
							for (int dy = -1; dy <= 1; dy++)
							{
								for (int dz = -1; dz <= 1; dz++)
								{
									// only cells the edge comes near
									const vec3 centre{ (cx + dx + 0.5) * CELL, (cy + dy + 0.5) * CELL, (cz + dz + 0.5) * CELL };
									const double t = std::clamp(dot(centre - pa, along) / (len * len), 0.0, 1.0);
									const vec3 d = centre - (pa + along * t);
									if (std::max({ std::fabs(d.x), std::fabs(d.y), std::fabs(d.z) }) <= CELL * 0.5 + ON_EDGE)
									{
										cells.insert(cell_key(cx + dx, cy + dy, cz + dz));
									}
								}
							}
						}
					}
					std::vector<std::pair<double, point>> on;
					for (const std::uint64_t key : cells)
					{
						const auto it = hash.find(key);
						if (it == hash.end())
						{
							continue;
						}
						for (const point& p : it->second)
						{
							const vec3 q{ p.xyz[0], p.xyz[1], p.xyz[2] };
							const double t = dot(q - pa, along) / (len * len);
							if (t * len < APART || (1.0 - t) * len < APART)
							{
								continue;
							}
							if (length(q - (pa + along * t)) < ON_EDGE)
							{
								on.emplace_back(t, p);
							}
						}
					}
					std::sort(on.begin(), on.end(), [](const std::pair<double, point>& x, const std::pair<double, point>& y) { return x.first < y.first; });
					double last = 0;
					for (const auto& [t, p] : on)
					{
						if ((t - last) * len < APART)
						{
							continue;
						}
						last = t;
						polygon.push_back(on_edge(a, b, t, p));
						grown = true;
					}
				}
				if (grown && vertices.size() + polygon.size() > 0xFFFF)
				{
					// the surface is full: the rest of its faces stay as they are
					full = true;
					surfaces_full++;
					polygon.assign(surface.vertices.begin() + rf.first_vertex, surface.vertices.begin() + rf.first_vertex + rf.vertex_count);
					grown = false;
				}
				render_face out = rf;
				out.first_vertex = static_cast<std::uint32_t>(vertices.size());
				out.vertex_count = static_cast<std::uint32_t>(polygon.size());
				std::vector<std::uint32_t> triangles;
				if (grown)
				{
					std::vector<vec3> corners;
					for (const c1::GfxWorldVertex& v : polygon)
					{
						corners.push_back(position(v));
					}
					triangulate(corners, triangles);
					added += polygon.size() - rf.vertex_count;
					faces_changed++;
					changed = true;
				}
				if (triangles.empty())
				{
					for (std::uint32_t k = 2; k < out.vertex_count; k++)
					{
						triangles.insert(triangles.end(), { 0u, k - 1, k });
					}
				}
				// the polygon is counter clockwise seen from the front, the game's triangles are clockwise
				for (std::size_t k = 0; k + 2 < triangles.size(); k += 3)
				{
					indices.push_back(static_cast<std::uint16_t>(out.first_vertex + triangles[k]));
					indices.push_back(static_cast<std::uint16_t>(out.first_vertex + triangles[k + 2]));
					indices.push_back(static_cast<std::uint16_t>(out.first_vertex + triangles[k + 1]));
				}
				vertices.insert(vertices.end(), polygon.begin(), polygon.end());
				faces.push_back(out);
			}
			if (changed)
			{
				surface.vertices = std::move(vertices);
				surface.indices = std::move(indices);
				surface.faces = std::move(faces);
			}
		}
		ctx.rep.notes.push_back(format("render: T-junctions: %zu vertices added to %zu faces%s", added, faces_changed,
			surfaces_full ? format(" ( %zu surfaces are full: some of their faces were left )", surfaces_full).c_str() : ""));
	}
}
