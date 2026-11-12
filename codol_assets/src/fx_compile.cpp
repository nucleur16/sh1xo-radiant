// EditorEffect <-> CompiledFx: what the effects converter of the game's tools does, and its inverse.
//
// Every rule here was read off the shipped effects: 453 of them have the name of an .efx of the CoD4 mod tools and
// most of those still are what that file says ( codol_asset_verify fx-source ), which gives source and result side by
// side. See README, "Effects".

#include "codol_assets/fx.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <set>

namespace codol::assets
{
	namespace
	{
		using zone::Status;

		constexpr float DEG_TO_RAD = 0.017453292f;
		constexpr std::uint32_t FLAG_VELOCITY_LOCAL = 0x01000000u, FLAG_VELOCITY_WORLD = 0x02000000u, FLAG_GRAVITY = 0x04000000u;
		constexpr std::uint32_t FLAG_NONUNIFORM = 0x10000000u;
		constexpr std::uint32_t FLAGS_DERIVED = FLAG_VELOCITY_LOCAL | FLAG_VELOCITY_WORLD | FLAG_GRAVITY;

		struct FlagName
		{
			const char* name;
			std::uint32_t bits;
			std::uint32_t mask;
		};
		// the words of the "flags" statement. Up to blocksSight they are the editor's own; the others are the names
		// the same bits have in the IW4 engine and are only carried, bit for bit.
		constexpr FlagName FLAG_NAMES[] = {
			{ "spawnRelative", 0x2, 0x2 }, { "spawnFrustumCull", 0x4, 0x4 }, { "runnerUsesRandRot", 0x8, 0x8 },
			{ "spawnOffsetNone", 0, 0x30 }, { "spawnOffsetSphere", 0x10, 0x30 }, { "spawnOffsetCylinder", 0x20, 0x30 },
			{ "runRelToWorld", 0, 0xC0 }, { "runRelToSpawn", 0x40, 0xC0 }, { "runRelToEffect", 0x80, 0xC0 }, { "runRelToOffset", 0xC0, 0xC0 },
			{ "useCollision", 0x100, 0x100 }, { "dieOnTouch", 0x200, 0x200 }, { "drawPastFog", 0x400, 0x400 }, { "drawWithViewModel", 0x800, 0x800 },
			{ "blocksSight", 0x1000, 0x1000 }, { "drawInThermalOnly", 0x2000, 0x2000 }, { "trailOrientByVelocity", 0x4000, 0x4000 },
			{ "emitOrientByElem", 0x8000, 0x8000 }, { "useOcclusionQuery", 0x10000, 0x10000 }, { "nodrawInThermal", 0x20000, 0x20000 },
			{ "spawnImpactFxWithSurfaceName", 0x40000, 0x40000 }, { "receiveDynamicLight", 0x80000, 0x80000 }, { "volumetricTrail", 0x100000, 0x100000 },
			{ "modelUsesPhysics", 0x08000000, 0x08000000 }, { "nonUniformScale", 0x10000000, 0x10000000 },
			{ "cloudShapeSphereLarge", 0x20000000, 0x60000000 }, { "cloudShapeSphereMedium", 0x40000000, 0x60000000 },
			{ "cloudShapeSphereSmall", 0x60000000, 0x60000000 },
		};

		constexpr const char* VISUAL_TYPES[c1::FX_ELEM_TYPE_COUNT] = { "billboardSprite", "orientedSprite", "rotatedSprite", "tail", "line", "trail", "cloud",
			"sparkCloud", "sparkFountain", "model", "light", "spotLight", "sound", "decal", "runner" };

		// which fields of the visual state an element type has
		enum : std::uint32_t { CH_COLOR = 1, CH_ROT = 2, CH_SIZE0 = 4, CH_SIZE1 = 8, CH_SCALE = 16 };
		std::uint32_t channels(const std::uint8_t type)
		{
			switch (type)
			{
			case c1::FX_ELEM_TYPE_SPRITE_BILLBOARD:
			case c1::FX_ELEM_TYPE_SPRITE_ORIENTED:
			case c1::FX_ELEM_TYPE_SPRITE_ROTATED:
			case c1::FX_ELEM_TYPE_TAIL:
			case c1::FX_ELEM_TYPE_LINE: return CH_COLOR | CH_ROT | CH_SIZE0 | CH_SIZE1;
			case c1::FX_ELEM_TYPE_TRAIL: return CH_COLOR | CH_SIZE0 | CH_SIZE1;
			case c1::FX_ELEM_TYPE_CLOUD:
			case c1::FX_ELEM_TYPE_SPARK_CLOUD:
			case c1::FX_ELEM_TYPE_SPARK_FOUNTAIN: return CH_COLOR | CH_SIZE0 | CH_SIZE1 | CH_SCALE;
			case c1::FX_ELEM_TYPE_MODEL: return CH_SCALE;
			case c1::FX_ELEM_TYPE_OMNI_LIGHT:
			case c1::FX_ELEM_TYPE_SPOT_LIGHT: return CH_COLOR | CH_SIZE0 | CH_SCALE;
			case c1::FX_ELEM_TYPE_SOUND: return CH_SCALE;
			case c1::FX_ELEM_TYPE_DECAL: return CH_COLOR | CH_ROT | CH_SIZE0;
			default: return 0;
			}
		}

		bool uses_material(const std::uint8_t type)
		{
			return type <= c1::FX_ELEM_TYPE_SPARK_FOUNTAIN || type == c1::FX_ELEM_TYPE_DECAL;
		}

		// ---- curves ----
		void eval(const FxCurve& c, const float t, float* out)
		{
			const std::size_t stride = static_cast<std::size_t>(1 + c.dimensions);
			const std::size_t count = c.keys.size() / stride;
			const float* k = c.keys.data();
			if (count == 0)
			{
				std::fill(out, out + c.dimensions, 0.0f);
				return;
			}
			if (t <= k[0])
			{
				std::copy(k + 1, k + stride, out);
				return;
			}
			for (std::size_t i = 1; i < count; i++)
			{
				const float* a = k + (i - 1) * stride;
				const float* b = k + i * stride;
				if (t <= b[0])
				{
					if (t == b[0])
					{
						std::copy(b + 1, b + stride, out);
						return;
					}
					const float f = b[0] > a[0] ? (t - a[0]) / (b[0] - a[0]) : 0.0f;
					for (int d = 0; d < c.dimensions; d++)
					{
						out[d] = a[1 + d] + (b[1 + d] - a[1 + d]) * f;
					}
					return;
				}
			}
			std::copy(k + (count - 1) * stride + 1, k + count * stride, out);
		}

		float eval1(const FxCurve& c, const float t)
		{
			float v[3] = {};
			eval(c, t, v);
			return v[0];
		}

