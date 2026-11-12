# codol_assets

Asset compiler for Call of Duty Online 1.1.1.13: the role Asset Manager + `converter.exe` + `linker_pc` play in the
CoD4 mod tools. It takes what a modder makes - images ( PNG / TGA / DDS ), material definitions ( a small text
format, or the material entries of a CoD4 `.gdt` ), models ( `xmodel_export` ), animations ( `xanim_export` ), sounds
( soundalias `.csv` + `.wav` ), effects ( `.efx` ) - and turns it into native C1 assets inside a fast file:
`GfxImage` ( and the IWI version 9 file of a streamed image ), `Material`, `XModel` with its surfaces, `XAnimParts`,
`snd_alias_list_t` / `LoadedSound` / `SndCurve`, `FxEffectDef`, with correct references to the assets of the game's
own zones.

Inputs are source formats, outputs are native C1 data; nothing is converted to or from another game's asset format.
No third party code: PNG / TGA / DDS readers, the DXT encoder, the zip writer and everything else are this
library's own; inflate / deflate, the zone writer and the game's loaders come from `codol_zone`, the readers the
results are checked with from `codol_render`.

**Nothing here has been loaded by the game client.** What has been shown: the game's own zone loaders ( the machine
translation in `codol_zone` ) consume every zone the linker writes to the last byte; `codol_view` draws the new
images, materials and models; a dedicated server ( CoD4x_Server `codol_dedrun` ) mounts the zones, decodes the models
and animations, starts a map whose zone carries them and runs a level script that plays a new animation on a new
model, a new effect and new sound aliases without a script error. Nobody has seen an animation or an effect of
this compiler move, or heard one of its sounds. See [Verification](#verification) and [Not done, taken on trust](#not-done-taken-on-trust).

## Layout

| path | what |
|---|---|
| `include/codol_assets/bitmap.hpp` | source images: `load_image` ( PNG, TGA, DDS ), `save_png`, `psnr` |
| `include/codol_assets/image.hpp` | `compile_image` ( mips, DXT ), `write_iwi`, `image_pieces`, `write_iwd`, `read_iwi_levels` |
| `include/codol_assets/material.hpp` | `MaterialDef`, `.mtl` / GDT readers, `MaterialLibrary`, `compile_material`, `extract_material` |
| `include/codol_assets/model.hpp` | `xmodel_export` reader / writer, `extract_model_lod`, `compile_model` |
| `include/codol_assets/anim.hpp` | `xanim_export` reader / writer, `compile_anim`, `read_raw_anim` / `decode_anim` ( a stored animation as key tracks ), `extract_anim` |
| `include/codol_assets/sound.hpp` | wave files, IMA ADPCM, soundaliases csv, curves, speaker maps, `compile_sound_aliases`, `build_sound_aliases`, extractors |
| `include/codol_assets/fx.hpp` | `.efx` reader / writer, `compile_fx`, `extract_fx`, `read_fx`, `link_fx`, the atlas table |
| `include/codol_assets/link.hpp` | `AssetLinker`: compiled assets -> zone -> fast file |
| `tools/codol_assetc.cpp` | the command line tool |
| `tools/asset_verify.cpp`, `verify_*.cpp` | `codol_asset_verify`: surveys of and round trips against the shipped assets; every number below ( `anim-*`, `sound-*`, `fx-*`, `model-*`, `material-*`, `image-*` ) |
| `tools/asset_probe.cpp` | `codol_asset_probe`: stream order of a shipped asset, surveys of the shipped images / materials / models |
| `tests/` | unit tests ( `ctest` ), test sources ( `make_test_sources.py`, `make_anim_sources.py`, `make_sound_sources.py`, `fx/` ), the server check scripts ( `asset_probe.ps1`: a level script that uses the new assets ) |

Additions to `codol_zone` ( `zone_editor.hpp` ): `ZoneEditor::add_new_asset` - an asset given as the pieces of stream
data the loaders read for it - and `ZoneEditor::add_asset_closure` - an asset copied from another zone together with
the assets it shares data with.

## Build

```
cmake -S codol_assets -B build/codol_assets_x64 -G "Visual Studio 17 2022" -A x64        ( or -A Win32 )
cmake --build build/codol_assets_x64 --config Release
ctest --test-dir build/codol_assets_x64 -C Release
```

Standalone like `codol_zone`: static CRT, `/std:c++latest /W4 /WX`, builds `codol_zone` and `codol_render` with it.
`CODOL_ASSETS_GAME_DIR` ( CMake cache or environment ) names a 1.1.1.13 install for the part of the tests that
builds a zone; without one that part is skipped. 496 checks ( x64 ).

## codol_assetc

```
codol_assetc --install <1.1.1.13 install> --out <zone.ff> [options] [assets]
  --exe <C1.exe>            default <install>/C1.exe ( the zone key )
  --base <zone.ff>          add to an existing zone ( a map zone, a mod zone ) instead of making a new one
  --name <zone name>        default: the output file name
  --matlib <file>           material library cache; default <out dir>/codol_matlib.txt, made on first use ( 20 s )
  --copy-techsets           copy every technique set into the zone, also those the common zones define
  --source-root <dir>       where the image files of GDT materials are looked for ( default: the GDT's folder )
  --iwi-dir <dir>           write images/<name>.iwi of the streamed images there
  --iwd <file.iwd>          pack them into a plain .iwd
  --zone-out <file>         also write the inflated zone

  --image <name>=<file>[,use=color|normal|specular|detail|ui|function][,format=dxt1|dxt3|dxt5|rgba|rgb|la|l|a]
                        [,streamed][,nomips][,nopicmip][,clamp][,flipy]
  --material <file.mtl>                 every material of the file
  --material gdt:<file.gdt>[:<entry>]   the material entries of a GDT; the images they name are compiled too
  --xmodel <name>=<lod0.xmodel_export>[@dist][,<lod1>[@dist] ..][,collision=<lod>][,contents=<n>]
                        [,mtl:<export material>=<material>][,hitloc:<bone>=<class>]
  --xanim <name>=<file.xanim_export>[,model=<file.xmodel_export>][,type=relative|delta|delta3d][,loop]
                        [,deltapart=<part>][,assettype=<n>][,rottol=<s16 units>][,transtol=<units>]
  --soundalias <file.csv>   sound aliases; loaded .wav files, curves and speaker maps from the tree around it
  --sound-root <dir>        the folder that holds sound/ and soundaliases/ ( default: above soundaliases/, else the csv's folder )
  --fx <name>=<file.efx>    an effect, named as loadfx takes it ( misc/my_effect ); effects it emits: <name>.efx of the same fx folder
  --fx-atlas <file>         atlas table of the shipped materials; default <out dir>/codol_fxatlas.txt, made on first use ( 30 s )
  --carry <zone.ff>[,<type> ..]   take assets of another fast file as they are, each with what it points at; default:
                            xanim, sndcurve, loaded_sound, sound, fx; types as --verify prints them, or all ( "Linking" )
  --rawfile <name>=<file>   also: animtrees/<tree>.atr, animlists/<map>.animlist ( "Animations" )

codol_assetc --verify <zone.ff> --install <dir>
  loads the file with the game's loaders, writes it again ( must be identical ), lists images ( inline ones are
  decoded ), materials, models, animations ( tracks decoded ), sounds ( ADPCM decoded ) and effects ( read back )
```

An image without `use=` gets its use from the material that names it ( `normalMap` -> normal .. ). A `--xmodel`
without `collision=` has no collision ( contents 0, as 42 % of the shipped static models ); `collision=<lod>` makes
the collision surfaces from that LOD with contents 1.

```
> codol_assetc --install <game> --out codol_test_assets.ff --image codol_test_crate_col=crate_col.png
    --image codol_test_crate_nml=crate_nml.png --image codol_test_crate_spc=crate_spc.png --material codol_test.mtl
    --material gdt:codol_test.gdt --xmodel codol_test_crate=crate_lod0.xmodel_export@600,crate_lod1.xmodel_export@2000,collision=1
image codol_test_crate_col: 256 x 256 DXT1 (BC1), 9 levels, 43704 bytes, inline
image codol_test_crate_nml: 256 x 256 DXT5 (BC3), 9 levels, 87408 bytes, inline
..
material mc/codol_test_crate: techset mc_l_sm_r0c0n0s0, sort 1, 3 textures, state as shipped materials have it
material wc/codol_test_wall: techset wc_l_sm_r0c0n0s0, sort 1, 3 textures, state as shipped materials have it
material wc/codol_test_gdt: techset wc_l_sm_r0c0, sort 1, 2 textures, state as shipped materials have it
xmodel codol_test_crate: 1 bones, 2 LODs, 2 surfaces, 120 vertices, 60 triangles, 1 collision surfaces
  material mc/codol_test_crate: technique set mc_l_sm_r0c0n0s0 by name ( defined in code_post_gfx_mp )
  techset wc_l_sm_r0c0n0s0: copied from client_shipRetail/chinese/mp_shipment_long.ff ( 1 assets with what it shares )
  techset wc_unlit_replace_lin: copied from client_shipRetail/chinese/ui_viewer_mp.ff ( 10 assets with what it shares )
  image $identitynormalmap: 1 x 1 copy added, as map zones have it
  material wc/codol_test_gdt: sampler n..p bound to $identitynormalmap as shipped materials do
  model codol_test_crate: LOD 0: triangle corner order reversed to the engine's winding
wrote codol_test_assets.ff: 24 assets, zone 773204 bytes
```

## Images

What the shipped images are ( survey of all 65281 `GfxImage` of the install's 146 fast files against the 21695 `.iwi`
files of the image archives; `codol_asset_probe images / iwis` ):

| use | semantic | IWI format | IWI flags ( most common ) |
|---|---|---|---|
| colour map | 2 | DXT1 5609, DXT5 1783, DXT3 640, 45 others | `0x340`: alpha weighted colours, sRGB, PWL gamma |
| normal map | 5 | DXT5 4700, 7 wavelet | `0x40000` normal map ( `0x40008` in 9 % ) |
| specular map | 8 | DXT5 5292, 6 others | `0x340` |
| detail map | 3 | DXT1 130, DXT5 3 | `0x340` |
| HUD / menu | 0 | RGBA32 1998, DXT5 608, L8A8 325, RGB24 248, DXT3 100, DXT1 87 | `0x173`: no picmip, no mips, clamp, sRGB |

* A normal map stores **x in alpha and y in all three colour channels** ( shipped `ch46e_body_damaged_nml`: R == B,
  grey = y ); z is rebuilt by the shader. The compiler takes the usual source ( x red, y green, z blue ) and stores it
  that way. `,flipy` for sources with green pointing down.
* 55 of the 21695 files use the wavelet formats ( 6 - 10 ). **No wavelet encoder**: the compiler writes DXT1 / DXT3 /
  DXT5 - what 99.7 % of the shipped colour, normal and specular maps are - and the plain formats.
* Mips: 2 x 2 box filter down to 1 x 1; colour is averaged in linear light ( sRGB ), a normal map as vectors and
  renormalised. Images with mips need power of two sizes ( as shipped ); `ui` images may have any size.
* DXT encoder: per block the principal axis of the colours, end points at its extremes, then up to four rounds of
  least squares refitting; DXT5 alpha with the eight value mode, an inset and a refit, and the six value mode when
  the block has 0 / 255; DXT1 punch through when a pixel has alpha below 128.
* Automatic format: colour -> DXT1, DXT5 when the image uses alpha; normal, specular -> DXT5; detail -> DXT1; ui ->
  RGBA32; function -> L8 when grey, else RGBA32.

Two ways the game gets pixels, both written:

* **streamed** ( `,streamed` ): the zone holds the 36 byte header, a `GfxImageLoadDef` without data and the name; the
  pixels are `images/<name>.iwi`. All shipped material textures are like this. Header fields ( bytes 7 .. 19: picmip
  2 / "no picmip", the card memory of all levels and of the levels from picmip 2 on ) are filled the way the shipped
  headers relate to their files ( 63305 of 64438 shipped headers have exactly `file size - 32` there ). The header of
  a 256 x 256 DXT1 image comes out byte identical to shipped `abdn_coaster_mk_sign_text_light_col` except byte 26,
  which is 1 in map zones and 0 in the common zones ( the compiler writes 0 ), and the `.iwi` has the same size and
  level table. `--iwd` packs the files into a plain zip: the game's file system only decrypts archives whose name
  contains `iw_image`, `iw_model`, `iw_config`, `iw_sound`, `iw_ui_image` or `iw_adlc` ( `main\codol_iwd.py` of the
  install ), so a plain archive has to be named differently, e.g. `zz_mod_images.iwd`. An `--iwd` **with** one of
  those names is written encrypted the way the game's are ( `write_iwd` with the key of `C1.exe`: AES-192 in
  counter mode over every entry's name and data, seeded with the CRC of the archive's file name - so the file must
  keep its name ); `--iwd-add <plain archive.iwd>` puts the files of an existing archive into it. The test writes
  one and reads it back with `codol_render`'s reader for the game's archives; the install's own `codol_iwd.py`
  lists such an archive ( `ENCRYPTED .. sound/codol_test/wind.wav` ), and the client opens one named
  `iw_sound_04.iwd` - a plain archive, under any name tried, it never opened.
