# codol_mapc

Map compile step for Call of Duty Online 1.1.1.13 ( "C1" ): takes a map that was imported from a shipped fast file
with `codol_edit` and edited, and writes a fast file with the edits in it. It is the place `cod4map` + `linker` have
in the CoD4 mod tools - but it is an *incremental* compiler: it does not build a zone from a `.map`, it applies the
difference between the import and the edited map to the zone the import came from. Everything an edit does not
touch stays exactly as the game's own map compiler left it ( light bake, portals, terrain collision, sounds, fx .. ).

Static library `codol_mapc` ( namespace `codol::mapc` ) + the tools `codol_mapc`, `codol_mapc_probe` + tests. Depends on
`codol_zone` ( reading and writing zones ), `codol_edit` ( the map document ) and, privately, on `codol_view` for the
light grid code the renderer lights models with.

**Nothing here has been loaded by the game client.** A dedicated server ( CoD4x_Server, `codol_dedrun` ) has loaded
compiled zones and run matches with bots on them: see [On a dedicated server](#on-a-dedicated-server). What is
verified offline is listed under [Verification](#verification), what a person has to test in the game first under
[What to test in the game](#what-to-test-in-the-game-first).

## Build

```
cmake -S codol_mapc -B build/codol_mapc -A x64 -DCODOL_EDIT_RAPIDJSON_INCLUDE=<checkout>/deps/rapidjson/include
cmake --build build/codol_mapc --config Release
build/codol_mapc/Release/codol_mapc_tests.exe --out build/mapc_test          ( or ctest --test-dir build/codol_mapc -C Release )
powershell -ExecutionPolicy Bypass -File codol_mapc/tests/run_e2e.ps1         ( tests + tool + codol_linker --verify + pictures )
```

Static CRT, `/std:c++latest`, `/W4 /WX`. Builds `codol_zone`, `codol_edit`, `codol_render`, `codol_view` with it, and
`codol_viewtool`, `codol_zonetool`, `codol_linker` ( options `CODOL_MAPC_BUILD_TOOLS`, `CODOL_MAPC_BUILD_TESTS`,
`CODOL_MAPC_BUILD_VIEWTOOL`, `CODOL_MAPC_GAME_DIR` ). Use the x64 build: a compile holds the zone several times.

## The tool

```
codol_mapc --base <map.ff> --exe <C1.exe> --map <edited.map> [--baseline <map.codol_base.map>] --out <new.ff>
           [--report <file>] [--zone-out <file>] [--donor <zone.ff>]... [--level 0..9]
           [--assets <project zone.ff>[,<type> ..]]... [--rawfile <name>=<file>]... [--append-rawfile <name>=<file>]...
           [--no-entities] [--no-models] [--no-brushes] [--no-surfaces]
codol_mapc --check <map.ff> --exe <C1.exe>
```

* `--base` the fast file the map was imported from, `--exe` the client ( fast file key ).
* `--baseline` the map as it was right after the import ( `document::import_baseline_map()` ). Without it the tool
  looks for `<map>.codol_base.map` next to the map ( `x.map` -> `x.codol_base.map` or `x.map.codol_base.map` ), and
  without that it imports `--base` again and uses that import.
* `--out` an unsigned `IWffu100` fast file ( see `codol_zone/README.md` for who loads those ). `--zone-out` the
  inflated zone. `--report` what was done, what was approximated, what was left out ( also printed ).
* `--donor` a fast file a new static model may be taken from when the map zone does not have it ( repeatable ). The
  model of any other entity ( `script_model` .. ) that the map zone does not have and a donor has is copied as well:
  a top level xmodel in front of the gfx_map with what it depends on ( "entity models: xmodel .. taken from zone .." in
  the report ). A model neither has is left alone - it may be in one of the game's common zones, which the compiler
  does not read. Loaded by server and client ( "A project's assets", below ).
* `--assets` a project's assets for the edited map, see [A project's assets](#a-projects-assets). `--rawfile`,
  `--append-rawfile`: a rawfile of the zone is replaced / added, or gets a file's text behind what it holds.
* `--no-brushes` also leaves out the surfaces: render surfaces are only made for brushes the brush stage compiled.
* Exit code 0: written. 1: failed, nothing written. A map without differences gives the base zone, byte for byte.
* A compile that changes collision data reads the result back the way a dedicated server does ( below ) and fails,
  writing nothing, when the server would refuse it although it takes the base zone.
* `--check` does that reading for any fast file and says what the server would object to ( exit code 1 ).

```
> codol_mapc --base mp_rust.ff --exe C1.exe --map mp_rust.map --out out/mp_rust.ff
difference: static models 0 moved, 0 deleted, 0 added; brushes 4 edited / added, 0 deleted
entities: unchanged ( 806 -> 806 )
brushes: 3 replaced, 0 deleted, 1 added; 5 sides and 4 planes added; 3 collision leafs / brush models touched
surfaces: 3 added ( 78 vertices, 38 triangles ), 0 faces not drawn
zone: 51550968 -> 51555166 bytes
  brushes: 3226 -> 3227 brushes; 3100 leaf brush nodes -> 3104 ( 4 put in front of a tree, 0 trees of their own ); ..
  warning: surfaces: 2 brushes were deleted or changed shape. A zone does not say which render surfaces a brush produced ..
wrote out/mp_rust.ff ( IWffu100 )
```

To look at a compiled map: `codol_viewtool render --game <install> --map mp_rust --zone-dirs out --out a.png --pos x,y,z --angles pitch,yaw`
( `--zone-dirs`: folders searched for `<map>.ff` ahead of the install ).

## API

```cpp
#include "codol_mapc/mapc.hpp"
namespace codol::mapc
{
    struct model_placement { std::string model; edit::vec3 origin, angles; double scale; bool angles_changed, scale_changed; };
    struct model_move      { int zone_index; model_placement to; };
    struct brush_edit      { edit::object_id brush; int sub_model; int replaces; };
    struct map_diff        { std::vector<model_move> models_moved; std::vector<int> models_deleted;
                             std::vector<model_placement> models_added; std::vector<brush_edit> brushes;
                             std::vector<int> brushes_deleted; std::vector<std::string> not_compiled; bool empty() const; };
    bool diff_documents(const zone::Zone& zone, const edit::document& baseline, const edit::document& edited,
                        map_diff& out, std::string& error);

    struct options { std::filesystem::path base, exe, map, baseline, out, zone_out; std::vector<std::filesystem::path> donors;
                     int level = 6; bool entities = true, models = true, brushes = true, surfaces = true; };
    struct report  { bool identical; map_diff diff; /* counts per stage */ std::vector<std::string> notes, warnings;
                     std::string text() const; };

    // the zone in `editor` ( loaded from the base fast file ) + the difference -> inflated zone; editor then holds it
    bool compile_zone(zone::ZoneEditor& editor, const edit::document& baseline, const edit::document& edited, const options& opts,
                      const zone::ClientKey& key, std::vector<std::uint8_t>& zone_bytes, report& rep, std::string& error);
    // files in, fast file out ( what the tool calls )
    bool compile(const options& opts, report& rep, std::string& error);
    // the entity string a document compiles to
    bool entity_string_of(const edit::document& baseline, const edit::document& edited, std::string_view original_ents,
                          std::string& out, std::vector<std::string>& warnings, std::string& error);
}

#include "codol_mapc/trace.hpp"          // clip_trace: this library's own trace through a zone's brushes ( for checks )
#include "codol_mapc/server_check.hpp"   // check_server_collision: the clip map of an inflated zone as a dedicated server decodes it
```

Typical use from the editor: `doc.save_map_file( "x.map" )`, write `doc.import_baseline_map()` to `x.codol_base.map`,
then `compile( { base, exe, "x.map", {}, "out/<map>.ff" }, report, error )`.

## How it works

1. **Difference** ( `diff.cpp` ). The import lists the zone's objects in a fixed order ( `src/codol/map_zone.cpp`,
   `map_ents.cpp` ): the collision brushes of sub model 0 in the worldspawn and those of sub model N in the entity with
   `"model" "*N"`, in `cbrush_t` order; one `misc_model` with `codol_static` per GfxWorld static model. That order maps
   the baseline's objects to zone indices. An object of the edited map that is not an exact copy of a baseline object
   is an edit: a static model with another placement is *moved* when a model of the same name is missing from the
   baseline's side, else *added*; a brush that is not in the baseline takes the place ( the index ) of a brush of the
   same entity that is gone, else it is new. So "moved" and "delete + add" are the same thing, with the same result.
2. **Stages**, each on the zone the previous one wrote ( the zone is written and loaded again in between, which also
   runs the game's loaders over every intermediate result ):

| stage | what changes in the zone |
|---|---|
| entities | `MapEnts.entityString` = what `document::export_entity_override` gives, without its comment header. Brush model entities keep their `*N`. New brush entities ( no compiled brush model ) and non static `misc_model`s are left out with a warning, as in the override export. |
| static models | GfxWorld `dpvs.smodelDrawInsts` ( origin, axis, scale, cull distance, primary light, reflection probe, ground lighting ), `dpvs.smodelInsts` ( bounds, lighting origin ), `smodelCount`, `smodelVisDataCount`, the static model lists of the cells' AABB trees and the boxes of the nodes above a moved / new model, the shadow geometry lists; clip map `staticModelList` ( origin, inverse scaled axis, bounds ), `smodelNodes`, their counts. |
| brushes | clip map `brushes`, `brushBounds`, `brushContents`, `brushsides`, `brushEdges`, the plane array ( shared with the GfxWorld: both `planeCount`s ), `leafbrushes`, `leafbrushNodes`, `leafs` ( root, contents, box ), `cmodels` ( box, radius, leaf ). |
| surfaces | GfxWorld `draw.vertices`, `draw.indices`, `dpvs.surfaces`, `surfacesBounds`, `sortedSurfIndex`, the surface runs and boxes of the cells' AABB trees, `models[0]` / the brush models' start indices, the surface ranges and counts of `dpvs`, the shadow geometry surface lists ( renumbered ). |

The stages do not use per-case patches of the zone stream. They use the typed editing of `codol_zone`
( `ZoneEditor::data_at`, `set_data`, `patch_value`, `set_pointer`, `insert_data`, `remove_data_range` .. ): an array is
replaced by one of another length with the pointers it holds given as targets, and the loaders decide where
everything ends up when the zone is written. See `codol_zone/README.md`, "Typed editing".

### A project's assets

An edited shipped map takes a project's animations, sounds, effects, models and materials the way a map compiled
from scratch does ( `codol_assetc --carry` after `codol_bsp` ): `--assets <project zone.ff>` carries every top level
asset of the kinds `xanim`, `sndcurve`, `loaded_sound`, `sound`, `fx`, `xmodel`, `material` ( or of the kinds named
behind the file, `--assets zone.ff,sound,loaded_sound,sndcurve`; names as `codol_linker --list` prints them ) into
the map zone, each with what it points at ( `ZoneEditor::add_asset_closure`: an alias list with its loaded sounds
and curve, an effect with its materials, a model with materials, images and technique sets ), behind the assets the
zone has. An asset whose kind and name the zone already has stays as it is, so a second run carries nothing. The
zone is a `--donor` as well: a new `misc_model` or `script_model` may name one of its models.

```
> codol_mapc --base mp_rust.ff --exe C1.exe --map mp_rust.map --out out/mp_rust.ff --assets mod/c4fx/c4fx_assets.ff
    --append-rawfile animlists/mp_rust.animlist=mod/c4fx/generated/animlists/map.animlist --rawfile animtrees/c4fx.atr=mod/c4fx/generated/animtrees/c4fx.atr
  assets: carried from c4fx_assets.ff: 1 fx, 3 loaded_sound, 3 material, 1 sndcurve, 4 sound, 2 xanim, 2 xmodel ( and 4 assets they point at ); 0 the zone already has, 0 failed
  container: the file has the length of the base file ( 27697429 bytes ), which the client checks
```

What goes with them, as for a built map ( `docs/CODOL.md`, "In the built map" ): **animations** need their names in
the map's animlist - the client and the server build the animtrees from `animlists/<map>.animlist`, so the
project's list is appended to the shipped map's ( `--append-rawfile` ), its animtree file added ( `--rawfile` ) - and
a level script that precaches and plays them, which the dedicated server reads from its script folder; **effects**
are placed by script ( `loadfx` / `playfx` ) - the two effect files of a shipped map are not edited; **streamed
sounds** need their archive, as `iw_sound_NN.iwd`, in `main\` of client and server ( `codol_assets/README.md` ).
CoDOL Radiant's File > Compile map passes all of this by itself when the map has a project: `--assets`, the
animlist, the animtrees and every file of the project's `scripts\` as a rawfile, and copies the scripts to
`compiled\<map>.scripts\`.

Limits. A **material** that comes this way can be used by the carried models and effects; it is not put into
`GfxWorld.materialMemory`, so a world brush face with it is still not drawn ( "What is not compiled" ). The file
has to keep the length of the base file ( the client's table of stock zones ): what is carried must fit into what
the better compression of the rebuild leaves - mp_rust with the test project ( 16 assets, 0.2 MB ) fits; when it
does not, the report warns and the client refuses the zone. The server does not check the length.

Checked: `codol_mapc_tests --assets <project zone>` - the unedited map with the project's assets and two rawfiles:
every asset of the project's kinds is in the zone, every asset of the map still is, the appended list holds the
old text and the new, the collision data reads as before, a second run carries nothing.

**Loaded by the server and the client** ( sixth round, `codol_bsp/README.md` ): mp_rust edited in the editor - a
static model and a `script_model` of the project placed - and compiled with the project's zone ( a material, a
model of two LODs, 3 loaded sounds, a curve, 4 alias lists, an effect ) and its level script as a rawfile. The file
keeps the shipped length. The dedicated server installs the collision ( 758 leaves ) and runs the level script
( `loadfx`, `precacheModel`, `setModel` of the project's assets ); the client enters the map, draws the static
model, the entity model and a model the script spawns, the effect the script plays, and plays the project's loop
sound ( audio session mean 0.20 against 0.02 after `stopLoopSound` ). Animations of a project in an edited shipped
map were not part of that run.

**A project's animations on the server** ( seventh round; `codol_bsp/tests/harness/round7/mapc_anim.ps1` ): shipped
mp_rust, unedited, with a project zone of two animations and two models carried in, the project's animlist appended
( `animlists/mp_rust.animlist`: 43 bytes behind the 0 the zone had ) and `animtrees/mp_c4fx.atr` as a rawfile; the
shipped level script with `#using_animtree( "mp_c4fx" )`, `precacheMpAnim` and `scriptModelPlayAnim` on two script
models in front of the server's. The dedicated server: `CoDOL animlists: mp_rust.animlist loaded, 2 entries`, at
`developer 1` `CoDOL mounted animtree: animtrees/mp_c4fx.atr`, `codol_animmodelprobe`: `10 client trees: ..
9=mp_c4fx 10=multiplayer`, `getAnimLength` 2 and 1.33 s, no "cannot play animation", no script error. The same
lines as for a map built from scratch. Not looked at in the client: script model animations do not move there on
this server, whatever the map ( `codol_bsp/README.md`, "Script model animations" ).

### Static models

* The XModel of a static model is **nested in the gfx_map asset**: the first instance of a model carries it ( its
  model field says "the data follows" ), every other user - later instances, the clip map's list, other assets of
  the zone - points at that field. So instances that carry a model keep their place; when one is deleted another
  instance of the model takes its slot; a hole is filled with the last instance that may move ( one that carries
  nothing and stays behind the instance that carries its model ), so that other assets' pointers do not move.
  When the last instance of a model goes, its XModel is removed from the gfx_map if nothing else points into it;
  otherwise the instance stays in the arrays as a holder that no cell, shadow list or collision list names
  ( reported; a new import shows it again ).
* A **new** instance of a model the map already draws points at the carrying instance's field like all others. A model
  the gfx_map does not have is used when it is a top level xmodel in front of the gfx_map, or it is taken from a
  `--donor` zone with `ZoneEditor::add_asset( .., withDependencies )` and put in front of the gfx_map. It needs to be a
  self contained top level asset of the donor ( `codol_linker --list` ); a model that is nested in another map's
  gfx_map is not.
* **Bounds** of a moved / new instance: the box of the model's LOD 0 vertices placed with origin, axis and scale. That
  is how the base zones' bounds are made: 1757 of 1757 sampled instances of mp_rust and 1519 of 1519 of mp_killhouse_sh
  whose vertices are in the zone are reproduced within 0.5 units. For a model whose vertices are not in the zone the
  placed XModel box ( `XModel + 320`, centre / half size ) is used, which holds the tight box.
* **Lighting** as `codol_view`'s `load_map` computes it ( same code: `light_grid`, `world_sight_trace` ): primary light =
  the light grid's sample at the lighting origin, reflection probe = the nearest probe, and for instances with the
  ground lighting flag ( 0x20 ) the stored colour = the light grid's ground lighting there. The lighting origin keeps
  its offset from the centre of the bounds. `lightingHandle` is a runtime value ( 0 in every zone ). Cull distance:
  scaled with the model scale; for a new instance what other instances of the model have per unit of scale, else
  the map's median.
* **Cells**: a moved / new instance is taken out of every cell list and put into the AABB tree leaf, of the cell the
  GfxWorld's BSP puts its centre in, whose box has to grow least; that leaf's box and its parents' grow to hold it.
  The lists keep the layout the client relies on ( `MapEdits.cs` of CODOL-AssetExplorer: the root's list is stored,
  every other node points into it, a node's list is its children's lists back to back - checked for every cell before
  anything is changed ).
* **Collision**: models with collision have an entry in `clipMap.staticModelList` ( in both maps exactly the models
  with `numCollSurfs > 0` ). A moved instance's entry follows; a new instance gets one when the model has entries
  ( or, from a donor, collision surfaces ). The AABB tree keeps its shape: boxes are recomputed along the paths that
  changed, an emptied node goes, a new entry joins the leaf that grows least.

### Brushes

* A brush becomes a `cbrush_t` from `document::geometry`: the six axial sides are implied by the bounds ( material per
  side ), the other planes are `cbrushside_t`s; planes the map already has are reused, new ones are appended to the
  plane array. The edge lists ( per side the sides across its edges, clockwise from outside ) come from the windings;
  an axis aligned box gets byte for byte the lists every shipped box has, other brushes the same cycles starting with
  their lowest side ( the start of a cycle in the shipped data follows the map compiler's winding clipper and was not
  reproduced ). Contents = the contents of the sides' clip materials together ( true for 3226 of 3226 brushes of
  mp_rust, 3075 of 3075 sampled of mp_killhouse_sh ).
* **Where it collides**: a world brush goes into the leaf brush tree of every collision leaf its box touches, found
  by walking the clip map's BSP; a brush model's brush into that model's tree. The trees keep their splits: a tree
  that is one list gets the brush appended; in a tree that splits the brush joins the brushes *on the split of the
  root* - the tree a node with `leafBrushCount` -1 has right behind it ( mostly one list ), which every trace that
  enters the tree tests before it picks a side. A root without one ( `leafBrushCount` 0 ) becomes -1 and gets a list
  behind it. So no node is made up and nothing depends on reading `dist` / `range` right. A leaf without brushes
  gets a one node tree. The leaf's box grows by the brush's box + 0.125, its contents by the brush's.
* A deleted or replaced brush leaves every list. A list left empty is removed, and so is every split that has
  nothing left on one side and nothing on its split ( what is on the other side takes its place ); a leaf whose tree
  is gone points at node 0 and has no brush contents. The child offsets are written again for where the nodes are.
* **The arrays are written whole, in the shape the game's map compiler gives them**: `brushsides` and `brushEdges`
  are the runs of the brushes in brush order, `leafbrushes` the lists of the nodes in node order, each back to back
  with nothing else in them ( the base zones are exactly that; lists the base zone stores behind the node array stay
  there ). A dedicated server reads these arrays by walking the brushes and the nodes and refuses a zone where a run
  is appended behind the others or left behind by a removed brush - see [On a dedicated server](#on-a-dedicated-server).
  The planes of removed brushes stay in the plane array ( it is the GfxWorld's ), unreferenced. New planes get unit
  normals in single precision.
* Brush models: box of the leaf = box of its brushes + 0.125, box of the model + 1.0, radius = distance of the far
  corner - the relations measured on the brush models of the base zone ( 26 in mp_rust, 269 in mp_killhouse_sh ).

### Patches

* Every patch of the worldspawn is new ( an imported map has none ) and is **drawn**: its tessellation ( terrain as
  it is, a curve at its subdivision, `edit::patch_subdivisions` - the same triangles `codol_bsp` compiles ) becomes
  one `GfxSurface` by the path of the new brush faces below: template surface of the same material, inserted behind
  it, into the AABB tree leaf of its cell. Per vertex: the patch's own vertex colour, the texel of the template
  nearest to it that faces its way ( a curve faces many ways ), a tangent from its own normal. Patches of brush
  entities are left out ( reported ).
* **Collision is not compiled.** The report says so and names the remedy: clip brushes where players and bullets
  have to stop. Terrain collision in the clip map is `verts` / `triIndices` / `triEdgeIsWalkable`, partitions with
  their borders, and per collision leaf a run of AABB tree roots ( `cLeaf_t.firstCollAabbIndex` / `collAabbCount` )
  whose nodes name their children and partitions by absolute index. Adding a patch means appending to the first
  five arrays and inserting new roots into the run of every leaf the patch's partitions touch - which renumbers
  every later AABB node, its child indices and the ranges of all later leaves - and the dedicated server's own
  reader of these arrays ( `codol_dedrun` ) has rules for them that were not established ( for the brush arrays they
  were: [On a dedicated server](#on-a-dedicated-server) ). `codol_bsp` ( clip_build.cpp ) writes all of this, for a
  map it builds whole. Not done here: it is work, not a limit of the format, and nothing was tested in the game.
* Tested: `codol_mapc_tests --only patches` - a bumpy 5 x 5 terrain mesh and a raised 5 x 3 curve over the spawn
  of the map: two surfaces, every new vertex inside the patches' box and with a texel of the bake, triangle count
  = the tessellation's, the zone reads back through `codol_zone` and is what a dedicated server takes, the clip map
  is the base zone's. Not seen in the game.

### Surfaces

* Every face of a new / edited world brush whose material is no tool material ( clip, caulk, hint .. ) is
  triangulated with `document::build_mesh`, one `GfxSurface` per brush and material. The world's materials are the
  brush materials with `wc/` in front.
* What a surface cannot know it takes from the nearest existing surface of the same material ( the *template* ):
  material pointer ( an entry of `GfxWorld.materialMemory` ), lightmap index, reflection probe, primary light, flags.
  It is inserted right behind the template, which keeps the surface array sorted by material inside its range.
* **Lighting without a bake**: all vertices of a face get the lightmap coordinate ( and vertex colour ) of one vertex
  of the template - the nearest one whose normal points the way the face does. The face is lit by one texel of the
  existing bake: evenly, as bright as a surface of that material next to it. No shadows are baked onto it or cast by
  it into the lightmap; it is in no light's shadow geometry list.
* It joins the AABB tree leaf ( of the cell its centre is in ) whose box grows least, in both halves of
  `sortedSurfIndex`.
* **Not removed**: a zone has no record of which surfaces came from which brush ( the game's map compiler merges and
  sorts the world's triangles ). The render geometry of a deleted or moved world brush therefore stays where it was;
  the compiler says so in the report. Collision is exact either way.

## What is not compiled

* New brush models ( a brush entity without `"model" "*N"` ), `physics_box` blocks, patches of brush entities.
* **Collision of patches** ( below, "Patches" ).
* Render surfaces of brush models' brushes ( their collision is compiled ).
* Faces whose `wc/<material>` no world surface uses: they collide but are not drawn ( reported ). Adding a material
  to `GfxWorld.materialMemory` and sorting the surface in is not done.
* A brush material that is not a clip material of the map gets the map's most used solid clip material ( reported ).
* Terrain collision ( `CollisionPartition`, `CollisionAabbTree` ), portals, the GfxWorld BSP, the light grid, lightmaps,
  reflection probes, light regions, dynEnts, glass, pathing ( `game_map_mp` ), fx: untouched.
* No signed container: see `codol_zone/README.md` for which clients load `IWffu100`.

## Approximations, and what is taken on trust

* Lighting of new surfaces ( above ): one borrowed texel per face, no bake, no shadow lists.
* Texture coordinates follow `codol_edit` ( quake style axes, 256 texel default size ): not compared with the game.
  Tangent = the texture's u axis, binormal sign from the v axis.
* Triangle winding: clockwise seen from the normal's side, as 4004 of 4004 / 3899 of 3899 sampled triangles of the
  base zones are.
* A static model that is turned or scaled gets collision bounds that hold the old bounds taken through both
  placements: never smaller than the tight box the game's compiler would give, usually larger ( reported ).
* A moved model lives in one cell afterwards. The base zones list some models in several cells ( 148 of 16335 in
  mp_killhouse_sh ); a model that reaches through a portal may be culled with the cell of its centre.
* New surfaces whose centre lies in a cell that draws no surfaces go to the nearest leaf of another cell ( reported ).
* **`cLeafBrushNode` and the loader.** The translated loader of `codol_zone` converts the `data` field of every node
  whose `leafBrushCount` is not zero as a zone pointer - also for nodes with count -1, where the field is the split
  distance. In the loaded image those distances are therefore garbage; `codol_mapc` reads the nodes from the zone
  *stream*. Whether the client really does this, or the translation is off by a sign extension, is not known ( the
  server's reader takes `dist` as a float for every node that lists nothing, so it reads it as this compiler does ).
  The compiler does not depend on it: it never computes with `dist`, and the tests check that every split of a
  compiled zone has the axis, `dist` and `range` of a split of the base zone, in the stream.
* The semantics of the leaf brush tree ( child 0 above `dist + range`, child 1 below `dist - range`, count -1: the next
  node holds the straddling brushes ) are those of the Quake 3 / IW engine family, confirmed on the data ( for every
  inner node of mp_killhouse_sh with a readable distance, child 0's brushes lie at `dist + range` or above ) and by
  the trace test below - not by reading the 1.1.1.13 client's code. Of these the compiler uses one: a trace that
  enters a node with count -1 enters the node behind it. In the base zones that node is a list for most splits and
  a tree of its own for some.
* `cplane_s.signbits` of new planes is 0 and `type` 3, as the planes of the base zones' brush sides have them.
* Layouts not in `c1_layouts.hpp` come from CODOL-AssetExplorer ( `MapEdits.cs`, `WorldCells.cs` ): `GfxAabbTree`
  ( 44 bytes ), `SModelAabbNode` ( 28 ), `GfxShadowGeometry` ( 12 ), `cNode_t` ( 8 ), `XModel.radius / bounds` at +316.

## Verification

All on the 1.1.1.13 install of this machine, x64 Release. `codol_mapc_tests` makes the edited maps with `codol_edit`
from the imports of **mp_rust** and **mp_killhouse_sh**, compiles the *saved* `.map` and reads the result back.
45752 checks, 0 failed. `tests/run_e2e.ps1` then runs the command line tool on the same maps ( its zone is byte for
byte the zone of the library call ), `codol_linker --verify` on every fast file ( decodes, every asset loads, read ->
write identical ), and renders pictures.

| case | edit | checked |
|---|---|---|
| identity | none | inflated zone == the base zone, byte for byte ( both maps ) |
| entities | one origin edited, one key added, one entity deleted, one added | the zone's entity string == `entity_string_of`; only `col_map_mp` ( which holds the MapEnts ) differs; import: same brushes and static models |
| static models | 3 moved ( one raised, one turned 90 degrees and raised, one scaled x2 ), 3 deleted ( incl. the map's first instance, which carries its XModel ), 1 added | counts; every instance has a model; every instance is in a cell list and in the shadow list of its primary light; all node lists are runs of the root's list; collision entries follow ( moved / deleted / added ), the collision AABB tree names every entry once and its boxes hold their children; only `gfx_map` and `col_map_mp` differ; **the import of the compiled zone has exactly the static models of the edited document** ( model, origin, axes, scale ) and the same brushes |
| donor | 1 instance of `machinery_windmill`, which mp_rust does not have, from mp_dome | without `--donor`: left out, zone unchanged; with: the xmodel is a new top level asset in front of the gfx_map with its surfaces, the instance exists, import equal |
| brushes + surfaces | a box and a wedge added in a material the world uses, one world brush moved 160 up, one deleted, the brush of a brush model made 8 units taller | brush count, sides ( box 0, wedge 1 ), contents, both plane counts; the traces below; surface / vertex / index counts; every static surface is in exactly one leaf run, identically in both halves, inside its leaf's box; new surfaces index their own vertices and lie where the brushes are; the base zone's surfaces, vertices and indices are all still there in order; only `gfx_map` and `col_map_mp` differ; **the import of the compiled zone has exactly the brushes of the edited document** ( planes and materials, per sub model ) |
| leaf brush trees ( collision only ) | around the spawn point every third world brush deleted ( mp_rust 334, mp_killhouse_sh 722 ), then all of them ( 1001 / 2166 ); 98 boxes added on a grid through the map and 6000 units above it | every way a tree changes: lists that run empty and splits that lose a side are removed ( up to 2072 nodes ), new brushes join a list, the brushes on a root's split, a new list on a root's split, a tree of their own; every new box is in the tree of a leaf whose box holds it; tree walk == brute force on 6000 segments; import equal |

Every compiled zone of every case is also read back **the way a dedicated server reads it** ( `check_server_collision`,
below ): accepted; every leaf brush tree walked from its leaf ends in lists; no brush outside its leaf's box, no list
whose node lacks a brush's contents, no brush in no tree ( all 0 in the base zones ); every split has the axis, `dist`
and `range` of a split of the base zone; every collision static model's pointer leads to where its XModel is stored.

"Differs" is judged by content ( bytes + what every pointer leads to ). `ZoneEditor::asset_fingerprint` is stricter:
it also changes for an asset that only *points into* an asset that gained a piece of data.

**Collision traces** ( `clip_trace`, `codol_mapc/trace.hpp` ). This is this library's own walk of the zone data - BSP
nodes, collision leafs, leaf brush trees, brush planes - written from the layouts. It is **not the game's trace**
and says nothing about what the game does; it shows that the written structures are consistent:

* tree walk against testing every brush, 4000 random segments ( every third a 30 x 30 x 70 box ) around a spawn point:
  mp_rust 199 hits, mp_killhouse_sh 1136 hits, **0 disagreements** on the base zones and on the compiled zones;
* a ray down through the new box enters at its top ( -130.00 / 136.00 ) where the base zone has nothing; a player
  sized box pushed against its side stops at the face; the box is in the tree of the collision leaf it stands in;
* a ray down onto the wedge meets the slope at half height;
* the short ray that met the deleted brush in the base zone meets nothing; the moved brush is met at its new place
  and not at the old one;
* the brush model's tree and its brush agree, its top is 8 units higher, its boxes and radius followed.

**Pictures** ( `build/mapc_test/png/<map>_<stage>_<view>_before|after.png`, from `run_e2e.ps1`; looked at by eye ):

* `mp_rust_models_a_*`: the two corrugated panels on the shack's roof are gone, a wall panel floats 120 units up
  beside the hole it left, a panel is turned and raised, a door is twice its size, a new panel lies above the spawn.
* `mp_rust_brushes_a_*`: a rusty metal box stands behind the fence next to the pipe and a wedge in front of the roof,
  both textured and lit like the metal around them.
* `mp_killhouse_sh_brushes_b_after`: the new box in the hall, in the brightness of the room.
* `mp_rust_donor_a_after`: a windmill of mp_dome at the spawn point.

`codol_zone`: unit tests 969 checks, 0 failed ( 42 new ones for the typed editing ); read -> write of all 153 fast
files of the install is still byte identical.

## What to test in the game first

Nobody has loaded a compiled zone in the client. In this order:

1. **The unedited map** compiled with the tool ( = the base zone in an unsigned container ): proves the container and
   the install's way of loading unsigned zones, before any edit is blamed.
2. **Entities only**: a moved spawn point.
3. **One static model moved a few units**: is it drawn, lit sensibly, culled correctly when turning the view and when
   walking away ( cull distance ), does it collide at the new place and not at the old one. Then a deleted one, an
   added one, and one of each next to a portal ( indoor / outdoor maps ). Watch for models that vanish at certain view
   angles: that would be the cell lists ( the client sorts the static models at load and renumbers the lists ).
4. **The last instance of a model deleted**: the holder / the removed XModel ( the report says which happened ).
5. **A box brush** in open space: walk against it, stand on it, shoot it ( bullet impacts use the clip material's
   surface type ), throw a grenade at it. Then the same box where several collision leafs meet, and a brush with
   sloped sides. This is where a wrong reading of the leaf brush tree or of the edge lists ( used by the engine's
   physics for brush geometry ) would show.
6. **The box's faces**: drawn at all, from all distances and angles ( the sorted surface lists ), texture alignment,
   brightness against the surroundings by day and with dynamic lights / flashlight, sun shadow on and from it.
7. A **deleted brush** and a **moved brush**: collision gone / moved; expect the old render geometry to still be there.
8. A **brush model's brush** edited ( a door, a platform ): collision of the entity in game.
9. A model from a **donor zone**: drawn, with the right materials ( its techsets and images have to be loadable from
   the map's or the common zones ).
10. On a **dedicated server** ( CoD4x_Server ): the zones load and matches run ( next section ). Still open: traces of
    players and bullets against the new brush, with the server's `codol_sweepprobe` / `codol_floorprobe` /
    `codol_solidprobe` ( `developer 1`, rcon with a password of 8 characters or more ).

## On a dedicated server

CoD4x_Server ( `codol_dedrun` ) does not run the game's loaders on a map zone. It decodes the clip map from the zone
stream with a reader of its own ( `src/codol/collision` ), and that reader takes the shape the game's map compiler
gives the data, not everything the loaders would follow. What it insists on, beyond valid indices and finite numbers:

| data | rule | reader |
|---|---|---|
| `brushsides`, `brushEdges` | walking the brushes in order, the sides ( edge bytes ) of each brush are the *next* run of the array, and after the last brush the array is used up. No run behind the others, no entries nobody names, nothing stored behind a brush | `codol_collision_brushes.c` |
| `leafbrushes` | the same for the lists of the leaf brush nodes, in node order ( a list may instead be stored behind the node array ) | `codol_collision_brush_tree.c` |
| `leafbrushNodes` | a node lists brushes ( count > 0 ) or splits: axis 0 .. 2, finite `dist`, `range` >= 0, both child offsets > 0 and inside the array; count -1 needs a node behind it. Only node 0 may be all zero: there is no empty node | `codol_collision_brush_tree.c` |
| planes | the clip map points at the GfxWorld's plane array, both counts equal; unit normals ( length^2 within 0.001 ) | `codol_world_planes.c`, `codol_collision_brushes.c` |
| side / node planes | pointers into that array ( never a plane stored behind the side ) | `codol_collision_brushes.c` |
| `staticModelList` | the model is a pointer, and it leads to where the XModel is stored: the model field of the GfxWorld instance that carries it, or the header slot of a top level xmodel | `codol_collision_stream.c`, `codol_load_blocks.c` |
| BSP nodes | children lead forward or to a leaf | `codol_collision_brushes.c` |

`cplane_s.type` / `signbits` are recomputed by the server, `smodelNodes` is not read, the checksum in the clip map
header ( +0xCC ) is taken as it is - the compiler leaves it alone, so an edited map reports the checksum of the
base map.

The brush stage used to append: the list of a tree that got a brush moved to the end of `leafbrushes`, the sides and
edge bytes of a replaced brush to the end of their arrays, the old runs stayed behind unreferenced, an emptied list
became a node with nothing in it. Each of these is a zone the loaders follow and the server refuses
( `CoDOL collision <map>: source decode failed (-1)` ); with one new box it was the first. The stage now writes the
three arrays whole and edits the trees as trees ( above ), and `check_server_collision` ( `codol_mapc/server_check.hpp`,
`codol_mapc --check` ) is that reader's set of rules, used by the tests on every compiled zone and by the compiler on
its own output. All 42 map zones of the install pass it as shipped.

Runs with `codol_dedrun`, dedicated, loopback, `+map mp_rust`, 12 bots, 80 s each, on a copy of the install with the
compiled `mp_rust.ff` in it. For every zone the server logged `CoDOL native collision installed: maps/mp/mp_rust.d3dbsp,
758 leaves ..`, all 12 bots played ( 48 .. 55 kills ), and the log has no error-like line the run on the shipped zone
does not have:

| zone | edits | brushes the server built |
|---|---|---|
| one new box in the world ( from the editor's Compile map ) | + entities | 3227 |
| `brushes` test | box, wedge, world brush moved, world brush deleted, the brush of brush model `*2` made taller, 3 render surfaces | 3227 |
| `models` test | 3 static models moved / turned / scaled, 3 deleted, 1 added | 3226 |
| `donor` test | a static model from another zone, with collision | 3226 |
| `brushes` + `models` in one map | all of the above | 3227 |
| `brush_trees_all` test | 1001 brushes deleted, 98 boxes added | 2323 |

Not shown by these runs: that a player or a bullet is stopped by a new brush and passes where one was deleted. The
server has commands for exactly that ( `codol_sweepprobe`, `codol_floorprobe`, `codol_solidprobe` ), behind
`developer 1`, and its rcon refuses passwords shorter than 8 characters; the runs above had neither.

## How the layouts were established

`codol_mapc_probe graph|fields|gfx|smodels|clip <map.ff> <C1.exe> [asset type]` prints the stream order of an asset's
data, where every pointer field of a header leads, and the properties of the shipped data the stages rely on ( which
lists are runs of which, where model pointers point, how the leaf brush trees are shaped .. ). Every stage checks the
properties it needs on the zone it is given before it changes anything and stops with a message when they do not
hold; nothing is written then.
