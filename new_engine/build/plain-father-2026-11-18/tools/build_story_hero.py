#!/usr/bin/env python3
"""New owned clothing derivatives. Original runtime/skeleton/animations stay read-only.
Unity geometry is a separately copied, private BakeMesh export, with an exact
zero-morph CPU-skinning baseline below 1 micron. No purchased source is edited.
"""
from pathlib import Path
import sys,json,copy,hashlib,io
import numpy as np
from PIL import Image
BASE=Path(__file__).resolve().parents[1]
ENGINE=BASE.parents[1]
sys.path.insert(0,str(ENGINE/'tools'))
import finalize_modular_knight_glb as glb
SOURCE=ENGINE/'build/cinematic-studio-2026-11-15/equipment-assets/rigs/sword.glb'
INTAKE=BASE/'assets/plain-father'
OUT=BASE/'assets/story_hero'
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def accessor(d,b,a,kind='VEC3',comp=5126):
 a=np.ascontiguousarray(a,dtype={5126:'<f4',5123:'<u2',5125:'<u4'}[comp]);i=glb.append_accessor(d,b,a,{'componentType':comp,'type':kind})
 if kind=='VEC3':d['accessors'][i].update(min=a.min(0).tolist(),max=a.max(0).tolist())
 return i
def compact(d,b):
 # Keep exact skeleton nodes and bind palette while eliminating unused armored
 # materials/images, geometry, weapons and all motion samples from body-only GLB.
 used_a={s['inverseBindMatrices'] for s in d['skins']}
 used_m=set()
 for m in d['meshes']:
  for p in m['primitives']:
   used_a.update(p['attributes'].values());used_a.add(p['indices']);used_m.add(p['material'])
 for an in d.get('animations',[]):
  for s in an['samplers']:used_a.update([s['input'],s['output']])
 m_map={old:i for i,old in enumerate(sorted(used_m))}; mats=[copy.deepcopy(d['materials'][i]) for i in sorted(used_m)]
 used_t=set()
 def textures(obj,edit=None):
  if isinstance(obj,dict):
   for k,v in obj.items():
    if k.endswith('Texture') and isinstance(v,dict) and 'index'in v:
     if edit is None:used_t.add(v['index'])
     else:v['index']=edit[v['index']]
    else:textures(v,edit)
  elif isinstance(obj,list):
   for x in obj:textures(x,edit)
 for m in mats:textures(m)
 t_map={old:i for i,old in enumerate(sorted(used_t))}; ts=[copy.deepcopy(d['textures'][i]) for i in sorted(used_t)]
 for m in mats:textures(m,t_map)
 used_i={t['source'] for t in ts};i_map={old:i for i,old in enumerate(sorted(used_i))}; ims=[copy.deepcopy(d['images'][i]) for i in sorted(used_i)]
 for t in ts:t['source']=i_map[t['source']]
 a_map={old:i for i,old in enumerate(sorted(used_a))}; acs=[copy.deepcopy(d['accessors'][i]) for i in sorted(used_a)]
 used_v={a['bufferView'] for a in acs}|{im['bufferView'] for im in ims}
 v_map={old:i for i,old in enumerate(sorted(used_v))}; bb=bytearray();vs=[]
 for old in sorted(used_v):
  v=copy.deepcopy(d['bufferViews'][old]);payload=b[v.get('byteOffset',0):v.get('byteOffset',0)+v['byteLength']];bb.extend(b'\0'*(-len(bb)%4));v.update(buffer=0,byteOffset=len(bb));bb.extend(payload);vs.append(v)
 for a in acs:a['bufferView']=v_map[a['bufferView']]
 for im in ims:im['bufferView']=v_map[im['bufferView']]
 for s in d['skins']:s['inverseBindMatrices']=a_map[s['inverseBindMatrices']]
 for m in d['meshes']:
  for p in m['primitives']:p['attributes']={k:a_map[v] for k,v in p['attributes'].items()};p['indices']=a_map[p['indices']];p['material']=m_map[p['material']]
 for an in d.get('animations',[]):
  for s in an['samplers']:s['input']=a_map[s['input']];s['output']=a_map[s['output']]
 d.update(accessors=acs,bufferViews=vs,materials=mats,textures=ts,images=ims)
 return d,bb

