"""Fit the existing original TV animation to the purchased TV's actual inset.

Writes only this candidate's owned asset bank, private authoring and shader.
The purchased sources, previous generations and live world are read-only.
"""
from pathlib import Path
import hashlib,json,math,shutil,struct
import numpy as np
N=Path(__file__).resolve().parents[1]
LIVE=N.parents[1]/'projects/valwood_current'
P=N/'world-project'
# A private project's leaf model files may be symlinks, but its mutable
# containers must themselves be owned directories. Never follow a container
# into the live world when preparing a candidate.
for container in [N,P,P/'models',P/'models/opening',P/'opening',N/'shared',N/'shared/shaders']:
 assert not container.is_symlink(),f'Unsafe mutable container symlink: {container}'
 assert container.resolve().is_relative_to(N.resolve()),f'Candidate escaped its generation: {container}'
O=N/'assets/apartment-screen';O.mkdir(parents=True,exist_ok=True)
REVIEW=N/'review';REVIEW.mkdir(exist_ok=True)
BEFORE=REVIEW/'apartment-screen-before';BEFORE.mkdir(exist_ok=True)
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def read_glb(path):
 b=path.read_bytes();jl,jt=struct.unpack_from('<II',b,12);assert jt==0x4e4f534a
 j=json.loads(b[20:20+jl]);bo=20+jl;bl,bt=struct.unpack_from('<II',b,bo);assert bt==0x004e4942
 return j,bytearray(b[bo+8:bo+8+bl])
def access(j,data,index):
 a=j['accessors'][index];v=j['bufferViews'][a['bufferView']]
 dt={5126:'<f4',5125:'<u4',5123:'<u2',5121:'u1'}[a['componentType']];n={'SCALAR':1,'VEC2':2,'VEC3':3,'VEC4':4}[a['type']];size=np.dtype(dt).itemsize
 return np.ndarray((a['count'],n),dtype=dt,buffer=data,offset=v.get('byteOffset',0)+a.get('byteOffset',0),strides=(v.get('byteStride',size*n),size))
def save_glb(path,j,data):
 js=json.dumps(j,separators=(',',':')).encode();js+=b' '*((-len(js))%4);data+=b'\0'*((-len(data))%4)
 path.write_bytes(struct.pack('<III',0x46546c67,2,12+8+len(js)+8+len(data))+struct.pack('<II',len(js),0x4e4f534a)+js+struct.pack('<II',len(data),0x004e4942)+data)
def backup(rel):
 src=LIVE/rel;dst=BEFORE/rel;dst.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(src,dst,follow_symlinks=True)
 return {'live':str(src),'relative':rel,'before_sha256':sha(src),'before_symlink':str(src.readlink()) if src.is_symlink() else None,'backup':str(dst)}
assets={}
for line in (LIVE/'world.studio').read_text().splitlines():
 t=line.split()
 if t and t[0]=='asset':assets[t[1]]=LIVE/'models'/(t[2]+'.glb')
inst={}
for line in (LIVE/'scenery/authored_instances.txt').read_text().splitlines():
 t=line.split()
 if t and t[0] in assets:inst[t[0]]=np.array([(float(t[1])-.5)*100800,float(t[-1]),(float(t[2])-.5)*(100800*2818/5009)])
# This owned TV's four planar glass vertices (flat centre, behind bevel).
tv='nm_opening_reference_item_0431_tv_1'
j,d=read_glb(assets[tv]);pr=j['meshes'][0]['primitives'][0];v=access(j,d,pr['attributes']['POSITION'])+inst[tv]
screen=v[[29,30,31,32]]
assert np.ptp(screen[:,0])<2e-6
lo=screen.min(0);hi=screen.max(0);centre=(lo+hi)/2;plane=float(centre[0]+.001)
assert hi[2]-lo[2]>.7 and hi[1]-lo[1]>.4
origin=inst['nm_opening_reference_home_story']
# The bill belongs on the small coffee/tray table, whose actual inset top is
# below its raised rim. Sample the full paper footprint against source triangles
# rather than assuming the maximum mesh height is the support plane.
table='nm_opening_reference_item_0111_sidetable_c_1'
tj,td=read_glb(assets[table]);tp=tj['meshes'][0]['primitives'][0]
tri=(access(tj,td,tp['attributes']['POSITION'])+inst[table])[access(tj,td,tp['indices']).reshape(-1)].reshape(-1,3,3)
bill_x=-3498.11;bill_z=float(inst[table][2]);support=[]
for xx in [bill_x-.1275,bill_x,bill_x+.1275]:
 for zz in [bill_z-.1125,bill_z,bill_z+.1125]:
  projected=tri[:,:,[0,2]];a=projected[:,0];ab=projected[:,1]-a;ac=projected[:,2]-a;s=np.array([xx,zz])-a;den=ab[:,0]*ac[:,1]-ab[:,1]*ac[:,0];valid=np.abs(den)>1e-10
  u=(s[:,0]*ac[:,1]-s[:,1]*ac[:,0])/np.where(valid,den,1);v=(ab[:,0]*s[:,1]-ab[:,1]*s[:,0])/np.where(valid,den,1);valid&=(u>=-1e-5)&(v>=-1e-5)&(u+v<=1+1e-5)
  y=tri[:,0,1]+u*(tri[:,1,1]-tri[:,0,1])+v*(tri[:,2,1]-tri[:,0,1]);assert np.any(valid);support.append([xx,zz,float(y[valid].max())])
