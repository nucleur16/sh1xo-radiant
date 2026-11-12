#pragma once

// .efx ( the text format of the effects editor, "iwfx 2" ) -> native 1.1.1.13 FxEffectDef.
//
// Three representations:
//
//   EditorEffect   what an .efx file says: per element the spawn / origin / velocity settings as ranges, the
//                  velocity, rotation, size, scale, colour and alpha graphs as curves ( key time 0 .. 1, value ) with
//                  a scale, the visuals by name. parse_efx / write_efx.
//   CompiledFx     the asset as the zone stores it, pointers replaced by names: FxEffectDef header fields, per element
//                  the 260 byte FxElemDef, its velocity and visual state samples, visuals, trail / spark fountain
//                  definition. read_fx reads a shipped one; link_fx writes one into a zone.
//   compile_fx     EditorEffect -> CompiledFx: the curves sampled at ( interval count + 1 ) points, velocities
//                  integrated, flags and element order derived. extract_fx is its inverse: CompiledFx -> EditorEffect
//                  with one curve key per stored sample.
//
// The rules of compile_fx were fitted to the shipped effects ( codol_asset_verify fx-roundtrip: every distinct
// shipped FxEffectDef -> extract_fx -> text -> parse_efx -> compile_fx -> compared field by field ) and to the
// shipped effects that still are what an .efx of the CoD4 mod tools says ( fx-source ).

#include "codol_assets/link.hpp"
#include "codol_zone/assets.hpp"

#include <functional>

namespace codol::assets
{
	// ---- the stored asset ---------------------------------------------------------------------------------------
	struct FxElem
	{
		c1::FxElemDef def{};								// pointer fields: 0 = null, anything else = present
		std::vector<c1::FxElemVelStateSample> velSamples;	// velIntervalCount + 1, or none
		std::vector<c1::FxElemVisStateSample> visSamples;	// visStateIntervalCount + 1, or none
		// material / model / sound alias / effect names; a decal has two materials per visual. Lights have none.
		std::vector<std::string> visuals;
		std::string effectOnImpact, effectOnDeath, effectEmitted;
		bool hasExtended = false;							// def.extended != 0
		c1::FxTrailDef trail{};								// elemType trail
		std::vector<c1::FxTrailVertex> trailVerts;
		std::vector<std::uint16_t> trailInds;
		c1::FxSparkFountainDef spark{};						// elemType spark fountain
		std::uint8_t extendedByte = 0;						// every other type: one byte
		bool hasString256 = false;
		std::string string256;
	};

	struct CompiledFx
	{
		std::string name;
		std::int32_t flags = 0;
		std::int32_t totalSize = 0;
		std::int32_t msecLoopingLife = 0;
		std::int32_t countLooping = 0, countOneShot = 0, countEmission = 0;
		std::vector<FxElem> elems;							// looping, one shot, emission
		std::vector<std::string> notes;
	};

	// a shipped effect ( header: pool address of an FxEffectDef that has its data in the zone )
	bool read_fx(const zone::Zone& zone, zone::zaddr header, CompiledFx& out);

	// what an effect with these elements occupies when the game copies it into one allocation ( FxEffectDef::totalSize )
	std::int32_t fx_total_size(const CompiledFx& fx);

	// ---- the editor's effect ------------------------------------------------------------------------------------
	struct FxCurve
	{
		int dimensions = 1;									// values per key ( colour: 3 )
		std::vector<float> keys;							// ( 1 + dimensions ) per key: time, values
		std::size_t key_count() const { return keys.size() / static_cast<std::size_t>(1 + dimensions); }
	};

	struct FxGraph
	{
		float scale = 1.0f;
		FxCurve curve[2];									// the second one is used with the "useRand.." editor flags
	};

	struct EditorTrailVertex
	{
		float pos[2] = {};
		float texCoord = 0;
	};

	struct EditorElem
	{
		std::string name;
		std::vector<std::string> editorFlags;
		std::vector<std::string> flags;
		float spawnRange[2] = {}, fadeInRange[2] = {}, fadeOutRange[2] = {};
		float spawnFrustumCullRadius = 0;
		std::int32_t spawnLooping[2] = { 200, 0 };			// interval msec, count
		std::int32_t spawnOneShot[2] = { 1, 0 };			// count base, amplitude
		std::int32_t spawnDelayMsec[2] = {}, lifeSpanMsec[2] = { 1000, 0 };
		float spawnOrigin[3][2] = {};
		float spawnOffsetRadius[2] = {}, spawnOffsetHeight[2] = {};
		float spawnAngles[3][2] = {};						// pitch yaw roll, degrees
		float angularVelocity[3][2] = {};					// degrees per second
		float initialRotation[2] = {};						// degrees
		float gravity[2] = {};								// percent
		float elasticity[2] = {};
		std::string atlasBehavior = "startFixed";			// startFixed | startRandom | startIndexed, + playOverLife, loopOnlyNTimes
		std::int32_t atlasIndex = 0, atlasFps = 0, atlasLoopCount = 0, atlasColIndexBits = 0, atlasRowIndexBits = 0, atlasEntryCount = 0;
		FxGraph velGraph[2][3];								// [ local, world ][ x y z ]
		FxGraph rotGraph, sizeGraph[2], scaleGraph, colorGraph, alphaGraph;
		float lightingFrac = 0;
		float collOffset[3] = {};
		float collRadius = 0;
		std::string fxOnImpact, fxOnDeath, emission;
		std::int32_t sortOrder = 0;
		bool hasSortOrder = false;
		float emitDist[2] = {}, emitDistVariance[2] = {};
		std::int32_t trailRepeatTime = 0;
		float trailSplitDist = 0, trailSplitArcDist = 0, trailSplitTime = 0;
		float trailScrollTime = 0;							// seconds
		std::int32_t trailRepeatDist = 0;
		std::vector<EditorTrailVertex> trailVerts;
		std::vector<std::uint16_t> trailInds;
		// billboardSprite | orientedSprite | rotatedSprite | tail | line | trail | cloud | sparkCloud | sparkFountain |
		// model | light | spotLight | sound | decal | runner; empty: an element without visuals
		std::string visualType;
		std::vector<std::string> visuals;
		// what the format has no word for, carried as "//#CODOL <key> <values>" comment lines inside the element
		std::vector<std::pair<std::string, std::string>> extra;

