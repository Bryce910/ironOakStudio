#!/usr/bin/env python3
"""Recompose only private campsite scenery; preserve all collision and water rows."""
from pathlib import Path
import collections, ctypes, hashlib, importlib.util, json, math, shlex, struct
import numpy as np

N=Path(__file__).resolve().parents[1]; E=N.parents[1]; P=N/'world-project'
B=N.with_name('river-swim-2026-11-19')/'world-project'; O=N/'review/camp-composition'
W=100800.; D=W*2818/5009; CAMP=np.array([-11988.,5172.])
def sha(p): return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def module(name,path):
 s=importlib.util.spec_from_file_location(name,path);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m
mesh=module('camp_mesh',E/'docs/seldonne-first-region/2026-11-07/region_mesh_grounding_v5.py')
audit=module('camp_read_geometry',N/'tools/review_forest_layout.py')
stage=module('camp_surface_sampler',N/'tools/build_forest_layers.py')
ground,helper=stage.sampler()
rng=np.random.default_rng(202611191923)
O.mkdir(parents=True,exist_ok=True)
assert N.name=='campsite-look-2026-11-19' and not (N/'PUBLISHED.json').exists()
studio=(P/'world.studio').read_text().splitlines()
initial_studio_sha=sha(P/'world.studio')
aliases={q[1]:q for s in studio if(q:=shlex.split(s)) and q[0]=='asset'}
new_aliases=[
 'asset camp_fallen_wood cinematic_deadwood 1 0 0 100 22 1',
 'asset camp_broadleaf_young cinematic_broadleaf 2 0 2 130 25 1',
]
for row in new_aliases:
 q=shlex.split(row)
 if q[1] in aliases: assert aliases[q[1]]==q
 else: studio.append(row);aliases[q[1]]=q
BUILT={'fern0':'woodland_fern_0','fern1':'woodland_fern_1','fern2':'woodland_fern_2'}
assets={};geometries={}
def asset(a):
 if a in geometries:return geometries[a]
 if a in BUILT:stem=BUILT[a];collision=0
 else:
  q=aliases[a];stem=q[2]+('_near'if q[3]=='2'else'');collision=int(q[4])
 path=P/'models'/(stem+'.glb')
 if not path.is_file():path=P/'models'/(aliases[a][2]+'.glb')
 g,tris,normalization=audit.geometry(path)
 geometries[a]=g;assets[a]=dict(path=str(path),sha256=sha(path),triangles=tris,collision=collision,span=np.ptp(g,axis=0).tolist(),runtime_y_normalization_m=normalization)
 return g
def dist(point,segments):
 a=segments[:,0];v=segments[:,1]-a;t=np.clip(np.sum((point-a)*v,axis=1)/np.maximum(np.sum(v*v,axis=1),1e-10),0,1)
 return float(np.linalg.norm(point-a-t[:,None]*v,axis=1).min())
routes=json.loads((P/'opening/first-journey.routes.json').read_text())['paths'];segs=[]
for points in routes.values():
 a=np.asarray(points)[:,[0,2]];segs.extend(zip(a[:-1],a[1:]))
traffic=np.array([[float(q[1]),float(q[3])] for s in(P/'opening/road-traffic.settings').read_text().splitlines()if(q:=s.split())and q[0]=='point'])
segs.extend(zip(traffic[:-1],traffic[1:]));segs=np.array(segs)
creek=next(shlex.split(json.loads(s[9:]))for s in studio if s.startswith('surfaces ')and json.loads(s[9:]).startswith('ribbon "West woodland creek"'))
controls=np.asarray(list(map(float,creek[9:]))).reshape(-1,3);curve=[controls[0]]
for i in range(len(controls)-1):
 a,b,c,d=controls[max(0,i-1)],controls[i],controls[i+1],controls[min(i+2,len(controls)-1)]
 for k in range(1,max(1,math.ceil(np.linalg.norm((c-b)[[0,2]])/2))+1):
  t=k/max(1,math.ceil(np.linalg.norm((c-b)[[0,2]])/2));v=.5*(2*b+(-a+c)*t+(2*a-5*b+4*c-d)*t*t+(-a+3*b-3*c+d)*t*t*t)
  if np.linalg.norm((v-curve[-1])[[0,2]])>.01:curve.append(v)
