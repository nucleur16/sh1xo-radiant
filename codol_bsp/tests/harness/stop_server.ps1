param([string]$Scratch = 'D:\CoD\iw3xo-radiant-codol\build-editor\bsptest')
# stops the server start_server.ps1 started ( by its recorded pid, and only if that pid still is a codol_dedrun )
$f = "$Scratch\server.pid"
if (Test-Path $f) {
  $id = [int](Get-Content $f)
  $p = Get-Process -Id $id -ErrorAction SilentlyContinue
  if ($p -and $p.ProcessName -eq 'codol_dedrun') { Stop-Process -Id $id -Force; "stopped $id" } else { "pid $id is not running" }
  Remove-Item $f
} else { 'no server.pid' }
