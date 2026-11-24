#!/usr/bin/env python3
"""Validate the opening deadlock fix against its unchanged demo assets."""
from pathlib import Path
import argparse,hashlib,json,sys
N=Path(__file__).resolve().parent
ROOT=N.parents[2]
BASE=N.parent/'demo-final-pass-2026-11-15'
RECEIPT=N/'OPENING_UNLOCK_READY.json'
DISPATCHES=[ROOT/name for name in ('Launch_Valwood_Demo.sh','Launch_Valwood_Arena_Final.sh')]

def digest(p):
    with Path(p).open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def stat(p):
    s=Path(p).stat();return [s.st_dev,s.st_ino,s.st_size,s.st_mtime_ns,s.st_ctime_ns]
def require(ok,message):
    if not ok:raise RuntimeError(message)
def pins(hashes,stats):
    for name,expected in hashes.items():
        p=Path(name)
        if p in DISPATCHES:p=N/'previous-launchers'/p.name
        before=stat(p)
        if before[2]<1048576 or before!=stats.get(name):require(digest(p)==expected and stat(p)==before,'Changed reviewed file: '+str(p))
def parent():
    d=json.loads((BASE/'DEMO_FINAL_READY.json').read_text())
    require(d['ready'] is True,'Demo baseline is not ready')
    pins(d['inputs'],d['frozen_stats'])
    for path,target in d['asset_routes'].items():require(Path(path).is_symlink() and str(Path(path).resolve(strict=True))==target,'Changed asset link: '+path)
def publish():
    require(not RECEIPT.exists(),'Published evidence is immutable')
    parent()
    ready=json.loads((N/'runtime-native-v1/READY.json').read_text())
    require(ready['ready'] and ready['application_units']==103,'Incomplete native build')
    old=json.loads((BASE/'runtime-native-v1/READY.json').read_text())['source_sha256']
    changed={p for p,h in ready['source_sha256'].items() if old.get(p)!=h}
    require(changed=={'engine/world_viewer.cpp','engine/world_cutscenes_runtime.inc','engine/world_opening_prologue_qa.inc'},'Unexpected source changes')
    for mode in ('baseline','natural','skip'):
        review=json.loads((N/'review'/mode/'REVIEW.json').read_text());require(review['passed'],'Missing native regression: '+mode)
        if mode!='baseline':require(review['binary_sha256']==ready['sha256'],'Tested binary changed')
    files=[N/'source-v1'/p for p in ready['source_sha256']]
    for p,h in ready['source_sha256'].items():require(digest(N/'source-v1'/p)==h,'Build source changed')
    files += [N/'runtime-native-v1'/p for p in ('READY.json','BUILD_PLAN.json','native-bin/rpg')]
    files += [Path(__file__),N/'launch_demo.sh',N/'launch_arena.sh',N/'run_startup_review.py',N/'CPU_REVIEW.txt',*DISPATCHES,BASE/'DEMO_FINAL_READY.json']
    files += [N/'review'/m/'REVIEW.json' for m in ('baseline','natural','skip')]
    files += [N/'review'/m/'result.bmp.world.json' for m in ('baseline','natural','skip')]
    hashes={str(p):digest(p) for p in files}
    require(digest(N/'runtime-native-v1/native-bin/rpg')==ready['sha256'],'Binary changed')
    RECEIPT.write_text(json.dumps(dict(ready=True,native_only=True,windows_work_performed=False,sha256=ready['sha256'],inputs=hashes,stats={str(p):stat(p) for p in files}),indent=2)+'\n')
def check():
    d=json.loads(RECEIPT.read_text());require(d['ready'],'Opening fix is not ready');parent()
    # The current dispatch is intentionally new; validate it against this pass.
    own=dict(d['inputs'])
    for dispatch in DISPATCHES:
        expected=own.pop(str(dispatch));require(digest(dispatch)==expected,'Changed launcher: '+str(dispatch))
    pins(own,d['stats'])
if __name__=='__main__':
    ap=argparse.ArgumentParser();ap.add_argument('--publish',action='store_true');ap.add_argument('--check-only',action='store_true');args=ap.parse_args()
    try:
        publish() if args.publish else check()
        print('Demo opening ready: natural playback, skipping and released controls verified.')
    except Exception as e:print('Opening check failed:',e,file=sys.stderr);sys.exit(1)
