"""Bake the actual owned PowerOrc14 FBXs into a separate opponent-only bank.

Blender background only. The original archives and FBXs remain read-only.
Generated body geometry, anatomical exchange and weapon measurements share one
pose boundary. Material fallback is explicitly provisional until owned maps land.
"""
from pathlib import Path
import sys,json,math,struct,hashlib
import bpy,bmesh,numpy as np
from mathutils import Matrix,Vector,Quaternion

STAGE=Path(__file__).resolve().parent
SOURCE=STAGE/'orc-intake/powerorc'
OUT=STAGE/'arena_orcs/body'
EXCHANGE=STAGE/'arena_orcs/body-semantic.npz'
ENGINE=STAGE.parents[1]
sys.path.insert(0,str(ENGINE/'tools'))
sys.path.insert(0,str(STAGE))
from import_village_shield_knight import tangents
from roster_guard_fit import fit as fit_guards

CLIPS=[('idle',4),('walk',8),('left',12),('right',12),('high',12),('low',12),('thrust',12),
       ('guard_left',4),('guard_right',4),('guard_high',4),('guard_low',4),('guard_thrust',4),
       ('hit',8),('parry',8),('hit_left',8),('hit_right',8),('hit_high',8),('hit_low',8),('hit_thrust',8),('death_back',24)]
NAMES=['Power_Orc3_1','Arm_Armors','Belt','Helm','Left_Shoulder_Armor','Legs_Armor','Weapon']
SOURCE_CLIPS=['Idle2','Walk','Attack1','Attack3','Attack6','Attack7','Attack5W','GetHit1','GetHit2','GetHit3','Death1']
ATTACKS={'left':'Attack3','right':'Attack6','high':'Attack1','low':'Attack7','thrust':'Attack5W'}
BASIS=np.asarray([[1.,0.,0.],[0.,0.,1.],[0.,-1.,0.]])
HEIGHT=2.1

def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def smooth(t):t=max(0.,min(1.,t));return t*t*(3.-2.*t)
def jsonwrite(path,data):path.parent.mkdir(parents=True,exist_ok=True);path.write_text(json.dumps(data,indent=2,allow_nan=False)+'\n')
def reset():bpy.ops.wm.read_factory_settings(use_empty=True);bpy.context.preferences.filepaths.use_scripts_auto_execute=False
def snapshots(path):
    cache=STAGE/'orc-intake/pose-source-cache';cache.mkdir(exist_ok=True)
    array_file=cache/(path.stem+'.npy');meta_file=cache/(path.stem+'.json')
    if array_file.exists() and meta_file.exists():
        meta=json.loads(meta_file.read_text())
        if meta.get('source_sha256')==sha(path) and meta.get('bone_names')==BONE_NAMES:
            data=np.load(array_file,allow_pickle=False)
            print('SOURCE_CACHE_VERIFIED',path.name,flush=True)
            return [[Matrix(m)for m in row]for row in data],meta['info']
    before=set(bpy.data.objects);before_actions=set(bpy.data.actions)
    bpy.ops.import_scene.fbx(filepath=str(path),use_anim=True,use_image_search=False)
    added=set(bpy.data.objects)-before
    arm=next(o for o in added if o.type=='ARMATURE')
    action=max((a for a in set(bpy.data.actions)-before_actions),key=lambda a:sum('pose.bones['in f.data_path for f in a.fcurves))
    assert sum('pose.bones['in f.data_path for f in action.fcurves)>100
    arm.animation_data.action=action
    for tr in arm.animation_data.nla_tracks:tr.mute=True
    frames=[];lo,hi=map(float,action.frame_range)
    assert list(arm.data.bones.keys())==BONE_NAMES,'Source skeleton semantics changed'
    for t in np.linspace(0.,1.,121):
        f=lo+(hi-lo)*float(t);bpy.context.scene.frame_set(math.floor(f),subframe=f%1);bpy.context.view_layer.update()
        frames.append([arm.matrix_world@arm.pose.bones[n].matrix for n in BONE_NAMES])
    info={'source':str(path),'source_sha256':sha(path),'frames':[lo,hi],'fps':bpy.context.scene.render.fps,'samples':121,'action':action.name}
    np.save(array_file,np.asarray(frames,dtype=np.float32));jsonwrite(meta_file,{'source_sha256':sha(path),'bone_names':BONE_NAMES,'info':info})
    for o in added:bpy.data.objects.remove(o,do_unlink=True)
    for a in set(bpy.data.actions)-before_actions:
        if not a.users:bpy.data.actions.remove(a)
    print('SOURCE_SAMPLED',path.name,flush=True)
    return frames,info

