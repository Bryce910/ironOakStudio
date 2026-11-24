#!/usr/bin/env python3
"""Bake three owned-knight archery poses; run in Blender with source scripts disabled.

blender -b -t 2 --disable-autoexec --factory-startup --python-exit-code 1 \
  --python tools/build_arena_bow.py -- --output build/combat-arena/assets
No purchased source or accepted sword pose is changed. Bow/string/arrow are
runtime geometry attached to the measured hand sockets in BOW_ASSETS.json.
"""
from pathlib import Path
import hashlib,json,math,sys
ROOT=Path(__file__).resolve().parents[1]

def main():
 import bpy,bmesh,numpy as np
 from mathutils import Matrix,Vector
 sys.path.insert(0,str(ROOT/'tools'))
 import import_village_shield_knight as cooker
 out=ROOT/'build/combat-arena/assets'
 if '--output' in sys.argv:out=Path(sys.argv[sys.argv.index('--output')+1]).resolve()
 allowed=(ROOT/'build/combat-arena').resolve()
 if out!=allowed and allowed not in out.parents:raise ValueError('Bow derivative must remain inside build/combat-arena')
 if out.exists() and any(out.iterdir()):raise ValueError('Use a fresh empty bow output')
 source=cooker.SOURCE/'Knight with shield.blend';digest=lambda p:hashlib.sha256(p.read_bytes()).hexdigest();source_hash=digest(source)
 protected={str(p):digest(p) for p in cooker.DERIVED.glob('*.png')}
 bpy.context.preferences.filepaths.use_scripts_auto_execute=False
 bpy.ops.wm.open_mainfile(filepath=str(source),load_ui=False,use_scripts=False)
 scene=bpy.context.scene;arm=bpy.data.objects['Knight with shield']
 for track in arm.animation_data.nla_tracks:track.mute=True
 for action in bpy.data.actions:
  for fc in action.fcurves:fc.data_path=fc.data_path.replace('mixamorig:','')
 arm.animation_data.action=bpy.data.actions['Idle'];scene.frame_set(1);bpy.context.view_layer.update()
 neutral={b.name:b.matrix_basis.copy() for b in arm.pose.bones}
 arm.animation_data.action=None;world=arm.matrix_world.copy();inverse=world.inverted()
 names=['Knight','Helm'];cooker.NAMES=names
 basis=np.array([[1.,0.,0.],[0.,0.,1.],[0.,-1.,0.]])
 # Fixed seams and fixed triangles are chosen once before any arm deformation.
 topology={};masks={};reach={}
 for name in names:
  ob=bpy.data.objects[name];bm=bmesh.new();bm.from_mesh(ob.data);bmesh.ops.triangulate(bm,faces=list(bm.faces),quad_method='BEAUTY',ngon_method='BEAUTY');bm.to_mesh(ob.data);bm.free();ob.data.update()
  groups={g.index for g in ob.vertex_groups if any(s in g.name for s in ['ForeArm','Hand','Thumb','Finger']) or g.name in ['LeftArm','RightArm']}
  masks[name]=np.array([sum(g.weight for g in v.groups if g.group in groups)>.45 for v in ob.data.vertices]) if name=='Knight' else np.zeros(len(ob.data.vertices),bool)
  values=[]
  for v in ob.data.vertices:
   value=0.;point=ob.matrix_world@v.co
   for g in v.groups:
    group=ob.vertex_groups[g.group].name
    if any(s in group for s in ['Hand','Thumb','Finger']):value+=g.weight
    elif group in ['LeftArm','RightArm','LeftForeArm','RightForeArm']:
     bone=arm.data.bones[group];a=world@bone.head_local;b=world@bone.tail_local;axis=b-a;t=max(0.,min(1.,(point-a).dot(axis)/max(axis.length_squared,1e-8)))
     value+=g.weight*((.5+.5*t) if group.endswith('ForeArm') else .5*t)
   values.append(min(1.,value))
  reach[name]=np.array(values)
 def assign():
  for bone in arm.pose.bones:bone.matrix_basis=neutral[bone.name].copy()
  bpy.context.view_layer.update()
 def sample(reference=False):
  graph=bpy.context.evaluated_depsgraph_get();a=arm.evaluated_get(graph);hip=a.matrix_world@a.pose.bones['Hips'].head;ps=[];ns=[];uvs=[];tris=[];offset=0;minimum=1e9;maximum=-1e9
  for name in names:
   ob=bpy.data.objects[name].evaluated_get(graph);m=ob.to_mesh();m.calc_loop_triangles();m.calc_normals_split();nm=ob.matrix_world.to_3x3().inverted().transposed()
   p=np.asarray([tuple(ob.matrix_world@v.co) for v in m.vertices]);n=np.asarray([tuple((nm@v.vector).normalized()) for v in m.corner_normals]);uv=np.asarray([tuple(v.uv) for v in m.uv_layers.active.data]);tri=np.asarray([list(t.loops) for t in m.loop_triangles],dtype=np.int64)
   minimum=min(minimum,float(p[:,2].min()));maximum=max(maximum,float(p[:,2].max()))
   for t in m.loop_triangles:
    bad=[i for i in t.loops if np.dot(n[i],n[i])<.5]
    if bad:
     q=p[list(t.vertices)];normal=np.cross(q[1]-q[0],q[2]-q[0]);normal/=max(np.linalg.norm(normal),1e-12)
     if np.linalg.norm(normal)<.5:normal=np.array([0.,0.,1.])
     for i in bad:n[i]=normal
   if reference:
    mapping={};representatives=[];remap=[]
    for i,loop in enumerate(m.loops):
     key=(loop.vertex_index,*np.round(uv[i],8),*np.round(n[i],6))
     if key not in mapping:mapping[key]=len(representatives);representatives.append(i)
     remap.append(mapping[key])
    topology[name]=dict(rep=np.asarray(representatives),raw=np.asarray([m.loops[i].vertex_index for i in representatives]),remap=np.asarray(remap),tri=tri,vertices=len(m.vertices),loops=len(m.loops))
   fixed=topology[name];assert fixed['vertices']==len(m.vertices) and fixed['loops']==len(m.loops) and np.array_equal(tri,fixed['tri'])
   ps.append(p[fixed['raw']]);ns.append(n[fixed['rep']]);q=uv[fixed['rep']].copy();q[:,1]=1-q[:,1];uvs.append(q);tris.append(fixed['remap'][tri]+offset);offset+=len(q);ob.to_mesh_clear()
  p,n,uv,tri=map(np.concatenate,[ps,ns,uvs,tris]);origin=np.array([hip.x,hip.y,minimum]);p-=origin
  return p@basis.T,n@basis.T,uv,tri,maximum-minimum,origin,a
 assign();p,n,uv,tri,height,origin,a=sample(True);scale=cooker.HEIGHT/height
 anchors={s:world@arm.pose.bones[s+'Foot'].head for s in ['Left','Right']}
 mask=np.concatenate([masks[name][topology[name]['raw']] for name in names]);selected=tri[np.all(mask[tri],axis=1)];arm_map=np.unique(selected);remap=np.full(len(p),-1);remap[arm_map]=np.arange(len(arm_map));arm_tri=remap[selected]
 arm_reach=np.concatenate([reach[name][topology[name]['raw']] for name in names])[arm_map]
 signs=cooker.tangents(p,n,uv,tri)[:,3];arm_signs=signs[arm_map]
 def target(v):return Vector(origin)+Vector((v[0],-v[2],v[1]))/scale
 def ik(side,goal,pole):
  upper,lower,hand=[arm.pose.bones[side+n] for n in ['Arm','ForeArm','Hand']];start=upper.head.copy();elbow=lower.head.copy();wrist=hand.head.copy();goal=inverse@goal;pole=inverse@pole
  u=(elbow-start).length;l=(wrist-elbow).length;v=goal-start;distance=min(max(v.length,abs(u-l)+.001),u+l-.001);direction=v.normalized();sidevec=pole-start-direction*(pole-start).dot(direction)
  if sidevec.length<1e-6:raise ValueError('Degenerate bow elbow plane')
  sidevec.normalize();along=(u*u-l*l+distance*distance)/(2*distance);e=start+direction*along+sidevec*math.sqrt(max(0,u*u-along*along));endpoint=start+direction*distance
  rotation=(elbow-start).rotation_difference(e-start)@upper.matrix.to_quaternion();upper.matrix=Matrix.LocRotScale(start,rotation,upper.matrix.to_scale());bpy.context.view_layer.update()
  rotation=(hand.head-lower.head).rotation_difference(endpoint-e)@lower.matrix.to_quaternion();lower.matrix=Matrix.LocRotScale(e,rotation,lower.matrix.to_scale());bpy.context.view_layer.update()
  return world@hand.head
 print('BOW_RIG', {s:((np.array(tuple(world@arm.pose.bones[s+'Arm'].head))-origin)@basis.T*scale).tolist() for s in ['Left','Right']}, flush=True)
 alignment_yaw=-math.atan2(.23-(-.12),.58-.06)
 cs,sn=math.cos(alignment_yaw),math.sin(alignment_yaw);alignment=np.array([[cs,0,sn],[0,1,0],[-sn,0,cs]])
 view_shift=np.array([0.,0.,.18]);entries=[];poses=[];maxerror=0.
 out.mkdir(parents=True,exist_ok=True)
 for index,draw in enumerate([0.,.5,1.]):
  assign();left=np.array([.23,1.40,.58]);right=np.array([.02,1.40,.25])*(1-draw)+np.array([-.12,1.49,.06])*draw
  goals={'Left':target(left),'Right':target(right)}
  ik('Left',goals['Left'],target((.48,1.32,.24)));ik('Right',goals['Right'],target((-.54,1.43,-.10)))
  # Source finger curls remain intact; only upper/lower arm IK is authored.
  bpy.context.view_layer.update();p,n,actual_uv,actual_tri,_,actual_origin,a=sample()
  p*=scale;assert np.array_equal(actual_uv,uv) and np.array_equal(actual_tri,tri) and abs(float(p[:,1].min()))<1e-5 and np.isfinite(p).all()
  sockets={}
  for key,bone in [('left_grip','LeftHand'),('right_string','RightHand'),('head','Head')]:sockets[key]=((np.array(tuple(a.matrix_world@a.pose.bones[bone].head))-actual_origin)@basis.T*scale).tolist()
  errors={s:(world@arm.pose.bones[s+'Hand'].head-goals[s]).length*scale for s in ['Left','Right']};maxerror=max(maxerror,*errors.values())
  feet={s:(world@arm.pose.bones[s+'Foot'].head-anchors[s]).length*scale for s in ['Left','Right']};assert max(feet.values())<1e-5
  p=p@alignment.T;n=n@alignment.T
  sockets={key:(np.array(value)@alignment.T).tolist() for key,value in sockets.items()}
  fps={key:(np.array(v)+view_shift).tolist() for key,v in sockets.items() if key!='head'}
  head=np.array(sockets['head']);poses.append(dict(index=index,draw_fraction=draw,actor_local=sockets,head_local={k:(np.array(v)-head).tolist() for k,v in sockets.items() if k!='head'},first_person_local=fps,hand_target_error_m=errors,ankle_displacement_m=feet))
  for kind,ids,indices,sgn in [('body',None,tri,signs),('arms',arm_map,arm_tri,arm_signs)]:
   q=p.copy() if ids is None else p[ids].copy();normal=n.copy() if ids is None else n[ids].copy();coords=uv if ids is None else uv[ids]
   if kind=='arms':
    q+=arm_reach[:,None]*view_shift;face=np.cross(q[indices[:,1]]-q[indices[:,0]],q[indices[:,2]]-q[indices[:,0]]);rebuilt=np.zeros_like(normal)
    for corner in range(3):np.add.at(rebuilt,indices[:,corner],face)
    length=np.linalg.norm(rebuilt,axis=1);valid=(length>1e-10)&(arm_reach<.999999);normal[valid]=rebuilt[valid]/length[valid,None]
   tangent=cooker.tangents(q,normal,coords,indices,sgn);path=out/f'bow_{kind}_{index:03}.glb';cooker.write_glb(path,(q,normal,coords,tangent,indices),f'Owned knight archery draw {draw:g} {kind}')
   entries.append(dict(file=path.name,kind=kind,index=index,draw_fraction=draw,sha256=digest(path),bytes=path.stat().st_size,vertices=len(q),triangles=len(indices),index_sha256=hashlib.sha256(indices.astype('<u4').tobytes()).hexdigest(),uv_sha256=hashlib.sha256(coords.astype('<f4').tobytes()).hexdigest(),bounds_min=q.min(0).tolist(),bounds_max=q.max(0).tolist()))
  print('BOW_POSE',index,draw,errors,sockets,flush=True)
 assert maxerror<.025,('Unreachable bow grip',maxerror)
 assert digest(source)==source_hash and all(digest(Path(p))==h for p,h in protected.items())
 for kind in ['body','arms']:
  rows=[e for e in entries if e['kind']==kind];assert len({(e['vertices'],e['triangles'],e['index_sha256'],e['uv_sha256']) for e in rows})==1
 manifest=dict(schema='valwood.arena-bow-assets',version=1,passed=True,source=str(source),source_sha256=source_hash,source_unchanged=True,source_scripts_executed=False,owned_material_maps_sha256=protected,source_parts=names,omitted_source_parts=['Sword','Shield'],basis='+X actor left, +Y up, +Z forward; metre units; feet Y0',body_loader='world_model_load_pose',arms_loader='world_model_load_pose_origin',interpolation='same vertex order, indices, UVs per kind; draw 0/.5/1, lerp adjacent poses',first_person_reach_shift_m=view_shift.tolist(),first_person_reach='Source skin weights taper shift through upper sleeve and forearm; rigid translation at hands',default_arms_instance=dict(position=[0,-1.15,.05],yaw=math.pi,scale=.80),maximum_hand_target_error_m=maxerror,poses=poses,assets=entries,alignment_yaw_radians=alignment_yaw,runtime_bow_geometry_included=False)
 socket_rows=[[[pose['actor_local' if kind==0 else 'first_person_local'][key] for key in ['left_grip','right_string']] for pose in poses] for kind in range(2)]
 def cpp(v):return '{'+','.join(cpp(x) if isinstance(x,list) else f'{x:.9f}f' for x in v)+'}'
 (out/'arena_bow_sockets.inc').write_text('// Generated from measured owned-knight IK sockets. Body/arms, draw0/.5/1, grip/string, XYZ metres.\nstatic constexpr float arena_bow_sockets[2][3][2][3]='+cpp(socket_rows)+';\n')
 (out/'BOW_ASSETS.json').write_text(json.dumps(manifest,indent=2)+'\n');print('BOW_ASSETS_READY',out,flush=True)
if __name__=='__main__':main()
