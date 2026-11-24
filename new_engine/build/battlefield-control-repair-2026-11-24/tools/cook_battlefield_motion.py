"""Offline owned MC capture derivative, canonical 95-joint body; no source writes.
Run Blender -b --python. Emits a CPU animation sidecar; not a rendered model.
Ground grappling is authored by WorldBattlefieldActors, not claimed as mocap.
"""
from pathlib import Path
import copy,hashlib,json,math,struct,sys
import bpy
from mathutils import Matrix,Vector,Quaternion
N=Path(__file__).resolve().parents[1];E=N.parents[1];OUT=N/'battlefield-motion';OUT.mkdir(exist_ok=True)
sys.path.insert(0,str(E/'tools'));import retarget_modular_knight as rt
CANON=N/'motion/rigs/sword.glb';BLEND=E/'assets/purchased/modular_knights/derived/knight_rig_weapon.blend'
SPECS={
 'BF_Ram':dict(file='Dodge/MCU_am_LongS_Uni_Dodge_Fwd_Ram.FBX',loop=False),
 'BF_Fall':dict(file='Death/MCU_am_LongS_Death_Back_01.FBX',loop=False),
 'BF_Rise':dict(file='Death/MCU_am_LongS_Death_Back_01_Getup.FBX',loop=False),
 'BF_StabA':dict(file='Finisher/MCU_am_LongS_StanceR_Finisher_ChestStab_A.FBX',loop=False),
 'BF_Victim':dict(file='Finisher/MCU_am_LongS_StanceR_Finisher_ChestStab_B2.FBX',loop=False),
 'BF_Slash':dict(file='Attack/MCU_am_LongS_Lv1_StanceR_Attack03.FBX',loop=False),
 'BF_DeathSide':dict(file='Death/MCU_am_LongS_Death_Left_01.FBX',loop=False),
}
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
inputs={str(p):sha(p)for p in [Path(__file__),CANON,BLEND,Path(rt.__file__),*[rt.SOURCE_ROOT/x['file']for x in SPECS.values()]]}
raw=CANON.read_bytes();js=struct.unpack_from('<I',raw,12)[0];original=json.loads(raw[20:20+js]);nodes=original['nodes'];joints=original['skins'][0]['joints'];names={n.get('name'):i for i,n in enumerate(nodes)};parents={c:i for i,n in enumerate(nodes)for c in n.get('children',[])}
def local(n):
 if 'matrix'in n:return Matrix([n['matrix'][i::4]for i in range(4)])
 q=n.get('rotation',[0,0,0,1]);return Matrix.LocRotScale(Vector(n.get('translation',[0,0,0])),Quaternion((q[3],q[0],q[1],q[2])),Vector(n.get('scale',[1,1,1])))
rest={}
def glob(i):
 if i not in rest:rest[i]=(glob(parents[i])if i in parents else Matrix.Identity(4))@local(nodes[i])
 return rest[i]
for i in range(len(nodes)):glob(i)
bpy.ops.wm.open_mainfile(filepath=str(BLEND));rig=next(o for o in bpy.data.objects if o.type=='ARMATURE');rig.animation_data.action=None
for track in rig.animation_data.nla_tracks:track.mute=True
for b in rig.pose.bones:b.matrix_basis=Matrix.Identity(4)
bpy.context.view_layer.update();C=Matrix(((1,0,0,0),(0,0,1,0),(0,-1,0,0),(0,0,0,1)));br={i:C@rig.matrix_world@rig.data.bones[nodes[i]['name']].matrix_local for i in joints};cal={i:br[i].inverted()@rest[i]for i in joints}
assert max((br[i].translation-rest[i].translation).length for i in joints)<2e-6
rt.CLIPS=SPECS
bpy.context.scene.render.fps=30;bpy.context.scene.render.fps_base=1
actions,report=rt.bake_actions(rig,clips=list(SPECS),fps=30,align_gait_phase=False,progress=lambda c,n,t:print('BF_SAMPLE',c,n,t,flush=True)if n%30==0 else None)
doc={'asset':{'version':'2.0','generator':'Valwood owned battlefield captures'},'scene':original.get('scene',0),'scenes':copy.deepcopy(original['scenes']),'nodes':copy.deepcopy(nodes),'buffers':[{'byteLength':0}],'bufferViews':[],'accessors':[],'animations':[]}
for n in doc['nodes']:
 for k in ('mesh','skin','weights'):n.pop(k,None)
