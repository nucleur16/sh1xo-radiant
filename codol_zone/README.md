# codol_zone

Native C++ reader and writer for Call of Duty Online 1.1.1.13 fast files ( `.ff`, version 334 ). It decrypts and
inflates a fast file, runs the game's own zone loaders over it and gives typed, read only access to the loaded
assets - models, materials, images, map geometry, entities - straight from zone memory, in CoDOL's own data
structures. The write side ( `ZoneEditor`, `encode_fastfile`, the `codol_linker` tool ) turns a zone into an asset
graph, edits it and builds a fast file again: see [Writing zones](#writing-zones).

Standalone: no dependency on the rest of this repository, its `deps/` submodules or its precompiled header. C++20,
MSVC, `/W4 /WX` clean for Win32 and x64, static CRT by default. The only system dependency is `bcrypt.dll`
( Windows CNG ).

## Layout

| path | what |
|---|---|
| `include/codol_zone/fastfile.hpp` | container: header, client key, decrypt + inflate ( `read_fastfile` ), `zlib_inflate`; writing: `encode_fastfile`, `write_fastfile`, `zlib_deflate` |
| `include/codol_zone/zone_editor.hpp` | `ZoneEditor`: asset graph of a zone, edits, `write()` back to an inflated zone |
| `include/codol_zone/zone.hpp` | `Zone`: load, asset table, script strings, image memory access |
| `include/codol_zone/assets.hpp` | typed views: `read_model`, `read_material`, `read_image`, `read_gfx_world`, `read_clip_map`, .. |
| `include/codol_zone/c1_layouts.hpp` | packed 1.1.1.13 asset structs with compile time size / offset checks ( `src/codol/c1_layouts.hpp` forwards here ) |
| `src/` | implementation; `loaders_1_1_1_13.inc` is generated |
| `tools/codol_zonetool.cpp` | command line tool ( reading ) |
| `tools/codol_linker.cpp` | command line tool ( writing ): edit / build fast files, `--verify`, `--roundtrip-all` |
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

## Writing zones

### How a zone is written

The loaders in `loaders_1_1_1_13.inc` are the only complete description of the zone layout ( 38 asset types, which
block each piece goes to, alignment, order ), so the writer does not have a second, hand written description per
asset type. It uses the loaders twice ( `zone_editor.cpp` ):

1. **Capture.** `ZoneEditor::load` loads the zone like `Zone` does, with an observer on the loaders:
   * every `DB_LoadXFileData` / `Load_XStringData` is a *chunk*: its stream bytes, its length, where it landed;
   * a 32 bit store that replaces the stream value `( block << 28 | offset ) + 1` of a field with the address it stands
     for ( `DB_ConvertOffsetToPointer` ) or with the word at that address ( `DB_ConvertOffsetToAlias` ) marks the field
     as a *relocation*. Its target is stored as chunk + offset, as an allocation the loader made without reading the
     stream ( `DB_InsertPointer` slots, runtime block arrays; identified by the ordinal of the `g_streamPos` store
     inside the asset ), or as the header slot of a top level asset in the XAsset table;
   * a 16 bit read of the script string array followed by a 16 bit store marks a *script string field*.

   Chunks are grouped into *units*: the XAssetList root ( script strings, XAsset table ) and one unit per top level
   asset, holding everything the loaders read for it, nested assets included.
2. **Write.** `ZoneEditor::write` runs the same loaders again without zone bytes: whatever they ask for next is
   handed out from the graph, chunk by chunk, with every relocation encoded for where its target is *now*. The
   loaders thereby decide block, alignment and order of everything, exactly as the game will when it loads the
   result. This is the approach of CODOL-AssetExplorer's `Porting/ZoneWriter.cs`. Pointers to data that is laid out
   later ( the shipped zones contain such forward references, 24691 in mp_killhouse_sh ) are patched once the target
   is placed. Afterwards the XFile header ( `size`, the eight block sizes ) and the XAsset table ( stream offset and
   size per asset ) are filled in. A graph that does not fit its own headers ( a count that does not match the data
   that follows ) is refused with a message naming the asset; nothing is written.

Block sizes: a zone captured from a file keeps what its header declared on top of what the loaders reach ( the
shipped zones declare 0x10 bytes more temp block than the loaders use, the other blocks exactly ); a zone made with
`create()` follows that rule ( `ZoneWriter.cs` does the same ).

What this gives and what it does not:

* An unedited zone is written back **byte identical** ( all fast files of the install, see below ).
* After an edit that changes sizes, every pointer, offset and size in the zone follows. Verified by loading the
  result again: all other assets have the same content and pointer structure ( `asset_fingerprint` ).
* The non pointer bytes of a chunk are carried as they are. The editor knows the *structure* of every asset type,
  not the *meaning* of its fields. Assets can therefore be kept, removed, added or taken whole from another zone for
  every type, but changed field by field only where an explicit edit exists ( MapEnts entity string, RawFile,
  StringTable ) - or through the typed editing below, by a caller that knows the layout.

### API

```cpp
#include "codol_zone/zone_editor.hpp"
using namespace codol::zone;

ClientKey key;
key.load_from_exe("C1.exe");

ZoneEditor editor;
Status s = editor.open("mp_killhouse_sh.ff", key);            // or load( inflatedZone, "name" ), or create( "name" )

s = editor.set_map_ents(entityString);                        // any length; MapEnts may be nested in the clip map
s = editor.set_rawfile("maps/mp/foo.gsc", bytes);             // replaces, or adds ( deflated; false = stored )
s = editor.set_string_table("mp/foo.csv", StringTableData{ columns, rows, cells });
s = editor.remove_asset(editor.find_asset(codol::c1::ASSET_TYPE_RAWFILE, "vision/x.vision"));

ZoneEditor donor;
donor.open("mp_dome.ff", key);
s = editor.add_asset(donor, donor.find_asset(codol::c1::ASSET_TYPE_XMODEL, "machinery_windmill"));
s = editor.replace_asset(index, donor, donorIndex);           // same type; pointers to the old header keep working

std::vector<std::uint8_t> zone;
ZoneWriteReport report;
s = editor.write(zone, &report);                              // inflated zone: XFileHeader + data
s = write_fastfile("out/mp_killhouse_sh.ff", zone, key);      // IWffu100
```

`editor.zone()` is the zone as loaded ( typed read access with `assets.hpp` ), `editor.asset( i )` / `asset_count()` /
`find_asset()` describe the current asset list, `asset_fingerprint( i )` hashes an asset independent of its position
( chunk bytes, pointer targets instead of pointer values, script strings by text ). Edits return `Status` and change
nothing when they fail.

Rules of the edits:

* **`set_map_ents`** replaces the chunk of the entity string and `MapEnts.numEntityChars`. The trigger models, hulls
  and slabs of the MapEnts are not touched ( they are separate data that the entity string refers to by index ).
* **`set_rawfile`**: `compressedLen` / `len` and the buffer. A deflated file is read by the game as `compressedLen`
  bytes, a stored one as `len + 1` ( terminator ).
* **`set_string_table`** rebuilds the asset: header, name, cells ( string + hash, `hash = tolower( c ) + 31 * hash` ),
  one string per cell. Fails when another asset shares one of the table's strings.
* **`remove_asset`** fails when another asset points into the removed one. This is common: the zone linker stores
  a nested asset ( a material inside an xmodel ) and every shared string once, at its first use, and later users
  point there.
* **`add_asset_closure`** is `add_asset` with dependencies for assets that share data with other top level assets of
  their zone: every asset of the donor that the copied one points *into* comes along, in front of it ( the technique
  sets of a map zone share techniques, passes and shaders; `codol_assets` copies them this way ).
* **`add_new_asset`** adds an asset that exists nowhere yet, given as the pieces of stream data the loaders read for
  it, in their order ( header first ), with its pointers ( to another piece, or to the header slot of a top level
  asset of the zone ) and its script string fields. Nothing is checked at that point; `write()` runs the loaders over
  it. `codol_assets` builds images, materials and models this way.
* **`add_asset` / `replace_asset`** copy a unit. Pointers that leave it are re-targeted: at the same asset ( type +
  name ) of the receiving zone when they lead to a top level asset's header slot ( `withDependencies` copies such
  assets when they are missing ), at an equal string when they lead to a string. Script string indices are
  translated to the receiving zone's list, which grows as needed. Anything else - data of another asset that is
  not a string, e.g. a material that was nested in an earlier xmodel of the donor - fails with a message that names
  the other asset. Assets that are self contained in the donor ( their materials are by-name references `,name` or
  nested in the asset itself ) copy cleanly.

