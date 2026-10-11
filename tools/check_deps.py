#!/usr/bin/env python3
import json,sys
from dependency_common import DEPS,LOCK,tree_sha,command
lane=sys.argv[1] if len(sys.argv)>1 else 'all'
if lane not in ('all','rp2040','esp'): raise SystemExit('lane must be all/rp2040/esp')
ledger_path=DEPS/'acquired.json'
if not ledger_path.exists(): raise SystemExit('Dependencies absent: run tools/acquire_deps.py explicitly on a networked host')
ledger=json.loads(ledger_path.read_text())
for d in LOCK['sources']:
    if lane not in ('all',d['lane']): continue
    p=DEPS/d.get('path',d['name'])
    if command(['git','-C',str(p),'rev-parse','HEAD'])!=d['commit']: raise SystemExit('Wrong source '+d['name'])
    if command(['git','-C',str(p),'status','--porcelain','--untracked-files=no']): raise SystemExit('Dirty source '+d['name'])
for d in LOCK['artifacts']:
    if lane not in ('all',d['lane']): continue
    rec=ledger.get(d['name'],{})
    if rec.get('archive_sha256')!=d['sha256'] or rec.get('tree_sha256')!=tree_sha(DEPS/d['name']):
        raise SystemExit('Unverified or modified artifact '+d['name'])
print('Dependency identities and extracted trees match acquisition ledger.')
