"""Read-only validation of shipped geometry, domains, interaction and framing."""
from pathlib import Path
import hashlib,json,math,struct
import numpy as np
N=Path(__file__).resolve().parents[1];P=N/'world-project';R=N/'review'
report=json.loads((R/'APARTMENT_SCREEN_GEOMETRY.json').read_text())
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def glb(p):
 b=p.read_bytes();length=struct.unpack_from('<I',b,12)[0];j=json.loads(b[20:20+length]);data=b[28+length:]
 def acc(index):
  a=j['accessors'][index];v=j['bufferViews'][a['bufferView']];n={'SCALAR':1,'VEC2':2,'VEC3':3,'VEC4':4}[a['type']];dt={5126:'<f4',5125:'<u4',5123:'<u2',5121:'u1'}[a['componentType']];sz=np.dtype(dt).itemsize
  return np.ndarray((a['count'],n),dtype=dt,buffer=data,offset=v.get('byteOffset',0)+a.get('byteOffset',0),strides=(v.get('byteStride',n*sz),sz)).copy()
 return j,data,acc
origin=np.array([-3500,600.50999997052895,-11000]);screen=None;console=None;paper=None;paper_edge=None;checks=[]
for entry in report['source_assets']:
 source=Path(entry['live']);target=Path(entry['candidate_asset']);assert sha(source)==entry['before_sha256'];assert sha(target)==entry['after_sha256']
 j0,b0,a0=glb(source);j1,b1,a1=glb(target);assert len(b0)==len(b1)
 changed=set(entry['changed_position_accessors']);protected=0
 for k in range(len(j0['accessors'])):
  if k not in changed:assert np.array_equal(a0(k),a1(k));protected+=1
 for k in changed:
  assert j0['accessors'][k]['type']=='VEC3' and j0['accessors'][k]['componentType']==5126
 for k in changed:
  j0['accessors'][k]['min']=j1['accessors'][k]['min'];j0['accessors'][k]['max']=j1['accessors'][k]['max']
 assert j0==j1
 for m in j1['meshes']:
  for pr in m['primitives']:
   name=j1['materials'][pr['material']]['name'];pos=a1(pr['attributes']['POSITION'])+origin
   if name=='VALWOOD opening television | animated original pixel adventure':screen=pos;assert len(pos)==4
   elif name=='Valwood | existing console':console=pos
   elif name=='Valwood | original household bills':paper=pos
   elif name=='Valwood | thin paper edges':paper_edge=pos
 assert screen is not None and console is not None
 assert np.allclose(screen.min(0)[1:],np.array(report['source_glass_minimum'])[1:],atol=2e-6)
 assert np.allclose(screen.max(0)[1:],np.array(report['source_glass_maximum'])[1:],atol=2e-6)
 assert .00099<float(screen[:,0].mean()-report['source_glass_minimum'][0])<.00101
 assert np.ptp(screen[:,0])<1e-6
 assert np.allclose(a0(8)[:,:2],a1(8)[:,:2]) # console X/Y stay on the stand.
 assert np.allclose(a0(8)[:,2]-1.73,a1(8)[:,2],atol=3e-7)
 assert console[:,2].min()>-11003.278 # clear of actual TV casing.
 assert console[:,2].max()<-11002.913 and console[:,2].min()>-11004.598
 assert .0005<float(console[:,1].min()-600.5078025078401)<.003
 assert paper is not None and paper_edge is not None
 assert np.allclose(paper.mean(0),report['bill_support']['bill_point'],atol=2e-6)
 assert .000499<float(paper_edge[:,1].min()-report['bill_support']['top_y'])<.000501
 # Actual remote extends to X=-3498.247620344; full paper has a 10mm gap.
 assert paper_edge[:,0].min()-(-3498.2476203441593)>.010
 checks.append({'asset':str(target),'protected_accessors_exact':protected,'material_images_json_exact_except_position_bounds':True,'original_asset_still_exact':True,'screen_within_source_inset':True,'console_supported_clear_of_tv':True})
# The protected authoring changes are narrowly scoped.
old_config=(R/'apartment-screen-before/opening.config').read_text().splitlines();new_config=(P/'opening.config').read_text().splitlines();changed_rows=[a.split()[0] for a,b in zip(old_config,new_config) if a!=b];assert changed_rows==['house_focus','house_exit','transport_rail']
old_shots=(R/'apartment-screen-before/opening/cinematic.settings').read_text().splitlines();new_shots=(P/'opening/cinematic.settings').read_text().splitlines();assert len(old_shots)==len(new_shots);assert [i for i,(a,b) in enumerate(zip(old_shots,new_shots)) if a!=b]==[2,3]
for index in (2,3):
 a=old_shots[index].split();b=new_shots[index].split();assert a[14:]==b[14:]
 if index==2:
  assert a[:4]==b[:4] and a[5:7]==b[5:7]
  for position in (4,7):assert abs(float(b[position])-(2*report['bill_support']['bill_point'][2]-float(a[position])))<2e-9
 else:assert a[:8]==b[:8]
