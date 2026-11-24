"""Offline humanoid retargeting for the user's MC longsword FBX source pack.
Only called inside Blender by build_combat_motion.py --longsword.
"""
from pathlib import Path
import math,hashlib
import bpy
from mathutils import Matrix,Vector

ROOT=Path(__file__).resolve().parents[1]
SOURCE=ROOT/'assets/purchased/longsword_mocap/source'
ANIM=SOURCE/'Animations/Unity_Skel'
FILES={
 'idle':'Idle/MCU_am_LongS_Lv1_StanceR_Idle_01.FBX',
 'walk':'Walk/MCU_am_LongS_Lv1_StanceR_Walk_Fwd_NoRM.FBX',
 'left':'Attack/MCU_am_LongS_Lv1_StanceR_Attack01.FBX',
 'right':'Attack/MCU_am_LongS_Lv1_StanceR_Attack03.FBX',
 'high':'Attack/MCU_am_LongS_StanceRBlock_Attack03.FBX',
 'low':'Attack/MCU_am_LongS_Lv1_StanceR_Attack11.FBX',
 'thrust':'Attack/MCU_am_LongS_Lv1_StanceR_Attack13.FBX',
 'guard_left':'Hitreact/MCU_am_LongS_Uni_BounceBackSmall_HighL_StanceR.FBX',
 'guard_right':'Hitreact/MCU_am_LongS_Uni_BounceBackSmall_HighR_StanceR.FBX',
 'guard_high':'Block/MCU_am_LongS_Lv1_StanceR_Idle_Block.FBX',
 'guard_low':'Hitreact/MCU_am_LongS_Uni_BounceBackSmall_LowL_StanceR.FBX',
 'guard_thrust':'Hitreact/MCU_am_LongS_Uni_BounceBackSmall_MidR_StanceR.FBX',
 'hit':'Hitreact/MCU_am_LongS_Uni_Hitreact_Light_Front_Mid_StanceR.FBX',
 'parry':'Hitreact/MCU_am_LongS_Uni_BounceBackSmall_MidR_StanceR.FBX'}
CONTACTS={'left':(40,53,65),'right':(38,50,61),'high':(31,44,56),'low':(13,21,32),'thrust':(17,32,43)}

def smooth(t):
 t=max(0.,min(1.,t));return t*t*(3-2*t)
def mix(a,b,t):
 return {n:(a[n][0].lerp(b[n][0],t),a[n][1].slerp(b[n][1],t),a[n][2].lerp(b[n][2],t))for n in a}
def humanoid_basis(arm,head,hip,left,right):
 def p(n):return arm.matrix_world@arm.data.bones[n].head_local
 up=(p(head)-p(hip)).normalized();side=p(right)-p(left);side=(side-up*side.dot(up)).normalized()
 return Matrix((side,up,side.cross(up))).transposed()

