#!/usr/bin/env python3
"""Read-only V3 serialized transforms, geometry support and collider audit."""
from pathlib import Path
import collections,hashlib,importlib.util,json,math,shlex
import numpy as np
N=Path(__file__).resolve().parents[1];E=N.parents[1];P=N/'world-project';O=N/'review/camp-composition';A=O/'before-v3'
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def mod(name,path):
 s=importlib.util.spec_from_file_location(name,path);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m
geo=mod('v3_read_geometry',N/'tools/review_forest_layout.py');stage=mod('v3_read_sampler',N/'tools/build_forest_layers.py');ground,helper=stage.sampler()
r=json.loads((O/'V3_PATCH.json').read_text());checks=[]
def check(name,ok,detail=None):checks.append(dict(name=name,passed=bool(ok),detail=detail))
check('world_studio_and_weather_unchanged',(P/'world.studio').read_bytes()==(A/'world.studio').read_bytes()and sha(P/'world.studio')==r['world_studio_unchanged_sha256'])
expected={f.name:f.read_text().splitlines()for f in(A/'scenery').glob('*.txt')};pairs=r['tree_replacements'];added=r['added_shore_details']
keys=set();expected_colliders=[]
for c in pairs:
 key=(c['file'],c['line']);check('unique_original_tree_'+str(key),key not in keys);keys.add(key);old=c['before_row'].split();new=c['after_row'].split();lines=expected[Path(c['file']).name]
 check('exact_original_tree_'+str(key),lines[c['line']-1]==c['before_row']and old[0]in['nm_city_region_oak','nm_city_region_beech']);lines[c['line']-1]=c['after_row']
 # Production parses U/V as double but stores yaw as float radians. Preserve
 # that exact effective orientation instead of overconstraining decimal text.
 check('tree_XZ_native_yaw_preserved_'+str(key),[float(x)for x in old[1:3]]==[float(x)for x in new[1:3]]and np.float32(float(old[3])*math.pi/180)==np.float32(float(new[3])*math.pi/180))
 check('tree_circle_not_expanded_AABB_'+str(key),all(float(x)==0 for x in new[8:10])and all(float(x)==1 for x in new[10:13])and new[0]in['pine','fir'])
expected['authored_instances.txt']+=['# Campsite look20261119 V3: low waterline detail and controlled mixed canopy.']+[d['row']for d in added]
allrows=[]
for name,lines in expected.items():
 actual=(P/'scenery'/name).read_text().splitlines();check('all_other_rows_exact_'+name,actual==lines)
 for i,s in enumerate(actual,1):
  q=s.split()
  if q and not q[0].startswith('#'):allrows.append((name,i,s))
counts=collections.Counter(s.split()[0]for _,_,s in allrows);check('native_source_record_budgets',len(allrows)<=16384 and max(counts.values())<=4096,dict(total=len(allrows),largest_alias=max(counts.values())))
check('exactly_twelve_mature_tree_replacements',len(keys)==12 and collections.Counter(c['new_alias']for c in pairs)=={'pine':6,'fir':6})
check('shore23_bounded_stones',sum(d['kind']=='shore_stone'for d in added)==23 and len(added)<=51)
for alias,meta in r['native_tree_alias_requirements'].items():
 for lod in ['near','mid','far']:
  path=P/'models'/(meta['stem']+'_'+lod+'.glb');check('existing_native_LOD_'+alias+'_'+lod,path.is_file())
source=(N/'source-v1/engine/world_scenery.cpp').read_text();check('production_circle_selection_exact_source','bool extended(const Placement &a){return a.pitch!=0||a.roll!=0||a.sx!=1||a.sy!=1||a.sz!=1;}'in source and 'circle_radius * a.scale'in source)
routes=json.loads((P/'opening/first-journey.routes.json').read_text())['paths'];segments=[]
for path in routes.values():
 pp=np.array(path)[:,[0,2]];segments.extend(zip(pp[:-1],pp[1:]))
