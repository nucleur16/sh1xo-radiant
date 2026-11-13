// animation tests of codol_assets_tests ( included by codol_assets_tests.cpp, which defines CHECK )

#include "codol_assets/anim.hpp"

namespace anim_tests
{
	using namespace codol::assets;
	namespace c1 = codol::c1;
	namespace z = codol::zone;

	inline Quat about_z(const float degrees)
	{
		const float half = degrees * 0.008726646f;
		return { 0, 0, std::sin(half), std::cos(half) };
	}

	inline Quat about_x(const float degrees)
	{
		const float half = degrees * 0.008726646f;
		return { std::sin(half), 0, 0, std::cos(half) };
	}

	inline ExportPose pose(const Quat& q, const float x, const float y, const float zed)
	{
		ExportPose p;
		axis_from_quat(q, p.axis);
		p.offset[0] = x;
		p.offset[1] = y;
		p.offset[2] = zed;
		return p;
	}

	inline ExportAnim make(const std::vector<std::string>& parts, const std::size_t frames)
	{
		ExportAnim anim;
		anim.parts = parts;
		anim.frames.assign(frames, std::vector<ExportPose>(parts.size()));
		return anim;
	}

	inline const DecodedBone* bone(const DecodedAnim& d, const std::string& name)
	{
		for (const DecodedBone& b : d.bones)
		{
			if (b.name == name)
			{
				return &b;
			}
		}
		return nullptr;
	}

	inline void test_text()
	{
		const char* text =
			"// made by hand\nANIMATION\nVERSION 3\n\nNUMPARTS 2\nPART 0 \"tag_origin\"\nPART 1 \"j_door\"\n\nFRAMERATE 24\nNUMFRAMES 3\n\n"
			"FRAME 10\nPART 0\nOFFSET 0 0 0\nSCALE 1 1 1\nX 1 0 0\nY 0 1 0\nZ 0 0 1\nPART 1\nOFFSET 1.5, 2, 3\nX 0 1 0\nY -1 0 0\nZ 0 0 1\n\n"
			"FRAME 11\nPART 1\nOFFSET 4 5 6\nX 1 0 0\nY 0 1 0\nZ 0 0 1\n\n"
			"FRAME 12\nPART 0\nOFFSET 7 0 0\nX 1 0 0\nY 0 1 0\nZ 0 0 1\n\n"
			"NOTETRACKS\n\nPART 0\nNUMTRACKS 1\nNOTETRACK 0\nNUMKEYS 2\nFRAME 11 \"door_open\"\nFRAME 12 \"anim_movement = \"run\"\"\n\nPART 1\nNUMTRACKS 0\n";
		ExportAnim anim;
		CHECK(parse_xanim_export(text, anim));
		CHECK(anim.parts.size() == 2 && anim.parts[1] == "j_door" && anim.framerate == 24 && anim.firstFrame == 10 && anim.frames.size() == 3 && !anim.local);
		CHECK(anim.frames[0][1].offset[0] == 1.5f && anim.frames[0][1].axis[0][1] == 1 && anim.frames[0][1].axis[1][0] == -1);
		// a part a frame leaves out keeps its pose
		CHECK(anim.frames[1][0].offset[0] == 0 && anim.frames[2][1].offset[2] == 6 && anim.frames[2][0].offset[0] == 7);
		CHECK(anim.notes.size() == 2 && anim.notes[0].name == "door_open" && anim.notes[0].frame == 11 && anim.notes[1].name == "anim_movement = \"run\"");

		anim.local = true;
		anim.noTranslation = { 1 };
		ExportAnim again;
		CHECK(parse_xanim_export(write_xanim_export(anim, "comment"), again));
		CHECK(again.local && again.noTranslation == std::vector<int>{ 1 } && again.parts == anim.parts && again.frames.size() == 3 && again.firstFrame == 10);
		CHECK(again.frames[0][1].offset[1] == 2 && again.frames[0][1].axis[1][0] == -1 && again.notes.size() == 2 && again.notes[1].name == anim.notes[1].name);

		CHECK(!parse_xanim_export("ANIMATION\nVERSION 2\n", again));
		CHECK(!parse_xanim_export("ANIMATION\nVERSION 3\nNUMPARTS 1\nPART 0 \"a\"\nFRAMERATE 30\nNUMFRAMES 2\nFRAME 0\nPART 0\nOFFSET 0 0 0\n", again));	// one frame of two
		CHECK(!parse_xanim_export("MODEL\nVERSION 6\n", again));
		CHECK(!parse_xanim_export("ANIMATION\nVERSION 3\nNUMPARTS 1\nPART 0 \"a\"\nFRAMERATE 30\nNUMFRAMES 1\nFRAME 0\nPART 3\nOFFSET 0 0 0\n", again));
	}

