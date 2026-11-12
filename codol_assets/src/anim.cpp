// XAnimParts: reading the stored form out of a zone, decoding it to key tracks.

#include "codol_assets/anim.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace codol::assets
{
	namespace
	{
		constexpr float QUAT_SCALE = 1.0f / 32767.0f;

		Quat normalised(Quat q)
		{
			const float length = std::sqrt(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);
			if (length < 1e-12f)
			{
				return { 0, 0, 0, 1 };
			}
			for (float& c : q)
			{
				c /= length;
			}
			return q;
		}

		template <typename T>
		bool read_array(const zone::Zone& zone, const zone::zaddr address, const std::size_t count, std::vector<T>& out)
		{
			out.clear();
			if (!count)
			{
				return true;
			}
			out = zone.get_array<T>(address, count);
			return out.size() == count;
		}

		bool read_indices(const zone::Zone& zone, const zone::zaddr address, const std::size_t count, const bool wide, std::vector<std::uint16_t>& out)
		{
			if (wide)
			{
				return read_array(zone, address, count, out);
			}
			std::vector<std::uint8_t> narrow;
			if (!read_array(zone, address, count, narrow))
			{
				return false;
			}
			out.assign(narrow.begin(), narrow.end());
			return true;
		}

		// reads a stream in order and says when it runs dry
		template <typename T>
		struct Reader
		{
			const std::vector<T>& data;
			std::size_t at = 0;
			bool ok = true;

			T next()
			{
				if (at >= data.size())
				{
					ok = false;
					return T{};
				}
				return data[at++];
			}
			void skip(const std::size_t n)
			{
				if (at + n > data.size())
				{
					ok = false;
				}
				at += n;
			}
			bool done() const { return ok && at == data.size(); }
		};

		float bits_to_float(const std::uint32_t bits)
		{
			float f;
			std::memcpy(&f, &bits, 4);
			return f;
		}
	}

	Quat quat_from_axis(const float axis[3][3])
	{
		const float m00 = axis[0][0], m10 = axis[0][1], m20 = axis[0][2];
		const float m01 = axis[1][0], m11 = axis[1][1], m21 = axis[1][2];
		const float m02 = axis[2][0], m12 = axis[2][1], m22 = axis[2][2];
		Quat q;
		const float trace = m00 + m11 + m22;
		if (trace > 0)
		{
			const float s = std::sqrt(trace + 1.0f) * 2;
			q = { (m21 - m12) / s, (m02 - m20) / s, (m10 - m01) / s, 0.25f * s };
		}
		else if (m00 > m11 && m00 > m22)
		{
			const float s = std::sqrt(1.0f + m00 - m11 - m22) * 2;
			q = { 0.25f * s, (m01 + m10) / s, (m02 + m20) / s, (m21 - m12) / s };
		}
		else if (m11 > m22)
		{
			const float s = std::sqrt(1.0f + m11 - m00 - m22) * 2;
			q = { (m01 + m10) / s, 0.25f * s, (m12 + m21) / s, (m02 - m20) / s };
		}
		else
		{
			const float s = std::sqrt(1.0f + m22 - m00 - m11) * 2;
			q = { (m02 + m20) / s, (m12 + m21) / s, 0.25f * s, (m10 - m01) / s };
		}
		return normalised(q);
	}

	void axis_from_quat(const Quat& q, float axis[3][3])
	{
		const float x = q[0], y = q[1], z = q[2], w = q[3];
		axis[0][0] = 1 - 2 * (y * y + z * z);
		axis[0][1] = 2 * (x * y + z * w);
		axis[0][2] = 2 * (x * z - y * w);
		axis[1][0] = 2 * (x * y - z * w);
		axis[1][1] = 1 - 2 * (x * x + z * z);
		axis[1][2] = 2 * (y * z + x * w);
		axis[2][0] = 2 * (x * z + y * w);
		axis[2][1] = 2 * (y * z - x * w);
		axis[2][2] = 1 - 2 * (x * x + y * y);
	}

	float quat_angle_between(const Quat& a, const Quat& b)
	{
		// through the chord: acos of a dot product near 1 has no precision left in floats
		const float sign = a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3] < 0 ? -1.0f : 1.0f;
		double chord = 0;
		for (std::size_t c = 0; c < 4; c++)
		{
			const double d = static_cast<double>(a[c]) - static_cast<double>(sign * b[c]);
			chord += d * d;
		}
		return static_cast<float>(4.0 * std::asin(std::min(1.0, std::sqrt(chord) / 2.0)));
	}

	// ---- tracks ---------------------------------------------------------------------------------------
	namespace
	{
		// key index k with frames[k] <= frame < frames[k + 1], and the fraction between them
		std::size_t locate(const std::vector<std::uint16_t>& frames, const float frame, float& fraction)
		{
			fraction = 0;
			if (frames.size() < 2 || frame <= static_cast<float>(frames.front()))
			{
				return 0;
			}
			if (frame >= static_cast<float>(frames.back()))
			{
				return frames.size() - 1;
			}
			const auto upper = std::upper_bound(frames.begin(), frames.end(), frame, [](const float f, const std::uint16_t v) { return f < static_cast<float>(v); });
			const std::size_t k = static_cast<std::size_t>(upper - frames.begin()) - 1;
			fraction = (frame - static_cast<float>(frames[k])) / static_cast<float>(frames[k + 1] - frames[k]);
			return k;
		}
	}

	Quat QuatTrack::at(const float frame) const
	{
		if (values.empty())
		{
			return { 0, 0, 0, 1 };
		}
		float t = 0;
		const std::size_t k = locate(frames, frame, t);
		if (t <= 0 || k + 1 >= values.size())
		{
			return values[k];
		}
		const Quat& a = values[k];
		Quat b = values[k + 1];
		// the stored values of neighbouring keys are on the same side ( checked by the survey ); guard anyway
		if (a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3] < 0)
		{
			b = { -b[0], -b[1], -b[2], -b[3] };
		}
		return normalised({ a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t, a[2] + (b[2] - a[2]) * t, a[3] + (b[3] - a[3]) * t });
	}

	Vec3 VecTrack::at(const float frame) const
	{
		if (values.empty())
		{
			return { 0, 0, 0 };
		}
		float t = 0;
		const std::size_t k = locate(frames, frame, t);
		if (t <= 0 || k + 1 >= values.size())
		{
			return values[k];
		}
		const Vec3& a = values[k];
		const Vec3& b = values[k + 1];
		return { a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t, a[2] + (b[2] - a[2]) * t };
	}

	// ---- reading --------------------------------------------------------------------------------------
	bool read_raw_anim(const zone::Zone& zone, const zone::zaddr anim, RawAnim& out, std::string* error)
	try
	{
		const auto fail = [&](const char* what)
		{
			if (error)
			{
				*error = what;
			}
			return false;
		};
		out = {};
		zone::AnimView view;
		if (!zone::read_anim(zone, anim, view) || view.isReference)
		{
			return fail("not an animation of this zone");
		}
		out.name = std::string(view.name);
		out.header = view.raw;
		const c1::XAnimParts& h = out.header;
		for (const std::string_view bone : view.boneNames)
		{
			out.boneNames.emplace_back(bone);
		}
		for (const auto& [name, time] : view.notetracks)
		{
			out.notifies.emplace_back(std::string(name), time);
		}
		const bool wide = h.numframes >= 256;
		if (!read_array(zone, h.dataByte, h.dataByteCount, out.dataByte) || !read_array(zone, h.dataShort, h.dataShortCount, out.dataShort)
			|| !read_array(zone, h.dataInt, h.dataIntCount, out.dataInt) || !read_array(zone, h.randomDataShort, h.randomDataShortCount, out.randomDataShort)
			|| !read_array(zone, h.randomDataByte, h.randomDataByteCount, out.randomDataByte) || !read_array(zone, h.randomDataInt, h.randomDataIntCount, out.randomDataInt)
			|| !read_array(zone, h.indices, wide ? h.indexCount : 0, out.indices))
		{
			return fail("a data stream is not mapped");
		}
		if (h.deltaPart)
		{
			out.hasDelta = true;
			std::uint32_t part[3];		// trans, quat2, quat
			if (!zone.get(h.deltaPart, part))
			{
				return fail("delta part is not mapped");
			}
			if (part[0])
			{
				RawDeltaTrans& t = out.deltaTrans;
				t.present = true;
				t.size = zone.get<std::uint16_t>(part[0]);
				t.small = zone.get<std::uint8_t>(part[0] + 2) != 0;
				if (t.size == 0)
				{
					zone.get(part[0] + 4, t.frame0);
				}
				else
				{
					zone.get(part[0] + 4, t.mins);
					zone.get(part[0] + 16, t.frameSize);
					const zone::zaddr frames = zone.get<std::uint32_t>(part[0] + 28);
					const std::size_t keys = static_cast<std::size_t>(t.size) + 1;
					if (!read_indices(zone, part[0] + 32, keys, wide, t.indices))
					{
						return fail("delta translation indices are not mapped");
					}
					if (t.small)
					{
						std::vector<std::uint8_t> bytes;
						if (!read_array(zone, frames, keys * 3, bytes))
						{
							return fail("delta translation frames are not mapped");
						}
						t.frames.assign(bytes.begin(), bytes.end());
					}
					else if (!read_array(zone, frames, keys * 3, t.frames))
					{
						return fail("delta translation frames are not mapped");
					}
				}
			}
			for (int which = 0; which < 2; which++)
			{
				const zone::zaddr address = part[1 + which];
				if (!address)
				{
					continue;
				}
				RawDeltaQuat& q = which == 0 ? out.deltaQuat2 : out.deltaQuat;
				const std::size_t width = which == 0 ? 2 : 4;
				q.present = true;
				q.size = zone.get<std::uint16_t>(address);
				if (q.size == 0)
				{
					if (!read_array(zone, address + 4, width, q.frames))
					{
						return fail("delta rotation is not mapped");
					}
				}
				else
				{
					const zone::zaddr frames = zone.get<std::uint32_t>(address + 4);
					const std::size_t keys = static_cast<std::size_t>(q.size) + 1;
					if (!read_indices(zone, address + 8, keys, wide, q.indices) || !read_array(zone, frames, keys * width, q.frames))
					{
						return fail("delta rotation frames are not mapped");
					}
				}
			}
		}
		return true;
	}
	catch (const std::exception&)
	{
		out = {};
		return false;
	}

	// ---- decoding -------------------------------------------------------------------------------------
	Status decode_anim(const RawAnim& raw, DecodedAnim& out)
	{
		out = {};
		const c1::XAnimParts& h = raw.header;
		const auto fail = [&](const std::string& what) { return Status::error("xanim '" + raw.name + "': " + what); };
		out.name = raw.name;
		out.lastFrame = h.numframes;
		out.framerate = h.framerate;
		out.frequency = h.frequency;
		out.flags = h.flags;
		out.assetType = h.assetType;
		out.notifies = raw.notifies;
		const std::size_t parts = h.boneCount[ANIM_PART_TOTAL];
		const std::uint8_t* counts = h.boneCount;
		if (static_cast<std::size_t>(counts[0]) + counts[1] + counts[2] + counts[3] + counts[4] != parts
			|| static_cast<std::size_t>(counts[5]) + counts[6] + counts[7] + counts[8] != parts || raw.boneNames.size() != parts)
		{
			return fail("the part counts do not add up");
		}
		const bool wide = h.numframes >= 256;
		Reader<std::uint8_t> dataByte{ raw.dataByte };
		Reader<std::int16_t> dataShort{ raw.dataShort };
		Reader<std::uint32_t> dataInt{ raw.dataInt };
		Reader<std::int16_t> randomShort{ raw.randomDataShort };
		Reader<std::uint8_t> randomByte{ raw.randomDataByte };
		Reader<std::uint16_t> indices{ raw.indices };

		bool tablesOk = true;
		const auto frame_table = [&](std::vector<std::uint16_t>& frames)
		{
			const std::size_t tableSize = static_cast<std::uint16_t>(dataShort.next());
			const std::size_t count = tableSize + 1;
			frames.resize(count);
			if (!wide)
			{
				for (std::uint16_t& f : frames)
				{
					f = dataByte.next();
				}
			}
			else if (tableSize < 64)
			{
				for (std::uint16_t& f : frames)
				{
					f = static_cast<std::uint16_t>(dataShort.next());
				}
			}
			else
			{
				// a coarse table for the search ( every 256th entry and the last ) in dataShort, the table itself in `indices`
				dataShort.skip(((tableSize - 1) >> 8) + 2);
				for (std::uint16_t& f : frames)
				{
					f = indices.next();
				}
			}
			for (std::size_t k = 0; k < count; k++)
			{
				tablesOk = tablesOk && frames[k] <= h.numframes && (k == 0 || frames[k] > frames[k - 1]);
			}
		};
		const auto half = [](const std::int16_t z, const std::int16_t w) { return normalised({ 0, 0, z * QUAT_SCALE, w * QUAT_SCALE }); };
		const auto full = [](const std::int16_t x, const std::int16_t y, const std::int16_t z, const std::int16_t w)
		{
			return normalised({ x * QUAT_SCALE, y * QUAT_SCALE, z * QUAT_SCALE, w * QUAT_SCALE });
		};

		out.bones.resize(parts);
		std::size_t part = 0;
		for (std::uint8_t type = ANIM_ROT_NONE; type <= ANIM_ROT_3D_STATIC; type++)
		{
			for (std::size_t i = 0; i < counts[type]; i++, part++)
			{
				DecodedBone& bone = out.bones[part];
				bone.name = raw.boneNames[part];
				bone.rotation = type;
				if (type == ANIM_ROT_NONE)
				{
					continue;
				}
				if (type == ANIM_ROT_2D || type == ANIM_ROT_3D)
				{
					frame_table(bone.rot.frames);
				}
				else
				{
					bone.rot.frames = { 0 };
				}
				Reader<std::int16_t>& values = type == ANIM_ROT_2D || type == ANIM_ROT_3D ? randomShort : dataShort;
				bone.rot.values.resize(bone.rot.frames.size());
				for (Quat& q : bone.rot.values)
				{
					if (type == ANIM_ROT_2D || type == ANIM_ROT_2D_STATIC)
					{
						const std::int16_t z = values.next(), w = values.next();
						q = half(z, w);
					}
					else
					{
						const std::int16_t x = values.next(), y = values.next(), z = values.next(), w = values.next();
						q = full(x, y, z, w);
					}
				}
			}
		}
		std::vector<bool> has(parts, false);
		bool bonesOk = true;
		const auto bone_index = [&]() -> std::size_t
		{
			const std::size_t index = dataByte.next();
			if (index >= parts || has[index])
			{
				bonesOk = false;
				return 0;
			}
			has[index] = true;
			return index;
		};
		const auto vec = [&]() -> Vec3
		{
			const float x = bits_to_float(dataInt.next()), y = bits_to_float(dataInt.next()), z = bits_to_float(dataInt.next());
			return { x, y, z };
		};
		for (std::uint8_t type = ANIM_TRANS_SMALL; type <= ANIM_TRANS_NONE && parts; type++)
		{
			for (std::size_t i = 0; i < counts[type] && bonesOk; i++)
			{
				DecodedBone& bone = out.bones[bone_index()];
				bone.translation = type;
				if (type == ANIM_TRANS_NONE)
				{
					continue;
				}
				if (type == ANIM_TRANS_STATIC)
				{
					bone.trans.frames = { 0 };
					bone.trans.values = { vec() };
					continue;
				}
				frame_table(bone.trans.frames);
				const Vec3 mins = vec(), size = vec();
				bone.trans.values.resize(bone.trans.frames.size());
				for (Vec3& v : bone.trans.values)
				{
					for (std::size_t c = 0; c < 3; c++)
					{
						const float raw16 = type == ANIM_TRANS_SMALL ? static_cast<float>(randomByte.next()) : static_cast<float>(static_cast<std::uint16_t>(randomShort.next()));
						v[c] = mins[c] + size[c] * raw16;
					}
				}
			}
		}
		if (!bonesOk)
		{
			return fail("a translation names a bone that does not exist, or one twice");
		}
		if (!tablesOk)
		{
			return fail("a frame table is not ascending inside the animation");
		}
		if (!dataByte.done() || !dataShort.done() || !dataInt.done() || !randomShort.done() || !randomByte.done() || !indices.done() || !raw.randomDataInt.empty())
		{
			return fail("the parts do not use the data streams exactly");
		}

		out.hasDelta = raw.hasDelta;
		if (raw.deltaTrans.present)
		{
			const RawDeltaTrans& t = raw.deltaTrans;
			if (t.size == 0)
			{
				out.deltaTrans.frames = { 0 };
				out.deltaTrans.values = { { t.frame0[0], t.frame0[1], t.frame0[2] } };
			}
			else
			{
				out.deltaTrans.frames = t.indices;
				for (std::size_t k = 0; k + 2 < t.frames.size(); k += 3)
				{
					out.deltaTrans.values.push_back({ t.mins[0] + t.frameSize[0] * static_cast<float>(t.frames[k]), t.mins[1] + t.frameSize[1] * static_cast<float>(t.frames[k + 1]),
						t.mins[2] + t.frameSize[2] * static_cast<float>(t.frames[k + 2]) });
				}
			}
		}
		const RawDeltaQuat* q = raw.deltaQuat.present ? &raw.deltaQuat : raw.deltaQuat2.present ? &raw.deltaQuat2 : nullptr;
		if (q)
		{
			out.delta3D = raw.deltaQuat.present;
			const std::size_t width = out.delta3D ? 4 : 2;
			out.deltaRot.frames = q->size == 0 ? std::vector<std::uint16_t>{ 0 } : q->indices;
			for (std::size_t k = 0; k + width <= q->frames.size(); k += width)
			{
				out.deltaRot.values.push_back(out.delta3D ? full(q->frames[k], q->frames[k + 1], q->frames[k + 2], q->frames[k + 3]) : half(q->frames[k], q->frames[k + 1]));
			}
			if (out.deltaRot.values.size() != out.deltaRot.frames.size())
			{
				return fail("the delta rotation has another number of values than keys");
			}
		}
		if (out.deltaTrans.values.size() != out.deltaTrans.frames.size())
		{
			return fail("the delta translation has another number of values than keys");
		}
		return {};
	}
}
