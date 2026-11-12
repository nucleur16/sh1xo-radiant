param([Parameter(Mandatory)][string]$Dir, [Parameter(Mandatory)][string]$Name, [int]$Bots = -1, [int]$Wait = 45,
      [string]$Scratch = 'D:\CoD\iw3xo-radiant-codol\build-editor\bsptest', [string]$Game = 'D:\CoD\CODOL Builds\Call of Duty Online 1.1.1.13',
      [string]$InputScript = 'D:\CoD\iw3xo-radiant-codol\build-editor\clienttest\input.ps1')
# With server, lobby and client running: another compiled map of our own into both zone folders, devmap over rcon,
# class and spawn again. -Bots: scr_codol_bots for the new map.
$H = (Resolve-Path "$PSScriptRoot\..").Path
foreach ($f in "$Name.ff", "${Name}_load.ff") { if (Test-Path "$Dir\$f") { & "$H\put_zone.ps1" -File "$Dir\$f" -Scratch $Scratch -Game $Game | Out-Null } }
$cmds = @('set scr_dm_timelimit 0', 'set scr_dm_scorelimit 0', "set codol_sessionMap $Name")
if ($Bots -ge 0) { $cmds += "set scr_codol_bots $Bots" }
$cmds += "devmap $Name"
foreach ($c in $cmds) { & "$H\rcon.ps1" -Cmd $c -WaitMs 600 -Scratch $Scratch | Out-Null }
Start-Sleep $Wait
& "$H\focus.ps1" | Out-Null; & $InputScript -Keys 'ESC' -NoRestore | Out-Null; Start-Sleep 2
& "$H\focus.ps1" | Out-Null; & $InputScript -Click '131,154' -NoRestore | Out-Null; Start-Sleep 2
& "$H\focus.ps1" | Out-Null; & $InputScript -Click '1802,83' -NoRestore | Out-Null; Start-Sleep 6
& "$H\rcon.ps1" -Cmd 'dev_clientcmd 0 god' -WaitMs 400 -Scratch $Scratch | Out-Null
((& "$H\rcon.ps1" -Cmd 'status' -Scratch $Scratch) -split "`n" | Where-Object { $_ -match '^map|^ +\d+ ' })
