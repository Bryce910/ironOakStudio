#!/usr/bin/env python3
"""Private, bounded V3: sampled shore details and twelve measured tree swaps."""
from pathlib import Path
import collections,hashlib,importlib.util,json,math,shlex
import numpy as np
N=Path(__file__).resolve().parents[1];E=N.parents[1];P=N/'world-project';O=N/'review/camp-composition';A=O/'before-v3'
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def mod(name,path):
 s=importlib.util.spec_from_file_location(name,path);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m
stage=mod('v3_sampler',N/'tools/build_forest_layers.py');geo=mod('v3_geometry',N/'tools/review_forest_layout.py');mesh=mod('v3_support',E/'docs/seldonne-first-region/2026-11-07/region_mesh_grounding_v5.py')
assert N.name=='campsite-look-2026-11-19' and not (N/'PUBLISHED.json').exists() and not(O/'V3_PATCH.json').exists()
studio=(P/'world.studio').read_bytes();assert studio==(A/'world.studio').read_bytes();studio_sha=sha(P/'world.studio')
ground,helper=stage.sampler();camp=np.array([-11988.,5172.]);actors=np.array([[-11990.15,5171.35],[-11985.8,5171.35],[-11987.9,5168.8]])
aliases={q[1]:q for s in studio.decode().splitlines()if(q:=shlex.split(s))and q[0]=='asset'}
paths=json.loads((P/'opening/first-journey.routes.json').read_text())['paths'];segments=[]
for pp in paths.values():
 pp=np.array(pp)[:,[0,2]];segments.extend(zip(pp[:-1],pp[1:]))
traffic=np.array([[float(q[1]),float(q[3])]for s in(P/'opening/road-traffic.settings').read_text().splitlines()if(q:=s.split())and q[0]=='point']);segments.extend(zip(traffic[:-1],traffic[1:]));segments=np.array(segments)
q=next(shlex.split(json.loads(s[9:]))for s in studio.decode().splitlines()if s.startswith('surfaces ')and json.loads(s[9:]).startswith('ribbon "West woodland creek"'));controls=np.array(list(map(float,q[9:]))).reshape(-1,3);curve=[controls[0]]
for i in range(len(controls)-1):
 a,b,c,d=controls[max(0,i-1)],controls[i],controls[i+1],controls[min(i+2,len(controls)-1)];steps=max(1,math.ceil(np.linalg.norm((c-b)[[0,2]])/2))
 for k in range(1,steps+1):
  t=k/steps;v=.5*(2*b+(-a+c)*t+(2*a-5*b+4*c-d)*t*t+(-a+3*b-3*c+d)*t*t*t)
  if np.linalg.norm((v-curve[-1])[[0,2]])>.01:curve.append(v)
curve=np.array(curve)
def wet(point):
 a=curve[:-1];v=curve[1:]-a;vv=v[:,[0,2]];t=np.clip(np.sum((point-a[:,[0,2]])*vv,axis=1)/np.sum(vv*vv,axis=1),0,1);q=a+v*t[:,None];d=np.linalg.norm(point-q[:,[0,2]],axis=1);i=int(np.argmin(d));return float(d[i]),float(q[i,1])
assets={};gg={}
def asset(a):
 if a in gg:return gg[a]
 if a in ['pine','fir']:stem='cinematic_'+a+'_near';collision=3
 else:
  q=aliases[a];stem=q[2]+('_near'if q[3]=='2'else'');collision=int(q[4])
 path=P/'models'/(stem+'.glb')
 if not path.is_file():path=P/'models'/(aliases[a][2]+'.glb')
 g,tri,norm=geo.geometry(path);gg[a]=g;assets[a]=dict(path=str(path),sha256=sha(path),triangles=tri,collision=collision,minimum=g.min(0).tolist(),maximum=g.max(0).tolist(),runtime_y_normalization_m=norm)
 return g
