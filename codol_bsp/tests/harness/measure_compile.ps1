param([Parameter(Mandatory)][string]$MapFile, [Parameter(Mandatory)][string]$Out, [string[]]$Extra = @(), [int]$LimitMB = 6000, [int]$LimitSeconds = 900,
      [string]$Scratch = 'D:\CoD\iw3xo-radiant-codol\build-editor\bsptest', [string]$Game = 'D:\CoD\CODOL Builds\Call of Duty Online 1.1.1.13')
# compiles a map and reports time and peak memory; the compile is stopped when it passes a memory or time limit
$Build = (Resolve-Path "$PSScriptRoot\..\..\..\build\codol_bsp\Release").Path
$dir = Split-Path $Out
New-Item -ItemType Directory -Force $dir | Out-Null
$a = @('--map', "`"$MapFile`"", '--exe', "`"$Game\C1.exe`"", '--install', "`"$Game`"", '--out', "`"$Out`"", '--cache', "`"$Scratch`"", '--report', "`"$Out.report.txt`"") + $Extra
$sw = [Diagnostics.Stopwatch]::StartNew()
$p = Start-Process -FilePath "$Build\codol_bsp.exe" -ArgumentList $a -NoNewWindow -PassThru -RedirectStandardOutput "$Out.stdout.txt" -RedirectStandardError "$Out.stderr.txt"
[long]$peak = 0
while (-not $p.HasExited) {
  $p.Refresh()
  if ($p.PeakWorkingSet64 -gt $peak) { $peak = $p.PeakWorkingSet64 }
  if ($peak -gt [long]$LimitMB * 1MB -or $sw.Elapsed.TotalSeconds -gt $LimitSeconds) { Stop-Process -Id $p.Id -Force; "STOPPED: over the limit ( $([int]($peak / 1MB)) MB, $([int]$sw.Elapsed.TotalSeconds) s )"; break }
  Start-Sleep -Milliseconds 200
}
"exit code $($p.ExitCode), $([Math]::Round($sw.Elapsed.TotalSeconds, 1)) s, peak working set $([int]($peak / 1MB)) MB, file $((Get-Item $Out -ErrorAction SilentlyContinue).Length) bytes"
Get-Content "$Out.stderr.txt" -Tail 4 | ForEach-Object { $_.Substring(0, [Math]::Min(400, $_.Length)) }
Get-Content "$Out.report.txt" -ErrorAction SilentlyContinue | Select-String -Pattern '^map|^collision|^render|^light|vis:|LEAK:|^zone:|^  [a-z]+ +\d|light grid does' | ForEach-Object { $_.Line.Substring(0, [Math]::Min(300, $_.Line.Length)) }
