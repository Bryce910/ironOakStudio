"""Verify the owned character-quality runtime, source, shaders and model tiers."""
from pathlib import Path
import argparse
import hashlib
import importlib.util
import json
import shlex
import struct
import subprocess
import sys

sys.dont_write_bytecode=True
N=Path(__file__).resolve().parent
E=N.parents[1]
B=N.with_name('thomas-polish-2026-11-21')
TIERS=('Low','Medium','High','Ultra')

def need(ok,message):
    if not ok:raise RuntimeError(message)

def sha(path):
    with Path(path).open('rb') as h:return hashlib.file_digest(h,'sha256').hexdigest()

def read(path):return json.loads(Path(path).read_text())

def owned(path):
    path=Path(path)
    need(path.is_file() and not path.is_symlink() and all(not p.is_symlink() for p in path.parents if p.is_relative_to(N)), 'Candidate payload must be owned: '+str(path))
    return path

def inventory(root):
    return {str(p.relative_to(root)):sha(p) for p in sorted(Path(root).rglob('*')) if p.is_file() and '__pycache__' not in p.parts and p.suffix not in ('.pyc','.pyo')}

def glb(path):
    p=Path(path)
    with p.open('rb') as h:
        magic,version,size=struct.unpack('<4sII',h.read(12));length,kind=struct.unpack('<II',h.read(8))
        need(magic==b'glTF' and version==2 and size==p.stat().st_size and kind==0x4e4f534a and 2<=length<=size-20,'Invalid GLB: '+str(path))
        return json.loads(h.read(length))

def load(name,path):
    spec=importlib.util.spec_from_file_location(name,path);module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module);return module

def payloads():
    paths=[N/p for p in ('BASELINE.json','SOURCE_ORIGIN.json','GUIDE.txt','launch.py','check_unified.py','build_native_candidate.py')]
    paths+=sorted(p for p in (N/'tools').rglob('*') if p.is_file() and p.suffix in ('.py','.cpp') and '__pycache__' not in p.parts)
    paths+=sorted(p for p in (N/'assets/character-quality').rglob('*') if p.is_file())
    paths+=sorted(p for p in (N/'motion').rglob('*') if p.is_file() and not p.is_symlink())
    return {str(owned(p).relative_to(N)):sha(p) for p in paths}

