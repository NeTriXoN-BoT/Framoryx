"""Promote only verified artifacts from this exact commit into a draft release."""
import argparse
import json
from pathlib import Path
import re
import subprocess

def gh(*args):
    return subprocess.run(['gh', *args], check=True, capture_output=True, text=True, encoding='utf-8').stdout

def api(path):
    return json.loads(gh('api', path))

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--repo', required=True)
parser.add_argument('--tag', required=True)
parser.add_argument('--run', required=True, type=int)
args=parser.parse_args()
if not re.fullmatch(r'[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+', args.repo):
    parser.error('Invalid repository')
root=Path(__file__).resolve().parents[1]
version=(root/'VERSION').read_text(encoding='utf-8').strip()
if not re.fullmatch(r'\d+\.\d+\.\d+', version) or args.tag!='v'+version:
    parser.error('Release tag must match VERSION')
sha=subprocess.run(['git','rev-parse','HEAD'], check=True, capture_output=True, text=True).stdout.strip()
base='repos/'+args.repo
run=api(base+'/actions/runs/'+str(args.run))
if run['head_sha']!=sha or run['name']!='Shared desktop / Windows and Linux' or run['conclusion']!='success' or run['head_branch']!='main':
    raise RuntimeError('Both native desktop packages must pass for this exact main commit')
runs=api(base+'/actions/runs?head_sha='+sha+'&per_page=100')['workflow_runs']
for name in ['Build and core checks', 'CodeQL security analysis', 'Secret scan']:
    matches=[item for item in runs if item['name']==name]
    if not matches or max(matches,key=lambda item:item['id'])['conclusion']!='success':
        raise RuntimeError(name+' has not passed for this commit')
if api(base+'/code-scanning/alerts?state=open&per_page=1'):
    raise RuntimeError('Open code-scanning alerts must be resolved before release')
destination=root/'dist/release-assets'
destination.mkdir(parents=True,exist_ok=False)
for artifact in ['Framoryx-Desktop-Windows-x64','Framoryx-Desktop-Linux-x64']:
    gh('run','download',str(args.run),'--repo',args.repo,'--name',artifact,'--dir',str(destination))
source=destination/'Framoryx-Source.zip'
subprocess.run(['git','archive','--format=zip','--prefix=Framoryx-source/','--output',str(source),sha],check=True)
import hashlib
(destination/(source.name+'.sha256')).write_text(hashlib.sha256(source.read_bytes()).hexdigest()+'  '+source.name+'\n',encoding='utf-8')
required=['Framoryx-Windows-x64.zip','Framoryx-Linux-x86_64.AppImage','Framoryx-Linux-x86_64.tar.gz','Framoryx-Source.zip']
for name in required:
    path=destination/name
    if not path.is_file() or not (destination/(name+'.sha256')).is_file():
        raise RuntimeError('Missing verified release asset: '+name)
    expected=(destination/(name+'.sha256')).read_text(encoding='utf-8').split()[0]
    with path.open('rb') as handle:
        actual=hashlib.file_digest(handle,'sha256').hexdigest()
    if actual!=expected:
        raise RuntimeError('Release asset checksum mismatch: '+name)
notes=root/('docs/releases/'+args.tag+'.md')
assets=[str(path) for path in sorted(destination.iterdir()) if path.is_file()]
print(gh('release','create',args.tag,*assets,'--repo',args.repo,'--draft','--target',sha,'--title','Framoryx '+version,'--notes-file',str(notes)))
