"""Bounded native character draws in a private compact scene, without focus injection."""
from pathlib import Path
import argparse
import hashlib
import json
import os
import shutil
import subprocess
import sys
import time

N = Path(__file__).resolve().parents[1]
E = N.parents[1]
sys.dont_write_bytecode = True
sys.path.insert(0, str(N))
from launch import configuration

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--view', choices=('third','first'), required=True)
parser.add_argument('--name', required=True)
parser.add_argument('--frames', type=int, default=60)
args = parser.parse_args()
if not args.name.replace('-', '').isalnum() or not 30 <= args.frames <= 300:
    raise ValueError('Choose a unique alphanumeric name and bounded frames30..300')
out = N/'review'/args.name
private = Path('/tmp')/('valwood-thomas-smoke-'+args.name)
if out.exists() or private.exists():
    raise ValueError('Preserve prior evidence; choose a new name')
out.mkdir(parents=True)
private.mkdir()
origin = N/'arena-project'
omitted = {'hero_locomotion','hero_grips','unarmed_hero','weapon_loadouts','aquatic_hero'}
for item in origin.iterdir():
    if item.name in {'saves','settings','studio-layout.ini','world.studio.workspace','arena.config','models'} or '.recovery' in item.name:
        continue
    target = private/item.name
    if item.is_dir():
        target.symlink_to(item.resolve(), target_is_directory=True)
    else:
        shutil.copy2(item,target,follow_symlinks=True)
models = private/'models'
models.mkdir()
for item in (origin/'models').iterdir():
    if item.name not in omitted:
        (models/item.name).symlink_to(item.resolve(),target_is_directory=item.is_dir())
(private/'saves').mkdir()
(private/'player-profile.config').write_text('VALWOOD_PLAYER_PROFILE 1\npath saves/player.progress\nend\n')
settings = (N/'shared/graphics.settings').read_text()
values = dict(shadow_side='512',reflections='0',contact_shadows='0',volumetric_fog='0',
              clouds='0',foliage_distance='Low',foliage_density='Low',vsync='0',
              render_scale='1',min_render_scale='1',auto_quality='0')
for key,value in values.items():
    lines = settings.splitlines()
    settings = '\n'.join(key+'='+value if line.startswith(key+'=') else line for line in lines)+'\n'
(private/'graphics.settings').write_text(settings)
shaders=private/'shaders'
if shaders.is_symlink():
    shaders.unlink()
shaders.symlink_to(N/'shared/shaders',target_is_directory=True)
# World mode still chooses the real Thomas appearance and production skeletal
# controller. The private compact project's existing spawn/collision are kept.
command, env, _ = configuration('world')
command[command.index('--project')+1] = str(private)
command[command.index('--scenic')+1] = 'city-walk'
command[command.index('--width')+1] = '960'
command[command.index('--height')+1] = '540'
env.update(SDL_VIDEODRIVER='offscreen',SDL_AUDIODRIVER='dummy',MESA_DEBUG='1',
           VALWOOD_PRESENTATION='1',VALWOOD_CAPTURE_VIEW=args.view)
# The compact scene owns a plane at64m. Its playtest spawn is the existing
# production route into that plane; no player/camera state is patched in code.
env['VALWOOD_PLAYTEST_SLICE']='city'
command += ['--hidden','--frames',str(args.frames),'--warmup','20','--capture',str(out/'result.bmp'),
            '--profile',str(out/'performance.json')]
sha=lambda path:hashlib.sha256(Path(path).read_bytes()).hexdigest()
inputs=[N/'assets/story_hero/civilian/body.glb',N/'assets/story_hero/civilian/appearance.cfg',
        N/'motion/rigs/sword.glb',N/'launch.py',Path(__file__),private/'graphics.settings']
before={str(path):sha(path) for path in inputs}
start=time.monotonic()
timed_out=False
with (out/'native.log').open('w') as log:
    process=subprocess.Popen(command,cwd=E,env=env,stdout=log,stderr=subprocess.STDOUT)
    (out/'RUNNING.json').write_text(json.dumps(dict(pid=process.pid,command=command),indent=2)+'\n')
    try:
        code=process.wait(timeout=420)
    except subprocess.TimeoutExpired:
        timed_out=True
        process.kill()
        code=process.wait()
log=(out/'native.log').read_text()
world_path=out/'performance.json.world.json'
if not world_path.exists():
    world_path=out/'result.bmp.world.json'
world=json.loads(world_path.read_text()) if world_path.exists() else {}
story=world.get('story_hero',{})
skeletal=world.get('skeletal_hero',{})
captured=(out/'result.bmp').is_file()
passed=(code==0 and not timed_out and captured and world.get('gl_error')==0 and
        story.get('civilian_ready') is True and story.get('failures')==0 and
        '[Story hero] Plain clothes loaded:' in log and
        '[skeletal-loadouts] loaded4 purchased rigs' in log and
        'Safe legacy fallback:' not in log and 'Story body update failed' not in log and
        all(sha(path)==expected for path,expected in before.items()))
record=dict(schema='valwood.thomas-kubold.native-character-smoke.v1',passed=passed,
            exit_code=code,timed_out=timed_out,elapsed_seconds=time.monotonic()-start,
            view=args.view,world=world,command=command,environment={key:value for key,value in env.items() if key.startswith('VALWOOD_') or key in ('SDL_VIDEODRIVER','LIBGL_ALWAYS_SOFTWARE')},
            inputs_sha256=before,binary_sha256=sha(command[0]),private_project=str(private),
            origin_project=str(origin),private_omitted_player_banks=sorted(omitted),
            shipping_projects_or_banks_changed=False,private_low_graphics=True,
            limitation='Static native draw smoke only; no user input, full-world traversal, motion flow, hardware performance or AAA quality is certified.',
            renderer_lines=[line for line in log.splitlines() if '[World] Renderer' in line])
(out/'EXECUTION.json').write_text(json.dumps(record,indent=2)+'\n')
if captured:
    from PIL import Image
    with Image.open(out/'result.bmp') as image:
        image.save(out/'result.png')
print(json.dumps({key:record[key] for key in ('passed','view','exit_code','elapsed_seconds','renderer_lines','limitation')},indent=2))
raise SystemExit(0 if passed else 1)
