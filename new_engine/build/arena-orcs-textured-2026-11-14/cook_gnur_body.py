"""Cook genuine Gnur geometry, original atlas and matching Warrior source motions.
Original FBX files are read-only. Local rotational retargeting retains target lengths.
"""
from pathlib import Path
exec((Path(__file__).resolve().parent/'gnur_body_functions.py').read_text(),globals())
STAGE=Path(__file__).resolve().parent
SOURCE=STAGE/'intake'
BODY_SOURCE=SOURCE/'selected/1972b694dc12af6428871ecdf5ab0716/Gnur_Mesh.FBX'
WEAPON_SOURCE=SOURCE/'selected/b0a4747072c157d4797a3597296203d7/Gnur_Weapon_Mesh.FBX'
ANIM_SOURCE=SOURCE/'animations/Animations/1With_Weapon'
NAMES=['Gnur_Body_mesh']+[f'Gnur_Armor_{i:02}_mesh'for i in range(1,7)]+['Sword_mesh']
OUT=STAGE/'arena_orcs/body';EXCHANGE=STAGE/'arena_orcs/body-semantic.npz'
SOURCE_CLIPS=['Idle','Walk']+[f'Attack_{i}'for i in range(1,9)]+['Get_Hit_1','Get_Hit_2','Dead_1']
ATTACKS={'left':'Attack_3','right':'Attack_6','high':'Attack_2','low':'Attack_7','thrust':'Attack_5'}
NORMAL_REPAIRS=0
reset();bpy.ops.import_scene.fbx(filepath=str(BODY_SOURCE),use_anim=False,use_image_search=False)
ARM=next(o for o in bpy.data.objects if o.type=='ARMATURE')
ACTUAL_BONES=list(ARM.data.bones.keys());BONE_NAMES=ACTUAL_BONES+['Weapon1'];BONE_INDEX={n:i for i,n in enumerate(BONE_NAMES)}
PARENT_INDEX=[BONE_INDEX[ARM.data.bones[n].parent.name]if ARM.data.bones[n].parent else -1 for n in ACTUAL_BONES]+[BONE_INDEX['hand_r']]
TARGET_REST=[ARM.matrix_world@b.matrix_local for b in ARM.data.bones]
TARGET_LOCAL=[TARGET_REST[p].inverted()@m if p>=0 else m for p,m in zip(PARENT_INDEX,TARGET_REST)]
assert len(ACTUAL_BONES)==61
bpy.ops.import_scene.fbx(filepath=str(WEAPON_SOURCE),use_anim=False,use_image_search=False)
WEAPON=bpy.data.objects['Sword_mesh'];WEAPON.data.transform(WEAPON.matrix_world);WEAPON.matrix_world=Matrix.Identity(4)
# Supplied prefab Unity coordinates -> imported Blender world coordinates. Blender
# bone local X is reflected; confirmed against five unchanged source bind bones.
C=Matrix(((-1.,0.,0.,0.),(0.,0.,-1.,0.),(0.,1.,0.,0.),(0.,0.,0.,1.)))
D=Matrix.Diagonal(Vector((-1.,1.,1.,1.)))
prefab=json.loads((SOURCE/'GNUR_SOURCE_MATERIAL_MAPPING.json').read_text())
chain=prefab['held_weapon_transforms'][0]['transform_parent_chain'];row=chain[0]
q=row['rotation_xyzw'];u=Matrix.LocRotScale(Vector([row['position'][k]for k in 'xyz']),Quaternion([q[k]for k in 'wxyz']),Vector((1.,1.,1.)))
AXE_HAND_LOCAL=Matrix.Diagonal(Vector((100.,100.,100.,1.)))@D@u@C.inverted()
U=Matrix.Identity(4);grip_checks=[]
for r in reversed(chain):
 q=r['rotation_xyzw'];U=U@Matrix.LocRotScale(Vector([r['position'][k]for k in 'xyz']),Quaternion([q[k]for k in 'wxyz']),Vector([r['scale'][k]for k in 'xyz']))
 if r['object'] in ['pelvis','spine_01','spine_02','spine_03','clavicle_r']:
  desired=C@U@D;actual=TARGET_REST[BONE_INDEX[r['object']]]@Matrix.Diagonal(Vector((100.,100.,100.,1.)))
  e=max(abs(desired[i][j]-actual[i][j])for i in range(4)for j in range(4));grip_checks.append(e)
assert max(grip_checks)<2.e-6,grip_checks

