param([string]$Scratch = 'D:\CoD\iw3xo-radiant-codol\build-editor\bsptest', [string]$Game = 'D:\CoD\CODOL Builds\Call of Duty Online 1.1.1.13',
      [switch]$Server, [int]$Bots = 8, [int]$Seconds = 90, [string[]]$Assets = @(), [int]$Port = 17733)
# End to end: the unit tests, the command line tool on every test map, codol_linker --verify and codol_bsp --check on
# every fast file, pictures with codol_viewtool, and ( -Server ) a run of the dedicated server with bots on the box room.
# -Assets <project zones>: the zone of tests/make_modder_assets.py ( bsp_test_* materials ) and codol_assets' test zone
# ( codol_test_crate ); with them the modder material map and the yard are compiled too, and -Server runs the bots on
# the yard, packaged by tools/package_map.ps1 ( minimap material, loading picture, level script ).
# -Port: the UDP port of the test server ( loopback ). Nothing is written outside $Scratch.
$ErrorActionPreference = 'Stop'
$Build = (Resolve-Path "$PSScriptRoot\..\..\build\codol_bsp").Path
$Bsp = "$Build\Release\codol_bsp.exe"
$Linker = "$Build\codol_mapc\codol_zone\Release\codol_linker.exe"
$View = "$Build\codol_mapc\codol_view\Release\codol_viewtool.exe"
$out = "$Scratch\e2e"
New-Item -ItemType Directory -Force "$out\ff", "$out\png" | Out-Null
$failed = 0

$ta = @('--out', "$out\tests", '--game', $Game)
foreach ($z in $Assets) { $ta += @('--assets', $z) }
& "$Build\Release\codol_bsp_tests.exe" @ta | Select-Object -Last 3
if ($LASTEXITCODE) { $failed++ }

$views = @{ box_room = '-450,-450,200;20,45'; sky_room = '-450,-450,200;20,45'; two_rooms = '-900,-250,100;5,10'; content = '-480,-230,150;15,30'; big = '-2900,-2900,150;10,45'
            terrain = '60,-460,300;28,105'; modder = '-380,-300,170;12,48'; yard = '-980,-720,330;22,38' }
$maps = @('box_room', 'sky_room', 'two_rooms', 'content', 'big', 'terrain')
if ($Assets.Count) { $maps += @('modder', 'yard') }
$donors = @(); foreach ($z in $Assets) { $donors += @('--assets', $z) }
foreach ($m in $maps) {
  $name = "mp_e2e_$($m -replace '_', '')"
  & $Bsp --map "$out\tests\maps\$m.map" --exe "$Game\C1.exe" --install $Game --out "$out\ff\$name.ff" --cache $Scratch --report "$out\ff\$name.report.txt" @donors | Select-Object -Last 1
  if ($LASTEXITCODE) { $failed++; continue }
  $v = & $Linker --verify "$out\ff\$name.ff" --exe "$Game\C1.exe" | Select-Object -Last 1
  if ($v -notmatch 'identical') { "verify $name FAILED: $v"; $failed++ } else { "verify ${name}: $($v.Trim())" }
  & $Bsp --check "$out\ff\$name.ff" --exe "$Game\C1.exe" | Select-Object -Last 1
  if ($LASTEXITCODE) { $failed++ }
  $pa = $views[$m].Split(';')
  $env:CODOL_VIEW_STRICT_CELLS = '1'
  & $View render --game $Game --map $name --zone-dirs "$out\ff" --out "$out\png\$name.png" --size 960x540 --pos $pa[0] --angles $pa[1] --stats |
    Select-String -Pattern 'world surfaces|cells:' | ForEach-Object { '  ' + ($_.Line.Trim() -replace ', \d+ of \d+ static.*', '') }
  Remove-Item Env:\CODOL_VIEW_STRICT_CELLS
}

if ($Server) {
  $H = "$PSScriptRoot\harness"
  if ($Assets.Count) {
    # the yard, as a mapper would ship it: zone with minimap material, load zone, level script in the server's overlay
    & "$PSScriptRoot\..\tools\package_map.ps1" -Map "$out\tests\maps\yard.map" -Name mp_e2eserver -Out "$out\yard" -Assets $Assets -Game $Game -Cache $Scratch -Bsp $Bsp | Select-Object -Last 1
    & "$H\stop_server.ps1" -Scratch $Scratch | Out-Null
    & "$H\make_base.ps1" -Scratch $Scratch -Game $Game | Out-Null
    Copy-Item "$out\yard\zone\mp_e2eserver.ff" "$Scratch\base\client_shipRetail\chinese\mp_e2eserver.ff" -Force
    & "$H\start_server.ps1" -Variant e2e -Map mp_e2eserver -Bots $Bots -Scratch $Scratch -Overlay "$out\yard\scripts" -Port $Port | Out-Null
    Start-Sleep $Seconds
  } else {
    & "$H\compile_and_serve.ps1" -MapFile "$out\tests\maps\box_room.map" -Name mp_e2eserver -Variant e2e -Bots $Bots -Wait $Seconds -Scratch $Scratch -Game $Game -Port $Port | Select-Object -Last 3
  }
  $log = "$Scratch\run\e2e\home\main\qconsole.log"
  $players = ((& "$H\rcon.ps1" -Cmd 'status' -Scratch $Scratch -Port $Port) -split "`n" | Where-Object { $_ -match 'Training' }).Count
  $kills = (Get-Content "$Scratch\run\e2e\home\main\games_mp.log" | Select-String -Pattern ' K;' | Measure-Object).Count
  $installed = (Select-String -Path $log -Pattern 'CoDOL native collision installed' | Measure-Object).Count
  & "$H\stop_server.ps1" -Scratch $Scratch | Out-Null
  $errors = (Select-String -Path $log -Pattern 'script runtime error|Pathnode .* is in solid' | Measure-Object).Count
  "server: collision installed $installed, $players bots, $kills kills, $errors script / path node errors"
  if (-not $installed -or $players -lt $Bots -or $kills -lt 1 -or ($Assets.Count -and $errors)) { $failed++ }
}
if ($failed) { "run_e2e: $failed step(s) FAILED"; exit 1 }
'run_e2e: all steps passed'
