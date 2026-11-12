param([string]$Scratch = 'D:\CoD\iw3xo-radiant-codol\build-editor\bsptest', [string]$Game = 'D:\CoD\CODOL Builds\Call of Duty Online 1.1.1.13',
      [string[]]$Only = @())
# How the game's lit shaders combine the two layers of the secondary lightmap and the primary lightmap: the box room is
# compiled with constant lightmaps ( CODOL_BSP_LM_TEST ) and rendered by codol_viewtool, which runs those shaders. The
# average colour of a patch of floor is printed per case.
$Build = (Resolve-Path "$PSScriptRoot\..\..\..\build\codol_bsp").Path
Add-Type -AssemblyName System.Drawing
$cases = [ordered]@{
  black        = '0 0 0 255 0 0 0 255 0'
  layer0_red   = '255 0 0 255 0 0 0 255 0'
  layer1_green = '0 0 0 255 0 255 0 255 0'
  both_grey    = '128 128 128 255 128 128 128 255 0'
  alpha0       = '128 128 128 0 128 128 128 0 0'
  primary_only = '0 0 0 255 0 0 0 255 255'
  l0_grey64    = '64 64 64 255 0 0 0 255 0'
  l1_grey64    = '0 0 0 255 64 64 64 255 0'
}
foreach ($k in $cases.Keys) {
  if ($Only.Count -and $Only -notcontains $k) { continue }
  $env:CODOL_BSP_LM_TEST = $cases[$k]
  & "$Build\Release\codol_bsp.exe" --map "$Scratch\maps\box_room.map" --exe "$Game\C1.exe" --install $Game --out "$Scratch\lm\$k\mp_boxroom.ff" --cache $Scratch --no-load | Out-Null
  & "$Build\codol_mapc\codol_view\Release\codol_viewtool.exe" render --game $Game --map mp_boxroom --zone-dirs "$Scratch\lm\$k" --out "$Scratch\png\lm_$k.png" --size 480x270 --pos 0,0,150 --angles 60,45 --no-shadows | Out-Null
  $bmp = New-Object System.Drawing.Bitmap "$Scratch\png\lm_$k.png"
  $sumR = 0; $sumG = 0; $sumB = 0; $n = 0
  for ($x = 200; $x -lt 280; $x += 8) { for ($y = 150; $y -lt 230; $y += 8) { $p = $bmp.GetPixel($x, $y); $sumR += $p.R; $sumG += $p.G; $sumB += $p.B; $n++ } }
  $bmp.Dispose()
  '{0,-14} lightmaps {1,-34} floor rgb {2,3} {3,3} {4,3}' -f $k, $cases[$k], [int]($sumR / $n), [int]($sumG / $n), [int]($sumB / $n)
}
Remove-Item Env:\CODOL_BSP_LM_TEST