	inline void test_classes_and_keys()
	{
		// j_spin turns about z at a constant rate and moves along a line; j_tilt is constant about x; j_still is the identity
		const std::size_t frames = 52;		// 51 steps: the straight moves land on whole steps of both boxes
		ExportAnim anim = make({ "j_still", "j_tilt", "j_spin", "j_far", "j_wave" }, frames);
		anim.framerate = 30;
		for (std::size_t f = 0; f < frames; f++)
		{
			const float t = static_cast<float>(f);
			anim.frames[f][0] = pose({ 0, 0, 0, 1 }, 0, 0, 0);
			anim.frames[f][1] = pose(about_x(30), 1, 2, 3);
			anim.frames[f][2] = pose(about_z(2 * t), 0.1f * t, 0, 0);			// 5.1 units in all: 8 bit steps
			anim.frames[f][3] = pose(about_z(90), 2 * t, 0, 0);					// 102 units: 16 bit steps
			anim.frames[f][4] = pose(about_x(20 * std::sin(t * 0.05f)), 0, 0, 5);	// a curve: needs its keys
		}
		anim.notes = { { "third", 17 }, { "start", 0 } };
		AnimOptions options;
		options.name = "codol_unit_anim";
		options.looping = true;
		options.rotationTolerance = 0;
		options.translationTolerance = 0;
		CompiledAnim compiled;
		CHECK(compile_anim(anim, options, compiled));
		const RawAnim& raw = compiled.raw;
		const c1::XAnimParts& h = raw.header;
		CHECK(h.numframes == 51 && h.flags == ANIM_FLAG_LOOP && h.framerate == 30 && std::fabs(h.frequency - 30.0f / 51.0f) < 1e-6f && h.assetType == 2 && !raw.hasDelta);
		// order: none, 2d keys, 3d keys, 2d constant, 3d constant
		CHECK((raw.boneNames == std::vector<std::string>{ "j_still", "j_spin", "j_wave", "j_far", "j_tilt" }));
		CHECK(h.boneCount[0] == 1 && h.boneCount[1] == 1 && h.boneCount[2] == 1 && h.boneCount[3] == 1 && h.boneCount[4] == 1 && h.boneCount[9] == 5);
		CHECK(h.boneCount[5] == 1 && h.boneCount[6] == 1 && h.boneCount[7] == 3 && h.boneCount[8] == 0);
		CHECK(h.notifyCount == 3 && raw.notifies[0].first == "start" && raw.notifies[1].first == "third" && std::fabs(raw.notifies[1].second - 17.0f / 51.0f) < 1e-6f && raw.notifies[2].first == "end"
			&& raw.notifies[2].second == 1.0f);
		CHECK(h.dataByteCount == raw.dataByte.size() && h.dataShortCount == raw.dataShort.size() && h.randomDataShortCount == raw.randomDataShort.size() && h.indexCount == 0);

		DecodedAnim d;
		CHECK(decode_anim(raw, d));
		const DecodedBone* spin = bone(d, "j_spin");
		const DecodedBone* farBone = bone(d, "j_far");
		const DecodedBone* wave = bone(d, "j_wave");
		const DecodedBone* tilt = bone(d, "j_tilt");
		CHECK(spin && farBone && wave && tilt);
		if (spin && farBone && wave && tilt)
		{
			CHECK(spin->rotation == ANIM_ROT_2D && spin->translation == ANIM_TRANS_SMALL && farBone->rotation == ANIM_ROT_2D_STATIC && farBone->translation == ANIM_TRANS_LARGE);
			CHECK(wave->rotation == ANIM_ROT_3D && wave->translation == ANIM_TRANS_STATIC && tilt->rotation == ANIM_ROT_3D_STATIC && tilt->translation == ANIM_TRANS_STATIC);
			// straight lines need their two ends only; the curve keeps most of its frames
			CHECK(spin->trans.frames.size() == 2 && farBone->trans.frames.size() == 2 && farBone->trans.frames.back() == 51 && wave->rot.frames.size() > 20);
			float worst = 0, worstMove = 0;
			for (std::size_t f = 0; f < frames; f++)
			{
				const float t = static_cast<float>(f);
				worst = std::max({ worst, quat_angle_between(spin->rot.at(t), about_z(2 * t)), quat_angle_between(wave->rot.at(t), about_x(20 * std::sin(t * 0.05f))),
					quat_angle_between(tilt->rot.at(t), about_x(30)) });
				worstMove = std::max({ worstMove, std::fabs(spin->trans.at(t)[0] - 0.1f * t), std::fabs(farBone->trans.at(t)[0] - 2 * t) });
			}
			CHECK(worst < 0.0004f && worstMove < 0.01f);
			CHECK(wave->trans.values[0][2] == 5 && tilt->trans.values[0][0] == 1);
		}

		// a tolerance drops keys of the curve and stays inside it
		options.rotationTolerance = 16;
		CompiledAnim loose;
		DecodedAnim dl;
		CHECK(compile_anim(anim, options, loose) && decode_anim(loose.raw, dl));
		const DecodedBone* looseWave = bone(dl, "j_wave");
		CHECK(looseWave && wave && looseWave->rot.frames.size() < wave->rot.frames.size());
		if (looseWave)
		{
			float worst = 0;
			for (std::size_t f = 0; f < frames; f++)
			{
				worst = std::max(worst, quat_angle_between(looseWave->rot.at(static_cast<float>(f)), about_x(20 * std::sin(static_cast<float>(f) * 0.05f))));
			}
			CHECK(worst < 4.0f * 17.5f / 32767.0f);
		}

		// one frame: a pose. numframes 0, frequency 0, everything constant
		ExportAnim still = make({ "a" }, 1);
		still.frames[0][0] = pose(about_x(10), 1, 0, 0);
		CompiledAnim one;
		CHECK(compile_anim(still, options, one) && one.raw.header.numframes == 0 && one.raw.header.frequency == 0 && one.raw.header.boneCount[4] == 1 && one.raw.notifies.size() == 1);

		// errors
		ExportAnim twice = make({ "a", "a" }, 2);
		CHECK(!compile_anim(twice, options, one));
		options.name.clear();
		CHECK(!compile_anim(still, options, one));
	}

