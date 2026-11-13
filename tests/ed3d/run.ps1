# Runs CoDOLRadiant.exe unattended with a list of console commands and collects its console log.
#
#   powershell -File tests\ed3d\run.ps1 -Exe build-ed3d\Release\CoDOLRadiant.exe -Script tests\ed3d\edit3d.cmds
#
# The script file has one console command per line ( '#' starts a comment; "wait <seconds>" pauses ). In it
#   $OUT   is the output folder ( -Out, default tests\ed3d\out ): screenshots go there ( ed_shot $OUT\x.png )
#   $MAP   is -Map
# The list should end with "quit"; the editor is stopped ( by its process id, nothing else ) when it has not
# closed after -Timeout seconds. Exit code: 0 when the editor closed by itself with exit code 0 and every
# "expect <text>" line of the script was found in the log, 1 otherwise.
#
# GDI captures of the editor are blank on some machines: the pictures are the editor's own frame dumps.

param(
	[Parameter(Mandatory = $true)][string]$Exe,
	[Parameter(Mandatory = $true)][string]$Script,
	[string]$Out = "",
	[string]$Map = "mp_ed3d_test",
	[int]$Timeout = 240,
	[switch]$KeepMap,
	[switch]$Windowed
)

$ErrorActionPreference = "Stop"
$exePath = (Resolve-Path $Exe).Path
$exeDir = Split-Path $exePath
if ($Out -eq "") { $Out = Join-Path (Split-Path (Resolve-Path $Script).Path) "out" }
New-Item -ItemType Directory -Force $Out | Out-Null
$Out = (Resolve-Path $Out).Path

$name = [IO.Path]::GetFileNameWithoutExtension($Script)
$log = Join-Path $Out "$name.log"
if (Test-Path $log) { Remove-Item $log -Confirm:$false }

# a map of the test's own: map_new refuses a name that exists
if (-not $KeepMap) {
	Get-ChildItem (Join-Path $exeDir "map_source") -Filter "$Map.*" -ErrorAction SilentlyContinue | Remove-Item -Confirm:$false
}

$expect = @()
$cmds = @()
foreach ($line in Get-Content $Script) {
	$t = $line.Trim()
	if ($t -eq "" -or $t.StartsWith("#")) { continue }
	$t = $t.Replace('$OUT', $Out).Replace('$MAP', $Map)
	if ($t.StartsWith("expect ")) { $expect += $t.Substring(7); continue }
	$cmds += $t
}

$env:CODOL_STARTUP_CMDS = ($cmds -join ";")
$env:CODOL_LOG = $log
# the scripts click and drag at pixels of the layout without the global toolbar ( which takes a row or two )
$env:CODOL_NO_TOOLBAR = "1"
if ($Windowed) { $env:CODOL_WINDOWED = "1" } else { Remove-Item Env:CODOL_WINDOWED -ErrorAction SilentlyContinue }

$p = Start-Process -FilePath $exePath -WorkingDirectory $exeDir -PassThru
$peak = 0
$deadline = (Get-Date).AddSeconds($Timeout)
while (-not $p.HasExited -and (Get-Date) -lt $deadline) {
	Start-Sleep -Milliseconds 500
	try { $p.Refresh(); if ($p.PeakWorkingSet64 -gt $peak) { $peak = $p.PeakWorkingSet64 } } catch {}
}

$ok = $true
if (-not $p.HasExited) {
	Write-Output "TIMEOUT after $Timeout s: stopping process $($p.Id)"
	Stop-Process -Id $p.Id -Force -Confirm:$false
	$ok = $false
}
else {
	Write-Output "exit code $($p.ExitCode), peak working set $([int]($peak / 1MB)) MB"
	if ($p.ExitCode -ne 0) { $ok = $false }
}

$text = if (Test-Path $log) { Get-Content $log -Raw } else { "" }
foreach ($e in $expect) {
	if (-not $text.Contains($e)) { Write-Output "MISSING in the log: $e"; $ok = $false }
}

Write-Output "log: $log"
if ($ok) { Write-Output "PASS $name"; exit 0 } else { Write-Output "FAIL $name"; exit 1 }
