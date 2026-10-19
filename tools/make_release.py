#!/usr/bin/env python3
"""Package explicit mod artifacts; never include game files, saves or local logs."""
import argparse,hashlib,json,shutil,subprocess,zipfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
SHA='6555f9f49c9b44f2a38dafc0846dd8af0e7e1f1e100cba976db60967b7173e2d'
def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--version',required=True);p.add_argument('--replace',action='store_true');a=p.parse_args()
 if not all(c.isalnum() or c in '.-' for c in a.version):p.error('Invalid version')
 out=ROOT/'local/releases'/a.version;out.mkdir(parents=True,exist_ok=True)
 stage=out/'NovaDriftCoop';stage.mkdir(exist_ok=True)
 if any(stage.iterdir()):
  if not a.replace or stage.is_symlink() or not (stage/'release.json').is_file():p.error('Release staging folder already exists; use --replace for a generated package')
  shutil.rmtree(stage);stage.mkdir()
 for name in ('InstallCoop.cmd','InstallCoop.ps1'):shutil.copy2(ROOT/'tools/release'/name,stage/name)
 payload=stage/'payload';runtime=payload/'runtime';runtime.mkdir(parents=True)
 for name in ('NovaDriftShips.dll','novacoop_sandbox.dll','novacoop_launch.exe','NovaDriftP2Controls.exe','SteamCoop.exe'):
  target=runtime/'mods/aurie'/name if name=='NovaDriftShips.dll' else runtime/name
  target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(ROOT/'local/build'/name,target)
 shutil.copy2(ROOT/'tools/PlayShips.cmd',runtime/'PlayCoop.cmd')
 shortcuts={
 'PlayCoop.cmd':'@echo off\ncall "%~dp0runtime\\PlayCoop.cmd" %*\n',
 'ConfigureControls.cmd':'@echo off\n"%~dp0runtime\\NovaDriftP2Controls.exe"\n',
 'ConfigureP2.cmd':'@echo off\ncall "%~dp0ConfigureControls.cmd"\n',
 'ConfigureRemotePlay.cmd':'@echo off\n"%~dp0runtime\\SteamCoop.exe" --setup\n',
 'PlayRemoteCoop.cmd':'@echo off\nif not exist "%~dp0runtime\\SteamCoop.exe" (\n echo SteamCoop.exe is missing. Run the installer again.\n pause\n exit /b 2\n)\nstart "" "steam://rungameid/480"\n'}
 for name,text in shortcuts.items():(payload/name).write_bytes(text.replace('\n','\r\n').encode())
 for name in ('README.md','README.RemotePlay.md','LICENSE'):shutil.copy2(ROOT/name,payload/name)
 shutil.copytree(ROOT/'docs',payload/'docs')
 licenses=payload/'licenses/GCC-runtime';licenses.mkdir(parents=True)
 for name in ('COPYING.LIB','COPYING.RUNTIME','COPYING3','README'):
  shutil.copy2(Path('/mnt/c/msys64/ucrt64/share/licenses/gcc-libs')/name,licenses/name)
 dependencies=[]
 for entry in json.loads((ROOT/'dependencies.json').read_text())['files']:
  if '/binaries/' not in entry['path']:continue
  name=Path(entry['path']).name
  dependencies.append(dict(url=entry['url'],sha256=entry['sha256'],destination={'AurieCore.dll':'mods/AurieCore.dll','YYToolkit.dll':'mods/aurie/YYToolkit.dll'}.get(name,name)))
 manifest=dict(version=a.version,source_commit=subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),game_sha256=SHA,dependencies=dependencies,files=[dict(path=p.relative_to(stage).as_posix(),sha256=digest(p)) for p in sorted(stage.rglob('*')) if p.is_file()])
 (stage/'release.json').write_text(json.dumps(manifest,indent=2)+'\n')
 (stage/'START HERE.txt').write_text('Nova Drift Co-op\n\nExtract the whole ZIP, then double-click InstallCoop.cmd.\nRequires Nova Drift 1.2.38.4 on Steam (Windows x64) and an internet connection.\nThe installer downloads the pinned Aurie / YYToolkit dependencies.\nIt creates a separate co-op copy; your Steam installation and saves stay separate.\nLaunch PlayCoop.cmd in the installed folder and start a fresh run.\nF6 opens controls for either player.\n\nSource: https://github.com/murlock1000/novadrift-coop\nCorresponding source and dependency sources accompany this release.\n')
 archive=out/f'NovaDriftCoop-{a.version}-Windows-x64.zip'
 with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED) as z:
  for item in sorted(stage.rglob('*')):
   if item.is_file():z.write(item,'NovaDriftCoop/'+item.relative_to(stage).as_posix())
 (out/'SHA256SUMS.txt').write_text(f'{digest(archive)}  {archive.name}\n')
 print(archive)
if __name__=='__main__':main()
