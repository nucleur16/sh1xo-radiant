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
| Radiant | `CoDOLRadiant.exe` | open a map of the install or one of your own, edit entities / models / brushes in 3D and 2D views with undo, light entities with a panel of their own, save as `.map` |
| `cod4map`, `cod4rad`, the map's linker step | `codol_bsp.exe` ( File > Build map ) | builds the whole zone of a map of your own from its `.map` alone: collision ( brushes, terrain patches, static models ), BSP, cells and portals, render geometry, light bake ( sun, lights, bounce, light grid, reflection probes ), minimap picture, load zone, level scripts. See [A map of your own](#a-map-of-your-own). |
| editing a shipped map | `codol_mapc.exe` ( File > Compile map ) | applies the edits of a `.map` to the fastfile of the map it was imported from: entities, static models moved / deleted / added, collision brushes, render faces of new world brushes. It patches the shipped zone; no light bake. |
| Asset Manager, `converter` | the Assets window, `codol_assetc.exe` | your own textures, materials, models, animations, sounds and effects: PNG / TGA / DDS, `.mtl` or the material entries of a CoD4 `.gdt`, `xmodel_export`, `xanim_export`, soundalias `.csv` + `.wav`, `.efx` -> native assets in `mod\<map>\<map>_assets.ff`. Materials and models are shown by the editor at once; animations, sounds and effects are for the level script of the built map. See [Your own assets](#your-own-assets). |
| the zone source `.csv` of a map, `linker_pc` on it | `tools\package_map.ps1` ( part of File > Build map ) | puts the project's assets, animtrees, animlist and scripts into the map zone ( `codol_assetc --carry`, `--rawfile` ), adds the minimap material, builds the load zone and writes the package |
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

   **The 3D view.** A map of your own has no zone of the game to be drawn in, so the editor draws it in a *stage*:
   an empty world that `codol_bsp` compiles from the light keys of the map's worldspawn ( a second or two, kept in
   `compiled\cache\stage\` per set of keys ). Brushes are drawn with their materials, models where their entities
   stand, lit by the sun, sky and ambient light the map will have. It is not the light bake: lights and shadows of
   brushes are what File > Build map makes.

   **Light.** A `light` is created with `radius 600`, `intensity 1`, `_color 1 1 1`; it reaches as far as its
   radius and is down to a quarter at half of it. `spawnflags 2` ( `PRIMARY_SPOT`, with `fov_outer` / `fov_inner` /
   `exponent` and `angles` or a `target` ) or `1` ( `PRIMARY_OMNI` ) makes it a primary light: the game lights the
   world and the models with it at run time instead of from the lightmap. The worldspawn takes `sundirection`,
   `suncolor`, `sunlight`, `ambient` with `_color`, `skylight` with `skycolor`; without them a map has a sun of
   1.3 from `-50 40 0`, an ambient light of 0.1 and a sky light of 0.4. All keys with their defaults:
   `codol_bsp/README.md`, "Light keys".

   Nobody has to type these keys. With a light selected the **Entity** window has a **Light** section: colour
   ( a swatch that opens a picker ), intensity, radius, the kind ( baked / primary omni / primary spot ), "no shadow
   map" ( `spawnflags 8` ), "scriptable" ( `spawnflags 4`, with `maxturn` / `maxmove` ), the falloff ( `def` ), and
   for a spot the outer and inner angle, the exponent and where it points - pitch and yaw, or a target picked from
   the targetnames of the map. With nothing selected the window shows **Light of the map**: sun, sun colour and
   direction ( with a small compass: from above, from the side ), sky light and ambient light of the worldspawn; a
   field whose key is not set shows the compiler's default and says so, the `x` behind a field takes the key away
   again. Fields that are dragged change the map while the mouse is down and are one undo step. "Show in 3D"
   appears when the light keys differ from those the stage was built with, and builds the stage again ( console:
   `ed_restage`; `ed_key <key> <value>` and `ed_delkey <key>` go to the worldspawn when no entity is selected ).

   While a light is selected the 3D view and the 2D views draw **where it reaches**: three circles of its radius
   ( and one at half of it ) for a light that shines all around, the cone of a spot - outer and inner angle, cut
   off at the radius, with its axis - in the colour of the light. The cone is computed the way `codol_bsp` reads
   the keys ( a target wins over the angles and gives a cone of 64 units around it unless `fov_outer` is set ), so
   what is drawn is what the bake uses. The classes `light_spot` and `light_omni` ( the primary lights of the
   shipped maps ) get the same section.

   "scriptable" with `maxturn` / `maxmove`, the classes `light_spot` / `light_omni` and falloff definitions other
   than `light_point_linear` are read by the compiler's light work that was written alongside this panel
   ( `codol_bsp/README.md`, "Light keys", says what the compiler you have reads; it prints `light keys: ..` with
   every build ). A compiler that does not know one of them ignores the key - the panel still writes it.
3. **Edit > Add gameplay entities** adds what the game's scripts ask of every multiplayer map and the map does not
   have yet: the care package with its collision ( without it `_airdrop.gsc` throws a script error at every map
   start ) and a helicopter path ( `heli_start`, `heli_loop_start` .. ). The build names what is missing.
4. **File > Build map** ( or "fast" while you work on the layout ). The result is a package in `compiled\<map>\`:

   | | |
   |---|---|
   | `zone\<map>.ff`, `zone\<map>_load.ff` | the map zone ( with the minimap material `compass_map_<map>` ) and its load zone; a picture `map_source\<map>_load.png` becomes the loading screen |
   | `scripts\maps\mp\<map>.gsc` ( + `_fx`, `createfx` ) | the level script for the dedicated server's script folder: generated, or your own ( below ) |
   | `main\zz_<project>_sounds.iwd` | only when the project has streamed sounds: their files, for `<install>\main\` |
   | `presence\` | the row, arena entry and name for the create-room list, and `INSTALL.txt`: where every file goes |
   | `work\<map>.report.txt`, `work\scripts\` | what the compiler did: counts, timings, warnings; the scripts as the compiler generated them |

   From the map's asset project ( next section ) the zone gets: the materials and models the map uses ( the
   compiler takes them ), and after the compile every animation, sound alias ( with its loaded sounds and curves ),
   effect and model of the project's zone, the animtrees and the animlist of the animations, and the files of the
   project's `scripts\` folder as rawfiles. See [In the built map](#in-the-built-map).

5. **File > Install the built map ..** asks where to: the game folder ( the install of File > Settings is filled
   in ), the server's script folder, and whether the map goes into the create-room list.

   | from `compiled\<map>\` | to |
   |---|---|
   | `zone\*.ff` | `<game>\usermaps\<map>\` |
   | `main\zz_*_sounds.iwd` | `<game>\main\` |
   | `scripts\..` | the script folder |
   | `presence\comaplist_row.csv`, `basemaps_arena_entry.txt`, `localize.txt` | the create-room list of `<game>`, with "Add to the create-room list" |

   * A file that is in the way and differs is kept once as `<file>.pre-<map>` before it is replaced; a file that
     is the same already is left alone - installing twice changes nothing ( "unchanged" in the console ).
   * Everything written is noted in `compiled\<map>\work\installed.txt`. **Uninstall** ( the same window, or
     `map_uninstall` ) undoes exactly that: files that were new are deleted, replaced files get their backup back,
     the map comes out of the list, `usermaps\<map>\` goes when it is empty.
   * **The create-room list** is three things ( the install's `usermaps\build_usermaps_patch.py` describes how the
     client reads them ): a row of the string table `mp/comaplist.csv` ( the map pool ), an entry of the rawfile
     `mp/basemaps.arena` ( the lobby scene ) and a localize entry for the name. The client keeps the first asset of
     a name it links and links the zones of `client_shipRetail\patch\` first, so the list is whichever zone of that
     folder holds `mp/comaplist.csv` - in the install of this machine `codol_common.ff`. The editor reads that zone
     with `codol_zone`, adds the row ( NUMBER and SEQUENCE: the next free ones ), the arena entry and the name,
     writes the zone again, reads it back and only then puts it in place. Without such a zone it makes
     `usermaps_patch.ff` from the stock list of `client_shipRetail\chinese\( patch_ )code_post_gfx_mp.ff`. A row
     for the map that the editor did not write is left alone. On uninstall the entries are taken out of the list as
     it is then; when that leaves what the backup held, the backup itself goes back and the file is byte for byte
     what it was.
   * `map_list [game folder]` prints which zone is the list, its last rows and how many arena entries it has.
   * Without the list a map is played by starting the lobby with `CODOL_FORCE_MAP=<map>`.

   What was checked, and what was not: on copies of the list zones under `build-editor\installtest\` - the list
   in `codol_common.ff` ( 96 -> 97 rows, 112 -> 113 arena entries, the name ), a list made from the stock zones
   ( 60 -> 61 rows ), a folder without either ( refused, with what is missing ); a second install wrote nothing;
   uninstall gave back files with the hashes they had, `codol_common.ff` included. **Not checked: the client or the
   lobby with a list written this way** - the tests did not start them. And one thing the editor cannot know: a
   lobby whose own build makes `codol_common.ff` again ( its sources are not part of the install ) drops the row
   with it; the lasting place for a map is then that build's map table, for which `presence\INSTALL.txt` and the
   three files of `presence\` are the input.

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
made again from it. A click on a line shows the asset in the **Preview** window ( below ); effects are built and
listed but not shown.

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
| look at it, listen to it | a click in the Assets, Models or Materials window | `asset_preview model <xmodel> \| material <name> \| image <file> \| anim <name> [model] \| sounds <csv> \| frame <n> \| play \| pause \| orbit <yaw> <pitch> [zoom] \| status \| close`, `asset_play <alias> [zone \| file.ff]`, `asset_play stop` |
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

Checked ( pictures in `build-editor\shots\` of the test run ): a shipped model and a project model, a shipped
world material and a project model material on the cube, a PNG, the two-bone test animation at frames 0, 20 and 30
( the lid moves ), the csv's rows; `asset_play` of a project alias from its `.wav` and from the built zone, and of
three aliases of shipped `mp_rust.ff` ( 32 and 44.1 kHz IMA ADPCM, decoded ). Not there: effects; streamed sounds
of the game ( files of its archives ); animations of the game on models of the game ( `asset_preview anim` takes
animations of the project ); a model with more than one LOD shows LOD 0 in the animation. The test machine has no
audio device to listen with: that Windows accepted the wave data is what was seen.

`codol_view`: a mesh drawn with the technique set of a model material ( `mc_..` ) uses packed model vertices now.
With world vertices such a mesh came out in one flat colour - a brush with a model material did too.

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

**Animations, sounds, effects.** What each source is and how it is compiled: `codol_assets/README.md`,
"Animations", "Sounds", "Effects".

* An animation needs the model it is for ( the `xanim_export` has no bone hierarchy ): a model of the project by
  its name, or an `.xmodel_export` file. Without one every part is taken as a root - right for a model of one bone.
  Every animation belongs to an animtree, by default one named after the project; the build writes
  `generated\animtrees\<tree>.atr` ( one name per line ) and `generated\animlists\map.animlist`.
* A soundaliases `.csv` is one line of the Assets window with everything it names. Loaded sounds go into the zone,
  streamed files into `zz_<project>_sounds.iwd`. A file that is not under `sound\` is referenced by name - a sound of
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
hanging clothes of shipped mp_crash_tropical stand still as well; **sound** cannot be heard on the test machine,
the client's log has no "Missing soundalias" and no load error. The effects a map places are made by the client
from the text of `maps\mp\<map>_fx.gsc` and `maps\createfx\<map>_fx.gsc`: leave those two to the build, or keep
their exact form.

Not there: deleting or renaming an asset from the editor ( delete the files, Rescan ), editing a material's
definition in a window ( edit the `.mtl`, Build ), a preview inside the import windows ( import, then click the
line ), showing an effect in the editor, streamed images ( `.iwi` in an `.iwd` ), per image options ( format, `flipy` ), the less
common animation options ( `deltapart`, `assettype`, tolerances: `codol_assetc --xanim` has them ), copying the
effects an effect emits on import ( put them into `fx\` by hand ). The shipped animtrees ( `animated_props` .. ) are
not extended: give your animations a tree of their own. Sounds and effects of a map edited with File > Compile map
( `codol_mapc` ) are not carried - only File > Build map does it.
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

- **Maps of your own: lighting is simpler than the game's own maps.** A surface has one primary light ( the sun,
  or one `light` with `PRIMARY_SPOT` / `PRIMARY_OMNI` ) and is not cut where the reach of two meets; a player's
  shadow in a primary light entity was not seen in the client ( the sun's is ); bounced light does not take the
  colour of the materials; no T-junction repair. See "Limits" in `codol_bsp/README.md`.
- **Static models have no player collision of their own.** As in the shipped maps, bullets hit a `misc_model`,
  players do not: put clip brushes around models.
- **Nobody has seen an animation of your own move in the game or heard a sound of your own there.** The editor
  plays both ( the Preview window: the compiled animation read back from the project's zone, the alias' loaded
  sound decoded from it ), the Assets window builds them and File > Build map puts them into the map with their
  animtrees, animlist and your level script; a dedicated server runs a level script that uses them without a
  script error, and the client loads such a map and draws its effect ( [In the built map](#in-the-built-map) ).
  But script model animations do not move in the client on the test server - shipped ones do not either -, the
  test machine has no audio, and streamed sounds need their `.iwd` in the game's `main\` folder ( the client
  started with one there; whether it played the file is not known ).
- **Editing a shipped map** ( File > Compile map ) cannot draw new world materials, does not remove render surfaces
  of deleted brushes and does not re-bake light. For that, build the map from scratch.
- **Imported maps hold collision brushes only.** Brush sides get the right material but a default projection;
  terrain, patches and decals of a shipped map exist only as render surfaces and are not in the imported `.map`.
- **The create-room list: written, not seen in the client.** File > Install the built map .. adds the map to the
  list zone of a game folder and takes it out again ( [A map of your own](#a-map-of-your-own), step 5 ); the zone
  reads back as written, but no client or lobby was started with it. `CODOL_FORCE_MAP=<map>` on the lobby plays a
  map without the list.
- **The 3D view of a map of your own is not the light bake.** It shows the sun, sky and ambient light of the
  worldspawn on brushes and models; light entities are shown as shapes, their light is in the built map.
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

In the order that unlocks the most:

1. **Brush texture alignment.** The UVs of brush sides can be recovered from the coincident
   render surface (in its Cast export); read them and write the texture projection of each side.
2. **Non-brush geometry.** Terrain, patches and decals from the render world as reference models.
3. **Map compile.** Get an IW3 to IW4 map converter (iw3x-port) and wire the chain: `cod4map` /
   `cod4rad`, `linker_pc`, IW3 to IW4, IW4x zonebuilder, AssetExplorer `iw4-port`, install as a
   usermap, `map <name>` over rcon. This is the step that lets edited geometry reach the game.
4. **Zone reader in C++** on top of `c1_layouts.hpp`, to drop the dependency on the external asset
   tool.
