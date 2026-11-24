#!/usr/bin/env python3
"""Offline, reproducible import of the user's purchased animated castle dragons.

python3 tools/import_castle_dragons.py --prepare
blender -b -t 2 --factory-startup --disable-autoexec --python-exit-code 1 \
 --python tools/import_castle_dragons.py -- --cook
python3 tools/import_castle_dragons.py --validate

Archive members are data, never executable instructions. Source blends are read
with script execution disabled, never saved. Mesh collapse happens before skin
sampling, once per source clip; correspondence is then asserted for every pose.
"""
from pathlib import Path,PurePosixPath
import argparse,hashlib,json,math,os,shutil,stat,struct,sys,tempfile,zipfile
ROOT=Path(__file__).resolve().parents[1]
ASSET=ROOT/'assets/purchased/castle_dragons'
SOURCE=ASSET/'source';DERIVED=ASSET/'derived'
OUT=ROOT/'projects/valwood_world/models/castle_dragons'
VARIANTS=('hellfire','frostshade')
ARCHIVES=('HellfireDragon+_+animated+FBX+&+Blender.zip','FrostshadeDragon+_+animated+FBX+&+Blender.zip')
CLIPS=(('flight','Flying',10,60.,True),('landing','Landing',10,160.,False),('idle','Idle',4,300.,True),('breath','FireBreath',10,170.,False),('takeoff','TakeOff',8,90.,False))
SIDE=1024;BASE_SIDE=2048;SCALE=.32;DECIMATE=.12
# A single source floor offset keeps skeletal lift/compression through clips.
# Actual support metadata is exported; runtime must retain origin, not auto-floor.
FLOOR_OFFSET=.08312644*SCALE
BASIS=((-1.,0.,0.),(0.,0.,1.),(0.,1.,0.))
MAPS=('basecolor','normal_opengl','orm')
def sha(path):
 h=hashlib.sha256()
 with Path(path).open('rb')as f:
  for block in iter(lambda:f.read(1024*1024),b''):h.update(block)
 return h.hexdigest()
def publish(path,data):
 path=Path(path);path.parent.mkdir(parents=True,exist_ok=True)
 if path.exists()and path.read_bytes()==data:return
 temporary=path.with_suffix(path.suffix+'.pending');temporary.write_bytes(data);os.replace(temporary,path)
def json_write(path,obj):publish(path,(json.dumps(obj,indent=2)+'\n').encode())
def prepare():
 from PIL import Image
 import numpy as np
 inventories=[]
 for variant,filename in zip(VARIANTS,ARCHIVES):
  archive=Path('/home/computer/Downloads')/filename;preserved=[]
  with zipfile.ZipFile(archive)as z:
   assert len(z.infolist())<1000 and sum(i.file_size for i in z.infolist())<1024**3
   for i in z.infolist():
    member=PurePosixPath(i.filename.replace('\\','/'))
    assert not member.is_absolute()and'..'not in member.parts and':'not in i.filename
    assert not stat.S_ISLNK(i.external_attr>>16)and not i.flag_bits&1
    if i.is_dir():continue
    dst=SOURCE/variant/Path(*member.parts[1:]);dst.parent.mkdir(parents=True,exist_ok=True)
    with tempfile.NamedTemporaryFile(dir=dst.parent,delete=False)as f:
     temporary=Path(f.name)
     with z.open(i)as s:shutil.copyfileobj(s,f,1024*1024)
    try:
     assert temporary.stat().st_size==i.file_size
     if dst.exists():assert sha(dst)==sha(temporary),'Original staged file changed: '+str(dst)
     else:os.replace(temporary,dst)
    finally:temporary.unlink(missing_ok=True)
    preserved.append({'file':str(dst.relative_to(ASSET)),'bytes':dst.stat().st_size,'sha256':sha(dst)})
  inventories.append({'variant':variant,'archive':str(archive),'archive_bytes':archive.stat().st_size,'archive_sha256':sha(archive),'files':preserved})
  texture=SOURCE/variant/'textures';dest=DERIVED/variant
  def image(suffix,mode,side=SIDE):
   p=next(texture.glob('*_'+suffix));im=Image.open(p);assert im.size==(4096,4096)
   return im.convert(mode).resize((side,side),Image.Resampling.LANCZOS)
  base=image('baseColor.jpg','RGB',BASE_SIDE)
  normal=np.asarray(image('normal.png','RGB'),dtype=np.float32)/127.5-1.
  normal/=np.maximum(np.linalg.norm(normal,axis=2,keepdims=True),1e-8)
  normal=Image.fromarray(np.uint8(np.clip(np.rint((normal+1.)*127.5),0,255)))
  orm=Image.merge('RGB',(image('occlusion.jpg','L'),image('roughness.jpg','L'),image('metallic.jpg','L')))
  for name,im in zip(MAPS,[base,normal,orm]):
   with tempfile.NamedTemporaryFile(suffix='.png')as f:
    im.save(f.name,compress_level=6);publish(dest/(name+'.png'),Path(f.name).read_bytes())
 json_write(ASSET/'source_inventory.json',{'schema_version':1,'archives':inventories,'source_scripts_executed':False,'source_files_modified':False,'license':'Purchased user-supplied assets. No redistribution rights inferred.'})
 print('DRAGON_PREPARE_PASS',flush=True)

