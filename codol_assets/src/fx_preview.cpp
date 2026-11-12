#include "codol_assets/fx_preview.hpp"

#include <algorithm>
#include <cmath>

// See fx_preview.hpp for what this is and what it is not. The formulas are the ones compile_fx stores: a velocity
// graph is units per second ( value x scale ), a size graph is the edge length ( the asset holds half of it ), a
// rotation graph degrees per second, gravity a percentage of 800 units / s^2.

namespace codol::assets
{
	namespace
	{
		constexpr float GRAVITY = 800.0f;
		constexpr float DEG = 3.14159265358979f / 180.0f;
		constexpr int STEPS = 24;		// of the velocity and rotation integrals

		// a number in [ 0, 1 ) that only depends on its arguments
		float rnd(const std::uint32_t seed, const int element, const int particle, const int which)
		{
			std::uint32_t h = seed * 0x9E3779B1u;
			for (const std::uint32_t v : { static_cast<std::uint32_t>(element), static_cast<std::uint32_t>(particle), static_cast<std::uint32_t>(which) })
			{
				h ^= v + 0x7F4A7C15u + (h << 6) + (h >> 2);
				h *= 0x85EBCA6Bu;
				h ^= h >> 13;
			}
			h *= 0xC2B2AE35u;
			h ^= h >> 16;
			return static_cast<float>(h >> 8) / 16777216.0f;
		}

		float range(const float pair[2], const float r) { return pair[0] + pair[1] * r; }
		float range(const std::int32_t pair[2], const float r) { return static_cast<float>(pair[0]) + static_cast<float>(pair[1]) * r; }

		// value of a graph for one particle: the first curve, or with `random` somewhere between both curves
		float graph(const FxGraph& g, const float t, const bool random, const float r, const int dimension = 0)
		{
			const float a = fx_curve_value(g.curve[0], t, dimension);
			if (!random || g.curve[1].key_count() == 0)
			{
				return a * g.scale;
			}
			return (a + (fx_curve_value(g.curve[1], t, dimension) - a) * r) * g.scale;
		}

		bool drawn(const std::string& type, FxShape& shape)
		{
			if (type == "billboardSprite") { shape = FxShape::billboard; return true; }
			if (type == "orientedSprite") { shape = FxShape::oriented; return true; }
			if (type == "rotatedSprite") { shape = FxShape::rotated; return true; }
			if (type == "tail") { shape = FxShape::tail; return true; }
			if (type == "line") { shape = FxShape::line; return true; }
			if (type == "cloud" || type == "sparkCloud") { shape = FxShape::cloud; return true; }
			return false;
		}

		const char* why_not(const std::string& type)
		{
			if (type == "trail") return "a ribbon behind a moving effect; this one stands still";
			if (type == "sparkFountain") return "spark fountains are not simulated";
			if (type == "model") return "models of an effect are not drawn";
			if (type == "light" || type == "spotLight") return "a light, nothing to draw";
			if (type == "sound") return "a sound";
			if (type == "decal") return "a decal on the world";
			if (type == "runner") return "it starts other effects, which are not followed";
			return "an element without visuals";
		}

		struct Spawn
		{
			float time = 0;		// seconds after the effect started
			int index = 0;
		};

		// the particles of one element that may be alive at `now`
		void spawns(const EditorElem& e, const int element, const float now, const std::uint32_t seed, std::vector<Spawn>& out)
		{
			out.clear();
			const float longest = (static_cast<float>(e.lifeSpanMsec[0]) + static_cast<float>(std::max(e.lifeSpanMsec[1], 0))) / 1000.0f;
			if (e.has_editor_flag("looping"))
			{
				const float interval = static_cast<float>(std::max(e.spawnLooping[0], 1)) / 1000.0f;
				const int count = e.spawnLooping[1];		// 0: for ever
				const float delay = range(e.spawnDelayMsec, rnd(seed, element, 0, 90)) / 1000.0f;
				int first = static_cast<int>(std::floor((now - delay - longest) / interval));
				first = std::max(first, 0);
				int last = static_cast<int>(std::floor((now - delay) / interval));
				if (count > 0)
				{
					last = std::min(last, count - 1);
				}
				last = std::min(last, first + 4000);
				for (int k = first; k <= last; k++)
				{
					out.push_back({ delay + static_cast<float>(k) * interval, k });
				}
				return;
			}
			const int count = std::clamp(static_cast<int>(range(e.spawnOneShot, rnd(seed, element, 0, 91)) + 0.5f), 0, 4000);
			for (int k = 0; k < count; k++)
			{
				out.push_back({ range(e.spawnDelayMsec, rnd(seed, element, k, 92)) / 1000.0f, k });
			}
		}
	}

