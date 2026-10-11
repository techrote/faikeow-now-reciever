from pathlib import Path
import hashlib, json, os, subprocess
ROOT=Path(__file__).resolve().parents[1]
DEPS=Path(os.environ.get('FNR_DEPS',ROOT/'.deps')).resolve()
LOCK=json.loads((ROOT/'dependencies.lock.json').read_text())
def sha(path):
    h=hashlib.sha256()
    with open(path,'rb') as f:
        for chunk in iter(lambda:f.read(1024*1024),b''): h.update(chunk)
    return h.hexdigest()
def tree_sha(path):
    h=hashlib.sha256()
    for p in sorted(path.rglob('*')):
        if p.is_file():
            h.update(p.relative_to(path).as_posix().encode()+b'\0')
            h.update(sha(p).encode()+b'\n')
    return h.hexdigest()
def command(args):
    return subprocess.check_output(args,text=True,timeout=120).strip()
