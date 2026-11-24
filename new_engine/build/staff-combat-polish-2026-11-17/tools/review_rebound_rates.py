from pathlib import Path
import csv,json,math,hashlib
N=Path(__file__).resolve().parents[1]
def groups(path):
 g={}
 for r in csv.DictReader(open(path)):
  k=(r['weapon'],r['direction']);g.setdefault(k,[]).append({n:(v if n=='weapon' else float(v)) for n,v in r.items()})
 return g
def met(a):
 def edge(r):return [[r[p+'_'+k] for k in 'xyz'] for p in ['base','tip']]
 active=[r for r in a if r['recoil']>0];b=max(max(math.dist(x,y) for x,y in zip(edge(p),edge(q))) for p,q in zip(active,active[1:]))
 return dict(step_m=b,contact=edge(active[0]),contact_frame=active[0]['frame'],ready_frame=next(r['frame'] for r in a if r['ready']),grip_m=max(r['grip_error'] for r in a),monotone=all(q['source_phase']<=p['source_phase']+1e-6 for p,q in zip(active,active[1:])),peak_cpu_ms=max(r['update_ms'] for r in active))
rows=[]
for fps in [30,60,120]:
 b=groups(N/f'review/steel-before-{fps}.csv');a=groups(N/f'review/steel-after-{fps}.csv')
 for k in a:
  x,y=met(b[k]),met(a[k]);diff=max(math.dist(p,q) for p,q in zip(x['contact'],y['contact']))
  ok=diff<1e-6 and x['contact_frame']==y['contact_frame'] and x['ready_frame']==y['ready_frame'] and y['grip_m']<1e-4 and y['monotone'] and y['step_m']<x['step_m']*.95
  rows.append(dict(fps=fps,weapon=k[0],direction=int(k[1]),before=x,after=y,contact_difference_m=diff,passed=ok))
r=dict(passed=all(x['passed'] for x in rows),cases=len(rows),rates=[30,60,120],native_binary_sha256=hashlib.sha256((N/'runtime/rpg').read_bytes()).hexdigest(),scope='Isolated real CPU controller with deterministic steel-contact stub; native owned-geometry tests are separate.',results=rows)
(N/'review/REBOUND_RATE_REVIEW.json').write_text(json.dumps(r,indent=2)+'\n');print('Rate regression',r['passed'],r['cases'],'cases');print('Failures',[(r['fps'],r['weapon'],r['direction']) for r in rows if not r['passed']])
