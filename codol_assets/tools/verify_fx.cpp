// codol_asset_verify fx-roundtrip: every distinct shipped effect -> extract_fx -> .efx text -> parse_efx -> compile_fx ->
// compared with the shipped FxEffectDef, field group by field group.
// codol_asset_verify fx-source: the shipped effects that have an .efx of the same name in a folder of effect sources
// ( raw/fx of the CoD4 mod tools ): that file compiled and compared with the shipped effect - the check of the
// converter's rules against data that this library did not write.
// codol_asset_verify fx-extract: the .efx of one shipped effect.

#include "verify.hpp"

#include "codol_assets/fx.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <functional>
#include <map>
#include <set>

using namespace codol::assets;
using namespace codol::zone;
namespace c1 = codol::c1;
namespace fs = std::filesystem;

namespace
{
	bool near(const float a, const float b)
	{
		return a == b || std::fabs(a - b) <= 2e-5f * std::max(std::fabs(a), std::fabs(b)) + 1e-9f;
	}

	template <typename T>
	bool near_all(const T& a, const T& b)
	{
		static_assert(sizeof(T) % 4 == 0);
		float x[sizeof(T) / 4], y[sizeof(T) / 4];
		std::memcpy(x, &a, sizeof(T));
		std::memcpy(y, &b, sizeof(T));
		for (std::size_t k = 0; k < sizeof(T) / 4; k++)
		{
			if (!near(x[k], y[k]))
			{
				return false;
			}
		}
		return true;
	}

	template <typename T>
	bool same(const T& a, const T& b) { return std::memcmp(&a, &b, sizeof(T)) == 0; }

	enum Group
	{
		G_TYPE, G_FLAGS, G_SPAWN, G_RANGES, G_TIMING, G_ORIGIN, G_ANGLES, G_PHYSICS, G_ATLAS, G_VISUALS, G_VEL_COUNT, G_VEL_SAMPLES, G_VIS_COUNT, G_VIS_SAMPLES,
		G_COLLISION, G_REFS, G_EMIT, G_EXTENDED, G_MISC, G_COUNT
	};
	constexpr const char* GROUP_NAMES[G_COUNT] = { "element type", "flags", "spawn ( interval / count )", "spawn range, fade ranges, cull radius", "delay, life span",
		"spawn origin, offset radius / height", "spawn angles, angular velocity, initial rotation", "gravity, elasticity", "atlas", "visuals ( count, names )",
		"velocity interval count", "velocity samples", "visual interval count", "visual samples", "collision bounds", "effect on impact / death / emitted",
		"emit distance", "trail / spark fountain definition", "sort order, lighting, item clip, fade info, +252, +256" };

	struct Totals
	{
		Rate group[G_COUNT], elem, effect, counts, headerFlags, totalSize, loopingLife;
		Rate velExact, visExact, velAlike, visAlike, elemLoose, effectLoose;
		std::uint32_t effects = 0, failed = 0, elems = 0;
		std::map<std::string, std::uint32_t> firstDifference;
	};

	// an amplitude: a difference of two curve values, so its error is that of the values it was taken from
	bool near_amplitude(const float a, const float b, const float base)
	{
		return near(a, b) || std::fabs(a - b) <= 2e-5f * std::fabs(base);
	}

	bool vis_amplitude_near(const c1::FxElemVisualState& a, const c1::FxElemVisualState& b, const c1::FxElemVisualState& base)
	{
		for (int k = 0; k < 4; k++)
		{
			if (std::abs(static_cast<int>(a.color[k]) - static_cast<int>(b.color[k])) > 1)
			{
				return false;
			}
		}
		return near_amplitude(a.rotationDelta, b.rotationDelta, base.rotationDelta) && near_amplitude(a.rotationTotal, b.rotationTotal, base.rotationTotal) &&
			near_amplitude(a.size[0], b.size[0], base.size[0]) && near_amplitude(a.size[1], b.size[1], base.size[1]) && near_amplitude(a.scale, b.scale, base.scale);
	}

