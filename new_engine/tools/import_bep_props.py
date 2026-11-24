#!/usr/bin/env python3
"""Source-preserving, bounded cook of the user's purchased BEP village details.

python3 tools/import_bep_props.py --prepare
env OMP_NUM_THREADS=2 OPENBLAS_NUM_THREADS=1 blender -b -t 2 \
  --disable-autoexec --factory-startup --python-exit-code 1 \
  --python tools/import_bep_props.py
python3 tools/import_bep_props.py --validate

Legacy diffuse/normal/AO are explicitly remapped, never followed via absolute
FBX paths. FBX roughness .6900348 and nonmetal values are retained: the source
Spec artwork is preserved but is not incorrectly inverted into roughness.
No embedded source script, source stage, animation, or duplicate LOD is run.
"""
from pathlib import Path, PurePosixPath
import hashlib, io, json, math, os, struct, sys, tempfile, zipfile

ROOT=Path(__file__).resolve().parents[1]
ASSET=ROOT/'assets/purchased/bep_props'
SOURCE=ASSET/'source'
DERIVED=ASSET/'derived'
OUT=ROOT/'projects/valwood_world/models'
MANIFEST='bep_props_manifest.json'
DOWNLOADS=Path('/home/computer/Downloads')
ROUGH=.690034806728363
FAMILIES={'Barrel&Chest':768,'Wood_Fence':512,'Wall_02':512,
    'Slate_Roof':512,'Stones&Woodbeams':512,'Planks':512,'Metall_01':256}
# Runtime names, archive, mesh file, exact source near parts, source metric
# scale, rotation around up in degrees, and offline mid triangle ratio.
SPECS=[
 ('bep_prop_barrel','Props.zip','Props/Barrel_Chest/Barrel_A.fbx',['Barrel'],1,0,.28),
 ('bep_prop_chest','Props.zip','Props/Barrel_Chest/Chest_A.fbx',['Chest_Bottom','Chest_Cap','Coins'],1,0,.20),
 ('bep_prop_bench','Props.zip','Props/Furniture/Bench_01.fbx',['Bench_01'],1,90,.28),
 ('bep_prop_bucket','Props.zip','Props/Furniture/Bucket_01.fbx',['Bucket_01','Bucket_01_Iron'],1,0,.25),
 ('bep_prop_fence','Props.zip','Props/Planks/Fences.fbx',['Fence_01'],1,0,.30),
 ('bep_prop_fence_broken','Props.zip','Props/Planks/Fences.fbx',['Fence_01_Broken'],1,0,.30),
 ('bep_structure_well','Building+Elements.zip','Building Elements/Well/Well.fbx',['Well','Well_Roof','Well_Wood'],1,0,.24),
 ('bep_structure_storehouse','Building+Elements.zip','Building Elements/Stall/Shed_01.fbx',
  ['Shed_Planks','Shed_Roof','Shed_Wall_Back','Shed_Wall_Blocks_Beams','Shed_Wall_Foundation','Shed_Wall_Front'],.5,0,.22),
]
BUDGETS={'bep_prop_barrel':(1900,490),'bep_prop_chest':(2400,690),
 'bep_prop_bench':(1600,450),'bep_prop_bucket':(1700,490),
 'bep_prop_fence':(1800,650),'bep_prop_fence_broken':(1800,650),
 'bep_structure_well':(11900,3900),'bep_structure_storehouse':(11900,3900)}

def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()

def immutable_write(p,data):
 p=Path(p);p.parent.mkdir(parents=True,exist_ok=True)
 if p.exists():assert p.read_bytes()==data,'Preserved source changed: '+str(p)
 else:p.write_bytes(data)

