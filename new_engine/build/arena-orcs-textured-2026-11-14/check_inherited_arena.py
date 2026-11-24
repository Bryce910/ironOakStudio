#!/usr/bin/env python3
"""Pin inherited arena mechanics and the exact new rigid-material opt-in."""
from pathlib import Path
import argparse
import hashlib
import json
import subprocess

STAGE=Path(__file__).resolve().parent
BASELINE=STAGE.parent/'arena-orcs-2026-11-14'
REPORT=STAGE/'opponent-review/INHERITED_ARENA_SOURCE_REVIEW.json'

def sha(path):
    with Path(path).open('rb') as stream:return hashlib.file_digest(stream,'sha256').hexdigest()

def require(value,message):
    if not value:raise RuntimeError(message)

def main():
    parser=argparse.ArgumentParser();parser.add_argument('--check-only',action='store_true');args=parser.parse_args()
    if args.check_only:
        d=json.loads(REPORT.read_text());require(d.get('passed') and d.get('only_exact_rigid_marker_opt_in'),'Inherited arena source review missing')
        require(d['inputs']=={p:sha(p) for p in d['inputs']},'Inherited source/evidence changed')
        print('PASS current inherited arena/source review; new Gnur geometry is not inherited');return
    inputs={}
    def pin(path):
        path=Path(path).resolve();digest=sha(path);inputs[str(path)]=digest;return digest
    old_root=BASELINE/'source-v1';new_root=STAGE/'source-v1'
    old={str(p.relative_to(old_root)):pin(p) for p in old_root.rglob('*') if p.is_file()}
    new={str(p.relative_to(new_root)):pin(p) for p in new_root.rglob('*') if p.is_file()}
    require(old.keys()==new.keys(),'Source file inventory changed beyond the reviewed opt-in')
    changed=[p for p in old if old[p]!=new[p]];require(changed==['engine/world_models.cpp'],'Unexpected source changes: '+repr(changed))
    before=(old_root/changed[0]).read_bytes();after=(new_root/changed[0]).read_bytes()
    needle=b'std::string_view(material->name)=="Owned PowerOrc14 axe"||'
    insertion=needle+b'std::string_view(material->name)=="Owned Unity Orc Gnur weapon"||'
    require(before.count(needle)==1 and after==before.replace(needle,insertion),'C++ delta is not the one exact additive Gnur rigid material marker')
    inherited=[]
    for name in ('OPPONENT_CONFIG_REVIEW.json','SPECIES_CYCLE_CPU_REVIEW.json','HUMAN_WORLD_SMOKE_ACCEPTED.json'):
        path=BASELINE/'opponent-review'/name;pin(path);receipt=json.loads(path.read_text())
        require(receipt.get('passed') is True,'Inherited evidence did not pass: '+name)
        for closure in ('inputs','outputs'):
            for p,expected in receipt.get(closure,{}).items():require(pin(p)==expected,'Inherited evidence is stale: '+p)
        inherited.append({'path':str(path),'sha256':inputs[str(path)],'scope':'arena configuration/save/reward/species isolation or unchanged Human rig; no Gnur geometry/contact inheritance'})
    ready_path=STAGE/'runtime-native-v1/READY.json';pin(ready_path);ready=json.loads(ready_path.read_text())
    require(ready.get('ready') is True,'New native build is not ready')
    for p,expected in ready['source_sha256'].items():require(new.get(p)==expected,'Compiled source dependency mismatch: '+p)
    binary=STAGE/'runtime-native-v1/native-bin/rpg';require(pin(binary)==ready['sha256'],'New native binary hash mismatch')
    launcher=STAGE/'launch_arena_opponents_candidate.sh';pin(launcher);text=launcher.read_text()
    subprocess.run(['bash','-n',str(launcher)],check=True)
    for expected in ('arena_opponents_candidate_dir/arena-project','arena_opponents_candidate_dir/runtime-native-v1/native-bin/rpg',
                     '--project|--project=*|--editor','flock -n 9','VALWOOD_CAPTURE_ARENA_START','VALWOOD_CAPTURE_VIEW','unset VALWOOD_OPENING_PROLOGUE VALWOOD_SHOWCASE',
                     'exec "$arena_opponents_binary" --project "$arena_opponents_project" --play --scenic city-walk'):
        require(expected in text,'Launcher isolation clause missing: '+expected)
    project=STAGE/'arena-project';require(project.is_dir() and not project.is_symlink(),'Candidate project must be private')
    require((project/'arena_orcs').resolve()==(STAGE/'arena_orcs').resolve(),'Candidate routes to a different Orc bank')
    require(not (project/'saves').is_symlink(),'Candidate saves cannot alias another project')
    pin(project/'arena.config');pin(STAGE/'HERO_ASSET.json');pin(Path(__file__).resolve())
    require(inputs=={p:sha(p) for p in inputs},'Source/evidence changed during inherited review')
    result={'schema':'valwood.gnur.inherited-arena-source-review.v1','passed':True,'only_exact_rigid_marker_opt_in':True,
        'source_files_compared':len(new),'source_changes':changed,'source_inventory_sha256':{'baseline':old,'new':new},
        'native_binary_sha256':ready['sha256'],'inherited_evidence':inherited,
        'candidate_launcher':{'path':str(launcher),'syntax_checked':True,'private_project':str(project),'shared_instance_lock':True,
            'external_project_or_editor_override_blocked':True,'qa_and_capture_environment_cleared':True,'forces_orc_run':False,
            'selection_safety':'Uses ordinary setup and saved private selection. Actual Orc assets must validate before an Orc run allocates.'},
        'new_gnur_geometry_contact_or_severing_acceptance':False,'inputs':inputs,
        'limits':['This reuses unchanged arena configuration, save/reward/species mechanics and Human rig evidence. It does not inherit old PowerOrc14 geometry, contacts, textures or severing proof.','New Gnur source/UV/anatomy/contact/severing and native TP/FP checks remain independent gates. No new controller interaction is established.']}
    REPORT.write_text(json.dumps(result,indent=2,sort_keys=True)+'\n')
    print(json.dumps({'passed':True,'source_files_compared':len(new),'source_changes':changed,'native_binary_sha256':ready['sha256']}))

if __name__=='__main__':main()
