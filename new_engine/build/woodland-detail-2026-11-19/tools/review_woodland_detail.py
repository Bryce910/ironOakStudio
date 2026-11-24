#!/usr/bin/env python3
"""Read-only independent remeasurement of the private authored woodland pass."""
from pathlib import Path
import collections, hashlib, importlib.util, json, math, re, shlex
import numpy as np
N=Path(__file__).resolve().parents[1];P=N/'world-project';E=N.parents[1];B=N.with_name('campsite-look-2026-11-19')/'world-project';O=N/'review/woodland-detail'
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def load(n,p):
 s=importlib.util.spec_from_file_location(n,p);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m
r=json.loads((O/'PATCH.json').read_text());stage=load('wd_stage',N/'tools/build_forest_layers.py');geo=load('wd_geo',N/'tools/review_forest_layout.py');mesh=load('wd_mesh',E/'docs/seldonne-first-region/2026-11-07/region_mesh_grounding_v5.py');ground,helper=stage.sampler();checks={};records=[];assets={};errors=[]
checks['scene_hashes_match_manifest']=all(sha(P/rel)==h['after_sha256']and sha(B/rel)==h['before_sha256']for rel,h in r['changed_files'].items())
checks['baseline_all_prior_scenery_rows_preserved_except_seven_plants']=True
changes={(p['file'],p['line']):p for p in r['changed_plant_rows']}
for f in sorted((B/'scenery').glob('*.txt')):
 old=f.read_text().splitlines();new=(P/'scenery'/f.name).read_text().splitlines()
 for i,s in enumerate(old,1):
  expected=changes.get(('scenery/'+f.name,i),{}).get('after_row',s)
  if new[i-1]!=expected:checks['baseline_all_prior_scenery_rows_preserved_except_seven_plants']=False;errors.append([f.name,i,'unexpected prior row mutation'])
checks['only_18_paints_and_one_local_litter_layer_changed']=True
old=(B/'world.studio').read_text().splitlines();new=(P/'world.studio').read_text().splitlines();byline={p['line']:p for p in r['changed_studio_rows']}
for i,(a,b)in enumerate(zip(old,new),1):
 if a!=b:
  expected=byline.get(i,{})
  if expected.get('before')!=a or expected.get('after')!=b:checks['only_18_paints_and_one_local_litter_layer_changed']=False
checks['world_studio_row_count_unchanged']=len(old)==len(new)
checks['water_roads_height_ecology_actors_lights_weather_unchanged']=all(a==b for a,b in zip(old,new)if not a.startswith('surfaces ')) and all(a==b for a,b in zip(old,new)if a.startswith('surfaces ')and shlex.split(json.loads(a[9:]))[0]in ['ribbon','bind','image'])
checks['base_height_files_unchanged']=all((P/'ground'/name).exists()==(B/'ground'/name).exists() and (not(P/'ground'/name).exists()or sha(P/'ground'/name)==sha(B/'ground'/name))for name in ['showcase.height','countryside.height'])
checks['gameplay_routes_and_supports_unchanged']=all(sha(P/rel)==sha(B/rel)for rel in ['opening/first-journey.routes.json','opening/road-traffic.settings','city.physics','opening.config'])
segments=[]
for p in json.loads((P/'opening/first-journey.routes.json').read_text())['paths'].values():
 v=np.array(p)[:,[0,2]];segments.extend(zip(v[:-1],v[1:]))
v=np.array([[float(q[1]),float(q[3])]for s in(P/'opening/road-traffic.settings').read_text().splitlines()if(q:=s.split())and q[0]=='point']);segments.extend(zip(v[:-1],v[1:]));segments=np.array(segments)
swim=[];previous=None
for s in(N/'review/fixtures/story-swim-west-creek.qa.walk').read_text().splitlines():
 q=s.split()
 if q and q[0]=='path':previous=None
 elif len(q)==3:
  try:pt=np.array([float(q[0]),float(q[2])])
  except ValueError:continue
  if previous is not None:swim.append([previous,pt])
  previous=pt
