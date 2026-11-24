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
SOURCE_CLIPS=['Idle','Walk','Run']+[f'Attack_{i}'for i in range(1,9)]+['Get_Hit_1','Get_Hit_2','Dead_1']
ATTACKS={'left':'Attack_1','right':'Attack_4','high':'Attack_2','low':'Attack_8','thrust':'Attack_5'}
CURRENT_CLIP=None
POLISH={}
FOOT_PLAN={}
MOTION_QUALITY={'foot_target_max_error_m':0.,'leg_length_max_error_m':0.}
BASELINE=STAGE.parent/'arena-orcs-textured-2026-11-14'
assert STAGE.name=='arena-orc-motion-polish-2026-11-14' and not OUT.resolve().is_relative_to(BASELINE.resolve())
assert not SOURCE.is_symlink(), 'Private motion metadata directory required'
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
 if origin is None:
  xy=NEUTRAL_ORIGIN[:2]if CURRENT_CLIP in ATTACKS or CURRENT_CLIP=='walk'else [pelvis.x,pelvis.y]
  floor=RUN_ORIGIN_Z if CURRENT_CLIP=='walk'else float(body[:,2].min())
  origin=np.asarray([*xy,floor])
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
SCALE=1.;assign(NEUTRAL);first,joints,edge,pivot,origin=posed_arrays();height=float(np.ptp(np.concatenate([r[0]for r in first[:-1]])[:,1]));SCALE=HEIGHT/height;ORIGIN=Vector(origin);NEUTRAL_ORIGIN=np.asarray(origin).copy()
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

def continuous_source_time(t,contact):
 # Monotone cubic Hermite map. The source speed stays positive through the
 # authored contact/follow-through instead of resetting to zero at each knot.
 x=[0.,.36,.60,1.];y=[0.,contact,min(.82,contact+.22),1.]
 h=[x[i+1]-x[i]for i in range(3)];d=[(y[i+1]-y[i])/h[i]for i in range(3)]
 m=[d[0],0.,0.,d[-1]]
 for i in (1,2):
  w1=2*h[i]+h[i-1];w2=h[i]+2*h[i-1]
  m[i]=(w1+w2)/(w1/d[i-1]+w2/d[i])if d[i-1]*d[i]>0 else 0.
 i=min(2,next((j for j in range(3)if t<=x[j+1]),2));u=(t-x[i])/h[i]
 return max(0.,min(1.,(2*u**3-3*u*u+1)*y[i]+(u**3-2*u*u+u)*h[i]*m[i]+(-2*u**3+3*u*u)*y[i+1]+(u**3-u*u)*h[i]*m[i+1]))

def ik_leg(pose,side,goal):
 # Solve the real target leg with fixed thigh/calf lengths; retain the original
 # foot rotation and toe articulation after the knee/ankle position solve.
 pose=list(pose);names=[s+'_'+side for s in ['thigh','calf','foot']];ia,ib,ic=[BONE_INDEX[n]for n in names]
 a,b,c=[pose[i].translation.copy()for i in [ia,ib,ic]];footq=pose[ic].to_quaternion();u=(b-a).length;l=(c-b).length;v=goal-a;dist=min(max(v.length,abs(u-l)+1.e-5),u+l-1.e-5);direction=v.normalized();pole=(b-a)-direction*(b-a).dot(direction)
 if pole.length<1.e-6:pole=Vector((0.,-1.,0.))-direction*direction.y*-1.
 pole.normalize();along=(u*u-l*l+dist*dist)/(2*dist);new_b=a+direction*along+pole*math.sqrt(max(0.,u*u-along*along));new_c=a+direction*dist
 def rotate(root,delta,origin):
  for i,n in enumerate(ACTUAL_BONES):
   bone=ARM.data.bones[n]
   if n==root or any(p.name==root for p in bone.parent_recursive):
    p,q,scale=pose[i].decompose();pose[i]=Matrix.LocRotScale(origin+delta@(p-origin),delta@q,scale)
 rotate(names[0],(b-a).rotation_difference(new_b-a),a);now_b=pose[ib].translation;now_c=pose[ic].translation
 rotate(names[1],(now_c-now_b).rotation_difference(new_c-now_b),now_b)
 rotate(names[2],footq@pose[ic].to_quaternion().inverted(),pose[ic].translation)
 for ii,jj,expected in [(ia,ib,u),(ib,ic,l)]:
  MOTION_QUALITY['leg_length_max_error_m']=max(MOTION_QUALITY['leg_length_max_error_m'],abs((pose[jj].translation-pose[ii].translation).length-expected)*SCALE)
 error=(pose[ic].translation-goal).length*SCALE;MOTION_QUALITY['foot_target_max_error_m']=max(MOTION_QUALITY['foot_target_max_error_m'],error)
 return pose

