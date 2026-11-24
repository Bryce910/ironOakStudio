#!/usr/bin/env python3
"""Read-only checks of private art patch, serialized native geometry and support."""
from pathlib import Path
import collections,ctypes,hashlib,importlib.util,json,math,re,shlex
import numpy as np
N=Path(__file__).resolve().parents[1];E=N.parents[1];P=N/'world-project';B=N.with_name('river-swim-2026-11-19')/'world-project';O=N/'review/camp-composition'
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def mod(name,path):
 s=importlib.util.spec_from_file_location(name,path);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m
gmod=mod('read_camp_mesh',N/'tools/review_forest_layout.py')
r=json.loads((O/'COMPOSITION_PATCH.json').read_text());checks=[]
def check(name,ok,detail=None):checks.append(dict(name=name,passed=bool(ok),detail=detail))
studio=(P/'world.studio').read_text();before=(O/'world.studio.before').read_text();expected=before
for c in r['ground_layer_changes']:
 check('unique_expected_ground_layer_before',expected.count(c['before'])==1)
 expected=expected.replace(c['before'],c['after'])
for c in r.get('regional_weather_changes',[]):
 check('unique_expected_weather_before',expected.count(c['before'])==1)
 expected=expected.replace(c['before'],c['after'])
 a=shlex.split(c['before']);b=shlex.split(c['after']);changed=[i for i,(x,y)in enumerate(zip(a,b))if x!=y]
 check('only_regional_cloud_fog_ranges',changed==[15,16,17,18]and b[15:19]==['.42','.60','.38','.54']and a[13:15]==b[13:15]==['.22','.38'])
expected+=''.join(a+'\n'for a in r['new_aliases'])
check('only_two_visual_aliases_local_ground_tint_regional_weather',studio==expected)
for c in r['ground_layer_changes']:
 a=shlex.split(json.loads(c['before'][9:]));b=shlex.split(json.loads(c['after'][9:]));check('ground_layer_change_only_RGB',a[:5]==b[:5]and a[8:]==b[8:]and a[1]=='Arrival detailed woodland litter')
aliases={q[1]:q for s in studio.splitlines()if(q:=shlex.split(s))and q[0]=='asset'}
for row in r['new_aliases']:
 q=shlex.split(row);lods,collision,vegetation=int(q[3]),int(q[4]),int(q[5]);distance,split=float(q[6]),float(q[7])
 check('native_asset_definition_'+q[1],1<=lods<=3 and collision==0 and 0<=vegetation<=2 and 30<=distance<=2000 and 10<=split<=500 and split+4<distance and not(vegetation==2 and lods<2) and not(vegetation!=2 and lods>2))
 for suffix in (['_near','_mid']if vegetation==2 and lods==2 else ['']):check('existing_lod_file_'+q[1]+suffix,(P/'models'/(q[2]+suffix+'.glb')).is_file())
source=(N/'source-v1/engine/world_scenery.cpp').read_text()
coll={m[0]:m[1]for m in re.findall(r'\{"([^"]+)",[^\n]*?Collision::(\w+)',source)}
for a,q in aliases.items():coll[a]='None'if q[4]=='0'else'AuthoredSolid'
removed=0;counts=collections.Counter();allrows=[]
for f in sorted((B/'scenery').glob('*.txt')):
 old=f.read_text().splitlines();new=(P/'scenery'/f.name).read_text().splitlines();rr=r['removed_rows'][f.name];indices={p['line']for p in rr}
 check('valid_exact_removed_'+f.name,len(indices)==len(rr)and all(old[p['line']-1]==p['row']and coll.get(p['alias'])=='None'for p in rr))
 expected=[s for i,s in enumerate(old,1)if i not in indices]
 if f.name=='authored_instances.txt':expected+=['# Campsite look20261119: broad rooted clusters replace tiny isolated pin scatter.']+[p['row']for p in r['placements']]
 check('exact_scene_delta_'+f.name,new==expected)
 # Stronger than tree checks: every original row whose alias has collision is preserved exactly and in order.
 solids=lambda lines:[s for s in lines if len(q:=s.split())>=6 and not q[0].startswith('#') and coll.get(q[0])not in ['None',None]]
 check('all_original_solid_rows_preserved_'+f.name,solids(old)==solids(new))
 removed+=len(rr)
 for i,s in enumerate(new,1):
  q=s.split()
  if not q or q[0].startswith('#'):continue
  counts[q[0]]+=1;allrows.append((f.name,i,q,s))
check('merged_native_record_limits',len(allrows)<=16384 and max(counts.values())<=4096,dict(total=len(allrows),max_alias=max(counts.values())))
check('removed_count',removed==r['removed_instances'])
helper=E/'build/camp-road-polish-2026-11-19/review/route-budget/sample-height.so'
for name in ['world_surface_authoring.cpp','world_surface_field.cpp']:
 check('sampler_source_'+name,(E/'build/camp-road-polish-2026-11-19/source-v1/engine'/name).read_bytes()==(N/'source-v1/engine'/name).read_bytes())
lib=ctypes.CDLL(str(helper));lib.load.argtypes=[ctypes.c_char_p,ctypes.c_char_p];lib.load.restype=ctypes.c_int;lib.sample_many.argtypes=[ctypes.c_uint,ctypes.POINTER(ctypes.c_double),ctypes.POINTER(ctypes.c_float),ctypes.c_int];lib.sample_many.restype=ctypes.c_int
assert lib.load(str(P/'ground/showcase.height').encode(),str(P/'world.studio').encode())==1
def ground(points):
 pp=np.ascontiguousarray(points,dtype=np.float64).reshape(-1,2);v=np.empty(len(pp),dtype=np.float32);assert lib.sample_many(len(pp),pp.ctypes.data_as(ctypes.POINTER(ctypes.c_double)),v.ctypes.data_as(ctypes.POINTER(ctypes.c_float)),1)==1;return v.astype(float)
