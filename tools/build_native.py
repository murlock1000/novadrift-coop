#!/usr/bin/env python3
"""Build the test bootstrap with an existing MSYS2 compiler; installs nothing."""
from pathlib import Path
import subprocess
ROOT=Path(__file__).resolve().parents[1]
COMPILER=Path('/mnt/c/msys64/ucrt64/bin/g++.exe')
def win(path):
    return subprocess.check_output(['wslpath','-w',str(path)],text=True).strip()
def build(source,target,*extra):
    command=[str(COMPILER),'-std=c++20','-O2','-static','-static-libgcc','-static-libstdc++','-municode',win(ROOT/source),*extra,'-o',win(ROOT/target)]
    subprocess.run(command,check=True,cwd=ROOT)
    (ROOT/target).chmod(0o755)
out=ROOT/'local/build';out.mkdir(parents=True,exist_ok=True)
build('src/sandbox.cpp','local/build/novacoop_sandbox.dll','-shared')
build('src/launch.cpp','local/build/novacoop_launch.exe','-ldbghelp')
print('Built sandbox and owned-process launcher in local/build')
