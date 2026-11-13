#pragma once

// Sounds: sound alias definitions ( the mod tools' soundaliases .csv ) + .wav -> snd_alias_list_t, LoadedSound,
// SndCurve as Call of Duty Online 1.1.1.13 stores them.
//
// 1.1.1.13 has the Miles sound system assets of IW4 ( asset types 11 "sound", 12 "sndcurve", 13 "loaded_sound" );
// what was measured on the 12969 shipped alias lists / 27397 aliases / 4604 loaded sounds ( codol_asset_verify
// sound-survey ):
//
//   * snd_alias_list_t: name ( the alias name in lower case ), the variants, their count. One asset per alias name.
//   * snd_alias_t is 108 bytes ( IW4: 100 ): masterPriority and separate master / slave percentages follow the
//     flags. flags: bit 0 looping, 3 / 4 the two reverb bits, 5 set on a part of the looping aliases only
//     ( "rlooping" ), 6 on 31 distant weapon aliases, 7 - 8 the type ( 1 loaded, 2 streamed ), 9 - 14 the channel
//     ( index into soundaliases/channels.def ); 1, 2 and 15 .. 31 are never set.
//   * a loaded sound is the wave data without its file header: IMA ADPCM ( format 17: 3951 of the 4401 distinct
//     ones ) or 16 bit PCM ( 450 ), mono or stereo, 11025 .. 48000 Hz, named like its source file under sound/
//     ( "weapons/foley/x.wav" ). AILSOUNDINFO has one more field than IW4's, 0xFFFFFFFF in every shipped sound.
//   * a streamed sound is a directory and a file name; the file is sound/<directory>/<name> in an .iwd
//     ( 12140 .mp3, 165 .wav ).
//   * the volume falloff curve is a SndCurve asset, in most zones a reference by name ( ",weapon2" ); every alias
//     has one ( "$default": 0,1 1,0 ).
//   * the speaker map is not an asset: 408 bytes inside the alias list, shared between the aliases of a list.
//     Every alias has one; the default map has an empty name and isDefault 1.

#include "codol_zone/zone_editor.hpp"

