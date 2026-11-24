from pathlib import Path
import json,importlib.util,numpy as np,hashlib
N=Path(__file__).resolve().parents[1];E=N.parents[1];P=N/'world-project';O=N/'review/camp-composition'
def mod(name,path):
 s=importlib.util.spec_from_file_location(name,path);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m
stage=mod('dry_sampler',N/'tools/build_forest_layers.py');geo=mod('dry_geometry',N/'tools/review_forest_layout.py');mesh=mod('dry_support',E/'docs/seldonne-first-region/2026-11-07/region_mesh_grounding_v5.py');ground,_=stage.sampler();g,_,_=geo.geometry(P/'models/cinematic_rock_0.glb');R=O/'V3_PATCH.json';r=json.loads(R.read_text());prop=json.loads((O/'SHORELINE_PROPOSAL.json').read_text());assert not r.get('shore_dry_revision');lines=(P/'scenery/authored_instances.txt').read_text().splitlines();oldlines=lines.copy();changes=[]
# Actual production curve samples are the same 2m Catmull-Rom bake used by the authoring sampler.
import shlex,math
q=next(shlex.split(json.loads(s[9:]))for s in(P/'world.studio').read_text().splitlines()if s.startswith('surfaces ')and json.loads(s[9:]).startswith('ribbon "West woodland creek"'));cc=np.array(list(map(float,q[9:]))).reshape(-1,3);curve=[cc[0]]
for i in range(len(cc)-1):
 a,b,c,d=cc[max(0,i-1)],cc[i],cc[i+1],cc[min(i+2,len(cc)-1)];n=max(1,math.ceil(np.linalg.norm((c-b)[[0,2]])/2))
 for k in range(1,n+1):
  t=k/n;v=.5*(2*b+(-a+c)*t+(2*a-5*b+4*c-d)*t*t+(-a+3*b-3*c+d)*t*t*t)
  if np.linalg.norm((v-curve[-1])[[0,2]])>.01:curve.append(v)
curve=np.array(curve)
def wet(point):
 a=curve[:-1];v=curve[1:]-a;vv=v[:,[0,2]];t=np.clip(np.sum((point-a[:,[0,2]])*vv,axis=1)/np.sum(vv*vv,axis=1),0,1);q=a+v*t[:,None];dd=np.linalg.norm(point-q[:,[0,2]],axis=1);i=int(dd.argmin());return float(dd[i]),float(q[i,1])
for item in r['added_shore_details']:
 if item['kind']!='shore_stone':continue
 before=item['row'];assert lines[item['line']-1]==before;a=prop['anchors'][int(item['label'].split('_')[2])];normal=np.array(a['outward_xz']);base=item['pose'];picked=None
 for shift in np.arange(.0,2.01,.04):
  p=base.copy();p.update(x=float(base['x']+normal[0]*shift),z=float(base['z']+normal[1]*shift));mesh.align_plant(p,ground);h=mesh.support(p,g,ground,.018)
  if h>.235:
   p['scale']*=.2349/h;h=mesh.support(p,g,ground,.018)
  v=(g*np.array([p['sx'],p['sy'],p['sz']])*p['scale'])@mesh.rotation(p['yaw_deg'],p['pitch_deg'],p['roll_deg']).T;bottom=v[v[:,1]<=v[:,1].min()+max(.02,h*.025)]
  if len(bottom)>64:bottom=bottom[np.linspace(0,len(bottom)-1,64,dtype=int)]
  gaps=np.array([p['y']+b[1]-ground(p['x']+b[0],p['z']+b[2])for b in bottom]);distance,water=wet(np.array([p['x'],p['z']]));rad=float(np.linalg.norm(v[:,[0,2]],axis=1).max())
  if distance<5.25 or p['y']-water<.18 or gaps.max()>.045 or gaps.min()<-.11:continue
  picked=(p,h,v,gaps,distance,water,rad,float(shift));break
 assert picked is not None,item['label']
 p,h,v,gaps,distance,water,rad,shift=picked;q=before.split();q[1]=f'{p["x"]/100800+.5:.12f}';q[2]=f'{p["z"]/(100800*2818/5009)+.5:.12f}';q[4]=f'{p["scale"]:.9f}';q[8]=f'{p["pitch_deg"]:.8f}';q[9]=f'{p["roll_deg"]:.8f}';q[13]=f'{p["y"]:.9f}';after=' '.join(q);lines[item['line']-1]=after;item['row']=after;item['pose']=p;item['support']=dict(height_m=float(h),embed_m=.018,minimum_basal_clearance_m=float(gaps.min()),maximum_basal_clearance_m=float(gaps.max()));item['full_mesh_radius_m']=rad;item['creek_clearance_m']=distance-rad;item['nearest_water_y']=water;item['actual_mesh_top_above_water_m']=float(v[:,1].max()+p['y']-water);item['actual_mesh_bottom_below_water_m']=float(water-v[:,1].min()-p['y']);item['dry_root_above_nearest_water_m']=p['y']-water;item['center_distance_to_water_curve_m']=distance
 changes.append(dict(line=item['line'],before_row=before,after_row=after,outward_shift_m=shift,pivot_above_water_m=p['y']-water,center_curve_distance_m=distance))
assert len(changes)==23
(O/'shore-wet-rejected-before.txt').write_text('\n'.join(oldlines)+'\n');(P/'scenery/authored_instances.txt').write_text('\n'.join(lines)+'\n');r['changed_scenery_files']['authored_instances.txt']['after_sha256']=hashlib.sha256((P/'scenery/authored_instances.txt').read_bytes()).hexdigest();r['shore_dry_revision']=dict(reason='Native production parser rejects roots within water unless pivot exceeds water by0.1m. Move stones to dry supported bank; never lift a mesh away from terrain.',minimum_center_curve_distance_m=min(c['center_curve_distance_m']for c in changes),minimum_pivot_above_water_m=min(c['pivot_above_water_m']for c in changes),changes=changes,tool_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest());R.write_text(json.dumps(r,indent=2)+'\n');print(json.dumps({k:v for k,v in r['shore_dry_revision'].items()if k not in ['changes']},indent=2))
