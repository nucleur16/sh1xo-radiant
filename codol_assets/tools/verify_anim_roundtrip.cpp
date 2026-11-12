// codol_asset_verify, animations: shipped XAnimParts -> xanim_export text -> compile_anim -> compared with the
// shipped one, part by part and frame by frame; and one animation taken out of a zone / compared between zones.

#include "verify.hpp"

#include "codol_assets/link.hpp"
#include "codol_zone/assets.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <set>

using namespace codol::assets;
using namespace codol::zone;
namespace c1 = codol::c1;
namespace fs = std::filesystem;

namespace
{
	struct AnimTotals
	{
		std::uint32_t anims = 0, failed = 0;
		Rate header, names, classes, notifies, delta, transforms, keyTables, streams, all;
		double worstRotation = 0, worstTranslation = 0;		// degrees, units
		std::uint64_t keysShipped = 0, keysCompiled = 0;
	};

	std::uint64_t key_count(const DecodedAnim& d)
	{
		std::uint64_t n = 0;
		for (const DecodedBone& bone : d.bones)
		{
			n += bone.rot.frames.size() + bone.trans.frames.size();
		}
		return n;
	}

	bool compare_anims(const RawAnim& shipped, const DecodedAnim& ds, const RawAnim& compiled, const float rotationTolerance, const float translationTolerance, AnimTotals& totals,
		std::string* why)
	{
		const auto differ = [&](const char* what)
		{
			if (why && why->empty())
			{
				*why = what;
			}
			return false;
		};
		DecodedAnim dc;
		if (!decode_anim(compiled, dc))
		{
			totals.all.add(false);
			return differ("the compiled animation cannot be decoded");
		}
		const c1::XAnimParts& a = shipped.header;
		const c1::XAnimParts& b = compiled.header;
		bool all = true;
		const bool header = a.numframes == b.numframes && a.flags == b.flags && a.framerate == b.framerate && std::fabs(a.frequency - b.frequency) <= 1e-6f * std::max(1.0f, a.frequency)
			&& a.assetType == b.assetType && a.notifyCount == b.notifyCount && a.boneCount[9] == b.boneCount[9];
		totals.header.add(header);
		if (!header && std::getenv("CODOL_ANIM_DEBUG"))
		{
			std::fprintf(stderr, "  %s: frames %u / %u, flags %u / %u, framerate %g / %g, frequency %g / %g, type %u / %u, notifies %u / %u, parts %u / %u\n", shipped.name.c_str(),
				a.numframes, b.numframes, a.flags, b.flags, a.framerate, b.framerate, a.frequency, b.frequency, a.assetType, b.assetType, a.notifyCount, b.notifyCount, a.boneCount[9],
				b.boneCount[9]);
		}
		all = all && (header || differ("header: frames, flags, rates, counts"));
		const bool names = shipped.boneNames == compiled.boneNames;
		totals.names.add(names);
		all = all && (names || differ("bone names or their order"));
		bool classes = std::memcmp(a.boneCount, b.boneCount, 10) == 0 && ds.bones.size() == dc.bones.size();
		for (std::size_t k = 0; classes && k < ds.bones.size(); k++)
		{
			classes = ds.bones[k].rotation == dc.bones[k].rotation && ds.bones[k].translation == dc.bones[k].translation;
		}
		totals.classes.add(classes);
		all = all && (classes || differ("how a rotation or translation is stored ( class )"));
		bool notifies = shipped.notifies.size() == compiled.notifies.size();
		for (std::size_t k = 0; notifies && k < shipped.notifies.size(); k++)
		{
			notifies = shipped.notifies[k].first == compiled.notifies[k].first && std::fabs(shipped.notifies[k].second - compiled.notifies[k].second) <= 2e-5f;
		}
		totals.notifies.add(notifies);
		if (!notifies && std::getenv("CODOL_ANIM_DEBUG"))
		{
			std::fprintf(stderr, "  %s notifies:", shipped.name.c_str());
			for (const auto& [name, time] : shipped.notifies)
			{
				std::fprintf(stderr, " %s@%.6f", name.c_str(), time);
			}
			std::fprintf(stderr, " | compiled:");
			for (const auto& [name, time] : compiled.notifies)
			{
				std::fprintf(stderr, " %s@%.6f", name.c_str(), time);
			}
			std::fprintf(stderr, "\n");
		}
		all = all && (notifies || differ("notifies"));
		const bool delta = shipped.hasDelta == compiled.hasDelta && shipped.deltaTrans.present == compiled.deltaTrans.present && shipped.deltaQuat2.present == compiled.deltaQuat2.present
			&& shipped.deltaQuat.present == compiled.deltaQuat.present && (shipped.deltaTrans.size == 0) == (compiled.deltaTrans.size == 0)
			&& shipped.deltaTrans.small_box == compiled.deltaTrans.small_box && (shipped.deltaQuat2.size == 0) == (compiled.deltaQuat2.size == 0);
		totals.delta.add(delta);
		all = all && (delta || differ("delta part: what it has, how it is stored"));

		// every frame of every part
		// radians. Components within e of each other ( the tolerance, and 1.5 for the two roundings ): the quaternions
		// are at most 2 e apart, which is a turn of 4 e
		const float rotationLimit = 4.0f * (std::max(rotationTolerance, 1.0f) + 1.5f) / 32767.0f;
		// One and a half steps of the box the track is stored in: the compiled key is within half a step of the frame,
		// a frame between keys within a step of that. A step is at most 0.05 in an 8 bit box; a 16 bit box has finer
		// ones unless it is larger than 3277 units ( a helicopter flying in ).
		const auto translation_limit = [&](const VecTrack& a, const VecTrack& b) { return translationTolerance + 1.5f * std::max({ a.step, b.step, 0.05f }) + 0.001f; };
		double worstRotation = 0, worstTranslation = 0;
		bool translationsFit = true;
		if (ds.bones.size() == dc.bones.size() && names)
		{
			for (std::uint32_t f = 0; f <= ds.lastFrame; f++)
			{
				const float frame = static_cast<float>(f);
				for (std::size_t k = 0; k < ds.bones.size(); k++)
				{
					const double angle = quat_angle_between(ds.bones[k].rot.at(frame), dc.bones[k].rot.at(frame));
					if (angle > worstRotation && angle > rotationLimit && std::getenv("CODOL_ANIM_DEBUG"))
					{
						const Quat x = ds.bones[k].rot.at(frame), y = dc.bones[k].rot.at(frame);
						std::fprintf(stderr, "  %s frame %u part %s ( class %u ): %.2f units; shipped %.1f %.1f %.1f %.1f, compiled %.1f %.1f %.1f %.1f; keys %zu / %zu\n", shipped.name.c_str(), f,
							ds.bones[k].name.c_str(), ds.bones[k].rotation, angle * 32767 / 2, x[0] * 32767, x[1] * 32767, x[2] * 32767, x[3] * 32767, y[0] * 32767, y[1] * 32767,
							y[2] * 32767, y[3] * 32767, ds.bones[k].rot.frames.size(), dc.bones[k].rot.frames.size());
					}
					worstRotation = std::max<double>(worstRotation, angle);
					if (ds.bones[k].translation != ANIM_TRANS_NONE && dc.bones[k].translation != ANIM_TRANS_NONE)
					{
						const Vec3 x = ds.bones[k].trans.at(frame), y = dc.bones[k].trans.at(frame);
						const float off = std::max({ std::fabs(x[0] - y[0]), std::fabs(x[1] - y[1]), std::fabs(x[2] - y[2]) });
						worstTranslation = std::max<double>(worstTranslation, off);
						translationsFit = translationsFit && off <= translation_limit(ds.bones[k].trans, dc.bones[k].trans);
					}
				}
				worstRotation = std::max<double>(worstRotation, quat_angle_between(ds.deltaRot.at(frame), dc.deltaRot.at(frame)));
				const Vec3 x = ds.deltaTrans.at(frame), y = dc.deltaTrans.at(frame);
				const float off = std::max({ std::fabs(x[0] - y[0]), std::fabs(x[1] - y[1]), std::fabs(x[2] - y[2]) });
				worstTranslation = std::max<double>(worstTranslation, off);
				translationsFit = translationsFit && off <= translation_limit(ds.deltaTrans, dc.deltaTrans);
			}
		}
		else
		{
			worstRotation = 4;
		}
		const bool transforms = worstRotation <= rotationLimit && translationsFit;
		totals.transforms.add(transforms);
		if (worstRotation < 4)
		{
			totals.worstRotation = std::max(totals.worstRotation, worstRotation * 57.29578);
			totals.worstTranslation = std::max(totals.worstTranslation, worstTranslation);
		}
		all = all && (transforms || differ("a part is somewhere else in some frame"));

		bool keyTables = ds.bones.size() == dc.bones.size();
		for (std::size_t k = 0; keyTables && k < ds.bones.size(); k++)
		{
			keyTables = ds.bones[k].rot.frames == dc.bones[k].rot.frames && ds.bones[k].trans.frames == dc.bones[k].trans.frames;
		}
		keyTables = keyTables && ds.deltaTrans.frames == dc.deltaTrans.frames && ds.deltaRot.frames == dc.deltaRot.frames;
		totals.keyTables.add(keyTables);
		totals.keysShipped += key_count(ds);
		totals.keysCompiled += key_count(dc);
		totals.streams.add(shipped.dataByte == compiled.dataByte && shipped.dataShort == compiled.dataShort && shipped.dataInt == compiled.dataInt
			&& shipped.randomDataShort == compiled.randomDataShort && shipped.randomDataByte == compiled.randomDataByte && shipped.indices == compiled.indices);
		totals.all.add(all);
		return all;
	}

