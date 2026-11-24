#!/usr/bin/env python3
"""Extend the isolated visual project's jungle; retain every original source object.
Deterministic owned/CC0 mesh derivatives, no downloads, source art remains read-only.
"""
from pathlib import Path
import copy, ctypes as C, hashlib, json, math, random, subprocess, tempfile, sys
import numpy as np
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'tools'))
import build_visual_upgrade_assets as assets
from compose_visual_upgrade import geometry
from build_cinematic_diversity import Writer
from build_starting_area import Atlas,segment_distance
PROJECT=ROOT/'projects/valwood_visual';MODELS=PROJECT/'models';OUT=MODELS/'jungle_expansion';OUT.mkdir(exist_ok=True)
DATA=PROJECT/'assets/jungle_expansion';DATA.mkdir(parents=True,exist_ok=True)
ORIGIN=(-26712.,9640.496705929329)
OLD=[(-15,95),(2,60),(-8,20),(5,-25),(-5,-65),(12,-110)]
LOOP=[(12,-110),(-22,-139),(-55,-170),(-68,-204),(-66,-248),(-43,-290),(-8,-329),(34,-329),(65,-296),(72,-253),(61,-214),(38,-178),(22,-143),(12,-110)]
SPUR=[(-66,-248),(-79,-253),(-83,-258)]
LANDMARKS=[('Hidden Pool',-83,-268),('The Fallen Shrine',-8,-322),('Canopy Watch',34,-329),('Boar Glade',-84,-231),('Leopard Glade',85,-285)]
def world(p):return (ORIGIN[0]+p[0],ORIGIN[1]+p[1])
def distance(p,line):return min(segment_distance(p,a,b)for a,b in zip(line,line[1:]))
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()

def cluster(name,stems,seed,spread=3.8):
 rng=random.Random(seed)
 for lod in ['near','mid']:
  materials=[];textures=[];images=[];groups={};sources={}
  for stem,x,z,yaw,scale in stems:
   key=(stem,lod)
   if key not in sources:
    path=MODELS/(stem+('_mid'if lod=='mid'else'_near')+'.glb')
    if not path.exists():path=MODELS/(stem+('.glb'if lod=='near'else'_mid.glb'))
    d,b,parts=geometry(path);mi=len(materials);ti=len(textures);ii=len(images)
    for material in copy.deepcopy(d['materials']):
     for obj in [material,material.get('pbrMetallicRoughness',{})]:
      for k,v in obj.items():
       if k.endswith('Texture')and isinstance(v,dict):v['index']+=ti
     materials.append(material)
    for t in copy.deepcopy(d.get('textures',[])):t['source']+=ii;t['sampler']=0;textures.append(t)
    for j,im in enumerate(copy.deepcopy(d.get('images',[]))):
     v=d['bufferViews'][im.pop('bufferView')];path=OUT/f'{name}_{lod}_{len(images)}.png';path.write_bytes(b[v.get('byteOffset',0):v.get('byteOffset',0)+v['byteLength']]);im['uri']=str(path);images.append(im)
    sources[key]=(parts,mi)
   parts,mi=sources[key];c,s=math.cos(yaw),math.sin(yaw);rot=np.array([[c,0,s],[0,1,0],[-s,0,c]])
   for pos,normal,uv,idx,mat in parts:
    g=groups.setdefault(mi+mat,[[],[],[],[]]);offset=sum(len(a)for a in g[0]);g[0].append(pos@rot.T*scale+[x,0,z]);g[1].append(normal@rot.T);g[2].append(uv);g[3].append(idx+offset)
  image_paths=[im['uri']for im in images];w=Writer(materials,textures,images)
  for mat,g in groups.items():w.mesh(name+str(mat),*[np.concatenate(a)for a in g],mat)
  w.save(OUT/(name+('_mid'if lod=='mid'else'')+'.glb'),embedded=True)
  for path in image_paths:Path(path).unlink()

