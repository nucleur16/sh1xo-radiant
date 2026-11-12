# codol_bsp

Map compiler for Call of Duty Online 1.1.1.13 ( "C1" ): a `.map` ( brushes, patches, entities, lights ) in, one fast
file with every asset a map zone of this game holds out. It has the place `cod4map` + `cod4rad` + `linker_pc` have in
the CoD4 mod tools. Unlike `codol_mapc` ( which patches the zone a map was imported from ) nothing is taken from a
shipped map zone: GfxWorld, clipMap, ComWorld, GameWorldMp, FxWorld and MapEnts are built from the `.map`. What a map
only *uses* - materials with their technique sets, shaders and images - is looked up in the install's zones and
carried over.

Static library `codol_bsp` ( namespace `codol::bsp` ) + the tools `codol_bsp`, `codol_bsp_probe` + `codol_bsp_tests`.
Depends on `codol_zone` ( zones ), `codol_edit` ( the map document ), `codol_mapc` ( `check_server_collision`,
`clip_trace` ).

**Status in one paragraph.** Maps compiled by this tool were loaded by the dedicated server ( CoD4x_Server
`codol_dedrun`: collision installed, bots play ) and by the real client ( `codoMP_client_shipRetail.exe` with the
install's Kernel33 ), under a map name no stock map has, with no change to Kernel33, lobby or server: see
[In the game](#in-the-game). Cells / portals and the light bake were verified with `codol_viewtool` ( which runs the
game's shaders ); their client run is described there too. What is approximate or missing is listed under
[Limits](#limits-and-what-is-not-done) - the main ones: static models are not compiled into the world, patches
collide through brushes instead of terrain triangles, reflection probes are flat colours, no T-junction repair.

## Build

```
cmake -S codol_bsp -B build/codol_bsp -A x64 -DCODOL_EDIT_RAPIDJSON_INCLUDE=<checkout>/deps/rapidjson/include
cmake --build build/codol_bsp --config Release
build/codol_bsp/Release/codol_bsp_tests.exe --out build/bsp_test          ( or ctest --test-dir build/codol_bsp -C Release )
powershell -ExecutionPolicy Bypass -File codol_bsp/tests/run_e2e.ps1       ( tests + tool + codol_linker --verify + pictures + a server run )
```

Static CRT, `/std:c++latest`, `/W4 /WX`, x64 and Win32 ( use x64 ). Builds `codol_zone`, `codol_edit`, `codol_render`,
`codol_view`, `codol_mapc` with it, and their tools ( `codol_viewtool`, `codol_linker`, `codol_zonetool` ).

## The tool

```
codol_bsp --map <file.map> --exe <C1.exe> --install <install dir> --out <mapname.ff> [--name <mapname>]
          [--report <file>] [--zone-out <file>] [--cache <dir>] [--donor <zone.ff>]... [--rawfile <name>=<file>]...
          [--fast | --quality n] [--no-light] [--no-vis] [--no-load] [--level 0..9] [--exact-size <bytes>]
codol_bsp --check <map.ff> --exe <C1.exe>
```

* `--install` the game folder: its zones ( `client_shipRetail/chinese`, `patch` ) are where materials come from. The
  first compile scans them ( about 16 s ) and writes `codol_bsp_catalog.txt` into `--cache` ( default: next to `--out` );
  later compiles read that file, until a fast file of the install changes.
* `--name` the map name ( `a-z 0-9 _` ); default: the name of the output file. The zone's assets are named
  `maps/mp/<name>.d3dbsp`.
* `--out` an unsigned `IWffu100` fast file. Next to it `<name>_load.ff` is written ( `--no-load`: not ), the zone the
  client loads for the loading screen.
* `--fast` / `--quality n`: the light bake - luxel size 32 / 16 ( default, quality 1 ) / 8 units and 24 / 64 / 64 n
  sky directions. `--no-light`: no bake, one ambient colour, everything in full sun. `--no-vis`: one cell, no portals
  ( hidden faces are still removed ).
* `--rawfile maps/mp/<name>_fx.gsc=<file>` compiles a file into the zone as a rawfile ( any number ).
* `--donor` a fast file searched for materials ahead of the install's catalog.
* `--exact-size` pads the file to a length ( only needed when a compiled map takes the place of a *stock* zone: the
  client compares the length of every stock zone with a table ).
* `--check` runs the validators on any map fast file. Shipped maps fail some of the render checks: those are written
  for this compiler's output and are stricter than what the game's compiler does ( several suns, light grid entries
  with special light indices, surfaces that reach out of their tree node ).
* Exit code 0: written. 1: failed, nothing written. The report ( `--report`, also printed ) lists counts per stage,
  warnings, a leak with its path, times.

```
> codol_bsp --map two_rooms.map --exe C1.exe --install "D:\..\Call of Duty Online 1.1.1.13" --out out/mp_tworooms.ff
map mp_tworooms: 22 brushes ( 18 structural, 3 detail ), 0 patches, 106 entities, 0 lights, 0 static models
collision: 22 brushes, 0 sides, 15 planes, 7 materials, 2 nodes, 3 leafs, 4 leaf brush nodes, 2 sub models
render: 25 surfaces, 168 vertices, 84 triangles, 5 materials, 5 cells, 8 portals, 88 faces removed
light: 1 lightmaps, 10296 light grid points, 2 primary lights, 2 reflection probes
  vis: 55 tree nodes, 56 leafs ( 19 solid, 5 open, 32 void ), 268 leaf portals; 5 cells, 8 portals; 88 faces hidden
  validated: server collision ok: .. ; render ok: ..
wrote out/mp_tworooms.ff ( IWffu100 )
```

To look at a compiled map: `codol_viewtool render --game <install> --map mp_tworooms --zone-dirs out --out a.png --pos
x,y,z --angles pitch,yaw --stats` ( environment `CODOL_VIEW_STRICT_CELLS=1`: only what the portals reach is drawn, as
in the game ).

## API

```cpp
#include "codol_bsp/bsp.hpp"
namespace codol::bsp
{
    struct options { std::filesystem::path map, exe, install, out, zone_out, cache, load_out; std::string name;
                     std::vector<std::filesystem::path> donors; std::vector<std::pair<std::string, std::filesystem::path>> rawfiles;
                     int level = 6, quality = 1; std::uint64_t exact_size = 0; bool light = true, vis = true, load_zone = true; };
    struct report  { /* counts per stage */ bool leaked; std::string leak_entity; std::vector<leak_point> leak_path;
                     std::vector<std::string> notes, warnings; std::string text() const; };
    bool compile(const options&, report&, std::string& error);                       // files in, fast file out
    bool compile_document(const edit::document&, const options&, const zone::ClientKey&,
                          std::vector<std::uint8_t>& zone_bytes, report&, std::string& error);
    bool validate_zone(std::span<const std::uint8_t> zone_bytes, std::vector<std::string>& problems, std::string* summary = nullptr);
}
```

**What the editor's Compile action should call:** `doc.save_map_file( "x.map" )`, then
`compile( { .map = "x.map", .exe = C1.exe, .install = game dir, .out = "<out>/<mapname>.ff" }, report, error )` on a
worker thread ( or `compile_document` with the open document ). `report.leak_path` is the point list for a leak line in
the viewport, `report.warnings` goes to the log. Nothing is wired into the editor yet.

## What a map zone holds

Asset order of a compiled zone ( `codol_linker --list` ); the shipped map zones have the same kinds in a similar order:

| asset | what |
|---|---|
| rawfile `animlists/<map>.animlist` | empty; the server refuses a map without it |
| `com_map` | ComWorld: primary lights - [0] none, [1] the sun ( colour x intensity, direction towards the sun ). It stores the string `maps/mp/<map>.d3dbsp` every other map asset points at |
| `fx_map` | FxWorld without glass ( all zero but `firstFreePiece` 0xFFFF ) |
| `image`, `techset`, `material` .. | what the world's materials need, see [Materials](#materials) |
| `gfx_map` | GfxWorld |
| `game_map_mp` | GameWorldMp: no path data ( the loader clears it ), an empty `G_GlassData` ( the server reads it without a null check ) |
| `col_map_mp` | clipMap with the MapEnts nested in it |
| rawfiles | `--rawfile` |

`<map>_load.ff`: the materials `$defeatbackdrop` and `$levelbriefing` with the technique set `2d` and a rawfile, as the
shipped load zones. The materials are carried over from a shipped load zone, so the loading picture is that map's.

How assets refer to each other, as in the shipped zones: a pointer to a top level asset points at its header slot in
the XAsset table; an asset of a zone that is loaded already ( common zones ) is an inline header of zeros whose name
is `,<name>`. The clip map's plane pointers lead into the plane array the GfxWorld stores; the dedicated server
requires exactly that ( GfxWorld planes inline, base name inline, clip map and GfxWorld sharing the name string ).

### How the assets are written

`codol_zone` got `ZoneEditor::add_built_asset`: an asset as the stream pieces the game's loader reads, in its order,
with pointers given as targets. `ZoneEditor::write` runs the game's loaders ( the machine translation in `codol_zone` )
over the result and fails when it does not fit, so a compiled zone is by construction one the client's loaders consume
to the last byte. Stream orders are those of the loaders ( `Load_GfxWorld` 0x4C7FE0, `Load_clipMap_t` 0x4BF410, .. ),
checked against the shipped zones with `codol_bsp_probe pieces|dump`. 1.1.1.13 keeps the IW4 layouts of GfxWorld,
clipMap, ComWorld, FxWorld and MapEnts byte for byte ( CODOL-AssetExplorer `Iw4ToV1Converters.cs` ); GameWorldMp is
0x54 bytes. `src/gfx_layout.hpp` has the structs `c1_layouts.hpp` lacks.

### Materials

A `.map` names materials as radiant does ( `ch_concrete` ); the world material is `wc/<name>`. Shipped zones store a
material nested in whatever used it first ( inside a `gfx_map` ), techniques inside the first technique set that has
them, so they cannot be taken as whole assets. `import_assets.cpp` writes them again from the loaded image of the zone
that has them, in loader order - `Material`: name, technique set, texture table + images, constants, state bits;
`MaterialTechniqueSet`: name, per technique { header, passes, per pass { vertex declaration, vertex shader, pixel
shader, arguments + literal constants }, name } - as top level assets of the compiled zone, each once. Images keep
their header ( pixels are streamed from the `.iwd` archives by name ) or their inline pixels. Clip materials ( surface
flags and contents per material name ) come from the clip maps of the shipped maps, also through the catalog: they are
properties of the material, and no other place has them.

A material no zone of the install has is reported and its faces are not drawn ( they still collide ). Materials with
water maps are refused. There is no material compiler: a map can only use materials the install has.

## Stages

1. **Read** ( `map_input.cpp` ). Brushes through `codol_edit` ( planes, windings, corners ). `detail` brushes, brush
   entities ( `model "*N"`, brushes are taken as model space: an entity with an `origin` is placed by it, as
   `codol_edit`'s import writes them ), `light` entities, worldspawn keys `sundirection`, `suncolor`, `sunlight`,
   `ambient`, `skycolor`, `skylight`. Brushes with `hint` / `skip` / `portal*` materials are no geometry; a face with
   the material `portal` is a cell boundary. Patches are tessellated by `codol_edit` ( 4 subdivisions per curve
   segment ).
2. **Collision** ( `clip_build.cpp` ). Per brush a `cbrush_t`: the six axial sides implied by the bounds, the others
   as `cbrushside_t`; edge lists from the windings as in `codol_mapc` ( boxes get the lists every shipped box has ).
   Contents = contents of the sides' clip materials. The world's brushes go into a **BSP of axial splits** ( candidate
   planes are the brushes' box faces; cost = larger side + straddlers; a leaf has at most 12 brushes or cannot be
   split ), node planes in the shared plane array, children in pre order ( a child index is larger than its parent's,
   which the server checks ). Leaf 0 is the empty leaf, every other leaf has a box ( its brushes inside its region
   + 0.125 ) and one leaf brush node that lists its brushes; a brush model has its own leaf and list. `leafbrushes` are
   the lists in node order, sides and edge bytes the runs of the brushes in brush order.
3. **Vis** ( `vis.cpp` ). A BSP from the faces of the structural brushes ( and the portal faces ); portals between its
   leafs by the Quake map compilers' method; solid leafs = centre inside a structural brush; **flood fill** from the
   point entities - reaching the outside is a **leak**, reported as the list of points from the entity through the
   leaf centres and portals to the hole. Unreached leafs are void. **Hidden faces**: a face is cut by the tree and kept
   when a piece borders a reached leaf ( a kept face is kept whole ); faces of solid brushes that are covered by
   another solid brush ( crate on crate ) go too. **Cells**: with portal faces in the map, the leaf portals inside them
   are the boundaries; without, the *closed openings* are - leaf portals in one plane that touch, form a convex patch
   of at most 72000 square units and are surrounded by solid ( the cross section of a door or window ). Cells are the
   connected reached leafs; a boundary with the same cell on both sides is dropped. Each boundary becomes two
   `GfxPortal`s ( normal into the other cell, corners clockwise seen from there, plane as `n . x + w = 0`, hull axes
   up / `n x up` - measured on the shipped portals with `codol_bsp_probe portals` ). The **node stream**
   ( `dpvsPlanes.nodes` ): a value up to the cell count is a leaf ( 0: no cell, else cell + 1 ), anything above a
   split by plane `value - cellCount - 1` whose front child follows and whose back child is `next value` entries on;
   sub trees of one cell collapse. The stream of every shipped map checked is consumed exactly by this reading.
4. **Render surfaces** ( `render_build.cpp` ). Faces that are drawn, grouped by material, cell and a 1024 unit grid
   into `GfxSurface`s; static surfaces ordered lit opaque ( sort key below the lit decal key 6, and sky ), then the
   rest; brush models' surfaces behind them. A face seen from several cells is drawn by each ( a surface is in one
   cell's tree ). Vertices per face ( no sharing ), triangles clockwise, texture coordinates by `codol_edit::face_uv`
   with the size of the material's colour map, tangent = u axis. Per cell an AABB tree ( binary, 8 surfaces per leaf )
   whose leaf runs are `sortedSurfIndex`.
5. **Light** ( `light.cpp` ), see below.
6. **Write** ( `gfx_build.cpp`, `compile.cpp` ), then **validate** ( `validate.cpp`, `validate_render.cpp` ).

### The light bake

How the game's lit shaders use the lightmaps was measured by compiling constant lightmaps and rendering them with
`codol_viewtool` ( `tests/harness/lm_experiment.ps1`, floor of the box room ):

| lightmaps ( upper layer, lower layer, primary ) | floor rgb |
|---|---|
| black, black, 0 | 2 2 2 |
| 255 red, black, 0 | 136 2 2 |
| black, 255 green, 0 | 2 45 2 |
| grey 64, black, 0 | 34 34 33 |
| black, grey 64, 0 | 7 8 8 |
| black, black, 255 | 170 156 136 ( the sun's colour ) |

So: the **primary** lightmap ( L8, 1024 x 2048 ) is how much of the sun reaches a texel - the shader adds the sun
itself; the **secondary** ( A8R8G8B8, 512 x 2048 ) is two layers of 512 x 1024 on top of each other, the upper one the
baked light without the sun, linear; the lower one adds little and not linearly and is written black ( what it stands
for was not established ); alpha has no effect. Lightmap coordinates 0 .. 1 span one layer.

* One chart per drawn face ( brush face or patch triangle ), one luxel per 16 units ( `--quality` ), a luxel of
  padding, shelf packed into **one** page; when the map does not fit, the luxel grows ( reported ).
* Per luxel: `ambient` + sky light ( the share of sky directions, cosine weighted, whose ray ends on a face with a
  `sky*` material or leaves the map, times `skycolor x skylight` ) + every `light` entity ( linear falloff to
  `radius`, `n . l`, spot cone from `fov_outer` / `fov_inner` / `angles`, shadow ray ). Per primary texel ( 2 x 2 per
  luxel ) one ray towards the sun; a 3 x 3 box filter per chart softens the edge. Rays run against the solid world
  brushes ( a BVH ), so crates and patches ( their collision brushes ) cast shadows.
* **Light entities are baked, they are not primary lights**: ComWorld has "none" and the sun only. Dynamic objects do
  not cast shadows in them.
* **Light grid**: a sample at every grid point ( 32 x 32 x 64 units ) that is not inside a brush: the same light without
  a normal, the same colour in all 56 directions, primary light = the sun where it is visible. Rows / runs as the
  shipped grids ( decoding the shipped grid of mp_rust with the validator's reader ends exactly at its entry count ).
  At most 3 million points: a map whose brushes reach far beyond its spawn points and path nodes gets a grid around
  those ( reported ).
* **Reflection probes**: probe 0 ( the default, black ), then one per `reflection_probe` entity, or one in the middle
  of the map. A probe is **not rendered**: it is a 64 x 64 cube map of one colour, the light arriving at its place.
* Sun shadows at run time: every surface has the flag the shipped surfaces have ( 1 ), the sun is the primary light of
  a surface it reaches; the shadow geometry lists per light are empty ( as for the suns of the shipped maps ).

## Validation

Every compile ends with `validate_zone` and writes nothing when it fails:

* `check_server_collision` ( `codol_mapc` ): the clip map as the dedicated server's decoder reads it.
* `validate_render.cpp`, on the zone as the game's loaders build it: names and the shared plane array; the cell tree
  is a well formed tree and every cell is a leaf of it; every surface's vertices, indices, lightmap, probe and light
  are inside their arrays; `sortedSurfIndex` is a permutation; every cell's AABB tree ( runs of a node = runs of its
  children, boxes hold children and surfaces, every static surface in exactly one leaf ); portals lead to another cell
  and lie on their plane; cells list a probe other than 0; the light grid's rows, entries and colours fit together;
  lightmap images have pixels; brush model ranges; ComWorld ( light 0 none, light 1 a sun with a unit direction );
  GameWorldMp glass data present; MapEnts starts with the worldspawn and has `mp_global_intermission`; the animation
  list exists.
* The stages check themselves: leaf centres against their portals, lightmap charts do not overlap.

`codol_bsp_tests` ( 103 checks, 0 failed on this machine ): writes six test maps with `codol_edit` and compiles them;
per map: validation, the zone read by `codol_zone` and written again is byte identical, tree walk == brute force on
3000 random segments ( `clip_trace` ), `codol_edit::import_fastfile` of the compiled zone gives back the map's brushes;
box room: traces hit floor, wall, ceiling, crate, stacked crates where the map has them; two rooms: 5 cells, 8
portals, more than 60 hidden faces; the same map with a gap in a wall: leak with a path that starts inside and ends
outside; content map: brush models, portal brush ( 2 cells, 2 portals ), patch; a 144 room map: more than 1000
brushes and 20 cells.

## In the game

All on this machine, loopback only, with the scripts in `tests/harness` ( a link based copy of the install as the
server's base path, the lobby emulator alpha0022 offline with `CODOL_FORCE_MAP`, the client from
`build-editor/shadow_client` ). Scratch: `build-editor/bsptest`.

**A new map name needs no change to Kernel33, lobby or server.** What it takes:

* client: `mp_x.ff` and `mp_x_load.ff` in `client_shipRetail\chinese\` ( or `usermaps\<folder>\` ), the client started as
  `codoMP_client_shipRetail.exe` with the install's `Kernel33.dll` ( `PatchUnsignedZones` lets an `IWffu100` file load
  from disk; `PatchPureCheck` keeps names that are not stock zones out of the pure check; the stock file length table
  only covers the 90 stock names );
* server: `mp_x.ff` under its base path ( `client_shipRetail\chinese\` or `usermaps\mp_x\` ); a level script
  `maps/mp/mp_x.gsc` in its script overlay is optional ( without it: "no level script", the map still runs );
* lobby: `CODOL_FORCE_MAP=mp_x` ( + `CODOL_FORCE_GAMETYPE` ) forces any name without touching the playlists.

Not needed to enter the map, and not made by this compiler: a `mp/comaplist.csv` row, an arena entry and localized
names ( the map does not appear in the create-room list without them ), a minimap material, a loading picture of its
own ( both need images in an `.iwd` ). They are built by the user's `build_zones.py`.

What a map needs in its entities for the server's scripts ( the test maps have them ): spawn points of the gametype
and `mp_global_intermission`; `node_pathnode` entities for bots ( the server links them itself ); a `care_package`
script_model that targets a script_brushmodel ( `_airdrop.gsc` reads it at every start; with `developer 1` its absence
stops the server ).

| run | result |
|---|---|
| box room, server, `developer 1` | `CoDOL native collision installed: maps/mp/mp_boxroom.d3dbsp, 2 leaves ..`, common world 3 lights, glass 0 pieces. `codol_sweepprobe`: floor at z 0.125, walls at +-496.875 ( 512 - 15 - 0.125 ), ceiling 249.875 + 70, crate top 64.125, stacked crates 128.125, crate side 112.875 + 15, low crate 32.125 |
| box room, server, 8 bots, 100 s | all 8 play, 104 kills; the only new error lines: two path nodes of the test map inside crates |
| box room, client ( name `mp_boxroom` ) | Multiplayer -> Start -> Search -> Ready enters the map; spawned; walking into the -x wall stops at x -497; dropped onto the crate: z 64; onto the stacked crates: z 128; shots fired at a crate ( impact effect ) |
| mp_rust's collision brushes as a map ( 3226 brushes ), server, 8 bots | collision installed, 747 leaves; bots play ( 30 kills in 100 s; the map has no terrain, so some path nodes are unconnected ) |
| 144 rooms ( 1421 brushes, 106 cells ), server, 8 bots, 100 s | collision installed, 309 leaves; all 8 play, 109 kills; new error lines: path nodes of the test map ( 6 in crates, 1 unconnected ) |

## Scale

x64 Release, this machine ( 32 threads ):

| map | brushes | compile | peak memory | zone / file |
|---|---|---|---|---|
| box room | 11 | 0.5 s | - | 7.0 MB / 29 KB |
| 144 rooms ( `make_big` ) | 1421 | 1.6 s | 260 MB | 8.8 MB / 310 KB |
| mp_rust collision import | 3226 | 3.6 s | 823 MB | 10.6 MB / 1.07 MB |

( 6 MB of every zone are the lightmap page; an empty catalog adds about 16 s once. )

## Constants taken on trust

Values the shipped maps all have, written without knowing more than that: the four sort keys of the GfxWorld header
( 6, 0x28, 0x31, 0x2C ); sky sampler state 0xF3; `fogTypesAllowed` 1; vertex layer data = four zero bytes; image
header fields of lightmaps, reflection probes and the outdoor image ( category, flags 0x08000003 / 0x08010301 / 3,
level counts ); the stage "stage 0" ( trigger 1024, sun 1 ) every MapEnts has; `cplane_s.type` ( axis for + axis
planes, 3 else ) and `signbits` 0; surface flag 1; plane normal `type` semantics; material memory 0; `isInUse` 1.
Leaf box padding 0.125 and model box padding 1.0 are the relations measured on shipped maps ( `codol_mapc` ).

## Limits and what is not done

* **Static models** ( `misc_model` ) are not compiled into GfxWorld / clipMap. Up to 128 become `script_model`
  entities ( drawn when the model is in a loaded zone, lit by the light grid, no collision, no baked shadow ); more
  are left out. Carrying xmodels over needs an XModel writer like the material one.
* **Patches** collide through an 8 unit thick brush under every triangle, not through the clip map's terrain
  triangles ( `verts`, `partitions`, `aabbTrees` stay empty ). Patches of brush entities are left out.
* **Leaf brush trees** are one list per leaf ( the BSP keeps leafs small ); no split nodes are written.
* **No T-junction repair**, no merging of coplanar faces; a face that is partly hidden is drawn whole.
* **Automatic portals** only find planar, convex openings up to 72000 square units; open terrain is one cell. A face
  seen from several cells is drawn once per cell.
* **Light**: one lightmap page; no bounced light; the lower secondary layer is black; light entities are not dynamic
  primary lights ( only `light_point_linear` falloff: linear ); reflection probes are flat colours; no sun flare; the
  outdoor image ( weather occlusion ) is empty; light regions and per light shadow lists are empty; the light grid has
  no directionality.
* **Brush models** are drawn and collide, but are in no cell ( the engine places dynamic models itself ) and are lit
  by the lightmap of the place they were compiled at.
* **Sky**: faces with a sky material become the sky surfaces of `skies[0]` with the material's cube map when its header
  is in a zone; not seen in the client yet.
* No material / image compiler, no sounds, no fx placement ( `createfx` scripts can be added as rawfiles ), no
  minimap, no vision file, no `MapTriggers` ( trigger entities are brush models `*N` ), no glass, no dynEnts, no path
  data ( GameWorldMp has none in this game ).
* Unsigned container only ( `codol_zone` ).