def prepare():
 selected={}
 for _,archive,file,*_ in SPECS:selected.setdefault(archive,set()).add(file)
 selected['Buildings&PropsTextures.zip']={
   'Buildings&PropsTextures/'+family+'_'+suffix+'.png'
   for family in FAMILIES for suffix in ['Dif','Nor','Aoc','Spec']}
 for archive,names in selected.items():
  with zipfile.ZipFile(DOWNLOADS/archive)as z:
   for name in sorted(names):
    p=PurePosixPath(name);assert not p.is_absolute()and '..'not in p.parts and ':'not in name
    inf=z.getinfo(name);assert not inf.is_dir()and inf.file_size<64*1024**2
    assert ((inf.external_attr>>16)&0o170000)!=0o120000
    immutable_write(SOURCE/name,z.read(name))
 print('Preserved',sum(len(v)for v in selected.values()),'selected source files',flush=True)

def material_family(name):
 name=name.lower()
 if name.startswith('barrel&chest'):return'Barrel&Chest'
 if name.startswith('wood_fence'):return'Wood_Fence'
 if name.startswith('wall_02'):return'Wall_02'
 if name.startswith('slate_roof'):return'Slate_Roof'
 if name.startswith('stone&woodbeams'):return'Stones&Woodbeams'
 if name.startswith('material.002'):return'Planks'
 if name.startswith('iron'):return'Metall_01'
 raise AssertionError('Unreviewed source material: '+name)

def prepare_images():
 import numpy as np
 from PIL import Image
 DERIVED.mkdir(parents=True,exist_ok=True)
 for family,side in FAMILIES.items():
  def image(suffix):return Image.open(SOURCE/'Buildings&PropsTextures'/(family+'_'+suffix+'.png')).convert('RGB').resize((side,side),Image.Resampling.LANCZOS)
  base=image('Dif');normal=np.asarray(image('Nor'),dtype=np.float32)/127.5-1
  normal/=np.maximum(np.linalg.norm(normal,axis=2,keepdims=True),1e-6)
  normal=Image.fromarray(np.uint8(np.clip(np.rint((normal+1)*127.5),0,255)))
  # AO is an explicit source map; legacy Spec has unknown gloss semantics.
  # Preserve scalar source material roughness instead of inventing a conversion.
  ao=image('Aoc').convert('L')
  arm=Image.merge('RGB',(ao,Image.new('L',(side,side),round(ROUGH*255)),Image.new('L',(side,side),0)))
  for kind,img in [('base',base),('normal',normal),('arm',arm)]:img.save(DERIVED/(family+'_'+kind+'.png'),compress_level=6)

def read_glb(p):
 raw=Path(p).read_bytes();assert struct.unpack_from('<III',raw)==(0x46546c67,2,len(raw))
 n,t=struct.unpack_from('<II',raw,12);assert t==0x4e4f534a
 data=json.loads(raw[20:20+n]);at=20+n;n,t=struct.unpack_from('<II',raw,at);assert t==0x004e4942
 return data,raw[at+8:at+8+n]

def write_glb(p,data,binary):
 s=json.dumps(data,separators=(',',':')).encode();s+=b' '*(-len(s)%4);binary+=b'\0'*(-len(binary)%4)
 Path(p).write_bytes(struct.pack('<III',0x46546c67,2,28+len(s)+len(binary))+struct.pack('<II',len(s),0x4e4f534a)+s+struct.pack('<II',len(binary),0x004e4942)+binary)

def attach_materials(p):
 data,binary=read_glb(p);binary=bytearray(binary);data['images']=[];data['textures']=[]
 data['samplers']=[dict(magFilter=9729,minFilter=9987,wrapS=10497,wrapT=10497)]
 texture_ids={}
 for material in data['materials']:
  family=material['name'];assert family in FAMILIES
  ids=[]
  for kind in ['base','normal','arm']:
   key=(family,kind)
   if key not in texture_ids:
    encoded=(DERIVED/(family+'_'+kind+'.png')).read_bytes();binary+=b'\0'*(-len(binary)%4)
    view=len(data['bufferViews']);data['bufferViews'].append(dict(buffer=0,byteOffset=len(binary),byteLength=len(encoded)));binary+=encoded
    image=len(data['images']);data['images'].append(dict(name='bep_shared_'+family+'_'+kind,bufferView=view,mimeType='image/png'))
    texture_ids[key]=len(data['textures']);data['textures'].append(dict(sampler=0,source=image))
   ids.append(texture_ids[key])
  material.clear();material.update(name='BEP '+family,pbrMetallicRoughness=dict(baseColorFactor=[1,1,1,1],baseColorTexture=dict(index=ids[0]),metallicRoughnessTexture=dict(index=ids[2]),metallicFactor=1,roughnessFactor=1),normalTexture=dict(index=ids[1]),occlusionTexture=dict(index=ids[2]),alphaMode='OPAQUE',doubleSided=False)
 for mesh in data['meshes']:
  for primitive in mesh['primitives']:primitive['attributes'].pop('COLOR_0',None)
 data['buffers']=[dict(byteLength=len(binary))];data.pop('extensionsRequired',None);data.pop('extensionsUsed',None)
 write_glb(p,data,bytes(binary))

