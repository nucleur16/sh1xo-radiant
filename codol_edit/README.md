# codol_edit

UI independent core of the standalone map editor for Call of Duty Online 1.1.1.13: the document model, the
`.map` reader / writer, geometry, picking, the editing operations with undo, the C1 entity class catalog, the
fastfile import and the entity override export.

Static library, namespace `codol::edit`. No ImGui, no Direct3D, no radiant, no global mutable state. It depends on
`codol_zone` ( fastfile reader ) and compiles `src/codol/map_ents.cpp`, `map_geometry.cpp` and `map_zone.cpp` of
this repository from where they are ( those need the rapidjson headers of `deps/rapidjson` ).

```
cmake -S codol_edit -B build-codol-edit -A x64        ( or -A Win32 )
cmake --build build-codol-edit --config Release
build-codol-edit/Release/codol_edit_tests.exe
```

or `add_subdirectory( codol_edit )` + `target_link_libraries( editor PRIVATE codol_edit )` ( the root
`CMakeLists.txt` already adds it ). An executable that links `codol_edit` must not compile `src/codol/map_*.cpp`
itself: the library contains them.

CMake options: `CODOL_EDIT_BUILD_TESTS`, `CODOL_EDIT_BUILD_TOOLS` ( both on when built standalone ),
`CODOL_EDIT_RAPIDJSON_INCLUDE`, and for the tests `CODOL_EDIT_GAME_DIR`, `CODOL_EDIT_MAP_SOURCE_DIR`.

## Headers

| header | contents |
| --- | --- |
| `codol_edit/math.hpp` | `vec3`, `plane`, `bounds`, `ray`, `transform`, game angles, grid snapping ( all double ) |
| `codol_edit/model.hpp` | `entity`, `brush`, `face`, `tex_def`, `patch`, `patch_vertex`, `layer`, `object_id`, `face_ref` |
| `codol_edit/geometry.hpp` | windings, `brush_geometry`, texture axes / uv, `mesh`, patch tessellation, ray tests, convex hull |
| `codol_edit/document.hpp` | `document`: files, model access, selection, picking, undo, every editing operation |
| `codol_edit/class_catalog.hpp` | `class_catalog::c1()`: the entity classes the shipped maps use |
| `codol_edit/codol_edit.hpp` | all of the above |

## Model

A document is a list of entities, the worldspawn first. An entity has ordered key / value pairs and owns brushes
and patches ( `children` ). Every entity, brush and patch has an `object_id` that is unique in the document, is
never reused and is the same after undo / redo.

Objects are immutable from the outside: `find_entity` / `find_brush` / `find_patch` return const pointers and an
edit replaces the object ( copy on write ). Keep ids, not pointers. `revision` of an object changes with every edit
and returns to its old value on undo, so `( id, revision )` identifies exact contents - the key for a GPU mesh cache.
`document::change_counter()` changes with any change of the document, `selection_counter()` and
`visibility_counter()` with selection and hide state.

- `brush`: faces ( three plane points as written, `texture`, `lightmap`, trailing text ), `layer`, `contents`
  ( `detail`, `nonColliding`, `weaponClip` ), `tool_flags`, unknown statements in `extra`. A block the reader does not
  understand, or one that only holds a `physics_box` / `physics_cylinder`, is an `opaque` brush: kept verbatim, never edited.
- `patch`: `mesh` ( terrain ) or `curve`, materials, smoothing, `width` x `height` control vertices with position,
  optional colour, texture numbers and the turned edge flag.
- `tex_def`: `size_x` / `size_y` are the world units one texture repeat covers ( radiant: texture size x scale ),
  `shift` in texels, `rotation` in degrees.
- `layer`: the table at the top of the file; `active` is saved, `hidden` is view state.

## Files

`load_map_file` / `load_map_text`, `save_map_file` / `save_map_text`, `new_map`.

