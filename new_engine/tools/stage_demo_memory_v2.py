#!/usr/bin/env python3
"""C19: four reviewed secondary sRGB atlases at1K; exact source geometry."""
from pathlib import Path
import copy,hashlib,io,json,shutil,sys
import numpy as np
from PIL import Image
from visual_ground_finish import GLB,append_view,write_glb
from cook_bep_countryside_materials import linear,srgb,resize
from stage_demo_memory_v1 import classify,mip
ROOT=Path(__file__).resolve().parents[1]
SOURCE=ROOT/'build/journey-opening/candidate-v18/project'
TARGET=ROOT/'build/journey-opening/candidate-v19/project'
OUT=ROOT/'docs/demo-memory/2026-11-05/stage-v2'
PAGES={
 '92757fed68154780f000bca4805265290c0680df31b946dd8a3847cb728fbe95':'MI_Flower_Petal_Atlas',
 '94b30a93cc648c3d0980fb0c314af5d50f2434868d008f9ff8ac971274d073c6':'MI_FoodSheet_02',
 '483f20ab93c635c4ef7840c1ed56961bd65defe18b67afe09e96bcc22184b834':'MI_WoodenPlough',
 'fa6c35946700e7e0145094457ba9569a82418dffe6c5ec8ffd44f31fe3c83708':'MI_SharpeningStone'}
def sha(p):
 with Path(p).open('rb')as f:return hashlib.file_digest(f,'sha256').hexdigest()
def dump(p,d):
 with p.open('x')as f:json.dump(d,f,indent=2,allow_nan=False);f.write('\n')
def clone(a,b):
 if a.is_symlink():b.symlink_to(a.resolve(strict=True),target_is_directory=a.is_dir())
 elif a.is_dir():
  b.mkdir()
  for child in a.iterdir():
   if child.name!='.valwood-instance.lock':clone(child,b/child.name)
 elif a.is_file():
  if a.relative_to(SOURCE).parts[0]=='models'and a.suffix.lower()in{'.glb','.bin','.png','.jpg','.jpeg'}:b.symlink_to(a.resolve(strict=True))
  else:shutil.copyfile(a,b)
 else:raise AssertionError(a)
