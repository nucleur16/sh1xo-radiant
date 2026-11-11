# codol_view

Native Direct3D 9 renderer for Call of Duty Online 1.1.1.13 maps. It draws the game's own data - GfxWorld surfaces,
static models, `script_model` entities, brush models - with the game's own materials, technique sets and compiled
SM3 shaders, read from the 1.1.1.13 fastfiles and IWDs. Nothing is converted to another format and no shader is
written by hand. It is a port of CODOL-AssetExplorer's `Codol.Rendering\GameMapRenderer` ( the owner's C# tool ) on
top of the two libraries of this repository:

* `codol_zone` - fastfile reader ( zones, typed views );
* `codol_render` - device independent core ( technique sets, shader arguments, code constants, state bits, IWI, IWD ).

C++20, MSVC, `/W4 /WX` clean for Win32 and x64, static CRT by default. Direct3D 9 from the Windows SDK only
( `d3d9.h` ) - no D3DX, no third party code.

```
cmake -S codol_view -B build/view_x86 -G "Visual Studio 17 2022" -A Win32      ( or -A x64 )
cmake --build build/view_x86 --config Release
ctest --test-dir build/view_x86 -C Release
```

## Layout

| path | what | C# source |
|---|---|---|
| `include/codol_view/map_renderer.hpp` | the public API | - |
| `src/map_renderer.cpp` | init, targets, device loss, editor queries | `GameMapRenderer.cs` ( EnsureTargets, Dispose ) |
| `src/renderer_load.cpp` | `load_map`: zones -> GPU resources, scene, model lighting, textures | `GameMapRenderer.cs` ( BuildWorld, ReadStaticModels, ReadEntityModels, GetMaterial, GetModel, Prepare, BuildModelLighting ), `GameTextureCache.cs`, `MapExtractor.cs` |
| `src/renderer_draw.cpp` | one frame: culling, draw order, passes, arguments, samplers, state save / restore | `GameMapRenderer.cs` ( RenderFrame .. BindSampler ) |
| `src/renderer_shadow.cpp` | sun shadow map ( two partitions ) | `GameSunShadow.cs`, `GameMapRenderer.SunShadow.cs` |
| `src/zone_library.*` | cross zone registry by name, frozen zones | `RenderAssetLibrary.cs`, `RenderLayouts.cs:85-128` |
| `src/model_surfaces.*` | packed model surfaces, streamed `models/<surfs>` files | `ModelExtractor.cs:140-222`, `StreamedSurfaceLoader.cs` |
| `src/light_grid.*`, `src/sight_trace.*` | light grid sampling, model lighting volume, collision sight trace | `LightGrid.cs`, `WorldSightTrace.cs` |
| `src/world_cells.*`, `src/world_visibility.*` | cells, portals, AABB trees, per frame visibility | `WorldCells.cs`, `GameWorldVisibility.cs` |
| `src/entities.*` | entity string parser | `MapEntityParser.cs` |
| `tools/codol_viewtool.cpp` | offscreen renderer, bench, state test, image compare | `RenderCommands.cs` |
| `tools/lighting_check.cpp`, `tools/visibility_check.cpp` | checks of the two CPU ports against the oracle | - |
| `tests/codol_view_tests.cpp` | unit tests ( no install, no device needed ) | - |

## What is drawn

| stage | state |
|---|---|
| world surfaces: vertex / index buffers, the three draw ranges, technique choice, passes with all arguments, material textures ( streamed IWIs and inline pixels ), lightmaps, reflection probes, state bits, sampler states | done |
| static models ( placement, cullDist, packed vertices, surfaces streamed from the IWDs ), `script_model` entities ( origin / angles / modelscale, destructible damage parts hidden ), model lighting ( light grid sampled on the CPU into the `$modellighting` volume, `lightprobeAmbient`, `baseLightingCoords` ) | done |
| brush models ( `*N` ): the surface run of `GfxWorld::models[N]` at the entity's origin / angles | done - **not in the C# renderer**. Entities with `"script_specialops" "1"` ( map wide special ops blockers ) are off by default ( `frame_options::draw_special_ops_brush_models` ) |
| primary lights ( sun / spot / omni constants, attenuation sampler ), frustum + cell / portal visibility | done |
| sun shadow map | done |
| float z pre-pass ( soft, z feathered materials ) | done |
| LOD selection by `XModelLodInfo::dist` | done, off by default ( `options::load_model_lods` + `frame_options::model_lods` ); the C# renderer always draws LOD 0 |
| fog | **not drawn**. The map's zones hold no fog; see `map_renderer::fog_source()` |
| spot / omni shadow maps, post effects ( film, glow ), FX, skinning / animation | not ported |

## Embedding ( host owns the device )

