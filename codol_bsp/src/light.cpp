// The light bake: lightmaps and the surfaces' lightmap coordinates, the light grid, reflection probes.
//
// How the game's lit shaders use the data was measured by rendering constant lightmaps with codol_viewtool, which
// runs those shaders ( tests/harness/lm_experiment.ps1 ):
//
//   primary lightmap    L8, 2 x the resolution of a secondary layer: how much of the sun reaches the texel. The sun
//                       itself is added by the shader ( ComWorld light 1 ), with the dynamic sun shadow on top.
//   secondary lightmap  A8R8G8B8, two layers on top of each other. The upper layer ( rows 0 .. LM_H ) is the baked
//                       light without the sun, linear: 255 comes out as about 0.86 of what full sun gives. The lower
//                       layer adds much less and not linearly ( 64 -> 7, 255 -> 45 of 255 ); what it stands for was
//                       not established and it is written black. Alpha has no effect.
//
// So: sun visibility goes to the primary image, everything else - ambient, sky light, the light entities with their
// shadows - to the upper layer of the secondary image. Light entities are baked, they are not primary lights.

#include "render.hpp"

#include <atomic>
#include <thread>

namespace codol::bsp
{
	using namespace gfx;

	namespace
	{
		constexpr double PI = 3.14159265358979323846;

		std::uint8_t to_byte(const float v)
		{
			return static_cast<std::uint8_t>(std::clamp(std::lround(v * 255.0f), 0l, 255l));
		}

		// 64 x 64 cube map, 6 faces, 7 mip levels, one colour
		std::vector<std::uint8_t> flat_probe(const float color[3])
		{
			std::vector<std::uint8_t> pixels(131064);
			for (std::size_t k = 0; k < pixels.size(); k += 4)
			{
				pixels[k] = to_byte(color[2]);
				pixels[k + 1] = to_byte(color[1]);
				pixels[k + 2] = to_byte(color[0]);
				pixels[k + 3] = 255;
			}
			return pixels;
		}

		// the grid cell of a world position: 32 units in x and y, 64 in z
		int grid_coord(const double v, const int axis)
		{
			const int i = static_cast<int>(std::floor(v)) + 0x20000;
			return axis == 2 ? i >> 6 : i >> 5;
		}

		double grid_position(const int coord, const int axis)
		{
			return axis == 2 ? (coord << 6) - 0x20000 + 32.0 : (coord << 5) - 0x20000 + 16.0;
		}

		// ---- rays against the solid brushes of the world -----------------------------------------
		struct ray_hit
		{
			double t = 0;
			int brush = -1, face = -1;
		};

		struct ray_world
		{
			struct node
			{
				box3 box;
				int child[2] = { -1, -1 };
				int first = 0, count = 0;
			};
			// a triangle that blocks light without being a brush: static models ( LOD 0 )
			struct triangle
			{
				vec3 a, e1, e2;
				box3 box;
				int brush = 0, face = -1;		// a patch triangle: -1 - patch, triangle ( the names its lightmap chart has ); face -1: a model
			};
			const world& w;
			std::vector<int> brushes;		// world::brushes indices, in tree order
			std::vector<node> nodes;
			std::vector<std::uint8_t> sky;	// per world brush: has a sky face
			std::vector<triangle> triangles;
			std::vector<node> triangle_nodes;

			int build_triangles(const int first, const int count)
			{
				const int index = static_cast<int>(triangle_nodes.size());
				triangle_nodes.push_back({});
				box3 box;
				for (int k = first; k < first + count; k++)
				{
					box.add(triangles[static_cast<std::size_t>(k)].box);
				}
				triangle_nodes[static_cast<std::size_t>(index)].box = box;
				if (count <= 4)
				{
					triangle_nodes[static_cast<std::size_t>(index)].first = first;
					triangle_nodes[static_cast<std::size_t>(index)].count = count;
					return index;
				}
				int axis = 0;
				for (int k = 1; k < 3; k++)
				{
					if (box.maxs[k] - box.mins[k] > box.maxs[axis] - box.mins[axis])
					{
						axis = k;
					}
				}
				std::sort(triangles.begin() + first, triangles.begin() + first + count, [&](const triangle& x, const triangle& y)
				{
					return x.box.mins[axis] + x.box.maxs[axis] < y.box.mins[axis] + y.box.maxs[axis];
				});
				const int half = count / 2;
				const int l = build_triangles(first, half);
				triangle_nodes[static_cast<std::size_t>(index)].child[0] = l;
				const int r = build_triangles(first + half, count - half);
				triangle_nodes[static_cast<std::size_t>(index)].child[1] = r;
				return index;
			}

			// does the ray meet a triangle within max_t? one_sided: triangles seen from behind do not count - a sample
			// inside a closed model ( the light grid point a model is lit by ) sees out of it
			bool hits_triangle(const vec3& o, const vec3& d, const double max_t, const bool one_sided) const
			{
				double t = max_t;
				return nearest_triangle(o, d, t, one_sided, true) >= 0;
			}

			// the first triangle the ray meets before `best` ( which becomes its distance ); -1: none. any: stop at the
			// first one found
			int nearest_triangle(const vec3& o, const vec3& d, double& best, const bool one_sided, const bool any = false) const
			{
				int found = -1;
				if (triangle_nodes.empty())
				{
					return found;
				}
				const vec3 inv{ 1.0 / (std::fabs(d.x) > 1e-12 ? d.x : 1e-12), 1.0 / (std::fabs(d.y) > 1e-12 ? d.y : 1e-12), 1.0 / (std::fabs(d.z) > 1e-12 ? d.z : 1e-12) };
				int stack[64];
				int top = 0;
				stack[top++] = 0;
				while (top > 0)
				{
					const node& n = triangle_nodes[static_cast<std::size_t>(stack[--top])];
					if (!box_hit(n.box, o, inv, best))
					{
						continue;
					}
					if (n.child[0] >= 0)
					{
						stack[top++] = n.child[0];
						stack[top++] = n.child[1];
						continue;
					}
					for (int k = n.first; k < n.first + n.count; k++)
					{
						const triangle& t = triangles[static_cast<std::size_t>(k)];
						const vec3 p = cross(d, t.e2);
						const double det = dot(t.e1, p);
						// model triangles are stored clockwise seen from outside ( cross( e1, e2 ) points inwards ): det < 0 is a ray that meets the front
						if (std::fabs(det) < 1e-12 || (one_sided && det > 0))
						{
							continue;
						}
						const double inv_det = 1.0 / det;
						const vec3 s = o - t.a;
						const double u = dot(s, p) * inv_det;
						if (u < 0 || u > 1)
						{
							continue;
						}
						const vec3 q = cross(s, t.e1);
						const double v = dot(d, q) * inv_det;
						if (v < 0 || u + v > 1)
						{
							continue;
						}
						const double at = dot(t.e2, q) * inv_det;
						if (at > 1e-3 && at < best)
						{
							best = at;
							found = k;
							if (any)
							{
								return found;
							}
						}
					}
				}
				return found;
			}

			// what a ray sees first
			enum class seen { nothing, sky, face, model };
			struct sight
			{
				seen what = seen::nothing;
				double t = 0;
				int brush = 0, face = -1;		// seen::face: a brush face, or a patch triangle ( brush -1 - patch )
			};
			sight look(const vec3& o, const vec3& d, const bool one_sided = false) const
			{
				sight s;
				ray_hit hit;
				double best = 1e9;
				if (trace(o, d, 1e9, hit))
				{
					best = hit.t;
					s.t = hit.t;
					s.brush = hit.brush;
					s.face = hit.face;
					const bool is_sky = hit.face >= 0 && sky[static_cast<std::size_t>(hit.brush)]
						&& w.brushes[static_cast<std::size_t>(hit.brush)].faces[static_cast<std::size_t>(hit.face)].material.rfind("sky", 0) == 0;
					s.what = is_sky ? seen::sky : hit.face >= 0 ? seen::face : seen::model;
				}
				else
				{
					s.what = seen::sky;			// out of the map
				}
				if (const int k = nearest_triangle(o, d, best, one_sided); k >= 0)
				{
					const triangle& t = triangles[static_cast<std::size_t>(k)];
					s.t = best;
					s.brush = t.brush;
					s.face = t.face;
					s.what = t.face >= 0 ? seen::face : seen::model;
				}
				return s;
			}