		// largest distance of a key of the curve from the curve sampled at `intervals` + 1 points
		float curve_error(const FxCurve& c, const int intervals)
		{
			const std::size_t stride = static_cast<std::size_t>(1 + c.dimensions);
			const std::size_t count = c.keys.size() / stride;
			float worst = 0;
			float lo[3], hi[3];
			for (std::size_t i = 0; i < count; i++)
			{
				const float* key = c.keys.data() + i * stride;
				const float x = std::clamp(key[0], 0.0f, 1.0f) * static_cast<float>(intervals);
				const int index = std::min(static_cast<int>(x), intervals - 1);
				const float f = x - static_cast<float>(index);
				eval(c, static_cast<float>(index) / static_cast<float>(intervals), lo);
				eval(c, static_cast<float>(index + 1) / static_cast<float>(intervals), hi);
				for (int d = 0; d < c.dimensions; d++)
				{
					worst = std::max(worst, std::fabs(lo[d] + (hi[d] - lo[d]) * f - key[1 + d]));
				}
			}
			return worst;
		}

		// The number of intervals the curves are sampled with: from 1 up to the limit, a count is taken when it lowers
		// the largest error of all curves by more than 0.02 ( delta fitted: 0.02 gives 1447 of 1448 velocity counts of
		// the shipped effects with a source, 0.018 and 0.022 under 1420 ).
		int decide_intervals(const std::vector<const FxCurve*>& curves, const int limit)
		{
			const auto error = [&](const int n)
			{
				float worst = 0;
				for (const FxCurve* c : curves)
				{
					worst = std::max(worst, curve_error(*c, n));
				}
				return worst;
			};
			int best = 1;
			float bestError = error(1);
			for (int n = 2; n <= limit && bestError > 0.02f; n++)
			{
				const float e = error(n);
				if (e < bestError - 0.02f)
				{
					bestError = e;
					best = n;
				}
			}
			return best;
		}

		int interval_limit(const std::int32_t* lifeSpanMsec)
		{
			return std::clamp((lifeSpanMsec[0] + lifeSpanMsec[1] / 2) / 100, 1, 80);
		}

		// ---- the formulas, shared by compile and extract ----
		float per_interval(const float value, const float scale, const int intervals) { return value * scale / (1000.0f * static_cast<float>(intervals)); }
		float rot_per_interval(const float value, const float scale, const int intervals) { return value * scale * DEG_TO_RAD / (1000.0f * static_cast<float>(intervals)); }
		float half_size(const float value, const float scale) { return value * scale * 0.5f; }
		float scaled(const float value, const float scale) { return value * scale; }
		float radians(const float degrees) { return degrees * DEG_TO_RAD; }
		float radians_per_msec(const float degrees) { return degrees * DEG_TO_RAD / 1000.0f; }
		float percent(const float value) { return value * 0.01f; }
		std::uint8_t to_byte(const float value) { return static_cast<std::uint8_t>(std::clamp(static_cast<int>(value * 255.0f + 0.5f), 0, 255)); }

		// the x near `guess` with f( x ) == target ( the stored float ), so that a definition compiles back bit for bit
		template <typename F>
		float solve(const float target, const float guess, F&& f)
		{
			if (!std::isfinite(guess) || f(guess) == target)
			{
				return guess;
			}
			float lo = guess, hi = guess;
			for (int k = 0; k < 16; k++)
			{
				lo = std::nextafter(lo, -INFINITY);
				if (f(lo) == target)
				{
					return lo;
				}
				hi = std::nextafter(hi, INFINITY);
				if (f(hi) == target)
				{
					return hi;
				}
			}
			return guess;
		}

		std::uint8_t elem_type(const std::string& visualType)
		{
			for (std::uint8_t t = 0; t < c1::FX_ELEM_TYPE_COUNT; t++)
			{
				if (visualType == VISUAL_TYPES[t])
				{
					return t;
				}
			}
			return c1::FX_ELEM_TYPE_SPRITE_BILLBOARD;
		}

		bool parse_ints(const std::string& text, std::int64_t* values, const int count)
		{
			const char* p = text.c_str();
			for (int k = 0; k < count; k++)
			{
				char* end = nullptr;
				values[k] = std::strtoll(p, &end, 0);
				if (end == p)
				{
					return false;
				}
				p = end;
			}
			return true;
		}

