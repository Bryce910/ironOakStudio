#!/usr/bin/env python3
"""Stage a bounded first quest in the private v16-derived opening candidate."""
from pathlib import Path
import copy,hashlib,io,json,math,struct,subprocess,sys
import numpy as np
from PIL import Image,ImageDraw,ImageFont
from build_city_countryside import readfield,sample
from build_village_showcase_project import glb_geometry

ROOT=Path(__file__).resolve().parents[1]
BASE=ROOT/'build/journey-opening';PROJECT=BASE/'candidate-v1/project'
ART=BASE/'art-v1';EVIDENCE=BASE/'scene-preflight'
SOURCE=ROOT/'build/opening-finward-reference/v16/project'
WIDTH=100800.;DEPTH=WIDTH*2818/5009
CONFIG=dict(camp=[-1486.37664868499,18.933204601097398,-10224.910611064201],
 rowan=[-1488.5266486849901,18.93575044238202,-10225.5606110642],
 bram=[-1484.1766486849899,18.945669940985017,-10225.5606110642],
 encounter=[-1404.0077270925497,18.944883621506374,-10212.922994171895],
 supplies=[-1397.3368877436963,19.918828885345754,-10208.085637873315],
 retry=[-1407.1181674027353,19.065666597715865,-10214.754353427777])

def sha(path):
 with Path(path).open('rb')as f:return hashlib.file_digest(f,'sha256').hexdigest()
def write_json(path,value):path.write_text(json.dumps(value,indent=2,allow_nan=False)+'\n')
def values(items):return ' '.join(format(float(v),'.17g')for v in items)
def read_glb(path):
 raw=path.read_bytes();n=struct.unpack_from('<I',raw,12)[0]
 return json.loads(raw[20:20+n]),raw[28+n:]
def make_sign(source,target):
 """Keep the owned crate's encoded PBR maps; add newly typeset lettering."""
 original,oldbin=read_glb(source);doc=dict(asset=dict(version='2.0',generator='Valwood first-journey sign'),scene=0,scenes=[dict(nodes=[0])],nodes=[dict(mesh=0)],meshes=[],buffers=[],bufferViews=[],accessors=[],materials=copy.deepcopy(original['materials']),images=[],textures=copy.deepcopy(original['textures']),samplers=copy.deepcopy(original.get('samplers',[])))
 binary=bytearray()
 def view(raw,target=None):
  binary.extend(b'\0'*((-len(binary))%4));item=dict(buffer=0,byteOffset=len(binary),byteLength=len(raw));binary.extend(raw)
  if target:item['target']=target
  doc['bufferViews'].append(item);return len(doc['bufferViews'])-1
 for image in original['images']:
  v=original['bufferViews'][image['bufferView']];raw=oldbin[v.get('byteOffset',0):v.get('byteOffset',0)+v['byteLength']]
  doc['images'].append(dict(bufferView=view(raw),mimeType=image['mimeType']))
 image=Image.new('RGBA',(1024,512),(0,0,0,0));draw=ImageDraw.Draw(image)
 fontpath=Path('/usr/share/fonts/truetype/dejavu/DejaVuSerif-Bold.ttf')
 for text,y,size in [('THE COPPER',30,95),('LANTERN',158,116),('FOOD  /  BEDS',345,52)]:
  font=ImageFont.truetype(str(fontpath),size);box=draw.textbbox((0,0),text,font=font);draw.text(((1024-(box[2]-box[0]))/2,y),text,font=font,fill=(245,216,155,255),stroke_width=1)
 stream=io.BytesIO();image.save(stream,format='PNG');image_index=len(doc['images']);doc['images'].append(dict(bufferView=view(stream.getvalue()),mimeType='image/png'))
 texture_index=len(doc['textures']);doc['textures'].append(dict(source=image_index))
 doc['materials'].append(dict(name='New Copper Lantern lettering',pbrMetallicRoughness=dict(baseColorTexture=dict(index=texture_index),metallicFactor=0,roughnessFactor=.9),alphaMode='MASK',alphaCutoff=.5,doubleSided=True))
 vertices=[[],[]];normals=[[],[]];uvs=[[],[]];tangents=[[],[]];indices=[[],[]]
 def quad(points,normal,material=0):
  base=len(vertices[material]);vertices[material].extend(points);normals[material].extend([normal]*4);uvs[material].extend([[0,1],[1,1],[1,0],[0,0]]);tangents[material].extend([[1,0,0,1]]*4);indices[material].extend([base,base+1,base+2,base,base+2,base+3])
 def box(lo,hi):
  x,y,z=lo;X,Y,Z=hi
  for points,n in [([[x,y,Z],[X,y,Z],[X,Y,Z],[x,Y,Z]],[0,0,1]),([[X,y,z],[x,y,z],[x,Y,z],[X,Y,z]],[0,0,-1]),([[x,y,z],[x,y,Z],[x,Y,Z],[x,Y,z]],[-1,0,0]),([[X,y,Z],[X,y,z],[X,Y,z],[X,Y,Z]],[1,0,0]),([[x,Y,Z],[X,Y,Z],[X,Y,z],[x,Y,z]],[0,1,0]),([[x,y,z],[X,y,z],[X,y,Z],[x,y,Z]],[0,-1,0])]:quad(points,n)
 box([-.065,0,-.065],[.065,2.28,.065]);box([-.89,1.31,-.065],[.89,2.18,.065]);box([-.94,1.25,-.11],[.94,1.34,.11]);box([-.94,2.15,-.11],[.94,2.24,.11]);box([-.94,1.25,-.11],[-.85,2.24,.11]);box([.85,1.25,-.11],[.94,2.24,.11])
 quad([[-.84,1.34,.067],[.84,1.34,.067],[.84,2.15,.067],[-.84,2.15,.067]],[0,0,1],1)
 def accessor(data,kind,index=False):
  data=np.asarray(data,dtype='<u4'if index else'<f4');v=view(data.tobytes(),34963 if index else 34962);a=dict(bufferView=v,componentType=5125 if index else 5126,count=len(data),type=kind)
  if not index:a.update(min=data.min(0).tolist(),max=data.max(0).tolist())
  doc['accessors'].append(a);return len(doc['accessors'])-1
 primitives=[]
 for material in [0,1]:
  attrs={name:accessor(data,kind)for name,data,kind in [('POSITION',vertices[material],'VEC3'),('NORMAL',normals[material],'VEC3'),('TANGENT',tangents[material],'VEC4'),('TEXCOORD_0',uvs[material],'VEC2')]}
  primitives.append(dict(attributes=attrs,indices=accessor(indices[material],'SCALAR',True),material=0 if material==0 else len(doc['materials'])-1))
 doc['meshes']=[dict(primitives=primitives)];doc['buffers']=[dict(byteLength=len(binary))]
 raw=json.dumps(doc,separators=(',',':')).encode();raw+=b' '*((-len(raw))%4);binary+=b'\0'*((-len(binary))%4)
 target.write_bytes(struct.pack('<III',0x46546c67,2,28+len(raw)+len(binary))+struct.pack('<II',len(raw),0x4e4f534a)+raw+struct.pack('<II',len(binary),0x004e4942)+binary)
 return dict(source_wood_model=str(source.resolve()),source_wood_sha256=sha(source),source_images_copied_bit_exact=True,lettering='Newly authored Copper Lantern sign',font=str(fontpath),font_sha256=sha(fontpath),font_license='/usr/share/doc/fonts-dejavu-core/copyright',triangles=74)

