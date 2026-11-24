"""Snapshot user state or verify the normal launchers after activation."""
from pathlib import Path
import argparse,json,subprocess,sys
N=Path(__file__).resolve().parents[1];ROOT=N.parents[2];P=N.with_name('thomas-hair-coverage-2026-11-21')
sys.dont_write_bytecode=True;sys.path.insert(0,str(N))
from check_unified import need,read,sha
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--snapshot',action='store_true');a=p.parse_args()
if a.snapshot:
    paths=read(P/'review/USER_STATE_BEFORE_PUBLICATION.json')['files_sha256']
    result={'files_sha256':{path:sha(path) for path in paths}}
    (N/'review/USER_STATE_BEFORE_PUBLICATION.json').write_text(json.dumps(result,indent=2)+'\n')
    print('Captured current user state:',len(paths),'files')
else:
    need(read(ROOT/'CURRENT_VALWOOD.json')['runtime_generation']==str(N),'Animation update is not active')
    checks={}
    for mode in ('demo','editor','world','arena','joust'):
        r=subprocess.run([str(ROOT/'Launch_Valwood.sh'),mode,'--check-only'],cwd=ROOT,text=True,capture_output=True)
        need(r.returncode==0,'Launcher failed: '+mode+' '+r.stdout+r.stderr);checks[mode]=r.stdout.strip()
    r=subprocess.run([str(ROOT/'Launch_Valwood.sh'),'world','--print-command'],cwd=ROOT,text=True,capture_output=True,check=True)
    world=json.loads(r.stdout)
    need(world['command'][0]==str(N/'runtime/rpg') and world['environment']['VALWOOD_SHADER_BANK_ROOT']==str(N/'shared/shaders'),'Normal game selects another payload')
    before=read(N/'review/USER_STATE_BEFORE_PUBLICATION.json')['files_sha256']
    changed=[path for path,h in before.items() if sha(path)!=h];need(not changed,'User state changed: '+str(changed))
    settings=[line for line in (N/'shared/graphics.settings').read_text().splitlines() if line.startswith('character_textures=')]
    result=dict(passed=True,launch_modes=checks,world_configuration=world,user_state_files_unchanged=len(before),files_sha256=before,saved_character_detail=settings,publication_sha256=sha(N/'PUBLICATION.json'))
    (N/'review/POST_PUBLICATION_CHECK.json').write_text(json.dumps(result,indent=2)+'\n')
    print('Verified all 5 launch modes and',len(before),'unchanged user state files;',','.join(settings))
