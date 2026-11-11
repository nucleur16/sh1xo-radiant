# codol_zone

Native C++ reader for Call of Duty Online 1.1.1.13 fast files ( `.ff`, version 334 ). It decrypts and inflates a
fast file, runs the game's own zone loaders over it and gives typed, read only access to the loaded assets - models,
materials, images, map geometry, entities - straight from zone memory, in CoDOL's own data structures.

Standalone: no dependency on the rest of this repository, its `deps/` submodules or its precompiled header. C++20,
MSVC, `/W4 /WX` clean for Win32 and x64, static CRT by default. The only system dependency is `bcrypt.dll`
( Windows CNG ).

## Layout

| path | what |
|---|---|
| `include/codol_zone/fastfile.hpp` | container: header, client key, decrypt + inflate ( `read_fastfile` ), `zlib_inflate` |
| `include/codol_zone/zone.hpp` | `Zone`: load, asset table, script strings, image memory access |
| `include/codol_zone/assets.hpp` | typed views: `read_model`, `read_material`, `read_image`, `read_gfx_world`, `read_clip_map`, .. |
| `include/codol_zone/c1_layouts.hpp` | packed 1.1.1.13 asset structs with compile time size / offset checks ( `src/codol/c1_layouts.hpp` forwards here ) |
| `src/` | implementation; `loaders_1_1_1_13.inc` is generated |
| `tools/codol_zonetool.cpp` | command line tool |
| `tools/transpile_cs_loaders.py` | regenerates `src/loaders_1_1_1_13.inc` |
| `tools/verify_against_oracle.py`, `tools/compare_xmodel_export.py` | verification against CODOL-AssetExplorer |
| `tests/` | unit tests ( `ctest` ) |

## Build

```
cmake -S codol_zone -B build/codol_zone_x86 -G "Visual Studio 17 2022" -A Win32
cmake --build build/codol_zone_x86 --config Release
ctest --test-dir build/codol_zone_x86 -C Release

cmake -S codol_zone -B build/codol_zone_x64 -G "Visual Studio 17 2022" -A x64
```

Options: `CODOL_ZONE_STATIC_CRT` ( ON ), `CODOL_ZONE_BUILD_TOOLS` ( ON ), `CODOL_ZONE_BUILD_TESTS` ( ON ),
`CODOL_ZONE_GAME_DIR` ( a 1.1.1.13 install; when it exists the tests also run against real zones, otherwise that
part is skipped. The environment variable of the same name overrides it; `CODOL_ZONE_TEST_ALL=1` additionally loads
every fast file of the install ).

## How it works

1. **Container** ( `fastfile.cpp` ). `IWff0100` ( signed ) or `IWffu100` ( unsigned ), version 334, a 0x15 byte
   header and a 0x100 byte RSA-OAEP( SHA-256 ) block that holds a 24 byte AES key and a 16 byte IV. The RSA-2048
   private key is a PKCS#1 DER blob inside the client executable ( `C1.exe` or `codoMP_client_shipRetail.exe`,
   file offset 0x83C248; found by scanning when it is elsewhere ). Signed files are 0x2000 byte chunks, AES-192-CTR
   ( little endian counter ) with per chunk IVs taken from a SHA-256 hash tree, every full chunk is verified.
   Unsigned files only encrypt what lies in the first 0x40000 bytes of the file. The payload is one zlib stream.
2. **Zone load** ( `zone_load.cpp` ). The client's `Load_XAsset` and the 261 functions below it exist as a machine
   translation ( `loaders_1_1_1_13.inc` ). They run against a private 32 bit address space ( `vmem.hpp` ) that holds
   the eight memory blocks of the zone and the loaders' own globals, so every asset type is traversed exactly as the
   game does it, alias references `( block << 28 | offset ) + 1` are resolved by the game's own code and the stream
   position is checked against the zone's asset table after every asset ( 1.1.1.13 stores the stream offset and
   size of each asset ). A load fails unless the stream ends exactly at the end of the zone.
3. **Image**. What stays is the image: blocks 1 - 7 as the game builds them plus a pool with one header per asset
   ( `DB_AddXAsset` ), in load order. Pointers inside the image are 32 bit image addresses ( block n starts at
   `( n + 1 ) << 28`, the pool at `0x90010000` ), never native pointers. The temp block ( 0 ) is reused for every
   asset and is not kept; inline image pixels that the game reads into it are copied to the pool.
