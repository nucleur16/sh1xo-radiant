# A link-based copy of the 1.1.1.13 install for server-only checks: the files are hard links, the big folders
# junctions, so nothing of the real install is copied or can be replaced by writing a new file under the same name
# ( writing a NEW file where a hard link was first needs the link name removed: Remove-Item on that one file only ).
#
#   make_shadow.ps1 -Install <1.1.1.13 install> -Shadow <new folder>
#
# Never delete recursively inside the shadow: the junctions lead into the real install. Remove a junction with
# `cmd /c rmdir <junction>`.
param([Parameter(Mandatory)][string]$Install, [Parameter(Mandatory)][string]$Shadow)
$ErrorActionPreference = 'Stop'
if (Test-Path $Shadow) { throw "$Shadow exists" }
New-Item -ItemType Directory -Force $Shadow | Out-Null
foreach ($f in Get-ChildItem -LiteralPath $Install -File) {
	if ($f.Extension -in '.exe', '.dll', '.ff', '.ff2', '.ini', '.dat', '.txt', '.bmp') {
		New-Item -ItemType HardLink -Path (Join-Path $Shadow $f.Name) -Target $f.FullName | Out-Null
	}
}
foreach ($d in 'main', 'miles', 'usermaps') {
	$source = Join-Path $Install $d
	if (Test-Path $source) { cmd /c mklink /J "`"$(Join-Path $Shadow $d)`"" "`"$source`"" | Out-Null }
}
foreach ($d in 'chinese', 'patch') {
	$target = Join-Path $Shadow "client_shipRetail\$d"
	New-Item -ItemType Directory -Force $target | Out-Null
	foreach ($f in Get-ChildItem -LiteralPath (Join-Path $Install "client_shipRetail\$d") -File -Filter *.ff) {
		New-Item -ItemType HardLink -Path (Join-Path $target $f.Name) -Target $f.FullName | Out-Null
	}
}
"shadow install at $Shadow"