def source_material(d,b,row,name,*,dye=None,minrough=.68):
 # Exact purchased skin conversion; garment dye is an explicitly authored color
 # adjustment retaining source cloth weave, seams and folds (no metal).
 def embed(path,role):
  data=Path(path).read_bytes()
  if role=='base' and dye is not None:
   im=np.asarray(Image.open(io.BytesIO(data)).convert('RGB')).astype(float)/255
   luminance=im@np.array([.2126,.7152,.0722]);factor=np.clip(luminance/.62,.30,1.35)
   im=np.clip(factor[:,:,None]*np.array(dye)[None,None,:],0,1)
   out=io.BytesIO();Image.fromarray(np.rint(im*255).astype('uint8'),'RGB').save(out,format='JPEG',quality=94);data=out.getvalue()
  if role=='orm' and dye is not None:
   im=np.asarray(Image.open(io.BytesIO(data)).convert('RGB')).copy();im[:,:,1]=np.maximum(im[:,:,1],round(minrough*255));im[:,:,2]=0
   out=io.BytesIO();Image.fromarray(im,'RGB').save(out,format='PNG');data=out.getvalue()
  idx=len(d.setdefault('images',[]));d['images'].append({'name':name+'_'+role,'bufferView':glb.append_view(d,b,data),'mimeType':'image/jpeg' if data[:2]==b'\xff\xd8' else 'image/png'})
  tex=len(d.setdefault('textures',[]));d['textures'].append({'source':idx,'sampler':0});return tex
 mat={'name':name,'doubleSided':False,'pbrMetallicRoughness':{'baseColorFactor':[1,1,1,1],'baseColorTexture':{'index':embed(row['base'],'base')},'metallicFactor':0 if dye else 1,'roughnessFactor':1,'metallicRoughnessTexture':{'index':embed(row['orm'],'orm')}},'normalTexture':{'index':embed(row['normal'],'normal'),'scale':.65 if dye else row['normal_scale']}}
 mat['occlusionTexture']={'index':mat['pbrMetallicRoughness']['metallicRoughnessTexture']['index']}
 idx=len(d['materials']);d['materials'].append(mat);return idx

def softened_normals(points,normals,transform):
 # Derive the actual nonlinear rest-shape Jacobian, keeping normals consistent
 # with modest clothing/footwear shaping instead of retaining wrong highlights.
 result=transform(points)
 columns=[]
 for k in range(3):
  offset=np.zeros_like(points);offset[:,k]=.0001
  columns.append((transform(points+offset)-transform(points-offset))/.0002)
 jac=np.stack(columns,axis=2)
 updated=np.einsum('nij,nj->ni',np.linalg.inv(jac).transpose(0,2,1),normals)
 updated/=np.linalg.norm(updated,axis=1)[:,None]
 return result,updated

def loosen_trousers(points,normal,d,b):
 ib=glb.array(d,b,d['skins'][0]['inverseBindMatrices']).reshape(-1,4,4).transpose(0,2,1)
 named={d['nodes'][n]['name']:np.linalg.inv(ib[i])[:3,3] for i,n in enumerate(d['skins'][0]['joints'])}
 def transform(a):
  out=a.copy()
  for side,positive in [('l',True),('r',False)]:
   mask=a[:,0]>0 if positive else a[:,0]<=0;q=a[mask];y=q[:,1]
   joints=np.array([named[k+'_'+side] for k in ['foot','calf','thigh']]);order=np.argsort(joints[:,1]);joints=joints[order]
   cx=np.interp(y,joints[:,1],joints[:,0]);cz=np.interp(y,joints[:,1],joints[:,2])
   ys=[.055,.075,.10,.20,.35,.52,.70,.89,1.01]
   sx=np.interp(y,ys,[1,1.05,1.88,1.63,1.23,1.14,1.09,1.015,1])
   sz=np.interp(y,ys,[1,1.03,1.40,1.30,1.12,1.08,1.05,1.005,1])
   out[mask,0]=cx+(q[:,0]-cx)*sx;out[mask,2]=cz+(q[:,2]-cz)*sz
   # Full source hose includes stocking feet. Keep those strictly inside the
   # new rounded low shoes, rather than leaving a visible blue pointed toe.
   forward=named['ball_'+side]-named['foot_'+side];forward[1]=0;forward/=np.linalg.norm(forward);lateral=np.cross([0,1,0],forward)
   offset=q-named['foot_'+side];f=offset@forward;t=offset@lateral
   foot=np.clip((.115-y)/.045,0,1);foot=foot*foot*(3-2*foot)
   cap=.190-12*t*t;delta=np.minimum(f-cap,0)+.12*np.maximum(f-cap,0)+cap-f
   out[mask]+=foot[:,None]*(delta[:,None]*forward-(t*.08)[:,None]*lateral)
   out[mask,1]+=.006*foot
  return out
 return softened_normals(points,normal,transform)

