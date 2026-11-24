"""Prove the first-person partition fix preserves every visible world triangle."""
from pathlib import Path
from collections import Counter
import hashlib,json,sys
import numpy as np
N=Path(__file__).resolve().parents[1];E=N.parents[1]
sys.path.insert(0,str(E/'tools'))
import finalize_modular_knight_glb as glb
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def triangles(path):
 d,b,_=glb.load(path);result=Counter()
 for p in d['meshes'][0]['primitives']:
  arrays=[glb.array(d,b,p['attributes'][key]).astype('<f4') for key in ['POSITION','NORMAL','TEXCOORD_0','JOINTS_0','WEIGHTS_0']]
  values=np.concatenate(arrays,axis=1);indices=glb.array(d,b,p['indices']).reshape(-1,3)
  material=d['materials'][p['material']]['name'].encode()
  for tri in indices:
   row=values[tri]
   # Preserve winding while ignoring a triangle's starting corner.
   key=min(np.roll(row,k,axis=0).tobytes() for k in range(3))
   result[hashlib.sha256(material+b'\0'+key).hexdigest()]+=1
 return result
before=N/'review/thomas-preview.glb';after=N/'assets/story_hero/civilian/body.glb'
a,b=triangles(before),triangles(after)
report=dict(passed=a==b,world_triangles_before=sum(a.values()),world_triangles_after=sum(b.values()),
            comparison='Exact float32 position/normal/UV/joint/weight and material for every oriented world triangle, ignoring primitive partition and cyclic corner starts.',
            first_person_change='Only distal jacket sleeves and hands selected; upper sleeves remain world-only.',
            before_sha256=sha(before),after_sha256=sha(after),missing=sum((a-b).values()),added=sum((b-a).values()))
(N/'review/WORLD_GEOMETRY_EQUIVALENCE.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
if not report['passed']:raise SystemExit(1)