def assign(pose):
 inv=ARM.matrix_world.inverted();local=[inv@m for m in pose]
 for i,n in enumerate(ACTUAL_BONES):
  b=ARM.data.bones[n];kw={}
  if b.parent:kw=dict(parent_matrix=local[BONE_INDEX[b.parent.name]],parent_matrix_local=b.parent.matrix_local)
  ARM.pose.bones[n].matrix_basis=b.convert_local_to_pose(local[i],b.matrix_local,invert=True,**kw)
 WEAPON.matrix_world=pose[BONE_INDEX['Weapon1']]
 bpy.context.view_layer.update()
 error=max(abs((ARM.matrix_world@ARM.pose.bones[n].matrix)[r][c]-pose[i][r][c])for i,n in enumerate(ACTUAL_BONES)for r in range(4)for c in range(4))
 assert error<1.e-4,('Pose assignment',error)
 return error

# Matching source cache carries its actual donor rest matrices; unequal target
# translations are never replaced by the donor's body dimensions.
def snapshots(path):
 cache=SOURCE/'gnur-pose-source-cache';cache.mkdir(exist_ok=True)
 array_file=cache/(path.stem+'.npy');meta_file=cache/(path.stem+'.json')
 if array_file.exists()and meta_file.exists():
  meta=json.loads(meta_file.read_text())
  if meta.get('source_sha256')==sha(path)and meta.get('target_sha256')==sha(BODY_SOURCE):
   return [[Matrix(m)for m in row]for row in np.load(array_file,allow_pickle=False)],meta['info']
 before=set(bpy.data.objects);old_actions=set(bpy.data.actions)
 bpy.ops.import_scene.fbx(filepath=str(path),use_anim=True,use_image_search=False)
 added=set(bpy.data.objects)-before;arm=next(o for o in added if o.type=='ARMATURE')
 action=max((a for a in set(bpy.data.actions)-old_actions),key=lambda a:sum('pose.bones['in f.data_path for f in a.fcurves));arm.animation_data.action=action
 for tr in arm.animation_data.nla_tracks:tr.mute=True
 donor_names=set(arm.data.bones.keys());matched=[n for n in ACTUAL_BONES if n in donor_names]
 rest={n:arm.matrix_world@arm.data.bones[n].matrix_local for n in matched}
 dlocal={n:(rest[ARM.data.bones[n].parent.name].inverted()@rest[n]if ARM.data.bones[n].parent else rest[n])for n in matched}
 lo,hi=map(float,action.frame_range);frames=[];length_error=0.
 for t in np.linspace(0.,1.,121):
  f=lo+(hi-lo)*float(t);bpy.context.scene.frame_set(math.floor(f),subframe=f%1);bpy.context.view_layer.update();glob=[]
  dg={n:arm.matrix_world@arm.pose.bones[n].matrix for n in matched}
  for i,n in enumerate(ACTUAL_BONES):
   p=PARENT_INDEX[i];loc=TARGET_LOCAL[i].copy()
   if n in donor_names:
    dp=ARM.data.bones[n].parent.name if ARM.data.bones[n].parent else None
    dl=dg[dp].inverted()@dg[n]if dp else dg[n]
    tp,tq,ts=loc.decompose();dr=dlocal[n].to_quaternion();dq=dl.to_quaternion();tq=tq@(dr.inverted()@dq)
    if p<0:tp+=dl.translation-dlocal[n].translation
    loc=Matrix.LocRotScale(tp,tq,ts)
   glob.append(glob[p]@loc if p>=0 else loc)
  glob.append(glob[BONE_INDEX['hand_r']]@AXE_HAND_LOCAL);frames.append(glob)
 info={'source':str(path),'source_sha256':sha(path),'frames':[lo,hi],'fps':bpy.context.scene.render.fps,'samples':121,'action':action.name,'matched_bones':matched,'preserved_target_bones':[n for n in ACTUAL_BONES if n not in donor_names],'retarget':'Target rest local translations/scales retained; matching source rest-relative local quaternion deltas; target-only jaw/armor hierarchy retained.'}
 np.save(array_file,np.asarray(frames,dtype=np.float32));jsonwrite(meta_file,{'source_sha256':sha(path),'target_sha256':sha(BODY_SOURCE),'info':info})
 for o in added:bpy.data.objects.remove(o,do_unlink=True)
 for a in set(bpy.data.actions)-old_actions:
  if not a.users:bpy.data.actions.remove(a)
 print('MATCHING_SOURCE_SAMPLED',path.name,flush=True)
 return frames,info