def blend_pose(a,b,t):
    if t<=0:return [m.copy()for m in a]
    if t>=1:return [m.copy()for m in b]
    result=[]
    for i,(ax,bx)in enumerate(zip(a,b)):
        parent=PARENT_INDEX[i]
        x=a[parent].inverted()@ax if parent>=0 else ax
        y=b[parent].inverted()@bx if parent>=0 else bx
        xp,xq,xs=x.decompose();yp,yq,ys=y.decompose()
        local=Matrix.LocRotScale(xp.lerp(yp,t),xq.slerp(yq,t),xs.lerp(ys,t))
        result.append(result[parent]@local if parent>=0 else local)
    return result
def sample(name,t):
    frames=CAPTURES[name];f=max(0.,min(1.,t))*120;i=min(119,int(f))
    return blend_pose(frames[i],frames[i+1],f-i)
def assign(pose):
    inv=ARM.matrix_world.inverted()
    local=[inv@m for m in pose]
    for i,n in enumerate(BONE_NAMES):
        b=ARM.data.bones[n];kw={}
        if b.parent:kw=dict(parent_matrix=local[BONE_INDEX[b.parent.name]],parent_matrix_local=b.parent.matrix_local)
        ARM.pose.bones[n].matrix_basis=b.convert_local_to_pose(local[i],b.matrix_local,invert=True,**kw)
    bpy.context.view_layer.update()
    error=max(abs((ARM.matrix_world@ARM.pose.bones[n].matrix)[r][c]-pose[i][r][c])for i,n in enumerate(BONE_NAMES)for r in range(4)for c in range(4))
    assert error<.0001,('Pose basis mismatch',error)
    return error
def with_weapon(pose):
    pose=list(pose);pose[BONE_INDEX['Weapon1']]=pose[BONE_INDEX['hand_r']]@AXE_HAND_LOCAL
    return pose
def semantic(name):
    if name.startswith('ik_') or name=='Weapon1':return None
    if name in ['head','neck_01']:return 0
    if name=='pelvis' or name.startswith(('spine_','clavicle_')):return 1
    if name.startswith(('hand_','index_','middle_','ring_','pinky_','thumb_')):return 4 if name.endswith('_l')else 5
    if name.startswith(('upperarm','lowerarm')):return 2 if name.endswith('_l')else 3
    if name.startswith(('thigh','calf','foot','ball')):return 6 if name.endswith('_l')else 7
    return 1

def ik_pose(pose,side,goal):
    """Two-bone adaptation on this actual source skeleton, fixed limb lengths."""
    pose=list(pose);names=[s+'_'+side for s in ['upperarm','lowerarm','hand']]
    ia,ib,ic=[BONE_INDEX[n]for n in names]
    a,b,c=[pose[i].translation.copy()for i in [ia,ib,ic]]
    u=(b-a).length;l=(c-b).length;v=goal-a;dist=min(max(v.length,abs(u-l)+1.e-5),u+l-1.e-5);d=v.normalized()
    pole=(b-a)-d*(b-a).dot(d)
    if pole.length<1.e-6:pole=Vector((1.,0.,0.))-d*d.x
    pole.normalize();along=(u*u-l*l+dist*dist)/(2*dist);new_b=a+d*along+pole*math.sqrt(max(0.,u*u-along*along));new_c=a+d*dist
    # Rotate the complete upper arm subtree, then complete lower arm subtree.
    def rotate_desc(root,delta,origin):
        for i,n in enumerate(BONE_NAMES):
            b=ARM.data.bones[n]
            if n==root or any(p.name==root for p in b.parent_recursive):
                m=pose[i];p,q,s=m.decompose();pose[i]=Matrix.LocRotScale(origin+delta@(p-origin),delta@q,s)
    rotate_desc(names[0],(b-a).rotation_difference(new_b-a),a)
    now_b=pose[ib].translation;now_c=pose[ic].translation
    rotate_desc(names[1],(now_c-now_b).rotation_difference(new_c-now_b),now_b)
    return pose

def make_guard(direction,t):
    return with_weapon(blend_pose(NEUTRAL,GUARD_POSES[direction],smooth(t)))