4. **Views** ( `assets.cpp` ). `read_*` functions copy the packed header out of the image and resolve its arrays
   into spans / decoded structs.

A `Zone` is immutable after `load()` and there is no global state: any number of zones can be loaded at once, from
several threads.

## API in short

```cpp
#include "codol_zone/assets.hpp"
using namespace codol::zone;

ClientKey key;
Status s = key.load_from_exe("C1.exe");                      // RSA key of the client

Zone zone;
s = zone.open("mp_killhouse_sh.ff", key);                    // read + decrypt + inflate + load
// or: read_fastfile( path, key, bytes ) and zone.load( std::move( bytes ), "name" )

for (const XAssetEntry& e : zone.assets()) { .. }            // the zone's asset table ( type, header, stream range )
for (const AssetRecord& r : zone.all_assets()) { .. }        // every asset incl. nested ones, in load order
zaddr h = zone.find_asset(codol::c1::ASSET_TYPE_XMODEL, "com_barrel_blue_rust");

ModelView model;
if (read_model(zone, h, model))
    for (const ModelLod& lod : model.lods)
        for (const SurfaceView& surface : lod.surfs.surfaces)
        {
            std::vector<SurfaceVertex> verts = surface.vertices();   // position, normal, tangent, uv, rgba
            std::vector<std::uint16_t> tris = surface.indices();
            std::vector<VertexWeights> skin = surface.weights(zone);
        }

GfxWorldView world;  read_gfx_world(zone, first_asset(zone, codol::c1::ASSET_TYPE_GFXWORLD), world);
ClipMapView clip;    read_clip_map(zone, first_asset(zone, codol::c1::ASSET_TYPE_CLIPMAP_MP), clip);
MapEntsView ents;    read_map_ents(zone, find_map_ents(zone), ents);
```

Nothing throws: functions return `Status` ( `ok`, `message` ) or `bool`, and a view of something that is not in
the image is empty. Raw access: `zone.bytes( address, size )`, `zone.get<T>( address )`, `zone.c_string( address )`,
`zone.script_string( handle )`.

Things to know when reading assets:

* **References.** An asset pointer can lead to an asset of another fast file. Then it is a placeholder:
  `zone.reference( address )` gives type and name, the views set `isReference`. Assets whose stored name starts
  with `,` are references too ( the loader registers them, their data is elsewhere ); `asset_name()` strips the comma.
* **Streamed data.** Most images have no pixels in the zone ( `ImageView::streamed`, they are `images/<name>.iwi`
  inside the encrypted `.iwd` archives ) and many models have no surfaces in the zone ( `ModelSurfsView::external`:
  `models/<surfs name>` in an `.iwd`, or an XModelSurfs of another fast file ). Reading `.iwd` archives is not part
  of this library.
* **Script strings.** u16 script string fields ( bone names, notetracks ) hold handles into the zone's own table:
  `zone.script_string( handle )`.
* **Text encoding.** Names are ASCII; localized values are GBK in this ( Chinese ) client. The library returns raw
  bytes.
* **Vertices** of model surfaces are stored in model space ( bind pose ), also for rigid and skinned surfaces.

## codol_zonetool

```
codol_zonetool info <ff> [--exe <client exe>] [--json]     header, blocks, asset counts per type
codol_zonetool assets <ff> [--all] [--json]                type + name per asset ( --all: nested ones too )
codol_zonetool check <folder> [--all] [--json]             load every .ff, report failures
codol_zonetool decrypt <ff> --out <file>                   the inflated zone
codol_zonetool dump-ents <ff> [--out <file>]
codol_zonetool dump-rawfile <ff> [<name>] [--out <file>]   dump-rawfiles <ff> --out <folder>
codol_zonetool model <ff> <name> [--json [--geometry]]     models <ff> [--json]
codol_zonetool material <ff> <name> [--json]               image <ff> <name> [--json]
codol_zonetool map <ff> [--json]                           GfxWorld / clipMap / MapEnts / ComWorld counts
codol_zonetool index <ff>                                  JSON summary of the whole zone
codol_zonetool peek <ff> <hex address> [<bytes>]           hex dump of image memory
```

