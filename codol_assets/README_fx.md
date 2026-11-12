## Effects

`.efx` - the text format of the effects editor ( `iwfx 2`, what the CoD4 / MW2 mod tools keep under `raw/fx` ) -
compiled to a native `FxEffectDef`. `include/codol_assets/fx.hpp`, `src/fx_efx.cpp` ( reader / writer ),
`src/fx_compile.cpp` ( the converter and its inverse ), `src/fx_zone.cpp` ( reading a shipped effect, linking,
the atlas table ), `tools/verify_fx.cpp`.

```
codol_assetc --install <game> --out my_fx.ff --fx misc/my_effect=raw/fx/misc/my_effect.efx
fx misc/my_effect: 3 elements ( 1 looping, 2 one shot, 0 emission ), 1843 bytes
  fx misc/my_effect: material gfx_smk_white_atlas is not in the zone, referenced by name
```

The name is what `loadfx( "misc/my_effect" )` takes. `--fx-atlas <file>`: the texture atlas table of the shipped
materials ( default `<out dir>/codol_fxatlas.txt`, made on first use: every fast file is opened once, half a minute;
343 atlas materials ). `--verify` lists the effects of a zone and reads each back.

### The asset as 1.1.1.13 stores it

Layouts in `codol_zone/include/codol_zone/c1_layouts.hpp` ( `FxEffectDef`, `FxElemDef`, `FxElemVelStateSample`,
`FxElemVisStateSample`, `FxTrailDef`, `FxTrailVertex`, `FxSparkFountainDef`, `FxElemAtlas` ), read off the loaders
( `0x004BD5E0` effect, `0x004BD2B0` element ) and the 1840 distinct shipped effects ( 9024 elements ).

* `FxEffectDef` ( 32 bytes ) and the first 252 bytes of `FxElemDef` are IW4's, field for field.
* `FxElemDef` is **260** bytes, IW4's is 252: four bytes at +252 ( an integer: 500 .. 5000 in the 9 elements that have
  it ) and a string pointer at +256 ( 16 elements, sound alias names such as `dst_electronics_elc_sm` ). Carried, not
  understood.
* The element type list is the later engines': IW4's plus "rotated sprite" ( 2 ) and "line" ( 4 ), everything behind
  them moved up - billboard 0, oriented 1, rotated 2, tail 3, line 4, trail 5, cloud 6, spark cloud 7, spark fountain
  8, model 9, omni light 10, spot light 11, sound 12, decal 13, runner 14. The loader agrees: a `FxTrailDef` follows a
  type 5 element, a `FxSparkFountainDef` a type 8, type 13 has pairs of materials, 9 a model, 12 and 14 a name, 10 / 11
  nothing.
* Visual state samples are 48 bytes ( two 24 byte states, as IW4; the later builds have 64 ).
* Shipped element types: billboard 4736, runner 848, tail 789, model 621, line 542, omni light 400, oriented 280,
  cloud 203, decal 169, rotated 169, sound 130, trail 53, spark fountain 46, spark cloud 37, spot light 1.
* How an effect names other assets ( `codol_asset_probe graph <zone> <exe> fx <name>` ): a material or model handle is
  the asset nested behind the field, an alias to the field that named it first, or a header that holds `,name` - an
  asset of another zone. Other effects ( runner, on impact / death, emitted ) and sound aliases are plain name strings
  in the stream. `link_fx` points at materials and models that are top level assets of the zone and writes `,name`
  references for the others.

### What the converter does

Every rule below was fitted to shipped data. 453 shipped effects have the name of an `.efx` of the CoD4 mod tools and
most of them still are what that file says: source and result side by side ( `fx-source` ).

* Sections: elements with the editor flag `looping`, then the one shot ones, both in file order; `disabled` ones are
  left out. The **emission** section holds the elements of the effects that elements of this one emit ( `emission
  "x"` with `playOnRun` ): `x.efx` is read from the same fx folder and its elements are compiled into the effect.
  Without that file the effect is only named and a note says so.