def transform(p,g):return(g*np.array([p['sx'],p['sy'],p['sz']])*p['scale'])@mesh.rotation(p['yaw_deg'],p['pitch_deg'],p['roll_deg']).T
def support_metrics(p,g,embed):
 h=mesh.support(p,g,ground,embed);v=transform(p,g);bottom=v[v[:,1]<=v[:,1].min()+max(.02,h*.025)]
 if len(bottom)>64:bottom=bottom[np.linspace(0,len(bottom)-1,64,dtype=int)]
 gaps=np.array([p['y']+b[1]-ground(p['x']+b[0],p['z']+b[2])for b in bottom]);return v,dict(height_m=float(h),embed_m=embed,minimum_basal_clearance_m=float(gaps.min()),maximum_basal_clearance_m=float(gaps.max()))
def row(a,p,wind=False):
 q=[a,f'{p["x"]/100800+.5:.12f}',f'{p["z"]/(100800*2818/5009)+.5:.12f}',f'{p["yaw_deg"]:.8f}',f'{p["scale"]:.9f}','0','0','0',f'{p["pitch_deg"]:.8f}',f'{p["roll_deg"]:.8f}',f'{p["sx"]:.10f}',f'{p["sy"]:.10f}',f'{p["sz"]:.10f}',f'{p["y"]:.9f}']
 if wind:q+=['wind','1','1']
 assert len(' '.join(q[:14]))<=256 and len(' '.join(q))<=320
 return ' '.join(q)
source={f.name:f.read_text().splitlines()for f in sorted((P/'scenery').glob('*.txt'))}
assert all('\n'.join(lines)+'\n'==(A/'scenery'/name).read_text()for name,lines in source.items())
oldtrees=[]
for name,lines in source.items():
 for i,s in enumerate(lines,1):
  q=s.split()
  if len(q)>=14 and q[0]in ['nm_city_region_oak','nm_city_region_beech']:
   p=stage.decode(q);pt=np.array([p['x'],p['z']]);d=np.linalg.norm(pt-camp)
   if 18<=d<=80:oldtrees.append(dict(file='scenery/'+name,line=i,row=s,alias=q[0],pose=p,point=pt))
targets=[[-11981.9,5152],[-12003.8,5161.7],[-11970,5144],[-11998,5118],[-12012,5119],[-12025,5124],[-12042,5146],[-11965,5124],[-12004,5128],[-11935,5157],[-12030,5135],[-11935,5113]]
replacements=[];used=set();rejects=collections.Counter()
for index,target in enumerate(targets):
 new_alias='pine'if index%2==0 else'fir';g=asset(new_alias);chosen=None
 for old in sorted(oldtrees,key=lambda p:np.linalg.norm(p['point']-target)):
  key=(old['file'],old['line'])
  if key in used:continue
  original_g=asset(old['alias']);oldp=old['pose'];old_h=float(np.ptp(transform(oldp,original_g)[:,1]));desired=float(np.clip(old_h,14.,22.));scale=desired/float(np.ptp(g[:,1]))
  p=dict(x=oldp['x'],z=oldp['z'],yaw_deg=oldp['yaw_deg'],pitch_deg=0.,roll_deg=0.,scale=scale,sx=1.,sy=1.,sz=1.)
  v,support=support_metrics(p,g,.045);pt=old['point'];radius=float(np.linalg.norm(v[:,[0,2]],axis=1).max());route=geo.dist(pt,segments)-radius;creek,wy=wet(pt)
  basal=v[v[:,1]<=v[:,1].min()+1.5];basal_radius=float(np.linalg.norm(basal[:,[0,2]],axis=1).max())
  trunk_radius=(.38 if new_alias=='pine'else .42)*scale;collider_route=geo.dist(pt,segments)-trunk_radius
  if route<2.:rejects['full_mesh_route']+=1;continue
  if creek-basal_radius<6.8 or ground(*pt)<wy+.18:rejects['dry_trunk_water']+=1;continue
  if np.linalg.norm(pt-camp)<18 or np.linalg.norm(pt-actors,axis=1).min()-radius<8:rejects['story_clearing']+=1;continue
  if support['maximum_basal_clearance_m']>.13 or support['minimum_basal_clearance_m']<-.3:rejects['base_support']+=1;continue
  chosen=dict(file=old['file'],line=old['line'],before_row=old['row'],after_row=row(new_alias,p),old_alias=old['alias'],new_alias=new_alias,
   x=p['x'],z=p['z'],target_height_m=desired,actual_height_m=support['height_m'],uniform_scale=scale,axes=[1,1,1],pitch_deg=0,roll_deg=0,
   full_mesh_radius_m=radius,minimum_full_mesh_route_clearance_m=route,minimum_collider_route_clearance_m=collider_route,
   minimum_basal_creek_clearance_m=creek-basal_radius,camp_root_distance_m=float(np.linalg.norm(pt-camp)),minimum_full_mesh_actor_clearance_m=float(np.linalg.norm(pt-actors,axis=1).min()-radius),
   root_ground_above_water_m=ground(*pt)-wy,support=support,collision=dict(mode=3,production_branch='Collision::Trunk -> append_collider; extended(a)==false',circle=True,x=p['x'],z=p['z'],radius_m=trunk_radius,ymin=p['y'],ymax=p['y']+float(g[:,1].max())*scale),pose=p)
  used.add(key);break
 assert chosen is not None,('No safe tree replacement',index,rejects)
 replacements.append(chosen)
