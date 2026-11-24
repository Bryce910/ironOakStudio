from pathlib import Path
import sys,subprocess,os,json,shutil,time,hashlib
N=Path(__file__).resolve().parents[1];E=N.parents[1];sys.path.insert(0,str(N));from launch import configuration
cmd,env,origin=configuration('arena');d=N/'review/arena-floor-route';d.mkdir(exist_ok=True)
private=Path('/tmp/valwood-surface-floor-route');private.mkdir(exist_ok=True)
seed=Path('/tmp/valwood-surface-review-armor-clean')
for f in seed.iterdir():
 if f.name=='saves':continue
 out=private/f.name
 if out.exists() or out.is_symlink():continue
 if f.is_symlink():out.symlink_to(f.resolve(),target_is_directory=f.is_dir())
 elif f.is_dir():shutil.copytree(f,out,symlinks=True)
 else:shutil.copy2(f,out)
(private/'saves').mkdir(exist_ok=True);shutil.copy2(N/'arena-scene-patch/world.studio',private/'world.studio')
route=d/'floor.route';route.write_text('''VALWOOD_SOURCE_REVIEW 1 60
north 0 10 1 -1800 65.75 -10492 0 -.38 1.0471975512
west 60 8 1 -1810 65.75 -10500 -1.57 -.45 1.0471975512
south 60 8 1 -1800 65.75 -10510 -3.14 -.38 1.0471975512
east 60 8 1 -1790 65.75 -10500 -4.71 -.45 1.0471975512
entry 60 8 1 -1800 65.75 -10510 -6.283185 -.25 1.0471975512
centre 60 8 1 -1800 65.75 -10503 -6.283185 -.65 1.0471975512
''')
env.update(SDL_VIDEODRIVER='offscreen',SDL_AUDIODRIVER='dummy',MESA_DEBUG='1',VALWOOD_CAPTURE_ARENA_START='1',VALWOOD_QA_SOURCE_ROUTE=str(route),VALWOOD_QA_SOURCE_ROUTE_REPORT=str(d/'ROUTE_RAW'),VALWOOD_QA_SOURCE_ROUTE_CAPTURE=str(d/'floor'))
cmd[cmd.index('--project')+1]=str(private);cmd[cmd.index('--width')+1]='1280';cmd[cmd.index('--height')+1]='720';cmd+=['--hidden','--benchmark','--frames','600','--warmup','30','--capture',str(d/'result.bmp'),'--profile',str(d/'performance.json')]
start=time.monotonic()
with (d/'native.log').open('w') as out:r=subprocess.run(cmd,cwd=E,env=env,stdout=out,stderr=subprocess.STDOUT,timeout=300)
raw=json.loads((d/'ROUTE_RAW.json').read_text()) if (d/'ROUTE_RAW.json').exists() else {};world=json.loads((d/'performance.json.world.json').read_text()) if (d/'performance.json.world.json').exists() else {}
passed=r.returncode==0 and world.get('gl_error')==0 and raw.get('complete') is True and raw.get('failures')==0
record=dict(passed=passed,exit_code=r.returncode,raw=raw,world=world,binary_sha256=hashlib.sha256(Path(cmd[0]).read_bytes()).hexdigest(),elapsed_seconds=time.monotonic()-start,scope='Visual camera sweep in the actual arena renderer; camera is driven by existing source-review route. Not a movement/collision test.')
(d/'EXECUTION.json').write_text(json.dumps(record,indent=2)+'\n')
from PIL import Image
for f in d.glob('*.bmp'):
 with Image.open(f) as im:im.save(f.with_suffix('.png'))
print(json.dumps(dict(passed=passed,exit_code=r.returncode,raw=raw),indent=2));sys.exit(0 if passed else 1)
