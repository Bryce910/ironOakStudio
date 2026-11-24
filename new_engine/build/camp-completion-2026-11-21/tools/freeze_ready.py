from pathlib import Path
import json,hashlib,importlib.util,sys
N=Path(__file__).resolve().parents[1]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
r=json.loads((N/'runtime-native-v1/BUILD_RECEIPT.json').read_text())
assert sha(N/'runtime/rpg')==r['binary_sha256']
for f,h in r['source_sha256'].items():assert sha(N/'source-v1'/f)==h,f
artifacts={}
for top in ['shared/shaders','presentation','world-project/scenery','world-project/models/art_completion','world-project/models/art_birch','world-project/models/camp_variants','world-project/models/camp_birds']:
 for p in (N/top).rglob('*'):
  if p.is_file():artifacts[str(p.relative_to(N))]=sha(p)
for f in ['launch.py','check_unified.py','world-project/world.studio','world-project/collision/structures/catalog']:
 artifacts[f]=sha(N/f)
# The active launch wrapper inherits immutable launch policy through several
# generations. Freeze those exact inputs as well as the visible overlay.
artifacts['vegetation/creek_dressing_instances.txt']=sha(N/'vegetation/creek_dressing_instances.txt')
sys.dont_write_bytecode=True
spec=importlib.util.spec_from_file_location('freeze_launch',N/'launch.py')
launcher=importlib.util.module_from_spec(spec);spec.loader.exec_module(launcher)
seen=set()
while launcher is not None:
 path=Path(launcher.__file__).resolve()
 assert path not in seen,'Cyclic inherited launch modules'
 seen.add(path);artifacts[str(path)]=sha(path)
 launcher=getattr(launcher,'base',None)
out=dict(binary_sha256=r['binary_sha256'],source_sha256=r['source_sha256'],artifacts=artifacts)
(N/'READY_INPUTS.json').write_text(json.dumps(out,indent=2)+'\n')
print('Frozen',len(r['source_sha256']),'sources and',len(artifacts),'runtime artifacts')
