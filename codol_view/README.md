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
| `src/renderer_edit.cpp` | camera math, hide / move / place, lighting of moved models, catalogs, thumbnails | - |
| `src/renderer_pick.cpp`, `src/pick_bvh.*` | picking, selection helpers, the BVH | - |
| `src/renderer_mesh.cpp` | dynamic meshes, the fixed function fallback, the overlay hook | - |
| `tools/codol_viewtool.cpp` | offscreen renderer, bench, state test, image compare, editor API test | `RenderCommands.cs` |
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

## Editor API

Everything a map editor needs on top of drawing the shipped map is in `map_renderer.hpp` as well. None of it is
in the C# renderer. Nothing changes a frame while it is not used: the nine reference views are byte identical to
the frames of the renderer before these additions, with and without `keep_pick_geometry` / `keep_light_grid`.

```cpp
codol::view::options options;
options.keep_pick_geometry = true;      // pick(), object_triangles()
options.keep_light_grid = true;         // moved / new models and meshes are lit where they stand
renderer.init(device, options, &error);
renderer.load_map("mp_rust", &error);

codol::view::frame_options frame;
frame.overlay = [&](const codol::view::overlay_context& context)      // selection outlines, gizmos, grids
{
    context.device->DrawPrimitiveUP(D3DPT_LINELIST, count, lines, sizeof(line));    // struct { float x, y, z; DWORD color; }, world space
};
renderer.render(camera, width, height, frame);

codol::view::pick_result hit;
if (renderer.pick(camera, width, height, mouse_x, mouse_y, hit))      // hit.object, .entity, .distance, .point, .normal, .material
{
    renderer.set_hidden(hit.object, true);
    codol::view::transform t;
    if (renderer.get_transform(hit.object, t)) { t.origin.z += 16; renderer.set_transform(hit.object, t); }
}
const int model = renderer.add_model("com_bomb_objective", { { x, y, z }, { 0, 90, 0 }, 1.0f });
const int brush = renderer.create_mesh(mesh_data);      // position, normal, colour, uv; material by name
```

| what | functions | notes |
|---|---|---|
| camera math | `view_matrix`, `projection_matrix`, `screen_ray`, `project` ( free functions ) | 16 floats, row-major, row vectors ( `D3DMATRIX` ); view space +X right, +Y up, +Z forward; infinite far plane, depth = 1 - z_near / view z. Pixel coordinates are Direct3D 9's: whole numbers are pixel centres |
| overlay hook | `frame_options::overlay` | called after the scene, before the state restore, with the scene's colour target and depth buffer bound and a plain fixed function state ( see the header ). Whatever the hook changes is restored by `render()` |
| picking | `pick`, `pick_ray`, `pick_options`, `pick_result` | triangle accurate against the world, static / entity / placed models ( LOD 0 ), brush models and meshes as placed now. Needs `options::keep_pick_geometry` |
| objects | `objects`, `object`, `object_ref`, `set_hidden`, `is_hidden`, `show_all` | `object_ref { kind, id }` names any object; `scene_object::ref()` gives it |
| transforms | `set_transform`, `get_transform`, `reset_transform`, `transform` | origin, angles ( pitch, yaw, roll in degrees ), uniform scale; static, entity and placed models, brush models |
| new models | `add_model`, `remove_model`, `model_bounds`, `model_names`, `release_zone_cache` | any XModel of the map, level, shared or `options::extra_zones` zones |
| zones at run time | `register_zone`, `unregister_zone` | a fast file anywhere on disk ( a zone of new assets ) becomes a zone like one of `options::extra_zones` while a map is loaded: its materials and models are in the catalogs and usable at once, nothing of the map is loaded again. Registering the name again reads the file again; existing meshes and placed models keep what they were made with until the host makes them again |
| meshes | `create_mesh`, `update_mesh`, `destroy_mesh`, `set_mesh_visible`, `mesh_shading_of`, `mesh_vertex`, `mesh_data` | host triangles with a game material |
| materials | `material_names`, `material_details`, `material_thumbnail`, `material_info` | colour map name and size, surface type bits, flags; thumbnails are `IDirect3DTexture9*` owned by the renderer |
| selection | `object_box`, `object_triangles`, `oriented_box` | to outline an object in the overlay hook |

### Picking