def write_glb(path,arrays,variant,label):
 import numpy as np
 p,n,uv,tangent,indices=arrays;blob=bytearray();views=[];accessors=[]
 def append(raw,target=None):
  blob.extend(b'\0'*(-len(blob)%4));v={'buffer':0,'byteOffset':len(blob),'byteLength':len(raw)}
  if target:v['target']=target
  views.append(v);blob.extend(raw);return len(views)-1
 def accessor(a,typ,component,target,bounds=False):
  a=np.ascontiguousarray(a);item={'bufferView':append(a.tobytes(),target),'componentType':component,'count':len(a),'type':typ}
  if bounds:item.update(min=a.min(0).tolist(),max=a.max(0).tolist())
  accessors.append(item);return len(accessors)-1
 attributes={key:accessor(a.astype('<f4'),typ,5126,34962,key=='POSITION')for key,a,typ in [('POSITION',p,'VEC3'),('NORMAL',n,'VEC3'),('TEXCOORD_0',uv,'VEC2'),('TANGENT',tangent,'VEC4')]}
 idx=accessor(indices.reshape(-1).astype('<u2'),'SCALAR',5123,34963)
 images=[{'name':variant+'_'+name,'bufferView':append((DERIVED/variant/(name+'.png')).read_bytes()),'mimeType':'image/png'}for name in MAPS]
 doc={'asset':{'version':'2.0','generator':'Valwood purchased dragon offline skin sampler v1'},'scene':0,'scenes':[{'nodes':[0]}],'nodes':[{'name':label,'mesh':0}],'meshes':[{'name':label,'primitives':[{'attributes':attributes,'indices':idx,'material':0,'mode':4}]}],
  'materials':[{'name':variant+' owned source PBR 2K color 1K normal ARM','pbrMetallicRoughness':{'baseColorFactor':[1,1,1,1],'baseColorTexture':{'index':0},'metallicFactor':1,'roughnessFactor':1,'metallicRoughnessTexture':{'index':2}},'normalTexture':{'index':1,'scale':1},'occlusionTexture':{'index':2,'strength':1},'alphaMode':'OPAQUE','doubleSided':True}],
  'images':images,'samplers':[{'magFilter':9729,'minFilter':9987,'wrapS':10497,'wrapT':10497}],'textures':[{'sampler':0,'source':i}for i in range(3)],'accessors':accessors,'bufferViews':views,'buffers':[{'byteLength':len(blob)}],
  'extras':{'valwood_pose_contract_version':1,'source_scripts_executed':False,'preserve_source_origin':True,'source_scale':SCALE,'source_forward':'+Y','engine_forward':'+Z','floor_offset_m':FLOOR_OFFSET}}
 raw=json.dumps(doc,separators=(',',':')).encode();raw+=b' '*(-len(raw)%4);blob.extend(b'\0'*(-len(blob)%4))
 publish(path,struct.pack('<III',0x46546c67,2,28+len(raw)+len(blob))+struct.pack('<II',len(raw),0x4e4f534a)+raw+struct.pack('<II',len(blob),0x004e4942)+blob)

