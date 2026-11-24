#!/usr/bin/env python3
"""Independent, read-only CPU audit of final forest rows/visible geometry/support.
Does not call authoring/staging code and never writes project or LIVE data.
"""
from pathlib import Path
import collections,ctypes,hashlib,json,math,shlex,struct,sys
import numpy as np
N=Path(__file__).resolve().parents[1];P=N/'world-project';B=N.with_name('apartment-camera-2026-11-19');OUT=N/'review/forest-layout';E=N.parents[1]
W,D=100800.,100800.*2818/5009
TREES={'nm_city_region_oak','nm_city_region_beech','pine','fir','broadleaf','pine_young'}
NATURAL={'opening_verge_grass_long','opening_verge_grass_broad','bep_nature_clover','bep_nature_fern','bep_nature_bush_a','bep_nature_bush_b','nm_city_region_oak','nm_city_region_beech','rock0'}
BUILTINS={'fern0':('woodland_fern_0.glb',0),'broadleaf':('cinematic_broadleaf_near.glb',3),'pine_young':('cinematic_pine_young_near.glb',3),'deadwood':('cinematic_deadwood.glb',1),'rock0':('cinematic_rock_0.glb',1),'rock1':('cinematic_rock_1.glb',1)}
def sha(p):
 with Path(p).open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def rot(y,p,r):
 y,p,r=np.radians([y,p,r]);cy,sy=math.cos(y),math.sin(y);cp,sp=math.cos(p),math.sin(p);cr,sr=math.cos(r),math.sin(r)
 return np.array([[cy,0,sy],[0,1,0],[-sy,0,cy]])@np.array([[1,0,0],[0,cp,-sp],[0,sp,cp]])@np.array([[cr,-sr,0],[sr,cr,0],[0,0,1]])
def geometry(path):
 raw=path.read_bytes();magic,ver,length=struct.unpack_from('<III',raw);assert magic==0x46546c67 and ver==2 and length==len(raw)
 sz,typ=struct.unpack_from('<II',raw,12);assert typ==0x4e4f534a;j=json.loads(raw[20:20+sz]);bs,bt=struct.unpack_from('<II',raw,20+sz);assert bt==0x004e4942;binary=memoryview(raw)[28+sz:28+sz+bs]
 visible=[];allv=[];tri=0
 def local(n):
  if 'matrix'in n:return np.array(n['matrix']).reshape(4,4,order='F')
  x,y,z,w=n.get('rotation',[0,0,0,1]);m=np.eye(4);m[:3,:3]=np.array([[1-2*(y*y+z*z),2*(x*y-z*w),2*(x*z+y*w)],[2*(x*y+z*w),1-2*(x*x+z*z),2*(y*z-x*w)],[2*(x*z-y*w),2*(y*z+x*w),1-2*(x*x+y*y)]])@np.diag(n.get('scale',[1,1,1]));m[:3,3]=n.get('translation',[0,0,0]);return m
 def visit(i,m,parents):
  nonlocal tri
  assert i not in parents;n=j['nodes'][i];assert 'skin'not in n;m=m@local(n)
  if 'mesh'in n:
   for prim in j['meshes'][n['mesh']]['primitives']:
    assert prim.get('mode',4)==4;a=j['accessors'][prim['attributes']['POSITION']];v=j['bufferViews'][a['bufferView']];assert a['componentType']==5126 and a['type']=='VEC3' and 'sparse'not in a
    g=np.ndarray((a['count'],3),'<f4',buffer=binary,offset=v.get('byteOffset',0)+a.get('byteOffset',0),strides=(v.get('byteStride',12),4)).astype(float);g=g@m[:3,:3].T+m[:3,3];allv.append(g)
    material=j.get('materials',[{}])[prim.get('material',0)]
    if material.get('name')!='Source_Pivot_Datum':visible.append(g);tri+=(j['accessors'][prim['indices']]['count']if'indices'in prim else a['count'])//3
  for c in n.get('children',[]):visit(c,m,parents|{i})
 for i in j['scenes'][j.get('scene',0)]['nodes']:visit(i,np.eye(4),set())
 assert visible and allv;minimum=np.concatenate(allv)[:,1].min();g=np.concatenate(visible);g[:,1]-=minimum;assert np.isfinite(g).all();return g,int(tri),float(minimum)
