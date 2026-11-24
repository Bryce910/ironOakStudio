#!/usr/bin/env python3
"""Owned scene art pass; exact old rows, actual terrain support and bounded paths."""
from pathlib import Path
import collections,ctypes,hashlib,importlib.util,json,math,shlex,shutil
import numpy as np
M=Path(__file__).resolve().parents[1];E=M.parents[1];N=M.with_name('woodland-detail-2026-11-19');P=M/'world-project';O=M/'review/composition'
assert not(M/'PUBLICATION.json').exists()
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def mod(name,p):
 s=importlib.util.spec_from_file_location(name,p);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m
st=mod('camp_author_support',N/'tools/build_forest_layers.py');st.N=M;st.P=P
geo=mod('camp_author_geometry',N/'tools/review_forest_layout.py');mesh=mod('camp_author_mesh',E/'docs/seldonne-first-region/2026-11-07/region_mesh_grounding_v5.py')
ground,helper=st.sampler();rng=np.random.default_rng(202611192330);W=100800.;D=W*2818/5009;camp=np.array([-11988.,5172.])
O.mkdir(exist_ok=True);assert not(O/'PATCH.json').exists()
studio=(P/'world.studio').read_text().splitlines();before_studio='\n'.join(studio)+'\n'
source={p.name:p.read_text().splitlines() for p in sorted((P/'scenery').glob('*.txt'))}
assert all('\n'.join(rows)+'\n'==(N/'world-project/scenery'/name).read_text() for name,rows in source.items())
archive=O/'before';archive.mkdir(exist_ok=True);shutil.copy2(P/'world.studio',archive/'world.studio');(archive/'scenery').mkdir(exist_ok=True)
for name in source:shutil.copy2(P/'scenery'/name,archive/'scenery'/name)
tree=json.loads((M/'review/tree-variants/TREE_VARIANT_REVIEW.json').read_text());assert tree['passed']
aliases={q[1]:q for s in studio if(q:=shlex.split(s)) and q[0]=='asset'}
new_defs=[v['recommended_asset_definition'] for v in tree['variants']]+[
 'asset camp_moss_boulder_0 mossy_boulder_0 1 1 0 170 30 1',
 'asset camp_moss_boulder_1 mossy_boulder_1 1 1 0 170 30 1',
 'asset camp_moss_boulder_2 mossy_boulder_2 1 1 0 170 30 1',
 'asset camp_exposed_roots showcase_roots 1 0 0 90 22 1',
 'asset camp_sorrel showcase_sorrel 1 0 1 90 25 1']
for s in new_defs:
 q=shlex.split(s);assert q[1]not in aliases;studio.append(s);aliases[q[1]]=q
built={'fern0':('woodland_fern_0',0),'fern1':('woodland_fern_1',0),'fern2':('woodland_fern_2',0),'rock0':('cinematic_rock_0',1),'rock1':('cinematic_rock_1',1),'rock2':('cinematic_rock_2',1)}
ggs={};assets={}
def asset(a):
 if a in ggs:return ggs[a]
 if a in built:stem,col=built[a]
 else:q=aliases[a];stem=q[2]+('_near'if q[3]=='2'else'');col=int(q[4])
 path=P/'models'/(stem+'.glb');g,t,n=geo.geometry(path);ggs[a]=g;assets[a]={'path':str(path),'sha256':sha(path),'triangles':t,'normalization':n,'collision':col};return g
routes=json.loads((P/'opening/first-journey.routes.json').read_text())['paths'];segments=[]
for points in routes.values():
 a=np.array(points)[:,[0,2]];segments.extend(zip(a[:-1],a[1:]))
