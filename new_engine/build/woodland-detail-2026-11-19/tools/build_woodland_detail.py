#!/usr/bin/env python3
"""Bounded private authored pass: creek bank pockets and distant woodland gaps."""
from pathlib import Path
import collections, hashlib, importlib.util, json, math, shlex, shutil
import numpy as np
N=Path(__file__).resolve().parents[1];E=N.parents[1];P=N/'world-project';B=N.with_name('campsite-look-2026-11-19')/'world-project';O=N/'review/woodland-detail'
assert N.name=='woodland-detail-2026-11-19' and not (N/'PUBLISHED.json').exists()
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def module(name,path):
 s=importlib.util.spec_from_file_location(name,path);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m
st=module('wd_sampler',N/'tools/build_forest_layers.py');geo=module('wd_geometry',N/'tools/review_forest_layout.py');mesh=module('wd_support',E/'docs/seldonne-first-region/2026-11-07/region_mesh_grounding_v5.py')
ground,helper=st.sampler();rng=np.random.default_rng(202611192032);W=100800.;D=W*2818/5009;camp=np.array([-11988.,5172.]);actors=np.array([[-11990.15,5171.35],[-11985.8,5171.35],[-11987.9,5168.8]])
studio=(P/'world.studio').read_text().splitlines();before_studio='\n'.join(studio)+'\n';assert before_studio==(B/'world.studio').read_text()
source={f.name:f.read_text().splitlines()for f in sorted((P/'scenery').glob('*.txt'))};assert all('\n'.join(q)+'\n'==(B/'scenery'/name).read_text()for name,q in source.items())
O.mkdir(parents=True,exist_ok=True);assert not(O/'PATCH.json').exists();archive=O/'before';archive.mkdir(exist_ok=True);shutil.copy2(P/'world.studio',archive/'world.studio');(archive/'scenery').mkdir(exist_ok=True)
for name in source:shutil.copy2(P/'scenery'/name,archive/'scenery'/name)
aliases={q[1]:q for s in studio if(q:=shlex.split(s))and q[0]=='asset'}
builtin={'rock0':('cinematic_rock_0',1),'rock1':('cinematic_rock_1',1),'rock2':('cinematic_rock_2',1),'fern0':('woodland_fern_0',0),'fern1':('woodland_fern_1',0),'fern2':('woodland_fern_2',0),'pine':('cinematic_pine_near',3),'fir':('cinematic_fir_near',3),'deadwood':('cinematic_deadwood',1),'showcase_shrub':('showcase_shrub',0),'showcase_shrub_b':('showcase_shrub_b',0)}
gg={};assets={}
def asset(a):
 if a in gg:return gg[a]
 if a in builtin:stem,collision=builtin[a]
 else:
  q=aliases[a];stem=q[2]+('_near'if q[3]=='2'else'');collision=int(q[4])
 path=P/'models'/(stem+'.glb')
 if not path.is_file():path=P/'models'/(aliases[a][2]+'.glb')
 g,tris,norm=geo.geometry(path);gg[a]=g;assets[a]=dict(path=str(path),sha256=sha(path),triangles=tris,collision=collision,minimum=g.min(0).tolist(),maximum=g.max(0).tolist(),normalization=norm);return g
segments=[]
for path in json.loads((P/'opening/first-journey.routes.json').read_text())['paths'].values():
 q=np.array(path)[:,[0,2]];segments.extend(zip(q[:-1],q[1:]))
traffic=np.array([[float(q[1]),float(q[3])]for s in(P/'opening/road-traffic.settings').read_text().splitlines()if(q:=s.split())and q[0]=='point']);segments.extend(zip(traffic[:-1],traffic[1:]));segments=np.array(segments)
swim=[];previous=None
for s in(N/'review/fixtures/story-swim-west-creek.qa.walk').read_text().splitlines():
 q=s.split()
 if q and q[0]=='path':previous=None
 elif len(q)==3:
  try:point=np.array([float(q[0]),float(q[2])])
  except ValueError:continue
  if previous is not None:swim.append([previous,point])
  previous=point
