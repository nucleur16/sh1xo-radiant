param([string]$File, [switch]$Remove, [string]$Scratch = 'D:\CoD\iw3xo-radiant-codol\build-editor\bsptest',
      [string]$Client = 'D:\CoD\iw3xo-radiant-codol\build-editor\shadow_client', [string]$Game = 'D:\CoD\CODOL Builds\Call of Duty Online 1.1.1.13')
# A test archive ( iw_sound_NN.iwd ) into main\ of the server's base and of the shadow client - both: the server lists
# the archives it has as pure, the client only plays streamed files out of those. Real copies in the two shadow
# folders, never in the install. -Remove ( with the same -File, or without one: every archive the script put ) takes
# them out again; what it put is noted in $Scratch\put_iwd.txt. An archive name the install has is refused.
$ErrorActionPreference = 'Stop'
$note = "$Scratch\put_iwd.txt"
$put = @(); if (Test-Path $note) { $put = @(Get-Content $note | Where-Object { $_ }) }
$folders = "$Scratch\base\main", "$Client\main"
if ($Remove) {
  $names = if ($File) { @(Split-Path $File -Leaf) } else { $put }
  foreach ($name in $names) {
    if ($put -notcontains $name) { "not put by this script: $name"; continue }
    foreach ($f in $folders) { if (Test-Path "$f\$name") { Remove-Item "$f\$name" -Force; "removed $f\$name" } }
    $put = @($put | Where-Object { $_ -ne $name })
  }
  if ($put.Count) { Set-Content $note $put } else { Remove-Item $note -ErrorAction SilentlyContinue }
  return
}
if (-not $File -or -not (Test-Path $File)) { throw "no file $File" }
$name = Split-Path $File -Leaf
if (Test-Path "$Game\main\$name") { throw "$name is an archive of the install" }
foreach ($f in $folders) {
  $item = Get-Item $f -ErrorAction SilentlyContinue
  if (-not $item -or ($item.Attributes -band [IO.FileAttributes]::ReparsePoint)) { throw "$f is not a folder of its own ( run make_base.ps1 )" }
  Copy-Item $File "$f\$name" -Force
  "put $f\$name"
}
if ($put -notcontains $name) { Add-Content $note $name }
