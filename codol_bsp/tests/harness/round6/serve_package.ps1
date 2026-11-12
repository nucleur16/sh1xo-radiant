param([Parameter(Mandatory)][string]$Package, [Parameter(Mandatory)][string]$Name, [string]$Variant, [int]$Bots = 0, [int]$Wait = 30, [int]$Developer = 0,
      [string]$Gametype = 'dm', [int]$Port = 17733, [switch]$NoClient,
      [string]$Scratch = 'D:\CoD\iw3xo-radiant-codol\build-editor\bsptest', [string]$Client = 'D:\CoD\iw3xo-radiant-codol\build-editor\shadow_client',
      [string]$Game = 'D:\CoD\CODOL Builds\Call of Duty Online 1.1.1.13')
# A package of package_map.ps1 ( zone\, scripts\, main\ ) into the test server's base and the shadow client, and the
# server started on it:
#   zone\*.ff                 -> base\client_shipRetail\chinese\ and ( unless -NoClient ) the shadow client's
#   main\zz_*_sounds.iwd      -> as iw_sound_NN.iwd ( encrypted, the first free number ) into main\ of both ( put_iwd.ps1 )
#   scripts\                  -> the server's script overlay
# Nothing goes into the install.
$ErrorActionPreference = 'Stop'
$H = (Resolve-Path "$PSScriptRoot\..").Path
$root = (Resolve-Path "$H\..\..\..").Path
if (-not $Variant) { $Variant = $Name }
& "$H\stop_server.ps1" -Scratch $Scratch | Out-Null
& "$H\make_base.ps1" -Scratch $Scratch -Game $Game
Get-ChildItem "$Package\zone" -Filter *.ff | ForEach-Object {
  Copy-Item $_.FullName "$Scratch\base\client_shipRetail\chinese\$($_.Name)" -Force
  if (-not $NoClient) { Copy-Item $_.FullName "$Client\client_shipRetail\chinese\$($_.Name)" -Force }
}
$archives = @(Get-ChildItem "$Package\main" -Filter 'zz_*.iwd' -ErrorAction SilentlyContinue)
if ($archives.Count) {
  $assetc = @("$root\build\codol_assets\Release\codol_assetc.exe", "$root\build-editor\Release\codol_assetc.exe") | Where-Object { Test-Path $_ } | Select-Object -First 1
  if (-not $assetc) { throw 'codol_assetc is not built' }
  & "$H\put_iwd.ps1" -Remove -Scratch $Scratch -Client $Client | Out-Null
  $number = 23
  while ($number -ge 4 -and ((Test-Path ("$Scratch\base\main\iw_sound_{0:d2}.iwd" -f $number)) -or (Test-Path ("$Client\main\iw_sound_{0:d2}.iwd" -f $number)))) { $number-- }
  if ($number -lt 4) { throw 'no free iw_sound_NN.iwd' }
  $iwd = "$Package\work\" + ('iw_sound_{0:d2}.iwd' -f $number)
  $a = @('--install', $Game, '--iwd', $iwd); foreach ($f in $archives) { $a += @('--iwd-add', $f.FullName) }
  & $assetc @a
  if ($LASTEXITCODE) { throw 'the sound archive could not be written' }
  & "$H\put_iwd.ps1" -File $iwd -Scratch $Scratch -Client $Client -Game $Game
}
& "$H\start_server.ps1" -Variant $Variant -Map $Name -Developer $Developer -Bots $Bots -Scratch $Scratch -Gametype $Gametype -Overlay "$Package\scripts" -Port $Port
Start-Sleep $Wait
$log = "$Scratch\run\$Variant\home\main\qconsole.log"
Select-String -Path $log -Pattern 'CoDOL native collision|common world installed|Server crashed|script runtime error|script compile error|source decode failed|mount failed|could not be constructed|Error: No |unknown sound alias|iw_sound_(0[4-9]|1|2)' |
  Select-Object -First 16 | ForEach-Object { $_.Line }
