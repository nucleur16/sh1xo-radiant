param([Parameter(Mandatory)][string]$Dvar, [Parameter(Mandatory)][string]$Prefix, [Parameter(Mandatory)][string[]]$Steps, [string]$View, [int]$SettleMs = 1500,
      [string]$Scratch = 'D:\CoD\iw3xo-radiant-codol\build-editor\bsptest')
# With a client spawned on a devmap whose level script listens to a dvar ( <Dvar>, answers in <Dvar>_done ): place the
# player once ( -View "x y z yaw pitch" ), then for every step set the dvar, wait and take a picture
# shots\<Prefix>_<n>_<step>_s.png. Prints what the script answered and the mean brightness of the left and the right
# half of the picture ( 0 .. 255 ), so that a change is a number as well as a picture.
$H = (Resolve-Path "$PSScriptRoot\..").Path
Add-Type -AssemblyName System.Drawing
$Steps = @($Steps | ForEach-Object { $_ -split ';' } | Where-Object { $_.Trim() } | ForEach-Object { $_.Trim() })
if ($View) { & "$H\rcon.ps1" -Cmd "dev_clientcmd 0 setviewpos $View" -WaitMs 400 -Scratch $Scratch | Out-Null; Start-Sleep -Milliseconds 2500 }
$n = 0
foreach ($s in $Steps) {
  $n++
  if ($s -ne '-') { & "$H\rcon.ps1" -Cmd "set $Dvar $s" -WaitMs 300 -Scratch $Scratch | Out-Null }
  Start-Sleep -Milliseconds $SettleMs
  $done = ((& "$H\rcon.ps1" -Cmd "${Dvar}_done" -WaitMs 500 -Scratch $Scratch) -split "`n" | Where-Object { $_ -match 'is:' }) -replace '^.*is: "([^"^]*).*$', '$1'
  $name = '{0}_{1}_{2}' -f $Prefix, $n, ($s -replace '[^a-z0-9]', '')
  $file = & "$H\shot.ps1" -Name $name -Scratch $Scratch
  $text = ''
  if ($file -and (Test-Path $file)) {
    $bmp = New-Object System.Drawing.Bitmap $file
    $sum = @(0.0, 0.0), @(0.0, 0.0), @(0.0, 0.0); $cnt = 0, 0
    for ($x = 8; $x -lt $bmp.Width; $x += 16) { for ($y = [int]($bmp.Height * 0.35); $y -lt $bmp.Height; $y += 16) {
      $p = $bmp.GetPixel($x, $y); $half = [int]($x -ge $bmp.Width / 2)
      $sum[0][$half] += $p.R; $sum[1][$half] += $p.G; $sum[2][$half] += $p.B; $cnt[$half]++ } }
    $bmp.Dispose()
    $text = 'left rgb {0,3:F0} {1,3:F0} {2,3:F0}   right rgb {3,3:F0} {4,3:F0} {5,3:F0}' -f ($sum[0][0] / $cnt[0]), ($sum[1][0] / $cnt[0]), ($sum[2][0] / $cnt[0]), ($sum[0][1] / $cnt[1]), ($sum[1][1] / $cnt[1]), ($sum[2][1] / $cnt[1])
  }
  '{0,-8} {1}   script: {2}   {3}' -f $s, $text, $done, (Split-Path $file -Leaf)
}
