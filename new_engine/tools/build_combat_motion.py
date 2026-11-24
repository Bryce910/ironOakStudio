#!/usr/bin/env python3
"""Bake owned knight attacks, directional guard adaptations, planted feet and FPS arms.
Source .blend files are read only; scripts are disabled. No third-party downloads.
"""
from pathlib import Path
import sys,math,json,hashlib
ROOT=Path(__file__).resolve().parents[1]
LONGSWORD='--longsword'in sys.argv
SOCKETS_ONLY='--sockets-only'in sys.argv
HELD_GUARDS_ONLY='--held-guards-only'in sys.argv
CHARACTER_POLISH='--character-polish'in sys.argv
OUT=ROOT/'projects/valwood_world/models'/('longsword_mocap'if LONGSWORD else '')
if '--output'in sys.argv:
    OUT=Path(sys.argv[sys.argv.index('--output')+1]).resolve()
sys.path.insert(0,str(ROOT/'tools'))
CLIPS=[('idle',4),('walk',8),('left',12),('right',12),('high',12),('low',12),('thrust',12),('guard_left',4),('guard_right',4),('guard_high',4),('guard_low',4),('guard_thrust',4),('hit',8),('parry',8)]
def main():
    import bpy,bmesh,numpy as np
    from mathutils import Matrix,Vector,Quaternion
    from import_village_shield_knight import SOURCE,NAMES,HEIGHT,tangents,write_glb
    polish=None
    if CHARACTER_POLISH:
        assert LONGSWORD and '--output'in sys.argv and not SOCKETS_ONLY and not HELD_GUARDS_ONLY
        from character_polish_bank import Extension,EXTRA_CLIPS,EXTRA_NAMES
        polish=Extension(OUT)
    bpy.context.preferences.filepaths.use_scripts_auto_execute=False
    bpy.ops.wm.open_mainfile(filepath=str(SOURCE/'Knight with shield.blend'),load_ui=False,use_scripts=False)
    scene=bpy.context.scene;arm=bpy.data.objects['Knight with shield']
    for track in arm.animation_data.nla_tracks:track.mute=True
    # Several supplied duplicate actions still reference the old Mixamo prefix.
    # Repair channels in memory only; the purchased source is never saved.
    for action in bpy.data.actions:
        for fc in action.fcurves:fc.data_path=fc.data_path.replace('mixamorig:','')
    basis=np.array([[1.,0.,0.],[0.,0.,1.],[0.,-1.,0.]])
    def snapshot(action,frame):
        arm.animation_data.action=bpy.data.actions[action];scene.frame_set(math.floor(frame),subframe=frame%1);bpy.context.view_layer.update()
        return {b.name:b.matrix_basis.copy().decompose() for b in arm.pose.bones}
    neutral=snapshot('Idle',1)
    world=arm.matrix_world.copy();inverse=world.inverted()
    anchors={side:world@arm.pose.bones[side+'Foot'].head for side in ['Left','Right']}
    neutral_hip=world@arm.pose.bones['Hips'].head
    foot_rotations={side:(world@arm.pose.bones[side+'Foot'].matrix).to_quaternion()for side in ['Left','Right']}
    def mix_pose(a,b,t):
        return {name:(a[name][0].lerp(b[name][0],t),a[name][1].slerp(b[name][1],t),a[name][2].lerp(b[name][2],t))for name in a}
    def assign(pose):
        arm.animation_data.action=None
        for b in arm.pose.bones:b.matrix_basis=Matrix.LocRotScale(*pose[b.name])
        bpy.context.view_layer.update()
    def ik(upper,lower,hand,target):
        a,b,c=[arm.pose.bones[n]for n in [upper,lower,hand]]
        s=a.head.copy();elbow=b.head.copy();w=c.head.copy();goal=inverse@target
        u=(elbow-s).length;l=(w-elbow).length;v=goal-s;dist=min(max(v.length,abs(u-l)+.001),u+l-.001);d=v.normalized()
        pole=(elbow-s)-d*(elbow-s).dot(d)
        if pole.length<.00001:pole=Vector((0,-1,0))-d*d.dot(Vector((0,-1,0)))
        pole.normalize();along=(u*u-l*l+dist*dist)/(2*dist);e=s+d*along+pole*math.sqrt(max(0,u*u-along*along));goal=s+d*dist
        rotation=(elbow-s).rotation_difference(e-s)@a.matrix.to_quaternion();a.matrix=Matrix.LocRotScale(s,rotation,a.matrix.to_scale());bpy.context.view_layer.update()
        current=c.head-b.head;rotation=current.rotation_difference(goal-e)@b.matrix.to_quaternion();b.matrix=Matrix.LocRotScale(e,rotation,b.matrix.to_scale());bpy.context.view_layer.update()
    maximum_ankle_error=0.
    def plant():
        nonlocal maximum_ankle_error
        hip=world@arm.pose.bones['Hips'].head;shift=Vector((hip.x-neutral_hip.x,hip.y-neutral_hip.y,0))
        if LONGSWORD:
            drop=0.
            for side in ['Left','Right']:
                a,b,c=[arm.pose.bones[side+n]for n in ['UpLeg','Leg','Foot']]
                start=world@a.head;elbow=world@b.head;end=world@c.head;goal=anchors[side]+shift
                reach=(start-elbow).length+(elbow-end).length-.0002
                horizontal=(Vector((start.x,start.y,0))-Vector((goal.x,goal.y,0))).length
                drop=max(drop,start.z-goal.z-math.sqrt(max(.0001,reach*reach-horizontal*horizontal)))
            if drop>0:
                hipbone=arm.pose.bones['Hips'];m=hipbone.matrix.copy();m.translation+=world.to_3x3().inverted()@Vector((0,0,-drop));hipbone.matrix=m;bpy.context.view_layer.update()
        for side in ['Left','Right']:
            target=anchors[side]+shift
            ik(side+'UpLeg',side+'Leg',side+'Foot',target)
            foot=arm.pose.bones[side+'Foot']
            if LONGSWORD:
                rotation=world.to_quaternion().inverted()@foot_rotations[side]
                foot.matrix=Matrix.LocRotScale(foot.head,rotation,foot.matrix.to_scale());bpy.context.view_layer.update()
            maximum_ankle_error=max(maximum_ankle_error,(world@foot.head-target).length)
    def smooth(t):return t*t*(3-2*t)
    attack_sources={'left':('slash 1',12,24,35),'right':('Attack',11,20,30),'high':('Attack 2',14,27,40),'low':('slash 2',80,54,42)}
    keys={name:[neutral,snapshot(action,w),snapshot(action,c),snapshot(action,f),neutral]for name,(action,w,c,f)in attack_sources.items()}
    guard=snapshot('Block 1',18);hit=snapshot('Hit',12);parry=snapshot('Block 2',7)
    sword=bpy.data.objects['Sword'];assign(neutral);graph=bpy.context.evaluated_depsgraph_get();ev=sword.evaluated_get(graph);mesh=ev.to_mesh();pts=np.array([tuple(ev.matrix_world@v.co)for v in mesh.vertices]);ev.to_mesh_clear();axis=np.linalg.eigh(np.cov(pts.T))[1][:,-1];axis=Vector(axis);axis*=1 if axis.z>0 else -1
    neutral_hand_world=world@arm.pose.bones['RightHand'].head
    neutral_wrist_rotation=(world@arm.pose.bones['RightHand'].matrix).to_quaternion()
    def set_clip(name,t):
        if name=='idle':pose=snapshot('Idle',1+t*30)
        elif name=='walk':pose=snapshot('Walk',1+t*33)
        elif name in keys:
            knots=[0,.16,.36,.60,1.];i=min(3,next((i for i in range(4)if t<=knots[i+1]),3));pose=mix_pose(keys[name][i],keys[name][i+1],smooth((t-knots[i])/(knots[i+1]-knots[i])))
        elif name=='thrust':pose=neutral
        elif name.startswith('guard_'):pose=mix_pose(neutral,guard,smooth(t))
        elif name=='hit':pose=mix_pose(neutral,hit,math.sin(math.pi*t)**2)
        else:pose=mix_pose(neutral,parry,math.sin(math.pi*t)**2)
        # Preserve a bounded grounded stance through strong source root turns.
        if name!='walk':
            pose=dict(pose);p,q,s=pose['Hips'];pose['Hips']=(p,neutral['Hips'][1].slerp(q,.25),s)
        assign(pose)
        if name=='thrust':
            amount=smooth(t/.36)if t<.36 else 1-smooth((t-.36)/.64)
            ik('RightArm','RightForeArm','RightHand',neutral_hand_world+Vector((.05*amount,-.53*amount,.16*amount)))
            hand=arm.pose.bones['RightHand']
            delta=(world@hand.matrix).to_quaternion()@neutral_wrist_rotation.inverted()
            current_axis=delta@axis
            rot=Quaternion((1,0,0,0)).slerp(current_axis.rotation_difference(Vector((0,-1,0))),amount)
            local=world.to_3x3().inverted()@rot.to_matrix()@world.to_3x3()
            hand.matrix=Matrix.LocRotScale(hand.head,local.to_quaternion()@hand.matrix.to_quaternion(),hand.matrix.to_scale());bpy.context.view_layer.update()
        if name.startswith('guard_'):
            offset={'guard_left':(-.22,0,.04),'guard_right':(.25,0,.04),'guard_high':(0,0,.30),'guard_low':(0,-.03,-.22),'guard_thrust':(0,-.17,.02)}[name]
            hand_world=world@arm.pose.bones['LeftHand'].head;ik('LeftArm','LeftForeArm','LeftHand',hand_world+Vector(offset)*smooth(t))
        if name!='walk':plant()
    mocap=None;weapon_metadata=None
    if LONGSWORD:
        from retarget_longsword import LongswordRetarget,install_longsword
        mocap=LongswordRetarget(arm)
        assign(mocap.neutral);mocap.bind_grip(ik);neutral=mocap.neutral;assign(neutral)
        anchors={side:world@arm.pose.bones[side+'Foot'].head for side in ['Left','Right']}
        neutral_hip=world@arm.pose.bones['Hips'].head
        foot_rotations={side:(world@arm.pose.bones[side+'Foot'].matrix).to_quaternion()for side in ['Left','Right']}
        weapon_metadata=install_longsword(arm)
        NAMES=tuple(n for n in NAMES if n!='Shield')
        def set_clip(name,t):
            assign(polish.pose(mocap,name,t)if polish and name in EXTRA_NAMES else mocap.pose(name,t))
            # Source attacks retain captured steps. Stationary holds/reactions
            # pin their ankles to the neutral support; both hands share a hilt.
            if name not in ['walk','left','right','high','low','thrust','death_back']:plant()
            if name!='death_back':
                if polish and name in EXTRA_NAMES and t<1.-1e-7:polish.constrain_grip(mocap,ik,arm,world)
                else:mocap.constrain_grip(ik)
    masks={};view_reach={}
    for name in NAMES:
        ob=bpy.data.objects[name];bm=bmesh.new();bm.from_mesh(ob.data)
        bmesh.ops.triangulate(bm,faces=list(bm.faces),quad_method='BEAUTY',ngon_method='BEAUTY');bm.to_mesh(ob.data);bm.free();ob.data.update()
        # Raised guards expose the elbow. Retain the continuous upper sleeve
        # instead of ending the first-person mesh at the forearm joint.
        groups={g.index for g in ob.vertex_groups if any(s in g.name for s in ['ForeArm','Hand','Thumb','Finger'])
                or (LONGSWORD and g.name in ['LeftArm','RightArm'])}
        masks[name]=np.array([sum(g.weight for g in v.groups if g.group in groups)>.45 for v in ob.data.vertices]) if name=='Knight' else np.full(len(ob.data.vertices),name in ['Shield','Sword'])
        if LONGSWORD and name=='Knight':
            # A third-person stance holds its hands close across the chest.
            # Adapt only the viewmodel: move the two hands and weapon together
            # into comfortable first-person reach, tapering to zero at each
            # shoulder. Full-body poses and the shared hilt remain unchanged.
            def reach(v):
                point=ob.matrix_world@v.co;value=0.
                for g in v.groups:
                    group=ob.vertex_groups[g.group].name
                    if any(s in group for s in ['Hand','Thumb','Finger']):value+=g.weight
                    elif group.endswith(('Arm','ForeArm')) and group in arm.data.bones:
                        bone=arm.data.bones[group];a=world@bone.head_local;b=world@bone.tail_local;axis=b-a
                        t=max(0.,min(1.,(point-a).dot(axis)/max(axis.length_squared,1e-8)))
                        value+=g.weight*((.5+.5*t)if group.endswith('ForeArm')else .5*t)
                return min(1.,value)
            view_reach[name]=np.array([reach(v)for v in ob.data.vertices])
        else:view_reach[name]=np.full(len(ob.data.vertices),float(name=='Sword'))
    topology = {}
    def source_sample(frame, reference=False, collapse=False):
        scene.frame_set(math.floor(frame), subframe=frame % 1.)
        bpy.context.view_layer.update()
        graph = bpy.context.evaluated_depsgraph_get()
        a = arm.evaluated_get(graph)
        hip = a.matrix_world @ a.pose.bones['Hips'].head
        positions, normals, texcoords, triangles = [], [], [], []
        offset = 0
        body_min = 1.e10
        body_max = -1.e10
        for name in NAMES:
            e = bpy.data.objects[name].evaluated_get(graph)
            m = e.to_mesh()
            m.calc_loop_triangles()
            m.calc_normals_split()
            nmatrix = e.matrix_world.to_3x3().inverted().transposed()
            source_positions = np.asarray([tuple(e.matrix_world @ v.co) for v in m.vertices])
            source_normals = np.asarray([tuple((nmatrix @ n.vector).normalized()) for n in m.corner_normals])
            # Skinning may collapse a tiny source face at a bent joint. Repair
            # only undefined corner normals, using the actual posed triangle;
            # a zero-area triangle falls back to its authored undeformed normal.
            for tri in m.loop_triangles:
                bad=[i for i in tri.loops if np.dot(source_normals[i],source_normals[i])<.5]
                if not bad:continue
                va,vb,vc=[source_positions[i]for i in tri.vertices];normal=np.cross(vb-va,vc-va);length=np.linalg.norm(normal)
                if length>1e-12:normal/=length
                else:normal=np.asarray(tuple((nmatrix@bpy.data.objects[name].data.loops[tri.loops[0]].normal).normalized()))
                if np.dot(normal,normal)<.5:normal=np.array([0.,0.,1.])
                for i in bad:source_normals[i]=normal
            original_uv = np.asarray([tuple(x.uv) for x in m.uv_layers.active.data])
            if name in ['Knight', 'Helm']:
                body_min = min(body_min, float(source_positions[:, 2].min()))
                body_max = max(body_max, float(source_positions[:, 2].max()))
            actual_triangles = np.asarray([list(t.loops) for t in m.loop_triangles], dtype=np.int64)
            if reference:
                mapping, representatives, remap = {}, [], []
                for i, loop in enumerate(m.loops):
                    key = (loop.vertex_index, *np.round(original_uv[i], 8), *np.round(source_normals[i], 6))
                    if key not in mapping:
                        mapping[key] = len(representatives)
                        representatives.append(i)
                    remap.append(mapping[key])
                topology[name] = dict(representatives=np.asarray(representatives),
                    raw_vertices=np.asarray([m.loops[i].vertex_index for i in representatives]),
                    remap=np.asarray(remap), triangles=actual_triangles, vertices=len(m.vertices), loops=len(m.loops))
            fixed = topology[name]
            assert len(m.vertices) == fixed['vertices'] and len(m.loops) == fixed['loops']
            assert np.array_equal(actual_triangles, fixed['triangles'])
            positions.append(source_positions[fixed['raw_vertices']])
            normals.append(source_normals[fixed['representatives']])
            uvs = original_uv[fixed['representatives']].copy()
            uvs[:, 1] = 1. - uvs[:, 1]
            texcoords.append(uvs)
            triangles.append(fixed['remap'][actual_triangles] + offset)
            offset += len(fixed['representatives'])
            e.to_mesh_clear()
        p, n, uv, indices = map(np.concatenate, [positions, normals, texcoords, triangles])
        origin=np.array([hip.x,hip.y,body_min]);lift=0.
        if polish:
            if reference:polish.reference_origin=origin.copy();polish.reference_hip=(a.matrix_world@a.pose.bones['Hips'].head).copy()
            if collapse:
                origin=polish.reference_origin.copy()
                lift=max(0.,float(origin[2]-p[:,2].min()))
            polish.current_origin=origin.copy();polish.floor_lift=lift
        p-=origin;p[:,2]+=lift
        hand=np.asarray(tuple(a.matrix_world@a.pose.bones['RightHand'].head));hand-=origin;hand[2]+=lift
        return p @ basis.T, n @ basis.T, uv, indices, body_max - body_min,hand@basis.T
    assign(neutral)
    p,n,uv,indices,height,hand=source_sample(1,True);scale=HEIGHT/height
    held_guard_records=[]
    if HELD_GUARDS_ONLY:
        assert LONGSWORD and not SOCKETS_ONLY,'Held guard derivatives require the owned longsword rig'
        # The purchased recoil clips are reactions, not readable held guards.
        # Author five stationary two-hand holds in front of the torso. Targets
        # use the cooked actor basis (+Z forward, +X the actor's left).
        old_set_clip=set_clip
        assign(neutral)
        neutral_right=world@arm.pose.bones['RightHand'].head
        neutral_wrist=(world@arm.pose.bones['RightHand'].matrix).to_quaternion()
        neutral_origin=Vector((neutral_hip.x,neutral_hip.y,neutral_right.z-hand[1]))
        ev=bpy.data.objects['Sword'].evaluated_get(bpy.context.evaluated_depsgraph_get());mesh=ev.to_mesh()
        points=np.asarray([tuple(ev.matrix_world@v.co)for v in mesh.vertices]);ev.to_mesh_clear()
        blade_axis=Vector(np.linalg.eigh(np.cov(points.T))[1][:,-1]);blade_axis*=1 if blade_axis.z>0 else -1
        held_rig_measurements={}
        for side in ['Right','Left']:
            bones=[world@arm.pose.bones[side+n].head for n in ['Arm','ForeArm','Hand']]
            held_rig_measurements[side]=dict(shoulder=((bones[0]-neutral_origin)*scale)[:],elbow=((bones[1]-neutral_origin)*scale)[:],hand=((bones[2]-neutral_origin)*scale)[:],reach_m=((bones[0]-bones[1]).length+(bones[1]-bones[2]).length)*scale)
        targets={
            'guard_left':((.12,1.40,.20),(.50,.85,.10)),
            'guard_right':((-.35,1.40,.25),(-.35,.88,.12)),
            'guard_high':((-.08,1.68,.20),(.94,.06,.18)),
            'guard_low':((-.10,1.18,.20),(.86,.05,.25)),
            'guard_thrust':((-.08,1.35,.27),(-.06,.92,.35)),
        }
        if '--held-guard-targets' in sys.argv:
            target_path=Path(sys.argv[sys.argv.index('--held-guard-targets')+1]).resolve()
            supplied=json.loads(target_path.read_text())
            assert set(supplied).issubset(targets) and supplied,'Unknown/empty held guard targets'
            for name,value in supplied.items():
                assert len(value)==2 and all(len(v)==3 for v in value)
                assert all(math.isfinite(float(x)) and abs(float(x))<3 for v in value for x in v)
                assert np.linalg.norm(value[1])>.01,'Degenerate held guard axis'
                targets[name]=tuple(tuple(float(x) for x in v) for v in value)
        held_measurements=[]
        def set_clip(name,t):
            if name not in targets:return old_set_clip(name,t)
            assign(neutral)
            target,direction=targets[name];amount=smooth(t)
            goal=neutral_origin+Vector((target[0],-target[2],target[1]))/scale
            desired=Vector((direction[0],-direction[2],direction[1])).normalized()
            delta=Quaternion((1,0,0,0)).slerp(blade_axis.rotation_difference(desired),amount)
            actual_goal=neutral_right.lerp(goal,amount)
            ik('RightArm','RightForeArm','RightHand',actual_goal)
            wrist=arm.pose.bones['RightHand']
            rotation=world.to_quaternion().inverted()@delta@neutral_wrist
            wrist.matrix=Matrix.LocRotScale(wrist.head,rotation,wrist.matrix.to_scale());bpy.context.view_layer.update()
            plant();mocap.constrain_grip(ik)
            r=world@arm.pose.bones['RightHand'].matrix;l=world@arm.pose.bones['LeftHand'].matrix
            held_measurements.append(dict(clip=name,sample=t,
                right_hand_target_error_m=(r.translation-actual_goal).length*scale,
                left_hand_grip_error_m=(l.translation-r@mocap.grip[0]).length*scale,
                ankle_error_m=maximum_ankle_error*scale))
    arm_vertices=np.concatenate([masks[name][topology[name]['raw_vertices']]for name in NAMES])
    selected=indices[np.all(arm_vertices[indices],axis=1)];arms_map=np.unique(selected);remap=np.full(len(p),-1);remap[arms_map]=np.arange(len(arms_map));arms_indices=remap[selected]
    arm_reach=np.concatenate([view_reach[name][topology[name]['raw_vertices']]for name in NAMES])[arms_map]
    sign=tangents(p,n,uv,indices)[:,3];arm_sign=sign[arms_map]
    assert len(arms_indices)>100 and len(arms_map)<len(p)*.7
    sockets=[]
    sword_start=sum(len(topology[n]['representatives'])for n in NAMES[:NAMES.index('Sword')]);sword_end=sword_start+len(topology['Sword']['representatives'])
    entries=[];clips=[];slot=0;OUT.mkdir(parents=True,exist_ok=True);minimum=1e9;maximum=-1e9
    for name,count in CLIPS+(EXTRA_CLIPS if polish else []):
        clips.append(dict(name=name,base=slot,count=count))
        for i in range(count):
            if polish and slot<108:
                polish.preserve(slot,entries);slot+=1;continue
            if HELD_GUARDS_ONLY and not name.startswith('guard_'):
                slot+=1;continue
            # Non-looping clips include both endpoints. The gait samples one
            # cyclic source period without duplicating its terminal frame.
            t=i/count if name in ['walk'] else i/(count-1)
            set_clip(name,t);p,n,actual_uv,actual_indices,_,hand=source_sample(scene.frame_current+scene.frame_subframe,collapse=name=='death_back')
            p*=scale;assert np.array_equal(actual_uv,uv)and np.array_equal(actual_indices,indices)
            assert np.isfinite(p).all()
            if name=='death_back':assert float(p[:,1].min())>=-1e-5
            else:assert abs(float(p[:,1].min()))<1e-5
            if polish:
                right=world@arm.pose.bones['RightHand'].matrix;left=world@arm.pose.bones['LeftHand'].matrix
                grip_error=(left.translation-right@mocap.grip[0]).length
                polish.record(name,t,slot,p,n,indices,hand,sword_start,sword_end,arm,world,basis,scale,maximum_ankle_error,grip_error)
            if HELD_GUARDS_ONLY:
                sword_points=p[sword_start:sword_end];center=sword_points.mean(axis=0);principal=np.linalg.eigh(np.cov(sword_points.T))[1][:,-1];along=(sword_points-center)@principal
                ends=[sword_points[along<=along.min()+.008].mean(axis=0),sword_points[along>=along.max()-.008].mean(axis=0)];grip=hand*scale
                tip=max(ends,key=lambda v:np.linalg.norm(v-grip));base=grip+(tip-grip)*.18
                held_guard_records.append(dict(slot=slot,clip=name,sample=i,base=base.tolist(),tip=tip.tolist(),hand=grip.tolist(),sword_vertex_range=[sword_start,sword_end]))
            if SOCKETS_ONLY:
                import struct
                original=ROOT/'projects/valwood_world/models/longsword_mocap'/f'combat_body_{slot:03}.glb'
                raw=original.read_bytes();jl=struct.unpack_from('<I',raw,12)[0];meta=json.loads(raw[20:20+jl]);acc=meta['accessors'][meta['meshes'][0]['primitives'][0]['attributes']['POSITION']];view=meta['bufferViews'][acc['bufferView']];offset=20+jl+8+view.get('byteOffset',0)+acc.get('byteOffset',0)
                cooked=np.frombuffer(raw,dtype='<f4',count=acc['count']*3,offset=offset).reshape(-1,3)
                assert cooked.shape==p.shape and float(np.max(np.abs(cooked-p)))<.00005,(slot,'baked weapon source differs')
                sword_points=cooked[sword_start:sword_end];center=sword_points.mean(axis=0);axis=np.linalg.eigh(np.cov(sword_points.T))[1][:,-1];along=(sword_points-center)@axis
                ends=[sword_points[along<=along.min()+.008].mean(axis=0),sword_points[along>=along.max()-.008].mean(axis=0)];grip=hand*scale
                tip=max(ends,key=lambda v:np.linalg.norm(v-grip));base=grip+(tip-grip)*.18
                sockets.append(dict(base=base.tolist(),tip=tip.tolist(),geometry_sha256=hashlib.sha256(raw).hexdigest()))
                slot+=1;continue
            for kind,vertices,triangles,signs in [('body',None,indices,sign),('arms',arms_map,arms_indices,arm_sign)]:
                q=p if vertices is None else p[vertices];normal=n if vertices is None else n[vertices];coords=uv if vertices is None else uv[vertices]
                if LONGSWORD and kind=='arms':
                    q=q+arm_reach[:,None]*np.array([.25,0.,.65]);normal=normal.copy()
                    # Rebuild normals only on stretched sleeve vertices. Hands
                    # and the rigid translated sword keep their original basis.
                    rebuilt=np.zeros_like(normal);corners=q[triangles]
                    face=np.cross(corners[:,1]-corners[:,0],corners[:,2]-corners[:,0])
                    for corner in range(3):np.add.at(rebuilt,triangles[:,corner],face)
                    lengths=np.linalg.norm(rebuilt,axis=1);valid=(lengths>1e-10)&(arm_reach<.999999)
                    normal[valid]=rebuilt[valid]/lengths[valid,None]
                tg=tangents(q,normal,coords,triangles,signs)
                path=OUT/f'combat_{kind}_{slot:03}.glb';write_glb(path,(q,normal,coords,tg,triangles),f'Owned knight {name} {i} {kind}')
                if polish:polish.repack(path,path)
                entries.append(dict(file=path.name,clip=name,sample=i,kind=kind,vertices=len(q),triangles=len(triangles),bytes=path.stat().st_size,bounds_min=q.min(axis=0).tolist(),bounds_max=q.max(axis=0).tolist()))
            print('COMBAT_POSE',slot,name,i,flush=True);slot+=1
    if SOCKETS_ONLY:
        (OUT/'weapon_sockets.json').write_text(json.dumps(dict(poses=sockets,source_geometry='Original cooked longsword meshes; each pose verified against regenerated source vertices within 0.05 mm'),indent=2)+'\n')
        lines=['// Generated from verified purchased longsword pose geometry. Metres, actor local.','static constexpr float combat_weapon_sockets[108][2][3]={']
        for entry in sockets:lines.append('    {{'+','.join(f'{v:.9f}f'for v in entry['base'])+'},{'+','.join(f'{v:.9f}f'for v in entry['tip'])+'}},')
        lines.append('};');(OUT/'world_combat_weapon_sockets.inc').write_text('\n'.join(lines)+'\n');print('SOCKETS_OK',len(sockets));return
    if HELD_GUARDS_ONLY:
        (OUT/'held_guard_geometry.json').write_text(json.dumps(dict(schema_version=1,poses=held_guard_records,measurements=held_measurements,rig=held_rig_measurements,targets=targets,source_modified=False,source_scripts_executed=False),indent=2)+'\n')
    manifest=dict(schema_version=1,pose_count=slot,clips=clips,assets=entries,source=str(SOURCE/'Knight with shield.blend'),source_scripts_executed=False,source_modified=False,source_sha256=hashlib.sha256((SOURCE/'Knight with shield.blend').read_bytes()).hexdigest(),maximum_stationary_ankle_error_source_m=maximum_ankle_error,source_channel_repair='strip stale mixamorig prefix in memory',owned_textures_reused=True,first_person='Actual purchased arm/hand vertices, shield and sword; head and torso removed by source skin weights',authored_adaptations=['Low cut from reversed slash2 excerpt','Analytic right-arm thrust with rigid wrist orientation','Five shield guard offsets from real Block1','Two-bone ankle planting on stationary clips'],not_included='Kubold pack has not been supplied or used')
    if mocap:manifest.update(longsword_mocap=mocap.metadata(),weapon=weapon_metadata,first_person='Actual purchased upper sleeves/forearms/hands and purchased longsword; shield/head/torso omitted; sleeve reach adapted for the first-person camera',viewmodel_hand_translation_m=[.25,0.,.65],authored_adaptations=['Retargeted five captured directional attacks and genuine thrust','Source attack timing cropped into the gameplay windup/contact/recovery windows','Fixed two-hand grip and stationary ankle constraints','One35frame walk cycle from the three-cycle source','Viewmodel-only shoulder-to-wrist reach adaptation; rigid hands/weapon translation'],not_included='Full runtime skeletal IK is not part of this vertex-pose renderer')
    if polish:polish.finish(manifest)
    (OUT/'combat_motion_manifest.json').write_text(json.dumps(manifest,indent=2)+'\n');print('COMBAT_COOK_OK',slot,len(arms_map),len(arms_indices),flush=True)
if __name__=='__main__':main()
