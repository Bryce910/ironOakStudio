#!/usr/bin/env python3
"""Offline owned source rigs -> fixed-layout native road animation poses."""
import sys, json, math
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parent))
from road_asset_common import *
import bpy
from mathutils import Matrix, Quaternion, Vector

MODELS=OUT/'models';REVIEW=STAGE/'review/assets'
G=np.array([[1.,0,0],[0,0,1],[0,-1,0]]) # Blender Z-up/-Y-forward -> native Y-up/+Z.

def frame(value):
    bpy.context.scene.frame_set(int(value),subframe=value-int(value));bpy.context.view_layer.update()

def reduce_bind(objects,rigs,ratio):
    for arm in rigs:arm.data.pose_position='REST'
    bpy.context.view_layer.update()
    for o in objects:
        if o.data.shape_keys:
            for k in o.data.shape_keys.key_blocks:k.value=0
            o.shape_key_clear()
        bpy.ops.object.select_all(action='DESELECT');o.select_set(True);bpy.context.view_layer.objects.active=o
        for a in list(o.data.color_attributes):o.data.color_attributes.remove(a)
        dec=o.modifiers.new('Road offline fixed bind reduction','DECIMATE');dec.ratio=ratio;dec.use_collapse_triangulate=True
        while o.modifiers.find(dec.name)>0:bpy.ops.object.modifier_move_up(modifier=dec.name)
        bpy.ops.object.modifier_apply(modifier=dec.name)
        tri=o.modifiers.new('Fixed source triangulation','TRIANGULATE')
        while o.modifiers.find(tri.name)>0:bpy.ops.object.modifier_move_up(modifier=tri.name)
        bpy.ops.object.modifier_apply(modifier=tri.name)
    for arm in rigs:arm.data.pose_position='POSE'
    bpy.context.view_layer.update()

class Capture:
    def __init__(self,objects,materials,uv_transform=None):
        self.objects=objects;self.materials=materials;self.uv_transform=uv_transform;self.layouts={};self.signs={}
    def posed(self,adjust=lambda o,p:p):
        graph=bpy.context.evaluated_depsgraph_get();groups={}
        for o in self.objects:
            ev=o.evaluated_get(graph);mesh=ev.to_mesh(preserve_all_data_layers=True,depsgraph=graph)
            mesh.calc_loop_triangles();mesh.calc_normals_split()
            vi=np.array([l.vertex_index for l in mesh.loops],dtype=np.int32)
            normals=np.array([l.normal[:] for l in mesh.loops],dtype=float)
            coords=np.array([v.co[:] for v in mesh.vertices],dtype=float)
            mx=np.array(ev.matrix_world);coords=coords@mx[:3,:3].T+mx[:3,3]
            normals=normals@np.linalg.inv(mx[:3,:3]);length=np.linalg.norm(normals,axis=1)
            if np.any(length<1e-8):
                for t in mesh.loop_triangles:
                    ls=np.array(t.loops);n=np.cross(coords[vi[ls[1]]]-coords[vi[ls[0]]],coords[vi[ls[2]]]-coords[vi[ls[0]]]);ln=np.linalg.norm(n)
                    if ln>1e-10:
                        for i in ls:
                            if length[i]<1e-8:normals[i]=n/ln;length[i]=1
            assert np.all(length>1e-8),(o.name,'zero genuine source normals')
            normals/=length[:,None]
            if o.name not in self.layouts:
                loops=np.array([i for t in mesh.loop_triangles for i in t.loops],dtype=np.int32)
                uv=np.array([x.uv[:] for x in mesh.uv_layers.active.data],dtype=float)
                if self.uv_transform:
                    for poly in mesh.polygons:
                        mat=mesh.materials[poly.material_index].name.split('.')[0]
                        uv[list(poly.loop_indices)]=self.uv_transform(mat,uv[list(poly.loop_indices)])
                uv[:,1]=1-uv[:,1]
                key=np.column_stack((vi[loops],np.round(normals[loops],5),np.round(uv[loops],7)))
                _,first,ix=np.unique(key,axis=0,return_index=True,return_inverse=True)
                chosen=loops[first]
                self.layouts[o.name]={'chosen':chosen,'vi':vi[chosen],'uv':uv[chosen].astype('<f4'),'ix':ix.reshape(-1,3)}
            l=self.layouts[o.name];p=adjust(o,coords[l['vi']]);n=normals[l['chosen']]
            p=p@G.T;n=n@G.T;n/=np.linalg.norm(n,axis=1)[:,None]
            group=self.materials[o.name];groups.setdefault(group,[]).append((p,n,l['uv'],l['ix']))
            ev.to_mesh_clear()
        out=[]
        for mat,parts in sorted(groups.items()):
            off=0;indices=[]
            for p,n,u,i in parts:indices.append(i+off);off+=len(p)
            p=np.concatenate([x[0]for x in parts]).astype('<f4');n=np.concatenate([x[1]for x in parts]).astype('<f4');uv=np.concatenate([x[2]for x in parts]);ix=np.concatenate(indices).astype('<u4')
            t=tangents(p,n,uv,ix,self.signs.get(mat))
            self.signs.setdefault(mat,t[:,3].copy());out.append({'p':p,'n':n,'uv':uv,'ix':ix,'t':t,'material':mat})
        return out

