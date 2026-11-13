# Dot-sourced by start_client.ps1 and stop_all.ps1: the shadow client's Kernel33.dll.
#
# In the shadow it is a HARD LINK to the install's Kernel33.dll. Nothing here ever writes through that link: a file
# that is in the way is removed first ( removing a hard link never touches the install's file ), then a file is
# copied or the link is made again. Kernel33.dll.harness marks a dll the harness put there.

function Set-HarnessKernel33([string]$Client, [string]$Dll) {
  $to = "$Client\Kernel33.dll"
  if (Test-Path $to) { Remove-Item $to -Force }      # the link ( or an earlier harness dll ), never the install's file
  Copy-Item $Dll $to
  if ((Get-Item $to).LinkType) { throw "$to is still a link: nothing was started" }
  "harness dll $((Get-FileHash $to -Algorithm SHA256).Hash) from $Dll" | Set-Content "$to.harness"
}

function Restore-InstallKernel33([string]$Client, [string]$Game) {
  $to = "$Client\Kernel33.dll"
  if (-not (Test-Path "$to.harness")) { return $true }           # the shadow still has its own link
  for ($try = 0; $try -lt 20 -and (Test-Path $to); $try++) {      # a client that was just stopped holds the dll for a moment
    Remove-Item $to -Force -ErrorAction SilentlyContinue
    if (Test-Path $to) { Start-Sleep -Milliseconds 300 }
  }
  if (Test-Path $to) { return $false }
  New-Item -ItemType HardLink -Path $to -Target "$Game\Kernel33.dll" | Out-Null
  Remove-Item "$to.harness"
  return $true
}
