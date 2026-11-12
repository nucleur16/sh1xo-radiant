// sixth round: a level script changes a PRIMARY_SCRIPTABLE light ( targetname c6lamp ) on request ( dvar c6lamp )
main()
{
	maps\mp\_load::main();
	level thread c6_lamp();
}

c6_lamp()
{
	wait 3;
	lamp = getEnt( "c6lamp", "targetname" );
	if ( !isDefined( lamp ) )
	{
		setDvar( "c6lamp_done", "NO LIGHT ENTITY" );
		return;
	}
	home = lamp.origin;
	full = lamp getLightIntensity();
	setDvar( "c6lamp", "" );
	setDvar( "c6lamp_done", "ready " + lamp.classname + " intensity " + full + " at " + home );
	for ( ;; )
	{
		wait 0.1;
		what = getDvar( "c6lamp" );
		if ( what == "" )
			continue;
		setDvar( "c6lamp", "" );
		if ( what == "off" )
			lamp setLightIntensity( 0 );
		else if ( what == "dim" )
			lamp setLightIntensity( full * 0.3 );
		else if ( what == "full" )
			lamp setLightIntensity( full );
		else if ( what == "red" )
			lamp setLightColor( ( 1, 0.15, 0.1 ) );
		else if ( what == "green" )
			lamp setLightColor( ( 0.2, 1, 0.2 ) );
		else if ( what == "white" )
			lamp setLightColor( ( 1, 1, 1 ) );
		else if ( what == "narrow" )
			lamp setLightFov( 40, 20 );
		else if ( what == "wide" )
			lamp setLightFov( 90, 40 );
		else if ( what == "move" )
			lamp.origin = home + ( 0, 250, 0 );
		else if ( what == "slide" )
			lamp moveTo( home + ( 0, -250, 0 ), 2 );
		else if ( what == "home" )
			lamp.origin = home;
		else if ( what == "blink" )
			lamp thread c6_blink( full );
		else if ( what == "steady" )
		{
			lamp notify( "c6_steady" );
			lamp setLightIntensity( full );
		}
		setDvar( "c6lamp_done", what + " intensity " + lamp getLightIntensity() + " at " + lamp.origin );
	}
}

c6_blink( full )
{
	self endon( "c6_steady" );
	for ( ;; )
	{
		self setLightIntensity( 0 );
		wait 0.4;
		self setLightIntensity( full );
		wait 0.4;
	}
}
