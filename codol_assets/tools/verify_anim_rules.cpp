// codol_asset_verify, animations: the second survey of the shipped XAnimParts - what is exact in them ( classes,
// signs ), how far a stored key is from what its neighbours interpolate to, how the translation boxes are used.

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
	std::string num(const double v)
	{
		char text[64];
		std::snprintf(text, sizeof(text), "%g", v);
		return text;
	}

	void print_histogram(const char* title, const std::map<std::string, std::uint32_t>& counts, const std::size_t limit = 24)
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
		std::printf("\n");
	}
}

int anim_survey_rules(const fs::path& install, const fs::path& exe, const std::string& filter)
{
	ClientKey key;
	if (const Status s = key.load_from_exe(exe); !s)
	{
		std::fprintf(stderr, "%s\n", s.message.c_str());
		return 1;
	}
	std::map<std::string, std::uint32_t> rotErr, transErr, exact, typeByPrefix, signRule, boxLow, boxTop, origin, deltaStart, coarse, gapHist;
	std::vector<std::string> smallBig, largeSmall;
	std::map<std::string, std::uint32_t> diagonalHist, rangeHist, typeNames;
	double smallMaxDiagonal = 0, largeMinDiagonal = 1e30;
	std::uint32_t shown = 0;
	const auto bucket = [](const double e) -> std::string
	{
		const double limits[] = { 0, 0.5, 1, 1.5, 2, 3, 4, 6, 8, 16, 32, 64 };
		for (const double l : limits)
		{
			if (e <= l)
			{
				return "<= " + num(l);
			}
		}
		return "> 64";
	};
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
			DecodedAnim d;
			if (!read_raw_anim(zone, r.header, raw) || !seen.insert(raw.name).second || !decode_anim(raw, d))
			{
				continue;
			}
			const c1::XAnimParts& h = raw.header;
			const std::string prefix = raw.name.substr(0, raw.name.find('_'));
			typeByPrefix[prefix + " -> " + num(h.assetType)]++;
			if (h.assetType != 1 && h.assetType != 2 && typeNames.size() < 60)
			{
				typeNames[num(h.assetType) + " " + raw.name + " ( " + file.stem().string() + " )"]++;
			}
			bool hasOrigin = false;
			for (const std::string& name : raw.boneNames)
			{
				hasOrigin = hasOrigin || name == "tag_origin";
			}
			origin[std::string(hasOrigin ? "tag_origin listed" : "no tag_origin") + ", flags " + num(h.flags)]++;
			if (!d.deltaTrans.values.empty())
			{
				const Vec3& v = d.deltaTrans.values.front();
				deltaStart[std::fabs(v[0]) + std::fabs(v[1]) + std::fabs(v[2]) < 1e-3f ? "delta translation starts at 0" : "delta translation starts elsewhere"]++;
			}
			if (!d.deltaRot.values.empty())
			{
				const Quat& q = d.deltaRot.values.front();
				deltaStart[std::fabs(q[3]) > 0.99999f ? "delta rotation starts at identity" : "delta rotation starts elsewhere"]++;
			}

			std::size_t shortAt = 0, byteAt = 0, intAt = 0, rshortAt = 0, rbyteAt = 0, indexAt = 0;
			const bool wide = h.numframes >= 256;
			const auto table = [&]() -> std::size_t
			{
				const std::size_t tableSize = static_cast<std::uint16_t>(raw.dataShort[shortAt++]);
				if (!wide)
				{
					byteAt += tableSize + 1;
				}
				else if (tableSize < 64)
				{
					shortAt += tableSize + 1;
				}
				else
				{
					// coarse table: is it indices[ 0, 256, 512 .. ] and the last one
					const std::size_t n = ((tableSize - 1) >> 8) + 2;
					bool every256 = true;
					for (std::size_t k = 0; k + 1 < n; k++)
					{
						every256 = every256 && static_cast<std::uint16_t>(raw.dataShort[shortAt + k]) == raw.indices[indexAt + k * 256];
					}
					const bool last = static_cast<std::uint16_t>(raw.dataShort[shortAt + n - 1]) == raw.indices[indexAt + tableSize];
					coarse[std::string(every256 ? "entries = indices[ 256 k ]" : "entries other") + (last ? ", last = indices[ size ]" : ", last other")]++;
					shortAt += n;
					indexAt += tableSize + 1;
				}
				return tableSize + 1;
			};
			std::size_t part = h.boneCount[0];
			for (int type = 1; type <= 2; type++)
			{
				for (std::size_t i = 0; i < h.boneCount[type]; i++, part++)
				{
					const std::size_t keys = table();
					const std::size_t width = type == 1 ? 2 : 4;
					const std::int16_t* v = raw.randomDataShort.data() + rshortAt;
					rshortAt += keys * width;
					bool allEqual = true, flat = true;
					int range = 0;
					for (std::size_t c = 0; c < width; c++)
					{
						int low = 32767, top = -32767;
						for (std::size_t k = 0; k < keys; k++)
						{
							low = std::min<int>(low, v[k * width + c]);
							top = std::max<int>(top, v[k * width + c]);
						}
						range = std::max(range, top - low);
					}
					rangeHist["animated rotation: largest component range " + bucket(range)]++;
					for (std::size_t k = 0; k < keys; k++)
					{
						allEqual = allEqual && std::memcmp(v, v + k * width, width * 2) == 0;
						flat = flat && width == 4 && v[k * 4] == 0 && v[k * 4 + 1] == 0;
					}
					exact[allEqual ? "animated rotation, all keys equal" : "animated rotation, keys differ"]++;
					if (width == 4)
					{
						exact[flat ? "3d animated with x = y = 0 in all keys" : "3d animated, x or y used"]++;
					}
					const std::vector<std::uint16_t>& frames = d.bones[part].rot.frames;
					for (std::size_t k = 1; k + 1 < keys; k++)
					{
						const double t = static_cast<double>(frames[k] - frames[k - 1]) / static_cast<double>(frames[k + 1] - frames[k - 1]);
						double worst = 0;
						for (std::size_t c = 0; c < width; c++)
						{
							const double lerp = v[(k - 1) * width + c] + (static_cast<double>(v[(k + 1) * width + c]) - v[(k - 1) * width + c]) * t;
							worst = std::max(worst, std::fabs(lerp - v[k * width + c]));
						}
						rotErr[bucket(worst)]++;
					}
					for (std::size_t k = 1; k < keys; k++)
					{
						const int gap = frames[k] - frames[k - 1];
						gapHist[gap <= 8 ? "gap " + num(gap) : gap <= 16 ? "gap 9 .. 16" : gap <= 64 ? "gap 17 .. 64" : "gap > 64"]++;
					}
					if (shown < 3 && keys > 8 && keys < 24 && h.numframes > 40 && width == 4)
					{
						shown++;
						std::printf("example %s part %s ( %u frames ):", raw.name.c_str(), raw.boneNames[part].c_str(), h.numframes + 1);
						for (std::size_t k = 0; k < keys; k++)
						{
							std::printf(" %u:( %d %d %d %d )", frames[k], v[k * 4], v[k * 4 + 1], v[k * 4 + 2], v[k * 4 + 3]);
						}
						std::printf("\n");
					}
				}
			}
			for (int type = 3; type <= 4; type++)
			{
				for (std::size_t i = 0; i < h.boneCount[type]; i++, part++)
				{
					const std::size_t width = type == 3 ? 2 : 4;
					const std::int16_t* v = raw.dataShort.data() + shortAt;
					shortAt += width;
					if (width == 4)
					{
						exact[v[0] == 0 && v[1] == 0 ? (v[2] == 0 ? "3d static that is the identity" : "3d static with x = y = 0")
							: std::abs(v[0]) <= 2 && std::abs(v[1]) <= 2 ? "3d static with | x |, | y | <= 2" : "3d static, x or y used"]++;
						// sign: what the usual matrix -> quaternion conversion gives
						const Quat q{ v[0] / 32767.0f, v[1] / 32767.0f, v[2] / 32767.0f, v[3] / 32767.0f };
						float axis[3][3];
						axis_from_quat(q, axis);
						const Quat back = quat_from_axis(axis);
						const float dot = q[0] * back[0] + q[1] * back[1] + q[2] * back[2] + q[3] * back[3];
						signRule[std::string(dot >= 0 ? "static 3d: sign of the usual conversion" : "static 3d: other sign") + (v[3] < 0 ? ", w < 0" : "")]++;
					}
					else
					{
						exact[v[0] == 0 ? "2d static that is the identity" : "2d static"]++;
						signRule[v[1] >= 0 ? "static 2d: w >= 0" : "static 2d: w < 0"]++;
					}
				}
			}
			for (int type = 5; type <= 6; type++)
			{
				for (std::size_t i = 0; i < h.boneCount[type]; i++)
				{
					const std::uint8_t bone = raw.dataByte[byteAt++];
					const std::size_t keys = table();
					float mins[3], size[3];
					std::memcpy(mins, raw.dataInt.data() + intAt, 12);
					std::memcpy(size, raw.dataInt.data() + intAt + 3, 12);
					intAt += 6;
					const double steps = type == 5 ? 255.0 : 65535.0;
					std::vector<std::uint32_t> v(keys * 3);
					bool allEqual = true;
					for (std::size_t k = 0; k < keys * 3; k++)
					{
						v[k] = type == 5 ? raw.randomDataByte[rbyteAt++] : static_cast<std::uint16_t>(raw.randomDataShort[rshortAt++]);
						allEqual = allEqual && v[k] == v[k % 3];
					}
					exact[allEqual ? "animated translation, all keys equal" : "animated translation, keys differ"]++;
					const double ex = size[0] * steps, ey = size[1] * steps, ez = size[2] * steps;
					const double diagonal = std::sqrt(ex * ex + ey * ey + ez * ez);
					(type == 5 ? smallMaxDiagonal : largeMinDiagonal) = type == 5 ? std::max(smallMaxDiagonal, diagonal) : std::min(largeMinDiagonal, diagonal);
					diagonalHist[std::string(type == 5 ? "u8 " : "u16 ") + (diagonal < 12.5 ? "< 12.5" : diagonal < 12.7 ? "< 12.7" : diagonal < 12.75 ? "< 12.75" : diagonal < 12.8 ? "< 12.8" : diagonal < 13 ? "< 13" : ">= 13")]++;
					char text[200];
					std::snprintf(text, sizeof(text), "%s/%s %.3f %.3f %.3f keys %zu", raw.name.c_str(), raw.boneNames[bone].c_str(), ex, ey, ez, keys);
					if (type == 5 && std::max({ ex, ey, ez }) > 9.0 && smallBig.size() < 12)
					{
						smallBig.push_back(text);
					}
					if (type == 6 && std::max({ ex, ey, ez }) < 9.0 && largeSmall.size() < 12)
					{
						largeSmall.push_back(text);
					}
					for (std::size_t c = 0; c < 3; c++)
					{
						std::uint32_t low = 0xFFFFFFFF, top = 0;
						for (std::size_t k = 0; k < keys; k++)
						{
							low = std::min(low, v[k * 3 + c]);
							top = std::max(top, v[k * 3 + c]);
						}
						if (size[c] != 0)
						{
							boxLow[(type == 5 ? "u8 low " : "u16 low ") + bucket(type == 5 ? low : low / 257.0)]++;
							boxTop[(type == 5 ? "u8 top " : "u16 top ") + bucket(type == 5 ? 255.0 - top : (65535.0 - top) / 257.0)]++;
						}
					}
					const std::vector<std::uint16_t>& frames = d.bones[bone].trans.frames;
					for (std::size_t k = 1; k + 1 < keys; k++)
					{
						const double t = static_cast<double>(frames[k] - frames[k - 1]) / static_cast<double>(frames[k + 1] - frames[k - 1]);
						double worst = 0;
						for (std::size_t c = 0; c < 3; c++)
						{
							const double lerp = v[(k - 1) * 3 + c] + (static_cast<double>(v[(k + 1) * 3 + c]) - v[(k - 1) * 3 + c]) * t;
							worst = std::max(worst, std::fabs(lerp - v[k * 3 + c]) * size[c]);
						}
						transErr[bucket(worst * 1000.0)]++;		// in 1 / 1000 units
					}
				}
			}
		}
	}
	print_histogram("asset type by name prefix", typeByPrefix, 60);
	print_histogram("tag_origin", origin);
	print_histogram("delta start", deltaStart);
	print_histogram("coarse frame table", coarse);
	print_histogram("exactness", exact);
	print_histogram("sign", signRule);
	print_histogram("interior rotation key vs interpolation of its neighbours ( s16 units )", rotErr);
	print_histogram("interior translation key vs interpolation ( 1 / 1000 units )", transErr);
	print_histogram("rotation key gaps", gapHist);
	print_histogram("box low ( steps of 1 / 255 of the box )", boxLow);
	print_histogram("box top", boxTop);
	print_histogram("box diagonal", diagonalHist);
	print_histogram("ranges", rangeHist);
	print_histogram("asset types other than 1 and 2", typeNames, 60);
	std::printf("largest diagonal of an 8 bit box %g, smallest of a 16 bit box %g\n", smallMaxDiagonal, largeMinDiagonal);
	for (const std::string& s : smallBig)
	{
		std::printf("8 bit, large box: %s\n", s.c_str());
	}
	for (const std::string& s : largeSmall)
	{
		std::printf("16 bit, small box: %s\n", s.c_str());
	}
	return 0;
}
