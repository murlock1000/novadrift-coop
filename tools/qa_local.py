#!/usr/bin/env python3
"""Exercise the copied local co-op build. Missing runtime evidence is a failure.

Run after build/staging: python3 tools/qa_local.py --seconds 140
The launcher owns process termination. UI messages address only its PID/window;
all game state assertions use plugin telemetry. No desktop screenshot is taken.
"""
import argparse
import hashlib
import json
import math
import os
from pathlib import Path
from runtime_paths import ORIGINAL_SAVES
import re
import subprocess
import struct
import sys
import time
from datetime import datetime, timezone

ROOT = Path(__file__).resolve().parents[1]
POWERSHELL = '/mnt/c/Windows/System32/WindowsPowerShell/v1.0/powershell.exe'


def project_directory(path):
    resolved = path.resolve()
    try:
        relative = resolved.relative_to(ROOT)
    except ValueError:
        raise ValueError('QA artifact directories must stay inside the project: ' + str(path))
    if not relative.parts:
        raise ValueError('QA artifact directory must be a project subdirectory')
    return resolved


def game_windows(status):
    return [window for window in status.get('windows', [])
            if re.match(r'^Nova Drift', window.get('title', ''), re.IGNORECASE)]


def fingerprint(directory):
    if not directory.is_dir():
        raise RuntimeError('Original save directory is unavailable: ' + str(directory))
    return {str(p.relative_to(directory)): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in sorted(directory.rglob('*')) if p.is_file()}


def windows(path):
    return subprocess.check_output(['wslpath', '-w', str(path)], text=True).strip()


def read_jsonl(path):
    rows = []
    if path.exists():
        for line in path.read_text(errors='replace').splitlines():
            try:
                row = json.loads(line)
                if isinstance(row, dict):
                    rows.append(row)
            except (ValueError, TypeError):
                # Writers flush each line; the final line can be in flight.
                continue
    return rows


def excursion(rows, x, y):
    points = [(float(r[x]), float(r[y])) for r in rows]
    if any(not math.isfinite(v) or abs(v) >= 999999 for p in points for v in p):
        raise RuntimeError('Missing/invalid coordinates: ' + x)
    return max(math.hypot(p[0] - points[0][0], p[1] - points[0][1]) for p in points)


def directional_motion(rows, axis, sign):
    """Sum observed frame deltas, excluding screen-wrap jumps and telemetry gaps."""
    excursion(rows, 'x', 'y')  # Validate all coordinate values before measuring.
    excursion(rows, 'player_x', 'player_y')
    signed = perpendicular = player_path = 0.0
    accepted = skipped_wrap = skipped_gap = 0
    for previous, current in zip(rows, rows[1:]):
        dt = current['time_ms'] - previous['time_ms']
        if dt <= 0 or dt > 300:
            skipped_gap += 1
            continue
        dx = current['x'] - previous['x']
        dy = current['y'] - previous['y']
        pdx = current['player_x'] - previous['player_x']
        pdy = current['player_y'] - previous['player_y']
        # The game wraps coordinates. A discontinuity contributes no displacement.
        if max(abs(dx), abs(dy), abs(pdx), abs(pdy)) > 150:
            skipped_wrap += 1
            continue
        signed += sign * (dx if axis == 'x' else dy)
        perpendicular += abs(dy if axis == 'x' else dx)
        player_path += math.hypot(pdx, pdy)
        accepted += 1
    return dict(signed_displacement=signed, orthogonal_path=perpendicular,
                player_path=player_path, accepted_deltas=accepted,
                skipped_wrap=skipped_wrap, skipped_gap=skipped_gap)


