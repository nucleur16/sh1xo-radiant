# Every unattended run of the editor in this folder, one after the other ( about ten minutes; the editor's window
# opens and closes for each ).
#
#   powershell -File tests\ed3d\run_all.ps1 -Exe build-editor\Release\CoDOLRadiant.exe [-Skip switch_stress]
#
# The runs with injected mouse input ( mouse, matdrop, switch_* ) click at pixels of the layout the editor is started
# with on a 2560 x 1440 desktop: `codol_editor_layout.ini` next to the exe, the window maximised. The others
# ( object, handles, shipped ) do not depend on the layout.

param(
	[Parameter(Mandatory = $true)][string]$Exe,
	[string[]]$Skip = @()
)

$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$failed = @()

foreach ($name in @("object", "handles", "shipped", "mouse", "matdrop", "switch_click", "switch_discard", "switch_stress")) {
	if ($Skip -contains $name) { continue }
	& powershell -NoProfile -File (Join-Path $here "run.ps1") -Exe $Exe -Script (Join-Path $here "$name.cmds") -Timeout 900
	if ($LASTEXITCODE -ne 0) { $failed += $name }
}

if ($failed.Count -eq 0) { Write-Output "ALL PASSED"; exit 0 }
Write-Output ("FAILED: " + ($failed -join ", "))
exit 1
