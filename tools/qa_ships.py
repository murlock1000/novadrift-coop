#!/usr/bin/env python3
"""Owned-runtime checks for actual native P2 and independent build application."""
from pathlib import Path
import shutil
import sys,subprocess,time,json,re,hashlib,math,os
mutation='--mutation-context' in sys.argv
ui_mutation='--mutation-native-ui' in sys.argv
sharing_mutation='--mutation-input-sharing' in sys.argv
from datetime import datetime,timezone
from qa_local import ROOT,windows,POWERSHELL,read_jsonl,fingerprint
from runtime_paths import active_runtime, ORIGINAL_SAVES
runtime=active_runtime()
out=ROOT/'local/fullship-test/qa'/datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%SZ')
out.mkdir(parents=True)
report={'status':'FAIL','checks':[],'artifact':str(out),'mutation_context':mutation,'mutation_native_ui':ui_mutation,'mutation_input_sharing':sharing_mutation}
process=None;pid=None;counter=0;config_before=None;mouse_fixture=None;saved_controls_fixture=None
before=fingerprint(ORIGINAL_SAVES)
def control(**changes):
 global counter
 counter+=1;c=dict(counter=counter,qa_focus=1,mutate=0,rescue_mode=int('--polish' in sys.argv),mutate_hud=int(ui_mutation),mutate_sharing=int(sharing_mutation),mutate_beacon=0,mutate_tray_input=int('--mutation-tray-input' in sys.argv),mutate_tray_cursor=int('--mutation-tray-cursor' in sys.argv),mutate_startup=int('--mutation-startup' in sys.argv),mutate_pause=int('--mutation-pause' in sys.argv),mutate_death_step=int('--mutation-death-step' in sys.argv),mutate_thrust_cleanup=int('--mutation-thrust-cleanup' in sys.argv),mutate_healthfx=int('--mutation-healthfx' in sys.argv));c.update(changes)
 temp=runtime/'ship-control.pending';temp.write_text(''.join(f'{k}={v}\n' for k,v in c.items()))
 # The native reader can briefly hold a Windows handle without delete sharing.
 for attempt in range(80):
  try:os.replace(temp,runtime/'ship-control.txt');break
  except PermissionError:
   if attempt==79:raise
   time.sleep(.025)
 return counter
def rows(metrics=False):return [r for r in read_jsonl(runtime/'ship-telemetry.jsonl') if bool(r.get('metrics'))==metrics]
def personal_tray(owner):return next((r for r in reversed(read_jsonl(runtime/'ship-balance.jsonl')) if r.get('kind')=='tray' and r['owner']==owner),None)
def check(name,condition,details):
 report['checks'].append(dict(name=name,pass_=bool(condition),details=details));print(('PASS ' if condition else 'FAIL ')+name+': '+str(details),flush=True)
 if not condition:raise RuntimeError(name)
def wait(test,seconds=15):
 end=time.monotonic()+seconds
 while time.monotonic()<end:
  errorlog=runtime/'aurie.log'
  if errorlog.exists() and '[critical]' in errorlog.read_text(errors='replace'):raise RuntimeError(errorlog.read_text(errors='replace')[-6000:])
  if process.poll() is not None:raise RuntimeError('Owned process exited: '+(out/'launcher.log').read_text()[-4000:])
  result=test()
  if result:return result
  time.sleep(.1)
 raise RuntimeError('Evidence timeout')
def phase(seconds=2,**changes):
 c=control(**changes);wait(lambda: next((r for r in reversed(rows()) if r['command']==c),None));time.sleep(seconds)
 result=[r for r in rows() if r['command']==c]
 check('phase advances native ships',len(result)>5 and result[-1]['steps2']>result[0]['steps2'],result[-1]);return result
