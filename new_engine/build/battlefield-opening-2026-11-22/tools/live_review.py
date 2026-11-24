"""Private normal-input battlefield review. No writes to published world or saves."""
from pathlib import Path
import argparse,hashlib,importlib.util,json,os,shutil,subprocess,sys
from private_review_resume import validate_resume_source,copy_resume_files,validate_profile_prefix
N=Path(__file__).resolve().parents[1];E=N.parents[1]
scenario=sys.argv[1];assert scenario in ('battlefield','full')
R=N/'review'/('live-'+scenario);Q=R/'project'
def launch(resume_from=None,profile_prefix=None,clean_performance=False,resume_render_compatible=False,shadow_fetch=None):
 profile=validate_profile_prefix(profile_prefix,R) if profile_prefix else None
 spec=importlib.util.spec_from_file_location('battlefield_launcher',N/'launch.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
 cmd,env,_=m.private_configuration('demo');cmd[cmd.index('--project')+1]=str(Q)
 if resume_render_compatible and not resume_from:raise ValueError('--resume-render-compatible requires actual archived native saves.')
 resume=validate_resume_source(resume_from,N/'review',cmd[0],R,N/'runtime-native-v1/BUILD_RECEIPT.json' if resume_render_compatible else None) if resume_from else None
 R.mkdir(exist_ok=False);Q.mkdir()
 if profile:profile.parent.mkdir(parents=True,exist_ok=True)
 P=N/'world-project';live=E/'projects/valwood_current'
 sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
 protected={str(p):sha(p) for p in [live/'world.studio',live/'graphics.settings',*(live/'saves').glob('*')] if p.is_file()}
 for p in P.iterdir():
  if p.name in ('saves','shaders'):continue
  if p.is_dir():(Q/p.name).symlink_to(p.resolve(),target_is_directory=True)
  else:shutil.copy2(p,Q/p.name,follow_symlinks=True)
 (Q/'saves').mkdir();(Q/'shaders').symlink_to(N/'shared/shaders')
 (Q/'PRIVATE_CAPTURE_PROJECT.json').write_text('{"private":true}\n')
 if resume:
  copy_resume_files(resume,Q)
  (R/'resume-provenance.json').write_text(json.dumps(resume,indent=2)+'\n')
 elif scenario=='battlefield':
  # Normal Continue action restores the real guarded checkpoint; no live state
  # mutation or special in-game input path bypasses the actual QTE controls.
  fields=(Q/'opening.config').read_text().splitlines()
  pose=next(x.split(' ',1)[1] for x in fields if x.startswith('meadow_spawn '))
  (Q/'saves/opening.progress').write_text('VALWOOD_OPENING 1\ncheckpoint 2\nplayer '+pose+'\nroom_lights 15\ncamp_spoken 0\nbattlefield_progress "VALWOOD_BATTLEFIELD 1\ncheckpoint 0\ndifficulty 1\nsword 0\nwins 0\n"\n')
 env.update(VALWOOD_QA_GRAPHICS_FILE=str(Q/'graphics.settings'),SDL_GAMECONTROLLER_IGNORE_DEVICES='0x046d/0xc21d',
  VALWOOD_OBSERVE_THIRD_PERSON=str(R/'frames.jsonl'),VALWOOD_OBSERVE_BATTLEFIELD=str(R/'battlefield.jsonl'),
  VALWOOD_BATTLEFIELD_CAPTURE=str(N/'battlefield-motion/captures.glb'),VALWOOD_BATTLEFIELD_KNIGHT_BODY=str(N/'arena-project/knights_roster/knight09/body.glb'),
  LD_PRELOAD=str(N.with_name('camp-completion-2026-11-21')/'tools/probe_gl_debug.so'))
 if clean_performance:
  # Ordinary gameplay controls remain enabled. Exclude the expensive GL debug
  # preload when measuring the actual shipping path during a normal playthrough.
  env.pop('LD_PRELOAD',None);env.pop('VALWOOD_QA_GPU_BUDGET',None)
 if profile:
  cmd.extend(['--profile',str(profile),'--frames','36000','--warmup','0'])
  env.update(VALWOOD_PROFILE_PHASES='1');env.pop('VALWOOD_QA_GPU_BUDGET',None);env.pop('LD_PRELOAD',None)
 if shadow_fetch:
  if not profile or not resume:raise ValueError('Explicit shadow-fetch comparison requires native profile output and actual archived saves.')
  env.pop('VALWOOD_QA_ENABLE_SHADOW_GATHER',None);env.pop('VALWOOD_QA_DISABLE_SHADOW_GATHER',None)
  env['VALWOOD_QA_ENABLE_SHADOW_GATHER' if shadow_fetch=='gather' else 'VALWOOD_QA_DISABLE_SHADOW_GATHER']='1'
 (R/'launch.json').write_text(json.dumps(dict(command=cmd,protected=protected,binary_sha256=sha(cmd[0]),resume=resume,profile_prefix=str(profile) if profile else None,clean_performance=clean_performance,gl_debug_preload=bool(env.get('LD_PRELOAD')),environment={k:v for k,v in env.items() if k.startswith('VALWOOD_')}),indent=2)+'\n')
 with (R/'game.log').open('w') as log:
  p=subprocess.Popen(cmd,cwd=E,env=env,stdout=log,stderr=subprocess.STDOUT);(R/'pid').write_text(str(p.pid));print('Private',scenario,'PID',p.pid,flush=True);code=p.wait();print('exit',code,flush=True)
 intact=all(sha(p)==h for p,h in protected.items());(R/'state-preservation.json').write_text(json.dumps(dict(live_state_unchanged=intact,exit_code=code),indent=2)+'\n')
 raise SystemExit(code if intact else 1)
if sys.argv[2]=='launch':
 parser=argparse.ArgumentParser(description='Launch a new private review with the current world and optional actual native archived saves.')
 parser.add_argument('--resume-from',type=Path,help='Cleanly closed archived private review below this candidate review directory.')
 parser.add_argument('--profile',type=Path,help='Absolute profiler output prefix below the new review directory; enables stationary phase/GPU timing without the debug preload.')
 parser.add_argument('--clean-performance',action='store_true',help='Use the ordinary input-enabled shipping path without GL debug preload.')
 parser.add_argument('--resume-render-compatible',action='store_true',help='Allow a verified renderer-only update with identical gameplay/save sources; still requires normal Continue.')
 parser.add_argument('--shadow-fetch',choices=('scalar','gather'),help='Private profiled A/B only: explicitly select the existing shadow-fetch shader path after launcher environment filtering.')
 args=parser.parse_args(sys.argv[3:]);launch(args.resume_from,args.profile,args.clean_performance,args.resume_render_compatible,args.shadow_fetch)
else:
 spec=importlib.util.spec_from_file_location('native_controls',N.with_name('camp-props-2026-11-22')/'tools/live_controls.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m);m.R=R;m.Q=Q
 if sys.argv[2]=='record':
  sec=float(sys.argv[4]);assert 0<sec<=30
  x,y,w,h=m.control();target=R/(sys.argv[3]+'.mp4');assert not target.exists()
  subprocess.run(['ffmpeg','-nostdin','-v','error','-f','x11grab','-framerate','30','-video_size',f'{w}x{h}','-i',f'{os.environ.get("DISPLAY",":0")}+{x},{y}','-t',str(sec),'-vf','scale=1280:-2','-c:v','libx264','-preset','fast','-crf','20','-pix_fmt','yuv420p',str(target)],check=True);print(target)
 else:sys.argv=sys.argv[1:];m.main()
