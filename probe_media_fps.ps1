$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
Add-Type -AssemblyName UIAutomationClient
Add-Type -AssemblyName UIAutomationTypes

Add-Type @"
using System;
using System.Runtime.InteropServices;
public static class NativeUi {
  public const uint WM_COMMAND = 0x0111;
  public const uint WM_LBUTTONDOWN = 0x0201;
  public const uint WM_LBUTTONUP = 0x0202;
  public const uint MK_LBUTTON = 0x0001;
  public const int IDM_FILE_IMPORT = 1032;
  public delegate bool EnumProc(IntPtr hWnd, IntPtr lParam);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc lp, IntPtr l);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr hWnd);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassNameW(IntPtr hWnd, System.Text.StringBuilder sb, int n);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hWnd);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr hWnd, out RECT r);
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);
  [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr hWnd, uint msg, IntPtr wParam, IntPtr lParam);
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr hWnd, uint msg, IntPtr wParam, IntPtr lParam);
  [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr hWnd, ref POINT pt);
  [DllImport("user32.dll")] public static extern bool ScreenToClient(IntPtr hWnd, ref POINT pt);
  [DllImport("user32.dll")] public static extern void mouse_event(uint flags, uint dx, uint dy, uint data, UIntPtr extra);
  [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
  [DllImport("user32.dll")] public static extern IntPtr WindowFromPoint(POINT pt);
  [DllImport("user32.dll", CharSet=CharSet.Auto)] public static extern int GetClassName(IntPtr hWnd, System.Text.StringBuilder sb, int n);
  [DllImport("user32.dll", CharSet=CharSet.Auto)] public static extern int GetWindowText(IntPtr hWnd, System.Text.StringBuilder sb, int n);
  [DllImport("kernel32.dll", CharSet=CharSet.Unicode)] public static extern IntPtr OpenFileMapping(uint access, bool inherit, string name);
  [DllImport("kernel32.dll")] public static extern IntPtr MapViewOfFile(IntPtr h, uint access, uint hi, uint lo, UIntPtr bytes);
  [DllImport("kernel32.dll")] public static extern bool UnmapViewOfFile(IntPtr p);
  [DllImport("kernel32.dll")] public static extern bool CloseHandle(IntPtr h);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
  [StructLayout(LayoutKind.Sequential)] public struct POINT { public int X, Y; }
  [StructLayout(LayoutKind.Sequential, Pack=8)]
  public struct GpuPlaybackStatus {
    public int sequence;
    public double current;
    public double duration;
    public int playing;
    public int muted;
    public int ready;
    public int hasMedia;
  }
  public const uint FILE_MAP_READ = 0x0004;
  public const uint MOUSEEVENTF_LEFTDOWN = 0x0002;
  public const uint MOUSEEVENTF_LEFTUP = 0x0004;
  public static IntPtr MakeLParam(int x, int y) { return (IntPtr)((y << 16) | (x & 0xFFFF)); }
}
"@

function Save-Shot([string]$name) {
  $path = Join-Path $PSScriptRoot "screenshots\$name"
  $b = [System.Windows.Forms.Screen]::PrimaryScreen.Bounds
  $bmp = New-Object System.Drawing.Bitmap $b.Width, $b.Height
  $g = [System.Drawing.Graphics]::FromImage($bmp)
  $g.CopyFromScreen($b.Location, [System.Drawing.Point]::Empty, $b.Size)
  $bmp.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
  $g.Dispose(); $bmp.Dispose()
  Write-Host "shot $path"
  return $path
}

function Click-Screen([int]$x, [int]$y) {
  [NativeUi]::SetCursorPos($x, $y) | Out-Null
  Start-Sleep -Milliseconds 40
  [NativeUi]::mouse_event([NativeUi]::MOUSEEVENTF_LEFTDOWN, 0, 0, 0, [UIntPtr]::Zero)
  Start-Sleep -Milliseconds 30
  [NativeUi]::mouse_event([NativeUi]::MOUSEEVENTF_LEFTUP, 0, 0, 0, [UIntPtr]::Zero)
  Start-Sleep -Milliseconds 120
}

