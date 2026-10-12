"""Bounded guarded attachment after independently reloading the RP observer.
The ordinary capture tool already established the ESP identity before B. This
attachment deliberately sends no ZERO/SNAP until the ongoing finite RUN ends.
"""
import contextlib, json, pathlib, subprocess, time, hashlib, sys, argparse
import serial
from datetime import datetime, timezone
ap = argparse.ArgumentParser(description=__doc__)
for key in ('manifest', 'rp-load', 'esp-write', 'prior', 'identities', 'output'):
    ap.add_argument('--' + key, type=pathlib.Path, required=True)
ap.add_argument('--manifest-sha256', required=True)
ap.add_argument('--board', choices=['A', 'B'], required=True)
ap.add_argument('--port', required=True)
ap.add_argument('--first', type=int, required=True)
ap.add_argument('--count', type=int, required=True)
ap.add_argument('--length', type=int, required=True)
args = ap.parse_args()
if serial.__version__ != "3.5":
    raise ValueError("Require pinned pyserial 3.5")
if not (1 <= args.count <= 1800 and 1 <= args.length <= 250 and (0 <= args.first <= 4294967295 - args.count)):
    raise ValueError('Bounded active reload phase required')
repo = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(repo / 'tools/hardware'))
from transport_capture import validate_selected_cdc, validate_firmware_source
from transport_assessment import assess_rp_restart
from rp2040_backup import sha256, private_destination
head = subprocess.check_output(['git', '-C', str(repo), 'rev-parse', 'HEAD'], text=True).strip()
if subprocess.check_output(['git', '-C', str(repo), 'status', '--porcelain'], text=True).strip():
    raise ValueError("Guard/assessment failed: not subprocess.check_output(['git', '-C', str(repo), 'status', '--porcelain'], text=True).strip()")
manifest = args.manifest
manifest_sha = args.manifest_sha256
if not sha256(manifest) == manifest_sha:
    raise ValueError('Guard/assessment failed: sha256(manifest) == manifest_sha')
m = json.loads(manifest.read_text(encoding='utf-8'))
source = m['source_commit']
if validate_firmware_source(repo, source) != m['source_tree']:
    raise ValueError('Firmware source tree mismatch')
if sha256(repo / 'dependencies.lock.json') != m['working_lock_sha256']:
    raise ValueError('Working dependency lock mismatch')
if not hashlib.sha256(subprocess.check_output(['git', '-C', str(repo), 'show', 'HEAD:dependencies.lock.json'])).hexdigest() == m['lock_sha256']:
    raise ValueError("Guard/assessment failed: hashlib.sha256(subprocess.check_output(['git', '-C', str(repo), 'show', 'HEAD:dependencies.lock.json'])).hexdigest() == m['lock_sha256']")
for a in m['artifacts']:
    if not sha256(pathlib.Path(a['path'])) == a['sha256']:
        raise ValueError("Guard/assessment failed: sha256(pathlib.Path(a['path'])) == a['sha256']")
rp = json.loads(args.rp_load.read_text(encoding='utf-8'))
if not (rp['source_commit'] == source and rp['board'] == args.board and rp['verified_load'] and (rp['mode'] == 'sram') and (rp['image_sha256'] == m['rp_sha256'])):
    raise ValueError("Guard/assessment failed: rp['source_commit'] == source and rp['board'] == args.board and rp['verified_load'] and (rp['mode'] == 'sram') and (rp['image_sha256'] == m['rp_sha256'])")
esp_write = args.esp_write
ew = json.loads(esp_write.read_text(encoding='utf-8'))
if not (ew['source_commit'] == source and ew['board'] == args.board and ew['exact_readback'] and (ew['image_sha256'] == m['esp_sha256']) and (sha256(esp_write.parent / 'readback.bin') == m['esp_sha256'])):
    raise ValueError("Guard/assessment failed: ew['source_commit'] == source and ew['board'] == args.board and ew['exact_readback'] and (ew['image_sha256'] == m['esp_sha256']) and (sha256(esp_write.parent / 'readback.bin') == m['esp_sha256'])")
prior = args.prior
pr = json.loads((prior / 'result.json').read_text(encoding='utf-8'))
if not (pr['source_commit'] == source and pr['manifest_sha256'] == manifest_sha and (pr['capture_sha256'] == sha256(prior / 'capture.jsonl'))):
    raise ValueError("Guard/assessment failed: pr['source_commit'] == source and pr['manifest_sha256'] == manifest_sha and (pr['capture_sha256'] == sha256(prior / 'capture.jsonl'))")
old = [json.loads(l) for l in (prior / 'capture.jsonl').read_text(encoding='utf-8').splitlines()]
if not any((x.get('command') == 'B' for x in old)):
    raise ValueError("Guard/assessment failed: any((x.get('command') == 'B' for x in old))")
