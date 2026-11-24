from pathlib import Path
import importlib.util,json,hashlib,numpy as np
N=Path(__file__).resolve().parents[1];E=N.parents[1];P=N/'world-project';O=N/'review/woodland-detail'
def module(name,path):
 s=importlib.util.spec_from_file_location(name,path);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m
st=module('st',N/'tools/build_forest_layers.py');geo=module('gg',N/'tools/review_forest_layout.py');mesh=module('m',E/'docs/seldonne-first-region/2026-11-07/region_mesh_grounding_v5.py');ground,_=st.sampler();r=json.loads((O/'PATCH.json').read_text());lines=(P/'scenery/authored_instances.txt').read_text().splitlines();changes=[]
for item in r['placements']:
 if item['kind']not in ['bank_pebble','bank_roots']or item['actual_height_m']<=.23:continue
 before=item['row'];p=item['pose'];g,_,_=geo.geometry(Path(r['assets'][item['alias']]['path']));p['scale']*=.229/item['actual_height_m'];st.native_scale(p);h=mesh.support(p,g,ground,item['support']['embed_m']);v=(g*np.array([p['sx'],p['sy'],p['sz']])*p['scale'])@mesh.rotation(p['yaw_deg'],p['pitch_deg'],p['roll_deg']).T;bottom=v[v[:,1]<=v[:,1].min()+max(.02,h*.025)]
 if len(bottom)>64:bottom=bottom[np.linspace(0,len(bottom)-1,64,dtype=int)]
 gaps=np.array([p['y']+b[1]-ground(p['x']+b[0],p['z']+b[2])for b in bottom]);q=before.split();q[4]=f'{p["scale"]:.9f}'
 for k,f in [(10,'sx'),(11,'sy'),(12,'sz')]:q[k]=f'{p[f]:.10f}'
 q[13]=f'{p["y"]:.9f}';after=' '.join(q);assert lines[item['line']-1]==before;lines[item['line']-1]=after;item['row']=after;oldrad=item['full_mesh_radius_m'];newrad=float(np.linalg.norm(v[:,[0,2]],axis=1).max());item['full_mesh_radius_m']=newrad
 for k in ['route_clearance_m','swim_fixture_clearance_m','creek_full_mesh_clearance_m']:item[k]+=oldrad-newrad
 item['actual_height_m']=float(h);item['support'].update(basal_samples=len(bottom),minimum_basal_clearance_m=float(gaps.min()),maximum_basal_clearance_m=float(gaps.max()));changes.append(dict(file=item['file'],line=item['line'],before_row=before,after_row=after,reason='Keep complete transformed noncolliding stone/root geometry below0.23m player step height.'))
path=P/'scenery/authored_instances.txt';path.write_text('\n'.join(lines)+'\n');r['changed_files']['scenery/authored_instances.txt']['after_sha256']=hashlib.sha256(path.read_bytes()).hexdigest();r['low_detail_height_revision']=changes;r['finalizer_sha256']=hashlib.sha256(Path(__file__).read_bytes()).hexdigest();(O/'PATCH.json').write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(dict(revised=len(changes),max_low_detail_height_m=max(p['actual_height_m']for p in r['placements']if p['kind']in ['bank_pebble','bank_roots']),patch_sha256=hashlib.sha256((O/'PATCH.json').read_bytes()).hexdigest()),indent=2))
