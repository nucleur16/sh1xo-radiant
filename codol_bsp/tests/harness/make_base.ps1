param([string]$Scratch = 'D:\CoD\iw3xo-radiant-codol\build-editor\bsptest', [string]$Game = 'D:\CoD\CODOL Builds\Call of Duty Online 1.1.1.13')
# A link based copy of the install to run the dedicated server from: nothing is written into the real install.
# chinese\ holds hard links to the stock zones, so compiled maps can be put next to them.
# main\ is a folder of its own with hard links to the game's archives ( *.iwd ) and junctions to its sub folders, so
# that a test archive can be put next to them ( put_iwd.ps1 ): the server tells the client which archives are
# "pure" from what it finds there, and a client that plays a streamed sound out of an archive the server does not
# have ends with "Impure client detected". ( Until the sixth round main\ was one junction into the install. )
# Never delete recursively inside it ( junctions lead into the real install ): remove junctions with cmd /c rmdir.
$ErrorActionPreference = 'Stop'
$Base = "$Scratch\base"
New-Item -ItemType Directory -Force "$Base\client_shipRetail\chinese" | Out-Null
if (-not (Test-Path "$Base\C1.exe")) { New-Item -ItemType HardLink -Path "$Base\C1.exe" -Target "$Game\C1.exe" | Out-Null }
foreach ($j in 'usermaps') { if (-not (Test-Path "$Base\$j")) { cmd /c mklink /J "$Base\$j" "$Game\$j" | Out-Null } }
if (-not (Test-Path "$Base\client_shipRetail\patch")) { cmd /c mklink /J "$Base\client_shipRetail\patch" "$Game\client_shipRetail\patch" | Out-Null }
$main = Get-Item "$Base\main" -ErrorAction SilentlyContinue
if ($main -and ($main.Attributes -band [IO.FileAttributes]::ReparsePoint)) { cmd /c rmdir "$Base\main" | Out-Null }     # the junction itself, nothing behind it
New-Item -ItemType Directory -Force "$Base\main" | Out-Null
$archives = 0
Get-ChildItem "$Game\main" -File -Filter *.iwd | ForEach-Object {
  $to = "$Base\main\$($_.Name)"
  if (-not (Test-Path $to)) { New-Item -ItemType HardLink -Path $to -Target $_.FullName | Out-Null; $archives++ }
}
Get-ChildItem "$Game\main" -Directory | ForEach-Object {
  $to = "$Base\main\$($_.Name)"
  if (-not (Test-Path $to)) { cmd /c mklink /J "$to" "$($_.FullName)" | Out-Null }
}
$n = 0
Get-ChildItem "$Game\client_shipRetail\chinese" -File -Filter *.ff | ForEach-Object {
  $to = "$Base\client_shipRetail\chinese\$($_.Name)"
  if (-not (Test-Path $to)) { New-Item -ItemType HardLink -Path $to -Target $_.FullName | Out-Null; $n++ }
}
"base ${Base}: $n zones linked, $archives archives linked"
