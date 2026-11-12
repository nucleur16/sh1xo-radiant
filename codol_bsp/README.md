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
[In the game](#in-the-game). A map can use static models from any zone ( also models that only exist inside a shipped
map's `gfx_map`, and models compiled by `codol_assets` ), a modder's own materials, terrain meshes that collide as
the clip map's terrain; the light bake has bounced light, several lightmap pages, a directional light grid and
rendered reflection probes; the compiler renders a minimap picture and writes the map's scripts
( `tools/package_map.ps1` puts a whole map together ). Everything was verified with `codol_viewtool` ( which runs the
game's shaders ), on the dedicated server ( probes, bullet traces, bots ) and - the third round - **in the game
client, with pictures** ( [Third round](#third-round-the-client-with-pictures) ): the yard loads with its own loading
picture and minimap, sky, baked and bounced light, models, a modder's materials, terrain one walks over; what the
client showed wrong was fixed ( cells that hid everything behind them, no dynamic sun shadows, a crash on shooting a
`codol_assets` model, a block of shadow under a fence ). The **fourth round**
( [Fourth round](#fourth-round-light-primary-lights-vision-sun-effects) ) read what the lit shaders do with the
lightmaps out of the shaders themselves: the bake now writes the game's units ( a room with one light entity was
nearly black ), light entities can be primary lights ( lit at run time, with baked visibility ), every map gets a
vision file and a map with a sun its flare, and the effect scripts have the shape the client reads. The **fifth
round** ( [Fifth round](#fifth-round-light-cuts-spot-shadows-t-junctions-sound) ) closed gaps against the game's own
compiler: surfaces are cut by the reach of the primary lights, light regions have a hull per cell with further
planes, lights can be scriptable, every light definition of the install can be named, T-junctions are repaired -
and it settled two open questions in the client: a primary spot **does** throw the shadows of players and script
models ( shadow map, seen on a compiled map ), and a project's loaded sounds **do** play ( measured at the client's
audio session ) while its streamed ones do not. What is approximate or missing is listed under
[Limits](#limits-and-what-is-not-done).

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
          [--assets <project zone.ff>]... [--minimap <picture.tga>] [--scripts <folder>] [--vision <file>] [--sun <file>]
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
* `--assets` / `--donor` a fast file searched for materials and xmodels ahead of the install's catalog: a
  `codol_assetc` project zone with a modder's own materials and models, a ported map.
* `--minimap`, `--scripts`: see [Minimap, scripts, the files around a map](#minimap-scripts-the-files-around-a-map).
* `--vision`, `--sun`: the map's vision and sun file, see [Vision file, sun flare](#vision-file-sun-flare).
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
render: 19 surfaces, 160 vertices, 80 triangles, 5 materials, 3 cells, 4 portals, 88 faces removed
light: 1 lightmaps, 10296 light grid points, 2 primary lights, 2 reflection probes
  vis: no portal brushes in the map; 2 openings found and given a portal ( .. )
  vis: 55 tree nodes, 56 leafs ( 19 solid, 5 open, 32 void ), 268 leaf portals; 3 cells, 4 portals; 88 faces hidden
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
| rawfiles `vision/<map>.vision`, `sun/<map>.sun` | glow and film tweaks; the sun's sprite and flare ( the second only with a sun and a sky ) |
| `lightdef` | `light_point_linear`, when the map has a primary light entity: a name, the falloff image by name, where its copy starts in row 0 of the lightmaps |
| `com_map` | ComWorld: primary lights - [0] none, [1] the sun ( colour x intensity, direction towards the sun ), then the primary light entities. It stores the string `maps/mp/<map>.d3dbsp` every other map asset points at |
| `fx_map` | FxWorld without glass ( all zero but `firstFreePiece` 0xFFFF ) |
| `image`, `techset`, `material` .. | what the world's materials need, see [Materials](#materials) |
| `gfx_map` | GfxWorld |
| `game_map_mp` | GameWorldMp: no path data ( the loader clears it ), an empty `G_GlassData` ( the server reads it without a null check ) |
| `col_map_mp` | clipMap with the MapEnts nested in it |
| rawfiles | `maps/mp/<map>_fx.gsc`, `maps/createfx/<map>_fx.gsc` ( always: the client reads them ), `--rawfile` |

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

A material no zone has is reported and its faces are not drawn ( they still collide ). Materials with water maps are
refused.

**Hardware shadow map technique sets.** Every shipped zone holds each lit technique set twice: `<..>_sm_<..>` and
`<..>_hsm_<..>` ( mp_rust: 323 and 308 names ). A material names the first; the game finds the second by name and
draws with it when it shadows with depth textures - which it does. A zone that only has the `_sm_` set draws its
materials without the sun's shadow map: no shadow of a player or a model on them, and the sun's shadow edges are the
stair steps of the primary lightmap ( seen in the client on the first compiled maps ). So the importer writes the
`_hsm_` sibling of every lit set it writes or copies, from the same donor; where no donor has one the report warns.
The report counts them ( `materials: 17 technique sets for hardware shadow maps ..` ).

**A modder's own materials** come from a `codol_assets` project zone ( `codol_assetc --image .. --material x.mtl` )
given with `--assets <zone.ff>`: it is searched ahead of the install, and the material, its images ( inline pixels )
and its `wc_*` technique set with what that shares are written into the map zone like any other, so the map does not
need the project zone at run time. Such a material is on no brush of a shipped map, so its clip material comes from
its definition: the surface type number << 20 as the shipped clip materials carry it, solid - or nothing for a
blended decal ( sort key 6 .. 13 ). A brush with an alpha tested or decal face hides no face behind it and splits no
cell, whatever its flags. Test: `tests/make_modder_assets.py` makes PNGs and an `.mtl` ( a lit brick wall with normal
and specular map, an alpha tested chain link fence, a blended decal ), the test map `modder` uses them: drawn by
`codol_viewtool` from the map zone alone ( `png\s2_modder.png` ), loaded by the server, where bullet traces report
the surface types brick and metal and pass through the decal.

## Stages

1. **Read** ( `map_input.cpp` ). Brushes through `codol_edit` ( planes, windings, corners ). `detail` brushes, brush
   entities ( `model "*N"`, brushes are taken as model space: an entity with an `origin` is placed by it, as
   `codol_edit`'s import writes them ), `light` entities and the worldspawn's light keys
   ( [Light keys](#light-keys) ). Brushes with `hint` / `skip` / `portal*` materials are no geometry; a face with
   the material `portal` is a cell boundary. Patches are tessellated by `codol_edit` ( 4 subdivisions per curve
   segment ). `misc_model` -> static models, `script_fx` / `script_sound` -> script lines.
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
   another solid brush ( crate on crate ) go too. **Cells**: the leaf portals inside the map's portal faces are the
   boundaries. A map without portal brushes gets its portal faces from the compiler, which looks for the *openings* a
   mapper would put one in and then starts over with them: a cross section of the open space in the plane of a brush
   face ( the leafs cut by the plane - the tree rarely splits a doorway along the face of its wall - and the leaf
   portals in it ) that is one convex piece of at most 72000 square units, has something solid just beyond every edge
   on at least one side of the plane, and on at least half of that outline on one side only: it lies in the face of
   a wall, where the space widens. A slice through a room or a corridor has solid on both sides all around and is no
   opening. Of the cross sections of one opening ( both faces of the wall .. ) one is kept. `CODOL_BSP_VIS_DEBUG=1`
   prints every cross section and what became of it. ( Until the third round every small convex group of leaf portals
   was a boundary: the tree's splits cut a room into a dozen cells, and a group that lay between several pairs of
   cells got portals for one pair only - in the client everything beyond the next cell was black. ) Cells are the
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
5. **Light** ( `light.cpp` ), see below. It also cuts faces by the reach of the primary lights ( "Primary lights" ).
   Then **T-junctions** ( `tjunction.cpp` ): every face is its own polygon, so where the corner of one face lies
   in the middle of an edge of another - a crate on a floor, a wall that ends halfway along another, a face the
   light stage cut, a terrain mesh along a brush - the rasteriser may leave single pixels open along the joint.
   As in the Quake map compilers every vertex of the world that lies on an edge of a face ( within 0.02 units )
   becomes a vertex of that face, with the position of the other face's vertex to the bit and the edge's texture
   and lightmap coordinates there, and the face is cut into triangles again so that no triangle has a vertex in
   the middle of a side. The report counts them ( the yard: 226 vertices added to 73 faces ).
6. **Write** ( `gfx_build.cpp`, `compile.cpp` ), then **validate** ( `validate.cpp`, `validate_render.cpp` ).

### Static models

`misc_model` ( keys `model`, `origin`, `angles`, `modelscale` ) is a GfxWorld static model ( draw instance, bounds,
the AABB tree leaf of the cell its centre is in, the list of its primary light ) and, when the xmodel has collision
surfaces, a clip map entry with a two level `smodelNodes` tree.

* **Where the xmodel comes from.** A top level asset of a zone - the common zones, a ported map, a `codol_assets`
  project zone given with `--assets` - is copied with everything it points into ( `ZoneEditor::add_asset_closure` ).
  A model that only exists *nested* in a shipped map's `gfx_map` ( most props: 2790 names in this install ) is written
  again from the loaded zone in loader order by `asset_importer::model_from`: bones, materials ( through the material
  importer ), the surfaces of every LOD with the collision trees of their rigid lists, collision surfaces, bone boxes;
  physics preset and collision map are dropped. The catalog lists both kinds.
* **Collision.** In this engine only *point* traces ( bullets, sight: `SV_Trace( .., staticmodels )` ->
  `CM_PointTraceStaticModels` ) look at static models; the box sweep of player movement never does, on shipped maps
  as on compiled ones - which is why `codol_sweepprobe` passes through any static model. A mapper puts a `clip`
  brush around a model players should not walk through ( the test map `yard` does ). The clip entry's matrix is the
  inverse of the scaled axis, `transpose( axis ) / scale`: 6952 of the 6953 clip entries of mp_rust, mp_highrise and mp_shipment_long are that
  ( `codol_bsp_probe smodels` ); the first version wrote `axis / scale`, which turned the collision of a yawed model
  the wrong way round. Shown with `bulletTrace` from a level script ( `tests/harness/trace_probe.ps1` ): a ray along
  the model's forward axis hits a crate yawed 30 degrees with the normal ( -0.866, -0.5, 0 ), a ray through a
  `codol_assets` crate yawed 45 degrees at ( -467.06, -207.06 ) with the normal ( -0.707, -0.707, 0 ), 24 units from
  its centre. In the client a bullet on a model also leaves an impact effect ( by the surface type of the collision
  surface ) and a mark, which the game projects through the collision tree of the model's surface: a model without
  those trees crashes the client at the first bullet. Models of shipped zones have them; `codol_assets` writes
  them since the third round ( its README ).
* **Cull distance**: the draw distance of the model's last LOD times the instance's scale, 0 ( never culled ) when
  that is more than 65535 - 9539 of the 9542 instances with LODs in mp_rust, mp_highrise and mp_shipment_long are
  exactly that, the other 3 differ by the rounding of the scale ( `codol_bsp_probe smodels <zone> <exe> csv` ).
* **Light.** The LOD 0 triangles of every model are occluders of the bake ( baked shadows on lightmaps and in the
  light grid ). A model itself is lit at run time from the light grid sample at its box centre; for points in space
  model triangles are one sided, so that sample looks out of the model.

### Terrain

A patch ( terrain mesh; curves as far as `codol_edit` tessellates them ) is drawn as its tessellation and collides
as the clip map's terrain: `verts`, `triIndices`, `triEdgeIsWalkable`, `borders`, `partitions`, `aabbTrees` and the
leafs' `firstCollAabbIndex` / `collAabbCount` / `terrainContents`. Shapes and rules were read off shipped maps
with `codol_bsp_probe terrain <zone> <exe>`:

* a triangle is stored so that `cross( v1 - v0, v2 - v0 )` points away from the side one stands on; indices are
  relative to the partition's vertex block ( block 0 here: at most 65535 collision vertices );
* a partition is a run of up to 16 triangles of one material that lie together ( the shipped ones have up to 255 );
  partitions take part in the collision BSP next to the brushes ( padded by 0.125: a level piece has no thickness ),
  and a leaf names one AABB tree per material whose leaf nodes name partitions ( inner nodes have up to 15
  children, a child index is larger than its parent's );
* a **border** is an edge two triangles share whose surface does not go on beyond it seen from above ( the brink of
  a ledge, a fold ) as a line in the xy plane - normal away from the surface, start along the edge, length, height
  at the start, slope. Edges with one triangle have none: 177 of 20572 open edges in mp_rust have a border, against
  77 to 100 % of the fold edges inside a partition ( by the kinds of the two triangles ), and 2 of 100000 edges whose
  surface goes on. The same rule gives the test ledge its 4 borders. On the server
  the borders made no difference ( the ledge test below gives the same numbers with and without them ), so their
  effect is not shown;
* `triEdgeIsWalkable`: three bits per triangle, set for triangles one can stand on ( normal z of at least 0.7 ). In
  mp_rust 71 % of the bits of level triangles are set and 96 % of the bits of steeper ones are clear: the real rule
  also looks at something else ( the neighbour over the edge ? ) and was not established.

Server probes on the test map `terrain` ( ramp of 26.6 degrees, plateau at z 128, a ledge folded from a mesh ):
a player dropped on the ramp rests at z 65.91 with the normal ( -0.447, 0, 0.894 ) ( the capsule on the plane
through z 64 ), on the plateau at 128.125, 6 units beyond its open edge on the edge ( normal ( 0.397, 0, 0.918 ) ),
further out he falls; walking into the ramp stops at x -449.8; the ledge's wall stops him at x 184.875, its brink
at x 187.84 ( z 90 ) and 192.27 ( z 94 ); bullet traces hit ramp and plateau at their heights with the material's
surface type. Terrain is one sided, as in the game: from below one passes through it.

### The light bake

How the game's lit shaders use the lightmaps was read from the shaders: `codol_viewtool` with
`CODOL_VIEW_DUMP_SHADERS=<folder>` writes the byte code of every shader it draws with, `D3DDisassemble` turns it
into text ( `lm_dfog_r0c0n0s0_sm3`, `lm_sm_sun_dfog_..`, `lm_spot_dfog_..` of the world, `lp_dfog_..` of the
models ). What they do:

| data | how the shader uses it |
|---|---|
| **secondary lightmap** ( A8R8G8B8, two layers of 512 x 1024 on top of each other ) | `light = ( upper.rgb * Nz + lower.rgb * cos( D, N ) )^2`. `N` is the normal of the normal map in tangent space, `D = ( dx, dy, 1 )` with `dx = upper.a * 4.08 - 2.08` along the tangent and `dy = lower.a * 4.0645 - 2.0645` along `cross( normal, tangent ) * binormal sign`. The upper layer is light without a direction, the lower layer light from the direction the two alpha channels give - and both hold **square roots** |
| **primary lightmap** ( L8, 1024 x 2048 ) | how much of the surface's *primary light* reaches the texel. The shader adds the light itself: `colour * max( 0, N . L )` for the sun, `colour * falloff^2 * cone * max( 0, N . L )` for a spot or omni light, with the run time shadow map on top |
| **row 0 of the secondary lightmap** | the falloff of the primary spot / omni lights: read at `u = ( lmapLookupStart + 32 * distance / radius ) / 512`, `v = 0`, and squared. The game's compiler lays a copy of the light definition's falloff picture there ( `codol_bsp_probe lights` prints the row of any map ) |
| **light grid colours** | a byte is `sqrt( light ) / 2`: the model shaders double and square what they read |
| **reflection probes** | square roots, too |

The first three rounds wrote linear values into all of them - the game squared them. A surface that receives 0.33
came out as 0.11: **a room lit by one light entity was nearly black** ( `shots\ed2_a`; with the fix `shots\c4ed_a` ).
The earlier table of this section ( a constant upper layer of 255 gives 0.86 of full sun, the lower layer "adds
little and not linearly", alpha "has no effect" ) was those squares and a direction of ( 2, 2, 1 ) seen without
knowing it. The shipped lightmaps agree with the reading: both layers are used ( mp_vacant: upper layer 128 in the
median, lower 68, alpha around 130 = straight along the normal ).

So the bake computes in linear light and writes what the game reads:

* **Per luxel** what arrives ( rgb ) and where it comes from ( the sum of luminance x direction over ambient
  light, sky, light entities, bounces ). The share that comes from one side - 0 for light from all over, up to
  0.85 for a single light - goes to the lower layer with its direction, the rest to the upper one; on a flat normal
  the two add up to exactly what arrives, a normal map sees the light's direction.
* **Charts and pages.** One chart per face of a surface ( brush face or patch triangle ), one luxel per 16 units
  ( `--fast` 32, `--quality 2` 8 ), a luxel of padding. The charts of a surface go into one page ( a surface names
  one lightmap ); surfaces are shelf packed, tallest chart first, into up to 30 pages, and only when that is not
  enough the luxel grows ( reported ). The 144 room map at `--quality 2`: 4484 charts, 2.8 million luxels, 7 pages.
* **Direct light** per luxel: the ambient light + sky light ( the share of sky directions, cosine weighted, whose
  ray ends on a face with a `sky*` material or leaves the map, times half of `skycolor x skylight` ) + every light
  entity that is not the surface's primary light ( colour x the falloff of the game's falloff picture, squared, x
  `n . l`, the cone of a spot, a shadow ray ) + the sun where the surface has another primary light. Per primary
  texel ( 2 x 2 per luxel ) one ray towards the surface's primary light; a 3 x 3 box filter per chart softens the
  edge.
* **The falloff** of a light entity is the game's: `images/falloff_linear.iwi` ( 32 x 1, L8 ) is read from the
  install's image archives ( once; kept in `codol_bsp_falloff.txt` next to the catalog ), laid into row 0 of every
  lightmap page for the primary lights' shader, and used - squared, as that shader does - for the lights the bake
  bakes. It is 1 up to a tenth of the radius and falls to 0 at the radius; in linear light a light is down to a
  quarter at half its radius.
* **Occluders**: the solid world brushes ( a BVH ) and triangles ( a second BVH ): the tessellation of the patches
  and LOD 0 of the static models. A brush with an alpha tested or decal face ( a fence, a grate ) lets the light
  through: the bake does not read the material's picture, and the solid block of shadow the yard's chain link fence
  threw in the client was the worse mistake. Such a brush gets its shadow from the game's sun shadow map.
* **Bounced light**: every luxel sends 24 cosine weighted rays ( 12 / 48 ) over its side and takes what the luxels
  they end on throw back - half of what arrives there ( direct light and the sun's share; the images of the materials
  are not read: a grey world with a reflectivity of 0.5 ). One bounce, two at `--quality 2`. Shadowed faces are no
  longer black.
* **Light entities** are baked unless they are primary lights ( [Primary lights](#primary-lights) ).
* **Light grid**: a sample at every grid point ( 32 x 32 x 64 units ) that is not inside a brush. A sample is what
  arrives from all around ( the sky directions: sky colour, or what the luxel the ray ends on throws back; the light
  entities and the sun, but for the sample's own primary light ) as an average and a lean per colour, `E( n ) = m0 + n . m1`; the 56 colours of an entry are that
  function at the 56 directions of a colour set - the surface texels of the 4 x 4 x 4 block the model lighting volume
  is looked up in with the normal ( `codol_view` `light_grid.cpp`, from CODOL-AssetExplorer ). So a model is brighter
  on the side the light comes from. Colour sets are deduplicated after dropping low bits ( 2, more when there are over
  65535 ); primary light = the primary light that gives the point the most light ( the sun where it is visible,
  a primary light entity in its reach ), none in the shade. Rows / runs as the shipped grids. At most 3 million points:
  a map whose brushes reach far beyond its spawn points and path nodes gets a grid around those ( reported ).
* **Reflection probes**: probe 0 ( the default, black ), then one per `reflection_probe` entity, or one in the middle
  of the map. A probe is a picture of the scene from its place: per texel of the 64 x 64 cube a ray, which shows the
  sky's colour or what the lightmap luxel it ends on throws back ( the lit scene without its textures ), with mip
  levels. Faces + x - x + y - y + z - z with the texel directions of a Direct3D cube map in world axes: that the game
  reads the cube in world axes is assumed, not shown.
* **Sun shadows at run time** ( the game's shadow map: players, models, the world near the camera ) work on a
  compiled map since the technique sets for hardware shadow maps come along ( [Materials](#materials) ): seen in the
  client. What the GfxWorld carries for them, as the shipped maps ( `codol_bsp_probe sun <zone> <exe>` prints it for
  any map ): every lightmapped surface has flag 1, the sun is the primary light of a surface it reaches, the sun's
  shadow geometry list names static models and no surfaces, the light grid header says `hasLightRegions`, the sun
  block ( flare ) ends with the direction towards the sun, the sun lit cell bits ( header + 420 ) are null. The last
  three were changed to what the shipped maps have while looking for the missing shadows and made no difference in
  the client; shipped mp_rust patched the other way ( `codol_bsp_probe patchworld` ) kept its shadows.

### Light keys

What the compiler reads; everything else on these entities is ignored. A colour gives the hue only: its largest
part counts as 1 ( `0.1 0.1 0.1` is white ), as radiant's light entity says and the suns of the shipped maps show
( mp_vacant: `suncolor 0.84 0.81 0.58`, `sunlight 1.6`, ComWorld colour `1.600 1.543 1.105` ).

**worldspawn** ( the keys of the shipped maps' worldspawn; values are linear light, 1 = a white surface is white ):

| key | default | what |
|---|---|---|
| `sundirection` | `-50 40 0` | pitch and yaw of the sun, as angles: `-90 0 0` is straight overhead |
| `suncolor`, `sunlight` | `1 0.95 0.85`, `1.3` | hue and brightness of the sun ( the shipped maps: 1.15 .. 1.6 ). `sunlight 0`: no sun |
| `ambient`, `_color` | `0.1`, `1 1 1` | brightness and hue of light that is everywhere, also in a closed room ( shipped: 0.01 .. 0.2 ) |
| `skylight`, `skycolor` | `0.4`, `0.8 0.9 1` | light of the sky dome: a surface that sees the whole sky receives half of it ( measured on mp_vacant's sun lit surfaces ) |

A worldspawn without any of them gets the defaults. ( Until the fourth round it got **nothing**: a key that was
missing was read as 0 - no sun, no sky, no ambient light. ) Not read: `diffusefraction`, `sundiffusecolor`,
`radiosityscale`, `sunradiosity`, `maxbounces`, `contrastgain`.

**light** ( radiant's light entity ):

| key | default | what |
|---|---|---|
| `origin` | | |
| `radius` | `600` | where the light ends. Radiant's own default is 200, which leaves a room of 512 units dark at the walls |
| `intensity`, `_color` | `1`, `1 1 1` | brightness at the light, and hue |
| `spawnflags` | `0` | 1 `PRIMARY_OMNI`, 2 `PRIMARY_SPOT`: a primary light; 4 `PRIMARY_SCRIPTABLE`: a primary light a script can change ( below ); 8 `PRIMARY_NOSHADOWMAP` |
| `maxturn`, `maxmove` | `0`, `0` | of a scriptable light: how far ( degrees, units ) a script may turn and move it. They become the record's rotation limit ( the cosine ) and translation limit |
| `target` or `angles` | straight down | where a spot points: at the entity with that `targetname` ( the cone is then a circle of 64 units around it ), or along the angles |
| `fov_outer`, `fov_inner` | 90, 0 ( primary omni: 120, 60 ) | the cone of a spot, in degrees. A light with `fov_outer` ( or a target ) is a spot also when it is only baked |
| `exponent` | `0` | the fade from `fov_inner` to `fov_outer`: 0 a hard edge, 1 linear, higher steeper - as the game's spot shader has it |
| `def` | `light_point_linear` | the light definition: the falloff picture. Any definition a zone of the install has ( below ); a name none has is reported and replaced |

The classes `light_spot` and `light_omni` - what the primary lights of the shipped maps are called in their entity
strings - are read like `light` and are a primary spot / omni light whatever their spawnflags say.

**Light definitions.** The shipped maps name eleven ( `codol_bsp_probe lights` over every map of the install ):

| definition | picture | maps |
|---|---|---|
| `light_point_linear` | `falloff_linear` | 39 |
| `light_point_quadratic` | `falloff_quadratic` | 10 |
| `white_light` | `falloff_linear` | 5 |
| `light_point_linear_nocenter` / `_nocenter_02` / `_nomiddle` | `falloff_lin_nocenter` / `falloff_lin_nocenter_02` ( 128 texels ) / `falloff_lin_nomiddle` | 3 / 1 / 1 |
| `light_point_dark_edges` | `falloff_center` ( coloured ) | 2 |
| `light_no_falloff` | `falloff_none` | 2 |
| `light_linear_bluefade`, `tungsten_lamp` | `falloff_linear_blue`, `falloff_tungsten` ( coloured ) | 1 each |
| `light_point_inverse_quadratic` | `falloff_inverse_quadratic` | 1 |

The install catalog lists which zone has which definition; the compiler reads it there ( picture name, sampler
state ), reads the picture `images/<picture>.iwi` from the image archives - red, green and blue: some are coloured -
and brings it to 32 texels. Every definition a primary light names gets a strip of 34 texels in row 0 of the
secondary lightmaps, in the order of first use from texel 1, 35, 69 .. ( `lmapLookupStart`; the shipped maps: 1, 35,
69, 103 ), and a `lightdef` asset; a picture the common zones do not have comes along as an image asset. Lights that
are only baked use the curve of their definition too. At most 15 definitions per map.

**Scriptable lights** ( `spawnflags 4`, alone or with 1 / 2 ). Found in four shipped maps ( mp_highrise 5,
mp_cargoship_sh 3, mp_brl_derail 3, mp_underpass 1; `codol_bsp_probe ents <zone> <exe> pl#` ): the light is a primary
light like any other - same record, limits 1 / 0 without `maxturn` / `maxmove` - **and stays an entity** of the map,
class `light_spot` / `light_omni`, whose first key `"pl#"` is the index of its primary light. The dedicated server
spawns it ( `light`, `light_spot`, `light_omni` are in its spawn table, `"pl#"` is a spawn field ), which is what a
script's `getEnt( .., "targetname" )` and `setLightIntensity` / `setLightColor` work on. The compiler writes the
same: the entity with `"pl#"` first, the other keys as the mapper gave them. Tested on the zone, and seen in the
client ( [sixth round](#sixth-round-streamed-sounds-edited-shipped-maps-scripted-light) ): a level script takes
the light with `getEnt( "c6lamp", "targetname" )` - its class is `light_spot` - and `setLightIntensity`,
`setLightColor`, `setLightFov` and a new `.origin` change it on the screen. The server's script methods are
`setLightIntensity` / `getLightIntensity`, `setLightColor`, `setLightRadius` ( not above the compiled radius ),
`setLightFov( outer [, inner] )` ( not wider than compiled ) and `setLightExponent`.

`codol_bsp` prints what it read ( `light keys: sun ( 1.30 1.23 1.11 ) from ( .. ), ambient ( .. ), sky ( .. ); 2
light entities, 1 of them primary` ). CoDOL Radiant gives a new `light` the keys `radius 600`, `intensity 1`,
`_color 1 1 1`.

### Primary lights

A light entity with `PRIMARY_SPOT` or `PRIMARY_OMNI` is a primary light of the map: the shader lights the world,
the static models and whatever moves with it at run time ( normal map, specular highlight, shadow map ), and the
lightmap only holds where it reaches. What a map stores for one was measured on the shipped zones with
`codol_bsp_probe lights <zone> <exe>` ( mp_vacant: 5 spots, mp_highrise: 64 lights, mp_nuked: 14 omni lights ) and
is written the same way:

* **ComWorld record**: type 2 ( spot ) / 3 ( omni ), `canUseShadowMap` ( 0 with `PRIMARY_NOSHADOWMAP` or a cone
  over 120 degrees ), exponent, colour, the way back along the beam, origin, radius, the cosines of the half cone
  angles ( the "expanded" one equals the outer, as in every shipped light ), rotation limit 1, translation limit 0,
  the definition's name. An omni light has a cone too ( it places the shadow map ): 120 / 60 degrees downwards.
* **the light definitions** ( [Light keys](#light-keys) ): sampler state 0x62, `lmapLookupStart` 1, 35, 69 .., and
  per definition its falloff in row 0 of every lightmap page: the texel before the strip a copy of the first
  value, 32 texels of curve, a copy of the last.
* **a surface names one primary light**, so **surfaces are cut by the lights' reach**. In the shipped maps
  surfaces of one material in one plane that name different lights meet all over ( `codol_bsp_probe lightcuts`:
  mp_vacant has 1863 such edges at its five spots - 1611 against surfaces without a primary light, 115 against the
  sun's, 137 between two spots; 88 to 96 % of the triangles of a spot's surfaces reach into its radius ). Here:
  every face a light entity can reach is sampled on a grid of 32 units in its plane ( which light gives the point
  the most light, shadows and the sun included ); where the answer changes within a face, the face is cut along
  grid lines around what each light entity wins, and each piece goes to a surface of its light. A floor under two
  spots is lit by both at run time, each on its side; a floor that reaches beyond a lamp's circle names the lamp
  only inside it. The primary lightmap of a surface is its light's visibility; what the other primary lights and
  the sun send to it is baked into the secondary lightmap like any light entity's. The game's compiler cuts along
  the pieces its BSP leaves of a face, not along a grid - the seams are elsewhere, the rule is the same.
* **light grid entries** and **static models** name the primary light that gives their place the most light.
* **shadow geometry**: per light entity the surfaces that name it, ascending, and the static models that do ( the
  shipped lists hold exactly those; the suns list models and no surfaces ).
* **light region**: per light entity **one hull per cell** it lights something in ( the shipped lights have up to
  three hulls: a light that shines through a doorway ), the hull of the light's own cell first. A hull is the
  ranges of what the light lights there ( luxels, grid points, model boxes ) along nine axes ( x, y, z, x + y,
  x - y, x + z, x - z, y + z, y - z; not normalised, relative to the light's origin ) and **up to eight further
  axes** with a unit direction. In the shipped hulls those are the diagonals of the cube ( by far the most frequent in mp_highrise
  and mp_nuked ) and, for a spot, planes that touch its cone: `sin( half angle ) x beam + cos( half
  angle ) x a direction across the beam`, eight of them 45 degrees apart - a point in the cone is never below 0
  along one ( mp_vacant's light 2: six axes of exactly that form with ranges 0 .. 200 = its radius ). The compiler
  takes of these twelve the ones that cut the most off the volume the nine ranges enclose. The game uses the
  region to decide which entities a light's shadow map holds and which light lights a moving model
  ( `C1.exe` 0xA858D0: a box outside every hull of a light is not drawn into its shadow map ). Not made: the
  planes through the light and the edges of a doorway some shipped hulls have.
* the header: `primaryLightCount`, the entity shadow visibility array ( run time ).

In the client ( `shots\c4spot_a`, `c4spot_b`, `c4side_a`, `c4side_c`, `c4omni_b` ): the spot's cone on floor and
wall with the colour of the light, the baked shadows of a brush and of a static model in it, the model and the
player's weapon lit from the light's side; the same for the omni light.

**Shadows of players and models in a primary spot** ( fifth round ). The client draws spot shadow maps, and a
compiled map gets them:

* *The code* ( `C1.exe`, the unpacked client ). The renderer's settings are registered at 0xA7C5xx; most names are
  numbers, in the order of the earlier games: `sm_enable`, `1601` ( sm_sunEnable ), `1602` ( sm_spotEnable, on ),
  `1603` ( sm_maxLights, 4 ), `1604` ( a saved 0 .. 4, 4 in the test client's config ), `1605` ( the least score
  a light needs, 0.5 ), `1606` ( fade time ), `1607` / `1608` ( the two distances of the score, 64 and 0.125 ).
  Every static surface that is drawn marks its primary light as used ( 0xABFE70 ); of the used lights that may
  have a shadow map ( `canUseShadowMap`, sm_spotEnable ) up to four with the best score `brightness x radius /
  distance` get one ( 0xA85330, 0xA86100 ), and their surfaces are drawn with the shadowed technique
  ( 0xAB7400: light type + 3 ). An entity is drawn into a light's shadow map when its box is inside the light's
  sphere, cone and region ( 0xA858D0, 0xA85030 ).
* *The frames.* The harness's frame dump dll counts what a frame draws into ( `frame_stats.ps1` ): on a compiled
  map with one spot and no sun a pass into a depth buffer of 1024 x 2048 with 4 draw calls and 2405 triangles
  ( the surfaces that name the light and the static model in it ); with a script model in the beam 5 draw
  calls and 4793 triangles; with three bots walking about 233 to 239 draw calls and 44722 to 48121 triangles.
* *The pictures.* A script model hanging in the beam throws its shadow on the wall behind it ( `c5dyn_along` );
  bots walking through the beam throw theirs, standing and crouching ( `c5bots_c8_crop`, `c5bots_c1` .. `c12`:
  the silhouettes move from frame to frame ).

The round before looked for the shadow of the player himself, standing in the beam. There is none: the first
person view has no body, in the spot's light as in the sun's ( `c5side_beam`; third round `cv9_3` ). On shipped
mp_vacant nothing was looked at again.

What is missing compared to the game's compiler is listed under [Limits](#limits-and-what-is-not-done).

### Vision file, sun flare

* **`vision/<map>.vision`** is asked for by the client when a level starts ( "couldn't open 'vision/mp_x.vision'" in
  its log without one ). In 1.1.1.13 the settings are named by **number** ( the dvars have numeric names ): `511`
  r_glow, `516` r_glowRadius0, `512` r_glowBloomCutoff, `513` r_glowBloomDesaturation, `514` r_glowBloomIntensity0,
  `520` r_filmEnable, `522` r_filmContrast, `521` r_filmBrightness, `523` r_filmDesaturation, `524`
  r_filmDesaturationDark, `525` r_filmInvert, `526` / `527` / `528` r_filmLightTint / MediumTint / DarkTint, `529`
  r_primaryLightUseTweaks, `530` / `531` r_primaryLightTweakDiffuseStrength / SpecularStrength ( `517`, `515`, `518`,
  `519`: the second glow radius, intensity and the sky bleed ) - read off `vision/default.vision` and
  `vision/mp_vacant.vision` of common_mp, which have the layout, order and values of the earlier games' files.
  The compiler writes a default ( a little glow, the film tweaks on without changing a colour ), or the mapper's
  file: `--vision <file>`, else `<map>.vision` next to the `.map`; `package_map.ps1 -Project` takes
  `mod\<project>\vision\<map>.vision`. The file may name the settings ( `r_filmContrast "1.25"` ) or number them;
  what the compiler does not know is reported and left out. Seen in the client: the mapper's file changes the
  picture ( `c4fx_props`: contrast 1.25, more glow ) against the default ( `c4fx2_props` ), and the log line is gone.
* **The sun's sprite and flare** are the sun block of the GfxWorld; `sun/<map>.sun` is the file the game's compiler
  makes it from, and the shipped map zones carry it as a rawfile. Its settings are numbers too ( `1704`
  r_sunsprite_shader .. `1724` r_sun_fx_position, in the order of the earlier games' file ), checked against the
  blocks of the same zones ( `codol_bsp_probe sun` ): mp_killhouse_sh has `1705 "23.9"` and a sprite size of 23.9,
  `1707 "343"` / `1709 "528"` and flare sizes of 171.5 / 264 ( half ), `1708 "71"` / `1710 "10.4909"` and the
  cosines 0.3256 / 0.9833, `1712 "2.8"` and 2800 ms, `1724 "351.691 181.278 -18.5703"` and the direction
  ( -0.989 -0.022 0.145 ). A map with a sun ( `sunlight` above 0 ) and a sky face gets a default flare ( materials
  `sun` and `sun_flare`, copied from the install; sprite 16, flare up to 600 at 2 degrees, alpha 0.5, no
  blinding ) where its sun stands; `--sun <file>`, `<map>.sun` next to the `.map` or
  `mod\<project>\sun\<map>.sun` replace it ( names or numbers ). Block and rawfile are written. Seen in the
  client: the sun in the sky of the yard ( `c4fx_sun`; before: `cv5_sky` ).

### Minimap, scripts, the files around a map

* `--minimap <file.tga>`: the map from above ( 512 x 512, north = + y up ) as the lightmaps light it; rays go through
  sky brushes, a roof hides what is under it; where nothing is, the picture is see through. The map gets the two
  `script_origin`s with the targetname `minimap_corner` the picture fits and `"northyaw" "90"` when it has no
  corners of its own ( they are left out of the leak test ).
* Entities `script_fx` ( `fx`: the effect as `loadfx` names it, `angles`, `delay`: seconds between repeats, none: a
  one shot effect that starts with the map, `soundalias` ) and `script_sound` ( `soundalias`: a looping sound ) are
  no entities of the game: they become `maps/createfx/<map>_fx.gsc` in the form the game's createfx tool writes
  ( `createOneshotEffect` / `createLoopEffect` / `createLoopSound` ) and `maps/mp/<map>_fx.gsc` ( `loadfx` per
  effect ), both rawfiles of every zone. **The client reads the text of these two files** when a level starts and
  makes the effects and looping sounds itself; it wants them as the game's createfx tool writes them. The placement
  has to begin with the line `//_createfx generated. Do not touch!!` - with the compiler's own first line the
  client logged "Unexpected text .. when trying to find '//_createfx generated. Do not touch!!' in map's effect
  file" and placed nothing ( the effects of the third round's pictures came from the server, which ran the
  placement as script ). So both files now have the shape of the shipped ones line for line, CR LF included, and
  like there the script only runs the placement in a developer build that asks for server side effects -
  otherwise every effect would be there twice. An effect has to be in a zone that is loaded ( the common zones:
  `fire/firelp_small_pm`, `smoke/thin_black_smoke_m` .. ) or in the map zone ( `package_map.ps1` carries a
  project's effects there ).
* `--scripts <dir>` also writes those two and the level script `maps/mp/<map>.gsc` ( `_fx`, `_load`, the minimap
  setup ) to a folder: the dedicated server reads level scripts from its script folder, never from a zone.
* `tools/package_map.ps1 -Map x.map -Name mp_x -Out <dir> [-Assets zone.ff ..] [-Loadscreen picture]` puts a map
  together: `codol_bsp` ( with minimap and scripts ), then `codol_assetc --base` adds the material
  `compass_map_<map>` ( technique set `2d`, the picture inline ) to the map zone, builds `<map>_load.ff` with
  `$levelbriefing` / `$defeatbackdrop` showing the picture given, and writes `presence\`: `INSTALL.txt` ( where
  every file goes ), the row of `mp/comaplist.csv`, the entry of `mp/basemaps.arena`, the localized name. The three
  list files live in one patch zone the install's own `usermaps\build_usermaps_patch.py` rebuilds from the folders
  under `usermaps\`; it makes the row, the arena entry and the name for any `usermaps\<map>\<map>.ff` by itself, so
  the generated files are for reference and for a display name of one's own. Nothing is installed by the script.

## Validation

Every compile ends with `validate_zone` and writes nothing when it fails:

* `check_server_collision` ( `codol_mapc` ): the clip map as the dedicated server's decoder reads it.
* `validate_render.cpp`, on the zone as the game's loaders build it: names and the shared plane array; the cell tree
  is a well formed tree and every cell is a leaf of it; every surface's vertices, indices, lightmap, probe and light
  are inside their arrays; `sortedSurfIndex` is a permutation; every cell's AABB tree ( runs of a node = runs of its
  children, boxes hold children and surfaces, every static surface in exactly one leaf ); portals lead to another cell
  and lie on their plane; cells list a probe other than 0; the light grid's rows, entries and colours fit together;
  lightmap images have pixels; brush model ranges; the sun lit cell bits are null; ComWorld ( light 0 none, light 1 a
  sun with a unit direction );
  GameWorldMp glass data present; MapEnts starts with the worldspawn and has `mp_global_intermission`; the animation
  list exists.
* The stages check themselves: leaf centres against their portals, lightmap charts do not overlap.

`codol_bsp_tests` ( 411 checks with `--assets`, 0 failed on this machine ): writes the test maps with `codol_edit` and compiles them;
per map: validation, the zone read by `codol_zone` and written again is byte identical, tree walk == brute force on
3000 random segments ( `clip_trace` ), `codol_edit::import_fastfile` of the compiled zone gives back the map's brushes;
box room: traces hit floor, wall, ceiling, crate, stacked crates where the map has them; two rooms: 3 cells, 4
portals, more than 60 hidden faces; the same map with a gap in a wall: leak with a path that starts inside and ends
outside; content map: brush models, portal brush ( 2 cells, 2 portals ), patch, the static model's draw instance
( cull distance ) and clip entry ( inverse matrix ); a model nested in mp_rust's `gfx_map` placed in the box room; the
terrain map: 144 triangles that lie on the ramp and face up, 4 borders, the server check's terrain counts, the
minimap picture and the generated scripts; the light bake's notes; a 144 room map: more than 1000 brushes and 20
cells. With `--assets <project zones>` ( the two test zones ) also the modder material map and the yard. Since the third
round: the openings found ( two rooms 2, the yard 10 - 3 cells, 20 portals -, the 144 rooms 264 - every room a cell ),
and per zone that every lit technique set has its hardware shadow map sibling or a warning. Since the fourth round,
read back from the zone as the game reads it: a room with one keyless light and a worldspawn without light keys
( the defaults in the report, both lightmap layers used, the floor under the light between 0.4 and 0.9 and more
than 1.6 times the corner, the falloff row full at texel 1, never rising, empty at texel 32, the vision rawfile
with numbers, the two effect files in the shipped shape ); the worldspawn keys ( `_color` tints the ambient light,
colours count by their largest part ); a primary spot ( ComWorld record field by field, the light definition, a
surface that names it, its shadow geometry = the surfaces that name it, one region hull that holds the floor under
the light and not the far corner, light grid points that name it, its light not in the secondary lightmap ) and a
primary omni light; a mapper's vision file with names, numbers and an unknown setting; the default sun flare
( block valid, two materials, sprite size, direction of the ComWorld sun, the `.sun` rawfile ). Since the fifth
round: two primary spots over one floor, the second scriptable with `light_point_quadratic` ( the floor's pieces
name the light of their side and meet in the middle; both definitions with their strips at 1 and 35, the quadratic
curve below the linear one; the picture of the second definition as an image of the zone; the record's limits
from `maxturn` / `maxmove`; the entity with `"pl#" "3"` in the entity string and no other light; a hull with one to
eight unit axes that holds the floor under the light and not a point beside the cone ); a definition no zone has is
replaced with a warning; and T-junctions - counted on the zones of the box room, the two rooms, the content and
the terrain map and the two spots: none, while the two spots compiled without the repair have some.

`codol_mapc` `check_server_collision` reads the terrain as the server's decoder does ( partition ranges, vertex
blocks, border runs, AABB tree nodes ); the shipped maps pass it.

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

Not needed to enter the map: a `mp/comaplist.csv` row, an arena entry and localized names ( the map does not appear
in the create-room list without them ), a minimap material, a loading picture of its own. `tools/package_map.ps1`
makes the last two ( inside the zones ) and writes down the first three and where everything goes
( [Minimap, scripts, the files around a map](#minimap-scripts-the-files-around-a-map) ).

What a map needs in its entities for the server's scripts ( the test maps have them ): spawn points of the gametype
and `mp_global_intermission`; `node_pathnode` entities for bots ( the server links them itself ); a `care_package`
script_model that targets a script_brushmodel ( `_airdrop.gsc` reads it at every start; with `developer 1` its absence
stops the server ); `script_origin`s named `heli_start` ( targets the way in ), `heli_loop_start` ( targets a closed
loop ), `heli_leave`, `heli_crash_start` - without the first two `_helicopter.gsc` sets nothing up and a harrier
called in by a bot runs into script errors ( seen on the yard without them: 12 "script runtime error" lines in
3 minutes; none with them ). `tests/test_maps.hpp` `airdrop_prefab` / `heli_prefab`.

| run | result |
|---|---|
| box room, server, `developer 1` | `CoDOL native collision installed: maps/mp/mp_boxroom.d3dbsp, 2 leaves ..`, common world 3 lights, glass 0 pieces. `codol_sweepprobe`: floor at z 0.125, walls at +-496.875 ( 512 - 15 - 0.125 ), ceiling 249.875 + 70, crate top 64.125, stacked crates 128.125, crate side 112.875 + 15, low crate 32.125 |
| box room, server, 8 bots, 100 s | all 8 play, 104 kills; the only new error lines: two path nodes of the test map inside crates |
| box room, client ( name `mp_boxroom` ) | Multiplayer -> Start -> Search -> Ready enters the map; spawned; walking into the -x wall stops at x -497; dropped onto the crate: z 64; onto the stacked crates: z 128; shots fired at a crate ( impact effect ) |
| content map, client ( name `mp_content`; baked light, 2 cells with a portal brush, patch, door, trigger, script_model ) | enters the map and stays connected; dropped onto the patch platform: z 24; walking into the door ( brush model ) stops at x 43 ( face at 28 + 15 ); walking through the doorway ( the portal ) reaches room 1. **No picture**: every capture of the game window was black during this run ( also of the menus; the cause - probably the display being off - was not established ), so how the baked light and the sky look in the client is not known |
| two rooms, client ( `devmap mp_tworooms` over rcon while connected ) | the client follows the map change; walking from room A through the corridor into room B ( x -500 -> 651 ) crosses all five cells ( three since the third round ), the wall beside the doorway stops the player at x -271. No picture ( as above ) |
| mp_rust's collision brushes as a map ( 3226 brushes ), server, 8 bots | collision installed, 747 leaves; bots play ( 30 kills in 100 s; the map has no terrain, so some path nodes are unconnected ) |
| 144 rooms ( 1421 brushes, 106 cells ), server, 8 bots, 100 s | collision installed, 309 leaves; all 8 play, 109 kills; new error lines: path nodes of the test map ( 6 in crates, 1 unconnected ) |

**Second round** ( server only; `png\` = `build-editor\bsptest\png\` ):

| run | result |
|---|---|
| content map + a `codol_assets` crate + two models nested in mp_rust's `gfx_map` ( `mp_s1models` ) | compiles ( 79 assets ), all four models drawn by `codol_viewtool` ( `png\s1_models.png` ); server: collision installed; `bulletTrace` hits the plastic case ( surface plastic, normal -0.866 -0.5 0 along its forward axis ), the project crate ( top 48.125, side at -467.06 -207.06 ), the barrier ( concrete, z 33.7 ), the gas pump at scale 1.5 ( metal, top 77.97 ); the floor beside them at 0.125 |
| sky room with models ( `mp_s4sky` ) | baked shadows of crate, pump and palm trunk on the floor, the sun lit crate lit from the grid ( `png\s1_sky_models.png` before the bounce, `png\s4_sky_bounce.png` with it: the shadow sides of the brush crates are no longer black ) |
| modder materials ( `mp_s2modder` ) | `png\s2_modder.png`: brick wall with its normal map, see through fence with the wall behind it drawn, decal; server: 4 bots join; `bulletTrace`: fence metal at y 191.875, wall brick at x -511.875, through the decal to the floor |
| terrain ( `mp_s3terrain` ) | `png\s3_terrain.png`; the probes listed under [Terrain](#terrain); with and without borders the same numbers |
| 144 rooms at `--quality 2` ( `mp_s4big` ) | 7 lightmap pages, `png\s4_big_pages.png` |
| the yard ( `mp_yard`, packaged by `tools/package_map.ps1`: 89 assets, minimap material, own loading picture, level script in the server's overlay ) | `png\yard_overview.png`, `png\yard_inside_west.png`, `png\yard_minimap.png`; server, 8 bots, 6.2 minutes: collision installed ( 18 leaves ), 259 path nodes, 0 in solid; 185 kills, every bot 20 to 31; the match ended once and the map restarted ( 2 `InitGame` ); 0 script runtime errors, no error line that a stock map does not also give ( `steam_api.dll`, empty `sv_mapRotation` ) |

### Third round: the client, with pictures

Pictures of the game are taken with a `d3d9.dll` of the harness in the test client's folder
( `tests/harness/d3d9_shot` ): it hands everything on to the system's and writes the back buffer of a frame when
asked through a file. On this machine the display is off, Windows then drops the frames of a windowed Direct3D 9
program, and a screen copy of the game window is black - while the game renders. `start_client.ps1` puts the dll
there, `shot.ps1` asks for a frame, `stop_all.ps1` takes it away; `join_match.ps1` goes Multiplayer -> Start ->
Search -> Ready, `devmap`, class, spawn; `client_views.ps1` places the player ( `setviewpos` over rcon ) and renders
the same views with `codol_viewtool`. Pictures: `build-editor\bsptest\shots\` ( `<name>_s.png` ), viewtool
renders `build-editor\bsptest\png\<name>_view.png`.

| what | the client showed | pictures |
|---|---|---|
| loading | the yard's own loading picture with the map name | `cv_load1` |
| the yard from outside | sky, palms, both buildings, crates, fence, mound, the smoke and fire effects of the generated `_fx` script; baked shadows of buildings, brush crates and models, bounced light in the shade; the view model brighter in the sun than in the shade ( light grid ), warm in the west building and cold in the east one ( the two `light` entities ) | `cv_load5`, `cv1_overview`, `cv1_mound`, `cv5_roofedge`, `cv5_sky`, `cv1_inwest`, `cv1_ineast` |
| against `codol_viewtool` | the same geometry, lightmaps and shadows; the client is brighter and warmer ( its tone curve and glow ) - also on shipped mp_killhouse_sh ( `ref_a` against `png\ref_a_view` ), where sun lit concrete is as close to white as in the yard ( `ref_d` ) | `cv1_overview` / `png\cv1_overview_view`, `cv1_westdoor` / `png\cv1_westdoor_view` |
| **cells** | from inside the west building doors and windows were black: only the cell next to the room was drawn ( `cv1_inwest`, the same in `png\cv1_inwest_view` ). After the fix ( openings found in the open space, 3 cells / 20 portals ) the yard is seen through every opening | `cv2_inwest`, `join_5_spawned` |
| minimap | the rendered picture on the HUD compass and in the class menu, the player's arrow where he stands, north up | `cv_esc`, `cv_spawn`, any later picture |
| static models | plastic case, barrier, gas pump, palms ( models nested in a shipped `gfx_map` ) and the `codol_assets` crate at 1 x and 2 x: placed, scaled, lit, with baked shadows; bullets on the barrier give the concrete puff and marks ( `cv3_barrier_fire`, `cv3_barrier_after` ) | `cv1_mound`, `cv2_crate2`, `cv5_moundtop` |
| **shooting the `codol_assets` crate** | first nothing ( the test zone was older than the surface types ), then, with the surface type wood: the client crashed ( access violation near 0x642E59, twice, right at the shot ). `codol_assets` wrote no collision trees for the surfaces ( see its README ); with them: wood splinters and bullet marks | `cv3_modelcrate_after` ( nothing ), `cv4_modelcrate_fire`, `cv4_modelcrate_after` |
| modder materials | brick wall with normal and specular map, the alpha tested fence ( see through, stops the player ), the blended decal on the ground | `cv5_wallgraze`, `cv1_fence`, `cv6_walk2`, `cv5_decal` |
| the fence's shadow | a solid block of baked shadow under the chain link fence ( `cv1_mound` ); after the fix none ( `cv2_fenceshadow` ) |  |
| terrain | drawn; walking ( W held ) from x -330 up the mound: z 62.6 at x -52, on over the top and down to the fence at x 179 | `cv6_walk1`, `cv6_walk2` |
| **dynamic sun shadows** | none: bots and bodies without a shadow, the shadow edges the stair steps of the lightmap ( `cv7_bot2`, `cv8_bot4_crop` ), while shipped mp_rust in the same client has shadow map edges ( `ref2_bot2`, `rx0_2` ). After the fix ( `_hsm_` technique sets ): smooth edges, the player's shadow on the ground | `ex_h_1`, `cv9_3`, `cv10_3` ( killcam ) |
| `light` entities | they light players and models through the light grid ( warm / cold view model ); they are no primary lights, nothing dynamic casts a shadow in them | `cv1_inwest`, `cv1_ineast` |
| reflections | nothing black or mirrored on the metal walls and the weapon; the probes are pictures of the lit scene without textures, so there is little to judge | `cv9_3`, `cv5_wallgraze` |
| the editor's room ( `mp_edroom`, built by the editor before these fixes: 5 cells in one room ) | loads, spawn, the room is drawn and lit by its one light - dark: a `light` without keys reaches 300 units; recompiled with this compiler ( 1 cell ) it looks the same | `ed1_a`, `ed1_c`, `ed2_a` |
| T-junctions | no crack or sparkle found along the joints of the yard's wall pieces at four times zoom ( 8 x anti-aliasing ) | `cv9_3_crop` |

Also seen: `Error: Missing soundalias` for a `script_sound` whose alias no loaded zone has ( the test map now names
one of common_mp ); `vision/<map>.vision` is asked for and not there ( no vision file is written ); the client once
crashed in its own in-game menu code while nothing of the map was involved.

### Fourth round: light, primary lights, vision, sun, effects

The same harness ( loopback, the test user, the frame dump dll in the shadow client only ), with the server at
`developer 0`: at `developer 1` this server stops a match on a script error of its own at the first spawn with a
class ( `codol_getLoadoutKillstreak: mp/coMarketplaceItems.csv is not mounted` ), which has nothing to do with the
map. Pictures: `build-editor\bsptest\shots\` ( `<name>_s.png` ), the maps and the asset project of the test under
`build-editor\bsptest\c4\`.

| what | the client showed | pictures |
|---|---|---|
| **the editor's room** ( `mp_edroom`: closed room of 512 units, one `light` without keys, a worldspawn without light keys ) | before: nearly black ( `ed2_a` ). Two causes: the lightmaps held linear light, which the shaders square; and the missing worldspawn keys were read as 0 instead of their defaults ( no ambient light ). After: floor, walls and ceiling lit, brightest under the light, the crate's baked shadow | `c4ed_a`, `c4ed_b` |
| **a primary spot** ( `mp_c4spot`: `spawnflags 2`, `fov_outer 100`, `fov_inner 50`, `exponent 1`, pointing down, colour 1 0.9 0.7, plus a baked blue light ) | the warm cone on the floor and up the wall, the baked shadow of the brush crate and of a static model in it, the model lit from above | `c4spot_a`, `c4spot_b` |
| **a primary omni light** ( `mp_c4omni`: `spawnflags 1` ) | the whole room lit by it at run time, the crate's shadow from its visibility in the primary lightmap, the static model lit | `c4omni_a`, `c4omni_b` |
| **a primary spot from the side** ( `mp_c4side`: on a wall at 96 units, `angles 12 0 0`, `fov_outer 80`, radius 800 ) | the beam across the room: floor, far wall and the static model in it lit, the rest of the room dim; the baked shadows of the crate ( on the far wall ) and of the model; the view model bright when the player stands in the beam | `c4side_a`, `c4side_c` |
| a player's shadow in a primary light | **not seen**: standing in the sideways beam with the light behind, the floor ahead shows the crate's baked shadow and none of the player ( `c4side_a` ). Under the spots of shipped mp_vacant no player shadow could be made out either ( `c4vac_c`, `c4vac_d`, `c4vac2_sheet` ) - the floor there is too uneven to tell, so whether this client draws spot shadow maps at all is open | |
| **the yard with the new bake** ( `mp_c4fx` ) | the same scene as in the third round with the light in the game's units: shadows lit by the sky, the bounce visible; the sky light halved against the first try of this round ( `c4fx2_over`: too bright a shade ) to what shipped mp_vacant holds | `c4fx3_over` ( third round: `cv1_overview` ) |
| **vision file** | the log line "couldn't open 'vision/<map>.vision'" is gone; a mapper's file next to the `.map` ( contrast 1.25, more glow ) changes the picture against the default | `c4fx_props` / `c4fx2_props` |
| **sun flare** | the sun stands in the sky where the worldspawn puts it, with its glow | `c4fx_sun` ( third round: `cv5_sky` ) |
| **a project's effect** ( `codol_test/spark_burst`, compiled by `codol_assets`, placed with `script_fx`, `delay 1.5` ) | its smoke puffs appear and rise, again and again; made by the client from the two effect files ( no parse error in its log any more ) | `c4fx_fx_sheet` ( six frames ) |
| **a project's animations** ( a crate with `codol_test_spin`, a two bone box with `codol_test_lid`, `scriptModelPlayAnim` ) | both models are drawn where the script spawns them; **neither moves** ( six frames over three seconds are the same ). The server reports no error. The hanging clothes of shipped mp_crash_tropical, which the game's own `_animatedmodels` script animates the same way, **do not move either** on this server ( five frames, mean difference 0.02 of 255 ): script model animations do not reach the client here, whatever the map | `c4fx_anim_sheet`, `c4crash_clothes_sheet` |
| **a project's sounds** ( `playLoopSound`, `playSound`, a `script_sound` with the project's alias ) | nothing can be heard on this machine. The client's log has no "Missing soundalias" and no load error for the map | |
| **bullets on the alpha tested fence** | six shots at the chain link fence: the impact puff at the fence, no crash; the fence throws its pattern as a run time sun shadow | `c4fence_before`, `c4fence_fire`, `c4fence_after` |
| **terrain borders** | walking ( W held ) across the west edge of the mound and over it ( x -300 -> 167, z 23 ), along its south and north edges ( 677 and 673 units, 5 and 15 off the line ), diagonally over a corner ( to 175 175, z 7 ) and down its north side: nowhere stuck, nowhere through. No seam between mound and ground | `c4terr_edge`, `c4walk_*` |
| **Esc / class / resume, with deaths in between** ( the sequence before the one client crash of the third round ) | eight rounds, four deaths and respawns: the client stays up. The crash did not come back | `c4respawn_end` |

### Fifth round: light cuts, spot shadows, T-junctions, sound

The same harness. New in it: `frame_stats.ps1` ( the frame dump dll now also counts, per pair of render target and
depth buffer, the draw calls and triangles of the frame that was just drawn - which shows a shadow map pass and
what is in it ), `audio_peak.ps1` ( the peak level of the client's audio session over some seconds, from Windows'
own meter: what the game sends to the sound card, on a machine nobody listens to ), and `stop_all.ps1` was seen
to take the dll out of the test client's folder after every run ( checked each time: it was gone ). Pictures:
`build-editor\bsptest\shots\`, maps and sources under `build-editor\bsptest\c5\`.

| what | the client showed | pictures / numbers |
|---|---|---|
| **two primary spots over one floor** ( `mp_c5two`: the box room, two spots pointing down 440 units apart, the second with `light_point_quadratic`, bluish, `spawnflags 6` ) | 3 faces cut into 13 pieces, 10 surfaces became 14. Both pools of light on the one floor at run time, the white one and the dimmer blue one, meeting between the lamps; no line where the pieces meet - what looks like one along the middle is where the white light's cone ends ( the brightness falls evenly to the room's level there: profile of `c5two_top` ) | `c5two_both`, `c5two_top`, `c5two_along`, `c5two_seam` |
| the same in `codol_viewtool` | the cone of a cut floor without a seam | `png\c5_spot_a`, `png\c5_spot_b` |
| **shadow of a script model in a primary spot** ( `mp_c5dyn`: the fourth round's sideways spot, a `script_model` hanging in the beam ) | its shadow on the wall behind it, next to the baked shadow of the static model below; the frame has a pass into a 1024 x 2048 depth buffer: 4 draw calls / 2405 triangles without the script model, 5 / 4793 with it | `c5dyn_along`; `c5side_beam` ( without ) |
| **shadows of players in a primary spot** ( `mp_c5bots`: the box room with path nodes, a spot along the floor, no sun, 3 bots ) | the bots' shadows on the far wall, standing and crouching, in other places from frame to frame; the shadow pass grows to 233 .. 239 draw calls / 44722 .. 48121 triangles | `c5bots_c8_crop`, `c5bots_c1` .. `c5bots_c12`, `c5bots_c_sheet` |
| the player's own shadow | none, in the spot's light as in the sun's: the first person view has no body. That is what the fourth round was looking for | `c5side_beam` |
| **T-junctions, terrain against brushes** | the yard packaged again ( 226 vertices added to 73 faces; with the project's effect, models and sounds ): buildings, crates, fence and the mound as before, no open pixels along the joints of the wall pieces or where the mound meets the ground. A missing pixel is easy to overlook, so it is also counted: on the test maps no vertex of the world is left in the middle of an unshared triangle side ( the tests ) | `c5yard_over`, `c5yard_edge`, `c5yard_mound`, `c5yard_props` |
| **a project's loaded sounds** ( `mp_c5snd`: a closed room, the project's aliases carried into the map zone, a level script that plays them on request ) | measured at the client's audio session, 4 s each: silence 0.0000; `playLoopSound( "codol_test_hum" )` on an entity 2 m away 0.1371 ( mean 0.125 ); the entity moved 6000 units up 0.0001; back 0.1371; `stopLoopSound` 0.0000; `playSound( "codol_test_beep" )` 0.1682. So an alias of a project reaches the mixer, with its distance falloff | `audio_peak.ps1` |
| **streamed sounds** | see below | |

**Streamed sounds: what was found.** `codol_assetc --iwd` wrote a plain zip ( `zz_<project>_sounds.iwd` ) for the
client's `main\`. The client never opens it: of the archives in `main\` it holds all of its own open
( `iw_config_NN`, `iw_image_NN`, `iw_imagestripped_NN`, `iw_model_NN`, `iw_sound_NN` and the `localized_` ones ) and
not ours, under that name or as `iw_c4fx_00.iwd` - an exclusive open of the file succeeds while the game runs.
With the archive or without it a streamed alias plays nothing and logs nothing. An archive named like the game's,
`iw_sound_04.iwd`, **is** opened ( held by the client ) - and those the game decrypts, so `codol_assetc` now
writes an archive whose name holds `iw_sound`, `iw_image` .. encrypted as the game's are ( `codol_assets/README.md` ).
With that archive in `main\` the client finds the file - and ends: `ambientPlay` of the streamed `.wav` alias stopped
the client with an access violation at 0x6286DE ( the engine's `va()` on a thread that has none of the engine's
thread data: something is reported from the stream's thread ), `playLoopSound` of a streamed `.mp3` alias ended it
without a word. Without the archive both calls do nothing. Streams as such work in this client: shipped mp_vacant,
whose level script calls `ambientPlay( "ambient_mp_vacant" )`, has sound all the time while the player stands still
( peaks 0.07 .. 0.13 over three windows of 8 s ). What our archive lacks was not found: the game's entries are all
deflated and carry extra fields ( NTFS times, and 20 bytes with the tag 0x9999 in the local header ), ours are
stored without any. So **a project's streamed sounds do not work yet**: under a plain name the archive is ignored and
the alias silent, under an `iw_sound` name the client ends when the alias plays. ( **Solved in the sixth round**,
below: the entries have to be deflated and the server has to have the archive too. ) Also seen:
`ambientPlay` of a *loaded* alias drops the client to the menu ( `ERROR: 19 codol_test_hum codol_test/hum.wav` ), and
`playSound` of a looping alias is a script error on the server.

Not looked at in the client this round: a scriptable light changed by a script, the other light definitions
( only `light_point_quadratic` ), an omni light's shadows, terrain borders ( nothing changed there ).

### Sixth round: streamed sounds, edited shipped maps, scripted light

The same harness; what is new in it is under `tests/harness/round6/` and at the end of this section. Pictures:
`build-editor\bsptest\shots\c6*`, sources and logs under `build-editor\bsptest\c6\`.

**Streamed sounds: why the client ended, and that they play now.** Two things, both read from the unpacked client
and then tried:

* *A stored entry cannot be sought in.* Miles reads a stream through the game's file callbacks ( registered at
  0x73DB40: open 0x73D2C0, close 0x73D2E0, seek 0x73D300, read 0x73D380 ). The seek ( 0x615220 ) of a file inside
  an archive opens the entry again and reads up to the wanted position **into a null pointer**
  ( `unzReadCurrentFile( file, 0, position )`, 0x6495D0 ). For a deflated entry the game's inflate leaves the
  output out; for a stored one the copy loop writes to address 0 - the access violation at 0x649719 that both
  crash logs of the fifth round's test client name ( the stack: `AIL_open_stream` -> the file callback ). Every
  entry of the game's own archives is deflated. `codol_assets` `write_iwd` now deflates every entry, also one
  that gets longer by it.
* *The server has to have the archive.* The client's file open for another thread ( 0x614550 ) accepts a file of
  an archive only when the archive is "pure" ( 0x613B80: its checksum is in the list the server sent ), a
  localized one, or one marked to skip the check; else it ends with "Impure client detected. Invalid .IWD files
  referenced!" - through the engine's `va()` ( 0x6286D0 ), which on Miles' thread has no thread data: the other
  crash address of the fifth round, 0x6286DE. The dedicated server lists the archives of *its* `main\`. The fifth
  round had put the archive into the test client only. ( Read from the code; that an archive missing on the server
  gives exactly this crash was not tried again. )
* *Names.* The client's scan of `main\` ( 0x6166E0 ) takes `iw_` + nothing or `imagestripped_` / `image_` /
  `sound_` / `model_` / `config_` + two digits up to 23 + `.iwd`. That is why `zz_x_sounds.iwd` and
  `iw_c4fx_00.iwd` were never opened. `codol_assets` has `game_mounts_iwd` and `free_sound_iwd` for it; the
  editor's install writes the package's plain archive as `<game>\main\iw_sound_NN.iwd` ( 23 downwards ).

The test ( `round6\build_snd.ps1`, `serve_package.ps1`, `snd_sequence.ps1`; `mp_c6snd`: the box room, a project of
three streamed aliases, its level script calls `ambientPlay( "c6_wind_amb" )` at the start of the map; the archive
as `iw_sound_23.iwd` in `main\` of the server's base and of the test client ). The client's audio session, 5 s each:

| what the level script did | highest peak | mean |
|---|---|---|
| nothing: the map's ambient track ( a streamed `.wav` ) since the map started | 0.1899 | 0.1252 |
| `ambientStop` | 0.0000 | 0.0000 |
| `ambientPlay` of the `.wav` alias | 0.1897 | 0.1316 |
| `ambientStop` | 0.0000 | 0.0000 |
| `ambientPlay` of an `.mp3` alias | 0.1792 | 0.1247 |
| `ambientStop` | 0.0000 | 0.0000 |
| `playLoopSound` of a streamed `.mp3` alias on an entity 2 m away | 0.2165 | 0.1249 |
| the entity moved 6000 units up | 0.0624 | 0.0208 |
| moved back | 0.1345 | 0.0816 |
| `stopLoopSound` | 0.0000 | 0.0000 |

The client stayed up through all of it. For the server's base the harness had to change: `make_base.ps1` makes
`main\` a folder of hard links to the game's archives ( it was one junction into the install ), so that
`put_iwd.ps1` can put a test archive next to them - in the base and in the test client, never in the install.

**An edited shipped map with a project's assets** ( `round6\editor_mapc.ps1`, `serve_stock.ps1` ). In the editor:
shipped mp_rust opened, a project `mod\mp_rust` given a material, a model ( two LODs ), sound aliases, an effect
and a level script ( the shipped one with a few lines added ), built; the model placed as a `misc_model` and as a
`script_model`; File > Compile map, which now hands the project to `codol_mapc` ( `--assets`, the rawfiles ). The
report: 1 static model added, the model taken with 4 assets it depends on, 1 effect, 3 loaded sounds, 1 curve and
4 alias lists carried, the level script as a rawfile, the file exactly as long as the shipped one.

| where | what was seen |
|---|---|
| dedicated server | collision installed ( 758 leaves ), 14 primary lights, the project's level script from `compiled\mp_rust.scripts\` in front of the server's own: `loadfx`, `precacheModel`, `spawn` + `setModel` run, the script reports ready; no script error but the server's own ( `mp/coMarketplaceItems.csv` at the first spawn with a class, as in every round ) |
| client | enters the map; the three crates of the project stand in the yard - the static model, the entity, the one the script spawns - with the project's material ( `c6rust_a_1`, `c6rust_b_2` ); the effect's smoke puffs and a spark above them, other in every frame ( `c6rust_fx_sheet` ); the project's loop sound on the crate: mean 0.2014 while it plays, 0.0217 after `stopLoopSound` ( the map's own sounds go on ) |

The zone replaced the link to the stock zone in the two shadow zone folders and the links were put back after the
test ( `put_zone.ps1`: a file in the way is removed first - the link, never the install's file - and then copied;
`compile_and_serve.ps1` goes through it too ). The install's `mp_rust.ff` has its date of 2014.

**A script changes a scriptable light** ( `mp_c6lamp`, `round6\make_light_maps.py`: the fifth round's two spots over
one floor; the first `spawnflags 6`, `targetname c6lamp`, `maxmove 300`, `maxturn 45`; the second with the falloff
definition `tungsten_lamp`; `round6\overlay_lamp`, `dvar_shots.ps1` ). The script reports
`ready light_spot intensity 1.5 at (-220, 0, 230)`. One view of both pools, a picture per step, and the mean colour
of the lower left and right part of the picture ( the scripted light is on the left ):

| step | left r g b | right r g b | picture |
|---|---|---|---|
| as compiled | 56 55 54 | 48 46 43 | `c6lamp_1_` |
| `setLightIntensity( 0 )` | 51 50 49 | 48 46 43 | `c6lamp_2_off`: the left pool is gone, the crate in it dark |
| back to 1.5 | 56 55 54 | 48 46 44 | `c6lamp_3_full` |
| `setLightIntensity( 0.45 )` | 52 51 49 | 48 46 44 | `c6lamp_4_dim` |
| `setLightColor( ( 1, 0.15, 0.1 ) )` | 56 50 49 | 48 46 43 | `c6lamp_6_red`: a red pool, the crate red |
| `setLightColor( ( 0.2, 1, 0.2 ) )` | 51 55 49 | 48 46 44 | `c6lamp_7_green` |
| `setLightFov( 40, 20 )` | 53 51 50 | 48 46 44 | `c6lamp_9_narrow`: a small bright disc |
| `.origin` 250 units along y | 52 51 49 | 48 46 43 | `c6lamp_11_move`: the pool moves away and ends where the surfaces that name the light end |
| `moveTo` 250 units the other way, 2 s | 57 55 54 | 48 46 44 | `c6lamp_13_slide`: no change made out after 1.5 s, the script still reads the old origin |

`c6lamp_sheet.png` has eight of them side by side. The right pool never changes: it is the second light, whose
white `_color` comes out warm - the coloured picture of `tungsten_lamp` ( the third definition seen in the client
after `light_point_linear` and `light_point_quadratic` ). What a moved light shows of its limits: the lightmap and
the cut of the floor stay where the map was compiled, so its pool is clipped at the edge of the surfaces that
name it.

**A primary omni light with bots** ( `mp_c6omni`: the room with path nodes, one `spawnflags 1` light at 215 units
above the middle, radius 900, three bots; `c6omni_sheet`, `c6omni_b_sheet` ). The room is lit by it at run time,
the crates throw their baked shadows. The frame has a shadow map pass - 172 draw calls, 27403 triangles into a
1024 x 2048 depth buffer ( `c6omni_stats` ) - so the bots are drawn into the light's shadow map. **A shadow of a bot
on the floor was not made out** in fourteen pictures from two places ( the floor under the light is close to white
there ). So an omni light's shadows of players are still not shown; under a spot they are ( fifth round ).

**The create-room list.** `round6\list_build.ps1`: the editor builds the box room as `mp_c6list` and installs it
with `list` into the test client, whose `client_shipRetail\patch\codol_common.ff` is made a real copy first ( it
was a hard link to the install's file ): row 97 of `mp/comaplist.csv`, the arena entry, the name. The lobby is
started without `CODOL_FORCE_MAP` ( `start_lobby.ps1` without `-Map` ) and with the test client's zone folder as
`CODOL_ZONE_DIR`. The client starts, loads the zone ( its log lists `codol_common` ) and its menus work:
Multiplayer -> Start -> the channel page with *Matchmaking*, *Rooms*, *Create Room* ( `c6list_2_start` ). **Create
Room does not open**: "Unlocks at player level 5" ( `c6list_3_create` ); the lobby's own line for the account says
`playerData level=1 .. uf_rank_1..1`. The lobby holds the level in `players\<uid>.json` of its own folder - level
5 is 9000 XP out of finished matches -, has no switch for it ( `CODOL_RANK_SEED_LEVEL` with a scratch
`CODOL_RANK_FILE` was tried: the per account file wins ), and that file is not the tests' to write. **So the list
was not seen and no map was started from it.** What the lobby does with a room's map was read from its sources
instead ( `handoff\map_handoff.py` ): the client sends the row's ID - the map name - as a CRC and as text; the
lobby knows a name only when `<map>.ff` is in its zone folder ( `installed_maps()`: `CODOL_ZONE_DIR`, else
`<install>\client_shipRetail\chinese`; never `usermaps\` ), and builds that table when it starts; a map it does
not know becomes its default map ( `CODOL_MAP_PREFER`, mp_killhouse_sh ), which it also pushes back to the client;
it then tells the server `set codol_sessionMap <map>` .. `map <map>` over rcon. Nothing in the install or the
lobby's folder builds `codol_common.ff`. After the test `map_uninstall` took everything out again and the list
zone was byte for byte the install's.

New in the harness: `put_zone.ps1`, `put_iwd.ps1`, `make_base.ps1` ( `main\` as a folder of links ),
`start_lobby.ps1` without `-Map`, and under `round6\`: `run_editor.ps1` ( the editor unattended ), `ui.ps1` ( one
click in the client's menus with a picture ), `views.ps1`, `dvar_shots.ps1`, `snd_sequence.ps1`, `change_map.ps1`,
`serve_map.ps1`, `serve_package.ps1`, `serve_stock.ps1`, `editor_mapc.ps1`, `editor_fx.ps1`, `list_build.ps1`. After
every run `stop_all.ps1` was seen to have taken the frame dump dll out of the test client's folder.

### Script model animations

`scriptModelPlayAnim` shows nothing in the client on `codol_dedrun`, for a project's animations and for the hanging
clothes of shipped mp_crash_tropical alike. Looked at from the map's side only ( the server is not ours to change ):

* **What a map has to bring, and does.** Client and server build their animtrees from the *animlists*: the rawfile
  `animlists/<map>.animlist` of the map zone ( `=<tree>` lines, then animation names ) after `animlists/common.animlist`
  ( the server's `codol_model_animation.c`, `CODOL_LoadClientAnimTrees`, which follows the client's loader at
  0x40831E ); the `xanim` assets have to be in a mounted zone, and the level script precaches with `precacheMpAnim`.
  `package_map.ps1` puts all of it into the map zone ( the project's `generated\animlists\map.animlist`,
  `animtrees\<tree>.atr`, the animations ). The server's log of the packaged yard: `CoDOL animlists: mp_c4fx.animlist
  loaded, 2 entries`, `mounted animtree: animtrees/mp_c4fx.atr`; its `codol_animmodelprobe` ( server only,
  `developer 1` ): `10 client trees: 1=destructibles 2=vehicles 3=ac130 4=script_model 5=player_3rd_person 6=sentry_gun
  7=dog 8=player 9=mp_c4fx 10=multiplayer` - the map's tree has a client id. For mp_crash_tropical the same with
  `9=animated_props` and 15 list entries. No `cannot play animation .. prop left static` warning in either run:
  `CODOL_ScrCmd_ScriptModelPlayAnim` found the precached name, resolved tree and entry and captured them for the
  entity's snapshot.
* **Where it is lost** is therefore behind that point, in what the server sends and the client makes of it - the
  same for a shipped map. The server's own notes ( `src/codol/assets/codol_model_animation.h` ) say what the client
  needs: for an entity of type 6 the snapshot field at +0x94 has to name the client's tree ( 4 bits ) and the field
  at +0x60 the entry inside that tree, with the field at +0x6C zero ( client code at 0x43FCF9 ); and they name
  what is not measured: that the entry index of the server's tree is the client's ( "an inference from the loader,
  not a measured client dump" ), and the client's tree id is the server's replay of the client's registration order.
  `codol_animmodelprobe` prints those three fields only for script models bound through the `setanim` family
  ( `0 bound script models` here ), not for the `scriptModelPlayAnim` path - so from outside the server it cannot be
  told which of the three is wrong on the wire. That probe, extended to every script model with a captured
  animation, is the next step, and it is the server's.

## Scale

x64 Release, this machine ( 32 threads ):

| map | brushes | compile | peak memory | zone / file |
|---|---|---|---|---|
| box room | 11 | 0.5 s | - | 7.0 MB / 29 KB |
| 144 rooms ( `make_big` ) | 1421 | 1.6 s | 260 MB | 8.8 MB / 310 KB |
| mp_rust collision import | 3226 | 3.6 s | 823 MB | 10.6 MB / 1.07 MB |
| the yard ( 66 brushes, 2 patches, 9 static models, 259 path nodes ) | 66 | 2.3 s ( `--fast` 2.1 s, `--quality 2` 5.4 s ) | 709 MB ( 739 MB ) | 13.8 MB / 2.46 MB; packaged with minimap 14.9 MB / 2.51 MB, load zone 35 KB |

( 6 MB of every zone are a lightmap page; an empty catalog adds about 16 s once. The first three rows are from
before the bounce pass; the 144 room map now takes 2.6 s at `--fast`, and 16.6 s / 53 MB zone / 4.9 MB file at
`--quality 2` with 7 lightmap pages. )

## Constants taken on trust

Values the shipped maps all have, written without knowing more than that: the four sort keys of the GfxWorld header
( 6, 0x28, 0x31, 0x2C ); sky sampler state 0xF3; `fogTypesAllowed` 1; vertex layer data = four zero bytes; image
header fields of lightmaps, reflection probes and the outdoor image ( category, flags 0x08000003 / 0x08010301 / 3,
level counts ); the stage "stage 0" ( trigger 1024, sun 1 ) every MapEnts has; `cplane_s.type` ( axis for + axis
planes, 3 else ) and `signbits` 0; surface flag 1; plane normal `type` semantics; material memory 0; `isInUse` 1.
Leaf box padding 0.125 and model box padding 1.0 are the relations measured on shipped maps ( `codol_mapc` ).

## Limits and what is not done

* **Static models**: players do not collide with them ( the engine's rule, see above ): a mapper adds clip brushes;
  the compiler does not make them. Physics presets of nested models are dropped. `flags` of the draw instance are 0
  ( shipped: 0 for 80 %; 0x20 with ground lighting for foliage, 1, 0x10: not written ), `groundLighting` 0.
* **Terrain**: at most 65535 collision vertices ( one vertex block ); patches of brush entities are left out; the
  effect of borders and of the walkable bits is not shown ( the server's traces give the same results without
  borders ); curve patches depend on `codol_edit`'s tessellation ( `codol_edit` only creates terrain meshes, a curve
  was not tested ).
* **Materials**: a brush with an alpha tested or decal ( blended, sort 6 .. 13 ) face hides nothing and splits no
  cell; other blended materials are left to the mapper's flags, because shipped maps have them on whole walls. The
  clip material of a material no shipped map has on a brush comes from its definition ( surface type; solid, a decal
  does not collide ) - contents other than "solid" ( bullets pass, see through for AI .. ) are not derived. Water
  materials are refused.
* **Leaf brush trees** are one list per leaf ( the BSP keeps leafs small ); no split nodes are written.
* No merging of coplanar faces; a face that is partly hidden is drawn whole. T-junctions are repaired between
  faces of the world ( and within a brush model ); a terrain mesh whose edge does not run along the brushes it
  meets still leaves the gap the mapper built.
* **Automatic portals** only find planar, convex openings up to 72000 square units in the face of something solid
  ( the report says how many cross sections were left out and why ); open terrain is one cell; a mapper's portal
  brushes switch the search off. A face seen from several cells is drawn once per cell ( and has a lightmap chart
  per cell ).
* **Light**: the world is grey for bounced light ( reflectivity 0.5, the materials' images are not read ); a see through brush ( fence ) casts no baked shadow; reflection probes show the
  scene without textures and their cube orientation is assumed; the outdoor image ( weather occlusion ) is empty;
  the light grid's 56 directions are taken from `codol_view`'s port of the model lighting lookup, not measured in
  the game. The worldspawn keys `diffusefraction`, `sundiffusecolor`, `radiosityscale`, `sunradiosity` and
  `maxbounces` are not read. That a surface under the open sky receives half of `skycolor x skylight` is fitted to
  one shipped map; how the game's compiler weighs `ambient` was not measured. The split of a luxel's light between
  the two lightmap layers ( up to 0.85 into the directed one ) is this compiler's own rule.
* **Primary lights**: what is written is what the shipped zones hold, with these differences. **Surfaces** are cut
  along a grid of 32 units around what a light wins, not along the pieces of the game compiler's BSP; a piece is
  never smaller than a grid square, and the specular highlight of a light ends at the cut ( its diffuse light goes
  on in the lightmap ). **Light regions**: a hull per cell with cone planes and diagonals; no planes through the
  light and the edges of a portal. **Scriptable lights** are written as the shipped zones have them and a script
  changes one in the client ( sixth round ); the baked visibility and the cut of the surfaces of a light that a
  script moves stay where they were compiled, and `moveTo` on a light showed no movement. At most 253 primary
  lights, 15 light definitions. Under an **omni** light the client draws a shadow map pass with the players in
  it, but no shadow of a player was made out in the pictures ( sixth round ). `codol_viewtool` draws primary lights without their
  shadow maps.
* **Brush models** are drawn and collide, but are in no cell ( the engine places dynamic models itself ) and are lit
  by the lightmap of the place they were compiled at.
* **Sky**: faces with a sky material become the sky surfaces of `skies[0]` with the material's cube map when its header
  is in a zone ( seen in the client ). The sun flare's blinding and glare ( `r_sunblind_*`, `r_sunglare_*` ) are
  written as given and were not looked at; the lens flare settings of some shipped `.sun` files ( `2692` .. `2745` )
  are not known to the compiler ( reported, left out ).
* **Effects and sounds**: `script_fx` / `script_sound` write the two effect files the client reads; the effect and
  sound assets have to be in a zone that is loaded - the common zones, or the map zone ( `package_map.ps1` carries a
  project's there ); a sound alias that is nowhere gives `Missing soundalias` lines in the client. Exploders and
  interval sounds ( `createExploder`, `createIntervalSound` of the shipped files ) are not written. A project's
  loaded sounds play in the client ( fifth round: measured, not heard ), its **streamed** ones too ( sixth round ) -
  from an archive named `iw_sound_NN.iwd` that client and server both have.
* **Script model animations** do not move in the client on the test server - a shipped map's do not either ( fourth
  round ) -, so a compiled animation has still not been seen moving. See [Script model
  animations](#script-model-animations): nothing is missing on the map's side.
* **Minimap**: the picture is the lightmapped scene from above, roofs included; the material is made by
  `codol_assetc` ( `package_map.ps1` ), the compiler itself only writes the picture. A room with a ceiling has a
  dark minimap.
* No `MapTriggers` ( trigger entities are brush models `*N` ), no glass, no dynEnts, no path data
  ( GameWorldMp has none in this game ), no sound aliases.
* Unsigned container only ( `codol_zone` ).
* `codol_mapc` ( the importing compiler ) had the same clip entry mistake for *added* / moved static models
  ( `axis / scale` instead of the transpose ); fixed there too ( `stage_models.cpp` ).
