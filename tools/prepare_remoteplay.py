#!/usr/bin/env python3
"""Build/review the isolated Steam Remote Play wrapper; never write Steam settings.

Default: project-local build, safe own-window self-test and review manifest only.
--stage: copy the helper/docs/shortcut into the already-owned co-op installation.
Neither mode launches Steam or a game, sends an invite, or changes a clipboard.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
from runtime_paths import ROOT, active_runtime, write_project_shortcuts

COMPILER = Path('/mnt/c/msys64/ucrt64/bin/g++.exe')

def win(path):
    return subprocess.check_output(['wslpath', '-w', str(path)], text=True).strip()

def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--runtime', type=Path, default=active_runtime())
    p.add_argument('--stage', action='store_true', help='Copy only to the marked, owned co-op folder; never edits Steam')
    p.add_argument('--skip-build', action='store_true')
    p.add_argument('--skip-self-test', action='store_true')
    args=p.parse_args()
    runtime=args.runtime.resolve()
    if not (runtime.parent/'.novadriftcoop-owned').is_file():
        p.error('Runtime parent has no .novadriftcoop-owned marker')
    for name in ('NovaDrift.exe','novacoop_launch.exe','novacoop_sandbox.dll','mods/aurie/NovaDriftShips.dll'):
        if not (runtime/name).is_file():
            p.error(f'Missing isolated co-op component: {name}')
    out=ROOT/'local/build/SteamCoop.exe'
    out.parent.mkdir(parents=True, exist_ok=True)
    review=ROOT/'local/remoteplay-re/prepared'
    review.mkdir(parents=True, exist_ok=True)
    if not args.skip_build:
        subprocess.run([str(COMPILER),'-std=c++20','-O2','-Wall','-Wextra','-Werror','-static',
                        '-static-libgcc','-static-libstdc++','-municode','-mwindows',
                        win(ROOT/'src/steam_coop.cpp'),'-luser32','-lgdi32','-lshell32','-o',win(out)],check=True,cwd=ROOT)
        out.chmod(0o755)
    if not args.skip_self_test:
        subprocess.run([str(out),'--runtime',win(runtime),'--self-test',win(review)],check=True,cwd=ROOT)
    launch_option=f'"{win(runtime)}\\SteamCoop.exe" --host %command%'
    (review/'steam-launch-options.txt').write_text(launch_option+'\n')
    manifest={'status':'prepared; run-specific validation is recorded separately',
              'donor_app_id':480,'donor_name':'Spacewar',
              'runtime':str(runtime),'launch_option':launch_option,
              'helper_sha256':digest(out),'steam_settings_written':False,
              'game_launched':False,'invite_sent':False,'runtime_staged':args.stage,
              'restore':'Restore the previous Launch Options in Spacewar Properties; empty if previously empty.'}
    if args.stage:
        shutil.copy2(out,runtime/'SteamCoop.exe')
        shutil.copy2(ROOT/'README.RemotePlay.md',runtime.parent/'README.RemotePlay.md')
        shortcut=(
            b'@echo off\r\n'
            b'if not exist "%~dp0runtime\\SteamCoop.exe" (\r\n'
            b'  echo SteamCoop.exe is missing from "%~dp0runtime".\r\n'
            b'  echo Restore the Steam launcher there before starting remote co-op.\r\n'
            b'  pause\r\n'
            b'  exit /b 2\r\n'
            b')\r\n'
            b'start "" "steam://rungameid/480"\r\n')
        (runtime.parent/'PlayRemoteCoop.cmd').write_bytes(shortcut)
        (runtime/'PlayRemoteCoop.cmd').write_bytes(
            b'@echo off\r\ncall "%~dp0..\\PlayRemoteCoop.cmd"\r\n')
        (runtime.parent/'ConfigureRemotePlay.cmd').write_bytes(b'@echo off\r\n"%~dp0runtime\\SteamCoop.exe" --setup\r\n')
        write_project_shortcuts(runtime.parent)
    (review/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    print(json.dumps(manifest,indent=2))

if __name__=='__main__':
    main()
