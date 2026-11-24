#!/usr/bin/env python3
"""Cook four user-supplied WildMesh wildlife families, with genuine source clips.

Prepare: python3 tools/import_wildmesh_wildlife.py --prepare
Cook: /home/computer/Downloads/blender-5.0.1-linux-x64/blender -b -t 2 \
  --disable-autoexec --factory-startup --python-exit-code 1 \
  --python tools/import_wildmesh_wildlife.py
Validate: python3 tools/import_wildmesh_wildlife.py --validate
Preview: same Blender invocation with -- --preview

Original ZIP, extracted blends and images stay unchanged. The runtime receives
bounded static pose meshes with compatible topology and one matte albedo atlas
per species; no source scripts, fur-shell expansion or invented PBR maps.
"""
import argparse
import hashlib
import json
import math
import os
from pathlib import Path, PurePosixPath
import shutil
import struct
import sys
import tempfile
import zipfile

ROOT=Path(__file__).resolve().parents[1]
ASSET=ROOT/'assets/purchased/wildmesh/wildlife'
SOURCE=ASSET/'source'
DERIVED=ASSET/'derived'
OUT=ROOT/'projects/valwood_world/models'
ARCHIVE=Path('/home/computer/Downloads/100+MODELS+-+WildMesh3D.zip')
BLENDER='/home/computer/Downloads/blender-5.0.1-linux-x64/blender'
MANIFEST='wildmesh_wildlife_manifest.json'
SIDE=512
SPECS={
 'buck':dict(folder='Buck',blend='SK_Buck.blend',arm='RigRoot',root='RigPelvis',head='RigHead',
   walk='Walk_2',idle='Idle_Breathing_01',height=2.15,near=9000,mid=3000,
   maps={'Deer_Body':('T_Buck_Body.png',(0,0,256,512)),
         'Deer_Head':('T_Buck_Head.png',(256,0,256,384)),
         'Chifre':('T_Buck_Antler.png',(256,384,128,128)),
         'Deer_Eye':('T_Buck_Eye.png',(384,384,128,128))}),
 'fox':dict(folder='Fox_1',blend='SK_Fox 1.blend',arm='SK-Fox',root='RigPelvis',head='RigHead',
   walk='Walk',idle='Idle_2',height=.55,near=9000,mid=2500,
   maps={'MI_MAT_Fox_01':('T_Fox_Body 1.png',(0,0,512,512))}),
 'wildboar':dict(folder='WildBoar',blend='SK_WildBoar.blend',arm='wild_boar_walk_fwd_bank_rgt.001',root='Hips',head='Head',
   walk='walk_fwd_01',idle='Idle',idle_range=(31,101),height=.95,near=9500,mid=3000,
   maps={'M_wildboar':('T_WildBoar_Body.png',(0,0,384,512)),
         'M_tusk':('T_WildBoar_Tusk.png',(384,0,128,512))}),
 'leopard':dict(folder='Leopard',blend='SK_Leopard.blend',arm='Leopard_M',root='RigPelvis',head='RigHead',
   walk='Walk-IP',idle='Idle_3',height=.80,near=9500,mid=3000,
   maps={'MI_Leopard_M_Body_01':('T_Leopard_Body.png',(0,0,512,512)),
         'Leopard_Eye-MAT':('T_Leopard_Body.png',(0,0,512,512))}),
}


def sha(path):
 h=hashlib.sha256()
 with Path(path).open('rb')as f:
  for b in iter(lambda:f.read(1024*1024),b''):h.update(b)
 return h.hexdigest()


def publish(path,raw):
 path=Path(path);path.parent.mkdir(parents=True,exist_ok=True)
 pending=path.with_name(path.name+'.pending');pending.write_bytes(raw);os.replace(pending,path)


def json_write(path,value):publish(path,(json.dumps(value,indent=2)+'\n').encode())


