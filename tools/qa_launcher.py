#!/usr/bin/env python3
"""Owned smoke of the ordinary launcher or Steam wrapper bootstrap (no network)."""
from pathlib import Path
import argparse,subprocess,time,json,re,hashlib,os,shutil
from datetime import datetime,timezone
from qa_local import ROOT,windows,POWERSHELL,fingerprint,read_jsonl
from runtime_paths import active_runtime, ORIGINAL_SAVES
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--remote-wrapper',action='store_true')
p.add_argument('--diagnostics',action='store_true')
a=p.parse_args();runtime=active_runtime()
label='wrapper-bootstrap' if a.remote_wrapper else 'normal-launcher'
out=ROOT/'local/fullship-test'/label/datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%SZ');out.mkdir(parents=True)
report={'status':'FAIL','mode':label,'diagnostics':a.diagnostics,'network_tested':False};proc=None;pid=None
wrapper_marker='novacoop-wrapper-qa-'+out.name
before=fingerprint(ORIGINAL_SAVES)
logs=['ship-telemetry.jsonl','ship-ui.jsonl','ship-performance.jsonl']
sizes={name:(runtime/name).stat().st_size if (runtime/name).exists() else 0 for name in logs}
def wait(f,seconds=50,phase='startup evidence'):
 end=time.monotonic()+seconds
 while time.monotonic()<end:
  if proc.poll() is not None:raise RuntimeError(f'{phase}: launcher exited early: {proc.returncode}')
  if a.remote_wrapper and not pid:
   identity=wrapper_identity()
   if identity:report['wrapper_identity']=identity
  if a.remote_wrapper and report.get('wrapper_identity',{}).get('error_dialog'):
   raise RuntimeError(f'{phase}: owned wrapper is blocked on its startup error dialog')
  v=f()
  if v:return v
  time.sleep(.15)
 raise RuntimeError(f'Evidence timeout: {phase}')
def log():return (runtime/'ship-events.log').read_text() if (runtime/'ship-events.log').exists() else ''
def wrapper_identity():
 path=windows(runtime/'SteamCoop.exe').replace("'","''")
 script=f"$rows=@(Get-CimInstance Win32_Process -Filter \"Name = 'SteamCoop.exe'\" | Where-Object {{$_.ExecutablePath -eq '{path}' -and $_.CommandLine.Contains('{wrapper_marker}')}}); if($rows.Count -gt 1){{throw 'Multiple owned QA wrappers'}}; if($rows.Count -eq 1){{$p=Get-Process -Id $rows[0].ProcessId; [pscustomobject]@{{pid=$p.Id;start_ticks=$p.StartTime.ToUniversalTime().ToFileTimeUtc();error_dialog=($p.MainWindowTitle -eq 'Nova Drift Steam co-op')}} | ConvertTo-Json -Compress}}"
 r=subprocess.run([POWERSHELL,'-NoProfile','-Command',script],capture_output=True,text=True,timeout=20)
 if r.returncode:raise RuntimeError(r.stderr)
 return json.loads(r.stdout.lstrip('\ufeff')) if r.stdout.strip() else None
def close_owned_wrapper():
 identity=report.get('wrapper_identity') or wrapper_identity()
 if not identity:raise RuntimeError('No wrapper ownership record; cleanup requires manual review')
 wrapper_pid=identity['pid'];ticks=identity['start_ticks'];exe=windows(runtime/'SteamCoop.exe').replace("'","''")
 script=f"$p=Get-Process -Id {wrapper_pid} -ErrorAction Stop; if($p.Path -ne '{exe}' -or $p.StartTime.ToUniversalTime().ToFileTimeUtc() -ne {ticks}){{throw 'Wrapper identity mismatch'}}; if(-not $p.CloseMainWindow()){{throw 'Owned wrapper has no closeable window'}}"
 closed=subprocess.run([POWERSHELL,'-NoProfile','-Command',script],capture_output=True,text=True,timeout=10)
 if closed.returncode:raise RuntimeError(closed.stderr.strip())
 proc.wait(timeout=10)
def process_tree():
 expected_wrapper=report.get('wrapper_identity',{}).get('pid',0)
 if not expected_wrapper:return None
 path=windows(runtime/'NovaDrift.exe').replace("'","''")
 script=f"$p=Get-CimInstance Win32_Process -Filter \"Name = 'NovaDrift.exe'\" | Where-Object {{$_.ExecutablePath -eq '{path}'}}; $p | ForEach-Object {{ $parent=Get-CimInstance Win32_Process -Filter ('ProcessId = '+$_.ParentProcessId); $grand=Get-CimInstance Win32_Process -Filter ('ProcessId = '+$parent.ParentProcessId); if($grand.ProcessId -eq {expected_wrapper}) {{ [pscustomobject]@{{pid=$_.ProcessId;exe=$_.ExecutablePath;parent=$parent.ExecutablePath;parent_command=$parent.CommandLine;grandparent=$grand.ExecutablePath}} }} }} | ConvertTo-Json -Compress"
 r=subprocess.run([POWERSHELL,'-NoProfile','-Command',script],capture_output=True,text=True,timeout=20)
 if r.returncode:raise RuntimeError(r.stderr)
 return json.loads(r.stdout.lstrip('\ufeff')) if r.stdout.strip() else None
