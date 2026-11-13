param([string]$Scratch = 'D:\CoD\iw3xo-radiant-codol\build-editor\bsptest', [string]$Client = 'D:\CoD\iw3xo-radiant-codol\build-editor\shadow_client',
      [string]$Game = 'D:\CoD\CODOL Builds\Call of Duty Online 1.1.1.13')
. "$PSScriptRoot\kernel33_shadow.ps1"
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
# the diagnostic Kernel33.dll start_client.ps1 put next to the client ( a file of the harness, marked by
# Kernel33.dll.harness ): it is taken away and the shadow's hard link to the install's Kernel33.dll is made again.
# A client that was just stopped is still in the process list for a moment: wait for it to go ( a client somebody
# else started stays, and so does the dll )
for ($wait = 0; $stopped_client -and $wait -lt 30 -and (Get-Process codoMP_client_shipRetail -ErrorAction SilentlyContinue); $wait++) { Start-Sleep -Milliseconds 200 }
if (-not (Get-Process codoMP_client_shipRetail -ErrorAction SilentlyContinue)) {
  Remove-Item "$Client\k33_shot.req", "$Client\k33_stats.req" -ErrorAction SilentlyContinue
  if (Test-Path "$Client\Kernel33.dll.harness") {
    if (Restore-InstallKernel33 -Client $Client -Game $Game) { "restored the link $Client\Kernel33.dll" }
    else { "WARNING: $Client\Kernel33.dll is still the harness dll: remove it by hand and link the install's again" }
  }
}
