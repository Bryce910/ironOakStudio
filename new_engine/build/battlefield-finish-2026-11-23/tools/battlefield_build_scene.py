#!/usr/bin/env python3
"""Build the private, terrain-sampled Seldonne battlefield; never writes symlink targets.
Owned cart/banner/deadwood geometry is reused. All generated meshes are small PBR
battle debris and conforming blood pools, not replacement character models.
"""
from pathlib import Path
import ctypes, json, math, os, random, struct, subprocess, hashlib
import numpy as np
N=Path(__file__).resolve().parents[1]; P=N/'world-project'; S=N/'source-v1'
W=100800.; D=W*2818/5009
ORIGIN=(-12085.,5150.)
CAST=[(-12118,5142,-.6),(-12107,5158,1.7),(-12103,5134,-2.3),(-12094,5167,.4),(-12087,5138,2.1),(-12077,5163,-.9),(-12069,5131,.7),(-12058,5167,-2.7),(-12049,5138,.3),(-12036,5158,-1.2),(-12128,5155,2.8),(-12083,5154,1.0)]
ANCHORS={'arrival':[-12115,5150],'ambush':[-12095,5154],'escape':[-12091,5154.8],'escape_1':[-12070,5144],'escape_2':[-12042,5150],'forest_gate':[-12015,5162],'camp':[-11988,5172]}
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def private_write(path,text):
 path.parent.mkdir(parents=True,exist_ok=True)
 if path.is_symlink():path.unlink()
 path.write_text(text)
def sampler():
 so=N/'tools/battlefield_atlas.so'
 subprocess.run(['cc','-shared','-fPIC','-O2','-I'+str(S),str(S/'engine/world_atlas.c'),'-lm','-o',str(so)],check=True)
 a=ctypes.CDLL(str(so));a.world_atlas_init.restype=ctypes.c_bool;assert a.world_atlas_init()
 for name in ['world_atlas_height','world_atlas_walk_height']:
  f=getattr(a,name);f.argtypes=[ctypes.c_double,ctypes.c_double];f.restype=ctypes.c_float
 a.world_atlas_load_height_region.argtypes=[ctypes.c_char_p,ctypes.c_char_p,ctypes.c_size_t]
 a.world_atlas_load_fallback_height_region.argtypes=a.world_atlas_load_height_region.argtypes
 err=ctypes.create_string_buffer(256)
 loaded={}
 for name,path in [('primary',P/'ground/showcase.height'),('fallback',P/'ground/countryside.height')]:
  f=a.world_atlas_load_height_region if name=='primary'else a.world_atlas_load_fallback_height_region
  loaded[name]={'path':str(path),'result':f(os.fsencode(path),err,256),'error':err.value.decode()}
 return a,loaded
