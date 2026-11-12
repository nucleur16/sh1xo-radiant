param([string[]]$Steps = @('ambstop', 'amb', 'ambstop', 'ambm', 'ambstop', 'ent', 'far', 'near', 'entstop'), [int]$Seconds = 5, [string]$Dvar = 'c6snd',
      [string]$Scratch = 'D:\CoD\iw3xo-radiant-codol\build-editor\bsptest')
# With a client in mp_c6snd: for every step set the dvar the level script listens to, wait a second and measure the
# client's audio session ( audio_peak.ps1 ). First line: what plays when the script is not touched.
$H = (Resolve-Path "$PSScriptRoot\..").Path
$Steps = @($Steps | ForEach-Object { $_ -split ';' } | Where-Object { $_.Trim() } | ForEach-Object { $_.Trim() })     # -File hands an array over as one string
'{0,-10} {1}' -f '( start )', (& "$H\audio_peak.ps1" -Seconds $Seconds)
foreach ($s in $Steps) {
  & "$H\rcon.ps1" -Cmd "set $Dvar $s" -WaitMs 300 -Scratch $Scratch | Out-Null
  Start-Sleep -Milliseconds 1500
  $done = ((& "$H\rcon.ps1" -Cmd "${Dvar}_done" -WaitMs 500 -Scratch $Scratch) -split "`n" | Where-Object { $_ -match 'is:' }) -replace '^.*is: "([^"^]*).*$', '$1'
  '{0,-10} {1}   ( script: {2} )' -f $s, (& "$H\audio_peak.ps1" -Seconds $Seconds), $done
  if (-not (Get-Process codoMP_client_shipRetail -ErrorAction SilentlyContinue)) { 'THE CLIENT IS GONE'; break }
}
