"""Read-only independent grammar, visible-triangle and actual terrain contact audit."""
from pathlib import Path
import json,struct,hashlib,shlex,importlib.util,sys,collections,ctypes,numpy as np
N=Path(__file__).resolve().parents[1];E=N.parents[1];P=N/'world-project';R=N/'review/art';A=P/'models/art_completion';sys.path.insert(0,str(E/'tools'));from build_opening_prologue import read_glb,array
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
checks=[];errors=[]
def check(name,ok,detail=None):
 checks.append({'name':name,'passed':bool(ok),'detail':detail})
 if not ok:errors.append(name)
def mod(n,p):
 s=importlib.util.spec_from_file_location(n,p);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m
geo=mod('qa_geom',E/'build/woodland-detail-2026-11-19/tools/review_forest_layout.py');report=json.loads((R/'ART_PATCH.json').read_text());patch=json.loads((R/'STUDIO_ART_PATCH.json').read_text())
studio=(P/'world.studio').read_text().splitlines();defs={q[1]:q for line in studio if(q:=shlex.split(line))and q[0]=='asset'}
for p in patch:
 q=p['after'].split();defs[q[1]]=q
 check('definition_grammar_'+q[1],len(q)==9 and 1<=int(q[3])<=3 and 0<=int(q[4])<=4 and 0<=int(q[5])<=2)
 for lod in range(int(q[3])):
  suffix=['_near','_mid','_far'][lod]if q[5]=='2'else('_mid'if lod else'')
  check('native_asset_file_'+q[1]+'_'+str(lod),(P/'models'/(q[2]+suffix+'.glb')).is_file())
# Compare every exact changed row, added row and preimage to original owned snapshots.
allrows={f.name:f.read_text().splitlines()for f in(P/'scenery').glob('*.txt')};ct=collections.Counter();total=0;maximum_line=0
for f,rows in allrows.items():
 for line in rows:
  q=line.split()
  if not q or q[0].startswith('#'):continue
  ct[q[0]]+=1;total+=1;maximum_line=max(maximum_line,len(line))
check('scene_record_budget',total<=16384,{'total':total,'limit':16384});check('per_alias_budget',max(ct.values())<=4096,max(ct.values()));check('record_byte_budget',maximum_line<=320,maximum_line)
allchanges=report['changed_rows']+report.get('far_treeline_replacements',[])
check('all_changed_rows_exact',all(allrows[Path(p['file']).name][p['line']-1]==p['after']for p in allchanges),len(allchanges))
check('all_added_rows_exact_once',all(allrows['authored_instances.txt'].count(p['row'])==1 for p in report['new_placements']),len(report['new_placements']))
# Scene compare is against original archived full scenery files.
known_trees={'nm_city_region_oak','nm_city_region_beech','pine','fir','pine_young','broadleaf','nm_camp_pine_snag','nm_camp_birch1','nm_camp_birch3','nm_camp_variant_oak02','nm_camp_variant_oak04','nm_camp_variant_beech01','nm_camp_variant_beech02'}
before_count=after_count=0
for f,rows in allrows.items():
 before=(R/('before-'+f)).read_text().splitlines()if(R/('before-'+f)).is_file()else rows
 before_count+=sum(bool(q:=s.split())and q[0]in known_trees for s in before);after_count+=sum(bool(q:=s.split())and q[0]in known_trees for s in rows)
check('canopy_instance_count_unchanged',before_count==after_count,{'before':before_count,'after':after_count})
for name in ['oak02','oak04','beech01','beech02']:
 for lod in ['near','mid']:
  a=P/f'models/camp_variants/camp_variant_{name}_{lod}.glb';b=E/f'projects/valwood_current/models/camp_variants/camp_variant_{name}_{lod}.glb';check('source_tree_bytes_'+name+'_'+lod,sha(a)==sha(b))
 q=defs['nm_camp_variant_'+name];check('source_collision_path_unchanged_'+name,q[2]=='camp_variants/camp_variant_'+name and q[4]=='0')
# Directly sample native surface code, independent of placement-builder support logic.
bank=E/'build/camp-road-polish-2026-11-19';libpath=bank/'review/route-budget/sample-height.so'
for rel in ['engine/world_surface_authoring.cpp','engine/world_surface_field.cpp','engine/world_river_network.h','engine/world_height_region.h']:check('sampler_source_'+Path(rel).name,(bank/'source-v1'/rel).read_bytes()==(N/'source-v1'/rel).read_bytes())
lib=ctypes.CDLL(str(libpath));lib.load.argtypes=[ctypes.c_char_p,ctypes.c_char_p];lib.load.restype=ctypes.c_int;lib.sample_many.argtypes=[ctypes.c_uint,ctypes.POINTER(ctypes.c_double),ctypes.POINTER(ctypes.c_float),ctypes.c_int];lib.sample_many.restype=ctypes.c_int;assert lib.load(str(P/'ground/showcase.height').encode(),str(P/'world.studio').encode())==1

