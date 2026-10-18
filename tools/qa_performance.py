#!/usr/bin/env python3
"""Measure GUI throughput in an owned game process with two native ships."""
from pathlib import Path
import shutil
import argparse,sys,subprocess,time,re,json,hashlib,statistics,os
from datetime import datetime,timezone
from qa_local import ROOT,windows,POWERSHELL,read_jsonl,fingerprint
from runtime_paths import active_runtime, ORIGINAL_SAVES
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--runtime',type=Path,default=active_runtime())
p.add_argument('--label',default='current')
p.add_argument('--seconds',type=float,default=15)
p.add_argument('--offline-logs',action='store_true')
a=p.parse_args();runtime=a.runtime
out=ROOT/'local/performance'/f'{datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")}-{a.label}'
out.mkdir(parents=True);report={'status':'FAIL','runtime':str(runtime),'label':a.label};proc=None;pid=None;counter=0
before=fingerprint(ORIGINAL_SAVES)
def control(**kw):
 global counter
 counter+=1;d=dict(counter=counter,qa_focus=1,mutate=0);d.update(kw)
 tmp=runtime/'ship-control.pending';tmp.write_text(''.join(f'{k}={v}\n' for k,v in d.items()));os.replace(tmp,runtime/'ship-control.txt')
def wait(f,seconds=40):
 end=time.monotonic()+seconds
 while time.monotonic()<end:
  if proc.poll() is not None:raise RuntimeError('Owned process exited')
  v=f()
  if v:return v
  time.sleep(.15)
 raise RuntimeError('Evidence timeout')
def rows():return read_jsonl(runtime/'ship-telemetry.jsonl')
try:
 for name in ['ship-events.log','ship-telemetry.jsonl','aurie.log','ship-performance.jsonl']:
  path=runtime/name
  if path.exists():shutil.move(str(path),str(out/('previous-'+name)))
 control()
 report['dll_sha256']=hashlib.sha256((runtime/'mods/aurie/NovaDriftShips.dll').read_bytes()).hexdigest()
 log=(out/'launcher.log').open('w')
 proc=subprocess.Popen([str(runtime/'novacoop_launch.exe'),windows(runtime/'NovaDrift.exe'),windows(runtime/'novacoop_sandbox.dll'),windows(runtime),'120'],stdout=log,stderr=subprocess.STDOUT)
 def started():
  m=re.search(r'STARTED pid=(\d+)',(out/'launcher.log').read_text());return int(m[1]) if m else None
 pid=wait(started);report['pid']=pid
 sha=hashlib.sha256((runtime/'NovaDrift.exe').read_bytes()).hexdigest()
 def ui(action,keys='ENTER'):
  r=subprocess.run([POWERSHELL,'-NoProfile','-ExecutionPolicy','Bypass','-File',windows(ROOT/'tools/game_window.ps1'),'-TargetPid',str(pid),'-ExpectedSha256',sha,'-Action',action,'-Keys',keys,'-AllowBackground'],capture_output=True,text=True,timeout=20)
  if r.returncode:raise RuntimeError(r.stderr)
  return json.loads(r.stdout.lstrip('\ufeff'))
 if a.offline_logs:
  wait(lambda: any(w['title']=='Nova Drift' for w in ui('status')['windows']),60)
  time.sleep(3)
  for _ in range(3):ui('press');time.sleep(2)
  control(join=1);time.sleep(3)
  control(augment=8,owner=2);time.sleep(1)
  control(fire=1,right=1,perf=1);time.sleep(a.seconds+2)
 else:
  wait(lambda:(runtime/'ship-events.log').exists() and 'full ship module ready' in (runtime/'ship-events.log').read_text())
  time.sleep(2)
  for _ in range(5):
   if any(r.get('p1',-1)>=100000 for r in rows()):break
   ui('press');time.sleep(1.5)
  control(join=1)
  joined=wait(lambda:next((r for r in reversed(rows()) if r.get('ready') and r.get('p1_asset')==r.get('p2_asset')==603),None))
  if joined['p1']==joined['p2'] or joined['b1']==joined['b2']:raise RuntimeError('Two independent players required')
  control(augment=8,owner=2);time.sleep(.8)
  control(fire=1,right=1,perf=1);time.sleep(a.seconds+2)
 ui('close');proc.wait(timeout=20)
 if proc.returncode:raise RuntimeError('Unclean exit')
 samples=[r for r in rows() if r.get('metrics') and r.get('command')==4]
 if len(samples)<10:raise RuntimeError('Missing measured native frames')
 last=samples[-1];first=next(r for r in samples if r['time']>=last['time']-a.seconds*1000)
 raw=[r for r in rows() if not r.get('metrics') and first['time']<=r['time']<=last['time']]
 if not raw or any(r['p1_asset']!=603 or r['p2_asset']!=603 or r['p1']==r['p2'] or r['b1']==r['b2'] for r in raw):raise RuntimeError('Two independent ships must survive benchmark')
 elapsed=(last['time']-first['time'])/1000;frames=last['input_frame']-first['input_frame']
 report.update(frames=frames,elapsed_seconds=elapsed,gui_frames_per_second=frames/elapsed,
               note='GUI draw throughput, not individual frame latency; QA instrumentation enabled',
               shots2=last['shots2']-first['shots2'],native_players=[raw[-1]['p1'],raw[-1]['p2']])

 report['status']='PASS'
except Exception as e:report['error']=str(e)
finally:
 if proc and proc.poll() is None and pid:
  (runtime/f'stop-{pid}.request').write_text('owned performance cleanup');proc.wait(timeout=20)
 for name in ['ship-events.log','ship-telemetry.jsonl','aurie.log','ship-performance.jsonl']:
  path=runtime/name
  if path.exists():(out/name).write_bytes(path.read_bytes())
 report['original_saves_unchanged']=before==fingerprint(ORIGINAL_SAVES)
 if not report['original_saves_unchanged']:report['status']='FAIL'
 (out/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2),flush=True);print(out,flush=True)
sys.exit(0 if report['status']=='PASS' else 1)