		Status compile_elem(const EditorElem& s, const bool looping, const FxCompileOptions& options, FxElem& e, std::vector<std::string>& notes)
		{
			const std::string where = "element '" + s.name + "': ";
			c1::FxElemDef& d = e.def;
			d = {};
			// flags
			std::uint32_t flags = 0;
			for (const std::vector<std::string>* list : { &s.flags, &s.editorFlags })
			{
				for (const std::string& word : *list)
				{
					const auto it = std::find_if(std::begin(FLAG_NAMES), std::end(FLAG_NAMES), [&](const FlagName& f) { return word == f.name; });
					if (it != std::end(FLAG_NAMES))
					{
						flags |= it->bits;
					}
					else if (list == &s.flags)
					{
						notes.push_back(where + "flag '" + word + "' is not known, ignored");
					}
				}
			}
			if (const std::string* bits = s.find_extra("flagBits"))
			{
				flags |= static_cast<std::uint32_t>(std::strtoul(bits->c_str(), nullptr, 0));
			}
			d.elemType = elem_type(s.visualType);
			if (looping)
			{
				d.spawn[0] = s.spawnLooping[0];
				d.spawn[1] = s.spawnLooping[1] > 0 ? s.spawnLooping[1] : INT32_MAX;		// 0: for ever
			}
			else
			{
				d.spawn[0] = s.spawnOneShot[0];
				d.spawn[1] = s.spawnOneShot[1];
			}
			// distances and positions: a range with a negative amplitude is stored from its other end. Angles, angular
			// velocity, gravity and elasticity are stored as they are written.
			const auto range = [](const float* v) { return v[1] < 0 ? c1::FxFloatRange{ v[0] + v[1], -v[1] } : c1::FxFloatRange{ v[0], v[1] }; };
			const auto range_of = [](const float base, const float amplitude) { return c1::FxFloatRange{ base, amplitude }; };
			const auto int_range = [](const std::int32_t* v) { return v[1] < 0 ? c1::FxIntRange{ v[0] + v[1], -v[1] } : c1::FxIntRange{ v[0], v[1] }; };
			d.spawnRange = range(s.spawnRange);
			d.fadeInRange = range(s.fadeInRange);
			d.fadeOutRange = range(s.fadeOutRange);
			d.spawnFrustumCullRadius = s.spawnFrustumCullRadius;
			d.spawnDelayMsec = int_range(s.spawnDelayMsec);
			d.lifeSpanMsec = int_range(s.lifeSpanMsec);
			for (int k = 0; k < 3; k++)
			{
				d.spawnOrigin[k] = range(s.spawnOrigin[k]);
				d.spawnAngles[k] = range_of(radians(s.spawnAngles[k][0]), radians(s.spawnAngles[k][1]));
				d.angularVelocity[k] = range_of(radians_per_msec(s.angularVelocity[k][0]), radians_per_msec(s.angularVelocity[k][1]));
			}
			d.spawnOffsetRadius = range(s.spawnOffsetRadius);
			d.spawnOffsetHeight = range(s.spawnOffsetHeight);
			d.initialRotation = range_of(radians(s.initialRotation[0]), radians(s.initialRotation[1]));
			d.gravity = range_of(percent(s.gravity[0]), percent(s.gravity[1]));
			d.reflectionFactor = { s.elasticity[0], s.elasticity[1] };
			if (d.gravity.base != 0 || d.gravity.amplitude != 0)
			{
				flags |= FLAG_GRAVITY;
			}
			// atlas: only elements that draw a material have one. The loop count is stored one higher than the editor
			// has it; columns and rows are the material's.
			const bool drawsMaterial = uses_material(d.elemType);
			if (drawsMaterial)
			{
				std::uint8_t behavior = 0;
				const std::string& b = s.atlasBehavior;
				behavior |= b.find("startRandom") != std::string::npos ? 1 : b.find("startIndexed") != std::string::npos ? 2 : 0;
				behavior |= b.find("playOverLife") != std::string::npos ? 4 : 0;
				behavior |= b.find("loopOnlyNTimes") != std::string::npos ? 8 : 0;
				d.atlas.behavior = behavior;
				d.atlas.index = static_cast<std::uint8_t>(s.atlasIndex);
				d.atlas.fps = static_cast<std::uint8_t>(s.atlasFps);
				d.atlas.loopCount = static_cast<std::uint8_t>(s.visuals.empty() ? 0 : s.atlasLoopCount + 1);
				int columnBits = s.atlasColIndexBits, rowBits = s.atlasRowIndexBits;
				int columns = 0, rows = 0;
				if (!s.visuals.empty() && options.materialAtlas && options.materialAtlas(d.elemType == c1::FX_ELEM_TYPE_DECAL && !s.visuals[0].starts_with("mc/") &&
						!s.visuals[0].starts_with("wc/") ? "mc/" + s.visuals[0] : s.visuals[0], columns, rows))
				{
					columnBits = rowBits = 0;
					while ((1 << (columnBits + 1)) <= columns) columnBits++;
					while ((1 << (rowBits + 1)) <= rows) rowBits++;
				}
				d.atlas.colIndexBits = static_cast<std::uint8_t>(columnBits);
				d.atlas.rowIndexBits = static_cast<std::uint8_t>(rowBits);
				d.atlas.entryCount = static_cast<std::int16_t>(s.visuals.empty() ? 0 : 1 << (columnBits + rowBits));
			}

			const int limit = interval_limit(s.lifeSpanMsec);
			std::int64_t hint[2] = { 0, 0 };
			const std::string* hintText = s.find_extra("intervals");
			const bool hinted = hintText && parse_ints(*hintText, hint, 2);

			// ---- velocity ----
			{
				std::vector<const FxCurve*> curves;
				for (int frame = 0; frame < 2; frame++)
				{
					for (int axis = 0; axis < 3; axis++)
					{
						curves.push_back(&s.velGraph[frame][axis].curve[0]);
						curves.push_back(&s.velGraph[frame][axis].curve[1]);
					}
				}
				const int intervals = hinted ? static_cast<int>(std::clamp<std::int64_t>(hint[0], 1, 255)) : decide_intervals(curves, limit);
				d.velIntervalCount = static_cast<std::uint8_t>(intervals);
				d.velSamples = c1::ZPTR_INLINE;
				e.velSamples.assign(static_cast<std::size_t>(intervals) + 1, {});
				// a graph set is a velocity in the element's own frame, or with "absVel<n>" in the world's
				const bool world[2] = { s.has_editor_flag("absVel0"), s.has_editor_flag("absVel1") };
				bool any[2] = {};
				// The random part. First graph set: its second curve is the other end of the range ( amplitude = second
				// - first ). Second graph set: its second curve is the spread itself - unless both sets are random
				// velocities of one frame, then it is an other end as well. ( 726 of 774 unambiguous samples of the
				// shipped effects with a source; the rest are two effects. )
				const bool random[2] = { s.has_editor_flag("useRandVel0"), s.has_editor_flag("useRandVel1") };
				const bool sameFrame = world[0] == world[1];
				const bool backCompat = s.has_editor_flag("useBackCompatVel");
				for (int i = 0; i <= intervals; i++)
				{
					const float t = static_cast<float>(i) / static_cast<float>(intervals);
					c1::FxElemVelStateSample& sample = e.velSamples[static_cast<std::size_t>(i)];
					for (int axis = 0; axis < 3; axis++)
					{
						float base[2], second[2];
						for (int set = 0; set < 2; set++)
						{
							base[set] = eval1(s.velGraph[set][axis].curve[0], t);
							second[set] = eval1(s.velGraph[set][axis].curve[1], t);
						}
						for (int set = 0; set < 2; set++)
						{
							const FxGraph& g = s.velGraph[set][axis];
							c1::FxElemVelStateInFrame& state = world[set] ? sample.world : sample.local;
							const float v = per_interval(base[set], g.scale, intervals);
							float a = 0;
							if (random[set])
							{
								a = set == 0 || (sameFrame && random[0]) ? per_interval(second[set] - base[set], g.scale, intervals) : per_interval(second[set], g.scale, intervals);
							}
							if (set == 0 && random[0] && !random[1] && sameFrame && backCompat)
							{
								// "back compatible" velocity: the range ends at the first set's second curve alone
								a -= per_interval(base[1], s.velGraph[1][axis].scale, intervals);
							}
							state.velocity.base[axis] += v;
							state.velocity.amplitude[axis] += a;
							any[world[set] ? 1 : 0] = any[world[set] ? 1 : 0] || v != 0 || a != 0;
						}
					}
				}
				for (int i = 1; i <= intervals; i++)
				{
					const c1::FxElemVelStateSample& a = e.velSamples[static_cast<std::size_t>(i) - 1];
					c1::FxElemVelStateSample& b = e.velSamples[static_cast<std::size_t>(i)];
					for (int axis = 0; axis < 3; axis++)
					{
						b.local.totalDelta.base[axis] = a.local.totalDelta.base[axis] + (a.local.velocity.base[axis] + b.local.velocity.base[axis]) * 0.5f;
						b.local.totalDelta.amplitude[axis] = a.local.totalDelta.amplitude[axis] + (a.local.velocity.amplitude[axis] + b.local.velocity.amplitude[axis]) * 0.5f;
						b.world.totalDelta.base[axis] = a.world.totalDelta.base[axis] + (a.world.velocity.base[axis] + b.world.velocity.base[axis]) * 0.5f;
						b.world.totalDelta.amplitude[axis] = a.world.totalDelta.amplitude[axis] + (a.world.velocity.amplitude[axis] + b.world.velocity.amplitude[axis]) * 0.5f;
					}
				}
				flags |= any[0] ? FLAG_VELOCITY_LOCAL : 0;
				flags |= any[1] ? FLAG_VELOCITY_WORLD : 0;
			}

			// ---- visual state ----
			const std::uint32_t ch = channels(d.elemType);
			const bool nonUniform = (flags & FLAG_NONUNIFORM) != 0;
			if (d.elemType != c1::FX_ELEM_TYPE_RUNNER)
			{
				const bool randColor = s.has_editor_flag("useRandColor"), randAlpha = s.has_editor_flag("useRandAlpha");
				const bool randRot = s.has_editor_flag("useRandRotDelta"), randScale = s.has_editor_flag("useRandScale");
				const bool randSize[2] = { s.has_editor_flag("useRandSize0"), s.has_editor_flag("useRandSize1") };
				const bool modColor = s.has_editor_flag("modColorByAlpha");
				std::vector<const FxCurve*> curves;
				const auto use = [&](const FxGraph& g, const bool random)
				{
					curves.push_back(&g.curve[0]);
					if (random)
					{
						curves.push_back(&g.curve[1]);
					}
				};
				if (ch & CH_ROT) use(s.rotGraph, randRot);
				if (ch & CH_SIZE0) use(s.sizeGraph[0], randSize[0]);
				if ((ch & CH_SIZE1) && nonUniform) use(s.sizeGraph[1], randSize[1]);
				const bool isLight = d.elemType == c1::FX_ELEM_TYPE_OMNI_LIGHT || d.elemType == c1::FX_ELEM_TYPE_SPOT_LIGHT;
				const bool lightScaleGraph = s.find_extra("lightScaleGraph") != nullptr;
				if ((ch & CH_SCALE) && (!isLight || lightScaleGraph)) use(s.scaleGraph, randScale);
				if (ch & CH_COLOR)
				{
					use(s.colorGraph, randColor);
					use(s.alphaGraph, randAlpha);
				}
				const int intervals = hinted ? static_cast<int>(std::clamp<std::int64_t>(hint[1], 1, 255)) : curves.empty() ? 1 : decide_intervals(curves, limit);
				d.visStateIntervalCount = static_cast<std::uint8_t>(intervals);
				d.visSamples = c1::ZPTR_INLINE;
				e.visSamples.assign(static_cast<std::size_t>(intervals) + 1, {});
				for (int i = 0; i <= intervals; i++)
				{
					const float t = static_cast<float>(i) / static_cast<float>(intervals);
					c1::FxElemVisualState& base = e.visSamples[static_cast<std::size_t>(i)].base;
					c1::FxElemVisualState& amp = e.visSamples[static_cast<std::size_t>(i)].amplitude;
					// colour: b g r a; the amplitude holds the second colour, not a difference
					float rgb[2][3] = { { 1, 1, 1 }, { 1, 1, 1 } };
					float alpha[2] = { 1, 1 };
					if (ch & CH_COLOR)
					{
						eval(s.colorGraph.curve[0], t, rgb[0]);
						eval(s.colorGraph.curve[randColor ? 1 : 0], t, rgb[1]);
						alpha[0] = eval1(s.alphaGraph.curve[0], t);
						alpha[1] = eval1(s.alphaGraph.curve[randAlpha ? 1 : 0], t);
					}
					for (int h = 0; h < 2; h++)
					{
						c1::FxElemVisualState& state = h == 0 ? base : amp;
						const float m = modColor ? alpha[h] : 1.0f;
						state.color[0] = to_byte(rgb[h][2] * m);
						state.color[1] = to_byte(rgb[h][1] * m);
						state.color[2] = to_byte(rgb[h][0] * m);
						state.color[3] = modColor ? 255 : to_byte(alpha[h]);
					}
					if (ch & CH_ROT)
					{
						const float r = eval1(s.rotGraph.curve[0], t);
						base.rotationDelta = rot_per_interval(r, s.rotGraph.scale, intervals);
						amp.rotationDelta = randRot ? rot_per_interval(eval1(s.rotGraph.curve[1], t) - r, s.rotGraph.scale, intervals) : 0.0f;
					}
					if (ch & CH_SIZE0)
					{
						const float v = eval1(s.sizeGraph[0].curve[0], t);
						base.size[0] = half_size(v, s.sizeGraph[0].scale);
						amp.size[0] = randSize[0] ? half_size(eval1(s.sizeGraph[0].curve[1], t) - v, s.sizeGraph[0].scale) : 0.0f;
					}
					if (ch & CH_SIZE1)
					{
						if (nonUniform)
						{
							const float v = eval1(s.sizeGraph[1].curve[0], t);
							base.size[1] = half_size(v, s.sizeGraph[1].scale);
							amp.size[1] = randSize[1] ? half_size(eval1(s.sizeGraph[1].curve[1], t) - v, s.sizeGraph[1].scale) : 0.0f;
						}
						else
						{
							base.size[1] = base.size[0];
							amp.size[1] = amp.size[0];
						}
					}
					if (isLight && !lightScaleGraph)
					{
						base.scale = 1.5f;		// what every light of an effect with a source has
					}
					else if (ch & CH_SCALE)
					{
						const float v = eval1(s.scaleGraph.curve[0], t);
						base.scale = scaled(v, s.scaleGraph.scale);
						amp.scale = randScale ? scaled(eval1(s.scaleGraph.curve[1], t) - v, s.scaleGraph.scale) : 0.0f;
					}
				}
				for (int i = 1; i <= intervals; i++)
				{
					const c1::FxElemVisStateSample& a = e.visSamples[static_cast<std::size_t>(i) - 1];
					c1::FxElemVisStateSample& b = e.visSamples[static_cast<std::size_t>(i)];
					b.base.rotationTotal = a.base.rotationTotal + (a.base.rotationDelta + b.base.rotationDelta) * 0.5f;
					b.amplitude.rotationTotal = a.amplitude.rotationTotal + (a.amplitude.rotationDelta + b.amplitude.rotationDelta) * 0.5f;
				}
			}

			// ---- visuals ----
			if (d.elemType == c1::FX_ELEM_TYPE_OMNI_LIGHT || d.elemType == c1::FX_ELEM_TYPE_SPOT_LIGHT)
			{
				d.visualCount = 1;
			}
			else if (d.elemType == c1::FX_ELEM_TYPE_DECAL)
			{
				// a decal names one material; the element holds the model and the world variant of each
				for (const std::string& name : s.visuals)
				{
					if (name.starts_with("mc/") || name.starts_with("wc/"))
					{
						e.visuals.push_back(name);
					}
					else
					{
						e.visuals.push_back("mc/" + name);
						e.visuals.push_back("wc/" + name);
					}
				}
				if (e.visuals.size() % 2)
				{
					return Status::error(where + "decal materials are given without prefix, or as mc/ and wc/ pairs");
				}
				d.visualCount = static_cast<std::uint8_t>(e.visuals.size() / 2);
				d.visuals = e.visuals.empty() ? 0 : c1::ZPTR_INLINE;
			}
			else
			{
				if (s.visuals.size() > 255)
				{
					return Status::error(where + "more than 255 visuals");
				}
				e.visuals = s.visuals;
				d.visualCount = static_cast<std::uint8_t>(e.visuals.size());
				d.visuals = e.visuals.empty() ? 0 : c1::ZPTR_INLINE;
			}

			d.collBounds.midPoint[0] = s.collOffset[0];
			d.collBounds.midPoint[1] = s.collOffset[1];
			d.collBounds.midPoint[2] = s.collOffset[2];
			d.collBounds.halfSize[0] = d.collBounds.halfSize[1] = d.collBounds.halfSize[2] = s.collRadius;
			if (s.has_editor_flag("playOnTouch"))
			{
				e.effectOnImpact = s.fxOnImpact;
			}
			if (s.has_editor_flag("playOnDeath"))
			{
				e.effectOnDeath = s.fxOnDeath;
			}
			if (s.has_editor_flag("playOnRun"))
			{
				e.effectEmitted = s.emission;
			}
			d.effectOnImpact = e.effectOnImpact.empty() ? 0 : c1::ZPTR_INLINE;
			d.effectOnDeath = e.effectOnDeath.empty() ? 0 : c1::ZPTR_INLINE;
			d.effectEmitted = e.effectEmitted.empty() ? 0 : c1::ZPTR_INLINE;
			d.emitDist = range(s.emitDist);
			d.emitDistVariance = range(s.emitDistVariance);

			// ---- trail, spark fountain ----
			if (d.elemType == c1::FX_ELEM_TYPE_TRAIL)
			{
				e.hasExtended = true;
				d.extended = c1::ZPTR_INLINE;
				e.trail.scrollTimeMsec = static_cast<std::int32_t>(s.trailScrollTime * 1000.0f);
				e.trail.repeatDist = s.trailRepeatDist;
				e.trail.invSplitDist = s.trailSplitDist != 0 ? 1.0f / s.trailSplitDist : 0.0f;
				e.trail.invSplitArcDist = s.trailSplitArcDist != 0 ? 1.0f / s.trailSplitArcDist : 0.0f;
				e.trail.invSplitTime = s.trailSplitTime != 0 ? 1.0f / s.trailSplitTime : 0.0f;
				// The outline: pairs of indices are its segments. A vertex gets the normal ( -dy, dx ) of its segments,
				// averaged. Where the normals of a vertex cancel - a ribbon drawn there and back - every segment gets
				// vertices of its own.
				std::vector<c1::FxTrailVertex> verts;
				for (const EditorTrailVertex& v : s.trailVerts)
				{
					verts.push_back({ { v.pos[0], v.pos[1] }, { 0, 0 }, v.texCoord });
				}
				const auto segment_normal = [&](const std::uint16_t a, const std::uint16_t b, float* n)
				{
					const float dx = verts[b].pos[0] - verts[a].pos[0], dy = verts[b].pos[1] - verts[a].pos[1];
					const float length = std::sqrt(dx * dx + dy * dy);
					n[0] = length > 0 ? -dy / length : 0.0f;
					n[1] = length > 0 ? dx / length : 0.0f;
				};
				bool valid = s.trailInds.size() % 2 == 0;
				for (const std::uint16_t index : s.trailInds)
				{
					valid = valid && index < verts.size();
				}
				if (!valid)
				{
					return Status::error(where + "trailDef: indices come in pairs and name vertices of the list");
				}
				if (const std::string* text = s.find_extra("trailNormals"))
				{
					const char* p = text->c_str();
					for (c1::FxTrailVertex& v : verts)
					{
						char* end = nullptr;
						v.normal[0] = std::strtof(p, &end);
						v.normal[1] = std::strtof(end, &end);
						p = end;
					}
					e.trailVerts = verts;
					e.trailInds = s.trailInds;
				}
				else
				{
					bool cancels = false;
					for (std::size_t k = 0; k + 1 < s.trailInds.size(); k += 2)
					{
						float n[2];
						segment_normal(s.trailInds[k], s.trailInds[k + 1], n);
						for (const std::uint16_t v : { s.trailInds[k], s.trailInds[k + 1] })
						{
							verts[v].normal[0] += n[0];
							verts[v].normal[1] += n[1];
						}
					}
					for (const std::uint16_t index : s.trailInds)
					{
						const float length = std::sqrt(verts[index].normal[0] * verts[index].normal[0] + verts[index].normal[1] * verts[index].normal[1]);
						cancels = cancels || length < 1e-4f;
					}
					if (cancels)
					{
						for (std::size_t k = 0; k + 1 < s.trailInds.size(); k += 2)
						{
							float n[2];
							segment_normal(s.trailInds[k], s.trailInds[k + 1], n);
							for (const std::uint16_t v : { s.trailInds[k], s.trailInds[k + 1] })
							{
								e.trailInds.push_back(static_cast<std::uint16_t>(e.trailVerts.size()));
								e.trailVerts.push_back({ { verts[v].pos[0], verts[v].pos[1] }, { n[0], n[1] }, verts[v].texCoord });
							}
						}
					}
					else
					{
						for (c1::FxTrailVertex& v : verts)
						{
							const float length = std::sqrt(v.normal[0] * v.normal[0] + v.normal[1] * v.normal[1]);
							if (length > 0)
							{
								v.normal[0] /= length;
								v.normal[1] /= length;
							}
						}
						e.trailVerts = verts;
						e.trailInds = s.trailInds;
					}
				}
				e.trail.vertCount = static_cast<std::int32_t>(e.trailVerts.size());
				e.trail.indCount = static_cast<std::int32_t>(e.trailInds.size());
				e.trail.verts = e.trailVerts.empty() ? 0 : c1::ZPTR_INLINE;
				e.trail.inds = e.trailInds.empty() ? 0 : c1::ZPTR_INLINE;
			}
			else if (d.elemType == c1::FX_ELEM_TYPE_SPARK_FOUNTAIN)
			{
				e.hasExtended = true;
				d.extended = c1::ZPTR_INLINE;
				if (const std::string* text = s.find_extra("sparkFountain"))
				{
					float v[13] = {};
					std::sscanf(text->c_str(), "%f %f %f %f %f %f %f %f %f %f %f %f %f", &v[0], &v[1], &v[2], &v[3], &v[4], &v[5], &v[6], &v[7], &v[8], &v[9], &v[10], &v[11], &v[12]);
					e.spark = { v[0], v[1], v[2], v[3], v[4], static_cast<std::int32_t>(v[5]), v[6], v[7], v[8], v[9], v[10], v[11], v[12] };
				}
				else
				{
					notes.push_back(where + "a spark fountain without '//#CODOL sparkFountain' values: all zero");
				}
			}

			d.sortOrder = static_cast<std::uint8_t>(s.hasSortOrder ? std::clamp(s.sortOrder, 0, 255) : 5);
			d.lightingFrac = static_cast<std::uint8_t>(std::clamp(static_cast<int>(s.lightingFrac * 255.0f), 0, 255));
			d.useItemClip = s.has_editor_flag("useItemClip") ? 1 : 0;
			if (const std::string* text = s.find_extra("fadeInfo"))
			{
				d.fadeInfo = static_cast<std::uint8_t>(std::atoi(text->c_str()));
			}
			if (const std::string* text = s.find_extra("unk252"))
			{
				const std::int32_t v = std::atoi(text->c_str());
				std::memcpy(d._unk252, &v, 4);
			}
			if (const std::string* text = s.find_extra("string256"))
			{
				e.hasString256 = true;
				e.string256 = *text;
				d._string256 = c1::ZPTR_INLINE;
			}
			d.flags = static_cast<std::int32_t>(flags);
			return {};
		}
	}