	bool vel_near(const c1::FxElemVelStateInFrame& a, const c1::FxElemVelStateInFrame& b)
	{
		bool ok = true;
		for (int k = 0; k < 3; k++)
		{
			ok = ok && near(a.velocity.base[k], b.velocity.base[k]) && near_amplitude(a.velocity.amplitude[k], b.velocity.amplitude[k], a.velocity.base[k]) &&
				near_amplitude(a.totalDelta.base[k], b.totalDelta.base[k], a.velocity.base[k]) &&
				near_amplitude(a.totalDelta.amplitude[k], b.totalDelta.amplitude[k], std::max(std::fabs(a.velocity.base[k]), std::fabs(a.totalDelta.base[k])));
		}
		return ok;
	}

	bool vis_state_near(const c1::FxElemVisualState& a, const c1::FxElemVisualState& b)
	{
		for (int k = 0; k < 4; k++)
		{
			if (std::abs(static_cast<int>(a.color[k]) - static_cast<int>(b.color[k])) > 1)
			{
				return false;
			}
		}
		return near(a.rotationDelta, b.rotationDelta) && near(a.rotationTotal, b.rotationTotal) && near(a.size[0], b.size[0]) && near(a.size[1], b.size[1]) && near(a.scale, b.scale);
	}

	// Two sample lists of different length describe the same curves when one, read at the other's sample times, is
	// within 2 % of each channel's peak ( the converter itself accepts 0.02 of a curve's range ).
	bool curves_alike(const std::vector<std::vector<float>>& a, const std::vector<std::vector<float>>& b)
	{
		if (a.empty() || b.empty() || a[0].size() != b[0].size())
		{
			return a.empty() && b.empty();
		}
		const std::size_t channels = a[0].size();
		std::vector<float> peak(channels, 0.0f);
		for (const auto& sample : a)
		{
			for (std::size_t c = 0; c < channels; c++)
			{
				peak[c] = std::max(peak[c], std::fabs(sample[c]));
			}
		}
		for (std::size_t i = 0; i < a.size(); i++)
		{
			const float x = a.size() > 1 ? static_cast<float>(i) / static_cast<float>(a.size() - 1) * static_cast<float>(b.size() - 1) : 0.0f;
			const std::size_t lo = std::min(static_cast<std::size_t>(x), b.size() - 1), hi = std::min(lo + 1, b.size() - 1);
			const float f = x - static_cast<float>(lo);
			for (std::size_t c = 0; c < channels; c++)
			{
				const float v = b[lo][c] + (b[hi][c] - b[lo][c]) * f;
				if (std::fabs(v - a[i][c]) > 0.02f * peak[c] + 1e-7f)
				{
					return false;
				}
			}
		}
		return true;
	}

	std::vector<std::vector<float>> vel_curves(const FxElem& e)
	{
		std::vector<std::vector<float>> out;
		for (const c1::FxElemVelStateSample& s : e.velSamples)
		{
			std::vector<float> v(24);
			std::memcpy(v.data(), &s, 96);
			// base + amplitude instead of the amplitude: the other end of the range
			for (std::size_t k = 0; k < 24; k += 6)
			{
				for (std::size_t d = 0; d < 3; d++)
				{
					v[k + 3 + d] += v[k + d];
				}
			}
			out.push_back(std::move(v));
		}
		return out;
	}

	std::vector<std::vector<float>> vis_curves(const FxElem& e)
	{
		std::vector<std::vector<float>> out;
		for (const c1::FxElemVisStateSample& s : e.visSamples)
		{
			std::vector<float> v;
			for (const c1::FxElemVisualState* state : { &s.base, &s.amplitude })
			{
				for (const std::uint8_t c : state->color)
				{
					v.push_back(static_cast<float>(c));
				}
				const float second = state == &s.amplitude ? 1.0f : 0.0f;
				v.insert(v.end(), { state->rotationDelta * static_cast<float>(e.visSamples.size() - 1) + second * s.base.rotationDelta * static_cast<float>(e.visSamples.size() - 1),
					state->rotationTotal + second * s.base.rotationTotal, state->size[0] + second * s.base.size[0], state->size[1] + second * s.base.size[1],
					state->scale + second * s.base.scale });
			}
			out.push_back(std::move(v));
		}
		return out;
	}

