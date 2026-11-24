"""Normal-play review in an isolated copy; launch scenario, then bounded controls."""
from pathlib import Path
import hashlib, importlib.util, json, os, shutil, subprocess, sys

N=Path(__file__).resolve().parents[1]
E=N.parents[1]
scenario=sys.argv[1]
assert scenario in ('apartment','camp')
R=N/'review'/('live-'+scenario)
Q=R/'project'

def launch():
    R.mkdir(exist_ok=False);Q.mkdir()
    P=E/'projects/valwood_current'
    sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
    protected={str(p):sha(p) for p in [P/'world.studio',P/'graphics.settings',*(P/'saves').glob('*')] if p.is_file()}
    for p in P.iterdir():
        if p.name in ('saves','shaders'):continue
        if p.is_dir():(Q/p.name).symlink_to(p.resolve(),target_is_directory=True)
        else:shutil.copy2(p,Q/p.name,follow_symlinks=True)
    (Q/'saves').mkdir()
    if scenario=='camp':shutil.copytree(P/'saves',Q/'saves',dirs_exist_ok=True)
    else:(Q/'saves/opening.progress').write_text('VALWOOD_OPENING 1\ncheckpoint 0\nplayer -3498 600 -11005 3.141592653589793 -0.03\nroom_lights 15\ncamp_spoken 0\n')
    (Q/'shaders').symlink_to(N/'shared/shaders',target_is_directory=True)
    spec=importlib.util.spec_from_file_location('response_launcher',N/'launch.py')
    m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
    cmd,env,_=m.private_configuration('demo');cmd[cmd.index('--project')+1]=str(Q)
    env.update(VALWOOD_QA_GRAPHICS_FILE=str(Q/'graphics.settings'),SDL_GAMECONTROLLER_IGNORE_DEVICES='0x046d/0xc21d',
               VALWOOD_OBSERVE_THIRD_PERSON=str(R/'frames.jsonl'),
               LD_PRELOAD=str(N.with_name('camp-completion-2026-11-21')/'tools/probe_gl_debug.so'))
    (R/'launch.json').write_text(json.dumps(dict(command=cmd,protected=protected,binary_sha256=sha(cmd[0]),
          environment={k:v for k,v in env.items() if k.startswith('VALWOOD_')}),indent=2)+'\n')
    with (R/'game.log').open('w') as log:
        p=subprocess.Popen(cmd,cwd=E,env=env,stdout=log,stderr=subprocess.STDOUT)
        (R/'pid').write_text(str(p.pid));print('Private normal-play',scenario,'PID',p.pid,flush=True)
        code=p.wait();print('exit',code,flush=True)
    intact=all(sha(p)==digest for p,digest in protected.items())
    (R/'state-preservation.json').write_text(json.dumps(dict(live_state_unchanged=intact,exit_code=code),indent=2)+'\n')
    raise SystemExit(code if intact else 1)

if sys.argv[2]=='launch':launch()
else:
    # Reuse the previously bounded native input/screenshot review helper.
    spec=importlib.util.spec_from_file_location('native_controls',N.with_name('camp-props-2026-11-22')/'tools/live_controls.py')
    m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
    m.R=R;m.Q=Q
    if sys.argv[2]=='record':
        seconds=float(sys.argv[4]);assert 0<seconds<=30
        x,y,w,h=m.control();target=R/(sys.argv[3]+'.mp4')
        assert not target.exists()
        subprocess.run(['ffmpeg','-nostdin','-v','error','-f','x11grab','-framerate','30',
                        '-video_size',f'{w}x{h}','-i',f'{os.environ.get("DISPLAY",":0")}+{x},{y}',
                        '-t',str(seconds),'-vf','scale=1280:-2','-c:v','libx264','-preset','fast',
                        '-crf','20','-pix_fmt','yuv420p',str(target)],check=True)
        print(target)
    else:
        sys.argv=sys.argv[1:];m.main()