* **inline** ( default ): the pixels are in the zone, in the `D3DFORMAT` of the same data, per face from level 0
  down. Shipped inline images: lightmaps, reflection probes ( A8R8G8B8 with mips ), `$white` .., and the 72 pad icons
  of `codol_common_postgfx_mp` ( A8R8G8B8 with mips, category 1 ). **Inline DXT is not among them**; `format=rgba`
  gives an inline image exactly like the pad icons ( four times the size ).

## Materials

A `Material` is decided by its technique set for the most part: which samplers and constants it has to supply,
which of the 48 technique slots have state bits, and how each slot's state follows from the material's own. The
state maps that said so are not in the install. `MaterialLibrary` therefore learns from the shipped materials: one
scan of the install ( 16968 distinct materials, 1045 technique sets, 760 distinct states; cached as text ) keeps per
technique set

* the texture table shape ( name hash, first / last character, semantic per slot; sampler state most used ) and the
  image shipped materials bind when they have none of their own - a `wc_l_sm_r0c0` material has a `normalMap` slot
  bound to `$identitynormalmap`;
* the constant table shape with the most common values ( `colorTint`, `envMapParms`, `detailScale` .. );
* every distinct state ( `stateBitsEntry[48]`, state bits table, `stateFlags` ) with the sort keys, game flags and
  camera regions of the materials that have it;
* where it is defined.

`compile_material` takes the state whose own slot ( lit, else emissive, else unlit ) has what the definition asks
for - blend, alpha test, cull, depth write / test, polygon offset; what is not given is left to the most common
state - preferring a state that shipped materials with the same game flags / sort key have. When no shipped material
of the technique set has the combination, the nearest one is changed field by field wherever a slot agrees with the
material's own slot ( reported as "derived" ).

The technique set is named ( `techset` ) or follows from the type and the maps present:

| type | technique set |
|---|---|
| `world phong` / `model phong` | `wc_` / `mc_` + `l_sm_` + `r0` ( replace ) / `b0` ( blend ) / `t0` ( alpha test ) + `c0` + `d0` ( detail map ) + `n0` ( normal map ) + `s0` ( specular map ) |
| `world unlit` / `model unlit` | `wc_` / `mc_` + `unlit_` + `replace` / `blend` / `add` / `multiply` / `screen` + `_lin` |
| `2d` | `2d` |
| `sky` | `wc_sky` |
| `effect` | `effect_blend`, `effect_add` .. |