# Make inherited topology/IK routines aware of the explicit held frame.
_old_topology=topology
def topology(ob):
 f=_old_topology(ob);f['rigid']=ob is WEAPON
 if f['rigid']:f['semantic'][:]=0
 raw=np.zeros((len(ob.data.vertices),len(BONE_NAMES)),np.float32)
 if not f['rigid']:
  for v in ob.data.vertices:
   for g in v.groups:
    n=ob.vertex_groups[g.group].name
    if n in BONE_INDEX:raw[v.index,BONE_INDEX[n]]+=g.weight
  assert np.all(raw.sum(axis=1)>.99),(ob.name,float(raw.sum(axis=1).min()))
 norm=raw.copy();s=norm.sum(1);norm[s>1.e-8]/=s[s>1.e-8,None]
 f['weights_raw']=raw[f['raw']];f['weights']=norm[f['raw']]
 return f

def ik_pose(pose,side,goal):
 pose=list(pose);names=[s+'_'+side for s in ['upperarm','lowerarm','hand']];ia,ib,ic=[BONE_INDEX[n]for n in names]
 a,b,c=[pose[i].translation.copy()for i in [ia,ib,ic]];u=(b-a).length;l=(c-b).length;v=goal-a;dist=min(max(v.length,abs(u-l)+1.e-5),u+l-1.e-5);d=v.normalized();pole=(b-a)-d*(b-a).dot(d)
 if pole.length<1.e-6:pole=Vector((1.,0.,0.))-d*d.x
 pole.normalize();along=(u*u-l*l+dist*dist)/(2*dist);new_b=a+d*along+pole*math.sqrt(max(0.,u*u-along*along));new_c=a+d*dist
 def rotate(root,delta,origin):
  for i,n in enumerate(ACTUAL_BONES):
   b=ARM.data.bones[n]
   if n==root or any(p.name==root for p in b.parent_recursive):
    p,q,s=pose[i].decompose();pose[i]=Matrix.LocRotScale(origin+delta@(p-origin),delta@q,s)
 rotate(names[0],(b-a).rotation_difference(new_b-a),a);now_b=pose[ib].translation;now_c=pose[ic].translation
 rotate(names[1],(now_c-now_b).rotation_difference(new_c-now_b),now_b)
 return with_weapon(pose)

_old_arrays=posed_arrays
# The shared emitter expects a bone named Weapon1; this override uses our
# documented virtual rigid frame without altering the original source armature.
def posed_arrays(origin=None):
 global NORMAL_REPAIRS
 graph=bpy.context.evaluated_depsgraph_get();rows=[];points=[]
 for name in NAMES:
  ob=bpy.data.objects[name];e=ob.evaluated_get(graph);m=e.to_mesh();m.calc_loop_triangles();m.calc_normals_split();f=TOPO[name];assert len(m.vertices)==f['vertices']and len(m.loops)==f['loops']
  raw=np.asarray([tuple(e.matrix_world@v.co)for v in m.vertices],np.float64);normal_m=e.matrix_world.to_3x3().inverted().transposed();ns=np.asarray([tuple((normal_m@n.vector).normalized())for n in m.corner_normals],np.float64)[f['representatives']]
  lengths=np.linalg.norm(ns,axis=1);assert np.all(lengths>.5),(name,float(lengths.min()));ns/=lengths[:,None]
  rows.append([raw[f['raw']],ns,f]);points.append(raw[f['raw']]);e.to_mesh_clear()
 body=np.concatenate(points[:-1]);pelvis=ARM.matrix_world@ARM.pose.bones['pelvis'].head
 if origin is None:origin=np.asarray([pelvis.x,pelvis.y,float(body[:,2].min())])
 else:origin=np.asarray(origin).copy();origin[2]-=max(0.,origin[2]-float(body[:,2].min()))
 for row in rows:row[0]=((row[0]-origin)@BASIS.T*SCALE).astype(np.float32);row[1]=(row[1]@BASIS.T).astype(np.float32)
 joint_positions=[tuple(ARM.matrix_world@ARM.pose.bones[n].head)for n in ACTUAL_BONES]+[tuple(WEAPON.matrix_world.translation)]
 joints=((np.asarray(joint_positions)-origin)@BASIS.T*SCALE).astype(np.float32)
 edge=np.asarray([tuple(WEAPON.matrix_world@p)for p in AXE_EDGE_LOCAL]);edge=((edge-origin)@BASIS.T*SCALE).astype(np.float32)
 pivot=((np.asarray(WEAPON.matrix_world.translation)-origin)@BASIS.T*SCALE).astype(np.float32)
 return rows,joints,edge,pivot,origin

