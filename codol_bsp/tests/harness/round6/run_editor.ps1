param([Parameter(Mandatory)][string]$Cmds, [Parameter(Mandatory)][string]$Log, [string]$Shot, [int]$Timeout = 300, [string]$Editor)
# CoDOL Radiant unattended: the console commands of -Cmds ( "a;wait 3;b", end with quit ) run once the UI is up,
# every console line goes to -Log, -Shot gets the rendered UI frame every two seconds ( ed_shot <file> inside the
# commands takes one at a chosen moment ). Waits for the editor to end; after -Timeout seconds an editor that is
# still there is stopped - only one started from this repository ( path filter ).
$ErrorActionPreference = 'Stop'
if (-not $Editor) { $Editor = (Resolve-Path "$PSScriptRoot\..\..\..\..\build-editor\Release\CoDOLRadiant.exe").Path }
Remove-Item $Log -ErrorAction SilentlyContinue
$env:CODOL_STARTUP_CMDS = $Cmds
$env:CODOL_LOG = $Log
$env:CODOL_WINDOWED = '1'
if ($Shot) { $env:CODOL_AUTOSHOT = $Shot } else { Remove-Item Env:\CODOL_AUTOSHOT -ErrorAction SilentlyContinue }
$p = Start-Process -FilePath $Editor -WorkingDirectory (Split-Path $Editor) -PassThru
if (-not $p.WaitForExit($Timeout * 1000)) {
  Get-Process CoDOLRadiant -ErrorAction SilentlyContinue | Where-Object { $_.Path -like 'D:\CoD\iw3xo-radiant-codol\*' -and $_.Id -eq $p.Id } | Stop-Process -Force
  "TIMEOUT after $Timeout s: the editor was stopped"
}
"editor ended ( exit code $($p.ExitCode) ); log $Log"
