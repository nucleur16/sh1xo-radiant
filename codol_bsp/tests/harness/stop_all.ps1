param([string]$Scratch = 'D:\CoD\iw3xo-radiant-codol\build-editor\bsptest')
# stops what the harness started: client, lobby ( with its python child ), server - by the recorded pids only
foreach ($what in 'client', 'lobby', 'server') {
  $f = "$Scratch\$what.pid"
  if (-not (Test-Path $f)) { continue }
  $id = [int](Get-Content $f)
  $p = Get-Process -Id $id -ErrorAction SilentlyContinue
  if ($p -and $p.ProcessName -in 'codoMP_client_shipRetail', 'cmd', 'codol_dedrun') {
    if ($what -eq 'lobby') {
      Get-CimInstance Win32_Process -Filter "ParentProcessId=$id" | ForEach-Object { Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue }
    }
    Stop-Process -Id $id -Force -ErrorAction SilentlyContinue
    "stopped $what $id"
  }
  Remove-Item $f
}