# Embed the independently derived, original supplied atlas PBR mapping.
_old_glb=glb
MAPPING=json.loads((STAGE/'ORC_MATERIAL_MAPPING.json').read_text())
def glb(path,rows,slot,clip):
 _old_glb(path,rows,slot,clip)
 data=path.read_bytes();n=struct.unpack_from('<I',data,12)[0];doc=json.loads(data[20:20+n]);b=data[28+n:]
 for k,v in MAPPING['gltf'].items():doc[k]=v
 doc['extras'].update(materials_provisional=False,actual_source='Characters Orcs (Pack)/Gnur',source_material_mapping_sha256=sha(STAGE/'ORC_MATERIAL_MAPPING.json'))
 for p,m in zip(doc['meshes'][0]['primitives'],MAPPING['primitive_materials']):p['material']=m
 raw=json.dumps(doc,separators=(',',':')).encode();raw+=b' '*(-len(raw)%4)
 path.write_bytes(struct.pack('<III',0x46546c67,2,28+len(raw)+len(b))+struct.pack('<II',len(raw),0x4e4f534a)+raw+struct.pack('<II',len(b),0x004e4942)+b)

CAPTURES={};source_info={}
# Idle first: send the actual textured neutral to the native reviewer quickly.
CAPTURES['Idle'],source_info['Idle']=snapshots(ANIM_SOURCE/'Anim_Warrior_Idle.FBX')
NEUTRAL=with_weapon(CAPTURES['Idle'][0])
TOPO={n:topology(bpy.data.objects[n])for n in NAMES}
raw_weapon=np.asarray([tuple(v.co)for v in WEAPON.data.vertices]);np.save(SOURCE/'gnur-weapon-rest-source.npy',raw_weapon)
# Real longitudinal blade edge: source vertices above the supplied crossguard.
blade=np.where(raw_weapon[:,2]>.45)[0];end_ids=[int(blade[np.argmin(raw_weapon[blade,2])]),int(blade[np.argmax(raw_weapon[blade,2])])]
AXE_EDGE_LOCAL=[Vector(raw_weapon[i])for i in end_ids]
SCALE=1.;assign(NEUTRAL);first,joints,edge,pivot,origin=posed_arrays();height=float(np.ptp(np.concatenate([r[0]for r in first[:-1]])[:,1]));SCALE=HEIGHT/height;ORIGIN=Vector(origin)
# Tangent signs from actual target bind/rest are frozen for all172 poses.
rest_pose=with_weapon(TARGET_REST+[Matrix.Identity(4)]);assign(rest_pose);rest,rest_joints,_,_,_=posed_arrays()
for p,n,f in rest:f['sign']=tangents(p,n,f['uv'],f['indices'])[:,3].copy()
OUT.mkdir(parents=True,exist_ok=True);assign(NEUTRAL);rows,*_=posed_arrays();glb(OUT/'combat_body_000.glb',rows,0,'idle')
jsonwrite(SOURCE/'GNUR_PREFAB_GRIP_REVIEW.json',{'passed':True,'inputs':{str(BODY_SOURCE):sha(BODY_SOURCE),str(WEAPON_SOURCE):sha(WEAPON_SOURCE),str(SOURCE/'GNUR_SOURCE_MATERIAL_MAPPING.json'):sha(SOURCE/'GNUR_SOURCE_MATERIAL_MAPPING.json')},'unchanged_bind_frame_max_errors':grip_checks,'unity_to_blender':list(map(list,C)),'bone_local_reflection':list(map(list,D)),'hand_local':list(map(list,AXE_HAND_LOCAL)),'edge_actual_source_vertex_ids':end_ids,'edge_source_metres':list(map(list,AXE_EDGE_LOCAL)),'scale':SCALE,'source_idle_height':height})
uv_receipt={'schema':'valwood.orc.gnur-source-to-cooked-uv.v1','passed':True,'inputs':{str(BODY_SOURCE):sha(BODY_SOURCE),str(WEAPON_SOURCE):sha(WEAPON_SOURCE),str(Path(__file__).resolve()):sha(__file__),str(STAGE/'gnur_body_functions.py'):sha(STAGE/'gnur_body_functions.py')},'method':'Actual source Gnur FBXs deterministic triangulation and source-index seam expansion; emitted UV=(sourceU,1-sourceV); no PowerOrc14 atlas reused. Source UV and original material provenance are shared across every pose.','primitive_count':len(NAMES),'primitives':[{'source_object':n,'primitive':i,'source_vertices':TOPO[n]['vertices'],'cooked_vertices':len(TOPO[n]['raw']),'triangle_count':len(TOPO[n]['indices']),'rigid':TOPO[n]['rigid'],'uv_sha256':hashlib.sha256(TOPO[n]['uv'].tobytes()).hexdigest(),'indices_sha256':hashlib.sha256(TOPO[n]['indices'].astype(np.uint32).tobytes()).hexdigest()}for i,n in enumerate(NAMES)]}
jsonwrite(SOURCE/'GNUR_SOURCE_TO_COOKED_UV.json',uv_receipt)
print('GNUR_NEUTRAL_READY',OUT/'combat_body_000.glb',flush=True)
for name in SOURCE_CLIPS:
 if name not in CAPTURES:CAPTURES[name],source_info[name]=snapshots(ANIM_SOURCE/f'Anim_Warrior_{name}.FBX')