	AnimOptions options_of(const RawAnim& raw)
	{
		AnimOptions options;
		options.name = raw.name;
		options.looping = (raw.header.flags & ANIM_FLAG_LOOP) != 0;
		options.type = raw.header.flags & ANIM_FLAG_DELTA_3D ? AnimType::delta3d : raw.header.flags & ANIM_FLAG_DELTA ? AnimType::delta : AnimType::relative;
		options.assetType = raw.header.assetType;
		return options;
	}

	void print_totals(const AnimTotals& totals)
	{
		print_rate("header ( frames, flags, rates, counts )", totals.header);
		print_rate("bone names and order", totals.names);
		print_rate("storage class of every rotation and translation", totals.classes);
		print_rate("notifies", totals.notifies);
		print_rate("delta part", totals.delta);
		print_rate("every part in every frame within the limits", totals.transforms);
		print_rate("all of the above", totals.all);
		print_rate("frame tables of every track identical", totals.keyTables);
		print_rate("data streams byte identical", totals.streams);
		std::printf("largest difference: rotation %.4f degrees, translation %.5f units; keys shipped %llu, compiled %llu ( %.1f %% )\n", totals.worstRotation, totals.worstTranslation,
			static_cast<unsigned long long>(totals.keysShipped), static_cast<unsigned long long>(totals.keysCompiled),
			totals.keysShipped ? 100.0 * static_cast<double>(totals.keysCompiled) / static_cast<double>(totals.keysShipped) : 0.0);
	}

