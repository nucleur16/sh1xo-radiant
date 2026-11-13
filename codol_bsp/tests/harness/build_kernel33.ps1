param([string]$Kernel33Repo = 'D:\CoD\k33work\cod-offline-1.1.1.13', [string]$Out = 'D:\CoD\iw3xo-radiant-codol\build-editor\bsptest\tools',
      [string]$Bash = 'C:\Program Files\Git\bin\bash.exe')
# Builds the diagnostic profile of Kernel33.dll ( the client patch dll; a checkout that has k33shot.c, branch
# k33-deferred-host or later ) and copies it to $Out\Kernel33.dll. start_client.ps1 puts it into the shadow client.
#
# Only that profile has the screenshot and frame statistics ( k33_screenshot, k33_framestats, the request files );
# the dll the install runs is the shipping profile and has none of it. K33_NO_COPY=1: the build never copies its
# result anywhere ( without it build.sh copies to ..\Kernel33.dll, which next to an install is the game's dll ).
$ErrorActionPreference = 'Stop'
if (-not (Test-Path "$Kernel33Repo\k33shot.c")) { throw "$Kernel33Repo has no k33shot.c: not a Kernel33 checkout with the screenshot feature" }
New-Item -ItemType Directory -Force $Out | Out-Null
# the log goes next to the result, not into the Kernel33 checkout
$log = '/' + $Out.Substring(0, 1).ToLower() + ($Out.Substring(2) -replace '\\', '/') + '/build_kernel33.log'
$unix = '/'+ $Kernel33Repo.Substring(0, 1).ToLower() + ($Kernel33Repo.Substring(2) -replace '\\', '/')
& $Bash -lc "cd '$unix' && export PATH=/c/msys64/mingw32/bin:`$PATH && K33_NO_COPY=1 K33_LOCAL_TESTING=1 K33_DIAGNOSTICS=1 sh build.sh > '$log' 2>&1"
if ($LASTEXITCODE) { "build FAILED: see $Out\build_kernel33.log"; exit 1 }
Copy-Item "$Kernel33Repo\Kernel33.dll" "$Out\Kernel33.dll" -Force
"$Out\Kernel33.dll"
