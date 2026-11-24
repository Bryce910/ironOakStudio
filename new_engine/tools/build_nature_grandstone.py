#!/usr/bin/env python3
"""Compose the purchased mountain citadel without recooking resident assets.

The first invocation preserves the previous stronghold document. Subsequent
invocations rebuild only this generated pass from that immutable baseline.
"""
from pathlib import Path
import argparse,copy,hashlib,json,math,random,re,shutil,subprocess,tempfile
import numpy as np
from build_starting_area import Atlas
from compose_visual_upgrade import geometry
from build_cinematic_diversity import Writer
from PIL import Image
from import_world_asset import cook

ROOT=Path(__file__).resolve().parents[1];PROJECT=ROOT/'projects/valwood_stronghold'
DATA=ROOT/'assets/purchased/nature_castle/citadel';DOC=ROOT/'docs/stronghold/2026-11-01/citadel'
X=-43344.;Z=-23987.824156518269;FLOOR=1130.8125

def mountain_height(x,z):
 # Same authored ridge profile as the retained hollow mountain shell.
 q=max(0,1-(x/195)**2-((z+83)/178)**2)
 ridge=103*q**.72+15*math.sin(x*.048+z*.018)*q+8*math.sin(x*.12-z*.035)*q
 front=53+12*math.exp(-((x+74)/36)**2)+16*math.exp(-((x-96)/42)**2)+3*math.sin(x*.09)
 t=max(0,min(1,(-z+12)/105));t=t*t*(3-2*t)
 return max(18,front*(1-t)+ridge*t)

def embed_height(x,z,rx,rz):
 return min(mountain_height(x+dx,z+dz)for dx in [-rx,0,rx]for dz in [-rz,0,rz])-4

def mirrored_market():
 path=DATA/'catalog.json';catalog=json.loads(path.read_text());source=next(a for a in catalog if a['name']=='market')
 found=next((a for a in catalog if a['name']=='market_mirrored'),None)
 if found:return
 files=[]
 for label,relative in zip(['near','mid'],source['files']):
  doc,blob,parts=geometry(ROOT/relative);images=copy.deepcopy(doc['images']);folder=DATA/'textures/mirrored_market';folder.mkdir(parents=True,exist_ok=True)
  for i,im in enumerate(images):
   v=doc['bufferViews'][im.pop('bufferView')];start=v.get('byteOffset',0);p=folder/f'{i}.png';p.write_bytes(blob[start:start+v['byteLength']]);im['uri']=str(p)
  writer=Writer(copy.deepcopy(doc['materials']),copy.deepcopy(doc['textures']),images)
  for p,n,uv,index,material in parts:
   p[:,2]*=-1;n[:,2]*=-1;writer.mesh('Mirrored forge arcade',p,n,uv,index[:,[0,2,1]],material)
  target=DATA/f'market_mirrored_{label}.glb';writer.save(target,embedded=True);files.append(target)
 stem=cook(ROOT/'projects/valwood_world','nm_market_mirrored',(files[0],files[1],None),1)
 record=copy.deepcopy(source);record.update(name='market_mirrored',stem=stem,files=[str(p.relative_to(ROOT))for p in files],variant='Purchased complete arcade mirrored across local Z for a matching accessible forge wing')
 catalog.append(record);path.write_text(json.dumps(catalog,indent=2)+'\n')

