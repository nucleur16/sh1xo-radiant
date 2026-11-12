param([string]$Scratch = 'D:\CoD\iw3xo-radiant-codol\build-editor\bsptest', [string]$Client = 'D:\CoD\iw3xo-radiant-codol\build-editor\shadow_client')
# stops what the harness started: client, lobby ( with its python child ), server - by the recorded pids only
$stopped_client = $false
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
  if ($what -eq 'client') { $stopped_client = $true }
  Remove-Item $f
}
# the frame dump d3d9.dll start_client.ps1 put next to the client ( a file of the harness, not a link into the install )
if (-not (Get-Process codoMP_client_shipRetail -ErrorAction SilentlyContinue)) {
  if ($stopped_client) { Start-Sleep -Milliseconds 800 }
  Remove-Item "$Client\d3d9.dll", "$Client\codol_shot.req", "$Client\codol_shot.log" -ErrorAction SilentlyContinue
}