binary=bytearray()
def acc(values,kind,component=5126):
 binary.extend(b'\0'*(-len(binary)%4));at=len(binary);flat=[v for row in values for v in row];fmt='f'if component==5126 else'H';binary.extend(struct.pack('<'+fmt*len(flat),*flat));view=len(doc['bufferViews']);doc['bufferViews'].append({'buffer':0,'byteOffset':at,'byteLength':len(flat)*(4 if component==5126 else 2)});index=len(doc['accessors']);a={'bufferView':view,'componentType':component,'count':len(values),'type':kind}
 if kind=='SCALAR':a.update(min=[min(flat)],max=[max(flat)])
 doc['accessors'].append(a);return index
def channel(anim,node,path,values,times):
 constant=all(struct.pack('<'+'f'*len(v),*v)==struct.pack('<'+'f'*len(values[0]),*values[0])for v in values)
 if constant:values,times=values[:1],times[:1]
 sampler=len(anim['samplers']);anim['samplers'].append({'input':acc([[t]for t in times],'SCALAR'),'output':acc(values,'VEC4'if path=='rotation'else'VEC3'),'interpolation':'LINEAR'});anim['channels'].append({'sampler':sampler,'target':{'node':node,'path':path}})
reports=[]
for label,action in actions.items():
 rig.animation_data.action=action;meta=json.loads(action['valwood_clip']);duration=meta['duration_seconds'];times=sorted(set(min(duration,k/30)for k in range(math.ceil(duration*30)+1)));tracks={i:[]for i in joints if nodes[i]['name']in rt.TARGET_TO_SOURCE};positions=[];samples=[];previous={}
 for t in times:
  rt._set_frame(1+t*30);posed={i:C@rig.matrix_world@rig.pose.bones[nodes[i]['name']].matrix@cal[i]for i in joints};point={}
  for i in tracks:
   parent=parents.get(i);pm=posed[parent]if parent in posed else rest[parent]if parent is not None else Matrix.Identity(4);tr,q,sc=(pm.inverted()@posed[i]).decompose();q.normalize()
   if i in previous and previous[i].dot(q)<0:q.negate()
   previous[i]=q.copy();tracks[i].append([q.x,q.y,q.z,q.w])
   if nodes[i]['name']=='pelvis':positions.append(list(tr))
   if nodes[i]['name']in('pelvis','head','hand_l','hand_r','foot_l','foot_r','ball_l','ball_r'):point[nodes[i]['name']]=list(posed[i].translation)
  samples.append({'t':t,'points':point})
 anim={'name':label,'channels':[],'samplers':[],'extras':{'source':SPECS[label]['file'],'source_sha256':sha(rt.SOURCE_ROOT/SPECS[label]['file']),'loop':False,'full_body':True,'authorship':'Purchased MoCap Central capture; canonical joint retarget only. Ground grapple is separate authored runtime choreography.'}}
 channel(anim,names['pelvis'],'translation',positions,times)
 for i,v in tracks.items():channel(anim,i,'rotation',v,times)
 doc['animations'].append(anim);reports.append({'name':label,'duration':duration,'metadata':meta,'samples':samples});print('BF_EXPORTED',label,duration,flush=True)
# Tiny importer-compatible triangle, never rendered. Canonical palette unchanged.
sourcebin=raw[28+js:];a=original['accessors'][original['skins'][0]['inverseBindMatrices']];v=original['bufferViews'][a['bufferView']];ib=acc([struct.unpack_from('<16f',sourcebin,v.get('byteOffset',0)+a.get('byteOffset',0)+i*64)for i in range(a['count'])],'MAT4');doc['skins']=copy.deepcopy(original['skins']);doc['skins'][0]['inverseBindMatrices']=ib;mn=next(i for i,n in enumerate(original['nodes'])if n.get('skin')==0 and'mesh'in n);doc['nodes'][mn].update(mesh=0,skin=0);p=acc([[0,0,0],[.001,0,0],[0,.001,0]],'VEC3');w=acc([[1,0,0,0]]*3,'VEC4');j=acc([[0,0,0,0]]*3,'VEC4',5123);doc['meshes']=[{'primitives':[{'attributes':{'POSITION':p,'JOINTS_0':j,'WEIGHTS_0':w},'mode':4}]}];doc['buffers'][0]['byteLength']=len(binary);jbytes=json.dumps(doc,separators=(',',':')).encode();jbytes+=b' '*(-len(jbytes)%4);binary.extend(b'\0'*(-len(binary)%4));out=OUT/'captures.glb';out.write_bytes(struct.pack('<III',0x46546c67,2,28+len(jbytes)+len(binary))+struct.pack('<II',len(jbytes),0x4e4f534a)+jbytes+struct.pack('<II',len(binary),0x004e4942)+binary)
assert inputs=={p:sha(p)for p in inputs}
(OUT/'CAPTURE_RECEIPT.json').write_text(json.dumps({'passed':True,'inputs_unchanged':True,'inputs':inputs,'output':str(out),'output_sha256':sha(out),'reports':reports,'native_visual_validation_pending':True},indent=2)+'\n')
