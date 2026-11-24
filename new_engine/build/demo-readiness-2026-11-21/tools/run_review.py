from pathlib import Path
import importlib.util, json, os, shutil, subprocess, sys
N=Path(__file__).resolve().parents[1];E=N.parents[1];R=N/'review/native-ui-v3';R.mkdir(exist_ok=True)
spec=importlib.util.spec_from_file_location('candidate_launch',N/'launch.py');L=importlib.util.module_from_spec(spec);spec.loader.exec_module(L)
cmd,env,P=L.private_configuration('demo')
Q=R/'project';Q.mkdir(exist_ok=False)
for f in Path(P).iterdir():
 if f.name in ('saves','shaders'):continue
 if f.is_dir():(Q/f.name).symlink_to(f.resolve(),target_is_directory=True)
 else:shutil.copy2(f,Q/f.name,follow_symlinks=True)
(Q/'saves').mkdir();shutil.copy2(E/'projects/valwood_current/saves/opening.progress',Q/'saves/opening.progress');(Q/'shaders').symlink_to(N/'shared/shaders',target_is_directory=True)
# Route only the derived camp cache through an owned private model directory.
(Q/'models').unlink();(Q/'models').mkdir()
for item in (Path(P)/'models').iterdir():
 if item.name!='camp_atmosphere':(Q/'models'/item.name).symlink_to(item.resolve(),target_is_directory=item.is_dir())
camp=Q/'models/camp_atmosphere';camp.mkdir()
for item in (Path(P)/'models/camp_atmosphere').iterdir():
 if item.name!='texture_blocks':(camp/item.name).symlink_to(item.resolve(),target_is_directory=item.is_dir())
(camp/'texture_blocks').symlink_to(N/'presentation/camp_models/texture_blocks',target_is_directory=True)
env['SDL_GAMECONTROLLER_IGNORE_DEVICES']='0x046d/0xc21d'
cmd[cmd.index('--project')+1]=str(Q)
g=Q/'graphics.settings';s=g.read_text().replace('auto_quality=1','auto_quality=0').replace('render_scale=1\n','render_scale=0.75\n').replace('min_render_scale=1\n','min_render_scale=0.75\n');g.write_text(s)
env['VALWOOD_QA_GRAPHICS_FILE']=str(g)
(R/'launch.json').write_text(json.dumps(dict(command=cmd,environment={k:v for k,v in env.items() if k.startswith('VALWOOD_')},private_project=str(Q)),indent=2)+'\n')
with (R/'game.log').open('w') as out:
 p=subprocess.Popen(cmd,env=env,cwd=E,stdout=out,stderr=subprocess.STDOUT)
 (R/'pid.txt').write_text(str(p.pid)+'\n');print('Native candidate PID',p.pid,flush=True);rc=p.wait();print('Native candidate exit',rc,flush=True)
 sys.exit(rc)
