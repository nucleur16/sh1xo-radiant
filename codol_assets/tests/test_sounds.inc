// Sound tests; included by codol_assets_tests.cpp ( uses its CHECK ). test_sounds needs nothing but the library -
// the zone it builds goes through the game's loaders, which are part of codol_zone; test_sounds_install compares
// with shipped assets.

namespace
{
	WaveData test_tone(const int channels, const std::uint32_t rate, const std::uint32_t frames)
	{
		WaveData w;
		w.channels = channels;
		w.rate = rate;
		w.blockAlign = static_cast<std::uint32_t>(2 * channels);
		for (std::uint32_t n = 0; n < frames; n++)
		{
			for (int c = 0; c < channels; c++)
			{
				const auto v = static_cast<std::int16_t>(9000.0 * std::sin(n * (0.05 + 0.02 * c)) + 4000.0 * std::sin(n * 0.31));
				w.data.push_back(static_cast<std::uint8_t>(v));
				w.data.push_back(static_cast<std::uint8_t>(v >> 8));
			}
		}
		return w;
	}

	double tone_snr(const WaveData& a, const WaveData& b, const std::size_t samples)
	{
		double signal = 0, noise = 0;
		for (std::size_t k = 0; k < samples; k++)
		{
			const double x = static_cast<std::int16_t>(a.data[k * 2] | (a.data[k * 2 + 1] << 8));
			const double y = static_cast<std::int16_t>(b.data[k * 2] | (b.data[k * 2 + 1] << 8));
			signal += x * x;
			noise += (x - y) * (x - y);
		}
		return 10.0 * std::log10(signal / std::max(noise, 1e-9));
	}

	void put_file(const fs::path& path, const std::string& text)
	{
		std::error_code ec;
		fs::create_directories(path.parent_path(), ec);
		z::write_file(path, { reinterpret_cast<const std::uint8_t*>(text.data()), text.size() });
	}

