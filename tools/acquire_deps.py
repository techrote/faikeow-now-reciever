#!/usr/bin/env python3
"""Explicit network step, never invoked by CMake. Linux x86_64 only."""
import argparse, json, os, platform, shutil, subprocess, tarfile, tempfile, urllib.request, zipfile
from pathlib import Path
from dependency_common import ROOT,DEPS,LOCK,sha,tree_sha,command
p=argparse.ArgumentParser();p.add_argument('--lane',choices=['all','rp2040','esp'],default='all');a=p.parse_args()
if platform.system()!='Linux' or platform.machine() not in ('x86_64','AMD64'):
    raise SystemExit('Only canonical Linux x86_64 artifacts are locked; use that host, including WSL2.')
DEPS.mkdir(parents=True,exist_ok=True); cache=DEPS/'downloads';cache.mkdir(exist_ok=True)
ledger_path=DEPS/'acquired.json'
ledger=json.loads(ledger_path.read_text()) if ledger_path.exists() else {}
for dep in LOCK['sources']:
    if a.lane not in ('all',dep['lane']): continue
    target=DEPS/dep.get('path',dep['name'])
    if not (target/'.git').exists():
        target.mkdir(parents=True,exist_ok=True)
        if any(target.iterdir()): raise SystemExit('Refusing nonempty unrecognized checkout: '+str(target))
        command(['git','init',str(target)])
        command(['git','-C',str(target),'remote','add','origin',dep['repo']])
        command(['git','-C',str(target),'fetch','--depth=1','origin',dep['commit']])
        command(['git','-C',str(target),'checkout','--detach','FETCH_HEAD'])
    if command(['git','-C',str(target),'rev-parse','HEAD'])!=dep['commit']:
        raise SystemExit('Wrong commit: '+str(target))
    if command(['git','-C',str(target),'status','--porcelain','--untracked-files=no']):
        raise SystemExit('Dirty dependency: '+str(target))
for dep in LOCK['artifacts']:
    if a.lane not in ('all',dep['lane']): continue
    archive=cache/dep['url'].rsplit('/',1)[-1]
    if not archive.exists():
        tmp=archive.with_suffix(archive.suffix+'.part')
        try:
            with urllib.request.urlopen(dep['url'],timeout=45) as r, open(tmp,'wb') as f:
                shutil.copyfileobj(r,f)
            tmp.replace(archive)
        finally:
            if tmp.exists(): tmp.unlink()
    if sha(archive)!=dep['sha256']: raise SystemExit('Checksum mismatch: '+str(archive))
    dest=DEPS/dep['name']
    if dest.exists():
        old=ledger.get(dep['name'],{})
        if old.get('tree_sha256')!=tree_sha(dest): raise SystemExit('Unrecognized/modified tree: '+str(dest))
        continue
    with tempfile.TemporaryDirectory(dir=DEPS) as temp:
        temp=Path(temp)
        if zipfile.is_zipfile(archive):
            with zipfile.ZipFile(archive) as z:
                for item in z.infolist():
                    f=(temp/item.filename).resolve()
                    if not f.is_relative_to(temp): raise SystemExit('Unsafe ZIP path')
                    if (item.external_attr>>16)&0o170000==0o120000: raise SystemExit('ZIP symlink rejected')
                z.extractall(temp)
        else:
            with tarfile.open(archive) as t: t.extractall(temp,filter='data')
        candidates=[temp]+[x for x in temp.iterdir() if x.is_dir()]
        candidates=[x for x in candidates if (x/dep['probe']).is_file()]
        if len(candidates)!=1: raise SystemExit('Unexpected archive layout: '+str(archive))
        shutil.copytree(candidates[0],dest,symlinks=True)
    ledger[dep['name']]={'archive_sha256':dep['sha256'],'tree_sha256':tree_sha(dest)}
    ledger_path.write_text(json.dumps(ledger,indent=2)+'\n')
print('Acquired pinned dependencies. No firmware was compiled or flashed.')