Named blends as the shipped materials have them: `blend` is ONE / INVSRCALPHA in the lit technique sets and
SRCALPHA / INVSRCALPHA in the unlit ones ( the definition leaves the source factor open ), `add` ONE / ONE,
`multiply` ZERO / SRCCOLOR, `screen` INVDESTCOLOR / ONE. Surface types: bit n - 1 for type n of the usual list
( bark, brick, carpet, cloth, concrete, dirt, flesh, foliage, glass, grass, gravel, ice, metal, mud, paper, plaster,
rock, sand, snow, water, wood, asphalt, ceramic, plastic, rubber, cushion, fruit, paintedmetal ) - read off the
shipped materials' names against their `surfaceTypeBits`.

### .mtl

```
material "mc/my_crate"
{
	type "model phong"            // or: techset mc_l_sm_r0c0n0s0
	colorMap crate_col            // [ tile | clamp | clampu | clampv ] [ nomip | nearest | bilinear | trilinear | aniso | aniso4 ]
	normalMap crate_nml
	specularMap crate_spc
	blendFunc replace             // replace | blend | add | multiply | screen;  or: blend <src> <dst> <op> [ <srcA> <dstA> <opA> ]
	alphaTest none                // none | gt0 | lt128 | ge128
	cullFace back                 // back | front | none
	depthWrite on                 // on | off
	depthTest lessequal           // lessequal | less | equal | always | disable
	polygonOffset none            // none | decal | impact
	sort 1                        // the sort key number
	surfaceType wood              // name or 0x.. bits
	gameFlags 0x52
	constant envMapParms 0.8 2 1 0.6
}
```

Everything but the name and the colour map can be left out. `extract_material` + `write_material_text` write the
definition of a shipped material in this format ( with `sampler=0x..`, `semantic=`, `colorWrite`, `gammaWrite`,
`stateBits <mask> <value>` for what has no name ).

### GDT

`--material gdt:<file>[:<entry>]` reads the entries whose definition file is `material.gdf`: `materialType`,
`colorMap` / `normalMap` / `specularMap` / `detailMap` ( file paths: the image is named after the file and compiled
from it ), `tile*`, `filter*`, `blendFunc`, `alphaTest`, `cullFace`, `depthTest`, `depthWrite`, `polygonOffset`,
`sort` ( numbers only: the sort names of CoD4 have other numbers here ), `surfaceType`, `envMapMin / Max / Exponent`,
`colorTint`. Inheritance ( `"name" [ "parent" ]` ) is followed. `cosinePowerMap` and custom blends are ignored.

## Models

One `xmodel_export` per LOD ( `MODEL`, `VERSION 6`, as CoD4's exporters and CODOL-AssetExplorer write it ). What the
compiler makes of it, each rule read off the shipped models ( 13024 models, 28506 surfaces in zones ):

* One `XSurface` per material and object of a LOD. Vertices are the distinct ( position, normal, colour, uv )
  corners: position as floats in model space ( also of rigid and skinned surfaces ), uv as half floats, colour
  B G R A, normal and tangent as packed unit vectors. The normal is stored with the length the export gives it
  ( shipped normals are not all unit vectors ). The tangent is the u direction of the triangles around the vertex,
  made perpendicular to the normal, with the sign of the binormal.
* Winding: with the corners in stored order `cross( v1 - v0, v2 - v0 )` points **against** the vertex normals ( 99.4 %
  of 8.9 million shipped triangles ). The compiler measures the file and reverses the corners when it has the other
  convention.
* Every surface has an **even** number of triangles ( 28506 of 28506 ): an odd one is followed by a triangle that
  repeats the last index three times. The rigid lists do not count it.
* A surface whose vertices follow one bone each is rigid: vertices and triangles grouped by bone **in the order the
  vertices bring the bones up** ( not by bone index ), one `XRigidVertList` per bone. A surface with weighted vertices
  is `deformed`: vertices sorted by their number of influences ( `vertInfoCount[4]` ), weights in the `vertsBlend`
  stream ( first bone, then bone + weight / 65536 pairs ), no rigid lists.
* Bones: roots first, every parent in front of its children ( reordered when the file is not ); local rotation as
  four s16, local translation, model space bind pose with `transWeight` 2. `partClassification` ( hit locations ) is 0
  unless given: `hitloc:<bone>=<class>` / `ModelOptions::partClassification`, or the comment line
  `//#PARTCLASS <bone index> <class>` that this library's own extractor writes ( the classic format has no place for
  it, and it is not a function of the bone name: `j_spine4` is 4 in 152 shipped models and 5 in 279 ).
* Bounds: model box and radius ( largest distance of a LOD 0 vertex from the origin ); per bone ( `XBoneInfo` ) the
  box of the bone's vertices in the bone's space, grown to hold the bone's origin, and its squared half diagonal.
* LODs: distance, surface range, the LOD's own index; `partBits` ( bit 31 - n of word n / 32 for bone n ) in the
  surfaces and the `XModelSurfs`. The `XModelSurfs` of a LOD is nested in the model ( `<model>_lod<n>` ).
* Collision ( `collision=<lod>` ): one `XModelCollSurf` per surface of that LOD and bone, in model space, with the
  48 byte triangle records: plane ( outward normal, distance ), and the two vectors with `dot( svec.xyz, p ) - svec.w`
  running 0 .. 1 along the triangle's edges. Surface flags: the material's surface type number << 20.