#include <filesystem>
#include <map>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace codol::assets
{
	using zone::Status;

	// ---- wave files -------------------------------------------------------------------------------------
	inline constexpr int WAVE_PCM = 1;
	inline constexpr int WAVE_IMA_ADPCM = 17;

	struct WaveData
	{
		int format = WAVE_PCM;				// WAVE_PCM ( 16 bit after load ) or WAVE_IMA_ADPCM
		int channels = 1;
		std::uint32_t rate = 0;
		int bits = 16;						// 16 for PCM, 4 for IMA ADPCM
		std::uint32_t blockAlign = 2;
		std::vector<std::uint8_t> data;		// the "data" chunk
		std::vector<std::string> notes;		// conversions made while reading

		std::uint32_t frames() const;		// sample frames the data holds
	};

	// RIFF WAVE: PCM 8 / 16 / 24 bit ( 8 and 24 become 16 ), IMA ADPCM; mono or stereo
	Status parse_wav(std::span<const std::uint8_t> file, WaveData& out);
	Status load_wav(const std::filesystem::path& file, WaveData& out);
	std::vector<std::uint8_t> write_wav(const WaveData& wave);

	// 16 bit PCM <-> IMA ADPCM ( the 4 bit format of Microsoft's wave files ). blockAlign 0: 256 bytes per channel
	// and 11025 Hz, as the shipped mono sounds have it. The last block is filled by repeating the last frame.
	Status ima_adpcm_encode(const WaveData& pcm, std::uint32_t blockAlign, WaveData& out);
	Status ima_adpcm_decode(const WaveData& adpcm, WaveData& out);

	// ---- definitions -------------------------------------------------------------------------------------
	// one row of a soundaliases .csv
	struct SoundAliasDef
	{
		std::string name;
		std::int32_t sequence = 0;
		std::string file;					// relative to sound/, with extension; '/' or '\\'
		float volMin = 1.0f;
		float volMax = 1.0f;
		std::string volMod;					// stored as the alias' mixer group ( max, mission, weapons, ambience, music )
		float pitchMin = 1.0f;
		float pitchMax = 1.0f;
		float distMin = 120.0f;
		float distMax = 600.0f;
		float velocityMin = 0.0f;
		std::string channel = "auto";
		bool streamed = false;				// type: loaded / streamed
		float probability = 1.0f;
		bool looping = false;
		bool rlooping = false;				// "rlooping": looping + bit 5
		std::uint8_t masterPriority = 50;
		float masterPercentage = 0.0f;
		float slavePercentage = 1.0f;
		std::string subtitle;
		std::string compression;			// "adpcm": a PCM source is stored as IMA ADPCM
		std::string secondaryAliasName;
		std::string chainAliasName;
		std::string volumeFalloffCurve;		// empty: $default
		std::int32_t startDelay = 0;
		std::string speakerMap;				// empty: the default map
		bool fullDryLevel = false;			// reverb: bit 3
		bool noWetLevel = false;			// reverb: bit 4
		float lfePercentage = 0.0f;
		float centerPercentage = 0.0f;
		float envelopMin = 0.0f;
		float envelopMax = 0.0f;
		float envelopPercentage = 0.0f;
		std::uint32_t extraFlags = 0;		// flag bits without a column ( 1, 2, 6; 5 without looping )
		bool noFile = false;				// extracted alias without a SoundFile ( two shipped ones )
	};

	// Reads a soundaliases .csv. With a header row ( first field "name" ) the columns are found by name, without
	// one the column order of the CoD4 mod tools is assumed. '#' starts a comment line.
	Status parse_sound_aliases(std::string_view csv, std::vector<SoundAliasDef>& out, std::vector<std::string>* notes = nullptr);
	std::string write_sound_aliases(std::span<const SoundAliasDef> aliases);

	// soundaliases/channels.def of code_pre_gfx_mp: index <-> name; -1 / "" when unknown
	int sound_channel_index(std::string_view name);
	std::string_view sound_channel_name(int index);

	// volume falloff curve: "SNDCURVE", the knot count, x y per knot ( up to 16 )
	struct SndCurveDef
	{
		std::string name;
		std::vector<std::pair<float, float>> knots;
	};
	Status parse_snd_curve(std::string_view text, SndCurveDef& out);
	std::string write_snd_curve(const SndCurveDef& curve);

	// speaker map: for a mono and a stereo source, the level of every source channel in each of 2 and of 6
	// output speakers. Text: "SPKRMAP", then four blocks "map <mono|stereo> <2|6>" + one line per speaker:
	// <speaker> <level of source channel 0> [ <level of source channel 1> ].
	struct SpeakerMapDef
	{
		std::string name;
		bool isDefault = false;
		c1::MSSChannelMap channelMaps[2][2]{};
	};
	SpeakerMapDef default_speaker_map();
	Status parse_speaker_map(std::string_view text, SpeakerMapDef& out);
	std::string write_speaker_map(const SpeakerMapDef& map);

	// what the aliases name besides sound files
	struct SoundLibrary
	{
		std::map<std::string, SpeakerMapDef> speakerMaps;
		std::map<std::string, SndCurveDef> curves;		// curves with a source; others are referenced by name
	};

	// ---- compiled ------------------------------------------------------------------------------------------
	struct CompiledLoadedSound
	{
		std::string name;					// "dir/file.wav"
		c1::MssSound info{};				// pointers 0
		std::vector<std::uint8_t> data;
	};
	Status compile_loaded_sound(std::string_view name, const WaveData& wave, CompiledLoadedSound& out);
	WaveData loaded_sound_wave(const CompiledLoadedSound& sound);

	struct CompiledAlias
	{
		c1::snd_alias_t raw{};				// pointer fields 0
		std::string aliasName, subtitle, secondaryAliasName, chainAliasName, mixerGroup;
		bool hasFile = true;
		bool streamed = false;
		std::string loadedName;				// loaded: name of the LoadedSound
		std::string streamDir, streamName;	// streamed
		std::string curve;					// SndCurve name
		SpeakerMapDef speakerMap;
	};

	struct CompiledAliasList
	{
		std::string name;					// lower case
		std::vector<CompiledAlias> aliases;
	};

	// groups the rows by name ( in order of appearance ) and fills the alias structs
	Status compile_sound_aliases(std::span<const SoundAliasDef> defs, const SoundLibrary& library, std::vector<CompiledAliasList>& out,
		std::vector<std::string>* notes = nullptr);

	// "weapons/foley/x.wav" of a row's file ( '\\' -> '/', no leading "sound/" )
	std::string sound_file_name(std::string_view file);

	// ---- shipped assets -> definitions ---------------------------------------------------------------------
	bool extract_sound_alias_list(const zone::Zone& zone, zone::zaddr header, CompiledAliasList& out);
	std::vector<SoundAliasDef> alias_list_defs(const CompiledAliasList& list);
	// the wave data is not part of the loaded image ( temporary block ): the editor's record of the stream has it
	bool extract_loaded_sound(const zone::ZoneEditor& editor, zone::zaddr header, CompiledLoadedSound& out);
	bool extract_snd_curve(const zone::Zone& zone, zone::zaddr header, SndCurveDef& out);

	// ---- linking ---------------------------------------------------------------------------------------------
	// Top level assets appended to the zone. A loaded sound or curve the alias names that the zone does not have is
	// written as a reference by name ( ",name": an asset of another loaded zone ), as the shipped zones do.
	Status link_loaded_sound(zone::ZoneEditor& editor, const CompiledLoadedSound& sound);
	Status link_snd_curve(zone::ZoneEditor& editor, const SndCurveDef& curve);
	Status link_sound_alias_list(zone::ZoneEditor& editor, const CompiledAliasList& list, std::vector<std::string>* log = nullptr);

	// ---- a whole alias file ----------------------------------------------------------------------------------
	// The source tree is laid out like raw/ of the mod tools:
	//   <root>/soundaliases/<anything>.csv        alias definitions
	//   <root>/sound/<file>                       what the `file` column names ( .wav for loaded, .mp3 / .wav for streamed )
	//   <root>/soundaliases/<name>.vfcurve        a volume falloff curve of your own ( otherwise referenced by name )
	//   <root>/soundaliases/<name>.spkrmap        a speaker map ( needed for every map but the default )
	struct SoundBuild
	{
		std::uint32_t lists = 0, aliases = 0, loadedSounds = 0, curves = 0;
		std::vector<std::string> log;
		// streamed files that exist under <root>/sound, as "sound/<file>": they belong into an .iwd
		std::vector<std::pair<std::string, std::vector<std::uint8_t>>> streamedFiles;
	};

	// <root> for a csv: the folder above soundaliases/ when the file is in one, else the file's folder
	std::filesystem::path sound_source_root(const std::filesystem::path& csvFile);

	// csv -> loaded sounds, curves and alias lists appended to the zone of `editor`
	Status build_sound_aliases(const std::filesystem::path& csvFile, const std::filesystem::path& root, zone::ZoneEditor& editor, SoundBuild& out);
}
