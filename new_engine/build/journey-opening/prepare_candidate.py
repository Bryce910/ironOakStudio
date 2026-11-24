#!/usr/bin/env python3
"""Create one private, unlaunched v16 opening candidate with shared combat banks."""
from pathlib import Path
import hashlib,json,shutil,sys

R=Path(__file__).resolve().parents[2];HERE=Path(__file__).resolve().parent
SOURCE=R/'build/opening-finward-reference/v16/project'
DEST=HERE/'candidate-v1';PROJECT=DEST/'project'
BANK=R/'build/character-polish/assets-v1'
ACCEPTED=R/'build/combat-arena/character-candidate-v1/local-origin.json'
sys.path.insert(0,str(R/'tools'))
from check_opening_prologue import mutable_snapshot

def sha(p):
    with p.open('rb') as stream:return hashlib.file_digest(stream,'sha256').hexdigest()
def dump(p,d):
    with p.open('x') as f:json.dump(d,f,indent=2);f.write('\n')
def copy_tree(source,dest):
    if source.is_symlink():
        target=source.resolve(strict=True);dest.symlink_to(target,target_is_directory=target.is_dir())
    elif source.is_dir():
        dest.mkdir()
        for p in sorted(source.iterdir()):copy_tree(p,dest/p.name)
    elif source.is_file():shutil.copyfile(source,dest)
    else:raise ValueError('Unsupported source entry: '+str(source))

def main():
    assert SOURCE.is_dir() and not SOURCE.is_symlink()
    assert not DEST.exists() and not DEST.is_symlink(),'Use a fresh candidate'
    accepted=json.loads(ACCEPTED.read_text())
    character=accepted['character_assets'];blood=accepted['blood_assets']
    assert Path(character['bank'])==BANK and sha(BANK/'CHARACTER_POLISH.json')==character['files_sha256']['CHARACTER_POLISH.json']
    before=mutable_snapshot(SOURCE)
    original_saves={str(p.relative_to(SOURCE/'saves')):sha(p) for p in (SOURCE/'saves').rglob('*') if p.is_file()}
    DEST.mkdir();PROJECT.mkdir()
    for p in sorted(SOURCE.iterdir()):
        if p.name in {'models','saves'}:continue
        copy_tree(p,PROJECT/p.name)
    (PROJECT/'saves').mkdir()
    models=PROJECT/'models';models.mkdir();links={}
    for p in sorted((SOURCE/'models').iterdir()):
        target=BANK if p.name=='longsword_mocap' else p.resolve(strict=True)
        (models/p.name).symlink_to(target,target_is_directory=target.is_dir());links[p.name]=str(target)
    assert links.get('longsword_mocap')==str(BANK)
    shutil.copyfile(BANK/'CHARACTER_POLISH.json',PROJECT/'CHARACTER_POLISH.json')
    blood_bank=Path(blood['bank']);assert blood_bank.is_dir()
    for n,h in blood['files_sha256'].items():assert sha(blood_bank/n)==h,n
    (PROJECT/'blood_assets').symlink_to(blood_bank,target_is_directory=True)
    # Bow data is shared as a passive bank, without an arena opt-in/config.
    (PROJECT/'arena_assets').symlink_to(R/'build/combat-arena/assets',target_is_directory=True)
    shaders={}
    for name in ['world_blood.vs.glsl','world_blood.fs.glsl','world_combat_vfx.vs.glsl','world_combat_vfx.fs.glsl']:
        source=R/'shaders'/name;shutil.copyfile(source,PROJECT/'shaders'/name);shaders['shaders/'+name]=sha(source)
    assert before==mutable_snapshot(SOURCE),'Source opening or actual saves changed'
    assert not any((PROJECT/'saves').iterdir()) and sha(PROJECT/'graphics.settings')==sha(SOURCE/'graphics.settings')
    generated={}
    for p in sorted(PROJECT.rglob('*')):
        if p.is_file() and not p.is_symlink():generated[p.relative_to(PROJECT).as_posix()]=sha(p)
    origin=dict(schema='valwood.journey-opening.private-candidate',version=1,project=str(PROJECT),source_project=str(SOURCE),source_origin_sha256=sha(SOURCE.parent/'opening-origin.json'),runtime=None,generated_sha256=generated,source_guard=before,source_save_sha256=original_saves,source_saves_copied=False,source_unchanged=True,private_saves_empty=True,graphics_preferences_copied_exactly=True,model_wrapper_links=links,character_assets=character,blood_assets=blood,combat_shader_sha256=shaders,shared_bank_bytes_copied=0,engine_sources_modified=False,gpu_used=False,activated=False)
    dump(DEST/'local-origin.json',origin)
    dump(DEST/'PREPARATION.json',dict(prepared=True,project=str(PROJECT),origin_sha256=sha(DEST/'local-origin.json'),copied_private_files=len(generated),models_linked=len(links),source_unchanged=True,source_saves_copied=False,graphics_preferences_unchanged=True,character_model_assets=character['model_assets'],character_bank=str(BANK),blood_bank=str(blood_bank),runtime_bound=False,compiled=False,game_launched=False))
    print(json.dumps(dict(prepared=True,project=str(PROJECT),origin=str(DEST/'local-origin.json'),copied_private_files=len(generated),models_linked=len(links),source_unchanged=True)))

if __name__=='__main__':main()