def cook():
 import bpy,bmesh,numpy as np
 from mathutils import Vector
 sys.path.insert(0,str(ROOT/'tools'));from import_village_shield_knight import tangents
 bpy.context.preferences.filepaths.use_scripts_auto_execute=False
 basis=np.asarray(BASIS);canonical=None;uv=None;indices=None;signs=None;all_arrays=[];metadata=[];clips=[];source=[];maxnormalerror=0.;minfacearea=1e9
 # Skin only the Hellfire source: independently validate matching Frostshade
 # evaluated geometry before reusing exact vertices for its distinct material.
 def open_source(variant,clip):
  p=next((SOURCE/variant).glob('* _ '+clip+'.blend'))
  bpy.ops.wm.open_mainfile(filepath=str(p),load_ui=False,use_scripts=False)
  ob=next(o for o in bpy.data.objects if o.type=='MESH'and len(o.data.vertices)>1000)
  arm=next(o for o in bpy.data.objects if o.type=='ARMATURE')
  assert len(ob.data.vertices)==50674 and len(ob.data.polygons)==50366
  return p,ob,arm
 def simplify(ob):
  bpy.ops.object.select_all(action='DESELECT');ob.select_set(True);bpy.context.view_layer.objects.active=ob
  modifiers=list(ob.modifiers)
  for m in modifiers:m.show_viewport=False
  m=ob.modifiers.new('Valwood fixed unposed topology','DECIMATE');m.ratio=DECIMATE;m.use_collapse_triangulate=True
  bpy.ops.object.modifier_apply(modifier=m.name)
  bm=bmesh.new();bm.from_mesh(ob.data);bmesh.ops.triangulate(bm,faces=list(bm.faces),quad_method='BEAUTY',ngon_method='BEAUTY');bm.to_mesh(ob.data);bm.free();ob.data.update()
  for m in modifiers:m.show_viewport=True
 def evaluate(ob,arm,frame,reference=False,pose_override=None,contact=0.):
  nonlocal canonical,uv,indices,signs,maxnormalerror,minfacearea
  scene=bpy.context.scene;scene.frame_set(math.floor(frame),subframe=frame%1)
  if pose_override is not None:
   arm.animation_data.action=None
   for b in arm.pose.bones:b.matrix_basis=pose_override[b.name]
  bpy.context.view_layer.update();g=bpy.context.evaluated_depsgraph_get()
  e=ob.evaluated_get(g);a=arm.evaluated_get(g);m=e.to_mesh();m.calc_loop_triangles();m.calc_normals_split()
  raw_p=np.asarray([tuple(e.matrix_world@v.co)for v in m.vertices]);normal_matrix=e.matrix_world.to_3x3().inverted().transposed()
  raw_n=np.asarray([tuple((normal_matrix@l.normal).normalized())for l in m.loops]);raw_uv=np.asarray([tuple(x.uv)for x in m.uv_layers.active.data]);raw_i=np.asarray([list(t.loops)for t in m.loop_triangles],dtype=np.int64)
  # Normals are repaired deterministically only for degenerate source corners.
  for tri in m.loop_triangles:
   bad=[i for i in tri.loops if np.dot(raw_n[i],raw_n[i])<.5]
   if bad:
    p=raw_p[list(tri.vertices)];n=np.cross(p[1]-p[0],p[2]-p[0]);length=np.linalg.norm(n);n=n/length if length>1e-12 else np.asarray([0.,0.,1.])
    for i in bad:raw_n[i]=n
  if reference:
   mapping={};reps=[];remap=[]
   for i,l in enumerate(m.loops):
    # Fixed source smooth groups/UV seams. Do not regroup posed normal frames.
    key=(l.vertex_index,*np.round(raw_uv[i],8),*np.round(raw_n[i],6))
    if key not in mapping:mapping[key]=len(reps);reps.append(i)
    remap.append(mapping[key])
   canonical={'representatives':np.asarray(reps),'vertices':np.asarray([m.loops[i].vertex_index for i in reps]),'remap':np.asarray(remap),'raw_triangles':raw_i,'raw_uv':raw_uv.copy(),'raw_vertices':len(m.vertices),'loops':len(m.loops)}
   uv=raw_uv[canonical['representatives']].copy();uv[:,1]=1.-uv[:,1];indices=canonical['remap'][raw_i]
  assert len(m.vertices)==canonical['raw_vertices']and len(m.loops)==canonical['loops']and np.array_equal(raw_i,canonical['raw_triangles'])and np.array_equal(raw_uv,canonical['raw_uv']),'Unstable source topology/UV'
  p=raw_p[canonical['vertices']]@basis.T*SCALE;p[:,1]+=FLOOR_OFFSET
  n=raw_n[canonical['representatives']]@basis.T;n/=np.maximum(np.linalg.norm(n,axis=1)[:,None],1e-12)
  maxnormalerror=max(maxnormalerror,float(abs(np.linalg.norm(n,axis=1)-1).max()))
  correction=np.maximum(-p[:,1],0.)*contact
  if float(correction.max())>1e-8:
   p[:,1]+=correction
   # Local tail/wing-tip floor contact, retaining the entire root/foot/spine
   # trajectory. Rebuild only normals bordering these changed triangles.
   changed=correction>1e-8;affected=indices[np.any(changed[indices],axis=1)]
   rebuilt=np.zeros_like(n);q=p[indices];face=np.cross(q[:,1]-q[:,0],q[:,2]-q[:,0])
   for corner in range(3):np.add.at(rebuilt,indices[:,corner],face)
   length=np.linalg.norm(rebuilt,axis=1);ids=np.unique(affected);ids=ids[length[ids]>1e-10]
   n[ids]=rebuilt[ids]/length[ids,None]
  tg=tangents(p,n,uv,indices,signs)
  if signs is None:signs=tg[:,3].copy()
  assert np.isfinite(p).all()and np.isfinite(n).all()and np.isfinite(tg).all()and np.array_equal(signs,tg[:,3])
  head=a.pose.bones['head'];mouth_source=a.matrix_world@head.tail
  # Head axis points through the nose. Its pitch/turn follows the actual rig.
  direction_source=(a.matrix_world.to_3x3()@(head.tail-head.head)).normalized()
  mouth=np.asarray(tuple(mouth_source))@basis.T*SCALE;mouth[1]+=FLOOR_OFFSET
  direction=np.asarray(tuple(direction_source))@basis.T;direction/=np.linalg.norm(direction)
  feet=[]
  for name in ['frontfootIK_L','frontfootIK_R','rearfootIK_L','rearfootIK_R']:
   point=np.asarray(tuple(a.matrix_world@a.pose.bones[name].tail))@basis.T*SCALE;point[1]+=FLOOR_OFFSET;feet.append(point)
  foot_groups={g.index for g in ob.vertex_groups if g.name.startswith(('toe_','finger-arm_'))or g.name in ['leg_L.003','leg_R.003','Bone.001_L.038','Bone.001_R.038']}
  foot_vertices=[v.index for v in ob.data.vertices if sum(g.weight for g in v.groups if g.group in foot_groups)>.5]
  assert len(foot_vertices)>50
  support_y=float(np.min(raw_p[foot_vertices,2]))*SCALE+FLOOR_OFFSET;support_y+=max(-support_y,0.)*contact
  record={'mouth':mouth.tolist(),'direction':direction.tolist(),'feet_y':support_y,'feet':np.asarray(feet).tolist(),'bounds_min':p.min(0).tolist(),'bounds_max':p.max(0).tolist(),'source_frame':frame,'ground_contact_weight':contact,'maximum_contact_correction_m':float(correction.max()),'root':list(a.matrix_world@a.pose.bones['root'].head)}
  e.to_mesh_clear();return(p,n,uv,indices,tg),record
 def contact_weight(name,t):
  def smooth(x):x=max(0.,min(1.,x));return x*x*(3.-2.*x)
  if name in ['idle','breath']:return 1.
  if name=='landing':return smooth((t-.32)/.12)
  if name=='takeoff':return 1.-smooth((t-.24)/.10)
  return 0.
 for name,source_clip,count,end,loop in CLIPS:
  path,ob,arm=open_source('hellfire',source_clip);simplify(ob)
  base=len(all_arrays);clips.append({'name':name,'base':base,'count':count,'duration':end/30.,'loop':loop,'source_frames':[0,end]})
  source.append({'file':str(path.relative_to(ASSET)),'sha256':sha(path),'action':arm.animation_data.action.name,'fps':bpy.context.scene.render.fps,'native_frames':[0,end]})
  for i in range(count):
   frame=end*i/(count if loop else count-1)
   arrays,record=evaluate(ob,arm,frame,canonical is None,contact=contact_weight(name,frame/end));metadata.append(record);all_arrays.append(arrays)
   print('DRAGON_BAKE',name,i,'vertices',len(arrays[0]),'triangles',len(indices),flush=True)
 # Verify complete source skin pose parity from the second purchased archive.
 # Same simplification must produce identical order/correspondence, not merely
 # visually similar bounds. Save no changes to either author's source files.
 for name,source_clip,count,end,loop in CLIPS:
  path,ob,arm=open_source('frostshade',source_clip);simplify(ob)
  base=next(c['base']for c in clips if c['name']==name)
  for i in sorted(set([0,count//2,count-1])):
   frame=end*i/(count if loop else count-1);arrays,_=evaluate(ob,arm,frame,contact=contact_weight(name,frame/end))
   errors=[float(abs(x-y).max())for x,y in zip(arrays,all_arrays[base+i])];print('DRAGON_PARITY',name,i,errors,flush=True);assert errors[0]<1e-6 and errors[1]<1e-4 and errors[2]==0 and errors[3]==0 and errors[4]<1e-4,'Variant rig mismatch'
  source.append({'file':str(path.relative_to(ASSET)),'sha256':sha(path),'parity_samples':[0,count//2,count-1]})
 # Authored airborne breath combines the native cyclic wing/body motion
 # with the purchased breath neck/head/jaw controls. No ground wing folding.
 path,ob,arm=open_source('hellfire','Flying');simplify(ob)
 flight_action=arm.animation_data.action
 breath_path=next((SOURCE/'hellfire').glob('* _ FireBreath.blend'))
 with bpy.data.libraries.load(str(breath_path),link=False)as(data_from,data_to):data_to.actions=['Action']
 breath_action=data_to.actions[0];base=len(all_arrays);count=8
 clips.append({'name':'flight_breath','base':base,'count':count,'duration':2.,'loop':True,'source_frames':[0,60],'authored_adaptation':'Flying body/wings plus FireBreath neck/head/jaw controls'})
 selected=[b.name for b in arm.pose.bones if b.name.startswith(('neck','jaw'))or b.name=='head']
 constraints=[(c,c.influence)for b in arm.pose.bones for c in b.constraints]
 for i in range(count):
  for c,influence in constraints:c.influence=influence
  t=i/count;arm.animation_data.action=flight_action;bpy.context.scene.frame_set(math.floor(t*60),subframe=t*60%1);bpy.context.view_layer.update()
  flying={b.name:b.matrix.copy()for b in arm.pose.bones}
  # The native head IK reaches eight ancestors, including the chest. Freeze
  # evaluated Flying matrices first, then transfer evaluated neck/jaw local
  # transforms. This guarantees that airborne breathing cannot fold/move wings.
  arm.animation_data.action=breath_action;frame=65.+8.*math.sin(t*2.*math.pi);bpy.context.scene.frame_set(math.floor(frame),subframe=frame%1);bpy.context.view_layer.update()
  breathing={b.name:b.matrix.copy()for b in arm.pose.bones};targets={k:v.copy()for k,v in flying.items()}
  for b in arm.pose.bones:
   if b.name in selected:targets[b.name]=targets[b.parent.name]@breathing[b.parent.name].inverted()@breathing[b.name]
  for c,_ in constraints:c.influence=0.
  pose={}
  for b in arm.pose.bones:
   kwargs={'parent_matrix':targets[b.parent.name],'parent_matrix_local':b.parent.bone.matrix_local}if b.parent else{}
   pose[b.name]=b.bone.convert_local_to_pose(targets[b.name],b.bone.matrix_local,invert=True,**kwargs)
  arrays,record=evaluate(ob,arm,t*60,pose_override=pose)
  for b in arm.pose.bones:
   if b.name not in selected:assert max(abs(v)for row in(b.matrix-targets[b.name])for v in row)<.0001,'Airbreath changed native flying bone '+b.name
  all_arrays.append(arrays);metadata.append(record)
  print('DRAGON_BAKE flight_breath',i,flush=True)
 source.append({'file':str(breath_path.relative_to(ASSET)),'sha256':sha(breath_path),'adapted_controls':selected,'body_source':'Flying','airborne_breath_source_frames':[57,73]})
 entries=[]
 for variant in VARIANTS:
  for clip in clips:
   for i in range(clip['count']):
    slot=clip['base']+i;p,n,uv,indices,tg=all_arrays[slot];path=OUT/f'{variant}_{clip["name"]}_{i:02}.glb'
    write_glb(path,(p,n,uv,tg,indices),variant,f'Purchased {variant} {clip["name"]} source sample {i}')
    entries.append({'file':path.name,'variant':variant,'clip':clip['name'],'sample':i,'slot':slot,'vertices':len(p),'triangles':len(indices),'sha256':sha(path),'bytes':path.stat().st_size})
 def f(v):return format(float(v),'.9g')+('f'if'.'in format(float(v),'.9g')or'e'in format(float(v),'.9g')else'.f')
 def vec(v):return'{'+','.join(f(x)for x in v)+'}'
 inc=['// Generated by tools/import_castle_dragons.py; source scripts disabled.','#define VALWOOD_DRAGON_POSE_COUNT 50','static constexpr WorldDragonClip world_dragon_clips[6]={']
 for c in clips:inc.append('{"%s",%d,%d,%s,%s},'%(c['name'],c['base'],c['count'],f(c['duration']),'true'if c['loop']else'false'))
 inc+=['};','static constexpr WorldDragonPoseDef world_dragon_pose_metadata[VALWOOD_DRAGON_POSE_COUNT]={']
 for r in metadata:inc.append('{%s,%s,%s,%s,%s},'%(vec(r['mouth']),vec(r['direction']),f(r['feet_y']),vec(r['bounds_min']),vec(r['bounds_max'])))
 inc+=['};',''];publish(OUT/'dragon_metadata.inc','\n'.join(inc).encode())
 texture_bytes=len(VARIANTS)*(BASE_SIDE*BASE_SIDE+2*SIDE*SIDE)*4*4//3
 vertex_bytes=sum(e['vertices']*52+e['triangles']*3*4 for e in entries)
 manifest={'schema_version':1,'variants':list(VARIANTS),'pose_count':len(all_arrays),'clips':clips,'poses':metadata,'assets':entries,'sources':source,'source_scripts_executed':False,'source_modified':False,'source_to_engine_basis':BASIS,'source_scale':SCALE,'common_floor_offset_m':FLOOR_OFFSET,'per_pose_floor_alignment':False,'ground_contact':{'landing_carrier_at_ground_by_phase':.44,'takeoff_carrier_leaves_after_phase':.30,'local_negative_tail_and_wingtip_contact':True,'maximum_correction_m':max(r['maximum_contact_correction_m']for r in metadata)},'world_model_load':'world_model_load_pose_origin','collapse_ratio':DECIMATE,'texture_sides':{'basecolor':BASE_SIDE,'normal_opengl':SIDE,'orm':SIDE},'maximum_normal_error':maxnormalerror,'geometry_gpu_bytes':vertex_bytes,'rgba_texture_mip_upper_bound_bytes':texture_bytes,'total_logical_gpu_upper_bound_bytes':vertex_bytes+texture_bytes,'bounds_min':np.min([r['bounds_min']for r in metadata],0).tolist(),'bounds_max':np.max([r['bounds_max']for r in metadata],0).tolist(),'notes':['All native source clips retained, resampled for GPU pose interpolation.','Animated vertical articulation retained; carrier needs support/bounds clearance.','Variant source evaluated geometry matches at beginning, midpoint and final sampled pose of every clip.','Texture cache shares identical embedded images across each variant pose bank.','Geometry/index upper bound includes separate skin variants and u32 uploaded indices.']}
 json_write(OUT/'dragon_manifest.json',manifest)
 print('DRAGON_COOK_PASS',len(all_arrays),'poses per variant',len(entries),'assets','GPU upper bound',vertex_bytes+texture_bytes,flush=True)

def validate():
 import numpy as np
 from PIL import Image
 m=json.loads((OUT/'dragon_manifest.json').read_text());inventory=json.loads((ASSET/'source_inventory.json').read_text())
 for a in inventory['archives']:
  assert sha(a['archive'])==a['archive_sha256']
  for item in a['files']:assert sha(ASSET/item['file'])==item['sha256']
 images={};reference=None;positions={};minimum=1e9;maximum=0.
 for entry in m['assets']:
  path=OUT/entry['file'];raw=path.read_bytes();magic,version,length=struct.unpack_from('<III',raw);assert magic==0x46546c67 and version==2 and length==len(raw)
  jslen,jstype=struct.unpack_from('<II',raw,12);assert jstype==0x4e4f534a;doc=json.loads(raw[20:20+jslen]);binlen,bintype=struct.unpack_from('<II',raw,20+jslen);assert bintype==0x004e4942
  binary=memoryview(raw)[28+jslen:28+jslen+binlen]
  def access(i):
   a=doc['accessors'][i];v=doc['bufferViews'][a['bufferView']];dtype={5126:'<f4',5123:'<u2',5125:'<u4'}[a['componentType']];n={'SCALAR':1,'VEC2':2,'VEC3':3,'VEC4':4}[a['type']]
   return np.frombuffer(binary,dtype=dtype,count=a['count']*n,offset=v.get('byteOffset',0)+a.get('byteOffset',0)).reshape(a['count'],n).copy()
  primitive=doc['meshes'][0]['primitives'][0];attrs=primitive['attributes'];p=access(attrs['POSITION']);n=access(attrs['NORMAL']);uv=access(attrs['TEXCOORD_0']);tg=access(attrs['TANGENT']);idx=access(primitive['indices'])
  assert len(p)==entry['vertices']and len(idx)==entry['triangles']*3 and int(idx.max())<len(p)
  assert all(np.isfinite(x).all()for x in [p,n,uv,tg]);assert abs(np.linalg.norm(n,axis=1)-1).max()<2e-6 and abs(np.linalg.norm(tg[:,:3],axis=1)-1).max()<2e-6
  assert abs((n*tg[:,:3]).sum(1)).max()<2e-5 and np.isin(tg[:,3],[-1,1]).all()
  key=(uv.tobytes(),idx.tobytes(),tg[:,3].tobytes())
  if reference is None:reference=key
  assert key==reference,'Pose correspondence mismatch'
  pose=m['poses'][entry['slot']];assert np.allclose(p.min(0),pose['bounds_min'],atol=2e-6)and np.allclose(p.max(0),pose['bounds_max'],atol=2e-6)
  positions[(entry['variant'],entry['slot'])]=p;minimum=min(minimum,float(p[:,1].min()));maximum=max(maximum,float(p[:,1].max()))
  assert sha(path)==entry['sha256'] and not doc.get('skins')
  for i,image in enumerate(doc['images']):
   view=doc['bufferViews'][image['bufferView']];data=bytes(binary[view.get('byteOffset',0):view.get('byteOffset',0)+view['byteLength']]);digest=hashlib.sha256(data).hexdigest()
   if(entry['variant'],i)in images:assert images[(entry['variant'],i)]==digest
   assert digest==sha(DERIVED/entry['variant']/(MAPS[i]+'.png'))
   import io
   with Image.open(io.BytesIO(data))as decoded:assert decoded.size==((BASE_SIDE,BASE_SIDE)if i==0 else(SIDE,SIDE))
   images[(entry['variant'],i)]=digest
  material=doc['materials'][0];assert material['alphaMode']=='OPAQUE'
 for slot in range(m['pose_count']):assert np.array_equal(positions[('hellfire',slot)],positions[('frostshade',slot)])
 clip_motion={}
 for clip in m['clips']:
  a=positions[('hellfire',clip['base'])];motion=max(float(np.linalg.norm(positions[('hellfire',clip['base']+i)]-a,axis=1).max())for i in range(clip['count']))
  assert motion>.05;clip_motion[clip['name']]=motion
 airborne_wing_error=0.
 for flight_slot,breath_slot in [(0,42),(5,46)]:
  a=positions[('hellfire',flight_slot)];b=positions[('hellfire',breath_slot)];wing=np.abs(a[:,0])>2.
  error=float(np.linalg.norm(a[wing]-b[wing],axis=1).max());airborne_wing_error=max(airborne_wing_error,error);assert error<.00005,'Airborne breath changed native wing pose'
 for clip in m['clips']:
  for i in range(clip['count']):
   pose=m['poses'][clip['base']+i]
   if clip['name']in['idle','breath']or clip['name']=='landing'and i/(clip['count']-1)>=.44:assert pose['bounds_min'][1]>=-1e-7 and abs(pose['feet_y'])<2e-6
 for pose in m['poses']:
  assert abs(np.linalg.norm(pose['direction'])-1)<1e-6 and all(math.isfinite(x)for x in pose['mouth'])
 report={'pass':True,'schema_version':1,'assets':len(m['assets']),'poses_per_variant':m['pose_count'],'vertices':m['assets'][0]['vertices'],'triangles':m['assets'][0]['triangles'],'common_topology_uv_tangent_signs':True,'source_original_hashes_unchanged':True,'source_scripts_executed':False,'skin_variants_geometry_identical':True,'distinct_cached_material_images':len(set(images.values())),'maximum_airbreath_wing_deviation_m':airborne_wing_error,'grounded_contact_floor_pass':True,'all_embedded_images_match_source_derivatives':True,'clip_maximum_vertex_motion_m':clip_motion,'vertical_bounds_m':[minimum,maximum],'gpu_upper_bound_bytes':m['total_logical_gpu_upper_bound_bytes'],'manifest_sha256':sha(OUT/'dragon_manifest.json')}
 json_write(ASSET/'validation.json',report);print(json.dumps(report,indent=2))

def main():
 args=sys.argv[sys.argv.index('--')+1:]if'--'in sys.argv else sys.argv[1:]
 parser=argparse.ArgumentParser();parser.add_argument('--prepare',action='store_true');parser.add_argument('--cook',action='store_true');parser.add_argument('--validate',action='store_true');a=parser.parse_args(args)
 if a.prepare:prepare()
 if a.cook:cook()
 if a.validate:validate()
 if not(a.prepare or a.cook or a.validate):parser.error('Choose --prepare, --cook or --validate')
if __name__=='__main__':main()
