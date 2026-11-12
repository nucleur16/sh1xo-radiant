param([string]$Out = 'D:\CoD\iw3xo-radiant-codol\build-editor\bsptest\tools')
# Builds the frame dump d3d9.dll ( x86, static CRT ) into $Out. start_client.ps1 puts it into the shadow client.
$vs = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -property installationPath
New-Item -ItemType Directory -Force $Out | Out-Null
$src = $PSScriptRoot
$cmd = "$Out\build_d3d9_shot.cmd"
Set-Content $cmd -Encoding ASCII -Value @"
@echo off
call "$vs\VC\Auxiliary\Build\vcvarsall.bat" x86 >nul 2>&1
cd /d "$Out"
cl /nologo /O2 /W3 /LD /MT /D_CRT_SECURE_NO_WARNINGS "$src\d3d9_shot.cpp" /Fe:d3d9.dll /link /DEF:"$src\d3d9_shot.def" 2>&1
"@
cmd /c "`"$cmd`""
if ($LASTEXITCODE) { 'build FAILED'; exit 1 }
"$Out\d3d9.dll"
