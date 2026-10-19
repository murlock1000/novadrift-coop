param(
    [Parameter(Mandatory=$true)][int]$TargetPid,
    [Parameter(Mandatory=$true)][string]$ExpectedSha256,
    [ValidateSet('status','focus','press','close','mouse')][string]$Action='status',
    [ValidateSet('leftDown','leftUp','rightDown','rightUp','middleDown','middleUp')][string]$MouseEvent='leftUp',
    [string]$Keys='ENTER',
    [ValidateRange(10,5000)][int]$HoldMs=120,
    [ValidateRange(0,5000)][int]$DelayMs=200,
    [long]$ExpectedStartTicks=0,
    [switch]$AllowBackground
)
$ErrorActionPreference='Stop'
Add-Type @'
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;
public static class NovaWindow {
 [StructLayout(LayoutKind.Sequential)] public struct MouseInput {public int dx,dy;public uint data,flags,time;public UIntPtr extra;}
 [StructLayout(LayoutKind.Sequential)] public struct Input {public uint type;public MouseInput mouse;}
 [DllImport("user32.dll")] static extern uint SendInput(uint count,Input[] inputs,int size);
 public static void MouseButton(uint flags) {
  var input=new Input();input.mouse.flags=flags;
  if(SendInput(1,new[]{input},Marshal.SizeOf(typeof(Input)))!=1)throw new Exception("Owned mouse SendInput failed");
 }
 public delegate bool EnumProc(IntPtr hwnd, IntPtr data);
 [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc callback, IntPtr data);
 [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr hwnd, out uint process);
 [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr hwnd);
 [DllImport("user32.dll")] public static extern bool IsHungAppWindow(IntPtr hwnd);
 [DllImport("user32.dll")] public static extern bool IsIconic(IntPtr hwnd);
 [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr hwnd,int command);
 [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hwnd);
 [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
 [DllImport("user32.dll")] public static extern bool AttachThreadInput(uint first,uint second,bool attach);
 [DllImport("kernel32.dll")] public static extern uint GetCurrentThreadId();
 [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetWindowText(IntPtr hwnd, StringBuilder text, int count);
 [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr hwnd,uint message,UIntPtr wparam,IntPtr lparam);
 [DllImport("user32.dll")] public static extern uint MapVirtualKey(uint code,uint type);
 public static long[] Find(uint process) {
  var list=new List<long>();
  EnumWindows((hwnd,data)=>{ uint owner; GetWindowThreadProcessId(hwnd,out owner); if(owner==process && IsWindowVisible(hwnd)) list.Add(hwnd.ToInt64()); return true; },IntPtr.Zero);
  return list.ToArray();
 }
 public static string Title(IntPtr hwnd) {var b=new StringBuilder(1024);GetWindowText(hwnd,b,b.Capacity);return b.ToString();}
 public static bool Focus(IntPtr hwnd) {
  uint owner; uint foreground=GetWindowThreadProcessId(GetForegroundWindow(),out owner);
  uint current=GetCurrentThreadId(); bool attached=false;
  try {
   if(current!=foreground) attached=AttachThreadInput(current,foreground,true);
   if(IsIconic(hwnd))ShowWindow(hwnd,9); SetForegroundWindow(hwnd);
  } finally {if(attached) AttachThreadInput(current,foreground,false);}
  return GetForegroundWindow()==hwnd;
 }
 public static void Key(IntPtr hwnd,uint key,bool up) {
  long bits=1L | ((long)MapVirtualKey(key,0)<<16);
  if(up) bits|=(1L<<30)|(1L<<31);
  if(!PostMessage(hwnd,up?0x101u:0x100u,new UIntPtr(key),new IntPtr(bits))) throw new Exception("PostMessage failed");
 }
}
'@
function OwnedProcess {
 $process=Get-Process -Id $TargetPid
 if($process.ProcessName -ne 'NovaDrift') {throw 'Target PID is not NovaDrift'}
 if($ExpectedStartTicks -ne 0 -and $process.StartTime.ToUniversalTime().Ticks -ne $ExpectedStartTicks) {throw 'Target PID creation time changed'}
 $actual=(Get-FileHash -LiteralPath $process.MainModule.FileName -Algorithm SHA256).Hash
 if($actual -ne $ExpectedSha256) {throw 'Target executable SHA256 differs from requested copied artifact'}
 return $process
}
$process=OwnedProcess
$mouseInfo=$null
$handles=@([NovaWindow]::Find([uint32]$TargetPid))
$windows=@($handles | ForEach-Object { [ordered]@{handle=$_;title=[NovaWindow]::Title([IntPtr]$_);hung=[NovaWindow]::IsHungAppWindow([IntPtr]$_)} })
if($Action -eq 'press' -or $Action -eq 'focus' -or $Action -eq 'close' -or $Action -eq 'mouse') {
 if($handles.Count -eq 0) {throw 'No visible window belongs to the target process'}
 $chosen=@($windows | Where-Object {$_.title -match '^Nova Drift'})
 if($chosen.Count -eq 0) {throw 'Game window not visible yet (Nova Drift title required; console excluded)'}
 if($chosen.Count -ne 1) {throw 'Target process has ambiguous Nova Drift game windows'}
 $hwnd=[IntPtr]$chosen[0].handle
 if($Action -eq 'close') {
  if(-not [NovaWindow]::PostMessage($hwnd,0x10,[UIntPtr]::Zero,[IntPtr]::Zero)) {throw 'WM_CLOSE to owned window failed'}
 } else {
 if($chosen[0].hung) {throw 'Target window is hung'}
 if(-not [NovaWindow]::Focus($hwnd) -and -not $AllowBackground) {throw 'Windows did not focus the requested owned game window'}
 if($Action -eq 'mouse') {
  $null=OwnedProcess
  if([NovaWindow]::GetForegroundWindow() -ne $hwnd) {throw 'Mouse input requires the verified game in foreground'}
  $flags=@{leftDown=2;leftUp=4;rightDown=8;rightUp=16;middleDown=32;middleUp=64}
  [NovaWindow]::MouseButton([uint32]$flags[$MouseEvent])
  $mouseInfo=[ordered]@{event=$MouseEvent}

 }
 $map=@{ENTER=13;ESCAPE=27;SPACE=32;UP=38;DOWN=40;LEFT=37;RIGHT=39;TAB=9;F7=118;F8=119}
 if($Action -eq 'press') {foreach($name in $Keys.Split(',')) {
  $null=OwnedProcess
  [uint32]$owner=0
  $null=[NovaWindow]::GetWindowThreadProcessId($hwnd,[ref]$owner)
  if($owner -ne $TargetPid) {throw 'Target window ownership changed'}
  if(-not $AllowBackground -and [NovaWindow]::GetForegroundWindow() -ne $hwnd) {throw 'Owned game window lost focus before key input'}
  $key=$name.Trim().ToUpperInvariant()
  if($map.ContainsKey($key)) {$code=$map[$key]} elseif($key -match '^[A-Z0-9]$') {$code=[int][char]$key} else {throw "Unsupported key: $key"}
  [NovaWindow]::Key($hwnd,$code,$false)
  try {Start-Sleep -Milliseconds $HoldMs} finally {
   [uint32]$releaseOwner=0
   $null=[NovaWindow]::GetWindowThreadProcessId($hwnd,[ref]$releaseOwner)
   if($releaseOwner -eq $TargetPid) {[NovaWindow]::Key($hwnd,$code,$true)}
  }
  Start-Sleep -Milliseconds $DelayMs
 }}
 }
}
[ordered]@{pid=$TargetPid;start_ticks=$process.StartTime.ToUniversalTime().Ticks;exe=$process.MainModule.FileName;sha256=$ExpectedSha256;action=$Action;mouse=$mouseInfo;foreground=[NovaWindow]::GetForegroundWindow().ToInt64();windows=$windows} | ConvertTo-Json -Depth 5 -Compress
