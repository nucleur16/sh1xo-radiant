# Server-only check of zones made by codol_assetc: the dedicated server ( CoD4x_Server codol_dedrun ) mounts them
# with its own decoder ( every asset is validated ) and `codol_checkmodel` reports what it makes of the models.
# No client, no lobby; loopback, port 17756. Follows build-editor\clienttest\start_server.ps1.
#
#   run_server_check.ps1 -Test <folder> -Zones a.ff,b.ff -Models name1,name2 [-Map mp_rust] [-Seconds 60]
#
# Without -Map the server only mounts the zones and checks the models, then quits. With -Map it starts that map on
# the shadow install ( put the zone to test into <Test>\shadow\client_shipRetail\chinese first ) and is stopped after
# -Seconds. Only the process started here is stopped.
param([Parameter(Mandatory)][string]$Test, [string[]]$Zones = @(), [string[]]$Models = @(), [string]$Map, [int]$Seconds = 45, [string]$Variant = 'run')
$ErrorActionPreference = 'Stop'
$S = "$Test\shadow"
$R = 'D:\CoD\CoD4x_Server'
$A = 'D:\CoD\CODOL Builds\Call of Duty Online 1.1.1.13\CODOL\1.1.1.13_server\alpha0022'
$port = 17756
if (Get-NetUDPEndpoint -LocalPort $port -ErrorAction SilentlyContinue) { throw "UDP $port is in use" }
$V = "$Test\$Variant"; $H = "$V\home"
New-Item -ItemType Directory -Force "$H\main", "$Test\state" | Out-Null
python "$R\tools\codol_stage_server_pack.py" --repo $R --home-main "$H\main"
if ($LASTEXITCODE) { throw 'stage failed' }
if (-not (Test-Path "$Test\rcon.txt")) { -join ((48..57) + (97..122) | Get-Random -Count 16 | ForEach-Object { [char]$_ }) | Set-Content "$Test\rcon.txt" -NoNewline }
$pw = Get-Content "$Test\rcon.txt" -Raw
$scripts = "$R\bin\home\codol_rawfiles"
$arguments = @(
 '+set','dedicated','2',
 '+set','fs_basepath',"`"$S`"",
 '+set','fs_homepath',"`"$H`"",
 '+set','codol_clientExe',"`"$S\C1.exe`"",
 '+set','net_ip','127.0.0.1',
 '+set','net_port',"$port",
 '+set','rcon_password',"`"$pw`"",
 '+set','logfile','2','+set','logrcon','1','+set','developer','0','+set','sv_fps','60',
 '+set','codol_scriptPath',"`"$scripts`"",
 '+set','codol_scriptOverlay',"`"$scripts`"",
 '+set','codol_scriptNativeOverlay',"`"$scripts`"",
 '+set','codol_securityKeyPath',"`"$Test\state\securitykey.bin`"",
 '+set','codol_admissionPath',"`"$Test\state\game_admissions.txt`"",
 '+set','codol_admissionRequired','0',
 '+set','codol_profilePath',"`"$A\resources`"",
 '+set','codol_zoneType','mp',
 '+set','g_gametype','dm',
 '+set','scr_codol_bots','0',
 '+set','sv_botDifficulty','recruit',
 '+codol_scriptPath','+codol_scriptOverlay','+codol_scriptNativeOverlay')
if ($Map) {
	$arguments += @('+set','codol_sessionMap',"`"$Map`"",'+map',$Map)
}
if ($Zones.Count) {
	$arguments += @('+codol_loadzones',"`"$S\C1.exe`"") + ($Zones | ForEach-Object { "`"$_`"" })
}
foreach ($m in $Models) { $arguments += @('+codol_checkmodel',$m) }
if (-not $Map) { $arguments += '+quit' }
$p = Start-Process -FilePath "$R\bin\codol_dedrun.exe" -ArgumentList $arguments -WorkingDirectory "$R\bin" -WindowStyle Hidden `
	-RedirectStandardOutput "$V\server.stdout.txt" -RedirectStandardError "$V\server.stderr.txt" -PassThru
$p.Id | Set-Content "$Test\server.pid"
if (-not $p.WaitForExit($Seconds * 1000)) {
	Stop-Process -Id $p.Id -Force
	"server pid $($p.Id) stopped after $Seconds s"
} else {
	"server pid $($p.Id) exited with code $($p.ExitCode)"
}
"log: $V\server.stdout.txt"