def prepare():
 from PIL import Image
 ASSET.mkdir(parents=True,exist_ok=True);DERIVED.mkdir(exist_ok=True)
 folders={s['folder']for s in SPECS.values()};entries=[]
 with zipfile.ZipFile(ARCHIVE)as z:
  species={category:sorted({PurePosixPath(i.filename).parts[2]for i in z.infolist()
   if len(PurePosixPath(i.filename).parts)>3 and PurePosixPath(i.filename).parts[1]==category})
   for category in ['Realistic Animals','Stylized Animals']}
  for i in z.infolist():
   parts=PurePosixPath(i.filename).parts
   if len(parts)<4 or parts[1]!='Realistic Animals' or parts[2]not in folders or i.is_dir():continue
   if not(i.filename.lower().endswith('.blend')or'/Textures/'in i.filename):continue
   relative=PurePosixPath(*parts[2:]);assert not relative.is_absolute()and'..'not in relative.parts
   dest=SOURCE/relative;dest.parent.mkdir(parents=True,exist_ok=True)
   if not dest.exists():
    with z.open(i)as f,dest.open('wb')as out:shutil.copyfileobj(f,out,length=1024*1024)
   assert dest.stat().st_size==i.file_size
   entries.append(dict(file=str(dest.relative_to(ROOT)),archive_entry=i.filename,bytes=i.file_size,sha256=sha(dest)))
 archive_hash=sha(ARCHIVE)
 json_write(ASSET/'PROVENANCE.json',dict(schema_version=1,archive=str(ARCHIVE),archive_bytes=ARCHIVE.stat().st_size,
  archive_sha256=archive_hash,selected_species=list(SPECS),source_files=entries,source_scripts_executed=False,
  source_preservation='Original ZIP and extracted source bytes retained unchanged; extraction and archive hashing use bounded1MiB chunks.',
  license='User-supplied WildMesh3D commercial asset collection. No public redistribution license inferred.',
  source_format='Original compressed Blender5 sources; Blender5.0.1 used with source auto-execution disabled.',
  material='Source diffuse only; opaque matte material, metallic0/roughness.82. Missing optional fur/normal references are not synthesized.'))
 json_write(ASSET/'archive-inventory.json',dict(archive=str(ARCHIVE),bytes=ARCHIVE.stat().st_size,
  species=species,selected=list(SPECS),files=entries))
 for animal,spec in SPECS.items():
  atlas=Image.new('RGBA',(SIDE,SIDE),(32,25,18,255))
  for image_name,rect in set(spec['maps'].values()):
   x,y,w,h=rect;gutter=2
   with Image.open(SOURCE/spec['folder']/'Textures'/image_name)as source:
    image=source.convert('RGB').resize((w-2*gutter,h-2*gutter),Image.Resampling.LANCZOS)
   # Padding uses edge samples; source images use no live opacity after fur
   # cards are removed, so utility alpha is explicitly omitted.
   padded=Image.new('RGB',(w,h));padded.paste(image,(gutter,gutter))
   padded.paste(image.crop((0,0,1,image.height)).resize((gutter,image.height)),(0,gutter))
   padded.paste(image.crop((image.width-1,0,image.width,image.height)).resize((gutter,image.height)),(w-gutter,gutter))
   padded.paste(padded.crop((0,gutter,w,gutter+1)).resize((w,gutter)),(0,0))
   padded.paste(padded.crop((0,h-gutter-1,w,h-gutter)).resize((w,gutter)),(0,h-gutter))
   atlas.paste(padded,(x,SIDE-y-h))
  atlas.save(DERIVED/f'{animal}_albedo.png',optimize=False)
 print('WILDMESH WILDLIFE PREPARED',archive_hash,flush=True)