swim=np.array(swim)
q=next(shlex.split(json.loads(s[9:]))for s in studio if s.startswith('surfaces ')and json.loads(s[9:]).startswith('ribbon "West woodland creek"'));cc=np.array(list(map(float,q[9:]))).reshape(-1,3);curve=[cc[0]]
for i in range(len(cc)-1):
 a,b,c,d=cc[max(0,i-1)],cc[i],cc[i+1],cc[min(i+2,len(cc)-1)];steps=max(1,math.ceil(np.linalg.norm((c-b)[[0,2]])/2))
 for k in range(1,steps+1):
  t=k/steps;v=.5*(2*b+(-a+c)*t+(2*a-5*b+4*c-d)*t*t+(-a+3*b-3*c+d)*t*t*t)
  if np.linalg.norm((v-curve[-1])[[0,2]])>.01:curve.append(v)
curve=np.array(curve)
def wet(point):
 a=curve[:-1];v=curve[1:]-a;vv=v[:,[0,2]];t=np.clip(np.sum((point-a[:,[0,2]])*vv,axis=1)/np.sum(vv*vv,axis=1),0,1);q=a+v*t[:,None];dd=np.linalg.norm(point-q[:,[0,2]],axis=1);i=int(dd.argmin());return float(dd[i]),float(q[i,1])
def transform(p,g):return(g*np.array([p['sx'],p['sy'],p['sz']])*p['scale'])@mesh.rotation(p['yaw_deg'],p['pitch_deg'],p['roll_deg']).T
existing_trees=[]
for lines in source.values():
 for s in lines:
  q=s.split()
  if len(q)>=14 and q[0]in ['nm_city_region_oak','nm_city_region_beech','pine','fir','broadleaf','pine_young']:p=st.decode(q);existing_trees.append(np.array([p['x'],p['z']]))
existing_trees=np.array(existing_trees);newtrees=[];placements=[];rejected=collections.Counter()
def serialize(a,p,wind=False):
 st.native_scale(p)
 q=[a,f'{p["x"]/W+.5:.12f}',f'{p["z"]/D+.5:.12f}',f'{p["yaw_deg"]:.8f}',f'{p["scale"]:.9f}','0','0','0',f'{p["pitch_deg"]:.8f}',f'{p["roll_deg"]:.8f}',f'{p["sx"]:.10f}',f'{p["sy"]:.10f}',f'{p["sz"]:.10f}',f'{p["y"]:.9f}']
 if wind:q+=['wind','1','1']
 assert len(' '.join(q[:14]))<=256 and len(' '.join(q))<=320;return ' '.join(q)
