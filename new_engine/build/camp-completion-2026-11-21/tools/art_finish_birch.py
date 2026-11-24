"""Prepare exact bark-only collision, far cards and staged Birch integration."""
from pathlib import Path
import sys,json,struct,copy,hashlib,math,importlib.util,collections
import numpy as np
N=Path(__file__).resolve().parents[1];E=N.parents[1];P=N/'world-project';R=N/'review/art/birch';A=P/'models/art_birch';sys.path.insert(0,str(E/'tools'));from build_opening_prologue import Glb,read_glb,array
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
def fnv(data):
 h=14695981039346656037
 for b in data:h=((h^b)*1099511628211)&((1<<64)-1)
 return h
def crossings(v,ix,y):
 out=[]
 for tri in ix:
  q=v[tri]
  for a,b in zip(q,np.roll(q,-1,axis=0)):
   if (a[1]<=y<b[1])or(b[1]<=y<a[1]):out.append(a+(b-a)*(y-a[1])/(b[1]-a[1]))
 assert len(out)>=6;out=np.array(out);return {'height':y,'center_xz':out[:,[0,2]].mean(0).tolist(),'maximum_pivot_radius':float(np.linalg.norm(out[:,[0,2]],axis=1).max()),'minimum_xz':out[:,[0,2]].min(0).tolist(),'maximum_xz':out[:,[0,2]].max(0).tolist()}
records=[];aliaspatch=[];catalogrows=[]
for rec in json.loads((R/'COOK.json').read_text()):
 name=rec['name'];near=A/(name+'_near.glb');d,b=read_glb(near);assert d['nodes']==[{'mesh':0,'name':rec['source_name']}];prims=d['meshes'][0]['primitives'];bark=[p for p in prims if 'white bark'in d['materials'][p['material']]['name']];assert len(bark)==1;pr=bark[0];v=array(d,b,pr['attributes']['POSITION']).astype(float);ix=array(d,b,pr['indices']).reshape(-1,3).astype(int);soil=rec['soil_plane_glb_y'];cap=soil+6.;vv=[];tt=[];lookup={};orig=[]
 for source_id,ids in enumerate(ix):
  poly=list(v[ids]);out=[]
  for a,z in zip(poly,[poly[-1]]+poly[:-1]):
   ai=a[1]<=cap;zi=z[1]<=cap
   if ai!=zi:out.append(z+(a-z)*((cap-z[1])/(a[1]-z[1])))
   if ai:out.append(a)
  for k in range(1,len(out)-1):
   q=np.asarray([out[0],out[k],out[k+1]],'<f4')
   if np.linalg.norm(np.cross(q[1].astype(float)-q[0],q[2].astype(float)-q[0]))<1e-8:continue
   row=[]
   for val in q:
    key=val.tobytes()
    if key not in lookup:lookup[key]=len(vv);vv.append(val)
    row.append(lookup[key])
   tt.append(row);orig.append(source_id)
 vv=np.asarray(vv,'<f4');tt=np.asarray(tt,'<u4');assert len(tt)>20
 raw=b'VWCOL1\0\0'+struct.pack('<II',len(vv),len(tt))+vv.tobytes()+tt.tobytes();raw+=struct.pack('<Q',fnv(raw));vwc=A/(name+'_trunk.vwc');vwc.write_bytes(raw)
 maxerr=0.
 for tri,source_id in zip(vv[tt].astype(float),orig):
  q=v[ix[source_id]];normal=np.cross(q[1]-q[0],q[2]-q[0]);normal/=np.linalg.norm(normal);maxerr=max(maxerr,float(np.abs((tri-q[0])@normal).max()))
 assert maxerr<2e-6
 sections=[crossings(v,ix,soil+y)for y in [0,.25,1.,2.]]
 # Far geometry obeys the native camera-facing X/Y-quad convention.
 g=Glb();mi=g.material('Birch actual source diffuse silhouette',[1,1,1],1,base=R/(name+'_card.png'));g.doc['materials'][mi].update(alphaMode='MASK',alphaCutoff=.42,doubleSided=True);height=rec['ortho_height'];width=rec['ortho_width'];top=rec['authored_full_height_m'];center=(rec['minimum'][2]+rec['maximum'][2])/2;cx=(rec['minimum'][0]+rec['maximum'][0])/2;vb=.5+center/height;vt=.5-(top-center)/height;g.mesh([[-width/2+cx,0,0],[width/2+cx,0,0],[width/2+cx,top,0],[-width/2+cx,top,0]],[[0,0,1]]*4,[[0,vb],[1,vb],[1,vt],[0,vt]],[0,1,2,0,2,3],mi);far=A/(name+'_far.glb');far.write_bytes(g.finish())
 alias='nm_camp_'+name;stem='art_birch/'+name;aliaspatch.append({'before':None,'after':f'asset {alias} {stem} 3 0 2 850 70 1'});modelpath=f'models/{stem}_near.glb';catalogrows +=[f'mesh "{alias}" "{modelpath}" "models/art_birch/{name}_trunk.vwc"',f'model "{alias}" {near.stat().st_size} {fnv(near.read_bytes()):016x}']
 record=rec|{'alias':alias,'stem':stem,'source_archive':'/home/computer/Downloads/village_3d_model(1).zip','source_fbx_sha256':sha(R/'source'/(rec['source_name']+'.fbx')),'soil_sections':sections,'collision':{'path':str(vwc.relative_to(P)),'vertices':len(vv),'triangles':len(tt),'maximum_source_facet_error_m':maxerr,'clipped_at_model_y':cap,'leaves_never_in_collision':True,'full_canopy_fallback_disallowed':True},'models':{lod:{'path':str(path.relative_to(P)),'sha256':sha(path)}for lod,path in [('near',near),('mid',A/(name+'_mid.glb')),('far',far)]}}
 records.append(record)
(R/'BIRCH_MODELS.json').write_text(json.dumps({'passed':True,'records':records,'distinct_new_named_tree_types':1,'biome':'Temperate birch; two authentic source variants, not two botanical species','native_materials':'Opaque white bark + MASK branch cards, dielectric roughness0.9, matching filtered normals/alpha; shared 1K longest-edge maps','no_scene_or_studio_changes':True},indent=2)+'\n')
(R.parent/'STUDIO_BIRCH_PATCH.json').write_text(json.dumps(aliaspatch,indent=2)+'\n');(R.parent/'COLLISION_BIRCH_PATCH.json').write_text(json.dumps({'append_only_catalog_rows':catalogrows,'required_before_placement':True,'native_alias_collision':0,'reason':'Exact bark-only static meshes supply movement/camera collision. The leaf canopy is never a collider.','models_and_vwc_installed':True},indent=2)+'\n');print(json.dumps({'passed':True,'models':len(records),'collision_triangles':[r['collision']['triangles']for r in records]}))
