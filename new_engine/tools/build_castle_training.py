#!/usr/bin/env python3
"""Bake genuine Defender practice motion into a bounded, shared-texture pose set.

blender -b -t 2 --factory-startup --disable-autoexec --python-exit-code 1 \
  --python tools/build_castle_training.py

The downloaded source is read with script auto-execution disabled and never
saved. Supplied Attack-two handed2 frames33..113 supply a measured two-handed
raising/striking exercise; four quaternion-slerped recovery samples close it.
Only this offline builder evaluates the rig. Runtime uses24 GPU-blended poses.
"""
from pathlib import Path
import argparse,hashlib,json,math,os,struct,sys,tempfile
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'projects/valwood_world/models'
POSES=24
SOURCE_START,SOURCE_END,SOURCE_SAMPLES=33.,113.,20
CYCLE_SECONDS=4.

def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def cook(output):
 import bpy,bmesh
 import numpy as np
 from mathutils import Matrix,Vector
 sys.path.insert(0,str(ROOT/'tools'))
 from village_defender import SOURCE,SOURCE_URL,LICENSE_URL,CREDIT,BODY_HEIGHT,IDLE_FRAME,_maps,_part
 from bake_gltf_uv import bake_uv_transforms
 from audit_village_pose_compatibility import glb,pose
 output=Path(output);output.mkdir(parents=True,exist_ok=True)
 bpy.context.preferences.filepaths.use_scripts_auto_execute=False
 bpy.ops.wm.open_mainfile(filepath=str(SOURCE),load_ui=False,use_scripts=False)
 scene=bpy.context.scene;arm=bpy.data.objects['Defender_Ske'];original=bpy.data.objects['Defender']
 for track in arm.animation_data.nla_tracks:track.mute=True
 # Fix source tessellation BEFORE skinning. Every baked pose then has the same
 # original triangle IDs, UV seams and material sections, even during a strike.
 bm=bmesh.new();bm.from_mesh(original.data);bmesh.ops.triangulate(bm,faces=list(bm.faces));bm.to_mesh(original.data);bm.free();original.data.update()
 mats=_maps(bpy)
 def sample(action,frame):
  arm.animation_data.action=action;scene.frame_set(math.floor(frame),subframe=frame%1.);bpy.context.view_layer.update()
  graph=bpy.context.evaluated_depsgraph_get();ev=original.evaluated_get(graph);a=arm.evaluated_get(graph)
  mesh=bpy.data.meshes.new_from_object(ev,depsgraph=graph);mesh.transform(ev.matrix_world)
  return mesh,a.matrix_world@a.pose.bones['Hips'].head,((a.matrix_world@a.pose.bones['RightFingerBase'].head).copy(),(a.matrix_world@a.pose.bones['LeftFingerBase'].head).copy())
 walk=bpy.data.actions['Walk-normal.001'];attack=bpy.data.actions['Attack-two handed2']
 ref,hip,ref_hand=sample(walk,IDLE_FRAME)
 indices={i for p in ref.polygons if p.material_index==1 for i in p.vertices}
 scale=BODY_HEIGHT/(max(ref.vertices[i].co.z for i in indices)-min(ref.vertices[i].co.z for i in indices))
 reference_gear=_part(bpy,ref,0,'Gear_Training_Poleaxe',mats[1]);reference_gear_points=[v.co.copy() for v in reference_gear.data.vertices]
 points=np.asarray([tuple(p) for p in reference_gear_points]);axis=np.linalg.eigh(np.cov(points.T))[1][:,-1];axis*=1 if axis[2]>0 else -1;reference_axis=Vector(axis);reference_grip=ref_hand[0]
 bpy.data.meshes.remove(ref)
 # Source endpoints are retained as local bone transforms. Recovery is an
 # explicit authored adaptation, never claimed as an additional source clip.
 endpoints=[]
 for f in (SOURCE_START,SOURCE_END):
  mesh,_,_=sample(attack,f);bpy.data.meshes.remove(mesh)
  endpoints.append({b.name:b.matrix_basis.copy().decompose() for b in arm.pose.bones})
 records=[];base=None;texture_images=None;bounds=[]
 with tempfile.TemporaryDirectory(prefix='valwood-training-') as directory:
  stage=Path(directory)
  for frame in range(POSES):
   fraction=None
   if frame<SOURCE_SAMPLES:
    source_frame=SOURCE_START+(SOURCE_END-SOURCE_START)*frame/(SOURCE_SAMPLES-1)
    mesh,hip,hand=sample(attack,source_frame)
   else:
    source_frame=None;fraction=(frame-SOURCE_SAMPLES+1)/(POSES-SOURCE_SAMPLES+1)
    arm.animation_data.action=None
    for b in arm.pose.bones:
     la,qa,sa=endpoints[1][b.name];lb,qb,sb=endpoints[0][b.name]
     b.matrix_basis=Matrix.LocRotScale(la.lerp(lb,fraction),qa.slerp(qb,fraction),sa.lerp(sb,fraction))
    bpy.context.view_layer.update();g=bpy.context.evaluated_depsgraph_get();ev=original.evaluated_get(g);a=arm.evaluated_get(g)
    mesh=bpy.data.meshes.new_from_object(ev,depsgraph=g);mesh.transform(ev.matrix_world)
    hip=a.matrix_world@a.pose.bones['Hips'].head;hand=((a.matrix_world@a.pose.bones['RightFingerBase'].head).copy(),(a.matrix_world@a.pose.bones['LeftFingerBase'].head).copy())
   body=_part(bpy,mesh,1,'Body_Training_Knight',mats[0]);bpy.data.meshes.remove(mesh)
   gear=bpy.data.objects.new('Gear_Training_Poleaxe',reference_gear.data.copy());bpy.context.collection.objects.link(gear)
   shaft=(hand[0]-hand[1]).normalized();rigid=reference_axis.rotation_difference(shaft)
   for v,p in zip(gear.data.vertices,reference_gear_points):v.co=rigid@(p-reference_grip)+hand[0]
   root=Vector((hip.x,hip.y,0.))
   for ob in (body,gear):
    for v in ob.data.vertices:v.co=(v.co-root)*scale
   floor=min(v.co.z for v in body.data.vertices)
   for ob in (body,gear):
    for v in ob.data.vertices:v.co.z-=floor
    ob.data.update()
   # Keep the weapon out of the ground. The bound is checked on the actual
   # normalized source mesh, not inferred from a character capsule.
   assert abs(min(v.co.z for v in body.data.vertices))<1e-6
   bpy.ops.object.select_all(action='DESELECT')
   for ob in (body,gear):ob.select_set(True)
   bpy.context.view_layer.objects.active=body
   path=stage/f'castle_training_knight_drill_{frame:02}.glb'
   bpy.ops.export_scene.gltf(filepath=str(path),export_format='GLB',use_selection=True,export_yup=True,export_animations=False,export_skins=False,export_image_format='AUTO',export_cameras=False,export_lights=False,export_extras=False)
   bake_uv_transforms(path)
   doc,binary=glb(path);sampled=pose(path)
   signatures=[p['signature'] for p in sampled['records']]
   if base is None:base=signatures
   assert signatures==base,('Unsafe pose topology/material',frame)
   points=np.concatenate([p['world']+np.array([0,sampled['raw_min_y'],0]) for p in sampled['records']])
   bodypoints=sampled['records'][0]['world']+np.array([0,sampled['raw_min_y'],0])
   assert abs(bodypoints[:,1].min())<1e-5
   lo=points.min(axis=0).tolist();hi=points.max(axis=0).tolist();bounds.append((lo,hi))
   images=[]
   for im in doc['images']:
    v=doc['bufferViews'][im['bufferView']];off=v.get('byteOffset',0);images.append(hashlib.sha256(binary[off:off+v['byteLength']]).hexdigest())
   if texture_images is None:texture_images=sorted(images)
   assert sorted(images)==texture_images
   triangles=sum(p['signature']['indices']//3 for p in sampled['records'])
   records.append(dict(file=path.name,sha256=sha(path),bytes=path.stat().st_size,frame=frame,source_frame=source_frame,recovery_fraction=fraction,triangles=triangles,bounds_min=lo,bounds_max=hi))
   for ob in (body,gear):
    data=ob.data;bpy.data.objects.remove(ob,do_unlink=True);bpy.data.meshes.remove(data)
   print('TRAINING_POSE',frame,triangles,lo,hi,flush=True)
  resident=pose(OUT/'village_character_knight_idle_00.glb')
  assert [p['signature']['material'] for p in resident['records']]==[p['material'] for p in base], 'Training must reuse resident texture bytes/policies'
  assert len(texture_images)==6
  geometry_bytes=0
  for r in records:
   doc,binary=glb(stage/r['file']);views={im['bufferView'] for im in doc['images']}
   geometry_bytes+=sum(v['byteLength'] for i,v in enumerate(doc['bufferViews']) if i not in views)
  assert geometry_bytes<16*1048576
  manifest=dict(schema_version=1,title='West court poleaxe practice',source=str(SOURCE.relative_to(ROOT)),source_sha256=sha(SOURCE),source_url=SOURCE_URL,license='CC BY3.0',license_url=LICENSE_URL,credit=CREDIT,source_action=attack.name,source_frames=[SOURCE_START,SOURCE_END],source_fps=25,source_samples=SOURCE_SAMPLES,authored_recovery_samples=POSES-SOURCE_SAMPLES,cycle_seconds=CYCLE_SECONDS,pose_count=POSES,instances=7,trainees=6,instructors=1,fixed_reference_scale=scale,body_height_reference_m=BODY_HEIGHT,source_scripts_executed=False,source_file_modified=False,geometry_bytes=geometry_bytes,unique_images=6,new_image_texture_policies=0,image_sha256=texture_images,all_adjacent_pose_topology_material_exact=True,weapon_attachment='Authored rigid poleaxe retarget: principal shaft axis aligned to evaluated two-finger grip line; right grip translation preserved; no mocap wrist roll',root_motion_removed='Hips horizontal translation; body sole grounded after skinning',bounds_min=np.min([b[0] for b in bounds],axis=0).tolist(),bounds_max=np.max([b[1] for b in bounds],axis=0).tolist(),assets=records)
  for r in records:
   dst=output/r['file'];pending=dst.with_suffix('.glb.pending');pending.write_bytes((stage/r['file']).read_bytes());os.replace(pending,dst)
  dst=output/'castle_training_manifest.json';pending=dst.with_suffix('.json.pending');pending.write_text(json.dumps(manifest,indent=2)+'\n');os.replace(pending,dst)
 print('CASTLE_TRAINING_COOK',json.dumps({k:manifest[k] for k in ('pose_count','geometry_bytes','unique_images','new_image_texture_policies','bounds_min','bounds_max')}),flush=True)
 return manifest

if __name__=='__main__':
 argv=sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else []
 parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--output',type=Path,default=OUT);args=parser.parse_args(argv);cook(args.output)
