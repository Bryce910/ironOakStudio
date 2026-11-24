"""Offline animation-only export: real owned capture, unchanged canonical nodes."""
import copy,hashlib,json,math,struct,sys
from pathlib import Path
import bpy
from mathutils import Matrix,Quaternion,Vector
BASE=Path(__file__).resolve().parents[1]
OUT=BASE/'assets/story_hero/story_clips';OUT.mkdir(parents=True,exist_ok=True)
ROOT=Path('/home/computer/Desktop/final_engine/new_engine')
TOOLS=ROOT/'tools';sys.path.insert(0,str(TOOLS))
import retarget_modular_knight as retarget
import animate_modular_knight_weapon as weapon
CANONICAL=ROOT/'build/cinematic-studio-2026-11-15/equipment-assets/rigs/sword.glb'
BLEND=ROOT/'assets/purchased/modular_knights/derived/knight_rig_weapon.blend'
SOCKET=ROOT/'assets/purchased/modular_knights/derived/weapon_socket_fit.json'
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
raw=CANONICAL.read_bytes();length=struct.unpack_from('<I',raw,12)[0];canonical=json.loads(raw[20:20+length]);nodes=canonical['nodes'];joints=canonical['skins'][0]['joints'];parents={c:i for i,n in enumerate(nodes) for c in n.get('children',[])}
def local(n):
    if 'matrix' in n:return Matrix([n['matrix'][i::4] for i in range(4)])
    q=n.get('rotation',[0,0,0,1]);return Matrix.LocRotScale(Vector(n.get('translation',[0,0,0])),Quaternion((q[3],q[0],q[1],q[2])),Vector(n.get('scale',[1,1,1])))
rest={}
def global_rest(i):
    if i not in rest:rest[i]=(global_rest(parents[i]) if i in parents else Matrix.Identity(4))@local(nodes[i])
    return rest[i]
for i in range(len(nodes)):global_rest(i)
bpy.ops.wm.open_mainfile(filepath=str(BLEND));rig=next(o for o in bpy.data.objects if o.type=='ARMATURE');rig.animation_data.action=None
for track in rig.animation_data.nla_tracks:track.mute=True
for bone in rig.pose.bones:bone.matrix_basis=Matrix.Identity(4)
bpy.context.view_layer.update()
assert len(joints)==95 and len(rig.data.bones)==95 and {nodes[i]['name'] for i in joints}==set(rig.data.bones.keys())
C=Matrix(((1,0,0,0),(0,0,1,0),(0,-1,0,0),(0,0,0,1)))
blender_rest={i:C@rig.matrix_world@rig.data.bones[nodes[i]['name']].matrix_local for i in joints}
origin_error=max((blender_rest[i].translation-rest[i].translation).length for i in joints);assert origin_error<2e-6
calibration={i:blender_rest[i].inverted()@rest[i] for i in joints}
rest_signature={b.name:[list(row) for row in b.matrix_local] for b in rig.data.bones}
retarget.CLIPS={
 'Story_SwimIdle':dict(file=str(ROOT/'build/arena-orcs-textured-2026-11-14/intake/animations/Animations/2Without_Weapon/Anim_Idle_Swim.FBX'),loop=True),
 'Story_SwimForward':dict(file=str(ROOT/'build/arena-orcs-textured-2026-11-14/intake/animations/Animations/2Without_Weapon/Anim_Swim.FBX'),loop=True),
 'Story_Death':dict(file=str(ROOT/'assets/purchased/longsword_mocap/source/Animations/Unity_Skel/Death/MCU_am_LongS_Death_Back_01.FBX'),loop=False)}
actions={};reports=[];weapon_report={}
for label in retarget.CLIPS:
 mapping=dict(retarget.TARGET_TO_SOURCE)
 if label.startswith('Story_Swim'):mapping['jaw']='B_Jaw'
 subset,rep=retarget.bake_actions(rig,clips=[label],source_root=Path('/'),target_map=mapping,fps=30,seam_frames=4.,align_gait_phase=False,progress=lambda clip,n,total:print('SAMPLE',clip,n,total,flush=True))
 actions.update(subset);reports.append(rep);weapon_report[label]={'mode':'Existing fixed target socket rest follows hand; swimming hides held equipment in runtime.'}