def residents_and_tour():
 header=ROOT/'engine/world_stronghold_data.h';baseline=DOC/'before-citadel-stronghold-data.h'
 if not baseline.exists():shutil.copy2(header,baseline)
 text=baseline.read_text();pattern=r'(stronghold_stations\[\]=\{\n)(.*?)(\n\};)'
 match=re.search(pattern,text,re.S);assert match
 stations=match[2].splitlines()
 # The royal station moves with its existing throne into the new end chamber.
 for i,z in [(0,-200.3),(3,-200),(4,-200)]:
  fields=stations[i].split(',');fields[2]=f'{Z+z:.12f}';fields[7]=f'{Z+z:.12f}';stations[i]=','.join(fields)
 for i,x in [(3,-19),(4,19)]:
  fields=stations[i].split(',');fields[1]=f'{X+x:.12f}';fields[6]=f'{X+x:.12f}';stations[i]=','.join(fields)
 for i,x,z,ez in [(15,-5,125,190),(16,5,190,125),(18,-18,-160,-193)]:
  fields=stations[i].split(',');fields[1]=fields[6]=f'{X+x:.12f}';fields[2]=f'{Z+z:.12f}';fields[7]=f'{Z+ez:.12f}';stations[i]=','.join(fields)
 extras=[(2,-44,89,90,None),(4,-96,77,-90,None),(3,45,106,-90,None),
         (3,-7,202,0,None),(3,7,202,0,None),(2,-8,121,0,(-8,190)),
         (4,8,190,180,(8,121)),(4,-49,-95,90,None),(2,49,-60,-90,None),(3,7,-162,180,None)]
 for j,(family,x,z,yaw,end)in enumerate(extras):
  ex,ez=end or(x,z)
  stations.append(f'{{{family},{X+x:.12f},{Z+z:.12f},{math.radians(yaw):.8f}f,1.0000f,{j*.23:.5f}f,{X+ex:.12f},{Z+ez:.12f},{str(end is not None).lower()}}},')
 text=text[:match.start(2)]+'\n'.join(stations)+text[match.end(2):]
 route=[(0,265),(0,225),(0,194),(-17,194),(-17,125),(-17,83),(-82,83),(-17,83),
        (0,105),(0,45),(0,2),(0,-40),(5,-75),(5,-130),(5,-175),(0,-190),(0,-197),
        (10,-202),(10,-210),(10,-202),(0,-197),(0,-190),(-5,-175),(-5,-130),(-5,-75),(0,-40),(0,2),(0,45),(17,84),
        (44,100),(17,100),(17,125),(17,194),(0,194),(0,225),(0,265)]
 planned=DOC/'wing-routes.json'
 if planned.exists():
  wing=json.loads(planned.read_text())
  if wing.get('version')==2:
   detour=[]
   for side in ['left','right']:
    points=wing[side];detour+=points+list(reversed(points[:-1]))
   at=route.index((5,-75));route[at:at]=[tuple(p)for p in detour]
 assert len(route)<=64,('Route waypoint budget',len(route))
 route_text='\n'.join(f'{{{X+x:.12f},{Z+z:.12f}}},'for x,z in route)
 text=re.sub(r'(stronghold_route\[\]\[2\]=\{\n).*?(\n\};)',lambda m:m[1]+route_text+m[2],text,flags=re.S)
 if header.read_text()!=text:header.write_text(text)
 return {'actors':len(stations),'patrols':6,'route_local_xz':route,'route_length_m':sum(math.dist(a,b)for a,b in zip(route,route[1:]))}

def mountain_shell():
 source=PROJECT/'models/stronghold/mountain_shell.glb';doc,blob,parts=geometry(source)
 images=copy.deepcopy(doc.get('images',[]));folder=DATA/'textures/mountain';folder.mkdir(parents=True,exist_ok=True)
 for i,im in enumerate(images):
  view=doc['bufferViews'][im.pop('bufferView')];start=view.get('byteOffset',0);p=folder/f'{i}.png';p.write_bytes(blob[start:start+view['byteLength']]);im['uri']=str(p)
 writer=Writer(copy.deepcopy(doc['materials']),copy.deepcopy(doc.get('textures',[])),images)
 removed=0
 for p,n,uv,idx,mat in parts:
  if doc['materials'][mat].get('name')=='Dark granite recess':removed+=len(idx);continue
  writer.mesh('Grandstone mountain',p,n,uv,idx,mat)
 # The central vaults cover the nave. Close the upper side aisles against the
 # mountain mass so exterior tower foundations never appear inside the cave.
 material=copy.deepcopy(doc['materials'][1]);material['name']='Dark excavated mountain interior';material['doubleSided']=True
 material['pbrMetallicRoughness']['baseColorFactor']=[.21,.24,.27,1];writer.g['materials'].append(material);mi=len(writer.g['materials'])-1
 def rock_quad(name,points,normal,uv):
  p=np.array(points);indices=np.array([[0,1,2],[0,2,3]])
  if np.dot(np.cross(p[1]-p[0],p[2]-p[0]),normal)<0:indices=indices[:,[0,2,1]]
  writer.mesh(name,p,np.tile(normal,(4,1)),np.array(uv),indices,mi)
 for side in [-1,1]:
  rock_quad('Upper mountain aisle boundary',[(side*35,17.9,12),(side*35,17.9,-236),(side*35,48,-236),(side*35,48,12)],[-side,0,0],[[0,0],[62,0],[62,7.5],[0,7.5]])
 rock_quad('High excavated mountain ceiling',[(-75,47.5,12),(-75,47.5,-236),(75,47.5,-236),(75,47.5,12)],[0,-1,0],[[0,0],[0,62],[37.5,62],[37.5,0]])
 target=PROJECT/'models/stronghold/nature_mountain_shell.glb';writer.save(target,embedded=True)
 return {'removed_old_low_ceiling_and_back_triangles':removed,'upper_aisle_boundaries_and_high_ceiling':True,'file':str(target.relative_to(ROOT))}