try:
 if '--saved-controls' in sys.argv:
  profile=runtime/'p1-controls.ini'
  saved_controls_fixture=(profile,profile.read_bytes() if profile.exists() else None)
  profile.write_text('[controls]\ndevice=keyboard\nsteering=tank\nmouse_aim=1\n[keys]\nup=1\nleft=65\ndown=83\nright=68\nfire=2\nstabilize=160\nformation=70\nupgrade=4\n')
 if '--tray-controls' in sys.argv:
  fixture=runtime.parent/'test-profile/Local/NovaDrift/save.ini'
  previous=fixture.read_bytes()
  mouse_fixture=(fixture,previous)
  fixture.write_bytes(re.sub(rb'disableMouseAim="[^"]*"',b'disableMouseAim="0.000000"',previous))
 for name in ['ship-events.log','ship-telemetry.jsonl','ship-ui.jsonl','ship-performance.jsonl','aurie.log','runner.log','ship-balance.jsonl']:
  p=runtime/name
  if p.exists():shutil.move(str(p),str(out/('previous-'+name)))
 control()
 with (out/'launcher.log').open('w') as f:
  process=subprocess.Popen([str(runtime/'novacoop_launch.exe'),windows(runtime/'NovaDrift.exe'),windows(runtime/'novacoop_sandbox.dll'),windows(runtime),'180','debug'],stdout=f,stderr=subprocess.STDOUT)
 def started():
  m=re.search(r'STARTED pid=(\d+)',(out/'launcher.log').read_text());return int(m[1]) if m else None
 pid=wait(started,30);report['pid']=pid
 sha=hashlib.sha256((runtime/'NovaDrift.exe').read_bytes()).hexdigest()
 report['dll_sha256']=hashlib.sha256((runtime/'mods/aurie/NovaDriftShips.dll').read_bytes()).hexdigest()
 (out/'NovaDriftShips.dll').write_bytes((runtime/'mods/aurie/NovaDriftShips.dll').read_bytes())
 def ui(action,keys='ENTER'):
  r=subprocess.run([POWERSHELL,'-NoProfile','-ExecutionPolicy','Bypass','-File',windows(ROOT/'tools/game_window.ps1'),'-TargetPid',str(pid),'-ExpectedSha256',sha,'-Action',action,'-Keys',keys,'-AllowBackground'],capture_output=True,text=True,timeout=20)
  if r.returncode:raise RuntimeError(r.stderr)
  return json.loads(r.stdout.lstrip('\ufeff'))
 wait(lambda: (runtime/'ship-events.log').exists() and 'full ship module ready' in (runtime/'ship-events.log').read_text(),40)
 time.sleep(2)
 for _ in range(5):
  if rows() and rows()[-1]['p1']>=100000:break
  ui('press');time.sleep(2)
 baseline=wait(lambda: next((r for r in reversed(rows()) if r['p1']>=100000 and r['b1']>=100000),None))
 check('one native P1',baseline['p1_asset']==603,baseline)
 if '--upgrade-flow' not in sys.argv and '--death-effects' not in sys.argv:control(join=1)
 joined=wait(lambda: next((r for r in reversed(rows()) if r.get('ready') and r['p2']>=100000),None))
 check('two distinct native player ships',joined['p1']!=joined['p2'] and joined['p1_asset']==joined['p2_asset']==603,joined)
 check('two distinct build owners',joined['b1']!=joined['b2'] and joined['hull1']>0 and joined['hull2']>0,joined)
 if '--upgrade-flow' in sys.argv:
  from qa_upgrade_flow import run
  run(globals());report['status']='PASS';raise SystemExit(0)
 if '--death-effects' in sys.argv:
  from qa_death_effects import run
  run(globals());report['status']='PASS';raise SystemExit(0)
 if '--stock-hud' in sys.argv and '--mutation-stock-hud' in sys.argv:
  from qa_stock_hud import run
  run(globals());raise RuntimeError('Shared-stock mutation unexpectedly completed')
 if '--tray-controls' in sys.argv:
  from qa_tray_controls import run
  run(globals());report['status']='PASS';raise SystemExit(0)
 if '--dual-controls' in sys.argv:
  from qa_dual_controls import run
  run(globals());report['status']='PASS';raise SystemExit(0)
 if '--starter-bank' in sys.argv:
  from qa_upgrade_flow import starters
  tray=starters(globals())
  control(level=1);wait(lambda:tray(1)['budget']==tray(2)['budget']==1)
  control(upgrade=1,deferred_upgrade=2);wait(lambda:tray(1)['open'] and tray(2)['open'])
  control(tray=1);wait(lambda:not tray(1)['open'])
  control(tray_action=1,owner=2,tray_pick=0);wait(lambda:tray(2)['budget']==0)
  check('P1 banked ordinary choice does not block P2',tray(1)['budget']==1 and rows(True)[-1]['ui']==0,[tray(1),tray(2)])
  ui('close');process.wait(timeout=20);check('clean owned banked-starter shutdown',process.returncode==0,process.returncode)
  report['status']='PASS';raise SystemExit(0)
 if any(flag in sys.argv for flag in ('--trays','--overlays','--stock-hud')):
  def trayrows():return [r for r in read_jsonl(runtime/'ship-balance.jsonl') if r.get('kind')=='tray']
  def tray(owner):return next((r for r in reversed(trayrows()) if r['owner']==owner),None)
  from qa_upgrade_flow import starters
  starters(globals())
  if '--stock-hud' in sys.argv:
   from qa_stock_hud import run
   run(globals());report['status']='PASS';raise SystemExit(0)
  control(augment=8,owner=2);wait(lambda:tray(2)['weapon']==269)
  control(level=3);wait(lambda:tray(1)['budget']>=3 and tray(2)['budget']>=3)
  if '--overlays' in sys.argv:
   from qa_overlay_checks import run
   run(globals());report['status']='PASS';raise SystemExit(0)
  c=control(tray=1,tray_enemy=1);wait(lambda:tray(1)['open']==1)
  c=control(engine_input=1,engine_key=0x76);wait(lambda:tray(2)['open']==1)
  control(engine_input=1)
  check('both personal trays open without native modal UI',rows(True)[-1]['ui']==rows(True)[-1]['paused']==rows(True)[-1]['menu_session']==0,{'first':tray(1),'second':tray(2)})
  image=runtime.parent/'test-profile/Local/NovaDrift/fullship-screen.png'
  stamp=image.stat().st_mtime_ns if image.exists() else 0
  control(screen=1);wait(lambda:image.exists() and image.stat().st_mtime_ns!=stamp)
  shutil.copy2(image,out/'both-upgrade-trays.png')
  control(engine_input=1,engine_key=ord('O'));time.sleep(.3)
  stamp=image.stat().st_mtime_ns
  control(engine_input=1,engine_key=ord('O'),screen=1);wait(lambda:image.stat().st_mtime_ns!=stamp)
  shutil.copy2(image,out/'compact-native-inspect.png')
  control(engine_input=1);time.sleep(.2)
  first=rows()[-1];time.sleep(1);last=rows()[-1]
  check('both native ships keep stepping while both trays are open',last['steps1']>first['steps1']+10 and last['steps2']>first['steps2']+10,{'first':first,'last':last})
  enemy_samples=[r for r in trayrows() if r['owner']==1 and r['open'] and r.get('enemy',-1)>=100000]
  check('native enemies advance while both players choose',len(enemy_samples)>1 and any(a['enemy']==b['enemy'] and abs(a['enemy_x']-b['enemy_x'])+abs(a['enemy_y']-b['enemy_y'])>1 for a in enemy_samples for b in enemy_samples),enemy_samples[-1] if enemy_samples else [])
  cards=tray(2)['cards'];rolls=tray(2)['rerolls'];generated=tray(2)['generated']
  control(tray=2);wait(lambda:tray(2)['open']==0)
  control(tray=2);wait(lambda:tray(2)['open']==1)
  check('collapse and reopen preserve exact offers without reroll',tray(2)['cards']==cards and tray(2)['generated']==generated and tray(2)['rerolls']==rolls,tray(2))
  control(tray=2);wait(lambda:tray(2)['open']==0)
  original=rows(True)[-1]
  control(engine_input=1,engine_key=ord('N'));wait(lambda:rows(True)[-1]['shots2']>original['shots2'])
  check('P2 can fire while P1 keeps choosing',tray(1)['open']==1 and tray(2)['open']==0,rows(True)[-1])
  budget=tray(2)['budget']
  control(tray=2,engine_input=1,engine_key=ord('N'));wait(lambda:tray(2)['open']==1)
  time.sleep(.4)
  check('held fire does not buy an upgrade on opening',tray(2)['budget']==budget,tray(2))
  control(engine_input=1);time.sleep(.2)
  budgets=(tray(1)['budget'],tray(2)['budget']);mods=rows(True)[-1];selected1=tray(1)['cards'][0];selected2=tray(2)['cards'][0]
  control(tray_action=1,owner=1,tray_pick=0);wait(lambda:tray(1)['budget']==budgets[0]-1)
  check('P1 native purchase leaves P2 budget and offers unchanged',tray(2)['budget']==budgets[1] and tray(2)['cards']==cards and tray(2)['open']==1,{'first':tray(1),'second':tray(2)})
  control(engine_input=1,engine_key=ord('O'),engine_key2=ord('N'));wait(lambda:tray(2)['budget']==budgets[1]-1)
  control(engine_input=1)
  wait(lambda:selected1 in tray(1)['acquired'] and selected2 in tray(2)['acquired'])
  check('selected native augments reach the correct builds',selected1 in tray(1)['acquired'] and selected2 in tray(2)['acquired'],{'first':tray(1),'second':tray(2)})
  check('P2 native purchase consumes only its own choice',tray(1)['budget']==budgets[0]-1 and rows(True)[-1]['ui']==0,{'first':tray(1),'second':tray(2)})
  if 217 not in tray(2)['acquired']:
   first_acquired=tray(1)['acquired']
   control(native_augment=217,owner=2);wait(lambda:217 in tray(2)['acquired'])
   check('standalone native Turret alarm belongs to P2',tray(1)['acquired']==first_acquired,{'first':tray(1),'second':tray(2)})
  control(tray=2);wait(lambda:tray(2)['open']==1)
  first=tray(1);rolls=tray(2)['rerolls'];budgets=tray(2)['budget'];generated=tray(2)['generated']
  control(tray_action=2,owner=2);wait(lambda:tray(2)['generated']>generated)
  check('native reroll charges only owner and preserves choices',tray(2)['rerolls']==rolls-1 and tray(2)['budget']==budgets and tray(1)['rerolls']==first['rerolls'],{'first':tray(1),'second':tray(2)})
  before_failure=tray(2)
  control(tray_action=2,owner=2,tray_fail=1);wait(lambda:tray(2)['failed']>before_failure['failed'])
  check('failed native reroll restores currency and exact offers',all(tray(2)[key]==before_failure[key] for key in ('cards','rerolls','budget','stage','bans')),{'before':before_failure,'after':tray(2)})
  control()
  # Change P2 to native Researcher, then use the real banish entry point.
  control(tray=2);wait(lambda:tray(2)['open']==0)
  control(augment=24,owner=2);wait(lambda:tray(2)['body']==8)
  control(tray=2);wait(lambda:tray(2)['open']==1)
  generation_before=tray(2)['generated'];budget=tray(2)['budget'];rolls=tray(2)['rerolls'];partner_rolls=tray(1)['rerolls']
  control(tray_action=3,owner=2,tray_pick=0)
  wait(lambda:tray(2)['generated']>generation_before)
  check('Researcher native banish charges only that player',tray(2)['rerolls']<rolls and tray(2)['budget']==budget and tray(1)['rerolls']==partner_rolls,{'first':tray(1),'second':tray(2)})
  before_failure=tray(2)
  control(tray_action=3,owner=2,tray_pick=0,tray_fail=1);wait(lambda:tray(2)['failed']>before_failure['failed'])
  check('failed native banish restores ban list currency and offers',all(tray(2)[key]==before_failure[key] for key in ('cards','rerolls','budget','stage','bans')),{'before':before_failure,'after':tray(2)})
  control()
  # Confirm an actually offered native gear replacement as well as mods.
  before_gear=tray(2);partner_gear=tray(1)
  control(tray_action=1,owner=2,tray_pick=len(before_gear['cards'])-1);wait(lambda:tray(2)['budget']==before_gear['budget']-1)
  wait(lambda:any(tray(2)[key]!=before_gear[key] for key in ('weapon','body','shield_type')))
  check('native gear replacement belongs only to buyer',all(tray(1)[key]==partner_gear[key] for key in ('weapon','body','shield_type','budget')),{'before':before_gear,'after':tray(2)})
  control(tray=2);wait(lambda:tray(2)['open']==1)
  check('temporary native brokers release their lists',all(r.get('leaked_lists')==0 for r in trayrows()),tray(2))
  ui('press','ESCAPE');wait(lambda:rows(True)[-1]['paused']==1)
  ui('press','ESCAPE');wait(lambda:rows(True)[-1]['paused']==0)
  check('manual pause survives personal tray use',tray(2)['open']==1 and rows(True)[-1]['ui']==0,tray(2))
  control(tray=2);wait(lambda:tray(2)['open']==0)
  control(native_augment=20,owner=2);wait(lambda:tray(2)['helix_helpers']>0)
  time.sleep(.8)
  check('P2 Orbital shield helpers keep their own build context',tray(2)['helix_helpers']>0 and rows(True)[-1]['ui']==0,tray(2))
  control(tray=2);wait(lambda:tray(2)['open']==1)
  control(kill=2);wait(lambda:tray(2)['open']==0 and rows()[-1]['p2']==-1)
  check('ship death invalidates that owner offers',tray(2)['cards']==[],tray(2))
  ui('close');process.wait(timeout=20);check('clean owned tray shutdown',process.returncode==0,process.returncode)
  report['status']='PASS';raise SystemExit(0)
 if '--polish' in sys.argv:
  def flow_rows():return read_jsonl(runtime/'ship-balance.jsonl')
  def state():return next((r for r in reversed(flow_rows()) if r.get('kind')=='state'),None)
  def rescue(owner):return next((r for r in reversed(flow_rows()) if r.get('kind')=='rescue' and r['owner']==owner),None)
  wait(lambda:state() and state()['zoom_scale']>=1.19999)
  zoom=[r['zoom_scale'] for r in flow_rows() if r.get('kind')=='state' and r.get('zoom_target')==1.2]
  check('joining animates through intermediate native view scales',any(1.001<x<1.199 for x in zoom) and zoom[-1]>=1.19999,zoom)
  initial=rows()[-1];generation=initial['generation'];builds=(initial['b1'],initial['b2'])
  for owner in (2,1):
   c=control(kill=owner);wait(lambda:rows()[-1]['p'+str(owner)]==-1)
   wait(lambda:rescue(owner) and rescue(owner)['down']==1)
   time.sleep(.7);check('death creates a delayed revive beacon',rescue(owner)['age']<5 and rescue(owner)['progress']==0,rescue(owner))
   time.sleep(.6)
   check('arena stays expanded while teammate is down',abs(state()['zoom_scale']-1.2)<.00001 and abs(state()['zoom_target']-1.2)<.00001,state())
   c=control(rescue_hold=owner)
   wait(lambda:rescue(owner)['age']>=5 and rescue(owner)['progress']>.3,30)
   ui('press','ESCAPE');wait(lambda:rows(True)[-1]['paused']==1)
   paused=rescue(owner)['progress'];time.sleep(.6)
   check('native pause freezes revive progress',abs(rescue(owner)['progress']-paused)<.04,rescue(owner))
   ui('press','ESCAPE');wait(lambda:rows(True)[-1]['paused']==0)
   revived=wait(lambda:rows()[-1] if rows()[-1]['p'+str(owner)]>=100000 else None,20)
   wait(lambda:rescue(owner)['down']==0)
   health=rescue(owner)
   check('native revive preserves both builds and run generation',revived['generation']==generation and (revived['b1'],revived['b2'])==builds,revived)
   check('revive returns half hull with native protection',.49<health['hull']/health['hullmax']<.51 and health['invulnerability']>90,health)
   wait(lambda:state()['zoom_scale']>=1.19999)
   c=control(collision=owner)
   collision=wait(lambda:next((r for r in reversed(flow_rows()) if r.get('kind')=='collision_probe' and r['counter']==c),None))
   check('native revive protection rejects enemy projectile collision',collision['timer']>0 and collision['hull']==collision['hull_after'] and collision['shield']==collision['shield_after'] and collision['partner_timer']==collision['partner_timer_after'],collision)
   wait(lambda:rescue(owner)['invulnerability']==0,8)
   c=control(collision=owner)
   collision=wait(lambda:next((r for r in reversed(flow_rows()) if r.get('kind')=='collision_probe' and r['counter']==c),None))
   check('native protection expires and the same collision can damage',collision['timer']==0 and collision['hull_after']+collision['shield_after']<collision['hull']+collision['shield'],collision)
  c=control(kill=1);wait(lambda:rows()[-1]['p1']==-1)
  c=control(kill=2);wait(lambda:rows(True)[-1]['game_over']==1)
  time.sleep(.5);check('both dead retain native game over without automatic revival',rows()[-1]['p1']==rows()[-1]['p2']==-1,rows(True)[-1])
  ui('close');process.wait(timeout=20);check('clean owned polish shutdown',process.returncode==0,process.returncode)
  report['status']='PASS';raise SystemExit(0)
 if '--balance' in sys.argv:
  def balance_rows():return read_jsonl(runtime/'ship-balance.jsonl')
  def state():return next((r for r in reversed(balance_rows()) if r.get('kind')=='state'),None)
  wait(lambda:state() and state()['width']>0 and state().get('zoom_scale',1.2)>1.19999)
  c=control(balance=1,balance_mutate=int('--mutation-balance-target' in sys.argv))
  wait(lambda:any(r.get('kind')=='probe_done' and r['counter']==c for r in balance_rows()))
  evidence=[r for r in balance_rows() if r.get('counter')==c];targets={r['phase']:r for r in evidence if r['kind']=='target_probe'}
  for phase,owner in [('p2_close',2),('p1_close',1),('wrap_p2',2),('p2_cloaked',1)]:
   r=targets[phase];dx=abs(r['target_x']-r['x'+str(owner)]);dx=min(dx%r['width'],r['width']-dx%r['width'])
   check('native enemy target and aim: '+phase,r['target']==r['p'+str(owner)] and dx<1 and r['owner']==0,r)
  arena=next(r for r in evidence if r['kind']=='arena_probe')
  check('arena grows in both axes without early native wrapping',abs(arena['width']/arena['native_width']-1.2)<.001 and abs(arena['height']/arena['native_height']-1.2)<.001 and abs(arena['inside_x']-arena['after_wrap_x'])<.01 and arena['inside_x']<arena['left']+(arena['width']-arena['native_width'])/2,arena)
  xp=[r for r in evidence if r['kind']=='xp_probe']
  check('ordinary XP scaled; scripted intro reward preserved',len(xp)==4 and all(abs(r['delta']/r['native_multiplier']-(1 if r['asset']==285 else .7))<.001 for r in xp),xp)
  spawns=[r for r in balance_rows() if r['kind']=='spawn'];native=sum(r['native_count'] for r in spawns);tuned=sum(r['tuned_count'] for r in spawns)
  check('native regular-wave group budget increases with bounded carry',native>0 and tuned>native and abs(tuned-1.6*native)<1.01,{'groups':len(spawns),'native_count':native,'tuned_count':tuned})
  c=control(kill=2);wait(lambda:rows()[-1]['p2']==-1 and rows()[-1]['p1']>=100000)
  c=control(balance=2);wait(lambda:any(r.get('kind')=='probe_done' and r['counter']==c for r in balance_rows()))
  survivor=next(r for r in balance_rows() if r.get('kind')=='target_probe' and r['counter']==c)
  check('native enemy targets surviving P1 after P2 dies',survivor['target']==survivor['p1'] and survivor['owner']==0,survivor)
  c=control(join=1);wait(lambda:rows()[-1]['p2']>=100000)
  prior=state();c=control(augment=114,owner=2)
  trait2=wait(lambda:state() if state()['counter']==c and state()['added2']==prior['added2']+2 else None)
  check('P2 native Grandeur grants only P2 bonus choices',trait2['level']==prior['level'] and trait2['base_level']==prior['base_level'] and trait2['choices2']==prior['choices2']+2 and trait2['choices1']==prior['choices1'] and trait2['effective1']==prior['effective1'],{'prior':prior,'after':trait2})
  prior=trait2;c=control(augment=114,owner=1)
  trait1=wait(lambda:state() if state()['counter']==c and state()['added1']==prior['added1']+2 else None)
  check('P1 native Grandeur grants only P1 bonus choices',trait1['level']==prior['level'] and trait1['choices1']==prior['choices1']+2 and trait1['choices2']==prior['choices2'] and trait1['effective2']==prior['effective2'],{'prior':prior,'after':trait1})
  prior=trait1;c=control(xp=1)
  level=wait(lambda:state() if state()['counter']==c and state()['level']==prior['level']+1 else None)
  check('shared XP still grants a separate choice to each build',level['choices1']==prior['choices1']+1 and level['choices2']==prior['choices2']+1,{'prior':prior,'after':level})
  control(screen=1);time.sleep(.3)
  ui('close');process.wait(timeout=20);check('clean owned balance shutdown',process.returncode==0,process.returncode)
  report['status']='PASS';raise SystemExit(0)
 if '--native-preview' in sys.argv:
  image=runtime.parent/'test-profile/Local/NovaDrift/fullship-screen.png'
  old=image.stat().st_mtime_ns if image.exists() else 0
  control(nativehud=1);time.sleep(.4)
  control(nativehud=1,screen=1)
  wait(lambda:image.exists() and image.stat().st_mtime_ns!=old)
  shutil.copy2(image,out/'native-hud-preview.png')
  ui('close');process.wait(timeout=20);check('clean native HUD preview',process.returncode==0,process.returncode)
  report['status']='PASS';raise SystemExit(0)
 if '--presentation-controls' in sys.argv:
  from PIL import Image
  def uistate():return read_jsonl(runtime/'ship-ui.jsonl')
  profile_path=runtime/'p2-controls.ini';config_before=profile_path.read_bytes()
  def snapshot(name):
   path=runtime.parent/'test-profile/Local/NovaDrift/fullship-screen.png'
   old=path.stat().st_mtime_ns if path.exists() else 0
   control(screen=1);wait(lambda:path.exists() and path.stat().st_mtime_ns!=old)
   image=out/(name+'.png');shutil.copy2(path,image)
   for owner in (1,2):
    native=runtime.parent/f'test-profile/Local/NovaDrift/native-hud-{owner}.png'
    wait(lambda:native.exists())
    shutil.copy2(native,out/f'{name}-native-{owner}.png')
   return image
  colors=wait(lambda:next((r for r in reversed(uistate()) if r['marker1']>0 and r['marker2']>0),None))
  check('actual ships and markers use red and gold',colors['color1']==(246|(91<<8)|(105<<16)) and colors['color2']==(255|(214<<8)|(64<<16)),colors)
  check('native health UI composed for both players',colors.get('native_hud')==1 and colors.get('native_frames',0)>0,colors)
  snapshot('two-players-full-health')
  wait(lambda:uistate()[-1].get('hint1')==0 and uistate()[-1].get('hint2')==0,8)
  snapshot('native-ui-hints-faded')
  check('identity and control hints fade after joining',uistate()[-1]['hint1']==uistate()[-1]['hint2']==0,uistate()[-1])
  before_hud=uistate()[-1];c=control(damage=35,owner=2)
  damaged=wait(lambda:next((r for r in reversed(uistate()) if r['command']==c and r['hull2']<before_hud['hull2']-20),None))
  check('P2 HUD health changes independently',damaged['hull1']==before_hud['hull1'] and damaged['shield2']<before_hud['shield2']-20,{'before':before_hud,'after':damaged})
  image=snapshot('p2-damaged')
  time.sleep(.7)
  image=snapshot('p2-damaged-settled');geometry=uistate()[-1]
  check('native bars use independent health fractions',geometry['bar1']>.95 and .4<geometry['bar2']<.85,geometry)
  output=Image.open(image).convert('RGB');w,h=output.size
  for owner in (1,2):
   native=Image.open(out/f'p2-damaged-settled-native-{owner}.png').convert('RGBA');nw,nh=native.size
   sx,sy,sw,sh=[geometry[f'src{owner}_{key}'] for key in ('x','y','w','h')]
   dx,dy,dw,dh=[geometry[f'dst{owner}_{key}'] for key in ('x','y','w','h')]
   samples=matches=0
   for yy in range(math.ceil(sy),math.floor(sy+sh)):
    for xx in range(math.ceil(sx),math.floor(sx+sw)):
     px=native.getpixel((xx,yy))
     if px[3]<250:continue
     ox=round((dx+(xx-sx)*dw/sw)*w/nw);oy=round((dy+(yy-sy)*dh/sh)*h/nh)
     if not (0<=ox<w and 0<=oy<h):continue
     samples+=1;matches+=sum(abs(a-b) for a,b in zip(px[:3],output.getpixel((ox,oy))))<35
   check(f'P{owner} uses rendered native widget pixels',samples>20 and matches/samples>.75,{'samples':samples,'matching':matches,'image':str(image)})
  # Persist through the real Win32 configuration dialog, using its explicit test profile.
  tool=runtime/'NovaDriftP2Controls.exe';result=subprocess.run([str(tool),'--self-test',windows(out/'editor-test.ini')],timeout=20)
  check('control editor key capture and persisted form',result.returncode==0,result.returncode)
  # Change Fire to P on the live profile; physical-key QA uses the actual profile translator.
  text=config_before.decode();text=text.replace('fire=78','fire=80',1)
  if text==config_before.decode():raise RuntimeError('Test rebind did not change profile')
  profile_path.write_text(text)
  wait(lambda:uistate()[-1]['fire_key']==ord('P'))
  control(augment=8,owner=2);time.sleep(.5)
  c=control(key=ord('N'));time.sleep(.4);oldshots=rows(True)[-1]['shots2'];time.sleep(.7)
  check('old fire key no longer fires',rows(True)[-1]['shots2']==oldshots,rows(True)[-1])
  c=control(key=ord('P'));wait(lambda:rows(True)[-1]['shots2']>oldshots)
  check('rebound P2 fire works in native gameplay',rows(True)[-1]['shots2']>oldshots,rows(True)[-1])
  # K now means steer down, rather than the old stabilization/back action.
  c=control(key=ord('K'));time.sleep(2)
  angles=[r['angle2'] for r in rows() if r.get('command')==c]
  check('directional down turns toward native heading 270',angles and abs((angles[-1]-270+180)%360-180)<20,angles[-8:])
  control();profile_path.write_bytes(config_before);wait(lambda:uistate()[-1]['fire_key']==ord('N'))
  check('saved defaults reload during play',uistate()[-1]['steering']==0,uistate()[-1])
  # Engine keyboard state is the game's native non-consuming held-key source.
  # Bind P2 fire to P1's S alternative to prove keyboard/mouse separation.
  text=config_before.decode().replace('fire=78','fire=83',1);profile_path.write_text(text)
  wait(lambda:uistate()[-1]['fire_key']==ord('S'))
  original_shots=rows(True)[-1]
  c=control(engine_input=1,engine_key=ord('S'))
  wait(lambda:uistate()[-1].get('p2_fire')==1 and rows(True)[-1]['shots2']>original_shots['shots2'])
  time.sleep(.4)
  check('engine keyboard input fires only P2 with a conflicting key',rows(True)[-1]['shots1']==original_shots['shots1'] and rows(True)[-1]['shots2']>original_shots['shots2'],{'before':original_shots,'after':rows(True)[-1]})
  control(engine_input=1)
  wait(lambda:uistate()[-1].get('p2_fire')==0)
  time.sleep(.4);released=rows(True)[-1]['shots2'];time.sleep(.5)
  check('engine key release stops native P2 fire',rows(True)[-1]['shots2']==released,rows(True)[-1])
  control();profile_path.write_bytes(config_before);wait(lambda:uistate()[-1]['fire_key']==ord('N'))
  control(xp=1);wait(lambda:rows(True)[-1]['choices1']>0 and rows(True)[-1]['choices2']>0)
  control(upgrade=1);wait(lambda:personal_tray(1)['open'] and personal_tray(2)['open'])
  check('configured upgrades stay compact',rows(True)[-1]['ui']==rows(True)[-1]['menu_session']==0,rows(True)[-1])
  snapshot('both-native-starter-trays')
  control(tray_action=1,owner=1,tray_pick=0);wait(lambda:rows(True)[-1]['choices1']==0)
  control(engine_input=1,engine_key=ord('O'),engine_key2=ord('K'));time.sleep(.25);control(engine_input=1);time.sleep(.15)
  check('P2 down browses without cancelling',personal_tray(2)['open'] and rows(True)[-1]['choices2']>0,personal_tray(2))
  snapshot('p2-native-starter-tray')
  control(engine_input=1,engine_key=ord('O'),engine_key2=ord('N'));time.sleep(.25);control(engine_input=1)
  wait(lambda:rows(True)[-1]['ui']==0 and rows(True)[-1]['choices2']==0)
  check('configured compact navigation and confirm',rows(True)[-1]['choices2']==0,rows(True)[-1])
  # Exercise the same deferred pause/editor route used by the physical F6 hotkey.
  control(configure=1)
  def editor_pid():
   matches=re.findall(r'P2 controls editor opened pid=(\d+)',(runtime/'ship-events.log').read_text())
   return int(matches[-1]) if matches else None
  editor=wait(editor_pid)
  toolsha=hashlib.sha256(tool.read_bytes()).hexdigest()
  def editor_window(action):
   result=subprocess.run([POWERSHELL,'-NoProfile','-ExecutionPolicy','Bypass','-File',windows(ROOT/'tools/config_window.ps1'),'-TargetPid',str(editor),'-ExpectedSha256',toolsha,'-Action',action],capture_output=True,text=True,timeout=20)
   if result.returncode:raise RuntimeError(result.stderr)
   return json.loads(result.stdout.lstrip('\ufeff'))
  editor_state=editor_window('status')
  check('F6 route opens visible P2 configuration window',any('PLAYER TWO CONTROLS' in label for label in editor_state['labels']),editor_state)
  editor_window('close');ui('press','ESCAPE');time.sleep(.3)
  ui('close');process.wait(timeout=20);check('clean owned shutdown',process.returncode==0,process.returncode)
  report['status']='PASS';raise SystemExit(0)

 if '--solo-ui' in sys.argv:
  control(kill=1);wait(lambda:rows()[-1]['p1']==-1 and rows()[-1]['p2']>=100000)
  control(xp=1);wait(lambda:personal_tray(2)['open'])
  control(tray_action=1,owner=2,tray_pick=1);wait(lambda:rows(True)[-1]['choices2']==0)
  check('solo P2 compact navigation and confirm',rows(True)[-1]['ui']==0,rows(True)[-1])
  ui('close');process.wait(timeout=20);report['status']='PASS';raise SystemExit(0)

 time.sleep(2)
 move=phase(2,thrust=1)
 path=sum(math.hypot(b['x2']-a['x2'],b['y2']-a['y2']) for a,b in zip(move,move[1:]) if abs(b['x2']-a['x2'])<150 and abs(b['y2']-a['y2'])<150)
 check('P2 native thrust',path>25,{'path':path})
 turn=phase(1,left=1)
 check('P2 native turn',abs(turn[-1]['angle2']-turn[0]['angle2'])>10,[turn[0]['angle2'],turn[-1]['angle2']])
 c=control(augment=8,owner=2);time.sleep(1)
 phase(2,fire=1)
 metric=wait(lambda: next((r for r in reversed(rows(True)) if r['shots2']>0),None),8)
 check('P2 creates native weapon projectiles',metric['shots2']>0,metric)
 original=rows()[-1]
 c=control(augment=168,owner=2,mutate=int(mutation))
 changed=wait(lambda: next((r for r in reversed(rows()) if r['command']==c and abs(r['rof2']-original['rof2'])>.01),None))
 check('P2 asynchronous Rapid Fire only',changed['rof1']==original['rof1'] and changed['rof2']>original['rof2'],{'before':original,'after':changed})
 c=control(augment=168,owner=1)
 changed1=wait(lambda: next((r for r in reversed(rows()) if r['command']==c and abs(r['rof1']-changed['rof1'])>.01),None))
 check('P1 asynchronous Rapid Fire only',changed1['rof2']==changed['rof2'] and changed1['rof1']>changed['rof1'],changed1)
 before_hull=rows(True)[-1];c=control(augment=123,owner=2)
 hullmod=wait(lambda: next((r for r in reversed(rows(True)) if r['command']==c and r['maxhull2']>before_hull['maxhull2']),None))
 check('P2 Hull Strength leaves P1 hull build unchanged',hullmod['maxhull1']==before_hull['maxhull1'],{'before':before_hull,'after':hullmod})
 previous=rows(True)[-1];c=control(xp=1)
 awarded=wait(lambda: next((r for r in reversed(rows(True)) if r['command']==c and r['level']>previous['level'] and r['choices2']>previous['choices2']),None))
 check('shared native XP awards two independent choices',awarded['level']==previous['level']+1 and awarded['choices1']==previous['choices1']+1 and awarded['choices2']==previous['choices2']+1,{'before':previous,'after':awarded})
 c=control(upgrade=1)
 wait(lambda:personal_tray(1)['open'] and personal_tray(2)['open'])
 opened=rows(True)[-1]
 check('both native starter offer sets use compact trays',opened['ui']==opened['menu_session']==0,opened)
 control(tray_action=1,owner=1,tray_pick=0)
 wait(lambda:rows(True)[-1]['choices1']==awarded['choices1']-1)
 time.sleep(.4);second=rows(True)[-1]
 check('P1 selection leaves P2 gear unchanged',second['weapon2']==awarded['weapon2'] and second['weapon1']!=awarded['weapon1'],second)
 control(tray_action=1,owner=2,tray_pick=1)
 wait(lambda:rows(True)[-1]['choices2']==awarded['choices2']-1)
 time.sleep(.4);closed=rows(True)[-1]
 check('each player spends only own choice',closed['choices1']==awarded['choices1']-1 and closed['choices2']==awarded['choices2']-1,closed)
 check('P2 selection leaves P1 gear unchanged',closed['weapon1']==second['weapon1'] and closed['weapon2']!=second['weapon2'],closed)
 phase(1,fire=1)
 state=rows()[-1];old_p2=state['p2'];old_b2=state['b2'];old_rof=state['rof2']
 c=control(kill=2)
 dead=wait(lambda: next((r for r in reversed(rows()) if r['command']==c and r['p2']==-1 and r['p1']>=100000),None))
 check('native P2 death preserves P1',dead['p1']==state['p1'] and dead['b2']==old_b2,dead)
 time.sleep(.6);live=rows()[-1]
 check('P1 continues after partner death',live['steps1']>dead['steps1']+10,live)
 c=control(join=1)
 respawn=wait(lambda: next((r for r in reversed(rows()) if r['command']==c and r['p2']>=100000 and r['p2']!=old_p2),None))
 check('P2 redeploy retains own augments',respawn['b2']==old_b2 and respawn['rof2']==old_rof and respawn['hull2']>0,respawn)
 phase(.8,thrust=1)
 old_generation=respawn['generation']
 ui('press','ESCAPE');time.sleep(.5)
 c=control(restart=1)
 wait(lambda: 'QA paused native restart object create' in (runtime/'ship-events.log').read_text(),10)
 control()
 for _ in range(5):
  if rows() and rows()[-1]['generation']>old_generation and rows()[-1]['p1']>=100000:break
  ui('press','ENTER');time.sleep(1)
 new=wait(lambda: next((r for r in reversed(rows()) if r['generation']>old_generation and r['p1']>=100000),None),15)
 check('native restart creates new run',new['generation']>old_generation,new)
 c=control(join=1)
 new2=wait(lambda: next((r for r in reversed(rows()) if r['command']==c and r['p2']>=100000 and r['b2']!=old_b2),None))
 check('fresh independent P2 after restart',new2['p2']!=old_p2 and new2['hull2']>0,new2)
 phase(.8,thrust=1)
 # Reverse death order: P2 remains a playable native ship when P1 dies.
 c=control(kill=1)
 first_dead=wait(lambda: next((r for r in reversed(rows()) if r['command']==c and r['p1']==-1 and r['p2']>=100000),None))
 check('native P1 death preserves P2',first_dead['p2']==new2['p2'],first_dead)
 time.sleep(.6)
 check('P2 continues after P1 death',rows()[-1]['steps2']>first_dead['steps2']+10,rows()[-1])
 survivor_ui=read_jsonl(runtime/'ship-ui.jsonl')[-1]
 check('surviving P2 retains native health display',survivor_ui['native_hud']==1 and survivor_ui['bar2']>.9,survivor_ui)
 previous=rows(True)[-1];c=control(xp=1)
 solo_award=wait(lambda: next((r for r in reversed(rows(True)) if r['command']==c and r['level']>previous['level'] and r['choices2']>previous['choices2']),None))
 check('surviving P2 still gains team XP choice',solo_award['level']==previous['level']+1,solo_award)
 wait(lambda:personal_tray(2)['open'])
 solo_ui=rows(True)[-1]
 control(tray_action=1,owner=2,tray_pick=1)
 wait(lambda:rows(True)[-1]['choices2']==previous['choices2'])
 check('surviving P2 can select upgrades',rows(True)[-1]['choices2']==previous['choices2'],rows(True)[-1])
 # The shared Controller now runs in P2 scope. Its native Escape binding must
 # still pause/resume even though the P2 upgrade Back key is independent.
 ui('press','ESCAPE')
 paused=wait(lambda: next((r for r in reversed(rows(True)) if r['time']>solo_ui['time'] and r.get('paused')==1),None))
 check('surviving P2 retains shared native pause',paused['paused']==1,paused)
 time.sleep(.3);stopped=rows()[-1]['steps2'];time.sleep(.3)
 check('surviving P2 stops stepping while paused',rows()[-1]['steps2']==stopped,rows()[-1])
 ui('press','ESCAPE')
 resumed=wait(lambda: next((r for r in reversed(rows(True)) if r['time']>paused['time'] and r.get('paused')==0),None))
 wait(lambda: rows()[-1]['steps2']>stopped+10)
 check('surviving P2 resumes through native Escape',resumed['paused']==0,resumed)
 # Exercise final-player death and the native score-screen controls. Do not
 # create the restart object through QA; only real native confirm events follow.
 old_generation=rows()[-1]['generation']
 c=control(kill=2)
 wait(lambda:rows()[-1]['p1']==-1 and rows()[-1]['p2']==-1)
 ended=wait(lambda: next((r for r in reversed(rows(True)) if r['command']==c and r.get('game_over')==1 and r.get('end_score_instances')==1),None))
 check('final combatant death reaches native score screen',ended['end_score_instances']==1,ended)
 time.sleep(2)
 image=runtime.parent/'test-profile/Local/NovaDrift/fullship-screen.png'
 old=image.stat().st_mtime_ns if image.exists() else 0
 control(screen=1);wait(lambda:image.exists() and image.stat().st_mtime_ns!=old)
 shutil.copy2(image,out/'double-death-native-score.png')
 control()
 for _ in range(8):
  if rows()[-1]['generation']>old_generation and rows()[-1]['p1']>=100000:break
  ui('press','ENTER');time.sleep(1)
 restarted=wait(lambda:next((r for r in reversed(rows()) if r['generation']>old_generation and r['p1']>=100000),None),5)
 check('native end-screen confirmation starts fresh run',restarted['generation']>old_generation,restarted)
 control(screen=1);time.sleep(.3)
 ui('close');process.wait(timeout=20);check('clean owned shutdown',process.returncode==0,process.returncode)
 report['status']='PASS'
