#!/usr/bin/env python3
"""Deterministically skin the supplied Meshy bandit's REAL walk/run clips offline.
No GPU, Blender, network, source scripts or per-frame skeletal CPU work is used.
python3 tools/import_tanthoon_bandits.py --prepare
python3 tools/import_tanthoon_bandits.py --cook
python3 tools/import_tanthoon_bandits.py --validate
"""
from pathlib import Path,PurePosixPath
import argparse,hashlib,io,json,math,os,stat,struct,tempfile,zipfile
import numpy as np
from PIL import Image
from import_wildmesh_farm import tangents,read_glb,inspect
ROOT=Path(__file__).resolve().parents[1]
ASSET=ROOT/'assets/purchased/tanthoon_bandits';SOURCE=ASSET/'source';DERIVED=ASSET/'derived'
OUT=ROOT/'projects/valwood_world/models';MANIFEST='tanthoon_bandit_manifest.json'
ARCHIVE=Path('/home/computer/Downloads/Meshy_AI_Bandit_Ranger_biped.zip')
PREFIX='Meshy_AI_Bandit_Ranger_biped_';COUNTS={'walk':16,'run':16,'stance':8}

def sha(p):
 h=hashlib.sha256()
 with Path(p).open('rb')as f:
  for b in iter(lambda:f.read(1048576),b''):h.update(b)
 return h.hexdigest()
def publish(p,b):
 p=Path(p);p.parent.mkdir(parents=True,exist_ok=True)
 if p.exists()and p.read_bytes()==b:return
 q=p.with_suffix(p.suffix+'.pending');q.write_bytes(b);os.replace(q,p)
def jsave(p,d):publish(p,(json.dumps(d,indent=2)+'\n').encode())

def quat_matrix(q):
 x,y,z,w=q/np.linalg.norm(q)
 return np.array([[1-2*(y*y+z*z),2*(x*y-z*w),2*(x*z+y*w)],
 [2*(x*y+z*w),1-2*(x*x+z*z),2*(y*z-x*w)],
 [2*(x*z-y*w),2*(y*z+x*w),1-2*(x*x+y*y)]])
def slerp(a,b,t):
 a=a/np.linalg.norm(a);b=b/np.linalg.norm(b);dot=np.dot(a,b)
 if dot<0:b=-b;dot=-dot
 if dot>.9995:q=a+(b-a)*t;return q/np.linalg.norm(q)
 angle=math.acos(np.clip(dot,-1,1));return (a*math.sin((1-t)*angle)+b*math.sin(t*angle))/math.sin(angle)

