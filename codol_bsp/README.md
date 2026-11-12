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
`codol_assets` model, a block of shadow under a fence ). What is approximate or missing is listed under
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
          [--assets <project zone.ff>]... [--minimap <picture.tga>] [--scripts <folder>]
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
   `codol_edit`'s import writes them ), `light` entities, worldspawn keys `sundirection`, `suncolor`, `sunlight`,
   `ambient`, `skycolor`, `skylight`. Brushes with `hint` / `skip` / `portal*` materials are no geometry; a face with
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
5. **Light** ( `light.cpp` ), see below.
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

* **Charts and pages.** One chart per face of a surface ( brush face or patch triangle ), one luxel per 16 units
  ( `--fast` 32, `--quality 2` 8 ), a luxel of padding. The charts of a surface go into one page ( a surface names
  one lightmap ); surfaces are shelf packed, tallest chart first, into up to 30 pages, and only when that is not
  enough the luxel grows ( reported ). The 144 room map at `--quality 2`: 4484 charts, 2.8 million luxels, 7 pages.
* **Direct light** per luxel: `ambient` + sky light ( the share of sky directions, cosine weighted, whose ray ends
  on a face with a `sky*` material or leaves the map, times `skycolor x skylight` ) + every `light` entity ( linear
  falloff to `radius`, `n . l`, spot cone from `fov_outer` / `fov_inner` / `angles`, shadow ray ). Per primary texel
  ( 2 x 2 per luxel ) one ray towards the sun; a 3 x 3 box filter per chart softens the edge.
* **Occluders**: the solid world brushes ( a BVH ) and triangles ( a second BVH ): the tessellation of the patches
  and LOD 0 of the static models. A brush with an alpha tested or decal face ( a fence, a grate ) lets the light
  through: the bake does not read the material's picture, and the solid block of shadow the yard's chain link fence
  threw in the client was the worse mistake. Such a brush gets its shadow from the game's sun shadow map.
* **Bounced light**: every luxel sends 24 cosine weighted rays ( 12 / 48 ) over its side and takes what the luxels
  they end on throw back - half of what arrives there ( direct light and the sun's share; the images of the materials
  are not read: a grey world with a reflectivity of 0.5 ). One bounce, two at `--quality 2`. Shadowed faces are no
  longer black.
* **Light entities are baked, they are not primary lights**: ComWorld has "none" and the sun only. Dynamic objects do
  not cast shadows in them.
* **Light grid**: a sample at every grid point ( 32 x 32 x 64 units ) that is not inside a brush. A sample is what
  arrives from all around ( the sky directions: sky colour, or what the luxel the ray ends on throws back; the light
  entities ) as an average and a lean per colour, `E( n ) = m0 + n . m1`; the 56 colours of an entry are that
  function at the 56 directions of a colour set - the surface texels of the 4 x 4 x 4 block the model lighting volume
  is looked up in with the normal ( `codol_view` `light_grid.cpp`, from CODOL-AssetExplorer ). So a model is brighter
  on the side the light comes from. Colour sets are deduplicated after dropping low bits ( 2, more when there are over
  65535 ); primary light = the sun where it is visible. Rows / runs as the shipped grids. At most 3 million points:
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

### Minimap, scripts, the files around a map

* `--minimap <file.tga>`: the map from above ( 512 x 512, north = + y up ) as the lightmaps light it; rays go through
  sky brushes, a roof hides what is under it; where nothing is, the picture is see through. The map gets the two
  `script_origin`s with the targetname `minimap_corner` the picture fits and `"northyaw" "90"` when it has no
  corners of its own ( they are left out of the leak test ).
* Entities `script_fx` ( `fx`: the effect as `loadfx` names it, `angles`, `delay`: seconds between repeats, none: a
  one shot effect that starts with the map, `soundalias` ) and `script_sound` ( `soundalias`: a looping sound ) are
  no entities of the game: they become `maps/createfx/<map>_fx.gsc` in the form the game's createfx tool writes
  ( `createOneshotEffect` / `createLoopEffect` / `createLoopSound` ) and `maps/mp/<map>_fx.gsc` ( `loadfx` per
  effect, then the placement ), both rawfiles of the zone. An effect has to be in a zone that is loaded ( the common
  zones: `fire/firelp_small_pm`, `smoke/thin_black_smoke_m` .. ); effect assets are not copied into the map.
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

`codol_bsp_tests` ( 223 checks with `--assets`, 0 failed on this machine ): writes the test maps with `codol_edit` and compiles them;
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
and per zone that every lit technique set has its hardware shadow map sibling or a warning.

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
* **No T-junction repair**, no merging of coplanar faces; a face that is partly hidden is drawn whole. No crack
  was found in the client's pictures of the yard.
* **Automatic portals** only find planar, convex openings up to 72000 square units in the face of something solid
  ( the report says how many cross sections were left out and why ); open terrain is one cell; a mapper's portal
  brushes switch the search off. A face seen from several cells is drawn once per cell ( and has a lightmap chart
  per cell ).
* **Light**: the world is grey for bounced light ( reflectivity 0.5, the materials' images are not read ); the lower
  secondary layer is black; **light entities are not dynamic primary lights** ( baked only; only `light_point_linear`
  falloff ), so players cast no shadow in them ( they are lit by them, through the light grid ); a `light` without
  a `radius` reaches 300 units - the editor's first room is dark; a see through brush ( fence ) casts no baked shadow;
  reflection probes show the scene without textures and their cube orientation is assumed; no sun flare; the outdoor
  image ( weather occlusion ) is empty; light regions are empty; the light grid's 56 directions are taken from
  `codol_view`'s port of the model lighting lookup, not measured in the game.
* **Brush models** are drawn and collide, but are in no cell ( the engine places dynamic models itself ) and are lit
  by the lightmap of the place they were compiled at.
* **Sky**: faces with a sky material become the sky surfaces of `skies[0]` with the material's cube map when its header
  is in a zone ( seen in the client ). No sun flare: the sun block only carries the direction.
* **Effects and sounds**: `script_fx` / `script_sound` only write script lines; effect and sound assets are not
  copied into the map zone ( they have to be in a zone that is loaded: a sound alias that is not gives `Missing
  soundalias` lines in the client ). The client runs the generated `_fx` rawfiles ( fire and smoke in the yard ); on
  the server the generated level script compiles and runs ( no script errors ).
* **Minimap**: the picture is the lightmapped scene from above, roofs included; the material is made by
  `codol_assetc` ( `package_map.ps1` ), the compiler itself only writes the picture. A room with a ceiling has a
  dark minimap.
* No vision file ( the client asks for `vision/<map>.vision` ), no `MapTriggers` ( trigger entities are brush models `*N` ), no glass, no dynEnts, no path data
  ( GameWorldMp has none in this game ), no sound aliases.
* Unsigned container only ( `codol_zone` ).
* `codol_mapc` ( the importing compiler ) had the same clip entry mistake for *added* / moved static models
  ( `axis / scale` instead of the transpose ); fixed there too ( `stage_models.cpp` ).
