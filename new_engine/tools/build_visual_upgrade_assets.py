#!/usr/bin/env python3
"""Deterministic derivatives for the isolated visual project; source art is read-only.
Powered by Poly Haven. No AI textures, original art edits, or animation rebakes.
"""
from pathlib import Path
import copy,hashlib,io,json,math,struct
import numpy as np
from PIL import Image
ROOT=Path(__file__).resolve().parents[1]
MODELS=ROOT/'projects/valwood_world/models'
OUT=MODELS/'visual_upgrade';OUT.mkdir(exist_ok=True)
PROJECT=ROOT/'projects/valwood_visual'
REPORT=ROOT/'docs/visual-upgrade/2026-10-30'

def sha(b):return hashlib.sha256(b).hexdigest()
def read_glb(path):
 raw=path.read_bytes();assert struct.unpack_from('<II',raw)==(0x46546c67,2)
 n=struct.unpack_from('<I',raw,12)[0];doc=json.loads(raw[20:20+n]);offset=28+n
 return doc,raw[offset:],raw

def save_glb(path,doc,blob):
 d=copy.deepcopy(doc);d['buffers']=[dict(byteLength=len(blob))];j=json.dumps(d,separators=(',',':')).encode();j+=b' '*(-len(j)%4);b=bytes(blob);b+=b'\0'*(-len(b)%4)
 raw=struct.pack('<III',0x46546c67,2,28+len(j)+len(b))+struct.pack('<II',len(j),0x4e4f534a)+j+struct.pack('<II',len(b),0x004e4942)+b
 path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(raw);return raw

def encoded(image,fmt='PNG'):
 b=io.BytesIO();image.save(b,format=fmt,**({'quality':95,'subsampling':0}if fmt=='JPEG'else{'compress_level':6}));return b.getvalue()

def replace_images(doc,blob,replacements):
 """Repack views, retaining every vertex/index/animation byte unchanged."""
 doc=copy.deepcopy(doc);old_images={im['bufferView']for im in doc.get('images',[])};mapping={};out=bytearray();views=[]
 for i,v in enumerate(doc['bufferViews']):
  if i in old_images:continue
  n=copy.deepcopy(v);n['byteOffset']=len(out);out.extend(blob[v.get('byteOffset',0):v.get('byteOffset',0)+v['byteLength']]);out.extend(b'\0'*(-len(out)%4));mapping[i]=len(views);views.append(n)
 for a in doc.get('accessors',[]):
  if 'bufferView'in a:a['bufferView']=mapping[a['bufferView']]
 for i,im in enumerate(doc.get('images',[])):
  original=next(v for k,v in enumerate(doc['bufferViews'])if k==im['bufferView']);data=replacements.get(i,blob[original.get('byteOffset',0):original.get('byteOffset',0)+original['byteLength']])
  im['bufferView']=len(views);im['mimeType']='image/jpeg'if data[:2]==b'\xff\xd8'else'image/png';im.pop('uri',None);views.append(dict(buffer=0,byteOffset=len(out),byteLength=len(data)));out.extend(data);out.extend(b'\0'*(-len(out)%4))
 doc['bufferViews']=views;return doc,out

def proxy_models(overrides):
 target=PROJECT/'models'
 if target.is_symlink():target.unlink();target.mkdir()
 assert target.is_dir()and not target.is_symlink()
 for p in MODELS.iterdir():
  if p.name.startswith('visual_'):continue
  dest=target/p.name
  if p.name in overrides:continue
  if not dest.exists()and not dest.is_symlink():dest.symlink_to(p.resolve(),target_is_directory=p.is_dir())
 for name,path in overrides.items():
  dest=target/name
  if dest.is_symlink():dest.unlink()
  if name=='animations':
   dest.mkdir(exist_ok=True)
   for p in (MODELS/'animations').iterdir():
    q=dest/p.name
    if q.is_symlink():q.unlink()
    if not q.exists():q.symlink_to((path/p.name)if(path/p.name).exists()else p.resolve())
  else:
   assert not dest.exists();dest.symlink_to(path.resolve(),target_is_directory=True)
 q=target/'visual_upgrade'
 if not q.exists():q.symlink_to(OUT.resolve(),target_is_directory=True)

