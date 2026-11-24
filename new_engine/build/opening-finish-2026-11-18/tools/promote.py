"""Publish only the reviewed Linux opening changes, preserving old asset stems."""
from pathlib import Path
import hashlib,json,os,shutil,subprocess,sys
N=Path(__file__).resolve().parents[1];E=N.parents[1];ROOT=E.parent;B=N.with_name('window-camera-2026-11-18')
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def atomic(p,data):
    t=p.with_name(p.name+'.opening-finish.tmp');t.write_bytes(data);os.replace(t,p)
def run(script,*args):subprocess.run([sys.executable,str(N/script),*args],check=True)
manifest=ROOT/'CURRENT_VALWOOD.json';prior_bytes=manifest.read_bytes();prior=json.loads(prior_bytes)
assert Path(prior['runtime_generation'])==B,'Current generation changed during review'
assert not (N/'UNIFIED_READY.json').exists(),'Published generations are immutable'
run('tools/audit_candidate.py');run('tools/final_review.py')
plan=json.loads((N/'review/AUTHORING_PLAN.json').read_text())
for row in plan['changes']:
    assert sha(row['live_path'])==row['before_sha256']
    assert sha(N/row['staged_path'])==row['after_sha256']
for row in plan['new_asset_links']:
    p=Path(row['live']);assert not p.exists() and not p.is_symlink(),str(p)
    assert sha(row['candidate_asset'])==row['after_sha256']
for row in plan['original_assets']:
    p=Path(row['live']);assert p.is_symlink() and os.readlink(p)==row['before_symlink']
    assert sha(p)==row['before_sha256'],'Original model changed'
# Carry the newest mode progress forward just before switching the pointer.
for mode in ['arena','joust']:
    for p in (B/(mode+'-project/saves')).iterdir():
        if p.is_file():shutil.copy2(p,N/(mode+'-project/saves')/p.name)
for name in ['arena_player_character.cfg','arena_armor_finish.cfg','graphics.settings']:
    shutil.copy2(B/'arena-project'/name,N/'arena-project'/name)
R=N/'rollback';R.mkdir();(R/'CURRENT_VALWOOD.json').write_bytes(prior_bytes)
shader_paths=[Path(prior['working_world'])/'shaders',*[Path(p)/'shaders' for p in prior['area_projects'].values()]]
assert all(p.is_symlink() for p in shader_paths)
old_links={str(p):os.readlink(p) for p in shader_paths}
(R/'shader-links.json').write_text(json.dumps(old_links,indent=2)+'\n')
for row in plan['changes']:
    target=R/'authoring'/Path(row['live_path']).relative_to(Path(prior['working_world']))
    target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(Path(row['live_path']).read_bytes())
changed=[];added=[];published=False
try:
    for row in plan['new_asset_links']:
        p=Path(row['live']);p.symlink_to(Path(row['candidate_asset']));added.append(p)
    for row in plan['changes']:
        p=Path(row['live_path']);atomic(p,(N/row['staged_path']).read_bytes());changed.append(row)
    for p in shader_paths:p.unlink();p.symlink_to(N/'shared/shaders',target_is_directory=True)
    run('tools/audit_candidate.py','--after-authoring');run('tools/final_review.py')
    run('check_unified.py','--publish');published=True
    current=dict(prior,runtime_generation=str(N),binary=str(N/'runtime/rpg'),source=str(N/'source-v1'),shared_shaders=str(N/'shared/shaders'),character_textures=str(N/'shared/character-textures'),guide=str(N/'GUIDE.txt'),binary_sha256=sha(N/'runtime/rpg'))
    atomic(manifest,(json.dumps(current,indent=2)+'\n').encode())
    for mode in ['demo','editor','arena','joust','world']:
        subprocess.run([str(ROOT/'Launch_Valwood.sh'),mode,'--check-only'],check=True,cwd=ROOT)
except Exception:
    atomic(manifest,prior_bytes)
    for row in changed:
        p=Path(row['live_path']);atomic(p,(R/'authoring'/p.relative_to(Path(prior['working_world']))).read_bytes())
    for p in added:
        if p.is_symlink():p.unlink()
    for p in shader_paths:
        if p.is_symlink():p.unlink()
        p.symlink_to(old_links[str(p)],target_is_directory=True)
    # This transaction started without a READY marker. Remove its own marker
    # on rollback, including a failed partial publication, before retrying.
    (N/'UNIFIED_READY.json').unlink(missing_ok=True)
    (N/'UNIFIED_READY.json.tmp').unlink(missing_ok=True)
    raise
report=dict(passed=True,old_generation=str(B),new_generation=str(N),binary_sha256=current['binary_sha256'],all_five_launchers_checked=True,intentional_authored_files=[r['live_path'] for r in changed],new_model_stems=[str(p) for p in added],original_model_stems_preserved=True,other_authored_files_and_progress_preserved=True,arena_and_joust_progress_carried_forward=True,native_only=True,Windows_work_performed=False)
(N/'review/PROMOTION.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