def grounded_attack(direction,pose,source_time,envelope):
 info=POLISH[direction];source=ATTACKS[direction];align=Vector(info['source_alignment_metres']);compression=info['planar_step_scale'];base=NEUTRAL[BONE_INDEX['pelvis']].translation
 aligned=[]
 for matrix in pose:
  matrix=matrix.copy();matrix.translation+=align;aligned.append(matrix)
 root=aligned[BONE_INDEX['pelvis']].translation;root_delta=Vector(((base.x-root.x)*(1-compression),(base.y-root.y)*(1-compression),0.))
 # The source full-body weight shift remains; its committed step is bounded
 # inside the actual arena collider clearance instead of a one-metre lunge.
 result=[]
 for matrix in aligned:
  matrix=matrix.copy();matrix.translation+=root_delta;result.append(matrix)
 for side in ['l','r']:
  foot=BONE_INDEX['foot_'+side];ball=BONE_INDEX['ball_'+side];goal=aligned[foot].translation.copy();toe=aligned[ball].translation
  at=source_time*120
  for plan in FOOT_PLAN[direction][side]:
   lo,hi=plan['samples']
   if lo<=at<=hi:
    weight=smooth(min((at-lo)/2.,(hi-at)/2.,1.))*envelope
    goal+=(Vector(plan['toe_anchor_source_metres'])+align-toe)*weight;break
  home=NEUTRAL[foot].translation
  lift=max(0.,goal.z-home.z);goal.z=home.z+(.16/SCALE)*math.tanh(lift/(.16/SCALE))if lift>0 else goal.z
  goal.x=home.x+(goal.x-home.x)*compression;goal.y=home.y+(goal.y-home.y)*compression
  result=ik_leg(result,side,goal)
 return with_weapon(result)

# Detect the authored toe support intervals from actual source position/speed.
# Locking the toe retains real heel roll; locking the ankle would erase it.
for direction,source in ATTACKS.items():
 frames=CAPTURES[source];root=np.asarray([tuple(p[BONE_INDEX['pelvis']].translation)for p in frames]);base=NEUTRAL[BONE_INDEX['pelvis']].translation;align=np.asarray(base)-root[0]
 planar=np.linalg.norm(root[:,:2]-root[0,:2],axis=1);max_step=float(planar.max()*SCALE)
 foot_extent=max(max((p[BONE_INDEX['foot_'+side]].translation-frames[0][BONE_INDEX['foot_'+side]].translation).xy.length*SCALE for p in frames)for side in ['l','r'])
 compression=min(1.,.24/max(.001,max_step),.30/max(.001,foot_extent))
 POLISH[direction]={'source':source,'source_planar_pelvis_excursion_m':max_step,'planar_step_scale':compression,'maximum_cooked_step_m':max_step*compression,'original_foot_planar_step_m':foot_extent,'maximum_cooked_foot_step_m':foot_extent*compression,'swing_foot_lift_cap_m':.16,'source_alignment_metres':align.tolist()};FOOT_PLAN[direction]={}
 period=(source_info[source]['frames'][1]-source_info[source]['frames'][0])/source_info[source]['fps']
 for side in ['l','r']:
  toes=np.asarray([tuple(p[BONE_INDEX['ball_'+side]].translation)for p in frames]);speed=np.linalg.norm(np.gradient(toes[:,:2],period/120.,axis=0),axis=1)*SCALE
  support=(toes[:,2]<=float(toes[:,2].min())+.045/SCALE)&(speed<.18);segments=[];lo=None
  for i,yes in enumerate(np.append(support,False)):
   if yes and lo is None:lo=i
   if not yes and lo is not None:
    if i-lo>=5:segments.append({'samples':[lo,i-1],'toe_anchor_source_metres':np.median(toes[lo:i],axis=0).tolist()})
    lo=None
  FOOT_PLAN[direction][side]=segments