### Typed editing

For asset types without a dedicated edit the graph can be changed directly, by whoever knows the layout
( `codol_mapc` does this for the GfxWorld and the clip map ). Every piece of stream data - one read of the loaders: a
header, an array, a string - has a `ZoneDataId` that stays valid across edits, and a pointer field is a target, not a
number:

```cpp
ZoneDataId header = editor.header_data(zone.find_asset(type, name));        // an asset header ( pool address )
ZoneDataId array  = editor.data_at(view.raw.someArray);                     // the data behind a pointer of the loaded zone
ZoneTarget t      = editor.pointer_at(header, offsetof(c1::clipMap_t, planes));   // .kind, .data, .offset, .asset
std::vector<ZonePointer> inside = editor.data_pointers(array);              // the zone pointers the array holds
std::vector<ZoneInbound> users  = editor.pointers_to(array);                // every pointer of the zone into it

editor.set_data(array, newBytes, newPointers);       // any length; pointers given as { offset, ZoneTarget }
editor.patch_value(header, offsetof(.., count), n);  // a field; refused when it touches a pointer field
editor.set_pointer(id, offset, ZoneTarget::to_data(other, 8 * k));          // or to_asset( index ), or plain 0 / 0xFFFFFFFF
editor.insert_data(anchor, /*after*/ true, bytes, pointers, created);       // new data where the loader will read it
editor.remove_data(id);  editor.remove_data_range(first, last);             // refused while something points into it
```