* Flags: the words of `flags` ( `spawnRelative` 0x2, `spawnFrustumCull` 0x4, `runnerUsesRandRot` 0x8,
  `spawnOffsetSphere` 0x10 / `Cylinder` 0x20, `runRelToSpawn` 0x40 / `Effect` 0x80 / `Offset` 0xC0, `useCollision`
  0x100, `dieOnTouch` 0x200, `drawPastFog` 0x400, `drawWithViewModel` 0x800, `blocksSight` 0x1000,
  `modelUsesPhysics` 0x8000000, `nonUniformScale` 0x10000000 ); derived: 0x1000000 / 0x2000000 the element has a
  velocity in its own / the world frame, 0x4000000 it has gravity. Bits 13 .. 20 and 29 / 30 have the names IW4 gives
  them ( `drawInThermalOnly` .. `cloudShapeSphereSmall` ) and are carried bit for bit.
* Ranges: a distance or position with a negative amplitude is stored from its other end ( `spawnOrgY 10 -20` ->
  base -10, amplitude 20 ); angles ( degrees -> radians ), angular velocity ( degrees / s -> radians / ms ), gravity
  ( percent -> fraction ) and elasticity as written. Looping count 0 is "for ever" ( `0x7FFFFFFF` ).
* Graphs are sampled at `intervals + 1` points. The limit is `( life + life amplitude / 2 ) / 100`, 1 .. 80; from 1 up
  to it a count is taken when it lowers the largest distance of a curve key from the sampled curve by more than
  0.02 ( with 0.02: 1447 of 1448 velocity counts and 1393 of 1448 visual counts of the paired elements, 99.8 % and
  99.1 % in `fx-source`; with 0.018 or 0.022 under 1420 ). Velocity: all six graphs. Visual state: only the graphs the
  element type draws with, a second curve only with its `useRand..` flag, `sizeGraph1` only with `nonUniformScale`.
* Velocity sample = graph value * scale / ( 1000 * intervals ), `totalDelta` its running trapezoid sum. A graph set is
  in the element's own frame, with `absVel<n>` in the world's; both sets add up when they share a frame. The random
  part: of the first set `second curve - first`; of the second set the second curve *is* the spread, unless both sets
  are random velocities of one frame ( 726 of 774 unambiguous samples; the rest are two effects ).
* Visual state: colour bytes b g r a, rounded; with `modColorByAlpha` the colour is multiplied by alpha and alpha is
  255; the "amplitude" colour is the second colour, not a difference. Rotation = degrees / s -> radians per interval,
  `rotationTotal` its sum. Size = graph * scale / 2 ( half width ); without `nonUniformScale` the second size repeats
  the first. Scale = graph * scale. Per type: sprites, tails, lines colour + rotation + sizes; trail colour + sizes;
  clouds and sparks colour + sizes + scale; model scale; decal colour + rotation + size; lights colour + size, scale
  1.5 ( what every light of a paired effect has; `//#CODOL lightScaleGraph` takes it from `scaleGraph` ); sound scale;
  a runner has no visual samples.
* Atlas: only elements that draw a material. Behaviour bits `startRandom` 1, `startIndexed` 2, `playOverLife` 4,
  `loopOnlyNTimes` 8; the loop count is stored one higher; columns and rows are the **material's** ( from the zone
  being built, else the atlas table ), as bit counts, `entryCount = 1 << ( both )`.
* A decal names one material and gets its `mc/` and `wc/` variants. A light has `visualCount` 1 and no visual.
* `fxOnImpact` / `fxOnDeath` / `emission` are taken with `playOnTouch` / `playOnDeath` / `playOnRun`. `lightingFrac`
  is cut to a byte; the effect's flag 1 says one of its elements is lit. Collision bounds: `collOffset` +-
  `collRadius`. `sortOrder` 5 when the file has none.
* Trail: `scrollTimeMsec`, `repeatDist`, the inverse split distances; a vertex gets the normal ( -dy, dx ) of its
  outline segments, averaged; where that cancels - a ribbon drawn there and back - every segment gets vertices of its
  own ( 2079 of 2082 trail / spark definitions in `fx-source` ).