class Source:
 def __init__(self,path):
  self.path=Path(path);raw=self.path.read_bytes();magic,version,size=struct.unpack_from('<III',raw);assert(magic,version,size)==(0x46546c67,2,len(raw))
  j,kind=struct.unpack_from('<II',raw,12);assert kind==0x4e4f534a;self.d=json.loads(raw[20:20+j]);self.blob=raw[28+j:]
  self.parents={c:i for i,n in enumerate(self.d['nodes'])for c in n.get('children',[])}
  self.primitive=self.d['meshes'][0]['primitives'][0];self.attrs={k:self.accessor(v)for k,v in self.primitive['attributes'].items()}
  self.indices=self.accessor(self.primitive['indices']).reshape(-1,3).astype(np.int64)
  self.skin=self.d['skins'][0];self.ibm=self.accessor(self.skin['inverseBindMatrices']).reshape(-1,4,4).transpose(0,2,1)
  self.root=next(i for i,n in enumerate(self.d['nodes'])if n.get('name')=='Hips')
  self.animations={a['name']:a for a in self.d.get('animations',[])}
 def accessor(self,i):
  a=self.d['accessors'][i];assert 'sparse'not in a;v=self.d['bufferViews'][a['bufferView']]
  dt={5126:'<f4',5125:'<u4',5123:'<u2',5121:'u1'}[a['componentType']];n={'SCALAR':1,'VEC2':2,'VEC3':3,'VEC4':4,'MAT4':16}[a['type']]
  stride=v.get('byteStride',np.dtype(dt).itemsize*n)
  out=np.ndarray((a['count'],n),dtype=dt,buffer=self.blob,offset=v.get('byteOffset',0)+a.get('byteOffset',0),strides=(stride,np.dtype(dt).itemsize)).copy()
  if a.get('normalized'):out=out.astype(float)/np.iinfo(np.dtype(dt)).max
  return out
 def bounds(self,animation):
  a=self.animations[animation];times=[self.accessor(s['input'])for s in a['samplers']];return min(float(t.min())for t in times),max(float(t.max())for t in times)
 def evaluate(self,animation=None,time=0,breathing=0,neutral=False):
  nodes=self.d['nodes'];trs=[dict(translation=np.array(n.get('translation',[0,0,0]),float),rotation=np.array(n.get('rotation',[0,0,0,1]),float),scale=np.array(n.get('scale',[1,1,1]),float))for n in nodes]
  if animation:
   a=self.animations[animation]
   for channel in a['channels']:
    sampler=a['samplers'][channel['sampler']];times=self.accessor(sampler['input'])[:,0];values=self.accessor(sampler['output']);interpolation=sampler.get('interpolation','LINEAR');assert interpolation in('LINEAR','STEP')
    lo=max(0,min(len(times)-1,int(np.searchsorted(times,time,side='right'))-1));hi=min(len(times)-1,lo+1);w=0 if hi==lo else float(np.clip((time-times[lo])/(times[hi]-times[lo]),0,1))
    path=channel['target']['path'];value=values[lo]if interpolation=='STEP'else slerp(values[lo],values[hi],w)if path=='rotation'else values[lo]*(1-w)+values[hi]*w
    trs[channel['target']['node']][path]=value
  if breathing:
   # Explicit small authored skeleton breathing, not a claimed supplied action.
   i=next(i for i,n in enumerate(nodes)if n.get('name')=='Spine')
   trs[i]['scale']=trs[i]['scale']*np.array([1+breathing*.002,1+breathing*.003,1+breathing*.002])
  globals={}
  def world(i):
   if i not in globals:
    t=trs[i];m=np.eye(4);m[:3,:3]=quat_matrix(t['rotation'])@np.diag(t['scale']);m[:3,3]=t['translation']
    if 'matrix'in nodes[i]:m=np.asarray(nodes[i]['matrix']).reshape(4,4).T
    globals[i]=world(self.parents[i])@m if i in self.parents else m
    if neutral and nodes[i].get('name')in('LeftArm','RightArm'):
     angle=math.radians(-20 if nodes[i]['name']=='LeftArm'else 20);c,s=math.cos(angle),math.sin(angle)
     globals[i][:3,:3]=np.array([[c,-s,0],[s,c,0],[0,0,1]])@globals[i][:3,:3]
   return globals[i]
  matrices=np.array([world(i)@ib for i,ib in zip(self.skin['joints'],self.ibm)])
  weights=self.attrs['WEIGHTS_0'].astype(float);weights/=weights.sum(axis=1,keepdims=True);joints=self.attrs['JOINTS_0'].astype(int)
  p=np.column_stack([self.attrs['POSITION'],np.ones(len(weights))]);n=self.attrs['NORMAL'];positions=np.zeros((len(p),3));normals=np.zeros_like(positions)
  for k in range(4):
   m=matrices[joints[:,k]];positions+=np.einsum('nij,nj->ni',m,p)[:,:3]*weights[:,k,None]
   normal=np.linalg.inv(m[:,:3,:3]).transpose(0,2,1);normals+=np.einsum('nij,nj->ni',normal,n)*weights[:,k,None]
  normals/=np.maximum(np.linalg.norm(normals,axis=1)[:,None],1e-12)
  bones={nodes[i].get('name',str(i)):world(i)[:3,3].copy()for i in self.skin['joints']}
  return positions,normals,bones