	inline void test_long()
	{
		// 400 frames: u16 frame numbers; a track with more than 64 keys has its table in `indices`, one with fewer in dataShort
		const std::size_t frames = 400;
		ExportAnim anim = make({ "j_busy", "j_slow" }, frames);
		for (std::size_t f = 0; f < frames; f++)
		{
			const float t = static_cast<float>(f);
			anim.frames[f][0] = pose(about_x(40 * std::sin(t * 0.7f)), 0, 0, 0);
			anim.frames[f][1] = pose(about_z(f < 200 ? 0.0f : 45.0f), 0, 0, 0);
		}
		AnimOptions options;
		options.name = "codol_unit_long";
		options.rotationTolerance = 0;
		CompiledAnim compiled;
		DecodedAnim d;
		CHECK(compile_anim(anim, options, compiled) && decode_anim(compiled.raw, d));
		const DecodedBone* busy = bone(d, "j_busy");
		const DecodedBone* slow = bone(d, "j_slow");
		CHECK(busy && slow && busy->rot.frames.size() > 64 && slow->rot.frames.size() == 4);
		CHECK(busy && compiled.raw.indices.size() == busy->rot.frames.size() && compiled.raw.header.indexCount == compiled.raw.indices.size());
		if (busy && slow)
		{
			float worst = 0;
			for (std::size_t f = 0; f < frames; f++)
			{
				const float t = static_cast<float>(f);
				worst = std::max({ worst, quat_angle_between(busy->rot.at(t), about_x(40 * std::sin(t * 0.7f))), quat_angle_between(slow->rot.at(t), about_z(f < 200 ? 0.0f : 45.0f)) });
			}
			CHECK(worst < 0.0004f);
		}
	}

