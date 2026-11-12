// xanim_export -> XAnimParts ( compile_anim ), and back ( extract_anim ).
//
// Every rule here was read off the 5698 shipped animations ( codol_asset_verify anim-survey / anim-rules ):
//   * frequency = framerate / numframes; numframes is the number of the last frame;
//   * the bone list is ordered by rotation class ( none, 2d keys, 3d keys, 2d constant, 3d constant ), inside a class
//     in the order the source has the parts; translations name their bone, ascending inside each class;
//   * a rotation is quantised to s16 ( * 32767 ); it is "2d" when x and y are 0 in every frame, constant when all
//     frames are equal *after* quantising ( 67111 shipped animated tracks move by one unit only ), none when it is
//     the identity;
//   * neighbouring keys of a rotation track are on the same side of the sphere;
//   * an animated translation is stored in the box of all its frames, with 8 bit steps when the diagonal of the box
//     is below 12.75 ( 255 steps of 0.05 ), else with 16 bit steps;
//   * frame tables: u8 in dataByte below 256 frames; else u16 in dataShort up to 64 entries, and above that in
//     `indices` with a coarse table ( every 256th entry and the last ) in dataShort;
//   * the last notify is "end" at 1; notifies ascend by time;
//   * a delta animation does not list the root part; its movement is the delta part ( stored as it is, not relative
//     to the first frame ); a root that does not move leaves the delta part empty.

