#!/usr/bin/env python3
"""Stage an owned Windows-local copy; preserve Steam, saves, and Ally fallback."""
from pathlib import Path
import argparse
import hashlib
import json
import shutil
import subprocess
from runtime_paths import ROOT, LOCATION, DEFAULT_BASE, ORIGINAL_SAVES, write_project_shortcuts

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--base', type=Path, default=DEFAULT_BASE)
parser.add_argument('--no-activate', action='store_true', help='Stage a separate QA copy without changing project shortcuts or active runtime')
args = parser.parse_args()
base = args.base.resolve()
runtime = base / 'runtime'
marker = base / '.novadriftcoop-owned'
legacy = ROOT / 'local/fullship-test'

def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def win(path):
    return subprocess.check_output(['wslpath', '-w', str(path)], text=True).strip()

if base.exists() and not marker.exists() and base != legacy:
    raise RuntimeError(f'Refusing to overwrite an unowned folder: {base}')
base.mkdir(parents=True, exist_ok=True)
marker.write_text('Nova Drift co-op isolated installation; original game lives in Steam.\n')
source = ORIGINAL_SAVES
fingerprint = {str(p.relative_to(source)): digest(p) for p in source.rglob('*') if p.is_file()}
if fingerprint != json.loads((ROOT/'local/original-saves-before.json').read_text()):
    raise RuntimeError('Original save baseline changed')
if not runtime.exists():
    shutil.copytree(ROOT/'game', runtime)
for directory in ['Local', 'Roaming', 'Documents', 'Temp']:
    (base/'test-profile'/directory).mkdir(parents=True, exist_ok=True)
profile = base/'test-profile/Local/NovaDrift'
if not profile.exists():
    shutil.copytree(source,profile) if source.exists() else profile.mkdir(parents=True)
files = {
    'PlayCoop.cmd': ROOT/'tools/PlayShips.cmd',
    'novacoop_sandbox.dll': ROOT/'local/build/novacoop_sandbox.dll',
    'novacoop_launch.exe': ROOT/'local/build/novacoop_launch.exe',
    'mods/AurieCore.dll': ROOT/'local/deps-research/binaries/v2.0.2/AurieCore.dll',
    'mods/aurie/YYToolkit.dll': ROOT/'local/deps-research/binaries/v5.0.0c/YYToolkit.dll',
    'mods/aurie/NovaDriftShips.dll': ROOT/'local/build/NovaDriftShips.dll',
}
config_tool = ROOT/'local/build/NovaDriftP2Controls.exe'
if config_tool.exists():
    files['NovaDriftP2Controls.exe'] = config_tool
for target, src in files.items():
    path = runtime/target
    path.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(src, path)
if config_tool.exists() and not (runtime/'p2-controls.ini').exists():
    subprocess.run([str(config_tool), '--write-defaults', win(runtime/'p2-controls.ini')], check=True)
# Aurie embeds an absolute loader path. Always patch a fresh pristine EXE.
exe = runtime/'NovaDrift.exe'
shutil.copy2(ROOT/'game/NovaDrift.exe', exe)
subprocess.run([str(ROOT/'local/deps-research/binaries/v2.0.2/AuriePatcher.exe'),
                win(exe), win(runtime/'mods/AurieCore.dll'), 'install'], check=True)
(base/'PlayCoop.cmd').write_bytes(b'@echo off\r\ncall "%~dp0runtime\\PlayCoop.cmd" %*\r\n')
(base/'ConfigureControls.cmd').write_bytes(b'@echo off\r\n"%~dp0runtime\\NovaDriftP2Controls.exe"\r\n')
(base/'ConfigureP2.cmd').write_bytes(b'@echo off\r\n"%~dp0runtime\\NovaDriftP2Controls.exe"\r\n')
for name in ('README.md','README.RemotePlay.md','dependencies.json'):
    shutil.copy2(ROOT/name,base/name)
shutil.copytree(ROOT/'docs',base/'docs',dirs_exist_ok=True)
if not args.no_activate:
    write_project_shortcuts(base)
    LOCATION.write_text(json.dumps({'runtime': str(runtime), 'windows': win(runtime),
                                   'profile': str(base/'test-profile'),
                                   'exe_sha256': digest(exe)}, indent=2)+'\n')
print(f'Windows-local runtime: {win(runtime)}')