`--exe` defaults to `C1.exe` / `codoMP_client_shipRetail.exe` found next to or up to four folders above the `.ff`.

## Asset types

All 38 asset types that `Load_XAsset` handles are traversed by the translated loaders; none is skipped by size.
36 of them occur in the 146 fast files of the install ( `map_ents` only nested in a clipMap ). The other two are
`col_map_sp`, which shares the loader of `col_map_mp`, and `game_map_sp`, whose loader has therefore not run on real
data.
Types 23 ( ui_map ), 29 ( snddriverglobals ), 32 - 35 ( aitype, mptype, character, xmodelalias ) have no loader in
the game either; the game skips such an entry and so does the library ( with a warning ).

Typed views exist for: XModel ( bones, LODs, XModelSurfs, XSurface: decoded vertices, indices, rigid lists, blend
weights, material per surface ), Material, MaterialTechniqueSet ( name ), GfxImage, RawFile, StringTable,
LocalizeEntry, XAnimParts ( header, bone names, notetracks ), MapEnts, clipMap_t ( planes, materials, brushes with
sides / contents / bounds, sub models, static models ), GfxWorld ( vertices, indices, surfaces, static model
instances, brush models, cells + portals ), ComWorld. Everything else ( sounds, fx, weapons, menus, physics .. ) is
loaded and reachable through `Zone::bytes()` but has no view and no layout struct yet.

## Verification

`tools/verify_against_oracle.py` compares the tool with CODOL-AssetExplorer's `CodolSoundDumper` on a real install.
Results on the 1.1.1.13 install of this machine ( 138 fast files in `client_shipRetail/chinese`, 8 in `patch` ),
same numbers for the Win32 and the x64 build:

| check | result |
|---|---|
| inflated zone byte-identical to `CodolSoundDumper decrypt` | 28 / 28 sampled files ( 10 signed, 18 unsigned, all 8 patch files, the 12 largest; 1.6 GB of zone data ) |
| `check`: stream ends exactly at the end of the zone, every asset inside its table range | 146 / 146, 0 failures ( 103912 assets, 444305 including nested ) |
| asset table ( type, zone offset, size per asset ) equal to `CodolSoundDumper assets` | 146 / 146 files, 103912 assets |
| every rawfile of 5 zones equal to `zone-rawfiles` | 379 / 379 files |
| models against the oracle's index, all 138 zones: bones, root bones, LOD count | 28815 / 28815 |
| .. LOD 0 surfaces, vertices, triangles ( models with surfaces in the zone ) | 14353 / 14353 |
| .. material names per model, pool address of the header | 28815 / 28815 |
| animations: last frame, framerate, bone count, notetrack count | 13448 / 13448 |
| localized strings ( key and value ) | 15767 / 15767 |
| inline images: format, levels, size, pixel bytes | 739 / 739 |
| material / image pairs, material and image counts, names of sound aliases, fx, weapons, xmodels, xanims per zone | 138 / 138 zones ( 102331 pairs ) |
| `map-info` counts ( 19 per map ) for mp_killhouse_sh, zm_sumpf_sh, mp_terminal_cls | all equal |
| `list-maps`: triangles, static models, distinct models, brushes, entities, materials | 42 / 42 maps |
| MapEnts string of 4 maps found verbatim in the oracle's zone behind a header of that length | 4 / 4 ( mp_killhouse_sh: 227009 bytes, 1967 entities, 143 trigger models ) |
| decoded vertex data of 7 models against `export-models --format xmodel_export` ( `compare_xmodel_export.py` ) | bones, positions, weights, indices, normals, uvs, colours all equal |

The oracle has no name column in `assets`; asset names were compared through its index ( rows above ) and with
`CoD4x_Server/docs/zone_assets/<zone>.txt` where such a file exists ( 90 of the 146 ): the per type counts agree for
all 89 stock zones ( the 90th, patch_common_gametype_zm, is a rebuilt file that no longer has the documented asset
list ), and 71877 of 73272 unflagged rows have the same type + name. The remaining rows are names that
document's own README calls heuristic ( it does not replay the loader ); for the types they fall into ( localize,
sound, xmodel, material, weapon, xanim ) the library's names equal the oracle's in every zone.