def inspect(p):
 import numpy as np
 from PIL import Image
 data,binary=read_glb(p)
 assert not any(data.get(k)for k in ['skins','animations','cameras','extensionsRequired'])
 def accessor(i):
  a=data['accessors'][i];v=data['bufferViews'][a['bufferView']]
  dt=np.dtype({5121:'u1',5123:'<u2',5125:'<u4',5126:'<f4'}[a['componentType']]);n={'SCALAR':1,'VEC2':2,'VEC3':3,'VEC4':4}[a['type']]
  return np.ndarray((a['count'],n),dtype=dt,buffer=binary,offset=v.get('byteOffset',0)+a.get('byteOffset',0),strides=(v.get('byteStride',n*dt.itemsize),dt.itemsize))
 points=[];triangles=0;vertices=0;primitive_count=0;opposing=0
 for mesh in data['meshes']:
  for q in mesh['primitives']:
   assert q.get('mode',4)==4 and 'TEXCOORD_0'in q['attributes']and 'TEXCOORD_1'not in q['attributes']
   v=accessor(q['attributes']['POSITION']);n=accessor(q['attributes']['NORMAL']);uv=accessor(q['attributes']['TEXCOORD_0']);idx=accessor(q['indices']).ravel()
   assert len(v)==len(n)==len(uv)and all(np.isfinite(a).all()for a in [v,n,uv])and idx.max()<len(v)
   assert np.max(np.abs(np.linalg.norm(n,axis=1)-1))<.005
   assert len(idx)%3==0
   faces=idx.reshape(-1,3);area_vectors=np.cross(v[faces[:,1]]-v[faces[:,0]],v[faces[:,2]]-v[faces[:,0]])
   assert (np.linalg.norm(area_vectors,axis=1)>1e-10).all(),'Degenerate runtime triangles: '+str(p)
   opposing+=int((np.einsum('ij,ij->i',area_vectors,n[faces].sum(axis=1))< -1e-9).sum())
   points.append(v);triangles+=len(idx)//3;vertices+=len(v);primitive_count+=1
 pnts=np.concatenate(points);lo=pnts.min(axis=0);hi=pnts.max(axis=0)
 assert abs(float(lo[1]))<.002 and abs(float(lo[0]+hi[0]))<.002 and abs(float(lo[2]+hi[2]))<.002
 for node in data['nodes']:
  assert not node.get('matrix')and node.get('translation',[0,0,0])==[0,0,0]and node.get('scale',[1,1,1])==[1,1,1]
 images=[]
 for image in data['images']:
  view=data['bufferViews'][image['bufferView']];b=binary[view.get('byteOffset',0):view.get('byteOffset',0)+view['byteLength']];im=Image.open(io.BytesIO(b))
  assert im.size[0]==im.size[1]and im.size[0]in FAMILIES.values()
  images.append(dict(name=image['name'],sha256=hashlib.sha256(b).hexdigest(),dimensions=list(im.size),bytes=len(b)))
 assert opposing/max(1,triangles)<.01,(p,'Opposing normal triangles',opposing,triangles)
 return dict(file=Path(p).name,triangles=triangles,vertices=vertices,primitive_count=primitive_count,material_count=len(data['materials']),bounds_min=lo.tolist(),bounds_max=hi.tolist(),dimensions_m=(hi-lo).tolist(),images=images,bytes=Path(p).stat().st_size,sha256=sha(p),geometry_checks=dict(finite=True,unit_normals=True,indices_in_range=True,uv0=True,nondegenerate_triangles=triangles,opposing_normal_triangles=opposing))

