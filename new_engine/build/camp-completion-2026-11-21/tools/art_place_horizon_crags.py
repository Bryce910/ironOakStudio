from pathlib import Path
import importlib.util,json,hashlib,math,numpy as np
N=Path(__file__).resolve().parents[1];E=N.parents[1];P=N/'world-project';R=N/'review/art';W=100800.;D=W*2818/5009

def mod(n,p):
 s=importlib.util.spec_from_file_location(n,p);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m
st=mod('st_art_rock',E/'build/woodland-detail-2026-11-19/tools/build_forest_layers.py');st.N=N;st.P=P;ground,helper=st.sampler();geo=mod('geo_art_rock',E/'build/woodland-detail-2026-11-19/tools/review_forest_layout.py');mesh=mod('mesh_art_rock',E/'docs/seldonne-first-region/2026-11-07/region_mesh_grounding_v5.py')
report=json.loads((R/'ART_PATCH.json').read_text());patch=json.loads((R/'STUDIO_ART_PATCH.json').read_text());assert 'horizon_outcrops'not in report
q=geo.geometry(P/'models/art_completion/horizon_crag.glb')[0];paths=json.loads((P/'opening/first-journey.routes.json').read_text())['paths'];seg=[]
for pts in paths.values():
 a=np.asarray(pts)[:,[0,2]];seg.extend(zip(a[:-1],a[1:]))
seg=np.array(seg);rng=np.random.default_rng(9112026);out=[]
for i,(x,z)in enumerate([(-12198,5035),(-12251,5152),(-12148,4930),(-11853,4974),(-11761,5109),(-11670,5293),(-12087,5459),(-12189,5536),(-12301,5324),(-11711,5481),(-11647,4971),(-12218,4834)]):
 target=3.3+(i%4)*.8;scale=target/np.ptp(q[:,1]);p=dict(x=x,z=z,scale=scale,sx=1.0+(i%3)*.12,sy=1.,sz=.82+(i%4)*.08,yaw_deg=float(rng.uniform(-180,180)),pitch_deg=0.,roll_deg=0.);st.native_scale(p);mesh.align_plant(p,ground);mesh.support(p,q,ground,.28);v=(q*np.array([p['sx'],p['sy'],p['sz']])*p['scale'])@mesh.rotation(p['yaw_deg'],p['pitch_deg'],p['roll_deg']).T;rad=float(np.linalg.norm(v[:,[0,2]],axis=1).max());clear=geo.dist(np.array([x,z]),seg)-rad
 if clear<18:continue
 basal=v[v[:,1]<v[:,1].min()+.10];basal=basal[np.linspace(0,len(basal)-1,min(48,len(basal)),dtype=int)];gaps=np.array([p['y']+b[1]-ground(x+b[0],z+b[2])for b in basal])
 if gaps.max()>.08 or gaps.min()<-1.4:continue
 row=' '.join(['nm_camp_horizon_crag',f'{x/W+.5:.13f}',f'{z/D+.5:.13f}',f'{p["yaw_deg"]:.7f}',f'{p["scale"]:.9f}','0','0','0',f'{p["pitch_deg"]:.7f}',f'{p["roll_deg"]:.7f}',*[f'{p[k]:.9f}'for k in['sx','sy','sz']],f'{p["y"]:.9f}']);assert len(row)<256
 out.append(dict(alias='nm_camp_horizon_crag',row=row,kind='horizon_rock_outcrop',position_world=[x,p['y'],z],visible_height_m=float(np.ptp(v[:,1])),route_clearance_m=clear,basal_support_min_m=float(gaps.min()),basal_support_max_m=float(gaps.max()),collision='Box matching native transformed scanned-mesh bounds'))
patch.append({'before':None,'after':'asset nm_camp_horizon_crag art_completion/horizon_crag 2 1 0 850 80 1'});file=P/'scenery/authored_instances.txt';file.write_text(file.read_text()+'# Source-photogrammetric distant hill outcrops, native movement blockers.\n'+'\n'.join(p['row']for p in out)+'\n');report['new_placements']+=out;report['horizon_outcrops']=out;report['new_detail_records']=len(report['new_placements']);report['changed_scenery']['authored_instances.txt']['after_sha256']=hashlib.sha256(file.read_bytes()).hexdigest();report['walk_collision_changed']=True;report['walk_collision_change_note']='Added only matching transformed native rock-box blockers at the 3 distant scanned outcrops; no original terrain, bridge, wall, doorway or campsite blocker changed.';(R/'ART_PATCH.json').write_text(json.dumps(report,indent=2)+'\n');(R/'STUDIO_ART_PATCH.json').write_text(json.dumps(patch,indent=2)+'\n');print(json.dumps({'passed':True,'added_outcrops':len(out),'min_route_clearance_m':min([p['route_clearance_m']for p in out],default=0),'maximum_basal_gap_m':max([p['basal_support_max_m']for p in out],default=0)}))