def settled_player_window(rows, player_id, since_ms):
    """Wait for a fresh observed second of natural player motion <= 10 pixels."""
    observed = [r for r in rows if r.get('time_ms', -1) >= since_ms]
    if not observed:
        return None
    if any(r.get('player_alive') != 1 or r.get('player_id') != player_id
           or r.get('player_count') != 1 for r in observed):
        raise RuntimeError('Player died, changed identity, or changed count during intro settlement')
    latest = observed[-1]['time_ms']
    starts = [i for i, row in enumerate(observed) if row['time_ms'] <= latest - 1000]
    if not starts:
        return None
    window = observed[starts[-1]:]
    if len(window) < 6:
        return None
    gaps = [b['time_ms'] - a['time_ms'] for a, b in zip(window, window[1:])]
    if any(gap <= 0 or gap > 300 for gap in gaps):
        return None
    excursion(window, 'player_x', 'player_y')
    # Include all motion, including any screen wrap; no artificial freezing/input.
    distance = sum(math.hypot(b['player_x'] - a['player_x'],
                              b['player_y'] - a['player_y'])
                   for a, b in zip(window, window[1:]))
    if distance > 10:
        return None
    return dict(player_id=player_id, player_path=distance, player_path_limit=10,
                observed_ms=window[-1]['time_ms'] - window[0]['time_ms'],
                samples=len(window), x=window[-1]['player_x'], y=window[-1]['player_y'])