def validate(directory=OUT):
 d=Path(directory);manifest=json.loads((d/MANIFEST).read_text());assert len(manifest['assets'])==len(SPECS)*2
 shared={};pairs={}
 for expected in manifest['assets']:
  r=inspect(d/expected['file']);assert r['sha256']==expected['sha256']
  assert r['triangles']<=BUDGETS[expected['alias']][0 if expected['lod']=='near'else 1]
  pairs.setdefault(expected['alias'],{})[expected['lod']]=r
  for i in r['images']:
   if i['name']in shared:assert shared[i['name']]==i
   else:shared[i['name']]=i
 for alias,p in pairs.items():
  assert set(p)=={'near','mid'}and p['mid']['triangles']<p['near']['triangles']
  assert max(abs(a-b)for a,b in zip(p['near']['dimensions_m'],p['mid']['dimensions_m']))<.15,(alias,p)
 budget=dict(types=len(SPECS),models=len(manifest['assets']),unique_images=len(shared),estimated_rgba8_mips_mib=sum(i['dimensions'][0]*i['dimensions'][1]*4*4/3 for i in shared.values())/1048576,near_triangles=sum(p['near']['triangles']for p in pairs.values()),mid_triangles=sum(p['mid']['triangles']for p in pairs.values()))
 assert budget['estimated_rgba8_mips_mib']<=30.001,budget
 print('VALIDATED BEP props:',json.dumps(budget),flush=True)
 return budget

