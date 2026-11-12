## Sounds

*( fragment for `codol_assets/README.md`; the table rows and option lines for the other sections are at the end )*

1.1.1.13 has the Miles sound system assets of IW4, not middleware banks: `sound` ( type 11, `snd_alias_list_t` ),
`sndcurve` ( 12 ), `loaded_sound` ( 13 ), read by `Load_snd_alias_list_t` 0x004B8750 .. `Load_MssSound_data`
0x004B7F30. The layouts are in `codol_zone/include/codol_zone/c1_layouts.hpp`; against IW4:

| struct | size | difference from IW4 |
|---|---|---|
| `snd_alias_list_t` | 12 | none: name, aliases, count. The name is the alias name in lower case |
| `snd_alias_t` | 108 ( IW4: 100 ) | `masterPriority` ( u8 ) and separate `masterPercentage` / `slavePercentage` behind `flags` ( the IW5 layout ) |
| `SoundFile` | 12 | none: type ( 1 loaded, 2 streamed ), `exists`, a `LoadedSound` or directory + file name |
| `LoadedSound` / `MssSound` | 48 / 44 ( IW4: 44 / 40 ) | the `AILSOUNDINFO` has one more field in front of `samples`, 0xFFFFFFFF in every shipped sound |
| `SndCurve` | 136 | none: name, knot count, 16 knots |
| `SpeakerMap` | 408 | none: default flag, name, four `MSSChannelMap` ( mono / stereo source to 2 / 6 speakers ) |

