param([string]$Click, [string]$Keys, [int]$Before = 0, [int]$Wait = 3, [string]$Shot, [switch]$Double, [string]$Scratch = 'D:\CoD\iw3xo-radiant-codol\build-editor\bsptest',
      [string]$InputScript = 'D:\CoD\iw3xo-radiant-codol\build-editor\clienttest\input.ps1')
# One step in the client's menus: ( wait -Before seconds, ) bring the game window to the front, click at client
# coordinates of the 1920 x 1080 window ( a picture <name>_s.png is 1600 wide: its coordinates times 1.2 ) and / or
# press keys, wait, take a picture shots\<Shot>_s.png. Only the game window gets input.
$H = (Resolve-Path "$PSScriptRoot\..").Path
if ($Before) { Start-Sleep $Before }
if ($Click -or $Keys) { & "$H\focus.ps1" | Out-Null }
if ($Click) { if ($Double) { & $InputScript -Click $Click -NoRestore -Double | Out-Null } else { & $InputScript -Click $Click -NoRestore | Out-Null } }
if ($Keys) { & $InputScript -Keys $Keys -NoRestore | Out-Null }
Start-Sleep $Wait
if ($Shot) { & "$H\shot.ps1" -Name $Shot -Scratch $Scratch }