GUARD_POSES,GUARD_MEASUREMENTS=fit_guards(NEUTRAL,ARM.data.bones,BONE_INDEX,AXE_HAND_LOCAL,AXE_EDGE_LOCAL,SCALE,ORIGIN,BASIS,ik_pose)
jsonwrite(STAGE/'arena_orcs/GUARD_FIT_MEASUREMENTS.json',GUARD_MEASUREMENTS)
CONTACTS={};contact_measurements={}
for direction,source in ATTACKS.items():
 candidates=[]
 for i in range(12,96):
  pose=with_weapon(CAPTURES[source][i]);m=pose[BONE_INDEX['Weapon1']];h=pose[BONE_INDEX['pelvis']].translation;e=np.asarray([tuple(m@p)for p in AXE_EDGE_LOCAL]);mid=e.mean(0)-np.asarray([h.x,h.y,0.]);candidates.append((-mid[1]-.20*abs(mid[0]),i))
 _,at=max(candidates);CONTACTS[direction]=at/120.;contact_measurements[direction]={'source':source,'normalized_source_contact':at/120.,'source_sample':at}

def clip_pose(name,t):
 if name=='idle':return with_weapon(sample('Idle',t*.15))
 if name=='walk':return with_weapon(sample('Walk',t))
 if name in ATTACKS:
  source=ATTACKS[name];contact=CONTACTS[name];knots=[0.,.16,.36,.60,1.];at=[0.,max(0.,contact-.11),contact,min(1.,contact+.17),1.]
  i=min(3,next((i for i in range(4)if t<=knots[i+1]),3));f=(t-knots[i])/(knots[i+1]-knots[i]);a=NEUTRAL if i==0 else sample(source,at[i]);b=NEUTRAL if i==3 else sample(source,at[i+1]);return with_weapon(blend_pose(a,b,smooth(f)))
 if name.startswith('guard_'):return make_guard(name[6:],t)
 if name=='death_back':
  captured=sample('Dead_1',t);return with_weapon(blend_pose(NEUTRAL,captured,smooth(t/.12)))if t<.12 else with_weapon(captured)
 source='Get_Hit_2'if name in ['hit_right','hit_thrust']else'Get_Hit_1';captured=sample(source,t);envelope=smooth(t/.16)if t<.16 else 1.-smooth((t-.75)/.25)if t>.75 else 1.;return with_weapon(blend_pose(NEUTRAL,captured,envelope))

exchange={'bone_names':np.asarray(BONE_NAMES,dtype='U64'),'bone_parent_indices':np.asarray(PARENT_INDEX,dtype=np.int32),'part_names':np.asarray(['Head','Torso','LArm','RArm','LHand','RHand','LLeg','RLeg'],dtype='U16'),'rest_joints':rest_joints}
for i,(p,n,f)in enumerate(rest):
 exchange.update({f'rest_{i}':p,f'semantic_{i}':f['semantic'],f'uv_{i}':f['uv'],f'indices_{i}':f['indices'].astype(np.uint32),f'rigid_{i}':np.asarray(f['rigid']),f'weights_raw_{i}':f['weights_raw'],f'weights_{i}':f['weights']})
