param([Parameter(Mandatory)][string[]]$Do, [string]$Prefix = 'c7', [int]$SettleMs = 1200, [switch]$Stats,
      [string]$Scratch = 'D:\CoD\iw3xo-radiant-codol\build-editor\bsptest')
# With a client spawned on a seventh round test map ( its level script runs c7_view.gsc ): a list of steps, each either
#   <command>            sent to the script through the dvar c7 ( view,x,y,z,pitch,yaw  box,x,y,z  clear  lset,..  lmove,.. )
#   shot=<name>          a picture shots\<Prefix>_<name>_s.png ( with -Stats also the frame's passes, <name>.stats.txt )
#   wait=<ms>
# Steps may be given as one string separated by ';'. Prints the script's answer to every command and, per picture, the
# mean brightness of its lower half ( 0 .. 255 ).
$H = (Resolve-Path "$PSScriptRoot\..").Path
Add-Type -AssemblyName System.Drawing
$Do = @($Do | ForEach-Object { $_ -split ';' } | Where-Object { $_.Trim() } | ForEach-Object { $_.Trim() })
foreach ($s in $Do) {
  if ($s -match '^wait=(\d+)$') { Start-Sleep -Milliseconds ([int]$Matches[1]); continue }
  if ($s -match '^shot=(.+)$') {
    $name = "${Prefix}_$($Matches[1])"
    $file = & "$H\shot.ps1" -Name $name -Scratch $Scratch
    $text = ''
    if ($file -and (Test-Path $file)) {
      $bmp = New-Object System.Drawing.Bitmap $file
      $sum = 0.0; $n = 0
      for ($x = 8; $x -lt $bmp.Width; $x += 16) { for ($y = [int]($bmp.Height / 2); $y -lt $bmp.Height; $y += 16) { $p = $bmp.GetPixel($x, $y); $sum += ($p.R + $p.G + $p.B) / 3; $n++ } }
      $bmp.Dispose()
      $text = 'lower half {0:F1}' -f ($sum / $n)
    }
    "$file   $text"
    if ($Stats) { & "$H\frame_stats.ps1" -Name $name -Scratch $Scratch | Select-Object -First 2 }
    continue
  }
  & "$H\rcon.ps1" -Cmd "set c7 $s" -WaitMs 300 -Scratch $Scratch | Out-Null
  Start-Sleep -Milliseconds $SettleMs
  $done = ((& "$H\rcon.ps1" -Cmd 'c7_done' -WaitMs 500 -Scratch $Scratch) -split "`n" | Where-Object { $_ -match 'is:' }) -replace '^.*is: "([^"^]*).*$', '$1'
  '{0,-40} -> {1}' -f $s, $done
}