def cook():
 import bpy
 import bmesh
 from mathutils import Matrix,Vector
 prepare();prepare_images();OUT.mkdir(parents=True,exist_ok=True);records=[];inventory=[]
 with tempfile.TemporaryDirectory(prefix='.bep-props-',dir=OUT)as temporary:
  stage=Path(temporary)
  for alias,archive,file,names,scale,yaw,ratio in SPECS:
   bpy.ops.wm.read_factory_settings(use_empty=True)
   bpy.ops.import_scene.fbx(filepath=str(SOURCE/file),use_anim=False)
   original=[]
   for name in names:
    o=bpy.data.objects[name];assert o.type=='MESH'and o.data.uv_layers and not o.modifiers
    o.data.calc_loop_triangles()
    inventory.append(dict(alias=alias,source_file=file,object=name,triangles=len(o.data.loop_triangles),materials=[m.name for m in o.data.materials]))
    original.append(o.copy());original[-1].data=o.data.copy()
   rotation=Matrix.Rotation(math.radians(yaw),4,'Z')@Matrix.Scale(scale,4)
   points=[rotation@o.matrix_world@v.co for o in original for v in o.data.vertices]
   lo=Vector([min(v[i]for v in points)for i in range(3)]);hi=Vector([max(v[i]for v in points)for i in range(3)])
   canonical=Matrix.Translation(Vector((-(lo.x+hi.x)*.5,-(lo.y+hi.y)*.5,-lo.z)))@rotation
   for o in original:o.data.calc_loop_triangles()
   source_triangles=sum(len(o.data.loop_triangles)for o in original)
   for lod in ['near','mid']:
    bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
    # Recreate only reviewed mesh parts; full FBX includes other source LODs.
    objects=[];material_cache={}
    for source in original:
     o=source.copy();o.data=source.data.copy();o.parent=None;bpy.context.collection.objects.link(o);o.matrix_world=canonical@source.matrix_world
     families=[material_family(m.name)for m in o.data.materials]
     o.data.materials.clear()
     for family in families:
      if family not in material_cache:
       material_cache[family]=bpy.data.materials.new(family)
       material_cache[family].use_nodes=True
      m=material_cache[family]
      # Set precisely named temporary materials, not automatically found paths.
      o.data.materials.append(m)
     bpy.context.view_layer.objects.active=o;o.select_set(True)
     bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
     budget=BUDGETS[alias][0 if lod=='near'else 1]
     if source_triangles>budget:
      dec=o.modifiers.new('Offline source-preserving detail reduction','DECIMATE');dec.ratio=min(.99,budget*.985/source_triangles);dec.use_collapse_triangulate=True
      bpy.ops.object.modifier_apply(modifier=dec.name)
      if o.data.has_custom_normals:bpy.ops.mesh.customdata_custom_splitnormals_clear()
     # The source contains collinear decoration faces and decimation can leave
     # invalid n-gons. Resolve before export; preserve corner UVs and smoothing.
     o.data.validate(clean_customdata=False)
     bm=bmesh.new();bm.from_mesh(o.data);bmesh.ops.triangulate(bm,faces=list(bm.faces))
     bad=[f for f in bm.faces if f.calc_area()<=1e-10]
     if bad:bmesh.ops.delete(bm,geom=bad,context='FACES_ONLY')
     bm.normal_update();bm.to_mesh(o.data);bm.free();o.data.update()
     if o.data.has_custom_normals:bpy.ops.mesh.customdata_custom_splitnormals_clear()
     o.select_set(False);objects.append(o)
    # Join by shared material family while retaining UVs. Drop material name
    # suffixes created by Blender so byte-identical image cache keys are shared.
    for o in objects:o.select_set(True)
    bpy.context.view_layer.objects.active=objects[0];bpy.ops.object.join();o=bpy.context.object;o.name=alias
    for m in o.data.materials:m.name=material_family(m.name)if m.name.split('.')[0]not in FAMILIES else m.name.split('.')[0]
    # The source family Slate_Roof, Wall_02 etc is already canonical here.
    for m in o.data.materials:
     if '.'in m.name:m.name=m.name.split('.')[0]
    p=[v.co for v in o.data.vertices];low=Vector([min(v[i]for v in p)for i in range(3)]);high=Vector([max(v[i]for v in p)for i in range(3)])
    correction=Vector((-(low.x+high.x)*.5,-(low.y+high.y)*.5,-low.z));o.data.transform(Matrix.Translation(correction))
    path=stage/(alias+'_'+lod+'.glb')
    # Material datablock names can receive .001 suffixes; attach uses canonical
    # family from a separate marker before serializing the runtime material.
    for m in o.data.materials:m['bep_family']=m.name.split('.')[0]
    bpy.ops.export_scene.gltf(filepath=str(path),export_format='GLB',use_selection=True,export_yup=True,export_animations=False,export_cameras=False,export_lights=False,export_materials='EXPORT',export_apply=True)
    data,binary=read_glb(path)
    for m in data['materials']:m['name']=m['name'].split('.')[0]
    write_glb(path,data,binary);attach_materials(path)
    r=inspect(path);r.update(alias=alias,lod=lod,source_archive=archive,source_file=file,source_objects=names,source_scale=scale,source_rotation_degrees=yaw,source_to_canonical_blender=[list(row)for row in canonical],lod_origin_correction_blender=list(correction));records.append(r)
    print('COOKED',path.name,r['triangles'],r['dimensions_m'],flush=True)
  manifest=dict(schema_version=1,source='User-supplied BEP purchased archives',provenance='assets/purchased/bep_props/PROVENANCE.json',origin='Metres Y up, applied transforms, minimumY0 and XZ bounds centered. Source FBX duplicate LODs/stages excluded.',materials='Explicit original UV0 diffuse/normal/AO. Authored normalXYZ retained, renormalized after resize. Source FBX scalar roughness .6900348/nonmetal retained; old Spec preserved and not inverted.',lods='Selected original source surfaces reduced offline to per-type near/mid budgets; same original canonical origin; each model centered/grounded after reduction.',triangle_budgets=BUDGETS,assets=records)
  (stage/MANIFEST).write_text(json.dumps(manifest,indent=2)+'\n');manifest['budget']=validate(stage);(stage/MANIFEST).write_text(json.dumps(manifest,indent=2)+'\n')
  for r in records:os.replace(stage/r['file'],OUT/r['file'])
  os.replace(stage/MANIFEST,OUT/MANIFEST)
 (ASSET/'COOK_INSPECTION.json').write_text(json.dumps(inventory,indent=2)+'\n')
 provenance=dict(schema_version=1,archives=[str(DOWNLOADS/a)for a in sorted({s[1]for s in SPECS}|{'Buildings&PropsTextures.zip'})],license='Purchased user-supplied assets; no separate license in archives. No public redistribution or open-license assertion.',source_files=[dict(file=str(p.relative_to(ASSET)),bytes=p.stat().st_size,sha256=sha(p))for p in sorted(SOURCE.rglob('*'))if p.is_file()],derived_files=[dict(file=str(p.relative_to(ASSET)),bytes=p.stat().st_size,sha256=sha(p))for p in sorted(DERIVED.glob('*.png'))],adaptations=manifest['materials'],budget=manifest['budget'])
 (ASSET/'PROVENANCE.json').write_text(json.dumps(provenance,indent=2)+'\n');validate()

