param([string]$Map, [int]$MenuWait = 70, [string]$Scratch = 'D:\CoD\iw3xo-radiant-codol\build-editor\bsptest',
      [string]$InputScript = 'D:\CoD\iw3xo-radiant-codol\build-editor\clienttest\input.ps1')
# Takes a freshly started client ( start_client.ps1; server and lobby running ) into the match and spawns:
# Multiplayer -> Start -> Search -> Ready, then ( -Map ) devmap <Map> over rcon with the time and score limits off -
# setviewpos, god .. only work on a devmap - and Esc -> Custom Class 1 -> Resume Game.
# The clicks are real mouse clicks at client coordinates of the 1920 x 1080 game window, which has to be in front.
# Pictures of the steps go to shots\join_*.png ( look at them when it does not get there ).
$H = $PSScriptRoot
function Step([string]$click, [string]$keys, [int]$wait, [string]$shot) {
  & "$H\focus.ps1" | Out-Null
  if ($click) { & $InputScript -Click $click -NoRestore | Out-Null }
  if ($keys) { & $InputScript -Keys $keys -NoRestore | Out-Null }
  Start-Sleep $wait
  if ($shot) { & "$H\shot.ps1" -Name $shot -Scratch $Scratch | Out-Null }
}
Start-Sleep $MenuWait
Step '960,540' '' 5 'join_1_multiplayer'      # Multiplayer
Step '1506,1020' '' 5 'join_2_start'          # Start
Step '1506,1020' '' 12 'join_3_search'        # Search: quick join puts the client into a room
Step '1506,1020' '' 30 'join_4_ready'         # Ready: the match loads
if ($Map) {
  foreach ($c in 'set scr_dm_timelimit 0', 'set scr_dm_scorelimit 0', 'set scr_war_timelimit 0', "set codol_sessionMap $Map", "devmap $Map") {
    & "$H\rcon.ps1" -Cmd $c -WaitMs 600 -Scratch $Scratch | Out-Null
  }
  Start-Sleep 40
}
Step '' 'ESC' 2 ''
Step '131,154' '' 2 ''                        # Custom Class 1
Step '1802,83' '' 6 ''                        # Resume Game
& "$H\rcon.ps1" -Cmd 'dev_clientcmd 0 god' -WaitMs 400 -Scratch $Scratch | Out-Null
& "$H\shot.ps1" -Name 'join_5_spawned' -Scratch $Scratch
((& "$H\rcon.ps1" -Cmd 'status' -Scratch $Scratch) -split "`n" | Where-Object { $_ -match '^map|edtest' })
