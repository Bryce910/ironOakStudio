#!/usr/bin/env python3
"""CPU-only alias/LOD/dependency audit for the repaired production story scene."""
from pathlib import Path
import collections,hashlib,json,re,shlex,struct
N=Path(__file__).resolve().parent;P=N/'story-project';S=N/'source-v1';R=N/'review'
def sha(p):
 with Path(p).open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def must(v,m):
 if not v:raise RuntimeError(m)
def main():
 inputs={str(Path(__file__).resolve()):sha(__file__)}
 source=S/'engine/world_scenery.cpp';inputs[str(source)]=sha(source)
 block=source.read_text().split('constexpr Alias builtins[] = {',1)[1].split('\n};',1)[0]
 aliases={}
 for row in block.splitlines():
  row=row.strip()
  if not row.startswith('{"'):continue
  a=[v.strip() for v in row.strip('{} ,').split(',')]
  alias,stem=json.loads(a[0]),json.loads(a[1]);tree=a[2]=='true';lods=int(a[6]);named=len(a)>13 and a[13]=='true';cast=len(a)>12 and a[12]=='true'
  aliases[alias]={'stem':stem,'tree':tree,'lods':lods,'named':named,'cast':cast}
 doc=P/'world.studio';inputs[str(doc)]=sha(doc);custom=0;allrefs=set()
 for line in doc.read_text().splitlines():
  a=shlex.split(line)
  if not a:continue
  if a[0]=='asset':
   custom+=1;alias,stem,lods,collision,vegetation=a[1:6];v=aliases.get(alias,{})
   tree=int(vegetation)==2
   aliases[alias]={'stem':stem,'lods':int(lods),'tree':tree,'named':tree if not v else v['named'],'cast':int(lods)>1 and alias.startswith('nm_') if not v else v['cast']}
  if a[0] in {'prop','material','spline'}:allrefs.add(a[1])
 must(custom==62 and custom<=128,'Runtime custom asset budget/parity changed')
 counts=collections.Counter();files=[]
 for path in sorted((P/'scenery').glob('*_instances.txt')):
  inputs[str(path)]=sha(path);n=0
  for lineno,row in enumerate(path.read_text().splitlines(),1):
   a=row.split('#',1)[0].split()
   if not a:continue
   must(a[0] in aliases,f'Unknown scenery alias {path.name}:{lineno}:{a[0]}')
   must(len(row.encode())<=320,'Placement record exceeds runtimebytebudget');counts[a[0]]+=1;n+=1
  files.append({'path':str(path),'placements':n})
 must(sum(counts.values())<=8192 and max(counts.values())<=4096,'Scene instance count exceeds production budget')
 allrefs|=counts.keys();models=set();external=set()
 for alias in sorted(allrefs):
  must(alias in aliases,'Unregistered authored document alias: '+alias)
  a=aliases[alias];stem=P/'models'/a['stem']
  suffix=['_near.glb','_mid.glb','_far.glb'] if a['tree'] else [('_near.glb' if a['named'] else '.glb'),'_mid.glb']
  paths=[Path(str(stem)+suffix[lod]) for lod in range(a['lods'])]
  if a['tree'] or a['cast']:paths.append(Path(str(stem)+'_mid.glb'))
  for path in paths:
   must(path.is_file() and not path.is_symlink(),'Missing independent model/LOD: '+str(path));models.add(path)
 for path in sorted(models):
  with path.open('rb') as f:
   head=f.read(20);must(len(head)==20,'Truncated GLB')
   magic,version,total,jlen,jtype=struct.unpack('<4sIIII',head)
   must(magic==b'glTF' and version==2 and total==path.stat().st_size and jtype==0x4e4f534a,'Invalid GLB framing: '+str(path))
   g=json.loads(f.read(jlen))
  must(g.get('meshes') and g.get('accessors') and g.get('bufferViews'),'GLB has no supporting geometry: '+str(path))
  for item in g.get('buffers',[])+g.get('images',[]):
   uri=item.get('uri','')
   if not uri or uri.startswith('data:'):continue
   dependency=(path.parent/uri).resolve();must(dependency.is_relative_to(P) and dependency.is_file(),'Missing/unsafe GLB dependency: '+str(dependency));external.add(dependency)
 for path in models|external:inputs[str(path)]=sha(path)
 # Every collision catalog part/model must exist, including unused authored districts.
 catalog=P/'collision/structures/catalog';inputs[str(catalog)]=sha(catalog);rows=catalog.read_text().splitlines();must(rows[0]=='VALWOOD_STRUCTURE_COLLISION 1','Unsupported collision catalog')
 collision_models=set();parts=set();revisions=set();catalog_aliases=set()
 for row in rows[1:]:
  if not row or row.startswith('#'):continue
  a=shlex.split(row);kind=a[0];catalog_aliases.add(a[1])
  if kind=='model':revisions.add(a[1]);continue
  must(kind in {'mesh','none'},'Unknown collision binding')
  model=P/a[2];must(model.is_file(),'Missing collision source model');collision_models.add(model)
  if kind=='mesh':
   
   if a[1] in allrefs:
    expected=P/'models'/(aliases[a[1]]['stem']+('_near.glb' if aliases[a[1]]['tree'] or aliases[a[1]]['named'] else '.glb'))
    must(expected==model,'Collision/source alias model mismatch: '+a[1]+' '+str(expected)+' != '+str(model))
   part=P/a[3];must(part.is_file(),'Missing collision part');parts.add(part)
 for path in collision_models|parts:inputs[str(path)]=sha(path)
 must(parts and revisions,'No structural support geometry/revision evidence')
 # Preserve the source fire that the publisher-scene flag intentionally enables.
 flames=P/'showcase.flames';inputs[str(flames)]=sha(flames)
 must((P/'publisher_showcase.json').is_file() and (P/'city_showcase.json').is_file(),'Known accepted source-scene flags missing')
 must('-11988' in flames.read_text() and '5172' in flames.read_text(),'Arrival source flame missing')
 for p in [P/'city_showcase.json',P/'publisher_showcase.json',P/'opening/home.physics',P/'ground/showcase.height',R/'STORY_PROJECT_REPAIR_REVIEW.json']:inputs[str(p)]=sha(p)
 repair=json.loads((R/'STORY_PROJECT_REPAIR_REVIEW.json').read_text())
 for p,h in repair['canonical_metadata_untouched'].items():must(sha(p)==h,'Canonical source metadata changed')
 receipt={'schema':'valwood.first-contact.story-scene-cpu-review.v1','passed':True,'custom_assets':custom,'runtime_custom_asset_limit':128,'placed_aliases':len(counts),'placements':sum(counts.values()),'source_files':files,'validated_model_lod_files':len(models),'validated_external_model_dependencies':len(external),'collision_models':len(collision_models),'collision_parts':len(parts),'collision_revision_bindings':len(revisions),'source_fire_present':True,'source_scene_flags_retained':True,'canonical_metadata_unchanged':True,'inputs':inputs,'outputs':{},'limits':['CPU scene dependency and GLB framing checks only. Actual model upload, source-collision capture and rendering require native startup. Collision binary geometry/checksum validation is independently covered by production C++ fixture.']}
 out=R/'STORY_SCENE_CPU_REVIEW.json';out.write_text(json.dumps(receipt,indent=2,sort_keys=True)+'\n');print('PASS',out, {k:receipt[k] for k in ['custom_assets','placements','validated_model_lod_files','collision_parts']})
if __name__=='__main__':main()
