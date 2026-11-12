param([string]$Scratch = 'D:\CoD\iw3xo-radiant-codol\build-editor\bsptest', [string]$Game = 'D:\CoD\CODOL Builds\Call of Duty Online 1.1.1.13',
      [switch]$Server, [int]$Bots = 8, [int]$Seconds = 90)
# End to end: the unit tests, the command line tool on every test map, codol_linker --verify and codol_bsp --check on
# every fast file, pictures with codol_viewtool, and ( -Server ) a run of the dedicated server with bots on the box room.
# Nothing is written outside $Scratch.
$ErrorActionPreference = 'Stop'
$Build = (Resolve-Path "$PSScriptRoot\..\..\build\codol_bsp").Path
$Bsp = "$Build\Release\codol_bsp.exe"
$Linker = "$Build\codol_mapc\codol_zone\Release\codol_linker.exe"
$View = "$Build\codol_mapc\codol_view\Release\codol_viewtool.exe"
$out = "$Scratch\e2e"
New-Item -ItemType Directory -Force "$out\ff", "$out\png" | Out-Null
$failed = 0

& "$Build\Release\codol_bsp_tests.exe" --out "$out\tests" --game $Game | Select-Object -Last 3
if ($LASTEXITCODE) { $failed++ }

$views = @{ box_room = '-450,-450,200;20,45'; sky_room = '-450,-450,200;20,45'; two_rooms = '-900,-250,100;5,10'; content = '-480,-230,150;15,30'; big = '-2900,-2900,150;10,45' }
foreach ($m in 'box_room', 'sky_room', 'two_rooms', 'content', 'big') {
  $name = "mp_e2e_$($m -replace '_', '')"
  & $Bsp --map "$out\tests\maps\$m.map" --exe "$Game\C1.exe" --install $Game --out "$out\ff\$name.ff" --cache $Scratch --report "$out\ff\$name.report.txt" | Select-Object -Last 1
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
  & "$H\compile_and_serve.ps1" -MapFile "$out\tests\maps\box_room.map" -Name mp_e2eserver -Variant e2e -Bots $Bots -Wait $Seconds -Scratch $Scratch -Game $Game | Select-Object -Last 3
  $log = "$Scratch\run\e2e\home\main\qconsole.log"
  $players = ((& "$H\rcon.ps1" -Cmd 'status' -Scratch $Scratch) -split "`n" | Where-Object { $_ -match 'Training' }).Count
  $kills = (Get-Content "$Scratch\run\e2e\home\main\games_mp.log" | Select-String -Pattern ' K;' | Measure-Object).Count
  $installed = (Select-String -Path $log -Pattern 'CoDOL native collision installed' | Measure-Object).Count
  & "$H\stop_server.ps1" -Scratch $Scratch | Out-Null
  "server: collision installed $installed, $players bots, $kills kills"
  if (-not $installed -or $players -lt $Bots -or $kills -lt 1) { $failed++ }
}
if ($failed) { "run_e2e: $failed step(s) FAILED"; exit 1 }
'run_e2e: all steps passed'