	void test_sound_files()
	{
		// ---- wave files
		const WaveData tone = test_tone(2, 44100, 5000);
		WaveData back;
		CHECK(parse_wav(write_wav(tone), back));
		CHECK(back.format == WAVE_PCM && back.channels == 2 && back.rate == 44100 && back.bits == 16 && back.blockAlign == 4 && back.data == tone.data && back.frames() == 5000);

		// 8 bit: becomes 16 bit
		std::vector<std::uint8_t> eight = { 'R', 'I', 'F', 'F', 0, 0, 0, 0, 'W', 'A', 'V', 'E', 'f', 'm', 't', ' ', 16, 0, 0, 0, 1, 0, 1, 0, 0x11, 0x2B, 0, 0, 0x11, 0x2B, 0, 0, 1, 0, 8, 0,
			'd', 'a', 't', 'a', 3, 0, 0, 0, 128, 255, 0 };
		CHECK(parse_wav(eight, back));
		CHECK(back.bits == 16 && back.rate == 11025 && back.data.size() == 6 && back.data[0] == 0 && back.data[1] == 0 && back.data[3] == 0x7F && back.data[5] == 0x80
			&& back.notes.size() == 1);
		eight[22] = 3;		// three channels
		CHECK(!parse_wav(eight, back));
		CHECK(!parse_wav(std::vector<std::uint8_t>(64, 0), back));

		// IMA ADPCM: block sizes as shipped, the coder's error, the file form
		WaveData coded, decoded;
		CHECK(ima_adpcm_encode(test_tone(1, 44100, 10000), 0, coded));
		CHECK(coded.format == WAVE_IMA_ADPCM && coded.blockAlign == 1024 && coded.bits == 4 && coded.data.size() == 5 * 1024 && coded.frames() == 5 * 2041);
		CHECK(ima_adpcm_decode(coded, decoded) && decoded.frames() == 5 * 2041);
		CHECK(tone_snr(test_tone(1, 44100, 10000), decoded, 10000) > 30.0);
		CHECK(ima_adpcm_encode(test_tone(1, 22050, 100), 0, coded) && coded.blockAlign == 512);
		CHECK(ima_adpcm_encode(test_tone(1, 32000, 100), 0, coded) && coded.blockAlign == 768);
		CHECK(ima_adpcm_encode(tone, 0, coded) && coded.blockAlign == 2048 && coded.frames() == 3 * 2041);
		CHECK(ima_adpcm_decode(coded, decoded) && tone_snr(tone, decoded, 10000) > 30.0);
		CHECK(parse_wav(write_wav(coded), back) && back.format == WAVE_IMA_ADPCM && back.blockAlign == 2048 && back.data == coded.data && back.channels == 2);
		CHECK(!ima_adpcm_encode(coded, 0, decoded));		// not PCM

		// ---- loaded sound: the fields as the shipped sounds have them
		CompiledLoadedSound sound;
		CHECK(compile_loaded_sound("sound\\Test/tone.wav", tone, sound));
		CHECK(sound.name == "Test/tone.wav" && sound.info.format == 1 && sound.info.bits == 16 && sound.info.channels == 2 && sound.info.rate == 44100
			&& sound.info.samples == 10000 && sound.info.blockSize == 4 && sound.info.dataLen == 20000 && sound.info.channelMask == 0xFFFFFFFFu);
		CHECK(compile_loaded_sound("a.wav", coded, sound) && sound.info.format == 17 && sound.info.bits == 4 && sound.info.samples == 3 * 2041 && sound.info.blockSize == 2048);
		CHECK(!compile_loaded_sound("a.wav", WaveData{}, sound));

		// ---- alias csv
		std::vector<SoundAliasDef> defs;
		std::vector<std::string> notes;
		const std::string csv =
			"# comment\n"
			"name,sequence,file,vol_min,vol_max,vol_mod,pitch_min,pitch_max,dist_min,dist_max,channel,type,probability,loop,masterslave,loadspec,subtitle,compression,"
			"secondaryaliasname,volumefalloffcurve,startdelay,speakermap,reverb,lfe percentage,center percentage,platform,envelop_min,envelop_max,envelop percentage,"
			"masterpriority,masterpercentage,flags,nonsense\r\n"
			"\r\n"
			"Test_Shot,,weapons\\test\\shot1.wav,0.8,0.9,weapons,0.95,1.05,35,2000,weapon,loaded,0.5,,0.7,all_mp,\"a, \"\"quoted\"\" subtitle\",adpcm,test_far,weapon2,5,,nowetlevel,0.3,0.1,,,,,75,0.25,0x40,x\n"
			"test_shot,,weapons/test/shot2.wav,0.8,,,,,35,2000,weapon\n"
			"test_music,,music/test.mp3,0.6,,music,,,,,music,streamed,,rlooping,,,,,,,,front,fulldrylevel nowetlevel\n"
			",,ignored.wav\n";
		CHECK(parse_sound_aliases(csv, defs, &notes));
		CHECK(defs.size() == 3 && notes.size() == 1);		// the unknown column, once per row that fills it
		if (defs.size() == 3)
		{
			const SoundAliasDef& d = defs[0];
			CHECK(d.name == "Test_Shot" && d.file == "weapons/test/shot1.wav" && d.volMin == 0.8f && d.volMax == 0.9f && d.volMod == "weapons" && d.pitchMin == 0.95f
				&& d.pitchMax == 1.05f && d.distMin == 35 && d.distMax == 2000 && d.channel == "weapon" && !d.streamed && d.probability == 0.5f && !d.looping);
			CHECK(d.slavePercentage == 0.7f && d.subtitle == "a, \"quoted\" subtitle" && d.compression == "adpcm" && d.secondaryAliasName == "test_far"
				&& d.volumeFalloffCurve == "weapon2" && d.startDelay == 5 && d.noWetLevel && !d.fullDryLevel && d.lfePercentage == 0.3f && d.centerPercentage == 0.1f
				&& d.masterPriority == 75 && d.masterPercentage == 0.25f && d.extraFlags == 0x40);
			CHECK(defs[1].volMax == 0.8f && defs[1].pitchMax == 1.0f && defs[1].masterPriority == 50 && defs[1].slavePercentage == 1.0f && defs[1].distMax == 2000);
			CHECK(defs[2].streamed && defs[2].looping && defs[2].rlooping && defs[2].fullDryLevel && defs[2].noWetLevel && defs[2].speakerMap == "front"
				&& defs[2].distMin == 120 && defs[2].distMax == 600);
		}
		// text -> rows -> text -> rows
		std::vector<SoundAliasDef> again;
		CHECK(parse_sound_aliases(write_sound_aliases(defs), again) && again.size() == defs.size() && write_sound_aliases(again) == write_sound_aliases(defs));
		// without a header row: the CoD4 column order; the type follows from the extension
		std::vector<SoundAliasDef> cod4;
		notes.clear();
		CHECK(parse_sound_aliases("plain,1,misc/a.wav,0.5,0.6,,1,1.1,10,20,voice,,,looping\nstream,,music/b.mp3,1\n", cod4, &notes) && cod4.size() == 2 && notes.size() == 1);
		if (cod4.size() == 2)
		{
			CHECK(cod4[0].sequence == 1 && cod4[0].channel == "voice" && cod4[0].distMax == 20 && cod4[0].looping && !cod4[0].streamed && cod4[1].streamed);
		}
		std::vector<SoundAliasDef> bad;
		CHECK(!parse_sound_aliases("name,file,channel\nx,a.wav,nochannel\n", bad));
		CHECK(!parse_sound_aliases("name,file,vol_min\nx,a.wav,loud\n", bad));
		CHECK(sound_channel_index("physics") == 0 && sound_channel_index("AUTO") == 3 && sound_channel_index("music") == 32 && sound_channel_index("shellshock") == 36
			&& sound_channel_index("x") == -1 && sound_channel_name(24) == "voice" && sound_channel_name(37).empty());

		// ---- curve and speaker map text
		SndCurveDef curve;
		CHECK(parse_snd_curve("SNDCURVE\n\n3\n0.0000 1.0000\n0.5 0.25\n1 0\n", curve) && curve.knots.size() == 3 && curve.knots[1].second == 0.25f);
		SndCurveDef curve2;
		CHECK(parse_snd_curve(write_snd_curve(curve), curve2) && curve2.knots == curve.knots);
		CHECK(parse_snd_curve("0 1  1 0", curve2) && curve2.knots.size() == 2);		// no head, no count
		CHECK(!parse_snd_curve("SNDCURVE\n1\n0 1\n", curve2) && !parse_snd_curve("SNDCURVE 2 0 1 x 0", curve2));

		const SpeakerMapDef standard = default_speaker_map();
		CHECK(standard.isDefault && standard.name.empty() && standard.channelMaps[0][0].speakerCount == 2 && standard.channelMaps[0][1].speakerCount == 6
			&& standard.channelMaps[0][1].speakers[1].levels[0] == 0.5f && standard.channelMaps[0][1].speakers[2].levels[0] == 0.0f
			&& standard.channelMaps[0][1].speakers[5].speaker == 5 && standard.channelMaps[0][1].speakers[5].numLevels == 1
			&& standard.channelMaps[1][0].speakers[0].levels[0] == 1.0f && standard.channelMaps[1][0].speakers[1].levels[1] == 1.0f
			&& standard.channelMaps[1][1].speakers[1].levels[0] == 0.0f && standard.channelMaps[1][1].speakers[3].numLevels == 2);
		SpeakerMapDef map;
		CHECK(parse_speaker_map(write_speaker_map(standard), map) && map.isDefault && std::memcmp(map.channelMaps, standard.channelMaps, sizeof map.channelMaps) == 0);
		CHECK(!parse_speaker_map("SPKRMAP\nmap mono 2\n0 0.5\n1 0.5\n", map));		// three blocks missing
		CHECK(!parse_speaker_map("SPKRMAP\nmap mono 3\n", map));

		// ---- compile
		SoundLibrary library;
		std::vector<CompiledAliasList> lists;
		CHECK(!compile_sound_aliases(defs, library, lists));		// speaker map "front" has no definition
		SpeakerMapDef front = standard;
		front.isDefault = false;
		front.channelMaps[0][1].speakers[2].levels[0] = 0.2f;
		library.speakerMaps["front"] = front;
		lists.clear();
		CHECK(compile_sound_aliases(defs, library, lists) && lists.size() == 2);
		if (lists.size() == 2)
		{
			CHECK(lists[0].name == "test_shot" && lists[0].aliases.size() == 2 && lists[0].aliases[0].aliasName == "Test_Shot" && lists[0].aliases[1].loadedName == "weapons/test/shot2.wav");
			const CompiledAlias& a = lists[0].aliases[0];
			// no wet level, bit 6, loaded, channel 21
			CHECK(a.raw.flags == (0x10u | 0x40u | (1u << 7) | (21u << 9)) && a.raw.masterPriority == 75 && a.raw.slavePercentage == 0.7f && a.raw.startDelay == 5
				&& a.curve == "weapon2" && a.speakerMap.isDefault && a.mixerGroup == "weapons");
			const CompiledAlias& m = lists[1].aliases[0];
			// looping, both reverb bits, bit 5, streamed, channel 32
			CHECK(m.raw.flags == (0x1u | 0x8u | 0x10u | 0x20u | (2u << 7) | (32u << 9)) && m.streamed && m.streamDir == "music" && m.streamName == "test.mp3" && m.curve == "$default"
				&& m.speakerMap.name == "front" && !m.speakerMap.isDefault);
			// and back to rows
			const std::vector<SoundAliasDef> rows = alias_list_defs(lists[1]);
			CHECK(rows.size() == 1 && rows[0].rlooping && rows[0].streamed && rows[0].file == "music/test.mp3" && rows[0].channel == "music" && rows[0].extraFlags == 0
				&& rows[0].volumeFalloffCurve.empty() && rows[0].speakerMap == "front");
		}
	}

