param([Parameter(Mandatory)][string]$Name, [string]$Scratch = 'D:\CoD\iw3xo-radiant-codol\build-editor\bsptest')
# Captures the game window through PrintWindow ( PW_RENDERFULLCONTENT ): the window's own content as the compositor has
# it, whatever is in front of it. For when a screen copy of the window comes out black. Saves <Name>.png and <Name>_s.png.
Add-Type -AssemblyName System.Drawing
Add-Type @'
using System; using System.Runtime.InteropServices;
public class W32pw {
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L, T, R, B; }
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr dc, uint flags);
  [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
}
'@
[void][W32pw]::SetProcessDPIAware()
$p = Get-Process codoMP_client_shipRetail -ErrorAction SilentlyContinue | Where-Object { $_.MainWindowHandle -ne 0 } | Select-Object -First 1
if (-not $p) { 'NOT CAPTURED: no game window'; return }
$r = New-Object W32pw+RECT; [void][W32pw]::GetClientRect($p.MainWindowHandle, [ref]$r)
$bmp = New-Object System.Drawing.Bitmap $r.R, $r.B
$g = [System.Drawing.Graphics]::FromImage($bmp)
$dc = $g.GetHdc()
$ok = [W32pw]::PrintWindow($p.MainWindowHandle, $dc, 3)     # PW_CLIENTONLY | PW_RENDERFULLCONTENT
$g.ReleaseHdc($dc); $g.Dispose()
$dir = "$Scratch\shots"; New-Item -ItemType Directory -Force $dir | Out-Null
$sum = 0; $n = 0
for ($x = 10; $x -lt $r.R; $x += 53) { for ($y = 10; $y -lt $r.B; $y += 47) { $c = $bmp.GetPixel($x, $y); $sum += $c.R + $c.G + $c.B; $n++ } }
$bmp.Save("$dir\$Name.png", [System.Drawing.Imaging.ImageFormat]::Png)
$sw = [Math]::Min(1600, $r.R); $sh = [int]($r.B * $sw / $r.R)
$s = New-Object System.Drawing.Bitmap $sw, $sh
$g2 = [System.Drawing.Graphics]::FromImage($s); $g2.InterpolationMode = 'HighQualityBicubic'; $g2.DrawImage($bmp, 0, 0, $sw, $sh); $g2.Dispose()
$s.Save("$dir\${Name}_s.png", [System.Drawing.Imaging.ImageFormat]::Png)
$bmp.Dispose(); $s.Dispose()
"$dir\${Name}_s.png ( PrintWindow $ok, $($r.R)x$($r.B), mean brightness $([int]($sum / $n / 3)) )"
