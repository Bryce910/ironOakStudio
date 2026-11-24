#!/usr/bin/env python3
from pathlib import Path
import hashlib,json,os,shutil,subprocess,time
n=Path(__file__).resolve().parent;e=n.parents[1]
review=n/'review/native';review.mkdir(parents=True,exist_ok=True)
project=review/'private-project'
if project.exists():raise SystemExit('Existing private review retained; choose a separate review directory before rerunning')
shutil.copytree(n/'arena-project',project,symlinks=True)
(project/'.valwood-arena-roster-qa').write_text('VALWOOD_PRIVATE_ARENA_ROSTER_V1\n')
assets=e/'build/arena-animation-polish-2026-11-15';equipment=e/'build/cinematic-studio-2026-11-15/equipment-assets'
hero=json.loads((equipment/'HERO_ASSET.json').read_text())['path']
env={k:v for k,v in os.environ.items() if not k.startswith('VALWOOD_') and k!='LD_PRELOAD'}
env.update(VALWOOD_PRESENTATION='1',VALWOOD_PLAYTEST_SLICE='city',VALWOOD_COMBAT_ARENA='1',VALWOOD_MODULAR_KNIGHT=hero,
 VALWOOD_MODULAR_KNIGHT_THIRD_PERSON='1',VALWOOD_MODULAR_KNIGHT_SHADER=str(assets/'shaders/world_skeletal.vs.glsl'),
 VALWOOD_MODULAR_KNIGHT_FIRST_PERSON_SHADER=str(assets/'shaders/rig_first_person.fs.glsl'),VALWOOD_SKELETAL_LOADOUTS=str(equipment/'loadouts.manifest'),
 VALWOOD_QA_ARENA_ROSTER='1',VALWOOD_QA_ARENA_CAPACITY='1',VALWOOD_QA_ARENA_ROSTER_DIR=str(review))
command=[str(n/'runtime-native-v1/native-bin/rpg'),'--project',str(project),'--play','--scenic','city-walk','--width','1600','--height','900','--frames','2800']
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
watched=[n/'arena-project/saves/player.progress',e/'build/demo-final-pass-2026-11-15/arena-project/saves/player.progress']
before={str(p):sha(p) for p in watched}
start=time.monotonic()
with (review/'native.log').open('w') as log:
 proc=subprocess.run(command,cwd=e,env=env,stdout=log,stderr=subprocess.STDOUT,timeout=480)
receipt=dict(command=command,exit_code=proc.returncode,elapsed_seconds=time.monotonic()-start,public_purse_before=before,
 public_purse_after={str(p):sha(p) for p in watched},binary_sha256=sha(n/'runtime-native-v1/native-bin/rpg'),windows_work_performed=False)
(review/'NATIVE_EXECUTION.json').write_text(json.dumps(receipt,indent=2)+'\n')
print(json.dumps(receipt,indent=2),flush=True)
raise SystemExit(proc.returncode)