def clip_pose(name,t):
    if name=='idle':return with_weapon(sample('Idle2',t*.15))
    if name=='walk':return with_weapon(sample('Walk',t))
    if name in ATTACKS:
        source=ATTACKS[name];contact=CONTACTS[name]
        knots=[0.,.16,.36,.60,1.];at=[0.,max(0.,contact-.11),contact,min(1.,contact+.17),1.]
        i=min(3,next((i for i in range(4)if t<=knots[i+1]),3));f=(t-knots[i])/(knots[i+1]-knots[i])
        a=NEUTRAL if i==0 else sample(source,at[i]);b=NEUTRAL if i==3 else sample(source,at[i+1])
        return with_weapon(blend_pose(a,b,smooth(f)))
    if name.startswith('guard_'):return make_guard(name[6:],t)
    if name=='death_back':
        captured=sample('Death1',t)
        return with_weapon(blend_pose(NEUTRAL,captured,smooth(t/.12)))if t<.12 else with_weapon(captured)
    hit_source='GetHit2'if name in ['hit_right','hit_thrust']else'GetHit3'if name=='hit_low'else'GetHit1'
    captured=sample(hit_source,t)
    # Reactions return to the exact shared idle pose, avoiding a rest-pose snap.
    envelope=smooth(t/.16)if t<.16 else 1.-smooth((t-.75)/.25)if t>.75 else 1.
    return with_weapon(blend_pose(NEUTRAL,captured,envelope))

def topology(ob):
    bm=bmesh.new();bm.from_mesh(ob.data);bmesh.ops.triangulate(bm,faces=list(bm.faces),quad_method='BEAUTY',ngon_method='BEAUTY');bm.to_mesh(ob.data);bm.free();ob.data.update()
    mesh=ob.data;mesh.calc_loop_triangles();mesh.calc_normals_split()
    # Rest normal/UV seams form immutable IDs shared by every posed sample.
    norm=[tuple(n.vector)for n in mesh.corner_normals];uv=[tuple(x.uv)for x in mesh.uv_layers.active.data]
    mapping={};representatives=[];raw=[];remap=[]
    for i,loop in enumerate(mesh.loops):
        key=((tuple(mesh.vertices[loop.vertex_index].co)if globals().get('CHARACTER')=='orc04'and ob in globals().get('WEAPONS',[])else loop.vertex_index),*np.round(uv[i],8),*np.round(norm[i],6))
        if key not in mapping:mapping[key]=len(raw);raw.append(loop.vertex_index);representatives.append(i)
        remap.append(mapping[key])
    ids=np.asarray(remap,dtype=np.int64)[np.asarray([t.loops[:]for t in mesh.loop_triangles])]
    coords=np.asarray(uv,dtype=np.float32)[representatives];coords[:,1]=1.-coords[:,1]
    sem=np.zeros((len(mesh.vertices),8),np.float32)
    for v in mesh.vertices:
        for g in v.groups:
            label=semantic(ob.vertex_groups[g.group].name)
            if label is not None:sem[v.index,label]+=g.weight
    if ob.name=='Weapon':sem[:]=0
    else:
        sums=sem.sum(axis=1);sem[sums>1.e-6]/=sums[sums>1.e-6,None];sem[sums<=1.e-6,1]=1
    return {'raw':np.asarray(raw),'representatives':np.asarray(representatives),'indices':ids,'uv':coords,'semantic':sem[raw],'rigid':ob.name=='Weapon','vertices':len(mesh.vertices),'loops':len(mesh.loops),'weld_loop_map':np.asarray(remap,dtype=np.int64)}