def rounded_shoes(d,b,primitive):
 p=glb.array(d,b,primitive['attributes']['POSITION']);n=glb.array(d,b,primitive['attributes']['NORMAL'])
 ib=glb.array(d,b,d['skins'][0]['inverseBindMatrices']).reshape(-1,4,4).transpose(0,2,1)
 named={d['nodes'][v]['name']:np.linalg.inv(ib[i])[:3,3] for i,v in enumerate(d['skins'][0]['joints'])}
 def smooth(x):x=np.clip(x,0,1);return x*x*(3-2*x)
 def transform(a):
  out=a.copy()
  for side,positive in [('l',True),('r',False)]:
   mask=a[:,0]>0 if positive else a[:,0]<=0;q=a[mask];origin=named['foot_'+side];forward=named['ball_'+side]-origin;forward[1]=0;forward/=np.linalg.norm(forward);lateral=np.cross([0,1,0],forward)
   offset=q-origin;f=offset@forward;t=offset@lateral;weight=smooth((f-.14)/.14)
   # Round the exaggerated pointed tip with a convex toe envelope, broaden its toe-box and
   # leave heel, sole height, ankle fit and foot weights unchanged.
   cap=.21-12*t*t
   delta=np.minimum(f-cap,0)+.15*np.maximum(f-cap,0)+cap-f
   out[mask]=q+delta[:,None]*forward+(t*.30*weight)[:,None]*lateral
  return out
 p,n=softened_normals(p,n,transform)
 result=copy.deepcopy(primitive);result['attributes']['POSITION']=accessor(d,b,p);result['attributes']['NORMAL']=accessor(d,b,n)
 return result