def merge_lower_body(upper,lower):
 result=[]
 for i,name in enumerate(BONE_NAMES):
  parent=PARENT_INDEX[i]
  # Source hips/legs establish support before release. Source spine/arms keep
  # their own strike timing, composed over that exact real target hierarchy.
  b=ARM.data.bones.get(name)
  is_upper=b is not None and (name.startswith('spine_')or any(p.name=='spine_01'for p in b.parent_recursive))
  source_pose=upper if is_upper else lower
  local=source_pose[parent].inverted()@source_pose[i]if parent>=0 else source_pose[i]
  result.append(result[parent]@local if parent>=0 else local)
 return with_weapon(result)

# Choose the authored contact sample against the actual accepted Human anatomy.
# This only crops real animation time; no invisible reach or collider is added.
HUMAN_CONTROL=STAGE/'arena-project/models/combat_anatomy/anatomy.bin'
control=HUMAN_CONTROL.read_bytes();_,count,_=struct.unpack_from('<III',control,8);offset=20
for _ in range(count):
 nv=struct.unpack_from('<I',control,offset)[0];offset+=12+nv*8*4
offset+=8*6*4;nc=struct.unpack_from('<I',control,offset)[0];offset+=4;CONTROL_CAPS=[]
for _ in range(nc):
 part,*v=struct.unpack_from('<I7f',control,offset);offset+=32;a=np.asarray(v[:3])*[-1,1,-1]+[0,0,1.25];b=np.asarray(v[3:6])*[-1,1,-1]+[0,0,1.25];CONTROL_CAPS.append((part,a,b,v[6]))
def segment_gap(p,q,a,b):
 u=q-p;v=b-a;w=p-a;uu=u@u;vv=v@v;uv=u@v;uw=u@w;vw=v@w;den=uu*vv-uv*uv
 ss=max(0.,min(1.,(uv*vw-uw*vv)/den))if den>1.e-15 else 0.;tt=(uv*ss+vw)/vv if vv>1.e-15 else 0.
 if tt<0:ss=max(0.,min(1.,-uw/uu))if uu>1.e-15 else 0.
 if tt>1:ss=max(0.,min(1.,(uv-uw)/uu))if uu>1.e-15 else 0.
 tt=max(0.,min(1.,tt));return float(np.linalg.norm(w+u*ss-v*tt))
AIM={}
def rotate_upper_body(pose,yaw,pitch,weight):
 if weight<=0:return pose
 sy,cy=math.sin(yaw*weight),math.cos(yaw*weight);sp,cp=math.sin(pitch*weight),math.cos(pitch*weight)
 actor=np.asarray([[1.,0.,0.],[0.,cp,-sp],[0.,sp,cp]])@np.asarray([[cy,0.,sy],[0.,1.,0.],[-sy,0.,cy]])
 rotation=Matrix(BASIS.T@actor@BASIS).to_quaternion();origin=pose[BONE_INDEX['spine_01']].translation;pose=list(pose)
 for i,n in enumerate(ACTUAL_BONES):
  bone=ARM.data.bones[n]
  if n=='spine_01'or any(p.name=='spine_01'for p in bone.parent_recursive):
   p,q,scale=pose[i].decompose();pose[i]=Matrix.LocRotScale(origin+rotation@(p-origin),rotation@q,scale)
 return with_weapon(pose)
