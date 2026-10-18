param(
 [Parameter(Mandatory=$true)][int]$TargetPid,
 [Parameter(Mandatory=$true)][string]$ExpectedSha256,
 [long]$ExpectedStartTicks=0,
 [string]$TitlePattern='^Nova Drift$',
 [ValidateSet('status','capture','chord','postkey','click','realclick','wheel','close')][string]$Action='status',
 [string]$OutputPath='',
 [ValidateSet('Left','Right')][string]$MouseButton='Left',
 [string]$Keys='SHIFT,TAB',[ValidateRange(10,3000)][int]$HoldMs=100,
 [int]$X=0,[int]$Y=0,[int]$WheelDelta=-360
)
$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.Drawing
Add-Type @'
using System; using System.Text; using System.Collections.Generic; using System.Runtime.InteropServices;
public static class OwnedWindow {
 public delegate bool EnumProc(IntPtr h, IntPtr d);
 [DllImport("user32.dll")] static extern bool EnumWindows(EnumProc f,IntPtr d);
 [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h,out uint p);
 [DllImport("user32.dll")] static extern bool IsWindowVisible(IntPtr h);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)] static extern int GetWindowText(IntPtr h,StringBuilder s,int n);
 [DllImport("user32.dll")] public static extern bool IsHungAppWindow(IntPtr h);
 [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h,out Rect r);
 [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h,IntPtr dc,uint flags);
 [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h,uint msg,UIntPtr w,IntPtr l);
 [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
 [DllImport("user32.dll")] static extern bool SetForegroundWindow(IntPtr h);
 [DllImport("user32.dll")] static extern bool ShowWindow(IntPtr h,int cmd);
 [DllImport("user32.dll")] static extern void SwitchToThisWindow(IntPtr h,bool altTab);
 [DllImport("user32.dll")] static extern bool AttachThreadInput(uint a,uint b,bool c);
 [DllImport("kernel32.dll")] static extern uint GetCurrentThreadId();
 [DllImport("user32.dll")] public static extern void keybd_event(byte key,byte scan,uint flags,UIntPtr extra);
 [DllImport("user32.dll")] public static extern IntPtr SetThreadDpiAwarenessContext(IntPtr context);
 [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h,ref Point p);
 [DllImport("user32.dll")] public static extern bool GetCursorPos(out Point p);
 [DllImport("user32.dll")] public static extern bool SetCursorPos(int x,int y);
 [DllImport("user32.dll")] public static extern void mouse_event(uint flags,uint x,uint y,uint data,UIntPtr extra);
 [DllImport("user32.dll")] public static extern uint MapVirtualKey(uint code,uint type);
 public struct Point {public int x,y;}
 public struct Rect {public int left,top,right,bottom;}
 public static long[] Find(uint pid){var a=new List<long>();EnumWindows((h,d)=>{uint p;GetWindowThreadProcessId(h,out p);if(p==pid&&IsWindowVisible(h))a.Add(h.ToInt64());return true;},IntPtr.Zero);return a.ToArray();}
 public static string Title(IntPtr h){var b=new StringBuilder(1024);GetWindowText(h,b,b.Capacity);return b.ToString();}
 public static bool Focus(IntPtr h){uint p;uint other=GetWindowThreadProcessId(GetForegroundWindow(),out p),mine=GetCurrentThreadId();bool joined=false;try{if(other!=mine)joined=AttachThreadInput(mine,other,true);ShowWindow(h,9);SetForegroundWindow(h);}finally{if(joined)AttachThreadInput(mine,other,false);}if(GetForegroundWindow()!=h)SwitchToThisWindow(h,true);for(int i=0;i<20&&GetForegroundWindow()!=h;i++)System.Threading.Thread.Sleep(50);return GetForegroundWindow()==h;}
}
'@
$null=[OwnedWindow]::SetThreadDpiAwarenessContext([IntPtr](-4))
function Verify {
 $p=Get-Process -Id $TargetPid
 if($ExpectedStartTicks -ne 0 -and $p.StartTime.ToUniversalTime().Ticks -ne $ExpectedStartTicks){throw 'PID creation time changed'}
 if((Get-FileHash -LiteralPath $p.Path -Algorithm SHA256).Hash -ne $ExpectedSha256){throw 'Executable identity differs'}
 return $p
}
$p=Verify
$windows=@([OwnedWindow]::Find($TargetPid)|ForEach-Object {[ordered]@{handle=$_;title=[OwnedWindow]::Title([IntPtr]$_);hung=[OwnedWindow]::IsHungAppWindow([IntPtr]$_)}})
if($Action -ne 'status'){
 $chosen=@($windows|Where-Object {$_.title -match $TitlePattern})
 if($chosen.Count -ne 1){throw 'Exactly one matching owned window required'}
 $h=[IntPtr]$chosen[0].handle
 if($chosen[0].hung){throw 'Owned window is hung'}
 $rect=New-Object OwnedWindow+Rect
 if(-not [OwnedWindow]::GetClientRect($h,[ref]$rect)){throw 'Client rectangle unavailable'}
 if($Action -eq 'capture'){
  if(-not $OutputPath){throw 'Output path required'}
  $b=New-Object Drawing.Bitmap($rect.right,$rect.bottom);$g=[Drawing.Graphics]::FromImage($b);$dc=$g.GetHdc()
  try{if(-not [OwnedWindow]::PrintWindow($h,$dc,3)){throw 'Window-only capture failed'}}finally{$g.ReleaseHdc($dc)}
  try{$b.Save($OutputPath)}finally{$g.Dispose();$b.Dispose()}
 }elseif($Action -eq 'close'){
  if(-not [OwnedWindow]::PostMessage($h,0x10,[UIntPtr]::Zero,[IntPtr]::Zero)){throw 'Close failed'}
 }elseif($Action -eq 'click' -or $Action -eq 'realclick' -or $Action -eq 'wheel'){
  if($X -lt 0 -or $Y -lt 0 -or $X -ge $rect.right -or $Y -ge $rect.bottom){throw 'Click is outside owned client area'}
  $null=Verify
  $focused=[OwnedWindow]::Focus($h)
  $point=[IntPtr](($Y -shl 16)-bor $X)
  [void][OwnedWindow]::PostMessage($h,0x200,[UIntPtr]::Zero,$point)
  if($Action -eq 'realclick'){
   if(-not $focused){throw 'Owned focus required for real mouse input'}
   $screen=New-Object OwnedWindow+Point;$screen.x=$X;$screen.y=$Y;$old=New-Object OwnedWindow+Point
   if(-not [OwnedWindow]::ClientToScreen($h,[ref]$screen) -or -not [OwnedWindow]::GetCursorPos([ref]$old)){throw 'Cursor coordinate conversion failed'}
   $down=$false
   try{
    if(-not [OwnedWindow]::SetCursorPos($screen.x,$screen.y)){throw 'Cursor movement failed'}
    if([OwnedWindow]::GetForegroundWindow() -ne $h){throw 'Owned focus lost before click'}
    [uint32]$downFlag=if($MouseButton -eq 'Right'){8}else{2};[uint32]$upFlag=if($MouseButton -eq 'Right'){16}else{4}
    [OwnedWindow]::mouse_event($downFlag,0,0,0,[UIntPtr]::Zero);$down=$true;Start-Sleep -Milliseconds $HoldMs
   }finally{if($down){[OwnedWindow]::mouse_event($upFlag,0,0,0,[UIntPtr]::Zero)};[void][OwnedWindow]::SetCursorPos($old.x,$old.y)}
  }elseif($Action -eq 'wheel'){
   $screen=New-Object OwnedWindow+Point;$screen.x=$X;$screen.y=$Y
   if(-not [OwnedWindow]::ClientToScreen($h,[ref]$screen)){throw 'Screen coordinate conversion failed'}
   $wheel=[UIntPtr]::new([uint32]([long]($WheelDelta-band 65535)-shl 16))
   $where=[IntPtr](($screen.y-shl 16)-bor ($screen.x-band 65535))
   if(-not [OwnedWindow]::PostMessage($h,0x20A,$wheel,$where)){throw 'Wheel failed'}
  }else{
  if(-not [OwnedWindow]::PostMessage($h,0x201,[UIntPtr]::new([uint32]1),$point)){throw 'Mouse down failed'}
  try{Start-Sleep -Milliseconds 80}finally{[void][OwnedWindow]::PostMessage($h,0x202,[UIntPtr]::Zero,$point)}
  }
 }elseif($Action -eq 'postkey'){
  $map=@{ENTER=13;ESCAPE=27;I=73;J=74;K=75;L=76;N=78}
  if(-not $map.ContainsKey($Keys)){throw 'Unsupported posted key'}
  [uint32]$code=$map[$Keys];[long]$bits=1-bor ([long][OwnedWindow]::MapVirtualKey($code,0)-shl 16)
  if(-not [OwnedWindow]::PostMessage($h,0x100,[UIntPtr]::new($code),[IntPtr]$bits)){throw 'Posted key down failed'}
  try{Start-Sleep -Milliseconds $HoldMs}finally{[uint32]$releasePid=0;$null=[OwnedWindow]::GetWindowThreadProcessId($h,[ref]$releasePid);if($releasePid -eq $TargetPid){[void][OwnedWindow]::PostMessage($h,0x101,[UIntPtr]::new($code),[IntPtr]($bits-bor (1L-shl 30)-bor (1L-shl 31)))}}
 }elseif($Action -eq 'chord'){
  $map=@{SHIFT=16;CTRL=17;ALT=18;TAB=9;ENTER=13;ESCAPE=27;F7=118;F8=119}
  $codes=@($Keys.Split(',')|ForEach-Object {$name=$_.Trim().ToUpperInvariant();if(-not $map.ContainsKey($name)){throw 'Unsupported chord key'};[byte]$map[$name]})
  $null=Verify
  if(-not [OwnedWindow]::Focus($h)){throw 'Owned focus required'}
  $held=New-Object 'System.Collections.Generic.List[byte]'
  try{
   foreach($code in $codes){if([OwnedWindow]::GetForegroundWindow() -ne $h){throw 'Owned focus lost'};[OwnedWindow]::keybd_event($code,0,0,[UIntPtr]::Zero);$held.Add($code);Start-Sleep -Milliseconds 60}
  }finally{for($i=$held.Count-1;$i -ge 0;$i--){[OwnedWindow]::keybd_event($held[$i],0,2,[UIntPtr]::Zero)}}
 }
}
[ordered]@{pid=$p.Id;start_ticks=$p.StartTime.ToUniversalTime().Ticks;action=$Action;windows=$windows}|ConvertTo-Json -Depth 4 -Compress
