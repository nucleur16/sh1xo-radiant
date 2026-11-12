// codol_asset_verify sound-survey:    what the shipped sound assets are ( the numbers of sound.hpp and the README )
// codol_asset_verify sound-roundtrip: every distinct shipped alias list -> csv text -> parsed -> compiled -> compared
//                                     with the shipped aliases; every distinct loaded sound -> .wav bytes -> parsed ->
//                                     compiled -> compared; the ADPCM coder against shipped ADPCM data
// codol_asset_verify sound-extract:   the alias lists, loaded sounds, curves and speaker maps of a zone as source files
// codol_asset_verify sound-zone:      the sounds of a shipped zone linked into a new zone, that zone loaded with the
//                                     game's loaders and compared with the shipped assets

#include "verify.hpp"

#include "codol_assets/sound.hpp"
#include "codol_zone/assets.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <set>

using namespace codol::assets;
using namespace codol::zone;
namespace c1 = codol::c1;
namespace fs = std::filesystem;

namespace
{
	template <typename K>
	void top(const char* title, const std::map<K, std::uint32_t>& m, const std::size_t n = 40)
	{
		std::vector<std::pair<std::uint32_t, K>> v;
		for (const auto& [k, c] : m)
		{
			v.push_back({ c, k });
		}
		std::sort(v.begin(), v.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
		std::printf("%s ( %zu distinct )\n", title, v.size());
		for (std::size_t i = 0; i < v.size() && i < n; i++)
		{
			if constexpr (std::is_same_v<K, std::string>)
			{
				std::printf("  %8u  %s\n", v[i].first, v[i].second.c_str());
			}
			else
			{
				std::printf("  %8u  %g\n", v[i].first, static_cast<double>(v[i].second));
			}
		}
	}

	bool is_reference(const Zone& zone, const zaddr header)
	{
		const std::string_view name = zone.c_string(zone.get<c1::zptr>(header));
		return !name.empty() && name.front() == ',';
	}

	struct Totals
	{
		Rate header, strings, file, curve, speakerMap, wholeAlias, wholeList;
		Rate soundInfo, soundData, wholeSound;
		std::uint32_t failed = 0;
	};

	bool same_alias(const CompiledAlias& a, const CompiledAlias& b, Totals* t)
	{
		const bool header = std::memcmp(&a.raw, &b.raw, sizeof a.raw) == 0;
		const bool strings = a.aliasName == b.aliasName && a.subtitle == b.subtitle && a.secondaryAliasName == b.secondaryAliasName && a.chainAliasName == b.chainAliasName
			&& a.mixerGroup == b.mixerGroup;
		const bool file = a.hasFile == b.hasFile && a.streamed == b.streamed && a.loadedName == b.loadedName && a.streamDir == b.streamDir && a.streamName == b.streamName;
		const bool curve = a.curve == b.curve;
		const bool map = a.speakerMap.name == b.speakerMap.name && a.speakerMap.isDefault == b.speakerMap.isDefault
			&& std::memcmp(a.speakerMap.channelMaps, b.speakerMap.channelMaps, sizeof a.speakerMap.channelMaps) == 0;
		if (t)
		{
			t->header.add(header);
			t->strings.add(strings);
			t->file.add(file);
			t->curve.add(curve);
			t->speakerMap.add(map);
			t->wholeAlias.add(header && strings && file && curve && map);
		}
		return header && strings && file && curve && map;
	}

	bool same_list(const CompiledAliasList& a, const CompiledAliasList& b, Totals* t)
	{
		bool all = a.name == b.name && a.aliases.size() == b.aliases.size();
		for (std::size_t k = 0; k < a.aliases.size() && k < b.aliases.size(); k++)
		{
			all = same_alias(a.aliases[k], b.aliases[k], t) && all;
		}
		return all;
	}

	bool same_sound(const CompiledLoadedSound& a, const CompiledLoadedSound& b, Totals* t)
	{
		const bool info = a.name == b.name && std::memcmp(&a.info, &b.info, sizeof a.info) == 0;
		const bool data = a.data == b.data;
		if (t)
		{
			t->soundInfo.add(info);
			t->soundData.add(data);
			t->wholeSound.add(info && data);
		}
		return info && data;
	}

	// the speaker maps a list uses, as the library a compile needs
	void add_maps(const CompiledAliasList& list, SoundLibrary& library)
	{
		for (const CompiledAlias& a : list.aliases)
		{
			if (!a.speakerMap.name.empty())
			{
				std::string key = a.speakerMap.name;
				std::transform(key.begin(), key.end(), key.begin(), [](const char c) { return static_cast<char>(c >= 'A' && c <= 'Z' ? c + 32 : c); });
				library.speakerMaps.emplace(key, a.speakerMap);
			}
		}
	}

	// csv text -> compiled lists, as codol_assetc does it
	Status through_text(const CompiledAliasList& shipped, std::vector<CompiledAliasList>& out)
	{
		const std::vector<SoundAliasDef> defs = alias_list_defs(shipped);
		std::vector<SoundAliasDef> parsed;
		if (const Status s = parse_sound_aliases(write_sound_aliases(defs), parsed); !s)
		{
			return s;
		}
		// the speaker maps go through their text form as well
		SoundLibrary library, text;
		add_maps(shipped, library);
		for (const auto& [name, map] : library.speakerMaps)
		{
			SpeakerMapDef again;
			if (const Status s = parse_speaker_map(write_speaker_map(map), again); !s)
			{
				return s;
			}
			again.name = map.name;
			text.speakerMaps.emplace(name, again);
		}
		return compile_sound_aliases(parsed, text, out);
	}

	double snr_db(const WaveData& a, const WaveData& b)
	{
		const std::size_t n = std::min(a.data.size(), b.data.size()) / 2;
		double signal = 0, noise = 0;
		for (std::size_t k = 0; k < n; k++)
		{
			const double x = static_cast<std::int16_t>(a.data[k * 2] | (a.data[k * 2 + 1] << 8));
			const double y = static_cast<std::int16_t>(b.data[k * 2] | (b.data[k * 2 + 1] << 8));
			signal += x * x;
			noise += (x - y) * (x - y);
		}
		return noise <= 0 ? 99.0 : signal <= 0 ? 0.0 : 10.0 * std::log10(signal / noise);
	}
}

int sound_survey(const fs::path& install, const fs::path& exe, const std::string& filter)
{
	ClientKey key;
	if (const Status s = key.load_from_exe(exe); !s)
	{
		std::fprintf(stderr, "%s\n", s.message.c_str());
		return 1;
	}
	std::map<std::string, std::uint32_t> perZone, curves, speakerMaps, mixers, loadedFormat, exts, fileKinds, nullness;
	std::map<std::uint32_t, std::uint32_t> flagBits, channels, types, upper, masterPrio, counts;
	std::set<std::string> seenList, seenLoaded, seenCurve;
	std::uint32_t lists = 0, aliases = 0, listRefs = 0, loadedTop = 0, loadedNested = 0, loadedRefs = 0, curveData = 0, curveRefs = 0, caseDiffers = 0, nonDefault = 0;
	std::uint64_t loadedBytes = 0;
	for (const fs::path& file : install_zones(install))
	{
		if (!filter.empty() && file.stem().string().find(filter) == std::string::npos)
		{
			continue;
		}
		ZoneEditor editor;
		if (!editor.open(file, key))
		{
			continue;
		}
		const Zone& zone = editor.zone();
		std::uint32_t here = 0;
		for (const AssetRecord& r : zone.all_assets())
		{
			if (r.type == c1::ASSET_TYPE_SOUND_CURVE)
			{
				SndCurveDef curve;
				if (!extract_snd_curve(zone, r.header, curve))
				{
					curveRefs++;
				}
				else
				{
					curveData++;
					if (seenCurve.insert(curve.name).second)
					{
						std::printf("curve %s ( %s ):", curve.name.c_str(), file.stem().string().c_str());
						for (const auto& [x, y] : curve.knots)
						{
							std::printf(" %g,%g", x, y);
						}
						std::printf("\n");
					}
				}
				continue;
			}
			if (r.type == c1::ASSET_TYPE_LOADED_SOUND)
			{
				CompiledLoadedSound sound;
				if (!extract_loaded_sound(editor, r.header, sound))
				{
					loadedRefs++;
					continue;
				}
				bool isTop = false;
				for (const XAssetEntry& e : zone.assets())
				{
					isTop = isTop || (e.type == r.type && e.header == r.header);
				}
				(isTop ? loadedTop : loadedNested)++;
				if (seenLoaded.insert(sound.name).second)
				{
					char text[160];
					std::snprintf(text, sizeof text, "format %d, %d bits, %d channel(s), mask 0x%X, %u Hz, block %u", sound.info.format, sound.info.bits, sound.info.channels,
						sound.info.channelMask, sound.info.rate, sound.info.blockSize);
					loadedFormat[text]++;
					loadedBytes += sound.info.dataLen;
					const auto dot = sound.name.rfind('.');
					exts["loaded " + (dot == std::string::npos ? std::string("(none)") : sound.name.substr(dot))]++;
				}
				continue;
			}
			if (r.type != c1::ASSET_TYPE_SOUND)
			{
				continue;
			}
			if (is_reference(zone, r.header))
			{
				listRefs++;
				continue;
			}
			const auto list = zone.get<c1::snd_alias_list_t>(r.header);
			const std::string listName(zone.c_string(list.aliasName));
			here++;
			if (!seenList.insert(listName).second)
			{
				continue;
			}
			lists++;
			counts[static_cast<std::uint32_t>(list.count)]++;
			for (const c1::snd_alias_t& a : zone.get_array<c1::snd_alias_t>(list.head, static_cast<std::size_t>(list.count)))
			{
				aliases++;
				caseDiffers += zone.c_string(a.aliasName) != listName ? 1 : 0;
				mixers[a.mixerGroup ? std::string(zone.c_string(a.mixerGroup)) : "(none)"]++;
				for (std::uint32_t b = 0; b < 7; b++)
				{
					if (a.flags & (1u << b))
					{
						flagBits[b]++;
					}
				}
				if ((a.flags & 0x21) == 0x20)
				{
					nullness["bit 5 without bit 0"]++;
				}
				types[(a.flags >> 7) & 3]++;
				channels[(a.flags >> 9) & 0x3F]++;
				upper[a.flags >> 15]++;
				masterPrio[a.masterPriority]++;
				nonDefault += (a.sequence || a.velocityMin != 0 || a.envelopMin != 0 || a.envelopMax != 0 || a.envelopPercentage != 0 || a._pad61[0] || a._pad61[1] || a._pad61[2]) ? 1 : 0;
				const auto empty_string = [&](const c1::zptr p) { return p != 0 && zone.c_string(p).empty(); };
				nullness["subtitle: pointer to an empty string"] += empty_string(a.subtitle) ? 1 : 0;
				nullness["secondary: pointer to an empty string"] += empty_string(a.secondaryAliasName) ? 1 : 0;
				nullness["chain: pointer to an empty string"] += empty_string(a.chainAliasName) ? 1 : 0;
				nullness["mixer group: pointer to an empty string"] += empty_string(a.mixerGroup) ? 1 : 0;
				nullness["alias name: null"] += a.aliasName ? 0 : 1;
				nullness["curve: null"] += a.volumeFalloffCurve ? 0 : 1;
				nullness["speaker map: null"] += a.speakerMap ? 0 : 1;
				if (a.volumeFalloffCurve)
				{
					std::string n(zone.c_string(zone.get<c1::zptr>(a.volumeFalloffCurve)));
					curves[!n.empty() && n.front() == ',' ? n.substr(1) + " ( by name )" : n + " ( data in the zone )"]++;
				}
				if (a.speakerMap)
				{
					const auto m = zone.get<c1::SpeakerMap>(a.speakerMap);
					speakerMaps[std::string(zone.c_string(m.name)) + (m.isDefault ? " ( default )" : "")]++;
					nullness["speaker map name: null"] += m.name ? 0 : 1;
				}
				if (a.soundFile)
				{
					const auto f = zone.get<c1::SoundFile>(a.soundFile);
					char text[64];
					std::snprintf(text, sizeof text, "SoundFile type %u exists %u, alias type %u", f.type, f.exists, (a.flags >> 7) & 3);
					fileKinds[text]++;
					if (f.type == c1::SAT_LOADED)
					{
						fileKinds[is_reference(zone, f.loadSndOrDir) ? "loaded sound by name" : "loaded sound with data in the zone"]++;
					}
					else
					{
						nullness["stream directory: null"] += f.loadSndOrDir ? 0 : 1;
						nullness["stream directory: empty string"] += empty_string(f.loadSndOrDir) ? 1 : 0;
						const std::string n(zone.c_string(f.streamName));
						const auto dot = n.rfind('.');
						exts["streamed " + (dot == std::string::npos ? std::string("(none)") : n.substr(dot))]++;
					}
				}
				else
				{
					fileKinds["no SoundFile"]++;
				}
			}
		}
		if (here)
		{
			perZone[file.stem().string()] = here;
		}
	}
	std::printf("alias lists %u ( distinct names ), aliases %u, lists by name %u; alias name differs from the list name ( case ) %u\n", lists, aliases, listRefs, caseDiffers);
	std::printf("loaded sounds: top level %u, nested %u, by name %u, distinct %zu, %llu bytes\n", loadedTop, loadedNested, loadedRefs, seenLoaded.size(),
		static_cast<unsigned long long>(loadedBytes));
	std::printf("curves: with data %u, by name %u\n", curveData, curveRefs);
	std::printf("aliases with sequence, velocityMin, envelop* or the bytes behind masterPriority not 0: %u\n", nonDefault);
	top("alias lists per zone", perZone, 12);
	top("aliases per list", counts, 8);
	top("flag bits 0 .. 6", flagBits);
	top("type ( bits 7 - 8 )", types);
	top("channel ( bits 9 - 14 )", channels, 64);
	top("flags >> 15", upper);
	top("master priority", masterPrio);
	top("mixer group", mixers);
	top("curves", curves);
	top("speaker maps", speakerMaps);
	top("sound files", fileKinds);
	top("loaded sound formats", loadedFormat, 60);
	top("file extensions", exts);
	top("pointers", nullness);
	return 0;
}

int sound_roundtrip(const fs::path& install, const fs::path& exe, const int every, const std::string& filter)
{
	ClientKey key;
	if (const Status s = key.load_from_exe(exe); !s)
	{
		std::fprintf(stderr, "%s\n", s.message.c_str());
		return 1;
	}
	Totals totals;
	std::set<std::string> seenList, seenSound;
	std::uint32_t index = 0, shown = 0, coded = 0, codedSounds = 0;
	double snrSum = 0, snrMin = 1e9;
	for (const fs::path& file : install_zones(install))
	{
		if (!filter.empty() && file.stem().string().find(filter) == std::string::npos)
		{
			continue;
		}
		ZoneEditor editor;
		if (!editor.open(file, key))
		{
			continue;
		}
		const Zone& zone = editor.zone();
		for (const AssetRecord& r : zone.all_assets())
		{
			if (r.type == c1::ASSET_TYPE_SOUND)
			{
				CompiledAliasList shipped;
				if (!extract_sound_alias_list(zone, r.header, shipped) || !seenList.insert(shipped.name).second || (index++ % static_cast<std::uint32_t>(every)) != 0)
				{
					continue;
				}
				std::vector<CompiledAliasList> compiled;
				const Status status = through_text(shipped, compiled);
				if (!status || compiled.size() != 1)
				{
					totals.failed++;
					totals.wholeList.add(false);
					if (shown++ < 10)
					{
						std::printf("  %s: %s\n", shipped.name.c_str(), status ? "the rows do not make one list" : status.message.c_str());
					}
					continue;
				}
				const bool same = same_list(shipped, compiled[0], &totals);
				totals.wholeList.add(same);
				if (!same && shown++ < 10)
				{
					std::printf("  differs: %s ( %s )\n", shipped.name.c_str(), file.stem().string().c_str());
				}
			}
			else if (r.type == c1::ASSET_TYPE_LOADED_SOUND)
			{
				CompiledLoadedSound shipped;
				if (!extract_loaded_sound(editor, r.header, shipped) || !seenSound.insert(shipped.name).second || (seenSound.size() % static_cast<std::size_t>(every)) != 0)
				{
					continue;
				}
				// through the bytes of a .wav file, as a modder would have it
				WaveData wave;
				CompiledLoadedSound compiled;
				Status status = parse_wav(write_wav(loaded_sound_wave(shipped)), wave);
				status = status ? compile_loaded_sound(shipped.name, wave, compiled) : status;
				if (!status)
				{
					totals.failed++;
					totals.wholeSound.add(false);
					if (shown++ < 10)
					{
						std::printf("  %s: %s\n", shipped.name.c_str(), status.message.c_str());
					}
					continue;
				}
				if (!same_sound(shipped, compiled, &totals) && shown++ < 10)
				{
					std::printf("  differs: %s: samples %u / %u, block %u / %u, len %u / %u\n", shipped.name.c_str(), shipped.info.samples, compiled.info.samples,
						shipped.info.blockSize, compiled.info.blockSize, shipped.info.dataLen, compiled.info.dataLen);
				}
				// the coder: shipped ADPCM -> PCM -> ADPCM ( same block size ) -> PCM against the first PCM
				if (shipped.info.format == WAVE_IMA_ADPCM && (codedSounds++ % 8) == 0)
				{
					WaveData pcm, again, pcm2;
					if (ima_adpcm_decode(wave, pcm) && ima_adpcm_encode(pcm, wave.blockAlign, again) && ima_adpcm_decode(again, pcm2) && again.data.size() == wave.data.size())
					{
						const double snr = snr_db(pcm, pcm2);
						snrSum += snr;
						snrMin = std::min(snrMin, snr);
						coded++;
					}
				}
			}
		}
	}
	std::printf("alias lists: %u distinct, %u compared, %u failed to compile\n", static_cast<unsigned>(seenList.size()), totals.wholeList.total, totals.failed);
	print_rate("alias struct ( the 76 non pointer bytes )", totals.header);
	print_rate("alias strings ( name, subtitle, secondary, chain, mixer group )", totals.strings);
	print_rate("sound file ( type, loaded sound name / stream directory + file )", totals.file);
	print_rate("volume falloff curve name", totals.curve);
	print_rate("speaker map ( name, default flag, 400 bytes of levels )", totals.speakerMap);
	print_rate("whole alias", totals.wholeAlias);
	print_rate("whole alias list", totals.wholeList);
	std::printf("loaded sounds: %u distinct\n", static_cast<unsigned>(seenSound.size()));
	print_rate("loaded sound info ( name, format, rate, bits, channels, samples, block size, length )", totals.soundInfo);
	print_rate("loaded sound data", totals.soundData);
	print_rate("whole loaded sound", totals.wholeSound);
	if (coded)
	{
		std::printf("ADPCM coder: %u shipped ADPCM sounds decoded, encoded again and decoded: signal to noise mean %.1f dB, min %.1f dB\n", coded, snrSum / coded, snrMin);
	}
	return 0;
}

int sound_extract(const fs::path& zoneFile, const fs::path& exe, const fs::path& outDir, const std::string& filter)
{
	ClientKey key;
	ZoneEditor source;
	if (Status s = key.load_from_exe(exe); !s || !(s = source.open(zoneFile, key)))
	{
		std::fprintf(stderr, "%s\n", s.message.c_str());
		return 1;
	}
	const Zone& zone = source.zone();
	std::vector<SoundAliasDef> rows;
	SoundLibrary library;
	std::set<std::string> done;
	std::uint32_t sounds = 0, lists = 0, curves = 0;
	const auto put = [&](const fs::path& path, const std::span<const std::uint8_t> bytes)
	{
		std::error_code ec;
		fs::create_directories(path.parent_path(), ec);
		return static_cast<bool>(write_file(path, bytes));
	};
	const auto put_text = [&](const fs::path& path, const std::string& text)
	{
		return put(path, { reinterpret_cast<const std::uint8_t*>(text.data()), text.size() });
	};
	for (const AssetRecord& r : zone.all_assets())
	{
		if (r.type == c1::ASSET_TYPE_SOUND)
		{
			CompiledAliasList list;
			if (extract_sound_alias_list(zone, r.header, list) && list.name.find(filter) != std::string::npos && done.insert("a:" + list.name).second)
			{
				const std::vector<SoundAliasDef> defs = alias_list_defs(list);
				rows.insert(rows.end(), defs.begin(), defs.end());
				add_maps(list, library);
				lists++;
			}
		}
		else if (r.type == c1::ASSET_TYPE_LOADED_SOUND)
		{
			CompiledLoadedSound sound;
			if (extract_loaded_sound(source, r.header, sound) && sound.name.find(filter) != std::string::npos && done.insert("s:" + sound.name).second)
			{
				sounds += put(outDir / "sound" / fs::path(sound.name), write_wav(loaded_sound_wave(sound))) ? 1 : 0;
			}
		}
		else if (r.type == c1::ASSET_TYPE_SOUND_CURVE)
		{
			SndCurveDef curve;
			if (extract_snd_curve(zone, r.header, curve) && done.insert("c:" + curve.name).second)
			{
				curves += put_text(outDir / "soundaliases" / (curve.name + ".vfcurve"), write_snd_curve(curve)) ? 1 : 0;
			}
		}
	}
	for (const auto& [name, map] : library.speakerMaps)
	{
		put_text(outDir / "soundaliases" / (name + ".spkrmap"), write_speaker_map(map));
	}
	put_text(outDir / "soundaliases" / (zoneFile.stem().string() + ".csv"), write_sound_aliases(rows));
	std::printf("%s: %u alias lists ( %zu rows ), %u loaded sounds, %u curves, %zu speaker maps -> %s\n", zoneFile.filename().string().c_str(), lists, rows.size(), sounds,
		curves, library.speakerMaps.size(), outDir.string().c_str());
	return 0;
}

int sound_zone(const fs::path& zoneFile, const fs::path& exe, const fs::path& outFile, const int limit)
{
	ClientKey key;
	ZoneEditor source;
	if (Status s = key.load_from_exe(exe); !s || !(s = source.open(zoneFile, key)))
	{
		std::fprintf(stderr, "%s\n", s.message.c_str());
		return 1;
	}
	const Zone& zone = source.zone();
	// source side: every list and sound of the zone, through their text / .wav form
	std::vector<CompiledAliasList> shippedLists;
	std::map<std::string, CompiledLoadedSound> shippedSounds;
	std::map<std::string, SndCurveDef> shippedCurves;
	std::set<std::string> seen;
	for (const AssetRecord& r : zone.all_assets())
	{
		CompiledAliasList list;
		CompiledLoadedSound sound;
		SndCurveDef curve;
		if (r.type == c1::ASSET_TYPE_SOUND && extract_sound_alias_list(zone, r.header, list) && seen.insert(list.name).second
			&& (limit <= 0 || shippedLists.size() < static_cast<std::size_t>(limit)))
		{
			shippedLists.push_back(std::move(list));
		}
		else if (r.type == c1::ASSET_TYPE_LOADED_SOUND && extract_loaded_sound(source, r.header, sound))
		{
			shippedSounds.emplace(sound.name, std::move(sound));
		}
		else if (r.type == c1::ASSET_TYPE_SOUND_CURVE && extract_snd_curve(zone, r.header, curve))
		{
			shippedCurves.emplace(curve.name, std::move(curve));
		}
	}
	ZoneEditor editor;
	if (const Status s = editor.create(outFile.stem().string()); !s)
	{
		std::fprintf(stderr, "%s\n", s.message.c_str());
		return 1;
	}
	std::vector<std::string> log;
	std::uint32_t sounds = 0, curves = 0;
	for (const CompiledAliasList& shipped : shippedLists)
	{
		std::vector<CompiledAliasList> compiled;
		Status status = through_text(shipped, compiled);
		for (const CompiledAlias& a : shipped.aliases)
		{
			if (const auto it = shippedSounds.find(a.loadedName); status && it != shippedSounds.end() && editor.find_asset(c1::ASSET_TYPE_LOADED_SOUND, a.loadedName) == ZoneEditor::npos)
			{
				WaveData wave;
				CompiledLoadedSound sound;
				status = parse_wav(write_wav(loaded_sound_wave(it->second)), wave);
				status = status ? compile_loaded_sound(a.loadedName, wave, sound) : status;
				status = status ? link_loaded_sound(editor, sound) : status;
				sounds++;
			}
			if (const auto it = shippedCurves.find(a.curve); status && it != shippedCurves.end() && editor.find_asset(c1::ASSET_TYPE_SOUND_CURVE, a.curve) == ZoneEditor::npos)
			{
				SndCurveDef curve;
				status = parse_snd_curve(write_snd_curve(it->second), curve);
				curve.name = a.curve;
				status = status ? link_snd_curve(editor, curve) : status;
				curves++;
			}
		}
		status = status && compiled.size() == 1 ? link_sound_alias_list(editor, compiled[0], &log) : status;
		if (!status)
		{
			std::fprintf(stderr, "%s: %s\n", shipped.name.c_str(), status.message.c_str());
			return 1;
		}
	}
	std::vector<std::uint8_t> zoneBytes;
	if (Status s = editor.write(zoneBytes); !s || !(s = write_fastfile(outFile, zoneBytes, key)))
	{
		std::fprintf(stderr, "the game's loaders do not accept the zone: %s\n", s.message.c_str());
		return 1;
	}
	ZoneEditor madeEditor;
	if (const Status s = madeEditor.open(outFile, key); !s)
	{
		std::fprintf(stderr, "%s: %s\n", outFile.string().c_str(), s.message.c_str());
		return 1;
	}
	const Zone& made = madeEditor.zone();
	Totals totals;
	std::uint32_t missing = 0;
	for (const CompiledAliasList& shipped : shippedLists)
	{
		CompiledAliasList mine;
		const zaddr header = made.find_asset(c1::ASSET_TYPE_SOUND, shipped.name);
		if (!header || !extract_sound_alias_list(made, header, mine))
		{
			missing++;
			totals.wholeList.add(false);
			continue;
		}
		totals.wholeList.add(same_list(shipped, mine, &totals));
	}
	for (const XAssetEntry& e : made.assets())
	{
		CompiledLoadedSound mine;
		if (e.type == c1::ASSET_TYPE_LOADED_SOUND && extract_loaded_sound(madeEditor, e.header, mine))
		{
			same_sound(shippedSounds[mine.name], mine, &totals);
		}
	}
	std::printf("%s -> %s: %zu alias lists, %u loaded sounds, %u curves linked, %zu by-name references; zone %zu bytes, %zu top level assets, %zu warnings\n",
		zoneFile.filename().string().c_str(), outFile.filename().string().c_str(), shippedLists.size(), sounds, curves, log.size(), zoneBytes.size(), made.assets().size(),
		made.warnings().size());
	std::printf("read back with the game's loaders: %u lists missing\n", missing);
	print_rate("whole alias", totals.wholeAlias);
	print_rate("whole alias list", totals.wholeList);
	print_rate("whole loaded sound", totals.wholeSound);
	return missing || totals.wholeList.equal != totals.wholeList.total || totals.wholeSound.equal != totals.wholeSound.total ? 1 : 0;
}