def foliage():
 for kind in ['fern','broadleaf','thicket']:
  rng=random.Random({'fern':530,'broadleaf':712,'thicket':855}[kind]);stems=[]
  for i in range(13 if kind=='fern'else 9):
   a=i*2.399963;r=math.sqrt((i+.3)/13)*4.5;x,z=math.cos(a)*r,math.sin(a)*r
   if kind=='fern':stem='bep_nature_fern'if i%3 else'bep_nature_grass_broad';scale=rng.uniform(1.05,1.85)
   elif kind=='broadleaf':stem='visual_milestone/broadleaf'if i%2 else'bep_nature_fern';scale=rng.uniform(.8,1.3)
   else:stem='bep_nature_bush_a'if i%3==0 else('bep_nature_fern'if i%3==1 else'bep_nature_clover');scale=rng.uniform(.7,1.25)
   stems.append((stem,x,z,rng.random()*math.tau,scale))
  cluster(kind,stems,1)
 print('Built three layered near/mid foliage islands',flush=True)

def landmarks():
 assets.OUT=OUT;m=assets.Model();g=PROJECT/'ground'
 from PIL import Image
 maps={}
 for role,filename,size in [('base','village_stone_BC.png',1024),('normal','village_stone_N.png',512),('arm','village_stone_ARM.png',512)]:
  im=Image.open(g/filename).convert('RGB');im.thumbnail((size,size),Image.Resampling.LANCZOS);q=OUT/('shrine_'+role+'.png');im.save(q);maps[role]=q
 stone=m.material('Weathered owned/CC0 stone',[.66,.73,.64],.93,maps=maps)
 moss=m.material('Damp mossed stone',[.36,.47,.24],.98,maps=maps)
 # Ruin sits beside the route: stairs, dislocated pillars and collapsed lintel.
 for y,w in [(0,4.3),(.18,3.6),(.36,2.8)]:m.box(stone,(-w/2,y,-1.7),(w/2,y+.18,1.7))
 for x,h in [(-1.7,3.9),(1.7,2.1)]:
  for j in range(int(h/.48)):m.box(stone,(x-.37,.54+j*.48,-.38),(x+.37,1+j*.48,.38))
  m.box(moss,(x-.4,.52,-.41),(x+.4,.61,.41))
 m.box(stone,(-.95,.55,-.65),(.95,.9,.65));m.box(moss,(-.97,.88,-.66),(.97,.94,.66))
 m.box(stone,(2.4,.05,-1.2),(5.1,.8,-.3),.32,(2.4,0,-1.2));m.save('fallen_shrine')
 m=assets.Model();wood=m.material('Weathered trail wood',[.24,.16,.075],.92);moss=m.material('Mossed wood',[.12,.18,.055],.96)
 m.box(wood,(-.075,0,-.075),(.075,1.5,.075));m.box(wood,(-.64,1.13,-.055),(.64,1.44,.055));m.box(moss,(-.65,1.42,-.065),(.65,1.46,.065));m.save('trail_marker')

