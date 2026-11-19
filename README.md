<h2 align="center">sh1xo-radiant - fork of xoxor4d's iw3xo radiant // radiant for sh1 / Call of Duty Online 1.1.1.13</h1>

<div align="center" markdown="1">

This project aims to fix existing bugs, extend functionalities across the board and to replace   
the dated MFC/Windows UI with a more modern and flexible UI powered by Dear ImGui.
</div>


<br>

If you want to buy xoxor4d a coffee:

[![ko-fi](https://xoxor4d.github.io/assets/img/social/kofi.png)](https://ko-fi.com/xoxor4d)

</div>

<br>

> **This is the `codol-1.1.1.13` branch: iw3xo-radiant for Call of Duty Online 1.1.1.13.**
> It adds a CoDOL menu with a script workbench (GSC, animtrees, `playeranim.script`, scripted-animation tables,
> animation validation), map entity import / export for shipped maps, hot reload through rcon and a docked game window.
> It also has a standalone editor, `CoDOLRadiant.exe`, with a map compiler ( `codol_bsp` ), a shipped-map patcher
> ( `codol_mapc` ), an asset compiler ( `codol_assetc` ) and a fastfile linker ( `codol_linker` ).
> Read **[docs/CODOL.md](docs/CODOL.md)** for what works, what does not exist yet, setup and usage.
> [Using it with the CoD Online offline build](#using-it-with-the-cod-online-offline-build) says how it connects
> to the current offline client. Everything after that section is the upstream readme.

## Using it with the CoD Online offline build

**Just want the tools?** Download the release zip ( `CoDOL-ModTools-*.zip` ) and follow its `INSTALL.txt`.

The offline build is `codoMP_client_shipRetail.exe` with `Kernel33.dll` ( the CoD Online 1.1.1.13 offline release ).
Kernel33 runs the lobby emulator and the match server **inside the game process**. You don't run a separate
dedicated server or a lobby. The server starts when a match starts. Its files are:

| What | Where in the game folder |
|---|---|
| map zones ( `.ff` ) | `client_shipRetail\chinese\` |
| server scripts | packed in `main\sh1x_svr_scripts.iwd`; loose `.gsc` files in `main\sh1x_rawfiles\` ( level scripts of your maps, edited scripts ) override the packed ones file by file |
| archives ( `.iwd`, sounds included ) | `main\`, which the client and the server share |
| rcon password | `p2p\offline\native_server_rcon.txt`, written the first time you host |
| rcon port | UDP `127.0.0.1:17760`. If that port is in use, a free port is chosen. To pin one, put `host_port=<port>` in `k33_offline.ini` next to the exe |

### Setup

The tools read the fastfile and archive keys and the image tables from the game's own
`codoMP_client_shipRetail.exe` ( patched for Kernel33 or not ); nothing else is needed. In **File > Settings**
( standalone editor ) or **CoDOL > Script Workbench > Setup** ( radiant plugin ):

| Setting | Value for the offline build |
|---|---|
| CoDOL install folder ( `codol_install_path` ) | the game folder |
| Script folder ( `codol_script_root` ), and the server script folder for *Install the built map* | `<game>\main\sh1x_rawfiles` |
| Start command ( `codol_launch_cmd` ) | `<game>\codoMP_client_shipRetail.exe` |
| Client process names ( `codol_client_exe` ) | the default ( `codoMP_client_shipRetail.exe` ) |
| Port ( `codol_rcon_port` ) | `17760`. The tools default to 17733, which was the old dedicated server's port |
| Password file ( `codol_rcon_password_file` ) | `<game>\p2p\offline\native_server_rcon.txt`. This one is not in the tools' automatic search list, so set it here |

### Build, install, play

1. Build the map ( **File > Build map** ), then install it with
   `map_install game=<game> zones=<game>\client_shipRetail\chinese scripts=<game>\main\sh1x_rawfiles`.
   The zone has to go into `client_shipRetail\chinese`, because the lobby doesn't look in `usermaps\`.
2. Start the game and open the console. `devmap <map> [gametype]` loads the map with cheats on, and `map <map> [gametype]`
   loads it with cheats off. The gametype defaults to `war` for `mp_`, `zm` for `zm_` and `coop` for `coop_` maps.
   The command makes a room, sets the map and presses Ready for you, so there's no menu to go through. During a match
   on your own server, `devmap` / `map` change the level on that server.
3. With the match running, **Reload in game** / `codol_reload` and the Map Entities export reach the server over rcon.
   They work only while a match is running, because before a match there is no server to talk to.

`CODOL_FORCE_MAP` in older docs was for the standalone lobby and doesn't apply to Kernel33; use `devmap` instead.


### Build / Compile

<br>

### How to build / compile the project using Visual Studio
1. Clone the repo! __(zip does not include deps!)__
2. Copy everything within the assets folder into your cod4-root folder
3. Use __update_submodules.bat__ to update submodules
4. Use __generate-buildfiles.bat__ to build project files with premake
5. Load the solution `(build/iw3xo-radiant.sln)` and open the iw3r project-settings to setup paths:
> - General &ensp; &ensp; &ensp; output directory path -> `path-to-cod4-root\bin\`
> - Debugging &ensp; command -> `path-to-cod4-root\bin\IW3xRadiant.exe`
> - Debugging &ensp; working directory -> `path-to-cod4-root\bin\`
> + Preferred: Create a new environment variable called `COD4_ROOT` that points to your cod4 directory and re-generate the project files.  
6. Build -> Build Solution or run with debugger  

<br>

### How to build the project using VSCode
1. Install C++ Build tools (msbuild)
2. Add msbuild folder to the "PATH" environment variable:
> `%ProgramFiles(x86)%\Microsoft Visual Studio\2019\Community\MSBuild\Current\Bin`
3. Add `COD4_ROOT` environment variable with path to your cod4 directory 
> `"COD4_ROOT" "D:\COD4-Modtools"`
4. Clone the repo (zip does not include deps!)
5. Copy everything within the assets folder into your cod4-root folder
6. Drag and drop all files from `.vscode/tracked\` into `.vscode\`
7. Open __iw3xo-radiant.code-workspace__ 
8. Run task: `update_submodules` or open __update_submodules.bat__
9. Run task: `generate-buildfiles` or open __generate-buildfiles.bat__

Use provided build-tasks with the option to copy `iw3r.dll` and `iw3r.pdb` to `%COD4_ROOT%/bin`  
__Run->Start Debugging__ will build and copy a debug build to to `%COD4_ROOT%/bin` and launch IW3xRadiant.

<br>

## Credits
- [xoxor4d](https://github.com/xoxor4d)
- [ssh](https://github.com/S3RAPH-1M) - structs and viewport
- [X Labs](https://github.com/XLabsProject) (especially [Snake](https://github.com/momo5502))
- [The Plutonium Project Team](https://plutonium.pw/) (especially [Rektinator](https://github.com/RektInator))
- [Nukem9 - LinkerMod](https://github.com/Nukem9/LinkerMod) & [detours](https://github.com/Nukem9/detours)
- [JTAG - NootNoot](https://twitter.com/imjtagmodz)
- [ocornut - Dear ImGui](https://github.com/ocornut/imgui)
- [CedricGuillemet - ImGuizmo](https://github.com/CedricGuillemet/ImGuizmo)
- [nlohmann - fifo_map](https://github.com/nlohmann/fifo_map)
- [David Gallardo - imgui_color_gradient](https://gist.github.com/galloscript/8a5d179e432e062550972afcd1ecf112)
- [nem0 - ImGui CurveEditor](https://github.com/nem0/LumixEngine/blob/39e46c18a58111cc3c8c10a4d5ebbb614f19b1b8/external/imgui/imgui_user.inl#L505-L930)
- [Limeoats - L2DFileDialog](https://github.com/Limeoats/L2DFileDialog/)
- [zfedoran - ImGui Spinner](https://github.com/ocornut/imgui/issues/1901)
- [patrickcjk - imgui-notify](https://github.com/patrickcjk/imgui-notify)
- [maluoi - tga writer](https://gist.github.com/maluoi/ade07688e741ab188841223b8ffeed22)
- [NVIDIA - PhysX](https://github.com/NVIDIAGameWorks/PhysX)
- [Infinity Ward - OG. Radiant and Effects Framework](https://www.infinityward.com)
- [id-Software - OG. Radiant](https://github.com/id-Software/Quake-III-Arena/tree/master/q3radiant)

<br>

## Disclaimer
This software has been created purely for the purposes of academic research. Project maintainers are not responsible or liable for misuse of the software. Use responsibly.
