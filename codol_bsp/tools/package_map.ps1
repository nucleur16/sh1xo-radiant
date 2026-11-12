param([Parameter(Mandatory)][string]$Map, [Parameter(Mandatory)][string]$Name, [Parameter(Mandatory)][string]$Out,
      [string]$Game = 'D:\CoD\CODOL Builds\Call of Duty Online 1.1.1.13', [string[]]$Assets = @(), [string]$Loadscreen, [string]$DisplayName,
      [string]$Environment = 'urban', [string]$Cache, [string[]]$Extra = @(), [string]$Bsp, [string]$Assetc)
# A map with everything around it, from a .map:
#
#   <Out>\zone\<Name>.ff            the map zone: world, collision, lights, the minimap material compass_map_<Name> with
#                                   its picture ( rendered by the compiler from above ), the effect scripts
#   <Out>\zone\<Name>_load.ff       the loading screen zone; with -Loadscreen <picture>: $levelbriefing / $defeatbackdrop
#                                   show that picture ( image loadscreen_<Name>, inline in the zone )
#   <Out>\scripts\maps\mp\<Name>.gsc ( + _fx, createfx )   the level script for the dedicated server's script folder
#   <Out>\presence\                 what the create-room list needs, and INSTALL.txt: where every file goes
#
# -Assets: codol_assetc project zones the map takes materials / models from. -Extra: further codol_bsp arguments.
# Nothing is written outside <Out>; the install is only read.
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path "$PSScriptRoot\..\..").Path
if (-not $Bsp) { $Bsp = "$root\build\codol_bsp\Release\codol_bsp.exe" }
if (-not $Assetc) { $Assetc = "$root\build\codol_assets\Release\codol_assetc.exe" }
foreach ($tool in $Bsp, $Assetc) { if (-not (Test-Path $tool)) { throw "$tool is not built" } }
if ($Name -notmatch '^[a-z0-9_]+$') { throw 'the map name may only hold a-z, 0-9 and _' }
if (-not $DisplayName) { $DisplayName = $Name }
if (-not $Cache) { $Cache = "$Out\work" }
$zone = "$Out\zone"; $work = "$Out\work"; $presence = "$Out\presence"
New-Item -ItemType Directory -Force $zone, $work, $presence, "$Out\scripts" | Out-Null

# 1. the map
$a = @('--map', $Map, '--exe', "$Game\C1.exe", '--install', $Game, '--out', "$zone\$Name.ff", '--name', $Name, '--cache', $Cache,
       '--report', "$work\$Name.report.txt", '--minimap', "$work\compass_map_$Name.tga", '--scripts', "$Out\scripts")
foreach ($z in $Assets) { $a += @('--assets', $z) }
& $Bsp @a @Extra | Select-Object -Last 2
if ($LASTEXITCODE) { throw 'codol_bsp failed' }

# 2. the minimap material, into the map zone
Set-Content "$work\minimap.mtl" -Encoding ASCII -Value @"
// the compass of ${Name}: the picture codol_bsp rendered
material "compass_map_$Name"
{
	type "2d"
	colorMap compass_map_$Name clamp
}
"@
& $Assetc --install $Game --base "$zone\$Name.ff" --name $Name --out "$zone\$Name.ff" --matlib "$Cache\codol_matlib.txt" `
  --image "compass_map_$Name=$work\compass_map_$Name.tga" --material "$work\minimap.mtl" | Select-String -Pattern 'compass_map|wrote|rror' | ForEach-Object { '  ' + $_.Line.Trim() }
if ($LASTEXITCODE) { throw 'adding the minimap material failed' }

# 3. the loading screen
if ($Loadscreen) {
  Set-Content "$work\load.mtl" -Encoding ASCII -Value @"
// the loading screen of $Name
material "`$levelbriefing"
{
	type "2d"
	colorMap loadscreen_$Name clamp
}

