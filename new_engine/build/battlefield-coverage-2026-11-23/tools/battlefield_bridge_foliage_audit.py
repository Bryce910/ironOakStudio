#!/usr/bin/env python3
"""Read-only proof of actual authored plant vertices above the bridge deck."""
from pathlib import Path
import re,math,shlex,subprocess,json,hashlib,sys
import numpy as np
N=Path(__file__).resolve().parents[1];P=N/'world-project';A=N/'review/bridge-foliage-audit';A.mkdir(exist_ok=True)
W=100800.;D=W*2818/5009;start=np.array([-12028.,5160.]);tangent=np.array([20.,8.]);length=np.linalg.norm(tangent);tangent/=length;side=np.array([-tangent[1],tangent[0]])
defs={}
for l in(N/'source-v1/engine/world_scenery.cpp').read_text().splitlines():
 q=l.strip().split(',')
 if len(q)>7 and q[0].startswith('{"'):
  name=q[0][2:-1];stem=q[1].strip().strip('"');tree=q[2].strip()=='true'
  try:veg=int(q[5].strip())
  except:continue
  defs[name]={'stem':stem,'plant':not tree and veg in(3,6),'tree':tree}
for l in(P/'world.studio').read_text().splitlines():
 q=shlex.split(l)
 if q and q[0]=='asset':defs[q[1]]={'stem':q[2],'plant':q[5]=='1','tree':q[5]=='2'}
baseline='--baseline' in sys.argv
receipt=json.loads((A/'RELOCATED.json').read_text()) if (A/'RELOCATED.json').exists() else None
def source_text(f):
 if baseline and receipt:
  key=('vegetation/creek_dressing_instances.txt' if 'creek_dressing' in str(f) else 'world-project/'+str(f.relative_to(P)))
  old=Path(receipt['backup_directory'])/key
  if old.is_file():return old.read_text()
 return f.read_text()
rows=[];missing=[]
for f in list((P/'scenery').glob('*.txt'))+[N/'vegetation/creek_dressing_instances.txt']:
 for i,l in enumerate(source_text(f).splitlines(),1):
  q=l.split()
  if len(q)<6 or q[0].startswith('#')or not defs.get(q[0],{}).get('plant'):continue
  x=(float(q[1])-.5)*W;z=(float(q[2])-.5)*D;v=np.array([x,z])-start;along=float(v@tangent);lateral=float(v@side)
  if not(-4<along<length+4 and abs(lateral)<5):continue
  extra=q[6:q.index('wind')]if'wind'in q else q[6:];extra=list(map(float,extra));stem=defs[q[0]]['stem'];path=P/'models'/(stem+'.glb')
  if not path.exists():path=P/'models'/(stem+'_near.glb')
  r={'source':str(f),'line':i,'alias':q[0],'root':[x,z],'tokens':q,'extra':extra,'path':str(path),'root_along':along,'root_lateral':lateral};rows.append(r)
  if len(extra)!=8:missing.append(r)
if missing:
 out=subprocess.run(['/tmp/battlefield_world_sample'],cwd=P,input=''.join(f"{r['root'][0]:.10f} {r['root'][1]:.10f}\n"for r in missing),text=True,capture_output=True,check=True)
 for r,l in zip(missing,out.stdout.splitlines()):r['sample']=list(map(float,l.split()))
cache={};intrusions=[]
for r in rows:
 q=r['tokens'];e=r['extra'];path=r['path']
 if path not in cache:
  out=A/(hashlib.sha256(path.encode()).hexdigest()[:16]+'.bin');subprocess.run(['/tmp/battlefield_dump_static_positions',path,str(out)],capture_output=True,text=True,check=True);cache[path]=np.fromfile(out,dtype='<f4').reshape(-1,3).astype(float)
 yaw,pitch,roll=map(math.radians,[float(q[3]),e[2]if len(e)>=7 else 0,e[3]if len(e)>=7 else 0]);cy,sy=math.cos(yaw),math.sin(yaw);cx,sx=math.cos(pitch),math.sin(pitch);cz,sz=math.cos(roll),math.sin(roll);axis=e[4:7]if len(e)>=7 else[1,1,1];m=np.array([[cy*cz+sy*sx*sz,-cy*sz+sy*sx*cz,sy*cx],[cx*sz,cx*cz,-sx],[-sy*cz+cy*sx*sz,sy*sz+cy*sx*cz,cy*cx]])@np.diag(np.array(axis)*float(q[4]));y=e[7]if len(e)==8 else r['sample'][2]-float(q[5])+(e[0]if e else 0)+(e[1]if len(e)>1 else 0);world=cache[path]@m.T+np.array([r['root'][0],y,r['root'][1]]);off=world[:,[0,2]]-start;along=off@tangent;lateral=off@side;deck=12+.15*along/length;hit=(along>=.05)&(along<=length-.05)&(abs(lateral)<=1.45)&(world[:,1]>deck+.04)&(world[:,1]<deck+2.2);r['vertices_above_deck_inside_rails']=int(hit.sum());r['world_bounds']=[world.min(axis=0).tolist(),world.max(axis=0).tolist()];r['deck_intrusion_proven']=bool(hit.sum()>=2)
 if r['deck_intrusion_proven']:intrusions.append(r);print(Path(r['source']).name,r['line'],r['alias'],r['vertices_above_deck_inside_rails'],'root_along/side',round(r['root_along'],2),round(r['root_lateral'],2))
report={'read_only':True,'bridge_start':start.tolist(),'bridge_end':[-12008.,5168.],'bridge_width':3.2,'proof_width_inside_rails':2.9,'above_deck_margin':.04,'model_positions':'Actual GLB scene node transforms + production lowestY normalization + exact source TRS','examined':len(rows),'intrusions':intrusions,'all_rows':rows,'source_hashes':{str(f):hashlib.sha256(source_text(f).encode()).hexdigest()for f in(P/'scenery').glob('*.txt')}}
(A/('BASELINE.json' if baseline else 'AFTER.json' if receipt else 'BEFORE.json')).write_text(json.dumps(report,indent=2)+'\n');print('ACTUAL authored intrusions',len(intrusions),'examined',len(rows))