a=np.array([[float(q[1]),float(q[3])] for s in(P/'opening/road-traffic.settings').read_text().splitlines() if(q:=s.split())and q[0]=='point']);segments.extend(zip(a[:-1],a[1:]));segments=np.array(segments)
actors=np.array([[-11990.15,5171.35],[-11985.8,5171.35],[-11987.9,5168.8]])
# Sample the same Catmull-Rom centreline used by surface authoring.
q=next(shlex.split(json.loads(s[9:])) for s in studio if s.startswith('surfaces ')and json.loads(s[9:]).startswith('ribbon "West woodland creek"'));controls=np.array(list(map(float,q[9:]))).reshape(-1,3);curve=[controls[0]]
for i in range(len(controls)-1):
 a,b,c,d=controls[max(0,i-1)],controls[i],controls[i+1],controls[min(i+2,len(controls)-1)];steps=max(1,math.ceil(np.linalg.norm((c-b)[[0,2]])/.75))
 for k in range(1,steps+1):
  t=k/steps;v=.5*(2*b+(-a+c)*t+(2*a-5*b+4*c-d)*t*t+(-a+3*b-3*c+d)*t*t*t)
  if np.linalg.norm((v-curve[-1])[[0,2]])>.01:curve.append(v)
curve=np.array(curve)
def wet(pt):
 a=curve[:-1];v=curve[1:]-a;vv=v[:,[0,2]];t=np.clip(np.sum((pt-a[:,[0,2]])*vv,axis=1)/np.maximum(np.sum(vv*vv,axis=1),1e-12),0,1);q=a+v*t[:,None];dd=np.linalg.norm(pt-q[:,[0,2]],axis=1);i=int(dd.argmin());return float(dd[i]),float(q[i,1])
def encode(a,p,wind=False):
 st.native_scale(p);
 if p["scale"]>4:
  factor=p["scale"]/2;p["scale"]=2
  for key in ("sx","sy","sz"):p[key]*=factor
 q=[a,f'{p["x"]/W+.5:.13f}',f'{p["z"]/D+.5:.13f}',f'{p["yaw_deg"]:.8f}',f'{p["scale"]:.10f}','0','0','0',f'{p["pitch_deg"]:.8f}',f'{p["roll_deg"]:.8f}',f'{p["sx"]:.10f}',f'{p["sy"]:.10f}',f'{p["sz"]:.10f}',f'{p["y"]:.9f}']
 if wind:q+=['wind','1','1']
 line=' '.join(q);assert len(' '.join(q[:14]))<=256 and len(line)<=320;return line
changes=[];selected=set();new_tree_stats=[];variants=tree['variants']
# Replace existing oaks across the near/middle woodland, preserving root XZ and tree count.
candidates=[]
for name,rows in source.items():
 for index,s in enumerate(rows):
  q=s.split()
  if len(q)<14 or q[0]!='nm_city_region_oak':continue
  p=st.decode(q);pt=np.array([p['x'],p['z']]);r=np.linalg.norm(pt-camp)
  if not 17<r<100:continue
  if np.linalg.norm(pt-actors,axis=1).min()<11:continue
  if wet(pt)[0]<13:continue
  candidates.append((name,index,s,p))
anchors=[]
for ring in [28,48,72]:
 for angle in np.linspace(-math.pi,math.pi,8,endpoint=False):anchors.append(camp+[math.cos(angle)*ring,math.sin(angle)*ring])
