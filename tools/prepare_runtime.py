#!/usr/bin/env python3
"""Prepare only local/runtime; preserve the pristine copied install and user save."""
from pathlib import Path
from runtime_paths import ORIGINAL_SAVES
import hashlib
import json
import shutil
import subprocess

ROOT=Path(__file__).resolve().parents[1]
runtime=ROOT/'local/runtime'
if not runtime.exists():
    shutil.copytree(ROOT/'game',runtime)
for name in ['Local','Roaming','Documents','Temp']:
    (ROOT/'local/test-profile'/name).mkdir(parents=True,exist_ok=True)
save=ORIGINAL_SAVES
def fingerprint(directory):
    return {str(p.relative_to(directory)):hashlib.sha256(p.read_bytes()).hexdigest()
            for p in sorted(directory.rglob('*')) if p.is_file()}
before=fingerprint(save)
manifest=ROOT/'local/original-saves-before.json'
if manifest.exists():
    if json.loads(manifest.read_text())!=before:
        raise RuntimeError('Original save changed since runtime baseline; do not start a test')
else:
    manifest.write_text(json.dumps(before,indent=2)+'\n')
profile_save=ROOT/'local/test-profile/Local/NovaDrift'
if not profile_save.exists():
    shutil.copytree(save,profile_save)
for name in ['aurie','native']:
    (runtime/'mods'/name).mkdir(parents=True,exist_ok=True)
files={
 'novacoop_sandbox.dll':ROOT/'local/build/novacoop_sandbox.dll',
 'novacoop_launch.exe':ROOT/'local/build/novacoop_launch.exe',
 'PlayCoop.cmd':ROOT/'tools/PlayCoop.cmd',
 'mods/AurieCore.dll':ROOT/'local/deps-research/binaries/v2.0.2/AurieCore.dll',
 'mods/aurie/YYToolkit.dll':ROOT/'local/deps-research/binaries/v5.0.0c/YYToolkit.dll',
 'mods/aurie/NovaDriftCoop.dll':ROOT/'local/build/NovaDriftCoop.dll',
}
for dest,source in files.items():
    shutil.copy2(source,runtime/dest)
probe=runtime/'mods/aurie/NovaDriftProbe.dll'
if probe.exists():
    probe.rename(probe.with_suffix('.dll.disabled'))
exe=runtime/'NovaDrift.exe'
if hashlib.sha256(exe.read_bytes()).hexdigest() == '6555f9f49c9b44f2a38dafc0846dd8af0e7e1f1e100cba976db60967b7173e2d':
    def windows(path):
        return subprocess.check_output(['wslpath','-w',str(path)],text=True).strip()
    subprocess.run([str(ROOT/'local/deps-research/binaries/v2.0.2/AuriePatcher.exe'),
                    windows(exe), windows(runtime/'mods/AurieCore.dll'), 'install'],check=True)
print('Prepared local/runtime; original save baseline:',len(before),'files')