def prepare():
 SOURCE.mkdir(parents=True,exist_ok=True);rows=[]
 with zipfile.ZipFile(ARCHIVE)as z:
  assert len(z.infolist())==2 and sum(i.file_size for i in z.infolist())<32*1048576
  for i in z.infolist():
   p=PurePosixPath(i.filename.replace('\\','/'));assert not p.is_absolute()and'..'not in p.parts and':'not in i.filename and not stat.S_ISLNK(i.external_attr>>16)and not i.flag_bits&1
   out=SOURCE/p.name;data=z.read(i)
   if out.exists():assert hashlib.sha256(data).hexdigest()==sha(out)
   else:publish(out,data)
   rows.append(dict(archive_member=i.filename,file=str(out.relative_to(ASSET)),bytes=len(data),sha256=sha(out)))
 source=Source(SOURCE/(PREFIX+'Meshy_AI_Meshy_Merged_Animations.glb'));v=source.d['bufferViews'][source.d['images'][1]['bufferView']]
 image=Image.open(io.BytesIO(source.blob[v.get('byteOffset',0):v.get('byteOffset',0)+v['byteLength']])).convert('RGB').resize((1024,1024),Image.Resampling.LANCZOS)
 b=io.BytesIO();image.save(b,format='PNG',compress_level=6);publish(DERIVED/'basecolor.png',b.getvalue())
 jsave(ASSET/'PROVENANCE.json',dict(source='User supplied Meshy bandit ranger biped archive',archive=str(ARCHIVE),archive_sha256=sha(ARCHIVE),source_files=rows,source_scripts_executed=False,originals_modified=False,
 license='User supplied asset; no separate license document in archive. No public redistribution grant asserted.',
 material='Original 2K color resized to shared1K RGB; alpha range131..255 is not transparency at .5 cutoff. OPAQUE,metallic0,roughness.82,emissive0. Source incorrectly connects color to emission and metallic defaults1; those defaults are not physical leather/cloth.',
 source_animations={name:dict(range=list(source.bounds(name)))for name in source.animations},
 atlas_sha256=sha(DERIVED/'basecolor.png'),atlas_side=1024,texture_bytes_rgba8_with_mips=1024*1024*4*4//3))
 print('PREPARED bandit source, genuine Walking/Running and one shared1K color atlas')

def write_glb(path,arrays,label):
 p,n,uv,t,idx=arrays;blob=bytearray();views=[];accessors=[]
 def add(raw,target=None):
  blob.extend(b'\0'*(-len(blob)%4));views.append(dict(buffer=0,byteOffset=len(blob),byteLength=len(raw),**({'target':target}if target else{})));blob.extend(raw);return len(views)-1
 def acc(a,kind,component,target,bounds=False):
  a=np.ascontiguousarray(a);accessors.append(dict(bufferView=add(a.tobytes(),target),componentType=component,count=len(a),type=kind,**(dict(min=a.min(axis=0).tolist(),max=a.max(axis=0).tolist())if bounds else{})));return len(accessors)-1
 attrs={k:acc(a.astype('<f4'),typ,5126,34962,k=='POSITION')for k,a,typ in [('POSITION',p,'VEC3'),('NORMAL',n,'VEC3'),('TEXCOORD_0',uv,'VEC2'),('TANGENT',t,'VEC4')]}
 indices=acc(idx.reshape(-1).astype('<u2'),'SCALAR',5123,34963);image=add((DERIVED/'basecolor.png').read_bytes())
 d=dict(asset=dict(version='2.0',generator='Valwood genuine supplied Meshy bandit offline skinning'),scene=0,scenes=[dict(nodes=[0])],nodes=[dict(name=label,mesh=0)],meshes=[dict(primitives=[dict(attributes=attrs,indices=indices,material=0,mode=4)])],
 materials=[dict(name='Bandit original shared cloth leather color',pbrMetallicRoughness=dict(baseColorFactor=[1,1,1,1],baseColorTexture=dict(index=0),metallicFactor=0,roughnessFactor=.82),emissiveFactor=[0,0,0],alphaMode='OPAQUE',doubleSided=True)],
 images=[dict(name='tanthoon_bandit_original_basecolor',mimeType='image/png',bufferView=image)],textures=[dict(sampler=0,source=0)],samplers=[dict(magFilter=9729,minFilter=9987,wrapS=10497,wrapT=10497)],buffers=[dict(byteLength=len(blob))],bufferViews=views,accessors=accessors,extras=dict(valwood_pose_contract_version=1))
 raw=json.dumps(d,separators=(',',':')).encode();raw+=b' '*(-len(raw)%4);blob+=b'\0'*(-len(blob)%4);publish(path,struct.pack('<III',0x46546c67,2,28+len(raw)+len(blob))+struct.pack('<II',len(raw),0x4e4f534a)+raw+struct.pack('<II',len(blob),0x004e4942)+blob)

def cook():
 source=Source(SOURCE/(PREFIX+'Meshy_AI_Meshy_Merged_Animations.glb'));static=Source(SOURCE/(PREFIX+'Character_output.glb'))
 uv=source.attrs['TEXCOORD_0'].copy();idx=source.indices.copy();families=[]
 for mode,count in COUNTS.items():
  # Source labels are retained verbatim in metadata: its "Running" is the
  # low-lift1.39m/s patrol gait; "Walking" is the high-knee4.88m/s fast gait.
  ref=static if mode=='stance'else source;action=next(iter(static.animations))if mode=='stance'else 'Running'if mode=='walk'else'Walking';start,end=ref.bounds(action);period=4.8 if mode=='stance'else end-start;poses=[];records=[];root0=None
  uv=ref.attrs['TEXCOORD_0'].copy();idx=ref.indices.copy();sign=None
  for i in range(count):
   p,n,bones=ref.evaluate(action,start if mode=='stance'else start+(end-start)*i/count,math.sin(i/count*2*math.pi)if mode=='stance'else 0,mode=='stance')
   if root0 is None:root0=bones['Hips'].copy()
   p[:,[0,2]]-=root0[[0,2]];p[:,1]-=p[:,1].min()
   t=tangents(p,n,uv,idx,sign)
   if sign is None:sign=t[:,3].copy()
   path=OUT/f'tanthoon_bandit_{mode}_{i:02}.glb';write_glb(path,(p,n,uv,t,idx),path.stem);records.append(inspect(path));poses.append(p.astype('<f4'))
  # LBS sourceglTF is already nativeYup/+Z; preserve actual1.7m scale.
  seam0=ref.evaluate(action,start)[0];seam1=ref.evaluate(action,end)[0];seam=np.linalg.norm(seam1-seam0,axis=1)
  corrections=[]
  for i in range(512):
   phase=i/512*count;j=int(phase);w=phase-j;corrections.append(float((poses[j]*(1-w)+poses[(j+1)%count]*w)[:,1].min()))
  contacts=[]
  for p in poses:
   contacts.extend(p[p[:,1]<=.035])
   for a,b in [(0,1),(1,2),(2,0)]:
    pa,pb=p[idx[:,a]],p[idx[:,b]];cross=(pa[:,1]<.035)&(pb[:,1]>.035)|(pb[:,1]<.035)&(pa[:,1]>.035)
    q=pa[cross];r=pb[cross];w=(.035-q[:,1])/(r[:,1]-q[:,1]);contacts.extend(q+(r-q)*w[:,None])
  foot=np.array(contacts);boundslo=np.min([p.min(axis=0)for p in poses],axis=0);boundshi=np.max([p.max(axis=0)for p in poses],axis=0)
  # Measure source stance backward footspeed, avoiding guessed gait speed.
  speeds=[]
  if mode!='stance':
   samples=[ref.evaluate(action,start+(end-start)*i/128)[2]for i in range(129)]
   for name in ['LeftFoot','RightFoot','LeftToeBase','RightToeBase']:
    a=np.array([b[name]-b['Hips'][[0,1,2]]*np.array([1,0,1])for b in samples]);v=np.diff(a[:,2])/(period/128);low=a[:-1,1]<=a[:,1].min()+.035
    speeds.extend((-v[(v<-.1)&low]).tolist())
  stride=float(np.median(speeds)*period)if speeds else 0
  families.append(dict(mode=mode,action=action,source_file=ref.path.name,source_period_seconds=end-start,cycle_seconds=period,source_time_range_seconds=[start,end],pose_count=count,assets=records,
    procedural_breathing=mode=='stance',stance_note='Supplied A stance with documented20degree upper-arm lowering to neutral and <=0.3% skeleton chest breathing; no labor/combat/idle clip claimed.'if mode=='stance'else None,
    stride_m=stride,bounds_min=boundslo.tolist(),bounds_max=boundshi.tolist(),
    endpoint_contact_half_x=float(np.max(np.abs(foot[:,0]))),endpoint_contact_half_z=float(np.max(np.abs(foot[:,2]))),
    foot_half_x=float(max(abs(boundslo[0]),abs(boundshi[0]))+.015),foot_half_z=float(max(abs(boundslo[2]),abs(boundshi[2]))+.015),
    support_envelope_note='Full source pose-union XZ projection +15mm conservatively encloses all linear pose-blend contact triangles, including newly grounded points after Y-only blend correction.',
    source_endpoint_max_m=float(seam.max()),blend_ground_correction_512=corrections,max_blend_ground_lift_m=max(corrections)))
 jsave(OUT/MANIFEST,dict(schema_version=1,pose_contract_version=1,source_provenance='assets/purchased/tanthoon_bandits/PROVENANCE.json',families=families,atlas_sha256=sha(DERIVED/'basecolor.png'),texture_bytes_rgba8_with_mips=1024*1024*4*4//3))
 validate();print('COOKED',[(f['mode'],f['stride_m'],f['bounds_min'],f['bounds_max'])for f in families])

def data():
 m=json.loads((OUT/MANIFEST).read_text());ox,oz=-20600.,8946.320622878819
 knots=np.array([[0,30],[-10,18],[-12,0],[-10,-18],[0,-24],[12,-18],[14,0],[12,18]],float);points=[]
 for i,p1 in enumerate(knots):
  p0,p2,p3=knots[(i-1)%len(knots)],knots[(i+1)%len(knots)],knots[(i+2)%len(knots)]
  for j in range(24):
   t=j/24;points.append(.5*((2*p1)+(-p0+p2)*t+(2*p0-5*p1+4*p2-p3)*t*t+(-p0+3*p1-3*p2+p3)*t*t*t))
 points.append(points[0]);length=[0.]
 for a,b in zip(points,points[1:]):length.append(length[-1]+float(np.linalg.norm(b-a)))
 inner=[]
 for i,p in enumerate(points[:-1]):
  tangent=points[(i+1)%(len(points)-1)]-points[(i-1)%(len(points)-1)];tangent/=np.linalg.norm(tangent)
  inner.append(p+1.3*np.array([-tangent[1],tangent[0]]))
 inner.append(inner[0]);inner_length=[0.]
 for a,b in zip(inner,inner[1:]):inner_length.append(inner_length[-1]+float(np.linalg.norm(b-a)))
 lines=['// Generated from actual supplied poses by tools/import_tanthoon_bandits.py --data.',
 f'static constexpr double bandit_camp_x={ox:.12f},bandit_camp_z={oz:.12f};',
 'static constexpr float bandit_camp_enter=170.f,bandit_camp_exit=210.f,bandit_camp_draw_radius=140.f;',
 'static constexpr unsigned bandit_camp_pose_count=40;',
 'static constexpr BanditCampClipDef bandit_camp_clips[]={']
 base=0
 for f in m['families']:
  hi=np.array(f['bounds_max']);lo=np.array(f['bounds_min']);r=float(np.linalg.norm(np.maximum(abs(lo),abs(hi))))
  lines.append('{%du,%du,%.9ff,%.9ff,%.9ff,%.9ff,%.9ff,%.9ff},'%(base,f['pose_count'],f['cycle_seconds'],f['stride_m'],hi[1],f['foot_half_x'],f['foot_half_z'],r));base+=f['pose_count']
 lines+=['};','static constexpr float bandit_camp_ground_corrections[3][512]={']
 for f in m['families']:lines.append('{'+','.join('%.9ff'%v for v in f['blend_ground_correction_512'])+'},')
 lines+=['};','static constexpr BanditCampPathPoint bandit_camp_path_points[]={']
 for path,distances in [(points,length),(inner,inner_length)]:
  for p,d in zip(path,distances):lines.append('{%.12f,%.12f,%.9ff},'%(ox+p[0],oz+p[1],d))
 lines+=['};','static constexpr BanditCampPathDef bandit_camp_paths[]={',
 '{0u,%du,%.9ff},'%(len(points),length[-1]),'{%du,%du,%.9ff},'%(len(points),len(inner),inner_length[-1]),'};','static constexpr BanditCampActorDef bandit_camp_actors[]={']
 actors=[]
 for i in range(5):actors.append(dict(mode=0,path=0,x=ox,z=oz,yaw=0,phase=i/5))
 actors.append(dict(mode=1,path=1,x=ox,z=oz,yaw=0,phase=.27))
 for x,z,yaw in [(-4,24,0),(21,-14,-1.1),(0,-26,0),(-18,4,1.4)]:actors.append(dict(mode=2,path=-1,x=ox+x,z=oz+z,yaw=yaw,phase=(len(actors)*.173)%1))
 for a in actors:lines.append('{%d,%d,%.12f,%.12f,%.9ff,%.9ff},'%(a['mode'],a['path'],a['x'],a['z'],a['yaw'],a['phase']))
 lines+=['};'];publish(ROOT/'engine/world_bandit_camp_data.inc',('\n'.join(lines)+'\n').encode())
 jsave(ASSET/'ACTOR_LAYOUT.json',dict(origin=[ox,oz],paths=[dict(points=[[ox+p[0],oz+p[1],d]for p,d in zip(path,distances)],length_m=distances[-1])for path,distances in [(points,length),(inner,inner_length)]],actors=actors,
 source_manifest_sha256=sha(OUT/MANIFEST),source_contact_note='Full source pose-union XZ projection +15mm encloses all blend-contact triangles, including newly grounded points after Y-only correction; endpoint clipped-triangle contacts retained for QA;512phase correction prevents floating.',
 source_modes=[dict(mode=f['mode'],action=f['action'],period=f['cycle_seconds'],source_period_seconds=f['source_period_seconds'],stride=f['stride_m'],foot_half_x=f['foot_half_x'],foot_half_z=f['foot_half_z'],bounds_min=f['bounds_min'],bounds_max=f['bounds_max'])for f in m['families']]))
 print('GENERATED five patrols/one separate-lane courier/four sentries,rounded paths',length[-1],inner_length[-1])

def validate():
 m=json.loads((OUT/MANIFEST).read_text());images=set()
 for f in m['families']:
  layouts=set();materials=set()
  for row in f['assets']:
   a=inspect(OUT/row['file']);assert a['sha256']==row['sha256'];layouts.add(a['pose_layout_sha256']);materials.add(a['material_sha256']);images.add(a['image_sha256']);assert a['triangles']==3000
  assert len(layouts)==len(materials)==1
  assert f['max_blend_ground_lift_m']<.15
 assert images=={m['atlas_sha256']};print('VALIDATED bandit40source/proceduralstanceposes,shared1Katlas,finitegroundedcompatiblefamilies')

if __name__=='__main__':
 a=argparse.ArgumentParser();a.add_argument('--prepare',action='store_true');a.add_argument('--cook',action='store_true');a.add_argument('--validate',action='store_true');a.add_argument('--data',action='store_true');o=a.parse_args()
 if o.prepare:prepare()
 if o.cook:cook()
 if o.validate:validate()
 if o.data:data()