The reader keeps the exact text of every object. The writer emits that text for everything that was not edited:
an unedited document is written back byte for byte ( comments, blank lines, odd spacing, CRLF or LF ), and after an
edit only the edited objects are rewritten ( plus the `// brush N` / `// entity N` numbers when objects were added or
removed ). Edited and new objects are written the way CoD4Radiant writes them ( `%.8g` numbers with three digit
exponents ). `save_map_text_canonical()` rewrites everything.

## Geometry

- `document::geometry( brush )` - `brush_geometry`, cached per revision: plane per face, welded corners, per face corner
  loop ( counter clockwise from outside; empty when the face has no winding, eg. a bevel plane ), bounds, `closed`, `valid`.
  Windings are built by clipping with a near exact epsilon, corners closer than 0.02 are welded.
- `document::build_mesh( id, mesh, patch_subdivisions )` - triangle list for a brush ( one `mesh_part` per face ) or a
  patch ( bezier tessellation for curves, the control grid for terrain ) with position, normal, uv, lightmap uv, colour.
- `set_texture_size_callback` supplies texture sizes ( material -> width, height; 256 when not set ). They are needed
  for the shift part of the uv, for texture lock and for the default scale of new faces.
- `object_bounds( id )` for anything; `set_entity_box_callback` for the box of point entities ( model bounds ), else
  the class catalog, else a 16 unit cube.
- Free functions in `geometry.hpp` do the same on objects that are not in a document.

## Selection, picking, queries

The selection holds brushes, patches and point entities; selecting an entity that has brushes selects its brushes.
Faces are selected separately ( `select_face` ). `pick( ray, result, options )` returns the closest hit ( object,
kind, owning entity, face index, distance, point ), `pick_all` all of them in order. `query_rect( axis, lo, hi,
fully_inside )` is the drag rectangle of a 2d view, `query_volume( planes, fully_inside )` a frustum. Hidden
objects and objects on hidden layers are skipped unless `pick_options::include_hidden`.

## Undo

Every operation is one named undo step. `begin_transaction( name )` .. `end_transaction()` makes any number of
operations one step ( a mouse drag ); a non zero merge key merges consecutive transactions with the same key.
`cancel_transaction()` rolls back. Undo restores the document and the selection as they were before the step. A
change of selection or of hidden state alone is not a step. `modified()` / `mark_saved()` follow undo and redo.

Operations return `false` / `no_object` and set `last_error()` when there is nothing to do or the result would be
invalid; in that case nothing changed.

## Operations

| group | functions |
| --- | --- |
| create | `create_box`, `create_cylinder`, `create_cone`, `create_wedge`, `create_brush_from_points`, `create_terrain_patch`, `create_point_entity`, `create_brush_entity` |
| selection | `delete_selection`, `clone_selection`, `translate`, `rotate`, `scale`, `flip`, `transform_selection`, `snap_to_grid` |
| shape | `move_face`, `resize_selection`, `move_brush_corners` ( vertices and edges ), `move_patch_vertices` |
| brush primitives, patches ( `codol_edit/patch_ops.hpp`, free functions ) | `create_brush_shape` ( box, wedge, sided prism, cone, sphere ), `brush_shape_from_selection`, `create_curve_patch`, `create_patch_shape` ( bevel .. cone ), `curve_to_terrain`, `faces_to_terrain`, `patch_insert` / `_delete` / `_insert_terrain` / `_extrude_terrain` / `_delete_terrain`, `patch_transpose`, `patch_invert`, `patch_redisperse`, `patch_split`, `patch_change_subdivision`, `patch_weld`, `patch_weld_to_brushes`, `move_patch_vertices_welded`, `patch_thicken`, `patch_cap`, `patch_turn_edges`, `patch_cap_texture`, `patch_natural_texture`, `patch_fit_texture`, `patch_set_color`, `patch_add_noise`, `edit_patch`. Tests: `codol_patch_ops_tests` |
| csg | `clip` ( front / back / both ), `csg_subtract`, `csg_hollow` |
| textures | `set_material`, `shift_texture`, `scale_texture`, `rotate_texture`, `set_texture`, `fit_texture`; `set_texture_lock` |
| entities | `set_key`, `remove_key`, `rename_key`, `connect_entities`, `targets_of`, `targeting`, `move_selection_to_worldspawn` |
| layers | `add_layer`, `remove_layer`, `rename_layer`, `set_active_layer`, `move_selection_to_layer`, `set_layer_hidden` |
| contents | `set_detail`, `set_contents` |
| visibility | `set_hidden`, `hide_selected`, `hide_unselected`, `show_all` |