* An array "resize" is `set_data` + `patch_value` of its count; a pointer that was null and now has data is
  `set_pointer( .., {}, 0xFFFFFFFF )` + `insert_data` at the place the loader reads it ( behind the struct, after the
  data of the pointer fields in front of it ).
* Nothing is checked against a schema - there is none. `write()` runs the game's loaders over the graph and fails,
  writing nothing, when it does not fit: a count that does not match its array, data the loaders do not ask for,
  a pointer that says "follows" without data. The message names the asset and the piece.
* `set_data` refuses to cut off data another pointer leads into; `remove_data_range` removes a whole sub tree ( a
  nested asset ) with its pointers, script string fields and loader allocations, and refuses when anything outside
  still uses it.
* `zone()` stays the zone as loaded; addresses always mean that zone. To read what an edit made, write and load.
* Loader allocations that are pointer targets ( `DB_InsertPointer` slots, runtime arrays ) are keyed by the piece of
  data they follow, so inserting or removing data elsewhere in the asset does not move them. An allocation directly
  at the place of an insertion cannot be told from one that belongs to the new data ( it is taken to come first ).
* `asset_fingerprint` of an asset changes when an asset it points *into* gains or loses a piece of data ( the
  position of the target in that asset's stream order is part of the hash ), although the asset itself is the same.

Limits, stated plainly:

* Relocations are recognised by what the loaders do. A field that holds a zone offset but that the loaders never
  convert would not move with its target; none is known, but nothing proves there is none. The reverse case
  exists: in `cLeafBrushNode` the translated loader converts the `data` union of every node whose 16 bit
  `leafBrushCount` is not zero, and for part of those nodes the union holds floats ( `dist`, `range` ) instead of
  a pointer: 83908 fields in 42 zones, all in `col_map_mp`, the same case `ZoneImage.RawReferences` of the
  reference exists for. They point at nothing the loaders placed and are written back unchanged, which keeps the
  round trip exact. If the bit pattern of such a float happened to be the offset of real data ( tiny positive
  values, below about 1e-3 even for a 160 MB block ), it would be taken for a pointer and moved with that data.
* `replace_asset` and `add_asset` do not convert anything: an asset of another game version does not fit.
* No signed container ( see below ). No `.iwd` / `.iwi` writing.
* Memory: an editor keeps the zone stream and the loaded image; `write()` builds a second image and the output.
  Roughly four times the inflated zone size at peak ( not measured ): use the x64 build for big maps.

### Container

`encode_fastfile` / `write_fastfile` produce the **unsigned** container, as `FastFileWriter.cs` ( `WriteUnsigned` )
of CODOL-AssetExplorer does:

```
0x000  "IWffu100"
0x008  u32 version = 334
0x00C  u8  allowOnlineUpdate = 1
0x00D  u32 FILETIME high, u32 FILETIME low
0x015  RSA-OAEP( SHA-256 ) block, 0x100 bytes: random 24 byte AES-192 key + 16 byte IV, wrapped with the client key
0x115  zlib stream of the zone; the bytes up to file offset 0x40000 are AES-CTR encrypted ( IV above ), the rest is not
```

The zlib stream comes from the library's own compressor ( `deflate.cpp`: LZ77 with hash chains, lazy matching,
dynamic Huffman blocks; level 6 gives files within 1 % of the size of the originals ). No hash table, no signature.

