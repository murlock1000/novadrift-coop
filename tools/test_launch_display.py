#!/usr/bin/env python3
"""Test launcher options/profile updates with a native helper; never run the game."""
import hashlib
import json
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
COMPILER = Path('/mnt/c/msys64/ucrt64/bin/g++.exe')


def windows(path):
    return subprocess.check_output(['wslpath', '-w', str(path)], text=True).strip()


def main():
    if not COMPILER.is_file():
        raise RuntimeError('Expected existing MSYS2 compiler; no installation is attempted')
    parent = ROOT / 'local/fullscreen-tests'
    parent.mkdir(parents=True, exist_ok=True)
    out = Path(tempfile.mkdtemp(prefix='contract-', dir=parent))
    binary = out / 'launch_display_contract.exe'
    subprocess.run([str(COMPILER), '-std=c++20', '-O2', '-static', '-static-libgcc',
                    '-static-libstdc++', '-municode', windows(ROOT / 'tools/launch_display_contract.cpp'),
                    '-ldbghelp', '-o', windows(binary)], check=True, cwd=ROOT)
    binary.chmod(0o755)
    completed = subprocess.run([str(binary), windows(out / 'synthetic-profile')],
                               text=True, capture_output=True, timeout=30)
    (out / 'contract.log').write_text(completed.stdout + completed.stderr)
    print(completed.stdout, end='')
    if completed.stderr:
        print(completed.stderr, end='')
    count = re.search(r'PASS (\d+) launcher command/preference checks', completed.stdout)
    report = {'status': 'PASS' if completed.returncode == 0 and count else 'FAIL',
              'checks': int(count.group(1)) if count else 0, 'game_launched': False,
              'profile': str(out / 'synthetic-profile'), 'artifact': str(out),
              'source_sha256': hashlib.sha256((ROOT / 'src/launch.cpp').read_bytes()).hexdigest()}
    (out / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
    if report['status'] != 'PASS':
        raise RuntimeError('Launcher display contract failed: ' + str(out))
    print('Artifacts:', out)


if __name__ == '__main__':
    main()
