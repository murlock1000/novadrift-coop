#!/usr/bin/env python3
"""Build the P2 control editor with the existing Windows MSYS2 compiler."""
from pathlib import Path
import subprocess
ROOT=Path(__file__).resolve().parents[1]
COMPILER=Path('/mnt/c/msys64/ucrt64/bin/g++.exe')
def win(path):
    return subprocess.check_output(['wslpath','-w',str(path)],text=True).strip()
out=ROOT/'local/build/NovaDriftP2Controls.exe'
out.parent.mkdir(parents=True,exist_ok=True)
subprocess.run([str(COMPILER),'-std=c++20','-O2','-Wall','-Wextra','-Werror','-static',
    '-static-libgcc','-static-libstdc++','-municode','-mwindows',win(ROOT/'src/ship_config_tool.cpp'),
    '-luser32','-lgdi32','-lshell32','-o',win(out)],check=True,cwd=ROOT)
out.chmod(0o755)
print(out)