def posed_arrays(origin=None):
    global NORMAL_REPAIRS
    graph=bpy.context.evaluated_depsgraph_get();rows=[]
    points=[]
    for name in NAMES:
        ob=bpy.data.objects[name];e=ob.evaluated_get(graph);m=e.to_mesh();m.calc_loop_triangles();m.calc_normals_split();fixed=TOPO[name]
        assert len(m.vertices)==fixed['vertices']and len(m.loops)==fixed['loops']
        raw_p=np.asarray([tuple(e.matrix_world@v.co)for v in m.vertices],np.float64)
        p=raw_p[fixed['raw']]
        normal_m=e.matrix_world.to_3x3().inverted().transposed()
        corners=np.asarray([tuple((normal_m@n.vector).normalized())for n in m.corner_normals],np.float64)
        bad=np.linalg.norm(corners,axis=1)<.5
        if np.any(bad):
            for tri in m.loop_triangles:
                invalid=[i for i in tri.loops if bad[i]]
                if not invalid:continue
                a,b,c=raw_p[list(tri.vertices)];normal=np.cross(b-a,c-a);length=np.linalg.norm(normal)
                if length>1.e-12:normal/=length
                else:normal=np.asarray(tuple((normal_m@ob.data.loops[tri.loops[0]].normal).normalized()))
                assert np.linalg.norm(normal)>.5,('Undefined source and posed normal',name)
                for i in invalid:corners[i]=normal;NORMAL_REPAIRS+=1
        ns=corners[fixed['representatives']]
        lengths=np.linalg.norm(ns,axis=1);assert np.all(lengths>.5);ns/=lengths[:,None]
        rows.append([p,ns,fixed]);points.append(p);e.to_mesh_clear()
    body=np.concatenate(points[:-1]);pelvis=ARM.matrix_world@ARM.pose.bones['pelvis'].head
    if origin is None:origin=np.asarray([pelvis.x,pelvis.y,float(body[:,2].min())])
    else:origin=np.asarray(origin).copy();origin[2]-=max(0.,origin[2]-float(body[:,2].min()))
    for row in rows:row[0]=((row[0]-origin)@BASIS.T*SCALE).astype(np.float32);row[1]=(row[1]@BASIS.T).astype(np.float32)
    joints=np.asarray([tuple(ARM.matrix_world@ARM.pose.bones[n].head)for n in BONE_NAMES]);joints=((joints-origin)@BASIS.T*SCALE).astype(np.float32)
    weapon=ARM.matrix_world@ARM.pose.bones['Weapon1'].matrix
    edge=np.asarray([tuple(weapon@p)for p in AXE_EDGE_LOCAL]);edge=((edge-origin)@BASIS.T*SCALE).astype(np.float32)
    pivot=((np.asarray(weapon.translation)-origin)@BASIS.T*SCALE).astype(np.float32)
    return rows,joints,edge,pivot,origin

def glb(path,rows,slot,clip):
    blob=bytearray();views=[];accessors=[]
    def add(raw,target=None):
        blob.extend(b'\0'*(-len(blob)%4));v={'buffer':0,'byteOffset':len(blob),'byteLength':len(raw)}
        if target:v['target']=target
        views.append(v);blob.extend(raw);return len(views)-1
    def acc(a,kind,component,target,bounds=False):
        a=np.ascontiguousarray(a);d={'bufferView':add(a.tobytes(),target),'componentType':component,'count':len(a),'type':kind}
        if bounds:d.update(min=a.min(axis=0).tolist(),max=a.max(axis=0).tolist())
        accessors.append(d);return len(accessors)-1
    prims=[]
    for name,(p,n,fixed)in zip(NAMES,rows):
        attrs={};tg=tangents(p,n,fixed['uv'],fixed['indices'],fixed['sign'])
        for key,a,kind in [('POSITION',p,'VEC3'),('NORMAL',n,'VEC3'),('TEXCOORD_0',fixed['uv'],'VEC2'),('TANGENT',tg,'VEC4')]:attrs[key]=acc(a.astype('<f4'),kind,5126,34962,key=='POSITION')
        component,dtype=(5123,'<u2')if len(p)<65536 else(5125,'<u4')
        ix=acc(fixed['indices'].reshape(-1).astype(dtype),'SCALAR',component,34963)
        mat=1 if fixed['rigid'] else 0
        prims.append({'attributes':attrs,'indices':ix,'material':mat,'mode':4,'extras':{'source_object':name,'rigid_weapon':fixed['rigid']}})
    materials=[{'name':'Owned Unity Orc Gnur body','pbrMetallicRoughness':{'baseColorFactor':[.45,.45,.45,1.],'metallicFactor':0.,'roughnessFactor':.7}}, {'name':'Owned Unity Orc Gnur weapon','pbrMetallicRoughness':{'baseColorFactor':[.35,.35,.35,1.],'metallicFactor':.8,'roughnessFactor':.45}}]
    doc={'asset':{'version':'2.0','generator':'Valwood actual owned Unity Gnur opponent baker'},'scene':0,'scenes':[{'nodes':[0]}],'nodes':[{'mesh':0,'name':f'Owned Unity Gnur {clip} slot{slot}'}],'meshes':[{'primitives':prims}],'materials':materials,'accessors':accessors,'bufferViews':views,'buffers':[{'byteLength':len(blob)}],'extras':{'source_name':'Owned Unity Orc Gnur','pose':slot,'clip':clip,'original_geometry':True,'materials_provisional':True,'source_scripts_executed':False}}
    raw=json.dumps(doc,separators=(',',':')).encode();raw+=b' '*(-len(raw)%4);blob.extend(b'\0'*(-len(blob)%4))
    path.write_bytes(struct.pack('<III',0x46546c67,2,28+len(raw)+len(blob))+struct.pack('<II',len(raw),0x4e4f534a)+raw+struct.pack('<II',len(blob),0x004e4942)+blob)