def granite_boulders():
 source=PROJECT/'models/stronghold/snow_boulder.glb';doc,blob,parts=geometry(source)
 images=copy.deepcopy(doc['images']);textures=copy.deepcopy(doc['textures']);materials=copy.deepcopy(doc['materials'])
 folder=DATA/'textures/boulder';folder.mkdir(parents=True,exist_ok=True)
 for i,im in enumerate(images):
  view=doc['bufferViews'][im.pop('bufferView')];start=view.get('byteOffset',0);p=folder/f'source_{i}.png';p.write_bytes(blob[start:start+view['byteLength']]);im['uri']=str(p)
 root=ROOT/'assets/purchased/dwarven_stronghold/textures'
 rough=np.asarray(Image.open(root/'mountain_arm.png').convert('RGB'))[:,:,0]
 Image.fromarray(np.stack([np.full_like(rough,255),rough,np.zeros_like(rough)],axis=2)).save(folder/'granite_arm.png')
 for mat in materials:
  if mat.get('name')!='Natural fractured mountain rock':continue
  mat['pbrMetallicRoughness']['baseColorFactor']=[.91,.95,1,1]
  for key,path,owner in [('baseColorTexture',root/'mountain_base.png',mat['pbrMetallicRoughness']),('normalTexture',root/'mountain_normal.png',mat),('metallicRoughnessTexture',folder/'granite_arm.png',mat['pbrMetallicRoughness'])]:
   images.append({'uri':str(path),'mimeType':'image/png'});textures.append({'source':len(images)-1,'sampler':0});owner[key]={'index':len(textures)-1}
 writer=Writer(materials,textures,images)
 for p,n,uv,idx,material in parts:
  if materials[material].get('name')=='Natural fractured mountain rock':uv=uv*3.2
  writer.mesh('Retextured winter granite',p,n,uv,idx,material)
 target=PROJECT/'models/stronghold/nature_snow_boulder.glb';writer.save(target,embedded=True)
 return str(target.relative_to(ROOT))

def service_wall():
 doc,blob,parts=geometry(PROJECT/'models/stronghold/mountain_foundation.glb')
 images=copy.deepcopy(doc['images']);folder=DATA/'textures/service';folder.mkdir(parents=True,exist_ok=True)
 for i,im in enumerate(images):
  v=doc['bufferViews'][im.pop('bufferView')];start=v.get('byteOffset',0);p=folder/f'{i}.png';p.write_bytes(blob[start:start+v['byteLength']]);im['uri']=str(p)
 writer=Writer(copy.deepcopy(doc['materials']),copy.deepcopy(doc['textures']),images)
 for p,n,uv,idx,mat in parts:
  if 'snow'in doc['materials'][mat].get('name','').lower():continue
  writer.mesh('Dry mountain service wall',p,n,uv,idx,mat)
 writer.save(PROJECT/'models/stronghold/nature_service_wall.glb',embedded=True)

def detailed_throne():
 script=ROOT/'tools/build_grandstone_throne.py';source=PROJECT/'models/stronghold/throne.glb';target=source.with_name('nature_throne.glb');stamp=DATA/'throne-cook.json'
 signature=hashlib.sha256(script.read_bytes()+source.read_bytes()).hexdigest()
 if stamp.exists()and target.exists():
  previous=json.loads(stamp.read_text())
  if previous.get('source_and_recipe_sha256')==signature and previous.get('output_sha256')==hashlib.sha256(target.read_bytes()).hexdigest():return
 with(DOC/'throne-build.log').open('w')as log:subprocess.run(['blender','-b','--threads','2','--python',str(script)],check=True,stdout=log,stderr=subprocess.STDOUT)
 stamp.write_text(json.dumps(dict(source_and_recipe_sha256=signature,output_sha256=hashlib.sha256(target.read_bytes()).hexdigest(),matching_original_collision_bounds=True),indent=2)+'\n')