Transforms move brushes ( plane points ), patches ( control vertices ) and point entities ( `origin`, and `angles` /
`angle` for rotations ). A brush entity that is selected completely takes its `origin` key along. With texture lock
( default ) the alignment of every face is recomputed so the texture stays on the surface: exact for translations
and for anything that keeps the face axial; on other faces the closest alignment the format can express.

## Class catalog

`class_catalog::c1()` lists the 143 classnames that occur in the entity strings of the 42 map zones of the install:
count, number of maps, point vs. compiled brush model ( `"model" "*N"` / `"?N"` ), every key with its frequency and
up to four example values, the spawnflags values. `tools/codol_classgen` generates `src/class_catalog_data.inc`:

```
codol_classgen --game "<install>" --out codol_edit/src/class_catalog_data.inc [--report classes.txt]
```

Hand written on top: the box of player spawn points ( 32 x 32 x 72 ) and of a few marker classes, a display colour
and a one line description.

## Fastfile import and entity override

`import_fastfile( map.ff, C1.exe, error )` reads a shipped map: entities, the collision brushes of the world and of
each brush model, static models as `misc_model` with `"codol_static" "1"`. The document it gives saves to exactly the
file `codol_mapconv --ff` writes ( same code: `mapgeo::read_zone_map` + `mapents::to_radiant_map` ). The keys
`codol_id`, `codol_base_crc32`, `codol_map`, `codol_trigger_models` .. stay on the entities.

`export_entity_override( out, error )` builds `<map>.d3dbsp.ents` with `mapents::merge_radiant_map`: every key that
was not edited is written with its value from the zone. For an unedited import the text after the three comment
lines is the zone's entity string. The export needs the baseline of the import: it is kept after `import_fastfile`;
for a map that was saved and loaded again, store `import_baseline_map()` and `import_original_ents()` next to the map
and hand them back with `set_import_baseline` ( the radiant fork uses `<map>.codol_base.map` and `<map>.codol_orig.ents` ).

## Tests

`codol_edit_tests [folders with .map files]`: reader / writer, round trip of every `.map` below
`CODOL_EDIT_MAP_SOURCE_DIR`, geometry, every operation with undo and redo, picking, the class catalog, and with the
install: import of `mp_rust` and `mp_killhouse_sh` ( all brushes closed and valid ), override export identity.
Also `--check <file.map>`, `--canonical <in> <out>` and `--import <ff> <exe> <out.map> [out.ents]`.

## What is approximate or missing

- Texture coordinates follow the quake rule ( base axes from the dominant axis of the normal, rotation, then
  `u = p . U / size_x + shift_x / texture width` ). This has not been compared with the game's renderer.
- Patch texture numbers are taken as texels ( divided by the texture size for the mesh ), the patch normal as
  `cross( along columns, along rows )`; the two numbers after the size of a patch ( `16 8` ) are kept, their meaning
  is not known. The patch syntax comes from the format strings of CoD4Radiant - no file with patches was available.
- No patch tools beyond moving vertices and transforms ( no cap, thicken, row / column insert, patch texturing ).
- No CSG merge, no prefab expansion ( `misc_prefab` is a point entity ), no model scale on `scale`.
- Point entity boxes are axis aligned ( not rotated by `angles` ).
- `query_volume` with `fully_inside = false` tests against the planes only: an object near a corner of the volume
  can be reported although it is just outside.
- Where an entity's `layer` line goes in the file is a guess ( first line of the entity ).
