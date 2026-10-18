# Passive window/monitor geometry only. Does not focus, resize or send input.
param(
 [Parameter(Mandatory=$true)][int]$TargetPid,
 [Parameter(Mandatory=$true)][long]$ExpectedStartTicks,
 [Parameter(Mandatory=$true)][string]$ExpectedSha256
)
$ErrorActionPreference='Stop'
Add-Type @'
using System;using System.Text;using System.Collections.Generic;using System.Runtime.InteropServices;
public static class DisplayGeometry {
 public delegate bool Callback(IntPtr window,IntPtr data);
 [DllImport("user32.dll")] static extern bool EnumWindows(Callback fn,IntPtr data);
 [DllImport("user32.dll")] static extern bool IsWindowVisible(IntPtr window);
 [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr window,out uint process);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)] public static extern int GetWindowText(IntPtr window,StringBuilder text,int length);
 [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr window,out Rect rect);
 [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr window,out Rect rect);
 [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr window,ref Point point);
 [DllImport("user32.dll")] public static extern IntPtr MonitorFromWindow(IntPtr window,uint flags);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)] public static extern bool GetMonitorInfo(IntPtr monitor,ref MonitorInfo info);
 [DllImport("user32.dll",EntryPoint="GetWindowLongPtrW")] public static extern IntPtr GetStyle(IntPtr window,int index);
 [DllImport("user32.dll")] public static extern uint GetDpiForWindow(IntPtr window);
 [DllImport("user32.dll")] public static extern IntPtr SetThreadDpiAwarenessContext(IntPtr context);
 [DllImport("dwmapi.dll")] public static extern int DwmGetWindowAttribute(IntPtr window,uint attribute,out Rect rect,int size);
 public struct Rect {public int left,top,right,bottom;}
 public struct Point {public int x,y;}
 [StructLayout(LayoutKind.Sequential,CharSet=CharSet.Unicode)] public struct MonitorInfo {
  public int size;public Rect monitor,work;public uint flags;[MarshalAs(UnmanagedType.ByValTStr,SizeConst=32)]public string device;
 }
 public static long[] Windows(uint pid){var list=new List<long>();EnumWindows((window,data)=>{uint process;GetWindowThreadProcessId(window,out process);if(process==pid&&IsWindowVisible(window))list.Add(window.ToInt64());return true;},IntPtr.Zero);return list.ToArray();}
}
'@
$p=Get-Process -Id $TargetPid -ErrorAction Stop
if($p.StartTime.ToUniversalTime().Ticks -ne $ExpectedStartTicks){throw 'Owned PID creation time changed'}
if((Get-FileHash -LiteralPath $p.Path -Algorithm SHA256).Hash -ne $ExpectedSha256){throw 'Owned executable identity differs'}
$oldDpi=[DisplayGeometry]::SetThreadDpiAwarenessContext([IntPtr](-4))
if($oldDpi -eq [IntPtr]::Zero){throw 'Per-monitor DPI awareness could not be set; physical geometry would be unreliable'}
function RectInfo($r){return [ordered]@{left=$r.left;top=$r.top;right=$r.right;bottom=$r.bottom;width=($r.right-$r.left);height=($r.bottom-$r.top)}}
try {
 $windows=@(foreach($handle in [DisplayGeometry]::Windows($TargetPid)){
  $h=[IntPtr]$handle;$window=New-Object DisplayGeometry+Rect;$client=New-Object DisplayGeometry+Rect;$frame=New-Object DisplayGeometry+Rect
  $point=New-Object DisplayGeometry+Point
  if(-not [DisplayGeometry]::GetWindowRect($h,[ref]$window) -or -not [DisplayGeometry]::GetClientRect($h,[ref]$client) -or -not [DisplayGeometry]::ClientToScreen($h,[ref]$point)){throw 'Window geometry unavailable'}
  $screen=New-Object DisplayGeometry+Rect;$screen.left=$point.x;$screen.top=$point.y;$screen.right=$point.x+$client.right;$screen.bottom=$point.y+$client.bottom
  $monitor=New-Object DisplayGeometry+MonitorInfo;$monitor.size=[Runtime.InteropServices.Marshal]::SizeOf($monitor)
  if(-not [DisplayGeometry]::GetMonitorInfo([DisplayGeometry]::MonitorFromWindow($h,2),[ref]$monitor)){throw 'Monitor geometry unavailable'}
  $title=New-Object Text.StringBuilder 1024;$null=[DisplayGeometry]::GetWindowText($h,$title,$title.Capacity)
  $frameResult=[DisplayGeometry]::DwmGetWindowAttribute($h,9,[ref]$frame,[Runtime.InteropServices.Marshal]::SizeOf($frame))
  $style=[DisplayGeometry]::GetStyle($h,-16).ToInt64();$extended=[DisplayGeometry]::GetStyle($h,-20).ToInt64()
  $covers=[Math]::Abs($screen.left-$monitor.monitor.left) -le 2 -and [Math]::Abs($screen.top-$monitor.monitor.top) -le 2 -and [Math]::Abs($screen.right-$monitor.monitor.right) -le 2 -and [Math]::Abs($screen.bottom-$monitor.monitor.bottom) -le 2
  [ordered]@{handle=$handle;title=$title.ToString();dpi=[DisplayGeometry]::GetDpiForWindow($h);window=(RectInfo $window);client_pixels=(RectInfo $client);client_screen=(RectInfo $screen);extended_frame=$(if($frameResult -eq 0){RectInfo $frame}else{$null});monitor=(RectInfo $monitor.monitor);work_area=(RectInfo $monitor.work);monitor_device=$monitor.device;style=('0x{0:X}' -f $style);extended_style=('0x{0:X}' -f $extended);has_caption=(($style -band 0x00C00000) -ne 0);client_covers_monitor=$covers}
 })
 [ordered]@{pid=$TargetPid;start_ticks=$ExpectedStartTicks;exe=$p.Path;passive=$true;dpi_aware=$true;windows=$windows;limitation='Monitor coverage cannot distinguish exclusive fullscreen from borderless fullscreen.'}|ConvertTo-Json -Depth 7 -Compress
}finally {if($oldDpi -ne [IntPtr]::Zero){$null=[DisplayGeometry]::SetThreadDpiAwarenessContext($oldDpi)}}
