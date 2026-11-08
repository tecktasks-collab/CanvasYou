$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing

Add-Type @"
using System;
using System.Text;
using System.Runtime.InteropServices;
using System.Collections.Generic;
public static class Probe {
  public const uint WM_COMMAND = 0x0111;
  public const uint WM_LBUTTONDOWN = 0x0201;
  public const uint WM_LBUTTONUP = 0x0202;
  public const uint MK_LBUTTON = 0x0001;
  public const int IDM_FILE_IMPORT = 1032;
  public const uint MOUSEEVENTF_LEFTDOWN = 0x0002;
  public const uint MOUSEEVENTF_LEFTUP = 0x0004;
  public const uint FILE_MAP_READ = 0x0004;
  public delegate bool EnumProc(IntPtr h, IntPtr l);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc lp, IntPtr l);
  [DllImport("user32.dll")] public static extern bool EnumChildWindows(IntPtr h, EnumProc lp, IntPtr l);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassNameW(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetWindowTextW(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int n);
  [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
  [DllImport("user32.dll")] public static extern void mouse_event(uint f, uint dx, uint dy, uint d, UIntPtr e);
  [DllImport("user32.dll")] public static extern bool ScreenToClient(IntPtr h, ref POINT p);
  [DllImport("user32.dll")] public static extern IntPtr WindowFromPoint(POINT p);
  [DllImport("kernel32.dll", CharSet=CharSet.Unicode)] public static extern IntPtr OpenFileMapping(uint a, bool i, string n);
  [DllImport("kernel32.dll")] public static extern IntPtr MapViewOfFile(IntPtr h, uint a, uint b, uint c, uint s);
  [DllImport("kernel32.dll")] public static extern bool UnmapViewOfFile(IntPtr p);
  [DllImport("kernel32.dll")] public static extern bool CloseHandle(IntPtr h);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L,T,R,B; }
  [StructLayout(LayoutKind.Sequential)] public struct POINT { public int X,Y; }
  [StructLayout(LayoutKind.Sequential, Pack=8)]
  public struct GpuPlaybackStatus {
    public int sequence; public double current; public double duration;
    public int playing, muted, ready, hasMedia;
  }
  public static IntPtr FindMain() {
    IntPtr found = IntPtr.Zero;
    EnumWindows((h,l) => {
      if (!IsWindowVisible(h)) return true;
      var c=new StringBuilder(256); GetClassNameW(h,c,256);
      if (c.ToString()=="ProgramWindow") { found=h; return false; }
      return true;
    }, IntPtr.Zero);
    return found;
  }
  public static List<string> DumpChildren(IntPtr parent) {
    var list = new List<string>();
    EnumChildWindows(parent, (h,l) => {
      var c=new StringBuilder(256); var t=new StringBuilder(256);
      GetClassNameW(h,c,256); GetWindowTextW(h,t,256);
      RECT r; GetWindowRect(h, out r);
      list.Add(string.Format("{0:X} {1} [{2}] {3},{4}-{5},{6}", h.ToInt64(), c, t, r.L,r.T,r.R,r.B));
      return true;
    }, IntPtr.Zero);
    return list;
  }
}
"@

function Save-Shot($name) {
  $path = Join-Path $PSScriptRoot "screenshots\$name"
  $b = [Windows.Forms.Screen]::PrimaryScreen.Bounds
  $bmp = New-Object Drawing.Bitmap $b.Width, $b.Height
  $g = [Drawing.Graphics]::FromImage($bmp)
  $g.CopyFromScreen($b.Location, [Drawing.Point]::Empty, $b.Size)
  $bmp.Save($path, [Drawing.Imaging.ImageFormat]::Png)
  $g.Dispose(); $bmp.Dispose()
  Write-Host "shot $name"
}

function Click-At([int]$x,[int]$y) {
  Write-Host "click $x,$y"
  [Probe]::SetCursorPos($x,$y) | Out-Null
  Start-Sleep -Milliseconds 50
  [Probe]::mouse_event([Probe]::MOUSEEVENTF_LEFTDOWN,0,0,0,[UIntPtr]::Zero)
  Start-Sleep -Milliseconds 40
  [Probe]::mouse_event([Probe]::MOUSEEVENTF_LEFTUP,0,0,0,[UIntPtr]::Zero)
  Start-Sleep -Milliseconds 200
}

function Sample-Fps([double]$sec) {
  $h = [Probe]::OpenFileMapping([Probe]::FILE_MAP_READ, $false, "Local\PresentationSoftwareGpuStatus")
  if ($h -eq [IntPtr]::Zero) { return "status-map-missing" }
  $v = [Probe]::MapViewOfFile($h, [Probe]::FILE_MAP_READ, 0, 0, 40)
  if ($v -eq [IntPtr]::Zero) { [Probe]::CloseHandle($h)|Out-Null; return "map-failed" }
  $sw = [Diagnostics.Stopwatch]::StartNew()
  $a = [Runtime.InteropServices.Marshal]::PtrToStructure($v, [type][Probe+GpuPlaybackStatus])
  while ($sw.Elapsed.TotalSeconds -lt $sec) { Start-Sleep -Milliseconds 16 }
  $b = [Runtime.InteropServices.Marshal]::PtrToStructure($v, [type][Probe+GpuPlaybackStatus])
  $sw.Stop()
  $dt = [Math]::Max($sw.Elapsed.TotalSeconds, 0.001)
  $seq = [Math]::Abs([int64]$b.sequence - [int64]$a.sequence)
  $hz = [Math]::Round(($seq / 2.0) / $dt, 1)
  $rate = [Math]::Round(($b.current - $a.current) / $dt, 3)
  [Probe]::UnmapViewOfFile($v)|Out-Null; [Probe]::CloseHandle($h)|Out-Null
  return "publishHz=$hz playing=$($b.playing) hasMedia=$($b.hasMedia) ready=$($b.ready) cur=$([Math]::Round($b.current,2))/$([Math]::Round($b.duration,2)) playRate=$rate seqDelta=$seq"
}