def mounted():
    sys.path.insert(0,str(ROOT/'tools'))
    import import_village_horseman as legacy
    bpy.ops.wm.read_factory_settings(use_empty=True)
    source=ROOT/'assets/purchased/horseman/source/Export/FBX';objects=[];rigs=[]
    for name in legacy.FBXS:
        old=set(bpy.data.objects);bpy.ops.import_scene.fbx(filepath=str(source/name))
        new=[o for o in bpy.data.objects if o not in old];rigs.extend(o for o in new if o.type=='ARMATURE')
        objects.extend(o for o in new if o.type=='MESH' and(name==legacy.FBXS[0]or o.name in legacy.RIDER_PARTS))
    assert len(objects)==24 and len(rigs)==2
    horse=next(a for a in rigs if a.name.startswith('horse'));rider=next(a for a in rigs if not a.name.startswith('horse'))
    horse_objects={o.name for o in objects if any(m.type=='ARMATURE'and m.object==horse for m in o.modifiers)}
    reduce_bind(objects,rigs,.30)
    frame(1);graph=bpy.context.evaluated_depsgraph_get()
    points=np.array([o.matrix_world@v.co for o in objects for v in o.evaluated_get(graph).data.vertices])
    scale=3.12/(points[:,2].max()-points[:,2].min());floor=points[:,2].min()
    def uvmap(mat,uv):
        x,y,tile=legacy.RECIPES[legacy.MATERIALS[mat]][4]
        return (np.clip(uv,0,1)*(tile-8)+np.array([x+4,y+4]))/np.array([2048.,1024.])
    cap=Capture(objects,{o.name:0 for o in objects},uvmap)
    pack=json.loads((OUT/'mounted_materials.json').read_text());rows={'walk':[],'idle':[]};measure=[];cooked=[]
    def sample(mode,phase):
        # Both source loops import one frame after their Unity frame-zero origin.
        # Preserve each source rig's authored vertical gait and relative seat.
        # The supplied horse idle is not an exact endpoint loop (17 mm gap).
        # Smooth source-only out/back sampling closes it without a rig snap.
        idle_phase=.5-.5*math.cos(math.tau*phase)
        hf=1404+34*phase if mode=='walk'else 3+98*idle_phase
        rf=477+34*phase if mode=='walk'else 47+100*phase
        frame(hf)
        # Evaluating a separate rig at its own source time requires saving its
        # evaluated bone/object state before changing the shared scene clock.
        hm={b.name:b.matrix_basis.copy()for b in horse.pose.bones};hworld=horse.matrix_world.copy()
        hlocal=horse.matrix_basis.copy();root_xy=np.array(horse.matrix_world.translation)[:2].copy() if mode=='walk'else np.zeros(2)
        frame(rf)
        # Freeze the exact sampled horse action for this CPU evaluation only.
        ha=horse.animation_data.action;horse.animation_data.action=None
        horse.matrix_basis=hlocal
        for b in horse.pose.bones:b.matrix_basis=hm[b.name]
        bpy.context.view_layer.update()
        def adjust(o,p):
            p=p.copy()
            if o.name in horse_objects:p[:,:2]-=root_xy
            p[:,2]-=floor;return p*scale
        q=cap.posed(adjust)
        horse.animation_data.action=ha
        return q,hf,rf
    endpoints={}
    for mode,count in [('walk',16),('idle',4)]:
        for i in range(count):
            q,hf,rf=sample(mode,i/count);r=write_glb(MODELS/f'mounted_{mode}_{i:02}.glb',q,pack,{'source_horse_frame':hf,'source_rider_frame':rf,'source_phase':i/count})
            r.update(source_horse_frame=hf,source_rider_frame=rf,phase=i/count);rows[mode].append(r)
            cooked.append((mode,i,q,hf,rf))
            measure.append({'mode':mode,'phase':i/count,'min_floor_m':r['bounds'][0][1],'height_m':r['bounds'][1][1]-r['bounds'][0][1]})
            print('MOUNTED',mode,i,r['triangles'],r['vertices'],flush=True)
        a,_,_=sample(mode,0);b,_,_=sample(mode,1)
        endpoints[mode]=max(float(np.max(np.linalg.norm(x['p']-y['p'],axis=1)))for x,y in zip(a,b))
    assert max(endpoints.values())<.004,endpoints
    # One measured origin shared by BOTH complete clips. No per-pose vertical
    # normalization: the genuine hoof heights and rider bounce remain intact.
    bank_min=min(x['bounds'][0][1]for v in rows.values()for x in v)
    floor+=bank_min/scale
    for mode,i,q,hf,rf in cooked:
        for primitive in q:primitive['p'][:,1]-=bank_min
        r=write_glb(MODELS/f'mounted_{mode}_{i:02}.glb',q,pack,{'source_horse_frame':hf,'source_rider_frame':rf,'source_phase':i/(16 if mode=='walk'else 4),'constant_bank_ground_shift_m':-bank_min})
        r.update(source_horse_frame=hf,source_rider_frame=rf,phase=i/(16 if mode=='walk'else 4));rows[mode][i]=r
    for metric in measure:metric['min_floor_m']-=bank_min
    assert all(x['triangles']<25000 and x['vertices']<100000 for v in rows.values()for x in v)
    assert len({r['layout_sha256']for v in rows.values()for r in v})==1
    result={**rows,'stride_m':1.2483881711959839*scale,'source_stride_m':1.2483881711959839,'cycle_seconds':34/30,'idle_seconds':100/30,'forward':'+Z','source_scale':scale,'fixed_floor_source':floor,'loop_endpoint_max_m':endpoints,'source_clips':{'horse_walk_imported':[1404,1438],'rider_walk_imported':[477,511],'horse_idle_imported':[3,101],'horse_idle_phase':'smooth cosine source-only out/back; original open endpoint 17mm','rider_idle_imported':[47,147],'fps':30},'bounds':[np.min([r['bounds'][0]for v in rows.values()for r in v],axis=0).tolist(),np.max([r['bounds'][1]for v in rows.values()for r in v],axis=0).tolist()],'source_files':{str(source/n):sha(source/n)for n in legacy.FBXS},'contact_measurements':measure}
    (OUT/'MOUNTED_ASSETS.json').write_text(json.dumps(result,indent=2)+'\n')