	std::int32_t fx_total_size(const CompiledFx& fx)
	{
		std::size_t size = sizeof(c1::FxEffectDef) + fx.name.size() + 1;
		for (const FxElem& e : fx.elems)
		{
			size += sizeof(c1::FxElemDef) + e.velSamples.size() * sizeof(c1::FxElemVelStateSample) + e.visSamples.size() * sizeof(c1::FxElemVisStateSample);
			if (e.def.elemType == c1::FX_ELEM_TYPE_DECAL)
			{
				size += e.visuals.size() * 4;
			}
			else if (e.visuals.size() > 1)
			{
				size += e.visuals.size() * 4;
			}
			if (e.def.elemType == c1::FX_ELEM_TYPE_TRAIL && e.hasExtended)
			{
				// counted with one vertex per index, also where the stored outline shares vertices ( shipped: 36 + 22 * indCount )
				size += sizeof(c1::FxTrailDef) + e.trailInds.size() * (sizeof(c1::FxTrailVertex) + 2);
			}
			else if (e.def.elemType == c1::FX_ELEM_TYPE_SPARK_FOUNTAIN && e.hasExtended)
			{
				size += sizeof(c1::FxSparkFountainDef);
			}
			if (e.hasString256)
			{
				size += e.string256.size() + 1;
			}
		}
		return static_cast<std::int32_t>(size);
	}

