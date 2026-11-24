"""Verify the owned vegetation runtime, retained shaders, and saved-world routing."""
from pathlib import Path
import argparse, hashlib, importlib.util, json, sys
N=Path(__file__).resolve().parent
B=N.with_name('thomas-animation-polish-2026-11-21')
sys.dont_write_bytecode=True
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def read(p):return json.loads(Path(p).read_text())
def need(ok,message):
    if not ok:raise RuntimeError(message)
def load(name,path):
    s=importlib.util.spec_from_file_location(name,path);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m
def verify_inputs():
    builder=load('vegetation_integrity_builder',N/'build_native_candidate.py')
    inputs=builder.reviewed_build_inputs()
    native=N/'runtime-native-v1';ready=read(native/'READY.json');plan=read(native/'BUILD_PLAN.json')
    need(ready['ready'] and ready['application_units']==104 and len(plan['units'])==104,'Incomplete application closure')
    binary=native/'native-bin/rpg'
    need(binary.is_file() and not binary.is_symlink() and sha(binary)==ready['sha256'] and (N/'runtime/rpg').resolve()==binary,'Wrong vegetation executable')
    source=builder.inventory(N/'source-v1')
    need(source==ready['source_sha256']==plan['source_sha256'],'Source changed after build')
    for k,v in inputs.items():need(ready[k]==v and plan[k]==v,'Reviewed build inputs changed: '+k)
    for row in plan['units']:
        obj=Path(row['target']);receipt=read(obj.with_suffix('.json'))
        need(obj.is_relative_to(native) and sha(obj)==ready['object_sha256'][str(obj)] and receipt['command']==row['command'],'Compiled object changed')
        for p,h in receipt['actual_dependency_sha256'].items():need(Path(p).is_file() and sha(p)==h,'Compiled dependency changed: '+p)
    shaders=builder.inventory(N/'shared/shaders')
    need(shaders==builder.inventory(B/'shared/shaders'),'Accepted character/hair shader bank changed')
    baseline=read(N/'BASELINE.json')
    need(baseline['baseline']==str(B) and baseline['baseline_ready_sha256']==sha(B/'UNIFIED_READY.json') and baseline['binary_sha256']==sha(B/'runtime/rpg'),'Accepted baseline changed')
    launcher=load('vegetation_integrity_launcher',N/'launch.py')
    for mode in launcher.MODES:
        command,env,project=launcher.configuration(mode);_,old_env,old_project=launcher.base.configuration(mode)
        need(command[0]==str(N/'runtime/rpg') and env['VALWOOD_SHADER_BANK_ROOT']==str(N/'shared/shaders'),'Mode uses a different runtime/shader bank')
        need(project==old_project and env.get('VALWOOD_CHARACTER_QUALITY_ROOT')==old_env.get('VALWOOD_CHARACTER_QUALITY_ROOT'),'Saved-world/model routing changed')
        need(bool(env.get('VALWOOD_VEGETATION_DRESSING'))==(mode in ('demo','editor','world')),'Incorrect vegetation overlay mode')
    for area in launcher.AREAS:
        _,env,_=launcher.configuration('world',['--area',area]);need('VALWOOD_VEGETATION_DRESSING' not in env,'Main-world overlay leaked into another area')
    payload={str(p.relative_to(N)):sha(p) for p in [N/'launch.py',N/'check_unified.py',N/'build_native_candidate.py',N/'GUIDE.txt',N/'vegetation/creek_dressing_instances.txt']}
    return dict(binary_sha256=sha(binary),source_sha256=source,shader_sha256=shaders,native_ready_sha256=sha(native/'READY.json'),baseline_ready_sha256=sha(B/'UNIFIED_READY.json'),payload_sha256=payload)
def check():
    ready=read(N/'UNIFIED_READY.json');need(ready.get('ready') is True and ready.get('generation')==str(N),'Vegetation update is not reviewed')
    need(ready.get('final_review_sha256')==sha(N/'review/FINAL_REVIEW.json'),'Final review changed');actual=verify_inputs();need(all(ready.get(k)==v for k,v in actual.items()),'Vegetation payload changed after review');return ready
if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--inputs-only',action='store_true');a=p.parse_args()
    try:verify_inputs() if a.inputs_only else check();print('Vegetation tiers: owned 104-unit application, retained character/shaders and saved-world routing verified.')
    except Exception as e:print(str(e),file=sys.stderr);sys.exit(1)