if not any((x.get('command') == f'X RUN {args.count} {args.length} 30 {args.first}' for x in old)):
    raise ValueError("Guard/assessment failed: any((x.get('command') == f'X RUN {args.count} {args.length} 30 {args.first}' for x in old))")
old_e = next((x['response'] for x in old if x.get('response', {}).get('event') == 'esp'))
if not (old_e['source'] == source and old_e['mac'] == m['radio_mac']):
    raise ValueError("Guard/assessment failed: old_e['source'] == source and old_e['mac'] == m['radio_mac']")
prior_pending = next((x['response'] for x in reversed(old) if x.get('response', {}).get('event') == 'snapshot'), None)
if prior_pending is None or prior_pending['datagram_pending'] != 1:
    raise ValueError('Prior capture did not hold an old RP consumer slot before B')
ident = json.loads(args.identities.read_text(encoding='utf-8-sig'))[args.board]
out = private_destination(args.output, repo)
out.mkdir()
subprocess.run(['pwsh', '-NoProfile', '-File', str(repo / 'tools/hardware/discover_windows.ps1'), '-OutputPath', str(out / 'inventory.json')], check=True)
validate_selected_cdc(json.loads((out / 'inventory.json').read_text(encoding='utf-8-sig'))['devices'], args.port, ident)
start = time.monotonic()
records = []
partial = bytearray()
total = 0
verified = False
result = {'board': args.board, 'source_commit': source, 'source_tree': m['source_tree'], 'tool_commit': head, 'captured_at_utc': datetime.now(timezone.utc).isoformat(), 'tool_sha256': sha256(pathlib.Path(__file__)), 'manifest_sha256': manifest_sha, 'rp_load_sha256': sha256(args.rp_load), 'esp_write_sha256': sha256(args.esp_write), 'result': 'NOT ASSESSED', 'prior_capture_sha256': sha256(prior / 'capture.jsonl')}

@contextlib.contextmanager
def shutdown_guard():
    try:
        yield
    finally:
        if verified:
            try:
                send('X STOP')
                receive(0.5)
                result['stop_submitted'] = True
            except Exception as error:
                result['cleanup_error'] = str(error)
try:
    with (out / 'capture.jsonl').open('w', encoding='utf-8') as log, serial.Serial(args.port, 115200, timeout=0.05, write_timeout=1) as port, shutdown_guard():
        port.dtr = True
        port.rts = False

        def record(r):
            r = {'host_elapsed': time.monotonic() - start, **r}
            records.append(r)
            log.write(json.dumps(r) + '\n')
            log.flush()
            if len(records) > 10000:
                raise ValueError('Record bound')

        def send(c):
            record({'command': c})
            b = (c + '\n').encode('ascii')
            if not port.write(b) == len(b):
                raise ValueError('Guard/assessment failed: port.write(b) == len(b)')

        def receive(seconds, event=None):
            global total
            end = time.monotonic() + seconds
            while time.monotonic() < end:
                b = port.read(4096)
                total += len(b)
                partial.extend(b)
                if total > 4 * 1024 * 1024 or len(partial) > 8192:
                    raise ValueError('Capture bound')
                while b'\n' in partial:
                    l, _, rest = partial.partition(b'\n')
                    partial[:] = rest
                    if not l.strip():
                        continue
                    r = json.loads(l)
                    record({'response': r})
                    if event and r.get('event') == event:
                        return r
            if event:
                raise ValueError('Missing ' + event)
        send('INFO')
        identity = receive(5, 'identity')
        if not (identity['source'] == source and identity['uid'].lower() == ident['flash_id'].lower()):
            raise ValueError("Guard/assessment failed: identity['source'] == source and identity['uid'].lower() == ident['flash_id'].lower()")
        verified = True
        send('S')
        initial = receive(5, 'snapshot')
        e = receive(70, 'esp')
        after = receive(5, 'after_esp')
        if not (e['source'] == source and e['mac'] == m['radio_mac']):
            raise ValueError("Guard/assessment failed: e['source'] == source and e['mac'] == m['radio_mac']")
        errors = assess_rp_restart(initial, after, old_e, e, prior_pending,
                                   first=args.first, count=args.count, length=args.length)
        if errors:
            raise ValueError('; '.join(errors))
        n = after['delivered']
        result.update(result='PASS', initial=initial, after=after, esp_after=e, delivered_suffix=n, first_pattern_id=args.first + args.count - n, last_pattern_id=args.first + args.count - 1)
except Exception as error:
    result['error'] = str(error)
    raise
finally:
    result['bytes_captured'] = total
    result['capture_sha256'] = sha256(out / 'capture.jsonl')
    (out / 'result.json').write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
print('PASS: independent RP reload, retained ESP generation, bounded parser recovery, exact contiguous payload suffix', result['delivered_suffix'])