for j,anchor in enumerate(anchors):
 v=variants[j%4];g=asset(v['alias']);soil=v['soil_plane_glb_y'];chosen=None
 for name,index,before,old in sorted(candidates,key=lambda c:np.linalg.norm(np.array([c[3]['x'],c[3]['z']])-anchor)):
  if (name,index)in selected:continue
  pt=np.array([old['x'],old['z']]);oldg=asset('nm_city_region_oak');oldheight=old['y']+float(oldg[:,1].max())*old['scale']*old['sy']-ground(*pt);height=float(np.clip(oldheight,15,23));scale=height/v['visible_height_above_source_soil'];yaw=old['yaw_deg']
  # Evaluate actual source-soil bark centre in the same rotated/scaled basis.
  cxz=np.array(v['collision']['soil_cross_sections'][0]['center_xz']);angle=math.radians(yaw);rot=np.array([[math.cos(angle),math.sin(angle)],[-math.sin(angle),math.cos(angle)]]);centre=pt+rot@cxz*scale
  trunk_radius=max(s['maximum_pivot_radius'] for s in v['collision']['soil_cross_sections'])*scale
  clear=geo.dist(pt,segments)-trunk_radius
  if clear<3.2:continue
  ys=[ground(*(centre+o))for o in [[0,0],[.7,0],[-.7,0],[0,.7],[0,-.7]]]
  if max(ys)-min(ys)>.33:continue
  p=dict(x=old['x'],z=old['z'],scale=scale,sx=1.,sy=1.,sz=1.,yaw_deg=yaw,pitch_deg=0.,roll_deg=0.,y=ground(*centre)-soil*scale)
  after=encode(v['alias'],p,True);source[name][index]=after;selected.add((name,index));changes.append(dict(file='scenery/'+name,line=index+1,before=before,after=after,kind='replace_oak_silhouette',pose=p,source_soil_m=soil,source_soil_world_xz=centre.tolist(),source_soil_error_m=p['y']+soil*scale-ground(*centre),trunk_bound_radius_m=trunk_radius,route_clearance_m=clear,height_above_soil_m=height,old_height_above_ground_m=oldheight))
  new_tree_stats.append({'variant':v['alias'],'height_m':height,'near_triangles':assets[v['alias']]['triangles']});chosen=True;break
 assert chosen,('No safe tree candidate',j,v['alias'])
assert len(selected)==24
# Replace half the dry upper-bank blocks with moss-covered real meshes at the same anchors.
rock_candidates=[]
for name,rows in source.items():
 for i,s in enumerate(rows):
  q=s.split()
  if len(q)<14 or q[0]not in ['rock0','rock1','rock2']:continue
  p=st.decode(q);pt=np.array([p['x'],p['z']]);dd,water=wet(pt)
  if 5.3<dd<15 and 5138<p['z']<5198:rock_candidates.append((name,i,s,p))
for j,(name,i,before,old) in enumerate(rock_candidates[::2][:12]):
 a='camp_moss_boulder_'+str(j%3);g=asset(a);oldg=asset(before.split()[0]);oldheight=float(np.ptp(oldg[:,1])*old['scale']*old['sy']);target=float(np.clip(oldheight*.90,.55,1.4));p=dict(x=old['x'],z=old['z'],scale=target/np.ptp(g[:,1]),sx=1.,sy=1.,sz=1.,yaw_deg=old['yaw_deg'],pitch_deg=0.,roll_deg=0.)
 mesh.align_plant(p,ground);mesh.support(p,g,ground,.075);trans=(g*np.array([p['sx'],p['sy'],p['sz']])*p['scale'])@mesh.rotation(p['yaw_deg'],p['pitch_deg'],p['roll_deg']).T;radius=float(np.linalg.norm(trans[:,[0,2]],axis=1).max());pt=np.array([p['x'],p['z']]);clear=geo.dist(pt,segments)-radius;dd,water=wet(pt)
 if clear<2.2 or dd-radius<5.05:continue
 after=encode(a,p);source[name][i]=after;changes.append(dict(file='scenery/'+name,line=i+1,before=before,after=after,kind='moss_rock_replace',pose=p,height_m=target,route_clearance_m=clear,creek_clearance_m=dd-radius))
# Recompose an obvious repeated foreground fringe, retaining grounded plant mass.
plant_candidates=[]
for name,rows in source.items():
 for i,s in enumerate(rows):
  q=s.split()
  if len(q)<14 or q[0]not in ['bep_nature_grass_broad','bep_nature_fern','bep_nature_grass_long']:continue
  p=st.decode(q)
  if -11982<p['x']<-11972 and 5172<p['z']<5194:plant_candidates.append((name,i,s,p))
