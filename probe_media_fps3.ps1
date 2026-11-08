$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
Add-Type -AssemblyName UIAutomationClient
Add-Type -AssemblyName UIAutomationTypes

Add-Type @"
using System;
using System.Text;
using System.Runtime.InteropServices;
public static class P3 {
  public const uint MOUSEEVENTF_LEFTDOWN = 0x0002;
  public const uint MOUSEEVENTF_LEFTUP = 0x0004;
  public const uint FILE_MAP_READ = 0x0004;
  public delegate bool EnumProc(IntPtr h, IntPtr l);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc lp, IntPtr l);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassNameW(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetWindowTextW(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int n);
  [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
  [DllImport("user32.dll")] public static extern void mouse_event(uint f, uint dx, uint dy, uint d, UIntPtr e);
  [DllImport("kernel32.dll", CharSet=CharSet.Unicode)] public static extern IntPtr OpenFileMapping(uint a, bool i, string n);
  [DllImport("kernel32.dll")] public static extern IntPtr MapViewOfFile(IntPtr h, uint a, uint b, uint c, uint s);
  [DllImport("kernel32.dll")] public static extern bool UnmapViewOfFile(IntPtr p);
  [DllImport("kernel32.dll")] public static extern bool CloseHandle(IntPtr h);
  [StructLayout(LayoutKind.Sequential, Pack=8)]
  public struct GpuPlaybackStatus {
    public int sequence; public double current; public double duration;
    public int playing, muted, ready, hasMedia;
  }
  public static IntPtr FindClass(string cls) {
    IntPtr found = IntPtr.Zero;
    EnumWindows((h,l) => {
      if (!IsWindowVisible(h)) return true;
      var c=new StringBuilder(256); GetClassNameW(h,c,256);
      if (c.ToString()==cls) { found=h; return false; }
      return true;
    }, IntPtr.Zero);
    return found;
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
  [P3]::SetCursorPos($x,$y) | Out-Null
  Start-Sleep -Milliseconds 40
  [P3]::mouse_event([P3]::MOUSEEVENTF_LEFTDOWN,0,0,0,[UIntPtr]::Zero)
  Start-Sleep -Milliseconds 35
  [P3]::mouse_event([P3]::MOUSEEVENTF_LEFTUP,0,0,0,[UIntPtr]::Zero)
  Start-Sleep -Milliseconds 150
}

function Sample-Fps([double]$sec) {
  $h = [P3]::OpenFileMapping([P3]::FILE_MAP_READ, $false, "Local\PresentationSoftwareGpuStatus")
  if ($h -eq [IntPtr]::Zero) { return "status-map-missing" }
  $v = [P3]::MapViewOfFile($h, [P3]::FILE_MAP_READ, 0, 0, 40)
  if ($v -eq [IntPtr]::Zero) { [P3]::CloseHandle($h)|Out-Null; return "map-failed" }
  $sw = [Diagnostics.Stopwatch]::StartNew()
  $a = [Runtime.InteropServices.Marshal]::PtrToStructure($v, [type][P3+GpuPlaybackStatus])
  while ($sw.Elapsed.TotalSeconds -lt $sec) { Start-Sleep -Milliseconds 16 }
  $b = [Runtime.InteropServices.Marshal]::PtrToStructure($v, [type][P3+GpuPlaybackStatus])
  $sw.Stop()
  $dt = [Math]::Max($sw.Elapsed.TotalSeconds, 0.001)
  $seq = [Math]::Abs([int64]$b.sequence - [int64]$a.sequence)
  $hz = [Math]::Round(($seq / 2.0) / $dt, 1)
  $rate = [Math]::Round(($b.current - $a.current) / $dt, 3)
  [P3]::UnmapViewOfFile($v)|Out-Null; [P3]::CloseHandle($h)|Out-Null
  return "publishHz=$hz playing=$($b.playing) hasMedia=$($b.hasMedia) ready=$($b.ready) cur=$([Math]::Round($b.current,2))/$([Math]::Round($b.duration,2)) playRate=$rate"
}

$lines = New-Object System.Collections.Generic.List[string]
Save-Shot "probe3_00_start.png"

$root = [Windows.Automation.AutomationElement]::RootElement
$condWin = New-Object Windows.Automation.PropertyCondition(
  [Windows.Automation.AutomationElement]::ControlTypeProperty,
  [Windows.Automation.ControlType]::Window)
$wins = $root.FindAll([Windows.Automation.TreeScope]::Children, $condWin)
$importEl = $null
for ($i=0; $i -lt $wins.Count; $i++) {
  $n = $wins.Item($i).Current.Name
  Write-Host "topwin: $n"
  if ($n -eq 'Import' -or $n -like 'Import*') { $importEl = $wins.Item($i); break }
}

if ($importEl -ne $null) {
  Write-Host "Import dialog found - selecting Blue Soda"
  $importEl.SetFocus()
  Start-Sleep -Milliseconds 200
  [Windows.Forms.SendKeys]::SendWait("Blue Soda")
  Start-Sleep -Milliseconds 400
  Save-Shot "probe3_01_typed_name.png"
  [Windows.Forms.SendKeys]::SendWait("{ENTER}")
  Start-Sleep -Seconds 2
  Save-Shot "probe3_02_after_import.png"
  $lines.Add("imported via dialog (Blue Soda)")
} else {
  $lines.Add("no import dialog - clicking toolbar import")
  Click-At 1880 70
  Start-Sleep -Milliseconds 800
  Save-Shot "probe3_01b_import_clicked.png"
}

$main = [P3]::FindClass("ProgramWindow")
[P3]::ShowWindow($main,5)|Out-Null
[P3]::SetForegroundWindow($main)|Out-Null
Start-Sleep -Milliseconds 400

$targets = @(
  @{n='card_r0c0'; x=90;  y=700},
  @{n='card_r0c1'; x=230; y=700},
  @{n='card_r0c2'; x=370; y=700},
  @{n='card_r1c0'; x=90;  y=850},
  @{n='card_r1c1'; x=230; y=850}
)

foreach ($t in $targets) {
  Write-Host "=== $($t.n) ==="
  [P3]::SetForegroundWindow($main)|Out-Null
  Click-At $t.x $t.y
  Start-Sleep -Milliseconds 1400
  $shotName = "probe3_$($t.n).png"
  Save-Shot $shotName
  $src = [Drawing.Bitmap]::FromFile((Join-Path $PSScriptRoot "screenshots\$shotName"))
  $preview = $src.Clone((New-Object Drawing.Rectangle 18,116,457,129), $src.PixelFormat)
  $previewPath = Join-Path $PSScriptRoot ("screenshots\probe3_$($t.n)_preview.png")
  $preview.Save($previewPath, [Drawing.Imaging.ImageFormat]::Png)
  $sum=0L; $nPix=0
  for ($yy=10; $yy -lt 120; $yy+=4) {
    for ($xx=10; $xx -lt 440; $xx+=4) {
      $px = $preview.GetPixel($xx,$yy)
      $sum += [int]$px.R + $px.G + $px.B
      $nPix++
    }
  }
  $avg = [Math]::Round($sum / [Math]::Max($nPix,1) / 3.0, 1)
  $preview.Dispose(); $src.Dispose()
  $fps = Sample-Fps 2.2
  $line = "$($t.n): previewAvgLum=$avg $fps"
  Write-Host $line
  $lines.Add($line)
}

Write-Host "=== media transition ==="
Click-At 90 700
Start-Sleep -Milliseconds 500
Click-At 230 700
$lines.Add("transition sample: $(Sample-Fps 3.0)")
Write-Host $lines[-1]
Save-Shot "probe3_transition.png"

$out = Join-Path $PSScriptRoot "screenshots\probe3_report.txt"
$lines | Set-Content $out -Encoding UTF8
Write-Host "==== REPORT ===="
Get-Content $out
