// Seventh round: what the test maps' level scripts share. The harness talks to it through the dvar c7 ( rcon:
// set c7 <word>,<number>,.. ) and reads the answer from c7_done:
//   view,x,y,z,pitch,yaw[,hold]   the human player there, looking that way ( this server's setviewpos keeps the angles );
//                                 hold: linked to a point there, so that he can look down from the air
//   box,x,y,z[,yaw]               a script model ( the plastic case ) there: something that throws a shadow
//   clear                         the boxes go
//   lset,<targetname>,x,y,z       a light entity's .origin
//   lmove,<targetname>,x,y,z,t    moveTo on the light entity, t seconds
//   llink,<targetname>,x,y,z,t    the light linked to a script_origin, and moveTo on that
//   lint,<targetname>,v           setLightIntensity
//   lget,<targetname>             only the answer: class, intensity, origin
run()
{
	precacheModel( "com_plasticcase_green_big_us_dirt" );
	level.c7_boxes = [];
	wait 2;
	setDvar( "c7", "" );
	setDvar( "c7_done", "ready" );
	for ( ;; )
	{
		wait 0.05;
		what = getDvar( "c7" );
		if ( what == "" )
			continue;
		setDvar( "c7", "" );
		t = strTok( what, "," );
		answer = what;
		if ( t[ 0 ] == "view" && t.size >= 6 )
		{
			who = c7_human();
			if ( isDefined( who ) )
			{
				who unlink();
				who setOrigin( ( float( t[ 1 ] ), float( t[ 2 ] ), float( t[ 3 ] ) ) );
				who setPlayerAngles( ( float( t[ 4 ] ), float( t[ 5 ] ), 0 ) );
				if ( t.size >= 7 && t[ 6 ] == "hold" )
				{
					if ( !isDefined( level.c7_anchor ) )
						level.c7_anchor = spawn( "script_origin", who.origin );
					level.c7_anchor.origin = who.origin;
					who linkTo( level.c7_anchor );
					wait 0.1;
					who setPlayerAngles( ( float( t[ 4 ] ), float( t[ 5 ] ), 0 ) );		// linking turns the player
				}
				answer = "view " + who.origin + " " + who getPlayerAngles();
			}
			else
				answer = "view: no human player";
		}
		else if ( t[ 0 ] == "box" && t.size >= 4 )
		{
			box = spawn( "script_model", ( float( t[ 1 ] ), float( t[ 2 ] ), float( t[ 3 ] ) ) );
			box setModel( "com_plasticcase_green_big_us_dirt" );
			if ( t.size >= 5 )
				box.angles = ( 0, float( t[ 4 ] ), 0 );
			level.c7_boxes[ level.c7_boxes.size ] = box;
			answer = "box " + level.c7_boxes.size + " at " + box.origin;
		}
		else if ( t[ 0 ] == "clear" )
		{
			for ( i = 0; i < level.c7_boxes.size; i++ )
				level.c7_boxes[ i ] delete();
			level.c7_boxes = [];
		}
		else if ( t[ 0 ] == "lset" || t[ 0 ] == "lmove" || t[ 0 ] == "llink" || t[ 0 ] == "lint" || t[ 0 ] == "lget" )
		{
			lamp = getEnt( t[ 1 ], "targetname" );
			if ( !isDefined( lamp ) )
				answer = t[ 0 ] + ": no entity " + t[ 1 ];
			else
			{
				if ( t[ 0 ] == "lset" )
					lamp.origin = ( float( t[ 2 ] ), float( t[ 3 ] ), float( t[ 4 ] ) );
				else if ( t[ 0 ] == "lmove" )
					lamp moveTo( ( float( t[ 2 ] ), float( t[ 3 ] ), float( t[ 4 ] ) ), float( t[ 5 ] ) );
				else if ( t[ 0 ] == "llink" )
				{
					if ( !isDefined( level.c7_mover ) )
					{
						level.c7_mover = spawn( "script_origin", lamp.origin );
						lamp linkTo( level.c7_mover );
					}
					level.c7_mover moveTo( ( float( t[ 2 ] ), float( t[ 3 ] ), float( t[ 4 ] ) ), float( t[ 5 ] ) );
				}
				else if ( t[ 0 ] == "lint" )
					lamp setLightIntensity( float( t[ 2 ] ) );
				answer = t[ 0 ] + " " + lamp.classname + " intensity " + lamp getLightIntensity() + " at " + lamp.origin;
			}
		}
		setDvar( "c7_done", answer );
	}
}

c7_human()
{
	players = getEntArray( "player", "classname" );
	for ( i = 0; i < players.size; i++ )
	{
		if ( players[ i ].name == "edtest" )
			return players[ i ];
	}
	return undefined;
}
