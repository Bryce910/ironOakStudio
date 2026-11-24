"""cap.py OUT [extra runtime args]  env: CAP_GEN (generation dir), CAP_BANK (shader bank, default gen's), CAP_MODE (world), CAP_FRAMES (180), CAP_PROFILE=1 for --profile, CAP_STUDIO=path to alternate world.studio"""
import os,sys,subprocess,importlib.util,shutil
from pathlib import Path
S=Path(os.environ.get('CAP_WORK','/tmp/valwood-qa'));S.mkdir(parents=True,exist_ok=True)
G=Path(os.environ.get('CAP_GEN',str(Path(__file__).resolve().parents[1])))
CAND=Path('/home/computer/Desktop/final_engine/new_engine/build/aaa-lighting-2026-11-20')
bank=Path(os.environ.get('CAP_BANK',G/'shared/shaders'))
out=os.path.abspath(sys.argv[1]);extra=sys.argv[2:]
spec=importlib.util.spec_from_file_location('L',(G if (G/'launch.py').exists() else CAND)/'launch.py');L=importlib.util.module_from_spec(spec);spec.loader.exec_module(L)
mode=os.environ.get('CAP_MODE','world')
cmd,env,project=L.configuration(mode,extra)
cmd[0]=str(G/'runtime/rpg') if (G/'runtime/rpg').exists() else cmd[0]
# private qa copy of the project, own shaders + saves
qa=S/('qa_'+Path(project).name+'_'+G.name+'_'+bank.name)
if not qa.exists():
    qa.mkdir()
    for f in Path(project).iterdir():
        if f.name in('shaders','saves'):continue
        (qa/f.name).symlink_to(f.resolve() if f.is_symlink() else f)
    (qa/'saves').mkdir()
sh=qa/'shaders'
if sh.is_symlink():sh.unlink()
sh.symlink_to(bank)
sv=qa/'showcase.views'
if sv.is_symlink() or sv.exists():sv.unlink()
if os.environ.get('CAP_VIEW'):
    views={l.split()[0]:l for l in open(Path(project)/'showcase.views') if l.strip()}
    v=os.environ['CAP_VIEW'];v=' '.join(views[v].split()[1:]) if v in views else v
    sv.write_text('city-town '+v+'\n');extra=list(extra)+['--scenic','city-town']
    cmd+=['--scenic','city-town']
else:sv.symlink_to(Path(project)/'showcase.views')
if os.environ.get('CAP_STUDIO'):
    env['VALWOOD_QA_STUDIO_DOCUMENT']=os.environ['CAP_STUDIO']
i=cmd.index('--project');cmd[i+1]=str(qa)
for k,v in os.environ.items():
    if k.startswith('VALWOOD_'):env[k]=v
cmd+=['--frames',os.environ.get('CAP_FRAMES','180')]
cmd+=(['--warmup','150','--profile',out] if os.environ.get('CAP_PROFILE') else ['--capture',out])
r=subprocess.run(cmd,env=env,cwd='/home/computer/Desktop/final_engine/new_engine',capture_output=True,text=True)
open(out+'.log','w').write(r.stdout+'\n----\n'+r.stderr)
print('rc',r.returncode)
for l in (r.stdout+r.stderr).splitlines():
    if any(k in l for k in('[Far shadow]','[Exposure]','[Motion]','error','Error','ERROR','failed','GL_')):print(' ',l[:240])