function Get-MainWindow {
  $script:foundHwnd = [IntPtr]::Zero
  $cb = [NativeUi+EnumProc]{
    param([IntPtr]$h, [IntPtr]$l)
    if (-not [NativeUi]::IsWindowVisible($h)) { return $true }
    $sb = New-Object System.Text.StringBuilder 256
    [void][NativeUi]::GetClassNameW($h, $sb, 256)
    $cls = $sb.ToString()
    if ($cls -eq 'ProgramWindow' -or $cls -eq 'MyCppWindow') {
      $script:foundHwnd = $h
      return $false
    }
    return $true
  }
  [void][NativeUi]::EnumWindows($cb, [IntPtr]::Zero)
  if ($script:foundHwnd -eq [IntPtr]::Zero) {
    $p = Get-Process Window -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($p -and $p.MainWindowHandle -ne 0) { return [IntPtr]$p.MainWindowHandle }
    throw "ProgramWindow/MyCppWindow not found"
  }
  return $script:foundHwnd
}

function Sample-GpuStatus([double]$seconds) {
  $h = [NativeUi]::OpenFileMapping([NativeUi]::FILE_MAP_READ, $false, "Local\PresentationSoftwareGpuStatus")
  if ($h -eq [IntPtr]::Zero) { return @{ ok=$false; note="status map missing" } }
  $view = [NativeUi]::MapViewOfFile($h, [NativeUi]::FILE_MAP_READ, 0, 0, [UIntPtr]40)
  if ($view -eq [IntPtr]::Zero) {
    [NativeUi]::CloseHandle($h) | Out-Null
    return @{ ok=$false; note="map view failed" }
  }
  $sw = [Diagnostics.Stopwatch]::StartNew()
  $first = [Runtime.InteropServices.Marshal]::PtrToStructure($view, [type][NativeUi+GpuPlaybackStatus])
  $last = $first
  $samples = 0
  while ($sw.Elapsed.TotalSeconds -lt $seconds) {
    Start-Sleep -Milliseconds 16
    $last = [Runtime.InteropServices.Marshal]::PtrToStructure($view, [type][NativeUi+GpuPlaybackStatus])
    $samples++
  }
  $sw.Stop()
  $dt = [Math]::Max($sw.Elapsed.TotalSeconds, 0.001)
  # PublishStatus increments sequence twice per tick
  $seqDelta = [Math]::Abs([int64]$last.sequence - [int64]$first.sequence)
  $publishHz = $seqDelta / 2.0 / $dt
  $playbackRate = ($last.current - $first.current) / $dt
  [NativeUi]::UnmapViewOfFile($view) | Out-Null
  [NativeUi]::CloseHandle($h) | Out-Null
  return @{
    ok=$true
    seconds=[Math]::Round($dt,3)
    samples=$samples
    seqDelta=$seqDelta
    publishHz=[Math]::Round($publishHz,1)
    playbackRate=[Math]::Round($playbackRate,3)
    playing=$last.playing
    hasMedia=$last.hasMedia
    ready=$last.ready
    current=[Math]::Round($last.current,3)
    duration=[Math]::Round($last.duration,3)
  }
}

function Import-MediaFile([string]$filePath) {
  $hwnd = Get-MainWindow
  [NativeUi]::ShowWindow($hwnd, 5) | Out-Null
  [NativeUi]::SetForegroundWindow($hwnd) | Out-Null
  Start-Sleep -Milliseconds 300
  [NativeUi]::PostMessage($hwnd, [NativeUi]::WM_COMMAND, [IntPtr][NativeUi]::IDM_FILE_IMPORT, [IntPtr]::Zero) | Out-Null

  $deadline = (Get-Date).AddSeconds(8)
  $root = [Windows.Automation.AutomationElement]::RootElement
  $dialog = $null
  while ((Get-Date) -lt $deadline -and $dialog -eq $null) {
    Start-Sleep -Milliseconds 150
    $cond = New-Object Windows.Automation.PropertyCondition(
      [Windows.Automation.AutomationElement]::ControlTypeProperty,
      [Windows.Automation.ControlType]::Window)
    $wins = $root.FindAll([Windows.Automation.TreeScope]::Children, $cond)
    for ($i=0; $i -lt $wins.Count; $i++) {
      $n = $wins.Item($i).Current.Name
      if ($n -eq 'Import' -or $n -like '*Import*Open*' -or $n -eq 'Open') {
        $dialog = $wins.Item($i)
        break
      }
    }
  }
  if ($dialog -eq $null) { throw "Import dialog did not appear" }

  # Filename edit (ComboBox or Edit)
  $editCond = New-Object Windows.Automation.PropertyCondition(
    [Windows.Automation.AutomationElement]::ControlTypeProperty,
    [Windows.Automation.ControlType]::Edit)
  $edits = $dialog.FindAll([Windows.Automation.TreeScope]::Descendants, $editCond)
  $fileEdit = $null
  for ($i=0; $i -lt $edits.Count; $i++) {
    $e = $edits.Item($i)
    # Prefer the bottom filename box: ClassName Edit or Name "File name:"
    if ($e.Current.Name -match 'File name|File Name' -or $e.Current.AutomationId -eq '1148') {
      $fileEdit = $e
      break
    }
  }
  if ($fileEdit -eq $null -and $edits.Count -gt 0) {
    $fileEdit = $edits.Item($edits.Count - 1)
  }
  if ($fileEdit -eq $null) { throw "Could not find filename edit" }

  $fileEdit.SetFocus()
  Start-Sleep -Milliseconds 100
  [System.Windows.Forms.SendKeys]::SendWait("^a")
  Start-Sleep -Milliseconds 50
  [System.Windows.Forms.SendKeys]::SendWait($filePath.Replace('+','{+}'))
  Start-Sleep -Milliseconds 200
  [System.Windows.Forms.SendKeys]::SendWait("{ENTER}")
  Start-Sleep -Milliseconds 800
}

