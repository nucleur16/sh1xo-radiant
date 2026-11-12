param([Parameter(Mandatory)][string]$MapFile, [Parameter(Mandatory)][string]$Name, [Parameter(Mandatory)][string]$Variant,
      [int]$Developer = 0, [int]$Bots = 0, [string]$MapCmd = 'map', [int]$Wait = 30, [string]$Gametype = 'dm', [string[]]$Extra = @(),
      [string]$Scratch = 'D:\CoD\iw3xo-radiant-codol\build-editor\bsptest', [string]$Game = 'D:\CoD\CODOL Builds\Call of Duty Online 1.1.1.13',
      [string]$Build)
# compiles a .map with codol_bsp, puts the fast file into the scratch base and starts the dedicated server on it
$ErrorActionPreference = 'Stop'
$H = $PSScriptRoot
if (-not $Build) { $Build = (Resolve-Path "$H\..\..\..\build\codol_bsp\Release").Path }
& "$H\stop_server.ps1" -Scratch $Scratch | Out-Null
& "$H\make_base.ps1" -Scratch $Scratch -Game $Game | Out-Null
$out = "$Scratch\out\$Variant"
New-Item -ItemType Directory -Force $out | Out-Null
& "$Build\codol_bsp.exe" --map $MapFile --exe "$Game\C1.exe" --install $Game --out "$out\$Name.ff" --cache $Scratch --report "$out\$Name.report.txt" @Extra | Select-Object -Last 4
if ($LASTEXITCODE) { throw 'compile failed' }
Copy-Item "$out\$Name.ff" "$Scratch\base\client_shipRetail\chinese\$Name.ff" -Force
& "$H\start_server.ps1" -Variant $Variant -Map $Name -Developer $Developer -Bots $Bots -MapCmd $MapCmd -Scratch $Scratch -Gametype $Gametype
Start-Sleep $Wait
$log = "$Scratch\run\$Variant\home\main\qconsole.log"
Select-String -Path $log -Pattern 'CoDOL native collision|common world installed|glass: installed|Server crashed|script runtime error|source decode failed|mount failed|could not be constructed|Error: No ' |
  Select-Object -First 12 | ForEach-Object { $_.Line }