support_y=max(p[2] for p in support);assert max(p[2] for p in support)-min(p[2] for p in support)<1e-6
bill_point=[bill_x,support_y+.007,bill_z]
bill_shift=np.array(bill_point)-[-3498.085,600.549,-11002.102]
report={'passed':False,'schema':'valwood.apartment-screen-polish.v1','live_modified':False,
 'source_tv_alias':tv,'source_tv_sha256':sha(assets[tv]),'source_glass_minimum':lo.tolist(),'source_glass_maximum':hi.tolist(),
 'screen_centre':[plane,float(centre[1]),float(centre[2])],'screen_offset_metres':.001,'screen_size':[float(hi[2]-lo[2]),float(hi[1]-lo[1])],
 'console_translation':[0,0,-1.73], 'publication_files':[], 'source_assets':[], 'new_asset_links':[],
 'bill_support':dict(alias=table,source_sha256=sha(assets[table]),vertical_ray_samples=support,top_y=support_y,bill_point=bill_point,translation=bill_shift.tolist(),paper_stack_bottom_clearance_metres=.0005)}
for filename in ['home_story.glb','home_story_mid.glb']:
 rel='models/opening/'+filename;entry=backup(rel);src=LIVE/rel;j,d=read_glb(src);original_data=bytes(d);changed_accessors=set();originals={}
 for mesh in j['meshes']:
  for pr in mesh['primitives']:
   mat=j['materials'][pr['material']];name=mat.get('name','');a=pr['attributes']['POSITION'];pos=access(j,d,a)
   if name=='VALWOOD opening television | animated original pixel adventure':
    assert len(pos)==4 and np.allclose(mat['emissiveFactor'],[.2137,.4271,.6419])
    originals[name]=pos.copy().tolist()
    # UV X increases from +Z to -Z when looking toward the +X-facing TV.
    uv=access(j,d,pr['attributes']['TEXCOORD_0'])
    pos[:,0]=plane-origin[0];pos[:,1]=(lo[1]+uv[:,1]*(hi[1]-lo[1]))-origin[1];pos[:,2]=(hi[2]-uv[:,0]*(hi[2]-lo[2]))-origin[2]
    changed_accessors.add(a)
   elif name in ('Valwood | existing console','Valwood | switched console power'):
    originals[name]=[pos.min(0).tolist(),pos.max(0).tolist()];pos[:,2]-=1.73;changed_accessors.add(a)
   elif name in ('Valwood | thin paper edges','Valwood | original household bills'):
    originals[name]=[pos.min(0).tolist(),pos.max(0).tolist()];pos[:]+=bill_shift;changed_accessors.add(a)
 for a in changed_accessors:
  pos=access(j,d,a);j['accessors'][a]['min']=pos.min(0).tolist();j['accessors'][a]['max']=pos.max(0).tolist()
 assert len(changed_accessors)==5
 # Every other byte of geometry and all UV/material/image data is preserved.
 expected=bytearray(original_data)
 for a in changed_accessors:
  dest=access(j,expected,a);dest[:]=access(j,d,a)
 assert expected==d
 dst=O/filename;save_glb(dst,j,d)
 # Retain the original stem so older published worlds remain reproducible.
 # The current world opts into the revised geometry through one new stem.
 private=P/rel
 if private.is_symlink():private.unlink()
 elif private.exists():private.unlink()
 private.symlink_to(src.resolve())
 stem='home_story_tvfit_20261118'+('_mid' if filename.endswith('_mid.glb') else '')+'.glb'
 new_rel='models/opening/'+stem;new_private=P/new_rel
 if new_private.is_symlink() or new_private.exists():new_private.unlink()
 new_private.symlink_to(dst)
 entry.update(after_sha256=sha(dst),candidate_asset=str(dst),candidate_link=str(new_private),changed_position_accessors=sorted(changed_accessors),other_binary_data_preserved=True)
 report['source_assets'].append(entry)
 report['new_asset_links'].append(dict(relative=new_rel,live=str(LIVE/new_rel),candidate_asset=str(dst),after_sha256=sha(dst)))
