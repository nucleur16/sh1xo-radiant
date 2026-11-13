#include maps\mp\_utility;
#using_animtree( "mp_c4fx" );

// seventh round: the level script of shipped mp_rust ( as the server has it ) with a project's ANIMATIONS in the
// edited map - two script models of the project, each playing one of its animations. What the script learns goes
// into the dvar c7anim.
main()
{

	maps\mp\mp_rust_precache::main();
	maps\createart\mp_rust_art::main();
	maps\mp\mp_rust_fx::main();

	precacheMpAnim( "codol_test_spin" );
	precacheMpAnim( "codol_test_lid" );
	precacheModel( "codol_test_lidbox" );
	precacheModel( "c4fx_spincrate" );

	maps\mp\_load::main();

	maps\mp\_compass::setupMiniMap( "compass_map_mp_rust" );

	setdvar( "compassmaxrange", "1400" );

	ambientPlay( "ambient_mp_duststorm" );

	game[ "attackers" ] = "allies";
	game[ "defenders" ] = "axis";

	thread killTrigger( (1080, 1645, -156 - 30), 40, 30 );

	level thread c7_anims();
}

c7_anims()
{
	wait 3;
	crate = spawn( "script_model", ( 1560, -30, -190 ) );
	crate setModel( "c4fx_spincrate" );
	crate scriptModelPlayAnim( "codol_test_spin" );
	box = spawn( "script_model", ( 1480, -30, -214 ) );
	box setModel( "codol_test_lidbox" );
	box scriptModelPlayAnim( "codol_test_lid" );
	setDvar( "c7anim", "spawned; lengths " + getAnimLength( %codol_test_spin ) + " " + getAnimLength( %codol_test_lid ) );
}
