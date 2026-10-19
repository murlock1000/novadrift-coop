# Focus and capture only a verified game client. No key presses or settings changes.
param(
 [Parameter(Mandatory=$true)][int]$TargetPid,
 [Parameter(Mandatory=$true)][long]$ExpectedStartTicks,
 [Parameter(Mandatory=$true)][long]$GameWindowHandle,
 [Parameter(Mandatory=$true)][string]$ExpectedSha256,
 [Parameter(Mandatory=$true)][string]$OutputPath
)
$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.Drawing
Add-Type @'
using System;using System.Runtime.InteropServices;
public static class GameCapture {
 [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
 [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
 [DllImport("user32.dll")] public static extern void SwitchToThisWindow(IntPtr h,bool altTab);
 [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h,out uint p);
 [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h,out Rect r);
 [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h,ref Point p);
 [DllImport("user32.dll")] public static extern IntPtr SetThreadDpiAwarenessContext(IntPtr context);
 public struct Point {public int x,y;}
 public struct Rect {public int left,top,right,bottom;}
}
'@
$p=Get-Process -Id $TargetPid
if($p.ProcessName -ne 'NovaDrift' -or $p.StartTime.ToUniversalTime().Ticks -ne $ExpectedStartTicks){throw 'Owned process identity changed'}
if((Get-FileHash -LiteralPath $p.Path -Algorithm SHA256).Hash -ne $ExpectedSha256){throw 'Owned executable identity differs'}
$h=[IntPtr]$GameWindowHandle;[uint32]$owner=0
$null=[GameCapture]::GetWindowThreadProcessId($h,[ref]$owner)
if($owner -ne $TargetPid){throw 'Window is not owned by the verified game'}
if([GameCapture]::GetForegroundWindow() -ne $h){$null=[GameCapture]::SetForegroundWindow($h);[GameCapture]::SwitchToThisWindow($h,$true)}
for($i=0;$i -lt 20 -and [GameCapture]::GetForegroundWindow() -ne $h;$i++){Start-Sleep -Milliseconds 50}
if([GameCapture]::GetForegroundWindow() -ne $h){throw 'Owned game must be foreground for capture'}
$old=[GameCapture]::SetThreadDpiAwarenessContext([IntPtr](-4))
if($old -eq [IntPtr]::Zero){throw 'Physical-pixel coordinates unavailable'}
try{
 $r=New-Object GameCapture+Rect;$point=New-Object GameCapture+Point
 if(-not [GameCapture]::GetClientRect($h,[ref]$r) -or -not [GameCapture]::ClientToScreen($h,[ref]$point)){throw 'Client bounds unavailable'}
 if($r.right -lt 640 -or $r.bottom -lt 360){throw 'Game client is minimized or too small for visual review'}
 $bitmap=New-Object Drawing.Bitmap($r.right,$r.bottom);$graphics=[Drawing.Graphics]::FromImage($bitmap)
 try{
  $graphics.CopyFromScreen($point.x,$point.y,0,0,$bitmap.Size)
  if([GameCapture]::GetForegroundWindow() -ne $h){throw 'Focus changed during capture'}
  $bitmap.Save($OutputPath)
 }finally{$graphics.Dispose();$bitmap.Dispose()}
 [pscustomobject]@{pid=$TargetPid;start_ticks=$ExpectedStartTicks;width=$r.right;height=$r.bottom;client_only=$true}|ConvertTo-Json -Compress
}finally{$null=[GameCapture]::SetThreadDpiAwarenessContext($old)}