traffic=np.array([[float(q[1]),float(q[3])]for s in(P/'opening/road-traffic.settings').read_text().splitlines()if(q:=s.split())and q[0]=='point']);segments.extend(zip(traffic[:-1],traffic[1:]));segments=np.array(segments)
g={a:geo.geometry(Path(m['path']))[0]for a,m in r['assets'].items()};records=[];max_support=0.;min_route=1e9
for item in pairs+added:
 line=item.get('after_row',item.get('row'));q,p=geo.decode(line);is_tree='after_row'in item;v=(g[q[0]]*p['axes']*p['scale'])@geo.rot(p['yaw'],p['pitch'],p['roll']).T;h=float(np.ptp(v[:,1]));bottom=v[v[:,1]<=v[:,1].min()+max(.02,h*.025)]
 if len(bottom)>64:bottom=bottom[np.linspace(0,len(bottom)-1,64,dtype=int)]
 hh=np.array([ground(p['x']+b[0],p['z']+b[2])for b in bottom]);support=float(np.quantile(hh-bottom[:,1],.35))-item['support']['embed_m'];err=abs(p['y']-support);max_support=max(max_support,err);rad=float(np.linalg.norm(v[:,[0,2]],axis=1).max());route=geo.dist(np.array([p['x'],p['z']]),segments)-rad;min_route=min(min_route,route)
 valid=len(line)<=320 and len(' '.join(q[:14]))<=256 and .15<=p['scale']<=(2 if is_tree else 4)and np.all((p['axes']>=.1)&(p['axes']<=8))and all(math.isfinite(float(x))for x in q[1:14])and len(q)in[14,17]and p['y']>.1
 if len(q)==17:valid &= q[14]=='wind'and all(0<=float(x)<=2 for x in q[15:])
 check('native_row_'+str(item['line']),valid)
 if is_tree:
  check('tree_height_root_route_'+str(item['line']),14-1e-5<=h<=22+1e-5 and route>=2 and item['camp_root_distance_m']>=18)
  radius=(.38 if q[0]=='pine'else .42)*p['scale'];c=item['collision'];check('production_collider_radius_height_'+str(item['line']),c['mode']==3 and c['circle']and abs(c['radius_m']-radius)<2e-8 and abs(c['ymin']-p['y'])<2e-5 and abs(c['ymax']-(p['y']+float(g[q[0]][:,1].max())*p['scale']))<2e-5)
  expected_colliders.append(dict(file=item['file'],line=item['line'],alias=q[0],circle=True,x=p['x'],z=p['z'],radius_m=radius,ymin=p['y'],ymax=p['y']+float(g[q[0]][:,1].max())*p['scale'],route_clearance_m=geo.dist(np.array([p['x'],p['z']]),segments)-radius))
 else:
  limit=.235 if item['kind']=='shore_stone'else .35 if item['kind']=='bank_grass'else .22
  check('shore_height_route_noncolliding_'+str(item['line']),h<=limit+1e-6 and route>=2 and item['collision_mode']==0 and item['creek_clearance_m']>=3.65)
 records.append(dict(alias=q[0],line=item['line'],tree=is_tree,height_m=h,serialized_support_error_m=err,full_mesh_route_clearance_m=route))
check('all_serialized_actual_mesh_support',max_support<2e-5,max_support);check('all_new_full_mesh_route_clearance',min_route>=2,min_route)
protected=['ground/showcase.height','city.scene','city.physics','opening/first-journey.routes.json','opening/road-traffic.settings','opening/arrival-camp.settings'];B=N.with_name('river-swim-2026-11-19')/'world-project'
for rel in protected:check('protected_file_'+rel,sha(P/rel)==sha(B/rel))
passed=all(c['passed']for c in checks);out=dict(schema='valwood.shore-canopy-v3-readonly-review.v1',passed=passed,project_or_runtime_modified=False,checks=checks,serialized_geometry_records=records,expected_production_tree_circles=expected_colliders,maximum_serialized_support_error_m=max_support,minimum_full_mesh_route_clearance_m=min_route,
 inputs_sha256={str(O/'V3_PATCH.json'):sha(O/'V3_PATCH.json'),str(P/'world.studio'):sha(P/'world.studio'),str(N/'tools/review_shore_canopy_v3.py'):sha(__file__)},limitations=['CPU reproduction of existing circle-branch collider math; independent native production load/sweeps and hardware visuals remain required.','Existing canopy may occlude replacement trees; static frustum inclusion is not pixel visibility.'])
(O/'V3_CPU_REVIEW.json').write_text(json.dumps(out,indent=2)+'\n');print(json.dumps(dict(passed=passed,failed=[c['name']for c in checks if not c['passed']],support_error=max_support,minimum_full_mesh_route_clearance_m=min_route,tree_circles=len(expected_colliders),details=len(added)),indent=2));raise SystemExit(0 if passed else 1)
