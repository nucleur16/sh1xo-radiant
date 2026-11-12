# iw3xo-radiant for Call of Duty Online 1.1.1.13

This branch (`codol-1.1.1.13`) turns iw3xo-radiant into a workbench for Call of Duty Online
1.1.1.13 ("C1", an IW4-derived engine, fastfile version 334). It keeps everything upstream does
and adds a **CoDOL** menu.

- [What works](#what-works)
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
| Radiant | `CoDOLRadiant.exe` | open a map of the install, edit entities / models / brushes in 3D and 2D views with undo, save as `.map` |
| `cod4map`, `cod4rad`, the map's linker step | `codol_bsp.exe` ( File > Build map ) | builds the whole zone of a map of your own from its `.map` alone: collision ( brushes, terrain patches, static models ), BSP, cells and portals, render geometry, light bake ( sun, lights, bounce, light grid, reflection probes ), minimap picture, load zone, level scripts. See [A map of your own](#a-map-of-your-own). |
| editing a shipped map | `codol_mapc.exe` ( File > Compile map ) | applies the edits of a `.map` to the fastfile of the map it was imported from: entities, static models moved / deleted / added, collision brushes, render faces of new world brushes. It patches the shipped zone; no light bake. |
| Asset Manager, `converter` | the Assets window, `codol_assetc.exe` | your own textures, materials and models: PNG / TGA / DDS, `.mtl` or the material entries of a CoD4 `.gdt`, `xmodel_export` -> native assets in `mod\<map>\<map>_assets.ff`, shown by the editor at once. See [Your own assets](#your-own-assets). `codol_assetc` alone also compiles animations ( `xanim_export` ), sounds ( soundalias `.csv`, `.wav` ) and effects ( `.efx` ). |
| `linker_pc` | `codol_linker.exe` | reads and writes fastfiles ( all 153 of the install round trip byte for byte ), replaces entities / rawfiles / string tables, takes assets from other zones, builds a zone of rawfiles |
| script editing | Script Workbench, rcon reload | unchanged |

Compiled fastfiles are unsigned ( `IWffu100` ); the signed form needs a key the client does not have. Per the notes
in the install, the retail client only loads unsigned zones from disk with the `Kernel33.dll` patch; the dedicated
server reads both. Maps built from scratch and edited shipped maps have both been loaded on the dedicated server and
in the client; `codol_bsp/README.md` and `codol_mapc/README.md` list what was checked where and what is still open.

### A map of your own

1. **File > New map**, a name like `mp_mymap`. It is saved as `map_source\mp_mymap.map`.
2. Draw the map: brushes, terrain patches, `misc_model`s, lights, `reflection_probe`s, spawn points
   ( `mp_dm_spawn`, `mp_tdm_spawn_*` .. ), one `mp_global_intermission`, `node_pathnode`s for bots. The map has to be
   closed ( no way from an entity to the void ) or stand in a sky box; the build reports a leak with its position.
3. **Edit > Add gameplay entities** adds what the game's scripts ask of every multiplayer map and the map does not
   have yet: the care package with its collision ( without it `_airdrop.gsc` throws a script error at every map
   start ) and a helicopter path ( `heli_start`, `heli_loop_start` .. ). The build names what is missing.
4. **File > Build map** ( or "fast" while you work on the layout ). The result is a package in `compiled\<map>\`:

   | | |
   |---|---|
   | `zone\<map>.ff`, `zone\<map>_load.ff` | the map zone ( with the minimap material `compass_map_<map>` ) and its load zone; a picture `map_source\<map>_load.png` becomes the loading screen |
   | `scripts\maps\mp\<map>.gsc` ( + `_fx`, `createfx` ) | the level script for the dedicated server's script folder |
   | `presence\` | the row, arena entry and name for the create-room list, and `INSTALL.txt`: where every file goes |
   | `work\<map>.report.txt` | what the compiler did: counts, timings, warnings |

   Materials and models of the map's asset project ( next section ) are taken into the zone.

5. **File > Install the built map into the game** copies the zones to `<install>\usermaps\<map>\` and the level
   scripts to the server's script folder ( File > Settings ). Start the lobby with `CODOL_FORCE_MAP=<map>` to play it;
   `presence\INSTALL.txt` describes the create-room list.

Console: `map_new <name>`, `ed_gameplay`, `map_build [fast]`, `map_install [zone folder] [script folder]`.
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
mod\<project>\<project>_assets.ff                 the zone built from all of it
mod\<project>\build_state.txt                     what the last build made of each source
mod\codol_matlib.txt                             the material library: one scan of the install ( 20 s, once )
```

**View > Assets** lists every source with its status - new, changed, built, failed with the compiler's message,
unused ( an image no material names ) - and has **Build**. A build compiles everything again on a worker thread
( `codol_assets`: `compile_image`, `compile_material`, `compile_model`, `AssetLinker` ) into a new zone; the editor
stays usable. The renderer then takes the zone without loading the map again ( `map_renderer::register_zone` ), and
every mesh and placed model is made again from it.

| what | where | command |
|---|---|---|
| material from images | Materials > **New material ..**, Assets window, or drop image files on the editor window ( `_n` / `_nml` / `_normal` and `_s` / `_spc` / `_spec` at the end of a name fill the normal and specular map ) | `asset_new_material <name> <kind> <colour map> [normal=<file>] [specular=<file>] [surface=<type>] [overwrite]` |
| model | Models > **Import model ..**, or drop `.xmodel_export` files | `asset_import_model <name> <lod0>[@dist] [<lod1>[@dist] ..] [collision=<lod>] [mtl:<export material>=<material>] [overwrite]` |
| materials of a CoD4 `.gdt` | Materials > **GDT ..**, or drop a `.gdt` | `asset_gdt <file> [root=<folder the image paths start from>] [overwrite] [all \| <material> ..]` ( without a choice: the list ) |
| build, list | Assets window | `asset_build`, `asset_list`, `asset_rescan`, `asset_status`, `asset_project [name]` |
| check the zone | | `asset_verify` ( `codol_assetc --verify`: the game's loaders read the zone, it is written again and compared ) |
| the windows, for unattended runs | | `asset_dialog material \| model \| gdt [file] \| close`, `asset_drop <file> ..` ( what dropping the files does ), `ed_windows a` |

Material kinds ( the technique set follows from the kind and the maps given, as `codol_assets` `techset_for` has it ):
`world`, `world_alpha` ( alpha tested ), `world_blend`, `decal`, `world_unlit` become `wc/<name>`; `model`,
`model_alpha`, `model_blend`, `model_unlit` become `mc/<name>`; `2d`. The material is the material of new brushes
right away and is applied like any other ( browser, `tex_set` ). A model is placed from the Models browser or with
`ed_place`; `ed_model_class misc_model` makes new model entities static models of the compiled map.

**In the compiled map** ( File > Compile map ). The project's zone is passed to `codol_mapc` as a `--donor`:

* A model of the project that an entity uses - a `misc_model` ( static ) or any other class ( `script_model` .. ) -
  is copied into the map zone with its materials and images ( "entity models / static models: xmodel .. taken from
  zone .." in the report ).
* **New world materials are not drawn in the game by Compile map** ( File > Build map draws them: the material, its
  images and technique set go into the built zone ). The incremental compiler draws new faces only with materials
  that the world of the base map already uses; faces with any other material collide but are invisible. The Assets
  window says so next to every such material, and the compile lists the faces - before it runs and with the
  compiler's report - in the console, in a toast and in the Assets window ( "Last compile: not in the game" ).
* Images are stored inline in the zone ( DXT ), which no shipped material texture does; see "Not done, taken on
  trust" in `codol_assets/README.md`. Nothing of this has been loaded by the game client or a server from the editor.

Animations, sounds and effects are not part of the project folder yet: compile them with `codol_assetc --xanim` /
`--soundalias` / `--fx` ( `codol_assets/README.md` ) into the map zone after File > Build map.

Not there: deleting or renaming an asset from the editor ( delete the files, Rescan ), editing a material's
definition in a window ( edit the `.mtl`, Build ), a 3D preview in the import windows, streamed images ( `.iwi` in an
`.iwd` ), per image options ( format, `flipy` ).
## What works

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

- **Maps of your own: lighting is simpler than the game's own maps.** `light` entities are baked, they are not
  dynamic primary lights; dynamic sun shadows are untested; no T-junction repair. See "Limits" in
  `codol_bsp/README.md`.
- **Static models have no player collision of their own.** As in the shipped maps, bullets hit a `misc_model`,
  players do not: put clip brushes around models.
- **Animations, sounds and effects of your own are command line only, and nobody has seen or heard one in the
  game.** `codol_assetc` compiles `xanim_export` ( `--xanim` ), soundalias `.csv` + `.wav` ( `--soundalias` ) and
  `.efx` ( `--fx` ) into a zone - into the built map zone with `--base compiled\<map>\zone\<map>.ff` - and the
  dedicated server runs a level script that uses them ( `codol_assets/README.md`: "Animations", "Sounds",
  "Effects", "On a dedicated server" ). The Assets window does not list or build them yet, and an animation also
  needs two rawfiles in the map zone ( `animtrees/<tree>.atr`, `animlists/<map>.animlist` ).
- **Editing a shipped map** ( File > Compile map ) cannot draw new world materials, does not remove render surfaces
  of deleted brushes and does not re-bake light. For that, build the map from scratch.
- **Imported maps hold collision brushes only.** Brush sides get the right material but a default projection;
  terrain, patches and decals of a shipped map exist only as render surfaces and are not in the imported `.map`.
- **The create-room list is not installed for you.** The build writes the files; `presence\INSTALL.txt` says which
  script of the install takes them in. `CODOL_FORCE_MAP=<map>` on the lobby plays a map without the list.
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

**CoDOL > Map Entities** (console: `codol_mapents`). This is the way to work on a CoDOL map from
radiant today.

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
up, `CODOL_AUTOSHOT=<file.png>` saves the rendered UI frame to that file every two seconds ( the standalone
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

In the order that unlocks the most:

1. **Brush texture alignment.** The UVs of brush sides can be recovered from the coincident
   render surface (in its Cast export); read them and write the texture projection of each side.
2. **Non-brush geometry.** Terrain, patches and decals from the render world as reference models.
3. **Map compile.** Get an IW3 to IW4 map converter (iw3x-port) and wire the chain: `cod4map` /
   `cod4rad`, `linker_pc`, IW3 to IW4, IW4x zonebuilder, AssetExplorer `iw4-port`, install as a
   usermap, `map <name>` over rcon. This is the step that lets edited geometry reach the game.
4. **Zone reader in C++** on top of `c1_layouts.hpp`, to drop the dependency on the external asset
   tool.
