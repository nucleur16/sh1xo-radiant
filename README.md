


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