except Exception as e:
 report['error']=str(e);print('ERROR',str(e),flush=True)
finally:
 if config_before is not None:(runtime/'p2-controls.ini').write_bytes(config_before)
 if process and process.poll() is None and pid:
  (runtime/f'stop-{pid}.request').write_text('owned fullship QA cleanup')
  try:process.wait(timeout=20)
  except subprocess.TimeoutExpired:report['cleanup_error']='Owned launcher did not stop'
 for name in ['ship-events.log','ship-telemetry.jsonl','ship-ui.jsonl','ship-performance.jsonl','ship-balance.jsonl','aurie.log']:
  p=runtime/name
  if p.exists():(out/name).write_bytes(p.read_bytes())
 if mouse_fixture is not None:mouse_fixture[0].write_bytes(mouse_fixture[1])
 if saved_controls_fixture is not None:
  path,previous=saved_controls_fixture
  if previous is None:path.unlink(missing_ok=True)
  else:path.write_bytes(previous)
 preserved=before==fingerprint(ORIGINAL_SAVES)
 report['original_saves_unchanged']=preserved
 if not preserved:report['status']='FAIL'
 (out/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(out,report['status'],flush=True)
 if report['status']!='PASS':sys.exit(1)
sys.exit(0 if report['status']=='PASS' else 1)