geometry={};assets=[]
for a,meta in r['assets'].items():
 path=Path(meta['path']);g,tri,norm=gmod.geometry(path);geometry[a]=g
 check('model_hash_and_triangle_count_'+a,sha(path)==meta['sha256']and tri==meta['triangles'])
 assets.append(dict(alias=a,actual_triangles=tri,source_sha256=sha(path)))
routes=json.loads((P/'opening/first-journey.routes.json').read_text())['paths'];segments=[]
for path in routes.values():
 pp=np.array(path)[:,[0,2]];segments.extend(zip(pp[:-1],pp[1:]))
traffic=np.array([[float(q[1]),float(q[3])]for s in(P/'opening/road-traffic.settings').read_text().splitlines()if(q:=s.split())and q[0]=='point']);segments.extend(zip(traffic[:-1],traffic[1:]));segments=np.array(segments)
records=[];max_support=0;min_route=1e9;bad=[];basal_gap=[1e9,-1e9]
for item in r['placements']:
 q=item['row'].split();vals=np.array([float(t)for t in q[1:14]]);x=(vals[0]-.5)*100800.;z=(vals[1]-.5)*100800.*2818/5009;y=vals[12];yaw,scale,pitch,roll=vals[2],vals[3],vals[7],vals[8];axes=vals[9:12]
 valid=coll.get(q[0])=='None'and np.isfinite(vals).all()and len(item['row'])<=320 and len(' '.join(q[:14]))<=256 and 0<=vals[0]<=1 and 0<=vals[1]<=1 and .15<=scale<=4 and np.all((axes>=.1)&(axes<=8))and np.max(np.abs([yaw,pitch,roll]))<=360 and np.all(vals[4:7]==0)and .1<y<=12000
 if len(q)>14:valid &= len(q)==17 and q[14]=='wind'and all(0<=float(t)<=2 for t in q[15:])
 valid &= (P/item['file']).read_text().splitlines()[item['line']-1]==item['row']
 if not valid:bad.append(item['line'])
 g=geometry[q[0]];v=(g*axes*scale)@gmod.rot(yaw,pitch,roll).T;h=np.ptp(v[:,1]);bottom=v[v[:,1]<=v[:,1].min()+max(.02,h*.025)]
 if len(bottom)>64:bottom=bottom[np.linspace(0,len(bottom)-1,64,dtype=int)]
 hh=ground(bottom[:,[0,2]]+[x,z]);support=float(np.quantile(hh-bottom[:,1],.35))-item['embed_m'];err=abs(y-support);max_support=max(max_support,err);gaps=y+bottom[:,1]-hh;basal_gap=[min(basal_gap[0],gaps.min()),max(basal_gap[1],gaps.max())]
 radius=float(np.linalg.norm(v[:,[0,2]],axis=1).max());route=gmod.dist(np.array([x,z]),segments)-radius;min_route=min(min_route,route)
 records.append(dict(line=item['line'],alias=q[0],kind=item['kind'],support_error_m=err,route_clearance_m=route,transformed_height_m=float(h),minimum_basal_clearance_m=float(gaps.min()),maximum_basal_clearance_m=float(gaps.max())))
check('all_added_native_rows_known_finite_exact_noncolliding',not bad,bad)
check('all_1205plus_mesh_support_recomputed',max_support<2e-5,max_support)
check('complete_mesh_route_clearance_recomputed',min_route>=1.-2e-5,min_route)
for rel in ['ground/showcase.height','city.scene','city.physics','opening/first-journey.routes.json','opening/road-traffic.settings','opening/arrival-camp.settings']:
 check('protected_file_'+rel,sha(P/rel)==sha(B/rel))
for key in ['ribbon','water','terrain']:
 records_of=lambda txt:[s for s in txt.splitlines()if (s.startswith(key+' ')or s.startswith('surfaces ')and json.loads(s[9:]).startswith(key+' '))]
 check('protected_studio_'+key,records_of(studio)==records_of(before))
passed=all(c['passed']for c in checks)
out=dict(schema='valwood.campsite-composition-readonly-review.v1',passed=passed,production_or_project_modified=False,checks=checks,
 maximum_serialized_mesh_support_error_m=max_support,minimum_actual_mesh_route_clearance_m=min_route,minimum_basal_clearance_m=float(basal_gap[0]),maximum_basal_clearance_m=float(basal_gap[1]),
 added_rows_checked=len(records),removed_rows_checked=removed,merged_rows=len(allrows),assets=assets,geometry_records=records,
 input_sha256={str(N/'tools/compose_camp_floor.py'):sha(N/'tools/compose_camp_floor.py'),str(P/'world.studio'):sha(P/'world.studio'),str(O/'COMPOSITION_PATCH.json'):sha(O/'COMPOSITION_PATCH.json')},
 limitations=['Read-only CPU transform/support and protected-row checks. Native captures and performance review are required separately.','Irregular low roots intentionally embed; all basal vertices are not forced coplanar.'])
(O/'CPU_REVIEW.json').write_text(json.dumps(out,indent=2)+'\n')
print(json.dumps(dict(passed=passed,failed=[c['name']for c in checks if not c['passed']],added=len(records),removed=removed,merged=len(allrows),support_error=max_support,route_clearance=min_route,basal_clearance=basal_gap),indent=2))
raise SystemExit(0 if passed else 1)
