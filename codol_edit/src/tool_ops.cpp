// codol_edit: generators - a rope between two points, noise on terrain meshes.

#include "doc_impl.hpp"

#include <cmath>

namespace codol::edit
{
	namespace
	{
		constexpr double WORLD_LIMIT = 131072.0;

		std::string new_object_layer(const document::impl& d)
		{
			for (std::size_t i = 0; i < d.layers.size(); i++)
			{
				if (d.layers[i].active)
				{
					return i == 0 ? std::string() : d.layers[i].name;
				}
			}

			return {};
		}

		// ---- value noise: a number in -1 .. 1 for every point, smooth, the same for the same seed
		double lattice(const std::int64_t x, const std::int64_t y, const std::int64_t z, const std::uint32_t seed)
		{
			std::uint64_t h = static_cast<std::uint64_t>(x) * 0x9E3779B97F4A7C15ull;
			h ^= static_cast<std::uint64_t>(y) * 0xC2B2AE3D27D4EB4Full + (h << 6) + (h >> 2);
			h ^= static_cast<std::uint64_t>(z) * 0x165667B19E3779F9ull + (h << 6) + (h >> 2);
			h ^= static_cast<std::uint64_t>(seed) * 0xD6E8FEB86659FD93ull;
			h ^= h >> 32;
			h *= 0xD6E8FEB86659FD93ull;
			h ^= h >> 32;
			return static_cast<double>(h & 0xFFFFFF) / static_cast<double>(0xFFFFFF) * 2.0 - 1.0;
		}

		double smooth(const double t)
		{
			return t * t * t * (t * (t * 6.0 - 15.0) + 10.0);
		}

		double value_noise(const vec3& p, const std::uint32_t seed)
		{
			const double fx = std::floor(p.x), fy = std::floor(p.y), fz = std::floor(p.z);
			const auto ix = static_cast<std::int64_t>(fx), iy = static_cast<std::int64_t>(fy), iz = static_cast<std::int64_t>(fz);
			const double tx = smooth(p.x - fx), ty = smooth(p.y - fy), tz = smooth(p.z - fz);

			double corner[2][2][2];
			for (int a = 0; a < 2; a++)
			{
				for (int b = 0; b < 2; b++)
				{
					for (int c = 0; c < 2; c++)
					{
						corner[a][b][c] = lattice(ix + a, iy + b, iz + c, seed);
					}
				}
			}

			const auto mix = [](const double a, const double b, const double t) { return a + (b - a) * t; };
			const double x00 = mix(corner[0][0][0], corner[1][0][0], tx), x10 = mix(corner[0][1][0], corner[1][1][0], tx);
			const double x01 = mix(corner[0][0][1], corner[1][0][1], tx), x11 = mix(corner[0][1][1], corner[1][1][1], tx);
			return mix(mix(x00, x10, ty), mix(x01, x11, ty), tz);
		}

		// octaves on top of each other: each half as strong and twice as fine; the sum stays within -1 .. 1
		double fractal_noise(const vec3& p, const std::uint32_t seed, const int octaves)
		{
			double sum = 0.0, weight = 1.0, total = 0.0, scale = 1.0;

			for (int o = 0; o < octaves; o++)
			{
				sum += value_noise(p * scale, seed + static_cast<std::uint32_t>(o) * 7919u) * weight;
				total += weight;
				weight *= 0.5;
				scale *= 2.0;
			}

			return total > 0.0 ? sum / total : 0.0;
		}
	}

