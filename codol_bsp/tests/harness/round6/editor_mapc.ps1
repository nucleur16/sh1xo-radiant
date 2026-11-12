param([string]$Scratch = 'D:\CoD\iw3xo-radiant-codol\build-editor\bsptest', [string]$Src = 'D:\CoD\iw3xo-radiant-codol\build-editor\edassets2\src', [int]$Timeout = 420)
# Sixth round, "an edited shipped map with a project's assets", driven through the editor: shipped mp_rust is opened,
# a project ( mod\mp_rust ) gets a material, a model, sound aliases, an effect and a level script, is built, the
# model is placed once as a static model and once as a script_model, and File > Compile map ( map_compile ) makes
# compiled\mp_rust.ff - with the project's zone handed to codol_mapc. -Src: the test sources ( codol_assets/tests:
# make_test_sources.py, make_sound_sources.py, fx\ ).
$here = $PSScriptRoot
$log = "$Scratch\c6\ed\mapc.log"
New-Item -ItemType Directory -Force "$Scratch\c6\ed", "$Scratch\shots" | Out-Null
$cmds = @(
  'map_open mp_rust', 'wait 12', 'ed_windows a',
  "asset_new_material codol_test_crate model $Src\codol_test_crate_col.png normal=$Src\codol_test_crate_nml.png specular=$Src\codol_test_crate_spc.png overwrite", 'wait 2',
  "asset_import_model codol_test_crate $Src\codol_test_crate_lod0.xmodel_export $Src\codol_test_crate_lod1.xmodel_export overwrite", 'wait 2',
  "asset_import_sounds $Src\soundaliases\codol_test_sounds.csv overwrite", 'wait 2',
  "asset_import_fx $Src\fx\codol_test\spark_burst.efx overwrite", 'wait 2',
  "asset_level_script $here\mapc\mp_rust.gsc", 'wait 8',
  'asset_build', 'wait 75', 'asset_list',
  'ed_model_class misc_model', 'ed_place codol_test_crate 1520 -100 -214',
  'ed_model_class script_model', 'ed_place codol_test_crate 1450 -170 -214', 'sel_none',
  'ed_camera 1629 -212 -150 5 135', 'wait 4', "ed_shot $Scratch\shots\c6ed_rust.png", 'wait 3',
  'map_compile', 'wait 120', 'quit') -join ';'
& "$here\run_editor.ps1" -Cmds $cmds -Log $log -Timeout $Timeout
