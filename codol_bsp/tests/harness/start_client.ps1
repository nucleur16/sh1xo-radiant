param([string]$Client = 'D:\CoD\iw3xo-radiant-codol\build-editor\shadow_client', [string]$User = 'edtest',
      [string]$Scratch = 'D:\CoD\iw3xo-radiant-codol\build-editor\bsptest', [switch]$NoKernel33, [string]$Kernel33,
      [string]$Game = 'D:\CoD\CODOL Builds\Call of Duty Online 1.1.1.13')
# The game client from the link based copy of the install ( its dwauth.ini points at the lobby on 127.0.0.1 ).
# Unless -NoKernel33: the diagnostic Kernel33.dll ( build_kernel33.ps1 -> $Scratch\tools\Kernel33.dll, or -Kernel33 )
# is put next to the exe in place of the shadow's link to the install's Kernel33.dll. Kernel33 draws in the client's
# Direct3D 9 device anyway, and its diagnostic profile writes the back buffer of a frame, or what a frame drew into,
# when asked through a file ( k33_shot.req, k33_stats.req; "+set k33_shotFile 1" ): shot.ps1 and frame_stats.ps1 get
# what the game renders also while the display is off. stop_all.ps1 puts the link back.
#
# The shadow's Kernel33.dll is a HARD LINK to the install's file: writing over it would write into the install. The
# link is removed first ( that removes the link, never the install's file ), then the harness's dll is copied: a real
# file of the shadow, marked by Kernel33.dll.harness next to it.
$ErrorActionPreference = 'Stop'
if (Get-Process codoMP_client_shipRetail -ErrorAction SilentlyContinue) { throw 'a client is running' }
. "$PSScriptRoot\kernel33_shadow.ps1"
if (-not $NoKernel33) {
  if (-not $Kernel33) { $Kernel33 = "$Scratch\tools\Kernel33.dll" }
  if (-not (Test-Path $Kernel33)) { throw "no ${Kernel33}: run build_kernel33.ps1" }
  Set-HarnessKernel33 -Client $Client -Dll $Kernel33
} else { Restore-InstallKernel33 -Client $Client -Game $Game }
Remove-Item "$Client\k33_shot.req", "$Client\k33_stats.req" -ErrorAction SilentlyContinue
$a = @('+set','969','100','+set','dw_username',$User,'+set','com_maxfps','0','+set','ui_unlock_allplaylists','1','+set','ui_unlockallloadout','1',
       '+969','100','+set','1848','0','+set','r_vsync','0')
if (-not $NoKernel33) { $a += @('+set','k33_shotFile','1') }
$p = Start-Process -FilePath "$Client\codoMP_client_shipRetail.exe" -ArgumentList $a -WorkingDirectory $Client -PassThru
$p.Id | Set-Content "$Scratch\client.pid"
"client pid $($p.Id)"