```cpp
codol::view::map_renderer renderer;
codol::view::options options;
options.game_root = L"D:/.../Call of Duty Online 1.1.1.13";
renderer.init(device, options, &error);            // once; AddRefs the device
renderer.load_map("mp_killhouse_sh", &error);      // seconds; builds every GPU resource, releases the zones

// per frame, on the thread that uses the device, anywhere in the host's frame:
codol::view::frame_options frame;                  // frame.target = host surface, or leave null
IDirect3DTexture9* colour = renderer.render(camera, width, height, frame);
// draw `colour` in the host's viewport ( it is an A8R8G8B8 render target texture of the size asked for )

// device loss:
renderer.on_device_lost();   device->Reset(&pp);   renderer.on_device_reset();
renderer.unload(); renderer.shutdown();            // or the destructor
```

* **State.** `render()` captures an `IDirect3DStateBlock9` of type `D3DSBT_ALL`, plus render target 0, the depth
  stencil surface and the two stream sources ( MEASURED: a state block does not restore a stream's *offset* ), sets a
  known baseline for every state the game's state bits do not cover ( scissor, fog, clip planes, stencil masks, mip
  bias .. ), draws, then restores the targets, applies the block and resets the streams. `codol_viewtool statetest`
  proves it by reading the whole device state back.
* **Never**: `Present`, the back buffer, the FPU control word, `Get*` state calls other than `GetRenderTarget` /
  `GetDepthStencilSurface` / `GetStreamSource`.
* **Scene.** `frame_options::scene`: `automatic` ( default ) calls `BeginScene` and only ends the scene when that
  succeeded - inside the host's scene it fails harmlessly and the scene stays the host's; `begin_end`; `inside_scene`.
* **Device.** `IDirect3DDevice9` or `IDirect3DDevice9Ex`, hardware or mixed vertex processing, shader model 3.
  `D3DCREATE_MULTITHREADED`, `FPU_PRESERVE` are fine.
* **Pools / device loss.** `resource_pool::managed` ( default for `IDirect3DDevice9` ): textures and buffers survive
  `Reset`, only the render targets and the state block are made again; the runtime keeps a system memory copy of all
  of them. `resource_pool::default_pool` ( always for `IDirect3DDevice9Ex` ): no system memory copy;
  `on_device_lost()` drops the map and `on_device_reset()` loads it again.
* **Threading.** One renderer is used by one thread at a time. No global mutable state.
* **Memory.** While loading, the map zone and at most one other zone are in memory; all zone memory is released
  when `load_map` returns. What stays in system memory: the frozen library of the shared / level zones ( technique
  sets, shader byte code, materials by name - kept between maps, `options::keep_library` ), the CPU side scene
  ( surfaces, instances, cells, entities ) and, with the managed pool, Direct3D's copies of the GPU resources.
  `options::texture_skip_mips` drops top mip levels of streamed textures.

## codol_viewtool

```
codol_viewtool render    --game <install> --map <name> --out <png> [--size WxH] [--pos x,y,z --angles pitch,yaw[,roll] | --spawn n] [--fov deg]
                         [--no-models] [--no-entity-models] [--no-brush-models] [--no-world] [--no-shadows] [--no-cell-culling] [--wireframe] [--lods]
                         [--ex] [--pool managed|default] [--skip-mips n] [--time s] [--stats] [--verbose]
codol_viewtool bench     ..same.. [--frames n]         frame time ( GPU synchronised ) and CPU time inside render()
codol_viewtool statetest ..same.. [--out <folder>]     state save / restore, host scene, device reset, FPU control word
codol_viewtool compare   <oracle.png> <ours.png> [--out <png>]   mean absolute difference, share of pixels off by > 8, side by side
```

The tool creates a hidden window and its own device ( `D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_MULTITHREADED |
D3DCREATE_FPU_PRESERVE` ) and reads the frame back; PNGs are written with WIC.

## Verification

Oracle: `CodolSoundDumper render-map --no-post --no-effects` ( C# renderer, Direct3D 9Ex ), same camera, 1280x720.
With `--no-shadows` on both sides and `--no-brush-models` semantics ( the oracle has no brush models ) the frames are
**bit identical**; see the report of the session that wrote this library for the table. With shadows on, the only
differences are where the oracle's spot shadow maps fall ( not ported ).

Deviations from the C# renderer, all deliberate:

* brush models are drawn ( above );
* code samplers `black` and `identityNormalMap` bind the black / flat normal stand-ins, the C# binds white for every
  code sampler it has no source for ( none of the compared views reads them );
* a model / material pointer that is a by-name placeholder ( `Zone::reference` ) is resolved by name; the C# treats it
  as unnamed;
* per frame caches are reset at the start of every `render()` ( the host changes the device in between ).
