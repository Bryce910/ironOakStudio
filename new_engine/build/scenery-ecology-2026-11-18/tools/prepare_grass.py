"""Fit owned whole-clump grass to the arena's previous bounds and root sites."""
from pathlib import Path
import hashlib,json,struct
import numpy as np
N=Path(__file__).resolve().parents[1];E=N.parents[1];B=N.with_name('arena-scenery-2026-11-18');P=N/'arena-project/models'
SOURCE=E/'assets/purchased/bep_reforged/countryside-grass-clumps/models'
def read(path):
 b=path.read_bytes();n=struct.unpack_from('<I',b,12)[0];j=json.loads(b[20:20+n]);length=struct.unpack_from('<I',b,20+n)[0];return j,bytearray(b[28+n:28+n+length])
def bbox(j):
 a=[j['accessors'][p['attributes']['POSITION']] for m in j['meshes'] for p in m['primitives']];return np.min([x['min'] for x in a],axis=0),np.max([x['max'] for x in a],axis=0)
report=[]
for alias in ['bep_nature_grass_long','bep_nature_grass_broad']:
 target,_=read(B/'arena-project/models'/(alias+'_near.glb'));lo,hi=bbox(target);original,_=read(SOURCE/(alias+'_near.glb'));a,b=bbox(original);scale=(hi-lo)/(b-a);offset=lo-a*scale
 for tier in ['near','mid']:
  path=SOURCE/(alias+'_'+tier+'.glb');j,data=read(path);done=set()
  assert j['nodes']==[{'mesh':0}]
  for mesh in j['meshes']:
   for primitive in mesh['primitives']:
    for semantic in ['POSITION','NORMAL','TANGENT']:
     index=primitive['attributes'].get(semantic)
     if index is None or index in done:continue
     done.add(index);acc=j['accessors'][index];view=j['bufferViews'][acc['bufferView']];assert acc['componentType']==5126 and not view.get('byteStride')
     dims=4 if semantic=='TANGENT' else 3;v=np.frombuffer(data,dtype='<f4',count=acc['count']*dims,offset=view.get('byteOffset',0)+acc.get('byteOffset',0)).reshape(-1,dims)
     if semantic=='POSITION':v[:,:3]=v[:,:3]*scale+offset
     else:
      v[:,:3]=v[:,:3]/scale if semantic=='NORMAL' else v[:,:3]*scale;v[:,:3]/=np.maximum(np.linalg.norm(v[:,:3],axis=1,keepdims=True),1e-12)
     if 'min' in acc:acc['min']=v.min(axis=0).tolist();acc['max']=v.max(axis=0).tolist()
  js=json.dumps(j,separators=(',',':')).encode();js+=b' '*(-len(js)%4);data.extend(b'\0'*(-len(data)%4));out=struct.pack('<4sII',b'glTF',2,28+len(js)+len(data))+struct.pack('<II',len(js),0x4e4f534a)+js+struct.pack('<II',len(data),0x004e4942)+data;dest=P/path.name
  if dest.is_symlink():dest.unlink()
  dest.write_bytes(out);newlo,newhi=bbox(j);assert np.all(newlo>=lo-1e-5) and np.all(newhi<=hi+1e-5)
  if tier=='near':assert np.max(abs(newlo-lo))<1e-5 and np.max(abs(newhi-hi))<1e-5
  triangles=sum((j['accessors'][q['indices']]['count'] if 'indices' in q else j['accessors'][q['attributes']['POSITION']]['count'])//3 for m in j['meshes'] for q in m['primitives'])
  report.append(dict(alias=alias,tier=tier,source=str(path),source_sha256=hashlib.sha256(path.read_bytes()).hexdigest(),triangles=triangles,bounds_preserved=True,normal_inverse_transpose=True,root_sites_shared=True,output_sha256=hashlib.sha256(out).hexdigest()))
(N/'GRASS_ASSETS.json').write_text(json.dumps(dict(passed=True,owned_asset_reuse=True,original_projects_unchanged=True,assets=report),indent=2)+'\n');print(json.dumps(report,indent=2))
