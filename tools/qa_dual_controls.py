"""Real native P1/P2 rebinding, hot reload and compact keyboard-tray checks."""
import math,time

def run(ns):
    control,wait,check,rows,ui=(ns[k] for k in ('control','wait','check','rows','ui'))
    runtime,read_jsonl=ns['runtime'],ns['read_jsonl']
    first=runtime/'p1-controls.ini';second=runtime/'p2-controls.ini'
    previous=first.read_bytes() if first.exists() else None
    second_before=second.read_bytes()
    def write(fire='B'):
        first.write_text('[controls]\nsteering=directional\ndevice=keyboard\nmouse_aim=0\ncontroller_slot=0\n[keys]\nup=84\nleft=70\ndown=71\nright=72\nfire='+str(ord(fire))+'\nupgrade=120\ndeploy=67\n')
        time.sleep(1.2)
    def metrics():return rows(True)[-1]
    def tray(owner):return next(r for r in reversed(read_jsonl(runtime/'ship-balance.jsonl')) if r.get('kind')=='tray' and r['owner']==owner)
    try:
        from qa_upgrade_flow import starters
        starters(ns)
        # Equip the native single-shot weapon for both to avoid firing tails.
        for owner in (1,2):control(augment=8,owner=owner);time.sleep(.2)
        first.write_text('[controls]\nsteering=tank\ndevice=keyboard\nmouse_aim=1\ncontroller_slot=0\n[keys]\nup=1\nfire=2\n')
        time.sleep(1.2);control(key=2);time.sleep(.7)
        check('P1 mouse-button binding reaches native fire',metrics()['shots1']>0,metrics())
        control(engine_input=1);time.sleep(.3)
        write();control(engine_input=1);time.sleep(.5)
        for key,owner in [('B',1),('N',2)]:
            before=metrics();control(engine_input=1,engine_key=ord(key));time.sleep(1)
            control(engine_input=1);time.sleep(.3);after=metrics()
            check('rebound key fires only its own ship',after['shots'+str(owner)]>before['shots'+str(owner)] and after['shots'+str(3-owner)]==before['shots'+str(3-owner)],{'key':key,'before':before,'after':after})
        write('V');before=metrics();control(engine_input=1,engine_key=ord('B'));time.sleep(.5);control(engine_input=1);time.sleep(.2)
        check('P1 hot reload releases old fire binding',metrics()['shots1']==before['shots1'],metrics())
        control(engine_input=1,engine_key=ord('V'));time.sleep(.7);control(engine_input=1);time.sleep(.2)
        check('P1 hot reload applies new fire binding',metrics()['shots1']>before['shots1'],metrics())
        before=rows()[-1];control(engine_input=1,engine_key=ord('T'));time.sleep(.8);control(engine_input=1);after=rows()[-1]
        check('P1 custom directional movement advances native ship',math.hypot(after['x1']-before['x1'],after['y1']-before['y1'])>5,{'before':before,'after':after})
        control(level=2);wait(lambda:tray(1)['budget']>=2)
        control(engine_input=1,engine_key=120);wait(lambda:tray(1)['open']==1);control(engine_input=1)
        before=tray(1)['budget'];partner=tray(2)['budget']
        control(engine_input=1,engine_key=ord('V'));wait(lambda:tray(1)['budget']==before-1);control(engine_input=1)
        check('P1 keyboard tray uses own confirm and budget',tray(2)['budget']==partner,{'first':tray(1),'second':tray(2)})
        ui('press','ESCAPE');wait(lambda:metrics()['paused']==1)
        ui('press','ESCAPE');wait(lambda:metrics()['paused']==0)
        check('shared pause works with custom P1 controls',metrics()['paused']==0,metrics())
        check('editing P1 preserves P2 profile',second.read_bytes()==second_before,{})
        old_generation=rows()[-1]['generation']
        control(kill=1);wait(lambda:rows()[-1]['p1']==-1)
        control(kill=2);wait(lambda:metrics()['game_over']==1)
        for _ in range(8):
            ui('press','ENTER');time.sleep(1)
            if rows()[-1]['generation']>old_generation:break
        check('native game-over restart remains usable with P1 profile',rows()[-1]['generation']>old_generation,metrics())
        ui('close');ns['process'].wait(timeout=20)
        check('clean dual controls shutdown',ns['process'].returncode==0,ns['process'].returncode)
    finally:
        if previous is None:first.unlink(missing_ok=True)
        else:first.write_bytes(previous)
