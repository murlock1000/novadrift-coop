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
process=None;pid=None;counter=0;config_before=None
before=fingerprint(ORIGINAL_SAVES)
def control(**changes):
 global counter
 counter+=1;c=dict(counter=counter,qa_focus=1,mutate=0,mutate_hud=int(ui_mutation),mutate_sharing=int(sharing_mutation));c.update(changes)
 temp=runtime/'ship-control.pending';temp.write_text(''.join(f'{k}={v}\n' for k,v in c.items()));os.replace(temp,runtime/'ship-control.txt');return counter
def rows(metrics=False):return [r for r in read_jsonl(runtime/'ship-telemetry.jsonl') if bool(r.get('metrics'))==metrics]
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
 control(join=1)
 joined=wait(lambda: next((r for r in reversed(rows()) if r.get('ready') and r['p2']>=100000),None))
 check('two distinct native player ships',joined['p1']!=joined['p2'] and joined['p1_asset']==joined['p2_asset']==603,joined)
 check('two distinct build owners',joined['b1']!=joined['b2'] and joined['hull1']>0 and joined['hull2']>0,joined)
 if '--balance' in sys.argv:
  def balance_rows():return read_jsonl(runtime/'ship-balance.jsonl')
  def state():return next((r for r in reversed(balance_rows()) if r.get('kind')=='state'),None)
  wait(lambda:state() and state()['width']>0)
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
  control(upgrade=1);wait(lambda:rows(True)[-1].get('menu_owner')==1 and rows(True)[-1].get('confirmable'))
  p1_image=snapshot('p1-red-upgrade');ui('press','ENTER')
  wait(lambda:rows(True)[-1].get('menu_owner')==2 and rows(True)[-1].get('confirmable'))
  control(key=ord('K'));time.sleep(.25);control();time.sleep(.15)
  check('P2 menu down no longer cancels upgrade',rows(True)[-1]['ui']==1 and rows(True)[-1]['choices2']>0,rows(True)[-1])
  p2_image=snapshot('p2-gold-upgrade')
  for path,color in [(p1_image,(246,91,105)),(p2_image,(255,214,64))]:
   img=Image.open(path).convert('RGB');iw,ih=img.size
   def accent(px):
    r,g,b=px
    return r>80 and r>1.4*g and r>1.2*b if color[1]<100 else r>100 and g>80 and b<.65*min(r,g) and abs(r-g)<100
   count=sum(accent(px) for px in img.crop((int(iw*.25),0,int(iw*.75),int(ih*.2))).getdata())
   check('upgrade owner banner rendered '+path.stem,count>150,{'pixels':count,'image':str(path)})
  wait(lambda:uistate()[-1].get('upgrade_hint')==0,5)
  snapshot('p2-gold-upgrade-hint-faded')
  check('upgrade player cue fades while menu remains open',rows(True)[-1]['ui']==1 and uistate()[-1]['upgrade_hint']==0,uistate()[-1])
  control(key=ord('L'));time.sleep(.15);control();time.sleep(.1)
  control(key=ord('N'));time.sleep(.25);control()
  wait(lambda:rows(True)[-1]['ui']==0 and rows(True)[-1]['choices2']==0)
  check('configured physical menu navigation and confirm',rows(True)[-1]['choices2']==0,rows(True)[-1])
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
  c=control(kill=1)
  wait(lambda: rows()[-1]['p1']==-1 and rows()[-1]['p2']>=100000)
  c=control(xp=1)
  wait(lambda: rows(True)[-1].get('ui')==1 and rows(True)[-1].get('confirmable')==1)
  print('SOLO MENU BEFORE',rows(True)[-1],flush=True)
  control(screen=1);time.sleep(.2)
  control(right=1);time.sleep(.3);control();time.sleep(.1)
  print('SOLO MENU SELECTED',rows(True)[-1],flush=True)
  control(right=1);time.sleep(.15);control();time.sleep(.1)
  control(confirm=1);time.sleep(.3);control()
  wait(lambda: rows(True)[-1].get('ui')==0 and rows(True)[-1].get('menu_session')==0)
  check('solo P2 menu navigation and confirm',rows(True)[-1]['choices2']==0,rows(True)[-1])
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
 opened=wait(lambda: next((r for r in reversed(rows(True)) if r['command']==c and r.get('ui')==1 and r.get('menu_owner')==1),None))
 check('native P1 upgrade screen',opened['ui']==1,opened)
 ui('press','ENTER');time.sleep(1)
 second=wait(lambda: next((r for r in reversed(rows(True)) if r.get('ui')==1 and r.get('menu_owner')==2 and r.get('confirmable')==1),None),20)
 check('automatic native P2 upgrade screen',second['choices1']==awarded['choices1']-1 and second['choices2']==awarded['choices2'],second)
 check('P1 selection leaves P2 gear unchanged',second['weapon2']==awarded['weapon2'] and second['weapon1']!=awarded['weapon1'],second)
 control(right=1);time.sleep(.15);control();time.sleep(.1)
 control(confirm=1);time.sleep(.3);control()
 closed=wait(lambda: next((r for r in reversed(rows(True)) if r['time']>second['time'] and r.get('ui')==0 and r.get('menu_session')==0),None),20)
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
 solo_ui=wait(lambda: next((r for r in reversed(rows(True)) if r.get('ui')==1 and r.get('menu_owner')==2 and r.get('confirmable')==1),None))
 control(right=1);time.sleep(.15);control();time.sleep(.1)
 control(confirm=1);time.sleep(.3);control()
 wait(lambda: rows(True)[-1].get('ui')==0 and rows(True)[-1].get('menu_session')==0)
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
 preserved=before==fingerprint(ORIGINAL_SAVES)
 report['original_saves_unchanged']=preserved
 if not preserved:report['status']='FAIL'
 (out/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(out,report['status'],flush=True)
 if report['status']!='PASS':sys.exit(1)
sys.exit(0 if report['status']=='PASS' else 1)
