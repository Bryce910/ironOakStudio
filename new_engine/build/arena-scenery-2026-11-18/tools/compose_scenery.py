#!/usr/bin/env python3
"""Deterministic tournament-yard art; leaves the fight/entry collision data intact."""
from pathlib import Path
import io,json,math,struct,random,hashlib,shutil
import numpy as np
from PIL import Image,ImageFilter

N=Path(__file__).resolve().parents[1];P=N/'arena-project';BASE=N.with_name('third-person-focus-2026-11-18')/'arena-project'
R=random.Random(18112026)

class GLB:
 def __init__(self):
  self.bin=bytearray();self.j=dict(asset=dict(version='2.0',generator='Valwood tournament scenery authoring'),buffers=[dict(byteLength=0)],bufferViews=[],accessors=[],images=[],textures=[],samplers=[dict(magFilter=9729,minFilter=9987,wrapS=10497,wrapT=10497)],materials=[],meshes=[],nodes=[],scenes=[dict(nodes=[])],scene=0);self.groups={}
 def blob(self,data):
  self.bin.extend(b'\0'*(-len(self.bin)%4));i=len(self.j['bufferViews']);self.j['bufferViews'].append(dict(buffer=0,byteOffset=len(self.bin),byteLength=len(data)));self.bin.extend(data);return i
 def image(self,path):
  b=Path(path).read_bytes();i=len(self.j['images']);self.j['images'].append(dict(bufferView=self.blob(b),mimeType='image/png' if b[:4]==b'\x89PNG' else 'image/jpeg'));t=len(self.j['textures']);self.j['textures'].append(dict(source=i,sampler=0));return t
 def material(self,name,color=(1,1,1,1),maps=None,rough=.9,metal=0):
  p=dict(baseColorFactor=list(color),roughnessFactor=rough,metallicFactor=metal);m=dict(name=name,pbrMetallicRoughness=p,doubleSided=False)
  if maps:
   p['baseColorTexture']=dict(index=maps[0]);m['normalTexture']=dict(index=maps[1],scale=.7);p['metallicRoughnessTexture']=dict(index=maps[2]);m['occlusionTexture']=dict(index=maps[2],strength=.75)
  i=len(self.j['materials']);self.j['materials'].append(m);return i
 def triangle(self,mat,points,uv=None):
  a=np.asarray(points,dtype=float);normal=np.cross(a[1]-a[0],a[2]-a[0]);n=np.linalg.norm(normal)
  assert n>1e-9
  normal/=n;uv=uv or [(0,0),(1,0),(1,1)];g=self.groups.setdefault(mat,[])
  for p,t in zip(a,uv):g.append([*p,*normal,*t])
 def quad(self,mat,a,b,c,d,scale=1):
  u=np.linalg.norm(np.array(b)-a)/scale;v=np.linalg.norm(np.array(d)-a)/scale
  self.triangle(mat,[a,b,c],[(0,0),(u,0),(u,v)]);self.triangle(mat,[a,c,d],[(0,0),(u,v),(0,v)])
 def beam(self,mat,a,b,w=.2,d=None,bevel=.018):
  a=np.array(a,dtype=float);b=np.array(b,dtype=float);axis=(b-a)/np.linalg.norm(b-a);ref=np.array([0,1,0]) if abs(axis[1])<.9 else np.array([1,0,0]);u=np.cross(axis,ref);u/=np.linalg.norm(u);v=np.cross(axis,u);d=d or w;h=w/2;k=d/2;t=min(bevel,h*.25,k*.25)
  section=[(-h+t,-k),(h-t,-k),(h,-k+t),(h,k-t),(h-t,k),(-h+t,k),(-h,k-t),(-h,-k+t)]
  aa=[a+u*x+v*y for x,y in section];bb=[b+u*x+v*y for x,y in section]
  for i in range(8):j=(i+1)%8;self.quad(mat,aa[i],aa[j],bb[j],bb[i],1.5)
  for i in range(1,7):self.triangle(mat,[aa[0],aa[i+1],aa[i]]);self.triangle(mat,[bb[0],bb[i],bb[i+1]])
 def write(self,path):
  primitives=[]
  for mat,rows in self.groups.items():
   arr=np.array(rows,dtype='<f4');attrs={}
   for name,start,count in [('POSITION',0,3),('NORMAL',3,3),('TEXCOORD_0',6,2)]:
    values=np.ascontiguousarray(arr[:,start:start+count]);i=len(self.j['accessors']);a=dict(bufferView=self.blob(values.tobytes()),componentType=5126,count=len(arr),type='VEC'+str(count))
    if name=='POSITION':a.update(min=values.min(axis=0).tolist(),max=values.max(axis=0).tolist())
    self.j['accessors'].append(a);attrs[name]=i
   primitives.append(dict(attributes=attrs,material=mat,mode=4))
  self.j['meshes']=[dict(primitives=primitives)];self.j['nodes']=[dict(mesh=0)];self.j['scenes'][0]['nodes']=[0];self.bin.extend(b'\0'*(-len(self.bin)%4));self.j['buffers'][0]['byteLength']=len(self.bin)
  js=json.dumps(self.j,separators=(',',':')).encode();js+=b' '*(-len(js)%4);out=struct.pack('<4sII',b'glTF',2,12+8+len(js)+8+len(self.bin))+struct.pack('<II',len(js),0x4e4f534a)+js+struct.pack('<II',len(self.bin),0x004e4942)+self.bin
  Path(path).write_bytes(out);return dict(triangles=sum(len(v)//3 for v in self.groups.values()),materials=len(primitives),bytes=len(out))

def jpg(array,path):Image.fromarray(np.uint8(np.clip(array,0,255))).save(path,quality=94)
ART=N/'review/materials';ART.mkdir(exist_ok=True)
# Baked, world-scale wear: no extra overlapping geometry/decal passes. The
# colour, normal and ARM use identical world mapping; blood stays above64.032.
size=2048;y,x=np.mgrid[0:size,0:size];xx=x/(size-1)*24-12;zz=y/(size-1)*24-12
ring=np.exp(-((np.sqrt((xx/1.05)**2+zz**2)-6.2)/1.2)**2)
lane=np.exp(-(xx/2.8)**4)*np.clip((-zz+2)/12,0,1)
noise=(np.sin(xx*.77+np.sin(zz*.33))*np.cos(zz*.62)+np.sin(xx*2.3+zz*.71)*.25)*.5
wear=np.clip(ring*.48+lane*.52,0,.78);edge=np.clip((np.maximum(abs(xx),abs(zz))-9.8)/2.2,0,1)
def tiled(path,tiles=15):
 im=np.array(Image.open(path).convert('RGB').resize((size//tiles+1,size//tiles+1),Image.Resampling.LANCZOS));return np.tile(im,(tiles+1,tiles+1,1))[:size,:size].astype(float)
color=tiled(N/'downloads/brown_mud/color.jpg');color*=((.99+noise*.055)*(1-wear*.13))[:,:,None]
# Dry compacted tread marks and softly scattered pale gravel, never wet gloss.
gray=color.mean(axis=2,keepdims=True);color=color*(1-wear[:,:,None]*.25)+gray*wear[:,:,None]*.25
normal=tiled(N/'downloads/brown_mud/normal.jpg');normal[:,:,:2]=128+(normal[:,:,:2]-128)*(1-wear[:,:,None]*.42)
arm=tiled(N/'downloads/brown_mud/arm.jpg');arm[:,:,1]=np.maximum(arm[:,:,1],220);arm[:,:,2]=0
jpg(color,ART/'floor_color.jpg')
for name,array in [('normal',normal),('arm',arm)]:Image.fromarray(np.uint8(np.clip(array,0,255))).save(ART/('floor_'+name+'.png'))
floor=GLB();maps=[floor.image(ART/('floor_'+n+('.jpg' if n=='color' else '.png'))) for n in ['color','normal','arm']];mat=floor.material('Dry packed tournament earth',maps=maps,rough=1)
floor.triangle(mat,[(-12,0,-12),(-12,0,12),(12,0,12)],[(0,0),(0,1),(1,1)]);floor.triangle(mat,[(-12,0,-12),(12,0,12),(12,0,-12)],[(0,0),(1,1),(1,0)])
floor_info=floor.write(P/'scenery/arena_floor.glb')

def timbers():
 g=GLB();m=[g.image(N/'downloads/wood_planks'/(n+'.jpg')) for n in ['color','normal','arm']]
 wood=g.material('Weathered oak joinery',(.43,.47,.44,1),m);dark=g.material('Dark cedar roof',(.20,.23,.22,1),m);trim=g.material('Pale worn edge grain',(.59,.58,.48,1),m);iron=g.material('Forged iron straps',(.07,.075,.08,1),rough=.7,metal=.8)
 blue=g.material('Faded indigo canvas',(.025,.073,.13,1),rough=.98);red=g.material('Oxide red canvas',(.25,.038,.025,1),rough=.98);cream=g.material('Unbleached linen',(.58,.49,.32,1),rough=.98)
 return g,wood,dark,trim,iron,blue,red,cream

# Roofed spectator stand. Front remains open; all geometry is beyond the ring.
g,wood,dark,trim,iron,blue,red,cream=timbers()
for x in [-12,-8,-4,0,4,8,12]:
 for z in [-4,4]:
  g.beam(wood,(x,0,z),(x,3.45,z),.27)
  g.beam(iron,(x,.32,z),(x,.44,z),.29)
  for s in [-1,1]:
   if -12<=x+s*.85<=12:g.beam(wood,(x,2.55,z),(x+s*.85,3.4,z),.13)
 g.beam(wood,(x,3.45,-4.45),(x,5.1,0),.19);g.beam(wood,(x,5.1,0),(x,3.45,4.45),.19)
 g.beam(wood,(x,3.4,-4),(x,3.4,4),.16)
for z,h in [(-4.35,3.46),(0,5.1),(4.35,3.46)]:g.beam(trim,(-12.6,h,z),(12.6,h,z),.23)
# Individual overlapping roofing boards with slightly varied ends.
for i in range(84):
 x=-12.6+(i+.5)*.3
 for s in [-1,1]:
  g.beam(dark,(x,5.13,0),(x,3.44,s*(4.6+R.uniform(-.05,.05))),.315,.055,.008)
# Lower rear panelling, safe raised spectator balustrade and end rails.
for i in range(80):
 x=-11.85+i*.3;g.beam(wood,(x,.1,4),(x,2.35,4),.29,.06,.008)
for h in [1.9,2.4]:g.beam(trim,(-12,h,4),(12,h,4),.16)
for x in [-12,12]:
 for h in [1,2.3]:g.beam(wood,(x,h,-4),(x,h,4),.15)
 g.beam(wood,(x,0,3.8),(x,2.8,0),.12);g.beam(wood,(x,0,0),(x,2.8,3.8),.12)
# Scalloped hanging linen and alternating tournament colours.
for i in range(24):
 x=-12+i;bottom=2.98 if i%2 else 2.86;m=[blue,cream,red,cream][i%4]
 g.quad(m,(x,3.37,-4.48),(x,3.37,-4.50),(x+1,3.37,-4.50),(x+1,3.37,-4.48))
 # two sides: cloth is thin, deliberately modelled rather than alpha blended.
 pts=[(x,3.37,-4.49),(x+.5,bottom,-4.49),(x+1,3.37,-4.49)];g.triangle(m,pts);g.triangle(m,pts[::-1])
# Three pendant banners behind the open front beam.
for x,m in [(-8,blue),(0,red),(8,blue)]:
 pts=[(x-.44,3.3,-3.94),(x-.44,2.15,-3.94),(x,1.95,-3.94),(x+.44,2.15,-3.94),(x+.44,3.3,-3.94)]
 for i in range(1,4):g.triangle(m,[pts[0],pts[i],pts[i+1]]);g.triangle(m,[pts[0],pts[i+1],pts[i]])
 # Simple heraldic cross in old gold, on both visible faces.
 g.beam(cream,(x,2.25,-3.97),(x,3.05,-3.97),.065,.02,.002);g.beam(cream,(x-.24,2.76,-3.97),(x+.24,2.76,-3.97),.065,.02,.002)
stand_info=g.write(P/'models/arena_pavilion.glb');shutil.copy2(P/'models/arena_pavilion.glb',P/'models/arena_pavilion_mid.glb')

# Timber entrance arch and two short stretches of perimeter bunting.
g,wood,dark,trim,iron,blue,red,cream=timbers()
for x in [-3.2,3.2]:
 g.beam(wood,(x,0,0),(x,4.1,0),.32);g.beam(trim,(x,3.7,-.02),(x,4.32,-.02),.4,.13)
 g.beam(wood,(x,2.75,0),(x-math.copysign(1,x),3.8,0),.16)
g.beam(wood,(-3.55,3.8,0),(3.55,3.8,0),.33)
for i in range(10):
 x=-3+i*.6;pts=[(x,3.66,-.2),(x+.3,3.12,-.2),(x+.6,3.66,-.2)];m=[red,cream,blue,cream][i%4];g.triangle(m,pts);g.triangle(m,pts[::-1])
gate_info=g.write(P/'models/arena_gate.glb');shutil.copy2(P/'models/arena_gate.glb',P/'models/arena_gate_mid.glb')

# Weathered timber cladding covers the existing stone barrier without moving
# its collider. The optional native draw replaces the old procedural rails.
g,wood,dark,trim,iron,blue,red,cream=timbers()
for side in range(4):
 def point(t,h,depth=0):
  return ((-12 if side==0 else 12)+depth,h,t) if side<2 else (t,h,(-12 if side==2 else 12)+depth)
 for i in range(80):
  t=-11.85+i*.3
  if side==2 and abs(t)<2.15:continue
  g.beam(wood,point(t,0),point(t,1.43+R.uniform(-.018,.018)),.3 if side<2 else .48,.48 if side<2 else .3,.012)
 for start,end in ([(-12,-2.15),(2.15,12)] if side==2 else [(-12,12)]):
  g.beam(trim,point(start,1.47),point(end,1.47),.51,.11,.012)
 for t in [-12,-8,-4,0,4,8,12]:
  if side==2 and abs(t)<2.15:continue
  g.beam(dark,point(t,0),point(t,1.62),.34 if side<2 else .49,.49 if side<2 else .34,.018)
  for h in [.28,1.25]:g.beam(iron,point(t-.17,h,-.253),point(t+.17,h,-.253),.045,.02,.004)
for x in [-1.9,1.9]:g.beam(wood,(x,0,12),(x,2.85,12),.22)
boundary_info=g.write(P/'scenery/arena_boundary.glb')

# Reuse the original, deterministic terrain and all original placements.
height=np.frombuffer((P/'ground/showcase.height').read_bytes(),dtype='<f4',offset=48).reshape(513,513)
def terrain(x,z):
 u=(x+512)/2;v=(z+512)/2;ix=int(u);iz=int(v);fx=u-ix;fz=v-iz
 return float(height[iz,ix]*(1-fx)*(1-fz)+height[iz,ix+1]*fx*(1-fz)+height[iz+1,ix]*(1-fx)*fz+height[iz+1,ix+1]*fx*fz)
rows=[];records=[]
def put(alias,x,z,s=1,yaw=None,y=None,wind=None):
 y=terrain(x,z) if y is None else y;yaw=R.uniform(-180,180) if yaw is None else yaw
 row=f'{alias} {(x-1800)/100800+.5:.16g} {(z-10500)/(100800*2818/5009)+.5:.16g} {yaw:.5f} {s:.5f} 0 0 0 0 0 1 1 1 {y:.6f}'
 if wind:row+=f' wind {wind[0]} {wind[1]}'
 rows.append(row);records.append(dict(alias=alias,x=x,z=z,y=y,scale=s,yaw=yaw))
put('nm_arena_pavilion',0,18,yaw=0,y=64);put('nm_arena_gate',0,-16,yaw=0,y=64)
def clear(x,z):
 if max(abs(x),abs(z))<13.1:return False
 if z< -12 and abs(x)<6.3:return False
 if -13<x<13 and 12<z<23:return False
 if -25<x< -15 and -6.5<z<6.5:return False
 if 15<x<25 and -7<z<5.5:return False
 if -22<x< -16 and 6<z<15:return False
 if 19<x<27 and -22<z< -13:return False
 return True
# Deliberate shrub/fern drifts instead of a uniform green perimeter.
centres=[(-15,8),(15,8),(-16,-9),(16,-9),(-17,23),(17,24),(-23,-15),(27,3),(-30,8),(30,18),(-25,31),(26,33),(-11,-31),(12,-29)]
for cx,cz in centres:
 for i in range(33):
  x=R.gauss(cx,2.2);z=R.gauss(cz,2.1)
  if not clear(x,z):continue
  alias=R.choices(['bep_nature_grass_long','bep_nature_grass_broad','bep_nature_fern','bep_nature_bush_a'],[5,3,3,1])[0]
  put(alias,x,z,R.uniform(.4,.85) if 'bush' not in alias else R.uniform(.5,.8),wind=(.85,.65))
 for i in range(2):
  x=cx+R.uniform(-1.4,1.4);z=cz+R.uniform(-1.4,1.4)
  if clear(x,z):put('rock0' if i%2 else 'rock1',x,z,R.uniform(.45,1.2),y=terrain(x,z)-.17)
# Lush, irregular low growth in the immediate apron. Keep shop, targets,
# spectator access and the southern entry route open.
for i in range(1500):
 x=R.uniform(-27,27);z=R.uniform(-30,30)
 if not clear(x,z) or max(abs(x),abs(z))>26:continue
 if R.random()>.72+.24*math.sin(x*.45)*math.cos(z*.35):continue
 put(R.choice(['bep_nature_grass_long','bep_nature_grass_broad','bep_nature_fern']),x,z,R.uniform(.75,1.15),wind=(.9,.7))
# Taller shrubs form a middle layer visible above the arena boundary.
for cx,cz in [(-16,-9),(16,9),(-16,10),(16,-10),(-16,24),(16,25),(-26,-17),(27,8)]:
 for i in range(5):
  x=cx+R.uniform(-1.6,1.6);z=cz+R.uniform(-1.6,1.6)
  if clear(x,z):put('bep_nature_bush_a' if i%2 else 'bep_nature_bush_b',x,z,R.uniform(.95,1.25),wind=(.6,.7))
# Wood-line detritus, root silhouettes and equipment clutter in useful groups.
for x,z in [(-18,26),(20,27),(-29,5),(29,11),(-25,-24),(27,-27)]:
 put('showcase_stump',x,z,R.uniform(.8,1.1));put('deadwood',x+2,z-1,R.uniform(.7,.95))
for x,z in [(-17,5),(17,5),(-13,23),(13,23),(-8,-23),(8,-23)]:
 put('purchased_barrel',x,z,R.uniform(.65,.9));put('purchased_crate',x+R.choice([-1,1])*.8,z+.7,.6)
files=sorted((P/'scenery').glob('*instances.txt'))
for i,f in enumerate(files):f.write_text((BASE/'scenery'/f.name).read_text()+'\n# Arena scenery detail pass; no props inside the fighting rectangle.\n'+'\n'.join(rows[i::len(files)])+'\n')
studio=(BASE/'world.studio').read_text().replace('look 0.18 0.95 0.90 1.0 15.2 100 0.38 0 0.25 0.08','look 0.18 0.98 0.90 1.0 15.6 100 0.30 0 0.45 0.045')
studio+='asset nm_arena_pavilion arena_pavilion 2 0 0 240 85 1\nasset nm_arena_gate arena_gate 2 0 0 180 75 1\n'
# Ground paint uses the editor's existing material system. It follows the
# actual terrain, including hills, so there are no floating decal planes.
surface_dir=P/'surfaces'
if surface_dir.is_symlink():
 old=surface_dir.resolve();surface_dir.unlink();surface_dir.mkdir()
 for item in old.iterdir():(surface_dir/item.name).symlink_to(item.resolve(),target_is_directory=item.is_dir())
for name in ['forest_litter_color.png','forest_litter_normal.png','forest_litter_material.png']:
 f=surface_dir/name
 if f.is_symlink():data=f.read_bytes();f.unlink();f.write_bytes(data)
for name in ['color','normal','arm']:shutil.copy2(N/'downloads/brown_mud'/(name+'.jpg'),surface_dir/('arena_mud_'+name+'.jpg'))
surfaces=['VALWOOD_SURFACES 2']
for i,(prefix,paths) in enumerate([('Mud',['surfaces/arena_mud_color.jpg','surfaces/arena_mud_normal.jpg','surfaces/arena_mud_arm.jpg']),('Leaf litter',['surfaces/forest_litter_color.png','surfaces/forest_litter_normal.png','surfaces/forest_litter_material.png'])]):
 for role,path in enumerate(paths):
  assert (P/path).is_file(),path
  surfaces.append(f'image "{prefix} {role}" "{path}" {role}')
 surfaces.append(f'layer "{prefix}" {i*3} {i*3+1} {i*3+2} 1 1 1 {1.3 if i==0 else 2.4} 1 0 0.55 0')
for x,z in [(-20,-14),(20,-10),(-22,12),(22,14),(-15,29),(15,28),(-30,-27),(30,-26)]:surfaces.append(f'paint {x-1800} {z-10500} 14 0.88 1 0')
for x,z in [(0,-12),(0,-18),(0,-24),(0,-30),(0,-36),(-18,0),(18,0),(0,18)]:surfaces.append(f'paint {x-1800} {z-10500} {4 if x==0 and z<0 else 8} 0.92 0 0')
payload='\n'.join(surfaces)+'\n';(N/'SURFACES.txt').write_text(payload)
quoted='\n'.join('surfaces '+json.dumps(line) for line in surfaces);studio=studio.replace('surfaces "VALWOOD_SURFACES 2"',quoted)
(P/'world.studio').write_text(studio)
report=dict(seed=18112026,additional_placements=len(records),added_vegetation=sum('nature_' in p['alias'] for p in records),floor=floor_info,stand=stand_info,gate=gate_info,boundary=boundary_info,floor_y=64.032,physics_changed=False,original_records_preserved=True,physical_materials=['Poly Haven brown_mud (CC0)','Poly Haven wood_planks (CC0)','Existing Poly Haven forest_leaves_03 (CC0)'])
(N/'SCENERY_LAYOUT.json').write_text(json.dumps(report,indent=2)+'\n');(N/'SCENERY_PLACEMENTS.json').write_text(json.dumps(records,indent=2)+'\n');print(json.dumps(report,indent=2))
