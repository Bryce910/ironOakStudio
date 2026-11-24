from pathlib import Path
import fcntl,hashlib,json,os,shutil
N=Path(__file__).resolve().parents[1];E=N.parents[1];R=E.parent;P=N/'world-project'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
# Share the launcher lock so an editor/game cannot begin on partially installed
# world data. Private GPU reviews are closed by the reviewer before this runs.
lock=os.open(E/'projects/seldonne/.valwood-instance.lock',os.O_RDWR|os.O_CREAT,0o600)
try:fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
except BlockingIOError:raise RuntimeError('Close the live game/editor before publishing')
import sys
sys.path.insert(0,str(N));from check_unified import check
check()
accept=json.loads((N/'review/NATIVE_ACCEPTANCE.json').read_text())
assert accept['passed'] and accept['binary_sha256']==sha(N/'runtime/rpg')
manifest=R/'CURRENT_VALWOOD.json';old=manifest.read_bytes();d=json.loads(old)
assert Path(d['runtime_generation']).name=='demo-readiness-2026-11-21','Active generation changed'
assert sha(Path(d['binary']))==d['binary_sha256'],'Published binary changed'
live=Path(d['working_world']);original=json.loads((N/'review/PROJECT_BASELINE.json').read_text())
changes=[Path('world.studio'),Path('collision/structures/catalog')]
changes += [p.relative_to(P) for p in (P/'scenery').rglob('*') if p.is_file() and sha(p)!=original.get(str(p.relative_to(P)))]
for rel in changes:
 expected=sha(N/'review/art/COLLISION_CATALOG.before') if str(rel)=='collision/structures/catalog' else original[str(rel)]
 assert sha(live/rel)==expected,'Live file changed: '+str(rel)
models=['art_completion','art_birch','camp_birds','camp_variants']
for name in models[:-1]:assert not (live/'models'/name).exists() and not (live/'models'/name).is_symlink()
assert (live/'models/camp_variants').is_dir() and not (live/'models/camp_variants').is_symlink()
# The replacement variants reference immutable original builds, never the live directory.
for p in (P/'models/camp_variants').iterdir():
 if p.is_symlink():assert not p.resolve().is_relative_to(live)
preserved={str(p.relative_to(live)):sha(p) for p in [live/'graphics.settings',*(live/'saves').glob('*')] if p.is_file()}
backup=N/'review/live-before';backup.mkdir(exist_ok=False)
(backup/'CURRENT_VALWOOD.json').write_bytes(old)
for rel in changes:
 dest=backup/rel;dest.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(live/rel,dest)
installed_models=[];moved_models=[];installed_files=[];temporary=[];manifest_installed=False
try:
 for name in models:
  target=live/'models'/name
  if target.exists():
   dest=backup/'models'/name;dest.parent.mkdir(parents=True,exist_ok=True);target.rename(dest);moved_models.append(name)
  target.symlink_to(P/'models'/name,target_is_directory=True);installed_models.append(name)
 for rel in changes:
  path=live/rel;tmp=path.with_name('.'+path.name+'.camp-completion.tmp');temporary.append(tmp)
  shutil.copy2(P/rel,tmp);os.replace(tmp,path);installed_files.append(rel)
 d.update(runtime_generation=str(N),binary=str(N/'runtime/rpg'),source=str(N/'source-v1'),shared_shaders=str(N/'shared/shaders'),guide=str(N/'GUIDE.txt'),binary_sha256=sha(N/'runtime/rpg'))
 tmp=manifest.with_name('.CURRENT_VALWOOD.camp-completion.tmp');temporary.append(tmp)
 tmp.write_text(json.dumps(d,indent=2)+'\n');assert manifest.read_bytes()==old;os.replace(tmp,manifest);manifest_installed=True
 for name,h in preserved.items():assert sha(live/name)==h,'Save/settings changed: '+name
except BaseException:
 # Restore only this transaction's world data. Never restore/overwrite saves.
 if manifest_installed:
  tmp=manifest.with_name('.CURRENT_VALWOOD.camp-rollback.tmp');tmp.write_bytes(old);os.replace(tmp,manifest)
 for rel in reversed(installed_files):
  path=live/rel;tmp=path.with_name('.'+path.name+'.camp-rollback.tmp');shutil.copy2(backup/rel,tmp);os.replace(tmp,path)
 for name in reversed(installed_models):
  target=live/'models'/name
  assert target.is_symlink() and target.readlink()==P/'models'/name,'Unexpected replacement during rollback'
  target.unlink()
 for name in reversed(moved_models):(backup/'models'/name).rename(live/'models'/name)
 for tmp in temporary:
  if tmp.exists():tmp.unlink()
 raise
(N/'review/PUBLISHED.json').write_text(json.dumps(dict(generation=str(N),binary_sha256=d['binary_sha256'],changed_files=[str(p) for p in changes],models=models,live_save_and_settings_hashes_preserved=preserved,backup=str(backup),Windows_work_performed=False),indent=2)+'\n')
print('Published',N.name,'; saves/settings preserved; no Windows build')