try:
 for name in ['ship-events.log','aurie.log','runner.log']:
  path=runtime/name
  if path.exists():shutil.move(str(path),str(out/('previous-'+name)))
 env=os.environ.copy()
 if a.remote_wrapper:
  # Simulates only the inherited app identity; Steam is not launched or configured.
  env['SteamAppId']='480';env['SteamGameId']='480'
  # WSL does not pass arbitrary Linux env vars into Windows executables.
  names={'steamappid','steamgameid'}
  inherited=[item for item in env.get('WSLENV','').split(':') if item and item.split('/')[0].lower() not in names]
  env['WSLENV']=':'.join(inherited+['SteamAppId/w','SteamGameId/w'])
  command=[str(runtime/'SteamCoop.exe'),'--host','ignored-donor-game.exe','1','debug',wrapper_marker]
 else:command=['/mnt/c/Windows/System32/cmd.exe','/d','/c','call',windows(ROOT/'PlayCoop.cmd')]+(['diagnostics'] if a.diagnostics else [])
 with (out/'launcher.log').open('w') as f:proc=subprocess.Popen(command,stdout=f,stderr=subprocess.STDOUT,env=env)
 if a.remote_wrapper:
  tree=wait(process_tree,phase='owned game/launcher/wrapper process chain');pid=tree['pid'];report['process_tree']=tree
  if not tree['parent'].lower().endswith('novacoop_launch.exe') or not tree['grandparent'].lower().endswith('steamcoop.exe'):raise RuntimeError('Wrong owned process chain')
  if not tree['parent_command'].rstrip().endswith(' 0 fullscreen'):raise RuntimeError('Donor arguments changed launcher duration')
 else:
  def started():
   match=re.search(r'STARTED pid=(\d+)',(out/'launcher.log').read_text());return int(match[1]) if match else None
  pid=wait(started,phase='ordinary launcher STARTED pid')
 report['pid']=pid
 sha=hashlib.sha256((runtime/'NovaDrift.exe').read_bytes()).hexdigest()
 def ui(action,keys='ENTER'):
  r=subprocess.run([POWERSHELL,'-NoProfile','-ExecutionPolicy','Bypass','-File',windows(ROOT/'tools/game_window.ps1'),'-TargetPid',str(pid),'-ExpectedSha256',sha,'-Action',action,'-Keys',keys,'-AllowBackground'],capture_output=True,text=True,timeout=20)
  if r.returncode:raise RuntimeError(r.stderr)
  return json.loads(r.stdout.lstrip('\ufeff'))
 wait(lambda:'full ship module ready' in log(),phase='full ship module ready log')
 controls=(runtime/'ship-control.txt').read_text();report['controls']=controls
 if 'qa_focus=0' not in controls or 'mutate=0' not in controls or 'qa_focus=1' in controls:raise RuntimeError('Physical-input mode required')
 report['window']=ui('focus')
 for _ in range(5):
  if 'join complete: independent native ship/build' in log():break
  ui('press');time.sleep(2)
 wait(lambda:'join complete: independent native ship/build' in log(),15,phase='two independent native ships joined')
 actors=re.findall(r'native Player Create owner=(\d) id=(\d+) asset=(\d+)',log())
 ids={int(owner):int(i) for owner,i,asset in actors if int(asset)==603}
 if 1 not in ids or 2 not in ids or ids[1]==ids[2]:raise RuntimeError('Two native ships required')
 report['native_players']=ids
 report['dll_sha256']=hashlib.sha256((runtime/'mods/aurie/NovaDriftShips.dll').read_bytes()).hexdigest()
 time.sleep(8) # reach steady gameplay beyond transient hints
 ui('close');proc.wait(timeout=20)
 if proc.returncode:raise RuntimeError('Unclean owned shutdown')
 report['launcher_exit']=proc.returncode
 if not a.diagnostics:
  after={name:(runtime/name).stat().st_size if (runtime/name).exists() else 0 for name in logs}
  report['normal_instrumentation_unchanged']=after==sizes
  if after!=sizes:raise RuntimeError('Normal play writes QA instrumentation')
 else:
  perf=read_jsonl(runtime/'ship-performance.jsonl');report['frame_measurements']=[r for r in perf if r.get('metric')==0][-3:]
 report['status']='PASS'
except Exception as e:report['error']=str(e)
finally:
 if proc and proc.poll() is None:
  try:
   if pid:
    (runtime/f'stop-{pid}.request').write_text('owned launcher smoke cleanup')
    try:proc.wait(timeout=20)
    except subprocess.TimeoutExpired:
     if not a.remote_wrapper:raise
     # The wrapper reports a nonzero owned launcher exit in a modal dialog.
     close_owned_wrapper()
   elif a.remote_wrapper:
    # Close only this run's unique, owned wrapper; never discovered game processes.
    close_owned_wrapper()
  except Exception as e:
   report['cleanup_error']=str(e);report['status']='FAIL'
 for name in ['ship-events.log','aurie.log']:
  path=runtime/name
  if path.exists():shutil.copy2(path,out/name)
 report['original_saves_unchanged']=before==fingerprint(ORIGINAL_SAVES)
 if not report['original_saves_unchanged']:report['status']='FAIL'
 (out/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2));print(out)
raise SystemExit(0 if report['status']=='PASS' else 1)