# Fixed topology across every pose lets the existing GPU pose interpolation flap
# feathered wings without per-frame allocations, uploads of geometry or model IO.
def bird(pose,glide=False):
 m=assets.Model();red=m.material('Scarlet macaw body',[.72,.045,.018],.65);blue=m.material('Blue flight feathers',[.025,.14,.5],.58);gold=m.material('Golden coverts',[.88,.57,.045],.65);white=m.material('Ivory face',[.72,.68,.5],.6);dark=m.material('Beak and eyes',[.018,.025,.026],.42)
 def ellipsoid(mat,c,r):
  for j in range(6):
   for i in range(10):
    pts=[]
    for u,v in [(i,j),(i+1,j),(i+1,j+1),(i,j+1)]:
     a=u/10*math.tau;b=-math.pi/2+.001+v/6*(math.pi-.002);pts.append((c[0]+r[0]*math.cos(b)*math.cos(a),c[1]+r[1]*math.sin(b),c[2]+r[2]*math.cos(b)*math.sin(a)))
    m.quad(mat,list(reversed(pts)))
 ellipsoid(red,(0,0,0),(.13,.16,.34));ellipsoid(red,(0,.12,-.30),(.14,.14,.145));ellipsoid(white,(0,.135,-.389),(.108,.085,.061));ellipsoid(dark,(0,.1,-.454),(.06,.07,.085))
 for s in [-1,1]:
  ellipsoid(dark,(s*.101,.155,-.386),(.016,.017,.018))
  angle=(.08+math.sin(pose/8*math.tau)*.72)if not glide else .05+math.sin(pose/4*math.tau)*.035
  def wing(x,y,z):return (s*(.1+x*math.cos(angle)),y+x*math.sin(angle),z)
  for j in range(7):
   a=.05+j*.10;b=a+.14;length=.40-j*.025
   pts=[wing(a,.03,-.12+j*.025),wing(b,.03,-.05+j*.025),wing(b+.10,.02,length),wing(a+.02,.02,length+.02)]
   m.quad(gold if j<3 else blue,pts);m.quad(gold if j<3 else blue,list(reversed(pts)))
 for j in range(3):
  x=(j-1)*.065;pts=[(x-.04,-.035,.20),(x+.04,-.035,.20),(x+.025,-.09,.88-abs(j-1)*.1),(x-.025,-.09,.88-abs(j-1)*.1)];m.quad(red if j==1 else blue,pts);m.quad(red if j==1 else blue,list(reversed(pts)))
 # Wildlife rigs use local+Z as forward; align the beak with the flight tangent.
 for part in m.parts.values():
  part[0]=[(-x,y,-z)for x,y,z in part[0]];part[1]=[(-x,y,-z)for x,y,z in part[1]]
 return m