# --- run ---
$report = @()
$hwnd = Get-MainWindow
$rect = New-Object NativeUi+RECT
[NativeUi]::GetWindowRect($hwnd, [ref]$rect) | Out-Null
Write-Host "Window rect $($rect.Left),$($rect.Top) - $($rect.Right),$($rect.Bottom)"
Save-Shot "probe_01_launch.png" | Out-Null
$report += "launch ok hwnd=$hwnd"

# Import two mockup videos as media-tab items
$mediaA = Join-Path $PSScriptRoot "Assets\mockups\transition_2_crossfade.mp4"
$mediaB = Join-Path $PSScriptRoot "Assets\mockups\transition_5_wipe.mp4"
Import-MediaFile $mediaA
$report += "imported A"
Start-Sleep -Milliseconds 500
Save-Shot "probe_02_after_import_a.png" | Out-Null
Import-MediaFile $mediaB
$report += "imported B"
Start-Sleep -Milliseconds 700
Save-Shot "probe_03_after_import_b.png" | Out-Null

# Click Media tab area then first couple of cards in the left library.
# Left panel is the left portion of the program window; cards sit under Media/Playlists/Transitions tabs.
$left = $rect.Left
$top = $rect.Top
$width = $rect.Right - $rect.Left
$height = $rect.Bottom - $rect.Top

# Approximate: program chrome + left lib starts near left edge after launch splash.
# Click "Media" tab
Click-Screen ($left + 70) ($top + 110)
Start-Sleep -Milliseconds 200

# First media card center-ish
Click-Screen ($left + 100) ($top + 200)
Start-Sleep -Milliseconds 900
Save-Shot "probe_04_after_click_card1.png" | Out-Null
$fps1 = Sample-GpuStatus 2.5
$report += ("card1 fps publishHz={0} playing={1} hasMedia={2} ready={3} cur={4}/{5} rate={6}" -f `
  $fps1.publishHz, $fps1.playing, $fps1.hasMedia, $fps1.ready, $fps1.current, $fps1.duration, $fps1.playbackRate)

# Second card
Click-Screen ($left + 260) ($top + 200)
Start-Sleep -Milliseconds 1200
Save-Shot "probe_05_after_click_card2.png" | Out-Null
$fps2 = Sample-GpuStatus 2.5
$report += ("card2 fps publishHz={0} playing={1} hasMedia={2} ready={3} cur={4}/{5} rate={6}" -f `
  $fps2.publishHz, $fps2.playing, $fps2.hasMedia, $fps2.ready, $fps2.current, $fps2.duration, $fps2.playbackRate)

# Click first again to force BG transition between media items
Click-Screen ($left + 100) ($top + 200)
Start-Sleep -Milliseconds 1500
Save-Shot "probe_06_after_reclick_card1.png" | Out-Null
$fps3 = Sample-GpuStatus 2.5
$report += ("reclick1 fps publishHz={0} playing={1} hasMedia={2} ready={3} cur={4}/{5} rate={6}" -f `
  $fps3.publishHz, $fps3.playing, $fps3.hasMedia, $fps3.ready, $fps3.current, $fps3.duration, $fps3.playbackRate)

$out = Join-Path $PSScriptRoot "screenshots\probe_report.txt"
$report | Set-Content -Path $out -Encoding UTF8
$report | ForEach-Object { Write-Host $_ }
Write-Host "report -> $out"
