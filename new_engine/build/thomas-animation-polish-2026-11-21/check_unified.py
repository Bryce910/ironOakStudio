"""Verify the owned application closure and retained character/hair payloads."""
from pathlib import Path
import argparse,hashlib,importlib.util,json,subprocess,sys
N=Path(__file__).resolve().parent;P=N.with_name('thomas-hair-coverage-2026-11-21');B=N.with_name('thomas-character-finish-2026-11-21');E=N.parents[1]
sys.dont_write_bytecode=True
def need(ok,message):
    if not ok:raise RuntimeError(message)
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def read(p):return json.loads(Path(p).read_text())
def load(name,path):
    spec=importlib.util.spec_from_file_location(name,path);m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m);return m
def owned(p):
    p=Path(p);need(p.is_file() and not p.is_symlink() and all(not parent.is_symlink() for parent in p.parents if parent.is_relative_to(N)),'Payload must be owned: '+str(p));return p
def verify_inputs():
    baseline=read(N/'BASELINE.json')
    need(baseline['baseline']==str(P) and baseline['baseline_ready_sha256']==sha(P/'UNIFIED_READY.json') and baseline['baseline_final_review_sha256']==sha(P/'review/FINAL_REVIEW.json'),'Accepted hair baseline changed')
    need(baseline['source_baseline']==str(B) and baseline['source_baseline_ready_sha256']==sha(B/'UNIFIED_READY.json'),'Accepted application baseline changed')
    r=subprocess.run([sys.executable,str(P/'check_unified.py')],capture_output=True,text=True,cwd=E)
    need(r.returncode==0,'Inherited character/hair validation failed: '+r.stdout+r.stderr)
    builder=load('animation_integrity_builder',N/'build_native_candidate.py')
    source=N/'source-v1';need(source.is_dir() and not source.is_symlink(),'Animation source must be private')
    for p in source.rglob('*'):
        need(not p.is_symlink(),'Private source contains an alias: '+str(p))
        prior=B/'source-v1'/p.relative_to(source)
        if p.is_file() and prior.is_file():need(p.stat().st_ino!=prior.stat().st_ino,'Private source hardlinks baseline')
    scope=builder.reviewed_build_inputs();native=N/'runtime-native-v1'
    ready=read(owned(native/'READY.json'));plan=read(owned(native/'BUILD_PLAN.json'))
    need(ready.get('ready') is True and ready.get('application_units')==104 and len(plan['units'])==104 and ready.get('full_application_dependency_closure_verified') is True,'Application dependency closure incomplete')
    binary=owned(native/'native-bin/rpg');need(sha(binary)==ready['sha256'] and (N/'runtime/rpg').resolve()==binary,'Runtime selects another application')
    source_inventory=builder.inventory(source);need(source_inventory==ready['source_sha256']==plan['source_sha256'],'Compiled source changed')
    for key,value in scope.items():need(ready.get(key)==value and plan.get(key)==value,'Reviewed source scope changed: '+key)
    for row in plan['units']:
        obj=owned(row['target']);receipt=read(owned(obj.with_suffix('.json')))
        need(sha(obj)==ready['object_sha256'][str(obj)] and receipt['command']==row['command'],'Compiled object changed')
        for p,h in receipt['actual_dependency_sha256'].items():need(Path(p).is_file() and sha(p)==h,'Compiled dependency changed: '+p)
    need(sha(builder.LIBRARY)==builder.PHYSICS_SHA,'Pinned physics library changed')
    shaders=builder.inventory(N/'shared/shaders');need(shaders==builder.inventory(P/'shared/shaders'),'Reviewed hair correction changed')
    for p,h in shaders.items():
        need(sha(owned(N/'shared/shaders'/p))==h,'Owned shader bank changed')
        if sha(source/'shaders'/p)!=h:
            # Five accepted material/post shaders already differ between the
            # baseline's archival source tree and its reviewed runtime bank.
            # Retain both verified inputs rather than rewrite unrelated code.
            need(sha(source/'shaders'/p)==sha(B/'source-v1/shaders'/p) and sha(B/'shared/shaders'/p)==h,'Unexpected source/runtime shader difference: '+p)
    launcher=load('animation_integrity_launcher',N/'launch.py')
    for mode in launcher.MODES:
        command,env,project=launcher.configuration(mode);_,old_env,old_project=launcher.base.configuration(mode)
        need(command[0]==str(N/'runtime/rpg') and env['VALWOOD_SHADER_BANK_ROOT']==str(N/'shared/shaders'),'Mode selects another runtime/shader bank')
        need(project==old_project and env.get('VALWOOD_CHARACTER_QUALITY_ROOT')==old_env.get('VALWOOD_CHARACTER_QUALITY_ROOT'),'State/model selection changed')
    paths=[N/p for p in ('BASELINE.json','SOURCE_ORIGIN.json','GUIDE.txt','launch.py','check_unified.py','build_native_candidate.py')]+sorted((N/'tools').glob('*.py'))
    return dict(binary_sha256=sha(binary),source_sha256=source_inventory,shader_sha256=shaders,native_ready_sha256=sha(native/'READY.json'),baseline_ready_sha256=sha(P/'UNIFIED_READY.json'),payload_sha256={str(p.relative_to(N)):sha(owned(p)) for p in paths})
def check():
    ready=read(N/'UNIFIED_READY.json');need(ready.get('ready') is True and ready.get('generation')==str(N),'Animation update is not frozen')
    actual=verify_inputs();need(all(ready.get(k)==v for k,v in actual.items()),'Animation payload changed after verification');return ready
if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--inputs-only',action='store_true');a=p.parse_args()
    try:
        verify_inputs() if a.inputs_only else check();print('Animation update: owned 104-unit application, inherited model tiers and hair verified.')
    except Exception as e:print(str(e),file=sys.stderr);sys.exit(1)
