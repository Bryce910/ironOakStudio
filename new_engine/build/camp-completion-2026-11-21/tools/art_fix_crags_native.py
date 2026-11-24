"""Use CURRENT linked production water sampler; remove wet crags, preserve dry geometry."""
from pathlib import Path
import json,struct,hashlib,subprocess,importlib.util,math,numpy as np
N=Path(__file__).resolve().parents[1];E=N.parents[1];P=N/'world-project';R=N/'review/art';report=json.loads((R/'ART_PATCH.json').read_text());file=P/'scenery/authored_instances.txt';before=file.read_text();lines=before.splitlines();crags=[]
s=importlib.util.spec_from_file_location('crag_exact',E/'build/woodland-detail-2026-11-19/tools/review_forest_layout.py');geo=importlib.util.module_from_spec(s);s.loader.exec_module(geo);g=geo.geometry(P/'models/art_completion/horizon_crag.glb')[0];pts=[]
for i,line in enumerate(lines):
 q=line.split()
 if not q or q[0]!='nm_camp_horizon_crag':continue
 nums=list(map(float,q[1:14]));x=(nums[0]-.5)*100800;z=(nums[1]-.5)*100800*2818/5009;scl=nums[3];axes=np.array(nums[9:12]);v=g*axes*scl@geo.rot(nums[2],nums[7],nums[8]).T;basal=v[v[:,1]<v[:,1].min()+.12];basal=basal[np.linspace(0,len(basal)-1,min(48,len(basal)),dtype=int)];points=np.vstack(([x,z],np.array([x,z])+basal[:,[0,2]]));start=len(pts);pts.extend(points);crags.append({'index':i,'before':line,'q':q,'point_start':start,'point_count':len(points),'root':nums[12],'x':x,'z':z})
assert len(crags)==12
np.asarray(pts,'<f8').tofile(R/'crag-current-water.points.bin');helper=N/'tools/art_native_ground_sampler';r=subprocess.run([str(helper),str(P),str(R/'crag-current-water.points.bin'),str(R/'crag-current-water.samples.bin')],text=True,capture_output=True);(R/'crag-current-water.log').write_text(r.stdout+r.stderr);r.check_returncode();samples=np.fromfile(R/'crag-current-water.samples.bin','<f8').reshape(-1,8);rm=[];kept=[]
for item in crags:
 q=item['q'];a=samples[item['point_start']:item['point_start']+item['point_count']];wet=bool(a[:,4].any());native_bad=bool(a[0,4]and item['root']<=a[0,5]+.1)
 if wet:
  lines[item['index']]=None;rm.append({'before':item['before'],'reason':'Actual production river/pond water at center or basal footprint; removed, never raised','center_ground_y':float(a[0,2]),'center_water_level_y':float(a[0,5]),'center_wet':bool(a[0,4]),'wet_sample_count':int(a[:,4].sum())});continue
 oldscale=float(q[4]);oldaxes=np.array(list(map(float,q[10:13])));newscale=min(4.,oldscale);newaxes=oldaxes*oldscale/newscale;q[4]=f'{newscale:.10f}'
 for k,value in zip([10,11,12],newaxes):q[k]=f'{value:.10f}'
 after=' '.join(q);lines[item['index']]=after;kept.append({'before':item['before'],'after':after,'center_ground_y':float(a[0,2]),'root_y':item['root'],'water_samples':len(a),'all_center_and_basal_samples_dry':True,'effective_axis_error':float(np.max(np.abs(np.array(list(map(float,q[10:13])))*float(q[4])-oldaxes*oldscale)))})
after='\n'.join(s for s in lines if s is not None)+'\n';(R/'before-native-crag-fix.txt').write_text(before);file.write_text(after)
removed_rows={r['before']for r in rm};kept_rows={r['before']:r['after']for r in kept};newplacements=[]
for p in report['new_placements']:
 if p['alias']!='nm_camp_horizon_crag':newplacements.append(p);continue
 if p['row']in removed_rows:continue
 p['row']=kept_rows[p['row']];newplacements.append(p)
report['new_placements']=newplacements;report['horizon_outcrops']=[p for p in newplacements if p['alias']=='nm_camp_horizon_crag'];report['new_detail_records']=len(newplacements);report['changed_scenery']['authored_instances.txt']['after_sha256']=hashlib.sha256(file.read_bytes()).hexdigest();report['native_crag_correction']={'removed':rm,'kept':kept,'water_sampler_source_generation':str(N),'sampler_sha256':hashlib.sha256(helper.read_bytes()).hexdigest(),'supersedes_older_sampler_water_checks':True};report['walk_collision_change_note']=f'Only {len(kept)} genuinely dry scanned outcrops retain matching native transformed Box blockers; {len(rm)} wet outcrops removed.';(R/'ART_PATCH.json').write_text(json.dumps(report,indent=2)+'\n');(R/'CRAG_NATIVE_FIX.json').write_text(json.dumps({'passed':True,'removed':rm,'kept':kept,'before_sha256':hashlib.sha256(before.encode()).hexdigest(),'after_sha256':hashlib.sha256(after.encode()).hexdigest(),'source_code':str(N/'source-v1'),'sampler_sha256':hashlib.sha256(helper.read_bytes()).hexdigest()},indent=2)+'\n');print(json.dumps({'passed':True,'removed_wet_crags':len(rm),'kept_dry_crags':len(kept),'all_prop_scales_le_4':all(float(k['after'].split()[4])<=4 for k in kept)}))
