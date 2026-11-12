param([string]$Scratch = 'D:\CoD\iw3xo-radiant-codol\build-editor\bsptest', [string]$Game = 'D:\CoD\CODOL Builds\Call of Duty Online 1.1.1.13',
      [string]$Wav, [string]$Mp3)
# The sound test of the sixth round: a project of three streamed aliases ( a .wav, an .mp3 ), built with codol_assetc,
# packaged with the box room as mp_c6snd. The level script plays c6_wind_amb as the map's ambient track.
# -Wav / -Mp3: the two sound files ( any stereo .wav and .mp3; default: those of the fifth round's scratch ).
$ErrorActionPreference = 'Stop'
$H = (Resolve-Path "$PSScriptRoot\..").Path
$root = (Resolve-Path "$H\..\..\..").Path
$S = "$Scratch\c6\snd"
if (-not $Wav) { $Wav = "$Scratch\c5\sndsrc\wind.wav" }
if (-not $Mp3) { $Mp3 = "$Scratch\c5\sndsrc\sound\c5\wind.mp3" }
New-Item -ItemType Directory -Force "$S\sound\c6" | Out-Null
Copy-Item "$PSScriptRoot\snd\*" $S -Recurse -Force
Copy-Item $Wav "$S\sound\c6\wind.wav" -Force
Copy-Item $Mp3 "$S\sound\c6\windm.mp3" -Force
$assetc = @("$root\build\codol_assets\Release\codol_assetc.exe", "$root\build-editor\Release\codol_assetc.exe") | Where-Object { Test-Path $_ } | Select-Object -First 1
& $assetc --install $Game --out "$S\c6snd_assets.ff" --matlib "$Scratch\codol_matlib.txt" --soundalias "$S\soundaliases\c6.csv" --iwd "$S\zz_c6snd_sounds.iwd"
if ($LASTEXITCODE) { throw 'the sound project did not build' }
& "$root\codol_bsp\tools\package_map.ps1" -Map "$Scratch\maps\box_room.map" -Name mp_c6snd -Out "$Scratch\c6\pkg_snd" -Assets "$S\c6snd_assets.ff" -Project $S -Cache $Scratch -Game $Game