	// bit n set: group n differs
	std::uint32_t compare_elem(const FxElem& a, const FxElem& b, Totals& t)
	{
		const c1::FxElemDef& x = a.def;
		const c1::FxElemDef& y = b.def;
		bool ok[G_COUNT];
		ok[G_TYPE] = x.elemType == y.elemType;
		ok[G_FLAGS] = x.flags == y.flags;
		ok[G_SPAWN] = same(x.spawn, y.spawn);
		ok[G_RANGES] = near_all(x.spawnRange, y.spawnRange) && near_all(x.fadeInRange, y.fadeInRange) && near_all(x.fadeOutRange, y.fadeOutRange) &&
			near(x.spawnFrustumCullRadius, y.spawnFrustumCullRadius);
		ok[G_TIMING] = same(x.spawnDelayMsec, y.spawnDelayMsec) && same(x.lifeSpanMsec, y.lifeSpanMsec);
		ok[G_ORIGIN] = near_all(x.spawnOrigin, y.spawnOrigin) && near_all(x.spawnOffsetRadius, y.spawnOffsetRadius) && near_all(x.spawnOffsetHeight, y.spawnOffsetHeight);
		ok[G_ANGLES] = near_all(x.spawnAngles, y.spawnAngles) && near_all(x.angularVelocity, y.angularVelocity) && near_all(x.initialRotation, y.initialRotation);
		ok[G_PHYSICS] = near_all(x.gravity, y.gravity) && near_all(x.reflectionFactor, y.reflectionFactor);
		ok[G_ATLAS] = same(x.atlas, y.atlas);
		ok[G_VISUALS] = x.visualCount == y.visualCount && a.visuals == b.visuals && (x.visuals != 0) == (y.visuals != 0);
		ok[G_VEL_COUNT] = x.velIntervalCount == y.velIntervalCount && a.velSamples.size() == b.velSamples.size();
		ok[G_VIS_COUNT] = x.visStateIntervalCount == y.visStateIntervalCount && a.visSamples.size() == b.visSamples.size();
		ok[G_VEL_SAMPLES] = ok[G_VEL_COUNT];
		bool exact = ok[G_VEL_COUNT];
		for (std::size_t i = 0; ok[G_VEL_SAMPLES] && i < a.velSamples.size(); i++)
		{
			ok[G_VEL_SAMPLES] = vel_near(a.velSamples[i].local, b.velSamples[i].local) && vel_near(a.velSamples[i].world, b.velSamples[i].world);
			exact = exact && same(a.velSamples[i], b.velSamples[i]);
		}
		t.velExact.add(exact && ok[G_VEL_SAMPLES]);
		ok[G_VIS_SAMPLES] = ok[G_VIS_COUNT];
		exact = ok[G_VIS_COUNT];
		for (std::size_t i = 0; ok[G_VIS_SAMPLES] && i < a.visSamples.size(); i++)
		{
			ok[G_VIS_SAMPLES] = vis_state_near(a.visSamples[i].base, b.visSamples[i].base) &&
				vis_amplitude_near(a.visSamples[i].amplitude, b.visSamples[i].amplitude, a.visSamples[i].base);
			exact = exact && same(a.visSamples[i], b.visSamples[i]);
		}
		t.visExact.add(exact && ok[G_VIS_SAMPLES]);
		ok[G_COLLISION] = near_all(x.collBounds, y.collBounds);
		ok[G_REFS] = a.effectOnImpact == b.effectOnImpact && a.effectOnDeath == b.effectOnDeath && a.effectEmitted == b.effectEmitted;
		ok[G_EMIT] = near_all(x.emitDist, y.emitDist) && near_all(x.emitDistVariance, y.emitDistVariance);
		ok[G_EXTENDED] = a.hasExtended == b.hasExtended && a.extendedByte == b.extendedByte;
		if (ok[G_EXTENDED] && x.elemType == c1::FX_ELEM_TYPE_TRAIL)
		{
			ok[G_EXTENDED] = a.trail.scrollTimeMsec == b.trail.scrollTimeMsec && a.trail.repeatDist == b.trail.repeatDist && near(a.trail.invSplitDist, b.trail.invSplitDist) &&
				near(a.trail.invSplitArcDist, b.trail.invSplitArcDist) && near(a.trail.invSplitTime, b.trail.invSplitTime) && a.trailInds == b.trailInds &&
				a.trailVerts.size() == b.trailVerts.size();
			for (std::size_t i = 0; ok[G_EXTENDED] && i < a.trailVerts.size(); i++)
			{
				ok[G_EXTENDED] = near_all(a.trailVerts[i], b.trailVerts[i]);
			}
		}
		if (ok[G_EXTENDED] && x.elemType == c1::FX_ELEM_TYPE_SPARK_FOUNTAIN)
		{
			ok[G_EXTENDED] = same(a.spark, b.spark);
		}
		ok[G_MISC] = x.sortOrder == y.sortOrder && x.lightingFrac == y.lightingFrac && x.useItemClip == y.useItemClip && x.fadeInfo == y.fadeInfo &&
			same(x._unk252, y._unk252) && a.hasString256 == b.hasString256 && a.string256 == b.string256;
		std::uint32_t mask = 0;
		for (int g = 0; g < G_COUNT; g++)
		{
			t.group[g].add(ok[g]);
			mask |= ok[g] ? 0 : 1u << g;
		}
		t.elem.add(mask == 0);
		t.elems++;
		// the same, but a different number of samples of the same curves passes
		std::uint32_t loose = mask;
		if (!ok[G_VEL_COUNT])
		{
			// velocities are stored per interval: compare per life
			auto va = vel_curves(a), vb = vel_curves(b);
			for (auto* list : { &va, &vb })
			{
				for (auto& sample : *list)
				{
					for (std::size_t k = 0; k < 24; k += 12)
					{
						for (std::size_t d = 0; d < 6; d++)
						{
							sample[k + d] *= static_cast<float>(list->size() - 1);
						}
					}
				}
			}
			const bool alike = curves_alike(va, vb);
			t.velAlike.add(alike);
			loose &= alike ? ~((1u << G_VEL_COUNT) | (1u << G_VEL_SAMPLES)) : ~0u;
		}
		if (!ok[G_VIS_COUNT])
		{
			const bool alike = curves_alike(vis_curves(a), vis_curves(b));
			t.visAlike.add(alike);
			loose &= alike ? ~((1u << G_VIS_COUNT) | (1u << G_VIS_SAMPLES)) : ~0u;
		}
		t.elemLoose.add(loose == 0);
		return mask | (loose ? 0x80000000u : 0);
	}

