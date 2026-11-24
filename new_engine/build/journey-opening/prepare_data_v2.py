#!/usr/bin/env python3
"""Fresh data-only revision fixing required base references; never edits V1."""
from pathlib import Path
import copy,hashlib,json,shutil,sys

R=Path(__file__).resolve().parents[2];B=Path(__file__).resolve().parent
OLD=B/'candidate-v1';NEW=B/'candidate-v2';SOURCE=OLD/'project';P=NEW/'project'
sys.path.insert(0,str(R/'tools'))
from check_opening_prologue import mutable_snapshot
def sha(p):
    with Path(p).open('rb')as f:return hashlib.file_digest(f,'sha256').hexdigest()
def dump(p,d):
    with p.open('x')as f:json.dump(d,f,indent=2);f.write('\n')
def clone(source,dest):
    if source.is_symlink():dest.symlink_to(source.resolve(strict=True),target_is_directory=source.is_dir())
    elif source.is_dir():
        dest.mkdir()
        for p in sorted(source.iterdir()):clone(p,dest/p.name)
    elif source.is_file():shutil.copyfile(source,dest)
    else:raise ValueError(str(source))

def main():
    assert not NEW.exists();before=mutable_snapshot(SOURCE);old_origin_sha=sha(OLD/'local-origin.json')
    ready_path=B/'runtime-v1/READY.json';ready=json.loads(ready_path.read_text());assert ready['ready']
    for key in ['native','windows']:assert sha(Path(ready[key]))==ready[key+'_sha256']
    NEW.mkdir();P.mkdir()
    for p in sorted(SOURCE.iterdir()):
        if p.name not in {'models','saves'}:clone(p,P/p.name)
    (P/'saves').mkdir();models=P/'models';models.mkdir();links={}
    for p in sorted((SOURCE/'models').iterdir()):
        target=p.resolve(strict=True);(models/p.name).symlink_to(target,target_is_directory=target.is_dir());links[p.name]=str(target)
    corrections=[]
    for stem in ['purchased_village_crate','purchased_prop_sack','purchased_prop_lamp']:
        new=models/(stem+'.glb');near=models/(stem+'_near.glb');assert not new.exists()and not new.is_symlink()and near.is_file()
        target=near.resolve(strict=True);new.symlink_to(target);links[new.name]=str(target)
        corrections.append(dict(base=str(new),target=str(target),sha256=sha(target),reason='Static scene requires stem.glb for bounds before LOD selection; exact existing near payload reused.'))
    references=[]
    for line in (P/'world.studio').read_text().splitlines():
        fields=line.split()
        if len(fields)>3 and fields[0]=='asset'and fields[1].startswith('nm_journey_'):
            suffixes=['.glb']+(['_near.glb','_mid.glb']if int(fields[3])==2 else[])
            for suffix in suffixes:
                path=models/(fields[2]+suffix);assert path.is_file(),str(path)
                references.append(dict(alias=fields[1],suffix=suffix,path=str(path),resolved=str(path.resolve()),sha256=sha(path)))
    assert len(references)==16
    assert before==mutable_snapshot(SOURCE)and old_origin_sha==sha(OLD/'local-origin.json')
    generated={str(p.relative_to(P)):sha(p)for p in sorted(P.rglob('*'))if p.is_file()and not p.is_symlink()and str(p.relative_to(P))not in ['graphics.settings','studio-layout.ini']}
    origin=copy.deepcopy(json.loads((OLD/'local-origin.json').read_text()))
    origin.update(schema='valwood.journey-opening.private-candidate',data_revision=2,project=str(P),source_project=str(SOURCE),source_origin_sha256=old_origin_sha,source_guard=before,source_unchanged=True,source_saves_copied=False,private_saves_empty=True,generated_sha256=generated,model_wrapper_links=links,model_reference_corrections=corrections,runtime=dict(ready_proof=str(ready_path),ready_proof_sha256=sha(ready_path),native=dict(path=ready['native'],sha256=ready['native_sha256']),windows=dict(path=ready['windows'],sha256=ready['windows_sha256']),compiled_runtime_version=1,data_revision=2),activation_pending=True)
    dump(NEW/'local-origin.json',origin)
    dump(NEW/'STAGING-v2.json',dict(ready=False,data_revision=2,data_only=True,runtime_recompiled=False,project=str(P),source_project=str(SOURCE),source_project_unchanged=True,source_origin_sha256=old_origin_sha,base_alias_corrections=corrections,asset_reference_matrix=references,all_references_exist=True,actual_cpu_loader_validation_pending=True,origin_sha256=sha(NEW/'local-origin.json'),native_gpu_used=False))
    print(json.dumps(dict(prepared=True,data_only=True,project=str(P),base_aliases_added=3,references_to_validate=16,source_unchanged=True,runtime_recompiled=False)))

if __name__=='__main__':main()