def characters():
 source=ROOT/'assets/purchased/shield_knight/source/Textures/Texture set 1'
 base=Image.open(source/'Knight_AlbedoTransparency.png').convert('RGB').resize((2048,2048),Image.Resampling.LANCZOS)
 n=np.asarray(Image.open(source/'Knight_Normal.png').convert('RGB').resize((1024,1024),Image.Resampling.LANCZOS),dtype=np.float32)/127.5-1;n/=np.maximum(np.linalg.norm(n,axis=2,keepdims=True),1e-6)
 normal=Image.fromarray(np.uint8(np.rint(np.clip(n*.5+.5,0,1)*255)))
 metallic=Image.open(source/'Knight_MetallicSmoothness.png').convert('RGBA').resize((1024,1024),Image.Resampling.LANCZOS)
 orm=Image.merge('RGB',(Image.open(source/'Knight_AO.png').convert('L').resize((1024,1024),Image.Resampling.LANCZOS),Image.eval(metallic.getchannel('A'),lambda v:255-v),metallic.getchannel('R')))
 maps={0:encoded(base,'JPEG'),1:encoded(normal),2:encoded(orm)};bank=MODELS/'visual_longsword_mocap';bank.mkdir(exist_ok=True);records=[]
 for p in sorted((MODELS/'longsword_mocap').glob('combat_*.glb')):
  doc,blob,raw=read_glb(p);d,b=replace_images(doc,blob,maps);d['materials'][0]['name']='Owned knight: original 4K source recovered at 2K; unchanged pose geometry';d.setdefault('extras',{})['visual_upgrade_source_sha256']=sha(raw);dst=bank/p.name;result=save_glb(dst,d,b)
  # Comparing every non-image view proves poses/socket geometry were not rebaked.
  source_views=[v for i,v in enumerate(doc['bufferViews'])if i not in{im['bufferView']for im in doc['images']}]
  for a,z in zip(source_views,d['bufferViews']):assert blob[a.get('byteOffset',0):a.get('byteOffset',0)+a['byteLength']]==b[z['byteOffset']:z['byteOffset']+z['byteLength']]
  records.append(dict(file=p.name,source_sha256=sha(raw),sha256=sha(result),bytes=len(result)))
 assert len(records)==216
 p=MODELS/'animations/white_knight_brown_armor_demo.glb';doc,blob,raw=read_glb(p)
 for m in doc['materials']:
  m.pop('emissiveTexture',None);m['emissiveFactor']=[0,0,0];m.get('extensions',{}).pop('KHR_materials_specular',None);pbr=m.setdefault('pbrMetallicRoughness',{});pbr['metallicFactor']=0;pbr['roughnessFactor']=.58
 doc.setdefault('extras',{})['visual_upgrade']='Source geometry/rig/animations retained; remove whole-character emission and default metallic skin'
 live=MODELS/'visual_live_animation';live.mkdir(exist_ok=True);save_glb(live/p.name,doc,blob)
 proxy_models({'longsword_mocap':bank,'animations':live})
 report=dict(hero_source='Owned purchased Knight with shield, texture set 1',hero_texture_sizes=[2048,1024,1024],unique_hero_texture_mip_bytes=int((2048**2+1024**2*2)*4*4/3),poses=records,npc_source_sha256=sha(raw),npc_fix='Non-emissive, nonmetallic body/skin; source art is not a replacement face or facial rig')
 (REPORT/'character-assets.json').write_text(json.dumps(report,indent=2)+'\n');print('CHARACTERS',len(records),'geometry-preserved poses, 2K hero and corrected live NPC material',flush=True)

