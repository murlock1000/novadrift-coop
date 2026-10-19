"""Locations for the isolated Windows installation; no user-specific paths."""
from functools import lru_cache
import json
import os
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
LOCATION = ROOT / 'local/active-runtime.json'
POWERSHELL = Path('/mnt/c/Windows/System32/WindowsPowerShell/v1.0/powershell.exe')


def linux_path(value):
    if re.match(r'^[A-Za-z]:[\\/]',str(value)) or str(value).startswith('\\\\'):
        return Path(subprocess.check_output(['wslpath','-u',str(value)],text=True).strip())
    return Path(value)


@lru_cache(maxsize=None)
def windows_folder(name):
    if name not in ('UserProfile','LocalApplicationData'):
        raise ValueError('Unsupported known folder')
    if POWERSHELL.is_file():
        value=subprocess.check_output([str(POWERSHELL),'-NoProfile','-Command',
            '[Console]::Write([Environment]::GetFolderPath("'+name+'"))'],text=True).strip()
        if not value:raise RuntimeError('Windows known folder is unavailable: '+name)
        return linux_path(value)
    # Source-only tools and tests can be inspected on Linux; launching/building
    # the native game still requires Windows with WSL interop.
    return Path.home() if name=='UserProfile' else Path.home()/'.local/share'


DEFAULT_BASE = linux_path(os.environ['NOVACOOP_INSTALL']) if os.environ.get('NOVACOOP_INSTALL') else windows_folder('UserProfile')/'Games/NovaDriftCoop'
ORIGINAL_SAVES = windows_folder('LocalApplicationData')/'NovaDrift'


def active_runtime():
    override = os.environ.get('NOVACOOP_TEST_RUNTIME')
    if override:return linux_path(override)
    if LOCATION.exists():return Path(json.loads(LOCATION.read_text())['runtime'])
    return ROOT/'local/fullship-test/runtime'


def write_project_shortcuts(base):
    """Keep tracked launchers portable; installation-specific data stays local/."""
    base=Path(base)
    native=subprocess.check_output(['wslpath','-w',str(base)],text=True).strip()
    private=ROOT/'local/installation.env.cmd';private.parent.mkdir(parents=True,exist_ok=True)
    private.write_bytes(('if not defined NOVACOOP_INSTALL set "NOVACOOP_INSTALL='+native+'"\r\n').encode())
    for name,target in [('PlayShips.cmd','PlayCoop.cmd'),('PlayRemoteCoop.cmd','PlayRemoteCoop.cmd'),('ConfigureP2.cmd','ConfigureP2.cmd'),('ConfigureControls.cmd','ConfigureControls.cmd'),('ConfigureRemotePlay.cmd','ConfigureRemotePlay.cmd')]:
        content=('@echo off\r\nsetlocal\r\n'
            'if exist "%~dp0local\\installation.env.cmd" call "%~dp0local\\installation.env.cmd"\r\n'
            'if not defined NOVACOOP_INSTALL set "NOVACOOP_INSTALL=%USERPROFILE%\\Games\\NovaDriftCoop"\r\n'
            'if not exist "%NOVACOOP_INSTALL%\\'+target+'" (\r\n'
            '  echo Install the co-op copy first. See README.md.\r\n  pause\r\n  exit /b 2\r\n)\r\n'
            'call "%NOVACOOP_INSTALL%\\'+target+'" %*\r\n')
        (ROOT/name).write_bytes(content.encode())
