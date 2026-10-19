#!/usr/bin/env python3
"""Exercise an early ordinary launch, before hook installation finishes.

Requires NOVACOOP_TEST_RUNTIME pointing at an isolated QA installation.
Uses physical-input mode, never the mod's QA command interface.
"""
import hashlib,json,math,os,re,shutil,subprocess,time
from datetime import datetime,timezone
from qa_local import ROOT,windows,POWERSHELL,fingerprint,read_jsonl
from runtime_paths import active_runtime,ORIGINAL_SAVES

if not os.environ.get('NOVACOOP_TEST_RUNTIME'):
 raise SystemExit('Set NOVACOOP_TEST_RUNTIME to an isolated QA runtime')
runtime=active_runtime()
out=ROOT/'local/fullship-test/bootstrap'/datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%SZ')
out.mkdir(parents=True)
report={'status':'FAIL','mode':'ordinary early launch','qa_focus':0};proc=None;pid=None
before=fingerprint(ORIGINAL_SAVES)
sha=hashlib.sha256((runtime/'NovaDrift.exe').read_bytes()).hexdigest()
report['dll_sha256']=hashlib.sha256((runtime/'mods/aurie/NovaDriftShips.dll').read_bytes()).hexdigest()
names=['ship-events.log','ship-telemetry.jsonl','ship-ui.jsonl','ship-balance.jsonl','aurie.log']
def log():
 p=runtime/'ship-events.log'
 return p.read_text(errors='replace') if p.exists() else ''
def check_crash():
 path=runtime/'aurie.log'
 if path.exists() and '[critical]' in path.read_text(errors='replace'):
  raise RuntimeError('Native crash: '+path.read_text(errors='replace')[-3500:])
def wait(f,seconds=40,phase='evidence'):
 end=time.monotonic()+seconds
 while time.monotonic()<end:
  check_crash()
  if proc.poll() is not None:raise RuntimeError(f'{phase}: launcher exited {proc.returncode}')
  value=f()
  if value:return value
  time.sleep(.2)
 raise RuntimeError('Timeout: '+phase)
def ui(action,keys='ENTER',hold=120):
 r=subprocess.run([POWERSHELL,'-NoProfile','-ExecutionPolicy','Bypass','-File',windows(ROOT/'tools/game_window.ps1'),
  '-TargetPid',str(pid),'-ExpectedSha256',sha,'-Action',action,'-Keys',keys,'-HoldMs',str(hold),'-AllowBackground'],capture_output=True,text=True,timeout=20)
 if r.returncode:raise RuntimeError(r.stderr.strip())
 return json.loads(r.stdout.lstrip('\ufeff'))
def mouse(event):
 r=subprocess.run([POWERSHELL,'-NoProfile','-ExecutionPolicy','Bypass','-File',windows(ROOT/'tools/game_window.ps1'),
  '-TargetPid',str(pid),'-ExpectedSha256',sha,'-Action','mouse','-MouseEvent',event],capture_output=True,text=True,timeout=20)
 if r.returncode:raise RuntimeError(r.stderr.strip())
def middle():mouse('middleClick')
def trays():return [r for r in read_jsonl(runtime/'ship-balance.jsonl') if r.get('kind')=='tray']
def latest_trays():
 rows=trays()
 return [next((r for r in reversed(rows) if r['owner']==owner),{}) for owner in (1,2)]
def capture():
 identity=ui('focus');report['window']=identity
 handle=next(w['handle'] for w in identity['windows'] if w['title'].startswith('Nova Drift'))
 path=runtime.parent/'test-profile/Temp/bootstrap.png'
 command=('& {\n'+(ROOT/'tools/capture_game.ps1').read_text()+'\n} -TargetPid '+str(pid)+
  ' -ExpectedStartTicks '+str(identity['start_ticks'])+' -GameWindowHandle '+str(handle)+
  " -ExpectedSha256 '"+sha+"' -OutputPath '"+windows(path).replace("'","''")+"'")
 r=subprocess.run([POWERSHELL,'-NoProfile','-Command',command],capture_output=True,text=True,timeout=20)
 if r.returncode:raise RuntimeError(r.stderr.strip())
 shutil.copyfile(path,out/'bootstrap.png')