def write_glb(path,arrays,animal,label,mode):
 import numpy as np
 p,n,uv,tangent,indices=arrays;blob=bytearray();views=[];accessors=[]
 def append(raw,target=None):
  blob.extend(b'\0'*(-len(blob)%4));v=dict(buffer=0,byteOffset=len(blob),byteLength=len(raw))
  if target:v['target']=target
  views.append(v);blob.extend(raw);return len(views)-1
 def accessor(a,typ,component,target,bounds=False):
  a=np.ascontiguousarray(a);v=dict(bufferView=append(a.tobytes(),target),componentType=component,count=len(a),type=typ)
  if bounds:v.update(min=a.min(axis=0).tolist(),max=a.max(axis=0).tolist())
  accessors.append(v);return len(accessors)-1
 attributes={key:accessor(a.astype('<f4'),typ,5126,34962,key=='POSITION')
  for key,a,typ in [('POSITION',p,'VEC3'),('NORMAL',n,'VEC3'),('TEXCOORD_0',uv,'VEC2'),('TANGENT',tangent,'VEC4')]}
 component,dtype=(5123,'<u2')if len(p)<65536 else(5125,'<u4')
 index=accessor(indices.reshape(-1).astype(dtype),'SCALAR',component,34963)
 image=append((DERIVED/f'{animal}_albedo.png').read_bytes())
 doc=dict(asset=dict(version='2.0',generator='Valwood deterministic original WildMesh wildlife cooker'),scene=0,
  scenes=[dict(nodes=[0])],nodes=[dict(name=label,mesh=0)],
  meshes=[dict(name=label,primitives=[dict(attributes=attributes,indices=index,material=0,mode=4)])],
  materials=[dict(name=f'WildMesh {animal} source diffuse matte',pbrMetallicRoughness=dict(
   baseColorFactor=[1,1,1,1],baseColorTexture=dict(index=0),metallicFactor=0,roughnessFactor=.82),alphaMode='OPAQUE',doubleSided=False)],
  images=[dict(name=f'wildmesh_{animal}_albedo',bufferView=image,mimeType='image/png')],
  textures=[dict(source=0,sampler=0)],samplers=[dict(magFilter=9729,minFilter=9987,wrapS=10497,wrapT=10497)],
  accessors=accessors,bufferViews=views,buffers=[dict(byteLength=len(blob))],
  extras=dict(valwood_pose_contract_version=1,source_species=animal,source_scripts_executed=False,source_mode=mode))
 raw=json.dumps(doc,separators=(',',':')).encode();raw+=b' '*(-len(raw)%4);blob.extend(b'\0'*(-len(blob)%4))
 publish(path,struct.pack('<III',0x46546c67,2,28+len(raw)+len(blob))+struct.pack('<II',len(raw),0x4e4f534a)+raw+struct.pack('<II',len(blob),0x004e4942)+blob)


def array_mesh(mesh,matrix,reference=None):
 """Use immutable reference loop representatives and index/UV seam mapping."""
 import numpy as np
 uv=mesh.uv_layers.active;assert uv
 position=np.array([v.co[:]for v in mesh.vertices],dtype=np.float64)
 m=np.asarray(matrix,dtype=np.float64)
 position=position@m[:3,:3].T+m[:3,3]
 normal=np.array([n.vector[:]for n in mesh.corner_normals],dtype=np.float64)
 normal=normal@np.linalg.inv(m[:3,:3])
 texture=np.array([v.uv[:]for v in uv.data],dtype='<f4')
 triangles=np.array([p.loop_indices[:]for p in mesh.polygons],dtype=np.int32);assert triangles.shape[1]==3
 loop_vertices=np.array([v.vertex_index for v in mesh.loops],dtype=np.int32)
 winding=-1 if np.linalg.det(m[:3,:3])<0 else 1
 zero=np.linalg.norm(normal,axis=1)<1e-12
 if np.any(zero):
  # A few source eye corners contain zero normals. Rebuild those corners
  # from their weighted geometric neighbours; retain all valid source normals.
  ids=loop_vertices[triangles];face=np.cross(position[ids[:,1]]-position[ids[:,0]],position[ids[:,2]]-position[ids[:,0]])*winding
  geometric=np.zeros_like(position)
  for corner in range(3):np.add.at(geometric,ids[:,corner],face)
  normal[zero]=geometric[loop_vertices[zero]]
  remaining=np.linalg.norm(normal,axis=1)<1e-12;normal[remaining]=(0,0,1)
 normal/=np.maximum(np.linalg.norm(normal,axis=1)[:,None],1e-12)
 if reference is None:
  keys={};representatives=[];remap=[]
  for i,vertex in enumerate(loop_vertices):
   key=(int(vertex),*texture[i],*np.round(normal[i],6))
   if key not in keys:keys[key]=len(representatives);representatives.append(i)
   remap.append(keys[key])
  reference=dict(representatives=np.asarray(representatives,dtype=np.int32),remap=np.asarray(remap,dtype=np.int32),
   vertices=loop_vertices.copy(),uv=texture.copy(),triangles=triangles.copy(),winding=winding)
 else:
  assert np.array_equal(loop_vertices,reference['vertices'])and np.array_equal(texture,reference['uv'])and np.array_equal(triangles,reference['triangles'])and winding==reference['winding']
 ids=reference['representatives'];p=position[loop_vertices[ids]];n=normal[ids];uv=texture[ids].copy()
 # glTF/decoded image V runs downward; Blender UV V runs upward. Keep this
 # conversion at the single encoded-array boundary, including static LODs.
 uv[:,1]=1-uv[:,1]
 indices=reference['remap'][triangles]
 if winding<0:indices=indices[:,[0,2,1]]
 return(p,n,uv,indices),reference