#include "codol_assets/anim.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace codol::assets
{
	namespace
	{
		Quat multiply(const Quat& a, const Quat& b)
		{
			return { a[3] * b[0] + a[0] * b[3] + a[1] * b[2] - a[2] * b[1], a[3] * b[1] - a[0] * b[2] + a[1] * b[3] + a[2] * b[0],
				a[3] * b[2] + a[0] * b[1] - a[1] * b[0] + a[2] * b[3], a[3] * b[3] - a[0] * b[0] - a[1] * b[1] - a[2] * b[2] };
		}

		Quat conjugate(const Quat& q) { return { -q[0], -q[1], -q[2], q[3] }; }

		Vec3 rotate(const Quat& q, const Vec3& v)
		{
			const Quat r = multiply(multiply(q, { v[0], v[1], v[2], 0 }), conjugate(q));
			return { r[0], r[1], r[2] };
		}

		struct Pose
		{
			Quat q{ 0, 0, 0, 1 };
			Vec3 t{ 0, 0, 0 };
		};

		Pose pose_of(const ExportPose& p)
		{
			return { quat_from_axis(p.axis), { p.offset[0], p.offset[1], p.offset[2] } };
		}

		Pose compose(const Pose& parent, const Pose& local)
		{
			const Vec3 r = rotate(parent.q, local.t);
			return { multiply(parent.q, local.q), { parent.t[0] + r[0], parent.t[1] + r[1], parent.t[2] + r[2] } };
		}

		Pose relative(const Pose& parent, const Pose& world)
		{
			const Quat inverse = conjugate(parent.q);
			return { multiply(inverse, world.q), rotate(inverse, { world.t[0] - parent.t[0], world.t[1] - parent.t[1], world.t[2] - parent.t[2] }) };
		}

		// the skeleton an animation is converted with: parents, bind pose relative to the parent, an order with every
		// parent in front of its children
		struct Skeleton
		{
			std::vector<int> parent;
			std::vector<Pose> bindLocal;
			std::vector<std::size_t> order;
			std::map<std::string, std::size_t> byName;

			explicit Skeleton(const ExportModel& model)
			{
				const std::size_t count = model.bones.size();
				std::vector<Pose> world(count);
				parent.resize(count);
				bindLocal.resize(count);
				for (std::size_t k = 0; k < count; k++)
				{
					const ExportBone& bone = model.bones[k];
					world[k] = { quat_from_axis(bone.axis), { bone.offset[0], bone.offset[1], bone.offset[2] } };
					parent[k] = bone.parent >= 0 && static_cast<std::size_t>(bone.parent) < count && static_cast<std::size_t>(bone.parent) != k ? bone.parent : -1;
					byName.emplace(bone.name, k);
				}
				std::vector<bool> placed(count, false);
				for (std::size_t round = 0; round < count && order.size() < count; round++)
				{
					for (std::size_t k = 0; k < count; k++)
					{
						if (!placed[k] && (parent[k] < 0 || placed[static_cast<std::size_t>(parent[k])]))
						{
							placed[k] = true;
							order.push_back(k);
						}
					}
				}
				for (std::size_t k = 0; k < count; k++)
				{
					if (!placed[k])		// a parent loop: treat as a root
					{
						parent[k] = -1;
						order.push_back(k);
					}
					bindLocal[k] = parent[k] < 0 ? world[k] : relative(world[static_cast<std::size_t>(parent[k])], world[k]);
				}
			}
		};

		std::int16_t to_s16(const float c)
		{
			return static_cast<std::int16_t>(std::clamp(std::lround(c * 32767.0f), -32767l, 32767l));
		}

		// frames kept as keys: the first, the last, and every frame the interpolation between kept neighbours does not
		// give within the tolerance. error( a, b, f ): how far frame f is from the interpolation between a and b.
		template <typename Error>
		std::vector<std::uint16_t> reduce_keys(const std::size_t frames, Error&& error)
		{
			std::vector<std::uint16_t> keys{ 0 };
			std::size_t a = 0;
			while (a + 1 < frames)
			{
				std::size_t b = a + 1;
				while (b + 1 < frames)
				{
					bool fits = true;
					for (std::size_t f = a + 1; f <= b && fits; f++)
					{
						fits = error(a, b + 1, f);
					}
					if (!fits)
					{
						break;
					}
					b++;
				}
				keys.push_back(static_cast<std::uint16_t>(b));
				a = b;
			}
			return keys;
		}

		struct RotTrack
		{
			std::uint8_t type = ANIM_ROT_NONE;
			std::vector<std::uint16_t> keys;
			std::vector<std::int16_t> values;		// 2 or 4 per key
		};

		// per frame rotations ( unit quaternions ) -> class, keys, s16 values
		RotTrack make_rotation(const std::vector<Quat>& frames, const float tolerance, const bool force3d = false, const bool force2d = false)
		{
			RotTrack out;
			const std::size_t count = frames.size();
			std::vector<std::array<std::int16_t, 4>> q(count);
			Quat previous{ 0, 0, 0, 1 };
			bool flat = true, constant = true;
			for (std::size_t f = 0; f < count; f++)
			{
				Quat v = frames[f];
				if (force2d)
				{
					const float length = std::sqrt(v[2] * v[2] + v[3] * v[3]);
					v = length > 1e-6f ? Quat{ 0, 0, v[2] / length, v[3] / length } : Quat{ 0, 0, 0, 1 };
				}
				// the first frame with w >= 0, every other one on the side of the frame before
				const bool flip = f == 0 ? v[3] < 0 : v[0] * previous[0] + v[1] * previous[1] + v[2] * previous[2] + v[3] * previous[3] < 0;
				if (flip)
				{
					v = { -v[0], -v[1], -v[2], -v[3] };
				}
				previous = v;
				q[f] = { to_s16(v[0]), to_s16(v[1]), to_s16(v[2]), to_s16(v[3]) };
				flat = flat && q[f][0] == 0 && q[f][1] == 0;
				constant = constant && q[f] == q[0];
			}
			const bool twoD = (flat && !force3d) || force2d;
			const std::size_t first = twoD ? 2 : 0;
			if (constant)
			{
				if (count == 0 || (q[0][0] == 0 && q[0][1] == 0 && q[0][2] == 0))
				{
					return out;		// identity
				}
				out.type = twoD ? ANIM_ROT_2D_STATIC : ANIM_ROT_3D_STATIC;
				out.keys = { 0 };
				out.values.assign(q[0].begin() + static_cast<std::ptrdiff_t>(first), q[0].end());
				return out;
			}
			out.type = twoD ? ANIM_ROT_2D : ANIM_ROT_3D;
			// One unit at least: the frames are rounded values, half a unit off each, and so are the two keys. A straight
			// line between two keys is nearer to the source than the rounded frames it replaces.
			const float limit = std::max(tolerance, 1.0f);
			out.keys = reduce_keys(count, [&](const std::size_t a, const std::size_t b, const std::size_t f)
			{
				// what the game makes of the two keys: the straight line between them, brought back to unit length
				const float t = static_cast<float>(f - a) / static_cast<float>(b - a);
				float lerp[4] = {}, length = 0, dot = 0;
				for (std::size_t c = first; c < 4; c++)
				{
					lerp[c] = static_cast<float>(q[a][c]) + (static_cast<float>(q[b][c]) - static_cast<float>(q[a][c])) * t;
					length += lerp[c] * lerp[c];
					dot += static_cast<float>(q[a][c]) * static_cast<float>(q[b][c]);
				}
				length = std::sqrt(length);
				// two keys of a track are never on opposite sides ( none of the 319169 shipped tracks has such a pair ): a
				// turn of more than half a circle keeps a key in between
				if (length < 1.0f || dot <= 0)
				{
					return false;
				}
				for (std::size_t c = first; c < 4; c++)
				{
					if (std::fabs(lerp[c] * 32767.0f / length - static_cast<float>(q[f][c])) > limit)
					{
						return false;
					}
				}
				return true;
			});
			for (const std::uint16_t k : out.keys)
			{
				out.values.insert(out.values.end(), q[k].begin() + static_cast<std::ptrdiff_t>(first), q[k].end());
			}
			return out;
		}

		struct TransTrack
		{
			std::uint8_t type = ANIM_TRANS_NONE;
			std::vector<std::uint16_t> keys;
			Vec3 mins{}, size{};					// animated
			std::vector<std::uint16_t> values;		// 3 per key, animated
			Vec3 constant{};						// static
		};

		constexpr float TRANS_SMALL_DIAGONAL = 12.75f;		// 255 steps of 0.05

		TransTrack make_translation(const std::vector<Vec3>& frames, const float tolerance, const float epsilon, const Vec3* bind)
		{
			TransTrack out;
			if (frames.empty())
			{
				return out;
			}
			Vec3 low = frames[0], high = frames[0];
			bool isBind = bind != nullptr;
			for (const Vec3& v : frames)
			{
				for (std::size_t c = 0; c < 3; c++)
				{
					low[c] = std::min(low[c], v[c]);
					high[c] = std::max(high[c], v[c]);
					isBind = isBind && std::fabs(v[c] - (*bind)[c]) <= epsilon;
				}
			}
			if (isBind)
			{
				return out;
			}
			if (high[0] - low[0] <= epsilon && high[1] - low[1] <= epsilon && high[2] - low[2] <= epsilon)
			{
				out.type = ANIM_TRANS_STATIC;
				out.keys = { 0 };
				out.constant = frames[0];
				return out;
			}
			const Vec3 extent{ high[0] - low[0], high[1] - low[1], high[2] - low[2] };
			const bool small = std::sqrt(extent[0] * extent[0] + extent[1] * extent[1] + extent[2] * extent[2]) < TRANS_SMALL_DIAGONAL;
			const float steps = small ? 255.0f : 65535.0f;
			out.type = small ? ANIM_TRANS_SMALL : ANIM_TRANS_LARGE;
			out.mins = low;
			out.size = { extent[0] / steps, extent[1] / steps, extent[2] / steps };
			std::vector<std::array<std::uint16_t, 3>> q(frames.size());
			for (std::size_t f = 0; f < frames.size(); f++)
			{
				for (std::size_t c = 0; c < 3; c++)
				{
					q[f][c] = out.size[c] > 0 ? static_cast<std::uint16_t>(std::clamp(std::lround((frames[f][c] - low[c]) / out.size[c]), 0l, static_cast<long>(steps))) : 0;
				}
			}
			out.keys = reduce_keys(frames.size(), [&](const std::size_t a, const std::size_t b, const std::size_t f)
			{
				const float t = static_cast<float>(f - a) / static_cast<float>(b - a);
				for (std::size_t c = 0; c < 3; c++)
				{
					const float lerp = static_cast<float>(q[a][c]) + (static_cast<float>(q[b][c]) - static_cast<float>(q[a][c])) * t;
					// one step at least, as for rotations: both the frame and the keys are rounded to steps
					if (std::fabs(lerp - static_cast<float>(q[f][c])) * out.size[c] > std::max(tolerance, out.size[c]))
					{
						return false;
					}
				}
				return true;
			});
			for (const std::uint16_t k : out.keys)
			{
				out.values.insert(out.values.end(), q[k].begin(), q[k].end());
			}
			return out;
		}

		void put_float(std::vector<std::uint32_t>& stream, const float v)
		{
			std::uint32_t bits;
			std::memcpy(&bits, &v, 4);
			stream.push_back(bits);
		}
	}

	Status compile_anim(const ExportAnim& source, const AnimOptions& options, CompiledAnim& out)
	{
		out = {};
		const auto fail = [&](const std::string& what) { return Status::error("xanim '" + options.name + "': " + what); };
		if (options.name.empty())
		{
			return Status::error("xanim: no name");
		}
		const std::size_t frames = source.frames.size();
		const std::size_t parts = source.parts.size();
		if (frames == 0 || frames > 65536)
		{
			return fail("1 .. 65536 frames");
		}
		for (const auto& frame : source.frames)
		{
			if (frame.size() != parts)
			{
				return fail("a frame does not have a pose for every part");
			}
		}
		for (std::size_t a = 0; a < parts; a++)
		{
			for (std::size_t b = a + 1; b < parts; b++)
			{
				if (source.parts[a] == source.parts[b])
				{
					return fail("part '" + source.parts[a] + "' is listed twice");
				}
			}
		}
		const bool wide = frames - 1 >= 256;

		// ---- poses relative to the parent --------------------------------------------------------
		std::vector<std::vector<Pose>> local(parts, std::vector<Pose>(frames));
		std::vector<bool> hasBind(parts, false);
		std::vector<Vec3> bind(parts);
		const bool useSkeleton = options.skeleton && !source.local;
		if (useSkeleton)
		{
			const Skeleton skeleton(*options.skeleton);
			const std::size_t bones = skeleton.parent.size();
			std::vector<int> partOfBone(bones, -1);
			std::vector<int> boneOfPart(parts, -1);
			std::string missing;
			std::size_t missingCount = 0;
			for (std::size_t p = 0; p < parts; p++)
			{
				if (const auto hit = skeleton.byName.find(source.parts[p]); hit != skeleton.byName.end())
				{
					boneOfPart[p] = static_cast<int>(hit->second);
					partOfBone[hit->second] = static_cast<int>(p);
					hasBind[p] = true;
					bind[p] = skeleton.bindLocal[hit->second].t;
				}
				else if (missingCount++ < 4)
				{
					missing += (missing.empty() ? "" : ", ") + source.parts[p];
				}
			}
			if (missingCount)
			{
				out.notes.push_back(std::to_string(missingCount) + " parts are not bones of the skeleton and are taken as roots: " + missing + (missingCount > 4 ? " .." : ""));
			}
			std::vector<Pose> world(bones);
			for (std::size_t f = 0; f < frames; f++)
			{
				// model space pose of every bone of the skeleton in this frame: from the file, or the bind pose carried
				// along by the parent
				for (const std::size_t b : skeleton.order)
				{
					if (partOfBone[b] >= 0)
					{
						world[b] = pose_of(source.frames[f][static_cast<std::size_t>(partOfBone[b])]);
					}
					else
					{
						world[b] = skeleton.parent[b] < 0 ? skeleton.bindLocal[b] : compose(world[static_cast<std::size_t>(skeleton.parent[b])], skeleton.bindLocal[b]);
					}
				}
				for (std::size_t p = 0; p < parts; p++)
				{
					const Pose pose = pose_of(source.frames[f][p]);
					const int b = boneOfPart[p];
					local[p][f] = b >= 0 && skeleton.parent[static_cast<std::size_t>(b)] >= 0 ? relative(world[static_cast<std::size_t>(skeleton.parent[static_cast<std::size_t>(b)])], pose) : pose;
				}
			}
		}
		else
		{
			for (std::size_t p = 0; p < parts; p++)
			{
				for (std::size_t f = 0; f < frames; f++)
				{
					local[p][f] = pose_of(source.frames[f][p]);
				}
			}
			if (!source.local && parts > 1)
			{
				out.notes.push_back("no skeleton: every part is taken as a root ( its pose in the file is used as it is )");
			}
		}
		std::vector<bool> noTranslation(parts, false);
		for (const int p : source.noTranslation)
		{
			if (p >= 0 && static_cast<std::size_t>(p) < parts)
			{
				noTranslation[static_cast<std::size_t>(p)] = true;
			}
		}

		RawAnim& raw = out.raw;
		raw.name = options.name;
		c1::XAnimParts& h = raw.header;
		h.numframes = static_cast<std::uint16_t>(frames - 1);
		h.framerate = source.framerate;
		h.frequency = h.numframes ? source.framerate / static_cast<float>(h.numframes) : 0.0f;
		h.assetType = options.assetType;
		h.flags = static_cast<std::uint8_t>((options.looping ? ANIM_FLAG_LOOP : 0) | (options.type == AnimType::delta ? ANIM_FLAG_DELTA : options.type == AnimType::delta3d ? ANIM_FLAG_DELTA_3D : 0));

		const float epsilon = options.staticTranslationEpsilon;

		// ---- the delta part ---------------------------------------------------------------------
		std::size_t deltaPart = parts;
		if (options.type != AnimType::relative)
		{
			raw.hasDelta = true;
			for (std::size_t p = 0; p < parts; p++)
			{
				if (source.parts[p] == options.deltaPart)
				{
					deltaPart = p;
				}
			}
			if (deltaPart == parts)
			{
				out.notes.push_back("delta animation without a part '" + options.deltaPart + "': the delta part is empty");
			}
			else
			{
				std::vector<Vec3> t(frames);
				std::vector<Quat> q(frames);
				for (std::size_t f = 0; f < frames; f++)
				{
					t[f] = local[deltaPart][f].t;
					q[f] = local[deltaPart][f].q;
				}
				const Vec3 zero{ 0, 0, 0 };
				const TransTrack trans = make_translation(t, options.translationTolerance, epsilon, &zero);
				if (trans.type != ANIM_TRANS_NONE)
				{
					RawDeltaTrans& d = raw.deltaTrans;
					d.present = true;
					if (trans.type == ANIM_TRANS_STATIC)
					{
						std::memcpy(d.frame0, trans.constant.data(), 12);
					}
					else
					{
						d.size = static_cast<std::uint16_t>(trans.keys.size() - 1);
						d.small = trans.type == ANIM_TRANS_SMALL;
						std::memcpy(d.mins, trans.mins.data(), 12);
						std::memcpy(d.frameSize, trans.size.data(), 12);
						d.indices = trans.keys;
						d.frames = trans.values;
					}
				}
				const bool full = options.type == AnimType::delta3d;
				const RotTrack rot = make_rotation(q, options.rotationTolerance, full, !full);
				if (rot.type != ANIM_ROT_NONE)
				{
					RawDeltaQuat& d = full ? raw.deltaQuat : raw.deltaQuat2;
					d.present = true;
					d.size = static_cast<std::uint16_t>(rot.keys.size() - 1);
					d.indices = d.size ? rot.keys : std::vector<std::uint16_t>{};
					d.frames = rot.values;
				}
			}
		}

		// ---- tracks ------------------------------------------------------------------------------
		std::vector<RotTrack> rotations(parts);
		std::vector<TransTrack> translations(parts);
		for (std::size_t p = 0; p < parts; p++)
		{
			if (p == deltaPart)
			{
				continue;
			}
			std::vector<Quat> q(frames);
			std::vector<Vec3> t(frames);
			for (std::size_t f = 0; f < frames; f++)
			{
				q[f] = local[p][f].q;
				t[f] = local[p][f].t;
			}
			rotations[p] = make_rotation(q, options.rotationTolerance);
			if (!noTranslation[p])
			{
				translations[p] = make_translation(t, options.translationTolerance, epsilon, hasBind[p] ? &bind[p] : nullptr);
			}
		}

		// ---- bone order: by rotation class, inside a class as the source has them ------------------
		std::vector<std::size_t> order;
		for (std::uint8_t type = ANIM_ROT_NONE; type <= ANIM_ROT_3D_STATIC; type++)
		{
			for (std::size_t p = 0; p < parts; p++)
			{
				if (p != deltaPart && rotations[p].type == type)
				{
					order.push_back(p);
					h.boneCount[type]++;
				}
			}
		}
		if (order.size() > 255)
		{
			return fail("more than 255 parts");
		}
		h.boneCount[ANIM_PART_TOTAL] = static_cast<std::uint8_t>(order.size());
		for (const std::size_t p : order)
		{
			raw.boneNames.push_back(source.parts[p]);
		}

		const auto frame_table = [&](const std::vector<std::uint16_t>& keys)
		{
			const std::size_t tableSize = keys.size() - 1;
			raw.dataShort.push_back(static_cast<std::int16_t>(static_cast<std::uint16_t>(tableSize)));
			if (!wide)
			{
				for (const std::uint16_t k : keys)
				{
					raw.dataByte.push_back(static_cast<std::uint8_t>(k));
				}
			}
			else if (tableSize < 64)
			{
				for (const std::uint16_t k : keys)
				{
					raw.dataShort.push_back(static_cast<std::int16_t>(k));
				}
			}
			else
			{
				for (std::size_t k = 0; k <= (tableSize - 1) >> 8; k++)
				{
					raw.dataShort.push_back(static_cast<std::int16_t>(keys[k * 256]));
				}
				raw.dataShort.push_back(static_cast<std::int16_t>(keys[tableSize]));
				raw.indices.insert(raw.indices.end(), keys.begin(), keys.end());
			}
		};
		for (const std::size_t p : order)
		{
			const RotTrack& r = rotations[p];
			if (r.type == ANIM_ROT_2D || r.type == ANIM_ROT_3D)
			{
				frame_table(r.keys);
				raw.randomDataShort.insert(raw.randomDataShort.end(), r.values.begin(), r.values.end());
			}
		}
		for (const std::size_t p : order)
		{
			const RotTrack& r = rotations[p];
			if (r.type == ANIM_ROT_2D_STATIC || r.type == ANIM_ROT_3D_STATIC)
			{
				raw.dataShort.insert(raw.dataShort.end(), r.values.begin(), r.values.end());
			}
		}
		for (std::uint8_t type = ANIM_TRANS_SMALL; type <= ANIM_TRANS_NONE; type++)
		{
			for (std::size_t index = 0; index < order.size(); index++)
			{
				const TransTrack& t = translations[order[index]];
				if (t.type != type)
				{
					continue;
				}
				h.boneCount[type]++;
				raw.dataByte.push_back(static_cast<std::uint8_t>(index));
				if (type == ANIM_TRANS_STATIC)
				{
					for (const float c : t.constant)
					{
						put_float(raw.dataInt, c);
					}
				}
				else if (type != ANIM_TRANS_NONE)
				{
					frame_table(t.keys);
					for (const float c : t.mins)
					{
						put_float(raw.dataInt, c);
					}
					for (const float c : t.size)
					{
						put_float(raw.dataInt, c);
					}
					for (const std::uint16_t v : t.values)
					{
						if (type == ANIM_TRANS_SMALL)
						{
							raw.randomDataByte.push_back(static_cast<std::uint8_t>(v));
						}
						else
						{
							raw.randomDataShort.push_back(static_cast<std::int16_t>(v));
						}
					}
				}
			}
		}
		if (raw.dataByte.size() > 0xFFFF || raw.dataShort.size() > 0xFFFF || raw.dataInt.size() > 0xFFFF || raw.randomDataByte.size() > 0xFFFF)
		{
			return fail("too much data for one animation ( a stream count does not fit 16 bits ): fewer frames or parts, or larger tolerances");
		}
		h.dataByteCount = static_cast<std::uint16_t>(raw.dataByte.size());
		h.dataShortCount = static_cast<std::uint16_t>(raw.dataShort.size());
		h.dataIntCount = static_cast<std::uint16_t>(raw.dataInt.size());
		h.randomDataByteCount = static_cast<std::uint16_t>(raw.randomDataByte.size());
		h.randomDataShortCount = static_cast<std::uint32_t>(raw.randomDataShort.size());
		h.indexCount = static_cast<std::uint32_t>(raw.indices.size());

		// ---- notifies ---------------------------------------------------------------------------
		for (const ExportNote& note : source.notes)
		{
			const float time = h.numframes ? std::clamp((note.frame - static_cast<float>(source.firstFrame)) / static_cast<float>(h.numframes), 0.0f, 1.0f) : 0.0f;
			raw.notifies.emplace_back(note.name, time);
		}
		std::stable_sort(raw.notifies.begin(), raw.notifies.end(), [](const auto& a, const auto& b) { return a.second < b.second; });
		// the "end" every shipped animation closes with; one the file gives for the last frame is that one
		for (auto it = raw.notifies.begin(); it != raw.notifies.end();)
		{
			it = it->first == "end" && (it->second >= 1.0f || h.numframes == 0) ? raw.notifies.erase(it) : it + 1;
		}
		raw.notifies.emplace_back("end", 1.0f);
		if (raw.notifies.size() > 255)
		{
			return fail("more than 254 notes");
		}
		h.notifyCount = static_cast<std::uint8_t>(raw.notifies.size());
		return {};
	}

	void extract_anim(const DecodedAnim& anim, const ExportModel* skeletonModel, ExportAnim& out)
	{
		out = {};
		out.framerate = anim.framerate;
		const std::size_t frames = static_cast<std::size_t>(anim.lastFrame) + 1;
		const std::size_t first = anim.hasDelta ? 1 : 0;
		if (anim.hasDelta)
		{
			out.parts.push_back("tag_origin");
		}
		for (const DecodedBone& bone : anim.bones)
		{
			out.parts.push_back(bone.name);
		}
		out.frames.assign(frames, std::vector<ExportPose>(out.parts.size()));
		const auto store = [](const Pose& pose, ExportPose& target)
		{
			axis_from_quat(pose.q, target.axis);
			std::memcpy(target.offset, pose.t.data(), 12);
		};
		// the stored pose of every part, relative to its parent
		const auto local_pose = [&](const std::size_t part, const float frame, const Vec3* bindTranslation) -> Pose
		{
			if (part < first)
			{
				return { anim.deltaRot.at(frame), anim.deltaTrans.at(frame) };
			}
			const DecodedBone& bone = anim.bones[part - first];
			Pose pose;
			pose.q = bone.rot.at(frame);
			pose.t = bone.translation == ANIM_TRANS_NONE ? (bindTranslation ? *bindTranslation : Vec3{ 0, 0, 0 }) : bone.trans.at(frame);
			return pose;
		};
		if (!skeletonModel)
		{
			out.local = true;
			for (std::size_t k = 0; k < anim.bones.size(); k++)
			{
				if (anim.bones[k].translation == ANIM_TRANS_NONE)
				{
					out.noTranslation.push_back(static_cast<int>(k + first));
				}
			}
			for (std::size_t f = 0; f < frames; f++)
			{
				for (std::size_t p = 0; p < out.parts.size(); p++)
				{
					store(local_pose(p, static_cast<float>(f), nullptr), out.frames[f][p]);
				}
			}
		}
		else
		{
			const Skeleton skeleton(*skeletonModel);
			const std::size_t bones = skeleton.parent.size();
			std::vector<int> partOfBone(bones, -1);
			std::vector<int> boneOfPart(out.parts.size(), -1);
			for (std::size_t p = 0; p < out.parts.size(); p++)
			{
				if (const auto hit = skeleton.byName.find(out.parts[p]); hit != skeleton.byName.end())
				{
					boneOfPart[p] = static_cast<int>(hit->second);
					partOfBone[hit->second] = static_cast<int>(p);
				}
			}
			std::vector<Pose> world(bones);
			for (std::size_t f = 0; f < frames; f++)
			{
				for (const std::size_t b : skeleton.order)
				{
					const Pose local = partOfBone[b] >= 0 ? local_pose(static_cast<std::size_t>(partOfBone[b]), static_cast<float>(f), &skeleton.bindLocal[b].t) : skeleton.bindLocal[b];
					world[b] = skeleton.parent[b] < 0 ? local : compose(world[static_cast<std::size_t>(skeleton.parent[b])], local);
				}
				for (std::size_t p = 0; p < out.parts.size(); p++)
				{
					store(boneOfPart[p] >= 0 ? world[static_cast<std::size_t>(boneOfPart[p])] : local_pose(p, static_cast<float>(f), nullptr), out.frames[f][p]);
				}
			}
		}
		for (const auto& [name, time] : anim.notifies)
		{
			out.notes.push_back({ name, time * static_cast<float>(anim.lastFrame) });
		}
	}
}
