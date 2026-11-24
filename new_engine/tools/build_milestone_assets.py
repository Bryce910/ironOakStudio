"""Focused courtyard/path art, preserving original sources and photographed UVs."""
from pathlib import Path
import sys,json,copy,math,hashlib
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'tools'))
SOURCE=ROOT/'assets/polyhaven/milestone';OUT=ROOT/'projects/valwood_world/models/visual_milestone'
REPORT=ROOT/'docs/milestone/2026-10-30';PROJECT=ROOT/'projects/valwood_visual'

def prepare():
 import build_cinematic_diversity as d
 d.base.SOURCE=SOURCE
 d.prepare_tree(dict(source='island_tree_01',name='island',mesh=0,leaf_material=1,near_leaves=4200,mid_leaves=1300))

def cook():
 import bpy
 from mathutils import Vector
 OUT.mkdir(parents=True,exist_ok=True);records=[]
 for kind in ['canopy','broadleaf']:
  for lod in ['near','mid']:
   bpy.ops.wm.read_factory_settings(use_empty=True)
   path=SOURCE/'island_tree_01'/('components_'+lod+'.gltf')if kind=='canopy'else SOURCE/'calathea_orbifolia_01/calathea_orbifolia_01.gltf'
   bpy.ops.import_scene.gltf(filepath=str(path))
   if kind=='broadleaf':
    keep=next(o for o in bpy.context.scene.objects if o.type=='MESH'and o.name.startswith('calathea_orbifolia_01_a'))
    for o in list(bpy.context.scene.objects):
     if o!=keep:bpy.data.objects.remove(o,do_unlink=True)
   for o in list(bpy.context.scene.objects):
    if o.type!='MESH':continue
    tri=sum(len(p.vertices)-2 for p in o.data.polygons)
    budget=(6500 if 'wood_0'in o.name else 10500)if lod=='near'else(2100 if 'wood_0'in o.name else 3300)
    if kind=='broadleaf':budget=5904 if lod=='near'else 1600
    if ('wood'in o.name or kind=='broadleaf')and tri>budget:
     bpy.context.view_layer.objects.active=o;m=o.modifiers.new('Bounded original source geometry','DECIMATE');m.ratio=budget/tri;bpy.ops.object.modifier_apply(modifier=m.name)
   bpy.context.view_layer.update();objects=[o for o in bpy.context.scene.objects if o.type=='MESH'];pts=[o.matrix_world@Vector(c)for o in objects for c in o.bound_box];low=Vector([min(v[i]for v in pts)for i in range(3)]);high=Vector([max(v[i]for v in pts)for i in range(3)]);scale=(13.5 if kind=='canopy'else 1.05)/(high.z-low.z);center=(low+high)*.5;center.z=low.z
   for o in objects:o.location=(o.location-center)*scale;o.scale*=scale
   for m in bpy.data.materials:
    if m.use_nodes:
     for n in m.node_tree.nodes:
      if n.type=='TEX_IMAGE'and n.image:n.image.pack()
   name=kind+('_near'if kind=='canopy'and lod=='near'else''if lod=='near'else'_mid');p=OUT/(name+'.glb');bpy.ops.export_scene.gltf(filepath=str(p),export_format='GLB',export_yup=True,export_animations=False,export_cameras=False,export_lights=False)
   records.append(dict(name=name,triangles=sum(sum(len(p.vertices)-2 for p in o.data.polygons)for o in objects),bytes=p.stat().st_size,sha256=hashlib.sha256(p.read_bytes()).hexdigest()))
   print('COOKED',records[-1],flush=True)
 (REPORT/'vegetation-assets.json').write_text(json.dumps(records,indent=2)+'\n')

