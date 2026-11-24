#!/usr/bin/env python3
"""Fast scene-data gate using the current generation's actual native parser."""
from pathlib import Path
import argparse,datetime,hashlib,json,os,subprocess,sys
N=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('project',nargs='?',default=str(N/'world-project'))
p.add_argument('--camera',choices=['camp','apartment'],default='camp')
p.add_argument('--records-only',action='store_true',help='Validate all strict records, deliberately stop before model/footprint phase')
p.add_argument('--overlay',type=Path,default=N/'vegetation/creek_dressing_instances.txt')
p.add_argument('--label',default=None)
a=p.parse_args()
label=a.label or a.camera+'-'+('records'if a.records_only else'full')+'-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S')
if not label or any(not(c.isalnum()or c in'-_')for c in label):p.error('Label must use letters, digits, dashes or underscores')
review=N/'review/preflight';review.mkdir(exist_ok=True)
receipt=json.loads((N/'runtime-native-v1/BUILD_RECEIPT.json').read_text())
build=review/'BUILD.json'
old=json.loads(build.read_text())if build.exists()else{}
source=N/'tools/preflight_scene.cpp';binary=N/'tools/preflight_scene'
if old.get('runtime_receipt_binary_sha256')!=receipt['binary_sha256']or old.get('source_sha256')!=hashlib.sha256(source.read_bytes()).hexdigest()or not binary.exists():
 subprocess.run([sys.executable,'-B',str(N/'tools/preflight_build.py')],check=True)
report=review/(label+'.json');log=review/(label+'.log')
command=[str(binary),str(Path(a.project).resolve()),'--camera',a.camera,'--overlay',str(a.overlay.resolve()),'--report',str(report)]
if a.records_only:command.append('--records-only')
env=dict(os.environ,SDL_VIDEODRIVER='offscreen',LIBGL_ALWAYS_SOFTWARE='1')
project=Path(a.project).resolve()
def input_hashes():
 names=['world.studio','opening.config','opening/interior.settings','ground/showcase.height','ground/countryside.height']
 names+=['scenery/'+n for n in ['authored_instances.txt','starting_area_instances.txt','rainforest_instances.txt','castle_village_instances.txt','tanthoon_bandit_camp_instances.txt','castle_streets_instances.txt','bep_environment_instances.txt','demo_finish_instances.txt','demo_polish_instances.txt','demo_completion_instances.txt']]
 files=[project/n for n in names]+[a.overlay.resolve()]
 return {str(f):hashlib.sha256(f.read_bytes()).hexdigest()for f in files if f.exists()}
before=input_hashes()
with log.open('w')as out:result=subprocess.run(command,stdout=out,stderr=subprocess.STDOUT,env=env)
after=input_hashes()
if report.exists():
 data=json.loads(report.read_text());data['input_sha256']=before;data['inputs_unchanged']=before==after
 if before!=after:data['passed']=False;data['error']='Input scene data changed during preflight; rerun after authoring finishes'
 report.write_text(json.dumps(data,indent=2)+'\n')
 print(('PASS'if data['passed']else'FAIL')+': '+data['stage']);print('%.3f seconds; '%data['elapsed_seconds']+data['mode'])
 if data['error']:print(data['error'])
 if data.get('instances')is not None:print('%s instances; %s aliases; %s files; %s colliders'%(data['instances'],data['aliases'],data['source_files'],data['colliders']))
else:print('Preflight exited without a report')
print('Log:',log);print('Report:',report)
raise SystemExit(result.returncode or (1 if before!=after else 0))