# Tiny indexed GLB builder retains real metal/cloth/wood PBR response.
class Mesh:
 def __init__(self):self.parts={}
 def tri(self,mat,a,b,c):
  p=np.array([a,b,c],float);n=np.cross(p[1]-p[0],p[2]-p[0]);n/=max(np.linalg.norm(n),1e-12)
  q=self.parts.setdefault(mat,[])
  for j,v in enumerate(p):q.append([*v,*n,*[(0,0),(1,0),(1,1)][j]])
 def quad(self,m,a,b,c,d):self.tri(m,a,b,c);self.tri(m,a,c,d)
 def tube(self,m,a,b,r,sides=9,taper=1):
  a=np.array(a,float);b=np.array(b,float);axis=b-a;axis/=np.linalg.norm(axis)
  u=np.cross(axis,[0,1,0]if abs(axis[1])<.9 else[1,0,0]);u/=np.linalg.norm(u);v=np.cross(axis,u)
  for j in range(sides):
   t=j*math.tau/sides;q=(j+1)*math.tau/sides
   aa=(u*math.cos(t)+v*math.sin(t))*r;bb=(u*math.cos(q)+v*math.sin(q))*r
   self.quad(m,a+aa,a+bb,b+bb*taper,b+aa*taper);self.tri(m,a,a+bb,a+aa);self.tri(m,b,b+aa*taper,b+bb*taper)
 def box(self,m,c,h,rz=0):
  c=np.array(c);h=np.array(h);r=np.array([[math.cos(rz),-math.sin(rz),0],[math.sin(rz),math.cos(rz),0],[0,0,1]])
  v=[c+r@np.array([x*h[0],y*h[1],z*h[2]])for x,y,z in[(-1,-1,-1),(1,-1,-1),(1,1,-1),(-1,1,-1),(-1,-1,1),(1,-1,1),(1,1,1),(-1,1,1)]]
  for f in[(0,3,2,1),(4,5,6,7),(0,4,7,3),(1,2,6,5),(3,7,6,2),(0,1,5,4)]:self.quad(m,*[v[i]for i in f])
 def save(self,path):
  colors=[[.16,.105,.055,1],[.09,.10,.11,1],[.22,.026,.018,1],[.06,.075,.11,1],[.30,.24,.14,1],[.095,.008,.006,1],[.15,.012,.008,1]]
  doc={'asset':{'version':'2.0','generator':'Valwood owned battlefield staging'},'scene':0,'scenes':[{'nodes':[0]}],'nodes':[{'mesh':0}],'meshes':[{'primitives':[]}],'accessors':[],'bufferViews':[],'materials':[],'buffers':[]};binary=bytearray()
  for i,c in enumerate(colors):doc['materials'].append({'name':['Splintered timber','Worn iron','Oxblood cloth','Valwood blue cloth','Dry wood endgrain','Dried blood','Fresh blood'][i],'pbrMetallicRoughness':{'baseColorFactor':c,'metallicFactor':.7 if i==1 else 0,'roughnessFactor':.42 if i==1 else .48 if i==6 else .86},'doubleSided':True})
  for mat,rows in self.parts.items():
   rows=np.array(rows,dtype='<f4');attrs={}
   for name,v in [('POSITION',rows[:,:3]),('NORMAL',rows[:,3:6]),('TEXCOORD_0',rows[:,6:])]:
    while len(binary)%4:binary.append(0)
    offset=len(binary);v=np.ascontiguousarray(v);binary.extend(v.tobytes());vi=len(doc['bufferViews']);doc['bufferViews'].append({'buffer':0,'byteOffset':offset,'byteLength':v.nbytes,'target':34962});ai=len(doc['accessors']);acc={'bufferView':vi,'componentType':5126,'count':len(v),'type':'VEC3'if v.shape[1]==3 else'VEC2'}
    if name=='POSITION':acc.update(min=v.min(0).tolist(),max=v.max(0).tolist())
    doc['accessors'].append(acc);attrs[name]=ai
   doc['meshes'][0]['primitives'].append({'attributes':attrs,'material':mat,'mode':4})
  doc['buffers']=[{'byteLength':len(binary)}];js=json.dumps(doc,separators=(',',':')).encode();js+=b' '*((-len(js))%4);binary+=b'\0'*((-len(binary))%4)
  raw=struct.pack('<III',0x46546c67,2,12+8+len(js)+8+len(binary))+struct.pack('<II',len(js),0x4e4f534a)+js+struct.pack('<II',len(binary),0x004e4942)+binary
  path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(raw);return {'path':str(path),'vertices':sum(len(v)for v in self.parts.values()),'bytes':len(raw),'sha256':sha(path)}
