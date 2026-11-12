// codol_asset_verify, animations with a skeleton: a shipped animation written the way an exporter writes it - every
// part in model space, on the skeleton of the shipped models that have its bones - and compiled with that skeleton.
// Checks the part of the compiler the plain round trip does not reach: model space -> parent relative, and "a
// translation that stays at the bind translation has no track".

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
	struct Bone
	{
		std::string name, parent;		// parent: empty for a root
		Quat q{ 0, 0, 0, 1 };			// relative to the parent
		Vec3 t{};
	};

	struct Skeleton
	{
		std::string model;
		std::vector<Bone> bones;
		std::set<std::string> names;
	};

	Quat multiply(const Quat& a, const Quat& b)
	{
		return { a[3] * b[0] + a[0] * b[3] + a[1] * b[2] - a[2] * b[1], a[3] * b[1] - a[0] * b[2] + a[1] * b[3] + a[2] * b[0],
			a[3] * b[2] + a[0] * b[1] - a[1] * b[0] + a[2] * b[3], a[3] * b[3] - a[0] * b[0] - a[1] * b[1] - a[2] * b[2] };
	}

	Vec3 rotate(const Quat& q, const Vec3& v)
	{
		const Quat r = multiply(multiply(q, { v[0], v[1], v[2], 0 }), { -q[0], -q[1], -q[2], q[3] });
		return { r[0], r[1], r[2] };
	}

	// The skeleton an animation plays on: the model with most of its parts, then the bones of further models that
	// hang on a bone already there ( a head on a body: the game joins the models of an entity by bone name ).
	bool build_skeleton(const std::vector<Skeleton>& models, const std::vector<std::string>& parts, ExportModel& out, std::size_t& covered, std::string& used)
	{
		out = {};
		used.clear();
		std::set<std::string> missing(parts.begin(), parts.end());
		std::map<std::string, std::size_t> index;
		std::vector<Quat> worldQ;
		std::vector<Vec3> worldT;
		for (int round = 0; round < 4 && !missing.empty(); round++)
		{
			const Skeleton* best = nullptr;
			std::size_t bestCount = 0;
			for (const Skeleton& m : models)
			{
				std::size_t count = 0;
				for (const std::string& name : missing)
				{
					count += m.names.count(name);
				}
				// the first model is the body; a later one has to hang on what is there
				if (count > bestCount && (round == 0 || std::any_of(m.bones.begin(), m.bones.end(), [&](const Bone& b) { return index.count(b.name) != 0; })))
				{
					best = &m;
					bestCount = count;
				}
			}
			if (!best)
			{
				break;
			}
			used += (used.empty() ? "" : " + ") + best->model;
			for (const Bone& b : best->bones)
			{
				if (index.count(b.name))
				{
					continue;
				}
				const auto parent = b.parent.empty() ? index.end() : index.find(b.parent);
				if (round > 0 && parent == index.end())
				{
					continue;		// a root of an attached model that is not a bone of the body
				}
				ExportBone e;
				e.name = b.name;
				Quat q = b.q;
				Vec3 t = b.t;
				if (parent != index.end())
				{
					e.parent = static_cast<int>(parent->second);
					const Vec3 r = rotate(worldQ[parent->second], b.t);
					q = multiply(worldQ[parent->second], b.q);
					t = { worldT[parent->second][0] + r[0], worldT[parent->second][1] + r[1], worldT[parent->second][2] + r[2] };
				}
				std::memcpy(e.offset, t.data(), 12);
				axis_from_quat(q, e.axis);
				index[b.name] = out.bones.size();
				out.bones.push_back(e);
				worldQ.push_back(q);
				worldT.push_back(t);
				missing.erase(b.name);
			}
		}
		covered = parts.size() - missing.size();
		return !out.bones.empty();
	}
}

