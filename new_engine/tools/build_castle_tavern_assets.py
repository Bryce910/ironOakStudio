#!/usr/bin/env python3
"""Author enterable tavern/shelter/bedroll geometry using exact resident PBR maps.
No downloads, GPU, source-model edits or additional image-cache policies.
python3 tools/build_castle_tavern_assets.py --cook
python3 tools/build_castle_tavern_assets.py --validate
"""
from pathlib import Path
import argparse,copy,hashlib,json,math,os,tempfile
import numpy as np
from build_castle_expansion_assets import Mesh as KitMesh
from build_village_keep_hero import Mesh as BaseMesh,GLBWriter,triangles,image_policies,ray_hits
from build_purchased_village import encode_glb
ROOT=Path(__file__).resolve().parents[1];MODELS=ROOT/'projects/valwood_world/models'
ASSET=ROOT/'assets/authored/castle_tavern';DONOR=MODELS/'purchased_village_house_a_near.glb'
FAMILIES=('village_tavern','castle_residents_shelter','castle_residents_bedroll')
FLOOR=.015
MATERIALS={
 'Stone':('Purchased_Tile_T_Tileable_Wall_Stone_01',[.88,.84,.76,1],.94),
 'Plaster':('Purchased_Source_Plaster',[.88,.79,.64,1],.96),
 'Wood':('Purchased_Source_Wood',[.69,.48,.29,1],.84),
 'DarkWood':('Purchased_Source_Wood',[.28,.21,.15,1],.94),
 'Roof':('Purchased_Tile_T_Roof_Tile_02',[.55,.57,.55,1],.94),
 'Metal':('Purchased_Source_Wood',[.16,.14,.11,1],.55),
 'Cloth':('Purchased_Source_Plaster',[.32,.28,.19,1],1),
 'Ember':('Purchased_Source_Wood',[.07,.025,.009,1],1)}

def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def publish(p,b):
 p=Path(p);p.parent.mkdir(parents=True,exist_ok=True)
 if p.exists()and p.read_bytes()==b:return
 q=p.with_suffix(p.suffix+'.pending');q.write_bytes(b);os.replace(q,p)
def jsave(p,d):publish(p,(json.dumps(d,indent=2)+'\n').encode())