What the shipped sounds are ( `codol_asset_verify sound-survey`: 12969 alias lists with 27397 aliases in 73 of the
install's fast files, 4401 distinct loaded sounds, 448 MB ):

* `flags`: bit 0 looping ( 1299 aliases ), bits 3 and 4 the two reverb bits ( 2118 / 3392, mostly together on 2D
  sounds ), bit 5 on 759 aliases that all loop as well ( taken to be `rlooping` ), bit 6 on 31 distant weapon
  aliases ( no column; `flags` carries it ), bits 7 - 8 the type, bits 9 - 14 the channel - the line number in
  `soundaliases/channels.def` ( rawfile of `code_pre_gfx_mp`: `physics` 0 .. `auto` 3 .. `weapon` 21 .. `voice` 24 ..
  `music` 32 .. `shellshock` 36 ). Bits 1, 2 and 15 .. 31 are never set; `sequence`, `velocityMin` and the three
  envelop fields are 0 in every alias.
* `masterPriority` is 50 ( 67 % ), 0 or 75; `slavePercentage` 1 ( 54 % ) .. 0.5; `masterPercentage` 0 ( 96 % ).
* The mixer group ( `vol_mod` column ) is empty for 87 %, else `mission`, `max`, `weapons`, `ambience`, `music`.
* 15090 aliases play a loaded sound, 12305 a streamed file ( 12140 `.mp3`, 165 `.wav`: `sound/<dir>/<file>` in the
  `iw_sound_*.iwd` archives ), 2 have no file.
* Loaded sounds are the wave data without the file header, named like their source ( `weapons/foley/x.wav` ):
  **IMA ADPCM** ( format 17, 4 bits; 3951 ) or **16 bit PCM** ( 450 ), mono or stereo, 11025 .. 48000 Hz. ADPCM
  blocks: 1024 bytes at 44100 / 48000 Hz mono, 768 at 32000, 512 at 22050, 256 at 11025; stereo 2048 or 1024.
  `samples` counts every sample of every channel for PCM and the frames of the whole blocks for ADPCM. The data is
  read into the temporary block ( Miles gets a copy ), so a loaded zone image does not keep it: `extract_loaded_sound`
  takes it from the `ZoneEditor`'s record of the stream.
* Every alias has a volume falloff curve - `$default` ( 0,1 1,0 ) for 68 % - and a speaker map - the default map
  ( empty name ) for 90 %. A curve is an asset and mostly a reference by name ( `,weapon2` ); a speaker map is 408
  bytes inside the alias list, shared by the aliases of a list. 12 distinct curves, 16 speaker maps.
* A loaded sound of another zone is named the same way ( a `LoadedSound` header with `,name` and no data: 1755
  aliases ).

### Source

The tree is laid out like `raw/` of the mod tools; `--sound-root` names it, by default it is the folder above
`soundaliases/` ( or the folder of the csv ):

```
soundaliases/<anything>.csv       alias definitions
sound/<file>                      what the file column names
soundaliases/<name>.vfcurve       a volume falloff curve of your own
soundaliases/<name>.spkrmap       a speaker map ( needed for every map but the default one )
```

**csv.** With a header row ( first field `name` ) the columns are found by name, without one the column order of the
CoD4 mod tools is assumed. `#` starts a comment line; fields may be quoted. Rows with the same name ( any case ) are
the variants of one alias.

| column | meaning | default |
|---|---|---|
| `name` | alias name; the asset is named in lower case | |
| `file` | relative to `sound/`, `/` or `\`, with extension | none: the alias plays nothing |
| `vol_min`, `vol_max` | 0 .. 1, stored as given | 1, `vol_min` |
| `vol_mod` | the mixer group string ( `max`, `mission`, `weapons`, `ambience`, `music` ); the volume is **not** scaled by it | none |
| `pitch_min`, `pitch_max` | | 1, `pitch_min` |
| `dist_min`, `dist_max` | | 120, 600 |
| `velocity_min` | | 0 |
| `channel` | a name of `channels.def` | `auto` |
| `type` | `loaded` / `streamed` | by extension: `.mp3` streamed, else loaded |
| `probability` | | 1 |
| `loop` | `looping`, `rlooping`, `nonlooping` | not looping |
| `masterslave` | a number: the slave percentage | 1 |
| `masterpriority`, `masterpercentage`, `slavepercentage` | the three fields of this build | 50, 0, 1 |
| `subtitle`, `secondaryaliasname`, `chainaliasname` | | none |
| `compression` | `adpcm`: a PCM source is stored as IMA ADPCM; otherwise the file's own format | |
| `volumefalloffcurve` | curve name | `$default` |
| `startdelay` | ms | 0 |
| `speakermap` | speaker map name | the default map |
| `reverb` | `fulldrylevel` ( bit 3 ), `nowetlevel` ( bit 4 ), or both | |
| `lfe percentage`, `center percentage`, `envelop_min`, `envelop_max`, `envelop percentage`, `sequence` | | 0 |
| `flags` | hex, the bits without a column ( 0x40 .. ) | 0 |
| `loadspec`, `platform`, `conversion_quality` | ignored: every row is compiled | |

**wav.** RIFF WAVE, mono or stereo: 16 bit PCM is stored as it is, 8 and 24 bit PCM become 16 bit, an IMA ADPCM file
is stored as it is. `compression adpcm` runs this library's IMA ADPCM coder ( block size as the shipped mono sounds:
256 bytes per channel and 11025 Hz ); the last block is filled by repeating the last frame, so a loop that has to be
seamless should stay PCM or have a whole number of blocks ( 2041 frames at 44100 Hz mono ). Other rates than the
shipped ones are not resampled.

**vfcurve.** `SNDCURVE`, the knot count, then `x y` per knot ( 2 .. 16 ). A curve without a source file is referenced
by name: the shipped ones ( `$default`, `default`, `weapon1`, `weapon2`, `weapon4`, `weapon5`, `reaction`,
`weapon_grenadebounce`, `helicopter1`, `exponential_gradual`, `exponential_40db`, `roomtone` ) live in the common and
map zones - a name no loaded zone defines gives the game's default asset.

**spkrmap.** This library's own text form ( the original source format is not in the install ): `SPKRMAP`, then four
blocks `map <mono|stereo> <2|6>` with one line per speaker, `<speaker> <level of source channel 0> [<level of source
channel 1>]`. `codol_asset_verify sound-extract` writes the shipped maps in this form.

### What is produced

* per distinct `loaded` file that exists under `sound/`: a top level `LoadedSound`. A file that is not there is
  referenced by name ( `,weapons/foley/x.wav` ) - the way to reuse a shipped sound - and the tool says so;
* per `.vfcurve` used: a top level `SndCurve`; other curves by name;
* per alias name: a `snd_alias_list_t` with its variants, strings, `SoundFile`, curve pointer and speaker map;
* streamed files found under `sound/` are packed into the `--iwd` archive as `sound/<file>` ( a plain zip; name it
  without `iw_sound`, as for images ). They are not converted: the shipped ones are `.mp3` and a few `.wav`.

```
> python tests/make_sound_sources.py src
> codol_assetc --install <game> --out codol_test_sounds.ff --iwd zz_codol_test_sounds.iwd --soundalias src/soundaliases/codol_test_sounds.csv
loaded sound codol_test/beep.wav: 16 bit PCM, 1 channel(s), 22050 Hz, 9922 frames, 19844 bytes
sound curve codol_test_near: 4 knots
loaded sound codol_test/click8.wav: 16 bit PCM, 1 channel(s), 11025 Hz, 2205 frames, 4410 bytes; 8 bit samples stored as 16 bit
loaded sound codol_test/hum.wav: IMA ADPCM, block 2048, 2 channel(s), 44100 Hz, 44902 frames, 45056 bytes
sound codol_test_shipped: loaded sound null.wav is not in the zone, referenced by name
sounds: 4 alias lists ( 5 aliases ), 3 loaded sounds, 1 curves, 1 streamed files found
wrote zz_codol_test_sounds.iwd: 0 images, 1 streamed sound files
wrote codol_test_sounds.ff: 8 assets, zone 72815 bytes
> codol_assetc --verify codol_test_sounds.ff --install <game>
codol_test_sounds.ff: the game's loaders consume the zone to the last byte: 8 top level assets, 12 with nested ones, 0 warnings
read -> write: identical ( 72815 bytes )
  loaded_sound codol_test/beep.wav                   16 bit PCM, 1 channel(s), 22050 Hz, 9922 samples, 19844 bytes
  sound     codol_test_beep                          2 alias(es): codol_test/beep.wav [auto] codol_test/click8.wav [auto]
  loaded_sound codol_test/hum.wav                    IMA ADPCM, 2 channel(s), 44100 Hz, 44902 samples, 45056 bytes, decodes
  sound     codol_test_wind                          1 alias(es): streamed:codol_test/wind.wav [ambient, looping]
  sound     codol_test_shipped                       1 alias(es): null.wav ( by name ) [auto2d]
  ..
```

`--base <map>.ff` adds them to a map zone ( checked with `mp_rust.ff`: 173 assets, loaders accept it, read -> write
identical ).

### Playing them

```
// level script ( maps/mp/<map>.gsc ), or any script of the map
emitter = spawn( "script_origin", ( 0, 0, 64 ) );
emitter playSound( "codol_test_beep" );          // one shot, a random variant of the alias
emitter playLoopSound( "codol_test_hum" );       // a looping alias ( loop column ); stopLoopSound() ends it
ambientPlay( "codol_test_wind" );                // the map's ambient track: a looping, streamed alias on channel ambient
player playLocalSound( "codol_test_beep" );      // 2D, for one player
```

The alias only has to exist in a zone the client has loaded - the map's zone ( `--base` ), or a zone of the map's
load list; there is nothing to precache in script. What a modder has to add besides the zone: the `.iwd` with the
streamed files in `main/`, and for subtitles the localized string the `subtitle` column names. `tests/sound_probe.ps1`
writes a level script overlay with exactly the calls above for the server harness of `codol_bsp`.

### Verification

`codol_asset_verify sound-roundtrip <install> <C1.exe>`: every distinct shipped alias list -> rows -> csv text ->
parsed -> compiled ( speaker maps through their text form ) -> compared with the shipped aliases; every distinct
loaded sound -> the bytes of a `.wav` file -> parsed -> compiled -> compared.

| field | equal |
|---|---|
| alias struct ( the 76 non pointer bytes: volumes, pitch, distances, flags, priorities, percentages, delay ) | 27397 / 27397 |
| alias strings ( name, subtitle, secondary, chain, mixer group ) | 27397 / 27397 |
| sound file ( type, loaded sound name / stream directory + file ) | 27310 / 27397, 99.68 % |
| volume falloff curve name | 27397 / 27397 |
| speaker map ( name, default flag, the 400 bytes of levels ) | 27397 / 27397 |
| **whole alias** | **27310 / 27397, 99.68 %** |
| **whole alias list** | **12899 / 12969, 99.46 %** |
| loaded sound: name, format, rate, bits, channels, samples, block size, length | 4401 / 4401 |
| loaded sound: data | 4401 / 4401 |

The 87 aliases that differ are streamed files of the zombie mode whose directory is stored with backslashes
( `wpn\matroyshka\chrs` ); the compiler writes `/` as the other 12218 have it. The default speaker map the compiler
makes by rule equals the shipped one in all 24563 uses. The ADPCM coder: 494 shipped ADPCM sounds decoded, encoded
again with the same block size and decoded - signal to noise 87.4 dB mean, 31.8 dB minimum ( the unit test encodes
sine mixes: above 30 dB ).

`codol_asset_verify sound-zone <zone.ff> <C1.exe> <out.ff>`: all alias lists, loaded sounds and curves of a shipped
zone compiled from their text / `.wav` form, linked into a new zone, that zone loaded with the game's loaders
( `codol_zone` ) and compared with the shipped assets:

| zone | lists / aliases | loaded sounds | new zone | read back equal |
|---|---|---|---|---|
| `mp_rust` | 24 / 40 | 31 | 4.8 MB, 55 assets | 24 / 24 lists, 31 / 31 sounds |
| `common_mp` | 2044 / 7030 | 837 | 36.7 MB, 2885 assets | 2044 / 2044, 837 / 837 |
| `common_gametype_zm` | 2354 / 5013 | 1416 | 67.2 MB, 3773 assets | 2325 / 2354 ( the backslash directories ), 1416 / 1416 |

`codol_asset_verify sound-extract <zone.ff> <C1.exe> <out dir> [<name part>]` writes the sounds of a zone as a
source tree ( csv, `.wav`, `.vfcurve`, `.spkrmap` ) to start from. Unit tests ( `tests/test_sounds.inc`, 209 checks
with an install ): wave reader / writer, the coder, csv with and without header, curve and speaker map text, flag
packing, a source tree built into a zone and read back through the loaders, the sounds of `mp_rust.ff` round
tripped into a zone.

### Not done, taken on trust

* **Nothing was played.** No client run, and the dedicated server was not started for sounds ( it has no sound
  system; what it can show is that a script calling `playSound` / `playLoopSound` / `ambientPlay` with the new
  aliases runs: `tests/sound_probe.ps1` ). That Miles accepts a `LoadedSound` made here rests on the fields and data
  being byte identical to the shipped ones for the same wave.
* Flag names: `looping` is certain ( looping ambient / engine aliases ); `fulldrylevel` = bit 3, `nowetlevel` = bit 4
  and `rlooping` = bit 5 are the IW4 order, not established from the exe. Bit 6 has no name.
* `vol_mod` is stored as the mixer group and does not scale the volume ( the shipped volumes look scaled:
  0.93, 0.729 .. ); `soundaliases/volumemods.def` is not read.
* `masterslave master` has no field here; use `masterpriority` / `masterpercentage`.
* No resampling, no `.mp3` encoding, no check that a streamed file is something the game can play. PCM is not
  compressed unless the row says `compression adpcm`.
* The `.spkrmap` text form is this library's own. `SpeakerMap` data is written once per alias list, not shared
  between lists as a linker with the original sources may do.
* A by-name reference ( loaded sound, curve ) to something no loaded zone defines gives the game's default asset;
  the linker does not check.
* The editor ( `editor/src/assets.cpp` ) does not scan `soundaliases/` / `sound/` yet: `build_sound_aliases` is the one
  call it needs ( csv, source root, the linker's `ZoneEditor` ).

---

Lines for the other sections of `README.md`:

Layout table:

| `include/codol_assets/sound.hpp` | wave files, IMA ADPCM, soundaliases csv, curves, speaker maps, `compile_sound_aliases`, `build_sound_aliases`, extractors |
| `tools/verify_sound.cpp` | `sound-survey`, `sound-roundtrip`, `sound-extract`, `sound-zone` |
| `tests/make_sound_sources.py`, `tests/sound_probe.ps1` | sound test sources; level script overlay that plays them |

`codol_assetc` options:

```
  --soundalias <file.csv>   sound aliases; loaded .wav files, curves and speaker maps from the tree around it
  --sound-root <dir>        the folder that holds sound/ and soundaliases/ ( default: above soundaliases/, else the csv's folder )
```

For the editor:

```cpp
SoundBuild sounds;
build_sound_aliases(project / "soundaliases" / "my_map.csv", project, linker.editor(), sounds);   // .log, .streamedFiles
// sounds.streamedFiles ( "sound/<file>" + bytes ) go into the project's .iwd next to the streamed images
```
