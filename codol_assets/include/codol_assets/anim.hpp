#pragma once

// xanim_export -> native 1.1.1.13 XAnimParts.
//
// Source: the text format the CoD exporters write ( ANIMATION / VERSION 3: a list of parts, and per frame and part
// the offset and the three axes of the bone in model space; note tracks ). The bone hierarchy is not part of that
// format: the converter of the mod tools takes it from the model the animation is for, and so does this one
// ( AnimOptions::skeleton, an xmodel_export ).
//
// What the game stores ( XAnimParts, the IW4 layout; every rule below measured on the shipped animations, see
// README "Animations" ):
//   * per bone a rotation relative to the parent and a translation relative to the parent. The bone list is
//     sorted by how the rotation is stored: none ( identity ), animated about z only ( two s16: z, w ), animated
//     ( four s16 ), constant about z, constant. Translations name their bone: animated with 8 bit or 16 bit steps
//     inside a box, constant, none ( the model's own bind translation ).
//   * animated tracks have key frames: a table of frame numbers ( u8 when the animation has fewer than 256 frames,
//     else u16; long tables live in `indices` ) and one value per key. Values between keys are interpolated.
//   * delta animations ( the movement of the whole character: walk cycles, mantles, deaths ) store the movement of
//     the root in a separate part: a translation track and a rotation about z ( or a full rotation: "delta 3d" ).
//   * note tracks: a script string and a time 0 .. 1.

#include "codol_assets/model.hpp"

#include <array>

namespace codol::assets
{
	// ---- xanim_export -----------------------------------------------------------------------------------
	struct ExportPose
	{
		float offset[3] = {};
		float axis[3][3] = { { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 } };	// rows X Y Z
	};

	struct ExportNote
	{
		std::string name;
		float frame = 0;				// frame number as in the file ( same numbering as the FRAME lines )
	};

	struct ExportAnim
	{
		std::vector<std::string> parts;
		float framerate = 30.0f;
		int firstFrame = 0;				// number of the first FRAME line
		std::vector<std::vector<ExportPose>> frames;	// [frame][part]
		std::vector<ExportNote> notes;
		// Not part of the classic format; written and read as comment lines "//#.." ( see write_xanim_export ):
		bool local = false;				// "//#LOCAL": poses are relative to the parent bone, not in model space
		std::vector<int> noTranslation;	// "//#NOTRANS <part>": the part has no translation track ( the model's own is used )
	};

	Status parse_xanim_export(std::string_view text, ExportAnim& out);
	Status load_xanim_export(const std::filesystem::path& file, ExportAnim& out);
	std::string write_xanim_export(const ExportAnim& anim, std::string_view comment = {});

	// ---- the stored form --------------------------------------------------------------------------------
	enum : std::uint8_t
	{
		ANIM_ROT_NONE = 0,			// boneCount[] index: identity
		ANIM_ROT_2D = 1,			// key frames, z and w
		ANIM_ROT_3D = 2,			// key frames, x y z w
		ANIM_ROT_2D_STATIC = 3,
		ANIM_ROT_3D_STATIC = 4,
		ANIM_TRANS_SMALL = 5,		// key frames, u8 steps
		ANIM_TRANS_LARGE = 6,		// key frames, u16 steps
		ANIM_TRANS_STATIC = 7,
		ANIM_TRANS_NONE = 8,
		ANIM_PART_TOTAL = 9,

		ANIM_FLAG_LOOP = 1,
		ANIM_FLAG_DELTA = 2,
		ANIM_FLAG_DELTA_3D = 4,
	};

	struct RawDeltaTrans
	{
		bool present = false;
		std::uint16_t size = 0;				// keys - 1; 0: one constant value ( frame0 )
		bool small_box = false;
		float frame0[3] = {};
		float mins[3] = {};
		float frameSize[3] = {};
		std::vector<std::uint16_t> indices;	// size + 1 frame numbers
		std::vector<std::uint16_t> frames;	// 3 per key ( u8 values when small )
	};

	struct RawDeltaQuat
	{
		bool present = false;
		std::uint16_t size = 0;
		std::vector<std::uint16_t> indices;
		std::vector<std::int16_t> frames;	// per key: 2 ( z w ) for the rotation about z, 4 for the full one
	};

	// An XAnimParts with everything it points at; what the zone holds, in a form that does not depend on addresses.
	struct RawAnim
	{
		std::string name;
		c1::XAnimParts header{};			// counts, flags, rates; pointer fields are not meaningful
		std::vector<std::string> boneNames;
		std::vector<std::pair<std::string, float>> notifies;
		std::vector<std::uint8_t> dataByte;
		std::vector<std::int16_t> dataShort;
		std::vector<std::uint32_t> dataInt;			// floats, as bits
		std::vector<std::int16_t> randomDataShort;
		std::vector<std::uint8_t> randomDataByte;
		std::vector<std::uint32_t> randomDataInt;
		std::vector<std::uint16_t> indices;
		bool hasDelta = false;
		RawDeltaTrans deltaTrans;
		RawDeltaQuat deltaQuat2;			// rotation about z
		RawDeltaQuat deltaQuat;				// full rotation
	};