	bool compare_fx(const CompiledFx& shipped, const CompiledFx& mine, Totals& t, std::string* why)
	{
		bool all = true, loose = true;
		const bool counts = shipped.countLooping == mine.countLooping && shipped.countOneShot == mine.countOneShot && shipped.countEmission == mine.countEmission;
		t.counts.add(counts);
		t.headerFlags.add(shipped.flags == mine.flags);
		t.totalSize.add(shipped.totalSize == mine.totalSize);
		t.loopingLife.add(shipped.msecLoopingLife == mine.msecLoopingLife);
		std::string first;
		const auto note = [&](const std::string& text)
		{
			if (first.empty())
			{
				first = text;
			}
			all = false;
		};
		if (!counts) note("element counts");
		if (shipped.flags != mine.flags) note("header flags");
		if (shipped.msecLoopingLife != mine.msecLoopingLife) note("looping life " + std::to_string(shipped.msecLoopingLife) + " / " + std::to_string(mine.msecLoopingLife));
		if (counts)
		{
			for (std::size_t i = 0; i < shipped.elems.size(); i++)
			{
				const std::uint32_t mask = compare_elem(shipped.elems[i], mine.elems[i], t);
				loose = loose && !(mask & 0x80000000u);
				for (int g = 0; (mask & 0x7FFFFFFFu) && g < G_COUNT; g++)
				{
					if (mask >> g & 1)
					{
						note(std::string(GROUP_NAMES[g]) + " ( element " + std::to_string(i) + ", type " + std::to_string(shipped.elems[i].def.elemType) + " )");
						t.firstDifference[GROUP_NAMES[g]]++;
						break;
					}
				}
			}
		}
		// everything but the samples and the total size, which follows their number
		t.effectLoose.add(loose && counts && shipped.flags == mine.flags && shipped.msecLoopingLife == mine.msecLoopingLife);
		if (shipped.totalSize != mine.totalSize) note("total size " + std::to_string(shipped.totalSize) + " / " + std::to_string(mine.totalSize));
		t.effect.add(all);
		if (why)
		{
			*why = first;
		}
		return all;
	}