	bool open_anim(const fs::path& zoneFile, const fs::path& exe, const std::string& name, Zone& zone, RawAnim& raw)
	{
		ClientKey key;
		if (const Status s = key.load_from_exe(exe); !s)
		{
			std::fprintf(stderr, "%s\n", s.message.c_str());
			return false;
		}
		if (const Status s = zone.open(zoneFile, key); !s)
		{
			std::fprintf(stderr, "%s: %s\n", zoneFile.string().c_str(), s.message.c_str());
			return false;
		}
		std::string error;
		if (!read_raw_anim(zone, zone.find_asset(c1::ASSET_TYPE_XANIMPARTS, name), raw, &error))
		{
			std::fprintf(stderr, "%s: xanim '%s': %s\n", zoneFile.string().c_str(), name.c_str(), error.c_str());
			return false;
		}
		return true;
	}
}

int anim_roundtrip(const fs::path& install, const fs::path& exe, const int every, const std::string& filter, const float rotationTolerance, const float translationTolerance)
{
	ClientKey key;
	if (const Status s = key.load_from_exe(exe); !s)
	{
		std::fprintf(stderr, "%s\n", s.message.c_str());
		return 1;
	}
	AnimTotals totals;
	std::uint32_t index = 0, shown = 0;
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
			if (r.type != c1::ASSET_TYPE_XANIMPARTS || zone.reference(r.header))
			{
				continue;
			}
			RawAnim raw;
			if (!read_raw_anim(zone, r.header, raw) || !seen.insert(raw.name).second || (index++ % static_cast<std::uint32_t>(every)) != 0)
			{
				continue;
			}
			DecodedAnim decoded;
			ExportAnim extracted, parsed;
			CompiledAnim compiled;
			AnimOptions options = options_of(raw);
			options.rotationTolerance = rotationTolerance;
			options.translationTolerance = translationTolerance;
			Status status = decode_anim(raw, decoded);
			if (status)
			{
				// the stored form as text ( poses relative to the parent ), through the parser, as a modder's file would go
				extract_anim(decoded, nullptr, extracted);
				status = parse_xanim_export(write_xanim_export(extracted), parsed);
			}
			if (!status || !(status = compile_anim(parsed, options, compiled)))
			{
				totals.failed++;
				if (shown++ < 10)
				{
					std::fprintf(stderr, "%s: %s\n", raw.name.c_str(), status.message.c_str());
				}
				continue;
			}
			totals.anims++;
			std::string why;
			if (!compare_anims(raw, decoded, compiled.raw, rotationTolerance, translationTolerance, totals, &why) && shown++ < 25)
			{
				std::fprintf(stderr, "%s ( %s ): first difference: %s\n", raw.name.c_str(), file.stem().string().c_str(), why.c_str());
			}
		}
	}
	std::printf("%u animations through xanim_export text and compile_anim ( tolerances %g units, %g ), %u failed to compile\n", totals.anims, rotationTolerance, translationTolerance,
		totals.failed);
	print_totals(totals);
	return 0;
}