	// reads a shipped ( or compiled ) animation out of a loaded zone
	bool read_raw_anim(const zone::Zone& zone, zone::zaddr anim, RawAnim& out, std::string* error = nullptr);

	// ---- decoded: key tracks ----------------------------------------------------------------------------
	using Quat = std::array<float, 4>;		// x y z w
	using Vec3 = std::array<float, 3>;

	struct QuatTrack
	{
		std::vector<std::uint16_t> frames;	// ascending; one entry ( 0 ) for a constant
		std::vector<Quat> values;
		Quat at(float frame) const;			// normalised linear interpolation between keys, as the game does it
	};

	struct VecTrack
	{
		std::vector<std::uint16_t> frames;
		std::vector<Vec3> values;
		float step = 0;						// largest stored step of the track's box ( 0 for a constant )
		Vec3 at(float frame) const;
	};

	struct DecodedBone
	{
		std::string name;
		std::uint8_t rotation = ANIM_ROT_NONE;
		std::uint8_t translation = ANIM_TRANS_NONE;
		QuatTrack rot;						// empty for ANIM_ROT_NONE
		VecTrack trans;						// empty for ANIM_TRANS_NONE
	};

	struct DecodedAnim
	{
		std::string name;
		std::uint32_t lastFrame = 0;		// XAnimParts::numframes
		float framerate = 30.0f;
		float frequency = 0;
		std::uint8_t flags = 0;
		std::uint8_t assetType = 0;
		std::vector<DecodedBone> bones;
		std::vector<std::pair<std::string, float>> notifies;
		bool hasDelta = false;
		bool delta3D = false;
		VecTrack deltaTrans;				// empty: none
		QuatTrack deltaRot;					// empty: none
	};

	Status decode_anim(const RawAnim& raw, DecodedAnim& out);

	// ---- compiling --------------------------------------------------------------------------------------
	enum class AnimType
	{
		relative,		// plain: every part is animated relative to its parent
		delta,			// the movement of the root part ( tag_origin ) goes to the delta part: translation and yaw
		delta3d,		// .. with the full rotation
	};

	struct AnimOptions
	{
		std::string name;
		AnimType type = AnimType::relative;
		bool looping = false;
		// the model the animation is for: gives the parent of every part and the bind translations ( a part whose
		// translation never leaves the bind translation gets no translation track ). Without one every part is taken
		// as a root: right for the animation of a one bone script model, and for files written with "//#LOCAL".
		const ExportModel* skeleton = nullptr;
		std::string deltaPart = "tag_origin";	// the part whose movement a delta animation takes out
		// XAnimParts::assetType. Shipped: 1 for everything named viewmodel_* / pt_*, 2 for the world ( characters,
		// vehicles, props ), 6 for the additive ones ( *_aim_2, pain_add_* ), 0 for 32 others.
		std::uint8_t assetType = 2;
		// Frames that an interpolation between their neighbours reproduces within these limits are not stored. 0: only
		// what the stored steps reproduce anyway. The shipped animations keep 36 % of the frames of a rotation track;
		// their interior keys are rarely nearer than 16 units / 0.016 to what their neighbours interpolate to.
		float rotationTolerance = 16.0f;		// in units of the stored s16 ( 1 / 32767 of a quaternion component )
		float translationTolerance = 0.016f;	// in game units
		// a translation that stays within this of its first frame is constant; within this of the skeleton's bind
		// translation it gets no track
		float staticTranslationEpsilon = 0.0001f;
	};

	struct CompiledAnim
	{
		RawAnim raw;
		std::vector<std::string> notes;
	};

	Status compile_anim(const ExportAnim& source, const AnimOptions& options, CompiledAnim& out);

	// A decoded animation as an export. Without a skeleton the poses are parent relative and the file says so
	// ( "//#LOCAL", "//#NOTRANS" ): the stored form put back into text. With one ( bones by name ) the poses are in
	// model space as an exporter writes them; parts the model lacks are roots.
	void extract_anim(const DecodedAnim& anim, const ExportModel* skeleton, ExportAnim& out);

	// ---- math shared with the tools ---------------------------------------------------------------------
	Quat quat_from_axis(const float axis[3][3]);
	void axis_from_quat(const Quat& q, float axis[3][3]);
	float quat_angle_between(const Quat& a, const Quat& b);		// radians, 0 .. pi
}
