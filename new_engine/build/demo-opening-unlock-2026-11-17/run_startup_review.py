from pathlib import Path
import json,os,subprocess,time,sys,hashlib
N=Path(__file__).resolve().parent
E=N.parents[1]
mode=sys.argv[1]
assert mode in ('natural','skip','baseline')
folder=N/'review'/mode;folder.mkdir(parents=True,exist_ok=True)
if mode=='baseline':
    import shutil
    project=folder/'private-project'
    if not project.exists():shutil.copytree(E/'build/demo-final-pass-2026-11-15/demo-project',project,symlinks=True,ignore=shutil.ignore_patterns('saves','recovery','autosaves'),dirs_exist_ok=True)
    (project/'saves').mkdir(exist_ok=True)
    binary=E/'build/demo-final-pass-2026-11-15/runtime-native-v1/native-bin/rpg'
else:
    project=folder/'private-project';binary=N/'runtime-native-v1/native-bin/rpg'
env={k:v for k,v in os.environ.items() if not k.startswith('VALWOOD_') and k!='LD_PRELOAD'}
env.update(VALWOOD_PRESENTATION='1',VALWOOD_OPENING_PROLOGUE='1',VALWOOD_QA_OPENING_PROLOGUE='1',VALWOOD_QA_OPENING_STARTUP_ONLY='1',VALWOOD_QA_OPENING_CAPTURE_DIR=str(folder))
if mode in ('natural','baseline'):env['VALWOOD_QA_OPENING_CINEMATIC']='1'
capture=folder/'result.bmp'
command=[str(binary),'--project',str(project),'--play','--width','960','--height','540','--warmup','0','--frames','400' if mode=='baseline' else '1800','--capture',str(capture)]
start=time.monotonic()
with (folder/'game.log').open('w') as log:
    result=subprocess.run(command,cwd=E,env=env,stdout=log,stderr=subprocess.STDOUT,timeout=420)
world=json.loads(Path(str(capture)+'.world.json').read_text())
qa=world['opening_prologue_qa'];opening=world['opening_prologue'];entry=world['opening_entry_qa']
if mode=='baseline':
    passed=result.returncode==0 and opening['stage']=='house_establish' and not qa['passed'] and entry['observed_seconds']<.05
else:
    passed=result.returncode==0 and qa['passed'] and opening['stage']=='house_explore' and 'no_library_new_journey_release' in qa['checks'] and world['gl_error']==0
    if mode=='natural':passed=passed and entry['shot_mask']==15 and entry['matched_handoff'] and opening['skips']==0
    else:passed=passed and 'house_skip' in qa['checks'] and opening['skips']==1
record=dict(passed=passed,mode=mode,exit_code=result.returncode,elapsed_seconds=time.monotonic()-start,command=command,binary_sha256=hashlib.sha256(binary.read_bytes()).hexdigest(),opening=opening,qa=qa,entry=entry,gl_error=world['gl_error'])
(folder/'REVIEW.json').write_text(json.dumps(record,indent=2)+'\n')
print(json.dumps(record,indent=2))
sys.exit(0 if passed else 1)