def main():
 assert not OUT.exists()and not TARGET.parent.exists();OUT.mkdir(parents=True)
 origin=SOURCE.parent/'local-origin.json';origin_hash=sha(origin);source_doc=json.loads(origin.read_text());assert len(source_doc['generated_sha256'])==519
 assert all(sha(SOURCE/n)==h for n,h in source_doc['generated_sha256'].items())
 payload=json.loads((ROOT/'docs/windows-demo/2026-11-05/payload-v6-final/INPUTS.json').read_text())['inputs'];names=sorted(n for n in payload if n.endswith('.glb'));assert len(names)==1147
 replacements={};models=[];pages={h:{'material':m,'source_sha256':h,'references':[]}for h,m in PAGES.items()};scanned={}
 for name in names:
  source=SOURCE/name;real=source.resolve(strict=True)
  if real in scanned:
   records=scanned[real]
   for digest in records:pages[digest]['references'].append(name)
   assert not records,('Unexpected aliased reviewed page outside canonical registry',name)
   continue
  g=GLB(source);d=copy.deepcopy(g.doc);binary=g.binary;roles=classify(d);changes=[];records=[]
  for i,im in enumerate(d.get('images',[])):
   if 'bufferView'not in im:continue
   v=g.doc['bufferViews'][im['bufferView']];raw=g.binary[v.get('byteOffset',0):v.get('byteOffset',0)+v['byteLength']];digest=hashlib.sha256(raw).hexdigest()
   if digest not in PAGES:continue
   assert name.startswith('models/imports/nm_city_'),name
   assert roles.get(i)=={'srgb'},(name,i,roles.get(i))
   selected=[]
   for m in d['materials']:
    t=m.get('pbrMetallicRoughness',{}).get('baseColorTexture',{})
    if t and d['textures'][t['index']]['source']==i:selected.append(m.get('name'))
   assert selected and set(selected)=={PAGES[digest]},(name,selected)
   image=Image.open(io.BytesIO(raw)).convert('RGBA');assert image.size==(2048,2048)
   if digest not in replacements:
    a=np.asarray(image,dtype=np.float32)/255.;alpha=np.clip(resize(a[:,:,3:4],(1024,1024)),0,1);rgb=srgb(np.clip(resize(linear(a[:,:,:3])*a[:,:,3:4],(1024,1024))/np.maximum(alpha,1e-8),0,1));pixels=np.uint8(np.clip(np.rint(np.dstack((rgb,alpha))*255),0,255));out=io.BytesIO();Image.fromarray(pixels).save(out,format='PNG',compress_level=3);replacements[digest]=out.getvalue();pages[digest].update(sha256=hashlib.sha256(out.getvalue()).hexdigest(),before=[2048,2048],after=[1024,1024],role='sRGB color; alpha independently linear',saving_bytes=mip(2048,2048)-mip(1024,1024))
   encoded=replacements[digest];binary,index=append_view(d,binary,encoded);im['bufferView']=index;im['mimeType']='image/png';im.pop('uri',None);records.append(digest);pages[digest]['references'].append(name);changes.append({'image':i,'source_sha256':digest,'sha256':pages[digest]['sha256'],'material':PAGES[digest]})
  scanned[real]=records
  if not changes:continue
  path=OUT/'models'/name.removeprefix('models/');path.parent.mkdir(parents=True,exist_ok=True);write_glb(path,d,binary);after=GLB(path);assert after.binary[:len(g.binary)]==g.binary
  for k in('meshes','accessors','nodes','scenes','materials','textures','samplers'):assert after.doc.get(k)==g.doc.get(k),(name,k)
  models.append({'relative_path':name,'path':str(path),'source_sha256':sha(source),'sha256':sha(path),'source_binary_exact_prefix':True,'mesh_accessors_indices_UV_materials_exact':True,'images':changes})
 assert set(replacements)==set(PAGES)and sum(p['saving_bytes']for p in pages.values())==64*1024*1024
 assert all(p['references']for p in pages.values())
 TARGET.parent.mkdir();clone(SOURCE,TARGET);assert all(sha(TARGET/n)==h for n,h in source_doc['generated_sha256'].items())
 def detach_parent(relative):
  parent=TARGET
  for part in Path(relative).parts[:-1]:
   parent=parent/part
   if parent.is_symlink():
    real=parent.resolve(strict=True);children=list(real.iterdir());parent.unlink();parent.mkdir()
    for child in children:(parent/child.name).symlink_to(child.resolve(strict=True),target_is_directory=child.is_dir())
   assert not parent.is_symlink()and parent.is_dir()
 for row in models:
  name=row['relative_path'];assert sha(TARGET/name)==row['source_sha256'];detach_parent(name);(TARGET/name).unlink();(TARGET/name).symlink_to(Path(row['path']));assert sha(TARGET/name)==row['sha256']
 changed={n for n,h in source_doc['generated_sha256'].items()if sha(TARGET/n)!=h};assert changed=={m['relative_path']for m in models}
 assert all(sha(SOURCE/n)==h for n,h in source_doc['generated_sha256'].items())and sha(origin)==origin_hash
 protected=['city.physics','opening/home.physics','world.studio','opening/first-journey.routes.json','opening/first-journey.settings','opening/arrival-camp.settings'];assert all(sha(TARGET/n)==sha(SOURCE/n)for n in protected)
 for f in(SOURCE/'models/opening').glob('*.glb'):assert sha(f)==sha(TARGET/'models/opening'/f.name)
 users={str(f.relative_to(SOURCE)):sha(f)for f in(SOURCE/'saves').rglob('*')if f.is_file()};assert users=={str(f.relative_to(TARGET)):sha(f)for f in(TARGET/'saves').rglob('*')if f.is_file()}
 for name in ['graphics.settings','studio-layout.ini']:
  if(SOURCE/name).exists():assert sha(SOURCE/name)==sha(TARGET/name)
 report={'schema':'valwood.demo-secondary-texture-memory-patch','version':2,'applied':True,'source_project':str(SOURCE),'target_project':str(TARGET),'source_origin_sha256':origin_hash,'runtime_model_files_scanned':len(names),'changed_models':models,'pages':list(pages.values()),'estimated_unique_RGBA8_mip_saving_bytes':64*1024*1024,'estimated_saving_MiB':64,'all_other_generated_files_exact':True,'all519_C18_source_hashes_unchanged':True,'all_home_HIGH_and_MID_files_unchanged':True,'hero_terrain_main_facade_images_unchanged':True,'all_geometry_indices_UV_materials_physics_story_routes_user_saves_unchanged':True,'newencoded_bytes_identical_for_all_references':True,'filter':'sRGB to linear; premultiplied color downsample, separate linear alpha; reencode sRGB','GPU_used':False,'tool_sha256':sha(Path(__file__))}
 dump(OUT/'REVIEW.json',report);dump(OUT/'APPLY.json',{'applied':True,'target_project':str(TARGET),'changed_models_sha256':{m['relative_path']:m['sha256']for m in models},'C18_origin_unchanged':True,'C18_generated_source_unchanged':True,'physics_unchanged':True,'user_saves_preferences_unchanged':True,'GPU_used':False});print(json.dumps({'applied':True,'models':len(models),'references':sum(len(p['references'])for p in pages.values()),'saving_MiB':64,'target':str(TARGET)}))
if __name__=='__main__':main()