curve=np.asarray(curve)
def wet(point):
 a=curve[:-1];v=curve[1:]-a;vv=v[:,[0,2]];t=np.clip(np.sum((point-a[:,[0,2]])*vv,axis=1)/np.sum(vv*vv,axis=1),0,1);q=a+v*t[:,None];dd=np.linalg.norm(point-q[:,[0,2]],axis=1);i=int(np.argmin(dd));return float(dd[i]),float(q[i,1])
actors=np.array([[-11990.15,5171.35],[-11985.8,5171.35],[-11987.9,5168.8]])
REMOVE={'opening_verge_grass_long','opening_verge_grass_broad','bep_nature_grass_long','bep_nature_grass_broad','bep_nature_clover','bep_nature_fern','fern0','fern1','fern2'}
# These200 baseline rows were deliberately composed in the native camp-eye
# foreground. V1 removed them and visibly regressed the clearing; retain their
# exact safe transforms while replacing the distant scattered pins.
old_foreground=json.loads((E/'build/scenery-forest-2026-11-19/review/forest-layout/ROW_PATCH.json').read_text())
old_foreground=[p for p in old_foreground['placements']if p['kind']=='camp_foreground']
assert len(old_foreground)==200
keep_foreground_rows={p['row']for p in old_foreground}
removals={};base={};kept={};trees=[]
for f in sorted((B/'scenery').glob('*.txt')):
 base[f.name]=f.read_bytes();rr=[];kk=[]
 for i,s in enumerate(f.read_text().splitlines(),1):
  q=s.split()
  if len(q)<14 or q[0].startswith('#'):kk.append(s);continue
  p=stage.decode(q);point=np.array([p['x'],p['z']]);rad=float(np.linalg.norm(point-CAMP))
  if q[0] in REMOVE and rad<76 and s not in keep_foreground_rows:
   assert assets.get(q[0],dict(collision=0))['collision']==0
   rr.append(dict(line=i,row=s,alias=q[0],x=p['x'],z=p['z']));continue
  if q[0] in ['nm_city_region_oak','nm_city_region_beech'] and rad<77:trees.append(point)
  kk.append(s)
 removals[f.name]=rr;kept[f.name]=kk
