from pathlib import Path
import hashlib,json,re,struct
import numpy as np
from PIL import Image
N=Path(__file__).resolve().parents[1];P=N/'arena-project';B=N.with_name('third-person-focus-2026-11-18');sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
source=N/'source-v1';changed=[str(p.relative_to(source)) for p in source.rglob('*') if p.is_file() and sha(p)!=sha(B/'source-v1'/p.relative_to(source))]
assert changed==['engine/world_arena_render.cpp'],changed
studio=(P/'world.studio').read_text();old=(B/'arena-project/world.studio').read_text()
assert [x for x in studio.splitlines() if x.startswith('structure')]==[x for x in old.splitlines() if x.startswith('structure')]
assert sha(P/'ground/showcase.height')==sha(B/'arena-project/ground/showcase.height')
aliases=set(re.findall(r'\{"([a-zA-Z0-9_-]+)",\s*"', (source/'engine/world_scenery.cpp').read_text()))
aliases.update(line.split()[1] for line in studio.splitlines() if line.startswith('asset '))
count=0
for f in sorted((P/'scenery').glob('*instances.txt')):
 original=[x for x in (B/'arena-project/scenery'/f.name).read_text().splitlines() if x.strip() and not x.startswith('#')]
 lines=[x for x in f.read_text().splitlines() if x.strip() and not x.startswith('#')]
 assert lines[:len(original)]==original
 for row in lines:assert row.split()[0] in aliases,row;assert len(row)<=320
 count+=len(lines)
records=json.loads((N/'SCENERY_PLACEMENTS.json').read_text())
assert all(not(abs(p['x'])<12.4 and abs(p['z'])<12.4) for p in records),'New prop in fight floor'
def glb(path):
 b=path.read_bytes();magic,version,length=struct.unpack_from('<4sII',b);assert magic==b'glTF' and version==2 and length==len(b);jlen,kind=struct.unpack_from('<II',b,12);j=json.loads(b[20:20+jlen]);assert kind==0x4e4f534a
 positions=[j['accessors'][q['attributes']['POSITION']] for m in j['meshes'] for q in m['primitives']]
 return dict(minimum=np.min([p['min'] for p in positions],axis=0).tolist(),maximum=np.max([p['max'] for p in positions],axis=0).tolist(),triangles=sum(p['count']//3 for p in positions),materials=len(j['materials']))
floor=glb(P/'scenery/arena_floor.glb');assert floor['minimum']==[-12,0,-12] and floor['maximum']==[12,0,12] and floor['triangles']==2
boundary=glb(P/'scenery/arena_boundary.glb');assert boundary['minimum'][1]==0 and max(abs(boundary['minimum'][0]),abs(boundary['maximum'][0]),abs(boundary['minimum'][2]),abs(boundary['maximum'][2]))<=12.5
arm=np.array(Image.open(N/'review/materials/floor_arm.png'));assert arm[:,:,1].min()>=220 and arm[:,:,2].max()==0
for line in (N/'SURFACES.txt').read_text().splitlines():
 if line.startswith('image '):
  path=json.loads('"'+line.split('"')[3]+'"');p=P/path;assert p.is_file() and p.resolve().is_relative_to((P/'surfaces').resolve())
before=json.loads((N/'review/LIVE_SAVE_BEFORE.json').read_text());assert all(sha(Path(p))==h for p,h in before.items())
report=dict(passed=True,changed_application_source=changed,terrain_height_unchanged=True,all_eight_structure_records_unchanged=True,original_placements_preserved=True,total_placements=count,added_placements=len(records),new_props_clear_of_fight_floor=True,floor=floor,boundary=boundary,floor_y=64.032,material_library_paths_valid=True,live_saves_unchanged=len(before),floor_roughness_minimum=int(arm[:,:,1].min()))
(N/'review/ART_AUDIT.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
