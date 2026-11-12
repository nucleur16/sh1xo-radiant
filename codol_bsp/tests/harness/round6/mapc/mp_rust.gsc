#include maps\mp\_utility;

// sixth round: the level script of shipped mp_rust ( as the server has it ) with what a project adds to the edited
// map - an effect, a loop sound and a one shot sound of the project, on request ( dvar c6mapc ) and by itself
main()
{

	maps\mp\mp_rust_precache::main();
	maps\createart\mp_rust_art::main();
	maps\mp\mp_rust_fx::main();

	level._effect[ "c6_sparks" ] = loadfx( "codol_test/spark_burst" );
	precacheModel( "codol_test_crate" );

	maps\mp\_load::main();

	maps\mp\_compass::setupMiniMap( "compass_map_mp_rust" );

	setdvar( "compassmaxrange", "1400" );

	if ( getDvar( "c6mapc_ambient" ) != "0" )
		ambientPlay( "ambient_mp_duststorm" );

	game[ "attackers" ] = "allies";
	game[ "defenders" ] = "axis";

	thread killTrigger( (1080, 1645, -156 - 30), 40, 30 );

	level thread c6_project();
}

c6_project()
{
	wait 3;
	// a model of the project spawned by script, the project's effect above it
	box = spawn( "script_model", ( 1560, -30, -214 ) );
	box setModel( "codol_test_crate" );
	box.angles = ( 0, 30, 0 );
	level thread c6_sparks( box.origin + ( 0, 0, 60 ) );
	setDvar( "c6mapc", "" );
	setDvar( "c6mapc_done", "ready" );
	for ( ;; )
	{
		wait 0.2;
		what = getDvar( "c6mapc" );
		if ( what == "" )
			continue;
		setDvar( "c6mapc", "" );
		if ( what == "hum" )
			box playLoopSound( "codol_test_hum" );
		else if ( what == "humstop" )
			box stopLoopSound();
		else if ( what == "beep" )
			box playSound( "codol_test_beep" );
		else if ( what == "ambstop" )
			ambientStop( 0 );
		else if ( what == "amb" )
			ambientPlay( "ambient_mp_duststorm" );
		setDvar( "c6mapc_done", what );
	}
}

c6_sparks( origin )
{
	for ( ;; )
	{
		playfx( level._effect[ "c6_sparks" ], origin );
		wait 1.5;
	}
}
