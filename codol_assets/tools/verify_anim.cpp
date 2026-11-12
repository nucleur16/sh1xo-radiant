// codol_asset_verify, animations: the survey of the shipped XAnimParts the compiler's rules were read off, and
// the round trip shipped animation -> xanim_export text -> compile_anim -> compare.

#include "verify.hpp"

#include "codol_assets/anim.hpp"
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
	template <typename Function>
	int for_each_anim(const fs::path& install, const fs::path& exe, const std::string& filter, Function&& function)
	{
		ClientKey key;
		if (const Status s = key.load_from_exe(exe); !s)
		{
			std::fprintf(stderr, "%s\n", s.message.c_str());
			return 1;
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
				if (r.type != c1::ASSET_TYPE_XANIMPARTS || zone.reference(r.header))
				{
					continue;
				}
				RawAnim raw;
				if (!read_raw_anim(zone, r.header, raw) || !seen.insert(raw.name).second)
				{
					continue;
				}
				function(zone, file, raw);
			}
		}
		return 0;
	}

	void histogram(const char* title, const std::map<std::string, std::uint32_t>& counts, const std::size_t limit = 24)
	{
		std::vector<std::pair<std::uint32_t, std::string>> sorted;
		for (const auto& [k, n] : counts)
		{
			sorted.emplace_back(n, k);
		}
		std::sort(sorted.rbegin(), sorted.rend());
		std::printf("%s:", title);
		for (std::size_t k = 0; k < sorted.size() && k < limit; k++)
		{
			std::printf("  %s x %u", sorted[k].second.c_str(), sorted[k].first);
		}
		std::printf(sorted.size() > limit ? "  .. ( %zu distinct )\n" : "\n", sorted.size());
	}

	std::string number(const double v, const char* format = "%g")
	{
		char text[64];
		std::snprintf(text, sizeof(text), format, v);
		return text;
	}
}