// Every n-th shipped animation compiled from its export, all of them linked into one zone, that zone run through the
// game's loaders and read back: what the loaders deliver must be the compiled animation, stream for stream.
int anim_zone(const fs::path& install, const fs::path& exe, const int every, const fs::path& outFile)
{
	LinkOptions link;
	link.install = install;
	link.exe = exe;
	link.zoneName = outFile.stem().string();
	link.libraryCache = outFile.parent_path() / "codol_matlib.txt";
	AssetLinker linker;
	if (const Status s = linker.begin(link); !s)
	{
		std::fprintf(stderr, "%s\n", s.message.c_str());
		return 1;
	}
	std::vector<RawAnim> compiledAnims;
	std::uint32_t index = 0;
	std::set<std::string> seen;
	for (const fs::path& file : install_zones(install))
	{
		Zone zone;
		if (!zone.open(file, linker.key()))
		{
			continue;
		}
		for (const AssetRecord& r : zone.all_assets())
		{
			if (r.type != c1::ASSET_TYPE_XANIMPARTS || zone.reference(r.header))
			{
				continue;
			}
			RawAnim raw;
			if (!read_raw_anim(zone, r.header, raw) || !seen.insert(raw.name).second || (index++ % static_cast<std::uint32_t>(every)) != 0)
			{
				continue;
			}
			DecodedAnim decoded;
			ExportAnim extracted, parsed;
			CompiledAnim compiled;
			AnimOptions options = options_of(raw);
			options.rotationTolerance = 0;
			options.translationTolerance = 0;
			if (!decode_anim(raw, decoded))
			{
				continue;
			}
			extract_anim(decoded, nullptr, extracted);
			if (!parse_xanim_export(write_xanim_export(extracted), parsed) || !compile_anim(parsed, options, compiled))
			{
				std::fprintf(stderr, "%s does not compile\n", raw.name.c_str());
				return 1;
			}
			if (const Status s = linker.add_anim(compiled); !s)
			{
				std::fprintf(stderr, "%s\n", s.message.c_str());
				return 1;
			}
			compiledAnims.push_back(std::move(compiled.raw));
		}
	}
	std::vector<std::uint8_t> zoneBytes;
	if (const Status s = linker.write_zone(zoneBytes); !s)
	{
		std::fprintf(stderr, "the game's loaders do not accept the zone: %s\n", s.message.c_str());
		return 1;
	}
	if (const Status s = write_fastfile(outFile, zoneBytes, linker.key()); !s)
	{
		std::fprintf(stderr, "%s\n", s.message.c_str());
		return 1;
	}
	Zone built;
	if (const Status s = built.open(outFile, linker.key()); !s)
	{
		std::fprintf(stderr, "%s: %s\n", outFile.string().c_str(), s.message.c_str());
		return 1;
	}
	Rate same;
	std::uint32_t wide = 0, delta = 0, delta3d = 0, constantDelta = 0, longTables = 0;
	for (const RawAnim& c : compiledAnims)
	{
		RawAnim back;
		const bool read = read_raw_anim(built, built.find_asset(c1::ASSET_TYPE_XANIMPARTS, c.name), back);
		const bool equal = read && std::memcmp(&back.header.dataByteCount, &c.header.dataByteCount, 44) == 0 && back.boneNames == c.boneNames && back.notifies == c.notifies
			&& back.dataByte == c.dataByte && back.dataShort == c.dataShort && back.dataInt == c.dataInt && back.randomDataShort == c.randomDataShort
			&& back.randomDataByte == c.randomDataByte && back.indices == c.indices && back.hasDelta == c.hasDelta && back.deltaTrans.present == c.deltaTrans.present
			&& back.deltaTrans.size == c.deltaTrans.size && back.deltaTrans.small_box == c.deltaTrans.small_box && back.deltaTrans.indices == c.deltaTrans.indices
			&& back.deltaTrans.frames == c.deltaTrans.frames && std::memcmp(back.deltaTrans.frame0, c.deltaTrans.frame0, 36) == 0
			&& back.deltaQuat2.present == c.deltaQuat2.present && back.deltaQuat2.indices == c.deltaQuat2.indices && back.deltaQuat2.frames == c.deltaQuat2.frames
			&& back.deltaQuat.present == c.deltaQuat.present && back.deltaQuat.indices == c.deltaQuat.indices && back.deltaQuat.frames == c.deltaQuat.frames;
		same.add(equal);
		if (!equal)
		{
			std::fprintf(stderr, "%s: what the loaders deliver is not what was compiled\n", c.name.c_str());
		}
		wide += c.header.numframes >= 256;
		delta += c.hasDelta;
		delta3d += c.deltaQuat.present;
		constantDelta += (c.deltaTrans.present && c.deltaTrans.size == 0) || (c.deltaQuat2.present && c.deltaQuat2.size == 0);
		longTables += !c.indices.empty();
	}
	std::printf("%s: %zu animations, %zu bytes; %u with 256 frames or more, %u with long frame tables, %u delta ( %u with a constant delta track, %u delta 3d )\n",
		outFile.string().c_str(), compiledAnims.size(), zoneBytes.size(), wide, longTables, delta, constantDelta, delta3d);
	print_rate("loaded by the game's loaders = compiled", same);
	return same.equal == same.total ? 0 : 1;
}

