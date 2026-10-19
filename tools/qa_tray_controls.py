"""Actual ship input with both compact trays visible; no inertia-only checks."""
import json
import math
import subprocess
import time


def run(ns):
    from qa_upgrade_flow import starters
    tray = starters(ns)
    control, wait, check, rows, ui = (ns[k] for k in ('control', 'wait', 'check', 'rows', 'ui'))
    identity = ui('focus')
    def mouse(event):
        r = subprocess.run([ns['POWERSHELL'], '-NoProfile', '-ExecutionPolicy', 'Bypass',
            '-File', ns['windows'](ns['ROOT']/'tools/game_window.ps1'),
            '-TargetPid', str(ns['pid']), '-ExpectedStartTicks', str(identity['start_ticks']),
            '-ExpectedSha256', ns['sha'], '-Action', 'mouse', '-MouseEvent', event],
            capture_output=True, text=True, timeout=20)
        with (ns['out']/'mouse-events.jsonl').open('a') as log:
            log.write(json.dumps({'event':event,'result':r.stdout,'error':r.stderr})+'\n')
        if r.returncode:
            raise RuntimeError(r.stderr)
    def point(x,y):
        # Native gameplay uses raw relative device deltas, which Windows
        # SendInput cannot reproduce. Position its real world-space cursor as
        # a QA fixture, then use actual Windows mouse-button events to choose.
        state=tray(1)
        control(engine_input=1,cursor_fixture=1,cursor_x=round(x*state['w']),cursor_y=round(y*state['h']))
        try:
            wait(lambda: abs(tray(1)['mx']-x*state['w'])<8 and abs(tray(1)['my']-y*state['h'])<8,3)
        except RuntimeError:
            check('native software cursor reaches the upgrade hex',False,tray(1))
    def metrics():
        return rows(True)[-1]
    def p1_thrust():
        return ns['read_jsonl'](ns['runtime']/'ship-ui.jsonl')[-1]['p1_thrust']
    def p2_thrust():
        return ns['read_jsonl'](ns['runtime']/'ship-ui.jsonl')[-1]['p2_thrust']
    def snapshot():
        return [dict(tray(1)), dict(tray(2))]
    def same_offers(before):
        return all(all(tray(owner)[k] == before[owner-1][k]
                       for k in ('cards', 'rerolls', 'selected', 'stage', 'purchased')) and
                   tray(owner)['budget']>=before[owner-1]['budget'] for owner in (1, 2))
    def rest():
        c = control(engine_input=1, beacon_center=2)
        wait(lambda: rows()[-1]['command'] == c)
        time.sleep(.2)
    try:
        for owner in (1, 2):
            control(augment=9, owner=owner);time.sleep(.3)
            control(augment=8, owner=owner);time.sleep(.3)
        control(level=20)
        wait(lambda: tray(1)['budget'] >= 20 and tray(2)['budget'] >= 20)
        control(upgrade=1, deferred_upgrade=2, engine_input=1)
        wait(lambda: tray(1)['open'] and tray(2)['open'])
        rest()
        point(.8,.3)
        before = snapshot()
        check('both trays are passive during live combat',
              metrics()['ui'] == metrics()['paused'] == metrics()['menu_session'] == 0 and
              tray(1)['focus'] == tray(2)['focus'] == 0, snapshot())
        mouse('leftDown')
        control(engine_input=1, engine_key=ord('I'))
        time.sleep(.7)
        motion = rows()[-1]
        check('both ships accelerate from rest with their trays open',
              all(math.hypot(motion['vx'+str(o)], motion['vy'+str(o)]) > .5 for o in (1, 2)), motion)
        mouse('leftUp')
        control(engine_input=1)
        check('thrust does not browse or purchase upgrades', same_offers(before), snapshot())
        angle=rows()[-1]['angle1']
        point(.2,.3);time.sleep(.4)
        check('native cursor still steers P1 with both trays visible',
              abs(math.remainder(rows()[-1]['angle1']-angle,360))>5, rows()[-1])
        shots = metrics()
        mouse('rightDown')
        control(engine_input=1, engine_key=ord('N'))
        wait(lambda: metrics()['shots1'] > shots['shots1'] and metrics()['shots2'] > shots['shots2'], 5)
        mouse('rightUp')
        control(engine_input=1)
        check('both players fire without buying from visible trays', same_offers(before), metrics())
        rest()
        angle = rows()[-1]['angle2']
        control(engine_input=1, engine_key=ord('L'))
        time.sleep(.5)
        check('P2 steering still reaches the ship with the shop visible',
              abs(math.remainder(rows()[-1]['angle2']-angle, 360)) > 5 and same_offers(before), rows()[-1])
        control(engine_input=1)
        # Entering shop focus with Fire already held must never purchase.
        control(engine_input=1, engine_key=ord('N'));time.sleep(.2)
        control(engine_input=1, engine_key=ord('N'), engine_key2=ord('O'));time.sleep(.3)
        check('holding fire before Inspect cannot accidentally buy', same_offers(before), snapshot())
        control(engine_input=1, engine_key=ord('O'));time.sleep(.2)
        selected = tray(2)['selected']
        control(engine_input=1, engine_key=ord('O'), engine_key2=ord('L'))
        wait(lambda: tray(2)['selected'] != selected)
        check('Inspect plus direction deliberately browses P2 offers', tray(2)['focus'] == 1, snapshot())
        control(engine_input=1, engine_key=ord('O'));time.sleep(.2)
        stage = tray(2)['stage'];purchases=tray(2)['purchased'];partner = dict(tray(1))
        control(engine_input=1, engine_key=ord('O'), engine_key2=ord('N'))
        wait(lambda: tray(2)['stage'] == stage+1)
        time.sleep(.3)
        check('Inspect plus confirm buys exactly one personal upgrade',
              tray(2)['stage'] == stage+1 and tray(2)['purchased']==purchases+1 and all(tray(1)[k] == partner[k] for k in ('cards','stage','rerolls')), snapshot())
        # Keep confirmation held: only Fire is consumed; steering must return.
        control(engine_input=1, engine_key=ord('N'), beacon_center=2)
        time.sleep(.2)
        resume = control(engine_input=1, engine_key=ord('N'), engine_key2=ord('I'))
        # Wait for this input command and measured movement rather than
        # sampling at an arbitrary wall-clock offset. The permanent input
        # latch mutation still cannot satisfy this bounded movement check.
        wait(lambda: rows()[-1]['command'] == resume and
             math.hypot(rows()[-1]['vx2'], rows()[-1]['vy2']) > .5, 3)
        check('flight resumes after choosing without a blanket input latch',
              p2_thrust() and math.hypot(rows()[-1]['vx2'], rows()[-1]['vy2']) > .5, rows()[-1])
        control(engine_input=1)
        # Mouse purchase consumes only LMB. RMB fire and P2 movement remain live.
        rest()
        partner = dict(tray(2));shots = metrics()
        mouse('rightDown')
        # The gameplay pointer is hidden: choose the highlighted offer with
        # an explicit mouse chord, without aiming at the shop.
        mouse('middleDown')
        wait(lambda:tray(1)['inspect'])
        stage = tray(1)['stage']
        mouse('leftDown')
        control(engine_input=1, engine_key=ord('I'))
        wait(lambda: tray(1)['stage'] == stage+1)
        shots = metrics()
        wait(lambda: metrics()['shots1'] > shots['shots1'])
        time.sleep(.3)
        check('mouse purchase preserves firing and teammate thrust',
              tray(2)['stage'] == partner['stage'] and math.hypot(rows()[-1]['vx2'], rows()[-1]['vy2']) > .5, rows()[-1])
        check('consumed shop click does not thrust P1 until released',
              not p1_thrust() and tray(1)['click']==1,
              {'ship': rows()[-1], 'thrust':p1_thrust(), 'click':tray(1)['click']})
        mouse('leftUp');mouse('rightUp');mouse('middleUp')
        control(engine_input=1)
        rest();mouse('leftDown');time.sleep(.5)
        check('P1 thrust returns on the next fresh mouse press',
              math.hypot(rows()[-1]['vx1'], rows()[-1]['vy1']) > .5, rows()[-1])
        mouse('leftUp')
        # Exercise the same consumption path with a rebound P1 mouse profile.
        first = ns['runtime']/'p1-controls.ini'
        previous = first.read_bytes() if first.exists() else None
        try:
            first.write_text('[controls]\ndevice=keyboard\nsteering=directional\nmouse_aim=1\n[keys]\nup=1\nfire=2\n')
            time.sleep(1.3)
            control(tray=1, engine_input=1);wait(lambda: tray(1)['open'])
            rest();stage=tray(1)['stage'];shots=metrics()
            mouse('rightDown');mouse('middleDown');wait(lambda:tray(1)['inspect']);mouse('leftDown')
            wait(lambda: tray(1)['stage']==stage+1)
            wait(lambda: metrics()['shots1']>shots['shots1'])
            check('rebound P1 mouse shop click consumes thrust but keeps fire',
                  not p1_thrust() and tray(1)['click']==1, {'ship':rows()[-1], 'thrust':p1_thrust(), 'click':tray(1)['click']})
            mouse('leftUp');mouse('rightUp');mouse('middleUp');rest();point(.5,.1);mouse('leftDown')
            wait(lambda:math.hypot(rows()[-1]['vx1'],rows()[-1]['vy1'])>.5,3)
            check('rebound P1 mouse thrust resumes after selection',
                  math.hypot(rows()[-1]['vx1'],rows()[-1]['vy1'])>.5, rows()[-1])
            mouse('leftUp')
        finally:
            if previous is None:first.unlink(missing_ok=True)
            else:first.write_bytes(previous)
        ui('press','ESCAPE');wait(lambda:metrics()['paused']==1)
        ui('press','ESCAPE');wait(lambda:metrics()['paused']==0)
        check('Escape remains a shared pause after shop input',True,metrics())
    finally:
        if ns['process'].poll() is None:
            mouse('leftUp');mouse('rightUp');mouse('middleUp');control(engine_input=1)
    ui('close');ns['process'].wait(timeout=20)
    check('clean owned tray-controls shutdown',ns['process'].returncode==0,ns['process'].returncode)