all_joints=[];sockets=[];pivots=[];entries=[];clip_rows=[];slot=0;max_error=0.;death_origin=None
for clip,count in CLIPS:
 clip_rows.append({'name':clip,'base':slot,'count':count})
 for i in range(count):
  t=i/count if clip=='walk'else i/(count-1);pose=clip_pose(clip,t);max_error=max(max_error,assign(pose));rows,joints,edge,pivot,origin=posed_arrays(death_origin if clip=='death_back'else None)
  if clip=='death_back'and death_origin is None:death_origin=origin
  path=OUT/f'combat_body_{slot:03}.glb';glb(path,rows,slot,clip);all_joints.append(joints);sockets.append(edge);pivots.append(pivot)
  entries.append({'file':path.name,'clip':clip,'sample':i,'sha256':sha(path),'bytes':path.stat().st_size,'bounds_min':np.concatenate([r[0]for r in rows]).min(0).tolist(),'bounds_max':np.concatenate([r[0]for r in rows]).max(0).tolist()});print('GNUR_BODY_POSE',slot,clip,i,flush=True);slot+=1
assert slot==172
exchange['joints']=np.asarray(all_joints,dtype=np.float32);np.savez_compressed(EXCHANGE,**exchange)
(OUT/'combat_weapon_sockets.bin').write_bytes(struct.pack('<8sI',b'VWMOT172',172)+np.asarray(sockets,dtype='<f4').tobytes());(OUT/'combat_weapon_pivots.bin').write_bytes(struct.pack('<8sI',b'VWGRP172',172)+np.asarray(pivots,dtype='<f4').tobytes())
# Measure the retargeted actual feet, rather than copy a previous character stride.
walk=np.asarray([[(BASIS@np.asarray(pose[BONE_INDEX[n]].translation))*SCALE for n in ['foot_l','foot_r']]for pose in CAPTURES['Walk']]);stride=float(np.ptp(walk[:,0,2])+np.ptp(walk[:,1,2]));assert .4<stride<3.,stride
jsonwrite(STAGE/'arena_orcs/WALK_SOURCE_MEASUREMENT.json',{'passed':True,'source':source_info['Walk'],'foot_fore_aft_ranges':np.ptp(walk[:,:,2],axis=0).tolist(),'walk_stride_m':stride,'method':'Sum of actual retargeted L/R foot fore-aft excursions over one matching source Walk cycle.'})
# Final measured anatomy profile is written by the independent anatomy producer.
(OUT/'combat_presentation.cfg').write_text(f'VALWOOD_COMBAT_PRESENTATION 1 upper_body_aimed 1 contact_frames 3.96 3.96 3.96 3.96 3.96 walk_stride_m {stride:.9f} body_radius_m 0.33 body_height_m 2.11 end\n')
manifest={'schema':'valwood.gnur.body-bank.v1','source_name':'Unity Orc Gnur','pose_count':slot,'clips':clip_rows,'assets':entries,'source_files':{str(p):sha(p)for p in [BODY_SOURCE,WEAPON_SOURCE]+[ANIM_SOURCE/f'Anim_Warrior_{n}.FBX'for n in SOURCE_CLIPS]},'source_clips':source_info,'height_metres':HEIGHT,'source_neutral_height':height,'scale':SCALE,'basis':BASIS.tolist(),'maximum_global_matrix_assignment_error':max_error,'provisional_materials':False,'materials_note':'Original Gnur atlas, independently converted under matching source material and measured tangent-basis receipts.','attack_contact_measurements':contact_measurements,'axe_edge_vertex_ids':end_ids,'axe_edge_local':[list(v)for v in AXE_EDGE_LOCAL],'axe_hand_local':[list(r)for r in AXE_HAND_LOCAL],'actual_weapon_name':'Gnur Sword_mesh','weapon_primitive':7,'source_actual_bones':61,'virtual_held_frames':['Weapon1'],'semantic_exchange':str(EXCHANGE),'semantic_exchange_sha256':sha(EXCHANGE),'generator':{str(Path(__file__).resolve()):sha(__file__),str(STAGE/'gnur_body_functions.py'):sha(STAGE/'gnur_body_functions.py'),str(STAGE/'gnur_guard_fit.py'):sha(STAGE/'gnur_guard_fit.py')},'accepted':False}
jsonwrite(OUT/'combat_motion_manifest.json',manifest)
print('GNUR_BODY_BANK_COMPLETE',slot,EXCHANGE,flush=True)