assert rest_signature=={b.name:[list(row) for row in b.matrix_local] for b in rig.data.bones}
document={'asset':{'version':'2.0','generator':'Valwood owned swim/death captured motion on exact canonical skeleton'},'scene':canonical.get('scene',0),'scenes':copy.deepcopy(canonical['scenes']),'nodes':copy.deepcopy(nodes),'buffers':[{'byteLength':0}],'bufferViews':[],'accessors':[],'animations':[],'extras':{'canonical_skin_joint_nodes':joints,'animation_only':True,'source_geometry_included':False}}
for node in document['nodes']:
    for field in ['mesh','skin','weights']:node.pop(field,None)
binary=bytearray();sampling=[];max_local_reconstruction=0.;max_world_reconstruction=0.
def accessor(values,kind):
    while len(binary)%4:binary.append(0)
    offset=len(binary);flat=[v for row in values for v in row];binary.extend(struct.pack('<'+'f'*len(flat),*flat));view=len(document['bufferViews']);document['bufferViews'].append({'buffer':0,'byteOffset':offset,'byteLength':len(flat)*4});index=len(document['accessors']);a={'bufferView':view,'componentType':5126,'count':len(values),'type':kind}
    if kind=='SCALAR':a.update(min=[min(flat)],max=[max(flat)])
    document['accessors'].append(a);return index
