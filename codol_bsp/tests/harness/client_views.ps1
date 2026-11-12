param([Parameter(Mandatory)][string]$Map, [Parameter(Mandatory)][string]$Prefix, [Parameter(Mandatory)][string[]]$Views, [switch]$ChangeMap, [switch]$NoSpawn,
      [string]$ZoneDir, [string]$Scratch = 'D:\CoD\iw3xo-radiant-codol\build-editor\bsptest', [string]$Game = 'D:\CoD\CODOL Builds\Call of Duty Online 1.1.1.13',
      [string]$InputScript = 'D:\CoD\iw3xo-radiant-codol\build-editor\clienttest\input.ps1')
# With a client connected to the test server: ( -ChangeMap: devmap <Map> over rcon, ) pick a class and spawn, then for
# every view "name=x y z yaw pitch" ( player origin, as setviewpos takes it ) take a screenshot of the client and render
# the same view with codol_viewtool ( eye 60 above the origin ). Pictures: shots\<Prefix>_<name>_s.png and
# png\<Prefix>_<name>_view.png.
$H = $PSScriptRoot
$Build = (Resolve-Path "$H\..\..\..\build\codol_bsp").Path
function RC($c) { & "$H\rcon.ps1" -Cmd "dev_clientcmd 0 $c" -WaitMs 400 -Scratch $Scratch | Out-Null }
if ($ChangeMap) {
  & "$H\rcon.ps1" -Cmd "set codol_sessionMap $Map" -Scratch $Scratch | Out-Null
  & "$H\rcon.ps1" -Cmd "devmap $Map" -WaitMs 3000 -Scratch $Scratch | Out-Null
  Start-Sleep 40
}
if (-not $NoSpawn) {
  & "$H\focus.ps1" | Out-Null; & $InputScript -Keys 'ESC' -NoRestore | Out-Null; Start-Sleep 2
  & "$H\focus.ps1" | Out-Null; & $InputScript -Click '131,154' -NoRestore | Out-Null; Start-Sleep 2
  & "$H\focus.ps1" | Out-Null; & $InputScript -Click '1802,83' -NoRestore | Out-Null; Start-Sleep 6
  RC 'god'
}
((& "$H\rcon.ps1" -Cmd 'status' -Scratch $Scratch) -split "`n" | Where-Object { $_ -match '^map' })
if (-not $ZoneDir) { $ZoneDir = "$Scratch\base\client_shipRetail\chinese" }
foreach ($v in $Views) {
  $name, $spec = $v.Split('=')
  $n = $spec.Trim().Split(' ')
  RC "setviewpos $spec"; Start-Sleep -Milliseconds 2500
  & "$H\focus.ps1" | Out-Null
  & "$H\shot.ps1" -Name "${Prefix}_$name" -Scratch $Scratch
  $env:CODOL_VIEW_STRICT_CELLS = '1'
  & "$Build\codol_mapc\codol_view\Release\codol_viewtool.exe" render --game $Game --map $Map --zone-dirs $ZoneDir --out "$Scratch\png\${Prefix}_${name}_view.png" --size 1600x900 `
    --pos "$($n[0]),$($n[1]),$([double]$n[2] + 60)" --angles "$($n[4]),$($n[3])" --fov 65 | Out-Null
  Remove-Item Env:\CODOL_VIEW_STRICT_CELLS
}
