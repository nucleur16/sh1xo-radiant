param([Parameter(Mandatory)][string]$Name, [string]$Dir, [int]$Bots = 0, [int]$Wait = 45,
      [string]$Scratch = 'D:\CoD\iw3xo-radiant-codol\build-editor\bsptest')
# With server, lobby and client running: the seventh round's script folder ( refreshed, with a level script for <Name> )
# in front of the server's scripts, the map <Dir>\<Name>.ff into both zone folders ( without -Dir: the map that is there ),
# devmap, class and spawn. The server takes a new script folder over rcon ( codol_scriptOverlay ), no restart.
$H = (Resolve-Path "$PSScriptRoot\..").Path
& "$PSScriptRoot\make_overlay.ps1" -Maps $Name -Out "$Scratch\c7\overlay" | Out-Null
& "$H\rcon.ps1" -Cmd "set codol_scriptOverlay `"$Scratch\c7\overlay`"" -WaitMs 300 -Scratch $Scratch | Out-Null
& "$H\rcon.ps1" -Cmd 'codol_scriptOverlay' -WaitMs 300 -Scratch $Scratch | Out-Null
if (-not $Dir) { $Dir = "$Scratch\c7\none" }
& "$H\round6\change_map.ps1" -Dir $Dir -Name $Name -Bots $Bots -Wait $Wait -Scratch $Scratch
# god mode for the human player: with bots he is not client 0
$slot = ((& "$H\rcon.ps1" -Cmd 'status' -Scratch $Scratch) -split "`n" | Where-Object { $_ -match '^\s*\d+\s.*edtest' } | ForEach-Object { [int](($_.Trim() -split '\s+')[0]) } | Select-Object -First 1)
if ($null -ne $slot) { & "$H\rcon.ps1" -Cmd "dev_clientcmd $slot god" -WaitMs 400 -Scratch $Scratch | Out-Null; "god for client $slot" }
((& "$H\rcon.ps1" -Cmd 'c7_done' -WaitMs 500 -Scratch $Scratch) -split "`n" | Where-Object { $_ -match 'is:' })
