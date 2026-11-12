param([string]$Client = 'D:\CoD\iw3xo-radiant-codol\build-editor\shadow_client', [string]$User = 'edtest',
      [string]$Scratch = 'D:\CoD\iw3xo-radiant-codol\build-editor\bsptest')
# The game client from the link based copy of the install ( its dwauth.ini points at the lobby on 127.0.0.1 ).
$ErrorActionPreference = 'Stop'
if (Get-Process codoMP_client_shipRetail -ErrorAction SilentlyContinue) { throw 'a client is running' }
$a = @('+set','969','100','+set','dw_username',$User,'+set','com_maxfps','0','+set','ui_unlock_allplaylists','1','+set','ui_unlockallloadout','1',
       '+969','100','+set','1848','0','+set','r_vsync','0')
$p = Start-Process -FilePath "$Client\codoMP_client_shipRetail.exe" -ArgumentList $a -WorkingDirectory $Client -PassThru
$p.Id | Set-Content "$Scratch\client.pid"
"client pid $($p.Id)"
