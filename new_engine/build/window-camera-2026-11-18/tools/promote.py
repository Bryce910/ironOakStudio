from pathlib import Path
import hashlib,json,os,shutil,subprocess,sys
N=Path(__file__).resolve().parents[1];E=N.parents[1];ROOT=E.parent;B=N.with_name('playable-opening-2026-11-18');sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
def atomic(p,data):
 t=p.with_name(p.name+'.window-camera.tmp');t.write_bytes(data);os.replace(t,p)
m=ROOT/'CURRENT_VALWOOD.json';old=m.read_bytes();prior=json.loads(old);assert Path(prior['runtime_generation'])==B
live=Path(prior['working_world'])/'world.studio';oldscene=live.read_bytes();assert sha(live)==sha(N/'authoring-snapshot/world.studio.before')
subprocess.run([sys.executable,str(N/'tools/audit_candidate.py')],check=True)
protected=json.loads((N/'review/LIVE_BEFORE.json').read_text());assert all(sha(p)==h for p,h in protected.items())
for p in (B/'arena-project/saves').iterdir():
 if p.is_file():shutil.copy2(p,N/'arena-project/saves'/p.name)
for p in (B/'joust-project/saves').iterdir():
 if p.is_file():shutil.copy2(p,N/'joust-project/saves'/p.name)
for name in ['arena_player_character.cfg','arena_armor_finish.cfg','graphics.settings']:
 shutil.copy2(B/'arena-project'/name,N/'arena-project'/name)
R=N/'rollback';R.mkdir();(R/'CURRENT_VALWOOD.json').write_bytes(old);(R/'world.studio').write_bytes(oldscene)
shader_paths=[Path(prior['working_world'])/'shaders',*[Path(p)/'shaders' for p in prior['area_projects'].values()]]
assert all(p.is_symlink() for p in shader_paths)
old_links={str(p):os.readlink(p) for p in shader_paths};(R/'shader-links.json').write_text(json.dumps(old_links,indent=2)+'\n')
try:
 for p in shader_paths:p.unlink();p.symlink_to(N/'shared/shaders',target_is_directory=True)
 subprocess.run([sys.executable,str(N/'check_unified.py'),'--publish'],check=True)
 atomic(live,(N/'authoring-snapshot/world.studio.after').read_bytes())
 current=dict(prior,runtime_generation=str(N),binary=str(N/'runtime/rpg'),source=str(N/'source-v1'),shared_shaders=str(N/'shared/shaders'),character_textures=str(N/'shared/character-textures'),guide=str(N/'GUIDE.txt'),binary_sha256=sha(N/'runtime/rpg'))
 atomic(m,(json.dumps(current,indent=2)+'\n').encode())
 for mode in ['demo','editor','arena','joust','world']:subprocess.run([str(ROOT/'Launch_Valwood.sh'),mode,'--check-only'],check=True,cwd=ROOT)
 assert all(sha(p)==(sha(N/'authoring-snapshot/world.studio.after') if Path(p)==live else h) for p,h in protected.items())
except Exception:
 atomic(m,old);atomic(live,oldscene)
 for p in shader_paths:
  p.unlink();p.symlink_to(old_links[str(p)],target_is_directory=True)
 raise
report=dict(passed=True,old_generation=str(B),new_generation=str(N),binary_sha256=current['binary_sha256'],all_five_launchers_checked=True,world_changes='Seven window lights and a shared apartment-only shader correction',other_authored_files_and_progress_preserved=True,arena_progress_carried_forward=True,older_open_demo_left_intact=True,restart_required_for_already_running_process=True,windows_work_performed=False)
(N/'review/PROMOTION.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