			explicit ray_world(const world& world_) : w(world_)
			{
				for (const map_model& model : w.models)
				{
					if (!model.ref.valid())
					{
						continue;
					}
					for (std::size_t k = 0; k + 2 < model.triangles.size(); k += 3)
					{
						triangle t;
						t.a = model.triangles[k];
						t.e1 = model.triangles[k + 1] - t.a;
						t.e2 = model.triangles[k + 2] - t.a;
						t.box.add(model.triangles[k]);
						t.box.add(model.triangles[k + 1]);
						t.box.add(model.triangles[k + 2]);
						triangles.push_back(t);
					}
				}
				// patches: their tessellation, stored like a model's triangles ( clockwise seen from the visible side )
				for (const map_patch& patch : w.patches)
				{
					if (is_tool_material(patch.material))
					{
						continue;
					}
					for (std::size_t k = 0; k + 2 < patch.indices.size(); k += 3)
					{
						vec3 p[3];
						for (int c = 0; c < 3; c++)
						{
							const edit::mesh_vertex& v = patch.vertices[patch.indices[k + static_cast<std::size_t>(c)]];
							p[c] = vec3{ v.position[0], v.position[1], v.position[2] };
						}
						triangle t;
						t.brush = -1 - static_cast<int>(&patch - w.patches.data());
						t.face = static_cast<int>(k / 3);
						t.a = p[0];
						t.e1 = p[2] - p[0];
						t.e2 = p[1] - p[0];
						t.box.add(p[0]);
						t.box.add(p[1]);
						t.box.add(p[2]);
						triangles.push_back(t);
					}
				}
				if (!triangles.empty())
				{
					build_triangles(0, static_cast<int>(triangles.size()));
				}
				sky.assign(w.brushes.size(), 0);
				for (std::size_t b = 0; b < w.brushes.size(); b++)
				{
					const map_brush& brush = w.brushes[b];
					// a brush one can see through ( a fence, a grate ) lets the light through: the bake does not read the
					// material's picture, and a solid block of shadow under a chain link fence is the worse mistake
					if (brush.model != 0 || !(brush.contents & 1) || brush.see_through)
					{
						continue;
					}
					brushes.push_back(static_cast<int>(b));
					for (const map_face& f : brush.faces)
					{
						sky[b] = sky[b] || f.material.rfind("sky", 0) == 0;
					}
				}
				if (!brushes.empty())
				{
					build(0, static_cast<int>(brushes.size()));
				}
			}

			int build(const int first, const int count)
			{
				const int index = static_cast<int>(nodes.size());
				nodes.push_back({});
				box3 box;
				for (int k = first; k < first + count; k++)
				{
					box.add(w.brushes[static_cast<std::size_t>(brushes[static_cast<std::size_t>(k)])].box);
				}
				nodes[static_cast<std::size_t>(index)].box = box;
				if (count <= 4)
				{
					nodes[static_cast<std::size_t>(index)].first = first;
					nodes[static_cast<std::size_t>(index)].count = count;
					return index;
				}
				int axis = 0;
				for (int k = 1; k < 3; k++)
				{
					if (box.maxs[k] - box.mins[k] > box.maxs[axis] - box.mins[axis])
					{
						axis = k;
					}
				}
				std::sort(brushes.begin() + first, brushes.begin() + first + count, [&](const int a, const int b)
				{
					const box3& x = w.brushes[static_cast<std::size_t>(a)].box;
					const box3& y = w.brushes[static_cast<std::size_t>(b)].box;
					return x.mins[axis] + x.maxs[axis] < y.mins[axis] + y.maxs[axis];
				});
				const int half = count / 2;
				const int l = build(first, half);
				nodes[static_cast<std::size_t>(index)].child[0] = l;
				const int r = build(first + half, count - half);
				nodes[static_cast<std::size_t>(index)].child[1] = r;
				return index;
			}

			static bool box_hit(const box3& box, const vec3& o, const vec3& inv, const double max_t)
			{
				double t0 = 0, t1 = max_t;
				for (int k = 0; k < 3; k++)
				{
					double a = (box.mins[k] - o[k]) * inv[k], b = (box.maxs[k] - o[k]) * inv[k];
					if (a > b)
					{
						std::swap(a, b);
					}
					t0 = std::max(t0, a);
					t1 = std::min(t1, b);
					if (t0 > t1)
					{
						return false;
					}
				}
				return true;
			}

			// first brush the ray enters ( or starts in ) within max_t
			bool trace(const vec3& o, const vec3& d, const double max_t, ray_hit& hit) const
			{
				if (nodes.empty())
				{
					return false;
				}
				const vec3 inv{ 1.0 / (std::fabs(d.x) > 1e-12 ? d.x : 1e-12), 1.0 / (std::fabs(d.y) > 1e-12 ? d.y : 1e-12), 1.0 / (std::fabs(d.z) > 1e-12 ? d.z : 1e-12) };
				double best = max_t;
				bool found = false;
				int stack[64];
				int top = 0;
				stack[top++] = 0;
				while (top > 0)
				{
					const node& n = nodes[static_cast<std::size_t>(stack[--top])];
					if (!box_hit(n.box, o, inv, best))
					{
						continue;
					}
					if (n.child[0] >= 0)
					{
						stack[top++] = n.child[0];
						stack[top++] = n.child[1];
						continue;
					}
					for (int k = n.first; k < n.first + n.count; k++)
					{
						const int index = brushes[static_cast<std::size_t>(k)];
						const map_brush& brush = w.brushes[static_cast<std::size_t>(index)];
						double enter = -1e30, leave = best;
						int face = -1;
						bool miss = false;
						for (std::size_t f = 0; f < brush.faces.size() && !miss; f++)
						{
							const edit::plane& p = brush.faces[f].plane;
							const double denom = dot(p.normal, d), dist = p.distance_to(o);
							if (std::fabs(denom) < 1e-12)
							{
								miss = dist > 0;
								continue;
							}
							const double t = -dist / denom;
							if (denom < 0)
							{
								if (t > enter)
								{
									enter = t;
									face = static_cast<int>(f);
								}
							}
							else
							{
								leave = std::min(leave, t);
							}
							miss = enter > leave;
						}
						if (miss || leave < 0 || enter >= best)
						{
							continue;
						}
						best = std::max(enter, 0.0);
						hit.t = best;
						hit.brush = index;
						hit.face = face;
						found = true;
					}
				}
				return found;
			}

			// true when the ray runs into the sky ( or leaves the map ) before anything else
			bool reaches_sky(const vec3& o, const vec3& d, const bool one_sided = false) const
			{
				ray_hit hit;
				if (!trace(o, d, 1e9, hit))
				{
					return !hits_triangle(o, d, 1e9, one_sided);
				}
				if (hit.face < 0 || !sky[static_cast<std::size_t>(hit.brush)])
				{
					return false;
				}
				return w.brushes[static_cast<std::size_t>(hit.brush)].faces[static_cast<std::size_t>(hit.face)].material.rfind("sky", 0) == 0
					&& !hits_triangle(o, d, hit.t, one_sided);
			}

			bool blocked(const vec3& from, const vec3& to, const bool one_sided = false) const
			{
				const vec3 delta = to - from;
				const double distance = length(delta);
				ray_hit hit;
				return distance > 1e-6 && (trace(from, delta * (1.0 / distance), distance - 0.5, hit) || hits_triangle(from, delta * (1.0 / distance), distance - 0.5, one_sided));
			}

			bool in_solid(const vec3& p) const
			{
				ray_hit hit;
				return trace(p, vec3{ 0, 0, 1 }, 0.01, hit);
			}
		};

		struct light_setup
		{
			const world& w;
			const ray_world& rays;
			vec3 sun;
			std::vector<vec3> sky_directions;	// over the sphere
			float sky_color[3] = {}, ambient[3] = {};
		};