	bool same_compiled(const CompiledAliasList& a, const CompiledAliasList& b)
	{
		bool same = a.name == b.name && a.aliases.size() == b.aliases.size();
		for (std::size_t k = 0; same && k < a.aliases.size(); k++)
		{
			const CompiledAlias& x = a.aliases[k];
			const CompiledAlias& y = b.aliases[k];
			same = std::memcmp(&x.raw, &y.raw, sizeof x.raw) == 0 && x.aliasName == y.aliasName && x.subtitle == y.subtitle && x.secondaryAliasName == y.secondaryAliasName
				&& x.chainAliasName == y.chainAliasName && x.mixerGroup == y.mixerGroup && x.hasFile == y.hasFile && x.streamed == y.streamed && x.loadedName == y.loadedName
				&& x.streamDir == y.streamDir && x.streamName == y.streamName && x.curve == y.curve && x.speakerMap.name == y.speakerMap.name
				&& x.speakerMap.isDefault == y.speakerMap.isDefault && std::memcmp(x.speakerMap.channelMaps, y.speakerMap.channelMaps, sizeof x.speakerMap.channelMaps) == 0;
		}
		return same;
	}

	// a source tree -> zone -> the game's loaders -> what they loaded
	void test_sound_zone(const fs::path& scratch)
	{
		const fs::path root = scratch / "codol_assets_sound_test";
		std::error_code ec;
		fs::remove_all(root, ec);
		const WaveData tone = test_tone(1, 22050, 3000);
		const std::vector<std::uint8_t> toneFile = write_wav(tone), stereoFile = write_wav(test_tone(2, 44100, 9000));
		fs::create_directories(root / "sound" / "unit", ec);
		z::write_file(root / "sound" / "unit" / "tone.wav", toneFile);
		z::write_file(root / "sound" / "unit" / "stereo.wav", stereoFile);
		z::write_file(root / "sound" / "unit" / "stream.wav", stereoFile);
		put_file(root / "soundaliases" / "unit_curve.vfcurve", "SNDCURVE\n\n3\n0 1\n0.3 0.4\n1 0\n");
		put_file(root / "soundaliases" / "unit_map.spkrmap", write_speaker_map(default_speaker_map()).substr(std::string("SPKRMAP\ndefault\n").size()));
		put_file(root / "soundaliases" / "unit.csv",
			"name,file,vol_min,channel,type,loop,compression,volumefalloffcurve,speakermap,subtitle,secondaryaliasname\n"
			"Unit_Tone,unit/tone.wav,0.9,auto,loaded,,,unit_curve,,SUBTITLE_REF,unit_stereo\n"
			"unit_tone,unit/stereo.wav,0.8,auto,loaded,,adpcm,unit_curve,unit_map\n"
			"unit_stereo,unit\\stereo.wav,1,ambient,loaded,looping,adpcm,,unit_map\n"
			"unit_stream,unit/stream.wav,1,music,streamed,looping\n"
			"unit_shipped,weapons/not_here.wav,1,weapon\n"
			"unit_silent,,1,menu\n");
		CHECK(sound_source_root(root / "soundaliases" / "unit.csv") == root && sound_source_root(root / "unit.csv") == root);

		z::ZoneEditor editor;
		SoundBuild build;
		CHECK(editor.create("codol_sound_test"));
		const z::Status built = build_sound_aliases(root / "soundaliases" / "unit.csv", root, editor, build);
		CHECK(built);
		if (!built)
		{
			std::printf("  %s\n", built.message.c_str());
		}
		CHECK(build.lists == 5 && build.aliases == 6 && build.loadedSounds == 2 && build.curves == 1 && build.streamedFiles.size() == 1);
		if (build.streamedFiles.size() == 1)
		{
			CHECK(build.streamedFiles[0].first == "sound/unit/stream.wav" && build.streamedFiles[0].second == stereoFile);
		}
		// a second time: the zone has them already
		SoundBuild twice;
		CHECK(!build_sound_aliases(root / "soundaliases" / "unit.csv", root, editor, twice));
		// errors name what is missing
		put_file(root / "soundaliases" / "bad.csv", "name,file,speakermap\nunit_bad,unit/tone.wav,no_such_map\n");
		const z::Status missing = build_sound_aliases(root / "soundaliases" / "bad.csv", root, editor, twice);
		CHECK(!missing && missing.message.find("no_such_map.spkrmap") != std::string::npos);

		std::vector<std::uint8_t> bytes, again;
		const z::Status written = editor.write(bytes);
		CHECK(written);
		if (!written)
		{
			std::printf("  %s\n", written.message.c_str());
			return;
		}
		z::ZoneEditor loaded;
		CHECK(loaded.load(bytes, "codol_sound_test") && loaded.write(again) && again == bytes);
		const z::Zone& zone = loaded.zone();
		CHECK(zone.warnings().empty() && zone.assets().size() == 8);		// 2 loaded sounds, 1 curve, 5 lists

		CompiledLoadedSound sound;
		CHECK(extract_loaded_sound(loaded, zone.find_asset(c1::ASSET_TYPE_LOADED_SOUND, "unit/tone.wav"), sound));
		CHECK(sound.info.format == WAVE_PCM && sound.info.rate == 22050 && sound.info.samples == 3000 && sound.data == tone.data);
		CHECK(extract_loaded_sound(loaded, zone.find_asset(c1::ASSET_TYPE_LOADED_SOUND, "unit/stereo.wav"), sound));
		WaveData decoded;
		CHECK(sound.info.format == WAVE_IMA_ADPCM && sound.info.channels == 2 && sound.info.blockSize == 2048 && sound.info.samples == 5 * 2041
			&& ima_adpcm_decode(loaded_sound_wave(sound), decoded) && tone_snr(test_tone(2, 44100, 9000), decoded, 18000) > 30.0);
		SndCurveDef curve;
		CHECK(extract_snd_curve(zone, zone.find_asset(c1::ASSET_TYPE_SOUND_CURVE, "unit_curve"), curve) && curve.knots.size() == 3 && curve.knots[1].first == 0.3f);

		CompiledAliasList list;
		CHECK(extract_sound_alias_list(zone, zone.find_asset(c1::ASSET_TYPE_SOUND, "unit_tone"), list) && list.aliases.size() == 2);
		if (list.aliases.size() == 2)
		{
			const CompiledAlias& a = list.aliases[0];
			CHECK(a.aliasName == "Unit_Tone" && a.subtitle == "SUBTITLE_REF" && a.secondaryAliasName == "unit_stereo" && a.chainAliasName.empty() && a.mixerGroup.empty());
			CHECK(a.hasFile && !a.streamed && a.loadedName == "unit/tone.wav" && a.curve == "unit_curve" && a.speakerMap.isDefault && a.speakerMap.name.empty());
			CHECK(a.raw.volMin == 0.9f && a.raw.volMax == 0.9f && a.raw.flags == ((1u << 7) | (3u << 9)) && a.raw.masterPriority == 50 && a.raw.slavePercentage == 1.0f
				&& a.raw.probability == 1.0f && a.raw.distMin == 120.0f && a.raw.distMax == 600.0f);
			CHECK(list.aliases[1].loadedName == "unit/stereo.wav" && list.aliases[1].speakerMap.name == "unit_map" && !list.aliases[1].speakerMap.isDefault
				&& std::memcmp(list.aliases[1].speakerMap.channelMaps, default_speaker_map().channelMaps, sizeof(c1::MSSChannelMap) * 4) == 0);
		}
		CHECK(extract_sound_alias_list(zone, zone.find_asset(c1::ASSET_TYPE_SOUND, "unit_stream"), list) && list.aliases.size() == 1 && list.aliases[0].streamed
			&& list.aliases[0].streamDir == "unit" && list.aliases[0].streamName == "stream.wav" && list.aliases[0].curve == "$default"
			&& list.aliases[0].raw.flags == (1u | (2u << 7) | (32u << 9)));
		// a loaded sound that is not in the source tree: by name, as shipped zones name sounds of other zones
		CHECK(extract_sound_alias_list(zone, zone.find_asset(c1::ASSET_TYPE_SOUND, "unit_shipped"), list) && list.aliases.size() == 1
			&& list.aliases[0].loadedName == "weapons/not_here.wav" && !zone.find_asset(c1::ASSET_TYPE_LOADED_SOUND, "weapons/not_here.wav"));
		CHECK(extract_sound_alias_list(zone, zone.find_asset(c1::ASSET_TYPE_SOUND, "unit_silent"), list) && list.aliases.size() == 1 && !list.aliases[0].hasFile);
		fs::remove_all(root, ec);
	}