	inline void test_skeleton_and_delta()
	{
		// tag_origin -> j_arm ( 10 along x ) -> j_hand ( 5 along the arm's y ). The arm swings about z, the hand stays put on it.
		ExportModel model;
		model.bones.resize(3);
		model.bones[0].name = "tag_origin";
		model.bones[1].name = "j_arm";
		model.bones[1].parent = 0;
		model.bones[1].offset[0] = 10;
		model.bones[2].name = "j_hand";
		model.bones[2].parent = 1;
		model.bones[2].offset[0] = 10;
		model.bones[2].offset[1] = 5;

		const std::size_t frames = 21;
		ExportAnim anim = make({ "tag_origin", "j_arm", "j_hand" }, frames);
		for (std::size_t f = 0; f < frames; f++)
		{
			const float t = static_cast<float>(f);
			const float yaw = 3 * t, walk = 4 * t;				// the whole thing walks along x and turns
			const float swing = 30 * std::sin(t * 0.3f);
			const float cy = std::cos(yaw * 0.01745329f), sy = std::sin(yaw * 0.01745329f);
			const float ca = std::cos((yaw + swing) * 0.01745329f), sa = std::sin((yaw + swing) * 0.01745329f);
			anim.frames[f][0] = pose(about_z(yaw), walk, 0, 0);
			const float armX = walk + 10 * cy, armY = 10 * sy;
			anim.frames[f][1] = pose(about_z(yaw + swing), armX, armY, 0);
			anim.frames[f][2] = pose(about_z(yaw + swing), armX - 5 * sa, armY + 5 * ca, 0);
		}
		AnimOptions options;
		options.name = "codol_unit_walk";
		options.type = AnimType::delta;
		options.skeleton = &model;
		options.rotationTolerance = 0;
		options.translationTolerance = 0;
		options.staticTranslationEpsilon = 0.001f;
		CompiledAnim compiled;
		DecodedAnim d;
		CHECK(compile_anim(anim, options, compiled) && decode_anim(compiled.raw, d));
		const RawAnim& raw = compiled.raw;
		// the root is the delta part and is not listed; the hand does not move on the arm: no rotation, no translation track
		CHECK(raw.header.flags == ANIM_FLAG_DELTA && raw.hasDelta && raw.deltaTrans.present && raw.deltaQuat2.present && !raw.deltaQuat.present);
		CHECK((raw.boneNames == std::vector<std::string>{ "j_hand", "j_arm" }) && raw.header.boneCount[0] == 1 && raw.header.boneCount[1] == 1 && raw.header.boneCount[8] == 2);
		CHECK(raw.deltaTrans.size == 1 && !raw.deltaTrans.small_box);		// a straight walk of 80 units: two keys, 16 bit
		const DecodedBone* arm = bone(d, "j_arm");
		CHECK(arm && arm->rotation == ANIM_ROT_2D && arm->translation == ANIM_TRANS_NONE);
		float worst = 0, worstMove = 0;
		for (std::size_t f = 0; f < frames && arm; f++)
		{
			const float t = static_cast<float>(f);
			worst = std::max({ worst, quat_angle_between(arm->rot.at(t), about_z(30 * std::sin(t * 0.3f))), quat_angle_between(d.deltaRot.at(t), about_z(3 * t)) });
			worstMove = std::max(worstMove, std::fabs(d.deltaTrans.at(t)[0] - 4 * t));
		}
		CHECK(worst < 0.0004f && worstMove < 0.01f);

		// back to model space on the skeleton and compiled again: the same animation
		ExportAnim extracted;
		extract_anim(d, &model, extracted);
		CHECK(extracted.parts.size() == 3 && extracted.parts[0] == "tag_origin" && !extracted.local && extracted.frames.size() == frames);
		CHECK(std::fabs(extracted.frames[10][1].offset[0] - anim.frames[10][2].offset[0]) < 0.01f && std::fabs(extracted.frames[10][1].offset[1] - anim.frames[10][2].offset[1]) < 0.01f
			&& extracted.parts[1] == "j_hand");
		CompiledAnim again;
		CHECK(compile_anim(extracted, options, again));
		CHECK(again.raw.boneNames == raw.boneNames && std::memcmp(again.raw.header.boneCount, raw.header.boneCount, 10) == 0 && again.raw.deltaTrans.size == raw.deltaTrans.size);

		// the stored form as text ( parent relative ) compiles without the skeleton
		ExportAnim local, parsed;
		extract_anim(d, nullptr, local);
		CHECK(local.local && local.noTranslation.size() == 2);
		options.skeleton = nullptr;
		CHECK(parse_xanim_export(write_xanim_export(local), parsed) && compile_anim(parsed, options, again));
		CHECK(again.raw.boneNames == raw.boneNames && std::memcmp(again.raw.header.boneCount, raw.header.boneCount, 10) == 0);

		// a root that does not move: a delta animation with an empty delta part ( 533 shipped ones )
		ExportAnim idle = make({ "tag_origin", "j_arm" }, 5);
		for (std::size_t f = 0; f < 5; f++)
		{
			idle.frames[f][1] = pose(about_z(static_cast<float>(f)), 10, 0, 0);
		}
		options.skeleton = &model;
		CHECK(compile_anim(idle, options, again) && again.raw.hasDelta && !again.raw.deltaTrans.present && !again.raw.deltaQuat2.present && again.raw.boneNames.size() == 1);

		// delta 3d keeps the whole rotation of the root
		options.type = AnimType::delta3d;
		for (std::size_t f = 0; f < 5; f++)
		{
			idle.frames[f][0] = pose(about_x(static_cast<float>(f) * 5), 0, 0, 0);
		}
		CHECK(compile_anim(idle, options, again) && again.raw.header.flags == ANIM_FLAG_DELTA_3D && again.raw.deltaQuat.present && !again.raw.deltaQuat2.present
			&& again.raw.deltaQuat.frames.size() == again.raw.deltaQuat.indices.size() * 4);
	}