placements=[];reject=collections.Counter();accepted_points=[]
def place(a,point,cluster,kind='plant',scale=None,sy=None,yaw=None):
 point=np.asarray(point,dtype=float);g=asset(a);assert assets[a]['collision']==0
 if np.linalg.norm(point-CAMP)>78:return False
 p=dict(x=float(point[0]),z=float(point[1]),yaw_deg=float(rng.uniform(-180,180)if yaw is None else yaw),pitch_deg=0.,roll_deg=0.,scale=1.,sx=float(rng.uniform(.86,1.16)),sy=1.,sz=float(rng.uniform(.85,1.18)))
 if 'grass' in a:p.update(scale=float(rng.uniform(.40,.68)),sy=float(rng.uniform(.82,1.20)))
 elif a=='bep_nature_clover':p.update(scale=float(rng.uniform(.23,.42)),sy=float(rng.uniform(.55,.82)))
 elif a.startswith('fern'):p.update(scale=float(rng.uniform(.70,1.08)),sy=float(rng.uniform(.86,1.12)))
 elif a=='bep_nature_fern':p.update(scale=float(rng.uniform(.40,.58)),sy=.86)
 elif 'bush' in a:p.update(scale=float(rng.uniform(.16,.22)),sy=float(rng.uniform(.42,.62)))
 elif a=='opening_road_stone':p.update(scale=float(rng.uniform(.15,.23)),sy=float(rng.uniform(.38,.58)))
 elif a=='opening_road_roots':p.update(scale=float(rng.uniform(.30,.55)),sy=.38)
 elif a=='camp_fallen_wood':p.update(scale=float(rng.uniform(.50,.80)),sy=.36)
 elif a=='camp_broadleaf_young':p.update(scale=float(rng.uniform(.45,.64)),sy=float(rng.uniform(.85,1.1)))
 if scale is not None:p['scale']=scale
 if sy is not None:p['sy']=sy
 if cluster.startswith('near_camp_verge_'):
  if 'grass' in a:p.update(scale=float(rng.uniform(.42,.60)),sy=float(rng.uniform(1.40,1.80)))
  elif a.startswith('fern'):p.update(scale=float(rng.uniform(.88,1.18)),sy=float(rng.uniform(.95,1.1)))
 stage.native_scale(p)
 if kind!='young_tree':mesh.align_plant(p,ground)
 embed=.035 if kind in ['stone','wood']else .022
 height=mesh.support(p,g,ground,embed)
 v=(g*np.array([p['sx'],p['sy'],p['sz']])*p['scale'])@mesh.rotation(p['yaw_deg'],p['pitch_deg'],p['roll_deg']).T
 bottom=v[v[:,1]<=v[:,1].min()+max(.02,height*.025)]
 if len(bottom)>64:bottom=bottom[np.linspace(0,len(bottom)-1,64,dtype=int)]
 gaps=np.array([p['y']+b[1]-ground(p['x']+b[0],p['z']+b[2])for b in bottom])
 if kind!='young_tree' and (gaps.max()>.065 or gaps.min()<-max(.18,height*.45)):
  reject['irregular_support']+=1;return False
 radius=float(np.linalg.norm(v[:,[0,2]],axis=1).max());route_clear=dist(point,segs)-radius
 margin=3.2 if kind=='young_tree'else 1.0 if kind=='plant'else 2.0
 if route_clear<margin:reject['route']+=1;return False
 cd,wy=wet(point)
 if cd-radius<5.8 or ground(*point)<wy+.12:reject['wet_bank']+=1;return False
 if np.linalg.norm(point-CAMP)<(7 if kind=='young_tree'else 2.1)+radius:reject['fire']+=1;return False
 if np.linalg.norm(point-np.array([-11988.,5180.]))<1.7+radius:reject['landing']+=1;return False
 if np.linalg.norm(actors-point,axis=1).min()<1.35+radius:reject['actors']+=1;return False
 if -11991.6-radius<point[0]<-11984.4+radius and 5163.5-radius<point[1]<5170.7+radius:reject['tent']+=1;return False
 if trees and kind!='young_tree' and min(np.linalg.norm(point-t)for t in trees)<.65:reject['trunk']+=1;return False
 spacing=.52 if kind=='plant' else .8
 # Stones and root fragments can nest beneath grass; their complete geometry
 # still obeys the route, story and wet-bank checks above.
 if kind in ['plant','young_tree'] and accepted_points and min(np.linalg.norm(point-pp)for pp in accepted_points)<spacing:reject['spacing']+=1;return False
 q=[a,f'{p["x"]/W+.5:.12f}',f'{p["z"]/D+.5:.12f}',f'{p["yaw_deg"]:.8f}',f'{p["scale"]:.8f}','0','0','0',f'{p["pitch_deg"]:.8f}',f'{p["roll_deg"]:.8f}',f'{p["sx"]:.10f}',f'{p["sy"]:.10f}',f'{p["sz"]:.10f}',f'{p["y"]:.9f}']
 if kind in ['plant','young_tree']:q+=['wind',f'{rng.uniform(.88,1.1):.5f}',f'{rng.uniform(.9,1.12):.5f}']
 assert len(' '.join(q[:14]))<256 and len(' '.join(q))<320
 placements.append(dict(alias=a,row=' '.join(q),cluster=cluster,kind=kind,x=p['x'],z=p['z'],y=p['y'],height_m=height,embed_m=embed,radius_m=radius,route_clearance_m=route_clear,creek_clearance_m=cd-radius,root_ground_above_water_m=ground(*point)-wy))
 accepted_points.append(point);return True