def interior_shader():
 terrain=PROJECT/'shaders/world_terrain.fs.glsl'
 text=terrain.read_text()
 text=re.sub(r'float hall=[^;]+;', 'float hall=step(abs(p.x),75.)*step(-236.,p.y)*step(p.y,12.);',text)
 text=re.sub(r'float grandstoneHall=[^;]+;', 'float grandstoneHall=step(abs(vPos.x-(-43344.0)),75.)*step(-24223.82415651827,vPos.z)*step(vPos.z,-23975.82415651827);',text)
 if terrain.read_text()!=text:terrain.write_text(text)
 foliage=PROJECT/'shaders/world_foliage.fs.glsl';text=foliage.read_text()
 needle='float coverage=uWinterSurface.x,ice=uWinterSurface.y;'
 replacement=needle+'\n    if(abs(p.x)<72.&&p.z>-236.&&p.z<12.&&p.y<55.)coverage=0.;'
 if replacement not in text:text=text.replace(needle,replacement);foliage.write_text(text)

def project_metadata(report,rows):
 path=PROJECT/'stronghold.json';backup=DOC/'before-citadel-stronghold.json'
 if not backup.exists():shutil.copy2(path,backup)
 metadata=json.loads(backup.read_text());metadata.update(version=3,name=report['name'],bounds=[[-180,-245],[180,298]])
 props=[]
 for row in rows:
  if not row.startswith('prop '):continue
  f=row.split();v=list(map(float,f[2:]));props.append(dict(alias=f[1],x=v[0],z=v[1],yaw=v[2],scale=v[3],offset_y=v[6],sx=v[9],sy=v[10],sz=v[11]))
 metadata['props']=props;metadata['actors']=[]
 body=re.search(r'stronghold_stations\[\]=\{\n(.*?)\n\};',(ROOT/'engine/world_stronghold_data.h').read_text(),re.S)[1]
 for i,row in enumerate(body.splitlines()):
  f=[p.strip()for p in row.strip('{} ,').split(',')];v=[float(p.rstrip('f'))for p in f[:-1]];patrol=f[-1]=='true'
  metadata['actors'].append(dict(family=int(v[0]),x=v[1],z=v[2],yaw=v[3],scale=v[4],phase=v[5],path=[[v[1],v[2]],[v[6],v[7]]]if patrol else[],role='king'if i==0 else'patrol'if patrol else'resident'))
 metadata['route']=[[X+x,Z+z]for x,z in report['residents']['route_local_xz']]
 metadata['hall']=dict(width_m=69,length_m=244,approximate_vault_height_m=46,connected_forge_wings=2)
 metadata.pop('gate_clearance',None);metadata['citadel']=dict(placements=report['placements'],purchased_placements=report['purchased_placements'],zones=report['zones'],route_length_m=report['residents']['route_length_m'],skyline_access='Visual architecture; upper floors are not part of the walking route')
 path.write_text(json.dumps(metadata,indent=2)+'\n')

