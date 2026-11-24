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
import importlib.util

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--generation', choices=('baseline','candidate'),default='candidate')
parser.add_argument('--tier', choices=('Low','Medium','High','Ultra'), required=True)
parser.add_argument('--view', choices=('third','first'), required=True)
parser.add_argument('--name', required=True)
parser.add_argument('--frames', type=int, default=60)
parser.add_argument('--hardware',action='store_true')
parser.add_argument('--walking',action='store_true')
args = parser.parse_args()
G = N.with_name('apartment-night-2026-11-22') if args.generation=='baseline' else N
spec=importlib.util.spec_from_file_location('review_launcher',G/'launch.py')
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
configuration=m.configuration
if not args.name.replace('-', '').isalnum() or not 30 <= args.frames <= (9000 if args.walking else 300):
    raise ValueError('Choose a unique alphanumeric name and bounded frames30..300')
out = N/'review'/args.name
private = Path('/tmp')/('valwood-thomas-smoke-'+args.name)
if out.exists() or private.exists():
    raise ValueError('Preserve prior evidence; choose a new name')
out.mkdir(parents=True)
private.mkdir()
origin = N/'arena-project'
omitted = {'hero_locomotion','hero_grips','weapon_loadouts','aquatic_hero'}
if not args.walking:omitted.add('unarmed_hero')
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
if args.walking:
    # A deliberate free-walk fixture allows complete captured takes without
    # running into the combat pad's12m fences. Native sweeps/support remain.
    studio=private/'world.studio'
    studio.write_text(''.join(line for line in studio.read_text().splitlines(True) if not line.startswith('structure '))+'structure 8 1 \"\" 3 120 3 0.35 1.1 3 0 0 0 0 2 -1800 64 -10560 -1800 64 -10440\n')
    physics=private/'city.physics';rows=physics.read_text().splitlines()
    rows=[line for line in rows if not line.startswith('body ') or line.startswith('body 1 ')]
    rows=[line.replace('1 12 0.5 12 .75','1 60 0.5 60 .75') if line.startswith('body 1 ') else line for line in rows]
    physics.write_text('\n'.join(rows)+'\n')
(private/'saves').mkdir()
(private/'player-profile.config').write_text('VALWOOD_PLAYER_PROFILE 1\npath saves/player.progress\nend\n')
settings = (E/'projects/valwood_current/graphics.settings').read_text()
values = dict(character_textures=args.tier,shadow_side='4096',reflections='1',contact_shadows='1',volumetric_fog='1',
              clouds='1',foliage_distance='Ultra',foliage_density='Ultra',vsync='0',
              render_scale='1',min_render_scale='1',auto_quality='0')
for key,value in values.items():
    lines = settings.splitlines()
    settings = '\n'.join(key+'='+value if line.startswith(key+'=') else line for line in lines)+'\n'
(private/'graphics.settings').write_text(settings)
shaders=private/'shaders'
if shaders.is_symlink():
    shaders.unlink()
shaders.symlink_to(G/'shared/shaders',target_is_directory=True)
# World mode still chooses the real Thomas appearance and production skeletal
# controller. The private compact project's existing spawn/collision are kept.
command, env, _ = configuration('world')
command[command.index('--project')+1] = str(private)
command[command.index('--scenic')+1] = 'city-walk'
command[command.index('--width')+1] = '1280' if args.walking else '960'
command[command.index('--height')+1] = '720' if args.walking else '540'
env.update(SDL_VIDEODRIVER='x11' if args.hardware else 'offscreen',SDL_AUDIODRIVER='dummy',MESA_DEBUG='1',
           VALWOOD_PRESENTATION='1',VALWOOD_CAPTURE_VIEW=args.view)
# The compact scene owns a plane at64m. Its playtest spawn is the existing
# production route into that plane; no player/camera state is patched in code.
env['VALWOOD_PLAYTEST_SLICE']='city'
env.pop('VALWOOD_VEGETATION_DRESSING',None)
command += ([] if args.walking else ['--hidden'])+['--frames',str(args.frames),'--warmup','20','--capture',str(out/'result.bmp'),
            '--profile',str(out/'performance.json')]
