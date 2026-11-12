// The light bake: lightmaps and the surfaces' lightmap coordinates, the light grid, reflection probes, and what the
// primary lights of the map hang on ( which surface, grid point and static model has which, shadow geometry, regions ).
//
// How the game's lit shaders use the data was read from the shaders themselves ( codol_viewtool with
// CODOL_VIEW_DUMP_SHADERS, D3DDisassemble; lm_dfog_r0c0n0s0_sm3, lm_spot_dfog_.., lm_sm_sun_dfog_.., lp_dfog_.. ):
//
//   secondary lightmap  A8R8G8B8, two layers on top of each other, read at ( u, v / 2 ) and ( u, v / 2 + 1 / 2 ).
//                         light = ( upper.rgb * Nz + lower.rgb * cos( D, N ) ) ^ 2
//                       N is the normal of the normal map in tangent space, D the direction ( dx, dy, 1 ) with
//                       dx = upper.a * 4.08 - 2.08 along the tangent and dy = lower.a * 4.0645 - 2.0645 along
//                       cross( normal, tangent ) * binormal sign. So the upper layer is light without a direction, the
//                       lower layer light from the direction the two alpha channels give, and both hold SQUARE ROOTS:
//                       the shader squares the sum. ( Bytes written as linear light come out squared - a room lit by a
//                       light entity was nearly black. )
//   primary lightmap    L8, twice the resolution of a layer: how much of the surface's primary light reaches the
//                       texel. The shader adds the light itself: colour * max( 0, N . L ) for the sun, colour *
//                       falloff ^ 2 * cone * max( 0, N . L ) for a spot or omni light.
//   falloff             a primary spot / omni light's falloff is read from ROW 0 of the secondary lightmap, at
//                       u = ( lmapLookupStart + 32 * distance / radius ) / 512 ( lightFalloffPlacement ): the compiler
//                       lays a copy of the light definition's falloff picture there, in every page.
//   light grid          a colour byte is sqrt( light ) / 2: the model shaders double and square what they read.
//   reflection probes   square roots as well.
//
// So everything here is computed in linear light and written as its square root. Per luxel the bake keeps what
// arrives ( rgb ) and where it comes from ( the sum of luminance x direction ): light that comes from one side goes
// to the lower layer with that direction, the rest to the upper layer; on a flat normal the two add up to exactly
// what arrives.
//
// A surface names one primary light: the sun, or a light entity with PRIMARY_SPOT / PRIMARY_OMNI. Its light is not in
// the secondary lightmap ( the shader adds it, with the dynamic shadows on top ); every other light that reaches the
// surface is baked.

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

		// linear light as the byte the lightmaps and reflection probes hold: its square root
		std::uint8_t root_byte(const float v)
		{
			return to_byte(std::sqrt(std::clamp(v, 0.0f, 1.0f)));
		}

		// linear light as a byte of a light grid colour: half its square root
		std::uint8_t grid_byte(const float v)
		{
			return static_cast<std::uint8_t>(std::clamp(std::lround(std::sqrt(std::max(v, 0.0f)) * 127.5f), 0l, 255l));
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

		double luminance(const float c[3])
		{
			return 0.3 * c[0] + 0.59 * c[1] + 0.11 * c[2];
		}

		// The falloff of a light definition as the game's shaders read it: 32 texels in row 0 of the secondary lightmap,
		// from the light's origin to its radius, read with a bilinear filter and squared.
		struct falloff_curve
		{
			std::array<std::array<float, 3>, 32> texel{};

			// at t ( 0: the light, 1: its radius ): the mean of the three colours; `rgb`: each of them
			double at(const double t, double rgb[3] = nullptr) const
			{
				const double x = std::clamp(t, 0.0, 1.0) * 32.0 - 0.5;
				const int i = static_cast<int>(std::floor(x));
				const double f = x - i;
				double sum = 0;
				for (std::size_t c = 0; c < 3; c++)
				{
					const double a = texel[static_cast<std::size_t>(std::clamp(i, 0, 31))][c], b = texel[static_cast<std::size_t>(std::clamp(i + 1, 0, 31))][c];
					const double v = a + (b - a) * f;
					if (rgb)
					{
						rgb[c] = v * v;
					}
					sum += v * v;
				}
				return sum / 3.0;
			}
		};

		struct light_setup
		{
			const world& w;
			const ray_world& rays;
			vec3 sun;
			std::vector<vec3> sky_directions;	// over the sphere
			float sky_color[3] = {}, ambient[3] = {};
			std::vector<falloff_curve> falloffs;	// per light definition ( world::light_defs )
			// the light entities: the primary ones ( world::lights from index 2 ) first, then those that are only baked
			std::vector<const map_light*> points;
			std::vector<int> primary_of;		// per point: its primary light index, -1: baked only
		};

		// How much of a light entity arrives at a point, before the angle of a surface and shadows: its falloff and its
		// cone, as the game's spot light shader has them. 0: out of reach. `towards`: the unit vector to the light.
		// `tint`: the colour of the falloff ( a coloured falloff picture ), 1 1 1 for a grey one - what the light's
		// colour is to be multiplied with.
		double light_reach(const light_setup& s, const map_light& light, const vec3& p, vec3& towards, double& distance, float* tint = nullptr)
		{
			if (tint)
			{
				tint[0] = tint[1] = tint[2] = 1.0f;
			}
			const vec3 to = vec3{ light.origin[0], light.origin[1], light.origin[2] } - p;
			distance = length(to);
			if (distance >= light.radius || distance < 1e-3)
			{
				return 0;
			}
			towards = to * (1.0 / distance);
			double rgb[3] = { 1, 1, 1 };
			double scale = s.falloffs[static_cast<std::size_t>(std::clamp(light.def_index, 0, static_cast<int>(s.falloffs.size()) - 1))].at(distance / light.radius, rgb);
			if (tint && scale > 1e-9)
			{
				for (int k = 0; k < 3; k++)
				{
					tint[k] = static_cast<float>(rgb[k] / scale);
				}
			}
			if (light.cone)
			{
				// `dir` points back along the beam
				const double c = dot(towards, vec3{ light.dir[0], light.dir[1], light.dir[2] });
				if (c <= light.cos_half_fov_outer)
				{
					return 0;
				}
				if (light.exponent > 0 && c < light.cos_half_fov_inner)
				{
					scale *= std::pow((c - light.cos_half_fov_outer) / std::max(1e-6, static_cast<double>(light.cos_half_fov_inner - light.cos_half_fov_outer)), light.exponent);
				}
			}
			return scale;
		}

		// light arriving at a point, and where it comes from
		struct arriving
		{
			float e[3] = {};		// linear
			double dir[3] = {};		// sum of luminance x direction

			void add(const float color[3], const double scale, const vec3& from)
			{
				float c[3];
				for (int k = 0; k < 3; k++)
				{
					c[k] = color[k] * static_cast<float>(scale);
					e[k] += c[k];
				}
				const double lum = luminance(c);
				dir[0] += lum * from.x;
				dir[1] += lum * from.y;
				dir[2] += lum * from.z;
			}
		};

		// Light arriving at a point: ambient, sky, the light entities, the sun. With a normal: on a surface; without: at
		// a point in space, where half of everything counts. `skip`: a primary light that is left out - the one the
		// surface names, which the shader adds; 0: none.
		void gather(const light_setup& s, const vec3& p, const vec3* normal, const int skip, arriving& out)
		{
			double sky = 0, weight = 0;
			vec3 sky_from{ 0, 0, 0 };
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
					sky_from = sky_from + d * c;
				}
			}
			// light without a direction counts as coming from all over the surface's side: its mean direction is 2 / 3 of the normal
			const vec3 around = normal ? *normal * (2.0 / 3.0) : vec3{ 0, 0, 0 };
			out.add(s.ambient, 1.0, around);
			if (weight > 0 && sky > 0)
			{
				out.add(s.sky_color, sky / weight, sky_from * (1.0 / sky));
			}
			for (std::size_t i = 0; i < s.points.size(); i++)
			{
				if (skip > 0 && s.primary_of[i] == skip)
				{
					continue;
				}
				const map_light& light = *s.points[i];
				vec3 l;
				double distance = 0;
				float tint[3];
				double scale = light_reach(s, light, p, l, distance, tint);
				scale = scale > 0 ? scale * (normal ? dot(l, *normal) : 0.5) : 0;
				if (scale <= 0 ||s.rays.blocked(p, vec3{ light.origin[0], light.origin[1], light.origin[2] }, !normal))
				{
					continue;
				}
				const float color[3] = { light.color[0] * tint[0], light.color[1] * tint[1], light.color[2] * tint[2] };
				out.add(color, scale, l);
			}
			if (skip != 1 && luminance(s.w.sun_color) > 0)
			{
				const double scale = normal ? dot(s.sun, *normal) : 0.5;
				if (scale > 0.01 && s.rays.reaches_sky(p, s.sun, !normal))
				{
					out.add(s.w.sun_color, scale, s.sun);
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
			std::uint32_t first_vertex = 0;		// of its face, in the surface
		};

		// Row 0 of every page belongs to the falloff curves of the light definitions ( see the head of this file ).
		constexpr int LM_FIRST_ROW = 1;

		// Shelf packing into pages of LM_W x LM_H luxels. All charts of a surface go into one page ( a surface names one
		// lightmap ); surfaces are placed tallest chart first, each into the first page that takes it. false: a surface
		// does not fit an empty page, or more than `max_pages` are needed.
		bool pack(std::vector<chart>& charts, const std::vector<std::vector<std::size_t>>& of_surface, const int max_pages, int& page_count)
		{
			struct shelf
			{
				int x = 0, y = LM_FIRST_ROW, height = 0;
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
						if (c.w > LM_W || c.h > LM_H - LM_FIRST_ROW)
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
						if (pages[p].x == 0 && pages[p].y == LM_FIRST_ROW && pages[p].height == 0)
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
				lightmap.secondary[k] = upper ? root_byte(w.ambient[2]) : 0;
				lightmap.secondary[k + 1] = upper ? root_byte(w.ambient[1]) : 0;
				lightmap.secondary[k + 2] = upper ? root_byte(w.ambient[0]) : 0;
				lightmap.secondary[k + 3] = 128;
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

	namespace
	{
		// the ranges of a set of points along the nine axes of a light region hull: x, y, z, x + y, x - y, x + z, x - z,
		// y + z, y - z ( not normalised ), relative to the light's origin - what the hulls of the shipped maps hold
		// ( codol_bsp_probe lights )
		//
		// Behind the nine come the ranges along further axes, the candidates for the extra planes a shipped hull has ( up
		// to eight ). Measured on mp_vacant, mp_highrise and mp_nuked: those are the four diagonals of the cube
		// ( 0.577 +-0.577 +-0.577 ) and, for a spot, planes that touch its cone - sin( half angle ) x beam + cos( half
		// angle ) x a direction across the beam, eight of them 45 degrees apart; a point inside the cone is never below
		// 0 along such an axis.
		//
		// And planes THROUGH THE LIGHT AND AN EDGE OF A PORTAL: what a light sends through a doorway lies inside the
		// planes through the light and the doorway's edges. Measured with codol_bsp_probe hullplanes: mp_highrise has 63
		// axes whose range starts at the light, 15 of them hold a portal edge; mp_nuked 16 / 8, zm_asylum_sh 79 / 27 -
		// nearly all in the hulls of cells the light is not in ( 10 of 67, 6 of 18, 21 of 60 such hulls have one ).
		// The candidates here: the edges of the portals nearest to the light, up to KDOP_PORTAL planes; a hull of a
		// cell the light is not in takes those that cut the most, like any other candidate. ( Most of the shipped
		// hulls' other axes are directions of the map's walls. )
		constexpr int KDOP_CONE = 4, KDOP_PORTAL = 12, KDOP_EXTRA = KDOP_PORTAL + 20;
		using kdop_axes = std::array<vec3, KDOP_EXTRA>;

		// how far a script may turn a light from where it was compiled ( "maxturn" ), in radians
		double turn_limit(const map_light& light)
		{
			return light.scriptable ? std::acos(std::clamp(static_cast<double>(light.rotation_limit), -1.0, 1.0)) : 0.0;
		}

		// A light a script may move or turn: what it can reach is more than what it reaches as compiled.
		bool movable(const map_light& light)
		{
			return light.primary && light.scriptable && (light.translation_limit > 0.0f || light.rotation_limit < 0.9999f);
		}

		// The half angle of the cone a light's beam stays in however a script turns it: the outer cone widened by
		// "maxturn" - what the record's "expanded" cone is ( zm_asylum_sh: four lights with maxturn 360 have -1 there ).
		double expanded_half_angle(const map_light& light)
		{
			return std::min(PI, std::acos(std::clamp(static_cast<double>(light.cos_half_fov_outer), -1.0, 1.0)) + turn_limit(light));
		}

		kdop_axes extra_axes(const map_light& light, const render_world& rw)
		{
			kdop_axes out{};
			const double d = 1.0 / std::sqrt(3.0);
			out[0] = { d, d, d };
			out[1] = { d, d, -d };
			out[2] = { d, -d, d };
			out[3] = { d, -d, -d };
			const vec3 beam{ -light.dir[0], -light.dir[1], -light.dir[2] };
			// the cone a script can turn the beam within; half a sphere or more has no planes that touch it
			const double half = expanded_half_angle(light);
			const bool spot = light.type == 2 && length(beam) > 0.5 && half < PI * 0.5 - 0.01;
			vec3 right = cross(beam, std::fabs(beam.z) < 0.99 ? vec3{ 0, 0, 1 } : vec3{ 0, 1, 0 });
			right = right * (1.0 / std::max(length(right), 1e-9));
			const vec3 up = cross(right, beam);
			const double c = std::cos(half), s = std::sin(half);
			for (int k = 0; k < 8; k++)
			{
				const double a = k * PI / 4.0;
				// an omni light has no cone: no candidates ( a zero axis is never taken )
				out[static_cast<std::size_t>(KDOP_CONE + k)] = spot ? beam * s + (right * std::cos(a) + up * std::sin(a)) * c : vec3{ 0, 0, 0 };
			}
			// the portals within the light's reach, nearest first; each is stored by both of its cells
			struct near_portal
			{
				double distance;
				const render_portal* portal;
				vec3 centre;
			};
			std::vector<near_portal> portals;
			const vec3 origin{ light.origin[0], light.origin[1], light.origin[2] };
			const double reach = static_cast<double>(light.radius) + (light.scriptable ? light.translation_limit : 0.0f);
			for (const render_cell& cell : rw.cells)
			{
				for (const render_portal& portal : cell.portals)
				{
					const std::size_t corners = portal.vertices.size() / 3;
					if (corners < 3)
					{
						continue;
					}
					vec3 centre{ 0, 0, 0 };
					for (std::size_t k = 0; k < corners; k++)
					{
						centre = centre + vec3{ portal.vertices[k * 3], portal.vertices[k * 3 + 1], portal.vertices[k * 3 + 2] } * (1.0 / static_cast<double>(corners));
					}
					double nearest = 1e30;
					for (std::size_t k = 0; k < corners; k++)
					{
						nearest = std::min(nearest, length(vec3{ portal.vertices[k * 3], portal.vertices[k * 3 + 1], portal.vertices[k * 3 + 2] } - origin));
					}
					nearest = std::min(nearest, length(centre - origin));
					const double off = portal.plane[0] * origin.x + portal.plane[1] * origin.y + portal.plane[2] * origin.z - portal.plane[3];
					if (nearest >= reach || std::fabs(off) < 1.0)
					{
						continue;		// out of reach, or the light lies in the portal's plane
					}
					bool known = false;
					for (const near_portal& other : portals)
					{
						known = known || length(other.centre - centre) < 0.5;
					}
					if (!known)
					{
						portals.push_back({ length(centre - origin), &portal, centre });
					}
				}
			}
			std::sort(portals.begin(), portals.end(), [](const near_portal& a, const near_portal& b) { return a.distance < b.distance; });
			std::size_t next = KDOP_PORTAL;
			for (const near_portal& np : portals)
			{
				const std::vector<float>& v = np.portal->vertices;
				const std::size_t corners = v.size() / 3;
				for (std::size_t k = 0; k < corners && next < KDOP_EXTRA; k++)
				{
					const std::size_t n = (k + 1) % corners;
					const vec3 a = vec3{ v[k * 3], v[k * 3 + 1], v[k * 3 + 2] } - origin, b = vec3{ v[n * 3], v[n * 3 + 1], v[n * 3 + 2] } - origin;
					vec3 normal = cross(a, b);
					if (length(normal) < 1.0 || length(a - b) < 4.0)
					{
						continue;
					}
					normal = normal * (1.0 / length(normal));
					if (dot(normal, np.centre - origin) < 0)
					{
						normal = normal * -1.0;		// what shines through the portal is on the plane's positive side
					}
					bool known = false;
					for (std::size_t o = 0; o < next; o++)
					{
						known = known || std::fabs(dot(out[o], normal)) > 0.9995;
					}
					if (!known)
					{
						out[next++] = normal;
					}
				}
			}
			return out;
		}

		// What a movable light ( `movable` ) could send to a point in some position a script may give it: the falloff at
		// the distance less "maxmove", inside the cone widened by "maxturn" - and, for a light that may be moved, with
		// its tip pulled back so far that the cone holds the cone of every position. 0: never. Shadows are the
		// caller's: the baked visibility of a light is the one from where it was compiled.
		double potential_reach(const light_setup& s, const map_light& light, const vec3& p)
		{
			const vec3 origin{ light.origin[0], light.origin[1], light.origin[2] };
			const double move = light.translation_limit, distance = length(origin - p);
			if (distance >= light.radius + move)
			{
				return 0;
			}
			if (light.cone)
			{
				const double half = expanded_half_angle(light);
				if (half < PI - 1e-3)
				{
					const vec3 back{ light.dir[0], light.dir[1], light.dir[2] };		// back along the beam
					const vec3 tip = origin + back * (move > 0 ? move / std::max(std::sin(std::min(half, PI * 0.5)), 0.05) : 0.0);
					const vec3 to = tip - p;
					if (length(to) > 1e-3 && dot(to * (1.0 / length(to)), back) <= std::cos(half))
					{
						return 0;
					}
				}
			}
			double rgb[3] = { 1, 1, 1 };
			const double scale = s.falloffs[static_cast<std::size_t>(std::clamp(light.def_index, 0, static_cast<int>(s.falloffs.size()) - 1))].at(
				std::max(distance - move, 0.0) / light.radius, rgb);
			return std::max(scale, 1e-6);
		}

		struct kdop
		{
			double lo[9 + KDOP_EXTRA], hi[9 + KDOP_EXTRA];
			bool any = false;

			kdop()
			{
				for (int a = 0; a < 9 + KDOP_EXTRA; a++)
				{
					lo[a] = 1e30;
					hi[a] = -1e30;
				}
			}
			void add(const vec3& p, const kdop_axes& extra)
			{
				double v[9 + KDOP_EXTRA] = { p.x, p.y, p.z, p.x + p.y, p.x - p.y, p.x + p.z, p.x - p.z, p.y + p.z, p.y - p.z };
				for (int a = 0; a < KDOP_EXTRA; a++)
				{
					v[9 + a] = dot(p, extra[static_cast<std::size_t>(a)]);
				}
				for (int a = 0; a < 9 + KDOP_EXTRA; a++)
				{
					lo[a] = std::min(lo[a], v[a]);
					hi[a] = std::max(hi[a], v[a]);
				}
				any = true;
			}
			void add(const kdop& other)
			{
				for (int a = 0; a < 9 + KDOP_EXTRA; a++)
				{
					lo[a] = std::min(lo[a], other.lo[a]);
					hi[a] = std::max(hi[a], other.hi[a]);
				}
				any = any || other.any;
			}
		};

		// The corners of the volume nine ranges enclose ( relative to the light ): every point where three of its 18
		// planes meet and that lies inside all of them. What an extra axis would cut away is measured against these.
		std::vector<vec3> kdop_corners(const double lo[9], const double hi[9])
		{
			static const vec3 axis[9] = { { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 }, { 1, 1, 0 }, { 1, -1, 0 }, { 1, 0, 1 }, { 1, 0, -1 }, { 0, 1, 1 }, { 0, 1, -1 } };
			struct plane
			{
				vec3 n;
				double d;		// n . x <= d
			};
			std::vector<plane> planes;
			for (int a = 0; a < 9; a++)
			{
				planes.push_back({ axis[a], hi[a] });
				planes.push_back({ axis[a] * -1.0, -lo[a] });
			}
			std::vector<vec3> corners;
			for (std::size_t i = 0; i < planes.size(); i++)
			{
				for (std::size_t j = i + 1; j < planes.size(); j++)
				{
					const vec3 nij = cross(planes[i].n, planes[j].n);
					if (dot(nij, nij) < 1e-9)
					{
						continue;
					}
					for (std::size_t k = j + 1; k < planes.size(); k++)
					{
						const double det = dot(nij, planes[k].n);
						if (std::fabs(det) < 1e-9)
						{
							continue;
						}
						const vec3 p = (cross(planes[j].n, planes[k].n) * planes[i].d + cross(planes[k].n, planes[i].n) * planes[j].d + nij * planes[k].d) * (1.0 / det);
						bool inside = true;
						for (std::size_t m = 0; m < planes.size() && inside; m++)
						{
							inside = dot(planes[m].n, p) <= planes[m].d + 1e-3;
						}
						if (inside)
						{
							corners.push_back(p);
						}
					}
				}
			}
			return corners;
		}

		// a world vertex between two others ( a cut through an edge of a face )
		c1::GfxWorldVertex mix_vertex(const c1::GfxWorldVertex& a, const c1::GfxWorldVertex& b, const double t)
		{
			c1::GfxWorldVertex out = t < 0.5 ? a : b;		// normal, tangent and binormal sign of the nearer end
			const float f = static_cast<float>(t);
			for (int k = 0; k < 3; k++)
			{
				out.xyz[k] = a.xyz[k] + (b.xyz[k] - a.xyz[k]) * f;
			}
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

		// the part of a convex polygon on one side of a line of the plane: keep where sign * ( axis . p - at ) >= 0
		std::vector<c1::GfxWorldVertex> clip_polygon(const std::vector<c1::GfxWorldVertex>& in, const vec3& axis, const double at, const double sign)
		{
			std::vector<c1::GfxWorldVertex> out;
			const auto side = [&](const c1::GfxWorldVertex& v) { return sign * (axis.x * v.xyz[0] + axis.y * v.xyz[1] + axis.z * v.xyz[2] - at); };
			for (std::size_t k = 0; k < in.size(); k++)
			{
				const c1::GfxWorldVertex& a = in[k];
				const c1::GfxWorldVertex& b = in[(k + 1) % in.size()];
				const double da = side(a), db = side(b);
				if (da >= 0)
				{
					out.push_back(a);
				}
				if ((da > 0 && db < 0) || (da < 0 && db > 0))
				{
					out.push_back(mix_vertex(a, b, da / (da - db)));
				}
			}
			return out;
		}

		double polygon_area(const std::vector<c1::GfxWorldVertex>& polygon)
		{
			vec3 sum{ 0, 0, 0 };
			for (std::size_t k = 2; k < polygon.size(); k++)
			{
				const vec3 a{ polygon[0].xyz[0], polygon[0].xyz[1], polygon[0].xyz[2] }, b{ polygon[k - 1].xyz[0], polygon[k - 1].xyz[1], polygon[k - 1].xyz[2] },
					c{ polygon[k].xyz[0], polygon[k].xyz[1], polygon[k].xyz[2] };
				sum = sum + cross(b - a, c - a);
			}
			return length(sum) * 0.5;
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
		for (const light_def& def : w.light_defs)
		{
			setup.falloffs.push_back({ def.curve });
		}
		if (setup.falloffs.empty())
		{
			setup.falloffs.push_back({ straight_falloff() });
		}
		if (std::getenv("CODOL_BSP_LIGHT_DEBUG"))
		{
			for (const map_light& b : w.bake_lights)
			{
				std::printf("bake light at %g %g %g radius %g colour %g %g %g cone %d type %d; falloff 0.5 %g\n", b.origin[0], b.origin[1], b.origin[2], b.radius, b.color[0], b.color[1],
					b.color[2], b.cone, b.type, setup.falloffs[0].at(0.5));
			}
		}
		const std::size_t light_count = w.lights.size();
		for (std::size_t l = 2; l < light_count; l++)
		{
			setup.points.push_back(&w.lights[l]);
			setup.primary_of.push_back(static_cast<int>(l));
		}
		for (const map_light& light : w.bake_lights)
		{
			setup.points.push_back(&light);
			setup.primary_of.push_back(-1);
		}
		// directions spread evenly over the sphere ( a Fibonacci spiral )
		const int direction_count = ctx.opts.quality <= 0 ? 24 : ctx.opts.quality == 1 ? 64 : 64 * ctx.opts.quality;
		for (int k = 0; k < direction_count; k++)
		{
			const double z = 1.0 - (2.0 * k + 1.0) / direction_count, r = std::sqrt(std::max(0.0, 1.0 - z * z)), a = k * PI * (3.0 - std::sqrt(5.0));
			setup.sky_directions.push_back({ r * std::cos(a), r * std::sin(a), z });
		}

		// What a surface throws back: what arrives times what its material reflects - the mean of its colour map, whose
		// texels the game's shaders square ( material_color.cpp ) - times 2 x "radiosityscale". The 2: a material whose
		// colour map is a mid grey ( 128: 0.25 squared ) throws back half, which is what every surface did until the
		// seventh round ( a grey world ) and what the sky and ambient light were fitted with; the game's pictures are
		// painted darker than what things reflect. A red wall now throws red light, a dark floor little.
		constexpr float REFLECT = 0.5f;		// of a material without a colour map that could be read, and of static models
		const int bounce_rays = ctx.opts.quality <= 0 ? 12 : ctx.opts.quality == 1 ? 24 : 48;
		const int bounces = !ctx.opts.light ? 0 : ctx.opts.quality <= 1 ? 1 : 2;

		const auto light_origin = [&](const int l) { return vec3{ w.lights[static_cast<std::size_t>(l)].origin[0], w.lights[static_cast<std::size_t>(l)].origin[1], w.lights[static_cast<std::size_t>(l)].origin[2] }; };
		// what primary light `l` sends to a point of a surface, shadows aside; false: nothing
		const auto primary_reach = [&](const int l, const vec3& p, const vec3& normal, float out[3])
		{
			double scale = 0;
			float tint[3] = { 1, 1, 1 };
			const map_light& light = w.lights[static_cast<std::size_t>(l)];
			if (l == 1)
			{
				scale = dot(normal, setup.sun);
				scale = scale > 0.01 ? scale : 0;
			}
			else
			{
				vec3 towards;
				double distance = 0;
				scale = light_reach(setup, light, p, towards, distance, tint);
				scale = scale > 0 ? scale * dot(towards, normal) : 0;
			}
			for (int k = 0; k < 3; k++)
			{
				out[k] = light.color[k] * tint[k] * static_cast<float>(std::max(scale, 0.0));
			}
			return scale > 0;
		};
		// the cell a point is in, by the node stream the game walks
		const auto cell_of = [&](const vec3& p)
		{
			std::size_t at = 0;
			const int cells = static_cast<int>(rw.cells.size());
			while (at < rw.cell_nodes.size())
			{
				const int value = rw.cell_nodes[at];
				if (value <= cells)
				{
					return std::max(value - 1, 0);
				}
				const c1::cplane_s& plane = w.planes.planes[static_cast<std::size_t>(value - cells - 1)];
				const double d = plane.normal[0] * p.x + plane.normal[1] * p.y + plane.normal[2] * p.z - plane.dist;
				at = d > 0 ? at + 2 : at + rw.cell_nodes[at + 1];
			}
			return 0;
		};
		std::vector<kdop_axes> light_axes(light_count);
		for (std::size_t l = 2; l < light_count; l++)
		{
			light_axes[l] = extra_axes(w.lights[l], rw);
		}
		// The lights a script may move or turn ( "maxmove", "maxturn" ). What such a light could reach in another
		// position belongs to it as well - where no light reaches as the map is compiled: the surfaces there name it
		// ( a surface names one primary light, and only the surfaces that name a light are lit by it at run time ),
		// its region holds them, grid points and models there are its. In the shipped maps the four turning lights of
		// zm_asylum_sh ( maxturn 360 ) are named by everything within their radius, all around.
		std::vector<int> movable_lights;
		for (std::size_t l = 2; l < light_count; l++)
		{
			if (movable(w.lights[l]))
			{
				movable_lights.push_back(static_cast<int>(l));
			}
		}
		// the movable light that could light a point of a surface the most, 0: none. For points nothing lights as compiled.
		const auto potential_light = [&](const vec3& p, const vec3* normal)
		{
			int best = 0;
			double most = 0;
			for (const int l : movable_lights)
			{
				const double reach = potential_reach(setup, w.lights[static_cast<std::size_t>(l)], p) * std::max<double>(luminance(w.lights[static_cast<std::size_t>(l)].color), 1e-3);
				if (reach > most && (!normal || dot(*normal, light_origin(l) - p) > 0.01) && !rays.blocked(p, light_origin(l), !normal))
				{
					best = l;
					most = reach;
				}
			}
			return best;
		};
		const auto primary_visible = [&](const int l, const vec3& p, const bool one_sided)
		{
			return l == 1 ? rays.reaches_sky(p, setup.sun, one_sided) : !rays.blocked(p, light_origin(l), one_sided);
		};
		// the primary light of a point in space ( a light grid sample, a static model ): the one that gives it the most
		// light; 0: none reaches it
		const auto primary_at = [&](const vec3& p)
		{
			int best = 0;
			double most = 0;
			if (luminance(w.sun_color) > 0 && rays.reaches_sky(p, setup.sun, true))
			{
				best = 1;
				most = luminance(w.sun_color);
			}
			for (std::size_t l = 2; l < light_count; l++)
			{
				vec3 towards;
				double distance = 0;
				const double lum = light_reach(setup, w.lights[l], p, towards, distance) * luminance(w.lights[l].color);
				if (lum > most && !rays.blocked(p, light_origin(static_cast<int>(l)), true))
				{
					best = static_cast<int>(l);
					most = lum;
				}
			}
			return best == 0 && most == 0 && !movable_lights.empty() ? potential_light(p, nullptr) : best;
		};
		std::vector<std::map<int, kdop>> lit(light_count);	// per primary light and cell: what it lights, relative to its origin
		std::vector<std::size_t> named_surfaces(light_count, 0);

		// per luxel of every chart: the light that arrives, and what the luxel throws back ( the source of the next bounce,
		// of the light grid and of the reflection probes )
		std::vector<chart> charts;
		std::vector<float> direct, indirect, primary_part, came_from, emit;	// 3 per luxel; `came_from`: the sum of luminance x direction
		std::unordered_map<std::uint64_t, std::vector<std::size_t>> chart_of_face;		// ( brush, face ) -> its charts ( a face cut by the lights' reach has several )
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
			// of the pieces of a face the one the point lies in
			std::size_t pick = it->second[0];
			for (const std::size_t ci : it->second)
			{
				const chart& o = charts[ci];
				const double u = dot(p - o.origin, o.u_axis), v = dot(p - o.origin, o.v_axis);
				if (u >= o.u_min - 0.01 && u <= o.u_max + 0.01 && v >= o.v_min - 0.01 && v <= o.v_max + 0.01)
				{
					pick = ci;
					break;
				}
			}
			const chart& c = charts[pick];
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
			// ---- surfaces are cut by the reach of the primary light entities. A surface names one primary light, so a
			// floor that two spots light side by side, or one that reaches far beyond a lamp's circle, has to be several
			// surfaces: the shipped maps have seams between surfaces of one material in one plane that name different
			// lights all over ( mp_vacant: 2087 such edges at its five spots; codol_bsp_probe lightcuts ). Here: every
			// face near a light entity is sampled on a grid of 32 units in its plane ( which light gives the point the
			// most light, the sun included ); where the answer changes within a face, the face is cut along the lines
			// of that grid around what each light entity wins, and every piece goes to the surface of its light. The
			// cuts follow the grid, not the outline of the light: what a piece's light does not reach is dark in the
			// primary lightmap, and what the other lights send there is baked as before.
			if (light_count > 2 && !std::getenv("CODOL_BSP_NO_LIGHT_CUT"))
			{
				constexpr double STEP = 32.0;
				struct piece
				{
					std::vector<c1::GfxWorldVertex> vertices;
					int light = 0;
				};
				std::vector<std::vector<std::vector<piece>>> pieces(rw.surfaces.size());	// per surface and face; none: the face stays as it is, with the others
				std::atomic<std::size_t> cut_faces{ 0 }, made{ 0 };
				parallel_for(rw.surfaces.size(), [&](const std::size_t si)
				{
					const render_surface& surface = rw.surfaces[si];
					if (surface.lightmap == 31)
					{
						return;
					}
					pieces[si].resize(surface.faces.size());
					for (std::size_t fi = 0; fi < surface.faces.size(); fi++)
					{
						const render_face& rf = surface.faces[fi];
						std::vector<c1::GfxWorldVertex> polygon(surface.vertices.begin() + rf.first_vertex, surface.vertices.begin() + rf.first_vertex + rf.vertex_count);
						if (polygon.size() < 3)
						{
							continue;
						}
						const auto at = [&](const std::size_t k) { return vec3{ polygon[k].xyz[0], polygon[k].xyz[1], polygon[k].xyz[2] }; };
						vec3 normal = cross(at(1) - at(0), at(2) - at(0));
						if (length(normal) < 1e-6)
						{
							continue;
						}
						normal = normal * (1.0 / length(normal));
						if (dot(normal, vec3{ rf.normal[0], rf.normal[1], rf.normal[2] }) < 0)
						{
							normal = normal * -1.0;
						}
						vec3 u_axis, v_axis;
						edit::face_texture_axes(normal, 0.0, u_axis, v_axis);
						u_axis = u_axis - normal * dot(u_axis, normal);
						u_axis = u_axis * (1.0 / std::max(length(u_axis), 1e-9));
						v_axis = cross(normal, u_axis);
						const vec3 origin = normal * dot(normal, at(0));
						double lo[2] = { 1e30, 1e30 }, hi[2] = { -1e30, -1e30 };
						box3 box;
						for (std::size_t k = 0; k < polygon.size(); k++)
						{
							const double uv[2] = { dot(at(k), u_axis), dot(at(k), v_axis) };
							for (int a = 0; a < 2; a++)
							{
								lo[a] = std::min(lo[a], uv[a]);
								hi[a] = std::max(hi[a], uv[a]);
							}
							box.add(at(k));
						}
						// the light entities that can reach the face at all
						std::vector<int> near;
						for (std::size_t l = 2; l < light_count; l++)
						{
							const vec3 o = light_origin(static_cast<int>(l));
							double d2 = 0;
							for (int a = 0; a < 3; a++)
							{
								const double d = std::max({ box.mins[a] - o[a], o[a] - box.maxs[a], 0.0 });
								d2 += d * d;
							}
							const double reach = static_cast<double>(w.lights[l].radius) + (movable(w.lights[l]) ? w.lights[l].translation_limit : 0.0f);
							if (d2 < reach * reach && dot(normal, o) - dot(normal, at(0)) > 0)
							{
								near.push_back(static_cast<int>(l));
							}
						}
						if (near.empty())
						{
							continue;
						}
						const int u0 = static_cast<int>(std::floor(lo[0] / STEP)), u1 = static_cast<int>(std::ceil(hi[0] / STEP)), v0 = static_cast<int>(std::floor(lo[1] / STEP)),
							v1 = static_cast<int>(std::ceil(hi[1] / STEP));
						const int cols = u1 - u0, rows = v1 - v0;
						if (cols < 1 || rows < 1 || static_cast<std::int64_t>(cols) * rows > 1 << 20)
						{
							continue;
						}
						// the light of every grid square whose centre lies on the face: -1 none sampled, UNLIT: no primary
						// light reaches it, 0: the sun wins, from 2: that light entity
						constexpr int UNLIT = -2;
						std::vector<int> best(static_cast<std::size_t>(cols) * rows, -1);
						const auto inside = [&](const double u, const double v)
						{
							int sign = 0;
							for (std::size_t k = 0; k < polygon.size(); k++)
							{
								const vec3 a = at(k), b = at((k + 1) % polygon.size());
								const double ax = dot(a, u_axis), ay = dot(a, v_axis), bx = dot(b, u_axis), by = dot(b, v_axis);
								const double side = (bx - ax) * (v - ay) - (by - ay) * (u - ax);
								if (std::fabs(side) < 1e-6)
								{
									continue;
								}
								if (sign && (side > 0) != (sign > 0))
								{
									return false;
								}
								sign = side > 0 ? 1 : -1;
							}
							return true;
						};
						std::set<int> winners;
						for (int r = 0; r < rows; r++)
						{
							for (int c = 0; c < cols; c++)
							{
								const double u = (u0 + c + 0.5) * STEP, v = (v0 + r + 0.5) * STEP;
								if (!inside(u, v))
								{
									continue;
								}
								const vec3 p = origin + u_axis * u + v_axis * v + normal * 1.0;
								int winner = 0;
								double most = 0;
								float e[3];
								if (luminance(w.sun_color) > 0 && primary_reach(1, p, normal, e) && primary_visible(1, p, false))
								{
									most = luminance(e);
								}
								for (const int l : near)
								{
									if (primary_reach(l, p, normal, e) && luminance(e) > most && primary_visible(l, p, false))
									{
										most = luminance(e);
										winner = l;
									}
								}
								if (most == 0 && !movable_lights.empty())
								{
									winner = potential_light(p, &normal);
								}
								if (winner == 0 && most == 0)
								{
									winner = UNLIT;		// no primary light reaches the square
								}
								best[static_cast<std::size_t>(r) * cols + c] = winner;
								winners.insert(winner);
							}
						}
						if (winners.empty())
						{
							// a face smaller than the grid: its middle
							vec3 middle{ 0, 0, 0 };
							for (std::size_t k = 0; k < polygon.size(); k++)
							{
								middle = middle + at(k) * (1.0 / static_cast<double>(polygon.size()));
							}
							const vec3 p = middle + normal * 1.0;
							int winner = 0;
							double most = 0;
							float e[3];
							if (luminance(w.sun_color) > 0 && primary_reach(1, p, normal, e) && primary_visible(1, p, false))
							{
								most = luminance(e);
							}
							for (const int l : near)
							{
								if (primary_reach(l, p, normal, e) && luminance(e) > most && primary_visible(l, p, false))
								{
									most = luminance(e);
									winner = l;
								}
							}
							if (most == 0 && !movable_lights.empty())
							{
								winner = potential_light(p, &normal);
							}
							winners.insert(winner);
						}
						if (winners.size() == 1)
						{
							// the whole face goes to one light ( or to none of the entities )
							pieces[si][fi].push_back({ polygon, std::max(*winners.begin(), 0) });
							continue;
						}
						// ---- the cut. Two steps:
						// 1. Where different lights win ( the sun, light entities ), the face is divided between them: a
						//    block of the grid in which more than one of them wins squares is split along the grid line
						//    that separates them best, down to blocks of two squares; squares nothing reaches do not count.
						//    ( Until the seventh round the face was cut once around all a light wins and each block went to
						//    whatever had the most squares in it - unlit ones included: a floor under several rooms, lit
						//    in one of them, went to "nothing" as a whole and was not lit at run time at all. )
						// 2. A block that is one light entity's is cut around what the light wins in it; the rest of the
						//    block names no light entity ( in the shipped maps 88 to 96 % of the triangles of a spot's
						//    surfaces reach into its radius ).
						std::vector<int> kinds;		// the lights that win squares: 0 the sun, from 2 the light entities
						for (const int l : winners)
						{
							if (l >= 0)
							{
								kinds.push_back(l);
							}
						}
						// per kind the number of its squares left of and above a grid point
						const std::size_t stride = static_cast<std::size_t>(cols) + 1;
						std::vector<std::vector<int>> sums(kinds.size(), std::vector<int>(stride * (static_cast<std::size_t>(rows) + 1), 0));
						for (std::size_t k = 0; k < kinds.size(); k++)
						{
							for (int r = 0; r < rows; r++)
							{
								for (int c = 0; c < cols; c++)
								{
									sums[k][(static_cast<std::size_t>(r) + 1) * stride + c + 1] = (best[static_cast<std::size_t>(r) * cols + c] == kinds[k])
										+ sums[k][static_cast<std::size_t>(r) * stride + c + 1] + sums[k][(static_cast<std::size_t>(r) + 1) * stride + c]
										- sums[k][static_cast<std::size_t>(r) * stride + c];
								}
							}
						}
						struct block
						{
							int c0, c1, r0, r1;		// squares c0 .. c1 - 1, r0 .. r1 - 1
						};
						const auto count = [&](const std::size_t k, const block& b)
						{
							return sums[k][static_cast<std::size_t>(b.r1) * stride + b.c1] - sums[k][static_cast<std::size_t>(b.r0) * stride + b.c1]
								- sums[k][static_cast<std::size_t>(b.r1) * stride + b.c0] + sums[k][static_cast<std::size_t>(b.r0) * stride + b.c0];
						};
						// the squares of a block that are not its foremost light's, and that light ( -1: nothing wins in it )
						const auto minority = [&](const block& b, int* top = nullptr)
						{
							int all = 0, most = 0, which = -1;
							for (std::size_t k = 0; k < kinds.size(); k++)
							{
								const int n = count(k, b);
								all += n;
								if (n > most)
								{
									most = n;
									which = kinds[k];
								}
							}
							if (top)
							{
								*top = which;
							}
							return all - most;
						};
						std::vector<std::pair<block, int>> leaves;		// with the light of the block; -1: none
						std::vector<block> open{ { 0, cols, 0, rows } };
						while (!open.empty())
						{
							const block b = open.back();
							open.pop_back();
							int top = -1;
							const int mixed = minority(b, &top);
							const int wide = b.c1 - b.c0, high = b.r1 - b.r0;
							if (mixed <= 2 || (wide <= 2 && high <= 2) || leaves.size() + open.size() >= 400)
							{
								leaves.emplace_back(b, top);
								continue;
							}
							// the grid line that leaves the fewest squares on the wrong side; of equals the one nearest the middle
							int best_mixed = mixed + 1, best_off = 1 << 30, cut_at = 0;
							bool cut_u = true;
							for (int pass = 0; pass < 2; pass++)
							{
								const bool along_u = pass == 0;
								const int from = along_u ? b.c0 : b.r0, to = along_u ? b.c1 : b.r1;
								for (int line = from + 1; line < to; line++)
								{
									const block first = along_u ? block{ b.c0, line, b.r0, b.r1 } : block{ b.c0, b.c1, b.r0, line };
									const block second = along_u ? block{ line, b.c1, b.r0, b.r1 } : block{ b.c0, b.c1, line, b.r1 };
									const int m = minority(first) + minority(second), off = std::abs(2 * line - from - to);
									if (m < best_mixed || (m == best_mixed && off < best_off))
									{
										best_mixed = m;
										best_off = off;
										cut_at = line;
										cut_u = along_u;
									}
								}
							}
							if (best_mixed >= mixed)
							{
								// no line helps ( lights in a chequer ): halve the longer side
								cut_u = wide >= high;
								cut_at = cut_u ? (b.c0 + b.c1) / 2 : (b.r0 + b.r1) / 2;
							}
							if (cut_u)
							{
								open.push_back({ b.c0, cut_at, b.r0, b.r1 });
								open.push_back({ cut_at, b.c1, b.r0, b.r1 });
							}
							else
							{
								open.push_back({ b.c0, b.c1, b.r0, cut_at });
								open.push_back({ b.c0, b.c1, cut_at, b.r1 });
							}
						}
						// the piece of the face inside a block; the grid's outermost lines are the face's own edges
						const auto add_piece = [&](const block& b, const int l)
						{
							if (b.c0 >= b.c1 || b.r0 >= b.r1)
							{
								return;
							}
							std::vector<c1::GfxWorldVertex> part = polygon;
							if (b.c0 > 0)
							{
								part = clip_polygon(part, u_axis, (u0 + b.c0) * STEP, 1.0);
							}
							if (b.c1 < cols)
							{
								part = clip_polygon(part, u_axis, (u0 + b.c1) * STEP, -1.0);
							}
							if (b.r0 > 0)
							{
								part = clip_polygon(part, v_axis, (v0 + b.r0) * STEP, 1.0);
							}
							if (b.r1 < rows)
							{
								part = clip_polygon(part, v_axis, (v0 + b.r1) * STEP, -1.0);
							}
							if (part.size() >= 3 && polygon_area(part) > 0.01)
							{
								pieces[si][fi].push_back({ std::move(part), l });
							}
						};
						for (const auto& [b, l] : leaves)
						{
							if (l < 2)
							{
								add_piece(b, 0);		// the sun's, or nothing's
								continue;
							}
							// around what the light entity wins in the block
							block own{ b.c1, b.c0, b.r1, b.r0 };
							for (int r = b.r0; r < b.r1; r++)
							{
								for (int c = b.c0; c < b.c1; c++)
								{
									if (best[static_cast<std::size_t>(r) * cols + c] == l)
									{
										own.c0 = std::min(own.c0, c);
										own.c1 = std::max(own.c1, c + 1);
										own.r0 = std::min(own.r0, r);
										own.r1 = std::max(own.r1, r + 1);
									}
								}
							}
							add_piece(own, l);
							add_piece({ b.c0, b.c1, b.r0, own.r0 }, 0);
							add_piece({ b.c0, b.c1, own.r1, b.r1 }, 0);
							add_piece({ b.c0, own.c0, own.r0, own.r1 }, 0);
							add_piece({ own.c1, b.c1, own.r0, own.r1 }, 0);
						}
						// pieces of one light that lie side by side along a whole side become one ( fewer vertices )
						for (bool merged = pieces[si][fi].size() <= 128; merged;)
						{
							merged = false;
							std::vector<piece>& all = pieces[si][fi];
							for (std::size_t i = 0; i < all.size() && !merged; i++)
							{
								for (std::size_t j = i + 1; j < all.size() && !merged; j++)
								{
									if (all[i].light != all[j].light || all[i].vertices.size() != 4 || all[j].vertices.size() != 4)
									{
										continue;
									}
									double lo_i[2] = { 1e30, 1e30 }, hi_i[2] = { -1e30, -1e30 }, lo_j[2] = { 1e30, 1e30 }, hi_j[2] = { -1e30, -1e30 };
									const auto range = [&](const piece& p, double lo2[2], double hi2[2])
									{
										bool axial = true;
										for (const c1::GfxWorldVertex& v : p.vertices)
										{
											const vec3 q{ v.xyz[0], v.xyz[1], v.xyz[2] };
											const double uv[2] = { dot(q, u_axis), dot(q, v_axis) };
											for (int k = 0; k < 2; k++)
											{
												lo2[k] = std::min(lo2[k], uv[k]);
												hi2[k] = std::max(hi2[k], uv[k]);
											}
										}
										for (const c1::GfxWorldVertex& v : p.vertices)
										{
											const vec3 q{ v.xyz[0], v.xyz[1], v.xyz[2] };
											const double uv[2] = { dot(q, u_axis), dot(q, v_axis) };
											axial = axial && (std::fabs(uv[0] - lo2[0]) < 0.01 || std::fabs(uv[0] - hi2[0]) < 0.01) && (std::fabs(uv[1] - lo2[1]) < 0.01 || std::fabs(uv[1] - hi2[1]) < 0.01);
										}
										return axial;		// a rectangle along the grid
									};
									if (!range(all[i], lo_i, hi_i) || !range(all[j], lo_j, hi_j))
									{
										continue;
									}
									for (int k = 0; k < 2 && !merged; k++)
									{
										const int o = 1 - k;
										const bool same_side = std::fabs(lo_i[o] - lo_j[o]) < 0.01 && std::fabs(hi_i[o] - hi_j[o]) < 0.01;
										const bool touch = std::fabs(hi_i[k] - lo_j[k]) < 0.01 || std::fabs(hi_j[k] - lo_i[k]) < 0.01;
										if (same_side && touch)
										{
											// the rectangle around both, with the vertices of the first as the pattern
											const double lo2[2] = { std::min(lo_i[0], lo_j[0]), std::min(lo_i[1], lo_j[1]) }, hi2[2] = { std::max(hi_i[0], hi_j[0]), std::max(hi_i[1], hi_j[1]) };
											std::vector<c1::GfxWorldVertex> part = polygon;
											part = clip_polygon(part, u_axis, lo2[0], 1.0);
											part = clip_polygon(part, u_axis, hi2[0], -1.0);
											part = clip_polygon(part, v_axis, lo2[1], 1.0);
											part = clip_polygon(part, v_axis, hi2[1], -1.0);
											if (part.size() == 4)
											{
												all[i].vertices = std::move(part);
												all.erase(all.begin() + static_cast<std::ptrdiff_t>(j));
												merged = true;
											}
										}
									}
								}
							}
						}
						if (pieces[si][fi].size() > 1)
						{
							cut_faces++;
							made += pieces[si][fi].size();
						}
					}
				});
				// the surfaces again: what a surface holds goes to one surface per light, in the surface's place
				std::vector<render_surface> surfaces;
				std::size_t before = rw.surfaces.size();
				for (std::size_t si = 0; si < rw.surfaces.size(); si++)
				{
					render_surface& surface = rw.surfaces[si];
					std::set<int> lights;
					bool whole = true;
					for (const std::vector<piece>& of_face : pieces[si])
					{
						// a face no light entity is near goes with the pieces of "none"
						if (of_face.empty())
						{
							lights.insert(0);
						}
						for (const piece& p : of_face)
						{
							lights.insert(p.light);
						}
						whole = whole && of_face.size() <= 1;
					}
					whole = whole && lights.size() <= 1;
					if (whole)
					{
						surface.light_hint = surface.lightmap == 31 || lights.empty() ? -1 : *lights.begin();
						surfaces.push_back(std::move(surface));
						continue;
					}
					for (const int l : lights)
					{
						render_surface out;
						out.material = surface.material;
						out.cell = surface.cell;
						out.model = surface.model;
						out.lightmap = surface.lightmap;
						out.reflection_probe = surface.reflection_probe;
						out.primary_light = surface.primary_light;
						out.casts_sun_shadow = surface.casts_sun_shadow;
						out.sky = surface.sky;
						out.light_hint = l;
						for (std::size_t fi = 0; fi < surface.faces.size(); fi++)
						{
							const render_face& rf = surface.faces[fi];
							std::vector<piece> whole_face;
							if (pieces[si][fi].empty())
							{
								whole_face.push_back({ std::vector<c1::GfxWorldVertex>(surface.vertices.begin() + rf.first_vertex, surface.vertices.begin() + rf.first_vertex + rf.vertex_count), 0 });
							}
							for (const piece& p : pieces[si][fi].empty() ? whole_face : pieces[si][fi])
							{
								if (p.light != l)
								{
									continue;
								}
								render_face part = rf;
								part.first_vertex = static_cast<std::uint32_t>(out.vertices.size());
								part.vertex_count = static_cast<std::uint32_t>(p.vertices.size());
								for (const c1::GfxWorldVertex& v : p.vertices)
								{
									out.vertices.push_back(v);
									out.box.add(v.xyz);
								}
								for (std::uint32_t k = 2; k < part.vertex_count; k++)
								{
									out.indices.push_back(static_cast<std::uint16_t>(part.first_vertex));
									out.indices.push_back(static_cast<std::uint16_t>(part.first_vertex + k));
									out.indices.push_back(static_cast<std::uint16_t>(part.first_vertex + k - 1));
								}
								out.faces.push_back(part);
							}
						}
						if (!out.faces.empty() && out.vertices.size() <= 0xFFFF)
						{
							surfaces.push_back(std::move(out));
						}
					}
				}
				rw.surfaces = std::move(surfaces);
				// what was counted in surfaces: the models' ranges, the ends of the draw classes, the sky's list
				for (render_model& model : rw.models)
				{
					model.first_surface = 0;
					model.surface_count = 0;
				}
				rw.sky_surfaces.clear();
				rw.lit_opaque_end = rw.lit_trans_end = 0;
				for (std::size_t si = 0; si < rw.surfaces.size(); si++)
				{
					const render_surface& surface = rw.surfaces[si];
					render_model& model = rw.models[static_cast<std::size_t>(surface.model)];
					if (model.surface_count == 0)
					{
						model.first_surface = static_cast<std::uint32_t>(si);
					}
					model.surface_count++;
					if (surface.model == 0)
					{
						(rw.materials[static_cast<std::size_t>(surface.material)].draw_class == 0 ? rw.lit_opaque_end : rw.lit_trans_end) = static_cast<std::uint32_t>(si + 1);
						if (surface.sky)
						{
							rw.sky_surfaces.push_back(static_cast<std::int32_t>(si));
						}
					}
				}
				rw.lit_trans_end = std::max(rw.lit_trans_end, rw.lit_opaque_end);
				ctx.rep.notes.push_back(format("light: %zu faces cut by the reach of the primary lights into %zu pieces; %zu surfaces became %zu", cut_faces.load(), made.load(), before,
					rw.surfaces.size()));
			}

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
					c.first_vertex = rf.first_vertex;
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
					chart_of_face[face_key(rf.brush, rf.face)].push_back(charts.size());
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
			ctx.rep.notes.push_back(format("light: %zu faces in %d lightmap page( s ), %zu luxels of %.1f units; %zu light entities ( %zu primary ), %d sky directions, %d bounce( s ) of %d rays",
				charts.size(), page_count, luxel_count, luxel, setup.points.size(), light_count - 2, direction_count, bounces, bounce_rays));
			// self check: no two charts share a luxel, none lies in the row of the falloff curves
			{
				std::vector<std::uint8_t> used(static_cast<std::size_t>(page_count) * LM_W * LM_H, 0);
				bool overlap = false;
				for (const chart& c : charts)
				{
					overlap = overlap || c.y < LM_FIRST_ROW;
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
					lightmap.secondary[k] = 128;		// no direction: along the normal
				}
			}
			direct.assign(luxel_count * 3, 0.0f);
			indirect.assign(luxel_count * 3, 0.0f);
			primary_part.assign(luxel_count * 3, 0.0f);
			came_from.assign(luxel_count * 3, 0.0f);
			const auto position = [&](const chart& c, const double lx, const double ly)
			{
				// lx, ly: luxels from the chart's corner; the padding repeats the edge of the face
				const double u = std::clamp(c.u_min + (lx - 1.0) * luxel, c.u_min, c.u_max), v = std::clamp(c.v_min + (ly - 1.0) * luxel, c.v_min, c.v_max);
				return c.origin + c.u_axis * u + c.v_axis * v + c.normal * 1.0;
			};

			// ---- the primary light of every surface: the one that gives it the most light. A surface names one; what
			// the others send to it is baked into its lightmap like the light of any light entity.
			std::vector<int> surface_light(rw.surfaces.size(), 0);
			{
				std::vector<double> reach(charts.size() * light_count, 0.0);
				parallel_for(charts.size(), [&](const std::size_t index)
				{
					const chart& c = charts[index];
					for (int y = 0; y < c.h; y++)
					{
						for (int x = 0; x < c.w; x++)
						{
							const vec3 p = position(c, x + 0.5, y + 0.5);
							for (std::size_t l = 1; l < light_count; l++)
							{
								float e[3];
								if (primary_reach(static_cast<int>(l), p, c.normal, e) && luminance(e) > 0 && primary_visible(static_cast<int>(l), p, false))
								{
									reach[index * light_count + l] += luminance(e);
								}
							}
						}
					}
				});
				for (std::size_t si = 0; si < rw.surfaces.size(); si++)
				{
					double most = 0;
					const int hint = rw.surfaces[si].light_hint;
					for (std::size_t l = 1; l < light_count; l++)
					{
						// a surface that was cut out for a light entity names it; one cut out for none of them the sun or nothing
						if ((hint >= 2 && static_cast<int>(l) != hint) || (hint == 0 && l != 1))
						{
							continue;
						}
						double sum = 0;
						for (const std::size_t ci : of_surface[si])
						{
							sum += reach[ci * light_count + l];
						}
						if (sum > most)
						{
							most = sum;
							surface_light[si] = static_cast<int>(l);
						}
					}
					if (most == 0 && hint != 0 && !movable_lights.empty())
					{
						// nothing lights the surface as compiled: the movable light that could ( the one it was cut out for )
						std::vector<int> votes(light_count, 0);
						for (const std::size_t ci : of_surface[si])
						{
							const chart& c = charts[ci];
							const int l = potential_light(position(c, c.w * 0.5, c.h * 0.5), &c.normal);
							votes[static_cast<std::size_t>(l)] += l >= 2 && (hint < 2 || l == hint);
						}
						const auto top = std::max_element(votes.begin() + 2, votes.end());
						if (top != votes.end() && *top > 0)
						{
							surface_light[si] = static_cast<int>(top - votes.begin());
						}
					}
					named_surfaces[static_cast<std::size_t>(surface_light[si])]++;
				}
			}

			// ---- direct light per luxel: ambient, sky and every light but the surface's primary one; the primary light's
			// visibility per primary texel ( 2 x 2 per luxel )
			std::vector<std::uint8_t> primary_lit(charts.size(), 0);
			// per chart and cell: a face that several cells see is drawn by each of them, whole - what a light lights of
			// it belongs to the hull of the cell the luxel lies in, not of the cell that draws the copy
			std::vector<std::map<int, kdop>> chart_lit(charts.size());
			parallel_for(charts.size(), [&](const std::size_t index)
			{
				const chart& c = charts[index];
				const int light = surface_light[static_cast<std::size_t>(c.surface)];
				std::vector<std::uint8_t>& primary = rw.lightmaps[static_cast<std::size_t>(c.page)].primary;
				const vec3 origin = light >= 2 ? light_origin(light) : vec3{ 0, 0, 0 };
				for (int y = 0; y < c.h; y++)
				{
					for (int x = 0; x < c.w; x++)
					{
						const std::size_t at = c.first + static_cast<std::size_t>(y) * static_cast<std::size_t>(c.w) + static_cast<std::size_t>(x);
						const vec3 p = position(c, x + 0.5, y + 0.5);
						arriving a;
						gather(setup, p, &c.normal, light, a);
						for (int k = 0; k < 3; k++)
						{
							direct[at * 3 + static_cast<std::size_t>(k)] = a.e[k];
							came_from[at * 3 + static_cast<std::size_t>(k)] = static_cast<float>(a.dir[k]);
						}
						if (light == 0)
						{
							continue;
						}
						int lit_texels = 0;
						for (int sy = 0; sy < 2; sy++)
						{
							for (int sx = 0; sx < 2; sx++)
							{
								// a texel is lit when it faces the light and sees it ( the light's reach is the shader's business )
								const vec3 q = position(c, x + 0.25 + 0.5 * sx, y + 0.25 + 0.5 * sy);
								const double facing = light == 1 ? dot(c.normal, setup.sun) : dot(c.normal, origin - q);
								const bool seen = facing > 0.01 && primary_visible(light, q, false);
								primary[(static_cast<std::size_t>(c.y + y) * 2 + static_cast<std::size_t>(sy)) * (LM_W * 2) + static_cast<std::size_t>(c.x + x) * 2
									+ static_cast<std::size_t>(sx)] = seen ? 255 : 0;
								lit_texels += seen;
							}
						}
						float e[3];
						if (lit_texels && primary_reach(light, p, c.normal, e))
						{
							primary_lit[index] = 1;
							for (int k = 0; k < 3; k++)
							{
								primary_part[at * 3 + static_cast<std::size_t>(k)] = e[k] * static_cast<float>(lit_texels) * 0.25f;
							}
							if (light >= 2)
							{
								chart_lit[index][cell_of(p)].add(p - origin, light_axes[static_cast<std::size_t>(light)]);
							}
						}
						else if (lit_texels && light >= 2 && movable(w.lights[static_cast<std::size_t>(light)])
							&& potential_reach(setup, w.lights[static_cast<std::size_t>(light)], p) > 0)
						{
							// what a moved or turned light could light is in its region
							chart_lit[index][cell_of(p)].add(p - origin, light_axes[static_cast<std::size_t>(light)]);
						}
					}
				}
			});
			for (std::size_t ci = 0; ci < charts.size(); ci++)
			{
				const int light = surface_light[static_cast<std::size_t>(charts[ci].surface)];
				for (const auto& [cell, hull] : chart_lit[ci])
				{
					lit[static_cast<std::size_t>(light)][cell].add(hull);
				}
			}
			// bounced light: every luxel looks around ( cosine weighted directions over its side ) and takes what the
			// luxels it sees throw back - in the colour of its material
			if (ctx.opts.light)
			{
				read_material_colours(ctx, rw);
			}
			std::vector<float> reflect(luxel_count * 3, REFLECT);
			for (const chart& c : charts)
			{
				const render_material& material = rw.materials[static_cast<std::size_t>(rw.surfaces[static_cast<std::size_t>(c.surface)].material)];
				for (std::size_t at = c.first; at < c.first + static_cast<std::size_t>(c.w) * static_cast<std::size_t>(c.h) && at < luxel_count; at++)
				{
					for (std::size_t k = 0; k < 3; k++)
					{
						reflect[at * 3 + k] = std::clamp(2.0f * w.radiosity_scale * material.albedo[k], 0.0f, 0.95f);
					}
				}
			}
			emit.assign(luxel_count * 3, 0.0f);
			for (std::size_t k = 0; k < luxel_count * 3; k++)
			{
				emit[k] = reflect[k] * (direct[k] + primary_part[k]);
			}
			std::vector<vec3> hemisphere;
			for (int k = 0; k < bounce_rays; k++)
			{
				const double r = std::sqrt((k + 0.5) / bounce_rays), a = k * PI * (3.0 - std::sqrt(5.0));
				hemisphere.push_back({ r * std::cos(a), r * std::sin(a), std::sqrt(std::max(0.0, 1.0 - r * r)) });
			}
			for (int bounce = 0; bounce < bounces; bounce++)
			{
				std::vector<float> gathered(luxel_count * 3, 0.0f), gathered_from(luxel_count * 3, 0.0f);
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
							vec3 sum_from{ 0, 0, 0 };
							for (const vec3& h : hemisphere)
							{
								const double hx = h.x * ct - h.y * st, hy = h.x * st + h.y * ct;
								const vec3 d = c.u_axis * hx + c.v_axis * hy + c.normal * h.z;
								const ray_world::sight s = rays.look(p, d);
								if (s.what != ray_world::seen::face)
								{
									continue;
								}
								const std::size_t source = luxel_at(s.brush, s.face, p + d * s.t);
								if (source != npos)
								{
									sum[0] += emit[source * 3];
									sum[1] += emit[source * 3 + 1];
									sum[2] += emit[source * 3 + 2];
									sum_from = sum_from + d * luminance(&emit[source * 3]);
								}
							}
							for (int ch = 0; ch < 3; ch++)
							{
								gathered[at * 3 + static_cast<std::size_t>(ch)] = static_cast<float>(sum[ch] / bounce_rays);
								gathered_from[at * 3 + static_cast<std::size_t>(ch)] = static_cast<float>(sum_from[ch] / bounce_rays);
							}
						}
					}
				});
				for (std::size_t k = 0; k < gathered.size(); k++)
				{
					indirect[k] += gathered[k];
					came_from[k] += gathered_from[k];
					emit[k] = reflect[k] * gathered[k];
				}
			}
			// what every luxel throws back in the end
			for (std::size_t k = 0; k < luxel_count * 3; k++)
			{
				emit[k] = reflect[k] * (direct[k] + indirect[k] + primary_part[k]);
			}
			// ---- the two layers of the secondary lightmap: light without a direction above, light from the direction the
			// two alpha channels give below, as square roots that add up to the square root of what arrives
			for (const chart& c : charts)
			{
				std::vector<std::uint8_t>& secondary = rw.lightmaps[static_cast<std::size_t>(c.page)].secondary;
				// the frame the shader has the direction in: the tangent of the face's vertices, cross( normal, tangent ) x
				// the binormal sign, the normal
				const c1::GfxWorldVertex& vertex = rw.surfaces[static_cast<std::size_t>(c.surface)].vertices[c.first_vertex];
				float unpacked[3];
				zone::unpack_unit_vector(vertex.tangent, unpacked);
				vec3 tangent{ unpacked[0], unpacked[1], unpacked[2] };
				if (length(tangent - c.normal * dot(tangent, c.normal)) < 1e-3)
				{
					tangent = c.u_axis;
				}
				const vec3 in_plane = tangent - c.normal * dot(tangent, c.normal);
				const vec3 binormal = cross(c.normal, tangent) * (vertex.binormalSign < 0 ? -1.0 : 1.0);
				const double tangent_up = dot(tangent, c.normal);
				for (int y = 0; y < c.h; y++)
				{
					for (int x = 0; x < c.w; x++)
					{
						const std::size_t at = (c.first + static_cast<std::size_t>(y) * static_cast<std::size_t>(c.w) + static_cast<std::size_t>(x)) * 3;
						float e[3];
						for (int k = 0; k < 3; k++)
						{
							e[k] = direct[at + static_cast<std::size_t>(k)] + indirect[at + static_cast<std::size_t>(k)];
						}
						const double lum = luminance(e);
						vec3 towards{ came_from[at], came_from[at + 1], came_from[at + 2] };
						double share = 0, dx = 0, dy = 0;
						if (lum > 1e-6 && length(towards) > 1e-9)
						{
							// how much of the light comes from one side: 2 / 3 for light from all over, 1 for one direction
							const double concentration = std::min(1.0, length(towards) / lum);
							share = std::clamp((concentration - 2.0 / 3.0) * 3.0, 0.0, 1.0) * 0.85;
							towards = towards * (1.0 / length(towards));
							double up = dot(towards, c.normal);
							if (up < 0.35)
							{
								// not along the surface or from behind it
								towards = towards + c.normal * (0.35 - up) * 1.5;
								towards = towards * (1.0 / length(towards));
								up = dot(towards, c.normal);
							}
							const vec3 flat = towards - c.normal * up;
							const double a = dot(flat, in_plane) / dot(in_plane, in_plane), b = dot(flat, binormal) / std::max(dot(binormal, binormal), 1e-9);
							const double along = std::max(up - a * tangent_up, 0.05);
							dx = std::clamp(a / along, -2.0, 2.0);
							dy = std::clamp(b / along, -2.0, 2.0);
						}
						const double cosine = 1.0 / std::sqrt(dx * dx + dy * dy + 1.0);		// of the direction and a flat normal, as the shader has it
						std::uint8_t* upper = secondary.data() + (static_cast<std::size_t>(c.y + y) * LM_W + static_cast<std::size_t>(c.x + x)) * 4;
						std::uint8_t* lower = upper + static_cast<std::size_t>(LM_W) * LM_H * 4;
						for (int k = 0; k < 3; k++)
						{
							const double root = std::sqrt(std::clamp(static_cast<double>(e[k]), 0.0, 1.0));
							const double directed = std::min(root * share / cosine, 1.0);
							upper[2 - k] = to_byte(static_cast<float>(root - directed * cosine));
							lower[2 - k] = to_byte(static_cast<float>(directed));
						}
						upper[3] = to_byte(static_cast<float>((dx + 2.08) / 4.08));
						lower[3] = to_byte(static_cast<float>((dy + 2.06451607) / 4.06451607));
					}
				}
			}
			// soften the primary light's edge: a 3 x 3 box filter inside every chart
			{
				std::vector<std::vector<std::uint8_t>> sharp;
				for (const render_lightmap& lightmap : rw.lightmaps)
				{
					sharp.push_back(lightmap.primary);
				}
				const int pw = LM_W * 2;
				for (const chart& c : charts)
				{
					const std::vector<std::uint8_t>& source = sharp[static_cast<std::size_t>(c.page)];
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
										sum += source[static_cast<std::size_t>(yy) * pw + static_cast<std::size_t>(xx)];
										n++;
									}
								}
							}
							primary[static_cast<std::size_t>(y) * pw + static_cast<std::size_t>(x)] = static_cast<std::uint8_t>(sum / n);
						}
					}
				}
			}
			{
				std::size_t sun_faces = 0, light_faces = 0;
				for (std::size_t ci = 0; ci < charts.size(); ci++)
				{
					const int light = surface_light[static_cast<std::size_t>(charts[ci].surface)];
					sun_faces += primary_lit[ci] && light == 1;
					light_faces += primary_lit[ci] && light >= 2;
				}
				ctx.rep.notes.push_back(format("light: the sun reaches %zu of %zu faces, primary light entities %zu", sun_faces, charts.size(), light_faces));
			}

			// lightmap page, coordinates and primary light of the surfaces
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
				std::size_t next = 0;
				surface.lightmap = charts[of_surface[si][0]].page;
				for (const render_face& rf : surface.faces)
				{
					// the charts of a surface are in the order of its faces ( a face without area has none )
					if (next >= of_surface[si].size() || charts[of_surface[si][next]].first_vertex != rf.first_vertex)
					{
						continue;
					}
					const std::size_t ci = of_surface[si][next++];
					const chart& c = charts[ci];
					for (std::uint32_t k = 0; k < rf.vertex_count; k++)
					{
						c1::GfxWorldVertex& v = surface.vertices[rf.first_vertex + k];
						const vec3 p{ v.xyz[0], v.xyz[1], v.xyz[2] };
						const double lx = 1.0 + (dot(p - c.origin, c.u_axis) - c.u_min) / luxel, ly = 1.0 + (dot(p - c.origin, c.v_axis) - c.v_min) / luxel;
						v.lmapCoord[0] = static_cast<float>((c.x + lx) / LM_W);
						v.lmapCoord[1] = static_cast<float>((c.y + ly) / LM_H);
					}
				}
				surface.primary_light = surface_light[si];
			}
		}
		// ---- the falloff curve of the light definition: row 0 of the upper layer of every page, 32 texels from texel
		// 1 ( GfxLightDef::lmapLookupStart ), with a copy of the end texel on either side for the bilinear filter
		// Every definition a primary light names has its strip, where GfxLightDef::lmapLookupStart says.
		{
			std::set<int> strips;
			for (std::size_t l = 2; l < light_count; l++)
			{
				strips.insert(w.lights[l].def_index);
			}
			if (strips.empty())
			{
				strips.insert(0);
			}
			for (render_lightmap& lightmap : rw.lightmaps)
			{
				for (const int d : strips)
				{
					if (d < 0 || static_cast<std::size_t>(d) >= w.light_defs.size())
					{
						continue;
					}
					const light_def& def = w.light_defs[static_cast<std::size_t>(d)];
					for (int x = 0; x < 34 && def.lookup_start - 1 + x < LM_W; x++)
					{
						const std::array<float, 3>& v = def.curve[static_cast<std::size_t>(std::clamp(x - 1, 0, 31))];
						std::uint8_t* texel = lightmap.secondary.data() + static_cast<std::size_t>(def.lookup_start - 1 + x) * 4;
						texel[0] = to_byte(v[2]);
						texel[1] = to_byte(v[1]);
						texel[2] = to_byte(v[0]);
						texel[3] = 255;
					}
				}
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
			h.hasLightRegions = 1;			// as every shipped map ( the regions themselves are empty: only suns )
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
					// the primary light of the point is added at run time; every other light that reaches it goes into its
					// colours. A light from one direction, as average and lean: c max( 0, n . l ) ~ c ( 1 / 4 + n . l / 2 )
					s.light = static_cast<std::uint8_t>(primary_at(p));
					const auto add = [&](const float color[3], const double scale, const vec3& l)
					{
						for (int k = 0; k < 3; k++)
						{
							s.m0[k] += static_cast<float>(color[k] * scale * 0.25);
							for (int a = 0; a < 3; a++)
							{
								s.m1[k][a] += static_cast<float>(color[k] * scale * 0.5 * l[a]);
							}
						}
					};
					for (std::size_t i = 0; i < setup.points.size(); i++)
					{
						if (s.light > 0 && setup.primary_of[i] == s.light)
						{
							continue;
						}
						const map_light& light = *setup.points[i];
						vec3 l;
						double distance = 0;
						float tint[3];
						const double scale = light_reach(setup, light, p, l, distance, tint);
						if (scale > 0 && !rays.blocked(p, vec3{ light.origin[0], light.origin[1], light.origin[2] }, true))
						{
							const float color[3] = { light.color[0] * tint[0], light.color[1] * tint[1], light.color[2] * tint[2] };
							add(color, scale, l);
						}
					}
					if (s.light != 1 && luminance(w.sun_color) > 0 && rays.reaches_sky(p, setup.sun, true))
					{
						add(w.sun_color, 1.0, setup.sun);
					}
				}
			});
			// the grid points a primary light entity lights belong to its region
			for (std::size_t rc = 0; rc < static_cast<std::size_t>(rows) * cols; rc++)
			{
				for (int z = 0; z < layers; z++)
				{
					const sample& s = samples[rc * static_cast<std::size_t>(layers) + static_cast<std::size_t>(z)];
					if (!s.solid && s.light >= 2)
					{
						const vec3 p{ grid_position(mins[0] + static_cast<int>(rc / static_cast<std::size_t>(cols)), 0),
							grid_position(mins[1] + static_cast<int>(rc % static_cast<std::size_t>(cols)), 1), grid_position(mins[2] + z, 2) };
						lit[s.light][cell_of(p)].add(p - light_origin(s.light), light_axes[s.light]);
					}
				}
			}
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
							colors.rgb[d][k] = static_cast<std::uint8_t>(grid_byte(static_cast<float>(v)) & mask);
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
				arriving around;
				gather(setup, p, nullptr, 0, around);
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
									c[k] = REFLECT * around.e[k];		// a static model: lit like the probe's own place
								}
							}
							std::memcpy(&level[(static_cast<std::size_t>(y) * 64 + static_cast<std::size_t>(x)) * 3], c, sizeof(c));
						}
					}
					for (int size = 64; size >= 1; size /= 2)
					{
						for (int k = 0; k < size * size; k++)
						{
							probe.pixels.push_back(root_byte(level[static_cast<std::size_t>(k) * 3 + 2]));
							probe.pixels.push_back(root_byte(level[static_cast<std::size_t>(k) * 3 + 1]));
							probe.pixels.push_back(root_byte(level[static_cast<std::size_t>(k) * 3]));
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
			s.primary_light = !ctx.opts.light ? 1 : primary_at(centre);
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

		// ---- per primary light: its shadow geometry - the surfaces that name it; the static models that do are added
		// by the writer - and its region: the convex hull ( as the ranges along nine axes ) of everything it lights.
		// The suns have neither, as in the shipped maps.
		rw.light_surfaces.assign(light_count, {});
		rw.light_regions.assign(light_count, {});
		for (std::size_t si = 0; si < rw.surfaces.size(); si++)
		{
			const render_surface& surface = rw.surfaces[si];
			if (surface.model == 0 && surface.primary_light >= 2 && surface.casts_sun_shadow && si <= 0xFFFF)
			{
				rw.light_surfaces[static_cast<std::size_t>(surface.primary_light)].push_back(static_cast<std::uint16_t>(si));
			}
		}
		for (const render_smodel& s : rw.smodels)
		{
			if (s.primary_light >= 2)
			{
				const box3& box = w.models[static_cast<std::size_t>(s.model)].box;
				for (int corner = 0; corner < 8; corner++)
				{
					lit[static_cast<std::size_t>(s.primary_light)][s.cell].add(vec3{ corner & 1 ? box.maxs[0] : box.mins[0], corner & 2 ? box.maxs[1] : box.mins[1],
						corner & 4 ? box.maxs[2] : box.mins[2] } - light_origin(s.primary_light), light_axes[static_cast<std::size_t>(s.primary_light)]);
				}
			}
		}
		// The region of a light: one hull per cell it lights something in ( the shipped maps have several hulls where a
		// light shines through a doorway ), the hull of the light's own cell first and with the light itself in it.
		// Besides its nine ranges a hull gets up to eight of the light's further axes: those that cut the most off
		// the volume the nine ranges enclose - the planes of a spot's cone, the diagonals.
		for (std::size_t l = 2; l < light_count; l++)
		{
			std::map<int, kdop>& by_cell = lit[l];
			const int own_cell = cell_of(light_origin(static_cast<int>(l)));
			if (by_cell.empty())
			{
				// nothing names the light: a box around what it can reach
				const double r = w.lights[l].radius;
				for (int corner = 0; corner < 8; corner++)
				{
					by_cell[own_cell].add(vec3{ corner & 1 ? r : -r, corner & 2 ? r : -r, corner & 4 ? r : -r }, light_axes[l]);
				}
			}
			if (const auto own = by_cell.find(own_cell); own != by_cell.end())
			{
				own->second.add(vec3{ 0, 0, 0 }, light_axes[l]);
			}
			std::vector<int> order;
			for (const auto& [cell, hull] : by_cell)
			{
				order.push_back(cell);
			}
			std::stable_partition(order.begin(), order.end(), [&](const int cell) { return cell == own_cell; });
			std::size_t axes_written = 0, portal_axes_written = 0;
			double size[3] = {};
			for (const int cell : order)
			{
				const kdop& hull = by_cell[cell];
				render_light_hull out;
				double lo[9], hi[9];
				for (int a = 0; a < 9; a++)
				{
					const double pad = a < 3 ? 16.0 : 32.0;
					lo[a] = hull.lo[a] - pad;
					hi[a] = hull.hi[a] + pad;
					out.mid[a] = static_cast<float>((lo[a] + hi[a]) * 0.5);
					out.half[a] = static_cast<float>((hi[a] - lo[a]) * 0.5);
				}
				for (int a = 0; a < 3; a++)
				{
					size[a] = std::max(size[a], hi[a] - lo[a]);
				}
				// what each candidate would cut off the nine ranges' volume: the diagonals, the planes of a spot's cone,
				// and - in a cell the light is not in - the planes through the light and the edges of the portals
				const std::vector<vec3> corners = kdop_corners(lo, hi);
				std::vector<std::pair<double, int>> gains;
				for (int a = 0; a < KDOP_EXTRA && !corners.empty(); a++)
				{
					if (length(light_axes[l][static_cast<std::size_t>(a)]) < 0.5 || (a >= KDOP_PORTAL && cell == own_cell))
					{
						continue;
					}
					double least = 1e30, most = -1e30;
					for (const vec3& corner : corners)
					{
						const double d = dot(corner, light_axes[l][static_cast<std::size_t>(a)]);
						least = std::min(least, d);
						most = std::max(most, d);
					}
					const double gain = std::max(0.0, most - (hull.hi[9 + a] + 16.0)) + std::max(0.0, (hull.lo[9 + a] - 16.0) - least);
					if (gain > 8.0)
					{
						gains.emplace_back(gain, a);
					}
				}
				std::sort(gains.begin(), gains.end(), [](const std::pair<double, int>& a, const std::pair<double, int>& b) { return a.first > b.first; });
				for (std::size_t k = 0; k < gains.size() && k < 8; k++)
				{
					const int a = gains[k].second;
					render_light_hull::axis axis;
					axis.dir[0] = static_cast<float>(light_axes[l][static_cast<std::size_t>(a)].x);
					axis.dir[1] = static_cast<float>(light_axes[l][static_cast<std::size_t>(a)].y);
					axis.dir[2] = static_cast<float>(light_axes[l][static_cast<std::size_t>(a)].z);
					if (a >= KDOP_PORTAL)
					{
						// a plane through the light: what shines through the portal is on its positive side, the range
						// starts at the light ( as in the shipped hulls ) unless something lit lies behind it
						// ( luxels along the beam's edge reach a little over it: within 48 units the plane itself is the end )
						const double from = hull.lo[9 + a] > -48.0 ? 0.0 : hull.lo[9 + a] - 16.0, to = hull.hi[9 + a] + 16.0;
						axis.mid = static_cast<float>((from + to) * 0.5);
						axis.half = static_cast<float>((to - from) * 0.5);
						portal_axes_written++;
					}
					else
					{
						axis.mid = static_cast<float>((hull.lo[9 + a] + hull.hi[9 + a]) * 0.5);
						axis.half = static_cast<float>((hull.hi[9 + a] - hull.lo[9 + a]) * 0.5 + 16.0);
					}
					out.axes.push_back(axis);
				}
				axes_written += out.axes.size();
				rw.light_regions[l].push_back(std::move(out));
			}
			ctx.rep.notes.push_back(format("light: primary light %zu ( %s%s at %.0f %.0f %.0f, radius %.0f, %s ): %zu surfaces and %zu static models name it; region of %zu hull( s ) with %zu further axes ( %zu through the light and a portal edge ), %.0f x %.0f x %.0f",
				l, w.lights[l].scriptable ? "scriptable " : "", w.lights[l].type == 2 ? "spot" : "omni", w.lights[l].origin[0], w.lights[l].origin[1],
				w.lights[l].origin[2], w.lights[l].radius, w.lights[l].def.c_str(), named_surfaces[l],
				static_cast<std::size_t>(std::count_if(rw.smodels.begin(), rw.smodels.end(), [&](const render_smodel& m) { return m.primary_light == static_cast<int>(l); })),
				rw.light_regions[l].size(), axes_written, portal_axes_written, size[0], size[1], size[2]));
			if (movable(w.lights[l]))
			{
				ctx.rep.notes.push_back(format("light: primary light %zu may be moved %.0f units and turned %.0f degrees by a script: what it could reach then and nothing else lights is its as well",
					l, w.lights[l].translation_limit, turn_limit(w.lights[l]) * 180.0 / PI));
			}
		}

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
									// what the luxel throws back, as bright as a mid grey surface was without the materials' colours
								c[k] = std::sqrt(emit[from * 3 + static_cast<std::size_t>(k)] / (REFLECT * w.radiosity_scale));
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