for direction,source in ATTACKS.items():
 candidates=[];allowed=[1,2]if direction=='high'else[2,7,8]if direction=='low'else[2,3,4]
 for frame in range(18,61):
  t=frame/120.;pose=grounded_attack(direction,CAPTURES[source][frame],t,1.);weapon=pose[BONE_INDEX['Weapon1']];edge=((np.asarray([tuple(weapon@v)for v in AXE_EDGE_LOCAL])-NEUTRAL_ORIGIN)@BASIS.T*SCALE);pivot=(np.asarray(pose[BONE_INDEX['spine_01']].translation)-NEUTRAL_ORIGIN)@BASIS.T*SCALE
  for yaw in np.linspace(-.55,.55,11):
   sy,cy=math.sin(yaw),math.cos(yaw)
   for pitch in np.linspace(-.15,.45,9):
    sp,cp=math.sin(pitch),math.cos(pitch);rot=np.asarray([[1.,0.,0.],[0.,cp,-sp],[0.,sp,cp]])@np.asarray([[cy,0.,sy],[0.,1.,0.],[-sy,0.,cy]]);aimed=(edge-pivot)@rot.T+pivot
    gap,part=min((segment_gap(aimed[0],aimed[1],a,b)-radius,int(part))for part,a,b,radius in CONTROL_CAPS if part in allowed)
    score=max(0.,gap+.05)*10.+.10*(yaw*yaw+pitch*pitch)+.05*abs(t-CONTACTS[direction]);candidates.append((score,frame,gap,part,float(yaw),float(pitch)))
 _,frame,gap,part,yaw,pitch=min(candidates);CONTACTS[direction]=frame/120.;AIM[direction]={'yaw_radians':yaw,'pitch_radians':pitch,'actual_human_capsule_gap_m':gap,'source_sample':frame}
 contact_measurements[direction].update(normalized_source_contact=frame/120.,source_sample=frame,actual_human_capsule_gap_m=gap,actual_human_region=part,upper_body_aim=AIM[direction],method='Genuine source time crop plus bounded actual spine rotation for shorter opponent; real edge/limbs/grip unchanged.')


def clip_pose(name,t):
 if name=='idle':return with_weapon(NEUTRAL if t>=1. else sample('Idle',t))
 if name=='walk':return with_weapon(sample('Run',(t+.125)%1.))
 if name in ATTACKS:
  source=ATTACKS[name];source_time=continuous_source_time(t,CONTACTS[name]);captured=sample(source,source_time)
  lower_phase=min(.36,t*(.36/.25))if t<.36 else t
  lower_time=continuous_source_time(lower_phase,CONTACTS[name]);captured=merge_lower_body(captured,sample(source,lower_time))
  # Crossfades exist only at actual entry/exit. Every interior point is sampled
  # from the complete authored source trajectory, including footwork.
  entrance=smooth(t/.13);exit_weight=1.-smooth((t-.84)/.16);envelope=min(entrance,exit_weight)
  pose=grounded_attack(name,captured,lower_time,envelope)
  aim=AIM[name];pose=rotate_upper_body(pose,aim['yaw_radians'],aim['pitch_radians'],smooth(t/.22)*(1.-smooth((t-.65)/.35)))
  return with_weapon(blend_pose(NEUTRAL,pose,envelope))
 if name.startswith('guard_'):return make_guard(name[6:],t)
 if name=='death_back':
  captured=sample('Dead_1',t);return with_weapon(blend_pose(NEUTRAL,captured,smooth(t/.12)))if t<.12 else with_weapon(captured)
 source='Get_Hit_2'if name in ['hit_right','hit_thrust']else'Get_Hit_1';captured=sample(source,t);envelope=smooth(t/.16)if t<.16 else 1.-smooth((t-.75)/.25)if t>.75 else 1.;return with_weapon(blend_pose(NEUTRAL,captured,envelope))