The **signed** container `IWff0100` is not written. Its auth chunk holds an RSA signature over the hash of the sub
header ( `FastFileAuthHeader.SignedSubheaderHash`, 256 bytes at offset 44 of the `IWffs100` chunk ). The client
verifies it with a public key ( DER at 0x00C3D138, 0x10E bytes, `0x004B55C0` .. `0x004B56A0` ); the private half is
not in the client - the key at 0x00C3D248 that this library reads only unwraps the AES key. The chunk layout and
the hash tree could be reproduced, the signature cannot.

### What loads an unsigned file

Nothing below was tested in this work: no game, server or editor process was started. The statements are those of
the sources named.

| consumer | `IWffu100` written here | source |
|---|---|---|
| this library ( `read_fastfile`, `Zone`, `codol_zonetool`, the editor's map loader ) | yes | tests, `--roundtrip-all` |
| CODOL-AssetExplorer | yes, it reads and writes the same layout | `FastFileReader.cs`, `FastFileWriter.cs` |
| the dedicated server ( CoD4x_Server, `codol_fastfile.c` ) | reads `IWff0100` and `IWffu100`; for signed files it checks the block hashes and does not verify the RSA signature | `CoD4x_Server/docs/CODOL_PORT.md` ( "Archive reader", "the header's RSA signature is **not** verified" ) |
| retail client, unmodified | **no** when the file is on disk. `DB_LoadXFile` 0x004B5D50 accepts the magic and sets signed = 0, then `0x4B5EFB cmp eax, ebx ; jne ; cmp [stream.handle], -1 ; jne 0x4B5F1A` refuses an unsigned zone that has a file handle ( the player sees `WIN_FILE_CORRUPT` ). Unsigned is only taken from memory: the lobby's `ffotd_client.ff`, whose buffer is limited to 65535 bytes ( 0x005926F2 ) | `Kernel33-main/kernel33.c` in the install ( comment above `PatchUnsignedZones` ), `CoD4x_Server/docs/CODOL_ENGLISH_UI.md`, `CODOL_FASTMAG_EVENT_ASSET_20260911.md` |
| retail client with the install's `Kernel33.dll` | yes: `PatchUnsignedZones` replaces the `jne` at 0x004B5F02 with two nops, after which "an unsigned zone loads from a file like any other". A stock zone is replaced by putting the unsigned build in its stock folder under the same name; additional zones go to `client_shipRetail\patch\` ( loaded ahead of the stock zones, alphabetical, name must not equal a stock zone ). The install already runs this way: 64 of its 153 fast files are `IWffu100`, the largest 76 MB | same `kernel33.c` |

Whether the *content* of an edited zone is accepted by the game ( an entity string the map scripts can live with, a
script that compiles ) is outside of what this library can check. What it checks is that the game's own loaders,
translated, consume the written zone to the last byte and build the same assets.

### codol_linker

```
codol_linker --base <zone.ff> --exe <C1.exe> [edits] --out <new.ff>
codol_linker --new <zone name> --exe <C1.exe> [edits] --out <new.ff>      a zone from nothing
    --set-ents <file>                    replace the MapEnts entity string
    --add-rawfile <name>=<file>          replace the rawfile <name>, or add it
    --set-stringtable <name>=<file.csv>  replace or add a string table
    --remove <type>:<name>               remove a top level asset
    --take <type>:<name>@<donor.ff>      copy an asset from another zone ( replaces one of the same type and name )
    --with-deps  --store-rawfiles  --level <0..9>  --zone-out <file>  --no-check
codol_linker --list <file.ff> --exe <C1.exe>            asset list with stream sizes, chunk and pointer counts
codol_linker --verify <file.ff> --exe <C1.exe>          decode, load, write again, compare
codol_linker --roundtrip-all <install> [--exe <C1.exe>] [--out-dir <dir>] [--filter <text>] [--no-container] [--level <n>] [--keep]
```

Edits are applied in the order given. After writing, the file is read back from disk, loaded, and compared asset
by asset with the base ( `--no-check` skips this ):

```
> codol_linker --base mp_killhouse_sh.ff --exe C1.exe --set-ents ents.txt --add-rawfile maps/mp/test.gsc=test.gsc --out out/mp_killhouse_sh.ff
base mp_killhouse_sh.ff: 345 assets, 36349 chunks, 171479 zone pointers
  entity string replaced: 227184 characters
  rawfile maps/mp/test.gsc added: 35 bytes
  zone: 106655475 bytes, 36352 chunks, 171479 zone pointers ( 24691 forward )
wrote out/mp_killhouse_sh.ff: IWffu100, 44830678 bytes
check: the file decodes and all 346 assets load ( 0 warnings ). 344 assets unchanged, 2 changed / added / removed:
    col_map_mp:maps/mp/mp_killhouse_sh.d3dbsp
    +rawfile:maps/mp/test.gsc
```

( the MapEnts of a map is nested in its `col_map_mp`, hence the changed clip map. ) A script only zone, the
equivalent of CoD4's `mod.ff`:

```
> codol_linker --new codol_mod --exe C1.exe --add-rawfile maps/mp/gametypes/_mod.gsc=_mod.gsc --set-stringtable mp/mod.csv=mod.csv --out out/codol_mod.ff
```

`--roundtrip-all` never writes into the install; `--out-dir` ( default `./codol_linker_roundtrip` ) receives the
written files, which are deleted again unless `--keep` is given.

### Verification of the writer

On the 1.1.1.13 install of this machine, x64 Release build. `--roundtrip-all` walks `client_shipRetail`
recursively and finds 153 `.ff` files: the 138 of `chinese` and 8 of `patch` that the reader was verified
on, plus 7 more ( 5 in backup sub folders of `chinese`, 2 in `temp` ). 89 are signed, 64 unsigned.

| check | result |
|---|---|
| read -> capture -> write gives the byte identical inflated zone | 153 / 153 ( 4.3 GiB of zone data, 2771086 chunks, 5724460 zone pointers re-encoded, 83908 of them kept as they are: the `cLeafBrushNode` fields above ) |
| that zone written as `IWffu100`, read back from disk with `read_fastfile`, equal to what was written | 153 / 153 ( 2.16 GiB written at level 6, the originals are 2.15 GiB ) |
| mp_killhouse_sh with an entity string of another length ( 227009 -> 227184 characters ) and a new rawfile: written, read back with the reader | loads without warnings; entity string equal to the input; 344 of 345 assets have the same fingerprint, the one that differs is the clip map that holds the MapEnts; `codol_zonetool map` prints the same counts except entities + 1 |
| an xmodel taken from mp_dome into mp_killhouse_sh ( script strings translated ) | same fingerprint as in the donor, same bone names, same `codol_zonetool model` output; all assets of the base unchanged |
| a zone from nothing ( 2 rawfiles, 1 string table ) | loads, contents equal, `--verify` identical |
| the string table hash against the 120 tables of common_gametype_mp | every cell equal |
| unit tests ( `ctest` ): deflate against the inflater at 5 levels, editor on a synthetic zone, the cases above, typed editing ( the entity string replaced with `set_data` + `patch_value` gives the bytes `set_map_ents` gives; refused edits; insert + remove gives the original ) | 969 checks, 0 failed, Win32 and x64 |
| `codol_mapc` ( static models, brushes, surfaces of mp_rust and mp_killhouse_sh edited through the typed editing ) | see `codol_mapc/README.md` |

Time for `--roundtrip-all`: 133 s for everything ( load, capture, write, deflate, file write, read back ).

Not verified: any load in a running game or server ( see above ); `replace_asset` between real zones beyond the
cases above; `withDependencies` on real zones; string tables and rawfiles are checked through this library's own
reader, not through the game.

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
list ), and 71512 of 72907 unflagged rows of those 89 files have the same type + name. The remaining rows are names that
document's own README calls heuristic ( it does not replay the loader ); for the types they fall into ( localize,
sound, xmodel, material, weapon, xanim ) the library's names equal the oracle's in every zone.

