"""Select the reviewed runtime and exact scenery row patch under the game lock."""
from pathlib import Path
import json,shutil,subprocess,sys,os,hashlib,fcntl
N=Path(__file__).resolve().parents[1];B=N.with_name('opening-focus-2026-11-18');R=N.parents[2]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def run(*a):subprocess.run([sys.executable,*map(str,a)],check=True)
manifest=R/'CURRENT_VALWOOD.json';old=manifest.read_bytes();prior=json.loads(old)
assert prior['runtime_generation']==str(B),'Current runtime changed during review'
assert not (N/'UNIFIED_READY.json').exists(),'Published generations are immutable'
lock=os.open(R/'new_engine/projects/seldonne/.valwood-instance.lock',os.O_RDWR|os.O_CREAT,0o600)
fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
assert manifest.read_bytes()==old,'Current runtime changed while acquiring the game lock'
run(N/'tools/audit_candidate.py')
final=json.loads((N/'review/FINAL_REVIEW.json').read_text());assert final['passed']
assert final['scenery_native_visual_passed'],'Scenery needs native acceptance'
patch=json.loads((N/'review/scenery/ROW_PATCH.json').read_text())
world=R/'new_engine/projects/valwood_current';scene_changes=[]
for f in patch['files']:
 path=Path(f['path']);assert not path.is_absolute() and '..' not in path.parts
 live=world/path;prepared=N/'world-project'/path
 assert sha(live)==f['before_sha256'],'Designer file changed; review conflict: '+str(live)
 assert sha(prepared)==f['after_sha256'],'Prepared scenery changed: '+str(prepared)
 scene_changes.append((live,prepared,live.read_bytes(),live.stat().st_mode&0o777))
# Bring forward only mutable mode progress/settings at the final safe boundary.
for mode in ('arena','joust'):
 for p in (B/(mode+'-project/saves')).iterdir():
  if p.is_file():shutil.copy2(p,N/(mode+'-project/saves')/p.name)
for name in ('arena_player_character.cfg','arena_armor_finish.cfg','graphics.settings'):
 p=B/'arena-project'/name
 if p.is_file():shutil.copy2(p,N/'arena-project'/name)
rollback=N/'rollback';rollback.mkdir();(rollback/'CURRENT_VALWOOD.json').write_bytes(old)
for live,prepared,before,mode in scene_changes:
 backup=rollback/'scene'/live.relative_to(world);backup.parent.mkdir(parents=True,exist_ok=True);backup.write_bytes(before)
def replace_bytes(path,data,mode):
 temp=path.with_name(path.name+'.opening-polish.tmp')
 with temp.open('wb') as f:f.write(data);f.flush();os.fsync(f.fileno())
 os.chmod(temp,mode);os.replace(temp,path)
try:
 # The full original bytes were reconstructed from the reviewed before/after
 # rows by the CPU checker. A changed designer file is never overwritten.
 for live,prepared,before,mode in scene_changes:replace_bytes(live,prepared.read_bytes(),mode)
 run(N/'check_unified.py','--publish')
 next_=dict(prior,runtime_generation=str(N),binary=str(N/'runtime/rpg'),source=str(N/'source-v1'),shared_shaders=str(N/'shared/shaders'),character_textures=str(N/'shared/character-textures'),guide=str(N/'GUIDE.txt'),binary_sha256=sha(N/'runtime/rpg'))
 temp=manifest.with_suffix('.opening-polish.tmp');temp.write_text(json.dumps(next_,indent=2)+'\n');os.replace(temp,manifest)
 for mode in ('demo','editor','arena','joust','world'):
  subprocess.run([str(R/'Launch_Valwood.sh'),mode,'--check-only'],cwd=R,check=True)
except Exception:
 for live,prepared,before,mode in scene_changes:replace_bytes(live,before,mode)
 temp=manifest.with_suffix('.opening-polish.tmp');temp.write_bytes(old);os.replace(temp,manifest)
 (N/'UNIFIED_READY.json').unlink(missing_ok=True);raise
print(json.dumps(dict(passed=True,generation=str(N),all_five_launchers_checked=True,reviewed_scenery_files_changed=len(scene_changes),other_authoring_and_progress_not_modified=True,rollback_scene_backups=str(rollback/'scene'),native_only=True,binary_sha256=next_['binary_sha256']),indent=2))