int anim_survey(const fs::path& install, const fs::path& exe, const std::string& filter)
{
	std::uint32_t anims = 0, undecodable = 0;
	std::map<std::string, std::uint32_t> flags, assetType, framerate, frequencyRule, isDefault, unk30, notifyLast, notifyOrder, deltaKind, deltaBone, classCounts;
	std::map<std::string, std::uint32_t> rotOrder, transOrder, quatSign, quatNorm, halfRule, keyFull, smallRule, staticRotRule, tableKind, firstKey, lastKey, staticTransZero;
	std::map<std::string, std::uint32_t> zeroBone, deltaTransKind, deltaFirst, transBoxRule, redundant;
	double smallMaxExtent = 0, largeMinExtent = 1e30;
	std::uint64_t rotTracks = 0, rotKeys = 0, rotFrames = 0, transTracks = 0;
	const int result = for_each_anim(install, exe, filter, [&](const Zone&, const fs::path&, const RawAnim& raw)
	{
		anims++;
		const c1::XAnimParts& h = raw.header;
		DecodedAnim d;
		if (const Status s = decode_anim(raw, d); !s)
		{
			if (undecodable++ < 10)
			{
				std::fprintf(stderr, "%s\n", s.message.c_str());
			}
			return;
		}
		flags[number(h.flags)]++;
		assetType[number(h.assetType)]++;
		framerate[number(h.framerate)]++;
		isDefault[number(h.isDefault)]++;
		unk30[number(h._unk30[0] | h._unk30[1] << 8)]++;
		const float expected = h.numframes ? h.framerate / static_cast<float>(h.numframes) : 0.0f;
		frequencyRule[std::fabs(h.frequency - expected) <= 1e-6f * std::max(1.0f, expected) ? "framerate / numframes" : h.numframes == 0 ? "numframes 0: " + number(h.frequency) : "other"]++;
		if (h.numframes == 0)
		{
			zeroBone["numframes 0, parts " + number(h.boneCount[9]) + ", flags " + number(h.flags)]++;
		}

		notifyLast[raw.notifies.empty() ? "no notify" : raw.notifies.back().first == "end" && raw.notifies.back().second == 1.0f ? "\"end\" at 1" : "other"]++;
		bool ascending = true;
		for (std::size_t k = 1; k < raw.notifies.size(); k++)
		{
			ascending = ascending && raw.notifies[k].second >= raw.notifies[k - 1].second;
		}
		notifyOrder[ascending ? "ascending by time" : "not ascending"]++;

		// order of the bones inside a rotation class, order of the translation bone indices
		{
			std::size_t at = 0;
			for (int type = 0; type <= 4; type++)
			{
				bool alpha = true;
				for (std::size_t i = 1; i < h.boneCount[type]; i++)
				{
					alpha = alpha && raw.boneNames[at + i - 1] <= raw.boneNames[at + i];
				}
				if (h.boneCount[type] > 1)
				{
					rotOrder[alpha ? "names ascending inside the class" : "not ascending"]++;
				}
				at += h.boneCount[type];
			}
		}
		for (const DecodedBone& bone : d.bones)
		{
			classCounts["rot " + number(bone.rotation)]++;
			classCounts["trans " + number(bone.translation)]++;
			if (bone.rotation == ANIM_ROT_2D || bone.rotation == ANIM_ROT_3D)
			{
				rotTracks++;
				rotKeys += bone.rot.frames.size();
				rotFrames += static_cast<std::uint64_t>(h.numframes) + 1;
				keyFull[bone.rot.frames.size() == static_cast<std::size_t>(h.numframes) + 1 ? "rot: every frame a key" : "rot: fewer keys"]++;
				firstKey[bone.rot.frames.front() == 0 ? "first key 0" : "first key > 0"]++;
				lastKey[bone.rot.frames.back() == h.numframes ? "last key = numframes" : "last key < numframes"]++;
				bool flips = false;
				for (std::size_t k = 1; k < bone.rot.values.size(); k++)
				{
					const Quat& a = bone.rot.values[k - 1];
					const Quat& b = bone.rot.values[k];
					flips = flips || a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3] < 0;
				}
				quatSign[flips ? "neighbouring keys on opposite sides" : "neighbouring keys on one side"]++;
				quatSign[bone.rot.values.front()[3] >= 0 ? "first key w >= 0" : "first key w < 0"]++;
			}
			if (bone.rotation == ANIM_ROT_2D_STATIC || bone.rotation == ANIM_ROT_3D_STATIC)
			{
				quatSign[bone.rot.values.front()[3] >= 0 ? "static w >= 0" : "static w < 0"]++;
			}
			if (bone.translation == ANIM_TRANS_SMALL || bone.translation == ANIM_TRANS_LARGE)
			{
				transTracks++;
				keyFull[bone.trans.frames.size() == static_cast<std::size_t>(h.numframes) + 1 ? "trans: every frame a key" : "trans: fewer keys"]++;
			}
			if (bone.translation == ANIM_TRANS_STATIC)
			{
				const Vec3& v = bone.trans.values.front();
				staticTransZero[v[0] == 0 && v[1] == 0 && v[2] == 0 ? "static translation 0 0 0" : "static translation other"]++;
			}
		}

		// the raw streams once more for what the decoded form no longer shows: quaternion length, box rule
		{
			std::size_t shortAt = 0, byteAt = 0, intAt = 0, rshortAt = 0, rbyteAt = 0;
			const bool wide = h.numframes >= 256;
			const auto table = [&]() -> std::size_t
			{
				const std::size_t tableSize = static_cast<std::uint16_t>(raw.dataShort[shortAt++]);
				if (!wide)
				{
					byteAt += tableSize + 1;
					tableKind["u8 in dataByte"]++;
				}
				else if (tableSize < 64)
				{
					shortAt += tableSize + 1;
					tableKind["u16 in dataShort"]++;
				}
				else
				{
					shortAt += ((tableSize - 1) >> 8) + 2;
					tableKind["u16 in indices"]++;
				}
				return tableSize + 1;
			};
			const auto norm = [&](const std::int16_t* v, const std::size_t n)
			{
				double sum = 0;
				for (std::size_t c = 0; c < n; c++)
				{
					sum += static_cast<double>(v[c]) * v[c];
				}
				const double length = std::sqrt(sum);
				quatNorm[std::fabs(length - 32767.0) <= 1.5 ? "length 32767 +- 1.5" : std::fabs(length - 32767.0) <= 8 ? "length 32767 +- 8" : "length other"]++;
			};
			for (int type = 1; type <= 2; type++)
			{
				for (std::size_t i = 0; i < h.boneCount[type]; i++)
				{
					const std::size_t keys = table();
					const std::size_t width = type == 1 ? 2 : 4;
					for (std::size_t k = 0; k < keys; k++)
					{
						norm(raw.randomDataShort.data() + rshortAt, width);
						rshortAt += width;
					}
				}
			}
			for (int type = 3; type <= 4; type++)
			{
				for (std::size_t i = 0; i < h.boneCount[type]; i++)
				{
					const std::size_t width = type == 3 ? 2 : 4;
					norm(raw.dataShort.data() + shortAt, width);
					shortAt += width;
				}
			}
			for (int type = 5; type <= 6; type++)
			{
				std::uint8_t previous = 0;
				for (std::size_t i = 0; i < h.boneCount[type]; i++)
				{
					const std::uint8_t bone = raw.dataByte[byteAt++];
					if (i > 0)
					{
						transOrder[bone > previous ? "animated: bone index ascending" : "animated: not ascending"]++;
					}
					previous = bone;
					const std::size_t keys = table();
					float mins[3], size[3];
					std::memcpy(mins, raw.dataInt.data() + intAt, 12);
					std::memcpy(size, raw.dataInt.data() + intAt + 3, 12);
					intAt += 6;
					// extent of the box, and whether the largest stored value reaches the end of the range
					const double steps = type == 5 ? 255.0 : 65535.0;
					std::uint32_t top[3] = {}, low[3] = { 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF };
					for (std::size_t k = 0; k < keys; k++)
					{
						for (std::size_t c = 0; c < 3; c++)
						{
							const std::uint32_t v = type == 5 ? raw.randomDataByte[rbyteAt++] : static_cast<std::uint16_t>(raw.randomDataShort[rshortAt++]);
							top[c] = std::max(top[c], v);
							low[c] = std::min(low[c], v);
						}
					}
					double extent = 0;
					for (std::size_t c = 0; c < 3; c++)
					{
						extent = std::max(extent, static_cast<double>(size[c]) * steps);
						if (size[c] != 0)
						{
							transBoxRule[low[c] == 0 && top[c] == static_cast<std::uint32_t>(steps) ? "axis uses the whole range 0 .. max" : low[c] == 0 ? "axis starts at 0, ends below max" : "axis starts above 0"]++;
						}
						else
						{
							transBoxRule[top[c] == 0 ? "flat axis: size 0, values 0" : "flat axis: size 0, values not 0"]++;
						}
					}
					if (type == 5)
					{
						smallMaxExtent = std::max(smallMaxExtent, extent);
					}
					else
					{
						largeMinExtent = std::min(largeMinExtent, extent);
						smallRule[extent < 16 ? "16 bit with box < 16" : extent < 64 ? "16 bit with box 16 .. 64" : "16 bit with box >= 64"]++;
					}
				}
			}
			std::uint8_t previous = 0;
			for (std::size_t i = 0; i < h.boneCount[7]; i++)
			{
				const std::uint8_t bone = raw.dataByte[byteAt++];
				if (i > 0)
				{
					transOrder[bone > previous ? "static: bone index ascending" : "static: not ascending"]++;
				}
				previous = bone;
			}
			for (std::size_t i = 0; i < h.boneCount[8]; i++)
			{
				const std::uint8_t bone = raw.dataByte[byteAt++];
				if (i > 0)
				{
					transOrder[bone > previous ? "none: bone index ascending" : "none: not ascending"]++;
				}
				previous = bone;
			}
		}

		// could a key be left out: is there a key that the linear interpolation of its neighbours' stored values gives exactly
		for (const DecodedBone& bone : d.bones)
		{
			if ((bone.rotation == ANIM_ROT_2D || bone.rotation == ANIM_ROT_3D) && bone.rot.frames.size() >= 3)
			{
				bool repeated = false;
				for (std::size_t k = 1; k + 1 < bone.rot.values.size() && !repeated; k++)
				{
					repeated = bone.rot.values[k] == bone.rot.values[k - 1] && bone.rot.values[k] == bone.rot.values[k + 1];
				}
				redundant[repeated ? "rot track with a key equal to both neighbours" : "rot track without"]++;
			}
		}

		if (raw.hasDelta)
		{
			deltaKind[std::string(raw.deltaTrans.present ? "trans " : "") + (raw.deltaQuat2.present ? "quat2 " : "") + (raw.deltaQuat.present ? "quat " : "") + "flags " + number(h.flags)]++;
			if (raw.deltaTrans.present)
			{
				deltaTransKind[raw.deltaTrans.size == 0 ? "constant" : raw.deltaTrans.small_box ? "u8 keys" : "u16 keys"]++;
				if (raw.deltaTrans.size)
				{
					deltaFirst[d.deltaTrans.frames.front() == 0 ? "first key 0" : "first key > 0"]++;
				}
			}
			bool origin = false;
			for (const std::string& name : raw.boneNames)
			{
				origin = origin || name == "tag_origin";
			}
			deltaBone[origin ? "delta anim lists tag_origin" : "delta anim without tag_origin"]++;
			if (!raw.boneNames.empty())
			{
				deltaBone["first part " + raw.boneNames.front()]++;
			}
		}
		else
		{
			deltaKind["no delta part, flags " + number(h.flags)]++;
		}
	});
	std::printf("%u animations, %u cannot be decoded\n", anims, undecodable);
	histogram("flags", flags);
	histogram("assetType", assetType);
	histogram("isDefault", isDefault);
	histogram("bytes 30 / 31", unk30);
	histogram("framerate", framerate);
	histogram("frequency", frequencyRule);
	histogram("numframes 0", zeroBone);
	histogram("last notify", notifyLast);
	histogram("notify order", notifyOrder);
	histogram("parts by class", classCounts);
	histogram("bone order inside a rotation class", rotOrder);
	histogram("translation bone order", transOrder);
	histogram("quaternion sign", quatSign);
	histogram("quaternion length", quatNorm);
	histogram("keys", keyFull);
	histogram("first key", firstKey);
	histogram("last key", lastKey);
	histogram("frame tables", tableKind);
	histogram("translation box", transBoxRule);
	histogram("16 bit translations", smallRule);
	histogram("static translations", staticTransZero);
	histogram("redundant keys", redundant);
	histogram("delta", deltaKind);
	histogram("delta translation", deltaTransKind);
	histogram("delta translation keys", deltaFirst);
	histogram("delta bones", deltaBone, 12);
	std::printf("largest box of an 8 bit translation: %g; smallest box of a 16 bit translation: %g\n", smallMaxExtent, largeMinExtent);
	std::printf("animated rotation tracks: %llu, keys %llu of %llu frames ( %.1f %% ); animated translation tracks: %llu\n", static_cast<unsigned long long>(rotTracks),
		static_cast<unsigned long long>(rotKeys), static_cast<unsigned long long>(rotFrames), rotFrames ? 100.0 * static_cast<double>(rotKeys) / static_cast<double>(rotFrames) : 0.0,
		static_cast<unsigned long long>(transTracks));
	return result;
}
