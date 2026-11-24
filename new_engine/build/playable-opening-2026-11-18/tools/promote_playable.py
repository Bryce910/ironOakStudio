from pathlib import Path
import hashlib,json,os,shutil,subprocess,sys
N=Path(__file__).resolve().parents[1];E=N.parents[1];ROOT=E.parent;B=N.with_name('opening-performance-2026-11-18')
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
def atomic(path,data):
 temp=path.with_name(path.name+'.playable.tmp');temp.write_bytes(data);os.replace(temp,path)
m=ROOT/'CURRENT_VALWOOD.json';old=m.read_bytes();prior=json.loads(old)
assert Path(prior['runtime_generation'])==B
assert not (N/'UNIFIED_READY.json').exists()
subprocess.run([sys.executable,str(N/'tools/audit_candidate.py')],check=True)
subprocess.run([sys.executable,str(N/'tools/summarize_review.py')],check=True)
protected=json.loads((N/'review/LIVE_BEFORE.json').read_text());assert all(sha(p)==h for p,h in protected.items())
for f in (B/'arena-project/saves').iterdir():
 if f.is_file():shutil.copy2(f,N/'arena-project/saves'/f.name)
for name in ['arena_player_character.cfg','arena_armor_finish.cfg','graphics.settings']:
 shutil.copy2(B/'arena-project'/name,N/'arena-project'/name)
R=N/'rollback';R.mkdir();(R/'CURRENT_VALWOOD.json').write_bytes(old)
subprocess.run([sys.executable,str(N/'check_unified.py'),'--publish'],check=True)
try:
 current=dict(prior,runtime_generation=str(N),binary=str(N/'runtime/rpg'),source=str(N/'source-v1'),shared_shaders=str(N/'shared/shaders'),character_textures=str(N/'shared/character-textures'),guide=str(N/'GUIDE.txt'),binary_sha256=sha(N/'runtime/rpg'))
 atomic(m,(json.dumps(current,indent=2)+'\n').encode())
 for mode in ('demo','editor','arena','joust','world'):
  subprocess.run([str(ROOT/'Launch_Valwood.sh'),mode,'--check-only'],check=True,cwd=ROOT)
 assert all(sha(p)==h for p,h in protected.items())
except Exception:atomic(m,old);raise
(N/'review/PROMOTION.json').write_text(json.dumps(dict(passed=True,old_generation=str(B),new_generation=str(N),binary_sha256=current['binary_sha256'],all_five_launchers_checked=True,working_world_files_modified=[],protected_live_documents_and_saves=len(protected),arena_progress_carried_forward=True,windows_work_performed=False),indent=2)+'\n')
print('Playable opening published. Live world and progress preserved.')
