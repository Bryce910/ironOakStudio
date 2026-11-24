"""Build the battlefield-opening candidate from the active apartment-night runtime.
Every linked object is checked against the dependency hashes recorded when it
was compiled (inputs.json, or its .d file for the water viewer object). Any
object whose dependency changed in this source tree is recompiled with its
original command, re-rooted here; every other object is linked unchanged and
its hash is verified before and after the link."""
from pathlib import Path
import hashlib,json,subprocess,time,re,sys
N=Path(__file__).resolve().parents[1];A=N.with_name('character-response-2026-11-22');S=N/'source-v1';O=N/'runtime-native-v1'
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
r=json.loads((A/'runtime-native-v1/BUILD_RECEIPT.json').read_text())
assert sha(A/'runtime/rpg')==r['binary_sha256'],'active binary changed'
def source_root(obj):return Path(str(obj).split('/runtime-native-v1/')[0])
def deps_of(obj):
    o=Path(obj);ij=Path(str(o)[:-2]+'.inputs.json')
    if ij.exists():d=json.loads(ij.read_text());return d['dependencies'],d['command']
    dfile=Path(str(o)[:-2]+'.d');text=dfile.read_text().replace('\\\n',' ')
    files=[t for t in text.split(':',1)[1].split() if t.startswith('/home/') and not t.endswith(':')]
    gen=source_root(o)
    # The water viewer was compiled from the water tree; its .d lists those files.
    deps={f:sha(f) for f in files if Path(f).exists() and f.startswith(str(gen))}
    return deps,r['compile']
changed_files=[str(p.relative_to(S)) for p in S.rglob('*') if p.is_file() and (not (A/'source-v1'/p.relative_to(S)).exists() or sha(p)!=sha(A/'source-v1'/p.relative_to(S)))]
print('changed sources:',changed_files)
link=list(r['link']);rebuild=[];covered=set()
for idx,x in enumerate(link):
    if not x.endswith('.o'):continue
    gen=source_root(x);deps,command=deps_of(x)
    stale=[]
    for dep,h in deps.items():
        if '/source-v1/' not in dep:
            if sha(dep)!=h:stale.append(dep)
            continue
        rel=dep.split('/source-v1/',1)[1]
        mine=S/rel
        if not mine.exists() or sha(mine)!=h:stale.append(rel)
        if rel in changed_files:covered.add(rel)
    if stale:rebuild.append((idx,x,gen,command,stale))
start=time.monotonic();rebuilt={}
frozen_source={str(p):sha(p) for p in S.rglob('*') if p.is_file() and p.suffix in ('.cpp','.c','.h','.inc')}
def assert_frozen():
    assert all(Path(p).exists() and sha(p)==h for p,h in frozen_source.items()),'Source changed during compilation; rebuild from frozen inputs'

for idx,x,gen,command,stale in rebuild:
    rel_obj=x.split('/runtime-native-v1/',1)[1]
    new_obj=O/rel_obj;new_obj.parent.mkdir(parents=True,exist_ok=True)
    cmd=[c.replace(str(gen)+'/source-v1',str(S)).replace(str(gen)+'/runtime-native-v1',str(O)) for c in command]
    assert str(new_obj) in cmd,(x,cmd[-3:])
    input_record=Path(str(new_obj)[:-2]+'.inputs.json')
    if new_obj.exists() and input_record.exists():
        cached=json.loads(input_record.read_text())
        if cached['command']==cmd and all(Path(f).exists() and sha(f)==h for f,h in cached['dependencies'].items()):
            covered.update(f.split('/source-v1/',1)[1] for f in cached['dependencies'] if '/source-v1/' in f)
            link[idx]=str(new_obj);rebuilt[rel_obj]=sha(new_obj)
            print('reuse candidate object',rel_obj,flush=True)
            continue
    print('compile',rel_obj,'because',stale[:4],flush=True)
    log=Path(str(new_obj)[:-2]+'.log')
    with log.open('w') as f:
        rc=subprocess.run(cmd,cwd=S,stdout=f,stderr=subprocess.STDOUT).returncode
    if rc:print(log.read_text()[-6000:]);sys.exit('compile failed: '+rel_obj)
    assert_frozen()
    d=Path(str(new_obj)[:-2]+'.d')
    files=[t for t in d.read_text().replace('\\\n',' ').split(':',1)[1].split() if t.startswith('/') and not t.endswith(':')] if d.exists() else []
    Path(str(new_obj)[:-2]+'.inputs.json').write_text(json.dumps({'command':cmd,'dependencies':{f:sha(f) for f in files}},indent=1))
    covered.update(f.split('/source-v1/',1)[1] for f in files if '/source-v1/' in f)
    link[idx]=str(new_obj);rebuilt[rel_obj]=sha(new_obj)
