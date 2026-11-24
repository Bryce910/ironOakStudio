"""Compile the changed viewer; link only hash-verified unchanged baseline units."""
from pathlib import Path
import hashlib,json,subprocess,time
N=Path(__file__).resolve().parents[1];B=N.with_name('camp-completion-2026-11-21');S=N/'source-v1';O=N/'runtime-native-v1'
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
r=json.loads((B/'runtime-native-v1/BUILD_RECEIPT.json').read_text())
assert sha(B/'runtime/rpg')==r['binary_sha256']
original={str(p.relative_to(B/'source-v1')):sha(p) for p in (B/'source-v1').rglob('*') if p.is_file()}
changed=[p for p,h in original.items() if sha(S/p)!=h]
assert changed==['engine/world_viewer.cpp'],changed
old_object=B/'runtime-native-v1/native-objects/engine/world_viewer.cpp.o'
inputs=json.loads(old_object.with_suffix('.inputs.json').read_text())
for dep,h in inputs['dependencies'].items():
    assert sha(dep)==h,dep
command=[x.replace(str(B/'source-v1'),str(S)).replace(str(B/'runtime-native-v1'),str(O)) for x in inputs['command']]
obj=O/'native-objects/engine/world_viewer.cpp.o';obj.parent.mkdir(parents=True,exist_ok=True)
start=time.monotonic()
with (O/'compile.log').open('w') as log:subprocess.run(command,cwd=S,stdout=log,stderr=subprocess.STDOUT,check=True)
link=[str(obj) if x==str(old_object) else x for x in r['link']]
dest=O/'native-bin/rpg';dest.parent.mkdir(parents=True,exist_ok=True);link[link.index('-o')+1]=str(dest)
unchanged_objects={x:sha(x) for x in link if x.endswith('.o') and x!=str(obj)}
subprocess.run(link,cwd=S,check=True)
assert all(sha(p)==h for p,h in unchanged_objects.items())
(N/'runtime/rpg').symlink_to(dest)
receipt=dict(binary_sha256=sha(dest),baseline_binary_sha256=r['binary_sha256'],changed_sources={p:sha(S/p) for p in changed},added_source_sha256={'engine/world_water_mirror_cache.h':sha(S/'engine/world_water_mirror_cache.h')},baseline_source_sha256=original,source_sha256={str(p.relative_to(S)):sha(p) for p in S.rglob('*') if p.is_file()},unchanged_object_sha256=unchanged_objects,compile=command,link=link,elapsed_seconds=time.monotonic()-start,Windows_work_performed=False)
(O/'BUILD_RECEIPT.json').write_text(json.dumps(receipt,indent=2)+'\n')
print('Water candidate built',receipt['binary_sha256'],flush=True)