def preview():
 """CPU-only actual cooked near meshes, neutral lighting, one tile per type."""
 import bpy
 from mathutils import Vector
 from PIL import Image,ImageDraw
 tiles=[];folder=ASSET/'preview';folder.mkdir(parents=True,exist_ok=True)
 manifest=json.loads((OUT/MANIFEST).read_text())
 for r in manifest['assets']:
  if r['lod']!='near':continue
  bpy.ops.wm.read_factory_settings(use_empty=True)
  bpy.ops.import_scene.gltf(filepath=str(OUT/r['file']))
  w,h,d=r['dimensions_m'];extent=max(w,h,d,1)
  bpy.ops.mesh.primitive_plane_add(size=extent*6,location=(0,0,-.015))
  mat=bpy.data.materials.new('Neutral QA support');mat.diffuse_color=(.18,.20,.19,1);bpy.context.object.data.materials.append(mat)
  world=bpy.data.worlds.new('Neutral QA daylight');world.use_nodes=True;world.node_tree.nodes['Background'].inputs[0].default_value=(.65,.74,.86,1);world.node_tree.nodes['Background'].inputs[1].default_value=.65;bpy.context.scene.world=world
  bpy.ops.object.light_add(type='AREA',location=(extent*.8,-extent,extent*2));light=bpy.context.object;light.data.energy=500*extent*extent;light.data.size=extent*1.5
  target=Vector((0,0,h*.43));bpy.ops.object.camera_add(location=target+Vector((extent*.9,-extent*1.4,extent*.8)));camera=bpy.context.object;camera.rotation_euler=(target-camera.location).to_track_quat('-Z','Y').to_euler();camera.data.type='ORTHO';camera.data.ortho_scale=extent*1.7
  scene=bpy.context.scene;scene.camera=camera;scene.render.engine='CYCLES';scene.cycles.device='CPU';scene.cycles.samples=12;scene.cycles.use_denoising=False;scene.render.resolution_x=384;scene.render.resolution_y=384;scene.render.resolution_percentage=100;scene.view_settings.view_transform='AgX';scene.render.image_settings.file_format='PNG';scene.render.filepath=str(folder/(r['alias']+'.png'))
  bpy.ops.render.render(write_still=True)
  tile=Image.open(scene.render.filepath).convert('RGB');draw=ImageDraw.Draw(tile);draw.rectangle((0,348,384,384),fill=(24,28,27));draw.text((8,352),r['alias'],fill=(238,238,230));draw.text((8,367),str(r['triangles'])+' near triangles',fill=(190,200,190));tiles.append(tile)
 sheet=Image.new('RGB',(384*4,384*2),(20,24,24))
 for i,tile in enumerate(tiles):sheet.paste(tile,((i%4)*384,(i//4)*384))
 sheet.save(ASSET/'cooked-props-preview.png');print('CPU QA preview:',ASSET/'cooked-props-preview.png',flush=True)

if __name__=='__main__':
 args=sys.argv[sys.argv.index('--')+1:]if '--'in sys.argv else sys.argv[1:]
 if '--prepare'in args:prepare()
 elif '--validate'in args:validate()
 elif '--preview'in args:preview()
 else:cook()
