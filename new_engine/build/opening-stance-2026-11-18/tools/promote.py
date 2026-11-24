"""Select only the fully reviewed natural-stance native generation."""
from pathlib import Path
import json,shutil,subprocess,sys,os,hashlib,fcntl
N=Path(__file__).resolve().parents[1];B=N.with_name('opening-polish-2026-11-18');R=N.parents[2]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def run(*a):subprocess.run([sys.executable,*map(str,a)],check=True)
manifest=R/'CURRENT_VALWOOD.json';old=manifest.read_bytes();prior=json.loads(old)
assert prior['runtime_generation']==str(B),'Current runtime changed during review'
assert not (N/'UNIFIED_READY.json').exists(),'Published generations are immutable'
lock=os.open(R/'new_engine/projects/seldonne/.valwood-instance.lock',os.O_RDWR|os.O_CREAT,0o600)
fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
assert manifest.read_bytes()==old,'Current runtime changed while acquiring the game lock'
run(N/'tools/audit_candidate.py')
final=json.loads((N/'review/FINAL_REVIEW.json').read_text());assert final['passed'] and final['stance_native_visual_passed']
for mode in ('arena','joust'):
 for p in (B/(mode+'-project/saves')).iterdir():
  if p.is_file():shutil.copy2(p,N/(mode+'-project/saves')/p.name)
for name in ('arena_player_character.cfg','arena_armor_finish.cfg','graphics.settings'):
 p=B/'arena-project'/name
 if p.is_file():shutil.copy2(p,N/'arena-project'/name)
rollback=N/'rollback';rollback.mkdir();(rollback/'CURRENT_VALWOOD.json').write_bytes(old)
try:
 run(N/'check_unified.py','--publish')
 next_=dict(prior,runtime_generation=str(N),binary=str(N/'runtime/rpg'),source=str(N/'source-v1'),shared_shaders=str(N/'shared/shaders'),character_textures=str(N/'shared/character-textures'),guide=str(N/'GUIDE.txt'),binary_sha256=sha(N/'runtime/rpg'))
 temp=manifest.with_suffix('.stance.tmp');temp.write_text(json.dumps(next_,indent=2)+'\n');os.replace(temp,manifest)
 for mode in ('demo','editor','arena','joust','world'):subprocess.run([str(R/'Launch_Valwood.sh'),mode,'--check-only'],cwd=R,check=True)
except Exception:
 temp=manifest.with_suffix('.stance.tmp');temp.write_bytes(old);os.replace(temp,manifest);(N/'UNIFIED_READY.json').unlink(missing_ok=True);raise
print(json.dumps(dict(passed=True,generation=str(N),all_five_launchers_checked=True,world_and_progress_not_modified=True,native_only=True,binary_sha256=next_['binary_sha256']),indent=2))
