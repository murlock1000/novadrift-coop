"""Native ship death with a live thrust effect, survivor combat and revival."""
import time


def run(ns):
    from qa_upgrade_flow import starters

    tray = starters(ns)
    control, wait, check, rows, ui = (
        ns[k] for k in ('control', 'wait', 'check', 'rows', 'ui'))
    runtime = ns['runtime']
    profile = runtime/'p1-controls.ini'
    previous = profile.read_bytes() if profile.exists() else None

    def send(**changes):
        return control(rescue_mode=1, **changes)

    def metrics():
        return rows(True)[-1]

    def rescue(owner):
        return next((r for r in reversed(ns['read_jsonl'](runtime/'ship-balance.jsonl'))
                     if r.get('kind') == 'rescue' and r['owner'] == owner), None)

    def build(owner):
        current = tray(owner)
        return {key: current[key] for key in
                ('body', 'weapon', 'shield_type', 'acquired')}

    def fire(owner, holding=0):
        """Release Salvo as well as holding fire; verify actual projectiles."""
        before = metrics()['shots'+str(owner)]
        settings = {'rescue_hold': holding}
        if owner == 1:
            send(engine_input=1, engine_key=ord('B'), **settings)
            time.sleep(.7)
            send(engine_input=1, **settings)
        else:
            send(fire=1, **settings)
            time.sleep(.7)
            send(**settings)
        wait(lambda: metrics()['shots'+str(owner)] > before, 6)
        return {'before': before, 'after': metrics()['shots'+str(owner)]}

    try:
        # Use a known P1 input instead of assuming the isolated native prefs.
        profile.write_text('[controls]\ndevice=keyboard\nsteering=tank\n'
                           'mouse_aim=0\n[keys]\nup=84\nfire=66\nupgrade=120\n')
        time.sleep(1.3)
        for owner in (1, 2):
            for asset in (18, 25, 23):  # Hullbreaker, Salvo, Reflect.
                send(augment=asset, owner=owner)
                time.sleep(.25)
            wait(lambda: tray(owner)['body'] == 3 and tray(owner)['weapon'] == 649
                 and tray(owner)['shield_type'] == 4)
        check('death fixture uses native Hullbreaker Salvo and Reflect for both pilots',
              all(tray(o)['body'] == 3 and tray(o)['weapon'] == 649
                  and tray(o)['shield_type'] == 4 for o in (1, 2)),
              [build(1), build(2)])

        initial = rows()[-1]
        generation = initial['generation']
        powers = (initial['b1'], initial['b2'])
        builds = [build(1), build(2)]
        for owner in (2, 1):
            partner = 3-owner
            before = rows()[-1]
            # playerThrust initializes decay, sprite and lifetime after Create;
            # constructing bare asset317 omits those required native fields.
            send()
            wait(lambda: tray(owner)['thrust_effects'] == 0, 4)
            if owner == 2:
                send(thrust=1)
            else:
                send(engine_input=1, engine_key=ord('T'))
            wait(lambda: tray(owner).get('thrust_effects', 0) > 0, 4)
            check('native thrust effect is live before pilot death',
                  tray(owner)['thrust_effects'] > 0,
                  {'owner': owner, 'tray': tray(owner)})
            send(kill=owner, shield_break=owner, rescue_hold=owner)
            dead = wait(lambda: rows()[-1] if rows()[-1]['p'+str(owner)] == -1
                        and rows()[-1]['p'+str(partner)] == before['p'+str(partner)]
                        else None, 6)
            wait(lambda: tray(owner)['thrust_effects'] == 0, 3)
            check('pilot death retires its thrust effect and preserves both Powers',
                  tray(owner)['thrust_effects'] == 0 and
                  (dead['b1'], dead['b2']) == powers and
                  dead['generation'] == generation and
                  [build(1), build(2)] == builds,
                  {'owner': owner, 'ships': dead, 'builds': [build(1), build(2)]})
            shots = fire(partner, holding=owner)
            live = rows()[-1]
            check('survivor keeps native simulation and firing after partner death',
                  live['steps'+str(partner)] > dead['steps'+str(partner)]+10 and
                  metrics()['game_over'] == 0 and live['p'+str(owner)] == -1,
                  {'down': owner, 'ships': live, 'shots': shots})
            wait(lambda: rescue(owner) and rescue(owner)['down'] == 1
                 and rescue(owner)['progress'] > .3, 15)
            progress = dict(rescue(owner))
            revived = wait(lambda: rows()[-1] if rows()[-1]['p'+str(owner)] >= 100000
                           and rows()[-1]['p'+str(owner)] != before['p'+str(owner)]
                           else None, 8)
            wait(lambda: rescue(owner)['down'] == 0, 3)
            check('proximity beacon revives a new ship with the same native build',
                  progress['age'] >= 5 and progress['progress'] > .3 and
                  revived['generation'] == generation and
                  (revived['b1'], revived['b2']) == powers and
                  [build(1), build(2)] == builds and
                  revived['hull'+str(owner)] > 0,
                  {'owner': owner, 'progress': progress, 'ships': revived,
                   'builds': [build(1), build(2)]})
            send()
            shots = fire(owner)
            check('revived pilot resumes native Salvo firing',
                  [build(1), build(2)] == builds,
                  {'owner': owner, 'shots': shots, 'builds': [build(1), build(2)]})

        # Do not use the revive position fixture during the final team death.
        send(kill=1)
        wait(lambda: rows()[-1]['p1'] == -1, 6)
        send(kill=2)
        wait(lambda: rows()[-1]['p1'] == rows()[-1]['p2'] == -1 and
             metrics()['game_over'] == 1 and metrics()['end_score_instances'] == 1, 6)
        time.sleep(.5)
        check('both deaths preserve the native end screen without automatic revival',
              rows()[-1]['p1'] == rows()[-1]['p2'] == -1 and
              metrics()['game_over'] == metrics()['end_score_instances'] == 1,
              metrics())
        ui('close')
        ns['process'].wait(timeout=15)
        check('clean owned death-effects shutdown', ns['process'].returncode == 0,
              ns['process'].returncode)
    finally:
        if previous is None:
            profile.unlink(missing_ok=True)
        else:
            profile.write_bytes(previous)
