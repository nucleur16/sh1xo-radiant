param([Parameter(Mandatory)][string]$Map, [Parameter(Mandatory)][string[]]$Traces, [Parameter(Mandatory)][string]$Out, [int]$Delay = 5,
      [string[]]$Lines = @())
# Writes a level script maps\mp\<Map>.gsc into the overlay folder $Out ( start_server.ps1 -Overlay ) that runs
# bulletTrace for every "name=x1 y1 z1 x2 y2 z2" some seconds after the map started and prints the results to the
# server's games_mp.log as lines "BSPTRACE <name> fraction <f> pos (x, y, z) normal (..) surface <type>" and into the
# dvars bsptrace_<name> ( read them with rcon.ps1 -Cmd bsptrace_<name>; the log is not flushed while nobody plays ).
# bulletTrace is the engine's point trace with static models ( G_LocationalTrace -> SV_Trace( .., staticmodels 1 ) ):
# the only kind of trace that looks at static model collision; the box sweeps of player movement never do.
# -Lines: further script lines for main() ( e.g. a minimap setup ).
$dir = "$Out\maps\mp"
New-Item -ItemType Directory -Force $dir | Out-Null
$body = "main()`n{`n`tmaps\mp\_load::main();`n"
foreach ($l in $Lines) { $body += "`t$l`n" }
$body += "`tlevel thread codol_bsp_traces();`n}`n`ncodol_bsp_traces()`n{`n`twait $Delay;`n"
foreach ($t in $Traces) {
  $name, $spec = $t.Split('=')
  $n = $spec.Trim() -split '\s+'
  $body += "`tcodol_bsp_trace( `"$name`", ( $($n[0]), $($n[1]), $($n[2]) ), ( $($n[3]), $($n[4]), $($n[5]) ) );`n"
}
$body += "}`n`ncodol_bsp_trace( name, a, b )`n{`n`tt = bulletTrace( a, b, false, undefined );`n"
$body += "`ttext = `"fraction `" + t[ `"fraction`" ] + `" pos `" + t[ `"position`" ] + `" normal `" + t[ `"normal`" ] + `" surface `" + t[ `"surfacetype`" ];`n"
$body += "`tsetDvar( `"bsptrace_`" + name, text );`n`tlogPrint( `"BSPTRACE `" + name + `" `" + text + `"\n`" );`n}`n"
Set-Content -Path "$dir\$Map.gsc" -Value $body -Encoding ASCII
"$dir\$Map.gsc"
