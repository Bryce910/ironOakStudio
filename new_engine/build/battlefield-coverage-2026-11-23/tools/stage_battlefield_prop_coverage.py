#!/usr/bin/env python3
"""Stage exact-mesh coverage tiers outside the candidate. --adopt is guarded."""
from pathlib import Path
import argparse, hashlib, json, os, shutil, stat, tempfile, datetime
N=Path(__file__).resolve().parents[1]
P=N/'world-project'
D=N/'review/battlefield-prop-coverage'
STAGE=D/'staged-world'
NAMES=('barricade','discarded_arms')
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def dump(p,d):p.parent.mkdir(parents=True,exist_ok=True);p.write_text(json.dumps(d,indent=2)+'\n')
def file_meta(p):return {'path':str(p),'exists':p.exists(),'is_symlink':p.is_symlink(),'resolved_path':str(p.resolve()),'bytes':p.stat().st_size,'sha256':sha(p)}
def stage():
    assert not P.joinpath('world.studio').is_symlink()
    STAGE.mkdir(parents=True,exist_ok=True)
    original=(P/'world.studio').read_bytes();text=original.decode()
    changes=[]
    for name in NAMES:
        old=f'asset battlefield_{name} battlefield/{name} 1 '
        new=f'asset battlefield_{name} battlefield/{name} 2 '
        assert text.count(old)==1,(name,'alias must be unique oneLOD')
        text=text.replace(old,new)
        source=P/f'models/battlefield/{name}.glb';dest=STAGE/f'models/battlefield/{name}_mid.glb'
        assert not (P/dest.relative_to(STAGE)).exists()
        dest.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(source,dest)
        assert source.read_bytes()==dest.read_bytes()
        changes.append({'relative_path':str(dest.relative_to(STAGE)),'source':file_meta(source),'staged':file_meta(dest),'target_expected_absent':True,'exact_geometry_material_image_bytes':True})
    (D/'world.studio.before').write_bytes(original)
    (STAGE/'world.studio').write_text(text)
    plan=json.loads((N/'review/PUBLISH_PLAN.json').read_text());assert len(plan['files'])==21
    world=next(f for f in plan['files']if f['relative_path']=='world.studio');world['candidate']=file_meta(STAGE/'world.studio')
    world['appended_lines']=[line.replace('asset battlefield_barricade battlefield/barricade 1 ','asset battlefield_barricade battlefield/barricade 2 ').replace('asset battlefield_discarded_arms battlefield/discarded_arms 1 ','asset battlefield_discarded_arms battlefield/discarded_arms 2 ')for line in world['appended_lines']]
    for step in plan['transaction_steps']:
        step['action']=step['action'].replace('journal 10 new scene paths','journal 12 new scene paths').replace('exactly 21 regular scene files','exactly 23 regular scene files').replace('validate 8 GLBs','validate 10 GLBs').replace('8 self-contained GLBs first','10 self-contained GLBs first')
    for row in changes:
        live=Path(plan['active_project'])/row['relative_path'];assert not live.exists()
        plan['files'].append({'operation':'create_regular_file','relative_path':row['relative_path'],'scope':'Byte-identical full geometry/PBR MID coverage; requested near image quality unchanged','baseline':{'exists':False},'candidate':row['staged'],'live_at_audit':{'path':str(live),'exists':False},'status':'safe_compare_and_swap'})
    plan['snapshot_result']['create_files']+=2;plan['snapshot_result']['total_files']+=2;plan['snapshot_result']['bytes_to_stage']+=sum(r['staged']['bytes']for r in changes)
    plan['staged_only']=True;plan['requires_candidate_adoption_before_publication']=True
    dump(D/'PUBLISH_PLAN.proposed.json',plan)
    receipt={'schema':'valwood.battlefield-prop-coverage.v1','created_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'stage_only':True,'candidate_world_unchanged':sha(P/'world.studio')==hashlib.sha256(original).hexdigest(),'world_before':file_meta(P/'world.studio'),'world_staged':file_meta(STAGE/'world.studio'),'models':changes,'plan_before_sha256':sha(N/'review/PUBLISH_PLAN.json'),'proposed_plan':file_meta(D/'PUBLISH_PLAN.proposed.json'),'near_selected_texture_limit_unchanged':True,'placement_collision_terrain_water_unchanged':True}
    dump(D/'STAGED.json',receipt);print(json.dumps({'staged':True,'candidate_unchanged':receipt['candidate_world_unchanged'],'paths':23,'receipt':str(D/'STAGED.json')}))
def assert_regular(p):
    assert p.is_file() and not p.is_symlink(),f'Expected regular file: {p}'
    assert p.resolve().is_relative_to(P.resolve()),f'Candidate escape: {p}'
def atomic_copy(src,dst):
    dst.parent.mkdir(parents=True,exist_ok=True)
    fd,tmp=tempfile.mkstemp(prefix='.coverage-',dir=dst.parent)
    try:
        with os.fdopen(fd,'wb')as f:f.write(src.read_bytes());f.flush();os.fsync(f.fileno())
        os.replace(tmp,dst)
    finally:
        if os.path.exists(tmp):os.unlink(tmp)
def adopt():
    receipt=json.loads((D/'STAGED.json').read_text())
    assert sha(D/'PUBLISH_PLAN.proposed.json')==receipt['proposed_plan']['sha256'],'Staged proposed plan changed'
    for proc in Path('/proc').glob('[0-9]*'):
        try:
            raw=(proc/'cmdline').read_bytes().split(b'\0');first=Path(os.fsdecode(raw[0])).name if raw else''
            assert first not in ('rpg','rpg.exe'),f'Close Valwood PID{proc.name} before candidate adoption'
        except (FileNotFoundError,ProcessLookupError,PermissionError):pass
    assert_regular(P/'world.studio');assert sha(P/'world.studio')==receipt['world_before']['sha256'],'Candidate world changed since staging'
    assert sha(N/'review/PUBLISH_PLAN.json')==receipt['plan_before_sha256'],'Publication plan changed since staging'
    for r in receipt['models']:
        dest=P/r['relative_path'];assert not dest.exists()and not dest.is_symlink();assert dest.parent.resolve().is_relative_to(P.resolve());assert sha(Path(r['staged']['path']))==r['staged']['sha256'];assert sha(Path(r['source']['path']))==r['source']['sha256']
    assert sha(STAGE/'world.studio')==receipt['world_staged']['sha256']
    backup=D/'adoption-before';backup.mkdir(exist_ok=False)
    shutil.copyfile(P/'world.studio',backup/'world.studio');shutil.copyfile(N/'review/PUBLISH_PLAN.json',backup/'PUBLISH_PLAN.json')
    created=[]
    try:
        for r in receipt['models']:
            target=P/r['relative_path'];atomic_copy(Path(r['staged']['path']),target);created.append(target)
        atomic_copy(STAGE/'world.studio',P/'world.studio')
        plan=json.loads((D/'PUBLISH_PLAN.proposed.json').read_text())
        for f in plan['files']:
            if f['relative_path']=='world.studio'or f['relative_path']in {r['relative_path']for r in receipt['models']}:f['candidate']=file_meta(P/f['relative_path'])
        plan.pop('staged_only',None);plan.pop('requires_candidate_adoption_before_publication',None)
        dump(N/'review/PUBLISH_PLAN.json',plan)
        dump(D/'ADOPTED.json',{'candidate_only':True,'live_mutations':False,'world_sha256':sha(P/'world.studio'),'plan_sha256':sha(N/'review/PUBLISH_PLAN.json'),'backup':str(backup),'files':{str(p.relative_to(P)):sha(p)for p in created}})
    except BaseException:
        atomic_copy(backup/'world.studio',P/'world.studio');atomic_copy(backup/'PUBLISH_PLAN.json',N/'review/PUBLISH_PLAN.json')
        for p in created:p.unlink(missing_ok=True)
        raise
    print('Adopted exact coverage tiers in candidate only; live project untouched')
if __name__=='__main__':
    ap=argparse.ArgumentParser();ap.add_argument('--stage',action='store_true');ap.add_argument('--adopt',action='store_true');args=ap.parse_args()
    if args.stage:assert not args.adopt;stage()
    elif args.adopt:adopt()
    else:
        print('Read-only default. Use --stage to prepare separate review assets, --adopt after closing Valwood for guarded candidate-only adoption.')
