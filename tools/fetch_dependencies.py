#!/usr/bin/env python3
"""Fetch pinned upstream dependencies into ignored local/. Never execute downloads."""
import argparse
import hashlib
import json
from pathlib import Path
import urllib.request

ROOT = Path(__file__).resolve().parents[1]

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--verify-only', action='store_true')
    args = parser.parse_args()
    entries = json.loads((ROOT/'dependencies.json').read_text())['files']
    for entry in entries:
        target = ROOT/entry['path']
        target.resolve().relative_to((ROOT/'local').resolve())
        if target.is_file():
            data = target.read_bytes()
        elif args.verify_only:
            raise RuntimeError('Missing dependency: '+entry['path'])
        else:
            request = urllib.request.Request(entry['url'], headers={'User-Agent':'NovaDriftCoop-source-setup'})
            with urllib.request.urlopen(request, timeout=60) as response:
                data = response.read()
        if hashlib.sha256(data).hexdigest() != entry['sha256']:
            raise RuntimeError('Dependency hash mismatch: '+entry['path'])
        if not target.is_file():
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(data)
        if target.suffix.lower() == '.exe':
            target.chmod(0o755)
    print('Verified', len(entries), 'pinned upstream dependency files')

if __name__ == '__main__':
    main()