class LongswordRetarget:
 def __init__(self,target):
  self.target=target;target.animation_data.action=None
  for track in target.animation_data.nla_tracks:track.mute=True
  for b in target.pose.bones:b.matrix_basis=Matrix.Identity(4)
  bpy.context.view_layer.update()
  self.target_rest={b.name:target.matrix_world@b.matrix_local for b in target.data.bones}
  self.target_frame=humanoid_basis(target,'Head','Hips','LeftArm','RightArm')
  self.order=sorted(target.pose.bones,key=lambda b:len(b.parent_recursive))
  self.names={'Hips':'pelvis','Spine':'spine_01','Spine1':'spine_02','Spine2':'spine_03','Neck':'neck_01','Head':'head'}
  for prefix,side in [('Left','l'),('Right','r')]:
   for a,b in [('Shoulder','clavicle'),('Arm','upperarm'),('ForeArm','lowerarm'),('Hand','hand'),('UpLeg','thigh'),('Leg','calf'),('Foot','foot'),('ToeBase','ball')]:self.names[prefix+a]=b+'_'+side
   for digit in ['Thumb','Index','Middle','Ring','Pinky']:
    for i in range(1,4):self.names[f'{prefix}Hand{digit}{i}']=f'{digit.lower()}_{i:02}_{side}'
  children={'Hips':'Spine','Spine':'Spine1','Spine1':'Spine2','Spine2':'Neck','Neck':'Head'}
  for side in ['Left','Right']:
   for a,b in [('Shoulder','Arm'),('Arm','ForeArm'),('ForeArm','Hand'),('Hand','HandIndex1'),('UpLeg','Leg'),('Leg','Foot'),('Foot','ToeBase')]:children[side+a]=side+b
   for digit in ['Thumb','Index','Middle','Ring','Pinky']:
    for i in [1,2]:children[f'{side}Hand{digit}{i}']=f'{side}Hand{digit}{i+1}'
  self.rigs={};self.cache={};self.used={};self.grip=None
  for file in sorted(set(FILES.values())):
   before=set(bpy.data.objects);bpy.context.scene.frame_set(1)
   bpy.ops.import_scene.fbx(filepath=str(ANIM/file),use_anim=True,ignore_leaf_bones=False)
   added=set(bpy.data.objects)-before;arm=next(o for o in added if o.type=='ARMATURE')
   bpy.context.scene.frame_set(1);bpy.context.view_layer.update()
   C=self.target_frame@humanoid_basis(arm,'head','pelvis','upperarm_l','upperarm_r').transposed();cq=C.to_quaternion()
   src_rest={b.name:arm.matrix_world@b.matrix_local for b in arm.data.bones};offset={}
   for to,frm in self.names.items():
    if to not in self.target_rest or frm not in src_rest:continue
    a=cq@src_rest[frm].to_quaternion();b=self.target_rest[to].to_quaternion()
    child=children.get(to);source_child=self.names.get(child)
    if child in self.target_rest and source_child in src_rest:
     source_direction=C@(src_rest[source_child].translation-src_rest[frm].translation)
     target_direction=self.target_rest[child].translation-self.target_rest[to].translation
     swing=source_direction.normalized().rotation_difference(target_direction.normalized())
    else:swing=Vector((0,0,1)).rotation_difference(Vector((0,0,1)))
    # FBX joint primary axes are not always anatomical bone directions. Use
    # mapped child joints for the A/T-pose swing, retaining full bind rotation
    # compensation for hips/head and terminal joints.
    offset[to]=(swing@a).inverted()@b
   scale=self.target_rest['Hips'].translation.z/max(.01,src_rest['pelvis'].translation.z)
   self.rigs[file]=(arm,C,cq,offset,src_rest,scale)
   self.used[file]={'sha256':hashlib.sha256((ANIM/file).read_bytes()).hexdigest(),'source_frames':list(arm.animation_data.action.frame_range)}
  self.neutral=self.sample('idle',1.)
  up=(target.matrix_world@target.pose.bones['Head'].head)-(target.matrix_world@target.pose.bones['Hips'].head)
  self.neutral_up_dot=up.normalized().dot(self.target_frame.col[1]);assert self.neutral_up_dot>.9,self.neutral_up_dot
  self.guards={n:self.sample(n,1. if n=='guard_high'else self.frame(n,.18))for n in FILES if n.startswith('guard_')}
 def frame(self,name,t):
  arm=self.rigs[FILES[name]][0];a,b=arm.animation_data.action.frame_range;return a+(b-a)*t
 def sample(self,name,frame):
  file=FILES[name];key=(file,round(frame,6))
  if key in self.cache:return self.cache[key]
  src,C,cq,offset,rest,scale=self.rigs[file];bpy.context.scene.frame_set(math.floor(frame),subframe=frame%1)
  for b in self.target.pose.bones:b.matrix_basis=Matrix.Identity(4)
  bpy.context.view_layer.update();inverse_rotation=self.target.matrix_world.to_quaternion().inverted()
  for bone in self.order:
   frm=self.names.get(bone.name)
   if bone.name not in offset or frm not in src.pose.bones:continue
   rotation=inverse_rotation@cq@(src.matrix_world@src.pose.bones[frm].matrix).to_quaternion()@offset[bone.name]
   position=bone.head.copy()
   if bone.name=='Hips':
    source_head=src.matrix_world@src.pose.bones[frm].head
    root=self.target_rest['Hips'].translation.copy();root.z+=(source_head.z-rest['pelvis'].translation.z)*scale
    position=self.target.matrix_world.inverted()@root
   bone.matrix=Matrix.LocRotScale(position,rotation,Vector((1,1,1)));bpy.context.view_layer.update()
  result={b.name:b.matrix_basis.copy().decompose()for b in self.target.pose.bones};self.cache[key]=result;return result
 def pose(self,name,t):
  if name=='idle':return self.sample(name,self.frame(name,t))
  if name=='walk':return self.sample(name,1.+35.*t) # One measured period from the three-cycle source.
  if name in CONTACTS:
   knots=[0,.16,.36,.60,1.];i=min(3,next((i for i in range(4)if t<=knots[i+1]),3))
   arm=self.rigs[FILES[name]][0];lo,hi=arm.animation_data.action.frame_range
   frames=[lo,*CONTACTS[name],hi];frame=frames[i]+(frames[i+1]-frames[i])*(t-knots[i])/(knots[i+1]-knots[i])
   # Sample the captured motion throughout the window, including its small
   # anticipation/weight shifts, rather than interpolating three held poses.
   captured=self.sample(name,frame);weight=min(smooth(t/.10),smooth((1-t)/.14))
   return mix(self.neutral,captured,weight)
  if name.startswith('guard_'):return mix(self.neutral,self.guards[name],smooth(t))
  captured=self.sample(name,self.frame(name,t));weight=min(smooth(t/.10),smooth((1-t)/.15))
  return mix(self.neutral,captured,weight)
 def bind_grip(self,ik):
  src,C,cq,offset,rest,scale=self.rigs[FILES['idle']];bpy.context.scene.frame_set(1);bpy.context.view_layer.update()
  delta=C@((src.matrix_world@src.pose.bones['hand_l'].head)-(src.matrix_world@src.pose.bones['hand_r'].head))*scale
  hand=self.target.pose.bones['LeftHand'];rotation=hand.matrix.to_quaternion()
  right=self.target.matrix_world@self.target.pose.bones['RightHand'].head
  ik('LeftArm','LeftForeArm','LeftHand',right+delta)
  hand.matrix=Matrix.LocRotScale(hand.head,rotation,hand.matrix.to_scale());bpy.context.view_layer.update()
  self.neutral={b.name:b.matrix_basis.copy().decompose()for b in self.target.pose.bones}
  r=self.target.matrix_world@self.target.pose.bones['RightHand'].matrix;l=self.target.matrix_world@self.target.pose.bones['LeftHand'].matrix
  self.grip=(r.inverted()@l.translation,r.to_quaternion().inverted()@l.to_quaternion())
 def constrain_grip(self,ik):
  r=self.target.matrix_world@self.target.pose.bones['RightHand'].matrix
  ik('LeftArm','LeftForeArm','LeftHand',r@self.grip[0]);hand=self.target.pose.bones['LeftHand']
  rotation=self.target.matrix_world.to_quaternion().inverted()@r.to_quaternion()@self.grip[1]
  hand.matrix=Matrix.LocRotScale(hand.head,rotation,hand.matrix.to_scale());bpy.context.view_layer.update()
 def metadata(self):
  return dict(neutral_up_dot=self.neutral_up_dot,source_directory=str(SOURCE),selected_clips=self.used,bone_mapping=self.names,contact_source_frames=CONTACTS,walk_period_frames=[1,36],root_motion='Horizontal root travel removed for the existing constrained training lane',retarget_method='Captured world joint rotations with semantic-child bind compensation; proportion-aware hips; fixed two-hand grip constraint',source_scripts_executed=False)

