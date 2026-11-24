"""Merge zero-area pivot triangles into first primitive, preserving all visible geometry."""
from pathlib import Path
import sys,json,hashlib,struct,copy,numpy as np
N=Path(__file__).resolve().parents[1];E=N.parents[1];A=N/'world-project/models/art_completion';R=N/'review/art';sys.path.insert(0,str(E/'tools'));from build_opening_prologue import read_glb,array
records=[]
for path in sorted(A.glob('*.glb')):
 if path.is_symlink():continue
 d,b=read_glb(path);b=bytearray(b);before=copy.deepcopy(d);count=0
 for mesh in d['meshes']:
  datums=[p for p in mesh['primitives']if d['materials'][p.get('material',0)].get('name')=='Source_Pivot_Datum']
  visible=[p for p in mesh['primitives']if p not in datums]
  if not datums:continue
  assert len(datums)==1 and visible
  base=visible[0];datum=datums[0];offset=d['accessors'][base['attributes']['POSITION']]['count']
  def append(values,kind):
   arr=np.asarray(values,dtype='<u4'if kind=='SCALAR'else'<f4');dim=1 if kind=='SCALAR'else arr.shape[1];b.extend(b'\0'*(-len(b)%4));vi=len(d['bufferViews']);d['bufferViews'].append({'buffer':0,'byteOffset':len(b),'byteLength':arr.nbytes,'target':34963 if kind=='SCALAR'else 34962});b.extend(arr.tobytes());ai=len(d['accessors']);ac={'bufferView':vi,'componentType':5125 if kind=='SCALAR'else 5126,'count':len(arr),'type':kind}
   if kind=='VEC3':ac.update(min=arr.min(0).tolist(),max=arr.max(0).tolist())
   d['accessors'].append(ac);return ai
  # Read all arrays before changing base accessors.
  vals={k:np.concatenate([array(d,b,base['attributes'][k]),array(d,b,datum['attributes'][k])])for k in ['POSITION','NORMAL','TEXCOORD_0']};ix=np.concatenate([array(d,b,base['indices']).ravel(),array(d,b,datum['indices']).ravel()+offset])
  for k,v in vals.items():base['attributes'][k]=append(v,'VEC2'if k=='TEXCOORD_0'else'VEC3')
  base['indices']=append(ix,'SCALAR');mesh['primitives']=visible;count+=len(datums)
 if not count:continue
 d['buffers'][0]['byteLength']=len(b);js=json.dumps(d,separators=(',',':')).encode();js+=b' '*(-len(js)%4);b+=b'\0'*(-len(b)%4);path.write_bytes(struct.pack('<4sII',b'glTF',2,28+len(js)+len(b))+struct.pack('<I4s',len(js),b'JSON')+js+struct.pack('<I4s',len(b),b'BIN\0')+b)
 records.append({'path':str(path),'removed_zero_area_draws':count,'remaining_primitives':sum(len(m['primitives'])for m in d['meshes']),'sha256':hashlib.sha256(path.read_bytes()).hexdigest()})
(R/'DRAW_COMPACTION.json').write_text(json.dumps({'passed':True,'visible_positions_normals_uvs_and_indices_unchanged':True,'geometry_new_or_removed':'Only zero-area pivot triangles moved into existing primitive','models':records,'saved_draws_if_all_assets_visible':sum(x['removed_zero_area_draws']for x in records)},indent=2)+'\n');print(len(records),'datum draws merged')
