param([string]$Client = 'D:\CoD\iw3xo-radiant-codol\build-editor\shadow_client', [string]$User = 'edtest',
      [string]$Scratch = 'D:\CoD\iw3xo-radiant-codol\build-editor\bsptest', [switch]$NoShim, [switch]$NoCmdFile)
# The game client from the link based copy of the install ( its dwauth.ini points at the lobby on 127.0.0.1 ).
# Unless -NoShim: the frame dump d3d9.dll ( d3d9_shot\build.ps1 -> $Scratch\tools\d3d9.dll ) is put next to the exe,
# so shot.ps1 gets the frames the game renders also while the display is off. Unless -NoCmdFile: Kernel33's test
# automation is on ( +set k33_cmdFile 1 ): client_cmd.ps1 runs console commands in the client.
$ErrorActionPreference = 'Stop'
if (Get-Process codoMP_client_shipRetail -ErrorAction SilentlyContinue) { throw 'a client is running' }
if (-not $NoShim) {
  if (-not (Test-Path "$Scratch\tools\d3d9.dll")) { throw "no $Scratch\tools\d3d9.dll: run d3d9_shot\build.ps1" }
  Copy-Item "$Scratch\tools\d3d9.dll" "$Client\d3d9.dll" -Force
} elseif (Test-Path "$Client\d3d9.dll") { Remove-Item "$Client\d3d9.dll" }
Remove-Item "$Client\codol_shot.req", "$Client\k33_cmd_*.txt" -ErrorAction SilentlyContinue
$a = @('+set','969','100','+set','dw_username',$User,'+set','com_maxfps','0','+set','ui_unlock_allplaylists','1','+set','ui_unlockallloadout','1',
       '+969','100','+set','1848','0','+set','r_vsync','0')
if (-not $NoCmdFile) { $a += @('+set', 'k33_cmdFile', '1') }
$p = Start-Process -FilePath "$Client\codoMP_client_shipRetail.exe" -ArgumentList $a -WorkingDirectory $Client -PassThru
$p.Id | Set-Content "$Scratch\client.pid"
"client pid $($p.Id)"
