# iw3xo-radiant for Call of Duty Online 1.1.1.13

This branch (`codol-1.1.1.13`) turns iw3xo-radiant into a workbench for Call of Duty Online
1.1.1.13 ("C1", an IW4-derived engine, fastfile version 334). It keeps everything upstream does
and adds a **CoDOL** menu.

- [CoDOL Radiant: the standalone editor](#codol-radiant-the-standalone-editor) - [the tool chain](#the-tool-chain),
  [3D editing](#3d-editing), [a map of your own](#a-map-of-your-own) step by step, [your own assets](#your-own-assets)
- [What works](#what-works) ( the radiant plugin )
- [What does not exist yet](#what-does-not-exist-yet)
- [Setup](#setup)
- [Script Workbench](#script-workbench)
- [Map Entities](#map-entities)
- [Game window and hot reload](#game-window-and-hot-reload)
- [Console commands and dvars](#console-commands-and-dvars)
- [Code layout, build and tests](#code-layout-build-and-tests)
- [Roadmap](#roadmap)

## CoDOL Radiant: the standalone editor

`CoDOLRadiant.exe` is its own program. It does not load, hook or need CoD4 radiant or the CoD4 mod tools: maps,
models, materials and shaders are read from the 1.1.1.13 install and drawn with the game's own shaders.

```
cmake -S editor -B build-editor -G "Visual Studio 17 2022" -A x64
cmake --build build-editor --config Release
build-editor\Release\CoDOLRadiant.exe
```

Settings are in `codol_editor.cfg` next to the exe ( File > Settings ); the CoDOL install folder is the one that is
needed. What it is made of:

| Part | Role |
|---|---|
| `editor/` | the application: window, Direct3D 9 device, docking UI, 3D view, browsers |
| `codol_zone/` | fastfile reader ( and writer, in progress ) |
| `codol_render/` | materials, technique sets, shaders, images |
| `codol_view/` | map renderer with the editor API: picking, moving / hiding / adding models, meshes with game materials |
| `codol_edit/` | map document: `.map` reader / writer, brushes, patches, entities, selection, undo, editing operations, entity classes taken from the shipped maps |
| `src/codol`, script workbench, game bridge | shared with the radiant plugin |

### The tool chain

| CoD4 mod tools | here | what it does today |
|---|---|---|
| Radiant | `CoDOLRadiant.exe` | open a map of the install or one of your own, edit entities / models / brushes in 3D and 2D views with undo, light entities with a panel of their own, save as `.map` |
| `cod4map`, `cod4rad`, the map's linker step | `codol_bsp.exe` ( File > Build map ) | builds the whole zone of a map of your own from its `.map` alone: collision ( brushes, terrain patches, static models ), BSP, cells and portals, render geometry, light bake ( sun, lights, bounce in the materials' colours, light grid, reflection probes ), primary lights ( spot, omni, scriptable ), minimap picture, load zone, level scripts. See [A map of your own](#a-map-of-your-own). |
| editing a shipped map | `codol_mapc.exe` ( File > Compile map ) | applies the edits of a `.map` to the fastfile of the map it was imported from: entities, static models moved / deleted / added, collision brushes, render faces of new world brushes, and the project's assets ( animations, sounds, effects, models ). It patches the shipped zone; no light bake. |
| Asset Manager, `converter` | the Assets window, `codol_assetc.exe` | your own textures, materials, models, animations, sounds and effects: PNG / TGA / DDS, `.mtl` or the material entries of a CoD4 `.gdt`, `xmodel_export`, `xanim_export`, soundalias `.csv` + `.wav`, `.efx` -> native assets in `mod\<map>\<map>_assets.ff`. Materials and models are shown by the editor at once; animations, sounds and effects are for the level script of the built map. See [Your own assets](#your-own-assets). |
| the zone source `.csv` of a map, `linker_pc` on it | `tools\package_map.ps1` ( part of File > Build map ) | puts the project's assets, animtrees, animlist and scripts into the map zone ( `codol_assetc --carry`, `--rawfile` ), adds the minimap material, builds the load zone and writes the package |
| `linker_pc` | `codol_linker.exe` | reads and writes fastfiles ( all 153 of the install round trip byte for byte ), replaces entities / rawfiles / string tables, takes assets from other zones, builds a zone of rawfiles |
| script editing | Script Workbench, rcon reload | unchanged |

Compiled fastfiles are unsigned ( `IWffu100` ); the signed form needs a key the client does not have. Per the notes
in the install, the retail client only loads unsigned zones from disk with the `Kernel33.dll` patch; the dedicated
server reads both. Maps built from scratch and edited shipped maps have both been loaded on the dedicated server and
in the client - on one machine, over loopback, with a test account, the lobby emulator of the install and a
dedicated server ( `codol_dedrun` ); nobody has played them over a network. `codol_bsp/README.md` and
`codol_mapc/README.md` list what was checked where, with the pictures, and what is still open.

### 3D editing

Brushes and patches are made and shaped in the 3D view as well as in the 2D views. The left mouse button belongs to
the tool that is on ( second row of the view's toolbar ); right drag looks around and flies ( W A S D ), middle drag
pans, the wheel moves forward and back - as before. Every drag is one undo step with a name ( Move, Resize, Move
face, Move vertices, Move edges, Clip, Create box .. ), snaps to the grid ( `[` `]`, the Snap box ), keeps the
texture lock ( Shift+T ) and shows in the 2D views while it happens. Esc during a drag takes it back. The two or
three lines at the bottom of the view say what the mouse and the keys do in the tool that is on.

| Tool | Key | Left button |
|---|---|---|
| **Select** | Esc | Click selects ( Ctrl or Shift adds / removes ). **Drag on the selection**: it moves, on the world plane that faces the camera most ( look down: on the floor; look along: up and sideways ). **Drag next to a selected brush**: the faces on that side follow the mouse - Radiant's resize; a drag that starts past a corner takes both faces. **Squares on the selection's box**: that side of every selected brush. **Ctrl+Shift click** selects a face ( again: deselects it ); **drag a selected face**: it moves along its normal, with every other selected face. **Shift drag**: everything in the rectangle is added to the selection. **Alt click**: the object behind the selected one under the mouse ( Alt+Shift click adds it ) - through walls and into stacks. With nothing selected a left drag drives the camera, as before. |
| **Vertices** | V | Handles on the corners of the selected brushes ( green ) and the control points of the selected patches ( pink ). Click a handle: select it; Ctrl or Shift click adds / removes; Shift drag: the handles in the rectangle. Drag a handle: the selected handles move on the grid, on the plane that faces the camera most; with **Alt** along the third axis instead. The arrows of the gizmo on the selected handles move them along one axis. A corner that brushes share moves in all of them; sides that are no longer flat are split into triangles ( the brush is rebuilt as the hull of its corners ). |
| **Edges** | E, while the handles are shown | Handles on the middle of the edges ( blue ); an edge moves as a whole, the same way. E again or V: back to the vertices. |
| **Clipper** | C | Click on a surface: a clip point; two or three, drag to move them. Two points cut straight through the surface the first was put on, three define the plane. The cut is drawn on every selected brush, the arrow shows the side that stays. Enter clips, Ctrl+Enter keeps the other side, Shift+Enter splits; Esc removes the points, then leaves. Points placed in a 2D view and in the 3D view are the same points. |
| **Create** | B | Drag a rectangle on the surface under the mouse - the ground, a wall, the top of a brush, the floor of an empty map: a brush stands on it, as high as the last selection was ( 64 when there was none ). It is selected: drag the squares on its box to size it. The list next to the button picks the shape: box, cylinder and cone ( with a number of sides ), wedge, terrain patch. |

The keys that were there stay: W / E / R pick the gizmo ( move / rotate / scale ) - E is the edge mode only while the
vertex handles are shown, where nothing is rotated -, arrows and PageUp / PageDown nudge the selection, Space clones,
Delete deletes, F goes to the selection. New, for the faces that are selected ( or every face of the selected brushes,
as in Radiant ): **Shift + arrows** shift the texture by the grid size, **Ctrl + arrows** scale it by 5 %,
**Shift + PageUp / PageDown** rotate it by the angle snap.

**Materials**: drag a material out of the Materials window and drop it on a face in the 3D view; with Shift held at
the drop it goes on every face of that brush; on a patch it replaces the patch's material. The selection does not
change.

What a drag works on is the document: in a shipped map the world is the zone's render geometry, which no tool
changes - the tools see its surfaces ( to put a clip point or a new brush on ) and the collision brushes
( `Brushes: all` shows them ).

For unattended runs the tools have a console command that calls what the mouse calls, at pixels of the view or at
the pixel a world point projects to: `ed3d tool <object|vertex|edge|clip|create>`, `ed3d primitive <box|cylinder|cone|wedge|terrain> [sides]`,
`ed3d press <px> <py> [shift] [ctrl] [alt]` / `drag <px> <py>` / `release` / `click ..`, the same with a world point
( `wpress x y z`, `wdrag`, `wclick` ), `ed3d drop <material> <px> <py> [brush]` / `wdrop`, `ed3d handles` ( the handles
with their pixels ), `ed3d faces` ( the faces of the selection ), `ed3d mouse <px> <py>` / `wmouse x y z` ( the injected
mouse of `ui_mouse`, on a pixel of the view ). `tests\ed3d\run.ps1 -Exe <CoDOLRadiant.exe> -Script tests\ed3d\<name>.cmds`
runs a command list and checks its `expect` lines against the console log: `object` ( Select tool ), `handles`
( vertices, edges, clipper, create, material ), `shipped` ( in a map of the install ) go through `ed3d press`;
`mouse` and `matdrop` feed the mouse and the keyboard themselves ( `ui_mouse`, `ui_key` ) and so go through the
gui, the gizmo and the shortcuts.

### A map of your own

From an empty editor to a map you walk around in. Every step is a menu entry and a console command; what was seen
of each step in the game, and where, is said with it - the details and the pictures are in `codol_bsp/README.md`.

**0. Once: settings.** File > Settings: the CoDOL install folder ( the one with `C1.exe`, `main\` and
`client_shipRetail\` ) and, for the install step, the dedicated server's script folder. The first build scans the
install's zones ( about 16 s ) and the first asset build its materials ( 20 s ); both are kept.

**1. File > New map**, a name like `mp_mymap` ( `a-z 0-9 _` ). It is saved as `map_source\mp_mymap.map`; its
asset project is `mod\mp_mymap\` ( [Your own assets](#your-own-assets) ).

**2. Draw the map.** Brushes, terrain patches, `misc_model`s ( static models ), `reflection_probe`s.

* The map has to be **closed** ( no way from an entity to the void ) or stand in a sky box; the build reports a
  leak with its position.
* **Players do not collide with static models** ( the engine's rule, bullets do ): put `clip` brushes around models
  one should not walk through.
* Doorways and windows between rooms become portals by themselves ( planar openings in a wall ); `portal` brushes
  of your own switch that search off.
* *The 3D view* draws a map of your own in a *stage*: an empty world that `codol_bsp` compiles from the light keys
  of the worldspawn ( a second or two, kept in `compiled\cache\stage\` ). Brushes with their materials, models
  where their entities stand, lit by the sun, sky and ambient light the map will have. **It is not the light
  bake**: light entities are drawn as shapes, their light and all shadows of brushes are what File > Build map
  makes.

**3. Light.** Nobody has to type keys: with nothing selected the **Entity** window shows *Light of the map* ( sun,
its colour and direction with a small compass, sky light, ambient light; a field whose key is not set shows the
compiler's default, the `x` behind a field takes the key away ), with a light selected its **Light** section
( colour, intensity, radius, kind, falloff, cone, "scriptable" ). "Show in 3D" builds the stage again when the
light keys changed ( `ed_restage` ). While a light is selected the views draw where it reaches: circles of its
radius, or the cone of a spot as the compiler reads it. Two keys have no field and are typed: `radiosityscale` of
the worldspawn and the cone of an omni light ( `ed_key <key> <value>`; it goes to the selected entity, or to the
worldspawn when nothing is selected ).

| what | keys | in the game |
|---|---|---|
| the map's light | worldspawn `sundirection`, `suncolor`, `sunlight`, `ambient` + `_color`, `skylight` + `skycolor`, `radiosityscale`. Without them: a sun of 1.3 from `-50 40 0`, ambient 0.1, sky 0.4, bounce 1 | sun with run time shadows of players and models, sky light, bounced light in the colour of the materials it comes off |
| a baked light | `light` with `radius` ( 600 ), `intensity` ( 1 ), `_color`; it is down to a quarter at half its radius | in the lightmaps and the light grid; nothing that moves throws a shadow in it |
| a primary spot | `spawnflags 2`, `fov_outer` / `fov_inner` / `exponent`, `angles` or a `target` | lights world and models at run time; players and script models throw shadows in it ( seen ) |
| a primary omni light | `spawnflags 1` | lights all around at run time. Players and models throw shadows **only inside its cone** - straight down, 120 degrees, full strength inside 60, as in the game's own maps; `fov_inner` / `fov_outer` / `angles` change the cone ( seen and measured ) |
| a light a script changes | "scriptable" ( `spawnflags 4` ), a `targetname`; `maxmove` / `maxturn` when the script moves or turns it | the script gets it with `getEnt` and calls `setLightIntensity`, `setLightColor`, `setLightFov`, sets `.origin` ( seen ). What the light could reach within `maxmove` / `maxturn` is lit by it at run time where no other light is |
| the falloff | `def`: `light_point_linear` ( default ), `light_point_quadratic`, `tungsten_lamp` .. - any definition of the install | three of them seen |

Two things a mapper runs into: a surface is lit at run time by **one** primary light - floors and walls are cut
where the reach of two of them meets, along a grid of 32 units; and on the test server a script moves a light only
by setting `.origin` ( `moveTo` and `linkTo` on a light are script errors there, though the game's own scripts use
them ). All keys with their defaults: `codol_bsp/README.md`, "Light keys".

**4. Your own textures, models, animations, sounds, effects** - optional: [Your own assets](#your-own-assets).
Import them, press **Build** in the Assets window, use them. A level script of your own
( Assets > **Level script** ) is where animations, sounds and effects are played from.

**5. What the game's scripts ask of every map.** Spawn points ( `mp_dm_spawn`, `mp_tdm_spawn_*` .. ), one
`mp_global_intermission`, `node_pathnode`s for bots. **Edit > Add gameplay entities** ( `ed_gameplay` ) adds what
is missing besides: the care package with its collision ( without it `_airdrop.gsc` throws a script error at every
map start ) and a helicopter path ( `heli_start`, `heli_loop_start` .. ). The build names what is missing.

**6. File > Build map** ( `map_build`; `map_build fast` while you work on the layout ). The compile, the light bake
and the packaging follow each other; the result is a package in `compiled\<map>\`:

| | |
|---|---|
| `zone\<map>.ff`, `zone\<map>_load.ff` | the map zone ( with the minimap material `compass_map_<map>` ) and its load zone; a picture `map_source\<map>_load.png` becomes the loading screen |
| `scripts\maps\mp\<map>.gsc` ( + `_fx`, `createfx` ) | the level script for the dedicated server's script folder: generated, or your own |
| `main\zz_<project>_sounds.iwd` | only when the project has streamed sounds: their files, as a plain archive |
| `presence\` | the row, arena entry and name for the create-room list, and `INSTALL.txt`: where every file goes |
| `work\<map>.report.txt`, `work\scripts\` | what the compiler did: counts, timings, warnings; the scripts as generated |

From the asset project the zone gets the materials and models the map uses, every animation, sound alias, effect
and model of the project's zone, the animtrees and the animlist, and the files of the project's `scripts\` as
rawfiles ( [In the built map](#in-the-built-map) ). Read the report's warnings: a leak, a material no zone has
( its faces are not drawn ), a light definition that was replaced.

**7. File > Install the built map ..** ( `map_install` ). It asks for the game folder, the server's script
folder and whether the map goes into the create-room list. Three things have to end up in the right place:

| from `compiled\<map>\` | to | why there |
|---|---|---|
| `zone\<map>.ff`, `<map>_load.ff` | **`<game>\client_shipRetail\chinese\`** - give it as the zone folder: `map_install game=<game> zones=<game>\client_shipRetail\chinese`. Without a zone folder they go to `<game>\usermaps\<map>\` | client and server can read both places, but the **lobby** starts a room's map only when `<map>.ff` is in *its* zone folder - `client_shipRetail\chinese` of the install, or the folder `CODOL_ZONE_DIR` names; it never looks into `usermaps\`. Every run in the game described here had the zones in `client_shipRetail\chinese`. A dedicated server with another base path needs the zone under *its* `client_shipRetail\chinese` too |
| `scripts\..` | the server's script folder | the dedicated server reads level scripts from its script folder, never from a zone. Without the level script the map still runs, but nothing of your script does |
| `main\zz_*_sounds.iwd` | `<game>\main\iw_sound_NN.iwd` ( written again: encrypted, every entry deflated, under the first free number from 23 down ) - **in `main\` of the client and of the server** | the game opens only archives named like its own, and plays a streamed file only out of an archive the *server* has as well ( the server tells the client which archives are "pure"; a client with an archive the server lacks ends with "Impure client detected" ). Client and server of one install share `main\`; **a server that runs from another folder needs a copy of the archive in its own `main\`**. By hand: `codol_assetc --install <game> --iwd <game>\main\iw_sound_23.iwd --iwd-add zz_<project>_sounds.iwd` |
| `presence\comaplist_row.csv`, `basemaps_arena_entry.txt`, `localize.txt` | the create-room list of `<game>`, with "Add to the create-room list" ( `list` ) | below |

* A file that is in the way and differs is kept once as `<file>.pre-<map>` before it is replaced; a file that is
  the same already is left alone - installing twice changes nothing.
* Everything written is noted in `compiled\<map>\work\installed.txt`. **Uninstall** ( the same window, or
  `map_uninstall` ) undoes exactly that: new files are deleted, replaced files get their backup back, the map
  comes out of the list, `usermaps\<map>\` goes when it is empty.
* The client has to be started as `codoMP_client_shipRetail.exe` with the install's `Kernel33.dll`: compiled zones
  are unsigned, and per the install's notes only that start loads unsigned zones from disk. No change to
  Kernel33, lobby or server is needed for a new map name.

**8. Play.**

* **Without the list**: start the lobby with `CODOL_FORCE_MAP=<map>` ( and `CODOL_FORCE_GAMETYPE` ), then
  Multiplayer > Start > Search > Ready in the client. This is how every map of these pages was entered.
* **From the create-room list**: install with "Add to the create-room list". The list is three things ( the
  install's `usermaps\build_usermaps_patch.py` describes how the client reads them ): a row of the string table
  `mp/comaplist.csv`, an entry of the rawfile `mp/basemaps.arena` and a localize entry for the name. The client
  keeps the first asset of a name it links and links the zones of `client_shipRetail\patch\` first, so the list is
  whichever zone of that folder holds `mp/comaplist.csv` - in the install of this machine `codol_common.ff`. The
  editor reads that zone, adds the row, the arena entry and the name, writes it again, reads it back and only then
  puts it in place ( without such a zone it makes `usermaps_patch.ff` from the stock list ). `map_list [game
  folder]` prints which zone is the list and its last rows. **Written and loaded by the client, not seen**: the
  client runs its menus with the list zone the editor wrote ( 97 rows ), but its *Create Room* is locked below
  player level 5 and the test account is level 1, so the row was not looked at and no map was started from it.
  Whatever build makes `codol_common.ff` is outside the install and the lobby's folder; when it runs again the row
  is gone with the file - `presence\` is the input for that build's map table.

**What is still open for a map of your own** ( the whole list: [What does not exist yet](#what-does-not-exist-yet) ):
animations of your own have not been seen *moving* in the game ( the test server does not send script model
animations to the client, shipped ones included ); the create-room row was not seen; primary lights cut surfaces
along a coarse grid and an omni light shadows only inside its cone; static models have no player collision; a
script moves a light only through `.origin` on the test server.

Console: `map_new <name>`, `ed_gameplay`, `map_build [fast]`, `map_install [zone folder] [script folder]` ( the
folders of File > Settings when none is given ), `map_install game=<folder> [scripts=<folder>] [zones=<folder>]
[list]`, `map_install window`, `map_uninstall [map]`, `map_list [game folder]`.
The radiant plugin ( `iw3r.dll` inside CoD4 radiant ) described below still builds, but the standalone editor is
where the work goes.

### Your own assets

Every map has a project folder next to the editor ( the project is named after the map; `asset_project <name>`
picks another one ):

```
mod\<project>\images\*.png | *.tga | *.dds        source images; the image asset is named after the file
mod\<project>\materials\*.mtl                    material definitions ( codol_assets/README.md, ".mtl" )
mod\<project>\models\<name>.model                a model: LOD files with distances, collision LOD, material names
mod\<project>\models\*.xmodel_export             LOD files; one that no .model names is a model of one LOD
                                                 ( <name>_lod0 .. _lod3.xmodel_export belong together )
mod\<project>\anims\<name>.anim                  an animation: its xanim_export, model, type, loop, animtree
mod\<project>\anims\*.xanim_export               one that no .anim names is an animation of that name, no options
mod\<project>\soundaliases\*.csv                 sound aliases ( columns of the CoD4 mod tools ); *.vfcurve, *.spkrmap
mod\<project>\sound\<file>                       what the file column of the aliases names ( .wav, streamed: .mp3 / .wav )
mod\<project>\fx\<folder>\<name>.efx             an effect; its name is <folder>/<name>, what loadfx takes
mod\<project>\scripts\..                         your own scripts: maps\mp\<map>.gsc, animtrees\<tree>.atr ..; not
                                                 compiled, the built map takes them as they are
mod\<project>\vision\<map>.vision               the map's vision file: glow, film tweaks ( r_glow "1", r_filmContrast
                                                 "1.2" .. ); without one the build writes a neutral default
mod\<project>\sun\<map>.sun                     the sun's sprite and flare ( r_sunsprite_size, r_sunflare_max_size .. );
                                                 without one a map with a sun and a sky gets a default flare
mod\<project>\generated\animtrees\<tree>.atr     written by the build: one per animtree the animations name
mod\<project>\generated\animlists\map.animlist   written by the build: becomes animlists/<map>.animlist of the map
mod\<project>\zz_<project>_sounds.iwd            written by the build: the streamed sound files
mod\<project>\<project>_assets.ff                 the zone built from all of it
mod\<project>\build_state.txt                     what the last build made of each source
mod\codol_matlib.txt                             the material library: one scan of the install ( 20 s, once )
mod\codol_fxatlas.txt                            the texture atlases of the install's materials ( 30 s, once, with
                                                 the first effect )
```

**View > Assets** lists every source with its status - new, changed, built, failed with the compiler's message,
unused ( an image no material names ), as is ( a script file ) - and has **Build**. A build compiles everything
again on a worker thread ( `codol_assets`: `compile_image`, `compile_material`, `compile_model`, `compile_anim`,
`build_sound_aliases`, `compile_fx`, `AssetLinker` ) into a new zone; the editor stays usable. The renderer then
takes the zone without loading the map again ( `map_renderer::register_zone` ), and every mesh and placed model is
made again from it. A click on a line shows the asset in the **Preview** window ( below ).

| what | where | command |
|---|---|---|
| material from images | Materials > **New material ..**, Assets window, or drop image files on the editor window ( `_n` / `_nml` / `_normal` and `_s` / `_spc` / `_spec` at the end of a name fill the normal and specular map ) | `asset_new_material <name> <kind> <colour map> [normal=<file>] [specular=<file>] [surface=<type>] [overwrite]` |
| model | Models > **Import model ..**, or drop `.xmodel_export` files | `asset_import_model <name> <lod0>[@dist] [<lod1>[@dist] ..] [collision=<lod>] [mtl:<export material>=<material>] [overwrite]` |
| materials of a CoD4 `.gdt` | Materials > **GDT ..**, or drop a `.gdt` | `asset_gdt <file> [root=<folder the image paths start from>] [overwrite] [all \| <material> ..]` ( without a choice: the list ) |
| animation | Assets > **Import animation ..** ( file, name, model, type, looping, animtree ), or drop an `.xanim_export` | `asset_import_anim <file.xanim_export> [name=<name>] [model=<model of the project \| file.xmodel_export>] [type=relative\|delta\|delta3d] [loop] [tree=<animtree>] [overwrite]` |
| sound aliases | Assets > **Sounds ..** ( the csv, the folder that holds `sound\` and `soundaliases\`, overwrite ), or drop a soundaliases `.csv`: the csv is copied with the sound files, curves and speaker maps it names ( from `sound\` and `soundaliases\` around it ) | `asset_import_sounds <file.csv> [root=<folder with sound\ and soundaliases\>] [overwrite]` |
| effect | Assets > **Effect ..** ( the file, a name of your own, overwrite ), or drop an `.efx`; it is named by its place below an `fx` folder ( `..\fx\misc\x.efx`: `misc/x` ), else `<project>/<file>` | `asset_import_fx <file.efx> [name=<folder/name>] [overwrite]` |
| level script | Assets > **Level script**: makes `scripts\maps\mp\<map>.gsc` with what the compiler would generate, to start from | `asset_level_script [<file.gsc>]` ( with a file: a copy of it ) |
| build, list | Assets window | `asset_build`, `asset_list`, `asset_rescan`, `asset_status`, `asset_project [name]` |
| check the zone | | `asset_verify` ( `codol_assetc --verify`: the game's loaders read the zone, it is written again and compared ) |
| look at it, listen to it | a click in the Assets, Models or Materials window | `asset_preview model <xmodel> \| material <name> \| image <file> \| anim <name> [model] \| sounds <csv> \| fx <name> [zone \| file.ff] \| time <seconds> \| frame <n> \| play \| pause \| orbit <yaw> <pitch> [zoom] \| status \| close`, `asset_play <alias> [zone \| file.ff]`, `asset_play stop` |
| the windows, for unattended runs | | `asset_dialog material \| model \| anim [file] \| sounds [file] \| fx [file] \| gdt [file] \| close`, `asset_drop <file> ..` ( what dropping the files does ), `ed_windows a` ( `p`: the Preview window ) |

Material kinds ( the technique set follows from the kind and the maps given, as `codol_assets` `techset_for` has it ):
`world`, `world_alpha` ( alpha tested ), `world_blend`, `decal`, `world_unlit` become `wc/<name>`; `model`,
`model_alpha`, `model_blend`, `model_unlit` become `mc/<name>`; `2d`. The material is the material of new brushes
right away and is applied like any other ( browser, `tex_set` ). A model is placed from the Models browser or with
`ed_place`; `ed_model_class misc_model` makes new model entities static models of the compiled map.

#### The Preview window

**View > Preview**, or a click: on a model in the Models window, a right click on a material in the Materials
window ( a left click too while the window is open ), on any line of the Assets window. A double click still uses
the asset. The pictures are drawn by the renderer of the open map into a target of their own; the camera turns
with the left mouse button and zooms with the wheel.

| what | what is shown |
|---|---|
| model ( of the game or of the project ) | the model as the game draws it, its size in units, **Place in the map** |
| material | a cube drawn with the material's own technique set and shaders, lit by the sun of the map; its colour map with its size; technique set, zone; **Use for new brushes**, **Apply to the selection** |
| image ( a source file of the project ) | the picture as `codol_assets` decodes it ( PNG, TGA, DDS ), size, alpha channel, whether its size is one the compiler takes; **New material from it ..** |
| animation | the animation on its model, with a **timeline**: Play / Pause, the frame ( drag it ), speed, Loop, length and frame rate, the note tracks as marks on the timeline and as a list; another model of the project can stand in. After a build the picture is made from the zone: the `XAnimParts` and the `XModel` are read back out of `<project>_assets.ff` ( `codol_zone`, `read_raw_anim`, `decode_anim`, `extract_model_lod` ), so what plays is the animation as the game gets it, compression included. Before the first build it is made from the `xanim_export` and the `xmodel_export`, and the window says which of the two it shows. The mesh is skinned in the editor ( every vertex by its bone weights ) and drawn with the model's materials. |
| sound aliases ( a csv of the project ) | every row with its file; **Play** plays the `.wav` of the row ( WinMM; an `.mp3` through MCI ), a row whose file is not in the project is a sound of the game. **Play from zone**: an alias name and a zone - empty for the project's zone, a zone of the game ( `mp_rust` .. ) or a `.ff` file -: the alias and its loaded sound are read out of the fastfile, IMA ADPCM is decoded by `codol_assets`, and that is played. |

| effect ( `fx\<name>.efx` of the project, or an effect of the game ) | the effect's particles over time, with a **timeline**: Play / Pause, the moment ( drag it ), speed, Loop, the number of particles. A project's effect is its `.efx`; one of the game ( `asset_preview fx fire/firelp_small_pm`, or with a zone / a `.ff` behind it ) is read out of the zone and turned back into curves ( `codol_assets` `read_fx`, `extract_fx` ). Both are simulated by the editor ( `codol_assets` `simulate_fx` ) and drawn as flat quads with the colour map of their material: sprites, tails, lines, clouds. **It is not the game's effect system**: no depth, no lighting, no fog, all particles blended - added where the material's name says light ( spark, fire, flare, glow .. ) -, a cloud is one particle, and trails, spark fountains, models, lights, sounds, decals and the effects an element starts are named under the picture and not drawn. The effect stands with its forward axis up, as `playfx( fx, origin )` plays it. |

Checked ( pictures in `build-editor\shots\` of the test run ): a shipped model and a project model, a shipped
world material and a project model material on the cube, a PNG, the two-bone test animation at frames 0, 20 and 30
( the lid moves ), the csv's rows; `asset_play` of a project alias from its `.wav` and from the built zone, and of
three aliases of shipped `mp_rust.ff` ( 32 and 44.1 kHz IMA ADPCM, decoded ); sixth round, the effect preview
( `build-editor\bsptest\shots\c6fx_sheet.png` ): the project's `codol_test/spark_burst` at 0.3, 0.9 and 1.8 s - its
sparks, then its smoke puffs growing -, shipped `fire/firelp_small_pm` out of `common_mp.ff` - flame, sparks, the
black and the grey smoke above it. Not there: streamed sounds
of the game ( files of its archives ); animations of the game on models of the game ( `asset_preview anim` takes
animations of the project ); a model with more than one LOD shows LOD 0 in the animation. The test machine has no
audio device to listen with: that Windows accepted the wave data is what was seen.

`codol_view`: a mesh drawn with the technique set of a model material ( `mc_..` ) uses packed model vertices now.
With world vertices such a mesh came out in one flat colour - a brush with a model material did too.

**In the compiled map** ( File > Compile map ). The project's zone is passed to `codol_mapc` with `--assets`, as
File > Build map carries it into a map of one's own: every animation, sound alias ( with its loaded sounds and
curves ), effect and model of the project goes into the edited map's zone; the project's `generated\animlists\map.animlist`
is put behind the map's own animlist, its animtrees and the files of `scripts\` become rawfiles. The project's
scripts are also copied to `compiled\<map>.scripts\` - the dedicated server reads level scripts from its script
folder, so a `scripts\maps\mp\<map>.gsc` of the project *replaces* the shipped map's level script there and has
to hold what that one does ( copy it from the server's script folder and add to it ). Streamed sounds need their
archive as for a built map; the console prints the command.

Loaded ( sixth round, `codol_bsp/README.md` ): shipped mp_rust, opened in the editor, with a project of one
material, one model, sound aliases, one effect and a level script; the model placed once as a static model and
once as a `script_model`; File > Compile map. The dedicated server takes the zone and runs the project's level
script; the client enters the map, draws the three crates ( static, entity, spawned by script ), the effect's
puffs above them, and plays the project's loop sound. **Animations** ( seventh round, on the server, with the
command line: `codol_mapc --assets` with the project's animlist appended and its animtree as a rawfile ): the
server reads `animlists/mp_rust.animlist` with the project's two entries, mounts `animtrees/<tree>.atr`, gives the
tree a client id, `getAnimLength` answers and `scriptModelPlayAnim` runs without a script error - the same lines
as for a map built from scratch. Whether they move in the client is the open question it is everywhere.

* A model of the project that an entity uses - a `misc_model` ( static ) or any other class ( `script_model` .. ) -
  is copied into the map zone with its materials and images ( "entity models / static models: xmodel .. taken from
  zone .." in the report ).
* **New world materials are not drawn in the game by Compile map** ( File > Build map draws them: the material, its
  images and technique set go into the built zone ). The incremental compiler draws new faces only with materials
  that the world of the base map already uses; faces with any other material collide but are invisible. The Assets
  window says so next to every such material, and the compile lists the faces - before it runs and with the
  compiler's report - in the console, in a toast and in the Assets window ( "Last compile: not in the game" ).
* Images are stored inline in the zone ( DXT ), which no shipped material texture does; see "Not done, taken on
  trust" in `codol_assets/README.md`.
* The fastfile of an edited shipped map has to keep the length of the shipped file ( the client checks it ): what a
  project adds has to fit into what the better compression leaves. mp_rust with the test project ( 0.3 MB ) fits.

**Animations, sounds, effects.** What each source is and how it is compiled: `codol_assets/README.md`,
"Animations", "Sounds", "Effects".

* An animation needs the model it is for ( the `xanim_export` has no bone hierarchy ): a model of the project by
  its name, or an `.xmodel_export` file. Without one every part is taken as a root - right for a model of one bone.
  Every animation belongs to an animtree, by default one named after the project; the build writes
  `generated\animtrees\<tree>.atr` ( one name per line ) and `generated\animlists\map.animlist`.
* A soundaliases `.csv` is one line of the Assets window with everything it names. Loaded sounds go into the zone,
  streamed files ( `.wav`, `.mp3`: the map's ambient track, long loops ) into `zz_<project>_sounds.iwd`, which the
  install turns into an archive of the game. A file that is not under `sound\` is referenced by name - a sound of
  the game.
* An effect names materials of the project or of the game; effects it emits are read from `fx\<name>.efx` of the
  project.

#### In the built map

File > Build map ( `tools\package_map.ps1 -Assets <project zone> -Project <project folder>` ) puts the project into
the zone of the map, so that a level script can use it:

| from the project | in `compiled\<map>\` |
|---|---|
| the zone's animations, sound alias lists, loaded sounds, curves, effects and models, each with what it points at ( materials, images, technique sets ) | assets of `zone\<map>.ff` ( `codol_assetc --carry`; what the compiler already took stays as it is ) |
| `generated\animtrees\<tree>.atr` | rawfile `animtrees/<tree>.atr` |
| `generated\animlists\map.animlist` | rawfile `animlists/<map>.animlist` ( the compiler's empty one is replaced ) |
| `scripts\<path>` | rawfile `<path>` and `scripts\<path>` |
| `zz_<project>_sounds.iwd` | `main\zz_<project>_sounds.iwd` |

**The rule for scripts:** a file of `mod\<project>\scripts\` goes in front of a generated file of the same path.
`scripts\maps\mp\<map>.gsc` is the level script instead of the generated one, `scripts\animtrees\<tree>.atr`
replaces the generated animtree ( it then has to list the project's animations of that tree; the Assets window says
when it does not ), `scripts\animlists\<map>.animlist` the generated animlist. What the compiler generated is kept in
`compiled\<map>\work\scripts\`. Files that an earlier build put into `scripts\` or `main\` of the package and that
are not made any more are removed; nothing else is deleted. A level script of your own has to make the calls of
the generated one - Assets > **Level script** writes them:

```
#include maps\mp\_utility;
#using_animtree( "mp_mymap" );                                    // the animtree of the project's animations

main()
{
	level._effect[ "sparks" ] = loadfx( "misc/my_sparks" );       // fx\misc\my_sparks.efx
	precacheModel( "my_lidbox" );                                 // a model no entity of the map uses
	maps\mp\mp_mymap_fx::main();                                  // generated: the script_fx / script_sound entities
	maps\mp\_load::main();
	maps\mp\_compass::setupMiniMap( "compass_map_mp_mymap" );
	game[ "attackers" ] = "allies";
	game[ "defenders" ] = "axis";
	level thread show();
}

show()
{
	wait 5;
	box = spawn( "script_model", ( 0, 0, 64 ) );
	box setModel( "my_lidbox" );
	box setAnim( %my_lid_open, 1, 0, 1 );                         // anims\my_lid_open.anim, model my_lidbox
	box playSound( "my_beep" );                                   // an alias of soundaliases\*.csv
	playfx( level._effect[ "sparks" ], box.origin );
}
```

Checked on the dedicated server ( loopback, no client; `mp_edassets2`, a room with one model, two animations, one
alias file and one effect imported and built in the editor, File > Build map, `map_install` into a link based copy
of the install ): the server mounts the map zone, reads `animlists/mp_edassets2.animlist` ( 2 entries ) and
`animtrees/mp_edassets2.atr` from it, runs the project's level script with 0 script errors, `getAnimLength` gives 2
and 1.33 s, `setAnim` binds the script model to the tree, `loadfx` / `playfx` and `playSound` / `playLoopSound` /
`ambientPlay` run; the server's own decoders take the animations and the model. Controls: the same script with an
alias no zone has stops with "unknown sound alias", the same package without the carried assets with "xanim .. is
not in any mounted zone".

**In the game client** ( loopback, the test user; `codol_bsp/README.md`, "Fourth round" ): a packaged map with a
project of one effect, two animated models and three sound aliases loads; the **effect** placed with a `script_fx`
entity is drawn, its smoke puffs coming again and again; the two **models** stand where the level script spawns
them but their **animations do not move** - on this test server no script model animation reaches the client, the
hanging clothes of shipped mp_crash_tropical stand still as well ( `codol_bsp/README.md`, "Script model
animations": what the map side provides is there, the rest is the server's ); **sounds**: a project's loaded sounds
play - measured at the client's audio session, with their distance falloff ( "Fifth round" ) -, and so do its
streamed ones ( sixth round: the ambient track of a map from a `.wav` and from an `.mp3`, an `.mp3` on an entity
with its distance falloff - measured the same way, not listened to ). The effects a map places are made by the client
from the text of `maps\mp\<map>_fx.gsc` and `maps\createfx\<map>_fx.gsc`: leave those two to the build, or keep
their exact form.

Not there: deleting or renaming an asset from the editor ( delete the files, Rescan ), editing a material's
definition in a window ( edit the `.mtl`, Build ), a preview inside the import windows ( import, then click the
line ), streamed images ( `.iwi` in an `.iwd` ), per image options ( format, `flipy` ), the less
common animation options ( `deltapart`, `assettype`, tolerances: `codol_assetc --xanim` has them ), copying the
effects an effect emits on import ( put them into `fx\` by hand ). The shipped animtrees ( `animated_props` .. ) are
not extended: give your animations a tree of their own. An edited shipped map has no install step: its fastfile,
`compiled\<map>.scripts\` and the sound archive are copied by hand.
## What works

This table and the sections below it are about the **radiant plugin** ( `iw3r.dll` inside CoD4 radiant ), the first
form of this work; what the standalone editor does is above.

| Area | Status |
|---|---|
| Script editing | Browse and edit the game's script tree (`.gsc`, animtrees, `mp/playeranim.script`, string tables, ...) inside radiant, with syntax highlighting, find / go to line, GBK and CRLF preserved on save. |
| Player animations | Structured view of `mp/playeranim.script` (defines, states, movement types, conditions, events) with jump-to-line, checked against `animtrees/multiplayer.atr`. |
| Scripted animations | Per-script tables of `level.scr_anim[..]` / `scr_model` / `scr_animtree` entries and `anim_*` calls, the governing `#using_animtree` for the cursor line, and an animtree browser. |
| Validation | A `%anim` that is not in its animtree is an error (the server would append it and shift every later index: T-pose / wrong clips). Optional: animations the game does not ship are warnings. |
| Map import | One click turns a shipped map into an editable radiant map: every collision brush with the material of each side, brush entities with their real brushes, every static model as a `misc_model`, and all entities. `mp_killhouse_sh`: 27,032 world brushes, 640 brushes in 280 brush entities, 16,335 static models, 1,967 entities. |
| Native viewport | **CoDOL > Viewport ( native )** draws a map the way the game does: render world, static models, entity models and brush models read straight from the 1.1.1.13 fastfiles and drawn with the game's own materials, technique sets and compiled shaders (`codol_zone`, `codol_render`, `codol_view`). Lightmaps, reflection probes, light-grid model lighting, sun shadows, cell/portal culling. Frames are bit-identical to CODOL-AssetExplorer's renderer on 8 of 9 test views. Console: `codol_viewport <map>`. |
| Map entities | Edited entities are exported as a server-side override (`<scripts>/maps/mp/<map>.d3dbsp.ents`). Unedited entities export byte-identical to the zone. |
| Hot reload | rcon to the local dedicated server: reload the level when a script is saved, load a map after an entity export. Optional pre-check command. |
| Game window | The window of the running game client can be docked inside radiant (CoDOL > Game Window) and handed back. |
| Asset layouts | `src/codol/c1_layouts.hpp`: the 1.1.1.13 zone layouts (XModel, XSurface, Material, techset, GfxImage, XAnimParts, clipMap, GfxWorld, MapEnts, ...) as C++ structs, every size and named offset checked at compile time. |

## What does not exist yet

Be aware of these before planning work around this branch:

- **Maps of your own: lighting is simpler than the game's own maps.** Surfaces are cut by the reach of the
  primary lights along a grid of 32 units, not as finely as the game's compiler does, and a surface is lit at run
  time by one primary light. Bounced light has the mean colour of a material, not its pattern. An omni light
  shadows players and models only inside its cone ( so do the game's own - it is what the shaders read ). A light a
  script moves keeps the shadows of where it was compiled and lights at run time only what no other light claims.
  See "Limits" in `codol_bsp/README.md`.
- **Moving a light by script: `.origin` only, on the test server.** The game's own scripts also use `moveTo` and
  `linkTo` on lights; `codol_dedrun` answers both with a script error for the classes `light_spot` / `light_omni`.
- **Static models have no player collision of their own.** As in the shipped maps, bullets hit a `misc_model`,
  players do not: put clip brushes around models.
- **Nobody has seen an animation of your own move in the game.** The editor plays the compiled animation ( the
  Preview window reads it back from the project's zone ), the Assets window builds it and File > Build map puts it
  into the map with its animtree, animlist and your level script; a dedicated server runs a level script that uses
  it without a script error - for a map built from scratch and for an edited shipped map -, and the client loads
  such a map and draws its effect ( [In the built map](#in-the-built-map) ). But script model animations do not
  move in the client on the test server - shipped ones do not either; what a map has to bring is there
  ( `codol_bsp/README.md`, "Script model animations" ), the rest is the server's.
- **Sounds were measured, not heard.** Loaded and streamed sounds of your own play in the client - measured at
  its audio session on a machine without anybody listening; the editor's Preview window plays the loaded ones.
- **Nothing was played over a network.** Every run in the game was one client, one dedicated server and the lobby
  emulator on one machine, with a test account of level 1.
- **Editing a shipped map** ( File > Compile map ) cannot draw new world materials, does not remove render surfaces
  of deleted brushes and does not re-bake light. For that, build the map from scratch. It has no install step
  either: its fastfile ( which replaces the shipped one and has to keep its length ), `compiled\<map>.scripts\`
  and the sound archive are copied by hand.
- **Imported maps hold collision brushes only.** Brush sides get the right material but a default projection;
  terrain, patches and decals of a shipped map exist only as render surfaces and are not in the imported `.map`.
- **The create-room list: written, loaded by the client, not seen.** File > Install the built map .. adds the map to
  the list zone of a game folder and takes it out again ( [A map of your own](#a-map-of-your-own), step 5 ). The
  client runs with the zone; its Create Room screen is locked below player level 5 and the test account is level 1,
  so the row was not looked at and a map was not started from it. `CODOL_FORCE_MAP=<map>` on the lobby plays a map
  without the list.
- **The 3D view of a map of your own is not the light bake.** It shows the sun, sky and ambient light of the
  worldspawn on brushes and models; light entities are shown as shapes, their light is in the built map.
- **Assets**: no deleting or renaming from the editor, no editing of a material in a window, no streamed images,
  the shipped animtrees are not extended ( the list under [In the built map](#in-the-built-map) ).
- **Entity overrides are mp only.** The server applies `<map>.d3dbsp.ents` in mp matches; co-op and
  zombies compose their own entities.
- The sections below this one describe the radiant plugin ( `iw3r.dll` in CoD4 radiant ), which the standalone
  editor replaces; the Script Workbench and the game bridge are the same code in both.

## Setup

Radiant needs a CoD4 mod tools root (the folder that contains `bin/`, `raw/`, `main/`, `map_source/`).
The DLL hooks `bin/IW3xRadiant.exe` (a patched `CoD4Radiant.exe`, in `assets/bin`).

1. Build `iw3r.dll` (see [build](#code-layout-build-and-tests)) and copy it with the PhysX dlls into
   `<root>/bin`. `cmake -DRADIANT_ROOT=<root>` does that after every build.
2. Copy `assets/bin` and `assets/map_source` into the root (`map_source/prefabs` has to exist,
   radiant aborts at startup without it).
3. Start `bin/IW3xRadiant.exe`, open **CoDOL > Script Workbench > Setup** and fill in:

| Setting | dvar | Meaning |
|---|---|---|
| Script folder | `codol_script_root` | The tree you edit: the folder the server loads loose scripts from. |
| Reference script folder | `codol_script_ref_root` | Optional second tree. Files of the script folder shadow it, animtrees of both are merged. |
| Game xanim list | `codol_xanim_list` | Optional text file, one animation name per line (first word of each line). |
| CoDOL install folder | `codol_install_path` | Contains `main\` and `client_shipRetail\`. Used to list map zones and to find the rcon password file. |
| Start command | `codol_launch_cmd` | Script or exe that starts the client (Launch button of the Game window). |
| Client process names | `codol_client_exe` | Used to find the game window. Default `codoMP_client_shipRetail.exe;C1.exe`. |
| Port | `codol_rcon_port` | UDP port of the local server. Default 17733. |
| Password file | `codol_rcon_password_file` | File with the rcon password. If empty: `<install>\main\k33_rcon.txt`, `<install>\p2p\state\native_server_rcon.txt`, `<install>\CODOL\1.1.1.13_server\alpha0022\native_server_rcon.txt`, `%CODOL_STATE_DIR%\native_server_rcon.txt`. |
| Pre-reload check | `codol_reload_precheck_cmd` / `_cwd` | Optional command that has to exit with 0 before the game is reloaded. |

Settings are saved in `<root>/bin/IW3xRadiant/dvars.cfg`. Without a script folder the workbench
falls back to `%CODOL_SCRIPTS%`, then to `<root>/raw`.

## Script Workbench

**CoDOL > Script Workbench** (console: `codol_workbench`).

- **Files**: the script tree with a path filter. Dot-folders (`.git`) are skipped.
- **Editor**: tabs, Ctrl+S, find (Enter / F3 / Shift+F3), go to line. A dot in the tab marks unsaved
  changes; a file changed outside of radiant is reloaded, or offered for reload if it has unsaved
  edits. Files that are not valid UTF-8 are treated as GBK (the game's text encoding), edited as
  UTF-8 and written back as GBK. Line endings are kept as they were.
- **Problems in the gutter**: a dot next to the line number, hover for the message.
- **Status bar**: line / column, language, encoding, line endings and the animtree that governs
  the cursor line (the last `#using_animtree` above it).

### Animations tab

- **Player**: `mp/playeranim.script` as a tree. Sections (`ANIMATIONS`, `CANNED_ANIMATIONS`,
  `STATECHANGES`, `EVENTS`), states, movement types, condition blocks and their
  `both / legs / torso / turret <anim>` commands. Click to jump. With a filter it becomes a flat
  list of matching commands; the tooltip shows the full path of the block.
- **Scripted**: for the selected script (it follows the active tab) the scene tables
  (`level.scr_anim[ who ][ scene ][ index ] = value`, also `scr_model`, `scr_animtree`, `scr_sound`,
  ...) and the animation calls with their string arguments.
- **Animtrees**: every `animtrees/*.atr` as a tree, flags (`additive`, `complete`, `loopsync`,
  `nonloopsync`) included.

### Problems tab

| Severity | Finding |
|---|---|
| error | `%anim` is not part of the animtree named by the governing `#using_animtree` |
| error | `%anim` is used before any `#using_animtree` |
| error | syntax errors in an animtree or in `playeranim.script` (unbalanced braces, ...) |
| warning | the animtree named by `#using_animtree` is not in the script roots |
| warning | a player animation is not part of `animtrees/multiplayer.atr` |
| warning | duplicate animation in an animtree (stock trees have these) |
| warning | an animation is used but is not in the game xanim list (only with `codol_xanim_list`) |

The index is rebuilt in the background with **Rescan** and updated for a file when it is saved.

## Map Entities

**CoDOL > Map Entities** (console: `codol_mapents`). This is the way to work on a CoDOL map from inside
CoD4 radiant ( the plugin ); the standalone editor opens a shipped map itself and has File > Compile map.

1. **Import** a map: pick a zone of the install or an override that already exists in the script
   folder, or give a file (`.ff`, a decrypted `.zone`, a `.d3dbsp.ents`, plain entity text).
   The map is written to
   `<root>/map_source/codol/<map>.map` and opened.
   - A `.ff` is read natively: `bin/codol_mapconv.exe` loads the fastfile with the zone reader
     (`codol_zone/`, key taken from the client exe in the install folder) and writes the map: all
     collision brushes with the material of each side, brush entities with the brushes of their sub
     model, static models as `misc_model`s marked `codol_static`, and the entities. Brushes that only
     have tool materials (clip, hint, mantle, ...) are on the layer `codol_tools`.
   - A `.d3dbsp.ents`, a decrypted `.zone` or entity text gives the entities only, as points.
2. **Edit** in radiant.
3. **Save + export override**: writes `<script folder>/maps/mp/<map>.d3dbsp.ents` and, if enabled,
   sends `map <map>` to the local server. A file that was already there is kept once as `.bak`.

How entities are represented:

- Point entities are what they are in the game.
- With geometry, an entity with a brush model (`"model" "*N"`) is a real brush entity with the
  brushes of that model. Its brushes are for looking at: the exported entity still names the
  compiled model.
- A trigger hull (`"model" "?N"`), and any brush model without geometry, becomes a `codol_bmodel`
  point entity; its real class is kept in `codol_classname`, the hull in `codol_model`.
  It can be moved (hull bounds are relative to the origin), re-keyed, deleted or copied. New hulls
  cannot be made. A `?N` that the zone does not have is skipped on export with a warning, because
  the server rejects the whole override otherwise.
- The worldspawn carries `codol_map`, `codol_base_crc32` (CRC-32 of the zone's entity string; the
  server only uses an override made for the zone it has mounted) and `codol_trigger_models`.
- New brush entities drawn in radiant, `misc_model` and `misc_prefab` are not exported (a warning
  says so); the static models of the import are skipped silently.

Radiant rewrites entities when it saves, even untouched ones: origins are rounded to one decimal,
classes it knows from CoD4 get their `model` replaced, point entities gain an `origin`. So right
after the import the map is saved once as a baseline (`<map>.codol_base.map`, next to
`<map>.codol_orig.ents`), and the export compares against it: a value radiant changed on its own
is exported the way the zone had it, only real edits are exported as edited. An unedited map
exports byte-identical to the zone's entity string.

## Game window and hot reload

- **Reload in game** (workbench toolbar, CoDOL menu, `codol_reload`) reloads the level on the local
  server. Modes (`codol_reload_mode`): `fast_restart`, `map_restart`, or `map <current map>`, which
  is the default because it recompiles scripts from disk. With **on save** it happens whenever a
  script is saved.
- **Pre-reload check**: a script the server cannot compile makes the map load fail. If a check
  command is set and exits non-zero, its output goes to the console and the game is not reloaded.
- **CoDOL > Game Window**: *Attach* re-parents the window of the running client into the dock and
  keeps it sized to it; *Detach* or closing the dock restores its style and position. *Launch*
  runs the start command. Radiant menus cannot draw over the embedded window, and if radiant is
  killed while the game is attached the game window goes with it.

The rcon packet is `\xff\xff\xff\xffrcon "<password>" <command>` to `127.0.0.1:<port>`.

## Console commands and dvars

| Command | |
|---|---|
| `codol_workbench`, `codol_mapents`, `codol_gameview` | toggle the windows |
| `codol_open <file> [line]` | open a file in the workbench |
| `codol_rcon <command>` | send an rcon command, print the answer |
| `codol_reload` | reload the level (see above) |
| `codol_map <map>` | `map <map>` on the server |
| `codol_import_ents <file> [map]` | import map entities |
| `codol_export_ents [noreload]` | save + export the override |
| `codol_attach`, `codol_detach` | dock / release the game window |

Other dvars: `codol_reload_on_save`, `codol_rcon_log`, `codol_rcon_password` (not saved).

For unattended testing: `CODOL_STARTUP_CMDS="cmd;wait 5;cmd"` runs console commands once the UI is
up ( words split on spaces, `"quoted words"` kept together: `ed_key angles "60 200 0"`; up to 32 k characters ),
`CODOL_AUTOSHOT=<file.png>` saves the rendered UI frame to that file every two seconds ( the standalone
editor also has `ed_shot <file.png>`: the next frame, at a moment the command list picks ), and
`CODOL_NO_MONO_FONT=1` skips loading the monospace editor font.

## Code layout, build and tests

| Path | |
|---|---|
| `src/codol/` | Engine independent code: script lexer, animation index, map entity conversion, 1.1.1.13 layouts. No radiant or imgui dependency. |
| `src/components/modules/codol_game.*` | rcon, reload, game window, settings |
| `src/ggui/script_workbench.*`, `src/ggui/codol_map_ents.*` | the windows |
| `codol_zone/` | Native 1.1.1.13 fastfile reader: container, every asset type, typed views. Own CMake project, tool `codol_zonetool`, verified against all 146 shipped zones. |
| `tools/codol_mapconv` | fastfile -> radiant `.map` through `codol_zone` |
| `tests/` | unit tests for `src/codol` |

```
cmake -S . -B build-cmake -G "Visual Studio 17 2022" -A Win32 [-DRADIANT_ROOT=<mod tools root>]
cmake --build build-cmake --config Release
build-cmake\Release\codol_tests.exe [<script tree>] [<map>.d3dbsp.ents] [<decrypted>.zone]
```

The upstream premake build files are untouched (`generate-buildfiles_vs22.bat`) but were not re-run on this branch. The tests take optional
real inputs: a script tree is indexed and summarised, an `.ents` file is round-tripped through a
radiant map, a `.zone` has its `MapEnts` header validated against its entity string.

## Roadmap

What the chain above still lacks, in the order that would help a mapper most:

1. **Animations that move in the client.** The map side is complete; the dedicated server has to send script
   model animations so that the client plays them ( `codol_bsp/README.md`, "Script model animations" ).
2. **The create-room list seen.** Needs an account of level 5; the editor's part is written and loaded.
3. **Light closer to the game's compiler**: surfaces cut along the pieces of a BSP instead of a grid, an omni
   light's cone aimed where the players are, baked shadows of see through brushes, brush models lit where they
   stand.
4. **Player collision for static models** ( clip brushes made by the compiler ).
5. **Editing shipped maps**: new world materials drawn, render surfaces of deleted brushes removed, an install
   step; brush texture alignment, terrain, patches and decals of a shipped map in the imported `.map`.
6. **Assets**: delete / rename, a material editor, streamed images.
