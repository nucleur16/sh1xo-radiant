param([Parameter(Mandatory)][string]$Zone, [Parameter(Mandatory)][string]$Name, [string]$Scripts, [string]$Variant, [int]$Bots = 0, [int]$Wait = 35, [switch]$Restore,
      [string]$Scratch = 'D:\CoD\iw3xo-radiant-codol\build-editor\bsptest', [string]$Client = 'D:\CoD\iw3xo-radiant-codol\build-editor\shadow_client',
      [string]$Game = 'D:\CoD\CODOL Builds\Call of Duty Online 1.1.1.13')
# An edited SHIPPED map ( codol_mapc ) on the test server and in the shadow client: the zone replaces the link to the
# stock zone in both shadow zone folders ( put_zone.ps1: the link is removed first, the install's file is never
# written ), -Scripts is the folder in front of all of the server's scripts ( the project's level script ).
# -Restore: the links to the stock zone come back ( do this after the test ).
$ErrorActionPreference = 'Stop'
$H = (Resolve-Path "$PSScriptRoot\..").Path
if ($Restore) { & "$H\put_zone.ps1" -Name "$Name.ff" -Restore -Scratch $Scratch -Client $Client -Game $Game; return }
if (-not $Variant) { $Variant = "c6_$Name" }
& "$H\stop_server.ps1" -Scratch $Scratch | Out-Null
& "$H\make_base.ps1" -Scratch $Scratch -Game $Game
& "$H\put_zone.ps1" -File $Zone -Name "$Name.ff" -Scratch $Scratch -Client $Client -Game $Game
$a = @{ Variant = $Variant; Map = $Name; Bots = $Bots; Scratch = $Scratch }
if ($Scripts) { $a.NativeOverlay = $Scripts }
& "$H\start_server.ps1" @a
Start-Sleep $Wait
$log = "$Scratch\run\$Variant\home\main\qconsole.log"
Select-String -Path $log -Pattern 'CoDOL native collision|common world installed|Server crashed|script runtime error|script compile error|source decode failed|mount failed|could not be constructed|Error: No |unknown sound alias|not in any mounted zone' |
  Select-Object -First 16 | ForEach-Object { $_.Line }
