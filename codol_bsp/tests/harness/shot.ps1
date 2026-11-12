param([Parameter(Mandatory)][string]$Name, [switch]$Full, [string]$Scratch = 'D:\CoD\iw3xo-radiant-codol\build-editor\bsptest')
# Captures the game window ( -Full: the whole primary screen ) to $Scratch\shots\<Name>.png and a copy of at most
# 1600 pixels width, <Name>_s.png. Prints the path of the small one.
Add-Type -AssemblyName System.Drawing
Add-Type @'
using System; using System.Runtime.InteropServices;
public class W32shot {
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L, T, R, B; }
  [StructLayout(LayoutKind.Sequential)] public struct POINT { public int X, Y; }
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h, ref POINT p);
  [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
}
'@
[void][W32shot]::SetProcessDPIAware()
$dir = "$Scratch\shots"
New-Item -ItemType Directory -Force $dir | Out-Null
$x = 0; $y = 0; $w = 2560; $h = 1440
if (-not $Full) {
  $p = Get-Process codoMP_client_shipRetail -ErrorAction SilentlyContinue | Where-Object { $_.MainWindowHandle -ne 0 } | Select-Object -First 1
  if ($p) {
    $r = New-Object W32shot+RECT; [void][W32shot]::GetClientRect($p.MainWindowHandle, [ref]$r)
    $pt = New-Object W32shot+POINT; [void][W32shot]::ClientToScreen($p.MainWindowHandle, [ref]$pt)
    if ($r.R -gt 0) { $x = $pt.X; $y = $pt.Y; $w = $r.R; $h = $r.B }
  }
}
$bmp = New-Object System.Drawing.Bitmap $w, $h
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.CopyFromScreen($x, $y, 0, 0, (New-Object System.Drawing.Size $w, $h))
$g.Dispose()
$bmp.Save("$dir\$Name.png", [System.Drawing.Imaging.ImageFormat]::Png)
$sw = [Math]::Min(1600, $w); $sh = [int]($h * $sw / $w)
$s = New-Object System.Drawing.Bitmap $sw, $sh
$g2 = [System.Drawing.Graphics]::FromImage($s); $g2.InterpolationMode = 'HighQualityBicubic'
$g2.DrawImage($bmp, 0, 0, $sw, $sh); $g2.Dispose()
$s.Save("$dir\${Name}_s.png", [System.Drawing.Imaging.ImageFormat]::Png)
$bmp.Dispose(); $s.Dispose()
"$dir\${Name}_s.png"