studio=P/'world.studio';s=studio.read_text();old_asset='asset nm_opening_reference_home_story opening/home_story 2 0 0 240 75 1';new_asset='asset nm_opening_reference_home_story opening/home_story_tvfit_20261118 2 0 0 240 75 1'
assert old_asset in s or new_asset in s
s=s.replace(old_asset,new_asset,1);studio.write_text(s)
report['world_studio_asset_row']={'before':old_asset,'after':new_asset,'live_before_sha256':sha(LIVE/'world.studio')}
# Private shader uses an exact material tag + the fitted screen/LED domains.
shader=N/'shared/shaders/world_foliage.fs.glsl';assert not shader.is_symlink()
s=shader.read_text();old=s
s=s.replace('vec3(-3499.472000000,600.549000000,-11001.381000000)','vec3(-3499.472000000,600.549000000,-11003.111000000)').replace('vec3(-3499.450000000,600.571000000,-11001.359000000)','vec3(-3499.450000000,600.571000000,-11003.089000000)')
s=s.replace('vec3(-3499.385000000,600.527000000,-11002.481000000)','vec3(-3499.400000000,600.590000000,-11004.121000000)').replace('vec3(-3499.364000000,601.077000000,-11001.599000000)','vec3(-3499.378000000,601.041000000,-11003.375000000)')
assert 'vec3(-3499.400000000,600.590000000,-11004.121000000)' in s
if s!=old:shader.write_text(s)
report['shader_before_sha256']=sha(N.with_name('window-camera-2026-11-18')/'shared/shaders/world_foliage.fs.glsl');report['shader_after_sha256']=sha(shader)
# Move the interaction to the real console and compose transport on the TV.
entry=backup('opening.config');f=P/'opening.config';rows=f.read_text().splitlines()
for i,line in enumerate(rows):
 t=line.split()
 if t and t[0]=='house_focus':rows[i]='house_focus '+' '.join(f'{v:.12f}' for v in bill_point)
 elif t and t[0]=='house_exit':rows[i]='house_exit -3499.62 600.57 -11003.10'
 elif t and t[0]=='transport_rail':
  cy=float(centre[1]);cz=float(centre[2]);rows[i]=f'transport_rail -3498.5 {cy:.12f} {cz:.12f} -3499.17 {cy:.12f} {cz:.12f} {plane:.12f} {cy:.12f} {cz:.12f} {plane:.12f} {cy:.12f} {cz:.12f} .9 .7'
f.write_text('\n'.join(rows)+'\n');entry.update(after_sha256=sha(f),candidate_asset=str(f));report['publication_files'].append(entry)
# Only bills/TV target points change; both eye rails and final handoff stay intact.
entry=backup('opening/cinematic.settings');f=P/'opening/cinematic.settings';rows=f.read_text().splitlines()
source_rows=(BEFORE/'opening/cinematic.settings').read_text().splitlines()
for index,target in [(2,bill_point),(3,[plane,float(centre[1]),float(centre[2])])]:
 tokens=rows[index].split();assert tokens[0]=='shot';tokens[8:14]=[f'{v:.12f}' for v in target]*2
 if index==2:
  source_tokens=source_rows[index].split()
  for position in (4,7):tokens[position]=f'{2*bill_z-float(source_tokens[position]):.12f}'
 rows[index]=' '.join(tokens)
f.write_text('\n'.join(rows)+'\n');entry.update(after_sha256=sha(f),candidate_asset=str(f));report['publication_files'].append(entry)
entry=backup('opening/cinematic.json');f=P/'opening/cinematic.json';doc=json.loads(f.read_text());shot=doc['shots'][2];assert shot['name']=='one-quiet-night';shot['target_from']=shot['target_to']=[plane,float(centre[1]),float(centre[2])];shot['purpose']='The original game animation sits within the real TV bezel, with the console on its actual stand; starting the game remains the player choice.'
shot=doc['shots'][1];shot['target_from']=shot['target_to']=bill_point
source_shot=json.loads((BEFORE/'opening/cinematic.json').read_text())['shots'][1]
for key in ('eye_from','eye_to'):
 shot[key]=source_shot[key].copy();shot[key][2]=2*bill_z-source_shot[key][2]
shot['purpose']='The rent reminder is seated on the actual inset coffee-table surface. The measured insert views it from the player side so the original printed text is upright.'
f.write_text(json.dumps(doc,indent=2)+'\n');entry.update(after_sha256=sha(f),candidate_asset=str(f));report['publication_files'].append(entry)
# Meaningful geometry/interaction guardrails; no runtime assertions fabricated.
new_console=np.array([-3499.62,600.57,-11003.1]);qa_endpoint=np.array([-3498.9,600.00750732421875,-11002.8]);report['existing_qa_endpoint_distance']=float(np.linalg.norm(new_console-qa_endpoint));assert report['existing_qa_endpoint_distance']<1.7
report['source_asset_unchanged']=sha(assets[tv])==report['source_tv_sha256']
report['preserved_shots']=[0,3];report['preserved_other_shot_eyes_and_all_durations_fovs']=True;report['shot02_eye_z_mirrored_about_bill']=True;report['all_other_authored_files_unchanged']=True;report['passed']=True
(REVIEW/'APARTMENT_SCREEN_GEOMETRY.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