def ground():
 dst=PROJECT/'ground'
 if dst.is_symlink():dst.unlink();dst.mkdir()
 for p in (ROOT/'projects/valwood_world/ground').iterdir():
  q=dst/p.name
  if not q.exists():q.symlink_to(p.resolve(),target_is_directory=p.is_dir())
 src=ROOT/'assets/polyhaven/visual_upgrade/forest_leaves_03';side=Image.open(dst/'terrain_dirt_nor.png').width
 for role,name in [('diff','forest_litter_color.png'),('nor_gl','forest_litter_normal.png')]:Image.open(src/(role+'.png')).convert('RGB').resize((side,side),Image.Resampling.LANCZOS).save(dst/name)
 arm=Image.open(src/'arm.png').convert('RGB').resize((side,side),Image.Resampling.LANCZOS);height=Image.open(src/'disp.png');values=np.asarray(height,dtype=np.float32);limit=65535. if values.max()>255 else 255.;height=Image.fromarray(np.uint8(np.clip(values/limit,0,1)*255)).resize((side,side),Image.Resampling.LANCZOS);r,g,b=arm.split();Image.merge('RGBA',(r,g,b,height)).save(dst/'forest_litter_material.png')
 (dst/'forest_litter_provenance.json').write_text((src/'provenance.json').read_text());print('GROUND',side,'px bounded litter array maps',flush=True)

class Model:
 def __init__(self):self.parts={};self.materials=[];self.images=[];self.textures=[]
 def material(self,name,color,rough=.75,metal=0,maps=None):
  m=dict(name=name,pbrMetallicRoughness=dict(baseColorFactor=[*color,1],roughnessFactor=rough,metallicFactor=metal))
  if maps:
   for role,path in maps.items():
    i=len(self.images);self.images.append(dict(uri=str(path),mimeType='image/png'if path.suffix=='.png'else'image/jpeg'));self.textures.append(dict(source=i,sampler=0));view=dict(index=i)
    if role=='base':m['pbrMetallicRoughness']['baseColorTexture']=view
    if role=='normal':m['normalTexture']=view
    if role=='arm':m['pbrMetallicRoughness']['metallicRoughnessTexture']=view;m['occlusionTexture']=view
  self.materials.append(m);return len(self.materials)-1
 def quad(self,mat,points,uv=None):
  p=np.asarray(points,dtype=float);normal=np.cross(p[1]-p[0],p[2]-p[0]);normal/=np.linalg.norm(normal);record=self.parts.setdefault(mat,[[],[],[],[]]);n=len(record[0]);record[0].extend(points);record[1].extend([normal]*4);record[2].extend(uv or[(0,0),(1,0),(1,1),(0,1)]);record[3].extend([(n,n+1,n+2),(n,n+2,n+3)])
 def box(self,mat,lo,hi,yaw=0,pivot=None):
  x,y,z=lo;X,Y,Z=hi;faces=[[(x,y,Z),(X,y,Z),(X,Y,Z),(x,Y,Z)],[(X,y,z),(x,y,z),(x,Y,z),(X,Y,z)],[(X,y,Z),(X,y,z),(X,Y,z),(X,Y,Z)],[(x,y,z),(x,y,Z),(x,Y,Z),(x,Y,z)],[(x,Y,Z),(X,Y,Z),(X,Y,z),(x,Y,z)],[(x,y,z),(X,y,z),(X,y,Z),(x,y,Z)]]
  for face in faces:
   if yaw:
    c,s=math.cos(yaw),math.sin(yaw);px,py,pz=pivot;face=[(px+c*(a-px)+s*(d-pz),b,pz-s*(a-px)+c*(d-pz))for a,b,d in face]
   width=math.dist(face[0],face[1]);height=math.dist(face[1],face[2]);self.quad(mat,face,[(0,0),(width,0),(width,height),(0,height)])
 def save(self,name):
  from build_cinematic_diversity import Writer
  w=Writer(copy.deepcopy(self.materials),copy.deepcopy(self.textures),copy.deepcopy(self.images))
  for mat,(p,n,uv,idx)in self.parts.items():w.mesh(name+str(mat),np.asarray(p),np.asarray(n),np.asarray(uv),np.asarray(idx),mat)
  dst=OUT/(name+'.glb');w.save(dst,embedded=True);return dict(file=str(dst.relative_to(ROOT)),bytes=dst.stat().st_size,triangles=sum(len(v[3])for v in self.parts.values()),sha256=sha(dst.read_bytes()))