def decode(line):
 q=line.split();assert len(q)>=14 and not q[0].startswith('#');a=np.array([float(x)for x in q[1:14]]);assert np.isfinite(a).all();return q,{'x':(a[0]-.5)*W,'z':(a[1]-.5)*D,'yaw':a[2],'scale':a[3],'burial':a[4],'elevation':a[5],'offset':a[6],'pitch':a[7],'roll':a[8],'axes':a[9:12],'y':a[12]}
def dist(point,segments):
 a,b=segments[:,0],segments[:,1];v=b-a;t=np.clip(np.sum((point-a)*v,axis=1)/np.maximum(np.sum(v*v,axis=1),1e-16),0,1);return float(np.linalg.norm(point-(a+t[:,None]*v),axis=1).min())
def rows(path):return [(i,line)for i,line in enumerate(path.read_text().splitlines(),1)if line.strip()and not line.lstrip().startswith('#')]
def main():
 patch_path=OUT/'ROW_PATCH.json';j=json.loads(patch_path.read_text());inputs={str(patch_path):sha(patch_path),str(Path(__file__)):sha(__file__)};checks=[];errors=[]
 def check(name,ok,detail=None):
  checks.append({'name':name,'passed':bool(ok),'detail':detail})
  if not ok:errors.append(name)
 for rel in ['engine/world_city_scene.h','engine/world_scenery.cpp','engine/world_scenery.h','engine/world_model_cpu.cpp','engine/world_transform_math.h','engine/world_authoring_limits.h','engine/world_surface_authoring.cpp','engine/world_surface_field.cpp','engine/world_surface_authoring.h','engine/world_surface_field.h','engine/world_river_network.h','engine/world_height_region.h']:
  p=N/'source-v1'/rel;inputs[str(p)]=sha(p)
 studio=(P/'world.studio').read_text().splitlines();inputs[str(P/'world.studio')]=sha(P/'world.studio');inputs[str(P/'city.scene')]=sha(P/'city.scene');check('final_studio_sha_matches_patch',inputs[str(P/'world.studio')]==j['final_surface_studio_sha256'] and j['final_surface_regrounded'] is True);check('no_unhandled_sculpt_tiles',not any(s.startswith('terrain ')for s in studio))
 alias={q[1]:q for s in studio if(q:=shlex.split(s))and q[0]=='asset'}
 # Directly load the previously compiled production surface sampler. Its source
 # is pinned and byte-compared to this generation; no builder functions run.
 bank=E/'build/camp-road-polish-2026-11-19';libpath=bank/'review/route-budget/sample-height.so';helper=bank/'review/route-budget/sample-height.cpp'
 for rel in ['engine/world_surface_authoring.cpp','engine/world_surface_field.cpp','engine/world_surface_authoring.h','engine/world_surface_field.h','engine/world_river_network.h','engine/world_height_region.h']:
  check('production_sampler_source_'+Path(rel).name,(bank/'source-v1'/rel).read_bytes()==(N/'source-v1'/rel).read_bytes())
 for p in [libpath,helper,P/'ground/showcase.height',P/'opening/first-journey.routes.json',P/'opening/road-traffic.settings',P/'opening/arrival-camp.settings']:inputs[str(p)]=sha(p)
 lib=ctypes.CDLL(str(libpath));lib.load.argtypes=[ctypes.c_char_p,ctypes.c_char_p];lib.load.restype=ctypes.c_int;lib.sample_many.argtypes=[ctypes.c_uint,ctypes.POINTER(ctypes.c_double),ctypes.POINTER(ctypes.c_float),ctypes.c_int];lib.sample_many.restype=ctypes.c_int;assert lib.load(str(P/'ground/showcase.height').encode(),str(P/'world.studio').encode())==1
 def ground(points):
  points=np.ascontiguousarray(points,dtype=np.float64).reshape(-1,2);out=np.empty(len(points),dtype=np.float32);assert lib.sample_many(len(points),points.ctypes.data_as(ctypes.POINTER(ctypes.c_double)),out.ctypes.data_as(ctypes.POINTER(ctypes.c_float)),1)==1;return out.astype(float)
 # Independently reconstruct exact baseline-minus-removals-plus-added rows.
 expected_total=0;actual_total=0;counts=collections.Counter();removed_count=0;line_checks=0
 for file,change in j['changed_scenery_files'].items():
  p=P/file;b=B/'world-project'/file;inputs[str(p)]=sha(p);inputs[str(b)]=sha(b);check('owned_final_'+file,p.is_file()and not p.is_symlink()and p.parent.is_dir()and not p.parent.is_symlink());check('before_after_sha_'+file,sha(p)==change['after_sha256']and sha(b)==change['before_sha256'])
  old=b.read_text().splitlines();new=p.read_text().splitlines();rm=j['removals'].get(file,[]);indices={r['line']for r in rm};valid_rm=len(indices)==len(rm)
  for r in rm:valid_rm &= 1<=r['line']<=len(old)and old[r['line']-1]==r['before']and r['alias']in NATURAL
  check('only_reported_natural_removals_'+file,valid_rm)
  kept=[s for k,s in enumerate(old,1)if k not in indices and s.strip()and not s.lstrip().startswith('#')];added=[r['row']for r in j['placements']if r['file']==file];actual=[s for _,s in rows(p)];check('exact_noncomment_row_delta_'+file,actual==kept+added)
  removed_count+=len(rm);expected_total+=len(kept)+len(added);actual_total+=len(actual)
  for _,line in rows(p):counts[line.split()[0]]+=1
 check('removed350_only_nature',removed_count==j['removed_instances']==350);check('added_count_matches_final_v4',len(j['placements'])==j['added_instances'] and len(j['placements'])>2428);check('merged_record_budget',actual_total==expected_total and actual_total<16384,actual_total);check('per_alias4096_limit',max(counts.values())<=4096,counts.most_common(3))
 routes=json.loads((P/'opening/first-journey.routes.json').read_text())['paths'];segments=[]
 for path in routes.values():
  p=np.array(path)[:,[0,2]];segments.extend(zip(p[:-1],p[1:]))
 traffic=np.array([[float(q[1]),float(q[3])]for line in(P/'opening/road-traffic.settings').read_text().splitlines()if(q:=line.split())and q[0]=='point']);segments.extend(zip(traffic[:-1],traffic[1:]));segments=np.array(segments)
 creekline=[shlex.split(json.loads(s[9:]))for s in studio if s.startswith('surfaces ')and json.loads(s[9:]).startswith('ribbon "West woodland creek"')][0];controls=np.array(list(map(float,creekline[9:]))).reshape(-1,3);curve=[controls[0]]
 for i in range(len(controls)-1):
  a,b,c,d=controls[max(0,i-1)],controls[i],controls[i+1],controls[min(i+2,len(controls)-1)];count=max(1,math.ceil(np.linalg.norm((c-b)[[0,2]])/2))
  for k in range(1,count+1):
   t=k/count;v=.5*(2*b+(-a+c)*t+(2*a-5*b+4*c-d)*t*t+(-a+3*b-3*c+d)*t*t*t)
   if np.linalg.norm((v-curve[-1])[[0,2]])>=.01:curve.append(v)
 curve=np.array(curve);curve_segments=np.array(list(zip(curve[:-1,[0,2]],curve[1:,[0,2]])));head=np.array(j['creek_polyline_xz']);head_segments=np.array(list(zip(head[:-1],head[1:])))
 asset={};asset_geometry=[]
 for a,meta in j['assets'].items():
  p=Path(meta['path']);inputs[str(p)]=sha(p);expected=BUILTINS[a][0]if a in BUILTINS else alias[a][2]+('_near'if alias[a][3]=='2'else'')+'.glb';expectedp=P/'models'/expected
  if not expectedp.is_file() and a in alias:expected=alias[a][2]+'.glb';expectedp=P/'models'/expected
  check('existing_asset_path_'+a,p==expectedp and sha(p)==meta['sha256']and (B/'world-project/models'/expected).is_file()and sha(B/'world-project/models'/expected)==sha(p))
  g,tri,shift=geometry(p);asset[a]=g;asset_geometry.append({'alias':a,'visible_vertices':len(g),'triangles':tri,'runtime_y_normalization_m':shift,'span':np.ptp(g,axis=0).tolist()})
 final_lines={rel:(P/rel).read_text().splitlines() for rel in {p['file'] for p in j['placements']}}
 support_error=0;route_min=1e9;creek_min=1e9;root_gap_min=1e9;root_gap_max=-1e9;rows_out=[];parser_bad=[];protected_bad=[];report_rows=True;class_metrics=collections.defaultdict(lambda:{'count':0,'minimum_route_clearance_m':1e9,'minimum_curve_clearance_m':1e9})
 camp=np.array([-11988.,5172.]);actors=np.array([[-11990.15,5171.35],[-11985.8,5171.35],[-11987.9,5168.8]]);protected=[(camp,4.),(np.array([-11988.,5180.]),2.7),(np.array([-11933.,5312.]),8.5),(np.array([-11930.5,5320.]),4.6),(np.array([-11968.,5582.]),5.5)]
 for index,item in enumerate(j['placements']):
  q,p=decode(item['row']);a=q[0];line=final_lines[item['file']][item['line']-1];report_rows &= line==item['row'];line_checks+=1
  maxscale=2.5 if a=='broadleaf' else 2. if a in TREES else 4.;valid=len(item['row'])<=320 and len(' '.join(q[:14]))<=256 and .15<=p['scale']<=maxscale and abs(p['yaw'])<=360 and abs(p['pitch'])<=360 and abs(p['roll'])<=360 and np.all((p['axes']>=.1)&(p['axes']<=8)) and 0<=float(q[1])<=1 and 0<=float(q[2])<=1 and p['burial']==0 and p['elevation']==0 and p['offset']==0 and -2000<=p['y']<=12000
  if len(q)>14:valid &= len(q)==17 and q[14]=='wind'and 0<=float(q[15])<=2 and 0<=float(q[16])<=2
  if not valid:parser_bad.append({'index':index,'alias':a,'row':item['row']})
  point=np.array([p['x'],p['z']]);g=asset[a];v=(g*p['axes']*p['scale'])@rot(p['yaw'],p['pitch'],p['roll']).T;h=np.ptp(v[:,1]);bottom=v[v[:,1]<=v[:,1].min()+max(.02,h*.025)]
  if len(bottom)>64:bottom=bottom[np.linspace(0,len(bottom)-1,64,dtype=int)]
  samples=ground(point+bottom[:,[0,2]]);candidate=samples-bottom[:,1];expected=float(np.quantile(candidate,.35))-item['embed_m'];err=abs(p['y']-expected);support_error=max(support_error,err);gaps=p['y']+bottom[:,1]-samples;root_gap_min=min(root_gap_min,float(gaps.min()));root_gap_max=max(root_gap_max,float(gaps.max()))
  center_ground=ground([point])[0];is_tree=item['kind'] in ('tree','restored_canopy','mature_canopy');low=v[v[:,1]+p['y']-center_ground<4.2] if is_tree else v;assert len(low);radius=np.linalg.norm(low[:,[0,2]],axis=1).max();route=dist(point,segments)-radius
  if item['kind'] in ('restored_canopy','mature_canopy'):
   basal=v[v[:,1]<=v[:,1].min()+1.5];creek_radius=float(np.linalg.norm(basal[:,[0,2]],axis=1).max())
  else:creek_radius=max(.3*p['scale'],.18) if item['kind']=='tree' else radius
  creek=dist(point,curve_segments)-creek_radius;route_min=min(route_min,route);creek_min=min(creek_min,creek);cm=class_metrics[item['kind']];cm['count']+=1;cm['minimum_route_clearance_m']=min(cm['minimum_route_clearance_m'],route);cm['minimum_curve_clearance_m']=min(cm['minimum_curve_clearance_m'],creek)
  if item['kind']=='camp_foreground':
   bad=np.linalg.norm(point-camp)<1.8+radius-1e-7 or np.linalg.norm(point-np.array([-11988.,5180.]))<1.5+radius-1e-7 or np.linalg.norm(point-actors,axis=1).min()<1.1+radius-1e-7 or (-11991.2-radius<point[0]<-11984.8+radius and 5163.8-radius<point[1]<5170.5+radius) or dist(point,np.array([[[-11988.,5180.],[-11988.,5171.]]]))<.75+radius-1e-7
   if bad:protected_bad.append(index)
  elif item['kind'] in ('restored_canopy','mature_canopy'):
   if np.linalg.norm(point-camp)<7-1e-7:protected_bad.append(index)
  elif any(np.linalg.norm(point-c)<r-1e-7 for c,r in protected)or np.linalg.norm(point-actors,axis=1).min()<3.5-1e-7:protected_bad.append(index)
  rows_out.append({'index':index,'file':item['file'],'line':item['line'],'alias':a,'kind':item['kind'],'support_error_m':err,'basal_samples':len(bottom),'minimum_basal_clearance_m':float(gaps.min()),'maximum_basal_clearance_m':float(gaps.max()),'actual_low_mesh_radius_m':float(radius),'route_clearance_m':route,'actual_curve_clearance_m':creek})
 check('reported_final_rows_are_exact',report_rows and line_checks==len(j['placements']));check('all_new_rows_satisfy_runtime_parser_numeric_limits',not parser_bad,parser_bad[:10]);check('all_final_v4_surface_support',support_error<2e-5,support_error);check('all_low_geometry_clear_of_player_horse_routes',all(v['minimum_route_clearance_m']>=(1. if k=='camp_foreground' else 3.2)-2e-5 for k,v in class_metrics.items()),dict(class_metrics));check('all_added_geometry_clear_of_actual_wet_channel',all(v['minimum_curve_clearance_m']>=(6.8 if k in ('restored_canopy','mature_canopy') else 6)-2e-5 for k,v in class_metrics.items()),dict(class_metrics));check('no_new_rows_in_story_interaction_clearings',not protected_bad,protected_bad[:10])
 # Re-evaluate350 removals against the bounded authored head corridor and actual
 # transformed visible-mesh low footprint. Architecture/story aliases excluded.
 rm_max_distance=0;rm_max_edge=-1e9;rm_ok=True
 for group in j['removals'].values():
  for r in group:
   q,p=decode(r['before']);g=asset[r['alias']];v=(g*p['axes']*p['scale'])@rot(p['yaw'],p['pitch'],p['roll']).T;low=v[v[:,1]<=v[:,1].min()+4.2]if r['alias']in TREES else v;radius=np.linalg.norm(low[:,[0,2]],axis=1).max();point=np.array([p['x'],p['z']]);distance=dist(point,head_segments);edge=distance-radius;rm_max_distance=max(rm_max_distance,distance);rm_max_edge=max(rm_max_edge,edge);rm_ok &= r['alias']in NATURAL and distance<30+2e-5 and edge<15+2e-5
 check('all_removals_within_bounded_creek_clearance',rm_ok,{'maximum_center_distance_m':rm_max_distance,'maximum_mesh_edge_distance_m':rm_max_edge})
 restored=[p for p in j['placements'] if p['kind']=='restored_canopy'];restored_keys=set();restore_ok=True
 for p in restored:
  old=p.get('restored_original',{});key=(old.get('file'),old.get('line'));restore_ok &= key not in restored_keys and any(r['line']==old.get('line') and r['before']==old.get('before') and r['alias']==p['alias'] for r in j['removals'].get(old.get('file'),[]));restored_keys.add(key)
  oq,op=decode(old['before']);q,np_=decode(p['row']);restore_ok &= abs(op['x']-np_['x'])<1e-6 and abs(op['z']-np_['z'])<1e-6 and abs(op['yaw']-np_['yaw'])<1e-6 and np.max(np.abs(op['axes']*op['scale']-np_['axes']*np_['scale']))<1e-6
 check('restored_canopies_have_unique_exact_owned_original_refs',restore_ok and len(restored)==j['canopy_restoration_v4']['restored_instances'],len(restored))
 check('v4_class_count_report_matches',class_metrics['mature_canopy']['count']==j['canopy_restoration_v4']['new_mature_canopy'] and class_metrics['camp_foreground']['count']==j['canopy_restoration_v4']['foreground_instances'])
 # V5 is a geometry/count-neutral relocation. Verify actual before/after
 # token/line identity and independently project the actual rendered vertices.
 frustum_evidence={}
 if 'foreground_relocation_v5' in j:
  archived=OUT/'before-foreground-relocation-v5';oldpatch=json.loads((archived/'ROW_PATCH.json').read_text());oldlines=(archived/'authored_instances.txt').read_text().splitlines();newlines=final_lines['scenery/authored_instances.txt']
  for p in [archived/'ROW_PATCH.json',archived/'authored_instances.txt']:inputs[str(p)]=sha(p)
  foreground=[p for p in j['placements'] if p['kind']=='camp_foreground'];changedlines={p['line']-1 for p in foreground};unchanged_other=len(oldlines)==len(newlines) and len(changedlines)==200 and all(a==b for i,(a,b) in enumerate(zip(oldlines,newlines)) if i not in changedlines)
  identity=True
  for item in foreground:
   a=oldlines[item['line']-1].split();b=item['row'].split();identity &= a!=b and len(a)==len(b) and all(a[k]==b[k] for k in [0,3,4,5,6,7,10,11,12,14,15,16])
  check('v5_exactly200_rows_no_canopy_count_shape_or_wind_change',unchanged_other and identity and len(oldpatch['placements'])==len(j['placements']))
  prep_path=N/'review/forest-views/forest-v4/candidate/camp-eye/PREPARATION.json';execution_path=prep_path.with_name('EXECUTION.json');prep=json.loads(prep_path.read_text());execution=json.loads(execution_path.read_text());inputs[str(prep_path)]=sha(prep_path);inputs[str(execution_path)]=sha(execution_path);check('camp_eye_projection_uses_actual_native_preparation',execution.get('preparation_sha256')==sha(prep_path) and prep['resolution']==[1600,900])
  camera=prep['camera'];eye=np.array([camera['x'],camera['y'],camera['z']]);yaw,pitch=camera['yaw'],camera['pitch'];forward=np.array([math.sin(yaw)*math.cos(pitch),math.sin(pitch),-math.cos(yaw)*math.cos(pitch)]);right=np.array([math.cos(yaw),0.,math.sin(yaw)]);up=np.cross(right,forward);tan=math.tan(camera['fov']/2);aspect=prep['resolution'][0]/prep['resolution'][1]
  def projected(line):
   q,p=decode(line);v=(asset[q[0]]*p['axes']*p['scale'])@rot(p['yaw'],p['pitch'],p['roll']).T+np.array([p['x'],p['y'],p['z']]);d=v-eye;depth=d@forward;safe=np.maximum(depth,1e-8);xx=(d@right)/(safe*tan*aspect);yy=(d@up)/(safe*tan);mask=(depth>.12)&(np.abs(xx)<.97)&(np.abs(yy)<.97)
   return {'fraction':float(mask.mean()),'median_ndc':[float(np.median(xx)),float(np.median(yy))],'minimum_depth_m':float(depth.min()),'maximum_depth_m':float(depth.max())}
  records=[];minimum_fraction=1.;report_error=0.;before_visible=0;after_visible=0;within_composed=True
  for item in foreground:
   before=projected(oldlines[item['line']-1]);after=projected(item['row']);minimum_fraction=min(minimum_fraction,after['fraction']);before_visible+=before['fraction']>=.9;after_visible+=after['fraction']>=.9;within_composed &= -.92<after['median_ndc'][0]<.92 and -.90<after['median_ndc'][1]<.1
   report_error=max(report_error,abs(item['camp_eye_frustum']['fraction']-after['fraction']));records.append({'line':item['line'],'alias':item['alias'],'before':before,'after':after})
  check('all200_foreground_meshes_in_actual_camp_eye_static_frustum',len(records)==200 and minimum_fraction>=.9 and within_composed,{'minimum_fraction':minimum_fraction,'before_90percent':before_visible,'after_90percent':after_visible})
  check('v5_frustum_claim_recomputed_from_actual_mesh',report_error<1e-12 and after_visible==j['foreground_relocation_v5']['after_rows_at_least90percent_in_frustum'],report_error)
  frustum_evidence={'camera':camera,'resolution':prep['resolution'],'projection_source':'world_viewer.cpp Camera::forward/right; actual native PREPARATION camera/FOV','minimum_visible_vertex_fraction':minimum_fraction,'before_rows_at_least90percent':before_visible,'after_rows_at_least90percent':after_visible,'records':records,'limitations':'Static-frustum inclusion only. Terrain/props/actors may occlude vertices, and wind may move blades. Only camp-eye composition is claimed; no all-four-view or unobstructed-pixel certification.'}
 unchanged=all(Path(p).is_file()and sha(p)==h for p,h in inputs.items());check('all_inputs_unchanged_during_audit',unchanged)
 receipt={'schema':'valwood.forest-layout-independent-cpu-review.v1','passed':not errors,'gpu_used':False,'project_or_live_modified':False,'production_source_modified':False,'inputs_sha256':inputs,'checks':checks,'errors':errors,'added_instances_checked':len(j['placements']),'new_instances_excluding_restored':len(j['placements'])-len(restored),'restored_instances_checked':len(restored),'class_metrics':dict(class_metrics),'foreground_v5_frustum':frustum_evidence,'removed_instances_checked':removed_count,'merged_instance_count':actual_total,'maximum_alias_instances':max(counts.values()),'actual_geometry':asset_geometry,'maximum_final_surface_support_error_m':support_error,'minimum_basal_vertex_clearance_m':root_gap_min,'maximum_basal_vertex_clearance_m':root_gap_max,'minimum_actual_low_mesh_route_clearance_m':route_min,'minimum_actual_curve_clearance_m':creek_min,'rows':rows_out,'limitations':['CPU geometry/support/clearance evidence, not native visual or performance acceptance.','Basal35th-percentile support intentionally embeds irregular natural roots; every basal vertex is not forced coplanar.','Restored/mature trees may overhang water with crowns; basal1.5m geometry stays6.8m from the creek center, while actual geometry below4.2m independently preserves3.2m road clearance.','Rainy foliage costs, crowd navigation, camera visibility and subjective quality require matched native scenes.']}
 (OUT/'INDEPENDENT_REVIEW.json').write_text(json.dumps(receipt,indent=2)+'\n');print(json.dumps({k:receipt[k]for k in ['passed','errors','merged_instance_count','maximum_final_surface_support_error_m','minimum_basal_vertex_clearance_m','maximum_basal_vertex_clearance_m','minimum_actual_low_mesh_route_clearance_m','minimum_actual_curve_clearance_m']},indent=2));return 0 if receipt['passed'] else 1
if __name__=='__main__':sys.exit(main())