Not verified against an independent source: the contents of asset types without a view, tangents of model
vertices, GfxWorld vertex contents ( only counts and index / vertex ranges ), brush side planes ( only counts and
that every side resolves to a plane and a material ), the `col_map_sp` / `game_map_sp` loaders ( no such zone ).

## Linking into the radiant DLL ( 32 bit, MSVC, static CRT )

* `add_subdirectory(codol_zone)` + `target_link_libraries(iw3r PRIVATE codol_zone)`; set `CODOL_ZONE_BUILD_TOOLS` /
  `CODOL_ZONE_BUILD_TESTS` to OFF there. The library is built with `/MT` ( `/MTd` in Debug ) unless
  `CODOL_ZONE_STATIC_CRT` is OFF - it must match the DLL. For premake: compile the eight `.cpp` files in `src/` with
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
| `src/deflate.cpp` | written for this library from RFC 1950 / 1951 |
| `src/fastfile_write.cpp` | written for this library; format after `FastFile/FastFileWriter.cs` |
| `src/zone_editor.cpp` | written for this library; the method ( loaders driven by a generated stream, forward reference patching, block size rule ) after `Porting/ZoneWriter.cs`, `ZoneCapture.cs`, `ZoneImage.cs`. The graph itself is different: stream chunks and relocations instead of a tagged memory image |

No third party source code is included: no zlib, no mbedtls, no libtomcrypt. CODOL-AssetExplorer carries no licence
file or licence headers; it is the repository owner's own project.

The native server ( `CoD4x_Server`, `src/codol/assets/` ) was named as a second source of truth. Its `origin/main`
could not be read in the session that wrote this library ( git access to that repository was refused by the
session's sandbox ), so nothing here is derived from or checked against that code; only the files of
`docs/zone_assets/` in its working tree were used, for the name comparison above.
