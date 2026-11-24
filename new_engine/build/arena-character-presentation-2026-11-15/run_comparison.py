from pathlib import Path
import json,os,subprocess,hashlib,time,sys
n=Path(__file__).resolve().parent;e=n.parents[1];b=n.parent/'arena-adaptive-combat-2026-11-15';eq=e/'build/cinematic-studio-2026-11-15/equipment-assets';assets=e/'build/arena-animation-polish-2026-11-15'
label=sys.argv[1];assert label in ('before','after');d=n/'review'/label;p=d/'private-project'
env={k:v for k,v in os.environ.items() if not k.startswith('VALWOOD_') and k!='LD_PRELOAD'}
env.update(VALWOOD_PRESENTATION='1',VALWOOD_PLAYTEST_SLICE='city',VALWOOD_COMBAT_ARENA='1',VALWOOD_MODULAR_KNIGHT=json.loads((eq/'HERO_ASSET.json').read_text())['path'],VALWOOD_MODULAR_KNIGHT_THIRD_PERSON='1',VALWOOD_MODULAR_KNIGHT_SHADER=str(assets/'shaders/world_skeletal.vs.glsl'),VALWOOD_MODULAR_KNIGHT_FIRST_PERSON_SHADER=str(assets/'shaders/rig_first_person.fs.glsl'),VALWOOD_SKELETAL_LOADOUTS=str(eq/'loadouts.manifest'),VALWOOD_QA_BLADE_CLASH='1',VALWOOD_QA_BLADE_CLASH_DIR=str(d),VALWOOD_QA_CLASH_FIRST_CASE='0',VALWOOD_QA_CLASH_LAST_CASE='2',SDL_AUDIODRIVER='dummy')
cmd=[str(b/'runtime-native-v1/native-bin/rpg'),'--project',str(p),'--play','--scenic','city-walk','--width','1600','--height','900','--frames','1200']
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
inputs={str(f):sha(f) for f in [b/'runtime-native-v1/native-bin/rpg',p/'shaders/world_foliage.fs.glsl',p/'world.studio',p/'graphics.settings',Path(__file__).resolve()]}
start=time.monotonic()
with (d/'native.log').open('w') as log:r=subprocess.run(cmd,cwd=e,env=env,stdout=log,stderr=subprocess.STDOUT,timeout=300)
assert all(sha(k)==v for k,v in inputs.items()),'Test inputs changed'
(d/'EXECUTION.json').write_text(json.dumps(dict(exit_code=r.returncode,inputs=inputs,command=cmd,elapsed_seconds=time.monotonic()-start,native_GPU=True,private_saves=True),indent=2)+'\n')
print(label,r.returncode,flush=True);raise SystemExit(r.returncode)
