from pathlib import Path
import importlib.util,json,os,shutil,subprocess,sys,hashlib
N=Path(__file__).resolve().parents[1];E=N.parents[1]
kind=sys.argv[1] if len(sys.argv)>1 else 'candidate';run=sys.argv[2] if len(sys.argv)>2 else kind
R=N/'review'/run;R.mkdir(exist_ok=True);Q=R/'project';Q.mkdir(exist_ok=False)
B=N.with_name('demo-readiness-2026-11-21') if kind=='baseline' else N
P=E/'projects/valwood_current' if kind=='baseline' else N/'world-project'
for f in P.iterdir():
 if f.name in ['saves','shaders']:continue
 if f.name=='scenery':shutil.copytree(f,Q/f.name,symlinks=False)
 elif f.is_dir():(Q/f.name).symlink_to(f.resolve(),target_is_directory=True)
 else:shutil.copy2(f,Q/f.name,follow_symlinks=True)
(Q/'saves').mkdir();shutil.copy2(E/'projects/valwood_current/saves/opening.progress',Q/'saves/opening.progress');(Q/'shaders').symlink_to(B/'shared/shaders',target_is_directory=True)
g=Q/'graphics.settings';v=g.read_text();v=v.replace('auto_quality=1','auto_quality=0');g.write_text(v)
if os.environ.get('VALWOOD_REVIEW_ENVIRONMENT_TEXTURES'):
 subprocess.run([str(N/'tools/set_private_environment_tier'),str(g),os.environ['VALWOOD_REVIEW_ENVIRONMENT_TEXTURES']],check=True)
spec=importlib.util.spec_from_file_location('review_launch',B/'launch.py');L=importlib.util.module_from_spec(spec);spec.loader.exec_module(L)
mode=os.environ.get('VALWOOD_REVIEW_MODE','demo')
cmd,env,_=L.private_configuration(mode);cmd[cmd.index('--project')+1]=str(Q);cmd += ['--profile',str(R/'profile'),'--frames',os.environ.get('VALWOOD_REVIEW_FRAMES','100000')]
if os.environ.get('VALWOOD_REVIEW_CAPTURE'):cmd += ['--capture',str(R/'frame.bmp')]
extra=json.loads(os.environ.get('VALWOOD_REVIEW_EXTRA','[]'));assert isinstance(extra,list) and all(isinstance(v,str) for v in extra);cmd+=extra
env['VALWOOD_QA_GRAPHICS_FILE']=str(g);env['SDL_GAMECONTROLLER_IGNORE_DEVICES']='0x046d/0xc21d'
if os.environ.get('VALWOOD_REVIEW_GL_DEBUG'):env['LD_PRELOAD']=str(N/'tools/probe_gl_debug.so')
if os.environ.get('VALWOOD_REVIEW_SHADER_BANK'):
 bank=Path(os.environ['VALWOOD_REVIEW_SHADER_BANK']).resolve()
 assert bank.is_dir() and bank.is_relative_to(N/'review'),'Diagnostic shaders must be private review data'
 env['VALWOOD_SHADER_BANK_ROOT']=str(bank)
if os.environ.get('VALWOOD_REVIEW_NO_PRESENTATION'):env['VALWOOD_DEMO_PRESENTATION']='0'
if os.environ.get('VALWOOD_REVIEW_ROUTE'):
 env.update(VALWOOD_QA_SOURCE_ROUTE=os.environ['VALWOOD_REVIEW_ROUTE'],VALWOOD_QA_SOURCE_ROUTE_REPORT=str(R/'route'),VALWOOD_QA_SOURCE_ROUTE_CAPTURE=str(R/'shot'))
 cmd+=['--benchmark']
shader_bank=Path(env['VALWOOD_SHADER_BANK_ROOT'])
(R/'launch.json').write_text(json.dumps(dict(command=cmd,binary_sha256=hashlib.sha256(Path(cmd[0]).read_bytes()).hexdigest(),shader_sha256={str(p.relative_to(shader_bank)):hashlib.sha256(p.read_bytes()).hexdigest() for p in shader_bank.glob('*') if p.is_file()},environment={k:v for k,v in env.items() if k.startswith('VALWOOD_')},private_project=str(Q)),indent=2)+'\n')
with (R/'game.log').open('w') as out:
 p=subprocess.Popen(cmd,env=env,cwd=E,stdout=out,stderr=subprocess.STDOUT);(R/'pid').write_text(str(p.pid));print('PID',p.pid,flush=True);rc=p.wait();print('exit',rc,flush=True);sys.exit(rc)
