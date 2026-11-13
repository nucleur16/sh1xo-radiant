param([string]$File, [string]$Name, [switch]$Restore, [switch]$NoClient, [switch]$Patch, [string]$Scratch = 'D:\CoD\iw3xo-radiant-codol\build-editor\bsptest',
      [string]$Client = 'D:\CoD\iw3xo-radiant-codol\build-editor\shadow_client', [string]$Game = 'D:\CoD\CODOL Builds\Call of Duty Online 1.1.1.13')
# A zone into the zone folder of the server's base and of the shadow client ( client_shipRetail\chinese, or with
# -Patch client_shipRetail\patch of the shadow client only ).
#
# The stock zones of both folders are HARD LINKS to the install's files. Writing over one would write into the
# install - so a file that is in the way is removed first ( that removes the link, never the install's file ), and
# then the new file is copied: a real file of the shadow. -Restore -Name <zone>.ff takes the test file away and puts
# the link to the install's zone back ( when the install has one of that name ).
$ErrorActionPreference = 'Stop'
$sub = if ($Patch) { 'client_shipRetail\patch' } else { 'client_shipRetail\chinese' }
$folders = @()
if (-not $Patch) { $folders += "$Scratch\base\$sub" }
if (-not $NoClient) { $folders += "$Client\$sub" }
if (-not $Name) { $Name = Split-Path $File -Leaf }
foreach ($f in $folders) {
  $item = Get-Item $f -ErrorAction SilentlyContinue
  if (-not $item -or ($item.Attributes -band [IO.FileAttributes]::ReparsePoint)) { throw "$f is a link into the install, not a folder of the shadow: nothing is put there" }
  $to = "$f\$Name"
  if (Test-Path $to) { Remove-Item $to -Force }
  if ($Restore) {
    if (Test-Path "$Game\$sub\$Name") { New-Item -ItemType HardLink -Path $to -Target "$Game\$sub\$Name" | Out-Null; "restored the link $to" } else { "removed $to" }
  } else {
    Copy-Item $File $to
    "put $to"
  }
}
