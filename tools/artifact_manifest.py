#!/usr/bin/env python3
"""Record observations, not acceptance. Do not put keys or credentials in metadata."""
import argparse,datetime,json,platform,subprocess
from pathlib import Path
from dependency_common import ROOT,LOCK,sha
p=argparse.ArgumentParser();p.add_argument('--lane',required=True,choices=['native','rp2040','esp']);p.add_argument('--source-id',required=True);p.add_argument('--compiler',required=True);p.add_argument('--config',required=True,type=Path);p.add_argument('--output',required=True,type=Path);p.add_argument('artifacts',nargs='+',type=Path);a=p.parse_args()
compiler=Path(a.compiler).resolve()
version=subprocess.check_output([str(compiler),'--version'],text=True,timeout=10)
config=json.loads(a.config.read_text())
required={'board','flash','profile','platform_protocol','internal_link_protocol','profile_schema','command','evidence_class'}
if not required.issubset(config): raise SystemExit('Missing configuration fields: '+str(sorted(required-config.keys())))
files=[{'path':str(f),'bytes':f.stat().st_size,'sha256':sha(f)} for f in a.artifacts]
record={'manifest_schema':1,'recorded_at':datetime.datetime.now(datetime.timezone.utc).isoformat(),'source_id':a.source_id,'assessed_base':LOCK['assessed_commit'],'lane':a.lane,'host':platform.platform(),'compiler':{'path':str(compiler),'sha256':sha(compiler),'version_output':version},'dependency_lock_sha256':sha(ROOT/'dependencies.lock.json'),'configuration':config,'artifacts':files,'physical_acceptance':False}
a.output.write_text(json.dumps(record,indent=2)+'\n')