# An irregular, low edge to the clearing; keep the tent/fire/three talk positions legible.
near=[[-11981.8,5177.2],[-11982.8,5174],[-11983.3,5171.6],[-11993.8,5177.4],[-11994.3,5174.5],[-11994.1,5170.8],[-11994,5161.6],[-11982.2,5161.8]]
bank=[[-12010.5,5164],[-12010.5,5172],[-12010.5,5182],[-12029.5,5190],[-12026,5180],[-12026.5,5163],[-12023,5155],[-12007,5142]]
anchors=near+bank+[[-11997,5184],[-11998,5175],[-11998,5166],[-11994,5158],[-11982,5158],[-11977,5166],[-11975,5175],[-11975,5186],[-11980,5201],[-11991,5196],[-12001,5191],[-11967,5196],[-11957,5203],[-11956,5187],[-11951,5171],[-11958,5157],[-11968,5149],[-11984,5148],[-12002,5145],[-12032,5154],[-12034,5168],[-12035,5182],[-12034,5197],[-12041,5213],[-12010,5211],[-12000,5223],[-11984,5222],[-11975,5234],[-11963,5237],[-11948,5229],[-11939,5209],[-11933,5188]]
# Tree-foot pockets anchor plants to something visible instead of uniform scatter.
for t in sorted(trees,key=lambda t:float(np.linalg.norm(t-CAMP))):
 if np.linalg.norm(t-CAMP)>70:continue
 if all(np.linalg.norm(t-np.asarray(c))>7 for c in anchors):anchors.append(t.tolist())
 if len(anchors)>=60:break