		bool has_editor_flag(std::string_view flag) const;
		bool has_flag(std::string_view flag) const;
		const std::string* find_extra(std::string_view key) const;
	};

	struct EditorEffect
	{
		int version = 2;
		std::vector<EditorElem> elems;
	};

	zone::Status parse_efx(std::string_view text, EditorEffect& out);
	zone::Status load_efx(const std::filesystem::path& file, EditorEffect& out);
	std::string write_efx(const EditorEffect& effect);

	// ---- conversion ---------------------------------------------------------------------------------------------
	struct FxCompileOptions
	{
		// Source of another effect, for the elements an "emission" effect brings into this one ( the emission section
		// of the FxEffectDef holds the elements of the effects its elements emit ). Not set, or false: the effect is
		// referenced by name only and a note says so.
		std::function<bool(std::string_view name, EditorEffect& effect)> loadEffect;
		// Columns and rows of a material's texture atlas ( the element's atlas has them as bit counts ). Not set, or
		// false: atlasColIndexBits / atlasRowIndexBits of the file are taken.
		std::function<bool(std::string_view material, int& columns, int& rows)> materialAtlas;
	};

	zone::Status compile_fx(std::string_view name, const EditorEffect& effect, CompiledFx& out, const FxCompileOptions& options = {});
	// the definition that compiles to `fx`: one curve key per stored sample
	// intervalHints: also write "//#CODOL intervals <velocity> <visual>", which makes compile_fx take the stored
	// interval counts instead of deciding them from the curves
	zone::Status extract_fx(const CompiledFx& fx, EditorEffect& out, bool intervalHints = false);

	// ---- texture atlases of the shipped materials -------------------------------------------------------------------
	// An element that draws an atlas material stores the atlas' column and row count as bit counts; the effects
	// editor takes them from the material. For the materials of the game's zones this table has them: one scan of
	// the install ( every fast file, about half a minute ), kept as a small text file.
	class FxAtlasLibrary
	{
	public:
		zone::Status scan(const std::filesystem::path& install, const zone::ClientKey& key);
		zone::Status save(const std::filesystem::path& file) const;
		zone::Status load(const std::filesystem::path& file);
		// load( cache ) when it exists and was made from this install, else scan + save
		zone::Status open(const std::filesystem::path& install, const zone::ClientKey& key, const std::filesystem::path& cache);

		void set(std::string_view material, int columns, int rows);
		bool find(std::string_view material, int& columns, int& rows) const;
		std::size_t size() const { return atlas_.size(); }

	private:
		std::map<std::string, std::pair<int, int>, std::less<>> atlas_;		// only materials with more than one cell
		std::string install_;
	};

	// ---- linking ------------------------------------------------------------------------------------------------
	// Appends the effect to a zone ( AssetLinker::editor(), or any ZoneEditor ). Materials and models that are top level assets of the zone are
	// pointed at; the others are referenced by name ( a header that holds ",name" ), as shipped effects refer to the
	// materials of the common zones. Other effects ( runner, on impact / death, emission ) and sound aliases are
	// names in the stream in any case. notes: what was referenced by name.
	zone::Status link_fx(zone::ZoneEditor& editor, const CompiledFx& fx, std::vector<std::string>* notes = nullptr);

	// The options a tool compiles with: the atlas of a material from the zone being built ( materials made in the
	// same run ) or the table of the shipped ones, emitted effects from <fxRoot>/<name>.efx. Both objects must
	// outlive the options.
	FxCompileOptions fx_compile_options(const zone::ZoneEditor& editor, const FxAtlasLibrary& atlas, const std::filesystem::path& fxRoot);
	// the folder effect names are relative to: for "misc/my_effect" and "raw/fx/misc/my_effect.efx" it is "raw/fx";
	// the file's own folder when the path does not end in the name
	std::filesystem::path fx_source_root(const std::filesystem::path& file, std::string_view name);
}