def compose():
 rng=random.Random(531072);base=(DATA/'base_world.studio').read_text();assert not any(l.startswith('prop ')for l in base.splitlines())
 rows=[];props=[];trees=[];labels=[]
 definitions=[('explore_fern','fern',2,0,1,68,20),('explore_broadleaf','broadleaf',2,0,1,75,22),('explore_thicket','thicket',2,0,1,80,22),('explore_canopy','../visual_milestone/canopy',2,3,2,170,38)]
 # Use an allowed relative stem with private symlinks to existing texture-identical LODs.
 for name in ['canopy','canopy_near','canopy_mid']:
  q=OUT/(name+'.glb')
  if not q.exists():q.symlink_to((MODELS/'visual_milestone'/(name+'.glb')).resolve())
 definitions[-1]=('explore_canopy','canopy',2,3,2,170,38)
 definitions += [('explore_shrine','fallen_shrine',1,1,0,130,35),('explore_marker','trail_marker',1,0,0,75,20)]
 for alias,stem,lods,collision,veg,dist,split in definitions:rows.append(f'asset {alias} jungle_expansion/{stem} {lods} {collision} {veg} {dist} {split} 1')
 with tempfile.TemporaryDirectory(prefix='jungle-author-')as tmp:
  atlas=Atlas(tmp);atlas.api.world_atlas_base_height.argtypes=[C.c_double,C.c_double];atlas.api.world_atlas_base_height.restype=C.c_float
  def height(p):return float(atlas.api.world_atlas_base_height(*world(p)))
  def add(alias,p,scale=1,yaw=None,burial=.03):
   x,z=world(p);yaw=rng.uniform(-180,180)if yaw is None else yaw
   props.append(dict(alias=alias,x=x,z=z,scale=scale,yaw=yaw,burial=burial,local=list(p)))
   rows.append(f'prop {alias} {x:.9f} {z:.9f} {yaw:.5f} {scale:.5f} {burial:.4f} 0 0')
  lines=[OLD,LOOP,SPUR];reserve=[(-22,-25,19),(-24,30,20),(-78,-32,12),(-83,-268,12),(-84,-231,13),(85,-285,14),(-8,-322,8),(34,-329,12)]
  def free(p,r=0):
   if min(distance(p,l)for l in lines)<5+r:return False
   if any(math.hypot(p[0]-x,p[1]-z)<rad+r for x,z,rad in reserve):return False
   # Preserve the established encounter/camp/quest service zone.
   if math.hypot(p[0]-24,p[1]-30)<30+r:return False
   return True
  # Blue-noise canopy across the entire finished footprint, including off-path.
  attempts=0
  while len(trees)<220 and attempts<12000:
   attempts+=1;p=(rng.uniform(-102,102),rng.uniform(-345,115))
   if not free(p,1.2)or any(math.dist(p,q)<12.5 for q in trees):continue
   trees.append(p);add('explore_canopy',p,rng.uniform(.95,1.50),burial=.12)
  # Stratified coverage avoids a thin line of props beside the camera route.
  cells=[(x,z)for z in range(-338,112,11)for x in range(-96,100,11)];rng.shuffle(cells)
  for x,z in cells:
   if len(props)>=810:break
   p=(x+rng.uniform(-3,3),z+rng.uniform(-3,3))
   if not free(p,1):continue
   h=height(p);slope=max(abs(height((p[0]+dx,p[1]+dz))-h)for dx,dz in [(3,0),(-3,0),(0,3),(0,-3)])
   if slope>1.4:continue
   alias=rng.choice(['explore_fern','explore_fern','explore_broadleaf','explore_thicket']);add(alias,p,rng.uniform(.9,1.45),burial=.12)
  # Fill selected gaps in the old pocket too; trees and undergrowth are walkable
  # between islands, while ground litter/grass remain present in their gaps.
  for i in range(48):
   for attempt in range(80):
    p=(rng.uniform(-98,98),rng.uniform(-342,112))
    if free(p,1):add('bep_nature_palm',p,rng.uniform(2,3.5),burial=.05);break
  for i in range(32):
   p=(rng.uniform(-98,98),rng.uniform(-342,112))
   if free(p,.3):add('finish_debris',p,rng.uniform(1.1,2.6),burial=.015)
  # Rocks and decaying wood from the owned rainforest library.
  for i in range(34):
   p=(rng.uniform(-99,99),rng.uniform(-343,112))
   if free(p,1.5):add('rock1'if i%2 else'deadwood',p,rng.uniform(.7,1.4),burial=.10)
  add('explore_shrine',(-8,-319),1,180,.08)
  # Deliberate compositions at the destinations supplement area-wide scatter.
  for p in [(-14,-318),(-2,-316),(-10,-313)]:add('explore_broadleaf',p,1.2,burial=.12)
  for p in [(-12,-320),(-5,-317)]:add('showcase_roots',p,.55,burial=.10)
  for p in [(-89,-265),(-86,-274),(-77,-271),(-90,-272)]:add('rock1',p,.5,burial=.15)
  for p in [(-92,-263),(-79,-276),(-91,-277)]:add('explore_fern',p,1.0,burial=.12)

  for line in [LOOP]:
   for i,p in enumerate(line[:-1]):
    q=line[(i+1)%len(line)];dx,dz=q[0]-p[0],q[1]-p[1];d=math.hypot(dx,dz);add('explore_marker',(p[0]-dz/d*3.4,p[1]+dx/d*3.4),1,math.degrees(math.atan2(dx,-dz)),.02)
  # Dense edge undergrowth gives off-path views depth and a natural boundary,
  # rather than exposing the sparsely dressed regional forest immediately.
  edge=[(x,z)for x in [-101,101]for z in range(-340,111,16)]
  edge += [(x,z)for z in [-344,113]for x in range(-84,85,18)]
  for p in edge:
   if free(p,0):add('explore_thicket',p,1.4,burial=.12)
  assert len(props)<=980,len(props)
  staged=DATA/'staged.studio';staged.write_text(base+'\n'+'\n'.join(rows)+'\n')
  surface=['VALWOOD_SURFACES 2','image "Forest trail color" "surfaces/jungle_expansion/trail_color.png" 0','image "Forest trail normal" "surfaces/jungle_expansion/trail_normal.png" 1','image "Forest trail ARM" "surfaces/jungle_expansion/trail_arm.png" 2','layer "Damp jungle trail" 0 1 2 .65 .65 .55 2.4 .94 0 .7 0']
  # The soil names are resolved to existing project images below.
  for name,route,width in [('Emerald return loop',LOOP,3.6),('Pool side trail',SPUR,2.5)]:
   points=[(*world(p)[:1],height(p),world(p)[1])for p in route]
   surface.append(f'ribbon "{name}" 0 0 {width} 2 1 0 {len(points)} '+' '.join(' '.join(f'{v:.9f}'for v in p)for p in points))
  for i,(x,z)in enumerate([(-50,-185),(52,-281)]):
   wx,wz=world((x,z));surface.append(f'wind "Canopy gust {i}" {wx} {height((x,z))+7} {wz} 54 30 65 .8 .3 1.1 .55 8 {i+531}')
  (DATA/'surfaces.txt').write_text('\n'.join(surface)+'\n')
  manifest=dict(version=1,origin=ORIGIN,old_bounds=[[-105,-115],[105,120]],bounds=[[-105,-350],[105,120]],old_area_m2=210*235,area_m2=210*470,route=[world(p)for p in OLD],loop=[world(p)for p in LOOP],pool_spur=[world(p)for p in SPUR],landmarks=[dict(name=n,x=world((x,z))[0],z=world((x,z))[1])for n,x,z in LANDMARKS],props=props)
  manifest['walk_metres']=sum(math.dist(a,b)for line in [OLD,LOOP]for a,b in zip(line,line[1:]))
  (PROJECT/'jungle_expansion.json').write_text(json.dumps(manifest,indent=2)+'\n')
  header=['// Generated by tools/build_jungle_expansion.py. Private visual-project profile.','#ifndef VALWOOD_JUNGLE_EXPANSION_DATA_H','#define VALWOOD_JUNGLE_EXPANSION_DATA_H','struct JunglePlace {const char *name;double x,z;};','inline constexpr JunglePlace kJunglePlaces[]={']
  header += [f'{{"{n}",{world((x,z))[0]:.12f},{world((x,z))[1]:.12f}}},'for n,x,z in LANDMARKS];header+=['};','inline constexpr double kJungleLoop[][2]={']+[f'{{{world(p)[0]:.12f},{world(p)[1]:.12f}}},'for p in LOOP]+['};','#endif']
  (ROOT/'engine/world_jungle_expansion_data.h').write_text('\n'.join(header)+'\n')
 print('Authored',len(props),'new scenery islands/objects;',len(trees),'canopies; footprint doubled to',manifest['area_m2'],'m²',flush=True)

if __name__=='__main__':
 foliage();landmarks();assets.OUT=OUT
 for i in range(12):bird(i if i<8 else i-8,i>=8).save('bird_'+('walk'if i<8 else'idle')+f'_{i if i<8 else i-8:02d}')
 compose()
 with tempfile.TemporaryDirectory(prefix='jungle-commit-')as temp:
  exe=Path(temp)/'author';subprocess.run(['c++','-std=c++17','-O2','-I',str(ROOT),*[str(ROOT/f)for f in ['tools/author_jungle_expansion.cpp','engine/world_studio.cpp','engine/world_surface_authoring.cpp','engine/world_entity_graph.cpp','engine/world_shader_graph.cpp','engine/world_surface_field.cpp','engine/world_atlas.o']],'-lm','-o',str(exe)],check=True)
  subprocess.run([str(exe),str(DATA/'staged.studio'),str(DATA/'surfaces.txt'),str(PROJECT/'world.studio')],check=True)
