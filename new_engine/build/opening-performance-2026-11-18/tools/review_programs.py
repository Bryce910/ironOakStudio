from pathlib import Path
import hashlib,json,os,shutil,subprocess,sys,time
N=Path(__file__).resolve().parents[1];E=N.parents[1];sys.path.insert(0,str(N))
from launch import configuration
D=N/'review/programs';P=D/'private-project';D.mkdir(exist_ok=True);P.mkdir(exist_ok=True)
cmd,env,origin=configuration('arena')
for f in origin.iterdir():
    if f.name in ('saves','studio-layout.ini','world.studio.workspace','player-profile.config'):continue
    target=P/f.name
    if target.exists() or target.is_symlink():continue
    if f.is_dir():target.symlink_to(f.resolve(),target_is_directory=True)
    else:shutil.copy2(f,target)
(P/'saves').mkdir(exist_ok=True)
(P/'player-profile.config').write_text('VALWOOD_PLAYER_PROFILE 1\npath saves/player.progress\nend\n')
cmd[cmd.index('--project')+1]=str(P)
cmd+=['--hidden','--frames','90','--warmup','30','--profile',str(D/'performance.json')]
env.update(VALWOOD_QA_CHARACTER_PROGRAMS='1',SDL_AUDIODRIVER='dummy',MESA_DEBUG='1',SDL_GAMECONTROLLER_IGNORE_DEVICES='0x046d/0xc21d')
started=time.monotonic()
with (D/'native.log').open('w') as f:r=subprocess.run(cmd,cwd=E,env=env,stdout=f,stderr=subprocess.STDOUT,timeout=240)
log=(D/'native.log').read_text();world=json.loads((D/'performance.json.world.json').read_text())
passed=r.returncode==0 and '[Character program QA] PASS:11 unique owners' in log and world['gl_error']==0
report=dict(passed=passed,binary_sha256=hashlib.sha256(Path(cmd[0]).read_bytes()).hexdigest(),
    elapsed_seconds=time.monotonic()-started,exit_code=r.returncode,gl_error=world['gl_error'],
    native_GPU=True,shared_editor_shader_reload_code=True,editor_UI_tested=False,private_saves=True,
    tests='Successful shader commit, post-link rollback, rejected link, moved-from cleanup, owned grass child and crowd isolation')
(D/'EXECUTION.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report));assert passed
