# End to end checks of codol_mapc against a Call of Duty Online 1.1.1.13 install ( read only ).
#
#   powershell -ExecutionPolicy Bypass -File codol_mapc\tests\run_e2e.ps1 [-Build build\codol_mapc] [-Game <install>] [-Out build\mapc_test] [-NoRender]
#
# 1. codol_mapc_tests: makes the edited maps ( <Out>\<map>\<stage>\edited.map + edited.codol_base.map ), compiles them
#    through the library and checks the result ( see the header of codol_mapc_tests.cpp ).
# 2. The command line tool on the same maps: its zone has to be, byte for byte, the zone the library call gave
#    ( and for the unedited map the zone of the shipped fast file ).
#    codol_mapc --check on every fast file written: its collision data is what a dedicated server takes.
# 3. codol_linker --verify on every fast file written: it decodes, every asset loads, read -> write is identical.
# 4. codol_viewtool: a picture of the shipped map and of the compiled one from the cameras in views.txt, to
#    <Out>\png\<map>_<stage>_<view>_before.png / _after.png. Look at them: nothing here judges a picture.
#
# Nothing is started but these tools; nothing is written outside <Out>. The game itself is never run.

param(
	[string]$Build = "build\codol_mapc",
	[string]$Game = "D:\CoD\CODOL Builds\Call of Duty Online 1.1.1.13",
	[string]$Out = "build\mapc_test",
	[switch]$NoRender
)

$ErrorActionPreference = "Stop"
$mapc = Join-Path $Build "Release\codol_mapc.exe"
$tests = Join-Path $Build "Release\codol_mapc_tests.exe"
$zonetool = Join-Path $Build "codol_zone\Release\codol_zonetool.exe"
$linker = Join-Path $Build "codol_zone\Release\codol_linker.exe"
$viewtool = Join-Path $Build "codol_view\Release\codol_viewtool.exe"
$exe = Join-Path $Game "C1.exe"
$zones = Join-Path $Game "client_shipRetail\chinese"
$failed = 0

function Fail([string]$text) { Write-Host "FAILED  $text" -ForegroundColor Red; $script:failed++ }
function ZoneHash([string]$fastfile, [string]$to)
{
	& $zonetool decrypt $fastfile --exe $exe --out $to | Out-Null
	if ($LASTEXITCODE -ne 0) { return "" }
	return (Get-FileHash $to -Algorithm SHA256).Hash
}

Write-Host "== 1. codol_mapc_tests"
& $tests --game $Game --out $Out
if ($LASTEXITCODE -ne 0) { Fail "codol_mapc_tests" }

New-Item -ItemType Directory -Force (Join-Path $Out "png") | Out-Null
foreach ($map in "mp_rust", "mp_killhouse_sh")
{
	foreach ($stage in "identity", "entities", "models", "brushes", "brush_trees", "brush_trees_all", "donor")
	{
		$folder = Join-Path $Out "$map\$stage"
		if (-not (Test-Path (Join-Path $folder "edited.map"))) { continue }
		$cli = Join-Path $folder "cli"
		New-Item -ItemType Directory -Force $cli | Out-Null
		$base = Join-Path $zones "$map.ff"
		$written = Join-Path $cli "$map.ff"

		Write-Host "== 2. codol_mapc  $map / $stage"
		$arguments = @("--base", $base, "--exe", $exe, "--map", (Join-Path $folder "edited.map"), "--out", $written, "--report", (Join-Path $cli "report.txt"), "--zone-out", (Join-Path $cli "zone.bin"))
		if ($stage -eq "donor") { $arguments += @("--donor", (Join-Path $zones "mp_dome.ff")) }
		if ($stage -like "brush_trees*") { $arguments += "--no-surfaces" }		# the tests compile these for collision only
		& $mapc @arguments | Out-Null
		if ($LASTEXITCODE -ne 0) { Fail "codol_mapc $map $stage"; continue }

		# the zone of the tool against the zone of the library call ( the fast file the tests wrote ), or the shipped one
		$reference = if ($stage -eq "identity") { $base } else { Join-Path $folder "$map.ff" }
		$ours = (Get-FileHash (Join-Path $cli "zone.bin") -Algorithm SHA256).Hash
		$theirs = ZoneHash $reference (Join-Path $cli "reference.bin")
		$reread = ZoneHash $written (Join-Path $cli "reread.bin")
		if ($ours -ne $theirs) { Fail "${map} ${stage}: the tool's zone differs from the reference zone" }
		if ($ours -ne $reread) { Fail "${map} ${stage}: the fast file does not decode to the zone that was written" }
		Remove-Item (Join-Path $cli "reference.bin"), (Join-Path $cli "reread.bin"), (Join-Path $cli "zone.bin") -ErrorAction SilentlyContinue

		# the collision data as a dedicated server reads it ( codol_mapc/server_check.hpp )
		& $mapc --check $written --exe $exe | Out-Null
		if ($LASTEXITCODE -ne 0) { Fail "${map} ${stage}: a dedicated server would refuse the collision data" }

		Write-Host "== 3. codol_linker --verify"
		$verify = & $linker --verify $written --exe $exe
		if ($LASTEXITCODE -ne 0 -or -not ($verify -match "read -> write: identical")) { Fail "${map} ${stage}: codol_linker --verify" }

		$views = Join-Path $folder "views.txt"
		if ($NoRender -or -not (Test-Path $views)) { continue }
		Write-Host "== 4. codol_viewtool"
		foreach ($line in Get-Content $views)
		{
			if ($line.StartsWith("#") -or -not $line.Trim()) { continue }
			$parts = $line.Trim() -split " "
			$name = $parts[0]
			$camera = $parts[1..($parts.Length - 1)]
			$before = Join-Path $Out "png\${map}_${stage}_${name}_before.png"
			$after = Join-Path $Out "png\${map}_${stage}_${name}_after.png"
			& $viewtool render --game $Game --map $map --size 960x540 --out $before @camera | Out-Null
			if ($LASTEXITCODE -ne 0) { Fail "${map} ${stage}: render of the shipped map" }
			& $viewtool render --game $Game --map $map --zone-dirs $cli --size 960x540 --out $after @camera | Out-Null
			if ($LASTEXITCODE -ne 0) { Fail "${map} ${stage}: render of the compiled map" }
		}
	}
}

if ($failed) { Write-Host "$failed checks failed" -ForegroundColor Red; exit 1 }
Write-Host "all end to end checks passed; pictures in $(Join-Path $Out 'png')"
exit 0