def main():
 parser=argparse.ArgumentParser();parser.add_argument('--output',type=Path);args=parser.parse_args()
 DOC.mkdir(parents=True,exist_ok=True);baseline=DOC/'before-citadel.world.studio'
 if not baseline.exists():shutil.copy2(PROJECT/'world.studio',baseline)
 mirrored_market();catalog={a['name']:a for a in json.loads((DATA/'catalog.json').read_text())};assert len(catalog)>=39
 rows=baseline.read_text().splitlines();kept=[];props=[];new=[];lights=[];particles=[];pads=[];groups={}
 remove={'gs_floor_tile','gs_mountain_foundation','gs_hall_wall','gs_hall_column','gs_hall_beam','gs_mountain_gate','gs_gate_pier','gs_gate_leaf','gs_mountain_gate_wing_left','gs_mountain_gate_wing_right','gs_brazier','gs_forge'}
 # Clear authored construction pads; preserve the frozen basin and forest edge.
 def footprint(x,z):
  if z<24:return False
  return (abs(x+69)<28 and abs(z-84)<23)or(abs(x-76)<26 and abs(z-117)<45)or(abs(abs(x)-32)<19 and 110<z<195)or(abs(x)<100 and abs(z-213)<12)
 for row in rows:
  kind=row.split()[0]if row.split()else''
  if kind in ['asset','group','light','particle','look']:continue
  if kind=='prop':
   f=row.split();name=f[1];x=float(f[2])-X;z=float(f[3])-Z
   if name in remove:continue
   if name in ['gs_winter_pine','gs_winter_sapling','gs_winter_trunk','gs_winter_sedge','gs_winter_talus','gs_snow_boulder','gs_winter_deadfall','gs_winter_cut_stump']and footprint(x,z):continue
   if name=='gs_throne':f[3]=f'{Z-205:.12f}';row=' '.join(f)
   props.append(row)
  else:kept.append(row)
 def add(name,x,z,yaw=0,scale=1,y=0,sx=1,sy=1,sz=1,zone='Great hall'):
  alias=name if name.startswith(('gs_','starting_','castle_','village_'))else'nm_'+name
  assert .15<=scale<=4 and -128<=y<=128
  record=dict(alias=alias,x=X+x,z=Z+z,yaw=yaw,scale=scale,offset_y=y,sx=sx,sy=sy,sz=sz,zone=zone)
  new.append(record);groups.setdefault(zone,[]).append(len(props)+len(new)-1)
  return record
 def pad(x,z,rx,rz,level=0):pads.append((x,z,rx,rz,level))
 def light(x,z,y=3,radius=22,power=7,color=(1,.55,.23),flicker=True):
  lights.append(f'light {X+x:.9f} {FLOOR+y:.6f} {Z+z:.9f} {radius} {power} '+' '.join(map(str,color))+f' 0 -70 180 {int(flicker)}')
 def fire(x,z,y=2,size=.65):
  particles.append(f'particle -1 2 1 {X+x:.9f} {FLOOR+y:.6f} {Z+z:.9f} {FLOOR+y+9:.6f} .32 1.25 .7 .65 .55 .85 {size} .15 0 {1800+len(particles)} 1 .53 .16 .65')
 def brazier(x,z,scale=1,zone='Great hall',lit=True):
  add('brazier',x,z,scale=scale,zone=zone)
  if lit and len(particles)<16:fire(x,z,2*scale,.55*scale)
 def tower(x,z,levels=2,scale=1,zone='Citadel skyline',base_y=0):
  if base_y:base_y=embed_height(x,z,8.7*scale,8.7*scale)
  add('tower_base',x,z,scale=scale,y=base_y-.15,zone=zone);y=base_y+catalog['tower_base']['dimensions'][1]*scale-.3
  for level in range(levels):add('tower_level',x,z,scale=scale,y=y,zone=zone);y+=catalog['tower_level']['dimensions'][1]*scale-.22
  for level in range(levels):add('tower_ornament',x,z,scale=scale,y=base_y+(5.8+level*8.1)*scale,zone=zone)
  add('tower_top',x,z,scale=scale,y=y,zone=zone)
  add('tower_roof',x,z,scale=scale,y=y+catalog['tower_top']['dimensions'][1]*scale-.55,zone=zone)
  if base_y==0:pad(x,z,11*scale,11*scale)
  return base_y

 # A monumental entrance frames the original terrain portal, with deeper
 # buttress towers and upper bridges embedded into the mountain face.
 add('main_gate',0,25,yaw=90,scale=1.6,zone='Mountain gate')
 for side in [-1,1]:
  add('guardian',side*16.4,39,yaw=180,scale=2.25,zone='Mountain gate')
  tower(side*28,18,levels=3,scale=1.12,zone='Mountain gate')
  add('buttress',side*28,28,yaw=180,scale=1.05,zone='Mountain gate')
  add('hall_column_statue',side*28,28.8,yaw=180,scale=1.05,y=6.1,zone='Mountain gate')
  add('barbican',side*48,13,scale=1.25,yaw=side*14,zone='Mountain gate')
  front_base=tower(side*81,-10,levels=3,scale=1.15,base_y=30)
  brazier(side*10.2,45,1.1,'Mountain gate');light(side*12,34,8,26,10)
  add('hall_front',side*81,1,yaw=180,scale=1.12,y=front_base,zone='Citadel skyline')
 # Fitted construction modules replace the blank wall/floor grid. The nave
 # remains open all the way to the royal end, with side aisles and work bays.
 for z in np.arange(-225.,5.,13.45):
  for x in np.arange(-30.2,31.,6.72):add('floor',float(x),float(z),scale=1.5,y=-.32)
 for side in [-1,1]:
  for j,z in enumerate(np.arange(-211.,-1.,14.0)):
   if z in [-57,-43]:continue
   add('hall_wall'if j%3 else'hall_wall_alt',side*34,z,yaw=0 if side<0 else 180,scale=1.25)
  for z in [-15,-40,-65,-90,-115,-140,-165,-190]:add('hall_column_statue',side*26,z,scale=.76)
  for z in [-22,-61,-100,-139,-178]:
   if z==-61:continue
   add('hall_gallery',side*27,z,yaw=0 if side<0 else 180,scale=1)
  add('arch',side*34,-55.2,yaw=90,scale=1.25,zone='Forge wings')
  for z in [-68.7,-41.7]:add('wall',side*34,z,yaw=90,scale=1.1,zone='Forge wings')
  for z in [-52,-130,-198]:add('wall_ornament',side*32,z,yaw=0 if side<0 else 180,scale=1,y=12)
  add('hall_gallery_stairs',side*26,-119,yaw=0 if side<0 else 180,scale=.62)
  for z in [-18,-70,-122,-174]:brazier(side*23.2,z,1.1,lit=z in [-18,-122])
  for z in [-35,-95,-155]:light(side*19,z,4,31,11)
 for z in [-19,-59,-99,-139,-179]:
  add('hall_vault',0,z,scale=2.6,y=14.4)
  # Measured centers of the four supplied vault springing pads. Column
  # capitals meet the pads, rather than an unrelated repeated floor grid.
  for px in [-5.0192322,4.82403956]:
   for pz in [-6.55174952,6.39823217]:
    add('hall_column',px*2.6,z+pz*2.6,scale=14.4/catalog['hall_column']['dimensions'][1],sx=4,sz=4)
 for side in [-1,1]:
  for z in [-20,-60,-100,-140,-180]:light(side*8,z,6,29,30,color=(1,.60,.27))
  for z in [-30,-90,-150]:add('gs_banner',side*21,z,yaw=90 if side<0 else -90,scale=.9,y=7)
 light(0,-105,28,40,25,color=(.50,.61,.82),flicker=False)
 add('hall_end',0,-215,scale=2,zone='Royal sanctuary')
 add('altar_roof',0,-213,scale=2,y=30,zone='Royal sanctuary')
 add('hall_front',0,-214.5,yaw=180,scale=1.3,zone='Royal sanctuary')
 for x in [-10,10]:
  add('guardian',x,-196,yaw=180,scale=1.8,zone='Royal sanctuary');brazier(x,-188,1.2,'Royal sanctuary')
  add('stair',x,-207,yaw=180,scale=1.25,zone='Royal sanctuary')
  height=catalog['stair']['dimensions'][1]*1.25
  add('floor',x,-210.5,scale=1.5,y=height-catalog['floor']['dimensions'][1]*1.5,sx=1.075,sz=.30,zone='Royal sanctuary')
 light(0,-200,9,35,12,color=(.53,.65,1),flicker=False)
 light(-3,-196,4,12,18,color=(1,.67,.36),flicker=False)
 light(0,-115,13,35,5,color=(.50,.61,1),flicker=False)
 # A side forge arcade and clan lodgings give the hall occupied edges.
 # Each doorway opens into a shallow alcove rather than an empty blank wall.
 for side in [-1,1]:
  for z in [-36,-82,-130,-176]:
   add('arch',side*27,z,yaw=90,scale=.8)
   add('fireplace',side*29,z,scale=.52,yaw=90 if side<0 else -90)
   add('gs_workbench',side*24,z-6,yaw=90)
   add('gs_anvil',side*22,z+4,scale=1.2)
 # Complete furnished arcades fill the connected lateral forge wings.
 for side in [-1,1]:
  add('market'if side<0 else'market_mirrored',side*53,-82,yaw=90,scale=1.2,zone='Forge wings')
  for z in [-113,-99.55,-86.1,-72.65,-59.2]:
   for x in [side*43,side*49.72,side*56.44,side*63.16]:add('floor',x,z,scale=1.5,y=-.32,zone='Forge wings')
  for z in [-61,-99]:
   add('fireplace',side*59,z,yaw=90 if side<0 else-90,scale=.65,zone='Forge wings');fire(side*58,z,1.2,.55)
   add('gs_anvil',side*50.5,z+4,zone='Forge wings');add('gs_workbench',side*48,z-6,yaw=90,zone='Forge wings')
  brazier(side*60,-78,zone='Forge wings');light(side*53,-82,5,29,27)
  for z in [-68,-91]:
   add('gs_banner',side*62.5,z,yaw=90 if side<0 else-90,scale=.85,y=3.2,zone='Forge wings')
   add('starting_crate',side*62,z-2,scale=.85,zone='Forge wings')
  for x in [61.2,63.2]:add('starting_barrel',side*x,-73,scale=.95,zone='Forge wings')
  add('gs_workbench',side*47,-87,yaw=90,zone='Forge wings')
  add('sculpture',side*47,-87,scale=1.4,y=1.1,zone='Forge wings')
 # A physical service wall bounds the inhabited chambers beneath the rock.
 for side in [-1,1]:
  for z in range(-223,6,24):add('gs_mountain_foundation',side*77,z,yaw=90,sx=4,sy=4,zone='Mountain interior')
 for x in range(-66,67,24):add('gs_mountain_foundation',x,-239,sx=4,sy=4,zone='Mountain interior')
 # The lower town is a connected, snowy defensive ward with a market hall,
 # paired lodgings, a barracks court and a gate at its far end.
 tower(-87,210,levels=2,scale=.88,zone='Lower town')
 tower(87,210,levels=2,scale=.88,zone='Lower town')
 add('outer_gate',0,210,scale=1.55,zone='Lower town');pad(0,210,15,18)
 for side in [-1,1]:
  for x in [22,38,54,70]:
   add('wall',side*x,210,yaw=0,scale=1.7,zone='Lower town')
   add('parapet',side*x,210,scale=2.1,y=13.2,zone='Lower town')
   pad(side*x,210,9,4)
  for z in [129,168]:
   x=side*27
   # The supplied complete arcade has a fitted roof and coherent interiors.
   # Smaller variants form the clan lodgings around the residential courts.
   add('market',x,z,yaw=90 if side<0 else -90,scale=.36,zone='Lower town')
   pad(x,z,10,14)
   add('gs_workbench',x-side*7,z-6,yaw=90,zone='Lower town')
   add('gs_banner',x-side*5,z+6,yaw=90 if side<0 else -90,scale=.6,zone='Lower town')
  for z in [62,108,151,192]:brazier(side*13,z,.95,'Lower town')
  for z in [140,176]:
   add('starting_barrel',side*21,z,scale=.8,zone='Lower town')
   add('starting_crate',side*22,z+1.5,scale=.7,zone='Lower town')
 add('market',-69,83,yaw=180,scale=.78,zone='Market hall');pad(-69,83,28,22)
 add('market',76,123,yaw=90,scale=.5,zone='Barracks court');pad(76,123,12,17)
 tower(91,70,levels=2,scale=.83,zone='Barracks court')
 for z in [73,96]:
  add('market',72,z,yaw=-90,scale=.32,zone='Barracks court');pad(72,z,10,13)
 for x,z in [(-45,66),(-45,76),(-45,98),(48,86),(48,105)]:
  add('gs_workbench',x,z,yaw=90,zone='Market hall');add('gs_anvil',x+2,z+2,zone='Market hall')
 for x,z in [(-78,75),(-75,75),(-70,75),(-64,75),(-59,75),(-78,91),(-73,91),(-68,91)]:
  add('starting_crate'if int(x)%2 else'starting_barrel',x,z,scale=.85,zone='Market hall')
 for x,z in [(-33,75),(34,110),(-34,151),(33,184)]:
  add('guardian',x,z,yaw=180,scale=1.15,zone='Lower town')
 # Crenellated retaining walls and a second ridge line create a distant
 # architectural silhouette beyond the first hall, rather than one isolated gate.
 for side in [-1,1]:
  rear_base=tower(side*68,-176,levels=3,scale=1.08,base_y=62)
  tower(side*109,-133,levels=2,scale=.92,base_y=65)
  add('hall_front',side*68,-166,yaw=180,scale=1.02,y=rear_base,zone='Citadel skyline')
 # The district uses curated winter conifers, replacing the ordinary green
 # procedural woodland around the new streets and roofs.
 rng=random.Random(81029);planted=[]
 for _ in range(2000):
  if len(planted)>=34:break
  x=rng.uniform(-151,151);z=rng.uniform(182,283)
  if abs(x)<24 or(abs(x)<109 and z<230):continue
  if any(math.hypot(x-a,z-b)<10 for a,b in planted):continue
  planted.append((x,z));scale=rng.uniform(.68,1.04)
  add('gs_winter_pine',x,z,yaw=rng.uniform(-180,180),scale=scale,zone='Winter woodland')
  add('gs_winter_trunk',x,z,scale=scale,zone='Winter woodland')
  if len(planted)%2:
   add('gs_winter_sapling',x+3.8,z-3.6,yaw=rng.uniform(-180,180),scale=.75,zone='Winter woodland')
   add('gs_winter_trunk',x+3.8,z-3.6,scale=.34,zone='Winter woodland')

 # A continuous buried construction pad avoids floating foundations. Keep
 # the existing wind-shaped drifts between the buildings and around the pool.
 terrain=[]
 with tempfile.TemporaryDirectory(prefix='grandstone-citadel-')as temp:
  atlas=Atlas(temp)
  for row in kept:
   if not row.startswith('terrain '):terrain.append(row);continue
   f=row.split();tx,tz=map(int,f[1:3]);values=list(map(float,f[3:]));assert len(values)==4225
   for j in range(65):
    for i in range(65):
     wx=tx*128+i*2;wz=tz*128+j*2;lx=wx-X;lz=wz-Z;old=atlas.sample(wx,wz).height+values[j*65+i]
     target=old;blend=0
     if abs(lx)<75 and -235<lz<12:target=FLOOR;blend=1
     for px,pz,rx,rz,level in pads:
      distance=max(abs(lx-px)-rx,abs(lz-pz)-rz)
      w=max(0,min(1,(7-distance)/7));w=w*w*(3-2*w)
      if w>blend:blend=w;target=FLOOR+level
     # Ordinary walking samples the same continuous support beneath the
     # ceremonial steps and their two small upper landings.
     for sx in [-10,10]:
      wx=max(0,min(1,(4.6-abs(lx-sx))/1.1))
      front=max(0,min(1,(-205-lz)/4.0));rear=max(0,min(1,(lz+216)/4.0))
      step=catalog['stair']['dimensions'][1]*1.25*min(front,rear)*wx
      if step>0:target=FLOOR+step;blend=1
     # Outer tile edges must remain exactly shared with their neighbors.
     if blend:values[j*65+i]+=(target-old)*blend
   terrain.append('terrain '+str(tx)+' '+str(tz)+' '+' '.join(f'{v:.9g}'for v in values))
 kept=terrain
 # Place the shell after editing its obsolete low interior ceiling/back plane.
 shell=mountain_shell();boulder=granite_boulders();service_wall();interior_shader()
 detailed_throne()
 used={p.split()[1]for p in props}|{p['alias']for p in new};definitions=[]
 for row in rows:
  if row.startswith('asset '):
   f=row.split()
   # Base castle/jungle source files also refer to these definitions, even
   # when their objects are outside the currently visible district.
   definitions.append(row.replace('stronghold/mountain_shell ','stronghold/nature_mountain_shell ').replace('stronghold/snow_boulder ','stronghold/nature_snow_boulder ').replace('stronghold/mountain_foundation ','stronghold/nature_service_wall ').replace('stronghold/throne ','stronghold/nature_throne '))
 for name,asset in catalog.items():
  if 'nm_'+name not in used:continue
  collision=0 if name in ['floor','hall_vault','altar_roof','roof','tower_roof','parapet','torch','sculpture','stair','hall_stairs','bridge_pier']else 4
  distance=950 if name in ['tower_level','tower_top','tower_roof','hall_front','main_gate','outer_gate','bridge','market']else 480
  definitions.append(f'asset nm_{name} {asset["stem"]} 2 {collision} 0 {distance} 80 1')
 for p in new:
  props.append(f'prop {p["alias"]} {p["x"]:.9f} {p["z"]:.9f} {p["yaw"]} {p["scale"]} 0 0 {p["offset_y"]} 0 0 {p["sx"]} {p["sy"]} {p["sz"]}')
 assert len(props)<=1024,(len(props),'Studio placement budget');assert len(definitions)<=128,len(definitions);assert len(lights)<=24;assert len(particles)<=16
 assert len(props)+len(lights)+len(particles)<=1024,'Combined authoring entity budget exceeded'
 group_rows=[]
 for name,indices in groups.items():
  for start in range(0,len(indices),128):
   chunk=indices[start:start+128];label=name if len(indices)<=128 else f'{name} {start//128+1}'
   group_rows.append(f'group "{label}" {len(chunk)} '+' '.join(str(8192+i)for i in chunk))
 text='\n'.join([kept[0],'look .25 1.02 .97 1.02 9.0 28 .28 .24 .38 .22',*kept[1:],*props,*definitions,*lights,*particles,
       *group_rows])+'\n'
 target=args.output or PROJECT/'world.studio';target.write_text(text)
 residents=residents_and_tour()
 report=dict(version=3,name='Grandstone | Mountain Citadel',origin=[X,Z],floor=FLOOR,production_document=str(target),baseline_sha256=hashlib.sha256(baseline.read_bytes()).hexdigest(),placements=len(props),new_placements=len(new),purchased_placements=sum(p['alias'].startswith('nm_')for p in new),asset_definitions=len(definitions),lights=len(lights),emitters=len(particles),zones={k:len(v)for k,v in groups.items()},new_props=new,construction_pads=pads,mountain=shell,retextured_boulder=boulder,residents=residents,source='Purchased NatureManufacture Modular Castle & Dungeon with retained snow/forest/ice sources')
 (DOC/'layout.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({k:v for k,v in report.items()if k not in ['new_props','construction_pads']},indent=2))
 if args.output is None:
  (PROJECT/'nature_citadel.json').write_text(json.dumps({k:v for k,v in report.items()if k not in ['new_props','construction_pads']},indent=2)+'\n')
  project_metadata(report,text.splitlines())

if __name__=='__main__':main()
