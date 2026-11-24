#!/usr/bin/env python3
"""Read-only topology/material audit of the76active native village NPC poses."""
from pathlib import Path
import hashlib,json,math,re,struct
import numpy as np
ROOT=Path(__file__).resolve().parents[1];MODELS=ROOT/'projects/valwood_world/models'
ACTIVE=[('village_character','knight',8,4),('village_character','archer',0,4),('purchased_character','trader',0,4),('purchased_character','traveler',8,4),('village_character','traderwoman',0,4),('village_character','travelerwoman',8,4),('purchased_character','merchant',0,4),('purchased_character','craftsman',8,4),('purchased_character','wayfarer',8,4)]

def digest(data):return hashlib.sha256(data).hexdigest()
def glb(path):
 raw=path.read_bytes();length,kind=struct.unpack_from('<II',raw,12);assert kind==0x4e4f534a
 doc=json.loads(raw[20:20+length]);n,k=struct.unpack_from('<II',raw,20+length);assert k==0x004e4942
 return doc,raw[28+length:28+length+n]
def values(doc,binary,index):
 a=doc['accessors'][index];v=doc['bufferViews'][a['bufferView']];assert not a.get('sparse')
 dtype=np.dtype({5126:'<f4',5125:'<u4',5123:'<u2',5122:'<i2',5121:'u1',5120:'i1'}[a['componentType']]);width={'SCALAR':1,'VEC2':2,'VEC3':3,'VEC4':4}[a['type']]
 x=np.ndarray((a['count'],width),dtype=dtype,buffer=binary,offset=v.get('byteOffset',0)+a.get('byteOffset',0),strides=(v.get('byteStride',dtype.itemsize*width),dtype.itemsize)).copy()
 if a.get('normalized'):x=x.astype(np.float64)/np.iinfo(dtype).max
 return x

def pose(path):
 doc,binary=glb(path);imagehash=[]
 for im in doc['images']:
  v=doc['bufferViews'][im['bufferView']];off=v.get('byteOffset',0);imagehash.append(digest(binary[off:off+v['byteLength']]))
 def view(v):
  if not v:return None
  t=doc['textures'][v['index']];sampler=doc.get('samplers',[])[t['sampler']] if 'sampler' in t else {}
  return {'image':imagehash[t['source']],'sampler':sampler,'parameters':{k:x for k,x in v.items() if k!='index'}}
 materials=[]
 for m in doc['materials']:
  p=m.get('pbrMetallicRoughness',{});materials.append(dict(base=view(p.get('baseColorTexture')),normal=view(m.get('normalTexture')),arm=view(p.get('metallicRoughnessTexture')),factors={k:v for k,v in p.items() if not k.endswith('Texture')},alphaMode=m.get('alphaMode','OPAQUE'),alphaCutoff=m.get('alphaCutoff',.5),doubleSided=m.get('doubleSided',False)))
 parents={c:i for i,n in enumerate(doc['nodes']) for c in n.get('children',[])}
 def matrix(i):
  n=doc['nodes'][i]
  if 'matrix' in n:local=np.asarray(n['matrix']).reshape(4,4).T
  else:
   x,y,z,w=n.get('rotation',[0,0,0,1]);local=np.eye(4);local[:3,:3]=np.asarray([[1-2*(y*y+z*z),2*(x*y-z*w),2*(x*z+y*w)],[2*(x*y+z*w),1-2*(x*x+z*z),2*(y*z-x*w)],[2*(x*z-y*w),2*(y*z+x*w),1-2*(x*x+y*y)]])@np.diag(n.get('scale',[1,1,1]));local[:3,3]=n.get('translation',[0,0,0])
  return matrix(parents[i])@local if i in parents else local
 records=[]
 for ni,node in enumerate(doc['nodes']):
  if 'mesh' not in node:continue
  transform=matrix(ni)
  for p in doc['meshes'][node['mesh']]['primitives']:
   a=p['attributes'];xyz=values(doc,binary,a['POSITION']);world=xyz@transform[:3,:3].T+transform[:3,3]
   uv=values(doc,binary,a['TEXCOORD_0']);index=values(doc,binary,p['indices']);normal=values(doc,binary,a['NORMAL']) if 'NORMAL' in a else None;tangent=values(doc,binary,a['TANGENT']) if 'TANGENT' in a else None
   signature=dict(vertices=len(xyz),indices=index.size,index_sha=digest(index.tobytes()),uv_sha=digest(uv.tobytes()),normal_present=normal is not None,tangent_present=tangent is not None,material=materials[p['material']])
   records.append(dict(signature=signature,world=world,normal=normal,tangent=tangent))
 low=min(float(p['world'][:,1].min()) for p in records);high=max(float(p['world'][:,1].max()) for p in records)
 for p in records:p['world'][:,1]-=low
 return dict(file=path.name,records=records,raw_min_y=low,height=high-low)