assert collections.Counter(p['new_alias']for p in replacements)=={'pine':6,'fir':6}
shore=json.loads((O/'SHORELINE_PROPOSAL.json').read_text());assert shore['source_studio_sha256']==studio_sha
details=[]
def detail(a,p,kind,label,embed,maxheight,wet_plant=False):
 g=asset(a);v,s=support_metrics(p,g,embed);pt=np.array([p['x'],p['z']]);rad=float(np.linalg.norm(v[:,[0,2]],axis=1).max());creek,wy=wet(pt);route=geo.dist(pt,segments)-rad
 if s['height_m']>maxheight+1e-6 or s['maximum_basal_clearance_m']>.045 or s['minimum_basal_clearance_m']<-.11 or route<2:return False
 # Keep a broad open central swimming corridor; shore stones may straddle the actual wet contour.
 if creek-rad<3.65:return False
 if wet_plant and (ground(*pt)-wy<.16 or creek-rad<4.7):return False
 details.append(dict(alias=a,kind=kind,label=label,row=row(a,p,kind=='bank_grass'),pose=p,support=s,full_mesh_radius_m=rad,creek_clearance_m=creek-rad,route_clearance_m=route,
  nearest_water_y=wy,actual_mesh_top_above_water_m=float(v[:,1].max()+p['y']-wy),actual_mesh_bottom_below_water_m=float(wy-v[:,1].min()-p['y']),collision_mode=0));return True
for i,s in enumerate(shore['proposals']):
 p=s['pose'].copy();assert detail('opening_road_stone',p,'shore_stone',f'waterline_pocket_{s["anchor"]}_{i}',.018,.235)
rng=np.random.default_rng(20261119)
for i,a in enumerate(shore['anchors']):
 n=np.array(a['outward_xz']);t=np.array([-n[1],n[0]]);c=np.array(a['center_xz'])
 for k in range(3):
  pt=c+n*(.70+.13*k)+t*((k-1)*.34);p=dict(x=pt[0],z=pt[1],yaw_deg=float(rng.uniform(-180,180)),pitch_deg=0.,roll_deg=0.,scale=float(rng.uniform(.8,1.1)),sx=.85,sy=.85,sz=.85);mesh.align_plant(p,ground)
  detail('opening_verge_grass_long',p,'bank_grass',f'bank_root_tuft_{i}_{k}',.02,.35,True)
 if i%2==0:
  pt=c+n*.55+t*.75;p=dict(x=pt[0],z=pt[1],yaw_deg=float(rng.uniform(-180,180)),pitch_deg=0.,roll_deg=0.,scale=.15,sx=.6,sy=.3,sz=.65);mesh.align_plant(p,ground)
  detail('opening_road_roots',p,'bank_root_fragment',f'bank_root_fragment_{i}',.025,.22,True)
