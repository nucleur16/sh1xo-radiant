param([string]$Scratch = 'D:\CoD\iw3xo-radiant-codol\build-editor\bsptest', [string]$Map = 'mp_rust', [string]$Project = 'codol_test/spark_burst',
      [string[]]$Shipped = @('fire/firelp_small_pm', 'smoke/thin_black_smoke_m'), [int]$Timeout = 240)
# The effect preview of the editor, unattended: a map is opened ( the pictures of the materials come from its
# renderer ), the project's effect and effects of the game are shown at chosen moments and the frame is saved:
# shots\c6fx_<n>_<moment>.png. The log has asset_preview status for each ( particles, what is not drawn ).
$here = $PSScriptRoot
$Shipped = @($Shipped | ForEach-Object { $_ -split ';' } | Where-Object { $_.Trim() })
New-Item -ItemType Directory -Force "$Scratch\c6\ed", "$Scratch\shots" | Out-Null
$cmds = @("map_open $Map", 'wait 12')
$n = 0
foreach ($fx in @($Project) + $Shipped) {
  $n++
  $cmds += "asset_preview fx $fx"; $cmds += 'wait 2'
  foreach ($t in '0.3', '0.9', '1.8') {
    $cmds += "asset_preview time $t"; $cmds += 'wait 1'; $cmds += "ed_shot $Scratch\shots\c6fx_${n}_$($t -replace '\.', '').png"; $cmds += 'wait 1'; $cmds += 'asset_preview status'
  }
}
$cmds += 'quit'
& "$here\run_editor.ps1" -Cmds ($cmds -join ';') -Log "$Scratch\c6\ed\fx.log" -Timeout $Timeout
Get-Content "$Scratch\c6\ed\fx.log" | Select-String -Pattern '^\[preview\]' | ForEach-Object { $_.Line.Substring(0, [Math]::Min(230, $_.Line.Length)) }
