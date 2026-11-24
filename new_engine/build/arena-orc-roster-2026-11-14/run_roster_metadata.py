from pathlib import Path
import json,subprocess,os,concurrent.futures
P=Path(__file__).resolve().parent
ids=[r['id']for r in json.loads((P/'intake_roster/ORC_SOURCE_ROSTER.json').read_text())['characters']if r['id']!='gnur']
def run(ident):
 env=os.environ.copy();env['VALWOOD_ORC_CHARACTER']=ident;env['VALWOOD_ORC_METADATA_ONLY']='1'
 with (P/(ident+'-metadata.log')).open('w')as out:r=subprocess.run(['/usr/bin/blender','-b','-t','2','--python',str(P/'cook_orc_roster_body.py')],env=env,stdout=out,stderr=subprocess.STDOUT)
 print(ident,r.returncode,flush=True);return ident,r.returncode
with concurrent.futures.ThreadPoolExecutor(max_workers=2)as pool:rows=list(pool.map(run,ids))
(P/'ROSTER_METADATA_PREFLIGHT.json').write_text(json.dumps({'processes':dict(rows)},indent=2)+'\n')