	float fx_curve_value(const FxCurve& curve, const float t, const int dimension)
	{
		const std::size_t count = curve.key_count();
		if (count == 0 || dimension < 0 || dimension >= curve.dimensions)
		{
			return 0.0f;
		}
		const std::size_t stride = static_cast<std::size_t>(1 + curve.dimensions), at = static_cast<std::size_t>(1 + dimension);
		if (t <= curve.keys[0])
		{
			return curve.keys[at];
		}
		for (std::size_t k = 1; k < count; k++)
		{
			const float t0 = curve.keys[(k - 1) * stride], t1 = curve.keys[k * stride];
			if (t <= t1)
			{
				const float f = t1 > t0 ? (t - t0) / (t1 - t0) : 1.0f;
				return curve.keys[(k - 1) * stride + at] + (curve.keys[k * stride + at] - curve.keys[(k - 1) * stride + at]) * f;
			}
		}
		return curve.keys[(count - 1) * stride + at];
	}

	float fx_preview_duration(const EditorEffect& effect)
	{
		float end = 0.5f;
		for (const EditorElem& e : effect.elems)
		{
			if (e.has_editor_flag("disabled"))
			{
				continue;
			}
			const float life = static_cast<float>(e.lifeSpanMsec[0] + std::max(e.lifeSpanMsec[1], 0)) / 1000.0f;
			const float delay = static_cast<float>(e.spawnDelayMsec[0] + std::max(e.spawnDelayMsec[1], 0)) / 1000.0f;
			float spawning = 0.0f;
			if (e.has_editor_flag("looping"))
			{
				const float interval = static_cast<float>(std::max(e.spawnLooping[0], 1)) / 1000.0f;
				spawning = interval * static_cast<float>(e.spawnLooping[1] > 0 ? e.spawnLooping[1] - 1 : 4);
			}
			end = std::max(end, delay + spawning + life);
		}
		return std::min(end, 60.0f);
	}