`options::keep_pick_geometry` keeps positions and indices of the world, of every brush model and of LOD 0 of every
distinct model on the CPU, each with a bounding volume hierarchy ( median split, four triangles per leaf ) in its own
space; instances are walked linearly with a sphere test and the ray is taken into the model's space through the
instance's placement. MEASURED ( x64, Release ):

| map | triangles | memory ( `statistics::pick_bytes` ) | build | pick, mean | pick, worst of 22592 rays |
|---|---|---|---|---|---|
| mp_rust ( 3513 static models ) | 295 960 | 15.5 MB | 0.09 s of the 1.9 s load | 0.016 ms | 0.08 ms |
| mp_killhouse_sh ( 16335 static models ) | 794 555 | 41.5 MB | 0.3 s of the 3.9 s load | 0.10 ms | 0.22 ms |

Rules, all chosen so that the hit is what is seen:

* hidden objects are skipped, moved ones are hit where they are, a static model beyond its cull distance from the
  ray origin is skipped;
* a triangle is hit only from the side the game draws it from ( the material's cull mode; `pick_options::backfaces`
  turns that off ), surfaces whose material the game never draws are not in the pick geometry;
* of surfaces in the same place ( a decal on its wall, the layers of terrain - within 0.01 units + 0.002 % of the
  distance ) the one drawn last wins, i.e. the decal;
* the sky box is skipped ( `pick_options::sky` );
* alpha tested materials ( fences, foliage ) are hit on their whole triangles, holes included - the alpha channel is
  not sampled. `material_info::alpha_tested` tells the host.

`codol_viewtool edittest` checks picks against the depth buffer of the drawn scene: for a grid of 336 pixels a marker
1 % in front of each picked point is drawn in the overlay hook and must be visible, a marker 1 % behind must be
hidden. That holds for every hit on a solid material; a material without depth writes or with alpha tested holes
does not hide what is behind it and is counted apart.

### Hiding, moving, placing

* `set_hidden` works on every object ( a world surface is one material batch of the GfxWorld, as in `objects()` ).
  Hidden objects are not drawn, cast no sun shadow and are not picked.
* `set_transform` replaces the placement of a model or brush model. Bounds, frustum culling, the sun shadow pass and
  picking use the new placement. A moved **static model** is no longer culled by cells ( the cell trees list it where
  it was ), only by the frustum. A moved **brush model** keeps its baked lightmaps.
* Lighting of a moved or new model: with `options::keep_light_grid` the light grid is sampled at the lighting origin
  the way `load_map` does it for a `script_model` ( primary light, the 4 x 4 x 4 block of the `$modellighting` volume
  or the ground lighting, nearest reflection probe; the lighting origin keeps its place relative to the model ). The
  volume grows when the editor places more lit models than the map had. Without the option a moved model keeps its
  lighting and a new one gets a neutral grey under the sun. `statistics::light_grid_bytes`: 16.4 MB on mp_rust.
* `reset_transform` puts placement and lighting back exactly ( the frame is byte identical again ).
* `add_model` loads the model the first time ( its zone is read again - 5 to 20 ms for a model of a shared zone on the
  test machine, more for a large zone ); the zone stays in memory for the next call and is dropped after 120 frames
  without one or by `release_zone_cache()`. `model_names()` is every XModel a zone of the library defines; a model can
  still fail to load ( no surfaces in LOD 0 ) - `add_model` then returns 0 with a text.
* A device reset of a **default pool** renderer ( always with `IDirect3DDevice9Ex` ) loads the map again;
  `on_device_reset()` then applies every edit again ( hidden, moved, placed models, meshes; handles stay ). Thumbnail
  pointers do not survive it.

### Dynamic meshes

`create_mesh` takes indexed triangles ( `mesh_vertex`: position, normal, colour, uv - 36 bytes laid out as
`D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_DIFFUSE | D3DFVF_TEX1` ) in world space and a material name. The winding does not
matter, every triangle is oriented by its vertex normals. `mesh_shading_of` tells how it ended up being drawn:

* **`game_material`** - the material's own technique set and shaders. The vertices become `GfxWorldVertex` ( packed
  normal, a tangent computed from the texture coordinates, zeroed layer data for technique sets with more than one
  layer ) and are drawn like a world surface whose primary light is the sun: the technique the game picks for that
  ( `lit sun` / `lit sun shadow` ), so the sun, the dynamic sun shadow map ( the mesh casts and receives ), normal and
  specular maps and the reflection probe all work. What a mesh cannot have is a lightmap. Where the pass reads the
  **primary lightmap** ( the baked sun shadow ) it gets white; where it reads the **secondary lightmap** ( the baked
  ambient ) it gets a 1 x 1 texture with the ambient colour of the light grid at the middle of the mesh
  ( `keep_light_grid`; a neutral grey without it ). MEASURED by swapping the two for solid colours: black secondary =
  only the sun patch is left, black primary = no sun. The ambient is one colour for the whole mesh and comes from the
  light grid, not from a bake, so a mesh is usually a little darker than the lightmapped wall next to it and has no
  ambient occlusion. A material of a model technique set ( `mc_..` ) works the same way and gets a block of the model
  lighting volume instead.
* **`color_map`** - the material exists but cannot be drawn that way: the game never draws it ( camera region >= 4,
  the tool textures ), its technique set is missing, or a pass has no declaration for world vertices. Fixed function:
  the colour map x vertex colour x ( ambient + sun N.L ). No shadows.
* **`flat_color`** - no such material, or it has no 2D colour map: `mesh_data::fallback_color` with the same fixed
  function lighting. ( On the test maps "clip" and friends are not in any zone the client loads, so they end here. )

Why not a technique of the game's for geometry without a lightmap: there is none. MEASURED slot masks of the shipped
technique sets ( mp_rust ): world sets ( `wc_l_sm_..` ) `0x50E0007FFE1F`, model sets ( `mc_l_sm_..` ) `0xD0FFFFFFFE1F` -
`unlit` ( 4 ) and every `lit ..` slot are filled, `fakelight normal` / `fakelight view` ( 40 / 41 ) are not. `unlit` is
the bare colour map without any light, so the lit sun technique with stand-in lightmaps is used.

For brush texturing `material_details` gives the colour map's size in pixels ( read from the IWI header for streamed
images, cached ); Radiant's default scale is 0.25 units per texel, i.e. `u = units * 4 / width`.

### Not done / limits

* Picking ignores alpha test and skinned / animated poses ( there is no animation in the renderer ).
* A click on the ground usually returns the decal or blended terrain layer that lies on top ( it is what is seen );
  hide it and pick again to get what is under it.
* A moved static model is not put into the cells it is in now; it is frustum culled only.
* Brush models cast no sun shadow ( as before ); fixed function meshes cast and receive none.
* Mesh lighting is one ambient colour per mesh; no spot / omni primary light is chosen for a mesh ( always the sun
  when the map has one ).
* Technique sets with more than one texture layer draw a mesh with zeroed layer data; that path is built but was not
  looked at with a multi layer material.
* `material_names()` lists every material of the library, including 2D / effect materials; `material_info::world_material`
  marks the ones made for brushes.
* Material thumbnails of cube maps ( the sky ) are not made.

## codol_viewtool

```
codol_viewtool render    --game <install> --map <name> --out <png> [--size WxH] [--pos x,y,z --angles pitch,yaw[,roll] | --spawn n] [--fov deg]
                         [--no-models] [--no-entity-models] [--no-brush-models] [--no-world] [--no-shadows] [--no-cell-culling] [--wireframe] [--lods]
                         [--ex] [--pool managed|default] [--skip-mips n] [--time s] [--stats] [--verbose]
codol_viewtool bench     ..same.. [--frames n]         frame time ( GPU synchronised ) and CPU time inside render()
codol_viewtool statetest ..same.. [--out <folder>]     state save / restore, host scene, device reset, FPU control word
codol_viewtool compare   <oracle.png> <ours.png> [--out <png>]   mean absolute difference, share of pixels off by > 8, side by side
codol_viewtool edittest  ..same.. [--out <folder>] [--model <xmodel>] [--material <name>]
                         the editor API: picks a grid of pixels and checks them against the drawn depth, hides, moves ( static model,
                         entity model, brush model ), places models, draws boxes with game materials, overlay lines and thumbnails,
                         resets the device; PASS / FAIL lines and edit_*.png
codol_viewtool pickbench ..same..                      time of pick_ray for 22592 rays
render also takes        --box "x,y,z,half:material;.."  --model "x,y,z[,yaw[,scale]]:xmodel;.."  --hide "kind,id;.."
                         --keep-edit-data ( keep_pick_geometry + keep_light_grid )  --extra-zones a,b
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
