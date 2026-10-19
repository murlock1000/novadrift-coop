#!/usr/bin/env python3
"""Create release source archive from committed project and pinned upstream trees."""
import argparse,hashlib,io,json,subprocess,urllib.request,zipfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def sha(data):return hashlib.sha256(data).hexdigest()
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--version',required=True);a=p.parse_args()
 if not all(c.isalnum() or c in '.-' for c in a.version):p.error('Invalid version')
 if subprocess.check_output(['git','status','--porcelain'],cwd=ROOT):p.error('Commit the release source first')
 out=ROOT/'local/releases'/a.version;manifest=json.loads((out/'NovaDriftCoop/release.json').read_text())
 commit=subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip()
 sdk=json.loads((ROOT/'dependencies.json').read_text())['sdk_commit']
 sources=[('Aurie-v2.0.2','AurieFramework/Aurie','v2.0.2'),('YYToolkit-v5.0.0c','AurieFramework/YYToolkit','v5.0.0c'),('YYToolkit-SDK','AurieFramework/YYToolkit',sdk)]
 archive=out/f'NovaDriftCoop-{a.version}-Source.zip';provenance={'project_commit':commit,'upstream_sources':[],'binary_manifest':manifest}
 with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED) as target:
  own=subprocess.check_output(['git','archive','--format=zip','HEAD'],cwd=ROOT)
  with zipfile.ZipFile(io.BytesIO(own)) as z:
   for name in z.namelist():
    if not name.endswith('/'):target.writestr('novadrift-coop/'+name,z.read(name))
  for label,repo,ref in sources:
   url=f'https://codeload.github.com/{repo}/zip/{ref}'
   data=urllib.request.urlopen(url,timeout=90).read();provenance['upstream_sources'].append(dict(name=label,url=url,sha256=sha(data)))
   with zipfile.ZipFile(io.BytesIO(data)) as z:
    for name in z.namelist():
     if name.endswith('/'):continue
     relative=name.split('/',1)[1]
     if '..' in Path(relative).parts:raise RuntimeError('Unexpected source archive path')
     target.writestr('third_party/'+label+'/'+relative,z.read(name))
  target.writestr('build-provenance.json',json.dumps(provenance,indent=2))
 sums=[]
 for item in sorted(out.glob('*.zip')):sums.append(f'{sha(item.read_bytes())}  {item.name}\n')
 (out/'SHA256SUMS.txt').write_text(''.join(sums));print(archive)
if __name__=='__main__':main()