	void print_totals(const Totals& t)
	{
		std::printf("%u effects, %u elements; %u could not be compiled\n", t.effects, t.elems, t.failed);
		print_rate("header: element counts ( looping / one shot / emission )", t.counts);
		print_rate("header: flags", t.headerFlags);
		print_rate("header: looping life", t.loopingLife);
		print_rate("header: total size", t.totalSize);
		for (int g = 0; g < G_COUNT; g++)
		{
			print_rate(GROUP_NAMES[g], t.group[g]);
		}
		print_rate("velocity samples, bit exact", t.velExact);
		print_rate("visual samples, bit exact", t.visExact);
		print_rate("whole element", t.elem);
		print_rate("whole effect", t.effect);
		if (t.velAlike.total || t.visAlike.total)
		{
			print_rate("other velocity interval count: same curves within 2 %", t.velAlike);
			print_rate("other visual interval count: same curves within 2 %", t.visAlike);
			print_rate("whole element, a different sampling of the same curves allowed", t.elemLoose);
			print_rate("whole effect, a different sampling of the same curves allowed", t.effectLoose);
		}
		for (const auto& [name, count] : t.firstDifference)
		{
			std::printf("  first difference of an element: %-60s %u\n", name.c_str(), count);
		}
	}

	template <typename F>
	bool for_each_shipped_fx(const fs::path& install, const fs::path& exe, const std::string& filter, F&& f, FxAtlasLibrary* atlas = nullptr)
	{
		ClientKey key;
		if (const Status s = key.load_from_exe(exe); !s)
		{
			std::fprintf(stderr, "%s\n", s.message.c_str());
			return false;
		}
		std::set<std::string> seen;
		for (const fs::path& file : install_zones(install))
		{
			if (!filter.empty() && file.stem().string().find(filter) == std::string::npos)
			{
				continue;
			}
			Zone zone;
			if (!zone.open(file, key))
			{
				continue;
			}
			for (const AssetRecord& r : zone.all_assets())
			{
				c1::Material material;
				if (atlas && r.type == c1::ASSET_TYPE_MATERIAL && !zone.reference(r.header) && zone.get(r.header, material))
				{
					// MaterialInfo: texture atlas row and column count behind the sort key
					atlas->set(zone.asset_name(r.type, r.header), material._unk6[1], material._unk6[0]);
				}
				CompiledFx fx;
				// ( a name with a comma in front: an effect of another zone, nothing to compare )
				if (r.type == c1::ASSET_TYPE_FX && !zone.reference(r.header) && read_fx(zone, r.header, fx) && !fx.name.starts_with(',') && !fx.elems.empty() &&
					seen.insert(fx.name).second)
				{
					f(fx, file);
				}
			}
		}
		return true;
	}
}