	Status compile_fx(const std::string_view name, const EditorEffect& effect, CompiledFx& out, const FxCompileOptions& options)
	{
		out = {};
		out.name = std::string(name);
		std::vector<FxElem> sections[3];
		std::set<std::string> emitted;
		for (const EditorElem& s : effect.elems)
		{
			if (s.has_editor_flag("disabled"))
			{
				continue;
			}
			const std::string* section = s.find_extra("section");
			const bool emission = section && *section == "emission";
			const bool looping = !emission && s.has_editor_flag("looping");
			FxElem e;
			if (const Status status = compile_elem(s, looping, options, e, out.notes); !status)
			{
				return Status::error(std::string(name) + ": " + status.message);
			}
			// the elements of the effects this one emits
			if (!emission && !e.effectEmitted.empty() && emitted.insert(e.effectEmitted).second)
			{
				EditorEffect child;
				if (options.loadEffect && options.loadEffect(e.effectEmitted, child))
				{
					for (const EditorElem& c : child.elems)
					{
						if (c.has_editor_flag("disabled"))
						{
							continue;
						}
						FxElem ce;
						if (const Status status = compile_elem(c, c.has_editor_flag("looping"), options, ce, out.notes); !status)
						{
							return Status::error(std::string(name) + ": emitted effect " + e.effectEmitted + ": " + status.message);
						}
						sections[2].push_back(std::move(ce));
					}
				}
				else if (!std::any_of(effect.elems.begin(), effect.elems.end(), [](const EditorElem& x) { const std::string* v = x.find_extra("section"); return v && *v == "emission"; }))
				{
					out.notes.push_back("emitted effect " + e.effectEmitted + ": no source, its elements are not in the emission section");
				}
			}
			sections[emission ? 2 : looping ? 0 : 1].push_back(std::move(e));
		}
		out.countLooping = static_cast<std::int32_t>(sections[0].size());
		out.countOneShot = static_cast<std::int32_t>(sections[1].size());
		out.countEmission = static_cast<std::int32_t>(sections[2].size());
		for (std::vector<FxElem>& list : sections)
		{
			for (FxElem& e : list)
			{
				out.elems.push_back(std::move(e));
			}
		}
		if (out.elems.empty())
		{
			return Status::error(std::string(name) + ": no elements");
		}
		for (std::int32_t i = 0; i < out.countLooping; i++)
		{
			const c1::FxElemDef& d = out.elems[static_cast<std::size_t>(i)].def;
			out.msecLoopingLife = std::max(out.msecLoopingLife, d.spawn[1] == INT32_MAX ? INT32_MAX
				: static_cast<std::int32_t>(std::min<std::int64_t>(static_cast<std::int64_t>(d.spawn[0]) * (d.spawn[1] - 1), INT32_MAX)));
		}
		for (const FxElem& e : out.elems)
		{
			out.flags |= e.def.lightingFrac != 0 ? 1 : 0;
		}
		out.totalSize = fx_total_size(out);
		return {};
	}

