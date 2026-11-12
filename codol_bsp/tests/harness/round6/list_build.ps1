param([string]$Name = 'mp_c6list', [switch]$Uninstall, [string]$Scratch = 'D:\CoD\iw3xo-radiant-codol\build-editor\bsptest',
      [string]$Client = 'D:\CoD\iw3xo-radiant-codol\build-editor\shadow_client', [string]$Game = 'D:\CoD\CODOL Builds\Call of Duty Online 1.1.1.13')
# Sixth round, the create-room list in a real client: the box room as a map of its own is built by the editor
# ( map_build ) and installed by the editor ( map_install .. list ) into the SHADOW client - zones into its zone
# folder ( usermaps\ of the shadow is a junction into the install and is not used ), the row / arena entry / name
# into its client_shipRetail\patch\codol_common.ff, which is made a real copy first ( it was a hard link to the
# install's file ). The zone also goes into the server's base. -Uninstall: map_uninstall, and the link comes back.
$ErrorActionPreference = 'Stop'
$here = $PSScriptRoot
$H = (Resolve-Path "$here\..").Path
$editor = (Resolve-Path "$here\..\..\..\..\build-editor\Release").Path
$list = "$Client\client_shipRetail\patch\codol_common.ff"
$stock = "$Game\client_shipRetail\patch\codol_common.ff"
New-Item -ItemType Directory -Force "$Scratch\c6\ed", "$Scratch\c6\list_scripts", "$editor\map_source" | Out-Null
if ($Uninstall) {
  & "$here\run_editor.ps1" -Cmds "map_load $editor\map_source\$Name.map;wait 4;map_uninstall;wait 8;map_list $Client;quit" -Log "$Scratch\c6\ed\list_uninstall.log" -Timeout 120
  $same = (Get-FileHash $list).Hash -eq (Get-FileHash $stock).Hash
  "codol_common.ff of the shadow client equals the install's: $same"
  if ($same) { Remove-Item $list -Force; New-Item -ItemType HardLink -Path $list -Target $stock | Out-Null; 'the link to the install is back' }
  foreach ($f in "$Name.ff", "${Name}_load.ff") { $z = "$Scratch\base\client_shipRetail\chinese\$f"; if (Test-Path $z) { Remove-Item $z -Force } }
  return
}
# a real copy of the list zone: remove the link ( never the install's file ), copy
Remove-Item $list -Force
Copy-Item $stock $list
"codol_common.ff of the shadow client is a real copy ( $((Get-Item $list).Length) bytes )"
Copy-Item "$Scratch\maps\box_room.map" "$editor\map_source\$Name.map" -Force
$cmds = "map_load $editor\map_source\$Name.map;wait 4;map_build fast;wait 45;map_list $Client;" +
        "map_install game=$Client zones=$Client\client_shipRetail\chinese scripts=$Scratch\c6\list_scripts list;wait 10;map_list $Client;quit"
& "$here\run_editor.ps1" -Cmds $cmds -Log "$Scratch\c6\ed\list_install.log" -Timeout 240
& "$H\make_base.ps1" -Scratch $Scratch -Game $Game | Out-Null
foreach ($f in "$Name.ff", "${Name}_load.ff") { Copy-Item "$editor\compiled\$Name\zone\$f" "$Scratch\base\client_shipRetail\chinese\$f" -Force }
Get-Content "$Scratch\c6\ed\list_install.log" | Select-String -Pattern 'install|create-room|row ' | ForEach-Object { $_.Line.Substring(0, [Math]::Min(260, $_.Line.Length)) }
"install's codol_common.ff untouched: $((Get-Item $stock).LastWriteTime)"
