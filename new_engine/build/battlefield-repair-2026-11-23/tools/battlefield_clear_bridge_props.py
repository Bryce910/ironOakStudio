#!/usr/bin/env python3
"""Private-candidate-only CAS relocation of four confirmed bridge obstructions."""
from pathlib import Path
import datetime,hashlib,json,math,subprocess,os,itertools
import numpy as np
N=Path(__file__).resolve().parents[1];P=N/'world-project';W=100800.;D=W*2818/5009
plan=json.loads((N/'review/PUBLISH_PLAN.json').read_text());expected={e['relative_path']:e['candidate']['sha256']for e in plan['files']}
changes=[('scenery/demo_polish_instances.txt',64,'nm_city_region_arrival_rock',-12045.,5178.),('scenery/authored_instances.txt',3275,'rock0',-12036.,5175.),('scenery/authored_instances.txt',3295,'deadwood',-12039.,5178.),('scenery/authored_instances.txt',3508,'rock0',-12038.,5174.)]
raw={};lines={};entries=[]
for rel,*_ in changes:
 if rel in raw:continue
 p=P/rel;assert not p.is_symlink();raw[rel]=p.read_bytes();assert hashlib.sha256(raw[rel]).hexdigest()==expected[rel];lines[rel]=raw[rel].decode().splitlines(keepends=True)
for rel,line,alias,x,z in changes:
 old=lines[rel][line-1].rstrip('\n');q=old.split();assert q[0]==alias and len(q)>=14;stem='cinematic_deadwood'if alias=='deadwood'else'cinematic_rock_0';path=P/'models'/(stem+'.glb')
 out=subprocess.run(['/tmp/battlefield_model_bounds',str(path)],text=True,capture_output=True,check=True).stdout.split();lo=np.array(list(map(float,out[1:4])));hi=np.array(list(map(float,out[4:7])));yaw,pitch,roll=map(math.radians,[float(q[3]),float(q[8]),float(q[9])]);cy,sy=math.cos(yaw),math.sin(yaw);cx,sx=math.cos(pitch),math.sin(pitch);cz,sz=math.cos(roll),math.sin(roll)
 rot=np.array([[cy*cz+sy*sx*sz,-cy*sz+sy*sx*cz,sy*cx],[cx*sz,cx*cz,-sx],[-sy*cz+cy*sx*sz,sy*sz+cy*sx*cz,cy*cx]])@np.diag(np.array(list(map(float,q[10:13])))*float(q[4]));corners=np.array(list(itertools.product(*zip(lo,hi))));transformed=corners@rot.T;bottom=transformed[:,1].min();samples=[(x,z)]
 for iz in range(5):
  for ix in range(5):
   p=rot@np.array([lo[0]+ix*.25*(hi[0]-lo[0]),lo[1],lo[2]+iz*.25*(hi[2]-lo[2])]);samples.append((x+p[0],z+p[2]))
 run=subprocess.run(['/tmp/battlefield_world_sample'],cwd=P,input=''.join(f'{a:.10f} {b:.10f}\n'for a,b in samples),text=True,capture_output=True,check=True);ground=[list(map(float,l.split()))for l in run.stdout.splitlines()];assert len(ground)==26 and all(l[3]==0 and l[2]>10.9 for l in ground)
 pivot=min(v[2]for v in ground)-bottom-float(q[5])-.05;oldroot=[(float(q[1])-.5)*W,(float(q[2])-.5)*D];q[1]=f'{x/W+.5:.14f}';q[2]=f'{z/D+.5:.14f}';q[13]=f'{pivot:.9f}';new=' '.join(q);lines[rel][line-1]=new+'\n';entries.append({'relative_path':rel,'line_1based':line,'alias':alias,'old_root':oldroot,'new_root':[x,pivot,z],'baseline_line':old,'candidate_line':new,'production_model_bounds':[lo.tolist(),hi.tolist()],'new_support_samples':ground,'all_support_samples_dry':True,'support_policy':'Exact renderer TRS and5x5 rigid-ground support footprint, minus5cm slack; explicit pivot retained'})
stamp=datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%SZ');backup=N/'review'/('bridge-prop-backup-'+stamp);backup.mkdir();committed=[]
try:
 for rel,data in raw.items():
  b=backup/rel;b.parent.mkdir(parents=True,exist_ok=True);b.write_bytes(data)
 for rel in raw:
  p=P/rel;assert hashlib.sha256(p.read_bytes()).hexdigest()==expected[rel];temp=p.with_name('.'+p.name+'.bridge-stage');assert not temp.exists();temp.write_text(''.join(lines[rel]));os.replace(temp,p);committed.append(rel)
except BaseException:
 for rel in reversed(committed):(P/rel).write_bytes(raw[rel])
 raise
receipt={'candidate_only':True,'live_world_mutations':False,'source_records_added_removed_reordered':False,'relocations':entries,'before_sha256':{r:hashlib.sha256(d).hexdigest()for r,d in raw.items()},'after_sha256':{r:hashlib.sha256((P/r).read_bytes()).hexdigest()for r in raw},'backup_directory':str(backup),'terrain_water_bridge_unchanged':True,'native_crossing_still_required':True}
(N/'review/battlefield_bridge_prop_relocations.json').write_text(json.dumps(receipt,indent=2)+'\n');print(json.dumps({k:v for k,v in receipt.items()if k!='relocations'}))