* `totalSize`: header, name, elements, samples, handle arrays, strings at +256, a trail as `36 + 22 * indices`, a
  spark fountain 52 ( 1840 of 1840 ).

What the classic format has no word for is read from, and written as, comment lines inside an element:
`//#CODOL section emission`, `intervals <velocity> <visual>` ( take these counts instead of deciding ), `flagBits`,
`fadeInfo`, `unk252`, `string256`, `sparkFountain <13 values>`, `trailNormals`, `lightScaleGraph`. Statements the
reader does not know are kept and written back, not compiled.

### Verification

All numbers from `codol_asset_verify` on the 1.1.1.13 install of this machine ( x64 Release ).

`fx-roundtrip`: every distinct shipped effect -> `extract_fx` ( one curve key per stored sample ) -> `.efx` text ->
`parse_efx` -> `compile_fx` -> compared with the shipped `FxEffectDef`. 1840 effects, 9024 elements, none fails to
compile. Floats within 2e-5 relative, colours within 1.

| field group | interval counts given | the compiler decides |
|---|---|---|
| element counts, effect flags, looping life | 1840 / 1840 | 1840 / 1840 |
| total size | 1840 / 1840 | 1389 / 1840 |
| type, flags, spawn, ranges, delay / life, origin, angles, gravity, visuals, collision, effect names, emit distance, trail / spark definition, sort order .. | 9024 / 9024 each | 9024 / 9024 each |
| atlas | 9023 / 9024 | 9023 / 9024 |
| velocity interval count / samples | 9024 / 9023 | 8614 / 8613, 95.4 % |
| visual interval count / samples | 9024 / 9024 | 8700 / 8700, 96.4 % |
| samples bit exact ( velocity / visual ) | 57.8 % / 51.2 % | |
| **whole element** | **9022 / 9024, 99.98 %** | 8313 / 9024, 92.1 % |
| **whole effect** | **1838 / 1840, 99.89 %** | 1388 / 1840, 75.4 % |
| whole effect, another sampling of the same curves ( within 2 % of each channel's peak ) allowed | | 1609 / 1840, 87.4 % |

"Given": the definition carries `//#CODOL intervals`, as `fx-extract` writes it. "Decides": the sampled curves are
all the compiler has, and where the artist's curve needed fewer or more samples than the resampled one the count
differs ( 410 + 324 elements ); the curves are the stored ones either way. The two effects that differ with given
counts: `impacts/double_points` ( an atlas with no loop count on an element that has a material ) and
`weapon/muzzleflashes/fx_freezegun_ug_view` ( one velocity sample ).

`fx-source`: the 453 shipped effects that have an `.efx` of their name in the CoD4 mod tools' `raw/fx`, that file
compiled ( emitted effects from the same folder, atlases from the shipped materials ) against the shipped effect.
This is the check against data the library did not write; the effects are 1.1.1.13's, the files CoD4's, so part of
every difference is content that changed in between ( other materials, cull radii and lighting added, colours ).

| | equal |
|---|---|
| element counts ( looping / one shot / emission ) | 401 / 453 |
| element type, delay / life, angles, origin, emit distance, effect names, collision, trail definition | 99.1 .. 100 % of 2082 elements |
| velocity interval count / visual interval count | 99.8 % / 99.1 % |
| velocity samples / visual samples | 98.8 % / 94.4 % |
| atlas | 96.7 % |
| gravity, elasticity / sort order, lighting .. | 97.5 % / 96.6 % |
| visuals ( the same materials ) | 93.9 % |
| flags / spawn range, fades, cull radius | 86.3 % / 87.0 % ( 204 elements got a frustum cull radius and its flag after CoD4 ) |
| whole element / whole effect | 72.0 % / 220 of 453 |

`fx-link`: all 1840 shipped effects recompiled from their definition text, linked into **one zone** ( 10.99 MB,
5998 by-name references ), the zone written, loaded with the game's loaders and read back with `read_fx`: 1840 / 1840
effects and 9024 / 9024 elements are what was compiled, names of materials, models, sounds and effects included.

Unit tests ( `tests/test_fx.inc`, part of `codol_assets_tests` ): the reader ( graphs, visuals, comments, unknown
statements, malformed curves ), every rule above on a hand made definition and on `tests/fx/codol_test/spark_burst.efx`
( a looping smoke billboard, a spark tail burst, a light ), extract -> text -> compile, trails ( ribbon and closed
outline ), decal, runner, model, sound, the emission section, a zone with two effects through the loaders and back,
the atlas table; with an install: the 234 effects of `common_mp` through definition text and back.

### In a map

`codol_bsp` writes the script side ( `codol_bsp/README.md`, "Entities" ): a `script_fx` entity with `fx
misc/my_effect` becomes a `loadfx( "misc/my_effect" )` line in `maps/mp/<map>_fx.gsc` and a `createOneshotEffect` /
`createLoopEffect` entry in `maps/createfx/<map>_fx.gsc`. The effect asset itself goes into the map's zone ( `--base
<map>.ff` ) or a zone loaded with it:

```
codol_assetc --install <game> --base mp_mymap.ff --out mp_mymap.ff --fx misc/my_effect=raw/fx/misc/my_effect.efx
```

In script:

```
level._effect[ "my_effect" ] = loadfx( "misc/my_effect" );
playfx( level._effect[ "my_effect" ], ( 0, 0, 64 ) );
```

### Limits, not verified

* **Not seen in the game.** The loaders take the zones and what they build is what was compiled; no client has drawn
  a compiled effect, and no server has run `loadfx` on one here ( the test recipe is in the report of this work; the
  server does not draw effects in any case ).
* The +252 integer and the +256 string of `FxElemDef` are carried, their meaning is not known. The flag bits above
  `blocksSight` have IW4's names; that 1.1.1.13 gives them the same meaning was not shown.
* The second velocity graph set's random rule and the "back compatible" case are fitted, with two shipped effects
  against them. `useBackCompatVel` has no other effect here.
* Light intensity ( the scale of a light's visual state ) is 1.5 unless `lightScaleGraph` is given; the editor's own
  field for it, if 1.1.1.13's editor had one, is not known. Spark fountain values are the stored numbers, no units
  converted. `fadeInfo` only through its comment line.
* A frustum cull radius is not computed ( 204 paired elements have one the CoD4 file does not ).
* Whether the game uses the copied emission elements or the named effect at run time was not examined; both are
  written, as in the shipped effects.
* The atlas of a material that is neither in the zone being built nor in a shipped zone is taken from
  `atlasColIndexBits` / `atlasRowIndexBits` of the file.
* The editor is not wired: `editor/src/assets.cpp` does not know effects yet.

### Server check ( recipe )

```
codol_assetc --install <game> --out <scratch>\codol_test_fx.ff --fx codol_test/spark_burst=codol_assets\tests\fx\codol_test\spark_burst.efx
codol_assetc --verify <scratch>\codol_test_fx.ff --install <game>
```

or, the way a map carries its effects, `--base mp_rust.ff --out <shadow>\mp_rust.ff`. Level script overlay
( `maps\mp\<map>.gsc`, as `codol_bsp/tests/harness/trace_probe.ps1` writes one ):

```
main()
{
	level._effect[ "codol_spark" ] = loadfx( "codol_test/spark_burst" );
	thread codol_fx_probe();
}

codol_fx_probe()
{
	wait 5;
	playfx( level._effect[ "codol_spark" ], ( 0, 0, 64 ) );
	ent = spawnfx( level._effect[ "codol_spark" ], ( 0, 0, 96 ) );
	triggerfx( ent );
	println( "CODOL_FX id " + level._effect[ "codol_spark" ] );
}
```

Expected: no script error, the effect id not 0 ( `loadfx` of a name no zone has gives the "missing_fx" effect and a
warning on the client; a dedicated server registers the name without looking at the asset, so the server shows that
the zone mounts and the script runs, not that the effect is right ).