	bool document::create_rope(const rope_params& params, std::vector<object_id>* out)
	{
		if (out)
		{
			out->clear();
		}

		const vec3 span = params.to - params.from;
		const double straight = length(span);

		if (!(straight > 1.0))
		{
			return d->fail("Rope: the two points are the same");
		}

		if (!(params.radius > 0.01) || params.radius > 512.0)
		{
			return d->fail("Rope: radius 0.01 to 512");
		}

		if (params.segments < 1 || params.segments > 256 || params.sides < 3 || params.sides > 15)
		{
			return d->fail("Rope: 1 to 256 segments, 3 to 15 sides");
		}

		for (const vec3& p : { params.from, params.to })
		{
			if (std::fabs(p.x) > WORLD_LIMIT || std::fabs(p.y) > WORLD_LIMIT || std::fabs(p.z) > WORLD_LIMIT)
			{
				return d->fail("Rope: outside the world");
			}
		}

		// the middle hangs `slack` below the straight line: a parabola, which a rope that hangs a little is close to
		const auto centre = [&](const double t)
		{
			vec3 p = params.from + span * t;
			p.z -= params.slack * 4.0 * t * (1.0 - t);
			return p;
		};

		const auto tangent = [&](const double t)
		{
			vec3 v = span;
			v.z -= params.slack * 4.0 * (1.0 - 2.0 * t);
			return normalize(v);
		};

		// rings of the tube, with the length of the rope up to each ( texture coordinates )
		struct ring
		{
			vec3 centre, side, top;
			double along = 0.0;
		};

		std::vector<ring> rings(static_cast<std::size_t>(params.segments) + 1);
		vec3 side_before;

		for (int i = 0; i <= params.segments; i++)
		{
			ring& r = rings[static_cast<std::size_t>(i)];
			const double t = static_cast<double>(i) / params.segments;
			r.centre = centre(t);

			const vec3 forward = tangent(t);
			vec3 side = cross(forward, vec3{ 0, 0, 1 });

			if (length(side) < 1e-6)
			{
				// straight up or down: any direction across is as good; keep the one of the ring before
				side = i > 0 ? side_before : vec3{ 1, 0, 0 };
			}

			r.side = normalize(side);
			r.top = normalize(cross(r.side, forward));
			side_before = r.side;

			if (i > 0)
			{
				r.along = rings[static_cast<std::size_t>(i) - 1].along + length(r.centre - rings[static_cast<std::size_t>(i) - 1].centre);
			}
		}

		const std::string material = params.material.empty() ? d->default_material : params.material;
		const std::string layer = new_object_layer(*d);
		const double around = 2.0 * PI * params.radius;

		transaction_scope tx(*d, "Rope");
		std::vector<object_id> made;

		// a mesh holds at most 16 rings: a long rope is a chain of meshes that share a ring
		for (int first = 0; first < params.segments; first += 15)
		{
			const int last = std::min(first + 15, params.segments);

			patch p;
			p.type = patch_type::mesh;
			p.material = material;
			p.lightmap = "lightmap_gray";
			p.width = last - first + 1;
			p.height = params.sides + 1;
			p.layer = layer;

			for (int c = first; c <= last; c++)
			{
				const ring& r = rings[static_cast<std::size_t>(c)];

				for (int row = 0; row <= params.sides; row++)
				{
					// the last vertex of a ring is its first again: the tube is closed
					const double angle = 2.0 * PI * (row % params.sides) / params.sides;
					const double fraction = static_cast<double>(row) / params.sides;

					patch_vertex v;
					v.xyz = r.centre + r.side * (std::cos(angle) * params.radius) + r.top * (std::sin(angle) * params.radius);
					v.tex = { r.along * 4.0, fraction * around * 4.0, r.along / 16.0, fraction * around / 16.0 };
					p.vertices.push_back(std::move(v));
				}
			}

			made.push_back(d->add_patch(std::move(p), d->world()));
		}

		d->sel.faces.clear();
		d->set_selection(made);

		if (out)
		{
			*out = made;
		}

		return true;
	}

	bool document::patch_noise(const noise_params& params)
	{
		if (params.octaves < 1 || params.octaves > 8 || !(params.frequency > 0.0))
		{
			return d->fail("Noise: 1 to 8 octaves, a frequency above 0");
		}

		if (!(length(params.direction) > 1e-9))
		{
			return d->fail("Noise: no direction");
		}

		std::vector<object_id> patches;

		for (const object_id id : d->sel.objects)
		{
			if (const patch* p = d->get_patch(id); p && p->width >= 2 && p->height >= 2)
			{
				patches.push_back(id);
			}
		}

		if (patches.empty())
		{
			return d->fail("Noise: select a terrain mesh");
		}

		const vec3 direction = normalize(params.direction);
		bool any = false;

		transaction_scope tx(*d, "Noise");

		for (const object_id id : patches)
		{
			patch& p = d->edit_patch(id);

			for (int c = 0; c < p.width; c++)
			{
				for (int r = 0; r < p.height; r++)
				{
					if (params.keep_edges && (c == 0 || r == 0 || c == p.width - 1 || r == p.height - 1))
					{
						continue;
					}

					patch_vertex& v = p.at(c, r);

					// Sampled where the vertex is across the direction it moves in: a second run with the same
					// numbers adds the same again instead of reading the noise somewhere else, and two meshes that
					// share a vertex position move it the same way.
					const vec3 flat = v.xyz - direction * dot(v.xyz, direction);
					const double n = fractal_noise(flat * params.frequency, params.seed, params.octaves);
					v.xyz += direction * (n * params.amplitude);
					any = true;
				}
			}
		}

		if (!any)
		{
			tx.cancel();
			return d->fail("Noise: the meshes have no inner vertices ( turn off \"keep the edges\" )");
		}

		return true;
	}
}
