#!/usr/bin/env python3
"""Isolated source-PBR cook for purchased village street props.

Uses the established architecture conversion helpers without writing its source,
models or manifest. Run --prepare with Python, then run this script in Blender
with --cook. Finished outputs have their own purchased_props_manifest.json.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,sys
ROOT=Path(__file__).resolve().parents[1];SOURCE=ROOT/'assets/purchased/village_props'
MODELS=ROOT/'projects/valwood_world/models';ARCH=ROOT/'assets/purchased/medieval_village'
FAMILIES={'lamp':('SM_Street_Lamp.FBX',2000,650),'flag_a':('SM_Flag_01.FBX',1000,300),
 'flag_b':('SM_Flag_02.FBX',1000,300),'sack':('SM_Sack_Bag_01.FBX',2500,900),
 'logs':('SM_Wooden_Logs_Pile.FBX',3600,900),'rope':('SM_Rope_Modular_Middle.FBX',2200,700),
 'barrel_small':('SM_Barrel_02.FBX',1500,500)}
SIZES={k:(512,256,128) for k in FAMILIES}
for k in ['lamp','sack','logs']:SIZES[k]=(1024,512,256)
SIZES['rope']=(256,128,64)
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def prepare():
 from PIL import Image
 import numpy as np
 dest=SOURCE/'prepared';dest.mkdir(parents=True,exist_ok=True)
 for p in (ARCH/'prepared').glob('*.png'):
  target=dest/p.name
  if not target.exists():target.symlink_to(Path('../../medieval_village/prepared')/p.name)
 for p in (SOURCE/'source_textures').glob('*.PNG'):
  kind=p.stem.rsplit('_',1)[-1];im=Image.open(p).convert('RGBA' if kind=='BC' else 'RGB')
  im.thumbnail((2048,2048) if kind=='BC' else ((1024,1024) if kind=='N' else (512,512)),Image.Resampling.LANCZOS)
  if kind=='N':a=np.asarray(im).copy();a[:,:,1]=255-a[:,:,1];im=Image.fromarray(a)
  im.save(dest/(p.stem+'.png'),optimize=True)
 shutil.copy2(ARCH/'unreal-material-source-audit.json',SOURCE/'unreal-material-source-audit.json')
 (SOURCE/'PROVENANCE.json').write_text(json.dumps(dict(source_archive='/home/computer/Downloads/Medieval_Village_ALL.rar',source_project='/home/computer/Downloads/Medieval_Village',originals_modified=False,license='User-provided purchased content; no open license or redistribution claim',source_meshes=[dict(file=p.name,sha256=sha(p),bytes=p.stat().st_size) for p in sorted((SOURCE/'source_meshes').glob('*.FBX'))],prepared_parent='Read-only relative links to the established source-PBR prepared maps; new prop maps prepared separately',adaptations=['Source LOD0 only, no UCX geometry','Bounded per-prop near/mid PBR atlases; no baked lighting','DX normal green inverted before tangent-space bake','Source ribbon flag coverage mapped to native alpha mask at0.3333','Rope source tiling_amount1.5 retained','Source lamp subsurface shader represented by albedo/normal/ARM; no SSS claim','Missing bake UV generated on a copy; original source UV0 and material assignments retained','Rigid modular rope rotated 90 degrees in generated bake copy for native horizontal spans']),indent=2)+'\n')
def configured():
 sys.path.insert(0,str(ROOT/'tools'));import build_purchased_village as helper
 cooking=SOURCE/'cooking';cooking.mkdir(exist_ok=True)
 if not (cooking/'prepared').exists():(cooking/'prepared').symlink_to('../prepared')
 shutil.copy2(SOURCE/'unreal-material-source-audit.json',cooking/'unreal-material-source-audit.json')
 helper.SOURCE=cooking;helper.OUT=SOURCE/'converted';helper.OUT.mkdir(parents=True,exist_ok=True)
 helper.FAMILIES=FAMILIES;helper.ATLAS_SIZES=SIZES;helper.PHYSICAL={};helper.TILE_KEYS=set();helper.ARCHITECTURE_GPU_BUDGET_MIB=40
 helper.MATERIALS.update({'MI_Ribbon_Flag':'T_Ribbon_Flag','MI_Statue_Candles':'T_Statue_Candles','MI_Wood_End_02':'T_Wood_End_02'})
 helper.SOURCE_INSTANCES.update({'MI_Ribbon_Flag':'MI_RibbonFlag','MI_Wood_End_02':'MI_WoodEnd_02'})
 original=helper.material_source_settings
 def settings(key):
  record,tiling=original(key)
  if key=='MI_Ribbon_Flag':record=dict(record,parent='MM_Master_Alpha')
  if key=='MI_Tileable_Rope':tiling=record.get('scalars',{}).get('tiling_amount',1.5)
  return record,tiling
 helper.material_source_settings=settings
 return helper
def bake_inputs(families):
 import bpy
 from mathutils import Matrix
 import math
 out=SOURCE/'cooking/source_meshes';out.mkdir(parents=True,exist_ok=True)
 for family in families:
  filename=FAMILIES[family][0];bpy.ops.wm.read_factory_settings(use_empty=True)
  bpy.ops.import_scene.fbx(filepath=str(SOURCE/'source_meshes'/filename),use_anim=False,use_custom_normals=True)
  meshes=[o for o in bpy.context.scene.objects if o.type=='MESH' and not o.name.startswith('UCX_') and ('_LOD0' in o.name or '_LOD' not in o.name)]
  assert len(meshes)==1;obj=meshes[0];assert obj.data.uv_layers
  if family=='rope':
   obj.data.transform(obj.matrix_world);obj.matrix_world=Matrix.Identity(4)
   obj.data.transform(Matrix.Rotation(math.pi*.5,4,'X'))
  obj.data.uv_layers[0].name='SourceUV'
  bpy.ops.object.select_all(action='DESELECT');obj.select_set(True);bpy.context.view_layer.objects.active=obj
  if 'LightMapUV' not in obj.data.uv_layers:
   layer=obj.data.uv_layers.new(name='LightMapUV');obj.data.uv_layers.active=layer
   bpy.ops.object.mode_set(mode='EDIT');bpy.ops.mesh.select_all(action='SELECT')
   bpy.ops.uv.smart_project(angle_limit=1.151917,island_margin=.025)
   bpy.ops.object.mode_set(mode='OBJECT')
  # Preserve source UV0/materials and originals. Only this generated FBX gains
  # a bounded, non-overlapping bake UV when the exported prop supplied none.
  bpy.ops.export_scene.fbx(filepath=str(out/filename),use_selection=True,add_leaf_bones=False,bake_anim=False)
def publish(helper):
 original=json.loads((SOURCE/'converted/purchased_village_manifest.json').read_text());rows=[]
 for entry in original['assets']:
  src=SOURCE/'converted'/(entry['asset']+'.glb');name=entry['asset'].replace('purchased_village_','purchased_prop_',1)
  doc,binary=helper.glb(src)
  if entry['family'].startswith('flag'):
   for material in doc['materials']:material['alphaMode']='MASK';material['alphaCutoff']=.3333
  payload=helper.encode_glb(doc,binary);staged=SOURCE/'converted'/(name+'.glb');staged.write_bytes(payload)
  actual=helper.inspect_glb(staged);row=dict(entry,**actual);row['asset']=name
  row['original_source_sha256']=sha(SOURCE/'source_meshes'/FAMILIES[row['family']][0])
  row['source_front_axis']='source Z transformed to native -Z; native vertical is Y'
  if row['family']=='rope':row['source_front_axis']+='; rigid rope rotated 90 degrees before baking to span horizontally with native yaw'
  row['anchor']='XZ centered, geometric minimum Y grounded'
  rows.append(row)
 unique={i['sha256']:i for r in rows for i in r['images']}
 gpu=sum(i['dimensions'][0]*i['dimensions'][1]*4*4/3 for i in unique.values());assert gpu<=40*1024*1024
 manifest=dict(schema=1,license=original['license'],source_archive=original['source_archive'],assets=rows,gpu_rgba8_mip_bytes=gpu)
 for r in rows:shutil.copy2(SOURCE/'converted'/(r['asset']+'.glb'),MODELS/(r['asset']+'.glb'))
 (MODELS/'purchased_props_manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
 print('PUBLISHED props:',len(rows),'GLBs,',round(gpu/1048576,3),'MiB RGBA8+mips',flush=True)
def validate():
 sys.path.insert(0,str(ROOT/'tools'));import build_purchased_village as helper
 manifest=json.loads((MODELS/'purchased_props_manifest.json').read_text());rows=manifest['assets'];assert len(rows)==14
 images={};near={}
 for row in rows:
  family,lod=row['family'],row['lod'];assert family in FAMILIES and lod in {'near','mid'}
  path=MODELS/(row['asset']+'.glb');actual=helper.inspect_glb(path)
  for field in ['sha256','triangles','primitive_count','bounds_y_up','images']:assert actual[field]==row[field],(path,field)
  assert actual['primitive_count']==1 and actual['triangles']<=FAMILIES[family][1 if lod=='near' else 2]
  assert abs(actual['bounds_y_up'][1][0])<(1e-6 if lod=='near' else .003)
  assert sha(SOURCE/'source_meshes'/FAMILIES[family][0])==row['original_source_sha256']
  doc,_=helper.glb(path);material=doc['materials'][0]
  assert 'normalTexture' in material and 'occlusionTexture' in material and 'metallicRoughnessTexture' in material['pbrMetallicRoughness']
  if family.startswith('flag'):assert material['doubleSided'] and material['alphaMode']=='MASK' and abs(material['alphaCutoff']-.3333)<1e-6
  if family=='rope':assert actual['bounds_y_up'][1][1]<.07 and actual['bounds_y_up'][2][1]-actual['bounds_y_up'][2][0]>.65
  if lod=='near':near[family]=row
  for image in actual['images']:images[image['sha256']]=image
 assert set(near)==set(FAMILIES)
 for row in rows:
  if row['lod']=='mid':
   assert row['images']==near[row['family']]['images'],('LOD maps must share encoded bytes',row['family'])
   assert row['triangles']<=near[row['family']]['triangles']
 gpu=sum(i['dimensions'][0]*i['dimensions'][1]*4*4/3 for i in images.values())
 assert gpu==manifest['gpu_rgba8_mip_bytes'] and gpu<=28*1048576
 print('VALID source props:14 GLBs,7 families,21 shared PBR images,',round(gpu/1048576,3),'MiB; source hashes, masks, budgets and horizontal rope PASS')
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--prepare',action='store_true');p.add_argument('--cook',action='store_true');p.add_argument('--preview',action='store_true');p.add_argument('--validate',action='store_true');p.add_argument('--families',default=','.join(FAMILIES));a=p.parse_args(sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else None)
 if a.prepare:prepare();return
 if a.validate:validate();return
 families=a.families.split(',');assert set(families)<=FAMILIES.keys()
 if a.cook:bake_inputs(families)
 h=configured()
 if a.preview:h.preview(families);return
 assert a.cook;h.cook(families);publish(h)
if __name__=='__main__':main()