swim=np.array(swim)
for item in r['placements']:
 s=(P/item['file']).read_text().splitlines()[item['line']-1];assert s==item['row'];q=s.split();p=stage.decode(q);a=q[0]
 if a not in assets:
  path=Path(r['assets'][a]['path']);assert sha(path)==r['assets'][a]['sha256'];assets[a]=geo.geometry(path)[0]
 g=assets[a];v=(g*np.array([p['sx'],p['sy'],p['sz']])*p['scale'])@mesh.rotation(p['yaw_deg'],p['pitch_deg'],p['roll_deg']).T;h=float(np.ptp(v[:,1]));bottom=v[v[:,1]<=v[:,1].min()+max(.02,h*.025)]
 if len(bottom)>64:bottom=bottom[np.linspace(0,len(bottom)-1,64,dtype=int)]
 candidates=np.array([ground(p['x']+b[0],p['z']+b[2])-b[1]for b in bottom]);expected=float(np.quantile(candidates,.35))-item['support']['embed_m'];gap=p['y']-candidates;pt=np.array([p['x'],p['z']]);rad=float(np.linalg.norm(v[:,[0,2]],axis=1).max());route=geo.dist(pt,segments)-rad;fixture=geo.dist(pt,swim)-rad
 record=dict(file=item['file'],line=item['line'],alias=a,kind=item['kind'],support_error_m=abs(expected-p['y']),minimum_basal_gap_m=float(gap.min()),maximum_basal_gap_m=float(gap.max()),actual_height_m=h,route_clearance_m=route,swim_fixture_clearance_m=fixture,collision=item['collision_mode'])
 assert record['support_error_m']<2e-5 and abs(h-item['actual_height_m'])<2e-5,(item['line'],'support or height')
 assert route>= (2.2 if item['collision_mode']else .75)-2e-5 and fixture>=(2.2 if item['collision_mode']else .6)-2e-5
 if item['kind']in ['bank_pebble','bank_roots']:assert h<=.23+2e-5 and item['collision_mode']==0
 if item['kind']=='horizon_tree':assert item['collision_mode']==3 and p['sx']==p['sy']==p['sz']==1 and p['pitch_deg']==p['roll_deg']==0 and 15<=h<=22
 records.append(record)
checks['all_added_mesh_support_recomputed']=len(records)==310
checks['all_new_solids_clear_routes_and_swim_fixture']=all(q['route_clearance_m']>=2.2-2e-5 and q['swim_fixture_clearance_m']>=2.2-2e-5 for q in records if q['collision'])
checks['low_noncolliding_debris_below_step_height']=all(q['actual_height_m']<=.23002 for q in records if q['kind']in ['bank_pebble','bank_roots'])
checks['new_tree_colliders_circular_uniform_unpitched']=all(p['pose']['sx']==p['pose']['sy']==p['pose']['sz']==1 and p['pose']['pitch_deg']==p['pose']['roll_deg']==0 for p in r['placements']if p['kind']=='horizon_tree')
checks['no_new_registered_aliases']=not r['registered_aliases_added']
checks['paint_budget_preserved']=sum(s.startswith('surfaces ')and shlex.split(json.loads(s[9:]))[0]=='paint'for s in new)==256
checks['native_record_grammar_and_budgets']=stage.validate_native_rows(r)['passed']
# Confirm meaningful actual geometry, not just counts, lies in both native views.
views=load('wd_views',N/'tools/run_forest_review.py').VIEWS;visible={}
for name in ['camp-eye','creek-surface']:
 c=views[name];eye=np.array([c[k]for k in ['x','y','z']]);f=np.array([math.sin(c['yaw'])*math.cos(c['pitch']),math.sin(c['pitch']),-math.cos(c['yaw'])*math.cos(c['pitch'])]);right=np.array([math.cos(c['yaw']),0,math.sin(c['yaw'])]);up=np.cross(right,f);cnt=collections.Counter();values=[]
 for p in r['placements']:
  pose=p['pose'];v=(assets[p['alias']]*np.array([pose['sx'],pose['sy'],pose['sz']])*pose['scale'])@mesh.rotation(pose['yaw_deg'],pose['pitch_deg'],pose['roll_deg']).T+np.array([pose['x'],pose['y'],pose['z']])-eye;depth=v@f;mask=(depth>.2)&(np.abs(v@right)<=depth*math.tan(c['fov']/2)*16/9)&(np.abs(v@up)<=depth*math.tan(c['fov']/2));fraction=float(mask.mean())
  if fraction>=.35:cnt[p['kind']]+=1
  values.append(dict(line=p['line'],kind=p['kind'],vertices_in_frustum_fraction=fraction))
 visible[name]=dict(at_least_35percent_vertices=dict(cnt),placements=values)
checks['horizon_trees_in_both_camera_frusta']=all(visible[name]['at_least_35percent_vertices'].get('horizon_tree',0)>=10 for name in visible)
checks['bank_boulders_in_creek_camera_frustum']=visible['creek-surface']['at_least_35percent_vertices'].get('bank_boulder',0)>=12
report=dict(schema='valwood.woodland-detail.cpu-review.v1',passed=all(checks.values()),checks=checks,errors=errors,records=records,frusta=visible,manifest_sha256=sha(O/'PATCH.json'),scene_inputs_sha256={str(P/rel):sha(P/rel)for rel in r['changed_files']},max_support_error_m=max(q['support_error_m']for q in records),added_lod0_triangles=r['added_lod0_triangles'],actual_terrain_sampler_sha256=sha(helper),authored_scene_modified=False,limitations=['No GPU or gameplay executed by this review.','Frustum inclusion cannot prove absence of occlusion or final image quality.','Production parser and transformed collider adapter are separately reviewed by the independent QA agent.'])
(O/'CPU_REVIEW.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({k:report[k]for k in ['passed','checks','max_support_error_m']},indent=2));print(json.dumps({k:v['at_least_35percent_vertices']for k,v in visible.items()},indent=2))
