# codol_assets

Asset compiler for Call of Duty Online 1.1.1.13: the role Asset Manager + `converter.exe` + `linker_pc` play in the
CoD4 mod tools. It takes what a modder makes - images ( PNG / TGA / DDS ), material definitions ( a small text
format, or the material entries of a CoD4 `.gdt` ), models ( `xmodel_export` ) - and turns it into native C1 assets
inside a fast file: `GfxImage` ( and the IWI version 9 file of a streamed image ), `Material`, `XModel` with its
surfaces, with correct references to the assets of the game's own zones.

Inputs are source formats, outputs are native C1 data; nothing is converted to or from another game's asset format.
No third party code: PNG / TGA / DDS readers, the DXT encoder, the zip writer and everything else are this
library's own; inflate / deflate, the zone writer and the game's loaders come from `codol_zone`, the readers the
results are checked with from `codol_render`.

**Nothing here has been loaded by the game client.** What has been shown: the game's own zone loaders ( the machine
translation in `codol_zone` ) consume every zone the linker writes to the last byte; `codol_view` draws the new
images, materials and models; a dedicated server ( CoD4x_Server `codol_dedrun` ) mounts the zones, decodes the models
and starts a map whose zone carries them. See [Verification](#verification) and [Not done, taken on trust](#not-done-taken-on-trust).

## Layout

| path | what |
|---|---|
| `include/codol_assets/bitmap.hpp` | source images: `load_image` ( PNG, TGA, DDS ), `save_png`, `psnr` |
| `include/codol_assets/image.hpp` | `compile_image` ( mips, DXT ), `write_iwi`, `image_pieces`, `write_iwd`, `read_iwi_levels` |
| `include/codol_assets/material.hpp` | `MaterialDef`, `.mtl` / GDT readers, `MaterialLibrary`, `compile_material`, `extract_material` |
| `include/codol_assets/model.hpp` | `xmodel_export` reader / writer, `extract_model_lod`, `compile_model` |
| `include/codol_assets/link.hpp` | `AssetLinker`: compiled assets -> zone -> fast file |
| `tools/codol_assetc.cpp` | the command line tool |
| `tools/asset_verify.cpp`, `verify_*.cpp` | `codol_asset_verify`: round trips against the shipped assets; every number below |
| `tools/asset_probe.cpp` | `codol_asset_probe`: stream order of a shipped asset, surveys of the shipped images / materials / models |
| `tests/` | unit tests ( `ctest` ), test sources, the server check scripts |

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
builds a zone; without one that part is skipped. 141 checks, x64 and Win32.

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
  --rawfile <name>=<file>

codol_assetc --verify <zone.ff> --install <dir>
  loads the file with the game's loaders, writes it again ( must be identical ), lists images ( inline ones are
  decoded ), materials and models
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
  install ), so the archive has to be named differently, e.g. `zz_mod_images.iwd`. No encrypted archive is written.
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

## For the editor

Everything is in the library; the tool is a thin shell around it. Keep one `MaterialLibrary` ( `open` with a cache
file ) and write new assets into a zone of their own in a folder the renderer searches ( `options::zone_directories` /
`extra_zones` of `codol_view` ).

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
* Models: no `XSurfaceCollisionTree` in the rigid lists ( 44 % of the shipped static surfaces have one; it is what
  decals are projected with ), no physics preset / collision map reference, `tileMode` 0 ( shipped: 0, 1, 2; rule
  unknown ), `lodRampType` 0, static model cache bytes of the LOD info 0, `memUsage` an estimate. Per collision
  surface contents other than the model's ( glass .. ) only through `ModelOptions::collisionContentsByMaterial`.
  `XBoneInfo` of skinned characters differs from shipped ( above ).
* A by-name reference to a technique set, image or material that no loaded zone defines gives the game's default
  asset; the linker does not check that a referenced image or material exists anywhere.
* Not started: xanim, sound aliases / loaded sounds, fx, signed fast files.

## Origin of the code

Everything in `codol_assets/` was written for this library. Formats and field meanings come from `codol_zone`
( layouts, loaders ), `codol_render` ( IWI, materials, state bits ), from the surveys of the shipped assets named
above, and from CODOL-AssetExplorer's exporters ( `XModelWriter.cs`: the `xmodel_export` dialect that is read back ).
No third party source code.