assert len([d for d in details if d['kind']=='shore_stone'])==23
assert len(details)<=51
final={name:lines.copy()for name,lines in source.items()}
for change in replacements:
 lines=final[Path(change['file']).name];assert lines[change['line']-1]==change['before_row'];lines[change['line']-1]=change['after_row']
start=len(final['authored_instances.txt'])+2
final['authored_instances.txt']+=['# Campsite look20261119 V3: low waterline detail and controlled mixed canopy.']+[d['row']for d in details]
for i,d in enumerate(details):d.update(file='scenery/authored_instances.txt',line=start+i)
counts=collections.Counter()
for name,lines in final.items():
 for s in lines:
  q=s.split()
  if q and not q[0].startswith('#'):counts[q[0]]+=1
assert sum(counts.values())<=16384 and max(counts.values())<=4096
assert studio==(P/'world.studio').read_bytes(),'Concurrent studio edit'
changed={}
for name,lines in final.items():
 before='\n'.join(source[name])+'\n';after='\n'.join(lines)+'\n'
 if before!=after:
  path=P/'scenery'/name;assert path.read_text()==before and not path.is_symlink();path.write_text(after);changed[name]=dict(before_sha256=hashlib.sha256(before.encode()).hexdigest(),after_sha256=sha(path))
tree_delta=sum(assets[r['new_alias']]['triangles']-assets[r['old_alias']]['triangles']for r in replacements)
detail_tris=sum(assets[d['alias']]['triangles']for d in details)
report=dict(schema='valwood.campsite-shore-canopy-v3.v1',private_candidate=True,published_or_live_modified=False,tool_sha256=sha(__file__),
 before_archive=str(A),world_studio_unchanged_sha256=studio_sha,changed_scenery_files=changed,tree_replacements=replacements,tree_replacement_count=12,new_pines=6,new_firs=6,
 added_shore_details=details,shore_detail_counts=dict(collections.Counter(d['kind']for d in details)),assets=assets,tree_lod0_triangle_delta=tree_delta,shore_detail_lod0_triangles=detail_tris,
 total_lod0_triangle_delta_from_v2=tree_delta+detail_tris,merged_records=sum(counts.values()),added_registered_aliases=[],existing_builtin_tree_aliases_used=['pine','fir'],
 native_tree_alias_requirements=dict(pine=dict(stem='cinematic_pine',lods=3,collision=3,trunk_radius=.38),fir=dict(stem='cinematic_fir',lods=3,collision=3,trunk_radius=.42)),
 all_other_rows_unchanged=True,terrain_height_unchanged_sha256=sha(P/'ground/showcase.height'),surface_sampler_sha256=sha(helper),
 expected_additional_unique_tree_rgba8_mip_bytes=233270548,texture_memory_note='Upper estimate for two mature conifer families if not already resident/compressed. Native texture cache statistics and hardware frame timing required.',
 rejected_tree_candidates=dict(rejects),limitations=['No native visual/performance acceptance yet.','Shore stones are decorative collision0 and below0.235m actual transformed height; the central channel remains clear.','Twelve tree blockers are intentionally regenerated by unchanged production Collision::Trunk logic with uniform axes and zero tilt, so they stay circular and cannot become large AABBs.','This small bounded detail pass does not add bank landforms or repair every exposed distant hill.'])
(O/'V3_PATCH.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({k:report[k]for k in ['tree_replacement_count','shore_detail_counts','tree_lod0_triangle_delta','shore_detail_lod0_triangles','total_lod0_triangle_delta_from_v2','merged_records']},indent=2))