class Mesh(KitMesh):
 def __init__(self,lod):
  BaseMesh.__init__(self,lod);self.parts={name:[[],[],[],[]]for name in MATERIALS};self.colliders=[]
 def face(self,mat,points,hint=None,uv=None,normals=None):
  if uv is None:
   p=np.asarray(points,float);normal=np.cross(p[1]-p[0],p[2]-p[0]);axis=int(np.argmax(abs(normal)));a,b=[(2,1),(0,2),(0,1)][axis]
   tile=2.0 if mat in('Stone','Plaster','Roof')else 1.2;uv=[[v[a]/tile,v[b]/tile]for v in p]
  BaseMesh.face(self,mat,points,hint,uv,normals)
 def solid(self,label,mat,x0,x1,y0,y1,z0,z1,collision=True):
  self.box(mat,x0,x1,y0,y1,z0,z1)
  if collision:self.colliders.append(dict(label=label,box=[x0,x1,z0,z1,y0,y1]))
 def mug(self,x,y,z,size=.12):
  n=12 if self.lod=='near'else 6
  self.lathe('Wood',[(0,size*.74),(.02,size*.92),(size*1.65,size), (size*1.72,size*.90),(size*1.72,size*.72),(size*.18,size*.72)],(x,y,z),n)
  self.lathe('DarkWood',[(size*1.58,size*.70),(size*1.58,0)],(x,y,z),n)
  if self.lod=='near':
   for i in range(8):
    a,b=-math.pi*.6+i*math.pi*1.2/8,-math.pi*.6+(i+1)*math.pi*1.2/8
    self.beam('Wood',(x+size+size*.55*math.cos(a),y+size*.85+size*.85*math.sin(a),z),(x+size+size*.55*math.cos(b),y+size*.85+size*.85*math.sin(b),z),size*.20)
 def barrel(self,x,y,z,r=.4,h=.85):
  n=16 if self.lod=='near'else 8
  self.lathe('Wood',[(0,r*.82),(.08*h,r*.92),(.5*h,r),(.92*h,r*.92),(h,r*.82)],(x,y,z),n)
  self.lathe('DarkWood',[(h,r*.82),(h,0)],(x,y,z),n)
  for yy in[h*.16,h*.78]:self.lathe('Metal',[(yy-.025,r*.96),(yy+.025,r*.96)],(x,y,z),n)
 def bench(self,label,x,z,width=3.2,yaw=0,back=True):
  # All caller benches face+Z; source seat/body contact is explicit metadata.
  assert yaw==0
  self.solid(label,'Wood',x-width/2,x+width/2,.425,.495,z-.23,z+.23,False)
  for xx in[x-width*.36,x+width*.36]:
   self.box('DarkWood',xx-.09,xx+.09,FLOOR,.425,z-.18,z+.18)
   self.beam('Wood',(xx,.12,z-.18),(xx,.12,z+.18),.08)
  self.beam('DarkWood',(x-width*.36,.20,z),(x+width*.36,.20,z),.09)
  if back:
   for xx in[x-width*.4,x+width*.4]:self.beam('DarkWood',(xx,FLOOR,z-.29),(xx,1.04,z-.33),.085)
   for yy in[.72,.96]:self.box('Wood',x-width/2,x+width/2,yy-.075,yy+.075,z-.37,z-.29)
  self.colliders.append(dict(label=label,box=[x-width/2,x+width/2,z-.38,z+.24,FLOOR,1.04 if back else .495]))
 def table(self,label,x,z):
  self.solid(label,'Wood',x-1.65,x+1.65,.745,.805,z-.55,z+.55,False)
  for xx in[x-1.32,x+1.32]:
   for zz in[z-.39,z+.39]:self.beam('DarkWood',(xx,FLOOR,zz),(xx,.745,zz),.13)
  self.beam('Wood',(x-1.32,.20,z),(x+1.32,.20,z),.12)
  self.colliders.append(dict(label=label,box=[x-1.65,x+1.65,z-.55,z+.55,FLOOR,.805]))
  if self.lod=='near':
   for i in range(7):self.box('DarkWood',x-1.64+i*.47,x-1.637+i*.47,.805,.807,z-.54,z+.54)
   for dx,dz in[(-.85,-.12),(.75,.11)]:self.mug(x+dx,.807,z+dz)
   self.lathe('Wood',[(0,.24),(.025,.25),(.04,.22),(.04,0)],(x,.808,z),16)

# Return physical wall cells; openings are absent mesh, not black image planes.
def wall(m,label,axis,side,start,end,holes,height=4.4,thick=.35):
 us=sorted({start,end,*[v for h in holes for v in h[:2]]});ys=sorted({FLOOR,height,.85,*[v for h in holes for v in h[2:]]})
 for a,b in zip(us,us[1:]):
  for c,d in zip(ys,ys[1:]):
   if any(h[0]<(a+b)/2<h[1]and h[2]<(c+d)/2<h[3]for h in holes):continue
   mat='Stone'if d<=.85+.0001 else'Plaster'
   if axis=='z':m.box(mat,a,b,c,d,side-thick/2,side+thick/2)
   else:m.box(mat,side-thick/2,side+thick/2,c,d,a,b)
 # Player/window collision stays conservative; only actual doors allow transit.
 doorholes=[h for h in holes if h[2]<=FLOOR+.001]
 ranges=[start,end,*[v for h in doorholes for v in h[:2]]];ranges=sorted(set(ranges))
 for a,b in zip(ranges,ranges[1:]):
  door=next((h for h in doorholes if h[0]<(a+b)/2<h[1]),None);low=door[3]if door else FLOOR
  box=[a,b,side-thick/2,side+thick/2,low,height]if axis=='z'else[side-thick/2,side+thick/2,a,b,low,height]
  m.colliders.append(dict(label=label,box=box))