if args.walking:
    if not args.hardware:raise ValueError('Input QA requires naturally focused visible X11 window')
    (private/'.valwood-walking-qa').write_text('VALWOOD_WALKING_QA_V1\n')
    env.update(VALWOOD_QA_WALKING_CONTROLS='1',VALWOOD_WALKING_QA_VIEW=args.view,
               VALWOOD_WALKING_QA_STOWED='1',VALWOOD_WALKING_QA_OUTPUT=str(out),VALWOOD_QA_CAPTURED_WALKING='1')
sha=lambda path:hashlib.sha256(Path(path).read_bytes()).hexdigest()
body=(G/'character-quality'/args.tier/'story_hero/civilian').resolve()
inputs=[body/'body.glb',body/'appearance.cfg',
        N/'motion/rigs/sword.glb',G/'launch.py',Path(__file__),private/'graphics.settings',G/'runtime-native-v1/BUILD_RECEIPT.json']
inputs += sorted(p for p in (N/'shared/shaders').rglob('*') if p.is_file())
inputs += [N/'motion/rigs/unarmed_transitions.glb',private/'world.studio',private/'city.physics',private/'playtest.route']
before={str(path):sha(path) for path in inputs}
start=time.monotonic()
timed_out=False
with (out/'native.log').open('w') as log:
    process=subprocess.Popen(command,cwd=E,env=env,stdout=log,stderr=subprocess.STDOUT)
    (out/'RUNNING.json').write_text(json.dumps(dict(pid=process.pid,command=command),indent=2)+'\n')
    try:
        # Consume bounded real-frame BMPs while the native route runs.
        from PIL import Image
        while process.poll() is None and time.monotonic()-start<600:
            for bmp in out.glob('walking-*.bmp'):
                try:
                    with Image.open(bmp) as image:image.save(bmp.with_suffix('.png'))
                    bmp.unlink()
                except OSError:pass
            time.sleep(.05)
        code=process.wait(timeout=1)
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
        str(body/'appearance.cfg')==story.get('appearance') and
        '[Characters/models] Loaded '+args.tier+' geometry' in log and
        env.get('VALWOOD_SHADER_BANK_ROOT')==str(G/'shared/shaders') and
        '[Story hero] Plain clothes loaded:' in log and
        '[skeletal-loadouts] loaded4 purchased rigs' in log and
        'Safe legacy fallback:' not in log and 'Story body update failed' not in log and
        all(sha(path)==expected for path,expected in before.items()))
if args.walking:
    checks=world.get('walking_controls_qa',{});finish=world.get('character_finish',{})
    passed=passed and checks.get('passed') is True and finish.get('facial_targets')==15 and finish.get('blink_frames',0)>0 and finish.get('captured_starts',0)>=3 and finish.get('captured_stops',0)>=3 and finish.get('captured_pivots',0)>=1
record=dict(schema='valwood.thomas-character-finish.native-character-smoke.v1',passed=passed,
            exit_code=code,timed_out=timed_out,elapsed_seconds=time.monotonic()-start,
            view=args.view,tier=args.tier,world=world,command=command,environment={key:value for key,value in env.items() if key.startswith('VALWOOD_') or key in ('SDL_VIDEODRIVER','LIBGL_ALWAYS_SOFTWARE')},
            inputs_sha256=before,binary_sha256=sha(command[0]),private_project=str(private),
            origin_project=str(origin),private_omitted_player_banks=sorted(omitted),
            shipping_projects_or_banks_changed=False,private_free_walk_floor=args.walking,private_low_graphics=False,private_character_tier=args.tier,private_shadow_side=4096,private_reflections=True,private_contact_shadows=True,
            limitation='Bounded private-world test, not full-world traversal or an AAA quality certification.',hardware_requested=args.hardware,walking_requested=args.walking,
            renderer_lines=[line for line in log.splitlines() if '[World] Renderer' in line])
(out/'EXECUTION.json').write_text(json.dumps(record,indent=2)+'\n')
if captured:
    from PIL import Image
    with Image.open(out/'result.bmp') as image:
        image.save(out/'result.png')
print(json.dumps({key:record[key] for key in ('passed','view','exit_code','elapsed_seconds','renderer_lines','limitation')},indent=2))
raise SystemExit(0 if passed else 1)