try:
 for name in names:
  path=runtime/name
  if path.exists():shutil.move(path,out/('previous-'+name))
 shutil.copyfile(ROOT/'tools/PlayShips.cmd',runtime/'PlayCoop.cmd')
 with (out/'launcher.log').open('w') as stream:
  proc=subprocess.Popen(['/mnt/c/Windows/System32/cmd.exe','/d','/c','call',windows(runtime/'PlayCoop.cmd'),'diagnostics','fullscreen'],stdout=stream,stderr=subprocess.STDOUT,cwd=runtime)
 def started():
  m=re.search(r'STARTED pid=(\d+)',(out/'launcher.log').read_text())
  return int(m[1]) if m else None
 pid=wait(started,phase='owned game PID');report['pid']=pid
 # Deliberately do not wait for module ready. This is the user-facing path.
 for _ in range(12):
  if 'native Player Create owner=1' in log():break
  try:ui('press')
  except RuntimeError as error:
   if 'not visible yet' not in str(error) and 'No visible window' not in str(error):raise
  time.sleep(.25)
 wait(lambda:'full ship module ready' in log(),60,'module ready')
 lines=log().splitlines()
 created=next((i for i,s in enumerate(lines) if 'native Player Create owner=1' in s),None)
 ready=next(i for i,s in enumerate(lines) if 'full ship module ready' in s)
 report['p1_created_before_ready']=created is not None and created<ready
 if not report['p1_created_before_ready']:raise RuntimeError('Early-launch race was not exercised')
 controls=(runtime/'ship-control.txt').read_text();report['controls']=controls
 if 'qa_focus=0' not in controls or 'diagnostics=1' not in controls or 'mutate=0' not in controls:raise RuntimeError('Wrong test mode')
 wait(lambda:'join complete: independent native ship/build' in log(),20,'two ships joined')
 def dual_hud():
  rows=read_jsonl(runtime/'ship-ui.jsonl')
  r=rows[-1] if rows else {}
  return r if r.get('native_hud')==1 and all(r.get(f'dst{o}_w',0)>0 and r.get(f'dst{o}_h',0)>0 for o in (1,2)) and abs(r['dst1_x']-r['dst2_x'])>50 else None
 report['dual_hud']=wait(dual_hud,15,'two separate native health bars')
 if 'all native hooks installed; co-op joining enabled' not in log():raise RuntimeError('Missing join readiness barrier')
 lines=log().splitlines()
 report['joined_after_hooks']=next(i for i,s in enumerate(lines) if 'join complete: independent native ship/build' in s)>next(i for i,s in enumerate(lines) if 'all native hooks installed; co-op joining enabled' in s)
 if not report['joined_after_hooks']:raise RuntimeError('Join occurred before hook installation completed')
 def both_shops():
  rows=latest_trays()
  return rows if all(r.get('open') and r.get('cards') for r in rows) else None
 # A stationary ship need not earn XP. Use actual combat input to collect the
 # first level rather than depending on a stray asteroid awarding it for us.
 mouse('rightDown');mouse('leftDown')
 try:report['trays']=wait(both_shops,55,'both native compact shops open after first earned level')
 finally:mouse('leftUp');mouse('rightUp')
 capture()
 # Real player key closes/reopens P2's tray; no QA commands are used.
 count=len(trays());ui('press','F7')
 report['p2_closed']=wait(lambda:trays()[-1] if len(trays())>count and trays()[-1]['owner']==2 and not trays()[-1]['open'] else None,5,'P2 upgrade key closes tray')
 count=len(trays());ui('press','F7')
 report['p2_reopened']=wait(lambda:trays()[-1] if len(trays())>count and trays()[-1]['owner']==2 and trays()[-1]['open'] else None,5,'P2 upgrade key reopens tray')
 count=len(trays());middle()
 report['p1_closed']=wait(lambda:latest_trays()[0] if len(trays())>count and latest_trays()[0].get('open')==0 else None,5,'P1 middle mouse closes tray')
 count=len(trays());middle()
 report['p1_reopened']=wait(lambda:latest_trays()[0] if len(trays())>count and latest_trays()[0].get('open')==1 else None,5,'P1 middle mouse reopens tray')
 def motion():return read_jsonl(runtime/'ship-telemetry.jsonl')
 for key,sign in (('J',1),('L',-1)):
  baseline=motion()[-1];ui('press',key,300)
  samples=[baseline]+[r for r in motion() if r['time']>baseline['time']]
  turn=sum(math.remainder(b['angle2']-a['angle2'],360) for a,b in zip(samples,samples[1:]))
  report['rotation_'+key]={'degrees':turn,'samples':len(samples)}
  if turn*sign<5:raise RuntimeError(key+' did not rotate the native P2 ship in the expected direction')
 before_thrust=read_jsonl(runtime/'ship-ui.jsonl')[-1]['time'];ui('press','I',700)
 report['p2_thrust']=motion()[-1]
 if math.hypot(report['p2_thrust']['vx2'],report['p2_thrust']['vy2'])<.5:raise RuntimeError('I did not thrust with the compact shop visible')
 report['translated_thrust']=any(r.get('p2_thrust') for r in read_jsonl(runtime/'ship-ui.jsonl') if r['time']>before_thrust)
 if not report['translated_thrust']:raise RuntimeError('Native movement did not include translated P2 thrust input')
 report['final_dual_hud']=wait(dual_hud,3,'both health bars remain active after input')
 check_crash();report['status']='PASS'
except Exception as error:report['error']=str(error)
finally:
 if proc and proc.poll() is None:
  try:
   if pid:
    for event in ('leftUp','rightUp','middleUp'):
     try:mouse(event)
     except Exception:pass
    ui('close')
    try:proc.wait(timeout=20)
    except subprocess.TimeoutExpired:
     (runtime/f'stop-{pid}.request').write_text('owned early-launch test cleanup')
     proc.wait(timeout=20)
   else:raise RuntimeError('No owned game PID available for cleanup')
  except Exception as error:
   report['cleanup_error']=str(error);report['status']='FAIL'
 for name in names:
  path=runtime/name
  if path.exists():shutil.copyfile(path,out/name)
 report['original_saves_unchanged']=fingerprint(ORIGINAL_SAVES)==before
 if not report['original_saves_unchanged']:report['status']='FAIL'
 (out/'report.json').write_text(json.dumps(report,indent=2)+'\n')
 print(json.dumps(report,indent=2));print(out)
raise SystemExit(0 if report['status']=='PASS' else 1)