for j,(name,i,before,old) in enumerate(plant_candidates[::2][:40]):
 a=['fern0','camp_sorrel','fern2','bep_nature_clover'][j%4];g=asset(a);target=[.46,.27,.4,.22][j%4]*float(rng.uniform(.9,1.10));p=dict(x=old['x'],z=old['z'],scale=target/np.ptp(g[:,1]),sx=1.,sy=1.,sz=1.,yaw_deg=float(rng.uniform(-180,180)),pitch_deg=0.,roll_deg=0.)
 mesh.align_plant(p,ground);mesh.support(p,g,ground,.02);after=encode(a,p,True);source[name][i]=after;changes.append(dict(file='scenery/'+name,line=i+1,before=before,after=after,kind='fringe_variety',pose=p,height_m=target))
# Low exposed root/stone fragments break the clear floor at its margins, without blockers.
placements=[];rejected=collections.Counter()
def place_low(a,pt,height,group,yaw=None):
 g=asset(a);p=dict(x=float(pt[0]),z=float(pt[1]),scale=float(height/np.ptp(g[:,1])),sx=1.,sy=1.,sz=1.,yaw_deg=float(rng.uniform(-180,180)if yaw is None else yaw),pitch_deg=0.,roll_deg=0.)
 if a=='camp_exposed_roots':p.update(scale=.30,sx=.9,sy=float(height/(np.ptp(g[:,1])*.30)),sz=.8)
 mesh.align_plant(p,ground);mesh.support(p,g,ground,.025);trans=(g*np.array([p['sx'],p['sy'],p['sz']])*p['scale'])@mesh.rotation(p['yaw_deg'],p['pitch_deg'],p['roll_deg']).T;radius=float(np.linalg.norm(trans[:,[0,2]],axis=1).max());pt=np.array(pt);dd,water=wet(pt)
 if geo.dist(pt,segments)-radius<.65 or dd-radius<5.1 or np.linalg.norm(pt-actors,axis=1).min()<1.6+radius or np.linalg.norm(pt-camp)<2.6+radius:rejected['routes']+=1;return
 if -11992-radius<pt[0]<-11983+radius and 5163-radius<pt[1]<5171+radius:rejected['tent']+=1;return
 bottom=trans[trans[:,1]<=trans[:,1].min()+max(.02,height*.05)]
 if len(bottom)>64:bottom=bottom[np.linspace(0,len(bottom)-1,64,dtype=int)]
 gaps=np.array([p['y']+b[1]-ground(p['x']+b[0],p['z']+b[2])for b in bottom])
 if gaps.max()>.08 or gaps.min()<-.3:rejected['support']+=1;return
 placements.append(dict(alias=a,row=encode(a,p),kind=group,pose=p,actual_height_m=float(np.ptp(trans[:,1])),maximum_basal_gap_m=float(gaps.max()),minimum_basal_gap_m=float(gaps.min()),route_clearance_m=geo.dist(pt,segments)-radius))
for i,anchor in enumerate([[-11996,5180],[-11978,5174],[-11978,5166],[-12007,5170],[-12030,5160],[-12028,5180],[-11993,5183],[-11979,5188],[-12002,5163],[-11977,5159]]):
 if i%2==0:place_low('camp_exposed_roots',anchor,.16,'exposed_root_fragment')
 for j in range(5):place_low('opening_road_stone',np.array(anchor)+rng.uniform(-1.4,1.4,2),float(rng.uniform(.07,.15)),'low_stone_scatter')