def place(a,pt,kind,group,scale=1.,axes=None,yaw=None,embed=None):
 g=asset(a);collision=assets[a]['collision'];tree=kind=='horizon_tree';solid=collision!=0
 p=dict(x=float(pt[0]),z=float(pt[1]),yaw_deg=float(rng.uniform(-180,180)if yaw is None else yaw),scale=float(scale),sx=1.,sy=1.,sz=1.,pitch_deg=0.,roll_deg=0.)
 if axes is not None:p.update(zip(['sx','sy','sz'],map(float,axes)))
 if not tree:mesh.align_plant(p,ground)
 embed=(.065 if solid else .02)if embed is None else embed;h=mesh.support(p,g,ground,embed);v=transform(p,g);pt=np.array(pt);radius=float(np.linalg.norm(v[:,[0,2]],axis=1).max());route=geo.dist(pt,segments)-radius;fixture=geo.dist(pt,swim)-radius;distance,water=wet(pt)
 bottom=v[v[:,1]<=v[:,1].min()+max(.02,h*.025)]
 if len(bottom)>64:bottom=bottom[np.linspace(0,len(bottom)-1,64,dtype=int)]
 gaps=np.array([p['y']+b[1]-ground(p['x']+b[0],p['z']+b[2])for b in bottom])
 threshold=.14 if solid else .08
 if gaps.max()>threshold or gaps.min()<(-.85 if tree else -.45 if solid else -.22):rejected['support']+=1;return False
 if (not tree and (distance<5.35 or p['y']-water<.16 or distance-radius<4.9)):rejected['dry_water']+=1;return False
 if route<(2.2 if solid else .75) or fixture<(2.2 if solid else .6):rejected['routes']+=1;return False
 if np.linalg.norm(pt-camp)<(19. if tree else 5.+radius) or np.linalg.norm(pt-actors,axis=1).min()<(8. if solid else 1.5)+radius:rejected['camp']+=1;return False
 # Preserve the complete 10m channel and keep solid geometry off its outer edge.
 # Explicit dry pivots obey the actual production placement guard.
 wetmargin=5.15 if solid else 4.8
 if distance-radius<wetmargin or (distance<12 and p['y']<=water+.17):rejected['water']+=1;return False
 if -12013.5-radius<pt[0]<-12008+radius and 5177.8-radius<pt[1]<5183.3+radius:rejected['reserved_bank_entry']+=1;return False
 if tree:
  if np.linalg.norm(pt-camp)<55:rejected['horizon_near']+=1;return False
  if min(np.linalg.norm(existing_trees-pt,axis=1))<3.5 or(newtrees and min(np.linalg.norm(np.array(newtrees)-pt,axis=1))<5.):rejected['trunk_spacing']+=1;return False
  if gaps.max()>.16 or gaps.min()<-.85:rejected['tree_support']+=1;return False
  newtrees.append(pt)
 if solid and not tree:
  # Conservative transformed production box encloses visible rock/wood.
  lo=np.array(assets[a]['minimum']);hi=np.array(assets[a]['maximum']);corners=np.array([[x,y,z]for x in [lo[0],hi[0]]for y in [lo[1],hi[1]]for z in [lo[2],hi[2]]]);box=transform(p,corners);br=float(np.linalg.norm(box[:,[0,2]],axis=1).max());route_box=geo.dist(pt,segments)-br;fixture_box=geo.dist(pt,swim)-br
  if min(route_box,fixture_box)<2.2 or distance-br<5.05:rejected['collider_clearance']+=1;return False
 else:br=None;route_box=None;fixture_box=None
 rec=dict(alias=a,row=serialize(a,p,kind in ['bank_plant','horizon_shrub']),kind=kind,group=group,pose=p,actual_height_m=float(h),full_mesh_radius_m=radius,route_clearance_m=route,swim_fixture_clearance_m=fixture,creek_full_mesh_clearance_m=distance-radius,center_curve_distance_m=distance,dry_pivot_above_water_m=p['y']-water,support=dict(embed_m=embed,basal_samples=len(bottom),minimum_basal_clearance_m=float(gaps.min()),maximum_basal_clearance_m=float(gaps.max())),collision_mode=collision,conservative_box_radius_m=br,box_route_clearance_m=route_box,box_swim_clearance_m=fixture_box)
 if tree:rec['production_trunk_radius_m']=(.38 if a=='pine'else .42)*p['scale']
 placements.append(rec);return True
# Grouped, irregular bank pockets: different silhouettes and size rhythms.
anchors=[]
for station in [5145.,5152.,5159.,5166.,5172.,5189.]:
 i=int(np.argmin(abs(curve[:,2]-station)));c=curve[i];t=curve[min(len(curve)-1,i+1),[0,2]]-curve[max(0,i-1),[0,2]];t/=np.linalg.norm(t);n=np.array([-t[1],t[0]])
 for side in [-1,1]:anchors.append(dict(center=c[[0,2]],normal=n*side,tangent=t,station=station,side=side))