material "`$defeatbackdrop"
{
	type "2d"
	colorMap loadscreen_$Name clamp
}
"@
  Set-Content "$work\${Name}_load.txt" -Encoding ASCII -Value "codol_bsp $Name"
  & $Assetc --install $Game --name "${Name}_load" --out "$zone\${Name}_load.ff" --matlib "$Cache\codol_matlib.txt" `
    --image "loadscreen_$Name=$Loadscreen,nomips" --material "$work\load.mtl" --rawfile "${Name}_load=$work\${Name}_load.txt" |
    Select-String -Pattern 'loadscreen|levelbriefing|wrote|rror' | ForEach-Object { '  ' + $_.Line.Trim() }
  if ($LASTEXITCODE) { throw 'building the load zone failed' }
}

# 4. what the create-room list needs
${key} = 'USERMAPS_' + ($Name.ToUpper() -replace '[^A-Z0-9_]', '_')
$tokens = 'default', 'mainline', 'alpha2', 'alpha3', 'ce2', 'ce3', 'cb1', 'cb2', 'cb3'
$version = ($tokens | ForEach-Object { "${_}_1" }) -join '|'
$row = @('') * 31
$row[0] = '<next NUMBER>'; $row[1] = $key; $row[2] = $key; $row[3] = "sfproto_$Name.tga"; $row[4] = $Name; $row[5] = 'pvp'; $row[6] = '4'; $row[7] = '18'
$row[18] = $version; $row[20] = '0'; $row[21] = '0'; $row[22] = '0'; $row[23] = '0'; $row[30] = '<next pvp SEQUENCE>'
Set-Content "$presence\comaplist_row.csv" -Encoding ASCII -Value ($row -join ',')
Set-Content "$presence\basemaps_arena_entry.txt" -Encoding ASCII -Value @"
{
	map			"$Name"
	longname	"$key"
	gametype	"dm war sab sab2 dom sd sd2 hc thc ctf koth"
	description	"$key"
	mapimage	"blank"
	environment	"$Environment"
	allieschar	"socom_141_desert"
	axischar	"opforce_composite"
}
"@
Set-Content "$presence\localize.txt" -Encoding UTF8 -Value "$key`t$DisplayName"
Set-Content "$presence\usermap_info_entry.py" -Encoding UTF8 -Value @"
# for USERMAP_INFO / NEW_LOCALIZE of usermaps\build_usermaps_patch.py ( only needed for a display name, a description,
# a create-room picture or an environment of your own: without an entry the script makes the row below by itself,
# with the name "$Name", the picture sfproto_$Name.tga and the environment "urban" )
USERMAP_INFO["$Name"] = dict(name="$key", desc="${key}_DESC", image="sfproto_$Name.tga", env="$Environment")
NEW_LOCALIZE["$key"] = u"$DisplayName"
NEW_LOCALIZE["${key}_DESC"] = u"$DisplayName"
"@
Set-Content "$presence\INSTALL.txt" -Encoding ASCII -Value @"
$Name - where the files go ( <game> = the Call of Duty Online 1.1.1.13 folder, <server> = CoD4x_Server )

1. the zones
     zone\$Name.ff, zone\${Name}_load.ff  ->  <game>\usermaps\$Name\        ( client and server read usermaps\<map>\ )
   The client has to be started as codoMP_client_shipRetail.exe with the install's Kernel33.dll ( unsigned zones ).

2. the level script
     scripts\maps\mp\$Name.gsc            ->  <server>\bin\home\codol_rawfiles\maps\mp\
     scripts\maps\mp\${Name}_fx.gsc, scripts\maps\createfx\${Name}_fx.gsc go next to it only when the server should
     run the effects itself; the client takes both from the zone ( they are rawfiles of $Name.ff ).
   Without the level script the map still runs ( "no level script" ), without minimap setup.

3. the create-room list ( map pool row, arena entry, localized name )
   These three live in one patch zone, client_shipRetail\patch\usermaps_patch.ff, which the install's own script
   rebuilds from the folders under usermaps\:
     python <game>\usermaps\build_usermaps_patch.py
   It finds usermaps\$Name\$Name.ff and adds by itself: a row of mp/comaplist.csv ( ID $Name, TYPE pvp, 4 - 18
   players, on for every build token ), an entry of mp/basemaps.arena ( environment urban ) and the localize key
   ${key} = "$Name". For another display name, picture or environment put the lines of
   presence\usermap_info_entry.py into the script's USERMAP_INFO / NEW_LOCALIZE first.
   What the script writes, for reference ( made here, not installed ):
     presence\comaplist_row.csv         the 31 cells of the row ( NUMBER and SEQUENCE are the next free ones )
     presence\basemaps_arena_entry.txt  the arena entry
     presence\localize.txt              key <tab> text

4. pictures
   The minimap ( material compass_map_$Name ) and the loading picture are inside the zones: nothing to install.
   The create-room picture is an image sfproto_$Name ( 200 x 120 ): usermaps\build_map_images.py makes
   images\sfproto_${Name}_usermap.iwi from <map>_load.png for the map pack's .iwd, and the map has to be listed in
   MENU_IMAGE_MAPS of build_usermaps_patch.py. Without it the list shows the default picture.

5. playing it without the list: start the lobby with CODOL_FORCE_MAP=$Name ( any map name works that way ).
"@
"packaged $Name -> $Out"
Get-ChildItem $zone -Filter "$Name*.ff" | ForEach-Object { '  ' + $_.Name + '  ' + $_.Length + ' bytes' }