# Existing local paint slots form irregular litter margins and worn smaller lobes.
paint_changes=[]
for i,s in enumerate(studio):
 if not s.startswith('surfaces '):continue
 q=shlex.split(json.loads(s[9:]))
 if q and q[0]=='paint':
  x,z,r,amount=float(q[1]),float(q[2]),float(q[3]),float(q[4]);layer=int(q[5]);after=None
  if abs(x+11988)<.01 and abs(z-5170)<.01 and r>10:after='paint -11988 5170 12.0 0.48 7 0'
  elif abs(x+11988)<.01 and abs(z-5172)<.01 and layer==5:after='paint -11988 5172 3.60 0.82 5 0'
  elif abs(x+11988)<.01 and abs(z-5177)<.01 and layer==5:after='paint -11988.8 5176.6 2.40 0.72 5 0'
  elif abs(x+11986)<.01 and abs(z-5181)<.01 and layer==5:after='paint -11985.3 5181.2 2.45 0.64 5 0'
  elif abs(x+11984)<.01 and abs(z-5185)<.01 and layer==5:after='paint -11983.6 5185.4 2.45 0.63 5 0'
  elif abs(x+11991)<.01 and abs(z-5170)<.01 and layer==5:after='paint -11991 5170 2.40 0.67 5 0'
  elif abs(x+11977.7899673)<.01 and abs(z-5215.83780717)<.01 and r>30:after='paint -11977.7899673 5215.83780717 38 0.34 7 0'
  elif math.hypot(x+11988,z-5172)<55 and layer in [1,7] and r<8:
   # Stronger substrate under planting; vary masks rather than one brown disk.
   after=f'paint {x:.7f} {z:.7f} {r*.90:.3f} {min(.90,amount+.06):.3f} {layer} 0'
  if after:
   new='surfaces '+json.dumps(after);studio[i]=new;paint_changes.append(dict(line=i+1,before=s,after=new))
# Install the genuine pebble pair within existing layer/image budgets.
proposed=(M/'review/lighting/PROPOSED_GRAVEL_ROWS.txt').read_text().splitlines();layerline=next(i for i,s in enumerate(studio) if s.startswith('surfaces ')and json.loads(s[9:]).startswith('layer "Shore gravel"'))
paint_changes.append(dict(line=layerline+1,before=studio[layerline],after=proposed[2]));studio[layerline]=proposed[2];studio+=proposed[:2]
# Do not alter authored water or terrain/quest/path records.
oldwaters=[s for s in before_studio.splitlines() if s.startswith('surfaces ')and json.loads(s[9:]).startswith('ribbon ')];assert [s for s in studio if s.startswith('surfaces ')and json.loads(s[9:]).startswith('ribbon ')]==oldwaters
start=len(source['authored_instances.txt'])+2;source['authored_instances.txt']+=['# Camp reference art: source-rooted ground fragments.']+[p['row']for p in placements]
for i,p in enumerate(placements):p['file']='scenery/authored_instances.txt';p['line']=start+i
for name,rows in source.items():(P/'scenery'/name).write_text('\n'.join(rows)+'\n')
(P/'world.studio').write_text('\n'.join(studio)+'\n')
# Exact append-only source collider merge; published/LIVE catalog stays untouched.
assert sha(P/'collision/structures/catalog')==tree['before_sha256'];shutil.copy2(M/'review/tree-variants/catalog.after',P/'collision/structures/catalog')
counts=collections.Counter(c['kind']for c in changes)
report=dict(schema='valwood.camp-composition.patch.v1',passed=True,tool_sha256=sha(__file__),baseline=str(N),tree_count_change=0,tree_replacements=24,tree_variant_counts=dict(collections.Counter(x['variant']for x in new_tree_stats)),changed_rows=changes,added_rows=placements,paint_changes=paint_changes,new_asset_definitions=new_defs,rejections=dict(rejected),unchanged_water_ribbons=True,unchanged_terrain_height_files=True,source_height_sampler_sha256=sha(helper),asset_inputs=assets,world_before_sha256=sha(archive/'world.studio'),world_after_sha256=sha(P/'world.studio'),scenery_changes={name:dict(before_sha256=sha(N/'world-project/scenery'/name),after_sha256=sha(P/'scenery'/name))for name in source if sha(N/'world-project/scenery'/name)!=sha(P/'scenery'/name)},source_triangles_before_24_oaks=24*assets['nm_city_region_oak']['triangles'],source_triangles_after_tree_variants=sum(x['near_triangles']for x in new_tree_stats),native_visual_review_pending=True)
(O/'PATCH.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({'changed':dict(counts),'low_ground_additions':len(placements),'paint_changes':len(paint_changes),'tree_triangles_before':report['source_triangles_before_24_oaks'],'tree_triangles_after':report['source_triangles_after_tree_variants']}))
