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

## What works

| Area | Status |
|---|---|
| Script editing | Browse and edit the game's script tree (`.gsc`, animtrees, `mp/playeranim.script`, string tables, ...) inside radiant, with syntax highlighting, find / go to line, GBK and CRLF preserved on save. |
| Player animations | Structured view of `mp/playeranim.script` (defines, states, movement types, conditions, events) with jump-to-line, checked against `animtrees/multiplayer.atr`. |
| Scripted animations | Per-script tables of `level.scr_anim[..]` / `scr_model` / `scr_animtree` entries and `anim_*` calls, the governing `#using_animtree` for the cursor line, and an animtree browser. |
| Validation | A `%anim` that is not in its animtree is an error (the server would append it and shift every later index: T-pose / wrong clips). Optional: animations the game does not ship are warnings. |
| Map entities | Import the entities of a shipped map zone into radiant, edit them, export a server-side override (`<scripts>/maps/mp/<map>.d3dbsp.ents`). Unedited entities export byte-identical to the zone. |
| Hot reload | rcon to the local dedicated server: reload the level when a script is saved, load a map after an entity export. Optional pre-check command. |
| Game window | The window of the running game client can be docked inside radiant (CoDOL > Game Window) and handed back. |
| Asset layouts | `src/codol/c1_layouts.hpp`: the 1.1.1.13 zone layouts (XModel, XSurface, Material, techset, GfxImage, XAnimParts, clipMap, GfxWorld, MapEnts, ...) as C++ structs, every size and named offset checked at compile time. |

## What does not exist yet

Be aware of these before planning work around this branch:

- **No CoDOL bsp compiler.** Nothing compiles a `.map` into a 1.1.1.13 map zone. `cod4map` / `cod4rad`
  / `linker_pc` still produce CoD4 output. World geometry, static models and lighting of a CoDOL map
  can therefore not be changed from radiant; only its entities can (see [Map Entities](#map-entities)).
- **No CoDOL asset loading.** Radiant itself is still the CoD4 radiant process: it loads CoD4 xmodels,
  materials and images from `raw/` and `main/`. CoDOL models and textures do not show up; a
  `script_model` whose model only exists in CoDOL is drawn as a placeholder box. The layouts in
  `c1_layouts.hpp` are the foundation for an importer but no zone reader uses them yet apart from
  the `MapEnts` asset.
- **No world preview of CoDOL maps.** Imported entities float in empty space. The d3dbsp viewer
  only understands CoD4 d3dbsp files.
- **Entity overrides are mp only.** The server applies `<map>.d3dbsp.ents` in mp matches; co-op and
  zombies compose their own entities.

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
| Asset tool | `codol_asset_tool` | `CodolSoundDumper.exe` of CODOL-AssetExplorer. Only needed to import entities straight from a `.ff`. |
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
   A `.ff` is decrypted with the asset tool. The entities are written to
   `<root>/map_source/codol/<map>.map` and opened.
2. **Edit** in radiant.
3. **Save + export override**: writes `<script folder>/maps/mp/<map>.d3dbsp.ents` and, if enabled,
   sends `map <map>` to the local server. A file that was already there is kept once as `.bak`.

How entities are represented:

- Point entities are what they are in the game.
- An entity that uses a hull compiled into the zone (`"model" "*N"` brush model, `"model" "?N"`
  trigger hull) becomes a `codol_bmodel` point entity; its real class is kept in `codol_classname`.
  It can be moved (hull bounds are relative to the origin), re-keyed, deleted or copied. New hulls
  cannot be made. A `?N` that the zone does not have is skipped on export with a warning, because
  the server rejects the whole override otherwise.
- The worldspawn carries `codol_map`, `codol_base_crc32` (CRC-32 of the zone's entity string; the
  server only uses an override made for the zone it has mounted) and `codol_trigger_models`.
- Brushes drawn in radiant, `misc_model` and `misc_prefab` are not exported (a warning says so).

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
up, `CODOL_AUTOSHOT=<file.png>` saves the rendered UI frame to that file every two seconds, and
`CODOL_NO_MONO_FONT=1` skips loading the monospace editor font.

## Code layout, build and tests

| Path | |
|---|---|
| `src/codol/` | Engine independent code: script lexer, animation index, map entity conversion, 1.1.1.13 layouts. No radiant or imgui dependency. |
| `src/components/modules/codol_game.*` | rcon, reload, game window, settings |
| `src/ggui/script_workbench.*`, `src/ggui/codol_map_ents.*` | the windows |
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

1. **Zone reader in C++.** Container (zlib + AES-CTR, key block from the client exe) and a stream
   walker for the 44 asset types, on top of `c1_layouts.hpp`. A zone is a sequential stream, so
   reaching one asset means walking every asset before it.
2. **Asset import.** XModel / XModelSurfs -> CoD4 `xmodel` / `xmodelparts` / `xmodelsurfs`,
   GfxImage (IWI 9, wavelet formats) -> IWI 6, Material -> a CoD4 material with the closest techset.
   That makes CoDOL models and textures visible in radiant.
3. **World preview.** GfxWorld + clipMap -> a CoD4 d3dbsp for the built-in viewer, or clipMap
   brushes -> reference brushes in the imported entity map.
4. **Map compile.** CoD4 d3dbsp -> 1.1.1.13 map zone. This is the piece that would allow new
   geometry.