int fx_roundtrip(const fs::path& install, const fs::path& exe, const bool hints, const std::string& filter)
{
	Totals totals;
	std::uint32_t shown = 0;
	const bool ok = for_each_shipped_fx(install, exe, filter, [&](const CompiledFx& shipped, const fs::path& file)
	{
		totals.effects++;
		EditorEffect extracted, parsed;
		CompiledFx compiled;
		Status status = extract_fx(shipped, extracted, hints);
		// through the text, as a modder would have it
		status = status ? parse_efx(write_efx(extracted), parsed) : status;
		status = status ? compile_fx(shipped.name, parsed, compiled) : status;
		if (!status)
		{
			totals.failed++;
			if (shown++ < 20)
			{
				std::fprintf(stderr, "%s ( %s ): %s\n", shipped.name.c_str(), file.stem().string().c_str(), status.message.c_str());
			}
			return;
		}
		std::string why;
		if (!compare_fx(shipped, compiled, totals, &why) && shown++ < 20)
		{
			std::fprintf(stderr, "%s ( %s ): first difference: %s\n", shipped.name.c_str(), file.stem().string().c_str(), why.c_str());
		}
	});
	if (!ok)
	{
		return 1;
	}
	std::printf("fx-roundtrip%s\n", hints ? ", interval counts given" : "");
	print_totals(totals);
	return 0;
}

int fx_source(const fs::path& install, const fs::path& exe, const fs::path& fxRoot, const std::string& filter)
{
	Totals totals;
	std::uint32_t shown = 0;
	FxAtlasLibrary atlas;
	std::vector<CompiledFx> shippedList;
	FxCompileOptions options;
	options.loadEffect = [&](const std::string_view name, EditorEffect& effect)
	{
		return static_cast<bool>(load_efx(fxRoot / (std::string(name) + ".efx"), effect));
	};
	options.materialAtlas = [&](const std::string_view material, int& columns, int& rows) { return atlas.find(material, columns, rows); };
	const bool ok = for_each_shipped_fx(install, exe, "", [&](const CompiledFx& shipped, const fs::path&)
	{
		std::error_code ec;
		if (fs::exists(fxRoot / (shipped.name + ".efx"), ec) && (filter.empty() || shipped.name.find(filter) != std::string::npos))
		{
			shippedList.push_back(shipped);
		}
	}, &atlas);
	if (!ok)
	{
		return 1;
	}
	for (const CompiledFx& shipped : shippedList)
	{
		totals.effects++;
		EditorEffect effect;
		CompiledFx compiled;
		Status status = load_efx(fxRoot / (shipped.name + ".efx"), effect);
		status = status ? compile_fx(shipped.name, effect, compiled, options) : status;
		if (!status)
		{
			totals.failed++;
			if (shown++ < 20)
			{
				std::fprintf(stderr, "%s: %s\n", shipped.name.c_str(), status.message.c_str());
			}
			continue;
		}
		std::string why;
		if (!compare_fx(shipped, compiled, totals, &why) && shown++ < 40)
		{
			std::fprintf(stderr, "%s: first difference: %s\n", shipped.name.c_str(), why.c_str());
		}
	}
	std::printf("fx-source: %u shipped effects have a source file of their name under %s\n", totals.effects, fxRoot.string().c_str());
	print_totals(totals);
	return 0;
}

