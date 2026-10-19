"""Native compact starter choices, independent builds, and pause inventories."""
import shutil
import subprocess
import time


def starters(ns):
    control, wait, check, rows = (ns[k] for k in ('control', 'wait', 'check', 'rows'))
    runtime = ns['runtime']
    def tray(owner):
        return next((r for r in reversed(ns['read_jsonl'](runtime/'ship-balance.jsonl'))
                     if r.get('kind') == 'tray' and r['owner'] == owner), None)
    ns['tray'] = tray
    wait(lambda: tray(1) and tray(2))
    check('automatic join preserves both starter stages',
          tray(1)['stage'] == tray(2)['stage'] == 1,
          [tray(1), tray(2)])
    control(level=3)
    wait(lambda: rows(True)[-1]['ui'] > 0 or (tray(1)['open'] and tray(2)['open']))
    check('starting choices never open the full-screen shop',
          rows(True)[-1]['ui'] == rows(True)[-1]['menu_session'] == rows(True)[-1]['paused'] == 0,
          rows(True)[-1])
    c = control(upgrade=1, deferred_upgrade=2)
    wait(lambda: rows(True)[-1]['command'] == c)
    time.sleep(.3)
    check('starting choices never open the full-screen shop',
          rows(True)[-1]['ui'] == rows(True)[-1]['menu_session'] == rows(True)[-1]['paused'] == 0,
          rows(True)[-1])
    for stage in (1, 2, 3):
        wait(lambda: tray(1)['open'] and tray(2)['open'] and
             tray(1)['stage'] == tray(2)['stage'] == stage)
        check('both owners receive native compact starter offers',
              rows(True)[-1]['ui'] == rows(True)[-1]['menu_session'] ==
              rows(True)[-1]['paused'] == 0 and
              len(tray(1)['cards']) > 1 and len(tray(2)['cards']) > 1,
              {'stage': stage, 'trays': [tray(1), tray(2)]})
        before = [dict(tray(1)), dict(tray(2))]
        # Bastion has a distinct shield animation; the subsequent flash probe
        # specifically exercises the ordinary native shield-break bursts.
        first_pick = next(i for i, card in enumerate(before[0]['cards'])
                          if stage != 2 or card != 4)
        control(tray_action=1, owner=1, tray_pick=first_pick)
        wait(lambda: tray(1)['stage'] == stage+1)
        check('starter purchase affects only its owner',
              tray(2)['stage'] == stage and
              tray(2)['budget'] == before[1]['budget'] and
              tray(2)['cards'] == before[1]['cards'], [tray(1), tray(2)])
        # The native rolls are independent: the same index can name a different
        # object, or two different indices can happen to name the same gear.
        second_pick = next(i for i, card in enumerate(before[1]['cards'])
                           if card != before[0]['cards'][first_pick] and
                           (stage != 2 or card != 4))
        control(tray_action=1, owner=2, tray_pick=second_pick)
        wait(lambda: tray(2)['stage'] == stage+1)
        check('each starter choice spends exactly one personal point',
              tray(1)['budget'] == before[0]['budget']-1 and
              tray(2)['budget'] == before[1]['budget']-1, [tray(1), tray(2)])
        time.sleep(.2)  # native gear Alarm applies the chosen build
    wait(lambda: tray(1)['weapon'] != tray(2)['weapon'] and
         tray(1)['body'] != tray(2)['body'] and
         tray(1)['shield_type'] != tray(2)['shield_type'])
    check('both ships keep separate weapons shields and bodies', True, [tray(1), tray(2)])
    return tray


def run(ns):
    tray = starters(ns)
    control, wait, check, rows, ui = (ns[k] for k in ('control', 'wait', 'check', 'rows', 'ui'))
    def records(kind):
        return [r for r in ns['read_jsonl'](ns['runtime']/'ship-balance.jsonl') if r.get('kind') == kind]
    def capture(name):
        identity = ui('focus')
        handle = next(w['handle'] for w in identity['windows'] if w['title'].startswith('Nova Drift'))
        path = ns['runtime'].parent/'test-profile/Temp'/name
        command = ('& {\n'+(ns['ROOT']/'tools/capture_game.ps1').read_text()+
                   '\n} -TargetPid '+str(ns['pid'])+' -ExpectedStartTicks '+str(identity['start_ticks'])+
                   ' -GameWindowHandle '+str(handle)+" -ExpectedSha256 '"+ns['sha']+
                   "' -OutputPath '"+ns['windows'](path)+"'")
        subprocess.run([ns['POWERSHELL'], '-NoProfile', '-Command', command], check=True, capture_output=True, text=True, timeout=20)
        shutil.copyfile(path, ns['out']/name)
        return ns['out']/name
    control(level=2)
    wait(lambda: tray(1)['budget'] >= 2 and tray(2)['budget'] >= 2)
    control(upgrade=1, deferred_upgrade=2)
    wait(lambda: tray(1)['open'] and tray(2)['open'])
    check('native normal and deferred entry both open compact trays',
          rows(True)[-1]['ui'] == rows(True)[-1]['menu_session'] == rows(True)[-1]['paused'] == 0,
          [tray(1), tray(2)])
    before = [dict(tray(1)), dict(tray(2))]
    control(tray_action=1, owner=1, tray_pick=0)
    wait(lambda: tray(1)['budget'] == before[0]['budget']-1)
    control(tray_action=1, owner=2, tray_pick=1)
    wait(lambda: tray(2)['budget'] == before[1]['budget']-1)
    wait(lambda: before[0]['cards'][0] in tray(1)['acquired'] and
         before[1]['cards'][1] in tray(2)['acquired'])
    check('compact native augments reach each independent build',
          rows(True)[-1]['ui'] == 0, [tray(1), tray(2)])
    # Break the actual native shield. Powers Step2 creates its real GUI bursts.
    for owner in (1, 2):
        c = control(shield_break=owner)
        wait(lambda: any(r['owner'] == owner and r['counter'] == c for r in records('health_effect')), 6)
        effects = [r for r in records('health_effect') if r['owner'] == owner and r['counter'] == c]
        check('native shield flashes follow their owner HUD',
              all((r['x'] < r['width']*.45 if owner == 1 else r['x'] > r['width']*.55) for r in effects), effects[-3:])
    ui('press', 'ESCAPE')
    wait(lambda: rows(True)[-1]['paused'] == 1)
    wait(lambda: len(records('pause_build')) >= 2)
    inventories = records('pause_build')[-2:]
    for owner in (1, 2):
        inventory = next(r for r in inventories if r['owner'] == owner)
        check('pause shows each build in its own native icon section',
              inventory['mods'] == tray(owner)['acquired'] and len(inventory['graphics']) >= 8 and
              all((g['x'] < 600 if owner == 1 else g['x'] > 1200) for g in inventory['graphics']), inventory)
    time.sleep(.5)
    capture('separate-native-pause-builds.png')
    c = control(engine_input=1, engine_key=9)
    wait(lambda: any(r['counter'] == c and r['owner'] == 2 for r in records('pause_stats')))
    stats = records('pause_stats')[-1]
    check('pause stats switch to the second independent build',
          all(stats[k] == tray(2)[k] for k in ('weapon', 'body', 'shield_type')), stats)
    control(engine_input=1)
    capture('second-native-pause-stats.png')
    ui('press', 'ESCAPE')
    wait(lambda: rows(True)[-1]['paused'] == 0)
    check('manual Escape pause remains functional', True, rows(True)[-1])
    ui('close')
    ns['process'].wait(timeout=20)
    check('clean owned upgrade-flow shutdown', ns['process'].returncode == 0, ns['process'].returncode)