	inline void test_anims()
	{
		test_text();
		test_classes_and_keys();
		test_long();
		test_skeleton_and_delta();
	}

	// two animations ( plain with long tables, delta ) linked into a zone, through the game's loaders and back
	inline void test_anim_zone(AssetLinker& linker, std::vector<RawAnim>& expected)
	{
		const std::size_t frames = 300;
		ExportAnim anim = make({ "tag_origin", "j_busy" }, frames);
		for (std::size_t f = 0; f < frames; f++)
		{
			const float t = static_cast<float>(f);
			anim.frames[f][0] = pose(about_z(t), t, 0, 0);
			anim.frames[f][1] = pose(about_x(40 * std::sin(t * 0.7f)), 0, 3 + std::sin(t * 0.2f), 0);
		}
		anim.notes = { { "fire", 150 } };
		AnimOptions options;
		options.name = "codol_unit_anim_delta";
		options.type = AnimType::delta;
		options.rotationTolerance = 0;
		CompiledAnim compiled;
		CHECK(compile_anim(anim, options, compiled) && linker.add_anim(compiled));
		CHECK(!linker.add_anim(compiled));		// a second one of that name
		expected.push_back(compiled.raw);
		options.name = "codol_unit_anim_plain";
		options.type = AnimType::relative;
		options.looping = true;
		CHECK(compile_anim(anim, options, compiled) && linker.add_anim(compiled));
		expected.push_back(compiled.raw);
	}

	inline void check_anim_zone(const z::Zone& zone, const std::vector<RawAnim>& expected)
	{
		for (const RawAnim& e : expected)
		{
			RawAnim back;
			DecodedAnim decoded;
			CHECK(read_raw_anim(zone, zone.find_asset(c1::ASSET_TYPE_XANIMPARTS, e.name), back) && decode_anim(back, decoded));
			CHECK(back.boneNames == e.boneNames && back.notifies == e.notifies && back.header.numframes == e.header.numframes && back.header.flags == e.header.flags);
			CHECK(back.dataByte == e.dataByte && back.dataShort == e.dataShort && back.dataInt == e.dataInt && back.randomDataShort == e.randomDataShort
				&& back.randomDataByte == e.randomDataByte && back.indices == e.indices && !back.indices.empty());
			CHECK(back.hasDelta == e.hasDelta && back.deltaTrans.indices == e.deltaTrans.indices && back.deltaTrans.frames == e.deltaTrans.frames
				&& back.deltaQuat2.indices == e.deltaQuat2.indices && back.deltaQuat2.frames == e.deltaQuat2.frames);
		}
	}
}
