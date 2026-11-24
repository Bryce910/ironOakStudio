#!/usr/bin/env python3
"""Stage exact existing HIGH geometry into MID for source-verified bridge cores.

No scene imports, source GLBs, shader, HIGH payload or purchased geometry is
changed. Source prefab LOD2 is a literal subset of LOD0, omitting these 20 solid
posts/rails rather than replacing them. Matching native HIGH triangles retain
all current UV, tangent, paint, GI and material data without a second bake.
"""
from pathlib import Path
import argparse,collections,copy,hashlib,json,math
from types import SimpleNamespace
import numpy as np
from scipy.spatial import cKDTree
from cook_nature_showcase import Source,DEFAULT_SOURCE,logical_units,ROOT
from bake_nature_material_fidelity import glb_document
from align_nature_showcase_lods import write_glb

DTYPES={5120:'i1',5121:'u1',5122:'<i2',5123:'<u2',5125:'<u4',5126:'<f4'}
SIZES={'SCALAR':1,'VEC2':2,'VEC3':3,'VEC4':4,'MAT4':16}
def sha(value):return hashlib.sha256(value).hexdigest()
def resolved(value):
 p=Path(value);return p if p.is_absolute()else ROOT.parent/p

def array(doc,binary,index):
 a=doc['accessors'][index];v=doc['bufferViews'][a['bufferView']]
 if a.get('sparse'):raise ValueError('Sparse accessor is unsupported')
 dt=np.dtype(DTYPES[a['componentType']]);n=SIZES[a['type']]
 return np.ndarray((a['count'],n),dt,binary,v.get('byteOffset',0)+a.get('byteOffset',0),strides=(v.get('byteStride',n*dt.itemsize),dt.itemsize))

def payload(doc,binary,image):
 im=doc['images'][image]
 if 'bufferView'not in im:raise ValueError('Only embedded source images are supported')
 v=doc['bufferViews'][im['bufferView']];start=v.get('byteOffset',0)
 return bytes(binary[start:start+v['byteLength']])

