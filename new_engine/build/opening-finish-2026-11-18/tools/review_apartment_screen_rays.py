from pathlib import Path
import json,struct,numpy as np
N=Path(__file__).resolve().parents[1];P=N/'world-project';R=N/'review';assets={}
for l in (P/'world.studio').read_text().splitlines():
 t=l.split()
 if t and t[0]=='asset':assets[t[1]]=P/'models'/(t[2]+'.glb')
def tris(path,origin):
 b=path.read_bytes();jl=struct.unpack_from('<I',b,12)[0];j=json.loads(b[20:20+jl]);data=b[28+jl:]
 def acc(a):
  v=j['accessors'][a];bv=j['bufferViews'][v['bufferView']];dt={5126:'<f4',5125:'<u4',5123:'<u2',5121:'u1'}[v['componentType']];n={'VEC3':3,'SCALAR':1}[v['type']];item=np.dtype(dt).itemsize
  return np.ndarray((v['count'],n),dtype=dt,buffer=data,offset=bv.get('byteOffset',0)+v.get('byteOffset',0),strides=(bv.get('byteStride',n*item),item)).copy()
 out=[]
 for m in j['meshes']:
  for p in m['primitives']:
   v=acc(p['attributes']['POSITION']);idx=acc(p['indices']).reshape(-1);out.append(v[idx].reshape(-1,3,3)+origin)
 return np.concatenate(out)
meshes=[]
for l in (P/'scenery/authored_instances.txt').read_text().splitlines():
 t=l.split()
 if not t or t[0] not in assets or not t[0].startswith('nm_opening_'):continue
 o=np.array([(float(t[1])-.5)*100800,float(t[-1]),(float(t[2])-.5)*(100800*2818/5009)])
 if np.hypot(o[0]+3498.6,o[2]+11003.7)>8:continue
 meshes.append((t[0],tris(assets[t[0]],o)))
points=[[-3497.8,601.35,-11003.8],[-3497.9,601.3,-11003.65],[-3498.5,600.81557944417,-11003.74835331055],[-3499.17,600.81557944417,-11003.74835331055]]
target=np.array([-3499.3895729055403,600.81557944417,-11003.74835331055]);results=[]
rays=[(p,target) for p in points]
bill=np.array(json.loads((R/'APARTMENT_SCREEN_GEOMETRY.json').read_text())['bill_support']['bill_point'])
shot=json.loads((P/'opening/cinematic.json').read_text())['shots'][1]
rays.extend([(shot[key],bill) for key in ('eye_from','eye_to')])
for origin,target in rays:
 origin=np.array(origin);d=target-origin;length=np.linalg.norm(d);d/=length;hits=[]
 for name,v in meshes:
  a=v[:,0];e1=v[:,1]-a;e2=v[:,2]-a;h=np.cross(np.broadcast_to(d,e2.shape),e2);det=np.einsum('ij,ij->i',e1,h);valid=np.abs(det)>1e-9;f=np.where(valid,1/np.where(valid,det,1),0);s=origin-a;u=f*np.einsum('ij,ij->i',s,h);q=np.cross(s,e1);vv=f*np.einsum('j,ij->i',d,q);dist=f*np.einsum('ij,ij->i',e2,q);ok=valid&(u>=0)&(vv>=0)&(u+vv<=1)&(dist>.005)&(dist<length-.003)
  if np.any(ok):hits.append([float(dist[ok].min()),name])
 hits.sort();results.append(dict(origin=origin.tolist(),target=target.tolist(),occluders=hits[:8]));print(results[-1])
r=dict(passed=all(not v['occluders'] for v in results),actual_imported_geometry=True,mesh_count=len(meshes),screen_centrelines=results)
(R/'APARTMENT_SCREEN_RAYS.json').write_text(json.dumps(r,indent=2)+'\n');assert r['passed']