def stage():
 assert PROJECT.is_dir()and not PROJECT.is_symlink()
 assert not ART.exists(),'Use a fresh art revision'
 studio=PROJECT/'world.studio';original=studio.read_text();assert 'nm_journey_'not in original
 assert sum(r.startswith('asset ')for r in original.splitlines())+6<=128
 old={str(p):sha(p)for p in [SOURCE/'world.studio',SOURCE/'opening.config',SOURCE/'graphics.settings',*list((SOURCE/'saves').glob('*'))]if p.is_file()}
 primary=readfield(PROJECT/'ground/showcase.height');country=readfield(PROJECT/'ground/countryside.height')
 def height(x,z):
  p=np.array([x,z]);a,o,s=primary
  if not(np.all(p>=o)and np.all(p<=o+s*(np.array(a.shape)[::-1]-1))):a,o,s=country
  return float(sample(a,p,o,s))
 def feet(p):return [float(p[0]),height(*p),float(p[1])]
 origin=np.array(json.loads((PROJECT/'city_showcase.json').read_text())['origin']);road=np.load(ROOT/'assets/purchased/bep_reforged/countryside/landscape-review.npz')['road_native']+origin[[0,2]]
 # Walking fields deliberately sit5cm below the actual source paving. Actors
 # stand on its exact rendered triangles while player routes use support Y.
 from check_village_source_collision import exact_ground
 ground_source=ROOT.parent/'.local-tools/village-source/source-ground-native.npz'
 ground_data=np.load(ground_source)
 def visible_city_feet(p):return [float(p[0]),float(exact_ground(ground_data['vertices'],ground_data['triangles'],p[0]-origin[0],p[1]-origin[2])+origin[1]),float(p[1])]
 config=copy.deepcopy(CONFIG);config['town']=visible_city_feet([-1306.4,-10202.584747454583])
 people=[config['town'],visible_city_feet([-1303.4,-10203.0]),visible_city_feet([-1307.1,-10199.2])]
 connector=np.array(json.loads((ROOT/'docs/arrival-camp/2026-11-04/art/ART_CANDIDATE.json').read_text())['connection_world'])[:,[0,2]]
 bram=np.array([config['bram'][0],config['bram'][2]+1.6]);rowan=np.array([config['rowan'][0],config['rowan'][2]+1.6]);retry=np.array(config['retry'])[[0,2]];pickup=np.array(config['supplies'])[[0,2]]+[-1.3,-.6]
 paths=dict(loan=np.vstack((connector[0],bram)),encounter=np.vstack((bram,connector,road[450:191:-12],retry)),satchel=np.vstack((retry,[-1406,-10210],[-1400,-10207.5],pickup)),**{'return':np.vstack((pickup,[-1400,-10207.5],[-1406,-10210],road[180],road[192:451:12],road[450],connector[::-1],rowan))},town=np.vstack((rowan,connector,road[450::-12],road[0],[-1359.1756065369,-10201.834747455],[-1324.9,-10213.334747455],[-1298.4,-10212.584747455],[-1298.4,-10202.584747455],[-1304.4,-10202.584747455],[-1304.4,-10200.8],[-1305.8,-10200.7])))
 dense={}
 for name,path in paths.items():
  points=[path[0]]
  for a,b in zip(path,path[1:]):
   for f in np.linspace(0,1,max(2,math.ceil(np.linalg.norm(b-a)/10)+1))[1:]:points.append(a+(b-a)*f)
  dense[name]=[feet(p)for p in points]
 ART.mkdir();EVIDENCE.mkdir(exist_ok=True)
 sign=ART/'copper_lantern_sign.glb';sign_provenance=make_sign(PROJECT/'models/purchased_village_crate_near.glb',sign)
 link=PROJECT/'models/journey_copper_lantern_sign.glb';assert not link.exists();link.symlink_to(sign)
 cart='imports/nm_city_34_sm_wooden_cart/21d86a5642a01aedd96fb7e1/asset'
 asset_specs=[('nm_journey_crate','purchased_village_crate',2,1),('nm_journey_satchel','purchased_prop_sack',2,0),('nm_journey_stock_sack','purchased_prop_sack',2,1),('nm_journey_cart',cart,2,1),('nm_journey_lamp','purchased_prop_lamp',2,1),('nm_journey_inn_sign','journey_copper_lantern_sign',1,1)]
 rows=[f'asset {a} {stem} {lod} {collision} 0 180 40 1'for a,stem,lod,collision in asset_specs]
 placements=[]
 def prop(alias,x,z,yaw,scale,offset=0):
  rows.append('prop '+alias+' '+values([x,z,yaw,scale,0,0,offset,0,0,1,1,1]));placements.append(dict(alias=alias,feet=feet([x,z]),yaw=yaw,scale=scale,offset_y=offset))
 prop('nm_journey_cart',-1409.5,-10205.0,58,.68)
 prop('nm_journey_crate',-1394,-10198.5,17,.75)
 prop('nm_journey_crate',-1393.3,-10198.7,-9,.55)
 prop('nm_journey_satchel',*np.array(config['supplies'])[[0,2]],24,.68)
 prop('nm_journey_stock_sack',-1393.95,-10198.45,-12,.55,.75*.9931)
 prop('nm_journey_lamp',-1409.4,-10208.1,-40,.72)
 prop('nm_journey_crate',-1302.4,-10205.8,9,.65)
 prop('nm_journey_stock_sack',-1302.4,-10205.8,21,.52,.65*.9931)
 prop('nm_journey_inn_sign',-1306.55,-10203.9,0,1)
 prop('nm_journey_lamp',-1303.4,-10205.8,0,.8)
 for x,z,power in [(-1409.4,-10208.1,.55),(-1303.4,-10205.8,.8)]:rows.append('light '+values([x,height(x,z)+2,z,4,power,1,.66,.34,0,-75,180,0]))
 # Validate the final paths through the real, read-only source UCX/height service.
 from check_village_source_collision import native_checker
 checker_path=EVIDENCE/'checker-path.txt';checker=Path(checker_path.read_text().strip())if checker_path.exists()else native_checker()
 audits={}
 for name,path in dense.items():
  walk=EVIDENCE/(name+'-final.walk');walk.write_text('VALWOOD_CITY_WALK 1\n'+''.join(f'p{i} {x:.15g} {z:.15g}\n'for i,(x,y,z)in enumerate(path)))
  run=subprocess.run([str(checker),str(PROJECT/'city.physics'),str(PROJECT/'ground/showcase.height'),str(walk),str(PROJECT/'ground/countryside.height')],capture_output=True,text=True,check=False)
  result=json.loads(run.stdout);audits[name]=dict(passed=run.returncode==0,result=result,stderr=run.stderr,walk_sha256=sha(walk));assert run.returncode==0,(name,run.stdout,run.stderr)
 # New props remain outside every walking centerline, except the collectible
 # whose pickup route intentionally stops1.43m away. Conservative circle test.
 specs={a:stem for a,stem,lod,c in asset_specs};clearances=[]
 for p in placements:
  stem=specs[p['alias']];path=PROJECT/'models'/(stem+('_near.glb'if p['alias']!='nm_journey_inn_sign'else'.glb'))
  if not path.exists():path=PROJECT/'models'/(stem+'.glb')
  verts,_=glb_geometry(path);lo,hi=verts.min(0),verts.max(0);yaw=math.radians(p['yaw']);rot=np.array([[math.cos(yaw),math.sin(yaw)],[-math.sin(yaw),math.cos(yaw)]])
  corner=np.array([[x,z]for x in [lo[0],hi[0]]for z in [lo[2],hi[2]]])*p['scale'];corner=corner@rot.T+np.array(p['feet'])[[0,2]];center=corner.mean(0);radius=np.linalg.norm(corner-center,axis=1).max();minimum=1e9
  for name,path in dense.items():
   if p['alias']=='nm_journey_satchel'and name=='town':continue
   q=np.asarray(path)[:,[0,2]];a,b=q[:-1],q[1:];d=b-a;f=np.clip(((center-a)*d).sum(1)/np.maximum((d*d).sum(1),1e-20),0,1);minimum=min(minimum,float(np.linalg.norm(center-(a+d*f[:,None]),axis=1).min()-radius-.42))
  clearances.append(dict(alias=p['alias'],feet=p['feet'],minimum_conservative_capsule_clearance=minimum))
  assert minimum>.10,(p,minimum)
 text=['VALWOOD_FIRST_JOURNEY 1']+[name+' '+values(config[name])for name in ['camp','rowan','bram','encounter','supplies','retry','town']]+['radii 2.7 4 2.1 3.2','end']
 (PROJECT/'opening/first-journey.settings').write_text('\n'.join(text)+'\n')
 write_json(PROJECT/'opening/first-journey.routes.json',dict(schema='valwood.first-journey-routes',version=1,paths=dense))
 people_rows=['VALWOOD_FIRST_JOURNEY_PEOPLE 1']
 for i,p in enumerate(people):
  yaw=math.pi/2 if i==0 else math.atan2(people[0][0]-p[0],people[0][2]-p[2]);people_rows.append('actor '+str(i)+' '+values(p+[yaw,0]))
 (PROJECT/'opening/first-journey.town-people.settings').write_text('\n'.join(people_rows+['end'])+'\n')
 studio.write_text(original.rstrip()+'\n'+'\n'.join(rows)+'\n')
 assert all(sha(Path(p))==h for p,h in old.items())
 sources=[]
 for alias,stem,lods,c in asset_specs:
  for suffix in (['_near.glb','_mid.glb']if lods==2 else['.glb']):
   path=PROJECT/'models'/(stem+suffix)
   if not path.exists()and suffix=='_near.glb':path=PROJECT/'models'/(stem+'.glb')
   sources.append(dict(alias=alias,path=str(path.resolve()),sha256=sha(path),license='User-owned Medieval Village purchased asset; private development, no open-redistribution claim'if alias!='nm_journey_inn_sign'else'New sign geometry/type, using owned crate PBR images unchanged'))
 manifest=dict(ready=True,project=str(PROJECT),config=config,town_people=people,frontage=dict(source_asset='SM_Shop_02',catalog_placement=154,designated_venue='The Copper Lantern',new_physical_sign=True,legacy_empty_town_endpoint_used=False),routes=audits,new_props=placements,new_prop_clearance=clearances,sign=dict(path=str(sign),sha256=sha(sign),provenance=sign_provenance),source_prop_inventory=sources,source_project_guard=old,source_project_unchanged=True,settings=[dict(path=str(PROJECT/n),sha256=sha(PROJECT/n))for n in ['world.studio','opening/first-journey.settings','opening/first-journey.routes.json','opening/first-journey.town-people.settings']],native_gpu_used=False,limits=['CPU-only source UCX/slope and new-prop conservative clearance; root must review native walking, all3people, sign readability, lighting and final dialogue framing.'])
 write_json(ART/'STAGING.json',manifest);print(json.dumps(dict(ready=True,manifest=str(ART/'STAGING.json'),town=config['town'],routes={n:r['result']for n,r in audits.items()}),indent=2))

if __name__=='__main__':stage()
