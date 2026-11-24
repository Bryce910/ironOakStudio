"""Verify the shader-only hair patch and its frozen application dependency."""
from pathlib import Path
import argparse,hashlib,importlib.util,json,subprocess,sys
N=Path(__file__).resolve().parent;B=N.with_name('thomas-character-finish-2026-11-21');E=N.parents[1]
sys.dont_write_bytecode=True

def need(ok,message):
    if not ok:raise RuntimeError(message)
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def read(p):return json.loads(Path(p).read_text())
def inventory(root):return {str(p.relative_to(root)):sha(p) for p in sorted(root.rglob('*')) if p.is_file()}
def verify_inputs():
    baseline=read(N/'BASELINE.json')
    need(baseline['baseline']==str(B) and baseline['baseline_ready_sha256']==sha(B/'UNIFIED_READY.json') and baseline['baseline_final_review_sha256']==sha(B/'review/FINAL_REVIEW.json'),'Frozen character dependency changed')
    result=subprocess.run([sys.executable,str(B/'check_unified.py')],capture_output=True,text=True,cwd=E)
    need(result.returncode==0,'Frozen application dependency failed: '+result.stdout+result.stderr)
    binary=(N/'runtime/rpg').resolve();need(binary==(B/'runtime/rpg').resolve() and sha(binary)==baseline['binary_sha256'],'Shader patch selects an unverified binary')
    shaders=N/'shared/shaders';need(shaders.is_dir() and not shaders.is_symlink(),'Shader patch must own its shader bank')
    for p in shaders.rglob('*'):need(not p.is_symlink(),'Shader bank aliases a mutable payload')
    before,after=inventory(B/'shared/shaders'),inventory(shaders)
    need(set(before)==set(after),'Shader inventory changed')
    changed={p:dict(before=before[p],after=after[p]) for p in after if before[p]!=after[p]}
    need(set(changed)=={'world_foliage.fs.glsl','world_foliage_shadow.fs.glsl'},'Hair patch scope exceeded two material fragments')
    scope=read(N/'review/SHADER_CHANGE_SCOPE.json');need(scope['changes']==changed and scope['candidate_sha256']==after,'Reviewed hair shader scope changed')
    spec=importlib.util.spec_from_file_location('verified_hair_launcher',N/'launch.py');launcher=importlib.util.module_from_spec(spec);spec.loader.exec_module(launcher)
    for mode in launcher.MODES:
        command,env,project=launcher.configuration(mode)
        need(command[0]==str(N/'runtime/rpg') and env['VALWOOD_SHADER_BANK_ROOT']==str(shaders),'Mode does not select hair patch: '+mode)
        if mode!='joust':need(env['VALWOOD_MODULAR_KNIGHT_SHADER']==str(shaders/'world_skeletal.vs.glsl'),'Hero vertex shader not in verified bank')
        original_command,original_env,original_project=launcher.base.configuration(mode)
        need(project==original_project and env.get('VALWOOD_CHARACTER_QUALITY_ROOT')==original_env.get('VALWOOD_CHARACTER_QUALITY_ROOT'),'State/model selection changed')
    paths=[N/'BASELINE.json',N/'launch.py',N/'check_unified.py',N/'GUIDE.txt',*sorted((N/'tools').glob('*.py'))]
    return dict(binary_sha256=sha(binary),baseline_ready_sha256=sha(B/'UNIFIED_READY.json'),baseline_native_ready_sha256=sha(B/'runtime-native-v1/READY.json'),shader_sha256=after,payload_sha256={str(p.relative_to(N)):sha(p) for p in paths})
def check():
    ready=read(N/'UNIFIED_READY.json');need(ready.get('ready') is True and ready.get('generation')==str(N),'Hair patch is not frozen')
    current=verify_inputs();need(all(ready.get(k)==v for k,v in current.items()),'Hair patch changed after verification');return ready
if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--inputs-only',action='store_true');a=p.parse_args()
    try:
        verify_inputs() if a.inputs_only else check();print('Hair coverage: owned shaders and frozen104-unit character dependency verified.')
    except Exception as e:print(str(e),file=sys.stderr);sys.exit(1)