	void simulate_fx(const EditorEffect& effect, const float seconds, FxPreview& out, const std::uint32_t seed)
	{
		out.particles.clear();
		out.skipped.clear();
		// local x is the effect's forward axis: up. The other two keep the frame right handed.
		static const float AXIS[3][3] = { { 0, 0, 1 }, { 0, 1, 0 }, { -1, 0, 0 } };
		const auto to_world = [](const float local[3], float world[3])
		{
			for (int k = 0; k < 3; k++)
			{
				world[k] = local[0] * AXIS[0][k] + local[1] * AXIS[1][k] + local[2] * AXIS[2][k];
			}
		};

		std::vector<Spawn> list;
		for (std::size_t n = 0; n < effect.elems.size(); n++)
		{
			const EditorElem& e = effect.elems[n];
			const int element = static_cast<int>(n);
			const std::string label = "element " + std::to_string(n) + ( e.name.empty() ? "" : " \"" + e.name + "\"" ) + " ( " + ( e.visualType.empty() ? "no visuals" : e.visualType ) + " )";
			if (e.has_editor_flag("disabled"))
			{
				out.skipped.push_back(label + ": disabled");
				continue;
			}
			FxShape shape{};
			if (!drawn(e.visualType, shape) || e.visuals.empty())
			{
				out.skipped.push_back(label + ": not drawn - " + why_not(e.visualType));
				continue;
			}
			for (const char* child : { "playOnTouch", "playOnDeath", "playOnRun" })
			{
				if (e.has_editor_flag(child))
				{
					out.skipped.push_back(label + ": the effect it starts ( " + child + " ) is not followed");
				}
			}
			if (shape == FxShape::cloud)
			{
				out.skipped.push_back(label + ": a swarm, shown as one particle each");
			}

			const bool world_vel[2] = { e.has_editor_flag("absVel0"), e.has_editor_flag("absVel1") };
			const bool rand_vel[2] = { e.has_editor_flag("useRandVel0"), e.has_editor_flag("useRandVel1") };
			const bool rand_size[2] = { e.has_editor_flag("useRandSize0"), e.has_editor_flag("useRandSize1") };
			const bool rand_color = e.has_editor_flag("useRandColor"), rand_alpha = e.has_editor_flag("useRandAlpha"), rand_rot = e.has_editor_flag("useRandRotDelta");
			const bool non_uniform = e.has_flag("nonUniformScale");
			const bool mod_color = e.has_editor_flag("modColorByAlpha");
			const int columns = 1 << std::clamp(e.atlasColIndexBits, 0, 8), rows = 1 << std::clamp(e.atlasRowIndexBits, 0, 8);
			const int cells = e.atlasEntryCount > 0 ? std::min(e.atlasEntryCount, columns * rows) : columns * rows;

			spawns(e, element, seconds, seed, list);
			for (const Spawn& s : list)
			{
				const auto r = [&](const int which) { return rnd(seed, element, s.index, which); };
				const float life = std::max(range(e.lifeSpanMsec, r(1)), 1.0f) / 1000.0f;
				const float age = seconds - s.time;
				if (age < 0.0f || age >= life)
				{
					continue;
				}
				const float t = age / life;

				FxParticle p;
				p.element = element;
				p.index = s.index;
				p.shape = shape;
				p.material = e.visuals[std::min(e.visuals.size() - 1, static_cast<std::size_t>(r(2) * static_cast<float>(e.visuals.size())))];
				p.age = age;
				p.life = life;
				const float forward[3] = { 1, 0, 0 };
				to_world(forward, p.normal);

				// where it starts: the origin ranges, and the cylinder around the forward axis
				float local[3] = { range(e.spawnOrigin[0], r(3)), range(e.spawnOrigin[1], r(4)), range(e.spawnOrigin[2], r(5)) };
				if (e.has_flag("spawnOffsetCylinder") || e.has_flag("spawnOffsetSphere"))
				{
					const float radius = range(e.spawnOffsetRadius, r(6)), turn = r(7) * 360.0f * DEG;
					if (e.has_flag("spawnOffsetSphere"))
					{
						const float z = r(8) * 2.0f - 1.0f, ring = std::sqrt(std::max(0.0f, 1.0f - z * z));
						local[0] += radius * z;
						local[1] += radius * ring * std::cos(turn);
						local[2] += radius * ring * std::sin(turn);
					}
					else
					{
						local[0] += range(e.spawnOffsetHeight, r(8));
						local[1] += radius * std::cos(turn);
						local[2] += radius * std::sin(turn);
					}
				}

				// the way it went: both velocity graph sets, integrated up to now; the last step gives the velocity
				float moved[2][3] = {}, speed[2][3] = {};
				const float blend[2] = { r(9), r(10) };
				for (int set = 0; set < 2; set++)
				{
					for (int axis = 0; axis < 3; axis++)
					{
						float previous = graph(e.velGraph[set][axis], 0.0f, rand_vel[set], blend[set]);
						for (int k = 1; k <= STEPS; k++)
						{
							const float value = graph(e.velGraph[set][axis], t * static_cast<float>(k) / static_cast<float>(STEPS), rand_vel[set], blend[set]);
							moved[set][axis] += (previous + value) * 0.5f * (age / static_cast<float>(STEPS));
							previous = value;
						}
						speed[set][axis] = previous;
					}
				}
				float world[3], vel[3] = {};
				to_world(local, world);
				for (int set = 0; set < 2; set++)
				{
					float d[3], v[3];
					if (world_vel[set])
					{
						std::copy(moved[set], moved[set] + 3, d);
						std::copy(speed[set], speed[set] + 3, v);
					}
					else
					{
						to_world(moved[set], d);
						to_world(speed[set], v);
					}
					for (int k = 0; k < 3; k++)
					{
						world[k] += d[k];
						vel[k] += v[k];
					}
				}
				const float fall = GRAVITY * range(e.gravity, r(11)) * 0.01f;
				world[2] -= 0.5f * fall * age * age;
				vel[2] -= fall * age;
				std::copy(world, world + 3, p.position);
				std::copy(vel, vel + 3, p.velocity);

				// how it looks now
				p.width = std::fabs(graph(e.sizeGraph[0], t, rand_size[0], r(12)));
				p.height = non_uniform ? std::fabs(graph(e.sizeGraph[1], t, rand_size[1], r(13))) : p.width;
				const float colour_blend = r(14), alpha_blend = r(15);
				for (int k = 0; k < 3; k++)
				{
					p.color[k] = e.colorGraph.curve[0].key_count() ? std::clamp(graph(e.colorGraph, t, rand_color, colour_blend, k) / (e.colorGraph.scale != 0 ? e.colorGraph.scale : 1.0f), 0.0f, 1.0f) : 1.0f;
				}
				p.color[3] = e.alphaGraph.curve[0].key_count() ? std::clamp(graph(e.alphaGraph, t, rand_alpha, alpha_blend) / (e.alphaGraph.scale != 0 ? e.alphaGraph.scale : 1.0f), 0.0f, 1.0f) : 1.0f;
				if (mod_color)
				{
					for (int k = 0; k < 3; k++)
					{
						p.color[k] *= p.color[3];
					}
					p.color[3] = 1.0f;
				}
				float turned = range(e.initialRotation, r(16));
				{
					const float rot_blend = r(17);
					float previous = graph(e.rotGraph, 0.0f, rand_rot, rot_blend);
					for (int k = 1; k <= STEPS; k++)
					{
						const float value = graph(e.rotGraph, t * static_cast<float>(k) / static_cast<float>(STEPS), rand_rot, rot_blend);
						turned += (previous + value) * 0.5f * (age / static_cast<float>(STEPS));
						previous = value;
					}
				}
				p.rotation = turned;

				// the cell of the atlas
				if (cells > 1)
				{
					int cell = e.atlasIndex;
					if (e.atlasBehavior.find("startRandom") != std::string::npos)
					{
						cell = static_cast<int>(r(18) * static_cast<float>(cells));
					}
					if (e.atlasBehavior.find("playOverLife") != std::string::npos)
					{
						cell += static_cast<int>(t * static_cast<float>(cells));
					}
					else if (e.atlasFps > 0)
					{
						cell += static_cast<int>(age * static_cast<float>(e.atlasFps));
					}
					cell = ((cell % cells) + cells) % cells;
					p.u0 = static_cast<float>(cell % columns) / static_cast<float>(columns);
					p.v0 = static_cast<float>(cell / columns) / static_cast<float>(rows);
					p.u1 = p.u0 + 1.0f / static_cast<float>(columns);
					p.v1 = p.v0 + 1.0f / static_cast<float>(rows);
				}
				out.particles.push_back(p);
			}
		}
	}
}