def main():
 manifests={prefix:{r['file']:r for r in json.loads((MODELS/('village_character_manifest.json' if prefix=='village_character' else 'purchased_character_manifest.json')).read_text())['assets']} for prefix in {p for p,_,_,_ in ACTIVE}}
 report=[];count=0
 for prefix,name,walk,idle in ACTIVE:
  groups={mode:[pose(MODELS/f'{prefix}_{name}_{mode}_{i:02}.glb') for i in range(n)] for mode,n in [('walk',walk),('idle',idle)] if n}
  allposes=sum(groups.values(),[]);base=allposes[0];mismatches=[];pairs=[];tangent_sign_mismatches=0
  for frame in allposes:
   if len(frame['records'])!=len(base['records']):mismatches.append(dict(file=frame['file'],field='primitive_count'));continue
   for j,(a,b) in enumerate(zip(base['records'],frame['records'])):
    for field in a['signature']:
     if a['signature'][field]!=b['signature'][field]:mismatches.append(dict(file=frame['file'],primitive=j,field=field))
    if a['tangent'] is not None and a['tangent'].shape==b['tangent'].shape:tangent_sign_mismatches+=int(np.sum(a['tangent'][:,3]!=b['tangent'][:,3]))
  for mode,poses in groups.items():
   for a,b in zip(poses,poses[1:]+poses[:1]):
    safe=len(a['records'])==len(b['records']) and all(x['signature']==y['signature'] for x,y in zip(a['records'],b['records']))
    row=dict(mode=mode,from_pose=a['file'],to_pose=b['file'],direct_vertex_pair_safe=safe)
    if safe:
     displacement=np.concatenate([np.linalg.norm(x['world']-y['world'],axis=1) for x,y in zip(a['records'],b['records'])]);row.update(position_rms_m=float(np.sqrt(np.mean(displacement**2))),position_max_m=float(displacement.max()))
    pairs.append(row)
  metadata=[manifests[prefix][p['file']] for p in allposes]
  record=dict(appearance=name,prefix=prefix,poses=len(allposes),walk_poses=walk,idle_poses=idle,primitive_counts=sorted({len(p['records']) for p in allposes}),vertex_counts=[p['signature']['vertices'] for p in base['records']],triangles=sum(p['signature']['indices']//3 for p in base['records']),all_pose_direct_correspondence=not mismatches,mismatch_count=len(mismatches),mismatch_examples=mismatches[:20],tangent_handedness_changes=tangent_sign_mismatches,tangents_present=all(p['signature']['tangent_present'] for p in base['records']),raw_min_y_range_m=[min(p['raw_min_y'] for p in allposes),max(p['raw_min_y'] for p in allposes)],raw_min_y_span_m=max(p['raw_min_y'] for p in allposes)-min(p['raw_min_y'] for p in allposes),height_range_m=[min(p['height'] for p in allposes),max(p['height'] for p in allposes)],foot_min_range_m=[min(p.get('min_foot_m',0) for p in metadata),max(p.get('min_foot_m',0) for p in metadata)],source_frames=[p.get('source_frame') for p in metadata if p['mode']=='walk'],source_fps=sorted({p.get('source_fps') for p in metadata if p.get('source_fps')}),pair_count=len(pairs),safe_pairs=sum(p['direct_vertex_pair_safe'] for p in pairs),pairs=pairs)
  report.append(record);count+=len(allposes)
  print(name,record['poses'],'poses',record['primitive_counts'],'primitives',record['vertex_counts'],'vertices','safe',record['safe_pairs'],'/',len(pairs),'pairs; all-pose topology',not mismatches,'ground span',record['raw_min_y_span_m'],'mismatch',record['mismatch_examples'][:2],flush=True)
 assert count==76
 result=dict(schema=1,active_appearances=9,active_pose_count=count,all_current_geometry_read_only=True,records=report)
 dest=ROOT/'assets/characters/pose-compatibility-audit.json';dest.write_text(json.dumps(result,indent=2)+'\n')
 print('AUDIT',count,'poses;',sum(r['safe_pairs'] for r in report),'/',sum(r['pair_count'] for r in report),'adjacent pairs direct-safe',flush=True)
if __name__=='__main__':main()