def install_longsword(target):
 """Bind the purchased pack's sword; reuse the owned knight's PBR atlas.
 The two source files remain unchanged. UV transfer is a cooked derivative.
 """
 import numpy as np
 from mathutils.bvhtree import BVHTree
 from mathutils.geometry import barycentric_transform
 reference=bpy.data.objects['Sword'];reference.name='Sword_Owned_Atlas_Reference'
 graph=bpy.context.evaluated_depsgraph_get();evaluated=reference.evaluated_get(graph);mesh=evaluated.to_mesh();mesh.calc_loop_triangles()
 old=np.asarray([tuple(evaluated.matrix_world@v.co)for v in mesh.vertices]);triangles=[tuple(t.vertices)for t in mesh.loop_triangles]
 coords=[tuple(mesh.uv_layers.active.data[i].uv)for i in range(len(mesh.loops))];triangle_uv=[tuple(coords[i]for i in t.loops)for t in mesh.loop_triangles]
 bvh=BVHTree.FromPolygons([Vector(p)for p in old],triangles,all_triangles=True)
 center=old.mean(axis=0);_,axes=np.linalg.eigh(np.cov(old.T));old_basis=axes[:,[1,2,0]];canonical=(old-center)@old_basis
 lo=canonical.min(axis=0);hi=canonical.max(axis=0)
 file=SOURCE/'Objects/Sword/SM_Erantis_NoviceSword_Sword_V2.FBX';before=set(bpy.data.objects)
 bpy.ops.import_scene.fbx(filepath=str(file),use_anim=False)
 imported=[o for o in set(bpy.data.objects)-before if o.type=='MESH'];assert len(imported)==1
 sword=imported[0];sword.name='Sword';raw=np.asarray([tuple(sword.matrix_world@v.co)for v in sword.data.vertices]);points=raw[:,[1,2,0]]
 nlo=points.min(axis=0);nhi=points.max(axis=0);assert 1.1<nhi[1]-nlo[1]<1.5
 # Nearest source triangles preserve actual steel/leather albedo, normal and
 # roughness regions, without three new copies of material images per pose.
 uv=sword.data.uv_layers.active
 for loop in sword.data.loops:
  q=points[loop.vertex_index];unit=(q-nlo)/np.maximum(nhi-nlo,1e-8);mapped=center+old_basis@(lo+unit*(hi-lo));hit,normal,index,distance=bvh.find_nearest(Vector(mapped))
  assert index is not None
  a,b,c=[Vector(old[i])for i in triangles[index]];ua,ub,uc=[Vector((u,v,0))for u,v in triangle_uv[index]]
  result=barycentric_transform(hit,a,b,c,ua,ub,uc);uv.data[loop.index].uv=result.xy
 for v,p in zip(sword.data.vertices,points):v.co=p
 # Preserve the exact mesh silhouette while giving the metal its continuous
 # bevel normals. Sharp edges above40degrees retain their geometric break.
 import bmesh
 bpy.context.view_layer.objects.active=sword
 if sword.data.has_custom_normals:bpy.ops.mesh.customdata_custom_splitnormals_clear()
 bm=bmesh.new();bm.from_mesh(sword.data)
 for face in bm.faces:face.smooth=True
 for edge in bm.edges:edge.smooth=not edge.is_manifold or edge.calc_face_angle(0.)<math.radians(40)
 bm.to_mesh(sword.data);bm.free();sword.data.use_auto_smooth=True;sword.data.auto_smooth_angle=math.radians(40);sword.data.update()
 def fist(side):
  hand=target.pose.bones[side+'Hand'];joints=[target.pose.bones.get(side+'Hand'+f+'1')for f in ['Index','Ring']];joints=[b for b in joints if b]
  return target.matrix_world@(sum((b.head for b in joints),Vector())/len(joints)if joints else hand.tail)
 right,left=fist('Right'),fist('Left');direction=(right-left).normalized();r=target.matrix_world@target.pose.bones['RightHand'].matrix
 width=r.to_3x3().col[0];width=(width-direction*width.dot(direction)).normalized();thickness=width.cross(direction).normalized()
 transform=Matrix((width,direction,thickness)).transposed().to_4x4();transform.translation=right
 sword.parent=target;sword.parent_type='BONE';sword.parent_bone='RightHand';bpy.context.view_layer.update();sword.matrix_world=transform;bpy.context.view_layer.update()
 assert max(abs(sword.matrix_world[i][j]-transform[i][j])for i in range(4)for j in range(4))<.00001
 evaluated.to_mesh_clear()
 return dict(file=str(file),sha256=hashlib.sha256(file.read_bytes()).hexdigest(),vertices=len(points),triangles=len(sword.data.polygons),world_length_m=float(nhi[1]-nlo[1]),material_method='Nearest original owned sword atlas triangle UVs, normalized in longitudinal/guard/thickness coordinates',parent_bone='RightHand',smoothing_angle_degrees=40,silhouette_unchanged=True)
