param([Parameter(Mandatory)][string]$Name, [string]$Scratch = 'D:\CoD\iw3xo-radiant-codol\build-editor\bsptest',
      [string]$Client = 'D:\CoD\iw3xo-radiant-codol\build-editor\shadow_client')
# What the frame the game just drew went into, through the diagnostic Kernel33.dll ( start_client.ps1 ): one line per
# pair of render target and depth buffer - size, format, draw calls, primitives - to $Scratch\shots\<Name>.stats.txt,
# which is printed. A shadow map pass shows as draws into a depth buffer that has not the size of the back buffer
# ( with hardware shadow maps: a depth texture, usage 0x2, under a render target of its size ).
$dir = "$Scratch\shots"
New-Item -ItemType Directory -Force $dir | Out-Null
if (-not (Test-Path "$Client\Kernel33.dll.harness")) { 'NO STATS: the harness Kernel33.dll is not in the client folder ( start_client.ps1 )'; return }
if (-not (Get-Process codoMP_client_shipRetail -ErrorAction SilentlyContinue)) { 'NO STATS: no client'; return }
$out = "$dir\$Name.stats.txt"
Remove-Item $out -ErrorAction SilentlyContinue
[IO.File]::WriteAllText("$Client\k33_stats.req", $out, [Text.Encoding]::ASCII)
for ($i = 0; -not (Test-Path $out) -and $i -lt 100; $i++) { Start-Sleep -Milliseconds 100 }
if (-not (Test-Path $out)) { Remove-Item "$Client\k33_stats.req" -ErrorAction SilentlyContinue; 'NO STATS: the client did not write them'; return }
Start-Sleep -Milliseconds 100
Get-Content $out
