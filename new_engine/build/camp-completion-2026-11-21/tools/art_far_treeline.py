"""Count-neutral distant tree variety, real baked cards and source-ground plane."""
from pathlib import Path
import importlib.util,json,hashlib,math,copy,numpy as np
N=Path(__file__).resolve().parents[1];E=N.parents[1];P=N/'world-project';R=N/'review/art';W=100800.;D=W*2818/5009

def mod(n,p):
 s=importlib.util.spec_from_file_location(n,p);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m
st=mod('art_st',E/'build/woodland-detail-2026-11-19/tools/build_forest_layers.py');st.N=N;st.P=P;ground,helper=st.sampler();geo=mod('art_geo',E/'build/woodland-detail-2026-11-19/tools/review_forest_layout.py')
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest();report=json.loads((R/'ART_PATCH.json').read_text());assert 'far_treeline_replacements'not in report
variants=json.loads((E/'build/camp-reference-2026-11-19/review/tree-variants/TREE_VARIANT_REVIEW.json').read_text())['variants'];camp=np.array([-11988.,5172.]);rng=np.random.default_rng(202611211)
ss={f.name:f.read_text().splitlines()for f in(P/'scenery').glob('*.txt')};source=copy.deepcopy(ss);candidates=[]
paths=json.loads((P/'opening/first-journey.routes.json').read_text())['paths'];seg=[]
for pts in paths.values():
 q=np.asarray(pts)[:,[0,2]];seg.extend(zip(q[:-1],q[1:]))
seg=np.asarray(seg)
for f,lines in ss.items():
 for i,line in enumerate(lines):
  q=line.split()
  if len(q)<14 or q[0]not in ['nm_city_region_oak','nm_city_region_beech']:continue
  p=st.decode(q);pt=np.array([p['x'],p['z']]);dist=np.linalg.norm(pt-camp)
  if 265<dist<900 and geo.dist(pt,seg)>10:candidates.append((f,i,line,p,dist))
chosen=set();changed=[];anchors=[]
for ri,ring in enumerate([295,350,415,495,590,700,815]):
 for j,a in enumerate(np.linspace(0,math.tau,28,endpoint=False)+ri*.13):anchors.append(camp+[math.cos(a)*ring,math.sin(a)*ring])
for j,anchor in enumerate(anchors):
 v=variants[(j//3+j%3)%4];soil=v['soil_plane_glb_y'];cxz=np.array(v['collision']['soil_cross_sections'][0]['center_xz']);radius=max(s['maximum_pivot_radius']for s in v['collision']['soil_cross_sections'])
 for f,i,line,old,dist in sorted(candidates,key=lambda c:np.linalg.norm(np.array([c[3]['x'],c[3]['z']])-anchor))[:35]:
  if(f,i)in chosen:continue
  h=float(rng.uniform(15.5,23.5));scale=h/v['visible_height_above_source_soil'];pt=np.array([old['x'],old['z']]);yaw=math.radians(old['yaw_deg']);rot=np.array([[math.cos(yaw),math.sin(yaw)],[-math.sin(yaw),math.cos(yaw)]]);center=pt+rot@cxz*scale
  ys=[ground(*(center+o))for o in [[0,0],[.6,0],[-.6,0],[0,.6],[0,-.6]]]
  if max(ys)-min(ys)>.45 or geo.dist(center,seg)-radius*scale<7:continue
  p=dict(x=old['x'],z=old['z'],scale=scale,sx=1.,sy=1.,sz=1.,yaw_deg=old['yaw_deg'],pitch_deg=0.,roll_deg=0.,y=ys[0]-soil*scale);q=[v['alias'],f'{p["x"]/W+.5:.13f}',f'{p["z"]/D+.5:.13f}',f'{p["yaw_deg"]:.7f}',f'{scale:.9f}','0','0','0','0','0','1','1','1',f'{p["y"]:.9f}','wind','1','1'];after=' '.join(q);assert len(' '.join(q[:14]))<256;ss[f][i]=after;chosen.add((f,i));changed.append(dict(file='scenery/'+f,line=i+1,before=line,after=after,kind='far_treeline_source_card',alias=v['alias'],distance_from_camp_m=dist,height_above_soil_m=h,source_soil_plane_y=soil,source_soil_world_xz=center.tolist(),source_soil_error_m=p['y']+soil*scale-ground(*center),route_clearance_m=geo.dist(center,seg)-radius*scale,far_triangles=2,original_mid_triangles=4057 if old and line.startswith('nm_city_region_oak ')else 4133));break
for name,lines in ss.items():
 if lines!=source[name]:
  if name not in report['changed_scenery']:
   (R/('before-'+name)).write_text('\n'.join(source[name])+'\n');report['changed_scenery'][name]={'before_sha256':hashlib.sha256(('\n'.join(source[name])+'\n').encode()).hexdigest()}
  (P/'scenery'/name).write_text('\n'.join(lines)+'\n');report['changed_scenery'][name]['after_sha256']=sha(P/'scenery'/name)
report['far_treeline_replacements']=changed;report['far_treeline_count_change']=0;report['far_card_triangle_comparison']={'replaced_instances':len(changed),'old_mid_triangles_sum':sum(c['original_mid_triangles']for c in changed),'new_far_triangles_sum':2*len(changed),'note':'Geometry count comparison when source old MID and new far LOD are both visible; not an FPS measurement. Extra horizon range is new visibility, not a measured performance saving.'};(R/'ART_PATCH.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report['far_card_triangle_comparison']))