def verify_inputs(*,inherited=True):
    baseline=read(N/'BASELINE.json')
    need(baseline.get('schema')=='valwood.thomas-character-finish.baseline.v1' and baseline.get('baseline')==str(B) and baseline.get('baseline_ready_sha256')==sha(B/'UNIFIED_READY.json'),'Accepted Thomas baseline changed')
    if inherited:
        p=subprocess.run([sys.executable,str(B/'check_unified.py')],cwd=E,capture_output=True,text=True)
        need(p.returncode==0,'Inherited Thomas integration failed: '+p.stdout+p.stderr)
    source=N/'source-v1'
    need(source.is_dir() and not source.is_symlink(),'Character quality requires its own source copy')
    for p in source.rglob('*'):
        need(not p.is_symlink(),'Private source contains alias: '+str(p))
        prior=Path(baseline['source'])/p.relative_to(source)
        if p.is_file() and prior.is_file():need(p.stat().st_ino!=prior.stat().st_ino,'Private source hardlinks inherited file: '+str(p))
    builder=load('quality_integrity_builder',N/'build_native_candidate.py')
    scope=builder.reviewed_build_inputs()
    native=N/'runtime-native-v1'
    ready=read(owned(native/'READY.json'));plan=read(owned(native/'BUILD_PLAN.json'))
    need(ready.get('ready') is True and ready.get('application_units')==104 and len(plan['units'])==104 and ready.get('full_application_dependency_closure_verified') is True,'Native 104-unit closure is incomplete')
    binary=owned(native/'native-bin/rpg')
    need(sha(binary)==ready['sha256'] and (N/'runtime/rpg').is_symlink() and (N/'runtime/rpg').resolve()==binary,'Runtime must select the owned compiled binary')
    source_inventory=inventory(source)
    need(source_inventory==ready['source_sha256']==plan['source_sha256'],'Compiled source snapshot changed')
    for key,value in scope.items():need(ready.get(key)==value and plan.get(key)==value,'Compiled source/shader scope changed: '+key)
    need(sha(builder.LIBRARY)==builder.PHYSICS_SHA,'Pinned native Jolt library changed')
    for row in plan['units']:
        obj=owned(row['target']);receipt=read(owned(obj.with_suffix('.json')))
        need(sha(obj)==ready['object_sha256'][str(obj)] and receipt['command']==row['command'],'Compiled object changed: '+row['logical_source'])
        for p,h in receipt['actual_dependency_sha256'].items():need(Path(p).is_file() and sha(p)==h,'Compiled dependency changed: '+p)
    tiers={}
    quality_build=read(owned(N/'assets/character-quality/QUALITY_BUILD.json'))
    need(quality_build.get('schema')=='valwood.character-quality.v1','Character quality build receipt is missing')
    validation=read(owned(N/'assets/character-quality/QUALITY_VALIDATION.json'))
    need(validation.get('schema')=='valwood.character-quality.validation.v1' and validation.get('passed') is True,'Character quality structural/texture validation did not pass')
    for path,h in quality_build['source_inputs'].items():
        need(Path(path).is_file() and sha(path)==h,'Character quality source changed: '+path)
    for tier in TIERS:
        tiers[tier]={}
        for outfit in ('civilian','armored'):
            path=N/'assets/character-quality'/tier/'story_hero'/outfit
            body=owned(path/'body.glb');cfg=owned(path/'appearance.cfg')
            need('body "body.glb"' in cfg.read_text(),'Appearance selects wrong tier body: '+str(cfg))
            doc=glb(body)
            need(doc.get('meshes') and doc.get('skins') and not doc.get('animations'),'Tier must retain skinned Thomas geometry and inherited motion: '+str(body))
            triangles=sum(doc['accessors'][p['indices']]['count']//3 for m in doc['meshes'] for p in m['primitives'])
            tiers[tier][outfit]=dict(path=str(body),triangles=triangles,sha256=sha(body))
            record=quality_build['tiers'][tier][outfit]
            recorded=Path(record['file'])
            if not recorded.is_absolute():recorded=N/recorded
            need(recorded==body and record['sha256']==sha(body) and record['triangles']==triangles,'Model tier differs from build receipt: '+tier+'/'+outfit)
            validated=validation['bodies'][tier+'/'+outfit]
            need(validated['triangles']==triangles and validated['file_bytes']==body.stat().st_size and validated['conservative_gpu_rgba8_with_mips_bytes']<=validated['native_profile_texture_budget_bytes'],'Validated model tier/budget changed: '+tier+'/'+outfit)
    for outfit in ('civilian','armored'):
        values=[tiers[t][outfit]['triangles'] for t in TIERS]
        need(values[0]<values[1]<values[2]<=values[3],'Geometry tiers must increase to full detail: '+outfit)
    hero=read(N/'motion/rigs/HERO_ASSET.json');rig=Path(hero['path'])
    need(rig.is_file() and sha(rig)==hero['sha256'],'Inherited Kubold motion bank changed')
    motion=read(owned(N/'motion/UNARMED_MOTION_BANK.json'))
    need(motion.get('passed') is True and motion['sidecar']['clips']==22,'Captured locomotion bank is incomplete')
    sidecar=owned(hero['unarmed_transition_sidecar'])
    need(sidecar==Path(motion['sidecar']['path']) and sha(sidecar)==hero['unarmed_transition_sha256']==motion['sidecar']['sha256'],'Captured locomotion sidecar changed')
    for path,h in motion['outputs'].items():need(sha(owned(path))==h,'Motion bank output changed: '+path)
    for row in motion['existing_banks']:
        need(row['existing_bank_byte_identical'] and sha(owned(row['path']))==row['sha256']==sha(row['baseline_path']),'Existing motion bank changed')
    composition=read(N/'review/CHARACTER_ASSET_COMPOSITION.json')
    need(composition.get('passed') is True and len(composition['rows'])==8,'Face and hair composition proof is missing')
    need(composition['facial_bank_sha256']==sha(N/'review/facial-assets/FACIAL_BANK.json') and composition['hair_recipe_sha256']==sha(N/'review/hair-assets/HAIR_RECIPE.json'),'Reviewed face/hair recipe changed')
    for row in composition['rows'].values():need(sha(owned(row['file']))==row['sha256'],'Composed character body changed')
    launch=load('quality_integrity_launcher',N/'launch.py')
    shaders=inventory(N/'shared/shaders')
    for mode in launch.MODES:
        command,env,project=launch.configuration(mode)
        need(command[0]==str(N/'runtime/rpg'),'Mode selects another binary: '+mode)
        need(env.get('VALWOOD_SHADER_BANK_ROOT')==str(N/'shared/shaders'),'Mode selects another generation shader bank: '+mode)
        if mode in ('demo','editor','world'):
            need(env.get('VALWOOD_CHARACTER_QUALITY_ROOT')==str(N/'assets/character-quality'),'Mode does not select character tier bank: '+mode)
            need('VALWOOD_STORY_CIVILIAN_APPEARANCE' not in env and 'VALWOOD_STORY_ARMORED_APPEARANCE' not in env,'Launcher must leave tier resolution to loaded settings')
        if mode!='joust':need(env['VALWOOD_MODULAR_KNIGHT']==str(rig),'Mode selects another motion bank: '+mode)
    return dict(binary_sha256=sha(binary),source_sha256=source_inventory,payload_sha256=payloads(),shader_sha256=shaders,
                model_tiers=tiers,baseline_ready_sha256=sha(B/'UNIFIED_READY.json'),native_ready_sha256=sha(native/'READY.json'))

def check():
    ready=read(N/'UNIFIED_READY.json')
    need(ready.get('ready') is True and ready.get('generation')==str(N) and ready.get('schema')=='valwood.thomas-character-finish.ready.v1','Character quality generation is not frozen')
    current=verify_inputs()
    need(all(ready.get(k)==v for k,v in current.items()),'Character quality inputs changed after verification; rerun QA and freeze')
    return ready

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--inputs-only',action='store_true');a=p.parse_args()
    try:
        verify_inputs() if a.inputs_only else check()
        print('Thomas character finish: owned 104-unit runtime, facial morphs, captured motion, hair and saved tiers verified.')
    except Exception as e:print(str(e),file=sys.stderr);sys.exit(1)