* Collision trees ( `XSurfaceCollisionTree` ): every rigid list of LOD 0 of a model with collision gets one - the
  triangles of the list in a tree of boxes, through which the game projects a bullet mark onto the model. In the
  shipped zones every model with collision surfaces has a tree on every rigid list of LOD 0 and nowhere else
  ( `codol_asset_probe trees <zone> <exe> *`: mp_rust 143 trees, mp_highrise 482, common_mp 61, no exception ), and
  without them the client crashes when a bullet hits the model ( seen with the test crate on a `codol_bsp` map:
  access violation near 0x642E59 right at the shot ). The rules, read off the shipped trees with the same probe and
  kept by every one of them: tree space is `( position + trans ) * scale` = 0 .. 65535 over the box of the list's
  triangles; a node whose child count has bit 15 is a leaf node, child begin its first entry in the leaf table and
  the low bits the number of entries ( 1 .. 16 ); an entry is a triangle of the surface ( counted from the surface's
  first, not the list's ), with bit 15 that one and the next; any other node has 2 .. 9 children that lie together
  behind it; node boxes hold their triangles. `build_collision_tree` halves a node of more than 16 triangles along
  the longest axis of their centres ( two children, one triangle per entry ). With it the crate takes bullets in the
  client: wood splinters and marks.
* Technique sets: a lit set `<..>_sm_<..>` copied from the install brings its sibling `<..>_hsm_<..>` ( the set the
  game draws with when it shadows with hardware shadow maps, found by name ); without it a material gets no
  dynamic sun shadow ( `codol_bsp/README.md`, Materials ).

## Animations

`XAnimParts` ( asset type 2 ) has the IW4 layout in 1.1.1.13 ( 0x58 bytes; `c1_layouts.hpp` ). One `xanim_export` per
animation ( `ANIMATION`, `VERSION 3`, as the CoD4 / MW2 exporters write it: parts, and per frame and part the offset
and the three axes of the bone **in model space**; note tracks ). The format has no bone hierarchy: like the
converter of the mod tools, the compiler takes it from the model the animation is for - `model=<file.xmodel_export>`.

What the shipped animations are ( `codol_asset_verify anim-survey` / `anim-rules`: 5698 distinct animations of the
install, 319169 animated rotation tracks, 90094 animated translation tracks ), and what the compiler therefore does:

* `numframes` is the number of the last frame, `frequency` = `framerate / numframes` ( 5698 of 5698; 0 for the one frame
  poses ). Framerate 30 ( 5478 ), 24, 16, 12, 60.
* `flags`: 1 looping, 2 delta, 4 delta 3d ( 0: 2700, 2: 1965, 3: 714, 1: 305, 4: 13, 5: 1 ).
* `assetType`: 1 for everything named `viewmodel_*` / `pt_*` ( 2784 ), 2 for characters, vehicles and props
  ( 2654 ), 6 for the additive ones ( `*_aim_2`, `pain_add_*`; 189 ), 7 ( 39 ), 0 ( 32 ). The compiler writes 2
  unless told ( `assettype=` ).
* Every part has a rotation and a translation relative to its parent. The bone list ( script strings ) is ordered by
  how the rotation is stored - none ( identity ), key frames about z only ( two s16: z, w ), key frames ( four
  s16 ), constant about z, constant - and inside a class it is **not** alphabetical: the compiler keeps the order
  of the source. Translations name their bone, ascending inside each class ( 544529 of 544529 ): key frames with 8
  bit steps, with 16 bit steps, constant, none.
* A rotation is `round( q * 32767 )` of the unit quaternion ( length 32767 +- 1.5 in 9985600 stored values ). It is
  "about z" when x and y are 0 in every frame after rounding ( no shipped 3d track has x = y = 0 throughout, 16147
  constant ones have | x |, | y | <= 2 ), constant when all rounded frames are equal ( 67111 animated tracks move by
  one unit only ), none when it is the identity.
* Neighbouring keys of a track are on the same side of the sphere ( 319169 of 319169 ). The first key has w >= 0 in
  94 % of the tracks; the compiler writes w >= 0 and keeps every further frame on the side of the one before. The
  remaining 6 % are not explained ( the sign of `conj( parent ) * child` of the converted matrices fits fewer: 74 % ).
* Key frames: a table of frame numbers and one value per key; always frame 0 and the last frame. Tables are u8 in
  `dataByte` for animations of up to 256 frames ( 386601 tables ); else u16 in `dataShort` up to 64 entries
  ( 11088 ), and above that in `indices` with a coarse table - every 256th entry and the last - in `dataShort`
  ( 11574 of 11574 ).
* The shipped tracks keep 36 % of their frames. An interior key is rarely nearer than 16 units ( rotation ) /
  0.016 ( translation ) to what its neighbours interpolate to, so the original converter drops frames inside a
  tolerance of about that size. The compiler does the same with `rottol=` ( default 16, in s16 units: a turn of at
  most 0.11 degrees ) and `transtol=` ( default 0.016 ): starting at a key it takes the longest run whose frames
  the game's interpolation ( straight line between the two keys, brought back to unit length ) gives within the
  tolerance, never less than one stored step. Compiled again with these defaults the shipped animations come out
  with 98.6 % of their keys.
* An animated translation is stored in the box of **all** its frames ( `mins`, `size` per step ): 39 % of the
  shipped 8 bit axes have no key at the low end of their box. 8 bit steps when the diagonal of the box is
  below 12.75 ( 255 steps of 0.05; largest shipped 8 bit box 12.7497, smallest 16 bit box 12.7531 ), else 16 bit.
* A translation without a track uses the model's own ( bind ) translation in the game. 45 % of the shipped
  translations have none, and no shipped constant translation is 0 0 0: the converter compares with the model.
  The compiler gives a part no translation track when it stays at the bind translation of the skeleton
  ( `model=` ) in every frame, a constant one when it stays within 0.0001 of its first frame.
* Delta animations ( the movement of a whole character: flags 2 / 4 ) do not list the root part ( `tag_origin`: 0
  of 2693 ); its movement is the delta part - a translation track ( own box, 8 or 16 bit, or one constant value )
  and a rotation about z ( delta ) or a full rotation ( delta 3d ) - stored as it is, not relative to the first
  frame ( 606 of 1889 delta translations do not start at 0 ). A root that does not move leaves the delta part empty
  ( 533 ).
* Notifies: script string and time `( frame - first frame ) / numframes`, ascending; the last one is always `end`
  at 1 ( 5698 of 5698 ) - the compiler adds it.

### Source and options

```
--xanim <name>=<file.xanim_export>[,model=<file.xmodel_export>][,type=relative|delta|delta3d][,loop]
        [,deltapart=<part>][,assettype=<n>][,rottol=<s16 units>][,transtol=<units>]
```

* `model=`: the skeleton ( any LOD's export of the model ). Parts are matched to its bones by name; a part the model
  does not have is taken as a root, and the tool says so. Bones of the model the file does not list stay in their
  bind pose on their parent. Without `model=` every part is a root: right for a one bone model ( a `script_model`
  that moves and turns as a whole ) and wrong for anything with a hierarchy - the tool says so.
* `type=delta` / `delta3d`: the movement of `tag_origin` ( `deltapart=` ) becomes the delta part; `loop`: flag 1.
* A part a frame does not list keeps its pose of the frame before. `SCALE` lines are read and ignored. A note name
  may contain quotes ( shipped: `anim_movement = "run"` ): it runs to the last quote of the line.
* Two comment lines of this library's own, for the files `codol_asset_verify anim-extract` writes: `//#LOCAL` -
  the poses are relative to the parent already - and `//#NOTRANS <part>` - the part has no translation track. They
  put the stored form of a shipped animation into text without needing its model.

```
> python tests/make_anim_sources.py src
> codol_assetc --install <game> --out codol_test_anims.ff --xanim codol_test_spin=src/xanim_export/codol_test_spin.xanim_export
    --xanim codol_test_lid=src/xanim_export/codol_test_lid.xanim_export,model=src/xmodel_export/codol_test_lidbox.xmodel_export
xanim codol_test_spin: 1 parts, 61 frames at 30 / s, 2 notifies, 144 bytes of tracks
xanim codol_test_lid: 2 parts, 41 frames at 30 / s, 3 notifies, 292 bytes of tracks
> codol_assetc --verify codol_test_anims.ff --install <game>
  xanim     codol_test_spin     1 parts, 61 frames at 30 / s, flags 0, tracks decode, notifies: codol_half@0.500 end@1.000
  xanim     codol_test_lid      2 parts, 41 frames at 30 / s, flags 0, tracks decode, notifies: lid_start@0.000 lid_open@1.000 end@1.000
```

### What a map needs besides the animation

Measured on the shipped zones and on the dedicated server ( below ):

* **the animtree**: `animtrees/<tree>.atr`, one animation name per line - what `#using_animtree( "<tree>" )` and
  `%<anim>` of a script are compiled against. The shipped ones are rawfiles of `common_mp` ( `animated_props.atr`,
  `script_model.atr`, `destructibles.atr` .. ). Add yours to the map zone: `--rawfile animtrees/my_tree.atr=my_tree.atr`.
  The dedicated server takes an animtree from the mounted zones and from its script folders ( `animtrees/` next to
  `maps/` ) only when no zone has it; names a script folder adds to a tree of a zone are appended.
* **the map's animlist**: the rawfile `animlists/<map>.animlist` of the map zone - `=<tree>` followed by the
  animations the map adds to that tree, one per line ( `mp_crash_tropical`: `=destructibles`, six fan and lamp
  animations, `=animated_props`, nine palms and clothes lines; `mp_rust`: empty ). The client builds its animtrees
  from these lists, not from script. Without the entry the server refuses `setAnim` ( "animtree has no 4-bit client
  tree id" ). `--rawfile animlists/<map>.animlist=my.animlist` replaces the map's list: keep what was in it.
* **script**: `getAnimLength( %anim )`, `getNotetrackTimes( %anim, "note" )`; on a `script_model` with a model
  `ent setAnim( %anim, 1, 0, 1 )` ( the multiplayer VM has no `useAnimTree`: the server binds the entity to the tree
  of the first animation it is given ), or `precacheMpAnim( "anim" )` before `_load::main()` and
  `ent scriptModelPlayAnim( "anim" )` as `maps/mp/_animatedmodels.gsc` does it ( that one starts at a random phase:
  for swaying props ).

`tests/asset_probe.ps1` writes such a level script for the server harness of `codol_bsp`.

### Verification of the animations

`codol_asset_verify anim-roundtrip <install> <C1.exe> [every] [zone | -] [rottol] [transtol]`: every distinct shipped
animation -> `decode_anim` ( key tracks ) -> `extract_anim` -> `xanim_export` text ( every frame, the game's own
interpolation between the stored keys; parent relative, `//#LOCAL` ) -> parsed -> `compile_anim` -> compared with the
shipped `XAnimParts`. 5698 animations, none fails to compile. "Within the limits": in every frame every rotation
within `4 * ( tolerance + 1.5 ) / 32767` radians ( 0.0105 degrees without a tolerance ) and every translation within
the tolerance plus one and a half steps of its box.

| field | without tolerance | default tolerances ( 16 units, 0.016 ) |
|---|---|---|
| header: frames, flags, framerate, frequency, asset type, counts | 5698 / 5698 | 5698 / 5698 |
| bone names and their order | 5694, 99.93 % | 5694 |
| storage class of every rotation and translation | 5693, 99.91 % | 5693 |
| notifies ( names, times ) | 5698 / 5698 | 5698 / 5698 |
| delta part: what it has, 8 / 16 bit, constant or keys | 5698 / 5698 | 5698 / 5698 |
| every part in every frame within the limits | 5694, 99.93 % | 5694 |
| **all of the above** | **5693 / 5698, 99.91 %** | 5693 / 5698 |
| largest difference of a rotation in the animations that pass | 0.0075 degrees | 0.1085 degrees |
| keys written / keys shipped | 101.6 % | 98.6 % |
| frame tables of every track identical | 1446, 25.4 % | 968 |
| data streams byte identical | 141, 2.5 % | 124 |

The five that differ: four in the order of the bones, one in a class. The install has five "animated" rotation
tracks whose keys are all equal; the compiler stores such a track as a constant, which moves the bone to another
class and another place in the list. Tables and streams are not expected to be identical: the source of this round trip is
the decoded animation, not the artist's file, so the box of a translation and the frames that were dropped are
found again only where nothing was lost. The largest translation difference ( 1.83 ) is in 16 bit boxes of tens of
thousands of units ( the helicopters and characters of the co-op intro ), where one step is about a unit.

`anim-model-roundtrip <install> <C1.exe> [every]`: the part the plain round trip does not reach - model space ->
parent relative, and "no track when the translation stays at the bind translation". The animation is posed on the
skeleton of the shipped models that have its parts ( 1242 distinct skeletons; a body, then the models that hang on
it ), written **in model space as an exporter writes it**, and compiled with that skeleton. Every third animation:
1897 with parts, 172199 of 187863 parts found on a skeleton, 600 animations with all parts on one.

| of those 600; 2 do not compile on the skeleton found for them ( not examined ) | |
|---|---|
| bone names and order, rotation class of every part | 596 / 598 |
| every part in every frame within the limits ( one step more for the six decimals of the matrices ) | 597 / 598 |
| translations without a track, found again as "stays at the bind translation" | 18598 / 18674, 99.6 % |
| shipped constant translations that equal the bind translation of that skeleton ( the compiler gives them no track ) | 3990 / 9243 |
| rotation and translation class of every part | 466 / 598, 77.9 % |

So the conversion from model space is right, and the "none" rule reproduces the shipped tracks that have none; but
43 % of the shipped constant translations sit at the bind translation of the skeleton found here and have a track
all the same ( whole hands at a time, in particular animations: `j_thumb_le_1`, `j_pinky_le_2` .. ). Either those
animations were converted with a model whose bones differ, or the original converter has a condition this one does
not know. On the model given the pose is the same either way; on another model with other proportions a constant
track forces the animation's bone length where no track keeps the model's.

`anim-zone <install> <C1.exe> <every> <out.ff>`: every fifth shipped animation compiled from its export text, all
1140 linked into one zone ( 24.9 MB; 62 with 256 frames or more, 53 with tables in `indices`, 546 delta, 72 with a
constant delta track, 3 delta 3d ), the zone run through the game's loaders and read back: 1140 / 1140 are what was
compiled, stream for stream. `anim-extract <zone.ff> <C1.exe> <name> <dir>` writes a shipped animation as an export
to start from, `anim-compare` compares two animations of two zones.

Unit tests ( `tests/anim_tests.hpp`, 62 checks ): the text reader ( parts a frame leaves out, note names with quotes,
errors ), classes and their order, 8 / 16 bit boxes, key reduction with and without tolerance, one frame poses, 400
frames with long tables, a three bone skeleton in model space with a walking and turning root ( delta, empty delta,
delta 3d ), back to text both ways, two animations through the linker and the loaders.

### Animations: not done, taken on trust

* **No animation of this compiler has been seen moving.** The server evaluates them ( below ). In the client
  ( `codol_bsp/README.md`, "Fourth round" ) a crate with `codol_test_spin` and the lid box with `codol_test_lid`,
  both started with `scriptModelPlayAnim` from the level script of a packaged map, are drawn where they were
  spawned and **do not move** - and neither do the hanging clothes of shipped mp_crash_tropical, which the game's
  own script animates the same way: on the test server ( CoD4x_Server `codol_dedrun` ) script model animations do
  not reach the client. So the client run says nothing about the animations themselves.
* The tolerance of the original converter is an estimate from the shipped keys; its criterion is not known.
* The sign of 6 % of the shipped rotations ( first key with w < 0 ) is not reproduced; it matters only where the game
  blends two animations without looking at the side, which was not examined.
* `assetType` is carried, not understood; additive animations ( type 6 ) are not made: the compiler stores the pose it
  is given, not a difference to a base pose.
* Constant translations at the bind translation ( above ). `staticTranslationEpsilon` ( 0.0001 ) is a choice.
* The `randomDataInt` stream is empty in every shipped animation and is never written. `isDefault` 0.
* The editor does not scan `xanim_export` files yet.

## Sounds

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
* streamed files found under `sound/` are packed into the `--iwd` archive as `sound/<file>`. They are not
  converted: the shipped ones are `.mp3` and a few `.wav`. **In the client a project's streamed sound does not
  play yet** ( `codol_bsp/README.md`, "Fifth round" ): the client only opens archives named like its own
  ( `iw_sound_NN.iwd` .. ) - a plain `zz_*.iwd` is never opened, the alias stays silent; an archive with such a
  name has to be encrypted, which `--iwd` now does by itself ( below ), and the client opens it - and then ends
  when the alias plays. Loaded sounds play.

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
streamed files in `main/`, and for subtitles the localized string the `subtitle` column names. `tests/asset_probe.ps1`
( and `tests/sound_probe.ps1`, sounds only ) writes a level script with exactly the calls above for the server
harness of `codol_bsp`.

### Verification of the sounds

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

### Sounds: not done, taken on trust

* **Nothing was heard.** The client was run with a map whose level script plays `codol_test_hum` as a loop sound
  and `codol_test_beep` every three seconds on script models, and which places `codol_test_hum` with a
  `script_sound` entity ( `codol_bsp/README.md`, "Fourth round" ): the map loads, the client's log has no "Missing
  soundalias" and no load error - but the test machine has no audio, so that says the aliases are found, not that
  they sound right. The dedicated server has no sound system; it does look an alias up when a script plays it ( "On
  a dedicated server" below: the new aliases are found, a name no zone has is a script error ). That Miles accepts
  a `LoadedSound` made here rests on the fields and data being byte identical to the shipped ones for the same
  wave.
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

### Verification of the effects

All numbers from `codol_asset_verify` on the 1.1.1.13 install of this machine ( x64 Release ). Animations, sounds
and effects have their numbers in their own sections above.

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

### Effects: limits, not verified

* **Seen in the game** ( `codol_bsp/README.md`, "Fourth round"; `build-editor\bsptest\shots\c4fx_fx_sheet.png` ):
  `codol_test/spark_burst`, carried into a map zone and placed with a `script_fx` entity ( `delay 1.5` ), is drawn
  by the client - the looping smoke element's puffs appear and rise in six frames over three seconds. The sparks
  and the light of the one shot elements were not caught in a frame. The client makes placed effects itself from
  the text of `maps/mp/<map>_fx.gsc` and `maps/createfx/<map>_fx.gsc`, which have to be exactly as the game's
  createfx tool writes them ( `codol_bsp` does ); `playfx` from script was not tried in the client. On the
  dedicated server `loadfx` + `playfx` of a compiled effect run without a script error, but that server gives every
  name an id without looking at the asset ( "On a dedicated server" below ).
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
* In the editor an effect is a file `mod\<project>\fx\<folder>\<name>.efx`; emitted effects are read from that
  `fx\` folder ( `docs/CODOL.md`, "Your own assets" ).

## Linking

How shipped zones refer to assets ( `codol_asset_probe graph` ), and what `AssetLinker` does:

* An asset pointer inside an asset is `0xFFFFFFFF` / `0xFFFFFFFE` followed by the asset itself ( nested ), or an
  alias: to the header slot of a top level asset in the zone's asset table, or to the pointer field through which the
  asset was nested the first time. The linker makes every new asset a top level asset and points at its slot.
* An asset of another zone is a nested header that holds only a name starting with `,`: `ui_mp` has 2434 materials
  and no technique set, its first material nests a `MaterialTechniqueSet` named `,2d`. The linker writes such a
  reference for a technique set that an always loaded zone defines ( `code_post_gfx_mp`, `common_mp` .. : e.g.
  `mc_l_sm_r0c0n0s0`, `2d` ), for images and for materials that are not part of the zone.
* Technique sets that only map zones define - all `wc_*` - cannot be referenced: which map is loaded is not known.
  Every map zone carries its own copies and so does the output: the technique set is copied from the smallest zone
  that has it, together with the technique sets it shares techniques, passes and shaders with
  ( `ZoneEditor::add_asset_closure` ). The shaders stay what they are there ( mostly `,name` references to the
  common zones ). With `--base <map>.ff` the map's own technique sets are used.
* `$identitynormalmap`, `$white`, `$black`, `$gray`: map zones carry their own 1 x 1 copies, so does the output.
* Order: an asset comes after everything it points at.
* The zone is written by running the game's loaders over the asset graph ( `codol_zone` ), so block, alignment and
  order of every piece are the game's. A zone the loaders do not accept is not written.

**From one zone into another** ( `AssetLinker::carry`, `codol_assetc --carry <zone.ff>[,<type> ..]` ): every top
level asset of the given types of another fast file is copied as it is, in the order of that file, with the assets
it points at ( `ZoneEditor::add_asset_closure` ) - a model with its materials, images and technique sets, an alias
list with its loaded sounds and curve, an effect with the materials it draws. An asset whose type and name the zone
already has stays as it is; one that cannot be copied is reported and the others still go in. This is how the
assets of an editor project get into the zone of a map ( `codol_bsp/tools/package_map.ps1` ):

```
> codol_assetc --install <game> --base mp_mymap.ff --out mp_mymap.ff --carry mod\mp_mymap\mp_mymap_assets.ff,xanim,sndcurve,loaded_sound,sound,fx,xmodel
    --rawfile animtrees/mp_mymap.atr=.. --rawfile animlists/mp_mymap.animlist=..
carried from mp_mymap_assets.ff: 2 xanim, 1 xmodel, 4 sound, 1 sndcurve, 3 loaded_sound, 1 fx ( and 4 assets they point at )
```

The unit test carries the animations and the model of the test zone into a new zone and reads them back; the zone
of the editor run below ( 33 assets ) loads with the game's loaders and writes back identical.

## Verification

All numbers from `codol_asset_verify` on the 1.1.1.13 install of this machine ( x64 Release ).

### Images

`image-roundtrip`: the top level of a shipped image is the source; it goes through the compiler ( own mips, own
encoder, the shipped file's format ), `write_iwi`, and back through `codol_render`'s IWI reader. "vs source": the
decoded level against the uncompressed level the encoder was given. "vs shipped": against the same level of the
shipped file - both are lossy views of one image, and from level 1 on the shipped file was made from the artist's
original with another filter. Sample of 40 per use ( at least 256 x 256, every n-th of the list ), PSNR in dB:

| use | images | level 0 vs source: mean, min | level 1 vs source | level 1 vs shipped | level 3 vs source | level 3 vs shipped |
|---|---|---|---|---|---|---|
| colour ( R G B, + A for DXT3 / 5 ) | 40 of 6126 | 50.50, 44.14 | 39.84, 33.38 | 34.74, 25.95 | 39.38, 29.22 | 32.25, 20.68 |
| specular ( R G B A ) | 40 of 4273 | 50.14, 44.45 | 40.58, 34.44 | 34.93, 26.87 | 39.73, 33.24 | 32.32, 26.34 |
| normal ( the stored x and y: A, G ) | 40 of 3897 | 51.31, 45.15 | 43.85, 33.06 | 36.47, 27.30 | 45.74, 37.68 | 36.68, 24.35 |

Level 0 is a shipped DXT image compressed again, so the number says the encoder finds the block it was given. From
an uncompressed source ( shipped RGBA32 images, encoded as DXT5 ): `loadscreen_mp_vacant` 1280 x 720 40.3 dB,
`ap_death_overlay` 40.1 dB, `small_box_lightfx` 45.0 dB. The IWI header, level count and the four
`fileSizeForPicmip` values equal the shipped file's for 111 of the 120 ( the others have a flag the compiler does not
set by default: legacy normals `0x8`, no picmip ).

PNG out is read by an independent decoder ( Python `zlib`, chunk CRCs checked ); PNG / TGA / DDS in are covered by
the unit tests and by the end to end run below ( one source of each ).

### Materials

`material-roundtrip`: every distinct shipped material -> `extract_material` -> definition text -> parsed ->
`compile_material` -> compared with the shipped `Material`. 16968 materials; 20 are water materials ( semantic 11,
no image ) and cannot be compiled.

A. From its own definition. The definition carries the technique set, the state of the material's own slot, sort
key, game flags, surface type, atlas, textures with sampler state, constants; the compiler derives the rest.

| field | equal |
|---|---|
| technique set, gameFlags, sortKey, atlas + drawSurf, surfaceTypeBits ( carried ) | 16948 / 16948 |
| `stateBitsEntry[48]` ( derived ) | 16948 / 16948 |
| state bits table ( derived ) | 16558 / 16948, 97.70 % |
| `stateFlags` ( derived ) | 16948 / 16948 |
| `cameraRegion` ( derived from the sort key ) | 16948 / 16948 |
| texture table: order, hash, name characters, sampler state, semantic | 16948 / 16948 |
| texture images | 16948 / 16948 |
| constant table | 16947 / 16948 |
| all 76 non pointer bytes of the header | 16557 / 16948, 97.69 % |
| whole material | 16557 / 16948, 97.69 % |

The 2.3 % are materials whose technique set has several shipped states that agree in the material's own slot and
differ in another slot; the definition has nothing that tells them apart beyond game flags and sort key.

B. The same, but the library may not use the material's own state unless another shipped material has it ( leave
one out ): entries + table + flags equal for 16311 / 16948 ( 96.24 % ). Of the 316 materials whose state no other
material has, deriving it from the nearest state gives the shipped one for 45 ( 14 % ) - **a state combination no
shipped material of the technique set has is a guess**; `codol_assetc` says "state derived" then.

C. Sort key, game flags, camera region, sampler states and semantics not given - what the compiler picks:

| field | equal to shipped |
|---|---|
| sortKey | 15818 / 16948, 93.3 % |
| gameFlags | 16168 / 16948, 95.4 % |
| cameraRegion | 16634 / 16948, 98.1 % |
| sampler state, per texture | 41021 / 42206, 97.2 % |
| semantic, per texture | 42203 / 42206 |

### Models

`model-roundtrip`: every distinct shipped model whose surfaces are in its zone -> `extract_model_lod` per LOD ->
`xmodel_export` text -> parsed -> `compile_model` ( LOD distances and the collision LOD as shipped ) -> compared
with the shipped `XModel`. 6389 models ( 6635 more have their surfaces in an `.iwd` or another zone ); none
failed to compile.

| field | equal |
|---|---|
| bone names, parents and root count, bind pose | 6389 / 6389 |
| bone local rotations ( s16, +- 10 ) / translations | 6386 / 6377 |
| LOD count, LOD distances, surfaces per LOD | 6389 / 6389 |
| surface headers ( counts, deformed, list count, base indices, `vertInfoCount` ) | 6304, 98.67 % |
| vertex positions, bit exact | 6317, 98.87 % |
| vertex uvs, bit exact | 6317, 98.87 % |
| vertex colours | 6382, 99.89 % |
| vertex normals, within 0.02 | 6317, 98.87 % |
| triangle indices | 6311, 98.78 % |
| rigid vertex lists | 6311, 98.78 % |
| blend weight streams ( weights +- 1 of 65536 ) | 6389 / 6389 |
| part bits | 6352, 99.42 % |
| model bounds / radius | 6389 / 6381 |
| bone info | 6172, 96.60 % |
| collision LOD, contents | 6389 / 6389 |
| collision surface count, collision triangle count ( 5310 models with collision ) | 5310 / 5310 |
| **all of the above** | **6087 / 6389, 95.27 %** |

The models that differ: skinned characters whose `XBoneInfo` is that of the shared skeleton rather than of the
model's own vertices, rigid surfaces of some characters whose triangles are grouped another way than "by the bone
of the first corner", and the few with their own radius rule ( `tag_origin` .. ). Tangents are derived data and are
not expected to be equal: 97.5 % of 10.5 million rebuilt tangents lie within 15 degrees of the shipped one, the
binormal sign is equal for 99.3 %. A model whose vertex order differs fails positions, uvs, normals and indices
together ( the comparison is by index ).

`model-extract` + `codol_assetc` + `model-compare`: three shipped models written out as export files, compiled and
linked into a zone, that zone loaded and its models compared with the shipped ones through `codol_zone`'s reader -
the whole path including the linker and the game's loaders ( 22 checks each ):

| model | result |
|---|---|
| `com_barrel_biohazard_dirt` ( static, 4 LODs, 8 surfaces, 748 vertices, collision LOD 1 ) | the same in every check; 272 collision records equal within 1.9e-6 |
| `vehicle_opfor_truck` ( 26 bones, rigid, 18 surfaces, 12384 vertices, collision LOD 2, contents 17 ) | the same in every check; 1302 of the 1313 collision records are in the shipped surface, in another order |
| `german_sheperd_dog` ( 56 bones, skinned, 13 surfaces, 12901 vertices ) | the same but for `XBoneInfo` ( bones, bind pose, weights, hit location classes, vertices, indices all equal ) |

Export files of **CODOL-AssetExplorer** ( `modtools/model_export/codol` ) against the shipped model ( `model-file` ):
`afch_bigrock_01` and `codo_blastdoor` compile to the shipped data in every field; `accessories_potato_bag` in every
field but the vertex positions, which that file has with six decimals ( and with them the bone box ).

### Drawn by codol_view

`codol_viewtool render --zone-dirs <out dir> --extra-zones codol_test_assets --box .. --model ..` in mp_rust, the
zone of the example above ( sources: `tests/make_test_sources.py` - a crate texture set as PNG, a TGA through a GDT
material, a DDS, a two LOD crate ):

* three boxes with the new world materials - lit with normal and specular map, the GDT material with the map's
  stand-in normal map, the unlit one - and two instances of the new crate model, lit by the map, the texture upright
  ( the red arrow of the colour map points along +u on every side ), outside faces drawn: the winding is right;
* `codol_viewtool edittest --material wc/codol_test_wall --model codol_test_crate`: passes ( `add_model`,
  `create_mesh` with a world and a model material "drawn as game_material", `material_thumbnail` ); the thumbnail of
  the new material is the crate texture;
* the shipped barrel and truck next to the ones recompiled from their export files: no visible difference.

### On a dedicated server

CoD4x_Server `codol_dedrun`, dedicated, loopback, port 17756, no client, on a link based copy of the install
( `tests/make_shadow.ps1`, `tests/run_server_check.ps1` ). The server does not run the game's loaders; it has its own
decoder ( `src/codol/assets` ), which validates every asset of a zone it mounts.

* `+codol_loadzones C1.exe mp_rust.ff codol_test_assets.ff codol_test_recompiled.ff`:
  `CoDOL archive set mounted with loader blocks: 3 archives`.
* `+codol_checkmodel codol_test_crate`: `skeleton 1, geometry 1, materials 1, mesh 1, core 1, complete 1`,
  `material 0: source 1, mc/codol_test_crate, native 1`. The recompiled barrel and truck: the same line as the shipped
  models, the same materials found; the shipped barrel also has `physics 0: .. barrel_metal`, the recompiled one none.
* The crate, its images and materials added to the map zone itself ( `--base mp_rust.ff` ), that file in the copy,
  `+map mp_rust`: `CoDOL native map archives mounted: 7 ( mp_rust, mp )`, `CoDOL native collision installed:
  maps/mp/mp_rust.d3dbsp, 758 leaves`, the game initialises, `codol_checkmodel codol_test_crate` complete.

**Animations, sounds, effects** ( loopback, port 17755, `codol_bsp/tests/harness` with a base of its own; the level
script from `tests/asset_probe.ps1`, put in front of the server's own scripts with `-NativeOverlay` ). One build of
everything into a copy of the map zone:

```
codol_assetc --install <game> --base mp_rust.ff --out <base>\client_shipRetail\chinese\mp_rust.ff --iwd zz_codol_test_sounds.iwd
    --image .. --material codol_test.mtl --xmodel codol_test_lidbox=xmodel_export\codol_test_lidbox.xmodel_export
    --xanim codol_test_spin=xanim_export\codol_test_spin.xanim_export
    --xanim codol_test_lid=xanim_export\codol_test_lid.xanim_export,model=xmodel_export\codol_test_lidbox.xmodel_export
    --soundalias soundaliases\codol_test_sounds.csv --fx codol_test/spark_burst=tests\fx\codol_test\spark_burst.efx
    --rawfile animtrees/codol_test.atr=animtrees\codol_test.atr --rawfile animlists/mp_rust.animlist=animlists\codol_test.animlist
```

184 assets, the loaders take it, read -> write identical. `+map mp_rust` with the level script:

| step of the script | what the server answered |
|---|---|
| the zone's rawfiles | `CoDOL mounted animtree: animtrees/codol_test.atr`; `CoDOL animlists: mp_rust.animlist loaded, 2 entries` |
| `getAnimLength( %codol_test_spin )`, `( %codol_test_lid )` | 2, 1.33333 ( 60 and 40 frames at 30 / s ) |
| `getNotetrackTimes( %codol_test_spin, "codol_half" )`, `( %codol_test_lid, "lid_open" )` | one time each: 0.5, 1 |
| `ent setModel( "codol_test_lidbox" )`, `ent setAnim( %codol_test_spin, 1, 0, 1 )`, `.. %codol_test_lid ..` | `ent 184 'codol_test_lidbox' bound server tree 6 'codol_test' ( 3 entries ) / client tree 9`, both animations set and cleared |
| `precacheMpAnim` + `ent scriptModelPlayAnim( .. )` for both | ran |
| `loadfx( "codol_test/spark_burst" )`, `playfx` | id 3, ran |
| `playSound( "codol_test_beep" )`, `playLoopSound( "codol_test_hum" )`, `ambientPlay( "codol_test_wind" )` | ran |
| rcon `codol_checkxanim codol_test_spin` / `codol_test_lid` ( the server's own decoder ) | `status 1, 1 bones, 5 finite samples` / `status 1, 2 bones, 5 finite samples` |
| rcon `codol_checkmodel codol_test_lidbox` | `skeleton 1, geometry 1, materials 1, mesh 1, core 1, complete 1`, bones `tag_origin`, `j_lid` |

0 script errors in the log. What the same run fails on when something is missing shows how much each line says:
without the model `setAnim` is refused ( no server animtree ), without the animlist entry "animtree 'codol_test' has
no 4-bit client tree id", `playSound( "codol_test_missing" )` is "unknown sound alias" - so the animations, the
animlist and the sound aliases are found in the zone. **`loadfx` of a name that no zone has gives an id as well**
( 4, no warning ): for effects the run shows that the zone mounts and the script runs, nothing about the effect.

**From the editor** ( `docs/CODOL.md`, "In the built map"; loopback, port 17755, a base of its own ). A room
`mp_edassets2`; in CoDOL Radiant, unattended: `asset_new_material`, `asset_import_model codol_test_lidbox`,
`asset_import_anim` twice ( the lid with `model=codol_test_lidbox` ), `asset_import_sounds`, `asset_import_fx`,
`asset_level_script <the script of tests/asset_probe.ps1 with #using_animtree( "mp_edassets2" )>`, `map_build fast`,
`map_install <base>\client_shipRetail\chinese <overlay>`. The project zone has 16 assets, the map zone 33 ( both:
loaders accept, read -> write identical ). `+map mp_edassets2`: `animlists: mp_edassets2.animlist loaded, 2 entries`,
`mounted animtree: animtrees/mp_edassets2.atr`, the same answers as in the table above ( lengths 2 and 1.33333, notes
0.5 and 1, `bound server tree 6 'mp_edassets2' ( 3 entries ) / client tree 9`, `loadfx` id 3, every sound call ran,
`codol_checkxanim` status 1 for both, `codol_checkmodel` complete ), 0 script errors. Controls: with `playSound(
"codol_test_missing" )` in the script the server stops with "unknown sound alias"; the same package built without
`--carry` gives "xanim 'codol_test_spin' is not in any mounted zone" and a script runtime error.

## For the editor

Everything is in the library; the tool is a thin shell around it. Keep one `MaterialLibrary` ( `open` with a cache
file ) and write new assets into a zone of their own in a folder the renderer searches ( `options::zone_directories` /
`extra_zones` of `codol_view` ), or hand the file to the renderer while a map is loaded ( `map_renderer::register_zone` ).

CoDOL Radiant does exactly this ( `editor/src/assets.cpp`, `asset_windows.cpp`; `docs/CODOL.md`, "Your own assets" ):
a project folder per map, a build on a worker thread, the Assets window, "New material ..", "Import model ..", GDT,
"Import animation ..", "Sounds ..", "Effect ..". File > Build map carries the zone's animations, sounds, effects and
models into the map zone ( `--carry`, "Linking" ) with the animtrees, the animlist and the project's scripts.

**New material from image**

```cpp
SourceImage source;                          load_image(file, source);
ImageOptions io;                             // use = color; normal / specular for further maps
CompiledImage image;                         compile_image(imageName, source, io, image);

MaterialDef def;                             def.name = "wc/" + name;  def.type = "world phong";   // or "model phong", "2d" ..
def.textures.push_back({ "colorMap", imageName });
def.surfaceTypeBits = surface_type_bits("concrete");
CompiledMaterial material;                   compile_material(def, library, material);              // .notes, .stateShipped

AssetLinker linker;                          linker.begin({ install, exe, cache, base, zoneName });
linker.add_image(image);  linker.add_material(material);  linker.write(zoneDir / (zoneName + ".ff"));
// renderer: material_thumbnail( def.name ), create_mesh( .. ) with that material
```

**Import model**

```cpp
ModelOptions options;                        options.name = modelName;
ModelLodSource lod;                          load_xmodel_export(file, lod.mesh);  lod.distance = 1000;
options.lods.push_back(lod);                 // up to four; options.collisionLod = 0 for a solid prop
CompiledModel model;                         compile_model(options, model);       // model.materials(): what it needs
// materials that are not in the zone and not shipped: make them as above first
linker.add_model(model);  linker.write(..);
// renderer: add_model( modelName, origin, angles )
```

**Animation, sounds, effect** ( `run_build` of `editor/src/assets.cpp` makes these calls )

```cpp
ExportAnim source;                           load_xanim_export(file, source);
ExportModel skeleton;                        load_xmodel_export(modelFile, skeleton);
AnimOptions ao;                              ao.name = animName;  ao.skeleton = &skeleton;   // ao.type, ao.looping
CompiledAnim anim;                           compile_anim(source, ao, anim);                 // anim.notes
linker.add_anim(anim);
// plus the rawfiles animtrees/<tree>.atr and animlists/<map>.animlist ( linker.add_rawfile )

SoundBuild sounds;
build_sound_aliases(project / "soundaliases" / "my_map.csv", project, linker.editor(), sounds);   // .log, .streamedFiles
// sounds.streamedFiles ( "sound/<file>" + bytes ) go into the project's .iwd next to the streamed images

FxAtlasLibrary atlas;                        atlas.open(install, linker.key(), cache);       // once, 30 s
EditorEffect effect;                         load_efx(file, effect);
CompiledFx fx;                               compile_fx("misc/my_effect", effect, fx, fx_compile_options(linker.editor(), atlas, fx_source_root(file, "misc/my_effect")));
link_fx(linker.editor(), fx);
```

`model.materials()` names what the model needs; `linker.editor().find_asset( ASSET_TYPE_MATERIAL, name )` and
`library` say what exists. `extract_material` / `write_material_text` and `extract_model_lod` /
`write_xmodel_export` give the source of a shipped asset to start from.

## Not done, taken on trust

* **No load in the game client.** The client loads unsigned fast files only with the install's `Kernel33.dll`
  ( `codol_zone/README.md` ); that was not tried here.
* **Inline DXT images** ( the default of `--image` ) have no shipped example; the loaders take them and
  `codol_render` decodes them, the game's upload of an inline DXT `GfxImageLoadDef` is not shown. `format=rgba` is
  the shipped case; `,streamed` + `--iwd` is how every shipped material texture works, but a plain `.iwd` next to the
  game's archives is the rule of the install's own notes, not something tested here.
* Image header: byte 26 ( 1 in map zones, 0 in common zones ) is written 0; byte 27 ( 2 in 80 % of the streamed and in
  the inline material images ) 2. Their meaning is not known.
* No wavelet encoder, no volume textures. Cube maps: a six face DDS compiles and reads back through `codol_render`
  as a cube ( flags `0x10340` ); not compared with a shipped cube map, not linked into a material.
* Material states no shipped material of the technique set has are derived, with a 14 % hit rate on the shipped
  cases. Water materials are not compiled. Sort names are not read, only numbers.
* Models: collision trees have two children per node and one triangle per leaf entry ( shipped: up to 9 and
  pairs ); a rigid list that reaches beyond triangle 32768 of its surface gets none. No physics preset / collision
  map reference, `tileMode` 0 ( shipped: 0, 1, 2; rule
  unknown ), `lodRampType` 0, static model cache bytes of the LOD info 0, `memUsage` an estimate. Per collision
  surface contents other than the model's ( glass .. ) only through `ModelOptions::collisionContentsByMaterial`.
  `XBoneInfo` of skinned characters differs from shipped ( above ).
* A by-name reference to a technique set, image or material that no loaded zone defines gives the game's default
  asset; the linker does not check that a referenced image or material exists anywhere.
* Animations, sounds and effects: see their own "not done" lists above. In short: none has been seen or heard in a
  client. The editor builds them and puts them into a built map, it does not show or play them.
* Not started: signed fast files; weapons, vehicles, impact effect tables, physics presets, fonts, menus.

## Origin of the code

Everything in `codol_assets/` was written for this library. Formats and field meanings come from `codol_zone`
( layouts, loaders ), `codol_render` ( IWI, materials, state bits ), from the surveys of the shipped assets named
above, and from CODOL-AssetExplorer's exporters ( `XModelWriter.cs`: the `xmodel_export` dialect that is read back )
and readers ( `AnimationExtractor.cs`: the order in which an `XAnimParts` uses its streams; `SoundStructures.cs`,
`FxEffects.cs` ). The CoD4 mod tools' `raw/fx/*.efx` text files served as a second source for the effect compiler's
rules ( `fx-source` ); no binary of the mod tools is used. No third party source code.
