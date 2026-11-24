#!/usr/bin/env python3
"""Link only the camera viewer update into the accepted arena's existing runtime."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
from build_arena_configuration_runtime import dependencies,sha,snapshot
ROOT=Path(__file__).resolve().parents[1]
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--platform',choices=('native','windows'),required=True)
    p.add_argument('--revision',type=int,default=1)
    a=p.parse_args();assert 1<=a.revision<100
    base=ROOT/'build/arena-configuration/runtime-v3';built=ROOT/f'build/arena-camera/runtime-v{a.revision}';out=ROOT/f'build/arena-camera/accepted-v{a.revision}'
    freeze=json.loads((built/'SOURCE_FREEZE.json').read_text())
    assert freeze['source_sha256']==snapshot(),'Source changed during compilation'
    original=json.loads((base/(a.platform.upper()+'_READY.json')).read_text())
    assert sha(original['binary'])==original['sha256'],'Accepted binary changed'
    command=list(original['link']['command']);cwd=Path(original['link']['cwd'])
    affected={ROOT/n for n in ['engine/world_viewer.cpp','engine/world_arena_combat.h','engine/world_combat_arena_runtime.inc','engine/world_arena_qa.inc']}
    records={};replaced=[]
    for i,arg in enumerate(command):
        if not arg.endswith(('.o','.obj')):continue
        obj=(Path(arg) if Path(arg).is_absolute() else cwd/arg).resolve(strict=True);records[str(obj)]=sha(obj)
        dep=next(d for d in [obj.with_suffix('.d'),Path(str(obj)+'.d')] if d.is_file())
        if affected.intersection(dependencies(dep)):
            assert obj.name=='world_viewer.cpp'+('.o' if a.platform=='native' else '.obj'),'Unexpected camera-dependent object: '+str(obj)
            new=built/(a.platform+'-objects')/obj.name
            assert new.is_file() and new.stat().st_mtime_ns>=(built/'SOURCE_FREEZE.json').stat().st_mtime_ns
            command[i]=str(new);replaced.append(dict(previous=str(obj),previous_sha256=records[str(obj)],replacement=str(new),sha256=sha(new)))
    assert len(replaced)==1,'The arena camera must replace exactly the viewer object'
    binary=out/'bin'/('rpg' if a.platform=='native' else 'rpg.exe');binary.parent.mkdir(parents=True,exist_ok=True)
    assert not binary.exists(),'Use a new patch revision'
    command[command.index('-o')+1]=str(binary)
    with (out/(a.platform+'-link.log')).open('w') as log:subprocess.run(command,cwd=cwd,stdout=log,stderr=subprocess.STDOUT,check=True)
    assert all(sha(n)==h for n,h in records.items()),'Accepted object changed'
    if a.platform=='windows':
        for dll in (base/'bin').glob('*.dll'):shutil.copyfile(dll,out/'bin'/dll.name)
    report=dict(ok=True,binary=str(binary),sha256=sha(binary),base_binary=original['binary'],base_sha256=original['sha256'],
        replaced=replaced,preserved_objects=records,link=dict(command=command,cwd=str(cwd)),
        scope='Camera-only viewer replacement. All other accepted arena objects retained, including scenery; concurrent source scenery edits are not activated.',
        source_freeze_sha256=sha(built/'SOURCE_FREEZE.json'))
    (out/(a.platform.upper()+'_READY.json')).write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(dict(ok=True,binary=str(binary),sha256=report['sha256'])))
if __name__=='__main__':main()
