param([Parameter(Mandatory)][string]$Dir, [Parameter(Mandatory)][string]$Name, [string]$Overlay, [int]$Bots = 0, [int]$Wait = 30, [switch]$Client,
      [string]$Scratch = 'D:\CoD\iw3xo-radiant-codol\build-editor\bsptest', [string]$Game = 'D:\CoD\CODOL Builds\Call of Duty Online 1.1.1.13')
# A compiled map of our own ( <Dir>\<Name>.ff and <Name>_load.ff, as codol_bsp writes them ) into the server's base and
# the shadow client, the server started on it ( -Overlay: the folder with its level script ), and with -Client the
# lobby ( map forced ) and the client started and taken into the match ( join_match.ps1 ).
$ErrorActionPreference = 'Stop'
$H = (Resolve-Path "$PSScriptRoot\..").Path
& "$H\stop_server.ps1" -Scratch $Scratch | Out-Null
& "$H\make_base.ps1" -Scratch $Scratch -Game $Game | Out-Null
foreach ($f in "$Name.ff", "${Name}_load.ff") { if (Test-Path "$Dir\$f") { & "$H\put_zone.ps1" -File "$Dir\$f" -Scratch $Scratch -Game $Game | Out-Null } }
$a = @{ Variant = "c6_$Name"; Map = $Name; Bots = $Bots; Scratch = $Scratch }
if ($Overlay) { $a.Overlay = $Overlay }
& "$H\start_server.ps1" @a
Start-Sleep $Wait
Select-String -Path "$Scratch\run\c6_$Name\home\main\qconsole.log" -Pattern 'CoDOL native collision|common world installed|Server crashed|script runtime error|script compile error|source decode failed|mount failed' |
  Select-Object -First 8 | ForEach-Object { $_.Line }
if ($Client) {
  & "$H\start_lobby.ps1" -Map $Name -Scratch $Scratch
  Start-Sleep 6
  & "$H\start_client.ps1" -Scratch $Scratch
  & "$H\join_match.ps1" -Map $Name -Scratch $Scratch
}
