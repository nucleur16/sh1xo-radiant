param([Parameter(Mandatory)][string]$Map, [Parameter(Mandatory)][string]$Out, [string]$AnimTree = 'codol_test', [string[]]$Anims = @('codol_test_spin', 'codol_test_lid'),
      [string]$Model = '', [string[]]$Notes = @('codol_test_spin=codol_half', 'codol_test_lid=lid_open'), [string[]]$Fx = @(), [string[]]$Sounds = @(), [string]$LoopSound = '',
      [string]$Ambient = '', [string]$Origin = '0 0 64', [int]$Delay = 5, [string]$AnimTreeSource = '')
# Writes a script overlay ( codol_bsp\tests\harness\start_server.ps1 -Overlay ) that uses a modder's own animations,
# effects and sound aliases from the level script of a map, the way a mod does:
#
#   <Out>\animtrees\<AnimTree>.atr     the animtree ( one animation name per line ): the server reads animtrees from its
#                                      script folders, like the level script
#   <Out>\maps\mp\<Map>.gsc            #using_animtree( "<AnimTree>" ); a script_model that plays every animation
#                                      ( setAnim, and precacheMpAnim / scriptModelPlayAnim as the
#                                      multiplayer scripts do it ); loadfx + playfx for every -Fx; playSound /
#                                      playLoopSound / ambientPlay for the aliases
#
# Every step sets a dvar assetprobe_<step> ( read with rcon.ps1 -Cmd assetprobe_<step> ) and writes a line
# "ASSETPROBE <step> <value>" to games_mp.log: the length of an animation as the server's XAnimParts has it, the time
# of a note, the id loadfx returns. A dedicated server draws and plays nothing: what this shows is that the names
# resolve and the script runs through every call without a script error.
$ErrorActionPreference = 'Stop'
New-Item -ItemType Directory -Force "$Out\maps\mp", "$Out\animtrees" | Out-Null
if ($AnimTreeSource) { Copy-Item $AnimTreeSource "$Out\animtrees\$AnimTree.atr" -Force }
else { Set-Content -Path "$Out\animtrees\$AnimTree.atr" -Value ($Anims -join "`n") -Encoding ASCII }
$o = ($Origin.Trim() -split '\s+') -join ', '
$b = ''
if ($Anims.Count) { $b += "#using_animtree( `"$AnimTree`" );`n`n" }
$b += "main()`n{`n"
foreach ($f in $Fx) { $id = ($f -replace '[^A-Za-z0-9]', '_'); $b += "`tlevel._effect[ `"$id`" ] = loadfx( `"$f`" );`n" }
foreach ($a in $Anims) { $b += "`tprecacheMpAnim( `"$a`" );`n" }
if ($Model) { $b += "`tprecacheModel( `"$Model`" );`n" }
$b += "`tmaps\mp\_load::main();`n`tlevel thread codol_asset_probe();`n}`n`ncodol_asset_probe()`n{`n`twait $Delay;`n"
$b += "`tent = spawn( `"script_model`", ( $o ) );`n"
if ($Model) { $b += "`tent setModel( `"$Model`" );`n" }
$b += "`tcodol_step( `"spawn`", `"script_model`" );`n"
if ($Anims.Count) {
  # no useAnimTree: the multiplayer script VM has none. The server binds a script_model to the tree of the first
  # %anim a setAnim call names.
  $b += "`tcodol_step( `"animtree`", `"$AnimTree`" );`n"
  foreach ($a in $Anims) {
    $b += "`tcodol_step( `"length_$a`", getAnimLength( %$a ) );`n"
  }
  foreach ($n in $Notes) {
    $a, $note = $n.Split('=')
    $b += "`ttimes = getNotetrackTimes( %$a, `"$note`" );`n`tcodol_step( `"note_${a}`", times.size + `" at `" + times[ 0 ] );`n"
  }
  foreach ($a in $Anims) {
    $b += "`tent setAnim( %$a, 1, 0, 1 );`n`tcodol_step( `"setanim_$a`", `"playing`" );`n`twait 0.5;`n"
    $b += "`tent clearAnim( %$a, 0 );`n"
    $b += "`tent scriptModelPlayAnim( `"$a`" );`n`tcodol_step( `"mpanim_$a`", `"playing`" );`n`twait 0.5;`n"
  }
}
foreach ($f in $Fx) {
  $id = ($f -replace '[^A-Za-z0-9]', '_')
  $b += "`tplayfx( level._effect[ `"$id`" ], ( $o ) );`n`tcodol_step( `"fx_$id`", level._effect[ `"$id`" ] );`n"
}
foreach ($s in $Sounds) { $b += "`tent playSound( `"$s`" );`n`tcodol_step( `"playsound_$s`", `"played`" );`n" }
if ($LoopSound) { $b += "`tent playLoopSound( `"$LoopSound`" );`n`tcodol_step( `"playloopsound`", `"$LoopSound`" );`n`twait 1;`n`tent stopLoopSound();`n" }
if ($Ambient) { $b += "`tambientPlay( `"$Ambient`" );`n`tcodol_step( `"ambientplay`", `"$Ambient`" );`n`twait 1;`n`tambientStop( 1 );`n" }
$b += "`tcodol_step( `"done`", `"all steps ran`" );`n}`n`ncodol_step( step, value )`n{`n`tsetDvar( `"assetprobe_`" + step, value );`n`tlogPrint( `"ASSETPROBE `" + step + `" `" + value + `"\n`" );`n}`n"
Set-Content -Path "$Out\maps\mp\$Map.gsc" -Value $b -Encoding ASCII
"$Out\maps\mp\$Map.gsc"
