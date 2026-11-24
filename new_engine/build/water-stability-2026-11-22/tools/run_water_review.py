"""Private native consecutive-frame water regression. No live project writes."""
from pathlib import Path
import csv, hashlib, importlib.util, json, os, shutil, subprocess, sys, time
N=Path(__file__).resolve().parents[1];E=N.parents[1]
B=N.with_name('camp-completion-2026-11-21')
kind,run,tier=sys.argv[1:4]
assert kind in ('baseline','candidate') and tier in ('Low','Medium','High','Ultra')
R=N/'review'/run;R.mkdir(exist_ok=False);Q=R/'project';Q.mkdir()
P=E/'projects/valwood_current'
for f in P.iterdir():
    if f.name in ('saves','shaders'):continue
    if f.is_dir():(Q/f.name).symlink_to(f.resolve(),target_is_directory=True)
    else:shutil.copy2(f,Q/f.name,follow_symlinks=True)
(Q/'saves').mkdir()
g={line.split('=',1)[0]:line.split('=',1)[1] for line in (N.with_name('camp-props-2026-11-22')/'review/live-controls-v2/project/graphics.settings').read_text().splitlines() if '=' in line}
g.update(version='8',auto_quality='0',render_scale='0.75',min_render_scale='0.75',reflection_scale='1',vsync='0',frame_rate_limit='0',water_quality=tier)
(Q/'graphics.settings').write_text(''.join(k+'='+v+'\n' for k,v in g.items()))
G=B if kind=='baseline' else N
(Q/'shaders').symlink_to(G/'shared/shaders',target_is_directory=True)
route=R/'water.route'
route.write_text('VALWOOD_SOURCE_REVIEW 1 60\ncreek 0 300 1 -12008 14 5184 -1.249 -.30 1.0471975512\n')
if os.environ.get('VALWOOD_WATER_BRIDGE'):
    route.write_text('VALWOOD_SOURCE_REVIEW 1 60\nbridge-water 0 300 1 -12068 9.5 5304 2.95 -.12 1.0471975512\n')
if os.environ.get('VALWOOD_WATER_MOTION'):
    route.write_text('VALWOOD_SOURCE_REVIEW 1 60\ncreek 0 160 1 -12008 14 5184 -1.249 -.30 1.0471975512\nslow-move 180 100 1 -12007.5 14.2 5184.5 -1.24 -.31 1.0471975512\nturn 90 100 1 -12007.5 14.2 5184.5 -.70 -.20 1.0471975512\nbridge 0 140 1 -12063 13.6 5318 -1.57 -.35 1.0471975512\n')
spec=importlib.util.spec_from_file_location('water_launcher',G/'launch.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
cmd,env,_=m.private_configuration('world');cmd[cmd.index('--project')+1]=str(Q)
cmd+=['--scenic','city-meadow','--profile',str(R/'profile'),'--frames','100000','--benchmark']
env.update(VALWOOD_QA_GRAPHICS_FILE=str(Q/'graphics.settings'),SDL_GAMECONTROLLER_IGNORE_DEVICES='0x046d/0xc21d',VALWOOD_DEMO_PRESENTATION='0',VALWOOD_QA_SOURCE_ROUTE=str(route),VALWOOD_QA_SOURCE_ROUTE_REPORT=str(R/'route'),VALWOOD_QA_SOURCE_ROUTE_CAPTURE=str(R/'shot'),VALWOOD_WATER_CAPTURE_PREFIX=str(R/'frame'),VALWOOD_WATER_CAPTURE_GATE=str(R/'capture.ready'))
if not os.environ.get('VALWOOD_WATER_ANIMATE'):env['VALWOOD_QA_FIXED_TIME']='1'
env['LD_PRELOAD']=str(N/'tools/libwater_frame_probe.so')+':'+str(B/'tools/probe_gl_debug.so')
if os.environ.get('VALWOOD_WATER_EVERY_FRAME'):env['VALWOOD_QA_MIRROR_EVERY_FRAME']='1'
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
(R/'launch.json').write_text(json.dumps(dict(command=cmd,binary_sha256=sha(cmd[0]),environment={k:v for k,v in env.items() if k.startswith('VALWOOD_')},kind=kind,tier=tier),indent=2)+'\n')
with (R/'game.log').open('w') as out:
    p=subprocess.Popen(cmd,cwd=E,env=env,stdout=out,stderr=subprocess.STDOUT)
    (R/'pid').write_text(str(p.pid));print('Private water review PID',p.pid,flush=True)
    start=time.monotonic();gated=False
    while p.poll() is None:
        if time.monotonic()-start>420:p.terminate();p.wait(timeout=20);raise TimeoutError('Water review timed out')
        frames=R/'route.frames.csv'
        if not gated and frames.exists():
            rows=list(csv.DictReader(frames.read_text().splitlines()))
            phase='motion' if os.environ.get('VALWOOD_WATER_MOTION') else 'hold'
            needed=30 if phase=='motion' else 80
            if sum(x.get('phase')==phase for x in rows)>=needed:
                (R/'capture.ready').touch();gated=True;print('Capture gate opened during '+phase,flush=True)
        time.sleep(.2)
    print('exit',p.returncode,'captured',len(list(R.glob('frame-*.ppm'))),flush=True)
    sys.exit(p.returncode)