def architecture():
 wooddir=ROOT/'assets/polyhaven/castle_village/brown_planks_08';stone=ROOT/'projects/valwood_world/ground';records=[]
 for style in ['open','glazed','slit']:
  m=Model();wood=m.material('CC0 brown_planks_08',[.82,.76,.64],.82,maps={'base':wooddir/'diff.jpg','normal':wooddir/'nor_gl.jpg','arm':wooddir/'arm.jpg'});masonry=m.material('Existing scanned village stone',[.82,.84,.80],.88,maps={'base':stone/'village_stone_BC.png','normal':stone/'village_stone_N.png','arm':stone/'village_stone_ARM.png'});dark=m.material('Recess shadow, nonemissive',[.027,.022,.017],.96);iron=m.material('Worn iron',[.055,.06,.057],.46,.8);glass=m.material('Opaque leaded glazing reflection',[.05,.075,.085],.13)
  width=.42 if style=='slit' else 1.12;height=1.7 if style=='slit' else 1.55;half=width/2
  # A real 24cm deep assembly sits in front of the old facade. Its back and
  # inner walls supply depth; no holes are claimed in the source building.
  m.box(dark,(-half,0,-.015),(half,height,.025));m.box(wood,(-half,0,.025),(-half+.065,height,.25));m.box(wood,(half-.065,0,.025),(half,height,.25));m.box(wood,(-half,0,.025),(half,.075,.25));m.box(wood,(-half,height-.075,.025),(half,height,.25))
  m.box(masonry,(-half-.13,-.13,-.025),(half+.13,.015,.31));m.box(masonry,(-half-.1,height-.01,-.025),(half+.1,height+.13,.29))
  if style=='glazed':
   m.box(glass,(-half+.075,.085,.055),(half-.075,height-.085,.069));m.box(wood,(-.035,.08,.07),(.035,height-.08,.19));m.box(wood,(-half+.08,height*.5-.025,.07),(half-.08,height*.5+.025,.19))
  elif style=='open':
   # Shutters swing outward about actual hinges; interior shelf and jambs are geometry.
   for side in [-1,1]:
    pivot=(side*half,0,.26);angle=side*math.radians(118)
    for j in range(4):
     a=side*(half-.015-j*width/8);b=side*(half-.015-(j+1)*width/8)
     m.box(wood,(min(a,b),.08,.24),(max(a,b),height-.08,.29),angle,pivot)
    for yy in [.3,height-.3]:m.box(iron,(side*half-.025,yy,.26),(side*half+.025,yy+.09,.33))
   m.box(wood,(-half+.08,.29,.02),(half-.08,.34,.18));m.box(wood,(-half+.08,.08,.02),(half-.08,.12,.18))
  else:m.box(iron,(-.018,.09,.08),(.018,height-.09,.12))
  records.append(m.save('window_'+style))
 (REPORT/'architecture-assets.json').write_text(json.dumps(dict(source='Original measured geometry; existing CC0 Poly Haven wood and stone maps',assemblies=records,glass_note='Opaque reflective leaded glazing; no full glass transmission pass'),indent=2)+'\n');print('ARCHITECTURE',[(r['triangles'],r['bytes'])for r in records],flush=True)

if __name__=='__main__':
 import sys
 if '--characters'in sys.argv:characters()
 if '--ground'in sys.argv:ground()
 if '--architecture'in sys.argv:architecture()