cluster_summary=[]
for i,center in enumerate(anchors):
 center=np.asarray(center);r=1.65 if i<8 else 2.3 if i<16 else 3.1+.4*(i%4);target=16 if i<8 else 20 if i<16 else 24;count=0
 cluster=f'near_camp_verge_{i:02d}'if i<8 else f'bank_verge_{i:02d}'if i<16 else f'rooted_floor_{i:02d}'
 # Broad grass tufts fill in multiple small lobes, never a single huge card.
 lobes=[center+np.array([-.9,.6]),center+np.array([1.1,-.5]),center+np.array([.2,1.3])]
 for attempt in range(target*15):
  if count>=target:break
  lobe=lobes[(attempt//5)%3];angle=attempt*2.399963+float(rng.uniform(-.3,.3));rad=r*math.sqrt(float(rng.uniform(.025,.8)));point=lobe+[math.cos(angle)*rad,math.sin(angle)*rad*.65]
  a=['bep_nature_grass_long','bep_nature_grass_broad','bep_nature_grass_long','bep_nature_clover','fern0','bep_nature_grass_broad','bep_nature_clover','bep_nature_fern'][count%8]
  if i%7==0 and count%11==8:a='fern1'
  if i%9==0 and count%13==9:a='fern2'
  count+=int(place(a,point,cluster))
 if i%3==0 and i>=8:place('bep_nature_bush_a'if i%2 else'bep_nature_bush_b',center+[1.6,-1.4],cluster)
 cluster_summary.append(dict(name=cluster,center_xz=center.tolist(),accepted=count,radius_m=r))
# Dry-bank drifted stones and fine branch debris; no solid/collider changes.
for i,center in enumerate(anchors[:28]):
 if i%2==0:
  for j in range(3):place('opening_road_stone',np.asarray(center)+[rng.uniform(-2,2),rng.uniform(-2,2)],f'bank_stones_{i:02d}','stone')
 if i%5==0:place('opening_road_roots',np.asarray(center)+[-1.5,1.2],f'root_flare_{i:02d}','wood')
for i,point in enumerate([[-12034,5179],[-12033,5154],[-11962,5160],[-11975,5208],[-11955,5218]]):place('camp_fallen_wood',point,f'fallen_branch_{i}','wood',yaw=[25,112,-30,75,148][i])
for i,point in enumerate([[-11964,5163],[-12040,5188],[-11952,5213]]):place('camp_broadleaf_young',point,f'canopy_lower_silhouette_{i}','young_tree')

assert len(placements)>600,(len(placements),reject)
assert ground.loaded_studio_sha==initial_studio_sha,'Concurrent studio edit while sampling'
changed={};removed_count=sum(len(r)for r in removals.values())
for name,lines in kept.items():
 if name=='authored_instances.txt':lines=lines+['# Campsite look20261119: broad rooted clusters replace tiny isolated pin scatter.']+[p['row']for p in placements]
 f=P/'scenery'/name;assert not f.is_symlink()
 out=('\n'.join(lines)+'\n').encode()
 if out!=base[name]:
  (O/(name+'.before')).write_bytes(base[name]);f.write_bytes(out);changed[name]=dict(before_sha256=hashlib.sha256(base[name]).hexdigest(),after_sha256=sha(f),removed_rows=len(removals[name]))
# Keep precise ground surface geometry and every water/road record unchanged.
layer_changes=[];weather_changes=[]
for i,s in enumerate(studio):
 if s.startswith('weather_area "Seldonne woodland rain" '):
  before=s;assert '.22 .38 .78 .90 .55 .72' in s
  studio[i]=s.replace('.22 .38 .78 .90 .55 .72','.22 .38 .42 .60 .38 .54')
  weather_changes.append(dict(before=before,after=studio[i]))
 if not s.startswith('surfaces '):continue
 q=shlex.split(json.loads(s[9:]))
 if q[:2]==['layer','Arrival detailed woodland litter']:
  before=s;q[5:8]=['0.58','0.56','0.47'];studio[i]='surfaces '+json.dumps(q[0]+' '+json.dumps(q[1])+' '+' '.join(q[2:]));layer_changes.append(dict(before=before,after=studio[i]))
assert len(layer_changes)==1
assert len(weather_changes)==1
assert sha(P/'world.studio')==initial_studio_sha
(O/'world.studio.before').write_bytes((P/'world.studio').read_bytes());(P/'world.studio').write_text('\n'.join(studio)+'\n')
new_start=len(kept['authored_instances.txt'])+2
for i,p in enumerate(placements):p.update(file='scenery/authored_instances.txt',line=new_start+i)
for a in REMOVE:asset(a)
removed_tris=sum(assets[r['alias']]['triangles']for rr in removals.values()for r in rr)
added_tris=sum(assets[p['alias']]['triangles']for p in placements)
report=dict(schema='valwood.campsite-composition.v1',private_candidate=True,live_or_published_modified=False,
 source_baseline=str(B),tool_sha256=sha(__file__),world_studio_before_sha256=initial_studio_sha,world_studio_after_sha256=sha(P/'world.studio'),
 changed_files=changed,new_aliases=new_aliases,ground_layer_changes=layer_changes,regional_weather_changes=weather_changes,removed_rows=removals,removed_instances=removed_count,
 added_instances=len(placements),placements=placements,clusters=cluster_summary,assets=assets,skipped_candidates=dict(reject),
 exact_preserved_baseline_foreground_rows=old_foreground,
 removed_triangles_lod0=removed_tris,added_triangles_lod0=added_tris,net_lod0_triangle_change=added_tris-removed_tris,
 minimum_actual_mesh_route_clearance_m=min(p['route_clearance_m']for p in placements),minimum_actual_mesh_wet_curve_clearance_m=min(p['creek_clearance_m']for p in placements),
 surface_sampler_path=str(helper),surface_sampler_sha256=sha(helper),height_sha256=sha(P/'ground/showcase.height'),
 collision_change=False,all_added_collision_modes_zero=True,existing_tree_rows_unchanged=True,water_geometry_unchanged=True,
 limitations=['CPU geometry-grounded composition; native screenshots and hardware frame timing pending.','No terrain height or large landform changes. Existing canopy and authored colliders preserved.','Noncolliding stones and branch debris are low decoration outside the protected routes.','Same existing art files; this does not reproduce Unity custom shaders or certify promo/AAA quality.'])
(O/'COMPOSITION_PATCH.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({k:report[k]for k in ['removed_instances','added_instances','removed_triangles_lod0','added_triangles_lod0','net_lod0_triangle_change','minimum_actual_mesh_route_clearance_m','minimum_actual_mesh_wet_curve_clearance_m']},indent=2))
