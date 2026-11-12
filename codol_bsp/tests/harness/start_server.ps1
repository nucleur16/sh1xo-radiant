param([Parameter(Mandatory)][string]$Variant, [Parameter(Mandatory)][string]$Map, [string]$MapCmd = 'map', [int]$Developer = 0, [int]$Bots = 0,
      [string]$Scratch = 'D:\CoD\iw3xo-radiant-codol\build-editor\bsptest', [string]$Base, [int]$Port = 17733, [string]$Gametype = 'dm',
      [string]$Server = 'D:\CoD\CoD4x_Server', [string]$Scripts, [string]$Overlay,
      [string]$Lobby = 'D:\CoD\CODOL Builds\Call of Duty Online 1.1.1.13\CODOL\1.1.1.13_server\alpha0022')
# The dedicated server ( CoD4x_Server codol_dedrun ) on loopback with a scratch home, for a compiled map.
# Nothing is written outside $Scratch. The server's pid goes to $Scratch\server.pid.
$ErrorActionPreference = 'Stop'
if (-not $Base) { $Base = "$Scratch\base" }
if (-not $Scripts) { $Scripts = "$Server\bin\home\codol_rawfiles" }
# -Overlay: a folder whose files go in front of the server's scripts ( maps\mp\<map>.gsc: the level script, which the
# server reads from its script folders, never from a zone )
if (-not $Overlay) { $Overlay = $Scripts }
if (Get-NetUDPEndpoint -LocalPort $Port -ErrorAction SilentlyContinue) { throw "UDP $Port is in use" }
$V = "$Scratch\run\$Variant"; $H = "$V\home"
New-Item -ItemType Directory -Force "$H\main" | Out-Null
New-Item -ItemType Directory -Force "$Scratch\state" | Out-Null
python "$Server\tools\codol_stage_server_pack.py" --repo $Server --home-main "$H\main" | Out-Null
if ($LASTEXITCODE) { throw 'stage failed' }
$pwFile = "$Scratch\rcon.txt"
if (-not (Test-Path $pwFile)) { -join ((48..57) + (97..122) | Get-Random -Count 14 | ForEach-Object { [char]$_ }) | Set-Content $pwFile -NoNewline }
$pw = (Get-Content $pwFile -Raw).Trim()
$a = @(
 '+set','dedicated','2',
 '+set','fs_basepath',"`"$Base`"",
 '+set','fs_homepath',"`"$H`"",
 '+set','codol_clientExe',"`"$Base\C1.exe`"",
 '+set','net_ip','127.0.0.1',
 '+set','net_port',"$Port",
 '+set','rcon_password',"`"$pw`"",
 '+set','logfile','2','+set','logrcon','1','+set','developer',"$Developer",'+set','sv_fps','60',
 '+set','codol_scriptPath',"`"$Scripts`"",
 '+set','codol_scriptOverlay',"`"$Overlay`"",
 '+set','codol_scriptNativeOverlay',"`"$Scripts`"",
 '+set','codol_securityKeyPath',"`"$Scratch\state\securitykey.bin`"",
 '+set','codol_admissionPath',"`"$Scratch\state\game_admissions.txt`"",
 '+set','codol_admissionRequired','0',
 '+set','codol_profilePath',"`"$Lobby\resources`"",
 '+set','codol_zoneType','mp',
 '+set','codol_sessionMap',"`"$Map`"",
 '+set','g_gametype',$Gametype,
 '+set','scr_codol_bots',"$Bots",
 '+set','sv_botDifficulty','recruit',
 '+codol_scriptPath','+codol_scriptOverlay','+codol_scriptNativeOverlay',
 "+$MapCmd",$Map)
$p = Start-Process -FilePath "$Server\bin\codol_dedrun.exe" -ArgumentList $a -WorkingDirectory "$Server\bin" -WindowStyle Hidden `
     -RedirectStandardOutput "$V\server.stdout.txt" -RedirectStandardError "$V\server.stderr.txt" -PassThru
$p.Id | Set-Content "$Scratch\server.pid"
"server pid $($p.Id) variant $Variant map $Map log $V"
