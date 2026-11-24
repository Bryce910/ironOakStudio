#!/usr/bin/env python3
"""Private animation-only derivative of four existing owned live rigs."""
from pathlib import Path
import copy,hashlib,json,shlex,struct
N=Path(__file__).resolve().parents[1]
BASE=N.parents[1]/'build/cinematic-studio-2026-11-15/equipment-assets'
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def read(p):
 raw=Path(p).read_bytes();magic,version,length=struct.unpack_from('<III',raw)
 assert magic==0x46546c67 and version==2 and length==len(raw)
 size,kind=struct.unpack_from('<II',raw,12);assert kind==0x4e4f534a
 document=json.loads(raw[20:20+size]);bsize,bkind=struct.unpack_from('<II',raw,20+size);assert bkind==0x004e4942
 return document,raw[28+size:28+size+bsize]
def write(p,g,b):
 g['buffers'][0]['byteLength']=len(b);j=json.dumps(g,separators=(',',':')).encode();j+=b' '*(-len(j)%4);b+=b'\0'*(-len(b)%4)
 p.write_bytes(struct.pack('<III',0x46546c67,2,28+len(j)+len(b))+struct.pack('<II',len(j),0x4e4f534a)+j+struct.pack('<II',len(b),0x004e4942)+b)
source=N/'assets/story_hero/story_clips/story_clips.compact.glb';g,b=read(source)
names=['Story_SwimIdle','Story_SwimForward','Story_Death']
assert [a['name']for a in g['animations']]==names
inputs={str(Path(__file__)):sha(__file__),str(source):sha(source)}
source_receipt=source.with_name('STORY_COMPACT_CLIPS_REVIEW.json');r=json.loads(source_receipt.read_text());assert r['passed'];inputs[str(source_receipt)]=sha(source_receipt)
for p,h in r.get('inputs',{}).items():assert sha(p)==h;inputs[p]=h
rigs=[('sword',Path(json.loads((BASE/'HERO_ASSET.json').read_text())['path']),None)]
for line in(BASE/'loadouts.manifest').read_text().splitlines()[1:]:
 fields=shlex.split(line)
 if fields:rigs.append(({'2':'sword_shield','3':'great_axe','4':'staff'}[fields[0]],Path(fields[1]),fields[2]))
out=N/'assets/story_hero/rigs';out.mkdir(parents=True,exist_ok=True);rows=[]
for name,path,prefix in rigs:
 old,raw=read(path);doc=copy.deepcopy(old);binary=bytearray(raw);inputs[str(path)]=sha(path)
 assert [n.get('name')for n in doc['nodes']]==[n.get('name')for n in g['nodes']]
 # Axes/staves have their own fixed source edge socket; their visible gear is
 # hand_r-bound. Do not import a Sword-specific animated edge socket into them.
 weapon=next(i for i,n in enumerate(g['nodes'])if n.get('name')=='valwood_weapon_r')
 for i,node in enumerate(doc['nodes']):
  if i==weapon and name in ['great_axe','staff']:continue
  for key in ['translation','rotation','scale','matrix','children']:assert node.get(key)==g['nodes'][i].get(key),(name,i,key)
 assert not set(a['name']for a in doc['animations']).intersection(names)
 binary.extend(b'\0'*(-len(binary)%4));offset=len(binary);binary.extend(b)
 view_offset=len(doc['bufferViews']);accessor_offset=len(doc['accessors'])
 for view in g['bufferViews']:
  value=copy.deepcopy(view);assert value.get('buffer',0)==0;value['byteOffset']=value.get('byteOffset',0)+offset;doc['bufferViews'].append(value)
 for accessor in g['accessors']:
  value=copy.deepcopy(accessor);assert 'sparse'not in value;value['bufferView']+=view_offset;doc['accessors'].append(value)
 for clip in g['animations']:
  value=copy.deepcopy(clip)
  if name in ['great_axe','staff']:value['channels']=[c for c in value['channels']if c['target']['node']!=weapon]
  for sampler in value['samplers']:sampler['input']+=accessor_offset;sampler['output']+=accessor_offset
  doc['animations'].append(value)
 assert len(doc['animations'])<=96
 target=out/(name+'.glb');write(target,doc,binary)
 final,fb=read(target)
 assert all(final.get(k)==old.get(k)for k in ['nodes','skins','meshes','materials','images','textures','samplers','scenes','scene'])
 assert final['animations'][:len(old['animations'])]==old['animations'] and fb[:len(raw)]==raw
 rows.append(dict(name=name,path=str(target),sha256=sha(target),original=str(path),original_sha256=sha(path),existing_clips=len(old['animations']),total_clips=len(final['animations']),original_binary_prefix_exact=True,geometry_materials_bind_and_existing_clips_exact=True,weapon_socket_policy='original own fixed edge rest'if name in ['great_axe','staff']else'actual source animated Sword attachment'))
manifest=out/'loadouts.manifest';manifest.write_text('VALWOOD_SKELETAL_LOADOUTS 1\n'+''.join(f'{i+2} "{rows[i+1]["path"]}" "{prefix}"\n'for i,prefix in enumerate(['Shield_','Axe_','Staff_'])))
hero=out/'HERO_ASSET.json';hero.write_text(json.dumps(dict(schema='valwood.story-hero-rig.v1',path=rows[0]['path'],sha256=rows[0]['sha256'],existing_animations_unchanged=True,added_clips=names),indent=2)+'\n')
report=dict(schema='valwood.story-swim-death.animation-only-derivative.v1',passed=True,inputs=inputs,rigs=rows,outputs={r['path']:r['sha256']for r in rows}|{str(manifest):sha(manifest),str(hero):sha(hero)},source_capture_is_owned_swim_and_death=True,native_validation_pending=True)
(N/'review/STORY_RIG_DERIVATIVES.json').write_text(json.dumps(report,indent=2,sort_keys=True)+'\n')
print(json.dumps({'passed':True,'rigs':len(rows),'manifest':str(manifest)}))