		// light that is not the sun, arriving at a point; with a normal: on a surface, without: at a point in space
		void gather(const light_setup& s, const vec3& p, const vec3* normal, float out[3])
		{
			double sky = 0, weight = 0;
			for (const vec3& d : s.sky_directions)
			{
				const double c = normal ? dot(d, *normal) : 0.5;
				if (c <= 0)
				{
					continue;
				}
				weight += c;
				if (s.rays.reaches_sky(p, d, !normal))
				{
					sky += c;
				}
			}
			const double sky_part = weight > 0 ? sky / weight : 0;
			for (int k = 0; k < 3; k++)
			{
				out[k] = s.ambient[k] + s.sky_color[k] * static_cast<float>(sky_part);
			}
			for (const map_light& light : s.w.bake_lights)
			{
				const vec3 origin{ light.origin[0], light.origin[1], light.origin[2] };
				const vec3 to = origin - p;
				const double distance = length(to);
				if (distance >= light.radius || distance < 1e-3)
				{
					continue;
				}
				const vec3 l = to * (1.0 / distance);
				double scale = (1.0 - distance / light.radius) * (normal ? dot(l, *normal) : 0.5);
				if (scale <= 0)
				{
					continue;
				}
				if (light.type == 2)
				{
					// a spot: `dir` points back along the beam
					const double c = dot(l, vec3{ light.dir[0], light.dir[1], light.dir[2] });
					if (c <= light.cos_half_fov_outer)
					{
						continue;
					}
					if (c < light.cos_half_fov_inner)
					{
						scale *= (c - light.cos_half_fov_outer) / std::max(1e-6, static_cast<double>(light.cos_half_fov_inner - light.cos_half_fov_outer));
					}
				}
				if (s.rays.blocked(p, origin, !normal))
				{
					continue;
				}
				for (int k = 0; k < 3; k++)
				{
					out[k] += light.color[k] * static_cast<float>(scale);
				}
			}
		}

		template <typename F>
		void parallel_for(const std::size_t count, F&& work)
		{
			const unsigned threads = std::max(1u, std::min(std::thread::hardware_concurrency(), 32u));
			std::atomic<std::size_t> next{ 0 };
			std::vector<std::thread> pool;
			for (unsigned t = 0; t < threads; t++)
			{
				pool.emplace_back([&]
				{
					for (std::size_t k = next++; k < count; k = next++)
					{
						work(k);
					}
				});
			}
			for (std::thread& t : pool)
			{
				t.join();
			}
		}

		// one face of a surface in the lightmap atlas
		struct chart
		{
			int surface = 0;
			int brush = 0, face = 0;
			vec3 origin, u_axis, v_axis, normal;
			double u_min = 0, v_min = 0, u_max = 0, v_max = 0;
			int w = 0, h = 0, x = 0, y = 0;		// in luxels of a secondary layer; one luxel of padding all around
			int page = 0;
			std::size_t first = 0;				// its first luxel in the arrays of the bake
		};

		// Shelf packing into pages of LM_W x LM_H luxels. All charts of a surface go into one page ( a surface names one
		// lightmap ); surfaces are placed tallest chart first, each into the first page that takes it. false: a surface
		// does not fit an empty page, or more than `max_pages` are needed.
		bool pack(std::vector<chart>& charts, const std::vector<std::vector<std::size_t>>& of_surface, const int max_pages, int& page_count)
		{
			struct shelf
			{
				int x = 0, y = 0, height = 0;
			};
			std::vector<std::size_t> order;
			std::vector<int> tallest(of_surface.size(), 0);
			for (std::size_t s = 0; s < of_surface.size(); s++)
			{
				if (!of_surface[s].empty())
				{
					order.push_back(s);
				}
				for (const std::size_t c : of_surface[s])
				{
					tallest[s] = std::max(tallest[s], charts[c].h);
				}
			}
			std::sort(order.begin(), order.end(), [&](const std::size_t a, const std::size_t b) { return tallest[a] != tallest[b] ? tallest[a] > tallest[b] : a < b; });
			std::vector<shelf> pages;
			for (const std::size_t s : order)
			{
				std::vector<std::size_t> list = of_surface[s];
				std::sort(list.begin(), list.end(), [&](const std::size_t a, const std::size_t b) { return charts[a].h != charts[b].h ? charts[a].h > charts[b].h : a < b; });
				bool placed = false;
				for (std::size_t p = 0; p <= pages.size() && !placed; p++)
				{
					if (p == pages.size())
					{
						if (static_cast<int>(pages.size()) >= max_pages)
						{
							return false;
						}
						pages.push_back({});
					}
					shelf state = pages[p];
					bool fits = true;
					std::vector<std::pair<int, int>> at;
					for (const std::size_t k : list)
					{
						const chart& c = charts[k];
						if (c.w > LM_W || c.h > LM_H)
						{
							return false;
						}
						if (state.x + c.w > LM_W)
						{
							state.x = 0;
							state.y += state.height;
							state.height = 0;
						}
						if (state.y + c.h > LM_H)
						{
							fits = false;
							break;
						}
						at.emplace_back(state.x, state.y);
						state.x += c.w;
						state.height = std::max(state.height, c.h);
					}
					if (!fits)
					{
						if (pages[p].x == 0 && pages[p].y == 0 && pages[p].height == 0)
						{
							return false;		// not even an empty page takes the surface
						}
						continue;
					}
					for (std::size_t k = 0; k < list.size(); k++)
					{
						charts[list[k]].x = at[k].first;
						charts[list[k]].y = at[k].second;
						charts[list[k]].page = static_cast<int>(p);
					}
					pages[p] = state;
					placed = true;
				}
			}
			page_count = static_cast<int>(pages.size());
			return true;
		}

		// the 56 directions of a light grid colour set: the texels on the surface of a 4 x 4 x 4 block, which the
		// model lighting volume is looked up in with the surface normal ( codol_view light_grid.cpp BLOCK_COLOR,
		// from CODOL-AssetExplorer's LightGrid.cs ): texel x + 4 y + 16 z looks along ( x, y, z ) - 1.5
		std::array<vec3, 56> grid_directions()
		{
			static constexpr std::uint8_t BLOCK_COLOR[64] =
			{
				0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
				16, 17, 18, 19, 20, 0, 3, 21, 22, 12, 15, 23, 24, 25, 26, 27,
				28, 29, 30, 31, 32, 40, 43, 33, 34, 52, 55, 35, 36, 37, 38, 39,
				40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55,
			};
			std::array<vec3, 56> out{};
			for (int t = 0; t < 64; t++)
			{
				const int x = t & 3, y = (t >> 2) & 3, z = t >> 4;
				if ((x == 1 || x == 2) && (y == 1 || y == 2) && (z == 1 || z == 2))
				{
					continue;		// inside the block: shows a corner's colour
				}
				const vec3 d{ x - 1.5, y - 1.5, z - 1.5 };
				out[BLOCK_COLOR[t]] = d * (1.0 / length(d));
			}
			return out;
		}

		void flat_lighting(compile_context& ctx, render_world& rw)
		{
			// --no-light: every surface reads one texel that holds the ambient light, in full sun
			const world& w = ctx.w;
			render_lightmap lightmap;
			lightmap.primary.assign(static_cast<std::size_t>(LM_W) * 2 * LM_H * 2, 255);
			lightmap.secondary.assign(static_cast<std::size_t>(LM_W) * LM_H * 2 * 4, 0);
			for (std::size_t k = 0; k < lightmap.secondary.size(); k += 4)
			{
				const bool upper = k < lightmap.secondary.size() / 2;
				lightmap.secondary[k] = upper ? to_byte(std::min(w.ambient[2], 1.0f)) : 0;
				lightmap.secondary[k + 1] = upper ? to_byte(std::min(w.ambient[1], 1.0f)) : 0;
				lightmap.secondary[k + 2] = upper ? to_byte(std::min(w.ambient[0], 1.0f)) : 0;
				lightmap.secondary[k + 3] = 255;
			}
			if (const char* test = std::getenv("CODOL_BSP_LM_TEST"))
			{
				// development aid ( lm_experiment.ps1 ): "r0 g0 b0 a0 r1 g1 b1 a1 primary" fills the two layers and the primary image
				int v[9] = {};
				std::sscanf(test, "%d %d %d %d %d %d %d %d %d", &v[0], &v[1], &v[2], &v[3], &v[4], &v[5], &v[6], &v[7], &v[8]);
				const std::size_t half = lightmap.secondary.size() / 2;
				for (std::size_t k = 0; k < lightmap.secondary.size(); k += 4)
				{
					const int* c = v + (k < half ? 0 : 4);
					lightmap.secondary[k] = static_cast<std::uint8_t>(c[2]);
					lightmap.secondary[k + 1] = static_cast<std::uint8_t>(c[1]);
					lightmap.secondary[k + 2] = static_cast<std::uint8_t>(c[0]);
					lightmap.secondary[k + 3] = static_cast<std::uint8_t>(c[3]);
				}
				std::fill(lightmap.primary.begin(), lightmap.primary.end(), static_cast<std::uint8_t>(v[8]));
			}
			rw.lightmaps.push_back(std::move(lightmap));
		}
	}

