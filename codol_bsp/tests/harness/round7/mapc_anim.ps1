param([string]$Project = 'D:\CoD\iw3xo-radiant-codol\build-editor\bsptest\c4\fxproj', [string]$Zone = 'c4fx_assets.ff', [string]$Tree = 'mp_c4fx',
      [string]$BaseMap = 'D:\CoD\iw3xo-radiant-codol\build-editor\Release\map_source\mp_rust.codol_base.map', [int]$Wait = 40, [switch]$Restore,
      [string]$Scratch = 'D:\CoD\iw3xo-radiant-codol\build-editor\bsptest', [string]$Game = 'D:\CoD\CODOL Builds\Call of Duty Online 1.1.1.13')
# Seventh round, "a project's animation in an edited shipped map": shipped mp_rust ( unedited: the map it was imported
# as ) with a project's zone carried in by codol_mapc --assets, the project's animlist appended to the map's and its
# animtree added as a rawfile; the dedicated server is started on it with a level script that plays the animations
# ( round7\mapc\mp_rust.gsc ). Prints what the server says about animlist, animtree and the script.
# The zone replaces the link to the stock zone in both shadow zone folders ( put_zone.ps1 ); -Restore puts the links
# back - do that after the test.
$ErrorActionPreference = 'Stop'
$H = (Resolve-Path "$PSScriptRoot\..").Path
if ($Restore) { & "$H\put_zone.ps1" -Name 'mp_rust.ff' -Restore -Scratch $Scratch -Game $Game; return }
$Build = (Resolve-Path "$H\..\..\..\build\codol_bsp").Path
$out = "$Scratch\c7\mapc"
New-Item -ItemType Directory -Force "$out\scripts\maps\mp" | Out-Null
Copy-Item "$PSScriptRoot\mapc\mp_rust.gsc" "$out\scripts\maps\mp\mp_rust.gsc" -Force
& "$Build\codol_mapc\Release\codol_mapc.exe" --base "$Game\client_shipRetail\chinese\mp_rust.ff" --exe "$Game\C1.exe" --map $BaseMap --baseline $BaseMap --out "$out\mp_rust.ff" `
  --report "$out\mp_rust.report.txt" --assets "$Project\zone\$Zone" --append-rawfile "animlists/mp_rust.animlist=$Project\generated\animlists\map.animlist" `
  --rawfile "animtrees/$Tree.atr=$Project\generated\animtrees\$Tree.atr" | Select-String 'assets:|container:|rawfile|wrote|error|failed' | ForEach-Object { $_.Line }
if ($LASTEXITCODE) { throw 'codol_mapc failed' }
& "$Build\codol_mapc\codol_zone\Release\codol_zonetool.exe" dump-rawfile "$out\mp_rust.ff" animlists/mp_rust.animlist --exe "$Game\C1.exe" --out "$out\mp_rust.animlist" | Out-Null
"animlists/mp_rust.animlist of the edited zone: $((Get-Content "$out\mp_rust.animlist" | Measure-Object -Line).Lines) lines, the last: $((Get-Content "$out\mp_rust.animlist" -Tail 3) -join ' | ')"
& "$H\round6\serve_stock.ps1" -Zone "$out\mp_rust.ff" -Name mp_rust -Scripts "$out\scripts" -Variant c7_mp_rust -Wait $Wait -Scratch $Scratch -Game $Game | Select-Object -Last 6
$log = "$Scratch\run\c7_mp_rust\home\main\qconsole.log"
Select-String -Path $log -Pattern 'animlist|animtree|script runtime error|script compile error|cannot play animation|not in any mounted zone' | Select-Object -First 20 | ForEach-Object { $_.Line }
(& "$H\rcon.ps1" -Cmd 'c7anim' -Scratch $Scratch) -split "`n" | Where-Object { $_ -match 'is:' }