// writes a shipped animation as <outDir>/<name>.xanim_export ( the stored form: parent relative, "//#LOCAL" )
int anim_extract(const fs::path& zoneFile, const fs::path& exe, const std::string& name, const fs::path& outDir)
{
	Zone zone;
	RawAnim raw;
	DecodedAnim decoded;
	if (!open_anim(zoneFile, exe, name, zone, raw))
	{
		return 1;
	}
	if (const Status s = decode_anim(raw, decoded); !s)
	{
		std::fprintf(stderr, "%s\n", s.message.c_str());
		return 1;
	}
	ExportAnim extracted;
	extract_anim(decoded, nullptr, extracted);
	std::error_code ec;
	fs::create_directories(outDir, ec);
	const fs::path file = outDir / (name + ".xanim_export");
	std::ofstream out(file, std::ios::binary);
	out << write_xanim_export(extracted, "extracted from " + zoneFile.filename().string() + " by codol_asset_verify anim-extract");
	std::printf("%s: %zu parts, %u frames, flags %u, asset type %u, %zu notifies -> %s\n", name.c_str(), raw.boneNames.size(), raw.header.numframes + 1u, raw.header.flags,
		raw.header.assetType, raw.notifies.size(), file.string().c_str());
	return out ? 0 : 1;
}

// two animations of two zones ( the shipped one, the one compiled from its export and linked ) through the zone loader
int anim_compare(const fs::path& zoneA, const std::string& nameA, const fs::path& zoneB, const std::string& nameB, const fs::path& exe, const float rotationTolerance,
	const float translationTolerance)
{
	Zone a, b;
	RawAnim rawA, rawB;
	DecodedAnim decoded;
	if (!open_anim(zoneA, exe, nameA, a, rawA) || !open_anim(zoneB, exe, nameB, b, rawB))
	{
		return 1;
	}
	if (const Status s = decode_anim(rawA, decoded); !s)
	{
		std::fprintf(stderr, "%s\n", s.message.c_str());
		return 1;
	}
	AnimTotals totals;
	std::string why;
	const bool same = compare_anims(rawA, decoded, rawB, rotationTolerance, translationTolerance, totals, &why);
	print_totals(totals);
	std::printf("%s / %s: %s%s\n", nameA.c_str(), nameB.c_str(), same ? "the same" : "different: ", why.c_str());
	return same ? 0 : 1;
}