def dressing():
 import numpy as np
 from PIL import Image
 import build_visual_upgrade_assets as v
 from compose_visual_upgrade import geometry
 from build_cinematic_diversity import Writer
 OUT.mkdir(parents=True,exist_ok=True);v.OUT=OUT
 cache=OUT/'images';cache.mkdir(exist_ok=True)
 def original(m,path,position=(0,0,0),scale=1,yaw=0):
  d,b,parts=geometry(path);first=len(m.materials);tex=len(m.textures);im=len(m.images)
  mats=copy.deepcopy(d['materials'])
  for mat in mats:
   for obj in [mat,mat.get('pbrMetallicRoughness',{})]:
    for key,value in obj.items():
     if key.endswith('Texture')and isinstance(value,dict):value['index']+=tex
  for image in d.get('images',[]):
   bv=d['bufferViews'][image['bufferView']];data=b[bv.get('byteOffset',0):bv.get('byteOffset',0)+bv['byteLength']];p=cache/(hashlib.sha256(data).hexdigest()+('.jpg'if data[:2]==b'\xff\xd8'else'.png'))
   if not p.exists():p.write_bytes(data)
   m.images.append(dict(uri=str(p),mimeType=image.get('mimeType','image/png')))
  for t in d.get('textures',[]):q=copy.deepcopy(t);q['source']+=im;q['sampler']=0;m.textures.append(q)
  m.materials+=mats;c,s=math.cos(yaw),math.sin(yaw);rot=np.array([[c,0,s],[0,1,0],[-s,0,c]])
  for pos,n,uv,idx,material in parts:
   key=first+material;g=m.parts.setdefault(key,[[],[],[],[]]);offset=len(g[0]);g[0].extend((pos@rot.T*scale+position).tolist());g[1].extend((n@rot.T).tolist());g[2].extend(uv.tolist());g[3].extend((idx+offset).tolist())
 # A photographed cloth surface with shallow, real folds and stitched crest.
 fabric=SOURCE/'fabric_pattern_07/textures';rough=Image.open(fabric/'fabric_pattern_07_rough_1k.jpg').convert('L');p=cache/'cloth_arm.png';Image.merge('RGB',(Image.new('L',rough.size,255),rough,Image.new('L',rough.size,0))).save(p)
 m=v.Model();cloth=m.material('CC0 woven cloth',[.11,.22,.29],.95,maps={'base':fabric/'fabric_pattern_07_col_1_1k.jpg','normal':fabric/'fabric_pattern_07_nor_gl_1k.jpg','arm':p});m.materials[cloth].update(doubleSided=True)
 wood=m.material('Weathered banner pole',[.21,.14,.075],.88);gold=m.material('Old gold thread',[.64,.44,.15],.6,.28);iron=m.material('Iron fittings',[.085,.09,.095],.45,.85)
 m.box(wood,(-.045,0,-.045),(.045,3.65,.045));m.box(iron,(-.075,3.56,-.07),(.075,3.71,.07));m.box(wood,(-.05,3.34,-.025),(1.25,3.4,.035))
 def zfold(x,y):return .07*math.sin(x*15+y*.8)+.025*math.sin(y*7+x*3)
 nx,ny=24,28
 for j in range(ny):
  for i in range(nx):
   q=[];uv=[]
   for u,w in [(i/nx,j/ny),((i+1)/nx,j/ny),((i+1)/nx,(j+1)/ny),(i/nx,(j+1)/ny)]:
    x=.09+u*1.10;y=1.32+w*2.02-.16*math.sin(u*math.pi)*(1-w);q.append((x,y,zfold(x,y)));uv.append((u,w*1.7))
   m.quad(cloth,q,uv)
 # Fine borders follow the folds instead of floating above a flat cloth card.
 def ribbon(a,b,width):
  dx,dy=b[0]-a[0],b[1]-a[1];length=max(1e-8,math.hypot(dx,dy));px,py=-dy/length*width*.5,dx/length*width*.5
  for k in range(max(1,int(length*24))):
   n=max(1,int(length*24));p0=(a[0]+dx*k/n,a[1]+dy*k/n);p1=(a[0]+dx*(k+1)/n,a[1]+dy*(k+1)/n);q=[(p0[0]+px,p0[1]+py),(p0[0]-px,p0[1]-py),(p1[0]-px,p1[1]-py),(p1[0]+px,p1[1]+py)];m.quad(gold,[(x,y,zfold(x,y)+.004)for x,y in q],[(0,0),(1,0),(1,1),(0,1)])
 for x in [.14,1.14]:ribbon((x,1.4),(x,3.27),.018)
 ribbon((.14,3.27),(1.14,3.27),.018);ribbon((.65,1.86),(.65,2.85),.038)
 for yy,xx in [(2.06,.23),(2.32,.32),(2.58,.23)]:
  for side in [-1,1]:ribbon((.65,yy),(.65+side*xx,yy+.25),.032)
 # Cloth is viewed at a distance; retain shared plant pages for the beds.
 for image in m.images:
  src=Path(image['uri']);im=Image.open(src)
  if max(im.size)>512:
   im.thumbnail((512,512),Image.Resampling.LANCZOS);q=cache/('banner-'+src.stem+'.png');im.save(q);image['uri']=str(q);image['mimeType']='image/png'
 banner=m.save('watch_banner')
 q=PROJECT/'models/village_banner.glb'
 if q.is_symlink():q.unlink()
 assert not q.exists();q.symlink_to(OUT/'watch_banner.glb')
 # Stone beds keep the middle of the training court open.
 m=v.Model();brick=m.material('Weathered limestone bed edge',[.62,.64,.58],.91,maps={'base':SOURCE/'cobblestone_floor_08/textures/cobblestone_floor_08_diff_1k.jpg'});soil=m.material('Shaded earth',[.065,.047,.026],1)
 for x in np.arange(-3,3,.5):
  for z in [-.76,.76]:m.box(brick,(x,.015,z-.14),(x+.485,.38,z+.14))
 for x in [-2.96,2.96]:m.box(brick,(x-.12,.015,-.64),(x+.12,.38,.64))
 m.box(soil,(-2.82,.08,-.62),(2.82,.22,.62))
 for x in [-2.1,-.7,.8,2.1]:
  original(m,ROOT/'projects/valwood_world/models/showcase_shrub_mid.glb',(x,.20,0),.44,x*.45)
  original(m,ROOT/'projects/valwood_world/models/bep_nature_clover_mid.glb',(x+.32,.22,.3),.72,x)
 bed=m.save('court_bed')
 # New paving replaces only the selected project's material proxy.
 folder=SOURCE/'cobblestone_floor_08/textures';maps={name:Image.open(folder/('cobblestone_floor_08_'+suffix+'_2k.jpg')).convert('RGBA')for name,suffix in [('village_stone_BC.png','diff'),('village_stone_N.png','nor_dx'),('village_stone_ARM.png','arm')]}
 for name,im in maps.items():
  path=PROJECT/'ground'/name
  if path.is_symlink():path.unlink()
  im.save(path)
 q=PROJECT/'models/visual_milestone'
 if not q.exists():q.symlink_to(OUT,target_is_directory=True)
 (REPORT/'courtyard-assets.json').write_text(json.dumps(dict(banner=banner,bed=bed,paving='CC0 cobblestone_floor_08, selected project only'),indent=2)+'\n')
 print('COURTYARD detailed cloth/crest and planted stone bed ready',flush=True)

if __name__=='__main__':
 if '--prepare'in sys.argv:prepare()
 if '--cook'in sys.argv:cook()
 if '--dressing'in sys.argv:dressing()
