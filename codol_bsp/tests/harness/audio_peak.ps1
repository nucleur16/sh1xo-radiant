param([int]$Seconds = 5, [string]$Process = 'codoMP_client_shipRetail')
# What the game sends to the sound card: the peak level ( 0 .. 1 ) of the client's audio session on the default
# output device over some seconds, read from Windows' own meter ( IAudioMeterInformation of the process's audio
# session ). Nothing is recorded and nothing in the game is touched. It is the evidence for "a sound plays" on a
# machine nobody listens to: 0 while the game is silent, above 0 while it mixes anything.
# Prints: sessions of the process, samples taken, the highest and the mean peak.
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
namespace CodolAudio {
  [ComImport, Guid("BCDE0395-E52F-467C-8E3D-C4579291692E")] class MMDeviceEnumerator { }
  [ComImport, Guid("A95664D2-9614-4F35-A746-DE8DB63617E6"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
  interface IMMDeviceEnumerator {
    int NotImpl1();
    [PreserveSig] int GetDefaultAudioEndpoint(int dataFlow, int role, out IMMDevice device);
  }
  [ComImport, Guid("D666063F-1587-4E43-81F1-B948E807363F"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
  interface IMMDevice {
    [PreserveSig] int Activate(ref Guid iid, int clsCtx, IntPtr activationParams, [MarshalAs(UnmanagedType.IUnknown)] out object iface);
  }
  [ComImport, Guid("77AA99A0-1BD6-484F-8BC7-2C654C9A9B6F"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
  interface IAudioSessionManager2 {
    int NotImpl1(); int NotImpl2();
    [PreserveSig] int GetSessionEnumerator(out IAudioSessionEnumerator enumerator);
  }
  [ComImport, Guid("E2F5BB11-0570-40CA-ACDD-3AA01277DEE8"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
  interface IAudioSessionEnumerator {
    [PreserveSig] int GetCount(out int count);
    [PreserveSig] int GetSession(int index, out IAudioSessionControl2 session);
  }
  [ComImport, Guid("BFB7FF88-7239-4FC9-8FA2-07C950BE9C6D"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
  interface IAudioSessionControl2 {
    int NotImpl0(); int NotImpl1(); int NotImpl2(); int NotImpl3(); int NotImpl4(); int NotImpl5(); int NotImpl6(); int NotImpl7(); int NotImpl8();
    int NotImpl9(); int NotImpl10();
    [PreserveSig] int GetProcessId(out uint pid);
  }
  [ComImport, Guid("C02216F6-8C67-4B5B-9D00-D008E73E0064"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
  interface IAudioMeterInformation {
    [PreserveSig] int GetPeakValue(out float peak);
  }
  public static class Meter {
    // per sample the largest peak over the process's sessions; -1: no output device, -2: the process has no session
    public static float Peak(uint pid, out int sessions) {
      sessions = 0;
      IMMDevice device;
      var enumerator = (IMMDeviceEnumerator)new MMDeviceEnumerator();
      if (enumerator.GetDefaultAudioEndpoint(0, 1, out device) != 0 || device == null) return -1;
      Guid iid = typeof(IAudioSessionManager2).GUID;
      object o;
      if (device.Activate(ref iid, 23, IntPtr.Zero, out o) != 0) return -1;
      IAudioSessionEnumerator list;
      if (((IAudioSessionManager2)o).GetSessionEnumerator(out list) != 0) return -1;
      int count; list.GetCount(out count);
      float best = -2;
      for (int i = 0; i < count; i++) {
        IAudioSessionControl2 session;
        if (list.GetSession(i, out session) != 0 || session == null) continue;
        uint owner; session.GetProcessId(out owner);
        if (owner != pid) continue;
        sessions++;
        float peak;
        if (((IAudioMeterInformation)session).GetPeakValue(out peak) == 0 && peak > best) best = peak;
      }
      return best;
    }
  }
}
'@
$p = Get-Process $Process -ErrorAction SilentlyContinue | Select-Object -First 1
if (-not $p) { "no process $Process"; return }
$max = 0.0; $sum = 0.0; $n = 0; $sessions = 0; $state = ''
$until = (Get-Date).AddSeconds($Seconds)
while ((Get-Date) -lt $until) {
  $s = 0
  $v = [CodolAudio.Meter]::Peak([uint32]$p.Id, [ref]$s)
  if ($v -eq -1) { $state = 'no default output device'; break }
  if ($v -eq -2) { $state = 'the process has no audio session' } else { $state = ''; $sessions = $s; $n++; $sum += $v; if ($v -gt $max) { $max = $v } }
  Start-Sleep -Milliseconds 50
}
if ($n) { 'audio of {0} ( pid {1} ): {2} session( s ), {3} samples in {4} s, highest peak {5:F4}, mean {6:F4}' -f $Process, $p.Id, $sessions, $n, $Seconds, $max, ($sum / $n) }
else { "audio of $Process ( pid $($p.Id) ): $state" }