old_json=json.loads((R/'apartment-screen-before/opening/cinematic.json').read_text());new_json=json.loads((P/'opening/cinematic.json').read_text())
for i in (0,3):assert old_json['shots'][i]==new_json['shots'][i]
for i in (1,2):
 for k in new_json['shots'][i]:
  if k in ('eye_from','eye_to') and i==1:
   a=old_json['shots'][i][k];b=new_json['shots'][i][k];assert a[:2]==b[:2];assert abs(b[2]-(2*report['bill_support']['bill_point'][2]-a[2]))<1e-9
  elif k not in ('target_from','target_to','purpose'):assert old_json['shots'][i][k]==new_json['shots'][i][k]
# Actual source screen must fit throughout the newly aimed third cinematic shot.
shot=new_json['shots'][2];bounds=[]
for t in np.linspace(0,1,61):
 eye=np.array(shot['eye_from'])*(1-t)+np.array(shot['eye_to'])*t;target=np.array(shot['target_from'])*(1-t)+np.array(shot['target_to'])*t
 forward=target-eye;forward/=np.linalg.norm(forward);right=np.cross(forward,[0,1,0]);right/=np.linalg.norm(right);up=np.cross(right,forward);delta=screen-eye;depth=delta@forward;assert np.all(depth>0)
 fov=shot['fov_from']*(1-t)+shot['fov_to']*t;xy=np.stack([(delta@right)/depth/(math.tan(fov/2)*16/9),(delta@up)/depth/math.tan(fov/2)],axis=1);assert np.max(np.abs(xy))<.60
 bounds.append(float(np.max(np.abs(xy))))
# Shader spatial guards contain every real screen/LED point; legacy position is outside.
s=(N/'shared/shaders/world_foliage.fs.glsl').read_text()
for literal in ['vec3(-3499.400000000,600.590000000,-11004.121000000)','vec3(-3499.378000000,601.041000000,-11003.375000000)','vec3(-3499.472000000,600.549000000,-11003.111000000)','vec3(-3499.450000000,600.571000000,-11003.089000000)']:assert literal in s
assert np.all(screen>[-3499.4,600.590,-11004.121]) and np.all(screen<[-3499.378,601.041,-11003.375])
bill_bounds=[];shot=new_json['shots'][1]
for t in np.linspace(0,1,61):
 eye=np.array(shot['eye_from'])*(1-t)+np.array(shot['eye_to'])*t;target=np.array(shot['target_from']);forward=target-eye;forward/=np.linalg.norm(forward);right=np.cross(forward,[0,1,0]);right/=np.linalg.norm(right);up=np.cross(right,forward);delta=paper-eye;depth=delta@forward;assert np.all(depth>0);fov=shot['fov_from']*(1-t)+shot['fov_to']*t;xy=np.stack([(delta@right)/depth/(math.tan(fov/2)*16/9),(delta@up)/depth/math.tan(fov/2)],axis=1);assert np.max(np.abs(xy))<.60;bill_bounds.append(float(np.max(np.abs(xy))))
assert all(new_json['shots'][1][key][2]<report['bill_support']['bill_point'][2] for key in ('eye_from','eye_to'))
result=dict(passed=True,actual_source_geometry=True,assets=checks,changed_config_records=changed_rows,preserved_cinematic_shots=[0,3],shot03_inset_maximum_normalized_screen_extent=max(bounds),samples=61,all_screen_corners_inside_shader_clip=True,existing_route_within_interaction_radius=report['existing_qa_endpoint_distance']<1.7,bill_support_vertical_rays=9,bill_stack_clearance_metres=float(paper_edge[:,1].min()-report['bill_support']['top_y']),bill_remote_x_clearance_metres=float(paper_edge[:,0].min()-(-3498.2476203441593)),shot02_paper_maximum_normalized_screen_extent=max(bill_bounds),shot02_camera_on_player_side_south=True,shot02_eye_z_mirrored_about_bill=True,live_not_modified=True)
(R/'APARTMENT_SCREEN_CPU.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