def tavern(lod):
 m=Mesh(lod)
 # Stable ground-aligned slab; top is15mm, like the existing receiving hall.
 m.box('DarkWood',-9.82,9.82,0,.008,-7.82,7.82)
 if lod=='near':
  for i in range(80):
   a=-9.81+i*19.62/80;m.box('Wood',a,a+19.62/80-.003,.008,FLOOR,-7.81,7.81)
 else:m.box('Wood',-9.81,9.81,.008,FLOOR,-7.81,7.81)
 front=[(-1.25,1.25,FLOOR,2.8),(-7.5,-5.5,1.35,3.05),(5.5,7.5,1.35,3.05)]
 back=[(-6.8,-5.2,FLOOR,2.65),(-.2,1.8,1.45,3.05)]
 wall(m,'front_wall','z',8,-10,10,front);wall(m,'rear_wall','z',-8,-10,10,back)
 for side in[-10,10]:wall(m,'side_wall','x',side,-8,8,[(-3.9,-2.1,1.35,3.05),(2.1,3.9,1.35,3.05)])
 # Half-timber bays, stone footings and diagonal corner braces.
 for z in[-8.22,8.22]:
  xs=[-9.85,-4.1,4.1,9.85]+([-1.43,1.43]if z>0 else[-6.98,-5.02])
  for x in xs:m.box('DarkWood',x-.09,x+.09,FLOOR,4.45,z-.07,z+.07)
  m.box('Wood',-10.12,10.12,4.21,4.45,z-.10,z+.10)
  for x in[-9.5,7.9]:m.beam('Wood',(x,1.05,z),(x+1.5,3.9,z),.115)
 for side in[-10.22,10.22]:
  for z in[-7.85,0,7.85]:m.box('DarkWood',side-.07,side+.07,FLOOR,4.45,z-.09,z+.09)
  m.box('Wood',side-.09,side+.09,4.21,4.45,-8.15,8.15)
  # Thick gable end has proper inward and outward faces.
  for x,hint in[(side,-1 if side<0 else 1),(side+(.24 if side<0 else -.24),1 if side<0 else -1)]:
   m.face('Plaster',[(x,4.4,-8),(x,4.4,8),(x,8.2,0)],[hint,0,0])
  for z in[-8,8]:m.beam('DarkWood',(side,4.4,z),(side,8.2,0),.15)
  m.beam('Wood',(side,4.4,0),(side,8.2,0),.14)
 # Real reveals, jambs, sills, mullions; open shutters lie beside the aperture.
 for z,windows in[(8,[(-6.5,2.0),(6.5,2.0)]),(-8,[(.8,2.0)])]:
  for x,w in windows:
   for xx in[x-w/2,x+w/2]:m.box('Wood',xx-.055,xx+.055,1.30,3.12,z-.24,z+.24)
   for yy in[1.31,3.09]:m.box('Wood',x-w/2-.09,x+w/2+.09,yy-.055,yy+.055,z-.24,z+.24)
   m.box('Wood',x-.027,x+.027,1.35,3.05,z-.045,z+.045)
   for sign in[-1,1]:m.box('DarkWood',x+sign*(w/2+.42)-.34,x+sign*(w/2+.42)+.34,1.4,3.0,z+(-.23 if z<0 else .23)-.025,z+(-.23 if z<0 else .23)+.025)
 for side in[-10,10]:
  for z in[-3,3]:
   for zz in[z-.9,z+.9]:m.box('Wood',side-.24,side+.24,1.3,3.12,zz-.055,zz+.055)
   for yy in[1.31,3.09]:m.box('Wood',side-.24,side+.24,yy-.055,yy+.055,z-1,z+1)
   m.box('Wood',side-.04,side+.04,1.35,3.05,z-.03,z+.03)
 # Fully open solid door leaves; no door geometry crosses the2.5m void.
 for sign in[-1,1]:
  xa,xb=sorted([sign*1.30,sign*2.42]);m.box('Wood',xa,xb,FLOOR,2.72,8.24,8.33)
  for yy in[.3,2.25]:m.box('Metal',xa,xb,yy,yy+.08,8.34,8.37)
  m.beam('DarkWood',(xa,.18,8.38),(xb,2.5,8.38),.08)
 # Back service door opens flat against rear wall.
 m.box('Wood',-8.55,-7.0,FLOOR,2.58,-8.32,-8.23)
 # Roof with actual top, wood underside and substantial eaves.
 def roof_y(z):return 8.2-(8.2-4.4)*abs(z)/8.6
 for sign in[-1,1]:
  a,b=0,sign*8.6
  top=[(-10.55,roof_y(a),a),(10.55,roof_y(a),a),(10.55,roof_y(b),b),(-10.55,roof_y(b),b)]
  m.face('Roof',top,[0,1,0]);m.face('Wood',[(x,y-.16,z)for x,y,z in top],[0,-1,0])
  for x in[-10.55,10.55]:m.face('Wood',[(x,roof_y(a),a),(x,roof_y(b),b),(x,roof_y(b)-.16,b),(x,roof_y(a)-.16,a)],[1 if x>0 else -1,0,0])
  m.beam('DarkWood',(-10.55,4.35,b),(10.55,4.35,b),.20)
  if lod=='near':
   for ix in range(26):
    x0=-10.5+ix*21/26;x1=x0+21/26-.012
    for iz in range(11):
     z0=sign*(iz*8.6/11);z1=sign*((iz+1)*8.6/11+.04)
     m.face('Roof',[(x0,roof_y(z0)+.025,z0),(x1,roof_y(z0)+.025,z0),(x1,roof_y(z1)+.04,z1),(x0,roof_y(z1)+.04,z1)],[0,1,0])
 for x in[-7.5,-2.5,2.5,7.5]:
  m.beam('DarkWood',(x,4.15,-7.7),(x,4.15,7.7),.22)
  for z in[-7.7,7.7]:m.beam('Wood',(x,4.15,z),(x,8.0,0),.18)
  m.beam('DarkWood',(x,4.15,0),(x,8.0,0),.18)
 m.colliders.append(dict(label='roof_above_walk_height',box=[-10.55,10.55,-8.6,8.6,4.15,8.3]))
 # Back bar/counter, return, shelf supports and independent open staff aisle.
 m.solid('bar_counter','Wood',-8.3,-1.6,FLOOR,1.08,-4.2,-3.2)
 m.box('DarkWood',-8.4,-1.5,1.08,1.13,-4.28,-3.12)
 for x in np.linspace(-8.1,-1.9,12):m.box('DarkWood',x-.035,x+.035,.12,1.01,-3.23,-3.19)
 for a,b in[(-8.8,-7.1),(-4.9,-2.1)]:
  for y in[.65,1.65,2.5]:m.solid('back_bar_shelf','DarkWood',a,b,y,y+.065,-7.7,-7.15,False)
  m.colliders.append(dict(label='back_bar_storage',box=[a,b,-7.8,-7.1,FLOOR,2.65]))
  for x in[a,b]:m.box('DarkWood',x-.065,x+.065,FLOOR,2.65,-7.65,-7.2)
 for i,x in enumerate([-8,-4.1,-2.8]):
  m.barrel(x,.715,-7.4,.27,.65)
  if lod=='near':m.mug(x,1.72,-7.37,.10);m.mug(x+.22,2.57,-7.4,.09)
 for x in[-7,-5,-3]:
  m.lathe('Wood',[(.705,.29),(.765,.30),(.765,0)],(x,0,-2.35),16 if lod=='near'else 8)
  for dx,dz in[(-.16,-.16),(.16,-.16),(-.16,.16),(.16,.16)]:m.beam('DarkWood',(x+dx,FLOOR,-2.35+dz),(x+dx*.7,.705,-2.35+dz*.7),.07)
  m.colliders.append(dict(label='bar_stool',box=[x-.3,x+.3,-2.65,-2.05,FLOOR,.765]))
  if lod=='near':m.mug(x+.1,1.132,-3.4)
 # Keep occupied front benches fixed; genuine seated-body hands/aprons need a
 # 40cm longer reach to the two dining tables than the original layout allowed.
 for label,x,z,front_z in[('left_table',-5,3.4,2),('right_table',5,3.4,2),('hearth_table',6.2,-1,-2)]:
  m.table(label,x,z);m.bench(label+'_front_bench',x,front_z);m.bench(label+'_rear_bench',x,z+1,back=False)
 # Stone hearth with a real open firebox, mantel, fuel and chimney.
 m.solid('hearth_plinth','Stone',5.0,8.0,FLOOR,.16,-7.8,-5.2,False)
 for xa,xb in[(5.15,5.55),(7.45,7.85)]:m.box('Stone',xa,xb,.16,2.0,-7.75,-5.7)
 m.box('Stone',5.15,7.85,1.80,2.12,-7.75,-5.7);m.box('Stone',5.0,8.0,2.12,2.28,-7.82,-5.55)
 m.box('Stone',5.15,7.85,.16,1.8,-7.75,-7.57)
 m.box('Stone',5.65,7.35,2.28,9.5,-7.75,-6.55);m.box('Stone',5.5,7.5,9.5,9.7,-7.88,-6.42)
 m.colliders.append(dict(label='hearth_hot_volume',box=[5,8,-7.8,-5.2,FLOOR,2.28]))
 m.colliders.append(dict(label='chimney',box=[5.5,7.5,-7.88,-6.42,2.28,9.7]))
 for dx in[-.42,0,.42]:m.beam('DarkWood',(6.5+dx,.22,-6.9),(6.5-dx*.4,.30,-6.0),.14)
 for i in range(9):
  x=6.05+(i%3)*.45;z=-6.9+(i//3)*.35;m.sphere('Ember',(x,.22,z),(.095,.045,.08),8,4)
 # Hanging bar lamp is real open cage and metal chain; root adds flame/light.
 for y in np.arange(3.15,4.15,.16):m.beam('Metal',(-4.6,y,-.7),(-4.6,y+.1,-.7),.025)
 for x in[-4.76,-4.44]:
  for z in[-.86,-.54]:m.beam('Metal',(x,2.7,z),(x,3.18,z),.025)
 m.box('Metal',-4.81,-4.39,2.67,2.73,-.91,-.49);m.box('Metal',-4.81,-4.39,3.15,3.21,-.91,-.49)
 m.lathe('Ember',[(2.77,.055),(2.83,.045)],(-4.6,0,-.7),10)
 # Carved hanging sign: tankard medallion with geometric handle and raisedrim.
 m.beam('Metal',(3.5,3.85,8.25),(3.5,3.85,9.25),.07)
 for z in[8.75,9.15]:m.beam('Metal',(3.5,3.85,z),(3.5,3.38,z),.025)
 m.box('Wood',2.9,4.1,2.30,3.40,8.88,8.99)
 for yy in[2.34,3.35]:m.box('DarkWood',2.88,4.12,yy-.035,yy+.035,8.98,9.04)
 m.box('Metal',3.20,3.60,2.58,3.12,9.045,9.08);m.box('Metal',3.15,3.65,3.10,3.17,9.045,9.08)
 for i in range(8):
  a,b=-math.pi/2+i*math.pi/8,-math.pi/2+(i+1)*math.pi/8
  m.beam('Metal',(3.6+.22*math.cos(a),2.86+.24*math.sin(a),9.07),(3.6+.22*math.cos(b),2.86+.24*math.sin(b),9.07),.045)
 return m

def shelter(lod):
 m=Mesh(lod);m.box('DarkWood',-6.95,6.95,0,.008,-2.95,2.95);m.box('Wood',-6.94,6.94,.008,FLOOR,-2.94,2.94)
 for x in[-6.7,6.7]:
  for z in[-2.65,2.65]:m.solid('canopy_post','DarkWood',x-.10,x+.10,FLOOR,2.75,z-.10,z+.10)
 for z in[-2.65,2.65]:m.beam('Wood',(-6.8,2.66,z),(6.8,2.66,z),.19)
 m.solid('shelter_rear_wall','Wood',-7,7,FLOOR,2.72,-3.0,-2.85)
 for x in[-6.7,6.7]:
  m.beam('Wood',(x,2.7,-2.9),(x,2.7,2.9),.18)
  for z in[-2.6,2.6]:m.beam('Wood',(x,1.75,z),(x*.83,2.65,z),.10)
 for sign in[-1,1]:
  p=[(-7.25,3.35,0),(7.25,3.35,0),(7.25,2.72,sign*3.25),(-7.25,2.72,sign*3.25)]
  m.face('Wood',p,[0,1,0]);m.face('DarkWood',[(x,y-.10,z)for x,y,z in p],[0,-1,0])
 for x in[-5,-2.5,0,2.5,5]:
  for z in[-3,3]:m.beam('DarkWood',(x,2.68,z),(x,3.25,0),.12)
 m.bench('west_rest_bench',-3.5,-1.5);m.bench('east_rest_bench',3.5,-1.5)
 m.colliders.append(dict(label='shelter_roof',box=[-7.25,7.25,-3.25,3.25,2.65,3.35]))
 return m

def bedroll(lod):
 m=Mesh(lod);segments=24 if lod=='near'else 12
 # Rounded folded blanket with actual edge thickness and pillow volume.
 for i in range(segments):
  z0=-.95+1.9*i/segments;z1=-.95+1.9*(i+1)/segments
  h0=.045+.012*math.sin(i*.9);h1=.045+.012*math.sin((i+1)*.9)
  m.face('Cloth',[(-.36,h0,z0),(.36,h0,z0),(.36,h1,z1),(-.36,h1,z1)],[0,1,0])
  for sign in[-1,1]:m.face('Cloth',[(sign*.36,0,z0),(sign*.36,h0,z0),(sign*.36,h1,z1),(sign*.36,0,z1)],[sign,0,0])
 m.box('Cloth',-.36,.36,0,.02,-.95,.95)
 m.sphere('Cloth',(0,.064,-.65),(.30,.053,.22),16 if lod=='near'else 8,8 if lod=='near'else 4)
 for x in[-.25,.25]:m.beam('DarkWood',(x,.049,.48),(x,.05,.82),.018)
 return m

BUILDERS=dict(zip(FAMILIES,[tavern,shelter,bedroll]))
ANCHORS=dict(floor_y=FLOOR,front_axis='+Z',tavern_door=dict(centre=[0,FLOOR,8],clear_width=2.5,clear_height=2.785),
 seated=[dict(label='left_patron',alias='village_tavern',seat=[-5,.495,2],yaw=0,foot_floor_y=FLOOR,backrest_z=1.67),dict(label='right_patron',alias='village_tavern',seat=[5,.495,2],yaw=0,foot_floor_y=FLOOR,backrest_z=1.67),dict(label='west_rest',alias='castle_residents_shelter',seat=[-3.5,.495,-1.5],yaw=0,foot_floor_y=FLOOR,backrest_z=-1.83),dict(label='east_rest',alias='castle_residents_shelter',seat=[3.5,.495,-1.5],yaw=0,foot_floor_y=FLOOR,backrest_z=-1.83)],
 standing=[dict(label='barstaff_a',root=[-5,FLOOR,-5.3],yaw=0),dict(label='barstaff_b',root=[-2.5,FLOOR,-5.5],yaw=0),dict(label='guest_a',root=[5.5,FLOOR,-3.5],yaw=math.pi),dict(label='guest_b',root=[-2,FLOOR,-1],yaw=math.pi)],
 walk_route=[[0,9],[0,5],[0,0],[1,-2],[1,-5.3],[-5,-5.3]],
 emitters=[dict(label='tavern_hearth',base=[6.5,.35,-6.45],height=.65,radius=.30,facing_normal=[0,0,1],clip_min=[5.1,.2,-7.55],clip_max=[7.9,1.7,-5.4]),dict(label='tavern_barlamp',base=[-4.6,2.82,-.7],height=.22,radius=.075,facing_normal=[0,1,0],clip_min=[-4.78,2.70,-.88],clip_max=[-4.42,3.2,-.52])])

def save(path,name,mesh):
 writer=GLBWriter();writer.doc['asset']['generator']='Valwood authored enterable tavern and shelter1';writer.doc['nodes'][0]['name']=name
 for mat,(source,factor,rough)in MATERIALS.items():
  p,n,uv,ix=mesh.parts[mat]
  if not ix:continue
  index=len(writer.doc['materials']);writer.material(DONOR,source,mat);material=writer.doc['materials'][-1];material['name']='Tavern_'+mat
  material['pbrMetallicRoughness'].update(baseColorFactor=factor,metallicFactor=.8 if mat=='Metal'else 0,roughnessFactor=rough)
  if mat in('Metal','Ember'):
   for key in['normalTexture','occlusionTexture']:material.pop(key,None)
   for key in['baseColorTexture','metallicRoughnessTexture']:material['pbrMetallicRoughness'].pop(key,None)
  material['emissiveFactor']=[.65,.075,.004]if mat=='Ember'else[0,0,0]
  writer.doc['meshes'][0]['primitives'].append(dict(mode=4,material=index,attributes=dict(POSITION=writer.accessor(p,3),NORMAL=writer.accessor(n,3),TEXCOORD_0=writer.accessor(uv,2)),indices=writer.accessor(ix,1,5125)))
 writer.doc['buffers'][0]['byteLength']=len(writer.binary);publish(path,encode_glb(writer.doc,bytes(writer.binary)));return writer.provenance

def report(path,mesh):
 d,p,t=triangles(path);lo=p.min(axis=0);hi=p.max(axis=0);donor=image_policies(DONOR);policies=image_policies(path)
 assert policies<=donor,'New texture policy';assert np.isfinite(p).all()and abs(lo[1])<1e-6
 assert len(t)<(22000 if mesh.lod=='near'else 12000)
 assert all(np.isfinite(np.array(b['box'])).all()for b in mesh.colliders)
 return dict(file=path.name,sha256=sha(path),bytes=path.stat().st_size,triangles=len(t),primitives=len(d['meshes'][0]['primitives']),bounds_min=lo.tolist(),bounds_max=hi.tolist(),collision_boxes=mesh.colliders,image_policies=len(policies),new_image_cache_policies=0,new_gpu_image_bytes=0)

def navigation_audit(manifest):
 rows={(a['alias'],a['lod']):a for a in manifest['assets']};counts=dict(capsule_route_probes=0,actual_door_rays=0,actual_seat_contact_rays=0)
 def clear(alias,x,z):
  for item in rows[alias,'near']['collision_boxes']:
   x0,x1,z0,z1,y0,y1=item['box']
   if y0>=FLOOR+1.75 or y1<=FLOOR:continue
   distance=math.hypot(max(x0-x,0,x-x1),max(z0-z,0,z-z1))
   assert distance>=.32,(alias,'blocked source route',x,z,item['label'],distance)
  counts['capsule_route_probes']+=1
 for alias,path in [('village_tavern',ANCHORS['walk_route']),('village_tavern',[[-6,-5.3],[-6,-10]]),('castle_residents_shelter',[[0,4],[0,.4],[-5,.4],[5,.4]])]:
  for a,b in zip(path,path[1:]):
   for t in np.linspace(0,1,max(2,math.ceil(math.dist(a,b)/.08)+1)):
    x,z=np.array(a)*(1-t)+np.array(b)*t;clear(alias,float(x),float(z))
 for a in ANCHORS['standing']:clear('village_tavern',a['root'][0],a['root'][2])
 for lod in ['near','mid']:
  _,_,t=triangles(MODELS/f'village_tavern_{lod}.glb')
  for x in[-.85,0,.85]:
   for y in[.15,1.,1.7,2.5]:
    hits=ray_hits(t,[x,y,9.1],[0,0,-1]);assert not len(hits)or hits[0]>3.,('mesh blocks front doorway',lod,x,y,hits[:2]);counts['actual_door_rays']+=1
  for y in[.15,1.,1.7,2.4]:
   hits=ray_hits(t,[-6,y,-9.1],[0,0,1]);assert not len(hits)or hits[0]>3.,('mesh blocks service doorway',lod,y,hits[:2]);counts['actual_door_rays']+=1
  for a in ANCHORS['seated']:
   _,_,tt=triangles(MODELS/f"{a['alias']}_{lod}.glb");x,y,z=a['seat'];hits=ray_hits(tt,[x,.9,z],[0,-1,0]);assert len(hits)and abs(.9-hits[0]-y)<.00001,(a,'seat contact');counts['actual_seat_contact_rays']+=1
 return counts

def cook():
 ASSET.mkdir(parents=True,exist_ok=True);assets=[];materials=[];collision_rows=[]
 with tempfile.TemporaryDirectory(prefix='tavern-stage-',dir=ASSET)as directory:
  stage=Path(directory)
  for family,builder in BUILDERS.items():
   pair=[]
   for lod in['near','mid']:
    m=builder(lod);path=stage/f'{family}_{lod}.glb';materials.extend(save(path,family,m));row=report(path,m);row.update(alias=family,lod=lod);assets.append(row);pair.append(row)
    if lod=='near':
     for box in m.colliders:collision_rows.append(dict(alias=family,**box))
   assert np.max(abs(np.array(pair[0]['bounds_min'])-pair[1]['bounds_min']))<.06
   assert np.max(abs(np.array(pair[0]['bounds_max'])-pair[1]['bounds_max']))<.06
  for row in assets:publish(MODELS/row['file'],(stage/row['file']).read_bytes())
 materials=list({json.dumps(row,sort_keys=True):row for row in materials}.values())
 manifest=dict(schema_version=1,recipe_sha256=sha(Path(__file__)),source_model=str(DONOR.relative_to(ROOT)),source_sha256=sha(DONOR),source_materials=materials,assets=assets,anchors=ANCHORS,colliders=collision_rows,new_image_cache_policies=0,new_gpu_image_bytes=0,
 source_note='New authored3D shell/interior/furniture/shelter/bedrolls using exact encoded resident licensed stone/wood/plaster/roof images and existing sampler policies. No source model changed.',floor_note='15mm slab aligns existing terrain walking; slab intentionally excluded from blocking colliders. Furniture/walls use explicit compound boxes and doorways remain clear.')
 jsave(ASSET/'castle_tavern_manifest.json',manifest);jsave(MODELS/'castle_tavern_manifest.json',manifest);jsave(ASSET/'anchors.json',ANCHORS)
 lines=['// Generated by tools/build_castle_tavern_assets.py; same local metres as actual geometry.','static const VillageCollisionBox castle_tavern_collision_boxes[]={']
 for b in collision_rows:lines.append('{"%s",%s}, // %s'%(b['alias'],','.join('%.9ff'%v for v in b['box']),b['label']))
 lines+=['};'];publish(ROOT/'engine/world_castle_tavern_colliders.inc',('\n'.join(lines)+'\n').encode())
 validate();print('COOKED tavernkit',[(a['file'],a['triangles'])for a in assets])

def validate():
 m=json.loads((ASSET/'castle_tavern_manifest.json').read_text());assert m['recipe_sha256']==sha(Path(__file__))and m['source_sha256']==sha(DONOR)
 for row in m['assets']:
  actual=report(MODELS/row['file'],BUILDERS[row['alias']](row['lod']));assert actual['sha256']==row['sha256']and actual['collision_boxes']==row['collision_boxes']
 proof=navigation_audit(m);jsave(ASSET/'NAVIGATION_AUDIT.json',dict(passed=True,manifest_sha256=sha(ASSET/'castle_tavern_manifest.json'),**proof))
 assert m['new_gpu_image_bytes']==0;print('VALIDATED6authoredtavern/shelter/bedrollGLBs;exactresidentimages;0newcachepolicies')

if __name__=='__main__':
 a=argparse.ArgumentParser();a.add_argument('--cook',action='store_true');a.add_argument('--validate',action='store_true');o=a.parse_args()
 if o.cook:cook()
 if o.validate:validate()