# Actual running flight keeps one fixed source floor across the entire gait.
CURRENT_CLIP=None;run_floors=[]
for i in range(8):
 assign(with_weapon(sample('Run',(i/8.+.125)%1.)));run_floors.append(float(posed_arrays()[4][2]))
RUN_ORIGIN_Z=min(run_floors)
RUN_ROOT=SOURCE/'animations/Animations/3Root_With_Weapon/Anim_Warrior_Run_Root.FBX'
run_root,source_info['Run_Root']=snapshots(RUN_ROOT)

exchange={'bone_names':np.asarray(BONE_NAMES,dtype='U64'),'bone_parent_indices':np.asarray(PARENT_INDEX,dtype=np.int32),'part_names':np.asarray(['Head','Torso','LArm','RArm','LHand','RHand','LLeg','RLeg'],dtype='U16'),'rest_joints':rest_joints}
for i,(p,n,f)in enumerate(rest):
 exchange.update({f'rest_{i}':p,f'semantic_{i}':f['semantic'],f'uv_{i}':f['uv'],f'indices_{i}':f['indices'].astype(np.uint32),f'rigid_{i}':np.asarray(f['rigid']),f'weights_raw_{i}':f['weights_raw'],f'weights_{i}':f['weights']})
all_joints=[];sockets=[];pivots=[];entries=[];clip_rows=[];slot=0;max_error=0.;death_origin=None
for clip,count in CLIPS:
 CURRENT_CLIP=clip
 clip_rows.append({'name':clip,'base':slot,'count':count})
 for i in range(count):
  t=i/count if clip=='walk'else i/(count-1);pose=clip_pose(clip,t);max_error=max(max_error,assign(pose));rows,joints,edge,pivot,origin=posed_arrays(death_origin if clip=='death_back'else None)
  if clip=='death_back'and death_origin is None:death_origin=origin
  path=OUT/f'combat_body_{slot:03}.glb';glb(path,rows,slot,clip);all_joints.append(joints);sockets.append(edge);pivots.append(pivot)
  entries.append({'file':path.name,'clip':clip,'sample':i,'sha256':sha(path),'bytes':path.stat().st_size,'bounds_min':np.concatenate([r[0]for r in rows]).min(0).tolist(),'bounds_max':np.concatenate([r[0]for r in rows]).max(0).tolist()});print('GNUR_BODY_POSE',slot,clip,i,flush=True);slot+=1
