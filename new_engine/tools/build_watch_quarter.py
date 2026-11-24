#!/usr/bin/env python3
"""Compose a compact, walkable watch quarter using existing owned art.
Only the isolated visual project is changed. Stable original record IDs stay put.
"""
from pathlib import Path
import json,math,sys
from PIL import Image,ImageDraw,ImageFont
import numpy as np
import build_visual_upgrade_assets as assets
from compose_visual_upgrade import FILES,WIDTH,DEPTH,line
from build_starting_area import glb_bounds
ROOT=Path(__file__).resolve().parents[1];PROJECT=ROOT/'projects/valwood_visual'
OUT=PROJECT/'models/watch_quarter';REPORT=ROOT/'docs/castle-quarter/2026-10-30'
MARK='# Watch quarter authored layer'
def build():
 OUT.mkdir(exist_ok=True);REPORT.mkdir(parents=True,exist_ok=True);assets.OUT=OUT
 records=[];models=[]
 def add(alias,x,z,yaw=0,scale=1,elevation=0):
  r=dict(alias=alias,x=x,z=z,yaw=yaw,scale=scale,elevation=elevation);records.append(r);return r
 # Four individually scaled homes frame a continuous market lane below the keep.
 for alias,x,yaw,scale in [('purchased_house_a',-1340,90,1.03),('purchased_house_b',-1325,-90,.97),('purchased_house_e',-1309,90,1.12),('purchased_house_a',-1293,90,.96)]:add(alias,x,-10324,yaw,scale)
 add('village_tavern',-1284,-10287,-90)
 add('purchased_shop_a',-1297,-10309,0,1.12)
 add('starting_table',-1301,-10308.5,0,.85)
 add('starting_table',-1295.5,-10289.2,0,.85)
 add('castle_yard_bench',-1297.5,-10289.2,90,.85)
 add('castle_yard_bench',-1293.5,-10289.2,90,.85)
 add('village_well',-1316,-10297,0,1)
 add('milestone_bed',-1333,-10308,0,1)
 add('milestone_bed',-1319,-10308,0,1)
 add('castle_yard_bench',-1326,-10308,180,1)
 add('village_banner',-1333,-10319,0,.9)
 add('village_banner',-1317,-10319,0,.9)
 # Readable painted signs share a single small atlas. Icon silhouettes remain readable at distance.
 atlas=Image.new('RGB',(1024,1024),'#263c3b');draw=ImageDraw.Draw(atlas)
 font=ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSerif-Bold.ttf',43)
 small=ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf',25)
 labels=[('WATCH ARMORY','EQUIPMENT & REPAIRS'),('KEEP PROVISIONS','BREAD  /  ORCHARD FRUIT'),("WATCHMAN'S REST",'HOT MEALS  /  BEDS'),('WATCH QUARTER','HOMES  /  MARKET  /  TAVERN')]
 for i,(title,sub)in enumerate(labels):
  y=i*256;draw.rectangle((10,y+10,1014,y+246),outline='#d9bc79',width=5);draw.line((40,y+180,984,y+180),fill='#b79555',width=2)
  draw.text((512,y+92),title,anchor='mm',font=font,fill='#f1dfae');draw.text((512,y+212),sub,anchor='mm',font=small,fill='#d9cba6')
 atlas.save(OUT/'signs.png')
 wooddir=ROOT/'assets/polyhaven/castle_village/brown_planks_08'
 def materials(m):
  wood=m.material('Owned CC0 weathered oak',[.8,.73,.61],.86,maps={'base':wooddir/'diff.jpg','normal':wooddir/'nor_gl.jpg','arm':wooddir/'arm.jpg'})
  iron=m.material('Worn forged iron',[.07,.08,.075],.53,.75)
  sign=m.material('Hand painted watch quarter signs',[1,1,1],.85,maps={'base':OUT/'signs.png'})
  return wood,iron,sign
 def sign_model(name,index,width=3.4):
  m=assets.Model();wood,iron,paint=materials(m)
  m.box(wood,(-.11,0,-.13),(.11,3.9,.13));m.box(wood,(-.2,3.65,-.18),(width+.1,3.82,.18));m.box(wood,(.35,2.25,-.12),(width,3.45,.12))
  for x in [.55,width-.2]:m.box(iron,(x,3.4,-.03),(x+.035,3.7,.03))
  a=index/4+.012;b=(index+1)/4-.012
  m.quad(paint,[(.4,2.3,.126),(width-.05,2.3,.126),(width-.05,3.4,.126),(.4,3.4,.126)],[(0,b),(1,b),(1,a),(0,a)])
  m.quad(paint,[(width-.05,2.3,-.126),(.4,2.3,-.126),(.4,3.4,-.126),(width-.05,3.4,-.126)],[(0,b),(1,b),(1,a),(0,a)])
  models.append(m.save(name))
 for i,name in enumerate(['armory_sign','grocer_sign','tavern_sign','quarter_sign']):sign_model(name,i)
 add('quarter_armory_sign',-1353,-10327,0)
 add('quarter_grocer_sign',-1300,-10305.8,0)
 add('quarter_tavern_sign',-1294,-10293.7,-90)
 add('quarter_quarter_sign',-1330,-10282.5,0)
 # Household belongings / food displays are one mesh group, reusing source materials.
 # Authoring merge keeps every source texture and triangle; only rigid transforms change.
 from compose_visual_upgrade import geometry
 from build_cinematic_diversity import Writer
 import copy
 parts=[];materials_all=[];textures=[];images=[];source_cache={}
 def copy_prop(stem,x,z,yaw=0,scale=1,y=0):
  p=PROJECT/'models'/(stem+'.glb')
  if not p.exists():p=PROJECT/'models'/(stem+'_near.glb')
  if stem not in source_cache:
   doc,blob,geo=geometry(p);mo=len(materials_all);to=len(textures);io=len(images)
   mats=copy.deepcopy(doc['materials'])
   for mat in mats:
    for key in ['normalTexture','occlusionTexture','emissiveTexture']:
     if key in mat:mat[key]['index']+=to
    for key in ['baseColorTexture','metallicRoughnessTexture']:
     if key in mat.get('pbrMetallicRoughness',{}):mat['pbrMetallicRoughness'][key]['index']+=to
   materials_all.extend(mats)
   for tex in doc.get('textures',[]):
    tex=copy.deepcopy(tex);tex['source']+=io;tex['sampler']=0;textures.append(tex)
   for j,im in enumerate(doc.get('images',[])):
    v=doc['bufferViews'][im['bufferView']];data=blob[v.get('byteOffset',0):v.get('byteOffset',0)+v['byteLength']];dest=OUT/f'source_{io+j}.bin';dest.write_bytes(data);images.append(dict(uri=str(dest),mimeType=im.get('mimeType','image/png')))
   source_cache[stem]=(geo,mo)
  geo,mo=source_cache[stem]
  a=math.radians(yaw);c,s=math.cos(a),math.sin(a);rot=np.array([[c,0,s],[0,1,0],[-s,0,c]])
  for pos,n,uv,idx,mat in geo:parts.append((pos@rot.T*scale+[x,y,z],n@rot.T,uv,idx,mat+mo))
 # Local origin at the plaza, avoids giant world-space vertex coordinates.
 ox,oz=-1310,-10300
 def prop(stem,x,z,yaw=0,scale=1,y=0):copy_prop(stem,x-ox,z-oz,yaw,scale,y)
 for i,x in enumerate([-1340,-1325,-1309,-1293]):
  prop('purchased_village_barrel',x+4,-10318.5,12*i,.6)
  prop('starting_bucket',x-3,-10318.5,16*i,.75)
  prop('purchased_village_crate',x+2.5,-10318.5,10,.5)
 for j,(x,z,stem)in enumerate([(-1301.3,-10308.8,'purchased_goods_bread'),(-1300.7,-10308.8,'purchased_goods_cheese'),(-1301.2,-10307.8,'purchased_goods_bread'),(-1295.8,-10289.2,'purchased_goods_bread'),(-1295.3,-10289.2,'purchased_goods_tankard')]):prop(stem,x,z,j*17,.85,.635)
 # Recessed window assemblies follow measured wall planes, clear of each
 # front door. Keep purchased house geometry intact and share the finish maps.
 from compose_visual_upgrade import facades
 for house in records[:4]:
  stem=house['alias'].replace('purchased_house','purchased_village_house')+'_near'
  front=1 if house['alias']=='purchased_house_b' else -1
  faces=[f for f in facades(PROJECT/'models'/(stem+'.glb'))if f[0]==0 and f[1]==front]
  if not faces:continue
  axis,sign,plane,lo,hi=faces[0];angle=math.radians(house['yaw']);c,s=math.cos(angle),math.sin(angle);scale=house['scale']
  nx,nz=c*sign,-s*sign;yaw=math.degrees(math.atan2(nx,nz))
  for floor,side in [(1.35,1),(3.3,-1)]:
   along=(lo[2]+hi[2])*.5+(hi[2]-lo[2])*.28*side
   wx=house['x']+(c*plane+s*along)*scale+nx*.04;wz=house['z']+(-s*plane+c*along)*scale+nz*.04
   prop('environment_finish/window_open'if side==1 else'environment_finish/window_glazed',wx,wz,yaw,.82*scale,floor*scale)
 # Tavern floor is .015; the original table/bar furniture is already built into its enterable shell.
 prop('purchased_goods_tankard',-1280.8,-10292,0,.9,1.095)
 writer=Writer(materials_all,textures,images)
 groups={}
 for p,n,uv,idx,mat in parts:
  g=groups.setdefault(mat,[[],[],[],[]]);offset=sum(len(v)for v in g[0]);g[0].append(p);g[1].append(n);g[2].append(uv);g[3].append(idx+offset)
 for mat,g in groups.items():writer.mesh('quarter_lived_in_'+str(mat),*[np.concatenate(v)for v in g],mat)
 writer.save(OUT/'frontages.glb',embedded=True);add('quarter_frontages',ox,oz)
 for scratch in OUT.glob('source_*.bin'):scratch.unlink()
 # Preserve every preceding layer and every source ordinal.
 scene=PROJECT/'scenery/demo_completion_instances.txt';base=scene.read_text().split(MARK)[0].rstrip();scene.write_text(base+'\n'+MARK+'\n'+'\n'.join(line(r)for r in records)+'\n')
 total=sum(sum(bool(l.split('#')[0].strip())for l in(PROJECT/'scenery'/f'{n}_instances.txt').read_text().splitlines())for n in FILES)
 assert total<=8192,(total,'record cap')
 studio=PROJECT/'world.studio';text='\n'.join(l for l in studio.read_text().splitlines()if not(l.startswith('asset quarter_')or l.startswith('actor "Quarter ')or l.startswith('light -1284 ')or l.startswith('light -1283.3 ')or l.startswith('light -1294 ')or l.startswith('particle -1 2 1 -1277.55 ')or l.startswith('particle -1 2 1 -1283.3 ')or any(l.startswith('decal '+str(x)+' 24.02 ')for x in [-1340,-1325,-1309,-1293,-1297])))+'\n'
 for name in ['armory_sign','grocer_sign','tavern_sign','quarter_sign','frontages']:text+=f'asset quarter_{name} watch_quarter/{name} 1 0 0 160 40 1\n'
 # Three townspeople; fixed merchants stay at their reachable service points.
 text+='actor "Quarter armorer" -1 -1 2 1 0 1.1 3 0 1 -1357.4 -10326.8\n'
 text+='actor "Quarter grocer" -1 -1 4 1 0 1.1 3 0 1 -1297 -10306\n'
 text+='actor "Quarter innkeeper" -1 -1 4 1 0 1.1 3 -90 1 -1278.7 -10292\n'
 text+='actor "Quarter resident" -1 -1 3 1 1 1.1 3 0 4 -1338 -10314 -1314 -10314 -1314 -10302 -1338 -10302\n'
 text+='light -1283.3 26.82 -10291.6 7 2.1 1 .65 .32 0 -45 180 1\n'
 text+='light -1294 27.2 -10291 6 1.1 1 .72 .4 0 -45 180 1\n'
 text+='particle -1 2 1 -1277.55 24.35 -10280.5 28 .3 .65 .8 1 1 1 1 0 0 31972\n'
 text+='particle -1 2 1 -1283.3 26.82 -10291.6 28.8 .13 .22 .4 1 1 1 .7 0 0 31973\n'
 for x in [-1340,-1325,-1309,-1293]:text+=f'decal {x} 24.02 -10318.8 6 .4 1.5 .13 .10 .055 0 .28 .9 0\n'
 text+='decal -1297 24.02 -10305.8 4 .4 1.4 .13 .10 .055 0 .3 .9 0\n'
 studio.write_text(text)
 (REPORT/'composition.json').write_text(json.dumps(dict(added_records=len(records),total_records=total,limit=8192,placements=records,assets=models,services=['armory','provisions','tavern'],training_court_preserved=True,source_art_unchanged=True),indent=2)+'\n')
 print('Watch quarter:',len(records),'added records;',total,'/8192')
if __name__=='__main__':build()
