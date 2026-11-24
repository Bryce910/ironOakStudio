from pathlib import Path
import argparse,json,hashlib,numpy as np,struct
P=Path(__file__).resolve().parent
parser=argparse.ArgumentParser();parser.add_argument('character');args=parser.parse_args();ident=args.character;BANK=P/'arena_orcs_roster'/ident;SOURCE=P/'intake_roster'/ident

def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
manifest=BANK/'body/combat_motion_manifest.json';m=json.loads(manifest.read_text());exchange=BANK/'body-semantic.npz';inputs={str(manifest.resolve()):sha(manifest),str(exchange.resolve()):sha(exchange),str(Path(__file__).resolve()):sha(__file__)}
for path,h in m['source_files'].items():assert sha(path)==h,('Source changed',path);inputs[path]=h
for path,h in m['generator'].items():assert sha(path)==h,('Generator changed',path);inputs[path]=h
with np.load(exchange,allow_pickle=False)as d:
 names=list(map(str,d['bone_names']));parents=d['bone_parent_indices'].copy();rest=d['rest_joints'].copy();joints=d['joints'].copy();rigid=[i for i in range(len(m['primitive_names']))if bool(d[f'rigid_{i}'])]
 lengths={i:np.linalg.norm(rest[i]-rest[p])/m['scale']for i,p in enumerate(parents[:-1])if p>=0 and names[i]!='pelvis'}
 source_rows=[];source_error=0.;pose_error=0.
 for key,info in m['source_clips'].items():
  f=SOURCE/'pose-source-cache'/(Path(info['source']).stem+'.npy');a=np.load(f,allow_pickle=False);meta=f.with_suffix('.json');mdata=json.loads(meta.read_text());assert mdata['retarget_identity']==m['retarget_identity'];assert a.shape==(121,len(names),4,4)
  error=max(abs(np.linalg.norm(a[:,i,:3,3]-a[:,parents[i],:3,3],axis=1)-length).max()for i,length in lengths.items());source_error=max(source_error,float(error));source_rows.append({'source':key,'samples':121,'max_parent_child_length_error_m':float(error)});inputs[str(f.resolve())]=sha(f);inputs[str(meta.resolve())]=sha(meta)
 for i,length in lengths.items():pose_error=max(pose_error,float(abs(np.linalg.norm(joints[:,i]-joints[:,parents[i]],axis=1)/m['scale']-length).max()))
 assert source_error<3.e-6 and pose_error<.0001,(source_error,pose_error)
 for i in range(len(m['primitive_names'])):
  w=d[f'weights_{i}'];raw=d[f'weights_raw_{i}'];assert np.isfinite(w).all()and np.isfinite(raw).all()and(w>=0).all()and(raw>=0).all()
  if i in rigid:assert not np.any(w)and not np.any(raw)
  else:assert np.max(abs(w.sum(1)-1.))<2.e-6

def arr(data,doc,ai):
 a=doc['accessors'][ai];v=doc['bufferViews'][a['bufferView']];typ={5126:'<f4',5123:'<u2',5125:'<u4'}[a['componentType']];width={'SCALAR':1,'VEC2':2,'VEC3':3,'VEC4':4}[a['type']];return np.frombuffer(data,dtype=typ,count=a['count']*width,offset=v.get('byteOffset',0)+a.get('byteOffset',0)).reshape(-1,width)
base=None;materials=None;source_objects=None
for i in range(172):
 f=BANK/f'body/combat_body_{i:03}.glb';raw=f.read_bytes();n=struct.unpack_from('<I',raw,12)[0];doc=json.loads(raw[20:20+n]);blob=raw[28+n:];layout=[];objects=[]
 for pi,primitive in enumerate(doc['meshes'][0]['primitives']):
  a={key:arr(blob,doc,ai)for key,ai in primitive['attributes'].items()};indices=arr(blob,doc,primitive['indices']);assert all(np.isfinite(v).all()for v in a.values());assert np.max(indices)<len(a['POSITION']);assert np.min(np.linalg.norm(a['NORMAL'],axis=1))>.99;assert np.max(abs(np.linalg.norm(a['NORMAL'],axis=1)-1.))<.001
  layout.append([len(a['POSITION']),len(indices),sha_bytes(a['TEXCOORD_0'].tobytes())if False else hashlib.sha256(a['TEXCOORD_0'].tobytes()).hexdigest(),hashlib.sha256(indices.tobytes()).hexdigest(),hashlib.sha256(a['TANGENT'][:,3].tobytes()).hexdigest(),primitive['material']]);objects.append(primitive['extras']['source_object']);marker=doc['materials'][primitive['material']]['name'];assert (marker=='Owned Unity Orc roster weapon')==(pi in rigid),(ident,pi,marker,rigid)
 if base is None:base=layout;materials=doc['materials'];source_objects=objects
 assert layout==base and materials==doc['materials']and source_objects==objects,('Changing pose layout',ident,i)
 inputs[str(f.resolve())]=sha(f)
uv=SOURCE/'SOURCE_TO_COOKED_UV.json';profile=SOURCE/'NEUTRAL_RIG_PROFILE.json'
for f in [uv,profile,BANK/'MOTION_CURVE_REVIEW.json',BANK/'COOK_SOURCE.json',BANK/'ORC_MATERIAL_MAPPING.json']:
 d=json.loads(f.read_text());assert d.get('passed',True),f;inputs[str(f.resolve())]=sha(f)
report={'schema':'valwood.orc.roster-matching-motion-retarget-review.v1','passed':True,'character_id':ident,'inputs':inputs,'actual_target_bones':len(names)-1,'explicit_virtual_held_frames':['Weapon1'],'source_sample_count':sum(r['samples']for r in source_rows),'maximum_original_limb_length_error_m':source_error,'maximum_baked_limb_length_error_m':pose_error,'source_length_tolerance_m':3.e-6,'baked_float_matrix_length_tolerance_m':.0001,'excluded_motion_anchor_link':'Source animated pelvis-to-root displacement is root motion, not an anatomical limb length','clips':source_rows,'body_poses':172,'source_mesh_primitives':len(base),'rigid_weapon_primitives':rigid,'immutable_uv_index_tangent_sign_material_layout':True,'layout':base,'actual_source_weights_normalized_without_ratio_changes':True,'source_origin_policy':m['pose_origin_policy']}
assert report['source_origin_policy']=='preserve';(SOURCE/'RETARGET_AND_LAYOUT_REVIEW.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({k:v for k,v in report.items()if k not in ['inputs','layout','clips']},indent=2))
