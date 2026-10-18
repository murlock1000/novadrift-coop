#!/usr/bin/env python3
"""Prepare an isolated Windows co-op copy from an owned, matching Steam install."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
from runtime_paths import ROOT, DEFAULT_BASE, ORIGINAL_SAVES, linux_path

SUPPORTED_EXE = '6555f9f49c9b44f2a38dafc0846dd8af0e7e1f1e100cba976db60967b7173e2d'

def digest(path):
    h=hashlib.sha256()
    with path.open('rb') as file:
        for block in iter(lambda:file.read(1024*1024),b''):h.update(block)
    return h.hexdigest()

def fingerprint(path):
    return {str(p.relative_to(path)):digest(p) for p in sorted(path.rglob('*')) if p.is_file()}

def run(tool, *args):
    subprocess.run([sys.executable,str(ROOT/'tools'/tool),*map(str,args)],check=True,cwd=ROOT)

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--game-dir',required=True,help='Windows or WSL path to your original Nova Drift install')
    parser.add_argument('--base',default=str(DEFAULT_BASE),help='Owned Windows-local installation destination')
    args=parser.parse_args()
    source=linux_path(args.game_dir).resolve();base=linux_path(args.base).resolve()
    for left,right in ((base,source),(source,base),(base,ROOT),(ROOT,base)):
        if left==right or right in left.parents:
            parser.error('Source, project and installation destination must be separate directories')
    if not (source/'NovaDrift.exe').is_file() or digest(source/'NovaDrift.exe')!=SUPPORTED_EXE:
        parser.error('Requires the supported pristine Nova Drift 1.2.38.4 executable; no files changed')
    if base.exists() and not (base/'.novadriftcoop-owned').is_file():
        parser.error('Destination exists without the ownership marker; choose an empty destination')
    # Staging must never replace a loaded executable/DLL, even on a later update.
    native=subprocess.check_output(['wslpath','-w',str(base)],text=True).strip()
    ps="@((Get-CimInstance Win32_Process) | Where-Object { $_.ExecutablePath -and $_.ExecutablePath.StartsWith("+"'"+native.replace("'","''")+"\\', [StringComparison]::OrdinalIgnoreCase) }).Count"
    count=int(subprocess.check_output(['powershell.exe','-NoProfile','-Command',ps],text=True).strip())
    if count:parser.error('Close the owned co-op game and helper before installing/updating')
    copied=ROOT/'game'
    if copied.exists():
        if fingerprint(copied)!=fingerprint(source):
            parser.error('Existing game/ differs from the supplied install; preserve/review it before setup')
    else:
        shutil.copytree(source,copied)
    private=ROOT/'local';private.mkdir(exist_ok=True)
    baseline=private/'original-saves-before.json';current=fingerprint(ORIGINAL_SAVES)
    if baseline.exists() and json.loads(baseline.read_text())!=current:
        parser.error('Original saves changed since the recorded baseline; review before updating')
    if not baseline.exists():baseline.write_text(json.dumps(current,indent=2)+'\n')
    (private/'source-game-manifest.json').write_text(json.dumps({'source':str(source),'files':fingerprint(copied)},indent=2)+'\n')
    run('fetch_dependencies.py')
    run('build_native.py')
    run('build_ship_config.py')
    build=subprocess.check_output(['wslpath','-w',str(ROOT/'tools/build_ship.cmd')],text=True).strip()
    subprocess.run(['powershell.exe','-NoProfile','-Command',"& '"+build.replace("'","''")+"'; exit $LASTEXITCODE"],check=True)
    run('prepare_ships.py','--base',base)
    run('prepare_remoteplay.py','--stage')
    print('Ready. Open PlayCoop.cmd in',native)
    print('For Steam: install Spacewar (steam://install/480), then follow README.RemotePlay.md.')
    print('Steam settings and original game/saves were not changed.')

if __name__=='__main__':main()
