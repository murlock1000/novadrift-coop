"""Real mouse-only purchases with a locked pointer; no cursor placement fixture."""
import json,math,subprocess,time
import shutil

def run(ns):
 from qa_upgrade_flow import starters
 tray=starters(ns)
 control,wait,check,rows,ui=(ns[k] for k in ('control','wait','check','rows','ui'))
 identity=ui('focus')
 def mouse(event):
  r=subprocess.run([ns['POWERSHELL'],'-NoProfile','-ExecutionPolicy','Bypass','-File',ns['windows'](ns['ROOT']/'tools/game_window.ps1'),
   '-TargetPid',str(ns['pid']),'-ExpectedStartTicks',str(identity['start_ticks']),'-ExpectedSha256',ns['sha'],'-Action','mouse','-MouseEvent',event],capture_output=True,text=True,timeout=20)
  with (ns['out']/'mouse-events.jsonl').open('a') as log:log.write(json.dumps({'event':event,'stdout':r.stdout,'stderr':r.stderr})+'\n')
  if r.returncode:raise RuntimeError(r.stderr)
 def view():return ns['read_jsonl'](ns['runtime']/'ship-ui.jsonl')[-1]
 def metrics():return rows(True)[-1]
 def snap():return [dict(tray(1)),dict(tray(2))]
 def capture(name):
  info=ui('focus');handle=next(w['handle'] for w in info['windows'] if w['title'].startswith('Nova Drift'))
  path=ns['runtime'].parent/'test-profile/Temp'/name
  command=('& {\n'+(ns['ROOT']/'tools/capture_game.ps1').read_text()+'\n} -TargetPid '+str(ns['pid'])+
   ' -ExpectedStartTicks '+str(info['start_ticks'])+' -GameWindowHandle '+str(handle)+" -ExpectedSha256 '"+ns['sha']+"' -OutputPath '"+ns['windows'](path).replace("'","''")+"'")
  r=subprocess.run([ns['POWERSHELL'],'-NoProfile','-Command',command],capture_output=True,text=True,timeout=20)
  if r.returncode:raise RuntimeError(r.stderr)
  shutil.copyfile(path,ns['out']/name)
 first=ns['runtime']/'p1-controls.ini';previous=first.read_bytes() if first.exists() else None
 try:
  for owner in (1,2):
   control(augment=9,owner=owner);time.sleep(.2)
   control(augment=8,owner=owner);time.sleep(.2)
  control(level=10);wait(lambda:tray(1)['budget']>=10 and tray(2)['budget']>=10)
  control(upgrade=1,deferred_upgrade=2,engine_input=1)
  wait(lambda:tray(1)['open'] and tray(2)['open'])
  check('mouse-only test uses no pointer placement fixture','cursor_fixture=1' not in (ns['runtime']/'ship-control.txt').read_text(),snap())
  selected=tray(1)['selected'];count=len(tray(1)['cards']);mouse('wheelDown')
  wait(lambda:tray(1)['selected']==(selected+1)%count)
  check('real mouse wheel changes the highlighted native offer',True,snap())
  before=snap();mouse('leftDown');time.sleep(.3)
  check('ordinary thrust cannot buy a highlighted offer',tray(1)['budget']>=before[0]['budget'] and tray(1)['purchased']==before[0]['purchased'],snap())
  mouse('middleDown')
  # The old build closes the tray here: this must fail before a purchase.
  wait(lambda:tray(1)['open'] and tray(1).get('inspect')==1,3)
  check('holding middle opens native item information without a pointer',tray(1)['focus']==0,snap())
  check('holding thrust before Inspect cannot buy',tray(1)['budget']==before[0]['budget'],snap())
  capture('mouse-shop-inspect.png')
  mouse('leftUp');before=snap();mouse('leftDown')
  wait(lambda:tray(1)['budget']==before[0]['budget']-1)
  check('middle plus fresh left click buys exactly one P1 item',tray(1)['purchased']==before[0]['purchased']+1 and tray(2)['budget']==before[1]['budget'] and tray(2)['cards']==before[1]['cards'],snap())
  check('purchase consumes thrust until release',view()['p1_thrust']==0 and tray(1)['click']==1,view())
  control(upgrade=1,engine_input=1);wait(lambda:tray(1)['open']);time.sleep(.2)
  check('held purchase click cannot buy from the next offers',tray(1)['budget']==before[0]['budget']-1,snap())
  mouse('leftUp');mouse('middleUp');time.sleep(.15)
  check('releasing used middle gesture keeps the next offers open',tray(1)['open']==1,snap())
  mouse('middleClick');wait(lambda:tray(1)['open']==0)
  mouse('middleClick');wait(lambda:tray(1)['open']==1)
  check('short middle taps still close and reopen the shop',True,snap())
  before=snap();mouse('rightDown');mouse('middleDown');wait(lambda:tray(1)['inspect']==1)
  check('already held fire never spends a reroll',tray(1)['rerolls']==before[0]['rerolls'],snap())
  mouse('rightUp');mouse('rightDown');wait(lambda:tray(1)['rerolls']==before[0]['rerolls']-1)
  check('middle plus fresh right click rerolls only P1',tray(2)['rerolls']==before[1]['rerolls'] and tray(1)['budget']==before[0]['budget'],snap())
  time.sleep(.2);shots=metrics()['shots1'];time.sleep(.5)
  check('held reroll click does not keep firing',tray(1)['rightclick']==1 and metrics()['shots1']==shots,metrics())
  mouse('rightUp');mouse('middleUp');time.sleep(.15)
  check('releasing middle after reroll keeps shop open',tray(1)['open']==1,snap())
  capture('mouse-shop-strip.png')
  control(engine_input=1,beacon_center=2);time.sleep(.2)
  mouse('leftDown');control(engine_input=1,engine_key=ord('I'))
  wait(lambda:math.hypot(rows()[-1]['vx1'],rows()[-1]['vy1'])>.5 and math.hypot(rows()[-1]['vx2'],rows()[-1]['vy2'])>.5,4)
  check('both ships fly with cursorless shop visible',metrics()['paused']==metrics()['ui']==0,rows()[-1])
  mouse('leftUp');control(engine_input=1)
  check('no native full-screen shop is needed',metrics()['ui']==metrics()['menu_session']==0,metrics())
  first.write_text('[controls]\nmouse_aim=1\nsteering=tank\ndevice=keyboard\n[keys]\nup=1\nfire=2\n')
  time.sleep(1.3);before=snap();control(engine_input=1,engine_key=ord('O'));mouse('leftDown');time.sleep(.3)
  check('P2 Inspect cannot turn P1 thrust into a purchase',tray(1)['inspect']==0 and tray(1)['budget']>=before[0]['budget'] and tray(1)['purchased']==before[0]['purchased'],snap())
  mouse('leftUp');control(engine_input=1)
  first.write_text('[controls]\nmouse_aim=1\nsteering=tank\ndevice=keyboard\n[keys]\nup=1\nfire=86\nupgrade=4\n')
  time.sleep(1.3);mouse('middleDown');wait(lambda:tray(1)['inspect']==1)
  before=tray(1)['rerolls'];mouse('rightDown');wait(lambda:tray(1)['rerolls']==before-1)
  shots=metrics()['shots1'];control(engine_input=1,engine_key=ord('V'))
  wait(lambda:metrics()['shots1']>shots)
  check('consuming RMB preserves an unrelated rebound fire key',tray(1)['rightclick']==1,metrics())
  control(engine_input=1);mouse('rightUp');mouse('middleUp')
 finally:
  if ns['process'].poll() is None:
   for event in ('leftUp','rightUp','middleUp'):
    try:mouse(event)
    except Exception:pass
   control(engine_input=1)
  if previous is None:first.unlink(missing_ok=True)
  else:first.write_bytes(previous)
 ui('close');ns['process'].wait(timeout=20)
 check('clean owned mouse-only shop shutdown',ns['process'].returncode==0,ns['process'].returncode)
