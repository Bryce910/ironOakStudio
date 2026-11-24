#!/usr/bin/env python3
"""Private C16 image-resolution/LOD data patch; original geometry stays exact."""
from pathlib import Path
import copy,hashlib,io,json,math,os,re
import numpy as np
from PIL import Image
from visual_ground_finish import GLB,append_view,write_glb
from cook_bep_countryside_materials import linear,srgb,resize
ROOT=Path(__file__).resolve().parents[1]
SOURCE=ROOT/'build/journey-opening/candidate-v15/project'
TARGET=ROOT/'build/journey-opening/candidate-v16/project'
OUT=ROOT/'docs/demo-memory/2026-11-05/stage-v1'
CITY={'11c589f2e7f2894f28d471776715d6802766a718bc3fccc7ba26490e0392763b','8460315c0bf5f90f7f40257f8447e05ea394b65caa2c31b8b49d1f4c87ee56d7'}
def sha(p):
 with Path(p).open('rb')as f:return hashlib.file_digest(f,'sha256').hexdigest()
def dump(p,v):
 with p.open('x')as f:json.dump(v,f,indent=2,allow_nan=False);f.write('\n')
def mip(w,h):
 total=0
 while True:
  total+=w*h*4
  if w==h==1:return total
  w=max(w//2,1);h=max(h//2,1)
def classify(d):
 roles={}
 def add(index,kind):
  image=d['textures'][index]['source'];roles.setdefault(image,set()).add(kind)
 used={p['material']for mesh in d['meshes']for p in mesh['primitives']if 'material'in p}
 for number in used:
  m=d['materials'][number];p=m.get('pbrMetallicRoughness',{});f=m.get('extensions',{}).get('VALWOOD_materials_facade',{}).get('parameters');mode=int(f[1])if f else 0
  for key in ('baseColorTexture',):
   if key in p:add(p[key]['index'],'srgb')
  if 'metallicRoughnessTexture'in p:add(p['metallicRoughnessTexture']['index'],'srgb'if mode in(3,4)else'linear')
  if 'normalTexture'in m:add(m['normalTexture']['index'],'normal')
  if 'emissiveTexture'in m:add(m['emissiveTexture']['index'],'srgb')
  # Match world_model_facade.h::facade_srgb_aux. Occlusion-only texture
  # fields are not independently sampled by the current native renderer.
  if f:
   for slot,index in enumerate(map(int,f[2:5])):
    if index>=0:add(index,'srgb'if((mode<=2 and(mode==1 or slot==2))or(mode==3 and slot in(1,2))or(mode==4 and slot==1)or(mode==5 and slot==0))else'linear')
 return roles

def main():
 assert not OUT.exists();OUT.mkdir(parents=True)
 assert TARGET.is_dir() and TARGET!=SOURCE
 origin=SOURCE.parent/'local-origin.json';origin_before=sha(origin)
 protected=['city.physics','world.studio','opening/home.physics','opening/first-journey.routes.json','opening/first-journey.settings','opening/arrival-camp.settings','ground/countryside.height','ground/countryside.coverage']
 before={n:sha(SOURCE/n)for n in protected}
 homes=[]
 for line in(SOURCE/'world.studio').read_text().splitlines():
  s=line.split()
  if s and s[0]=='asset'and('finward_scene'in s[2]or s[2]in('opening/home','opening/home_story','opening/home_enclosure')):homes.append('models/'+s[2]+'_mid.glb')
 assert len(homes)==15
 proof=json.loads((ROOT/'docs/windows-demo/2026-11-05/runtime-aliases-v3/RESULTS.json').read_text())
 names=sorted(set(proof['runtime_paths'])|set(homes));memo={};models=[];pages={};city_refs=[];protected_high={n.replace('_mid.glb','.glb'):sha(SOURCE/n.replace('_mid.glb','.glb'))for n in homes}
 for name in names:
  g=GLB(SOURCE/name);d=copy.deepcopy(g.doc);binary=g.binary;roles=classify(d);changes=[]
  for i,im in enumerate(d.get('images',[])):
   view=g.doc['bufferViews'][im['bufferView']];raw=g.binary[view.get('byteOffset',0):view.get('byteOffset',0)+view['byteLength']];digest=hashlib.sha256(raw).hexdigest()
   side=256 if name in homes else 1024 if digest in CITY else None
   if side is None or i not in roles:continue
   a=Image.open(io.BytesIO(raw)).convert('RGBA');w,h=a.size
   kinds=roles[i];assert len(kinds)==1,(name,i,kinds)
   kind=next(iter(kinds))
   if digest in CITY:
    assert kind=='srgb' and(w,h)==(2048,2048),(name,i,kind,w,h)
    city_refs.append(dict(path=name,image=i,sha256=digest,role='sRGB packed layer; not ordinary ORM'))
   factor=min(1.,side/max(w,h));size=(max(1,round(w*factor)),max(1,round(h*factor)))
   page=pages.setdefault((digest,kind,side),dict(source_sha256=digest,role=kind,before_dimensions=[w,h],after_dimensions=list(size),before_rgba8_mips=mip(w,h),after_rgba8_mips=mip(*size),references=[]));page['references'].append(name)
   if size==(w,h):continue
   key=(digest,kind,size)
   if key not in memo:
    v=np.asarray(a,dtype=np.float32)/255;alpha=np.clip(resize(v[:,:,3:4],size),0,1)
    if kind=='srgb':rgb=srgb(np.clip(resize(linear(v[:,:,:3])*v[:,:,3:4],size)/np.maximum(alpha,1e-8),0,1))
    elif kind=='normal':
     vector=resize(v[:,:,:3]*2-1,size);vector/=np.maximum(np.linalg.norm(vector,axis=2,keepdims=True),1e-8);rgb=vector*.5+.5
    else:rgb=np.clip(resize(v[:,:,:3],size),0,1)
    pixels=np.uint8(np.clip(np.rint(np.dstack((rgb,alpha))*255),0,255));stream=io.BytesIO();Image.fromarray(pixels).save(stream,format='PNG',compress_level=3);memo[key]=stream.getvalue()
   encoded=memo[key];binary,index=append_view(d,binary,encoded);im['bufferView']=index;im['mimeType']='image/png';im.pop('uri',None)
   changes.append(dict(image=i,source_sha256=digest,sha256=hashlib.sha256(encoded).hexdigest(),role=kind,before=[w,h],after=list(size),alpha_filtered_as_linear_separately=True))
  if not changes:continue
  path=OUT/'models'/name.removeprefix('models/');path.parent.mkdir(parents=True,exist_ok=True);write_glb(path,d,binary)
  new=GLB(path);assert new.binary[:len(g.binary)]==g.binary
  for key in('meshes','accessors','nodes','scenes','materials','textures','samplers'):
   assert d.get(key)==g.doc.get(key),(name,key)
  models.append(dict(relative_path=name,path=str(path),sha256=sha(path),source_sha256=sha(SOURCE/name),source_binary_exact_prefix=True,geometry_accessors_indices_uvs_materials_exact=True,images=changes))
 assert len(city_refs)==24 and {r['sha256']for r in city_refs}==CITY
 studio=(SOURCE/'world.studio').read_text();lines=[];edits=[]
 for number,line in enumerate(studio.splitlines(),1):
  s=line.split()
  if len(s)==9 and s[0]=='asset'and s[1]in('nm_arrival_oak04','nm_arrival_beech01'):
   assert s[7]=='70';s[7]='65';new=' '.join(s);edits.append(dict(line=number,before=line,after=new));line=new
  lines.append(line)
 assert len(edits)==2
 (OUT/'world.studio').write_text('\n'.join(lines)+'\n')
 assert all(sha(SOURCE/n)==h for n,h in protected_high.items())
 assert before=={n:sha(SOURCE/n)for n in protected}and sha(origin)==origin_before
 saving=sum(p['before_rgba8_mips']-p['after_rgba8_mips']for p in pages.values())
 report=dict(schema='valwood.demo-memory-data-patch',version=1,staged=True,applied=False,source_project=str(SOURCE),target_project=str(TARGET),source_origin_sha256=origin_before,source_protected_sha256=before,home_high_sha256=protected_high,home_mid_files_audited=15,changed_models=models,city_packed_page_references=city_refs,pages=list(pages.values()),estimated_rgba8_mip_saving_bytes=saving,estimated_rgba8_mip_saving_MiB=saving/2**20,memory_scope='Unique sampled image+role keys; outside apartment after HIGH retirement. Actual residency/admission must be verified natively.',filter_policy='sRGB decode and premultiplied alpha filter for color/packed source layers; linear-channel ARM/data; unit-vector normal filtering; alpha separate linear; deterministic shared PNG payloads.',no_border_mainterrain_loam_canopy_changes=True,city_main_basecolor_and_normal_pages_unchanged=True,home_high_images_unchanged=True,geometry_physics_story_routes_and_saves_unchanged=True,studio_edits=edits,studio_path=str(OUT/'world.studio'),studio_sha256=sha(OUT/'world.studio'),source_inputs_unchanged=True,gpu_used=False,tool_sha256=sha(Path(__file__)))
 dump(OUT/'REVIEW.json',report)
 # Detach only affected aliases/directories. Never write through a source link.
 def private_parent(relative):
  current=TARGET
  for part in Path(relative).parts[:-1]:
   current=current/part
   if current.is_symlink():
    real=current.resolve(strict=True);assert real.is_dir()and real.is_relative_to(ROOT);children=list(real.iterdir());current.unlink();current.mkdir()
    for child in children:(current/child.name).symlink_to(child.resolve(strict=True),target_is_directory=child.is_dir())
   assert current.is_dir()and not current.is_symlink()
 for row in models:
  name=row['relative_path'];q=TARGET/name;assert sha(q)==row['source_sha256'];private_parent(name);q=TARGET/name;q.unlink();q.symlink_to(Path(row['path']))
 assert not(TARGET/'world.studio').is_symlink() and sha(TARGET/'world.studio')==before['world.studio']
 (TARGET/'world.studio').write_bytes((OUT/'world.studio').read_bytes())
 assert sha(TARGET/'city.physics')==before['city.physics'] and sha(TARGET/'opening/home.physics')==before['opening/home.physics']
 assert all(sha(TARGET/n)==h for n,h in protected_high.items())
 assert before=={n:sha(SOURCE/n)for n in protected}and sha(origin)==origin_before
 dump(OUT/'APPLY.json',dict(applied=True,target_project=str(TARGET),model_files=len(models),models_sha256={m['relative_path']:m['sha256']for m in models},world_studio_sha256=sha(TARGET/'world.studio'),source_c15_unchanged=True,high_apartment_models_unchanged=True,physics_unchanged=True,gpu_used=False))
 print(json.dumps(dict(applied=True,models=len(models),home_mid_files_audited=15,packed_city_refs=len(city_refs),estimated_saving_MiB=saving/2**20,target=str(TARGET))))
if __name__=='__main__':main()