def cook():
 import bpy,bmesh
 import numpy as np
 from mathutils import Matrix
 sys.path.insert(0,str(ROOT/'tools'))
 from import_village_shield_knight import tangents
 provenance=json.loads((ASSET/'PROVENANCE.json').read_text());families=[]
 with tempfile.TemporaryDirectory(prefix='valwood-wildlife-')as temporary:
  stage=Path(temporary)
  for animal,spec in SPECS.items():
   bpy.context.preferences.filepaths.use_scripts_auto_execute=False
   path=SOURCE/spec['folder']/spec['blend']
   bpy.ops.wm.open_mainfile(filepath=str(path),load_ui=False,use_scripts=False)
   scene=bpy.context.scene;fps=scene.render.fps/scene.render.fps_base
   arm=bpy.data.objects[spec['arm']];assert arm.type=='ARMATURE'
   arm.animation_data_create()
   for track in arm.animation_data.nla_tracks:track.mute=True
   originals=[o for o in scene.objects if o.type=='MESH'and any(m and m.name in spec['maps']for m in o.data.materials)]
   assert originals
   removed=[];source_triangles=0
   for obj in originals:
    source_triangles+=sum(len(p.vertices)-2 for p in obj.data.polygons)
    mesh=obj.data;bm=bmesh.new();bm.from_mesh(mesh)
    unwanted=[p for p in bm.faces if mesh.materials[p.material_index].name not in spec['maps']]
    removed.append(dict(object=obj.name,removed_fur_triangles=sum(len(p.verts)-2 for p in unwanted)))
    if unwanted:bmesh.ops.delete(bm,geom=unwanted,context='FACES')
    loose=[v for v in bm.verts if not v.link_faces]
    if loose:bmesh.ops.delete(bm,geom=loose,context='VERTS')
    bm.to_mesh(mesh);bm.free();mesh.update()
   cleaned=sum(sum(len(p.vertices)-2 for p in o.data.polygons)for o in originals)
   for obj in originals:
    if cleaned>spec['near']:
     bpy.context.view_layer.objects.active=obj
     modifier=obj.modifiers.new('Wildlife bounded source topology','DECIMATE');modifier.ratio=spec['near']/cleaned
     while list(obj.modifiers).index(modifier)>0:bpy.ops.object.modifier_move_up(modifier=modifier.name)
     bpy.ops.object.modifier_apply(modifier=modifier.name)
    mesh=obj.data;bm=bmesh.new();bm.from_mesh(mesh)
    bmesh.ops.triangulate(bm,faces=list(bm.faces),quad_method='FIXED',ngon_method='EAR_CLIP');bm.to_mesh(mesh);bm.free();mesh.update()
    uv=mesh.uv_layers.active;assert uv
    for polygon in mesh.polygons:
     _,(x,y,w,h)=spec['maps'][mesh.materials[polygon.material_index].name]
     source_uv=[uv.data[index].uv.copy()for index in polygon.loop_indices]
     # Some boar UV islands use the neighbouring repeated tile at U<0.
     # Translate a whole triangle by one integer period, keeping seam
     # interpolation intact; never modulo individual corners across a seam.
     shifts=[math.floor((min(v[i]for v in source_uv)+max(v[i]for v in source_uv))*.5)for i in range(2)]
     for index in polygon.loop_indices:
      old=uv.data[index].uv.copy();old.x-=shifts[0];old.y-=shifts[1]
      assert -.002<=old.x<=1.002 and -.002<=old.y<=1.002,(animal,obj.name,'Triangle crosses repeated texture seam',tuple(old))
      uv.data[index].uv=((x+2+min(1,max(0,old.x))*(w-4))/SIDE,(y+2+min(1,max(0,old.y))*(h-4))/SIDE)
   def evaluate(action_name,fraction):
    action=bpy.data.actions[action_name];arm.animation_data.action=action
    curves=[f for layer in action.layers for strip in layer.strips for slot in action.slots
     for bag in [strip.channelbag(slot)]if bag for f in bag.fcurves]
    assert any(f.data_path.startswith('pose.bones[')for f in curves),(animal,action_name,'Not a skeletal source clip')
    if action.slots:arm.animation_data.action_slot=action.slots[0]
    a,b=spec.get('idle_range',action.frame_range)if action_name==spec['idle']else action.frame_range
    frame=a+fraction*(b-a)
    scene.frame_set(math.floor(frame),subframe=frame%1);bpy.context.view_layer.update()
    return action,float(frame)
   evaluate(spec['idle'],0)
   # Rest anatomy defines facing; animated head turns never rotate the root.
   forward=arm.matrix_world@arm.data.bones[spec['head']].head_local-arm.matrix_world@arm.data.bones[spec['root']].head_local
   angle=-math.pi/2-math.atan2(forward.y,forward.x);c,s=math.cos(angle),math.sin(angle)
   basis=np.array([[c,-s,0],[0,0,1],[-s,-c,0]],dtype=np.float64)
   references={}
   def sample(reference=False):
    graph=bpy.context.evaluated_depsgraph_get();parts=[]
    for original in originals:
     evaluated=original.evaluated_get(graph);mesh=evaluated.to_mesh(preserve_all_data_layers=True,depsgraph=graph)
     arrays,fixed=array_mesh(mesh,evaluated.matrix_world,None if reference else references[original.name])
     if reference:references[original.name]=fixed
     parts.append(arrays);evaluated.to_mesh_clear()
    p,n,uv=np.concatenate([a[0]for a in parts]),np.concatenate([a[1]for a in parts]),np.concatenate([a[2]for a in parts])
    offset=0;indexes=[]
    for a in parts:indexes.append(a[3]+offset);offset+=len(a[0])
    root=arm.evaluated_get(graph).matrix_world@arm.evaluated_get(graph).pose.bones[spec['root']].head
    return p,n,uv,np.concatenate(indexes),np.asarray(root)
   ref_p,ref_n,ref_uv,indices,ref_root=sample(True)
   fixed_scale=spec['height']/(ref_p[:,2].max()-ref_p[:,2].min())
   horizontal_offset=(ref_p[:,:2].min(axis=0)+ref_p[:,:2].max(axis=0))*.5-ref_root[:2]
   def normalized():
    p,n,uv,index,root=sample();p[:,:2]-=root[:2]+horizontal_offset;p[:,2]-=p[:,2].min()
    p=(p@basis.T*fixed_scale).astype('<f4');n=(n@basis.T).astype('<f4')
    assert np.array_equal(index,indices)and np.array_equal(uv,ref_uv)
    return p,n,uv,index
   p,n,uv,index=normalized();sign=tangents(p,n,uv,index)[:,3].copy()
   # Estimate physical travel from genuine stance-foot motion. Four foot
   # bone heads are sampled over64 intervals; only backward movement within
   #25mm of that foot's cycle minimum is treated as a support contact.
   feet=['LeftHand','RightHand','LeftFoot','RightFoot']if animal=='wildboar'else['RigLFLegAnkle','RigRFLegAnkle','RigLBLegAnkle','RigRBLegAnkle']
   foot_samples=[];root_samples=[]
   for step in range(65):
    evaluate(spec['walk'],step/64);graph=bpy.context.evaluated_depsgraph_get();rig=arm.evaluated_get(graph)
    root=rig.matrix_world@rig.pose.bones[spec['root']].head;root_samples.append(np.asarray(root))
    points=np.asarray([rig.matrix_world@rig.pose.bones[foot].head for foot in feet],dtype=np.float64)
    points[:,:2]-=np.asarray(root)[:2];foot_samples.append(points@basis.T*fixed_scale)
   foot_samples=np.asarray(foot_samples);minimum=foot_samples[:,:,1].min(axis=0)
   action=bpy.data.actions[spec['walk']];walk_seconds=(action.frame_range[1]-action.frame_range[0])/fps
   velocity=np.diff(foot_samples[:,:,2],axis=0)/(walk_seconds/64)
   support=(foot_samples[:-1,:,1]<=minimum+.025)&(foot_samples[1:,:,1]<=minimum+.025)&(velocity<-.02)
   speeds=-velocity[support]
   print('WILDLIFE STRIDE PROBE',animal,'velocity',velocity.min(axis=0).tolist(),velocity.max(axis=0).tolist(),
    'footheight_minmax',minimum.tolist(),foot_samples[:,:,1].max(axis=0).tolist(),'contacts',int(support.sum()),flush=True)
   assert len(speeds)>=8,(animal,'Insufficient stance contacts',len(speeds))
   contact_stride=float(np.median(speeds)*walk_seconds)
   root_displacement=float(((root_samples[-1]-root_samples[0])@basis.T*fixed_scale)[2])
   stride=abs(root_displacement)if abs(root_displacement)>.05 else contact_stride
   assert .05<stride<4,(animal,'Stride estimate',stride)
   stride_evidence=dict(method='Exact source root-cycle displacement when above50mm; otherwise median backward stance-foot head velocity within25mm of per-foot minimum over64 intervals, times cycle duration.',
    feet=feet,support_intervals=int(support.sum()),contact_estimate_m=contact_stride,source_root_cycle_forward_displacement_m=root_displacement,stride_m=stride)
   records=[];cycles={};all_positions=[];union_min=np.full(3,np.inf);union_max=np.full(3,-np.inf)
   for mode,count in [('walk',8),('idle',4)]:
    name=spec[mode];action=bpy.data.actions[name];original_range=list(map(float,action.frame_range))
    start,end=spec.get('idle_range',original_range)if mode=='idle'else original_range
    cycles[mode]=dict(action=name,original_source_frames=original_range,source_frames=[start,end],fps=fps,seconds=(end-start)/fps,pose_count=count)
    positions=[]
    for frame in range(count):
     _,source_frame=evaluate(name,frame/count);p,n,uv,index=normalized()
     assert np.isfinite(p).all()and np.isfinite(n).all()and abs(p[:,1].min())<.000001
     tangent=tangents(p,n,uv,index,sign);arrays=p,n,uv,tangent,index
     file=f'wildmesh_{animal}_{mode}_{frame:02}.glb';write_glb(stage/file,arrays,animal,f'Body_wildmesh_{animal}_{mode}_{frame:02}',mode)
     record=dict(file=file,mode=mode,frame=frame,source_frame=source_frame,source_action=name,triangles=len(index),vertices=len(p),
      bounds_min=p.min(axis=0).tolist(),bounds_max=p.max(axis=0).tolist(),sha256=sha(stage/file),bytes=(stage/file).stat().st_size)
     records.append(record);positions.append(p.copy());all_positions.append(p.copy())
     union_min=np.minimum(union_min,p.min(axis=0));union_max=np.maximum(union_max,p.max(axis=0))
    # Every clip moves actual source vertices, rather than duplicating a pose.
    delta=max(float(np.linalg.norm(a-positions[0],axis=1).max())for a in positions[1:]);assert delta>1e-5,(animal,mode,'Static action binding')
    pair_distances=[];max_float=0
    for a,b in zip(positions,positions[1:]+positions[:1]):
     d=np.linalg.norm(a-b,axis=1);pair_distances.append(dict(rms_m=float(np.sqrt(np.mean(d*d))),max_m=float(d.max())))
     for weight in np.linspace(0,1,17):max_float=max(max_float,float(((1-weight)*a+weight*b)[:,1].min()))
    cycles[mode].update(sampled_adjacent_pairs=pair_distances,interpolated_max_sole_float_m=max_float,max_vertex_motion_m=delta)
   near=f'wildmesh_{animal}_near.glb';shutil.copyfile(stage/f'wildmesh_{animal}_idle_00.glb',stage/near)
   # Static far catalog mesh; animated near poses retain the shared topology.
   evaluate(spec['idle'],0);p,n,uv,index=normalized()
   mesh=bpy.data.meshes.new('Wildlife static mid source');mesh.from_pydata(p.tolist(),[],index.tolist());mesh.update()
   layer=mesh.uv_layers.new(name='UVMap')
   for polygon in mesh.polygons:
    polygon.use_smooth=True
    for loop in polygon.loop_indices:
     u,v=uv[mesh.loops[loop].vertex_index];layer.data[loop].uv=(u,1-v)
   obj=bpy.data.objects.new('Wildlife static mid',mesh);scene.collection.objects.link(obj);bpy.context.view_layer.objects.active=obj
   if len(index)>spec['mid']:
    modifier=obj.modifiers.new('Static mid detail','DECIMATE');modifier.ratio=spec['mid']/len(index);modifier.use_collapse_triangulate=True
    bpy.ops.object.modifier_apply(modifier=modifier.name)
   mid_arrays,_=array_mesh(obj.data,Matrix.Identity(4));mp,mn,mu,mi=mid_arrays;mp[:,1]-=mp[:,1].min()
   mt=tangents(mp,mn,mu,mi);mid=f'wildmesh_{animal}_mid.glb';write_glb(stage/mid,(mp,mn,mu,mt,mi),animal,f'Body_wildmesh_{animal}_mid','static_mid')
   static=[dict(file=near,triangles=records[8]['triangles'],sha256=sha(stage/near),bounds_min=records[8]['bounds_min'],bounds_max=records[8]['bounds_max']),
    dict(file=mid,triangles=len(mi),sha256=sha(stage/mid),bounds_min=mp.min(axis=0).tolist(),bounds_max=mp.max(axis=0).tolist())]
   assert records[0]['triangles']<=10000 and len(mi)<=spec['mid']+10
   family=dict(species=animal,source_folder=spec['folder'],source_blend=str(path.relative_to(ROOT)),source_sha256=sha(path),
    source_armature=arm.name,source_root_bone=spec['root'],source_head_bone=spec['head'],source_forward='Rest head-to-pelvis anatomy',
    source_scripts_executed=False,source_total_triangles=source_triangles,removed_optional_fur=removed,
    near_triangles=records[0]['triangles'],mid_triangles=len(mi),reference_height_m=spec['height'],fixed_scale=fixed_scale,
    forward='+Z',up='+Y',grounding='Minimum body/antler/tusk Y=0 per pose; fixed horizontal reference offset plus removal of animated pelvis root motion.',
    bounds_min=union_min.tolist(),bounds_max=union_max.tolist(),cycles=cycles,stride_m=stride,stride_measurement=stride_evidence,pose_contract_version=1,
    material='Original diffuse atlas; OPAQUE, metallic0,roughness.82; no normal/ORM/emission maps.',
    atlas=dict(file=str((DERIVED/f'{animal}_albedo.png').relative_to(ROOT)),side=SIDE,sha256=sha(DERIVED/f'{animal}_albedo.png'),rgba8_mips_bytes=SIDE*SIDE*4*4//3),
    assets=records,static_catalog=static)
   families.append(family)
   print('WILDMESH WILDLIFE FAMILY',animal,len(records),family['near_triangles'],family['mid_triangles'],cycles,flush=True)
  manifest=dict(schema_version=1,source_archive=str(ARCHIVE),source_archive_sha256=provenance['archive_sha256'],
   source_preserved=True,source_scripts_executed=False,blender=BLENDER,families=families,
   texture_unique_entries=len(families),texture_rgba8_mips_bytes=sum(f['atlas']['rgba8_mips_bytes']for f in families),
   notes='Four source-authored wildlife species.48 genuine source pose meshes plus8 static near/mid catalog files. Animated poses have fixed reference topology/UV/tangent signs across walk and idle; source skeletal evaluation occurs offline. No fur expansion or invented PBR maps.')
  validate_manifest(stage,manifest)
  OUT.mkdir(parents=True,exist_ok=True)
  for family in families:
   for record in family['assets']+family['static_catalog']:publish(OUT/record['file'],(stage/record['file']).read_bytes())
  json_write(OUT/MANIFEST,manifest);json_write(ASSET/MANIFEST,manifest)
 print('WILDMESH WILDLIFE COOK PASS',len(families),manifest['texture_rgba8_mips_bytes'],flush=True)


def validate_manifest(directory,manifest):
 import numpy as np
 sys.path.insert(0,str(ROOT/'tools'))
 from audit_village_pose_compatibility import glb,pose,values
 images=set()
 for family in manifest['families']:
  baseline=None;sign=None
  assert len(family['assets'])==12
  for record in family['assets']+family['static_catalog']:
   path=Path(directory)/record['file'];assert sha(path)==record['sha256']
   doc,binary=glb(path);assert not doc.get('skins')and not doc.get('animations')and not doc.get('extensionsRequired')
   assert len(doc['meshes'])==len(doc['materials'])==len(doc['images'])==1
   m=doc['materials'][0];pbr=m['pbrMetallicRoughness'];assert pbr['metallicFactor']==0 and pbr['roughnessFactor']==.82 and m['alphaMode']=='OPAQUE'
   assert not m.get('normalTexture')and not pbr.get('metallicRoughnessTexture')and not m.get('emissiveTexture')
   view=doc['bufferViews'][doc['images'][0]['bufferView']];image=binary[view.get('byteOffset',0):view.get('byteOffset',0)+view['byteLength']]
   assert struct.unpack_from('>II',image,16)==(SIDE,SIDE)and hashlib.sha256(image).hexdigest()==family['atlas']['sha256'];images.add(family['atlas']['sha256'])
   primitive=doc['meshes'][0]['primitives'][0];a=primitive['attributes']
   p,n,uv,t=[values(doc,binary,a[k])for k in ['POSITION','NORMAL','TEXCOORD_0','TANGENT']]
   index=values(doc,binary,primitive['indices']).reshape(-1,3)
   assert all(np.isfinite(x).all()for x in[p,n,uv,t])and len(index)==record['triangles']
   assert abs(p[:,1].min())<.00001,(record['file'],'Grounding',float(p[:,1].min()))
   assert np.max(np.abs(np.linalg.norm(n,axis=1)-1))<.002,(record['file'],'Normal lengths',np.linalg.norm(n,axis=1).min(),np.linalg.norm(n,axis=1).max())
   assert np.max(np.abs(np.linalg.norm(t[:,:3],axis=1)-1))<.002 and np.max(np.abs(np.sum(n*t[:,:3],axis=1)))<.002
   assert set(np.unique(t[:,3]))<={-1,1}
   if record.get('mode')in['walk','idle']:
    signature=[v['signature']for v in pose(path)['records']]
    if baseline is None:baseline=signature;sign=t[:,3].copy()
    assert signature==baseline and np.array_equal(sign,t[:,3]),record['file']
  print('WILDMESH VALID',family['species'],family['near_triangles'],family['mid_triangles'],flush=True)
 assert len(images)==4 and manifest['texture_rgba8_mips_bytes']<6*1048576


def preview():
 import bpy
 from mathutils import Vector
 manifest=json.loads((OUT/MANIFEST).read_text())
 for family in manifest['families']:
  bpy.ops.wm.read_factory_settings(use_empty=True);scene=bpy.context.scene
  bpy.ops.import_scene.gltf(filepath=str(OUT/f'wildmesh_{family["species"]}_near.glb'))
  meshes=[o for o in scene.objects if o.type=='MESH'];assert len(meshes)==1
  body=meshes[0];points=[body.matrix_world@Vector(c)for c in body.bound_box]
  low=Vector(tuple(min(p[i]for p in points)for i in range(3)));high=Vector(tuple(max(p[i]for p in points)for i in range(3)))
  center=(low+high)*.5;span=max(high-low)
  bpy.ops.mesh.primitive_plane_add(size=100);ground=bpy.context.object
  mat=bpy.data.materials.new('Neutral preview ground');mat.diffuse_color=(.13,.14,.12,1);ground.data.materials.append(mat)
  world=bpy.data.worlds.new('Soft wildlife preview sky');world.use_nodes=True;world.node_tree.nodes['Background'].inputs[0].default_value=(.19,.24,.30,1);world.node_tree.nodes['Background'].inputs[1].default_value=.8;scene.world=world
  for relative,energy in[((-2,-3,4),700),((3,1,3),450)]:
   bpy.ops.object.light_add(type='AREA',location=center+Vector(relative)*span)
   light=bpy.context.object;light.data.energy=energy*span*span;light.data.shape='DISK';light.data.size=span*3
   light.rotation_euler=(center-light.location).to_track_quat('-Z','Y').to_euler()
  bpy.ops.object.camera_add(location=center+Vector((2,-3,1.25))*span)
  camera=bpy.context.object;camera.rotation_euler=(center-camera.location).to_track_quat('-Z','Y').to_euler();camera.data.type='ORTHO';camera.data.ortho_scale=span*1.4;scene.camera=camera
  scene.render.engine='CYCLES';scene.cycles.device='CPU';scene.cycles.samples=24;scene.cycles.use_denoising=False
  scene.render.resolution_x=800;scene.render.resolution_y=650;scene.render.resolution_percentage=100
  scene.render.filepath=str(ASSET/f'{family["species"]}-cooked-cpu-preview.png');bpy.ops.render.render(write_still=True)


if __name__=='__main__':
 arguments=sys.argv[sys.argv.index('--')+1:]if'--'in sys.argv else([]if'bpy'in sys.modules else sys.argv[1:])
 parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--prepare',action='store_true');parser.add_argument('--validate',action='store_true');parser.add_argument('--preview',action='store_true');args=parser.parse_args(arguments)
 if args.prepare:prepare()
 elif args.validate:validate_manifest(OUT,json.loads((OUT/MANIFEST).read_text()))
 elif args.preview:preview()
 else:cook()