def selected_source(source):
 units,_=logical_units(source,SimpleNamespace(exclude_group=[],group=[]));chosen=[];triangles=[];ids=[]
 for unit in units:
  mask=0
  for p in unit['placements']:mask|=p['lod_mask']
  if mask!=1 or 'Prefab_Main_Bridge'not in unit['key'][1]or '/Core/'not in unit['key'][1]:continue
  for p in unit['placements']:
   if p['name']not in ('Buttress_level0_00_j_LOD0','stone_element_28c_wall_line_x3_battlements_LOD0'):raise ValueError('Unreviewed bridge component: '+p['name'])
   v,indices=source.mesh(p['mesh']);m=np.asarray(p['matrix']).reshape(4,4,order='F');points=np.asarray(v[:,:3]@m[:3,:3].T+m[:3,3],dtype=np.float32)
   identity=len(chosen);chosen.append({'name':p['name'],'hierarchy':p['hierarchy'],'matrix':p['matrix'],'source_mesh':p['mesh'],'triangles':sum(len(i)//3 for i in indices)})
   for index in indices:
    tri=points[index.reshape(-1,3)];triangles.append(tri);ids.extend([identity]*len(tri))
 if len(chosen)!=20:raise ValueError('Expected exactly 20 source bridge components')
 return chosen,np.concatenate(triangles),np.asarray(ids)

def canonical_material(doc,binary,index):
 m=copy.deepcopy(doc['materials'][index])
 # Texture indices and image/view indices may differ between independently
 # repacked tiers. Compare the exact source payload and sampler semantics.
 def visit(obj):
  if isinstance(obj,dict):
   for key,val in list(obj.items()):
    if key.endswith('Texture')and isinstance(val,dict)and 'index'in val:
     t=doc['textures'][val['index']];val['index']={'image':sha(payload(doc,binary,t['source'])),'sampler':doc.get('samplers',[{}])[t.get('sampler',0)]}
    else:visit(val)
  elif isinstance(obj,list):
   for val in obj:visit(val)
 visit(m);return m

def append_array(doc,binary,values,old_accessor,target):
 values=np.ascontiguousarray(values);offset=len(binary);data=values.tobytes();binary.extend(data);binary.extend(b'\0'*(-len(binary)%4))
 view=len(doc['bufferViews']);doc['bufferViews'].append({'buffer':0,'byteOffset':offset,'byteLength':len(data),'target':target})
 a=copy.deepcopy(old_accessor);a.update(bufferView=view,byteOffset=0,count=len(values));a.pop('min',None);a.pop('max',None)
 if a['type']=='VEC3':a.update(min=values.min(0).tolist(),max=values.max(0).tolist())
 index=len(doc['accessors']);doc['accessors'].append(a);return index

def image_footprint(doc,binary):
 encoded={};rgba={}
 from PIL import Image
 import io
 for i in range(len(doc.get('images',[]))):
  raw=payload(doc,binary,i);key=sha(raw)
  if key in encoded:continue
  encoded[key]=len(raw);im=Image.open(io.BytesIO(raw));w,h=im.size;size=0
  while True:
   size+=w*h*4
   if w==h==1:break
   w=max(1,w//2);h=max(1,h//2)
  rgba[key]=size
 return encoded,rgba

def stage(args):
 catalog=copy.deepcopy(json.loads(args.catalog.read_text()));source=Source(args.source);chosen,target,component=selected_source(source)
 centers=target.mean(1).astype(np.float64);tree=cKDTree(centers);target_lo=target.min((0,1));target_hi=target.max((0,1));matched=np.zeros(len(target),dtype=np.uint32)
 reports=[];high_hashes={};all_materials=set();before_images={};after_images={}
 args.output.mkdir(parents=True,exist_ok=True)
 for record in catalog:
  near_path,mid_path=[resolved(p)for p in record['files']];high_hashes[str(near_path)]=sha(near_path.read_bytes())
  if 'world_bounds_min'not in record:raise ValueError('Source bounds required')
  lo=np.asarray(record['world_bounds_min']);hi=np.asarray(record['world_bounds_max'])
  if np.any(hi<target_lo-.001)or np.any(lo>target_hi+.001):continue
  nd,nb=glb_document(near_path);md,mb=glb_document(mid_path);original_mid=copy.deepcopy(md);old_binary=bytes(mb);selected=[];offset=np.asarray(record['origin'],np.float32);total_triangles=0;total_vertices=0
  for mesh_index,mesh in enumerate(nd['meshes']):
   if mesh.get('extras',{}).get('valwood_invisible_source_datum'):continue
   for primitive_index,primitive in enumerate(mesh['primitives']):
    p=array(nd,nb,primitive['attributes']['POSITION']);ii=array(nd,nb,primitive['indices']).reshape(-1,3);positions=p[ii].astype(np.float64);positions[:,:,2]*=-1;positions+=offset
    distance,index=tree.query(positions.mean(1),k=1,workers=1);potential=np.flatnonzero(distance<.0005)
    if not len(potential):continue
    # Compare vertex sets independent of handedness/winding; source datum is
    # excluded and no nearest-surface approximation can add foreign triangles.
    a=positions[potential];b=target[index[potential]].astype(np.float64);delta=np.abs(a[:,:,None,:]-b[:,None,:,:]).max(3)
    exact=(delta.min(2).max(1)<.0005)&(delta.min(1).max(1)<.0005);take=potential[exact]
    if not len(take):continue
    target_ids=index[take]
    if len(np.unique(target_ids))!=len(target_ids)or np.any(matched[target_ids]):raise ValueError('Source triangle matched more than once')
    matched[target_ids]+=1
    used,remap=np.unique(ii[take].reshape(-1),return_inverse=True);selected.append((mesh_index,primitive_index,primitive,used,remap.reshape(-1,3)))
    total_triangles+=len(take);total_vertices+=len(used)
  if not selected:continue
  # Materials for the source bridge are shared between HIGH and MID. Require
  # exact semantic and image identity rather than silently importing duplicates.
  material_map={}
  for _,_,primitive,_,_ in selected:
   index=primitive['material'];want=canonical_material(nd,nb,index);matches=[i for i in range(len(md['materials']))if canonical_material(md,mb,i)==want]
   if not matches:raise ValueError('MID lacks the exact existing HIGH bridge material: '+nd['materials'][index].get('name',''))
   material_map[index]=matches[0];all_materials.add(want['name'])
  before_encoded,before_rgba=image_footprint(md,mb);before_images.update(before_rgba)
  baseline_accessor_storage=sha(old_binary)
  new_primitives=[]
  for mesh_index,primitive_index,primitive,used,indices in selected:
   new=copy.deepcopy(primitive);new['material']=material_map[primitive['material']]
   for name,accessor in primitive['attributes'].items():new['attributes'][name]=append_array(md,mb,array(nd,nb,accessor)[used],nd['accessors'][accessor],34962)
   old_index=nd['accessors'][primitive['indices']];new['indices']=append_array(md,mb,indices.astype(DTYPES[old_index['componentType']]).reshape(-1,1),old_index,34963)
   new_primitives.append(new)
  mesh=len(md['meshes']);md['meshes'].append({'name':'Source bridge opaque MID continuity','primitives':new_primitives,'extras':{'valwood_source_exact_near_subset':True,'source_renderer_count':20}})
  node=len(md['nodes']);md['nodes'].append({'name':'Source bridge opaque MID continuity','mesh':mesh});md['scenes'][md.get('scene',0)]['nodes'].append(node)
  file=args.output/'chunks'/mid_path.name;file.parent.mkdir(exist_ok=True);write_glb(file,md,mb)
  check,raw=glb_document(file)
  if bytes(raw[:len(old_binary)])!=old_binary:raise ValueError('Original MID accessor/image bytes changed')
  # Existing nodes, materials, images and primitive semantics must be exact.
  for key in('materials','images','textures','samplers'):
   if check.get(key)!=original_mid.get(key):raise ValueError('Original MID '+key+' changed')
  verts=sum(array(check,raw,p['attributes']['POSITION']).shape[0]for m in check['meshes']for p in m['primitives']);tris=sum(array(check,raw,p['indices']).size//3 for m in check['meshes']for p in m['primitives'])
  if file.stat().st_size>128*1024**2 or verts>1000000 or tris>1000000:raise ValueError('Candidate exceeds native file/geometry caps')
  _,after_rgba=image_footprint(check,raw);after_images.update(after_rgba)
  record['files'][1]=str(file.resolve());record['file_sha256']=[high_hashes[str(near_path)],sha(file.read_bytes())]
  record['triangles'][1]+=total_triangles;record['source_mid_continuity']={'components':'Exact omitted Main_Bridge/Core sourceLOD0 geometry','added_triangles':total_triangles,'added_vertices':total_vertices,'added_native_geometry_bytes':total_vertices*68+total_triangles*12,'all_high_bytes_unchanged':True,'original_mid_payload_prefix_unchanged':True,'added_images':0}
  reports.append({'name':record['name'],'near':str(near_path),'source_mid':str(mid_path),'candidate_mid':str(file.resolve()),'source_mid_sha256':sha(mid_path.read_bytes()),'candidate_mid_sha256':sha(file.read_bytes()),'source_mid_bytes':mid_path.stat().st_size,'candidate_mid_bytes':file.stat().st_size,'total_vertices':verts,'total_triangles':tris,'original_mid_binary_sha256':baseline_accessor_storage,**record['source_mid_continuity']})
  print('MID CONTINUITY',record['name'],total_triangles,'triangles',total_vertices,'vertices',round(file.stat().st_size/1024**2,3),'MiB',flush=True)
 if not np.all(matched==1):raise ValueError('Every selected source triangle must match once: '+str(collections.Counter(matched.tolist())))
 for r in catalog:
  if sha(resolved(r['files'][0]).read_bytes())!=high_hashes[str(resolved(r['files'][0]))]:raise ValueError('HIGH source changed while staging')
 if before_images!=after_images:raise ValueError('Candidate added or changed embedded image data')
 args.output.joinpath('catalog.json').write_text(json.dumps(catalog,indent=2)+'\n')
 report={'chunks':len(catalog),'source_catalog':str(args.catalog.resolve()),'source_catalog_sha256':sha(args.catalog.read_bytes()),'source':str(args.source.resolve()),'scope':'Only20 Main_Bridge/Core sourceLOD0-only components; sourceLOD2 contains no replacements','source_proof':str((ROOT/'docs/stronghold/2026-11-01/visual-finish/source-bridge-lod-proof.json').resolve()),'source_components':chosen,'matched_source_triangles':len(target),'all_selected_triangles_matched_exactly_once':True,'maximum_vertex_matching_error_m':.0005,'all_high_files_byte_identical':True,'all_original_mid_binary_bytes_unchanged':True,'added_images':0,'extra_texture_rgba_mip_budget_bytes':0,'added_geometry_bytes':sum(r['added_native_geometry_bytes']for r in reports),'materials':sorted(all_materials),'files':reports,'production_imports_untouched':True,'geometry_budget_unchanged':True,'max_candidate_mid_bytes':max(r['candidate_mid_bytes']for r in reports)}
 args.output.joinpath('report.json').write_text(json.dumps(report,indent=2)+'\n');args.output.joinpath('READY.json').write_text(json.dumps({'catalog':str((args.output/'catalog.json').resolve()),'catalog_sha256':sha((args.output/'catalog.json').read_bytes()),'added_geometry_bytes':report['added_geometry_bytes'],'extra_texture_bytes':0,'all_high_files_unchanged':True},indent=2)+'\n')
 print('READY',len(reports),'MID files',report['added_geometry_bytes'],'geometry bytes; zero extra images',flush=True)

def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--catalog',type=Path,default=ROOT/'assets/purchased/nature_castle/showcase-closeup-pillars-final/catalog.json');ap.add_argument('--source',type=Path,default=DEFAULT_SOURCE);ap.add_argument('--output',type=Path,default=ROOT/'assets/purchased/nature_castle/showcase-bridge-mid-continuity');stage(ap.parse_args())
if __name__=='__main__':main()
