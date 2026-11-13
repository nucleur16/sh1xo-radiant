// sixth round sound test: the map's ambient track is a streamed alias of the project; more on request ( dvar c6snd )
main()
{
	maps\mp\mp_c6snd_fx::main();
	maps\mp\_load::main();
	if ( getDvar( "c6snd_auto" ) != "0" )
		ambientPlay( "c6_wind_amb" );
	level thread c6_sounds();
}

c6_sounds()
{
	wait 3;
	ent = spawn( "script_origin", ( 0, 120, 40 ) );
	setDvar( "c6snd", "" );
	setDvar( "c6snd_done", "ready" );
	for ( ;; )
	{
		wait 0.2;
		what = getDvar( "c6snd" );
		if ( what == "" )
			continue;
		setDvar( "c6snd", "" );
		if ( what == "amb" )
			ambientPlay( "c6_wind_amb" );
		else if ( what == "ambm" )
			ambientPlay( "c6_windm_amb" );
		else if ( what == "ambstop" )
			ambientStop( 0 );
		else if ( what == "ent" )
			ent playLoopSound( "c6_windm_ent" );
		else if ( what == "entstop" )
			ent stopLoopSound();
		else if ( what == "far" )
			ent.origin = ( 0, 120, 6000 );
		else if ( what == "near" )
			ent.origin = ( 0, 120, 40 );
		setDvar( "c6snd_done", what );
	}
}
