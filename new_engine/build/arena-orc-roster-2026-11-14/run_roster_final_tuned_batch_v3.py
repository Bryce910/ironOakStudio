from pathlib import Path
import subprocess,os,json,time
P=Path(__file__).resolve().parent
ids=['orc01','orc04','orc03','orc05','costalmed','kanit','ursag'];rows=[]
for ident in ids:
 env=os.environ.copy();env.update(VALWOOD_ORC_CHARACTER=ident,OPENBLAS_NUM_THREADS='1',OMP_NUM_THREADS='2')
 with(P/(ident+'-body-v3-final.log')).open('w')as log:r=subprocess.run(['/usr/bin/blender','-b','-t','2','--python-exit-code','1','--python',str(P/'cook_orc_roster_body_v3.py')],env=env,stdout=log,stderr=subprocess.STDOUT)
 passed=r.returncode==0
 if passed:
  with(P/(ident+'-retarget-review.log')).open('w')as log:r=subprocess.run(['python3',str(P/'check_roster_retarget.py'),ident],env=env,stdout=log,stderr=subprocess.STDOUT);passed=r.returncode==0
 row={'id':ident,'passed':passed,'time':time.time()};rows.append(row);print(json.dumps(row),flush=True);(P/'ROSTER_FINAL_TUNED_BATCH_STATUS.json').write_text(json.dumps({'characters':rows},indent=2)+'\n')
