# Brings the game window to the front ( Windows only lets a process that just "had input" do that: an Alt key press
# and release is sent first ). Prints whether the window is in front now.
Add-Type @'
using System; using System.Runtime.InteropServices;
public class W32focus {
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int cmd);
  [DllImport("user32.dll")] public static extern bool IsIconic(IntPtr h);
  [DllImport("user32.dll")] public static extern void keybd_event(byte vk, byte scan, uint f, UIntPtr e);
}
'@
$p = Get-Process codoMP_client_shipRetail -ErrorAction SilentlyContinue | Where-Object { $_.MainWindowHandle -ne 0 } | Select-Object -First 1
if (-not $p) { 'no game window'; return }
$h = $p.MainWindowHandle
if ([W32focus]::IsIconic($h)) { [void][W32focus]::ShowWindow($h, 9) }
[W32focus]::keybd_event(0x12, 0, 0, [UIntPtr]::Zero); [W32focus]::keybd_event(0x12, 0, 2, [UIntPtr]::Zero)
[void][W32focus]::SetForegroundWindow($h)
Start-Sleep -Milliseconds 500
if ([W32focus]::GetForegroundWindow() -eq $h) { 'game window in front' } else { 'game window NOT in front' }
