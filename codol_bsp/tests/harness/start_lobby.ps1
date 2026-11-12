param([string]$Map, [string]$Gametype = 'dm', [string]$Scratch = 'D:\CoD\iw3xo-radiant-codol\build-editor\bsptest',
      [string]$Lobby = 'D:\CoD\CODOL Builds\Call of Duty Online 1.1.1.13\CODOL\1.1.1.13_server\alpha0022', [string]$ZoneDir)
# The lobby emulator ( alpha0022 ), offline on 127.0.0.1, with its state in the scratch folder and the map forced.
# Without -Map nothing is forced: the lobby takes the map of the room the client creates ( the create-room list ).
# It only knows maps whose <map>.ff is in its zone folder - the install's client_shipRetail\chinese, or -ZoneDir
# ( CODOL_ZONE_DIR ); any other map of a room is replaced by its default map ( handoff\map_handoff.py ).
# The lobby's sources are only read ( no byte code is written into them ).
$ErrorActionPreference = 'Stop'
if (Get-NetTCPConnection -LocalPort 3074 -State Listen -ErrorAction SilentlyContinue) { throw 'TCP 3074 is in use ( a lobby is running )' }
New-Item -ItemType Directory -Force "$Scratch\state" | Out-Null
$env:CODOL_OFFLINE = '1'
$env:CODOL_LOBBY_HOST = '127.0.0.1'
$env:CODOL_STATE_DIR = "$Scratch\state"
if ($Map) { $env:CODOL_FORCE_MAP = $Map; $env:CODOL_FORCE_GAMETYPE = $Gametype }
else { Remove-Item Env:\CODOL_FORCE_MAP, Env:\CODOL_FORCE_GAMETYPE -ErrorAction SilentlyContinue }
$env:CODOL_MATCH_RCON_PASSWORD = (Get-Content "$Scratch\rcon.txt" -Raw).Trim()
$env:CODOL_MATCH_RCON_HOST = '127.0.0.1'
$env:CODOL_MATCH_RCON_PORT = '17733'
$env:CODOL_INSTANCE_PORTS = '17733'
# the launcher would otherwise name a script that starts more servers; this one does not exist, so none is ever started
$env:CODOL_INSTANCE_SPAWN = 'spawn_disabled_by_test_harness.cmd'
$env:PYTHONDONTWRITEBYTECODE = '1'
if ($ZoneDir) { $env:CODOL_ZONE_DIR = $ZoneDir }
$p = Start-Process -FilePath 'cmd.exe' -ArgumentList '/c', 'call', "`"$Lobby\1-START-LOBBY.cmd`"" -WorkingDirectory $Lobby -WindowStyle Hidden `
     -RedirectStandardOutput "$Scratch\lobby.out.log" -RedirectStandardError "$Scratch\lobby.err.log" -PassThru
$p.Id | Set-Content "$Scratch\lobby.pid"
"lobby pid $($p.Id) map $Map gametype $Gametype"