def main():
 OUT.mkdir(parents=True,exist_ok=True)
 sd,sb,_=glb.load(SOURCE)
 source_primitives=copy.deepcopy(sd['meshes'][0]['primitives'])
 assert len(source_primitives)==21
 assert [sd['materials'][source_primitives[x]['material']]['name'] for x in [0,4,15,16,17,18]]==['Gloves','RidingBoots_03','KnightHMouth','KnightHead02','MedievalCharacter_Pilosity','Knight Eyes']
 # Restore authored Unity hair multiplier and keep explicit alpha contract.
 for m in sd['materials']:
  if m.get('name')=='MedievalCharacter_Pilosity':
   m['pbrMetallicRoughness']['baseColorFactor']=[.54245275]*3+[1]
   m['alphaMode']='MASK';m['alphaCutoff']=.029;m['doubleSided']=True
 ad=copy.deepcopy(sd);ab=bytearray(sb);ad.pop('animations',None);ad['meshes'][0]['primitives']=copy.deepcopy(source_primitives[:19]);ad['meshes'][0]['name']='Valwood_StoryHero_Armored'
 ad,ab=compact(ad,ab);armored=OUT/'armored';armored.mkdir(exist_ok=True);(armored/'body.glb').write_bytes(glb.encode(ad,ab));(armored/'appearance.cfg').write_text('VALWOOD_KNIGHT_APPEARANCE 1\nbody "body.glb"\nfirst_person 3 0 5 9\nend\n')
 geo=json.loads((INTAKE/'geometry.json').read_text());materials=json.loads((INTAKE/'materials/materials.json').read_text());assert geo['status']=='exported'
 d=copy.deepcopy(sd);b=bytearray(sb);d.pop('animations',None);pr=[]
 palette={d['nodes'][n]['name']:i for i,n in enumerate(d['skins'][0]['joints'])};report=[]
 for piece in geo['pieces'][:3]:
  name=piece['mesh_name'];p=np.array(piece['positions']).reshape(-1,3);n=np.array(piece['normals']).reshape(-1,3);uv=np.array(piece['uv']).reshape(-1,2);uv[:,1]=1-uv[:,1]
  x=np.array(piece['local_to_prefab_root']).reshape(4,4);x=np.diag([-1,1,1,1])@x
  p=(np.c_[p,np.ones(len(p))]@x.T)[:,:3];n=n@np.linalg.inv(x[:3,:3]);n/=np.linalg.norm(n,axis=1)[:,None]
  if 'Hose' in name:p,n=loosen_trousers(p,n,d,b)
  # This is a complete loose shirt and full trousers, not the clipped under-
  # armor pieces. Hidden-laces/ribbon authored morphs remove hanging ties.
  indices=np.array(piece['submeshes'][0]['triangles'],dtype=np.uint32).reshape(-1,3)[:,::-1]
  weights=np.array(piece['bone_weights']).reshape(-1,4);source_j=np.array(piece['bone_indices']).reshape(-1,4)
  lookup=np.array([palette[bone['name']] for bone in piece['bones']],dtype=np.uint16);j=lookup[source_j]
  assert np.max(np.abs(weights.sum(1)-1))<1e-5
  sourcepath=piece['material_paths'][0]
  mat=source_material(d,b,materials[sourcepath],{'KnightDoublet_LOD0':'Father_Plain_Work_Shirt','KnightHose_LOD0':'Father_Dark_Denim_Trousers','Knight Body_Hands_LOD0':'Father_Bare_Hands'}[name],dye=(.25,.29,.30) if 'Doublet'in name else ((.13,.18,.25) if 'Hose'in name else None))
  attrs={'POSITION':accessor(d,b,p),'NORMAL':accessor(d,b,n),'TEXCOORD_0':accessor(d,b,uv,'VEC2'),'JOINTS_0':accessor(d,b,j,'VEC4',5123),'WEIGHTS_0':accessor(d,b,weights,'VEC4')}
  groups=[indices]
  if 'Doublet'in name:
   arm_joints={i for bn,i in palette.items() if any(z in bn for z in ['upperarm','lowerarm','clavicle'])}
   armw=np.sum(weights*np.isin(j,list(arm_joints)),axis=1)
   sleeve=np.mean(armw[indices],axis=1)>.55
   groups=[indices[~sleeve],indices[sleeve]]
  for group in groups:
   assert len(group)>0
   pr.append({'attributes':attrs.copy(),'indices':accessor(d,b,group.reshape(-1,1),'SCALAR',5125),'material':mat,'mode':4})
  report.append({'source_mesh':name,'vertices':len(p),'triangles':len(indices),'primitive_count':len(groups),'source_weights_max_error':float(np.max(np.abs(weights.sum(1)-1))),'authored_morphs':piece['authored_blendshape_weights'],'source_bake_basis_error_m':piece['corrected_bake_baseline_max_error_metres'],'materials_source':sourcepath,'positions_min':p.min(0).tolist(),'positions_max':p.max(0).tolist()})
 # Low work-shoe silhouette and exact current face/hair/eyes identity retained.
 shoe=rounded_shoes(d,b,source_primitives[4]);shoe['material']=source_material(d,b,materials['Assets/_TheTalesFactory/Modular Knights/Textures/Clothes/RidingBoots/RidingBoots_03.mat'],'Father_Rounded_Rough_Work_Shoes',dye=(.10,.085,.075),minrough=.78)
 pr.append(shoe)
 pr += [copy.deepcopy(source_primitives[i]) for i in [15,16,17,18]]
 d['meshes'][0]['primitives']=pr;d['meshes'][0]['name']='Valwood_StoryHero_PlainFather'
 civilian=OUT/'civilian';civilian.mkdir(exist_ok=True)
 # Diagnostic preview retains identical animation samples solely for inspection.
 preview=copy.deepcopy(d);preview['animations']=copy.deepcopy(sd['animations']);pd,pb=compact(preview,bytearray(b));(OUT/'civilian-preview-animated.glb').write_bytes(glb.encode(pd,pb))
 d,b=compact(d,b);(civilian/'body.glb').write_bytes(glb.encode(d,b));(civilian/'appearance.cfg').write_text('VALWOOD_KNIGHT_APPEARANCE 1\nbody "body.glb"\nfirst_person 2 1 3\nend\n')
 result={'schema':'valwood.story-hero-owned-body.v1','source_runtime':str(SOURCE),'source_runtime_sha256':sha(SOURCE),'purchased_source_fbx_sha256':sha(ENGINE/'assets/purchased/modular_knights/source/Assets/_TheTalesFactory/Modular Knights/Models/ModularKnight_Complete.fbx'),'private_intake_fbx_sha256':sha(INTAKE/'intake/inspection-project/Assets/_TheTalesFactory/Modular Knights/Models/ModularKnight_Complete.fbx'),'geometry_export_sha256':sha(INTAKE/'geometry.json'),'selection_sha256':sha(INTAKE/'selection.json'),'input_source_untouched':True,'bind_palette':'exact source95jointpalette andinversebind matrices; allweightsmappedbynames','civilian_primitives':['shirt_torso','shirt_sleeves','trousers','bare_hands','low_shoes','mouth','head02','hair02','eyes'],'civilian_shape_adjustments':['Loose straight trouser legs below thigh, preserving restskin weights.','Rounded toe envelope removes 6cm of exaggerated point; unchanged heel/sole height and weighted foot joints.','Derived normal Jacobians match reshaped garment and shoe surfaces.'],'civilian_first_person':[1,3],'armored_first_person':[0,5,9],'civilian_garments':report,'civilian_body_sha256':sha(civilian/'body.glb'),'armored_body_sha256':sha(armored/'body.glb'),'hair_factor':[.54245275]*3+[1],'hair_alpha_cutoff':.029,'notes':['Same face and hair mesh as purchased current hero; complete purchased garments, not armor sections removed over missing body.','Plain charcoal shirt and dark blue cloth trousers are artist-authored dyes of owned cloth textures. Garments originate as medieval doublet/hose and are a compatible armorless fallback, not a new purchased modern character.','No runtime animations inside either body GLB. Existing weapon rig motion remains authoritative.','Source geometry UVs converted UnityVup to glTFVdown; Unity toglTFreflectX winding corrected.']}
 (OUT/'BUILD_REPORT.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps({k:v for k,v in result.items() if k.endswith('sha256') or k=='civilian_primitives'},indent=2))
if __name__=='__main__':main()