	void bake_lighting(compile_context& ctx, render_world& rw)
	{
		const world& w = ctx.w;
		const ray_world rays(w);
		light_setup setup{ w, rays };
		setup.sun = vec3{ w.sun_dir[0], w.sun_dir[1], w.sun_dir[2] };
		for (int k = 0; k < 3; k++)
		{
			setup.sky_color[k] = w.sky_light[k];
			setup.ambient[k] = w.ambient_light[k];
		}
		// directions spread evenly over the sphere ( a Fibonacci spiral )
		const int direction_count = ctx.opts.quality <= 0 ? 24 : ctx.opts.quality == 1 ? 64 : 64 * ctx.opts.quality;
		for (int k = 0; k < direction_count; k++)
		{
			const double z = 1.0 - (2.0 * k + 1.0) / direction_count, r = std::sqrt(std::max(0.0, 1.0 - z * z)), a = k * PI * (3.0 - std::sqrt(5.0));
			setup.sky_directions.push_back({ r * std::cos(a), r * std::sin(a), z });
		}

		// What a surface throws back: half of what arrives ( the images of the materials are not read; a grey world ).
		// The sun's light in lightmap units: full sun on a surface is about 1.2 times a lightmap value of 1
		// ( lm_experiment ), so the sun colour counts 1 / 1.2.
		constexpr float REFLECT = 0.5f, SUN_SCALE = 1.0f / 1.2f;
		const int bounce_rays = ctx.opts.quality <= 0 ? 12 : ctx.opts.quality == 1 ? 24 : 48;
		const int bounces = !ctx.opts.light ? 0 : ctx.opts.quality <= 1 ? 1 : 2;

		// per luxel of every chart: the light that arrives, and what the luxel throws back ( the source of the next bounce,
		// of the light grid and of the reflection probes )
		std::vector<chart> charts;
		std::vector<float> direct, indirect, sun_part, emit;
		std::unordered_map<std::uint64_t, std::size_t> chart_of_face;		// ( brush, face ) -> a chart of it
		const auto face_key = [](const int brush, const int face) { return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(brush)) << 32) | static_cast<std::uint32_t>(face); };
		double luxel = ctx.opts.quality <= 0 ? 32.0 : ctx.opts.quality == 1 ? 16.0 : 8.0;
		// the luxel a point of a face lies in; npos: the face has no chart
		const auto luxel_at = [&](const int brush, const int face, const vec3& p) -> std::size_t
		{
			const auto it = chart_of_face.find(face_key(brush, face));
			if (it == chart_of_face.end())
			{
				return npos;
			}
			const chart& c = charts[it->second];
			const int lx = std::clamp(static_cast<int>(std::floor(1.0 + (dot(p - c.origin, c.u_axis) - c.u_min) / luxel)), 0, c.w - 1);
			const int ly = std::clamp(static_cast<int>(std::floor(1.0 + (dot(p - c.origin, c.v_axis) - c.v_min) / luxel)), 0, c.h - 1);
			return c.first + static_cast<std::size_t>(ly) * static_cast<std::size_t>(c.w) + static_cast<std::size_t>(lx);
		};

		// ---- lightmaps
		if (!ctx.opts.light)
		{
			flat_lighting(ctx, rw);
		}
		else
		{
			// one chart per face of a surface ( a face drawn by several cells has one per surface: the charts of a
			// surface share a page )
			std::vector<std::vector<std::size_t>> of_surface(rw.surfaces.size());
			for (std::size_t si = 0; si < rw.surfaces.size(); si++)
			{
				const render_surface& surface = rw.surfaces[si];
				if (surface.lightmap == 31)
				{
					continue;
				}
				for (const render_face& rf : surface.faces)
				{
					// the face as the surface has it: a brush face, or a triangle of a patch
					std::vector<vec3> polygon;
					for (std::uint32_t k = 0; k < rf.vertex_count; k++)
					{
						const c1::GfxWorldVertex& v = surface.vertices[rf.first_vertex + k];
						polygon.push_back({ v.xyz[0], v.xyz[1], v.xyz[2] });
					}
					chart c;
					c.surface = static_cast<int>(si);
					c.brush = rf.brush;
					c.face = rf.face;
					c.normal = cross(polygon[1] - polygon[0], polygon[2] - polygon[0]);
					if (length(c.normal) < 1e-6)
					{
						continue;
					}
					c.normal = c.normal * (1.0 / length(c.normal));
					if (dot(c.normal, vec3{ rf.normal[0], rf.normal[1], rf.normal[2] }) < 0)
					{
						c.normal = c.normal * -1.0;
					}
					vec3 v_axis;
					edit::face_texture_axes(c.normal, 0.0, c.u_axis, v_axis);
					c.u_axis = c.u_axis - c.normal * dot(c.u_axis, c.normal);
					c.u_axis = c.u_axis * (1.0 / std::max(length(c.u_axis), 1e-9));
					c.v_axis = cross(c.normal, c.u_axis);
					c.origin = c.normal * dot(c.normal, polygon[0]);
					c.u_min = c.v_min = 1e30;
					c.u_max = c.v_max = -1e30;
					for (const vec3& p : polygon)
					{
						const double u = dot(p - c.origin, c.u_axis), v = dot(p - c.origin, c.v_axis);
						c.u_min = std::min(c.u_min, u);
						c.u_max = std::max(c.u_max, u);
						c.v_min = std::min(c.v_min, v);
						c.v_max = std::max(c.v_max, v);
					}
					chart_of_face.emplace(face_key(rf.brush, rf.face), charts.size());
					of_surface[si].push_back(charts.size());
					charts.push_back(c);
				}
			}
			// the luxel size: as asked for, larger when the map does not fit the pages a surface can name ( 31 )
			int page_count = 1;
			for (;; luxel *= 1.25)
			{
				for (chart& c : charts)
				{
					c.w = static_cast<int>(std::ceil((c.u_max - c.u_min) / luxel)) + 2;
					c.h = static_cast<int>(std::ceil((c.v_max - c.v_min) / luxel)) + 2;
				}
				if (pack(charts, of_surface, 30, page_count) || luxel > 4096)
				{
					break;
				}
			}
			std::size_t luxel_count = 0;
			for (chart& c : charts)
			{
				c.first = luxel_count;
				luxel_count += static_cast<std::size_t>(c.w) * static_cast<std::size_t>(c.h);
			}
			ctx.rep.notes.push_back(format("light: %zu faces in %d lightmap page( s ), %zu luxels of %.1f units; %zu light entities, %d sky directions, %d bounce( s ) of %d rays",
				charts.size(), page_count, luxel_count, luxel, w.bake_lights.size(), direction_count, bounces, bounce_rays));
			// self check: no two charts share a luxel
			{
				std::vector<std::uint8_t> used(static_cast<std::size_t>(page_count) * LM_W * LM_H, 0);
				bool overlap = false;
				for (const chart& c : charts)
				{
					for (int y = c.y; y < c.y + c.h && !overlap; y++)
					{
						for (int x = c.x; x < c.x + c.w; x++)
						{
							std::uint8_t& u = used[(static_cast<std::size_t>(c.page) * LM_H + static_cast<std::size_t>(y)) * LM_W + static_cast<std::size_t>(x)];
							overlap = overlap || u;
							u = 1;
						}
					}
				}
				if (overlap)
				{
					throw compile_error("light: internal error, lightmap charts overlap");
				}
				ctx.rep.notes.push_back(format("light: the %zu charts do not overlap ( checked )", charts.size()));
			}

			rw.lightmaps.assign(static_cast<std::size_t>(page_count), {});
			for (render_lightmap& lightmap : rw.lightmaps)
			{
				lightmap.primary.assign(static_cast<std::size_t>(LM_W) * 2 * LM_H * 2, 0);
				lightmap.secondary.assign(static_cast<std::size_t>(LM_W) * LM_H * 2 * 4, 0);
				for (std::size_t k = 3; k < lightmap.secondary.size(); k += 4)
				{
					lightmap.secondary[k] = 255;
				}
			}
			direct.assign(luxel_count * 3, 0.0f);
			indirect.assign(luxel_count * 3, 0.0f);
			sun_part.assign(luxel_count, 0.0f);
			const auto position = [&](const chart& c, const double lx, const double ly)
			{
				// lx, ly: luxels from the chart's corner; the padding repeats the edge of the face
				const double u = std::clamp(c.u_min + (lx - 1.0) * luxel, c.u_min, c.u_max), v = std::clamp(c.v_min + (ly - 1.0) * luxel, c.v_min, c.v_max);
				return c.origin + c.u_axis * u + c.v_axis * v + c.normal * 1.0;
			};
			// direct light: sky, ambient and the light entities per luxel; the sun's visibility per primary texel
			std::vector<std::uint8_t> sun_lit(charts.size(), 0);
			parallel_for(charts.size(), [&](const std::size_t index)
			{
				const chart& c = charts[index];
				std::vector<std::uint8_t>& primary = rw.lightmaps[static_cast<std::size_t>(c.page)].primary;
				const double facing = dot(c.normal, setup.sun);
				for (int y = 0; y < c.h; y++)
				{
					for (int x = 0; x < c.w; x++)
					{
						const std::size_t at = c.first + static_cast<std::size_t>(y) * static_cast<std::size_t>(c.w) + static_cast<std::size_t>(x);
						gather(setup, position(c, x + 0.5, y + 0.5), &c.normal, &direct[at * 3]);
						int lit_texels = 0;
						for (int sy = 0; sy < 2; sy++)
						{
							for (int sx = 0; sx < 2; sx++)
							{
								const bool lit = facing > 0.01 && rays.reaches_sky(position(c, x + 0.25 + 0.5 * sx, y + 0.25 + 0.5 * sy), setup.sun);
								primary[(static_cast<std::size_t>(c.y + y) * 2 + static_cast<std::size_t>(sy)) * (LM_W * 2) + static_cast<std::size_t>(c.x + x) * 2
									+ static_cast<std::size_t>(sx)] = lit ? 255 : 0;
								lit_texels += lit;
							}
						}
						sun_lit[index] = sun_lit[index] || lit_texels;
						sun_part[at] = static_cast<float>(lit_texels * 0.25 * std::max(0.0, facing));
					}
				}
			});
			// bounced light: every luxel looks around ( cosine weighted directions over its side ) and takes what the
			// luxels it sees throw back
			emit.assign(luxel_count * 3, 0.0f);
			for (std::size_t k = 0; k < luxel_count; k++)
			{
				for (int ch = 0; ch < 3; ch++)
				{
					emit[k * 3 + static_cast<std::size_t>(ch)] = REFLECT * (direct[k * 3 + static_cast<std::size_t>(ch)] + w.sun_color[ch] * SUN_SCALE * sun_part[k]);
				}
			}
			std::vector<vec3> hemisphere;
			for (int k = 0; k < bounce_rays; k++)
			{
				const double r = std::sqrt((k + 0.5) / bounce_rays), a = k * PI * (3.0 - std::sqrt(5.0));
				hemisphere.push_back({ r * std::cos(a), r * std::sin(a), std::sqrt(std::max(0.0, 1.0 - r * r)) });
			}
			for (int bounce = 0; bounce < bounces; bounce++)
			{
				std::vector<float> gathered(luxel_count * 3, 0.0f);
				parallel_for(charts.size(), [&](const std::size_t index)
				{
					const chart& c = charts[index];
					for (int y = 0; y < c.h; y++)
					{
						for (int x = 0; x < c.w; x++)
						{
							const std::size_t at = c.first + static_cast<std::size_t>(y) * static_cast<std::size_t>(c.w) + static_cast<std::size_t>(x);
							const vec3 p = position(c, x + 0.5, y + 0.5);
							// the same directions for every luxel, turned by an angle of its own
							const double turn = static_cast<double>((at * 2654435761u) & 0xFFFF) * (2.0 * PI / 65536.0), ct = std::cos(turn), st = std::sin(turn);
							double sum[3] = { 0, 0, 0 };
							for (const vec3& h : hemisphere)
							{
								const double hx = h.x * ct - h.y * st, hy = h.x * st + h.y * ct;
								const vec3 d = c.u_axis * hx + c.v_axis * hy + c.normal * h.z;
								const ray_world::sight s = rays.look(p, d);
								if (s.what != ray_world::seen::face)
								{
									continue;
								}
								const std::size_t from = luxel_at(s.brush, s.face, p + d * s.t);
								if (from != npos)
								{
									sum[0] += emit[from * 3];
									sum[1] += emit[from * 3 + 1];
									sum[2] += emit[from * 3 + 2];
								}
							}
							for (int ch = 0; ch < 3; ch++)
							{
								gathered[at * 3 + static_cast<std::size_t>(ch)] = static_cast<float>(sum[ch] / bounce_rays);
							}
						}
					}
				});
				for (std::size_t k = 0; k < gathered.size(); k++)
				{
					indirect[k] += gathered[k];
					emit[k] = REFLECT * gathered[k];
				}
			}
			// what every luxel throws back in the end
			for (std::size_t k = 0; k < luxel_count; k++)
			{
				for (int ch = 0; ch < 3; ch++)
				{
					const std::size_t i = k * 3 + static_cast<std::size_t>(ch);
					emit[i] = REFLECT * (direct[i] + indirect[i] + w.sun_color[ch] * SUN_SCALE * sun_part[k]);
				}
			}
			for (const chart& c : charts)
			{
				std::vector<std::uint8_t>& secondary = rw.lightmaps[static_cast<std::size_t>(c.page)].secondary;
				for (int y = 0; y < c.h; y++)
				{
					for (int x = 0; x < c.w; x++)
					{
						const std::size_t at = (c.first + static_cast<std::size_t>(y) * static_cast<std::size_t>(c.w) + static_cast<std::size_t>(x)) * 3;
						std::uint8_t* texel = secondary.data() + (static_cast<std::size_t>(c.y + y) * LM_W + static_cast<std::size_t>(c.x + x)) * 4;
						texel[0] = to_byte(direct[at + 2] + indirect[at + 2]);
						texel[1] = to_byte(direct[at + 1] + indirect[at + 1]);
						texel[2] = to_byte(direct[at] + indirect[at]);
					}
				}
			}
			// soften the sun's edge: a 3 x 3 box filter inside every chart
			{
				std::vector<std::vector<std::uint8_t>> sharp;
				for (const render_lightmap& lightmap : rw.lightmaps)
				{
					sharp.push_back(lightmap.primary);
				}
				const int pw = LM_W * 2;
				for (const chart& c : charts)
				{
					const std::vector<std::uint8_t>& from = sharp[static_cast<std::size_t>(c.page)];
					std::vector<std::uint8_t>& primary = rw.lightmaps[static_cast<std::size_t>(c.page)].primary;
					for (int y = c.y * 2; y < (c.y + c.h) * 2; y++)
					{
						for (int x = c.x * 2; x < (c.x + c.w) * 2; x++)
						{
							int sum = 0, n = 0;
							for (int dy = -1; dy <= 1; dy++)
							{
								for (int dx = -1; dx <= 1; dx++)
								{
									const int xx = x + dx, yy = y + dy;
									if (xx >= c.x * 2 && xx < (c.x + c.w) * 2 && yy >= c.y * 2 && yy < (c.y + c.h) * 2)
									{
										sum += from[static_cast<std::size_t>(yy) * pw + static_cast<std::size_t>(xx)];
										n++;
									}
								}
							}
							primary[static_cast<std::size_t>(y) * pw + static_cast<std::size_t>(x)] = static_cast<std::uint8_t>(sum / n);
						}
					}
				}
			}
			ctx.rep.notes.push_back(format("light: the sun reaches %zu of %zu faces", static_cast<std::size_t>(std::count(sun_lit.begin(), sun_lit.end(), std::uint8_t{ 1 })),
				charts.size()));

			// lightmap page and coordinates of the surfaces
			for (std::size_t si = 0; si < rw.surfaces.size(); si++)
			{
				render_surface& surface = rw.surfaces[si];
				if (surface.lightmap == 31)
				{
					continue;
				}
				if (of_surface[si].empty())
				{
					surface.lightmap = 31;
					continue;
				}
				bool any_sun = false;
				std::size_t next = 0;
				surface.lightmap = charts[of_surface[si][0]].page;
				for (const render_face& rf : surface.faces)
				{
					// the charts of a surface are in the order of its faces ( a face without area has none )
					if (next >= of_surface[si].size() || charts[of_surface[si][next]].brush != rf.brush || charts[of_surface[si][next]].face != rf.face)
					{
						continue;
					}
					const std::size_t ci = of_surface[si][next++];
					const chart& c = charts[ci];
					any_sun = any_sun || sun_lit[ci];
					for (std::uint32_t k = 0; k < rf.vertex_count; k++)
					{
						c1::GfxWorldVertex& v = surface.vertices[rf.first_vertex + k];
						const vec3 p{ v.xyz[0], v.xyz[1], v.xyz[2] };
						const double lx = 1.0 + (dot(p - c.origin, c.u_axis) - c.u_min) / luxel, ly = 1.0 + (dot(p - c.origin, c.v_axis) - c.v_min) / luxel;
						v.lmapCoord[0] = static_cast<float>((c.x + lx) / LM_W);
						v.lmapCoord[1] = static_cast<float>((c.y + ly) / LM_H);
					}
				}
				surface.primary_light = any_sun ? 1 : 0;
			}
		}

		// what a ray from a point in space brings back: the sky's colour, or what the luxel it ends on throws back
		const auto incoming = [&](const vec3& p, const vec3& d, float out[3])
		{
			const ray_world::sight s = rays.look(p, d, true);
			out[0] = out[1] = out[2] = 0.0f;
			if (s.what == ray_world::seen::sky)
			{
				std::memcpy(out, setup.sky_color, sizeof(setup.sky_color));
			}
			else if (s.what == ray_world::seen::face && !emit.empty())
			{
				if (const std::size_t from = luxel_at(s.brush, s.face, p + d * s.t); from != npos)
				{
					std::memcpy(out, &emit[from * 3], 3 * sizeof(float));
				}
			}
			return s.what;
		};

		// ---- light grid: a sample for every point of the grid that is not inside a brush. A sample is the light
		// arriving from all around as an average and a direction per colour ( E( n ) = m0 + n . m1 ), from which the
		// 56 colours of the entry follow: a model is lit from the side the light comes from.
		{
			render_grid& grid = rw.grid;
			GfxLightGrid& h = grid.header;
			h.hasLightRegions = 0;
			h.lastSunPrimaryLightIndex = 1;
			// The grid covers the map; a map whose brushes reach far beyond where anything happens ( sky boxes, clip
			// walls ) gets a grid around its entities, and at most GRID_SAMPLES points are sampled.
			constexpr double GRID_SAMPLES = 3.0e6;
			box3 region = rw.bounds;
			const auto samples_of = [](const box3& b)
			{
				return (static_cast<double>(b.maxs[0] - b.mins[0]) / 32 + 1) * (static_cast<double>(b.maxs[1] - b.mins[1]) / 32 + 1)
					* (static_cast<double>(b.maxs[2] - b.mins[2]) / 64 + 1);
			};
			if (samples_of(region) > GRID_SAMPLES)
			{
				// where the game takes place: spawn points and path nodes, else every point entity
				box3 play;
				for (int pass = 0; pass < 2 && !play.valid(); pass++)
				{
					for (const map_entity& e : w.entities)
					{
						if (!e.world && e.brushes.empty() && !e.value("origin").empty()
							&& (pass == 1 || e.classname.rfind("mp_", 0) == 0 || e.classname.rfind("node_", 0) == 0))
						{
							play.add(e.origin);
						}
					}
				}
				if (play.valid())
				{
					play.expand(512.0f);
					for (int k = 0; k < 3; k++)
					{
						region.mins[k] = std::max(region.mins[k], play.mins[k]);
						region.maxs[k] = std::min(region.maxs[k], play.maxs[k]);
					}
				}
				while (samples_of(region) > GRID_SAMPLES)
				{
					for (int k = 0; k < 3; k++)
					{
						const float trim = (region.maxs[k] - region.mins[k]) * 0.05f;
						region.mins[k] += trim;
						region.maxs[k] -= trim;
					}
				}
				ctx.rep.warnings.push_back(format("light: the light grid does not cover the whole map ( %.0f %.0f %.0f ) - ( %.0f %.0f %.0f ) but ( %.0f %.0f %.0f ) - ( %.0f %.0f %.0f )",
					rw.bounds.mins[0], rw.bounds.mins[1], rw.bounds.mins[2], rw.bounds.maxs[0], rw.bounds.maxs[1], rw.bounds.maxs[2], region.mins[0], region.mins[1],
					region.mins[2], region.maxs[0], region.maxs[1], region.maxs[2]));
			}
			int mins[3], maxs[3];
			for (int k = 0; k < 3; k++)
			{
				mins[k] = grid_coord(region.mins[k], k);
				maxs[k] = grid_coord(region.maxs[k], k);
			}
			maxs[2] = std::min(maxs[2], mins[2] + 250);
			h.rowAxis = 0;
			h.colAxis = 1;
			const int rows = maxs[0] - mins[0] + 1, cols = maxs[1] - mins[1] + 1, layers = maxs[2] - mins[2] + 1;
			struct sample
			{
				float m0[3] = {}, m1[3][3] = {};	// per colour: the average, and the direction it leans to
				std::uint8_t light = 0;
				bool solid = true;
			};
			std::vector<sample> samples(static_cast<std::size_t>(rows) * cols * layers);
			parallel_for(static_cast<std::size_t>(rows) * cols, [&](const std::size_t rc)
			{
				const int r = static_cast<int>(rc / static_cast<std::size_t>(cols)), col = static_cast<int>(rc % static_cast<std::size_t>(cols));
				for (int z = 0; z < layers; z++)
				{
					const vec3 p{ grid_position(mins[0] + r, 0), grid_position(mins[1] + col, 1), grid_position(mins[2] + z, 2) };
					sample& s = samples[rc * static_cast<std::size_t>(layers) + static_cast<std::size_t>(z)];
					if (rays.in_solid(p))
					{
						continue;
					}
					s.solid = false;
					if (!ctx.opts.light)
					{
						for (int k = 0; k < 3; k++)
						{
							s.m0[k] = std::min(w.ambient[k], 1.0f);
						}
						s.light = 1;
						continue;
					}
					for (int k = 0; k < 3; k++)
					{
						s.m0[k] = setup.ambient[k];
					}
					const double share = 1.0 / static_cast<double>(setup.sky_directions.size());
					for (const vec3& d : setup.sky_directions)
					{
						float c[3];
						incoming(p, d, c);
						for (int k = 0; k < 3; k++)
						{
							s.m0[k] += static_cast<float>(c[k] * share);
							for (int a = 0; a < 3; a++)
							{
								s.m1[k][a] += static_cast<float>(2.0 * c[k] * share * d[a]);
							}
						}
					}
					for (const map_light& light : w.bake_lights)
					{
						const vec3 origin{ light.origin[0], light.origin[1], light.origin[2] };
						const vec3 to = origin - p;
						const double distance = length(to);
						if (distance >= light.radius || distance < 1e-3)
						{
							continue;
						}
						const vec3 l = to * (1.0 / distance);
						double scale = 1.0 - distance / light.radius;
						if (light.type == 2)
						{
							const double c = dot(l, vec3{ light.dir[0], light.dir[1], light.dir[2] });
							if (c <= light.cos_half_fov_outer)
							{
								continue;
							}
							if (c < light.cos_half_fov_inner)
							{
								scale *= (c - light.cos_half_fov_outer) / std::max(1e-6, static_cast<double>(light.cos_half_fov_inner - light.cos_half_fov_outer));
							}
						}
						if (rays.blocked(p, origin, true))
						{
							continue;
						}
						// a light from one direction, as average and lean: c max( 0, n . l ) ~ c ( 1 / 4 + n . l / 2 )
						for (int k = 0; k < 3; k++)
						{
							s.m0[k] += static_cast<float>(light.color[k] * scale * 0.25);
							for (int a = 0; a < 3; a++)
							{
								s.m1[k][a] += static_cast<float>(light.color[k] * scale * 0.5 * l[a]);
							}
						}
					}
					s.light = rays.reaches_sky(p, setup.sun, true) ? 1 : 0;
				}
			});
			for (int k = 0; k < 3; k++)
			{
				h.mins[k] = static_cast<std::uint16_t>(mins[k]);
				h.maxs[k] = static_cast<std::uint16_t>(maxs[k]);
			}
			// The colour sets: an entry names one of at most 65535. Sets are compared after dropping low bits; when there
			// are too many, more bits are dropped.
			const std::array<vec3, 56> directions = grid_directions();
			std::vector<std::uint32_t> set_of(samples.size(), 0);
			for (int drop = 2; drop < 8; drop++)
			{
				grid.colors.clear();
				std::unordered_map<std::string, std::uint32_t> known;
				const std::uint8_t mask = static_cast<std::uint8_t>(0xFF << drop);
				bool full = false;
				for (std::size_t i = 0; i < samples.size() && !full; i++)
				{
					const sample& s = samples[i];
					if (s.solid)
					{
						continue;
					}
					GfxLightGridColors colors{};
					for (std::size_t d = 0; d < 56; d++)
					{
						for (int k = 0; k < 3; k++)
						{
							const double v = s.m0[k] + s.m1[k][0] * directions[d].x + s.m1[k][1] * directions[d].y + s.m1[k][2] * directions[d].z;
							colors.rgb[d][k] = static_cast<std::uint8_t>(to_byte(static_cast<float>(v)) & mask);
						}
					}
					const std::string key(reinterpret_cast<const char*>(&colors), sizeof(colors));
					const auto [it, added] = known.emplace(key, static_cast<std::uint32_t>(grid.colors.size()));
					if (added)
					{
						full = grid.colors.size() >= 0xFFFF;
						grid.colors.push_back(colors);
					}
					set_of[i] = it->second;
				}
				if (!full)
				{
					ctx.rep.notes.push_back(format("light: %zu light grid colour sets ( %d low bits dropped )", grid.colors.size(), drop));
					break;
				}
			}
			for (int r = 0; r < rows; r++)
			{
				// per column: the range of layers from the lowest to the highest sample in the open
				std::vector<std::pair<int, int>> range(static_cast<std::size_t>(cols), { 0, 0 });
				int z_low = layers, z_high = -1, c_first = cols, c_last = -1;
				for (int col = 0; col < cols; col++)
				{
					int low = layers, high = -1;
					for (int z = 0; z < layers; z++)
					{
						if (!samples[(static_cast<std::size_t>(r) * cols + col) * layers + z].solid)
						{
							low = std::min(low, z);
							high = z;
						}
					}
					if (high >= 0)
					{
						range[static_cast<std::size_t>(col)] = { low, high - low + 1 };
						z_low = std::min(z_low, low);
						z_high = std::max(z_high, high);
						c_first = std::min(c_first, col);
						c_last = col;
					}
				}
				if (c_last < 0 || grid.raw_rows.size() / 4 >= 0xFFFF)
				{
					grid.row_starts.push_back(0xFFFF);
					continue;
				}
				grid.row_starts.push_back(static_cast<std::uint16_t>(grid.raw_rows.size() / 4));
				GfxLightGridRow row{};
				row.colStart = static_cast<std::uint16_t>(mins[1] + c_first);
				row.colCount = static_cast<std::uint16_t>(c_last - c_first + 1);
				row.zStart = static_cast<std::uint16_t>(mins[2] + z_low);
				row.zCount = static_cast<std::uint16_t>(z_high - z_low + 1);
				row.firstEntry = static_cast<std::uint32_t>(grid.entries.size());
				const auto* bytes = reinterpret_cast<const std::uint8_t*>(&row);
				grid.raw_rows.insert(grid.raw_rows.end(), bytes, bytes + sizeof(row));
				for (int col = c_first; col <= c_last;)
				{
					// columns with the same range of layers are one run
					int run = 1;
					while (col + run <= c_last && run < 255 && range[static_cast<std::size_t>(col + run)] == range[static_cast<std::size_t>(col)])
					{
						run++;
					}
					const auto [low, count] = range[static_cast<std::size_t>(col)];
					grid.raw_rows.push_back(static_cast<std::uint8_t>(run));
					grid.raw_rows.push_back(static_cast<std::uint8_t>(count));
					if (count)
					{
						grid.raw_rows.push_back(static_cast<std::uint8_t>(low - z_low));
						if (row.zCount > 255)
						{
							grid.raw_rows.push_back(0);
						}
					}
					for (int k = 0; k < run; k++)
					{
						for (int z = low; z < low + count; z++)
						{
							const std::size_t i = (static_cast<std::size_t>(r) * cols + col + k) * layers + z;
							const sample& s = samples[i];
							GfxLightGridEntry entry{};
							// a point inside a brush between two open ones: black, the first set when there is none
							entry.colorsIndex = static_cast<std::uint16_t>(s.solid ? 0 : set_of[i]);
							entry.primaryLightIndex = s.light;
							grid.entries.push_back(entry);
						}
					}
					col += run;
				}
				while (grid.raw_rows.size() % 4)
				{
					grid.raw_rows.push_back(0);
				}
			}
			if (grid.colors.empty())
			{
				grid.colors.push_back({});
			}
		}

		// ---- reflection probes: [0] is the default every map has; then one per reflection_probe entity, or one in the
		// middle of the map. A probe is a picture of the scene from its place: per texel of the cube a ray, which shows
		// the sky's colour or what the lightmap luxel it ends on throws back ( the scene without its textures ).
		// Faces in the order + x - x + y - y + z - z with the texel directions of a Direct3D cube map, world axes.
		{
			const auto probe_at = [&](const vec3& p)
			{
				render_probe probe;
				probe.origin[0] = static_cast<float>(p.x);
				probe.origin[1] = static_cast<float>(p.y);
				probe.origin[2] = static_cast<float>(p.z);
				float around[3];
				gather(setup, p, nullptr, around);
				for (int face = 0; face < 6; face++)
				{
					std::vector<float> level(64 * 64 * 3);
					for (int y = 0; y < 64; y++)
					{
						for (int x = 0; x < 64; x++)
						{
							const double u = 2.0 * (x + 0.5) / 64 - 1.0, v = 2.0 * (y + 0.5) / 64 - 1.0;
							vec3 d = face == 0 ? vec3{ 1, -v, -u } : face == 1 ? vec3{ -1, -v, u } : face == 2 ? vec3{ u, 1, v } : face == 3 ? vec3{ u, -1, -v }
								: face == 4 ? vec3{ u, -v, 1 } : vec3{ -u, -v, -1 };
							d = d * (1.0 / length(d));
							float c[3];
							if (!ctx.opts.light)
							{
								std::memcpy(c, w.ambient, sizeof(c));
							}
							else if (incoming(p, d, c) == ray_world::seen::model)
							{
								for (int k = 0; k < 3; k++)
								{
									c[k] = REFLECT * around[k];		// a static model: lit like the probe's own place
								}
							}
							std::memcpy(&level[(static_cast<std::size_t>(y) * 64 + static_cast<std::size_t>(x)) * 3], c, sizeof(c));
						}
					}
					for (int size = 64; size >= 1; size /= 2)
					{
						for (int k = 0; k < size * size; k++)
						{
							probe.pixels.push_back(to_byte(std::clamp(level[static_cast<std::size_t>(k) * 3 + 2], 0.0f, 1.0f)));
							probe.pixels.push_back(to_byte(std::clamp(level[static_cast<std::size_t>(k) * 3 + 1], 0.0f, 1.0f)));
							probe.pixels.push_back(to_byte(std::clamp(level[static_cast<std::size_t>(k) * 3], 0.0f, 1.0f)));
							probe.pixels.push_back(255);
						}
						if (size > 1)
						{
							const int half = size / 2;
							std::vector<float> smaller(static_cast<std::size_t>(half) * half * 3);
							for (int y = 0; y < half; y++)
							{
								for (int x = 0; x < half; x++)
								{
									for (int k = 0; k < 3; k++)
									{
										const auto at = [&](const int xx, const int yy) { return level[(static_cast<std::size_t>(yy) * size + static_cast<std::size_t>(xx)) * 3 + static_cast<std::size_t>(k)]; };
										smaller[(static_cast<std::size_t>(y) * half + static_cast<std::size_t>(x)) * 3 + static_cast<std::size_t>(k)] =
											0.25f * (at(2 * x, 2 * y) + at(2 * x + 1, 2 * y) + at(2 * x, 2 * y + 1) + at(2 * x + 1, 2 * y + 1));
									}
								}
							}
							level = std::move(smaller);
						}
					}
				}
				return probe;
			};
			const float black[3] = { 0, 0, 0 };
			render_probe none;
			none.pixels = flat_probe(black);
			rw.probes.push_back(std::move(none));
			for (const map_entity& e : w.entities)
			{
				if (e.classname == "reflection_probe" && rw.probes.size() < 255)
				{
					rw.probes.push_back(probe_at(e.origin));
				}
			}
			if (rw.probes.size() == 1)
			{
				rw.probes.push_back(probe_at(vec3{ (rw.bounds.mins[0] + rw.bounds.maxs[0]) * 0.5, (rw.bounds.mins[1] + rw.bounds.maxs[1]) * 0.5,
					(rw.bounds.mins[2] + rw.bounds.maxs[2]) * 0.5 }));
			}
			// a cell lists the probes inside its box ( all of them when none is ); a surface uses the nearest of its cell
			for (render_cell& cell : rw.cells)
			{
				for (std::size_t p = 1; p < rw.probes.size(); p++)
				{
					if (cell.box.contains(rw.probes[p].origin, 1.0f))
					{
						cell.probes.push_back(static_cast<std::uint8_t>(p));
					}
				}
				for (std::size_t p = 1; cell.probes.empty() && p < rw.probes.size(); p++)
				{
					cell.probes.push_back(static_cast<std::uint8_t>(p));
				}
			}
			for (render_surface& surface : rw.surfaces)
			{
				const std::vector<std::uint8_t>& list = rw.cells[static_cast<std::size_t>(surface.model == 0 ? surface.cell : 0)].probes;
				float best = 1e30f;
				for (const std::uint8_t p : list)
				{
					float d = 0;
					for (int k = 0; k < 3; k++)
					{
						const float delta = (surface.box.mins[k] + surface.box.maxs[k]) * 0.5f - rw.probes[p].origin[k];
						d += delta * delta;
					}
					if (d < best)
					{
						best = d;
						surface.reflection_probe = p;
					}
				}
			}
		}

		// ---- static models: the sun where it reaches the model's centre, the nearest probe of its cell. Their
		// brightness comes from the light grid at run time ( the sample at their lighting origin looks out of the model:
		// its triangles are one sided for points in space ); their LOD 0 triangles shadow the lightmaps and the grid.
		for (render_smodel& s : rw.smodels)
		{
			const box3& box = w.models[static_cast<std::size_t>(s.model)].box;
			const vec3 centre{ (box.mins[0] + box.maxs[0]) * 0.5, (box.mins[1] + box.maxs[1]) * 0.5, (box.mins[2] + box.maxs[2]) * 0.5 };
			s.primary_light = !ctx.opts.light || rays.reaches_sky(centre, setup.sun, true) ? 1 : 0;
			float best = 1e30f;
			for (const std::uint8_t p : rw.cells[static_cast<std::size_t>(s.cell)].probes)
			{
				const vec3 d = centre - vec3{ rw.probes[p].origin[0], rw.probes[p].origin[1], rw.probes[p].origin[2] };
				if (static_cast<float>(dot(d, d)) < best)
				{
					best = static_cast<float>(dot(d, d));
					s.reflection_probe = p;
				}
			}
		}
		rw.light_surfaces.assign(w.lights.size(), {});

		// ---- the minimap picture: the map from above as the lightmaps light it, north ( + y ) up; where a ray from
		// above meets nothing, the picture is see through
		if (!ctx.opts.minimap.empty())
		{
			constexpr int SIZE = 512;
			std::vector<std::uint8_t> pixels(static_cast<std::size_t>(SIZE) * SIZE * 4, 0);
			const vec3 down{ 0, 0, -1 };
			parallel_for(static_cast<std::size_t>(SIZE), [&](const std::size_t row)
			{
				for (int col = 0; col < SIZE; col++)
				{
					vec3 p{ w.minimap_mins[0] + (col + 0.5) * (w.minimap_maxs[0] - w.minimap_mins[0]) / SIZE,
						w.minimap_maxs[1] - (static_cast<double>(row) + 0.5) * (w.minimap_maxs[1] - w.minimap_mins[1]) / SIZE, rw.bounds.maxs[2] + 16.0 };
					float c[3] = { 0, 0, 0 };
					bool found = false;
					for (int step = 0; step < 16 && !found; step++)
					{
						const ray_world::sight s = rays.look(p, down, true);
						if (s.what == ray_world::seen::nothing || (s.what == ray_world::seen::sky && s.t <= 0))
						{
							break;
						}
						const bool sky_brush = s.what == ray_world::seen::sky || (s.what == ray_world::seen::face && s.brush >= 0 && rays.sky[static_cast<std::size_t>(s.brush)]);
						if (s.what == ray_world::seen::sky && s.t >= 1e8)
						{
							break;			// nothing below
						}
						if (sky_brush)
						{
							// through the sky: on below the brush
							p.z -= s.t + 1.0;
							for (int k = 0; k < 4096 && rays.in_solid(p); k++)
							{
								p.z -= 4.0;
							}
							continue;
						}
						found = true;
						if (s.what == ray_world::seen::face && !emit.empty())
						{
							if (const std::size_t from = luxel_at(s.brush, s.face, p + down * s.t); from != npos)
							{
								for (int k = 0; k < 3; k++)
								{
									c[k] = emit[from * 3 + static_cast<std::size_t>(k)] / REFLECT;
								}
							}
						}
						else
						{
							c[0] = c[1] = c[2] = 0.35f;		// a static model, a face without a lightmap
						}
					}
					std::uint8_t* out = &pixels[(row * SIZE + static_cast<std::size_t>(col)) * 4];
					if (found)
					{
						// lightmap brightness, lifted so that shadow is not black on the compass
						out[0] = to_byte(0.15f + 0.6f * std::min(c[2], 1.4f));
						out[1] = to_byte(0.15f + 0.6f * std::min(c[1], 1.4f));
						out[2] = to_byte(0.15f + 0.6f * std::min(c[0], 1.4f));
						out[3] = 255;
					}
				}
			});
			write_tga(ctx.opts.minimap, SIZE, SIZE, pixels);
			ctx.rep.notes.push_back("minimap: " + ctx.opts.minimap.string() + format(" ( %d x %d, the square ( %g %g ) - ( %g %g ) )", SIZE, SIZE, w.minimap_mins[0], w.minimap_mins[1],
				w.minimap_maxs[0], w.minimap_maxs[1]));
		}
	}
}