class Harness:
    def __init__(self, args):
        self.args = args
        self.runtime = project_directory(args.runtime)
        self.profile = project_directory(args.profile_root)
        self.screenshot_roots = [('profile', self.profile), ('runtime', self.runtime)]
        self.output = ROOT / 'local/qa' / datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S.%fZ')
        self.output.mkdir(parents=True)
        self.report = {'status': 'FAIL', 'checks': [], 'artifacts': str(self.output),
                       'mutation_movement': args.mutation_movement, 'profile_root': str(self.profile),
                       'suite': 'core' + ('+combat' if args.combat or args.lifecycle else '') + ('+lifecycle' if args.lifecycle else ''),
                       'combat_result': 'NOT_RUN' if args.combat or args.lifecycle else 'NOT_REQUESTED',
                       'basic_result': 'NOT_RUN', 'inconclusive_trials': [],
                       'lifecycle_result': 'NOT_RUN' if args.lifecycle else 'NOT_REQUESTED'}
        self.process = None
        self.pid = None
        self.start_ticks = 0
        self.counter = 0
        self.telemetry = self.runtime / 'coop-telemetry.jsonl'
        self.launch_log = self.output / 'launcher.log'

    def check(self, name, ok, details):
        self.report['checks'].append({'name': name, 'pass': bool(ok), 'details': details})
        print(('PASS ' if ok else 'FAIL ') + name + ': ' + str(details), flush=True)
        if not ok:
            raise RuntimeError(name + ': ' + str(details))

    def ui(self, action='status', keys='ENTER'):
        command = [POWERSHELL, '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File',
                   windows(ROOT / 'tools/game_window.ps1'), '-TargetPid', str(self.pid),
                   '-ExpectedSha256', self.executable_hash, '-Action', action,
                   '-ExpectedStartTicks', str(self.start_ticks), '-Keys', keys]
        # Every harness command explicitly opts in to the plugin's QA focus shim.
        if action == 'press':
            command.append('-AllowBackground')
        result = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                text=True, timeout=15)
        if result.returncode:
            raise RuntimeError('Owned-window helper failed: ' + result.stderr.strip())
        status = json.loads(result.stdout.strip().lstrip('\ufeff'))
        self.start_ticks = status['start_ticks']
        with (self.output / 'ui.jsonl').open('a') as stream:
            stream.write(json.dumps(status) + '\n')
        if action != 'close' and any(w.get('title') == 'Code Error' for w in status.get('windows', [])):
            error_log = self.runtime / 'aurie.log'
            diagnostic = error_log.read_text(errors='replace') if error_log.exists() else ''
            start = diagnostic.rfind('[critical]')
            raise RuntimeError('Owned GameMaker Code Error dialog: ' + diagnostic[max(0, start):][:2500])
        return status

    def alive(self):
        if self.process.poll() is not None:
            raise RuntimeError('Owned launcher/game exited early; inspect launcher.log')

    def wait(self, predicate, seconds, description):
        end = time.monotonic() + seconds
        while time.monotonic() < end:
            self.alive()
            value = predicate()
            if value:
                return value
            time.sleep(0.1)
        raise RuntimeError('Timed out waiting for ' + description)

    def control(self, **changes):
        self.counter += 1
        command = dict(counter=self.counter, enabled=1, bot_x=0, bot_y=0,
                       aim_x=1, aim_y=0, fire=0, grant=0, screenshot=0, kill_ally=0, restart=0, qa_aim_enemy=0, qa_focus=1,
                       qa_mutate_movement=int(self.args.mutation_movement))
        command.update(changes)
        temporary = self.runtime / 'coop-control.pending'
        temporary.write_text(''.join('{}={}\n'.format(k, v) for k, v in command.items()))
        os.replace(str(temporary), str(self.runtime / 'coop-control.txt'))
        with (self.output / 'commands.jsonl').open('a') as stream:
            stream.write(json.dumps(command) + '\n')
        return self.counter

    def rows(self, counter=None):
        rows = read_jsonl(self.telemetry)
        return [r for r in rows if counter is None or r.get('command') == counter]

    def phase(self, name, seconds=2.0, **changes):
        counter = self.control(**changes)
        self.wait(lambda: self.rows(counter), 8, 'command acknowledgement ' + name)
        end = time.monotonic() + seconds
        while time.monotonic() < end:
            self.alive()
            time.sleep(0.1)
        rows = self.rows(counter)
        self.check(name + ' telemetry', len(rows) >= 5, '{} samples for command {}'.format(len(rows), counter))
        self.check(name + ' game progress', rows[-1].get('time_ms', 0) - rows[0].get('time_ms', 0) >= seconds * 700
                   and rows[-1].get('steps', 0) > rows[0].get('steps', 0),
                   'fresh telemetry spans the requested phase with advancing native steps')
        self.check(name + ' active mode', all(r.get('enabled') == 1 and r.get('alive') == 1
                   and r.get('player_alive') == 1 and r.get('ally_count') == 1 for r in rows),
                   'enabled local control with one live Ally and live player throughout phase')
        return rows

    def verify_movement(self, label, axis, sign, changes):
        for attempt in range(1, 4):
            trial_name = label if attempt == 1 else '{} retry {}'.format(label, attempt)
            rows = self.phase(trial_name, seconds=1.2, **changes)
            if self.args.mutation_movement:
                self.check(trial_name + ' movement mutation accepted',
                           all(r.get('qa_mutate_movement') == 1 for r in rows),
                           'plugin confirms velocity-write mutation while enabled remains one')
            motion = directional_motion(rows, axis, sign)
            # These fail immediately; retries never hide missing/reversed P2 input.
            self.check(trial_name + ' sufficient observed deltas',
                       motion['accepted_deltas'] >= 4 and
                       motion['accepted_deltas'] >= 0.7 * (len(rows) - 1), motion)
            self.check(trial_name + ' commanded direction', motion['signed_displacement'] >= 25, motion)
            self.check(trial_name + ' orthogonal drift',
                       motion['orthogonal_path'] <= 0.2 * motion['signed_displacement'] + 5, motion)
            independent = motion['signed_displacement'] > 2 * motion['player_path'] + 10
            if independent:
                self.check(trial_name + ' independent player motion', True, motion)
                return rows
            trial = dict(phase=label, attempt=attempt, command=rows[0].get('command'),
                         first_time_ms=rows[0]['time_ms'], last_time_ms=rows[-1]['time_ms'],
                         reason='Relative independence inconclusive while player moves', motion=motion)
            self.report['inconclusive_trials'].append(trial)
            print('INCONCLUSIVE {} attempt {}: {}'.format(label, attempt, motion), flush=True)
            if attempt == 3:
                self.check(label + ' independent player motion after three trials', False, motion)
            # Release P2 and observe a fresh quiet P1 window; no world/player edits.
            counter = self.control()
            quiet_start = self.wait(lambda: next(iter(reversed(self.rows(counter))), None),
                                    8, 'released P2 control acknowledgement before movement retry')
            quiet = self.wait(lambda: settled_player_window(self.rows(), quiet_start['player_id'],
                              quiet_start['time_ms']), 20,
                              'fresh P1 path <= 10 pixels over one second before movement retry')
            trial['retry_setup'] = quiet
            self.check(label + ' quiet player before retry {}'.format(attempt + 1), True, quiet)

    def combat(self):
        self.report['combat_result'] = 'RUNNING'
        baseline = self.rows()[-1]
        fields = ('enemy_hull_damage_total', 'enemy_destroyed_after_collision_total',
                  'projectile_created_total')
        self.check('native combat counters present', all(name in baseline for name in fields),
                   {name: baseline.get(name) for name in fields})
        counter = self.control(fire=1, qa_aim_enemy=1)
        def damage_proof():
            rows = self.rows(counter)
            if not rows:
                return None
            if any(r.get('player_alive') != 1 or r.get('alive') != 1 or
                   r.get('player_count') != 1 or r.get('ally_count') != 1 or
                   r.get('enabled') != 1 or r.get('qa_aim_enemy') != 1 for r in rows):
                raise RuntimeError('Player/Ally survival, controlled mode, or requested enemy-aim mode lost during combat')
            if any(name not in rows[-1] for name in fields):
                raise RuntimeError('Native combat counter telemetry missing')
            latest = rows[-1]
            damage = latest['enemy_hull_damage_total'] - baseline['enemy_hull_damage_total']
            destroyed = latest['enemy_destroyed_after_collision_total'] - baseline['enemy_destroyed_after_collision_total']
            if len(rows) < 5 or latest.get('steps', 0) < rows[0].get('steps', 0) + 3:
                return None
            if damage <= 0 and destroyed <= 0:
                return None
            return dict(enemy_hull_damage=damage, enemies_destroyed_after_collision=destroyed,
                        actual_projectiles=latest['projectile_created_total'] - baseline['projectile_created_total'],
                        samples=len(rows), native_steps=latest['steps'] - rows[0]['steps'],
                        observed_ms=latest['time_ms'] - rows[0]['time_ms'],
                        target_enemy_id=latest.get('qa_target_enemy_id'),
                        target_enemy_hull=latest.get('qa_target_enemy_hull'),
                        stimulus='aim at existing enemies and fire; enemy health/spawn unchanged')
        proof = self.wait(damage_proof, 15, 'actual native Ally projectile enemy hull damage or destruction')
        self.check('Ally projectiles damage a real enemy',
                   proof['enemy_hull_damage'] > 0 or proof['enemies_destroyed_after_collision'] > 0, proof)
        self.report['combat_result'] = 'PASS'
        counter = self.control()  # Restore ordinary aim, release fire, clear all QA stimuli.
        self.wait(lambda: self.rows(counter), 8, 'combat aim/fire reset acknowledgement')

    def lifecycle(self):
        counter = self.control()
        baseline = self.wait(lambda: next(iter(reversed(self.rows(counter))), None), 8,
                             'idle control acknowledgement before pause')
        original_id = baseline['ally_id']
        self.ui('press', 'ESCAPE')
        paused = self.rows()[-1]
        pause_steps = paused['steps']
        end = time.monotonic() + 0.8
        while time.monotonic() < end:
            self.alive()
            time.sleep(0.1)
        pause_status = self.ui()
        game_window = game_windows(pause_status)
        self.check('pause keeps owned process responsive',
                   bool(game_window) and all(not w.get('hung') for w in game_window), pause_status)
        self.check('pause stops native Ally steps', self.rows()[-1]['steps'] == pause_steps,
                   {'before': pause_steps, 'after': self.rows()[-1]['steps']})
        self.ui('press', 'ESCAPE')
        resumed = self.wait(lambda: next((r for r in reversed(self.rows())
                            if r.get('time_ms', 0) > paused['time_ms'] and
                            r.get('steps', 0) >= pause_steps + 3 and r.get('alive') == 1), None),
                            8, 'native Ally steps after resume')
        self.check('resume preserves controlled Ally', resumed.get('ally_id') == original_id,
                   {'before': original_id, 'after': resumed.get('ally_id')})

        counter = self.control(kill_ally=1)
        dead = self.wait(lambda: next((r for r in reversed(self.rows(counter))
                         if r.get('alive') == 0 and r.get('ally_count') == 0), None),
                         12, 'natural Ally destruction after explicit QA hull-zero stimulus')
        self.check('Ally destruction preserves player', dead.get('player_alive') == 1 and
                   dead.get('player_count') == 1, dead)
        counter = self.control(grant=1)
        reborn = self.wait(lambda: next((r for r in reversed(self.rows(counter))
                           if r.get('alive') == 1 and r.get('ally_count') == 1), None),
                           12, 'Ally redeployment after destruction')
        self.check('redeployment selects new Ally', reborn.get('ally_id', -1) >= 100000 and
                   reborn['ally_id'] != original_id,
                   {'old_id': original_id, 'new_id': reborn.get('ally_id')})
        self.recovery_controls('redeployed')

        before = self.rows()[-1]
        generation = before.get('scene_generation')
        self.check('scene generation telemetry exists', isinstance(generation, int), generation)
        # Nova's normal quit/restart flow starts from its pause menu. Pausing
        # deactivates gameplay objects before restartGame clears shared grids.
        self.control()
        self.ui('press', 'ESCAPE')
        restart_pause_steps = self.rows()[-1]['steps']
        end = time.monotonic() + 0.8
        while time.monotonic() < end:
            self.alive()
            time.sleep(0.1)
        status = self.ui()
        self.check('restart starts from responsive pause menu',
                   bool(game_windows(status)) and all(not w.get('hung') for w in game_windows(status))
                   and self.rows()[-1]['steps'] == restart_pause_steps, status)
        events = self.runtime / 'coop-events.log'
        previous_events = len(events.read_text(errors='replace')) if events.exists() else 0
        self.control(restart=1)
        self.wait(lambda: events.exists() and 'QA restartGame requested' in
                  events.read_text(errors='replace')[previous_events:], 8, 'game restart script command consumption')
        self.control()  # Explicitly clear the one-shot stimulus after it is consumed.
        def new_player():
            return next((r for r in reversed(self.rows()) if r.get('player_alive') == 1 and
                         r.get('scene_generation', -1) > generation), None)
        for _ in range(self.args.menu_attempts):
            if new_player():
                break
            self.ui('press', self.args.start_keys)
            time.sleep(1.5)
        restarted = self.wait(new_player, 12, 'new Player Create generation after restart/menu navigation')
        self.check('restart creates new player scene', restarted['scene_generation'] > generation and
                   restarted.get('player_count') == 1,
                   {'before_generation': generation, 'after_generation': restarted['scene_generation'],
                    'player_id': restarted.get('player_id')})
        settled = self.wait(lambda: settled_player_window(self.rows(), restarted['player_id'],
                            restarted['time_ms']), 40, 'natural intro settlement after restart: one fresh second with path <= 10 pixels')
        self.check('restarted player settles naturally', True, settled)
        counter = self.control(grant=1)
        restored = self.wait(lambda: next((r for r in reversed(self.rows(counter))
                           if r.get('scene_generation', -1) > generation and r.get('alive') == 1
                           and r.get('ally_count') == 1 and r.get('player_alive') == 1), None),
                           12, 'controlled Ally in restarted scene')
        self.check('restart restores one controlled Ally', restored.get('player_count') == 1 and
                   restored.get('ally_id', -1) >= 100000, restored)
        self.recovery_controls('restarted')
        self.control()
        self.report['lifecycle_result'] = 'PASS'

    def recovery_controls(self, label):
        self.verify_movement(label + ' movement', 'x', 1, dict(bot_x=1))
        rows = self.phase(label + ' fire', seconds=2, fire=1)
        created = rows[-1]['projectile_created_total'] - rows[0]['projectile_created_total']
        self.check(label + ' actual projectile creation', created >= 1, created)

    def run(self):
        original = fingerprint(self.args.original_saves)
        baseline = ROOT / 'local/original-saves-before.json'
        if baseline.exists():
            self.check('original save baseline', original == json.loads(baseline.read_text()),
                       '{} files match preserved baseline'.format(len(original)))
        self.original = original
        executable = self.runtime / 'NovaDrift.exe'
        sandbox = self.runtime / 'novacoop_sandbox.dll'
        launcher = ROOT / 'local/build/novacoop_launch.exe'
        for path in (executable, sandbox, launcher, self.runtime / 'mods/aurie/NovaDriftCoop.dll'):
            self.check('required artifact ' + path.name, path.is_file(), str(path))
        self.executable_hash = hashlib.sha256(executable.read_bytes()).hexdigest()
        self.report['executable_sha256'] = self.executable_hash
        self.report['artifact_sha256'] = {
            str(path.relative_to(ROOT)): hashlib.sha256(path.read_bytes()).hexdigest()
            for path in (executable, sandbox, launcher,
                         self.runtime / 'mods/aurie/NovaDriftCoop.dll',
                         self.runtime / 'mods/aurie/YYToolkit.dll',
                         self.runtime / 'mods/AurieCore.dll')}
        # Refuse concurrent game sessions; never terminate a process discovered this way.
        query = subprocess.run([POWERSHELL, '-NoProfile', '-Command',
                                "@(Get-Process -Name NovaDrift -ErrorAction SilentlyContinue).Count"],
                               stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, timeout=15)
        self.check('exclusive runtime', query.returncode == 0 and query.stdout.strip() == '0',
                   'existing NovaDrift processes: ' + query.stdout.strip())
        for name in ('coop-events.log', 'coop-telemetry.jsonl', 'coop-control.txt', 'probe.log', 'runner.log', 'aurie.log'):
            path = self.runtime / name
            if path.exists():
                path.rename(self.output / ('previous-' + name))
        archived = set()
        for label, directory in self.screenshot_roots:
            for path in directory.rglob('qa-screen-*.png'):
                if path.resolve() in archived:
                    continue
                archived.add(path.resolve())
                previous = self.output / 'previous-screens' / label / path.relative_to(directory)
                previous.parent.mkdir(parents=True, exist_ok=True)
                path.rename(previous)
        self.control()
        arguments = [str(launcher), windows(executable), windows(sandbox), windows(self.runtime), str(self.args.seconds)]
        if self.args.debug:
            arguments.append('debug')
        if self.args.no_audio:
            arguments.append('silent')
        self.report['launcher_arguments'] = arguments[1:]
        with self.launch_log.open('w') as log:
            self.process = subprocess.Popen(arguments, stdout=log, stderr=subprocess.STDOUT, cwd=ROOT)
            self.started_at = time.monotonic()
        def started():
            match = re.search(r'STARTED pid=(\d+)', self.launch_log.read_text(errors='replace'))
            return int(match.group(1)) if match else None
        self.pid = self.wait(started, 30, 'owned game PID')
        self.report['pid'] = self.pid
        self.wait(lambda: game_windows(self.ui()), 30, 'visible owned Nova Drift game window (console excluded)')
        self.wait(lambda: (self.runtime / 'coop-events.log').exists() and
                  'hook NovaCoop.PlayerStep' in (self.runtime / 'coop-events.log').read_text(errors='replace'),
                  45, 'co-op module hooks')
        self.check('module loaded', True, 'co-op hook initialization recorded')
        for _ in range(self.args.menu_attempts):
            rows = self.rows()
            if rows and rows[-1].get('player_alive') == 1:
                break
            self.ui('press', self.args.start_keys)
            time.sleep(1.5)
        player = self.wait(lambda: next((r for r in reversed(self.rows()) if r.get('player_alive') == 1), None),
                           12, 'live player after target-window menu input')
        self.check('player entered gameplay', player.get('player_id', -1) >= 100000, player)
        settlement_started = time.monotonic()
        settled = self.wait(lambda: settled_player_window(self.rows(), player['player_id'], player['time_ms']),
                            40, 'natural player intro settlement: one fresh second with path <= 10 pixels')
        settled['wait_seconds'] = round(time.monotonic() - settlement_started, 3)
        self.check('player intro settled naturally', True, settled)
        self.control(grant=1)
        spawned = self.wait(lambda: next((r for r in reversed(self.rows(self.counter)) if r.get('alive') == 1), None),
                            12, 'native Ally deployment')
        self.check('one original player', spawned.get('player_count') == 1, spawned.get('player_count'))
        self.check('one controlled Ally', spawned.get('ally_count') == 1 and spawned.get('ally_id', -1) >= 100000,
                   {'ally_count': spawned.get('ally_count'), 'ally_id': spawned.get('ally_id')})
        for phase, axis, sign, changes in [
                ('right', 'x', 1, dict(bot_x=1)),
                ('left', 'x', -1, dict(bot_x=-1)),
                ('up', 'y', -1, dict(bot_y=-1))]:
            self.verify_movement(phase, axis, sign, changes)
        idle = self.phase('released fire', seconds=2)
        self.check('actual projectile counter present', all('projectile_created_total' in r for r in idle),
                   'actual object creation counter is required; emitter-call counts are insufficient')
        idle_delta = idle[-1]['projectile_created_total'] - idle[0]['projectile_created_total']
        self.check('released trigger creates no Ally projectiles', idle_delta == 0, idle_delta)
        firing = self.phase('held fire', seconds=3, fire=1)
        fire_delta = firing[-1]['projectile_created_total'] - firing[0]['projectile_created_total']
        self.check('held trigger creates Ally projectiles', fire_delta >= 2, fire_delta)
        released = self.phase('released after firing', seconds=2)
        self.check('release stops Ally projectiles',
                   released[-1]['projectile_created_total'] == released[0]['projectile_created_total'],
                   [released[0]['projectile_created_total'], released[-1]['projectile_created_total']])
        if self.args.combat or self.args.lifecycle:
            self.combat()
        counter = self.control(screenshot=1)
        def captured():
            matches = {p.resolve() for label, directory in self.screenshot_roots
                       for p in directory.rglob('qa-screen-{}.png'.format(counter))
                       if p.is_file() and p.stat().st_size > 100}
            return next(iter(matches)) if len(matches) == 1 else None
        screenshot = self.wait(captured, 8, 'one new game-rendered screenshot beneath isolated profile/runtime')
        data = screenshot.read_bytes()
        png = data[:8] == b'\x89PNG\r\n\x1a\n'
        dimensions = struct.unpack('>II', data[16:24]) if png and len(data) >= 24 else (0, 0)
        self.check('game screenshot', png and dimensions[0] >= 300 and dimensions[1] >= 200,
                   {'path': str(screenshot), 'dimensions': dimensions})
        captured_artifact = self.output / 'game-screenshot.png'
        captured_artifact.write_bytes(data)
        self.report['screenshot_source'] = str(screenshot)
        self.report['screenshot'] = str(captured_artifact)
        self.check('both combatants alive', released[-1].get('alive') == 1 and released[-1].get('player_alive') == 1,
                   {'ally_hull': released[-1].get('hull'), 'player_hull': released[-1].get('player_hull')})
        self.report['basic_result'] = 'PASS'
        if self.args.lifecycle:
            self.report['lifecycle_result'] = 'RUNNING'
            self.lifecycle()
        self.report['status'] = 'PASS'

    def capture_failure(self):
        if not self.pid or not self.process or self.process.poll() is not None:
            return
        counter = self.control(screenshot=1)
        def captured():
            matches = {p.resolve() for _, directory in self.screenshot_roots
                       for p in directory.rglob('qa-screen-{}.png'.format(counter))
                       if p.is_file() and p.stat().st_size > 100}
            return next(iter(matches)) if len(matches) == 1 else None
        screenshot = self.wait(captured, 4, 'game-owned failure screenshot')
        artifact = self.output / 'failure-screenshot.png'
        artifact.write_bytes(screenshot.read_bytes())
        self.report['failure_screenshot'] = str(artifact)

    def finish(self):
        if self.process and self.process.poll() is None:
            # The launcher owns and bounds the target lifetime, including debugger runs.
            if self.pid:
                try:
                    self.ui('close')
                    self.report['shutdown_request'] = 'WM_CLOSE sent to identity-verified owned game window'
                    self.process.wait(timeout=10)
                except Exception as error:
                    self.report['shutdown_request'] = str(error)
            print('Waiting for owned launcher deadline/exit...', flush=True)
            remaining = max(0, self.args.seconds - (time.monotonic() - self.started_at))
            try:
                self.process.wait(timeout=remaining + 35)
            except subprocess.TimeoutExpired:
                self.report['status'] = 'FAIL'
                self.report['error'] = 'Launcher exceeded its process-ownership deadline; no unowned process was killed'
        if self.process:
            self.report['launcher_exit'] = self.process.poll()
            log = self.launch_log.read_text(errors='replace')
            expected_timeout = self.process.returncode == 3 and 'stopping owned test process' in log
            clean_exit = self.process.returncode == 0
            if not (expected_timeout or clean_exit) or 'EXCEPTION code=' in log:
                self.report['status'] = 'FAIL'
                self.report['checks'].append({'name': 'owned process lifecycle', 'pass': False,
                                             'details': 'Crash or unexpected launcher exit; inspect launcher.log'})
        if hasattr(self, 'original'):
            unchanged = fingerprint(self.args.original_saves) == self.original
            self.report['checks'].append({'name': 'original saves unchanged', 'pass': unchanged})
            if not unchanged:
                self.report['status'] = 'FAIL'
        for name in ('coop-events.log', 'coop-telemetry.jsonl', 'runner.log', 'aurie.log'):
            path = self.runtime / name
            if path.exists():
                (self.output / name).write_bytes(path.read_bytes())
        (self.output / 'report.json').write_text(json.dumps(self.report, indent=2) + '\n')
        print('RESULT: ' + self.report['status'] + ' ' + str(self.output / 'report.json'), flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime', type=Path, default=ROOT / 'local/runtime')
    parser.add_argument('--profile-root', type=Path, default=ROOT / 'local/test-profile',
                        help='Isolated profile directory inside this project, outside the game working directory')
    parser.add_argument('--original-saves', type=Path, default=ORIGINAL_SAVES)
    parser.add_argument('--seconds', type=int, default=140)
    parser.add_argument('--menu-attempts', type=int, default=8)
    parser.add_argument('--start-keys', default='ENTER')
    parser.add_argument('--debug', action='store_true')
    parser.add_argument('--no-audio', action='store_true', help='Diagnostic run with runner audio disabled')
    parser.add_argument('--lifecycle', action='store_true',
                        help='Also test real-enemy combat, pause/resume, Ally destruction/redeployment, and game restart')
    parser.add_argument('--combat', action='store_true',
                        help='Require native Ally projectile damage/destruction against existing enemies (15-second limit)')
    parser.add_argument('--mutation-movement', action='store_true',
                        help='Request skipped manual velocity writes; directional checks should fail')
    args = parser.parse_args()
    if args.seconds < 75 or args.seconds > 600:
        parser.error('--seconds must be between 75 and 600')
    try:
        harness = Harness(args)
    except ValueError as error:
        parser.error(str(error))
    try:
        harness.run()
    except Exception as error:
        harness.report['status'] = 'FAIL'
        if harness.report['basic_result'] == 'NOT_RUN':
            harness.report['basic_result'] = 'FAIL'
        if harness.report.get('combat_result') == 'RUNNING':
            harness.report['combat_result'] = 'FAIL'
        if harness.report.get('lifecycle_result') == 'RUNNING':
            harness.report['lifecycle_result'] = 'FAIL'
        harness.report['error'] = str(error)
        print('FAIL: ' + str(error), file=sys.stderr, flush=True)
        try:
            harness.capture_failure()
        except Exception as capture_error:
            harness.report['failure_capture_error'] = str(capture_error)
    finally:
        harness.finish()
    return 0 if harness.report['status'] == 'PASS' else 1


if __name__ == '__main__':
    sys.exit(main())