// Every n-th shipped effect -> definition text -> compiled -> linked into one new zone -> the zone written, loaded by
// the game's loaders and read back: what comes out of the loaders has to be what was compiled.
int fx_link(const fs::path& install, const fs::path& exe, const fs::path& cache, const int every, const fs::path& zoneOut)
{
	LinkOptions options;
	options.install = install;
	options.exe = exe;
	options.libraryCache = cache;
	options.zoneName = "codol_fx_roundtrip";
	AssetLinker linker;
	if (const Status s = linker.begin(options); !s)
	{
		std::fprintf(stderr, "%s\n", s.message.c_str());
		return 1;
	}
	std::vector<CompiledFx> compiled;
	std::uint32_t index = 0, failed = 0;
	std::vector<std::string> notes;
	const bool ok = for_each_shipped_fx(install, exe, "", [&](const CompiledFx& shipped, const fs::path&)
	{
		if ((index++ % static_cast<std::uint32_t>(every)) != 0)
		{
			return;
		}
		EditorEffect extracted, parsed;
		CompiledFx fx;
		Status status = extract_fx(shipped, extracted, true);
		status = status ? parse_efx(write_efx(extracted), parsed) : status;
		status = status ? compile_fx(shipped.name, parsed, fx) : status;
		status = status ? link_fx(linker.editor(), fx, &notes) : status;
		if (!status)
		{
			failed++;
			std::fprintf(stderr, "%s: %s\n", shipped.name.c_str(), status.message.c_str());
			return;
		}
		compiled.push_back(std::move(fx));
	});
	if (!ok)
	{
		return 1;
	}
	std::vector<std::uint8_t> zoneBytes;
	if (const Status s = linker.write_zone(zoneBytes); !s)
	{
		std::fprintf(stderr, "the loaders do not take the zone: %s\n", s.message.c_str());
		return 1;
	}
	if (!zoneOut.empty())
	{
		if (const Status s = linker.write(zoneOut); !s)
		{
			std::fprintf(stderr, "%s\n", s.message.c_str());
			return 1;
		}
	}
	const std::size_t zoneSize = zoneBytes.size();
	Zone zone;
	if (const Status s = zone.load(std::move(zoneBytes), options.zoneName); !s)
	{
		std::fprintf(stderr, "%s\n", s.message.c_str());
		return 1;
	}
	Totals totals;
	std::uint32_t missing = 0, shown = 0;
	for (const CompiledFx& fx : compiled)
	{
		CompiledFx back;
		totals.effects++;
		if (!read_fx(zone, zone.find_asset(c1::ASSET_TYPE_FX, fx.name), back))
		{
			missing++;
			continue;
		}
		std::string why;
		if (!compare_fx(fx, back, totals, &why) && shown++ < 20)
		{
			std::fprintf(stderr, "%s: after the loaders: %s\n", fx.name.c_str(), why.c_str());
		}
	}
	std::printf("fx-link: %zu effects linked ( %u failed ), zone %zu bytes, %zu by-name references; read back from the loaded zone: %u missing\n", compiled.size(), failed,
		zoneSize, notes.size(), missing);
	print_rate("element counts", totals.counts);
	print_rate("whole element", totals.elem);
	print_rate("whole effect ( header, elements, names of visuals and effects )", totals.effect);
	return failed || missing || totals.effect.equal != totals.effect.total ? 1 : 0;
}

int fx_extract(const fs::path& zoneFile, const fs::path& exe, const std::string& name, const fs::path& out)
{
	ClientKey key;
	Zone zone;
	if (const Status s = key.load_from_exe(exe); !s)
	{
		std::fprintf(stderr, "%s\n", s.message.c_str());
		return 1;
	}
	if (const Status s = zone.open(zoneFile, key); !s)
	{
		std::fprintf(stderr, "%s\n", s.message.c_str());
		return 1;
	}
	CompiledFx fx;
	EditorEffect effect;
	if (!read_fx(zone, zone.find_asset(c1::ASSET_TYPE_FX, name), fx))
	{
		std::fprintf(stderr, "no effect '%s' in %s\n", name.c_str(), zoneFile.string().c_str());
		return 1;
	}
	if (const Status s = extract_fx(fx, effect, true); !s)
	{
		std::fprintf(stderr, "%s\n", s.message.c_str());
		return 1;
	}
	std::ofstream file(out, std::ios::binary);
	file << write_efx(effect);
	std::printf("%s: %zu elements ( %d looping, %d one shot, %d emission ) -> %s\n", name.c_str(), fx.elems.size(), fx.countLooping, fx.countOneShot, fx.countEmission,
		out.string().c_str());
	return file.good() ? 0 : 1;
}
