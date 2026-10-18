param([int]$TargetPid,[string]$ExpectedSha256,[ValidateSet('status','close')][string]$Action='status')
$ErrorActionPreference='Stop'
$process=Get-Process -Id $TargetPid
if($process.ProcessName -ne 'NovaDriftP2Controls'){throw 'Unexpected editor process'}
$path=$process.Path
if((Get-FileHash -Algorithm SHA256 -LiteralPath $path).Hash.ToLowerInvariant() -ne $ExpectedSha256.ToLowerInvariant()){throw 'Editor executable hash mismatch'}
Add-Type @'
using System;
using System.Text;
using System.Collections.Generic;
using System.Runtime.InteropServices;
public class CoopEditorWindow {
 public delegate bool Visit(IntPtr window,IntPtr arg);
 [DllImport("user32.dll")] public static extern bool EnumWindows(Visit callback,IntPtr arg);
 [DllImport("user32.dll")] public static extern bool EnumChildWindows(IntPtr parent,Visit callback,IntPtr arg);
 [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr window,out uint pid);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)] public static extern int GetClassName(IntPtr window,StringBuilder text,int max);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)] public static extern int GetWindowText(IntPtr window,StringBuilder text,int max);
 [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr window);
 [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr window,uint message,IntPtr w,IntPtr l);
 public static long Find(uint pid) {
  long found=0;EnumWindows((w,a)=>{uint p;GetWindowThreadProcessId(w,out p);var text=new StringBuilder(256);GetClassName(w,text,256);if(p==pid&&text.ToString()=="NovaDriftP2Controls"&&IsWindowVisible(w))found=w.ToInt64();return true;},IntPtr.Zero);return found;
 }
 public static string[] Labels(long window) {
  var values=new List<string>();EnumChildWindows(new IntPtr(window),(w,a)=>{var text=new StringBuilder(2048);GetWindowText(w,text,2048);if(text.Length>0)values.Add(text.ToString());return true;},IntPtr.Zero);return values.ToArray();
 }
}
'@
$handle=[CoopEditorWindow]::Find($TargetPid)
if(!$handle){throw 'Visible owned control editor not ready'}
$result=@{pid=$TargetPid;exe=$path;handle=$handle;labels=[CoopEditorWindow]::Labels($handle);action=$Action}
if($Action -eq 'close'){[void][CoopEditorWindow]::PostMessage([IntPtr]::new($handle),0x10,[IntPtr]::Zero,[IntPtr]::Zero)}
$result | ConvertTo-Json -Compress -Depth 4