	// shipped alias lists and loaded sounds of a map zone: as text / .wav through the compiler into a zone, loaded
	// again and compared
	void test_sounds_install(const fs::path& install)
	{
		z::ClientKey key;
		z::ZoneEditor shipped;
		CHECK(key.load_from_exe(install / "C1.exe"));
		if (!shipped.open(install / "client_shipRetail" / "chinese" / "mp_rust.ff", key))
		{
			std::printf("mp_rust.ff does not open: the shipped sound test is skipped\n");
			return;
		}
		const z::Zone& zone = shipped.zone();
		z::ZoneEditor editor;
		CHECK(editor.create("codol_sound_shipped"));
		std::vector<CompiledAliasList> lists;
		std::map<std::string, CompiledLoadedSound> sounds;
		for (const z::AssetRecord& r : zone.all_assets())
		{
			CompiledLoadedSound sound;
			CompiledAliasList list;
			if (r.type == c1::ASSET_TYPE_LOADED_SOUND && extract_loaded_sound(shipped, r.header, sound))
			{
				WaveData wave;
				CompiledLoadedSound compiled;
				CHECK(parse_wav(write_wav(loaded_sound_wave(sound)), wave) && compile_loaded_sound(sound.name, wave, compiled) && compiled.data == sound.data
					&& std::memcmp(&compiled.info, &sound.info, sizeof sound.info) == 0);
				if (sounds.emplace(sound.name, sound).second)
				{
					CHECK(link_loaded_sound(editor, compiled));
				}
			}
			else if (r.type == c1::ASSET_TYPE_SOUND && extract_sound_alias_list(zone, r.header, list))
			{
				SoundLibrary library;
				for (const CompiledAlias& a : list.aliases)
				{
					library.speakerMaps[a.speakerMap.name] = a.speakerMap;
				}
				std::vector<SoundAliasDef> rows;
				std::vector<CompiledAliasList> compiled;
				CHECK(parse_sound_aliases(write_sound_aliases(alias_list_defs(list)), rows) && compile_sound_aliases(rows, library, compiled) && compiled.size() == 1);
				if (compiled.size() == 1 && editor.find_asset(c1::ASSET_TYPE_SOUND, list.name) == z::ZoneEditor::npos)
				{
					CHECK(same_compiled(list, compiled[0]));
					CHECK(link_sound_alias_list(editor, compiled[0]));
					lists.push_back(std::move(list));
				}
			}
		}
		CHECK(lists.size() >= 20 && sounds.size() >= 20);
		std::vector<std::uint8_t> bytes;
		z::ZoneEditor made;
		CHECK(editor.write(bytes) && made.load(bytes, "codol_sound_shipped"));
		if (!made.loaded())
		{
			return;
		}
		std::size_t same = 0, sameSounds = 0;
		for (const CompiledAliasList& list : lists)
		{
			CompiledAliasList mine;
			same += extract_sound_alias_list(made.zone(), made.zone().find_asset(c1::ASSET_TYPE_SOUND, list.name), mine) && same_compiled(list, mine) ? 1 : 0;
		}
		for (const auto& [name, sound] : sounds)
		{
			CompiledLoadedSound mine;
			sameSounds += extract_loaded_sound(made, made.zone().find_asset(c1::ASSET_TYPE_LOADED_SOUND, name), mine) && mine.data == sound.data
				&& std::memcmp(&mine.info, &sound.info, sizeof sound.info) == 0 ? 1 : 0;
		}
		CHECK(same == lists.size());
		CHECK(sameSounds == sounds.size());
	}
}