Not verified against an independent source: the contents of asset types without a view, tangents of model
vertices, GfxWorld vertex contents ( only counts and index / vertex ranges ), brush side planes ( only counts and
that every side resolves to a plane and a material ), the `col_map_sp` / `game_map_sp` loaders ( no such zone ).

## Linking into the radiant DLL ( 32 bit, MSVC, static CRT )

* `add_subdirectory(codol_zone)` + `target_link_libraries(iw3r PRIVATE codol_zone)`; set `CODOL_ZONE_BUILD_TOOLS` /
  `CODOL_ZONE_BUILD_TESTS` to OFF there. The library is built with `/MT` ( `/MTd` in Debug ) unless
  `CODOL_ZONE_STATIC_CRT` is OFF - it must match the DLL. For premake: compile the five `.cpp` files in `src/` with
  `include/` on the include path, `/bigobj` for `zone_load.cpp`, link `bcrypt.lib`.
* The public headers only use the standard library and compile with `/std:c++20` and `/std:c++latest`. They do not
  include `windows.h` and need no precompiled header ( do not force the DLL's PCH onto the library's sources ).
* `zone_load.cpp` is one 13000 line struct ( needs `/bigobj` ); it compiles in a few seconds.
* Exceptions: the library uses C++ exceptions internally and catches them at its API, so it needs `/EHsc`.
* Memory: a loaded zone keeps roughly its inflated size ( blocks 1 - 7 + pool ). While loading, the inflated bytes
  and the image exist side by side: peak working set of the 32 bit tool is about 250 MB for the largest maps
  ( zm_sumpf_sh, mp_killhouse_sh ). Each block is one contiguous allocation ( 83 MB for block 3 of mp_killhouse_sh ),
  and so is the inflated zone while loading ( 134 MB for zm_sumpf_sh ), which matters in a fragmented 2 GB address
  space - load zones early or make the host large address aware.
* Speed ( Release ): about 0.5 s for mp_killhouse_sh ( 43 MB file, 107 MB zone ); all 138 fast files ( 4.4 GB of zone
  data ) load in 14 s with the x64 build.
* Threads: a `Zone` may be loaded on any thread ( the tests load on `std::thread`s with the default 1 MB stack ) and
  read from several threads at once.

## Origin of the code

| file | origin |
|---|---|
| `src/loaders_1_1_1_13.inc` | generated by `tools/transpile_cs_loaders.py` from `CODOL-AssetExplorer/src/Codol.FastFiles/Emulation/Generated/V1_1_1_13/GeneratedLoaders.cs` ( the owner's tool; that file is itself generated from the 1.1.1.13 client ). Syntactic conversion only. |
| `src/zone_load.cpp` | written for this library; the driver and the engine stubs follow `ZoneLoadEmulator.cs` and `LoaderNatives.cs` of the same project |
| `src/fastfile.cpp` | written for this library; format after `FastFileReader.cs`, `FastFileHeader.cs`, `FastFileKeyProvider.cs`, `LibTomCryptCtr.cs` |
| `src/assets.cpp` | written for this library; field meaning after `Codol.Assets` ( `ModelExtractor.cs`, `MeshExtractor.cs`, `SkeletonExtractor.cs`, `SkinningExtractor.cs`, `MaterialExtractor.cs`, `MapExtractor.cs`, `BrushGeometry.cs`, `AnimationExtractor.cs`, `RawFileCollector.cs` ) |
| `include/codol_zone/c1_layouts.hpp` | moved from `src/codol/c1_layouts.hpp` of this repository ( `Bounds` corrected to centre + half extents ) |
| `src/crypto_win.cpp` | written for this library on top of Windows CNG ( `bcrypt.h` ): RSA-OAEP, AES, SHA-256 |
| `src/inflate.cpp` | written for this library from RFC 1950 / 1951 |

No third party source code is included: no zlib, no mbedtls, no libtomcrypt. CODOL-AssetExplorer carries no licence
file or licence headers; it is the repository owner's own project.

The native server ( `CoD4x_Server`, `src/codol/assets/` ) was named as a second source of truth. Its `origin/main`
could not be read in the session that wrote this library ( git access to that repository was refused by the
session's sandbox ), so nothing here is derived from or checked against that code; only the files of
`docs/zone_assets/` in its working tree were used, for the name comparison above.