	// ---------------------------------------------------------------------------------------------------------------
	Status extract_fx(const CompiledFx& fx, EditorEffect& out, const bool intervalHints)
	{
		out = {};
		for (std::size_t index = 0; index < fx.elems.size(); index++)
		{
			const FxElem& e = fx.elems[index];
			const c1::FxElemDef& d = e.def;
			EditorElem s;
			s.name = "elem" + std::to_string(index);
			const bool looping = index < static_cast<std::size_t>(fx.countLooping);
			const bool emission = index >= static_cast<std::size_t>(fx.countLooping) + static_cast<std::size_t>(fx.countOneShot);
			if (d.elemType >= c1::FX_ELEM_TYPE_COUNT)
			{
				return Status::error(fx.name + ": element " + std::to_string(index) + " has the unknown type " + std::to_string(d.elemType));
			}
			if (looping)
			{
				s.editorFlags.push_back("looping");
				s.spawnLooping[0] = d.spawn[0];
				s.spawnLooping[1] = d.spawn[1] == INT32_MAX ? 0 : d.spawn[1];
			}
			else
			{
				s.spawnOneShot[0] = d.spawn[0];
				s.spawnOneShot[1] = d.spawn[1];
			}
			if (emission)
			{
				s.extra.emplace_back("section", "emission");
			}
			// flags
			std::uint32_t rest = static_cast<std::uint32_t>(d.flags) & ~FLAGS_DERIVED;
			for (const FlagName& f : FLAG_NAMES)
			{
				if ((static_cast<std::uint32_t>(d.flags) & f.mask) == f.bits)
				{
					s.flags.push_back(f.name);
					rest &= ~f.mask;
				}
			}
			if (rest)
			{
				char text[16];
				std::snprintf(text, sizeof(text), "0x%X", rest);
				s.extra.emplace_back("flagBits", text);
			}
			s.spawnRange[0] = d.spawnRange.base;
			s.spawnRange[1] = d.spawnRange.amplitude;
			s.fadeInRange[0] = d.fadeInRange.base;
			s.fadeInRange[1] = d.fadeInRange.amplitude;
			s.fadeOutRange[0] = d.fadeOutRange.base;
			s.fadeOutRange[1] = d.fadeOutRange.amplitude;
			s.spawnFrustumCullRadius = d.spawnFrustumCullRadius;
			s.spawnDelayMsec[0] = d.spawnDelayMsec.base;
			s.spawnDelayMsec[1] = d.spawnDelayMsec.amplitude;
			s.lifeSpanMsec[0] = d.lifeSpanMsec.base;
			s.lifeSpanMsec[1] = d.lifeSpanMsec.amplitude;
			const auto degrees = [](const float stored) { return solve(stored, stored / DEG_TO_RAD, radians); };
			const auto degrees_per_second = [](const float stored) { return solve(stored, stored * 1000.0f / DEG_TO_RAD, radians_per_msec); };
			for (int k = 0; k < 3; k++)
			{
				s.spawnOrigin[k][0] = d.spawnOrigin[k].base;
				s.spawnOrigin[k][1] = d.spawnOrigin[k].amplitude;
				s.spawnAngles[k][0] = degrees(d.spawnAngles[k].base);
				s.spawnAngles[k][1] = degrees(d.spawnAngles[k].amplitude);
				s.angularVelocity[k][0] = degrees_per_second(d.angularVelocity[k].base);
				s.angularVelocity[k][1] = degrees_per_second(d.angularVelocity[k].amplitude);
			}
			s.spawnOffsetRadius[0] = d.spawnOffsetRadius.base;
			s.spawnOffsetRadius[1] = d.spawnOffsetRadius.amplitude;
			s.spawnOffsetHeight[0] = d.spawnOffsetHeight.base;
			s.spawnOffsetHeight[1] = d.spawnOffsetHeight.amplitude;
			s.initialRotation[0] = degrees(d.initialRotation.base);
			s.initialRotation[1] = degrees(d.initialRotation.amplitude);
			s.gravity[0] = solve(d.gravity.base, d.gravity.base * 100.0f, percent);
			s.gravity[1] = solve(d.gravity.amplitude, d.gravity.amplitude * 100.0f, percent);
			s.elasticity[0] = d.reflectionFactor.base;
			s.elasticity[1] = d.reflectionFactor.amplitude;
			s.atlasBehavior = (d.atlas.behavior & 3) == 1 ? "startRandom" : (d.atlas.behavior & 3) == 2 ? "startIndexed" : "startFixed";
			s.atlasBehavior += d.atlas.behavior & 4 ? " playOverLife" : "";
			s.atlasBehavior += d.atlas.behavior & 8 ? " loopOnlyNTimes" : "";
			if (d.atlas.behavior & ~0xF)
			{
				s.extra.emplace_back("atlasBehaviorBits", std::to_string(d.atlas.behavior));
			}
			s.atlasIndex = d.atlas.index;
			s.atlasFps = d.atlas.fps;
			s.atlasLoopCount = d.atlas.loopCount ? d.atlas.loopCount - 1 : 0;
			s.atlasColIndexBits = d.atlas.colIndexBits;
			s.atlasRowIndexBits = d.atlas.rowIndexBits;
			s.atlasEntryCount = d.atlas.entryCount;

			// ---- graphs: one key per stored sample ----
			const auto key_time = [](const std::size_t i, const std::size_t intervals) { return static_cast<float>(i) / static_cast<float>(intervals); };
			// get( sample, second ): the stored base ( second = false ) / amplitude; fn( curve value, scale ): the compile formula
			// spread: the second curve is the amplitude itself ( second velocity graph set ), not the other end
			const auto make_graph = [&](FxGraph& g, const std::size_t samples, const auto& get, const auto& fn, const float unit, const bool spread = false)
			{
				const std::size_t intervals = samples > 1 ? samples - 1 : 1;
				float peak = 0;
				bool random = false;
				for (std::size_t i = 0; i < samples; i++)
				{
					peak = std::max({ peak, std::fabs(get(i, false)), std::fabs(get(i, false) + get(i, true)) });
					random = random || get(i, true) != 0;
				}
				g.scale = peak == 0 ? 0.0f : peak / unit;
				// a scale the editor would show: no more digits than needed
				g.curve[0].keys.clear();
				g.curve[1].keys.clear();
				for (std::size_t i = 0; i < std::max<std::size_t>(samples, 2); i++)
				{
					const std::size_t at = std::min(i, samples ? samples - 1 : 0);
					float base = 0, second = 0;
					if (samples && g.scale != 0)
					{
						const float target = get(at, false);
						base = solve(target, target / fn(1.0f, g.scale), [&](const float x) { return fn(x, g.scale); });
						second = spread ? 0.0f : base;
						if (random)
						{
							const float amp = get(at, true);
							second = spread ? solve(amp, amp / fn(1.0f, g.scale), [&](const float x) { return fn(x, g.scale); })
								: solve(amp, base + amp / fn(1.0f, g.scale), [&](const float x) { return fn(x - base, g.scale); });
						}
					}
					const float t = key_time(i, intervals);
					g.curve[0].keys.insert(g.curve[0].keys.end(), { t, base });
					g.curve[1].keys.insert(g.curve[1].keys.end(), { t, second });
				}
				return random;
			};

			const int velIntervals = std::max<int>(d.velIntervalCount, 1);
			bool randVel[2] = {};
			for (int frame = 0; frame < 2; frame++)
			{
				for (int axis = 0; axis < 3; axis++)
				{
					const auto get = [&](const std::size_t i, const bool second)
					{
						const c1::FxElemVelStateInFrame& state = frame == 0 ? e.velSamples[i].local : e.velSamples[i].world;
						return second ? state.velocity.amplitude[axis] : state.velocity.base[axis];
					};
					const auto fn = [&](const float value, const float scale) { return per_interval(value, scale, velIntervals); };
					randVel[frame] = make_graph(s.velGraph[frame][axis], e.velSamples.size(), get, fn, per_interval(1.0f, 1.0f, velIntervals), frame == 1) || randVel[frame];
				}
			}
			s.editorFlags.push_back("absVel1");		// the second graph set is the world frame's
			if (randVel[0]) s.editorFlags.push_back("useRandVel0");
			if (randVel[1]) s.editorFlags.push_back("useRandVel1");

			const std::uint32_t ch = channels(d.elemType);
			const int visIntervals = std::max<int>(d.visStateIntervalCount, 1);
			const std::size_t visCount = e.visSamples.size();
			const auto state = [&](const std::size_t i, const bool second) -> const c1::FxElemVisualState& { return second ? e.visSamples[i].amplitude : e.visSamples[i].base; };
			if (ch & CH_ROT)
			{
				if (make_graph(s.rotGraph, visCount, [&](const std::size_t i, const bool second) { return state(i, second).rotationDelta; },
						[&](const float value, const float scale) { return rot_per_interval(value, scale, visIntervals); }, rot_per_interval(1.0f, 1.0f, visIntervals)))
				{
					s.editorFlags.push_back("useRandRotDelta");
				}
			}
			for (int k = 0; k < 2; k++)
			{
				if ((ch & (k == 0 ? CH_SIZE0 : CH_SIZE1)) && (k == 0 || (static_cast<std::uint32_t>(d.flags) & FLAG_NONUNIFORM)))
				{
					if (make_graph(s.sizeGraph[k], visCount, [&](const std::size_t i, const bool second) { return state(i, second).size[k]; }, half_size, 0.5f))
					{
						s.editorFlags.push_back(k == 0 ? "useRandSize0" : "useRandSize1");
					}
				}
			}
			if (d.elemType == c1::FX_ELEM_TYPE_OMNI_LIGHT || d.elemType == c1::FX_ELEM_TYPE_SPOT_LIGHT)
			{
				s.extra.emplace_back("lightScaleGraph", "");
			}
			if (ch & CH_SCALE)
			{
				if (make_graph(s.scaleGraph, visCount, [&](const std::size_t i, const bool second) { return state(i, second).scale; }, scaled, 1.0f))
				{
					s.editorFlags.push_back("useRandScale");
				}
			}
			{
				bool randColor = false, randAlpha = false;
				for (std::size_t i = 0; i < visCount; i++)
				{
					randColor = randColor || std::memcmp(e.visSamples[i].base.color, e.visSamples[i].amplitude.color, 3) != 0;
					randAlpha = randAlpha || e.visSamples[i].base.color[3] != e.visSamples[i].amplitude.color[3];
				}
				s.colorGraph.scale = s.alphaGraph.scale = 1.0f;
				for (int h = 0; h < 2; h++)
				{
					FxCurve& color = s.colorGraph.curve[h];
					FxCurve& alpha = s.alphaGraph.curve[h];
					color.dimensions = 3;
					for (std::size_t i = 0; i < std::max<std::size_t>(visCount, 2); i++)
					{
						const float t = key_time(i, static_cast<std::size_t>(visIntervals));
						std::uint8_t c[4] = { 255, 255, 255, 255 };
						if (visCount && (ch & CH_COLOR))
						{
							std::memcpy(c, (h == 1 ? e.visSamples[std::min(i, visCount - 1)].amplitude : e.visSamples[std::min(i, visCount - 1)].base).color, 4);
						}
						color.keys.insert(color.keys.end(), { t, static_cast<float>(c[2]) / 255.0f, static_cast<float>(c[1]) / 255.0f, static_cast<float>(c[0]) / 255.0f });
						alpha.keys.insert(alpha.keys.end(), { t, static_cast<float>(c[3]) / 255.0f });
					}
				}
				if ((ch & CH_COLOR) && randColor) s.editorFlags.push_back("useRandColor");
				if ((ch & CH_COLOR) && randAlpha) s.editorFlags.push_back("useRandAlpha");
			}
			if (intervalHints)
			{
				s.extra.emplace_back("intervals", std::to_string(d.velIntervalCount) + " " + std::to_string(d.visStateIntervalCount));
			}

			s.lightingFrac = solve(static_cast<float>(d.lightingFrac), static_cast<float>(d.lightingFrac) / 255.0f,
				[](const float x) { return static_cast<float>(std::clamp(static_cast<int>(x * 255.0f), 0, 255)); });
			s.collOffset[0] = d.collBounds.midPoint[0];
			s.collOffset[1] = d.collBounds.midPoint[1];
			s.collOffset[2] = d.collBounds.midPoint[2];
			s.collRadius = d.collBounds.halfSize[0];
			s.fxOnImpact = e.effectOnImpact;
			s.fxOnDeath = e.effectOnDeath;
			s.emission = e.effectEmitted;
			if (!e.effectOnImpact.empty()) s.editorFlags.push_back("playOnTouch");
			if (!e.effectOnDeath.empty()) s.editorFlags.push_back("playOnDeath");
			if (!e.effectEmitted.empty()) s.editorFlags.push_back("playOnRun");
			if (d.useItemClip) s.editorFlags.push_back("useItemClip");
			s.sortOrder = d.sortOrder;
			s.hasSortOrder = true;
			s.emitDist[0] = d.emitDist.base;
			s.emitDist[1] = d.emitDist.amplitude;
			s.emitDistVariance[0] = d.emitDistVariance.base;
			s.emitDistVariance[1] = d.emitDistVariance.amplitude;
			if (d.elemType == c1::FX_ELEM_TYPE_TRAIL)
			{
				s.trailScrollTime = static_cast<float>(e.trail.scrollTimeMsec) / 1000.0f;
				s.trailRepeatDist = e.trail.repeatDist;
				const auto inverse = [](const float stored) { return stored != 0 ? solve(stored, 1.0f / stored, [](const float x) { return 1.0f / x; }) : 0.0f; };
				s.trailSplitDist = inverse(e.trail.invSplitDist);
				s.trailSplitArcDist = inverse(e.trail.invSplitArcDist);
				s.trailSplitTime = inverse(e.trail.invSplitTime);
				for (const c1::FxTrailVertex& v : e.trailVerts)
				{
					s.trailVerts.push_back({ { v.pos[0], v.pos[1] }, v.texCoord });
				}
				s.trailInds = e.trailInds;
				// the stored normals, for a definition that compiles back to them
				std::string normals;
				for (const c1::FxTrailVertex& v : e.trailVerts)
				{
					char text[64];
					std::snprintf(text, sizeof(text), "%s%.9g %.9g", normals.empty() ? "" : " ", v.normal[0], v.normal[1]);
					normals += text;
				}
				s.extra.emplace_back("trailNormals", normals);
			}
			else if (d.elemType == c1::FX_ELEM_TYPE_SPARK_FOUNTAIN)
			{
				char text[400];
				const c1::FxSparkFountainDef& f = e.spark;
				std::snprintf(text, sizeof(text), "%.9g %.9g %.9g %.9g %.9g %d %.9g %.9g %.9g %.9g %.9g %.9g %.9g", f.gravity, f.bounceFrac, f.bounceRand, f.sparkSpacing,
					f.sparkLength, f.sparkCount, f.loopTime, f.velMin, f.velMax, f.velConeFraction, f.restSpeed, f.boostTime, f.boostFactor);
				s.extra.emplace_back("sparkFountain", text);
			}
			if (d.fadeInfo)
			{
				s.extra.emplace_back("fadeInfo", std::to_string(d.fadeInfo));
			}
			std::int32_t unk252 = 0;
			std::memcpy(&unk252, d._unk252, 4);
			if (unk252)
			{
				s.extra.emplace_back("unk252", std::to_string(unk252));
			}
			if (e.hasString256)
			{
				s.extra.emplace_back("string256", e.string256);
			}
			// graphs the element type has no use for: constant 0
			for (FxGraph* g : { &s.velGraph[0][0], &s.velGraph[0][1], &s.velGraph[0][2], &s.velGraph[1][0], &s.velGraph[1][1], &s.velGraph[1][2], &s.rotGraph,
					 &s.sizeGraph[0], &s.sizeGraph[1], &s.scaleGraph })
			{
				for (FxCurve& c : g->curve)
				{
					if (c.keys.empty())
					{
						c.keys = { 0.0f, 0.0f, 1.0f, 0.0f };
					}
				}
			}
			s.visualType = VISUAL_TYPES[d.elemType];
			if (d.elemType == c1::FX_ELEM_TYPE_DECAL)
			{
				// pairs "mc/x", "wc/x" are written as x
				for (std::size_t k = 0; k + 1 < e.visuals.size(); k += 2)
				{
					const std::string& a = e.visuals[k];
					const std::string& b = e.visuals[k + 1];
					if (a.starts_with("mc/") && b.starts_with("wc/") && a.substr(3) == b.substr(3))
					{
						s.visuals.push_back(a.substr(3));
					}
					else
					{
						s.visuals.push_back(a);
						s.visuals.push_back(b);
					}
				}
			}
			else
			{
				s.visuals = e.visuals;
			}
			out.elems.push_back(std::move(s));
		}
		return {};
	}
}
