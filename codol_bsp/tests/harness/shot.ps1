param([Parameter(Mandatory)][string]$Name, [switch]$Full, [switch]$Screen, [string]$Scratch = 'D:\CoD\iw3xo-radiant-codol\build-editor\bsptest',
      [string]$Client = 'D:\CoD\iw3xo-radiant-codol\build-editor\shadow_client')
# A picture of the game to $Scratch\shots\<Name>.png and a copy of at most 1600 pixels width, <Name>_s.png. Prints the
# path of the small one.
#   default   the frame the game renders, through the diagnostic Kernel33.dll in the client ( start_client.ps1 puts
#             it there; its request file k33_shot.req ): works whatever is in front of the window, without input
#             focus and while the display is off ( a screen copy of the window is black then )
#   -Screen   a screen copy of the game window ( nothing is saved while another window covers it )
#   -Full     the whole primary screen
Add-Type -AssemblyName System.Drawing
$dir = "$Scratch\shots"
New-Item -ItemType Directory -Force $dir | Out-Null
function Save-Small($bmp) {
  $w = $bmp.Width; $h = $bmp.Height
  $bmp.Save("$dir\$Name.png", [System.Drawing.Imaging.ImageFormat]::Png)
  $sw = [Math]::Min(1600, $w); $sh = [int]($h * $sw / $w)
  $s = New-Object System.Drawing.Bitmap $sw, $sh
  $g2 = [System.Drawing.Graphics]::FromImage($s); $g2.InterpolationMode = 'HighQualityBicubic'
  $g2.DrawImage($bmp, 0, 0, $sw, $sh); $g2.Dispose()
  $s.Save("$dir\${Name}_s.png", [System.Drawing.Imaging.ImageFormat]::Png)
  $s.Dispose()
  "$dir\${Name}_s.png"
}
if (-not $Full -and -not $Screen -and (Test-Path "$Client\Kernel33.dll.harness")) {
  if (-not (Get-Process codoMP_client_shipRetail -ErrorAction SilentlyContinue)) { 'NOT CAPTURED: no client'; return }
  $out = "$dir\$Name.bmp"
  Remove-Item $out -ErrorAction SilentlyContinue
  [IO.File]::WriteAllText("$Client\k33_shot.req", $out, [Text.Encoding]::ASCII)
  for ($i = 0; -not (Test-Path $out) -and $i -lt 100; $i++) { Start-Sleep -Milliseconds 100 }
  if (-not (Test-Path $out)) { Remove-Item "$Client\k33_shot.req" -ErrorAction SilentlyContinue; 'NOT CAPTURED: the client did not write the frame'; return }
  Start-Sleep -Milliseconds 100
  $bytes = [IO.File]::ReadAllBytes($out)
  $ms = New-Object IO.MemoryStream (, $bytes)
  $bmp = New-Object System.Drawing.Bitmap $ms
  Save-Small $bmp
  $bmp.Dispose(); $ms.Dispose()
  Remove-Item $out
  return
}
Add-Type @'
using System; using System.Runtime.InteropServices;
public class W32shot {
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L, T, R, B; }
  [StructLayout(LayoutKind.Sequential)] public struct POINT { public int X, Y; }
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h, ref POINT p);
  [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern IntPtr WindowFromPoint(POINT p);
  [DllImport("user32.dll")] public static extern IntPtr GetAncestor(IntPtr h, uint f);
}
'@
[void][W32shot]::SetProcessDPIAware()
$x = 0; $y = 0; $w = 2560; $h = 1440
if (-not $Full) {
  $p = Get-Process codoMP_client_shipRetail -ErrorAction SilentlyContinue | Where-Object { $_.MainWindowHandle -ne 0 } | Select-Object -First 1
  if ($p) {
    $r = New-Object W32shot+RECT; [void][W32shot]::GetClientRect($p.MainWindowHandle, [ref]$r)
    $pt = New-Object W32shot+POINT; [void][W32shot]::ClientToScreen($p.MainWindowHandle, [ref]$pt)
    if ($r.R -gt 0) { $x = $pt.X; $y = $pt.Y; $w = $r.R; $h = $r.B }
    # only the game is captured: the window is brought to the front, and nothing is saved while another window covers it
    [void][W32shot]::SetForegroundWindow($p.MainWindowHandle); Start-Sleep -Milliseconds 400
    foreach ($f in @(@(0.5, 0.5), @(0.1, 0.1), @(0.9, 0.1), @(0.1, 0.9), @(0.9, 0.9))) {
      $q = New-Object W32shot+POINT; $q.X = [int]($x + $w * $f[0]); $q.Y = [int]($y + $h * $f[1])
      if ([W32shot]::GetAncestor([W32shot]::WindowFromPoint($q), 2) -ne $p.MainWindowHandle) { 'NOT CAPTURED: another window covers the game window'; return }
    }
  } else { 'NOT CAPTURED: no game window'; return }
}
$bmp = New-Object System.Drawing.Bitmap $w, $h
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.CopyFromScreen($x, $y, 0, 0, (New-Object System.Drawing.Size $w, $h))
$g.Dispose()
Save-Small $bmp
$bmp.Dispose()