# New actor renderer has no object in the published baseline. Reuse its exact
# C++17 compiler/include/dependency flags, replacing only source/output paths.
for relative_source in ['engine/world_battlefield_actors.cpp']:
    template=next(x for x in link if x.endswith('/engine/world_animation_render.cpp.o'))
    template_gen=source_root(template);_,original=deps_of(template)
    new_obj=O/'native-objects'/Path(relative_source+'.o');new_obj.parent.mkdir(parents=True,exist_ok=True)
    cmd=[c.replace(str(template_gen)+'/source-v1',str(S)).replace(str(template_gen)+'/runtime-native-v1',str(O)).replace('engine/world_animation_render.cpp',relative_source) for c in original]
    cache=Path(str(new_obj)[:-2]+'.inputs.json')
    reusable=False
    if new_obj.exists() and cache.exists():
        cached=json.loads(cache.read_text());reusable=cached['command']==cmd and all(Path(f).exists() and sha(f)==h for f,h in cached['dependencies'].items())
    if not reusable:
        print('compile new',relative_source,flush=True)
        log=Path(str(new_obj)[:-2]+'.log')
        with log.open('w') as f:rc=subprocess.run(cmd,cwd=S,stdout=f,stderr=subprocess.STDOUT).returncode
        if rc:print(log.read_text()[-6000:]);sys.exit('compile failed: '+relative_source)
        assert_frozen()
        files=[t for t in Path(str(new_obj)[:-2]+'.d').read_text().replace('\\\n',' ').split(':',1)[1].split() if t.startswith('/') and not t.endswith(':')]
        cache.write_text(json.dumps({'command':cmd,'dependencies':{f:sha(f) for f in files}},indent=1))
    deps=json.loads(cache.read_text())['dependencies'];covered.update(f.split('/source-v1/',1)[1] for f in deps if '/source-v1/' in f)
    link.insert(link.index('-o'),str(new_obj));rebuilt[str(new_obj.relative_to(O))]=sha(new_obj)
uncovered=[f for f in changed_files if not f.startswith('tests/') and f not in covered and f.endswith(('.c','.cpp','.h','.inc'))]
assert not uncovered,('changed files not reached by any object',uncovered)
assert_frozen()
dest=O/'native-bin/rpg';dest.parent.mkdir(parents=True,exist_ok=True)
temporary=dest.with_name('rpg.link-'+str(time.time_ns()));link[link.index('-o')+1]=str(temporary)
unchanged={x:sha(x) for x in link if x.endswith('.o') and not x.startswith(str(O))}
subprocess.run(link,cwd=S,check=True)
assert all(sha(p)==h for p,h in unchanged.items())
temporary.replace(dest);link[link.index('-o')+1]=str(dest)
rt=N/'runtime/rpg'
if rt.is_symlink() or rt.exists():rt.unlink()
rt.symlink_to(dest)
receipt=dict(binary_sha256=sha(dest),baseline_binary_sha256=r['binary_sha256'],baseline_generation=str(A),changed_sources={p:sha(S/p) for p in changed_files},
    rebuilt_objects=rebuilt,unchanged_object_sha256=unchanged,source_sha256={str(p.relative_to(S)):sha(p) for p in S.rglob('*') if p.is_file()},
    link=link,elapsed_seconds=time.monotonic()-start,Windows_work_performed=False)
(O/'BUILD_RECEIPT.json').write_text(json.dumps(receipt,indent=2)+'\n')
print('Battlefield opening candidate built',receipt['binary_sha256'],'rebuilt',list(rebuilt),flush=True)
