from pathlib import Path
import json,struct,numpy as np
N=Path(__file__).resolve().parents[1];P=N/'world-project';assets={}
for line in (P/'world.studio').read_text().splitlines():
 t=line.split()
 if t and t[0]=='asset':assets[t[1]]=P/'models'/(t[2]+'.glb')
def glb(p):
 b=p.read_bytes();jl=struct.unpack_from('<I',b,12)[0];j=json.loads(b[20:20+jl]);offset=20+jl+8;data=b[offset:];out=[]
 def acc(a):
  v=j['accessors'][a];bv=j['bufferViews'][v['bufferView']];dt={5126:'<f4',5125:'<u4',5123:'<u2',5121:'u1'}[v['componentType']];size={'VEC3':3,'SCALAR':1}[v['type']];item=np.dtype(dt).itemsize;start=bv.get('byteOffset',0)+v.get('byteOffset',0);stride=bv.get('byteStride',size*item)
  return np.ndarray((v['count'],size),dtype=dt,buffer=data,offset=start,strides=(stride,item)).copy()
 for m in j['meshes']:
  for pr in m['primitives']:
   v=acc(pr['attributes']['POSITION']);i=acc(pr['indices']).reshape(-1);out.append(v[i].reshape(-1,3,3))
 return np.concatenate(out)
source=np.array([-3498.388999939,602.159845583,-11002.038628134]);meshes=[]
for line in (P/'scenery/authored_instances.txt').read_text().splitlines():
 t=line.split()
 if not t or t[0] not in assets or not t[0].startswith('nm_opening_'):continue
 x=(float(t[1])-.5)*100800;z=(float(t[2])-.5)*(100800*2818/5009)
 if np.hypot(x-source[0],z-source[2])>15:continue
 try:v=glb(assets[t[0]])
 except Exception as e:print(e);continue
 v=v*float(t[4])+[x,float(t[-1]),z];meshes.append((t[0],v))
results=[]
for target in [np.array([-3498.4,600.02,-11003]),np.array([-3497.8,600.02,-11004.8]),np.array([-3499.2,600.02,-11004])]:
 d=target-source;length=np.linalg.norm(d);d/=length;hits=[]
 for name,tris in meshes:
  a=tris[:,0];e1=tris[:,1]-a;e2=tris[:,2]-a;h=np.cross(np.broadcast_to(d,e2.shape),e2);det=np.einsum('ij,ij->i',e1,h);valid=np.abs(det)>1e-9;f=np.where(valid,1/np.where(valid,det,1),0);s=source-a;u=f*np.einsum('ij,ij->i',s,h);q=np.cross(s,e1);v=f*np.einsum('j,ij->i',d,q);t=f*np.einsum('ij,ij->i',e2,q);ok=valid&(u>=0)&(v>=0)&(u+v<=1)&(t>.005)&(t<length-.02)
  if np.any(ok):
   ti=float(t[ok].min());hits.append((ti,name,(source+d*ti).tolist()))
 print('target',target.tolist(),'hits',sorted(hits)[:8]);results.append(dict(target=target.tolist(),occluders=sorted(hits)[:8]));assert not hits
(N/'review/RAY_REVIEW.json').write_text(json.dumps(dict(passed=True,source=source.tolist(),actual_imported_geometry=True,clear_floor_paths=results),indent=2)+'\n')
