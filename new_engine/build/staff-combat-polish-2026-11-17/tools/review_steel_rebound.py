from pathlib import Path
import csv,json,math
N=Path(__file__).resolve().parents[1]
def read(name):
 rows=list(csv.DictReader(open(N/'review'/name)));groups={}
 for r in rows:
  key=(r['weapon'],int(r['direction']));groups.setdefault(key,[]).append({k:(v if k=='weapon' else float(v)) for k,v in r.items()})
 return groups
def metrics(rows):
 def edge(r):return [[r[p+'_'+k] for k in ['x','y','z']] for p in ['base','tip']]
 moving=[];phase=[];cost=[]
 for a,b in zip(rows,rows[1:]):
  if a['recoil']>0 and b['recoil']>0:
   moving.append(max(math.dist(x,y) for x,y in zip(edge(a),edge(b))))
   phase.append(b['source_phase']<=a['source_phase']+1e-6)
  if b['recoil']>0:cost.append(b['update_ms'])
 first=next(r for r in rows if r['recoil']>0);last=next(r for r in rows if r['ready'])
 return dict(maximum_recoil_step_m=max(moving),monotone_source=all(phase),contact_edge=edge(first),first_recoil_frame=int(first['frame']),ready_frame=int(last['frame']),maximum_grip_error_m=max(r['grip_error'] for r in rows),maximum_recoil_update_ms=max(cost))
before,after=read('steel-before-60.csv'),read('steel-after-60.csv');result=[]
for key,rows in after.items():
 b,a=metrics(before[key]),metrics(rows);entry=max(math.dist(x,y) for x,y in zip(a['contact_edge'],b['contact_edge']))
 result.append(dict(weapon=key[0],direction=key[1],before=b,after=a,contact_difference_m=entry,reduction_percent=100*(1-a['maximum_recoil_step_m']/b['maximum_recoil_step_m']),passed=entry<1e-6 and a['first_recoil_frame']==b['first_recoil_frame'] and a['ready_frame']==b['ready_frame'] and a['monotone_source'] and a['maximum_grip_error_m']<1e-4))
report=dict(passed=all(r['passed'] for r in result),scope='Actual CPU controller and combat clock, deterministic steel-contact stub. Native geometry acceptance is separate.',fps=60,results=result)
(N/'review/REBOUND_CPU_REVIEW.json').write_text(json.dumps(report,indent=2)+'\n')
for weapon in ['sword','sword_shield','great_axe','staff']:
 r=[r for r in result if r['weapon']==weapon];print(weapon,'worst before/after',max(x['before']['maximum_recoil_step_m'] for x in r),max(x['after']['maximum_recoil_step_m'] for x in r),'min reduction',min(x['reduction_percent'] for x in r),'CPU max ms',max(x['after']['maximum_recoil_update_ms'] for x in r))
print('passed',report['passed'])