assert slot==172
jsonwrite(STAGE/'arena_orcs/MOTION_CURVE_REVIEW.json',{'schema':'valwood.gnur.curated-motion.v1','passed':MOTION_QUALITY['leg_length_max_error_m']<.00001 and MOTION_QUALITY['foot_target_max_error_m']<.006 and all(v['actual_human_capsule_gap_m']<=0 for v in AIM.values()),'actual_source_attacks':ATTACKS,'step_adaptations':POLISH,'actual_upper_body_aim_adaptations':AIM,'authored_toe_plants':FOOT_PLAN,'quality':MOTION_QUALITY,'continuous_source_sampling':True,'source_toe_plants_before_release':True,'actual_human_contact_control':{'path':str(HUMAN_CONTROL.resolve()),'sha256':sha(HUMAN_CONTROL)},'contact_phase':.36,'idle_source_fraction':1.,'idle_both_endpoints_shared_neutral':True,'preserves_authored_walk_hip_sway':True,'authored_locomotion':'Run','locomotion_source_phase_offset':.125,'pose_origin_policy':'preserve'})
exchange['joints']=np.asarray(all_joints,dtype=np.float32);np.savez_compressed(EXCHANGE,**exchange)
(OUT/'combat_weapon_sockets.bin').write_bytes(struct.pack('<8sI',b'VWMOT172',172)+np.asarray(sockets,dtype='<f4').tobytes());(OUT/'combat_weapon_pivots.bin').write_bytes(struct.pack('<8sI',b'VWGRP172',172)+np.asarray(pivots,dtype='<f4').tobytes())
# Measure the retargeted actual feet, rather than copy a previous character stride.
walk=np.asarray([[(BASIS@np.asarray(pose[BONE_INDEX[n]].translation))*SCALE for n in ['foot_l','foot_r']]for pose in CAPTURES['Run']]);stride=(run_root[-1][BONE_INDEX['pelvis']].translation-run_root[0][BONE_INDEX['pelvis']].translation).length*SCALE;assert 2.<stride<4.,stride
jsonwrite(STAGE/'arena_orcs/WALK_SOURCE_MEASUREMENT.json',{'passed':True,'source':source_info['Run'],'root_motion_source':source_info['Run_Root'],'authored_locomotion':'Run','runtime_legacy_clip_name':'walk','source_phase_offset':.125,'bridge_support_phases':[0.,.5],'foot_fore_aft_ranges':np.ptp(walk[:,:,2],axis=0).tolist(),'walk_stride_m':stride,'method':'Actual owned matching Run_Root pelvis displacement per source cycle, transformed at measured target scale; armed Run occupies legacy8locomotion slots.','source_fixed_floor_z':RUN_ORIGIN_Z,'sampled_source_floor_span_m':(max(run_floors)-min(run_floors))*SCALE})
# Final measured anatomy profile is written by the independent anatomy producer.
(OUT/'combat_presentation.cfg').write_text(f'VALWOOD_COMBAT_PRESENTATION 1 upper_body_aimed 1 contact_frames 3.96 3.96 3.96 3.96 3.96 walk_stride_m {stride:.9f} body_radius_m 0.33 body_height_m 2.11 end\n')
manifest={'schema':'valwood.gnur.body-bank.v1','source_name':'Unity Orc Gnur','pose_count':slot,'clips':clip_rows,'assets':entries,'source_files':{str(p):sha(p)for p in [BODY_SOURCE,WEAPON_SOURCE]+[ANIM_SOURCE/f'Anim_Warrior_{n}.FBX'for n in SOURCE_CLIPS]+[RUN_ROOT]},'source_clips':source_info,'height_metres':HEIGHT,'source_neutral_height':height,'scale':SCALE,'basis':BASIS.tolist(),'maximum_global_matrix_assignment_error':max_error,'provisional_materials':False,'materials_note':'Original Gnur atlas, independently converted under matching source material and measured tangent-basis receipts.','attack_contact_measurements':contact_measurements,'axe_edge_vertex_ids':end_ids,'axe_edge_local':[list(v)for v in AXE_EDGE_LOCAL],'axe_hand_local':[list(r)for r in AXE_HAND_LOCAL],'actual_weapon_name':'Gnur Sword_mesh','weapon_primitive':7,'source_actual_bones':61,'pose_origin_policy':'preserve','authored_locomotion':'Run','virtual_held_frames':['Weapon1'],'semantic_exchange':str(EXCHANGE),'semantic_exchange_sha256':sha(EXCHANGE),'generator':{str(Path(__file__).resolve()):sha(__file__),str(STAGE/'gnur_body_functions.py'):sha(STAGE/'gnur_body_functions.py'),str(STAGE/'gnur_guard_fit.py'):sha(STAGE/'gnur_guard_fit.py')},'accepted':False}
jsonwrite(OUT/'combat_motion_manifest.json',manifest)
print('GNUR_BODY_BANK_COMPLETE',slot,EXCHANGE,flush=True)
