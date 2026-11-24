"""N-owned skeletal camper conversion; originals stay untouched.
Real supplied mesh/rig, authored continuous skeletal acting, not supplied mocap.
"""
from pathlib import Path
import importlib.util,json,math,hashlib,sys
import bpy
from mathutils import Matrix,Quaternion,Vector
N=Path(__file__).resolve().parents[1]; E=N.parents[1]
spec=importlib.util.spec_from_file_location('purchased',E/'tools/build_purchased_characters.py'); P=importlib.util.module_from_spec(spec);spec.loader.exec_module(P)
OUT=N/'presentation/camp_people';OUT.mkdir(parents=True,exist_ok=True)
provenance=json.loads((P.ASSETS/'PROVENANCE.json').read_text()); records=[]
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
for role,family in [('rowan',2),('bram',4),('ellis',1)]:
 bpy.ops.wm.read_factory_settings(use_empty=True)
 path=P.SOURCE/f'NPC_Character_{family:02}/Mesh_UE4/Full/SK_NPC_Character_{family:02}_Full_01.FBX'
 bpy.ops.import_scene.fbx(filepath=str(path),automatic_bone_orientation=True)
 originals=[o for o in bpy.context.scene.objects if o.type=='MESH']; arms=[o for o in bpy.context.scene.objects if o.type=='ARMATURE']
 skin=P.author_missing_skin(bpy,originals,family) if not arms else None
 arm=next(o for o in bpy.context.scene.objects if o.type=='ARMATURE');assert not bpy.data.actions
 textures=provenance['textures']; materials={'body':P.material(bpy,f'body{family:02}',textures[f'body{family:02}'],masked=family in [1,4]),'head':P.material(bpy,'head01' if family==1 else 'head',textures['head01' if family==1 else 'head'])}
 materials['skin']=P.material(bpy,f'CampHead{family:02}',textures['head01' if family==1 else 'head'])
 materials['skin'].name=f'CampHead{family:02}'
 if family!=4:materials['hair']=P.material(bpy,f'hair{family:02}',textures[f'hair{family:02}'],masked=True)
 def budget(name):
  if 'Head' in name:return 4400
  if 'Eyes' in name:return 300
  if 'Hair' in name:return 2900
  if 'Beard' in name or 'Mustache' in name:return 800
  if 'Hands' in name:return 1100
  if 'Shoes' in name:return 1750
  if 'Pants_Detail' in name:return 650
  if 'Pants' in name:return 1600
  if 'Bag' in name:return 450
  if 'Belt' in name:return 400
  if 'Cloth_02' in name:return 450
  if 'Cloth_Detail' in name:return 650
  if 'Cloth_01' in name:return 1900
  if 'Body_Cloth' in name:return 3200
  if 'Body_Detail' in name:return 650
  raise ValueError(name)
 for obj in originals:
  if obj.data.shape_keys:obj.shape_key_clear()
  name=obj.name; kind='hair' if any(s in name for s in ['Hair','Beard','Mustache']) else 'skin' if 'Head' in name else 'head' if any(s in name for s in ['Hands','Eyes']) else 'body'
  obj.data.materials.clear();obj.data.materials.append(materials[kind])
  for poly in obj.data.polygons:poly.material_index=0
  for a in list(obj.data.color_attributes):obj.data.color_attributes.remove(a)
  triangles=sum(len(p.vertices)-2 for p in obj.data.polygons)
  bpy.ops.object.select_all(action='DESELECT');obj.select_set(True);bpy.context.view_layer.objects.active=obj
  dec=obj.modifiers.new('Offline bounded character geometry','DECIMATE');dec.ratio=min(1.,budget(name)/triangles);dec.use_collapse_triangulate=True
  while obj.modifiers.find(dec.name)>0:bpy.ops.object.modifier_move_up(modifier=dec.name)
  bpy.ops.object.modifier_apply(modifier=dec.name)
 bones=arm.pose.bones;world=arm.matrix_world.copy();inverse=world.inverted()
 def update():bpy.context.view_layer.update()
 def head(name):return world@bones[name].head
 def aim(name,direction):
  b=bones[name];m=world@b.matrix;start=head(name);delta=(world@b.tail-start).normalized().rotation_difference(Vector(direction).normalized());loc,rot,scale=m.decompose();b.matrix=inverse@Matrix.LocRotScale(loc,delta@rot,scale);update()
 def rotate(name,axis,angle):
  m=world@bones[name].matrix;loc,rot,scale=m.decompose();bones[name].matrix=inverse@Matrix.LocRotScale(loc,Quaternion(Vector(axis),angle)@rot,scale);update()
 for b in bones:b.matrix_basis=Matrix.Identity(4);b.rotation_mode='QUATERNION'
 update()
 for side,sign in [('l',1),('r',-1)]:
  aim('upperarm_'+side,(sign*.13,-.045,-1));aim('lowerarm_'+side,(sign*.025,-.14,-1))
 # Loose, lightly curled fingers retain source finger bones and their skin.
 for b in bones:
  if any(b.name.startswith(p) for p in ['index_','middle_','ring_','pinky_']):b.rotation_quaternion=Quaternion((1,0,0),.12)@b.rotation_quaternion
 update();neutral={b.name:b.matrix_basis.copy() for b in bones}
 feet={name:(world@bones[name].matrix).to_quaternion() for name in ['foot_l','foot_r','ball_l','ball_r']};ankles={s:head('foot_'+s).copy() for s in ['l','r']};hip=head('pelvis').copy()
 graph=bpy.context.evaluated_depsgraph_get();points=[o.matrix_world@v.co for o in originals for v in o.evaluated_get(graph).data.vertices];low=min(v.z for v in points);high=max(v.z for v in points);scale=1.82/(high-low);origin=Vector((hip.x,hip.y,low))
 def leg(side,target):
  h=head('thigh_'+side);k=head('calf_'+side);a=(k-h).length;b=(head('foot_'+side)-k).length;delta=target-h;d=min(delta.length,a+b-.0001);direction=delta.normalized();along=(a*a-b*b+d*d)/(2*d);bend=Vector((0,-1,0));bend=(bend-direction*bend.dot(direction)).normalized();joint=h+direction*along+bend*math.sqrt(max(0,a*a-along*along));aim('thigh_'+side,joint-h);aim('calf_'+side,target-head('calf_'+side))
  for name in ['foot_'+side,'ball_'+side]:
   m=world@bones[name].matrix;loc,rot,s=m.decompose();bones[name].matrix=inverse@Matrix.LocRotScale(loc,feet[name],s);update()
 # One joined skinned mesh node is the native loader contract.
 bpy.ops.object.select_all(action='DESELECT')
 for o in originals:o.select_set(True)
 bpy.context.view_layer.objects.active=originals[0];bpy.ops.object.join();body=originals[0];body.name='Camper_'+role
 normalizer=bpy.data.objects.new('ValwoodMetres',None);bpy.context.collection.objects.link(normalizer);normalizer.scale=(scale,)*3;normalizer.location=-origin*scale
 old=arm.matrix_world.copy();arm.parent=normalizer;arm.matrix_basis=old
 # Author in source world coordinates; normalizer stays outside the armature.
 actions=[]; lengths={'Ambient':12.,'Listen':10.,'Talk':8.,'WarmHands':9.}
 scene=bpy.context.scene;scene.render.fps=24
 for ci,(name,duration) in enumerate(lengths.items()):
  action=bpy.data.actions.new(name);arm.animation_data_create();arm.animation_data.action=action;action.use_fake_user=True;actions.append(action)
  for frame in range(int(duration*24)+1):
   t=frame/24.;phase=2*math.pi*t/duration
   for b in bones:b.matrix_basis=neutral[b.name].copy()
   update()
   # Fixed support feet; pelvis settles and shifts, torso counterbalances.
   shift=.014*math.sin(phase);m=world@bones['pelvis'].matrix;m.translation.x+=shift;m.translation.z-=.012*(.5-.5*math.cos(phase));bones['pelvis'].matrix=inverse@m;update()
   rotate('spine_02',(1,0,0),.007*math.sin(phase*3));rotate('spine_02',(0,1,0),-.012*math.sin(phase))
   rotate('neck_01',(0,0,1),.025*math.sin(phase));rotate('head',(0,0,1),.05*math.sin(phase+.25)*math.sin(phase*.5)**2)
   if name=='Listen':rotate('head',(1,0,0),.035*math.sin(phase*2)*math.sin(phase*.5)**2)
   elif name=='Talk':
    # Open forearm gesture, followed by a quieter opposite-hand response.
    gesture=math.sin(phase*.5)**2;wave=gesture*(.6+.4*math.sin(phase*2))
    aim('upperarm_r',(-.19,-.12-.33*gesture,-1));aim('lowerarm_r',(-.04,-.14-.95*wave,-1+.55*gesture));rotate('hand_r',(0,0,1),-.23*gesture)
    rotate('head',(1,0,0),.04*math.sin(phase*3)*gesture);rotate('spine_03',(0,0,1),-.025*gesture)
   elif name=='WarmHands':
    envelope=math.sin(phase*.5)**2
    for side,sign in [('l',1),('r',-1)]:
     aim('upperarm_'+side,(sign*(.13-.06*envelope),-.045-.5*envelope,-1));aim('lowerarm_'+side,(sign*(.025+.04*envelope),-.14-2.*envelope,-1+.7*envelope));rotate('hand_'+side,(1,0,0),.20*envelope)
    rotate('spine_02',(1,0,0),.035*envelope)
   else:
    envelope=math.sin(phase*.5)**4
    rotate('head',(0,0,1),(.13 if role=='ellis' else -.09)*envelope)
    aim('upperarm_l',(.13,-.045-.08*envelope,-1));aim('lowerarm_l',(.025,-.14-.18*envelope,-1))
   for side in ['l','r']:leg(side,ankles[side])
   update()
   # Store dense true local rotations/translations, not baked vertex poses.
   for b in bones:
    b.keyframe_insert(data_path='rotation_quaternion',frame=frame);b.keyframe_insert(data_path='location',frame=frame)
  for fc in action.fcurves:
   for key in fc.keyframe_points:key.interpolation='LINEAR'
  arm.animation_data.action=None
  track=arm.animation_data.nla_tracks.new();track.name=name;strip=track.strips.new(name,0,action);strip.name=name
 scene.frame_start=0;scene.frame_end=288;scene.frame_set(0)
 # Recompute helper world after normalization only for exporter; animation
 # channels retain exact source local-space transforms and inverse binds.
 bpy.ops.object.select_all(action='DESELECT');arm.select_set(True);body.select_set(True);normalizer.select_set(True);bpy.context.view_layer.objects.active=arm
 target=OUT/f'{role}.glb'
 bpy.ops.export_scene.gltf(filepath=str(target),export_format='GLB',use_selection=True,export_yup=True,export_animations=True,export_nla_strips=True,export_frame_range=False,export_force_sampling=True,export_skins=True,export_all_influences=False,export_def_bones=False,export_image_format='AUTO',export_cameras=False,export_lights=False)
 records.append({'role':role,'file':target.name,'sha256':sha(target),'bytes':target.stat().st_size,'source':str(path),'source_sha256':sha(path),'source_family':family,'skin_transfer':skin,'clips':lengths,'reference_height_m':1.82,'method':'Continuous authored skeletal breathing, planted-foot weight shift, looking, listening/nodding, talk gesture and warming hands; 24Hz local joint keys. Not supplied mocap. Source mesh/UV/material atlases preserved; bounded offline geometry matching prior camper budget.'})
 print('CAMP_SKELETAL',json.dumps(records[-1]),flush=True)
(OUT/'manifest.json').write_text(json.dumps({'schema':1,'assets':records,'license':P.PROPRIETARY,'originals_unchanged':True},indent=2)+'\n')