def puller():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    source=ROOT/'assets/purchased/characters/source/NPC_Character_04/Mesh_UE4/Full/SK_NPC_Character_04_Full_01.FBX'
    bpy.ops.import_scene.fbx(filepath=str(source),automatic_bone_orientation=True)
    objects=[o for o in bpy.context.scene.objects if o.type=='MESH'];arm=next(o for o in bpy.context.scene.objects if o.type=='ARMATURE')
    assert not bpy.data.actions
    reduce_bind(objects,[arm],.24)
    bones=arm.pose.bones;world=arm.matrix_world.copy();inverse=world.inverted()
    def update():bpy.context.view_layer.update()
    def head(name):return world@bones[name].head
    def aim(name,direction):
        b=bones[name];mx=world@b.matrix;start=head(name);current=world@b.tail-start
        delta=current.normalized().rotation_difference(Vector(direction).normalized());l,r,s=mx.decompose();b.matrix=inverse@Matrix.LocRotScale(l,delta@r,s);update()
    def rotate(name,axis,angle):
        mx=world@bones[name].matrix;l,r,s=mx.decompose();bones[name].matrix=inverse@Matrix.LocRotScale(l,Quaternion(Vector(axis),angle)@r,s);update()
    for side,sign in [('l',1),('r',-1)]:aim('upperarm_'+side,(sign*.13,-.045,-1));aim('lowerarm_'+side,(sign*.025,-.14,-1))
    neutral={b.name:b.matrix_basis.copy()for b in bones};ankles={s:head('foot_'+s).copy()for s in ('l','r')};hip=head('pelvis').copy()
    neutral_feet={n:(world@bones[n].matrix).to_quaternion()for n in ('foot_l','foot_r','ball_l','ball_r')}
    graph=bpy.context.evaluated_depsgraph_get();pts=np.array([o.matrix_world@v.co for o in objects for v in o.evaluated_get(graph).data.vertices]);low=pts[:,2].min();scale=1.82/(pts[:,2].max()-low)
    foot_indices={}
    for o in objects:
        names={g.index:g.name for g in o.vertex_groups};foot_indices[o.name]={s:[v.index for v in o.data.vertices if sum(g.weight for g in v.groups if names.get(g.group) in('foot_'+s,'ball_'+s))>.4]for s in ('l','r')}
    def floor_values():
        graph=bpy.context.evaluated_depsgraph_get();out={s:[]for s in ('l','r')}
        for o in objects:
            coords=o.evaluated_get(graph).data.vertices
            for s,indices in foot_indices[o.name].items():out[s].extend((o.matrix_world@coords[i].co).z for i in indices)
        return {s:min(x)for s,x in out.items()}
    def solve(a,b,c,target,pole):
        start=head(a);middle=head(b);end=head(c);l1=(middle-start).length;l2=(end-middle).length
        v=target-start;dist=v.length;assert dist<l1+l2-.00001,(a,'unreachable',dist,l1+l2)
        axis=v.normalized();along=(l1*l1-l2*l2+dist*dist)/(2*dist);bend=Vector(pole);bend=(bend-axis*bend.dot(axis)).normalized()
        elbow=start+axis*along+bend*math.sqrt(max(0,l1*l1-along*along));aim(a,elbow-start);aim(b,target-head(b))
    cart=json.loads((OUT/'CART_ASSETS.json').read_text());grips=np.array(cart['grips_local']);grips=grips[np.argsort(-grips[:,0])]
    # Native+X is source left. Hand targets are actual source shaft tip centers.
    source_grips={s:Vector((g[0]/scale+hip.x,-g[2]/scale+hip.y,g[1]/scale+low))for s,g in zip(('l','r'),grips)}
    pack=json.loads((OUT/'puller_materials.json').read_text());names=[m.get('name','')for m in pack['gltf']['materials']]
    bodymat=next(i for i,n in enumerate(names)if'body04'in n);headmat=next(i for i,n in enumerate(names)if'head'in n)
    materials={o.name:headmat if any(s in o.name for s in ('Head','Hands','Eyes')) else bodymat for o in objects}
    cap=Capture(objects,materials);rows={'walk':[],'idle':[]};metrics=[]
    def sample(mode,fraction):
        for b in bones:b.matrix_basis=neutral[b.name].copy()
        update();phase=math.tau*fraction
        if mode=='walk':
            pelvis=world@bones['pelvis'].matrix;pelvis.translation.z-=.055+.008*math.cos(phase*2);bones['pelvis'].matrix=inverse@pelvis;update()
            rotate('pelvis',(0,0,1),.035*math.sin(phase));rotate('spine_02',(0,0,1),-.025*math.sin(phase))
            for s,off in [('l',0),('r',math.pi)]:
                t=ankles[s].copy();t.y-=.245*math.cos(phase+off);t.z+=.075*max(0,-math.sin(phase+off));solve('thigh_'+s,'calf_'+s,'foot_'+s,t,(0,-1,0))
                for name in ('foot_'+s,'ball_'+s):
                    mx=world@bones[name].matrix;l,r,sc=mx.decompose();bones[name].matrix=inverse@Matrix.LocRotScale(l,neutral_feet[name],sc);update()
        else:rotate('spine_02',(1,0,0),.006*math.sin(phase))
        rotate('spine_02',(1,0,0),.055)
        floors=floor_values();shift=low-min(floors.values());mx=world@bones['pelvis'].matrix;mx.translation.z+=shift;bones['pelvis'].matrix=inverse@mx;update()
        residual=[]
        for s,sign in [('l',1),('r',-1)]:
            # Orient the actual palm/fingers downward around the shaft; then
            # solve the original two-bone arm, retaining its measured lengths.
            aim('hand_'+s,(0,0,-1))
            palm=head('middle_01_'+s)*.5+head('ring_01_'+s)*.5;offset=palm-head('hand_'+s)
            target=source_grips[s]-offset
            solve('upperarm_'+s,'lowerarm_'+s,'hand_'+s,target,(sign*.5,.8,-.1))
            aim('hand_'+s,(0,0,-1))
            # Recompute after hand orientation; one exact translation-free IK
            # iteration compensates the palm offset without stretching bones.
            palm=head('middle_01_'+s)*.5+head('ring_01_'+s)*.5;offset=palm-head('hand_'+s)
            solve('upperarm_'+s,'lowerarm_'+s,'hand_'+s,source_grips[s]-offset,(sign*.5,.8,-.1));aim('hand_'+s,(0,0,-1))
            palm=head('middle_01_'+s)*.5+head('ring_01_'+s)*.5;residual.append((palm-source_grips[s]).length*scale)
            for finger in ('index','middle','ring','pinky'):
                for j,angle in ((1,.35),(2,.75),(3,.55)):
                    name=f'{finger}_{j:02}_{s}'
                    if name in bones:rotate(name,(sign,0,0),angle)
        def adjust(o,p):return (p-np.array([hip.x,hip.y,low]))*scale
        q=cap.posed(adjust);feet=floor_values()
        return q,{'hand_grip_error_m':residual,'feet_height_m':{s:(h-low)*scale for s,h in feet.items()},'pelvis_vertical_ground_correction_m':shift*scale}
    endpoints={}
    for mode,count in [('walk',8),('idle',4)]:
        for i in range(count):
            q,metric=sample(mode,i/count);r=write_glb(MODELS/f'puller_{mode}_{i:02}.glb',q,pack,{'owned_source_family':4,'authored_grip_constrained_gait':True,'phase':i/count})
            r['phase']=i/count;rows[mode].append(r);metrics.append({'mode':mode,'phase':i/count,**metric});print('PULLER',mode,i,r['triangles'],metric,flush=True)
        a,_=sample(mode,0);b,_=sample(mode,1);endpoints[mode]=max(float(np.max(np.linalg.norm(x['p']-y['p'],axis=1)))for x,y in zip(a,b))
    assert max(endpoints.values())<.001
    assert max(max(x['hand_grip_error_m'])for x in metrics)<.004,metrics
    assert len({x['layout_sha256']for v in rows.values()for x in v})==1
    result={**rows,'stride_m':.9811,'cycle_seconds':.9,'idle_seconds':3.,'forward':'+Z','grips_local':grips.tolist(),'cart_local_offset':cart['cart_local_offset'],'body_height_m':1.82,'loop_endpoint_max_m':endpoints,'contact_measurements':metrics,'source_files':{str(source):sha(source)},'method':'Owned family04 original skinweights; retained authored two-bone grounded gait with actual arm reach and fixed shaft-tip palm constraints. This is an authored variation, not a supplied cart-pull clip.','bounds':[np.min([r['bounds'][0]for v in rows.values()for r in v],axis=0).tolist(),np.max([r['bounds'][1]for v in rows.values()for r in v],axis=0).tolist()]}
    (OUT/'PULLER_ASSETS.json').write_text(json.dumps(result,indent=2)+'\n')

if __name__=='__main__':
    selected=sys.argv[sys.argv.index('--')+1:]if'--'in sys.argv else ['mounted','puller']
    if'mounted'in selected:mounted()
    if'puller'in selected:puller()