$main = [Probe]::FindMain()
if ($main -eq [IntPtr]::Zero) { throw "no ProgramWindow" }
[Probe]::ShowWindow($main, 5) | Out-Null
[Probe]::SetForegroundWindow($main) | Out-Null
Start-Sleep -Milliseconds 400
$r = New-Object Probe+RECT
[Probe]::GetWindowRect($main, [ref]$r) | Out-Null
Write-Host "main $main $($r.L),$($r.T)-$($r.R),$($r.B)"
[Probe]::DumpChildren($main) | Select-Object -First 40 | ForEach-Object { Write-Host $_ }

Save-Shot "probe2_01_before.png"

# Media cards from crop analysis: crop origin (0,520), cards roughly at
# Emerald (80,150)->(80,670), Enders (210,150)->(210,670), FireIce (340,150)->(340,670)
# second row GeoCircle (80,300)->(80,820), GeoPulse (210,300)->(210,820)
$cards = @(
  @{n='Emerald City'; x=90; y=690},
  @{n='Enders Clouds'; x=220; y=690},
  @{n='Geo Pulse-2'; x=220; y=840}
)

$lines = New-Object System.Collections.Generic.List[string]
$lines.Add("main=$main")

foreach ($c in $cards) {
  Write-Host "=== $($c.n) ==="
  Click-At $c.x $c.y
  Start-Sleep -Milliseconds 1200
  Save-Shot ("probe2_card_{0}.png" -f ($c.n -replace '[^a-zA-Z0-9]','_'))
  $fps = Sample-Fps 2.5
  $line = "$($c.n): $fps"
  Write-Host $line
  $lines.Add($line)
}

# Transition between two media items and sample during/after
Write-Host "=== transition Emerald -> Enders ==="
Click-At 90 690
Start-Sleep -Milliseconds 400
Click-At 220 690
Start-Sleep -Milliseconds 300
$transFps = Sample-Fps 3.0
$lines.Add("during/after Enders transition: $transFps")
Write-Host $lines[-1]
Save-Shot "probe2_after_transition.png"

# Try File > Import via keyboard, then type path
Write-Host "=== import via Alt+F ==="
[Probe]::SetForegroundWindow($main) | Out-Null
Start-Sleep -Milliseconds 200
[Windows.Forms.SendKeys]::SendWait("%f")
Start-Sleep -Milliseconds 400
Save-Shot "probe2_file_menu.png"
# Try 'i' for Import if underlined
[Windows.Forms.SendKeys]::SendWait("i")
Start-Sleep -Milliseconds 800
Save-Shot "probe2_after_import_key.png"

# Look for any new top-level dialogs
$dialogs = @()
$cb = [Probe+EnumProc]{
  param($h,$l)
  if (-not [Probe]::IsWindowVisible($h)) { return $true }
  $sb = New-Object Text.StringBuilder 256
  [void][Probe]::GetClassNameW($h, $sb, 256)
  $tb = New-Object Text.StringBuilder 256
  [void][Probe]::GetWindowTextW($h, $tb, 256)
  if ($sb.ToString() -eq '#32770' -or $tb.ToString() -match 'Import|Open') {
    $script:dialogs += "$h $($sb) [$($tb)]"
  }
  return $true
}
[void][Probe]::EnumWindows($cb, [IntPtr]::Zero)
$lines.Add("dialogs: $($dialogs -join ' | ')")
Write-Host $lines[-1]

if ($dialogs.Count -gt 0) {
  $path = Join-Path $PSScriptRoot "Assets\mockups\transition_6_cover.mp4"
  [Windows.Forms.SendKeys]::SendWait("^a")
  Start-Sleep -Milliseconds 50
  [Windows.Forms.SendKeys]::SendWait($path)
  Start-Sleep -Milliseconds 200
  [Windows.Forms.SendKeys]::SendWait("{ENTER}")
  Start-Sleep -Seconds 1
  Save-Shot "probe2_after_import_file.png"
  $lines.Add("imported $path")
  # Click a lower card that may be the new one - scroll not handled; click first card again
  Click-At 90 690
  Start-Sleep -Milliseconds 1000
  $lines.Add("after import click: $(Sample-Fps 2.0)")
}

$out = Join-Path $PSScriptRoot "screenshots\probe2_report.txt"
$lines | Set-Content $out -Encoding UTF8
Write-Host "REPORT:"
$lines | ForEach-Object { Write-Host $_ }