int anim_model_roundtrip(const fs::path& install, const fs::path& exe, const int every, const float epsilon)
{
	ClientKey key;
	if (const Status s = key.load_from_exe(exe); !s)
	{
		std::fprintf(stderr, "%s\n", s.message.c_str());
		return 1;
	}
	// every distinct skeleton of the install
	std::vector<Skeleton> models;
	std::set<std::string> seenModels, seenShapes;
	const std::vector<fs::path> zones = install_zones(install);
	for (const fs::path& file : zones)
	{
		Zone zone;
		if (!zone.open(file, key))
		{
			continue;
		}
		for (const AssetRecord& r : zone.all_assets())
		{
			ModelView view;
			if (r.type != c1::ASSET_TYPE_XMODEL || zone.reference(r.header) || !read_model(zone, r.header, view, false) || view.isReference || view.bones.size() < 2
				|| !seenModels.insert(std::string(view.name)).second)
			{
				continue;
			}
			Skeleton s;
			s.model = std::string(view.name);
			std::string shape;
			for (const ModelBone& bone : view.bones)
			{
				Bone b;
				b.name = std::string(bone.name);
				b.parent = bone.parent >= 0 && static_cast<std::size_t>(bone.parent) < view.bones.size() ? std::string(view.bones[static_cast<std::size_t>(bone.parent)].name) : std::string();
				const bool root = b.parent.empty();
				std::memcpy(b.q.data(), root ? bone.baseQuat : bone.localQuat, 16);
				std::memcpy(b.t.data(), root ? bone.baseTrans : bone.localTrans, 12);
				char text[96];
				std::snprintf(text, sizeof(text), "%s<%s:%.3f,%.3f,%.3f;", b.name.c_str(), b.parent.c_str(), b.t[0], b.t[1], b.t[2]);
				shape += text;
				s.names.insert(b.name);
				s.bones.push_back(std::move(b));
			}
			if (seenShapes.insert(shape).second)
			{
				models.push_back(std::move(s));
			}
		}
	}
	std::printf("%zu models with a skeleton, %zu distinct skeletons\n", seenModels.size(), models.size());

	Rate fullyCovered, names, classes, rotationClasses, transforms, all, signFirstW, signProduct;
	std::uint64_t noneShipped = 0, noneKept = 0, staticShipped = 0, staticBecameNone = 0, partsTotal = 0, partsCovered = 0;
	double worstRotation = 0, worstTranslation = 0;
	std::uint32_t index = 0, shown = 0, anims = 0;
	std::set<std::string> seen;
	for (const fs::path& file : zones)
	{
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
			DecodedAnim ds;
			if (!read_raw_anim(zone, r.header, raw) || !seen.insert(raw.name).second || raw.boneNames.empty() || (index++ % static_cast<std::uint32_t>(every)) != 0 || !decode_anim(raw, ds))
			{
				continue;
			}
			ExportModel skeleton;
			std::size_t covered = 0;
			std::string used;
			std::vector<std::string> parts = raw.boneNames;
			if (raw.hasDelta)
			{
				parts.push_back("tag_origin");
			}
			if (!build_skeleton(models, parts, skeleton, covered, used))
			{
				continue;
			}
			anims++;
			partsTotal += parts.size();
			partsCovered += covered;
			fullyCovered.add(covered == parts.size());
			if (covered != parts.size())
			{
				continue;
			}

			ExportAnim extracted, parsed;
			extract_anim(ds, &skeleton, extracted);
			AnimOptions options;
			options.name = raw.name;
			options.looping = (raw.header.flags & ANIM_FLAG_LOOP) != 0;
			options.type = raw.header.flags & ANIM_FLAG_DELTA_3D ? AnimType::delta3d : raw.header.flags & ANIM_FLAG_DELTA ? AnimType::delta : AnimType::relative;
			options.assetType = raw.header.assetType;
			options.rotationTolerance = 0;
			options.translationTolerance = 0;
			options.staticTranslationEpsilon = epsilon;
			options.skeleton = &skeleton;
			CompiledAnim compiled;
			DecodedAnim dc;
			if (!parse_xanim_export(write_xanim_export(extracted), parsed) || !compile_anim(parsed, options, compiled) || !decode_anim(compiled.raw, dc))
			{
				all.add(false);
				continue;
			}
			// the compiler keeps the source order inside a class; the classes may differ here, so compare by name
			std::map<std::string, const DecodedBone*> byName;
			for (const DecodedBone& bone : dc.bones)
			{
				byName[bone.name] = &bone;
			}
			const bool sameNames = raw.boneNames == compiled.raw.boneNames;
			bool sameClasses = true, sameRotationClasses = true;
			double rotation = 0, translation = 0;
			for (const DecodedBone& a : ds.bones)
			{
				const auto hit = byName.find(a.name);
				if (hit == byName.end())
				{
					sameClasses = sameRotationClasses = false;
					continue;
				}
				const DecodedBone& b = *hit->second;
				sameRotationClasses = sameRotationClasses && a.rotation == b.rotation;
				sameClasses = sameClasses && a.rotation == b.rotation && a.translation == b.translation;
				noneShipped += a.translation == ANIM_TRANS_NONE;
				noneKept += a.translation == ANIM_TRANS_NONE && b.translation == ANIM_TRANS_NONE;
				staticShipped += a.translation == ANIM_TRANS_STATIC;
				staticBecameNone += a.translation == ANIM_TRANS_STATIC && b.translation == ANIM_TRANS_NONE;
				for (std::uint32_t f = 0; f <= ds.lastFrame; f++)
				{
					const float frame = static_cast<float>(f);
					rotation = std::max<double>(rotation, quat_angle_between(a.rot.at(frame), b.rot.at(frame)));
					if (a.translation != ANIM_TRANS_NONE && b.translation != ANIM_TRANS_NONE)
					{
						const Vec3 x = a.trans.at(frame), y = b.trans.at(frame);
						translation = std::max<double>({ translation, std::fabs(x[0] - y[0]), std::fabs(x[1] - y[1]), std::fabs(x[2] - y[2]) });
					}
				}
				// sign of the stored rotation: w >= 0 in the first key, against the plain product of the two model space
				// rotations as the usual matrix conversion gives them
				if (!a.rot.values.empty() && a.rotation != ANIM_ROT_2D && a.rotation != ANIM_ROT_2D_STATIC)
				{
					signFirstW.add(a.rot.values.front()[3] >= 0);
					const auto part = std::find(extracted.parts.begin(), extracted.parts.end(), a.name);
					const auto bone = std::find_if(skeleton.bones.begin(), skeleton.bones.end(), [&](const ExportBone& e) { return e.name == a.name; });
					if (part != extracted.parts.end() && bone != skeleton.bones.end() && bone->parent >= 0)
					{
						const auto parentPart = std::find(extracted.parts.begin(), extracted.parts.end(), skeleton.bones[static_cast<std::size_t>(bone->parent)].name);
						if (parentPart != extracted.parts.end())
						{
							const Quat child = quat_from_axis(extracted.frames[0][static_cast<std::size_t>(part - extracted.parts.begin())].axis);
							const Quat parent = quat_from_axis(extracted.frames[0][static_cast<std::size_t>(parentPart - extracted.parts.begin())].axis);
							const Quat local = multiply({ -parent[0], -parent[1], -parent[2], parent[3] }, child);
							const Quat& stored = a.rot.values.front();
							signProduct.add(local[0] * stored[0] + local[1] * stored[1] + local[2] * stored[2] + local[3] * stored[3] >= 0);
						}
					}
				}
			}
			rotation = std::max<double>(rotation, quat_angle_between(ds.deltaRot.at(0), dc.deltaRot.at(0)));
			for (std::uint32_t f = 0; f <= ds.lastFrame; f++)
			{
				const Vec3 x = ds.deltaTrans.at(static_cast<float>(f)), y = dc.deltaTrans.at(static_cast<float>(f));
				translation = std::max<double>({ translation, std::fabs(x[0] - y[0]), std::fabs(x[1] - y[1]), std::fabs(x[2] - y[2]) });
				rotation = std::max<double>(rotation, quat_angle_between(ds.deltaRot.at(static_cast<float>(f)), dc.deltaRot.at(static_cast<float>(f))));
			}
			// one more step than the plain round trip: the model space matrices have six decimals
			const bool within = rotation <= 4.0 * 2.5 / 32767.0 && translation <= 0.051 + 0.01;
			names.add(sameNames);
			classes.add(sameClasses);
			rotationClasses.add(sameRotationClasses);
			transforms.add(within);
			all.add(sameClasses && within);
			worstRotation = std::max(worstRotation, rotation * 57.29578);
			worstTranslation = std::max(worstTranslation, translation);
			if (!(sameClasses && within) && shown++ < 12)
			{
				std::fprintf(stderr, "%s on %s: classes %s, rotation off by %.4f degrees, translation by %.4f\n", raw.name.c_str(), used.c_str(), sameClasses ? "equal" : "differ",
					rotation * 57.29578, translation);
			}
		}
	}
	std::printf("%u animations with parts; parts found on a shipped skeleton: %llu of %llu\n", anims, static_cast<unsigned long long>(partsCovered), static_cast<unsigned long long>(partsTotal));
	print_rate("animations whose parts are all on a skeleton", fullyCovered);
	std::printf("of those, written in model space on that skeleton and compiled with it ( bind epsilon %g ):\n", epsilon);
	print_rate("bone names and order", names);
	print_rate("rotation class of every part", rotationClasses);
	print_rate("rotation and translation class of every part", classes);
	print_rate("every part in every frame within the limits", transforms);
	print_rate("classes and frames", all);
	std::printf("translations without a track: %llu shipped, %llu found again ( the frames stay at the bind translation ); constant ones: %llu shipped, %llu taken for the bind translation\n",
		static_cast<unsigned long long>(noneShipped), static_cast<unsigned long long>(noneKept), static_cast<unsigned long long>(staticShipped), static_cast<unsigned long long>(staticBecameNone));
	std::printf("largest difference: rotation %.4f degrees, translation %.5f units\n", worstRotation, worstTranslation);
	print_rate("sign of a stored 3d rotation: first key has w >= 0", signFirstW);
	print_rate("sign of a stored 3d rotation: that of conj( parent ) * child of the converted matrices", signProduct);
	return 0;
}