bpy.context.scene.render.fps=30;bpy.context.scene.render.fps_base=1
for name,action in actions.items():
    rig.animation_data.action=action;meta=json.loads(action['valwood_clip']);duration=meta['duration_seconds'] if 'duration_seconds' in meta else (meta['source_frame_range'][1]-meta['source_frame_range'][0])/30.
    count=int(math.ceil(duration*30))+1;times=[min(duration,k/30.) for k in range(count)];tracks={i:{'translation':[],'rotation':[],'scale':[]} for i in joints};previous={};max_step=0.;prior_points=None
    vertical_lift=0.;head_values=[]
    if name.startswith('Story_Swim'):
        head_index=next(i for i in joints if nodes[i]['name']=='head')
        for seconds in times:
            retarget._set_frame(1+seconds*30.)
            head_values.append((C@rig.matrix_world@rig.pose.bones['head'].matrix@calibration[head_index]).translation.y)
        vertical_lift=1.60-sorted(head_values)[len(head_values)//2]
    pelvis_index=next(i for i in joints if nodes[i]['name']=='pelvis')
    def descendant(i):
        while i!=pelvis_index and i in parents:i=parents[i]
        return i==pelvis_index
    for seconds in times:
        retarget._set_frame(1+seconds*30.)
        posed={i:C@rig.matrix_world@rig.pose.bones[nodes[i]['name']].matrix@calibration[i] for i in joints}
        if vertical_lift:
            for i in joints:
                if descendant(i):posed[i].translation.y+=vertical_lift
        reconstructed={};points={i:posed[i].translation for i in joints}
        if prior_points:max_step=max(max_step,max((points[i]-prior_points[i]).length for i in joints))
        prior_points={i:v.copy() for i,v in points.items()}
        def reconstruct(i):
            global max_local_reconstruction,max_world_reconstruction
            if i in reconstructed:return reconstructed[i]
            parent=parents.get(i);parent_pose=reconstruct(parent) if parent is not None else Matrix.Identity(4)
            desired=posed.get(i,rest[i]);m=parent_pose.inverted()@desired
            if i not in tracks:reconstructed[i]=rest[i];return reconstructed[i]
            t,q,s=m.decompose();q.normalize()
            if i in previous and previous[i].dot(q)<0:q.negate()
            previous[i]=q.copy();tracks[i]['translation'].append(list(t));tracks[i]['rotation'].append([q.x,q.y,q.z,q.w]);tracks[i]['scale'].append(list(s))
            rebuilt=Matrix.LocRotScale(t,q,s);max_local_reconstruction=max(max_local_reconstruction,max(abs(m[r][c]-rebuilt[r][c]) for r in range(4) for c in range(4)));reconstructed[i]=parent_pose@rebuilt;max_world_reconstruction=max(max_world_reconstruction,(reconstructed[i].translation-desired.translation).length);return reconstructed[i]
        for i in joints:reconstruct(i)
    anim={'name':name,'channels':[],'samplers':[],'extras':{'loop':bool(meta['loop']),'source_sha256':meta['source_sha256'],'source_frame_range':meta['source_frame_range'],'source_fps':30,'source_duration':duration,'constant_swim_root_lift_m':vertical_lift}}
    time_accessor=accessor([[v] for v in times],'SCALAR')
    for i in joints:
        for path,kind in [('translation','VEC3'),('rotation','VEC4'),('scale','VEC3')]:
            sampler=len(anim['samplers']);anim['samplers'].append({'input':time_accessor,'output':accessor(tracks[i][path],kind),'interpolation':'LINEAR'});anim['channels'].append({'sampler':sampler,'target':{'node':i,'path':path}})
    document['animations'].append(anim);sampling.append({'name':name,'frames':len(times),'duration':duration,'loop':bool(meta['loop']),'max_actual_joint_step_m':max_step,'constant_swim_root_lift_m':vertical_lift,'source_head_y_range_m':[min(head_values),max(head_values)] if head_values else None,'retarget_metadata':meta,'weapon_attachment':weapon_report[name]})
assert max_world_reconstruction<1e-5 and max_local_reconstruction<2e-3,(max_world_reconstruction,max_local_reconstruction)
document['buffers'][0]['byteLength']=len(binary);js=json.dumps(document,separators=(',',':')).encode();js+=b' '*((-len(js))%4);binary+=b'\0'*((-len(binary))%4);blob=struct.pack('<III',0x46546c67,2,28+len(js)+len(binary))+struct.pack('<II',len(js),0x4e4f534a)+js+struct.pack('<II',len(binary),0x004e4942)+binary
out=OUT/'story_clips.glb';out.write_bytes(blob)
sources=[Path(__file__),CANONICAL,BLEND,SOCKET,Path(retarget.__file__),Path(weapon.__file__)]+[retarget.SOURCE_ROOT/spec['file'] for spec in retarget.CLIPS.values()]
receipt={'schema':'valwood.story-source-clips.v1','passed':True,'prototype_only':True,'canonical_nodes':len(nodes),'canonical_joint_count':len(joints),'clips':sampling,'rest_node_json_preserved':all({k:v for k,v in n.items() if k not in ['mesh','skin','weights']}==document['nodes'][i] for i,n in enumerate(nodes)),'canonical_joint_order':joints,'max_source_to_canonical_rest_origin_error_m':origin_error,'max_sampled_local_trs_reconstruction_error':max_local_reconstruction,'max_sampled_world_joint_reconstruction_error_m':max_world_reconstruction,'target_blender_rest_unchanged':True,'source_meshes_included':False,'source_weapon_motion':'Original target socket rest follows hand; no sword-specific socket animation is imported into these body clips.','axis_conversion':'Blender X/Z/-Y to glTF X/Y/Z; each sampled actual joint is rebased with bind compensation onto the exact canonical node rest axes.','inputs':{str(p):sha(p) for p in sources},'outputs':{str(out):sha(out)},'limitations':['No actual in-game draw/stow acceptance; controller/visibility/grip bridge is a separate isolated prototype.','Swimming and death motion require actual runtime visual review; this is an offline exact-skeleton derivative, not gameplay validation.']}
(OUT/'STORY_SOURCE_CLIPS_REVIEW.json').write_text(json.dumps(receipt,indent=2)+'\n');print('EQUIPMENT_EXPORT',json.dumps({'passed':True,'output':str(out),'bytes':len(blob),'clips':[(x['name'],x['duration']) for x in sampling]}))
