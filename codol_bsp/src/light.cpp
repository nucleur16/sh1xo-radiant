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
			const world& w;
			std::vector<int> brushes;		// world::brushes indices, in tree order
			std::vector<node> nodes;
			std::vector<std::uint8_t> sky;	// per world brush: has a sky face

			explicit ray_world(const world& world_) : w(world_)
			{
				sky.assign(w.brushes.size(), 0);
				for (std::size_t b = 0; b < w.brushes.size(); b++)
				{
					const map_brush& brush = w.brushes[b];
					if (brush.model != 0 || !(brush.contents & 1))
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
			bool reaches_sky(const vec3& o, const vec3& d) const
			{
				ray_hit hit;
				if (!trace(o, d, 1e9, hit))
				{
					return true;
				}
				if (hit.face < 0 || !sky[static_cast<std::size_t>(hit.brush)])
				{
					return false;
				}
				return w.brushes[static_cast<std::size_t>(hit.brush)].faces[static_cast<std::size_t>(hit.face)].material.rfind("sky", 0) == 0;
			}

			bool blocked(const vec3& from, const vec3& to) const
			{
				const vec3 delta = to - from;
				const double distance = length(delta);
				ray_hit hit;
				return distance > 1e-6 && trace(from, delta * (1.0 / distance), distance - 0.5, hit);
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
				if (s.rays.reaches_sky(p, d))
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
				if (s.rays.blocked(p, origin))
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

		// one face in the lightmap atlas
		struct chart
		{
			int brush = 0, face = 0;
			vec3 origin, u_axis, v_axis, normal;
			double u_min = 0, v_min = 0, u_max = 0, v_max = 0;
			int w = 0, h = 0, x = 0, y = 0;		// in luxels of a secondary layer; one luxel of padding all around
		};

		// shelf packing into one LM_W x LM_H page; false when it does not fit
		bool pack(std::vector<chart>& charts)
		{
			std::vector<std::size_t> order(charts.size());
			for (std::size_t k = 0; k < order.size(); k++)
			{
				order[k] = k;
			}
			std::sort(order.begin(), order.end(), [&](const std::size_t a, const std::size_t b)
			{
				return charts[a].h != charts[b].h ? charts[a].h > charts[b].h : a < b;
			});
			int x = 0, y = 0, shelf = 0;
			for (const std::size_t k : order)
			{
				chart& c = charts[k];
				if (c.w > LM_W || c.h > LM_H)
				{
					return false;
				}
				if (x + c.w > LM_W)
				{
					x = 0;
					y += shelf;
					shelf = 0;
				}
				if (y + c.h > LM_H)
				{
					return false;
				}
				c.x = x;
				c.y = y;
				x += c.w;
				shelf = std::max(shelf, c.h);
			}
			return true;
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

		// ---- lightmaps
		if (!ctx.opts.light)
		{
			flat_lighting(ctx, rw);
		}
		else
		{
			// one chart per brush face that is drawn ( a face drawn by several cells has one chart )
			std::map<std::pair<int, int>, std::size_t> chart_of;
			std::vector<chart> charts;
			for (const render_surface& surface : rw.surfaces)
			{
				if (surface.lightmap == 31)
				{
					continue;
				}
				for (const render_face& rf : surface.faces)
				{
					if (rf.brush < 0 || chart_of.count({ rf.brush, rf.face }))
					{
						continue;
					}
					const map_face& face = w.brushes[static_cast<std::size_t>(rf.brush)].faces[static_cast<std::size_t>(rf.face)];
					chart c;
					c.brush = rf.brush;
					c.face = rf.face;
					c.normal = face.plane.normal;
					vec3 v_axis;
					edit::face_texture_axes(c.normal, 0.0, c.u_axis, v_axis);
					c.u_axis = c.u_axis - c.normal * dot(c.u_axis, c.normal);
					c.u_axis = c.u_axis * (1.0 / std::max(length(c.u_axis), 1e-9));
					c.v_axis = cross(c.normal, c.u_axis);
					c.origin = c.normal * face.plane.dist;
					c.u_min = c.v_min = 1e30;
					c.u_max = c.v_max = -1e30;
					for (const vec3& p : face.winding)
					{
						const double u = dot(p - c.origin, c.u_axis), v = dot(p - c.origin, c.v_axis);
						c.u_min = std::min(c.u_min, u);
						c.u_max = std::max(c.u_max, u);
						c.v_min = std::min(c.v_min, v);
						c.v_max = std::max(c.v_max, v);
					}
					chart_of[{ rf.brush, rf.face }] = charts.size();
					charts.push_back(c);
				}
			}
			// the luxel size: as asked for, larger when the map does not fit one page
			double luxel = ctx.opts.quality <= 0 ? 32.0 : ctx.opts.quality == 1 ? 16.0 : 8.0;
			for (;; luxel *= 1.25)
			{
				for (chart& c : charts)
				{
					c.w = static_cast<int>(std::ceil((c.u_max - c.u_min) / luxel)) + 2;
					c.h = static_cast<int>(std::ceil((c.v_max - c.v_min) / luxel)) + 2;
				}
				if (pack(charts) || luxel > 4096)
				{
					break;
				}
			}
			ctx.rep.notes.push_back(format("light: %zu faces in the lightmap, one luxel is %.1f units; %zu light entities, %d sky directions", charts.size(), luxel,
				w.bake_lights.size(), direction_count));

			render_lightmap lightmap;
			lightmap.primary.assign(static_cast<std::size_t>(LM_W) * 2 * LM_H * 2, 0);
			lightmap.secondary.assign(static_cast<std::size_t>(LM_W) * LM_H * 2 * 4, 0);
			for (std::size_t k = 3; k < lightmap.secondary.size(); k += 4)
			{
				lightmap.secondary[k] = 255;
			}
			std::vector<std::uint8_t> sun_lit(charts.size(), 0);
			parallel_for(charts.size(), [&](const std::size_t index)
			{
				const chart& c = charts[index];
				const double facing = dot(c.normal, setup.sun);
				const auto position = [&](const double lx, const double ly)
				{
					// lx, ly: luxels from the chart's corner; the padding repeats the edge of the face
					const double u = std::clamp(c.u_min + (lx - 1.0) * luxel, c.u_min, c.u_max), v = std::clamp(c.v_min + (ly - 1.0) * luxel, c.v_min, c.v_max);
					return c.origin + c.u_axis * u + c.v_axis * v + c.normal * 1.0;
				};
				for (int y = 0; y < c.h; y++)
				{
					for (int x = 0; x < c.w; x++)
					{
						float color[3];
						gather(setup, position(x + 0.5, y + 0.5), &c.normal, color);
						std::uint8_t* texel = lightmap.secondary.data() + (static_cast<std::size_t>(c.y + y) * LM_W + static_cast<std::size_t>(c.x + x)) * 4;
						texel[0] = to_byte(color[2]);
						texel[1] = to_byte(color[1]);
						texel[2] = to_byte(color[0]);
						for (int sy = 0; sy < 2; sy++)
						{
							for (int sx = 0; sx < 2; sx++)
							{
								const bool lit = facing > 0.01 && rays.reaches_sky(position(x + 0.25 + 0.5 * sx, y + 0.25 + 0.5 * sy), setup.sun);
								lightmap.primary[(static_cast<std::size_t>(c.y + y) * 2 + static_cast<std::size_t>(sy)) * (LM_W * 2) + static_cast<std::size_t>(c.x + x) * 2
									+ static_cast<std::size_t>(sx)] = lit ? 255 : 0;
								sun_lit[index] = sun_lit[index] || lit;
							}
						}
					}
				}
			});
			// soften the sun's edge: a 3 x 3 box filter inside every chart
			{
				const std::vector<std::uint8_t> sharp = lightmap.primary;
				const int pw = LM_W * 2;
				for (const chart& c : charts)
				{
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
										sum += sharp[static_cast<std::size_t>(yy) * pw + static_cast<std::size_t>(xx)];
										n++;
									}
								}
							}
							lightmap.primary[static_cast<std::size_t>(y) * pw + static_cast<std::size_t>(x)] = static_cast<std::uint8_t>(sum / n);
						}
					}
				}
			}
			rw.lightmaps.push_back(std::move(lightmap));

			// lightmap coordinates of the vertices
			for (render_surface& surface : rw.surfaces)
			{
				if (surface.lightmap == 31)
				{
					continue;
				}
				bool any_sun = false;
				for (const render_face& rf : surface.faces)
				{
					const auto it = chart_of.find({ rf.brush, rf.face });
					if (it == chart_of.end())
					{
						continue;
					}
					const chart& c = charts[it->second];
					any_sun = any_sun || sun_lit[it->second];
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

		// ---- light grid: a sample for every point of the grid that is not inside a brush
		{
			render_grid& grid = rw.grid;
			GfxLightGrid& h = grid.header;
			h.hasLightRegions = 0;
			h.lastSunPrimaryLightIndex = 1;
			int mins[3], maxs[3];
			for (int k = 0; k < 3; k++)
			{
				mins[k] = grid_coord(rw.bounds.mins[k], k);
				maxs[k] = grid_coord(rw.bounds.maxs[k], k);
			}
			maxs[2] = std::min(maxs[2], mins[2] + 250);
			h.rowAxis = 0;
			h.colAxis = 1;
			const int rows = maxs[0] - mins[0] + 1, cols = maxs[1] - mins[1] + 1, layers = maxs[2] - mins[2] + 1;
			struct sample
			{
				std::uint8_t rgb[3] = {};
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
					float color[3] = { std::min(w.ambient[0], 1.0f), std::min(w.ambient[1], 1.0f), std::min(w.ambient[2], 1.0f) };
					if (ctx.opts.light)
					{
						gather(setup, p, nullptr, color);
						s.light = rays.reaches_sky(p, setup.sun) ? 1 : 0;
					}
					else
					{
						s.light = 1;
					}
					for (int k = 0; k < 3; k++)
					{
						s.rgb[k] = static_cast<std::uint8_t>(to_byte(color[k]) & 0xFC);
					}
				}
			});
			for (int k = 0; k < 3; k++)
			{
				h.mins[k] = static_cast<std::uint16_t>(mins[k]);
				h.maxs[k] = static_cast<std::uint16_t>(maxs[k]);
			}
			std::map<std::uint32_t, std::uint16_t> color_index;
			const auto color_of = [&](const sample& s)
			{
				const std::uint32_t key = s.rgb[0] | (s.rgb[1] << 8) | (s.rgb[2] << 16);
				if (const auto it = color_index.find(key); it != color_index.end())
				{
					return it->second;
				}
				GfxLightGridColors colors{};
				for (auto& rgb : colors.rgb)
				{
					std::memcpy(rgb, s.rgb, 3);
				}
				const std::uint16_t index = static_cast<std::uint16_t>(grid.colors.size());
				grid.colors.push_back(colors);
				color_index.emplace(key, index);
				return index;
			};
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
							const sample& s = samples[(static_cast<std::size_t>(r) * cols + col + k) * layers + z];
							GfxLightGridEntry entry{};
							entry.colorsIndex = color_of(s);
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
				color_of(sample{});
			}
		}

		// ---- reflection probes: [0] is the default every map has; then one per reflection_probe entity, or one in the
		// middle of the map. A probe is not rendered: it is one colour, the light arriving at its place.
		{
			const auto probe_at = [&](const vec3& p)
			{
				float color[3];
				gather(setup, p, nullptr, color);
				render_probe probe;
				for (int k = 0; k < 3; k++)
				{
					color[k] = std::clamp(color[k] + (ctx.opts.light && rays.reaches_sky(p, setup.sun) ? w.sun_color[k] * 0.25f : 0.0f), 0.02f, 1.0f);
				}
				probe.pixels = flat_probe(color);
				probe.origin[0] = static_cast<float>(p.x);
				probe.origin[1] = static_cast<float>(p.y);
				probe.origin[2] = static_cast<float>(p.z);
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

		rw.light_surfaces.assign(w.lights.size(), {});
	}
}