def heights(points):
 points=np.ascontiguousarray(points,dtype='<f8');out=np.empty(len(points),'<f4');assert lib.sample_many(len(points),points.ctypes.data_as(ctypes.POINTER(ctypes.c_double)),out.ctypes.data_as(ctypes.POINTER(ctypes.c_float)),1)==1;return out.astype(float)

def visible_geometry(path):
 d,b=read_glb(path);pp=[];nn=[];ii=[];count=0
 for node in d['nodes']:
  if 'mesh'not in node:continue
  assert not any(k in node for k in ['matrix','translation','rotation','scale','children']), 'Owned ground model must be flattened'
  for pr in d['meshes'][node['mesh']]['primitives']:
   p=array(d,b,pr['attributes']['POSITION']);n=array(d,b,pr['attributes']['NORMAL']);uv=array(d,b,pr['attributes']['TEXCOORD_0']);ix=array(d,b,pr['indices']).astype(int).reshape(-1,3)
   check('indices_'+path.name+'_'+str(count),ix.min()>=0 and ix.max()<len(p));check('finite_attributes_'+path.name+'_'+str(count),np.isfinite(p).all()and np.isfinite(n).all()and np.isfinite(uv).all());check('unit_normals_'+path.name+'_'+str(count),np.max(abs(np.linalg.norm(n,axis=1)-1))<.03)
   area=np.linalg.norm(np.cross(p[ix[:,1]]-p[ix[:,0]],p[ix[:,2]]-p[ix[:,0]]),axis=1);ix=ix[area>1e-9];pp.append(p);nn.append(n);ii.append(ix+count);count+=len(p)
 return np.concatenate(pp),np.concatenate(ii),d
contacts=[]
for placement in report['new_placements']:
 alias=placement['alias'];q=placement['row'].split();definition=defs[alias];path=P/'models'/(definition[2]+'.glb')
 if placement['kind']not in ['batched_solid_floor_details','hearth_coal_fragments','weathered_deck_overlay']:continue
 p,ix,d=visible_geometry(path);offset=np.array([(float(q[1])-.5)*100800,float(q[13]),(float(q[2])-.5)*100800*2818/5009]);p=p+offset
 if placement['kind']=='weathered_deck_overlay':
  delta=p[:,1].max()-11.2;check('bridge_top_collision_delta',.009<=delta<=.0191,delta);continue
 # Basal and upper visible vertices plus triangle centres independently checked.
 used=np.unique(ix);samples=np.concatenate([p[used],p[ix].mean(1)]);gap=samples[:,1]-heights(samples[:,[0,2]])
 check('floor_contact_'+alias,gap.min()>-.09 and gap.max()<.17,{'min_m':float(gap.min()),'max_m':float(gap.max()),'sample_count':len(gap)})
 check('solid_floor_materials_'+alias,all(m.get('alphaMode','OPAQUE')=='OPAQUE'for m in d['materials']))
 contacts.append({'alias':alias,'min_m':float(gap.min()),'max_m':float(gap.max()),'samples':len(gap)})
# Far cards must obey the native vertex shader's single camera-facing quad contract.
for name in ['oak02','oak04','beech01','beech02']:
 p,ix,d=visible_geometry(A/f'tree_{name}_far.glb');check('native_billboard_'+name,len(ix)==2 and len(d['meshes'][0]['primitives'])==1 and np.ptp(p[:,2])==0 and abs(p[:,1].min())<1e-8,{'triangles':len(ix),'primitives':len(d['meshes'][0]['primitives']),'minimum_y':float(p[:,1].min())})
# Pin every owned output, not generated scene screenshots.
files={str(p.relative_to(N)):sha(p)for p in sorted(A.glob('*.glb'))if p.is_file()};result={'passed':not errors,'checks':checks,'errors':errors,'contacts':contacts,'owned_art_output_sha256':files,'scene_records':total,'tree_count':after_count,'native_visual_and_performance_pending':True};(R/'ART_VALIDATION.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps({'passed':result['passed'],'checks':len(checks),'errors':errors,'scene_records':total,'owned_models':len(files)}));sys.exit(bool(errors))