for i,anchor in enumerate(anchors):
 c,n,t=anchor['center'],anchor['normal'],anchor['tangent'];group=f'bank_pocket_{i:02d}';nrocks=0
 for attempt in range(25):
  if nrocks>=2:break
  alias=['rock1','rock0','rock2'][(i+nrocks)%3];pt=c+n*float(rng.uniform(6.75,8.6))+t*float(rng.uniform(-1.5,1.5));scale={'rock0':.46,'rock1':.37,'rock2':.26}[alias]*float(rng.uniform(.9,1.15))
  nrocks+=int(place(alias,pt,'bank_boulder',group,scale,axes=[1,float(rng.uniform(.72,.98)),float(rng.uniform(.80,1.12))]))
 # Tiny noncolliding fragments remain below player step height, not hidden blockers.
 nlow=0
 for attempt in range(20):
  if nlow>=3:break
  pt=c+n*float(rng.uniform(5.55,8.7))+t*float(rng.uniform(-2.0,2.0));nlow+=int(place('opening_road_stone',pt,'bank_pebble',group,.15,axes=[float(rng.uniform(.55,.95)),float(rng.uniform(.42,.58)),float(rng.uniform(.5,.9))],embed=.025))
 count=0
 for attempt in range(120):
  if count>=14:break
  pt=c+n*float(rng.uniform(6.1,10.6))+t*float(rng.uniform(-3.2,3.2));alias=['bep_nature_grass_long','fern0','bep_nature_grass_broad','fern0','bep_nature_clover','fern1','bep_nature_grass_long'][count%7]
  scale=float(rng.uniform(.38,.61))if 'grass'in alias else float(rng.uniform(.57,.83))if alias.startswith('fern')else float(rng.uniform(.25,.37))
  count+=int(place(alias,pt,'bank_plant',group,scale,axes=[float(rng.uniform(.84,1.1)),float(rng.uniform(.78,1.12)),float(rng.uniform(.85,1.12))]))
 # Exposed roots and fallen branch scraps on the upper bank, not identical rim rows.
 for k in range(2):
  for attempt in range(12):
   pt=c+n*float(rng.uniform(7.3,10.8))+t*float(rng.uniform(-2.5,2.5))
   if place('opening_road_roots',pt,'bank_roots',group,.23,axes=[1,.4,.8],embed=.035):break
 if i%3==0:
  for attempt in range(15):
   pt=c+n*float(rng.uniform(9.5,12.8))+t*float(rng.uniform(-1.5,1.5))
   if place('deadwood',pt,'bank_deadwood',group,.55,axes=[1,.7,.9],embed=.09):break
 # Low upper-bank shrubs interrupt a uniformly cropped plant edge.
 for k in range(2):
  for attempt in range(10):
   pt=c+n*float(rng.uniform(9.5,13.5))+t*float(rng.uniform(-3.2,3.2))
   if place('showcase_shrub'if(i+k)%2 else'showcase_shrub_b',pt,'bank_shrub',group,float(rng.uniform(.65,1.0)),axes=[1,float(rng.uniform(.75,.95)),1]):break
# Three composed copses occupy the pale gaps seen through existing foreground trunks.
horizon=[[-12018,5114],[-12046,5109],[-12078,5136]];horizon_groups=[]
for i,center in enumerate(horizon):
 center=np.array(center);count=0
 for attempt in range(120):
  if count>=6:break
  angle=attempt*2.399963;radius=2+math.sqrt(attempt%19)*2.1;pt=center+[math.cos(angle)*radius,math.sin(angle)*radius*.75];a='pine'if count%3==0 else'fir';g=asset(a);height=float(rng.uniform(15,22));scale=height/float(np.ptp(g[:,1]));count+=int(place(a,pt,'horizon_tree',f'hill_copse_{i}',scale,embed=.045))
 horizon_groups.append(dict(center_xz=center.tolist(),trees=count))
 for k in range(6):
  for attempt in range(20):
   pt=center+[rng.uniform(-14,14),rng.uniform(-9,9)]
   if place('showcase_shrub'if k%2 else'showcase_shrub_b',pt,'horizon_shrub',f'hill_copse_{i}',float(rng.uniform(.85,1.25)),axes=[1,float(rng.uniform(.85,1.0)),1]):break
assert all(q['trees']>=4 for q in horizon_groups),(horizon_groups,rejected)
# Rework a measured subset of the repeated near-right broadleaf fringe; preserve mass.
changes=[];candidates=[]
for name,lines in source.items():
 for i,s in enumerate(lines):
  q=s.split()
  if len(q)>=14 and q[0]in ['bep_nature_fern','bep_nature_grass_broad']:
   p=st.decode(q)
   if -11982<p['x']<-11973 and 5173<p['z']<5189:candidates.append((name,i,s,p))
for j,(name,i,before,p)in enumerate(candidates[::3][:24]):
 alias='fern0'if j%2 else'fern2';g=asset(alias);oldg=asset(before.split()[0]);old_height=float(np.ptp(transform(p,oldg)[:,1]));height=float(np.clip(old_height*.85,.38,.67));p.update(scale=height/float(np.ptp(g[:,1])),sx=1.,sy=1.,sz=1.,pitch_deg=0.,roll_deg=0.);mesh.align_plant(p,ground);mesh.support(p,g,ground,.022);after=serialize(alias,p,True);source[name][i]=after;changes.append(dict(file='scenery/'+name,line=i+1,before_row=before,after_row=after,old_alias=before.split()[0],new_alias=alias,kind='fringe_species_variation',pose=p))
# Recompose only existing nearby paint donors. River material still controls the wet shoulder.
paint_changes=[];donors=[]
for i,s in enumerate(studio):
 if not s.startswith('surfaces '):continue
 q=shlex.split(json.loads(s[9:]))
 if q and q[0]=='paint' and int(q[5])in [1,7] and (float(q[1])+11988)**2+(float(q[2])-5172)**2<110**2 and float(q[3])<8:donors.append(i)
