param([Parameter(Mandatory)][string]$Map, [Parameter(Mandatory)][string]$Out, [string]$Once = 'codol_test_beep', [string]$Loop = 'codol_test_hum',
      [string]$Ambient = 'codol_test_wind', [string]$Origin = '0 0 64', [int]$Delay = 5)
# Writes a level script maps\mp\<Map>.gsc into the overlay folder $Out ( codol_bsp\tests\harness\start_server.ps1
# -Overlay ) that plays sound aliases of a zone made by codol_assetc --soundalias some seconds after the map
# started: playSound and playLoopSound on a script_origin at $Origin, ambientPlay for the streamed alias. A
# dedicated server has no sound system: what it can show is that the script compiles and runs through every call
# without a script error. Each step sets a dvar ( read with rcon.ps1 -Cmd sndprobe_<step> ) and writes a line
# "SNDPROBE <step> <alias>" to games_mp.log. Hearing the sounds needs a client that has loaded the zone.
$dir = "$Out\maps\mp"
New-Item -ItemType Directory -Force $dir | Out-Null
$o = ($Origin.Trim() -split '\s+') -join ', '
$body = @"
main()
{
	maps\mp\_load::main();
	level thread codol_sound_probe();
}

codol_sound_probe()
{
	wait $Delay;
	emitter = spawn( "script_origin", ( $o ) );
	codol_sound_step( "spawn", "script_origin" );
	emitter playSound( "$Once" );
	codol_sound_step( "playsound", "$Once" );
	emitter playLoopSound( "$Loop" );
	codol_sound_step( "playloopsound", "$Loop" );
	ambientPlay( "$Ambient" );
	codol_sound_step( "ambientplay", "$Ambient" );
	wait 2;
	emitter stopLoopSound();
	ambientStop( 1 );
	codol_sound_step( "stopped", "$Loop" );
}

codol_sound_step( step, alias )
{
	setDvar( "sndprobe_" + step, alias );
	logPrint( "SNDPROBE " + step + " " + alias + "\n" );
}
"@
Set-Content -Path "$dir\$Map.gsc" -Value $body -Encoding ASCII
"$dir\$Map.gsc"
