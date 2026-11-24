"""Replace nearby whole-clump cards by source-photographed individual 3D tufts.
Run after terrain authoring. No movement blockers are added; final rotated footprints require clearance review.
"""
from pathlib import Path
import json,hashlib,struct,shlex,importlib.util,math,subprocess,sys
import numpy as np
C=Path(__file__).resolve().parents[1];P=C/'world-project';R=C/'review/grass';E=C.parents[1];W=100800.;D=W*2818/5009
spec=importlib.util.spec_from_file_location('mesh_ground',E/'docs/seldonne-first-region/2026-11-07/region_mesh_grounding_v5.py');g=importlib.util.module_from_spec(spec);spec.loader.exec_module(g)
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def geom(p):
 v=g.geometry(p);raw=p.read_bytes();n=struct.unpack_from('<I',raw,12)[0];d=json.loads(raw[20:20+n]);b=raw[28+n:];bottom=[]
 for mesh in d['meshes']:
  for pr in mesh['primitives']:
   a=d['accessors'][pr['attributes']['POSITION']];view=d['bufferViews'][a['bufferView']];vv=np.ndarray((a['count'],3),'<f4',buffer=b,offset=view.get('byteOffset',0)+a.get('byteOffset',0),strides=(view.get('byteStride',12),4));bottom.append(vv[:,1].min())
 v[:,1]-=min(bottom);return v
names=['fine','arching','wild'];new={k:geom(P/f'models/camp_atmosphere/grass_{k}.glb')for k in names}
for k in names:
 mid=geom(P/f'models/camp_atmosphere/grass_{k}_mid.glb');assert np.max(np.abs(np.ptp(new[k],axis=0)-np.ptp(mid,axis=0)))<.0001
text=(P/'world.studio').read_text();defs={q[1]:q for s in text.splitlines() if(q:=shlex.split(s))and q[0]=='asset'}
oldaliases=['opening_verge_grass_broad','opening_verge_grass_long','bep_nature_grass_broad','bep_nature_grass_long'];old={a:geom(P/'models'/(defs[a][2]+'.glb'))for a in oldaliases}
records=[]
for path in sorted((P/'scenery').glob('*.txt')):
 for line,s in enumerate(path.read_text().splitlines(),1):
  q=s.split()
  if len(q)<14 or q[0]not in old:continue
  x=(float(q[1])-.5)*W;z=(float(q[2])-.5)*D
  if math.hypot(x+11988,z-5172)>34:continue
  scale=float(q[4]);axes=np.array(list(map(float,q[10:13])));size=np.ptp(old[q[0]],axis=0)*scale*axes
  if not(.14<size[1]<1.2 and min(size[[0,2]])>.16):continue
  k=names[int.from_bytes(hashlib.sha256((path.name+':'+str(line)).encode()).digest()[:4],'little')%3];v=new[k];ns=1.;na=size/np.ptp(v,axis=0)
  # Individual source tufts retain a believable blade aspect ratio; do not
  # widen them beyond the card's existing footprint or stretch long stalks.
  na[[0,2]]=np.minimum(na[[0,2]],na[1]*1.25)
  na=np.maximum(na,.1001);ns=1.
  if max(na)>8 or min(na)<.1:raise ValueError('Axis outside native authoring contract')
  pose={'x':x,'z':z,'yaw_deg':float(q[3]),'pitch_deg':float(q[8]),'roll_deg':float(q[9]),'sx':na[0],'sy':na[1],'sz':na[2],'scale':ns};rot=g.rotation(pose['yaw_deg'],pose['pitch_deg'],pose['roll_deg']);rel=(v*na)@rot.T;low=rel[rel[:,1]<rel[:,1].min()+max(.025,np.ptp(rel[:,1])*.05)]
  if len(low)>64:low=low[np.linspace(0,len(low)-1,64,dtype=int)]
  records.append({'file':str(path.relative_to(P)),'line':line,'before':s,'q':q,'variant':k,'x':x,'z':z,'axes':na,'rel':rel,'basal':low,'old_size':size})
if '--prepare'in sys.argv:
 print(json.dumps({'proposed_rows':len(records),'unique_texture_family':1,'new_models':6,'count_increase':0,'new_blockers':0}));sys.exit()
if len(sys.argv)!=2:raise SystemExit('Usage:place_ground_grass.py NATIVE_HEIGHT_SAMPLER')
helper=Path(sys.argv[1]);points=[]
for r in records:
 r['sample_start']=len(points);points.extend(np.array([r['x'],r['z']])+r['basal'][:,[0,2]]);r['sample_end']=len(points)
pts=np.asarray(points,'<f8');inp=R/'grass_support_points.bin';out=R/'grass_support_result.bin';pts.tofile(inp)
subprocess.run([str(helper.resolve()),str(P.resolve()),str(inp.resolve()),str(out.resolve())],check=True)
samples=np.fromfile(out,'<f8').reshape(-1,8);assert len(samples)==len(pts) and np.all(np.isfinite(samples[:,:4]));assert np.max(np.abs(samples[:,2]-samples[:,3]))<1e-5
rows={f:(P/f).read_text().splitlines()for f in {r['file']for r in records}};applied=[]
for r in records:
 h=samples[r['sample_start']:r['sample_end'],2];low=r['basal'];supports=h-low[:,1];y=float(np.quantile(supports,.35))-.02;gaps=y+low[:,1]-h;y-=max(0,float(gaps.max())-.04);gaps=y+low[:,1]-h
 if gaps.min()<-.30 or gaps.max()>.065:continue
 q=r['q'];q[0]='camp_ground_grass_'+r['variant'];q[4]='1';q[10:13]=[f'{v:.10f}'for v in r['axes']];q[13]=f'{y:.9f}';after=' '.join(q);assert len(' '.join(q[:14]))<256
 assert rows[r['file']][r['line']-1]==r['before'];rows[r['file']][r['line']-1]=after
 applied.append({'file':r['file'],'line':r['line'],'before':r['before'],'after':after,'variant':r['variant'],'old_size':r['old_size'].tolist(),'new_size':np.ptp(r['rel'],axis=0).tolist(),'support_gap_min':float(gaps.min()),'support_gap_max':float(gaps.max())})
archive=R/'before-grass';archive.mkdir()
for f,s in rows.items():
 (archive/Path(f).name).write_bytes((P/f).read_bytes());(P/f).write_text('\n'.join(s)+'\n')
# Merge only definitions into the latest Studio text, retaining other owners' rows.
text=(P/'world.studio').read_text();(archive/'world.studio').write_text(text)
for k in names:
 alias='camp_ground_grass_'+k;assert ('asset '+alias+' ')not in text
 text+=f'asset {alias} camp_atmosphere/grass_{k} 2 0 1 75 20 1\n'
(P/'world.studio').write_text(text)
report={'passed':True,'replaced_rows':len(applied),'count_increase':0,'movement_blockers_added':0,'new_aliases':3,'new_model_files':6,'source':'https://polyhaven.com/a/grass_medium_01','maps':'Matching color,normal,ARM at1024², shared by all3variants and bothLODs','native_walk_and_render_height_max_delta':float(np.max(np.abs(samples[:,2]-samples[:,3]))),'rows':applied,'input_sha256':{str(p):sha(p)for p in [Path(__file__),helper,R/'SOURCE.json',R/'COOK.json',P/'world.studio',P/'ground/showcase.height']}}
(R/'PLACEMENT.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({k:v for k,v in report.items()if k not in ['rows','input_sha256']}))