assert len(donors)>=18
paint_targets=[(-11980.8,5177.4,4.8,.84,7),(-11978.6,5183.2,4.3,.67,1),(-11995.0,5176.0,4.5,.76,7),(-11998.1,5168.4,5.8,.80,1),(-11983.8,5158.0,5.0,.76,7),(-11974.9,5163.0,5.1,.80,1)]
for a in anchors:
 pt=a['center']+a['normal']*13.0+a['tangent']*(1.4 if a['side']>0 else-1.1);paint_targets.append((pt[0],pt[1],5.2 if a['station']<5165 else 6.3,.80 if a['side']>0 else.70,6 if a['station']in [5152.,5166.]else 7))
for index,target in zip(donors[:18],paint_targets):
 x,z,r,amount,layer=target;before=studio[index];after='surfaces '+json.dumps(f'paint {x:.7f} {z:.7f} {r:.2f} {amount:.2f} {layer} 0');studio[index]=after;paint_changes.append(dict(line=index+1,before=before,after=after))
old='surfaces '+json.dumps('layer "Arrival detailed woodland litter" 2 3 4 0.58 0.56 0.47 2.4 0.98 0 1 0');assert studio.count(old)==1;idx=studio.index(old);new='surfaces '+json.dumps('layer "Arrival detailed woodland litter" 2 3 4 0.58 0.56 0.47 1.35 0.98 0 1.10 0');studio[idx]=new;paint_changes.append(dict(line=idx+1,before=old,after=new,kind='world_tiled_local_litter_scale_normal'))
start=len(source['authored_instances.txt'])+2;source['authored_instances.txt']+=['# Woodland detail20261119: irregular bank pockets and targeted distant woodland.']+[r['row']for r in placements]
for i,r in enumerate(placements):r.update(file='scenery/authored_instances.txt',line=start+i)
counts=collections.Counter()
for lines in source.values():
 for s in lines:
  q=s.split()
  if q and not q[0].startswith('#'):counts[q[0]]+=1
assert sum(counts.values())<=16384 and max(counts.values())<=4096
assert before_studio==(P/'world.studio').read_text()
for name,lines in source.items():assert (P/'scenery'/name).read_bytes()==(B/'scenery'/name).read_bytes()
changed={}
for name,lines in source.items():
 path=P/'scenery'/name;after='\n'.join(lines)+'\n'
 if after!=path.read_text():before=sha(path);path.write_text(after);changed['scenery/'+name]=dict(before_sha256=before,after_sha256=sha(path))
(P/'world.studio').write_text('\n'.join(studio)+'\n');changed['world.studio']=dict(before_sha256=hashlib.sha256(before_studio.encode()).hexdigest(),after_sha256=sha(P/'world.studio'))
report=dict(schema='valwood.woodland-detail.authored.v1',private_candidate=str(N),baseline=str(B.parent),published_or_live_modified=False,tool_sha256=sha(__file__),changed_files=changed,placements=placements,changed_plant_rows=changes,changed_studio_rows=paint_changes,counts=dict(collections.Counter(r['kind']for r in placements)),horizon_groups=horizon_groups,rejected_attempts=dict(rejected),assets=assets,added_lod0_triangles=sum(assets[r['alias']]['triangles']for r in placements),replacement_lod0_triangle_delta=sum(assets[r['new_alias']]['triangles']-assets[r['old_alias']]['triangles']for r in changes),total_records=sum(counts.values()),registered_aliases_added=[],terrain_height_unchanged_sha256=sha(P/'ground/showcase.height'),water_ribbons_unchanged=True,road_ribbons_unchanged=True,baseline_solids_unchanged=True,actual_terrain_sampler_sha256=sha(helper),limitations=['Native visual and performance review remains required.','No bank sculpt: exact water/swimming, terrain, bridge/camp/road supports are preserved.','River ribbon material overrides paint within its shoulder; paint improvements target clearing and upper bank.','All new rocks/logs above step height use production collision aliases; small collision0 stones remain below step height.'])
(O/'PATCH.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({k:report[k]for k in ['counts','added_lod0_triangles','replacement_lod0_triangle_delta','total_records','horizon_groups','rejected_attempts']},indent=2))
