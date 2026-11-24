#!/usr/bin/env python3
"""Build all 108 Linux application units from this candidate's owned source."""
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor, as_completed
import hashlib, json, subprocess, time
N=Path(__file__).resolve().parent
B=N.with_name('demo-readiness-2026-11-21')
P=N.with_name('woodland-polish-2026-11-21')
S=N/'source-v1';O=N/'runtime-native-v1'
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def rewrite(s):return s.replace(str(P/'source-v1'),str(S)).replace(str(P/'runtime-native-v1'),str(O))
def main():
    baseline=json.loads((N/'BASELINE.json').read_text())
    if sha(B/'runtime/rpg')!=baseline['binary_sha256']:raise RuntimeError('Published baseline changed during candidate work')
    plan=json.loads((P/'runtime-native-v1/BUILD_PLAN.json').read_text())
    units=[]
    for row in plan['units']:
        units.append(dict(source=row['logical_source'],command=[rewrite(a) for a in row['command']],object=rewrite(row['target'])))
    template=next(u for u in units if u['source']=='engine/world_bloom.cpp')
    for name in ('world_auto_exposure','world_object_motion','world_upscale','world_ambience'):
        source='engine/'+name+'.cpp'
        units.append(dict(source=source,command=[a.replace('world_bloom.cpp',name+'.cpp') for a in template['command']],object=template['object'].replace('world_bloom.cpp',name+'.cpp')))
    before={str(p.relative_to(S)):sha(p) for p in S.rglob('*') if p.is_file()}
    start=time.monotonic()
    def compile_unit(row):
        obj=Path(row['object']);obj.parent.mkdir(parents=True,exist_ok=True)
        receipt=obj.with_suffix('.inputs.json');dep=obj.with_suffix('.d')
        # Reuse only this candidate's previous compile with exact dependencies.
        if obj.exists() and receipt.exists():
            old=json.loads(receipt.read_text())
            if old['command']==row['command'] and all(Path(p).exists() and sha(p)==h for p,h in old['dependencies'].items()):return row['source'],'cached'
        result=subprocess.run(row['command'],capture_output=True,text=True,cwd=S)
        obj.with_suffix('.log').write_text(result.stdout+result.stderr)
        if result.returncode:raise RuntimeError(row['source']+'\n'+result.stderr[-5000:])
        import shlex
        deps=shlex.split(dep.read_text().replace('\\\n',' ').split('\n',1)[0].split(':',1)[1])
        receipt.write_text(json.dumps(dict(command=row['command'],dependencies={p:sha(p) for p in deps}),indent=2)+'\n')
        return row['source'],'compiled'
    results=[]
    with ThreadPoolExecutor(max_workers=4) as pool:
        for task in as_completed([pool.submit(compile_unit,u) for u in units]):
            name,status=task.result();results.append(dict(source=name,status=status));print(status,name,flush=True)
    after={str(p.relative_to(S)):sha(p) for p in S.rglob('*') if p.is_file()}
    if before!=after:raise RuntimeError('Source changed during compilation')
    link=[rewrite(a) for a in plan['link']];i=link.index('-o')
    link[i:i]=[u['object'] for u in units if u['source'] in {'engine/world_auto_exposure.cpp','engine/world_object_motion.cpp','engine/world_upscale.cpp','engine/world_ambience.cpp'}]
    (O/'native-bin').mkdir(parents=True,exist_ok=True);subprocess.run(link,check=True)
    binary=O/'native-bin/rpg';r=N/'runtime/rpg';r.unlink();r.symlink_to(binary)
    (O/'BUILD_RECEIPT.json').write_text(json.dumps(dict(units=results,unit_count=len(units),binary_sha256=sha(binary),source_sha256=before,link=link,elapsed_seconds=time.monotonic()-start,Windows_work_performed=False),indent=2)+'\n')
    print('Built',len(units),'units:',sha(binary),flush=True)
if __name__=='__main__':main()