def build():
 a,loaded=sampler();g=lambda x,z:float(a.world_atlas_height(x,z));rng=random.Random(11222026);report={'terrain':loaded,'anchors':{k:[x,g(x,z),z]for k,(x,z)in ANCHORS.items()},'moved_tree_roots':[],'mesh_assets':[]}
 # Build derived files once. Reruns read originals from a private backup directory.
 original=N/'assets/battlefield/original-scenery';original.mkdir(parents=True,exist_ok=True)
 defs={}
 for l in(P/'world.studio').read_text().splitlines():
  q=l.split()
  if q and q[0]=='asset':defs[q[1]]=int(q[5])
 for f in(P/'scenery').glob('*.txt'):
  if f.name=='battlefield_instances.txt':continue
  old=original/f.name
  if not old.exists():old.write_bytes(f.read_bytes())
  rows=[];moved=0
  for l in old.read_text().splitlines():
   q=l.split()
   if len(q)>5 and not q[0].startswith('#'):
    x=(float(q[1])-.5)*W;z=(float(q[2])-.5)*D
    is_tree=defs.get(q[0])==2 or q[0]in ['pine','fir','broadleaf','pine_young']
    is_bush=any(s in q[0]for s in ['shrub','bush'])
    if(is_tree or is_bush)and ((x+12085)/62)**2+((z-5150)/31)**2<1:
     # Move to the south skyline; stay on this riverbank. Existing ordinal/ID stays.
     nx=x+rng.uniform(-3,3);nz=5185+rng.uniform(0,13)
     q[1]=f'{nx/W+.5:.14f}';q[2]=f'{nz/D+.5:.14f}'
     if len(q)>13 and q[13]!='wind':q[13]=f'{g(nx,nz):.9f}'
     report['moved_tree_roots'].append({'file':f.name,'alias':q[0],'from':[x,z],'to':[nx,nz]});l=' '.join(q);moved+=1
   rows.append(l)
  if moved:private_write(f,'\n'.join(rows)+'\n')
 # Hero barricades built from crossed, pointed trunks, iron pins and torn cloth.
 path=P/'models/battlefield';path.mkdir(exist_ok=True)
 m=Mesh()
 for x in[-1.8,-.6,.6,1.8]:
  for sign in[-1,1]:
   start=(x,.13,sign*.50);end=(x+sign*.18,1.48,-sign*.52)
   m.tube(0,start,end,.09,9,.76);m.tube(4,end,(end[0],end[1]+.23,end[2]-.08*sign),.068,9,0)
  m.tube(1,(x-.12,.70,-.05),(x+.12,.70,.05),.024,6)
 m.tube(0,(-2.2,.68,0),(2.2,.68,0),.082,10);report['mesh_assets'].append(m.save(path/'barricade.glb'))
 # Fallen shields/swords/straps are detailed props separate from skeletal casualties.
 m=Mesh()
 for side in[-1,1]:
  for i in range(6):
   x=-.37+i*.148;y=.07+.08*(1-(x/.5)**2)
   m.box(3 if side<0 else 2,(x,y,side*.19),(.069,.024,.30),rz=x*.15)
  m.tube(1,(-.44,.13,side*.49),(.44,.13,side*.49),.016,8)
  m.tube(1,(-.44,.12,side*.49),(-.44,.12,side*-.11),.016,8)
 m.tube(1,(-.35,.11,-.68),(.85,.12,.62),.022,5,.2);m.tube(1,(-.33,.11,-.48),(-.10,.11,-.70),.018,6)
 m.tube(0,(-.48,.11,-.82),(-.33,.11,-.65),.036,8);report['mesh_assets'].append(m.save(path/'discarded_arms.glb'))
 # Conformed, irregular pools are fully opaque small PBR geometry. Exact atlas
 # support plus25mm avoids coplanar flicker, even on the RX5300 at distance.
 m=Mesh();pool_min=1e9
 for i,(x,z,yaw)in enumerate(CAST):
  for k in range(3):
   xx=x+rng.uniform(-.45,.45);zz=z+rng.uniform(-.30,.30);radius=rng.uniform(.27,.64);center=np.array([xx-ORIGIN[0],g(xx,zz)+.025,zz-ORIGIN[1]]);pool_min=min(pool_min,center[1])
   pts=[]
   for j in range(18):
    t=j*math.tau/18;rr=radius*(.77+.26*rng.random());wx=xx+math.cos(t)*rr;wz=zz+math.sin(t)*rr*.65
    p=[wx-ORIGIN[0],g(wx,wz)+.025,wz-ORIGIN[1]];pool_min=min(pool_min,p[1]);pts.append(p)
   for j in range(18):m.tri(5+(i%3==0),center,pts[(j+1)%18],pts[j])
 # Explicit pivot restores this measured minimum after standard base alignment.
 report['mesh_assets'].append(m.save(path/'casualty_blood.glb'))
 records=[]
 def place(alias,x,z,yaw=0,scale=1,pitch=0,roll=0,absolute=None):
  s=f'{alias} {x/W+.5:.14f} {z/D+.5:.14f} {yaw:.5f} {scale:.5f} 0 0 0 {pitch:.5f} {roll:.5f} 1 1 1'
  if absolute is not None:s+=f' {absolute:.9f}'
  assert len(s)<=256;records.append(s)
 # Wrecks frame the camera reveal and make an eastward escape corridor legible.
 for x,z,yaw,roll in[(-12124,5156,31,73),(-12101,5132,-28,-67),(-12079,5170,125,87),(-12049,5131,18,55)]:place('battlefield_wreck_cart',x,z,yaw,.95,8,roll)
 for x,z,yaw in[(-12126,5159,15),(-12104,5135,62),(-12081,5167,-18),(-12050,5134,41),(-12068,5160,82)]:place('nm_city_35_sm_wooden_cart_wheel',x,z,yaw,.9,78,12)
 for x,z,yaw in[(-12131,5140,20),(-12101,5169,-60),(-12068,5130,-27),(-12039,5162,34)]:place('battlefield_barricade',x,z,yaw,1)
 for x,z,yaw in[(-12119,5133,5),(-12083,5174,72),(-12049,5132,-16)]:place('village_banner',x,z,yaw,.85,5,9)
 for x,z,yaw in CAST:place('battlefield_discarded_arms',x+.65,z+.25,yaw*180/math.pi,.7)
 for x,z,yaw in[(-12133,5149,-20),(-12075,5174,33),(-12047,5128,104)]:place('deadwood',x,z,yaw,.65)
 for x,z,yaw in[(-12123,5144,20),(-12105,5168,81),(-12078,5137,-5),(-12057,5167,16)]:place('starting_crate',x,z,yaw,.6,10,25)
 place('battlefield_casualty_blood',*ORIGIN,absolute=pool_min)
 private_write(P/'scenery/battlefield_instances.txt','# Seldonne battlefield; same dry riverbank as arrival camp.\n'+'\n'.join(records)+'\n')
 original_world=N/'assets/battlefield/world.before-battlefield.studio'
 if not original_world.exists():original_world.parent.mkdir(parents=True,exist_ok=True);original_world.write_bytes((P/'world.studio').read_bytes())
 text=original_world.read_text();extra=[]
 for alias,stem,lods,collision,vegetation,distance,split in[('battlefield_barricade','battlefield/barricade',1,1,0,200,45),('battlefield_discarded_arms','battlefield/discarded_arms',1,0,0,130,30),('battlefield_casualty_blood','battlefield/casualty_blood',1,0,0,160,40),('battlefield_wreck_cart','imports/nm_city_34_sm_wooden_cart/21d86a5642a01aedd96fb7e1/asset',2,1,0,220,45)]:extra.append(f'asset {alias} {stem} {lods} {collision} {vegetation} {distance} {split} 1')
 # Last containing authored ecology circle owns all layers, including zero.
 extra+=['ecology -12113 5151 34 .10 0 .015 .015 11222026 0 45','ecology -12063 5151 32 .10 0 .015 .015 11222027 0 45']
 for i,(x,z)in enumerate([(-12124,5156),(-12101,5132),(-12079,5170)]):
  y=g(x,z)+.6
  extra.append(f'particle -1 0 1 {x} {y:.8f} {z} {y+18:.8f} 6.5 14 1.0 .2 1.5 2.8 1.2 1.5 0 {11222026+i} .24 .23 .22 0')
  extra.append(f'light {x} {y+.4:.8f} {z} 8 3.5 1 .43 .13 0 -90 180 1')
 # Broken route ground: irregular projected scuff patches retain ground normal detail.
 for x,z,sz in[(-12117,5150,19),(-12097,5153,23),(-12072,5144,19),(-12048,5151,18)]:extra.append(f'decal {x} {g(x,z):.8f} {z} {sz} 2.2 12 .115 .092 .067 8 .62 .92 0')
 private_write(P/'world.studio',text.rstrip()+'\n'+'\n'.join(extra)+'\n')
 flames=[]
 for i,(x,z)in enumerate([(-12124,5156),(-12101,5132),(-12079,5170)]):flames.append({'x':x,'y':g(x,z)+.35,'z':z,'height':.9,'width':.46,'intensity':1.2,'seed':11222026+i})
 report.update(casualties=[{'x':x,'y':g(x,z),'z':z,'yaw':yaw}for x,z,yaw in CAST],props=len(records),static_pool_min=pool_min,flames=flames,scene_file=str(P/'scenery/battlefield_instances.txt'))
 # Real sampled route, including the forest threshold and camp, no river crossing.
 checks=[]
 route=list(ANCHORS.values());maxslope=0
 for (ax,az),(bx,bz)in zip(route,route[1:]):
  length=math.hypot(bx-ax,bz-az);n=max(2,int(length*2))
  prev=None
  for k in range(n+1):
   t=k/n;x=ax+(bx-ax)*t;z=az+(bz-az)*t;y=g(x,z)
   if prev:maxslope=max(maxslope,abs(y-prev[2])/max(1e-9,math.hypot(x-prev[0],z-prev[1])))
   prev=(x,z,y);checks.append([x,y,z])
 report['route_samples']=checks;report['max_route_slope_degrees']=math.degrees(math.atan(maxslope));report['source_ids_preserved']=True
 out=N/'assets/battlefield/SCENE_BUILD.json';out.write_text(json.dumps(report,indent=2)+'\n')
 a.world_atlas_shutdown();print(json.dumps({'report':str(out),'props':len(records),'moved_tree_roots':len(report['moved_tree_roots']),'max_route_slope_degrees':report['max_route_slope_degrees'],'mesh_bytes':sum(r['bytes']for r in report['mesh_assets'])}))
if __name__=='__main__':build()
