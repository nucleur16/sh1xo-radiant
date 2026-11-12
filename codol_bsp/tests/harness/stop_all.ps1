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
# the frame dump d3d9.dll start_client.ps1 put next to the client ( a file of the harness, not a link into the install ).
# A client that was just stopped is still in the process list for a moment: wait for it to go ( a client somebody
# else started stays, and so does the dll )
for ($wait = 0; $stopped_client -and $wait -lt 30 -and (Get-Process codoMP_client_shipRetail -ErrorAction SilentlyContinue); $wait++) { Start-Sleep -Milliseconds 200 }
if (-not (Get-Process codoMP_client_shipRetail -ErrorAction SilentlyContinue)) {
  # the client holds the dll for a moment after it is stopped: try until it is gone
  for ($try = 0; $try -lt 20; $try++) {
    Remove-Item "$Client\d3d9.dll", "$Client\codol_shot.req", "$Client\codol_shot.log" -ErrorAction SilentlyContinue
    if (-not (Test-Path "$Client\d3d9.dll")) { break }
    Start-Sleep -Milliseconds 300
  }
  if (Test-Path "$Client\d3d9.dll") { "WARNING: $Client\d3d9.dll is still there: remove it by hand" }
}
