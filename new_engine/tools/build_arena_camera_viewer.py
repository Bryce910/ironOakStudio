#!/usr/bin/env python3
"""Recompile only the camera viewer from the recorded, checked platform recipes."""
import argparse
from concurrent.futures import ThreadPoolExecutor
import json
from pathlib import Path
import subprocess
from build_arena_configuration_runtime import ROOT,sha,snapshot

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--revision',type=int,required=True);args=parser.parse_args()
    assert 2<=args.revision<100
    output=ROOT/f'build/arena-camera/runtime-v{args.revision}'
    freeze=output/'SOURCE_FREEZE.json';record=json.loads(freeze.read_text());freeze_hash=sha(freeze)
    assert record['source_sha256']==snapshot(),'Sources changed after freeze'
    plan=json.loads((ROOT/'build/arena-camera/runtime-v1/BUILD_PLAN.json').read_text())
    def compile(platform):
        units=[u for u in plan['plans'][platform]['units'] if Path(u['source']).name=='world_viewer.cpp'];assert len(units)==1
        command=[arg.replace(str(ROOT/'build/arena-camera/runtime-v1'),str(output)) for arg in units[0]['command']]
        target=output/(platform+'-objects');target.mkdir(exist_ok=True)
        obj=Path(command[command.index('-o')+1]);assert not obj.exists()
        with (target/'world_viewer.cpp.log').open('w') as log:subprocess.run(command,cwd=ROOT,stdout=log,stderr=subprocess.STDOUT,check=True)
        return dict(platform=platform,command=command,object=str(obj),sha256=sha(obj))
    with ThreadPoolExecutor(max_workers=2) as pool:results=list(pool.map(compile,['native','windows']))
    assert record['source_sha256']==snapshot() and sha(freeze)==freeze_hash,'Frozen sources changed during build'
    (output/'VIEWER_READY.json').write_text(json.dumps(dict(ok=True,compile=results,source_freeze_sha256=freeze_hash),indent=2)+'\n')
    print(json.dumps(dict(ok=True,revision=args.revision,platforms=['native','windows'])))
if __name__=='__main__':main()
