"""Publish the verified native generation with reversible shader/arena-scene updates."""
from pathlib import Path
import hashlib,json,os,subprocess,sys,time
N=Path(__file__).resolve().parents[1];E=N.parents[1];root=E.parent;sys.path.insert(0,str(N))
from launch import configuration,AREAS
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
def atomic(path,data):
 temp=path.with_name(path.name+'.surface-polish.tmp');temp.write_bytes(data);os.replace(temp,path)
manifest=root/'CURRENT_VALWOOD.json';old_bytes=manifest.read_bytes();old=json.loads(old_bytes);assert Path(old['runtime_generation']).name=='animation-polish-2026-11-17','Active generation changed; review before promotion'
review=N/'review';rollback=N/'rollback';rollback.mkdir(exist_ok=True)
(rollback/'CURRENT_VALWOOD.json').write_bytes(old_bytes)
projects={p.resolve() for p in [configuration(m)[2] for m in ('demo','editor','arena','joust','world')]+[p for p,_ in AREAS.values()]}
links={str(p/'shaders'):os.readlink(p/'shaders') for p in projects};(rollback/'SHADER_LINKS.json').write_text(json.dumps(links,indent=2)+'\n')
scene=(N/'arena-project/world.studio').resolve();origin=json.loads((N/'arena-scene-patch/ORIGIN.json').read_text());assert sha(scene)==origin['original_sha256'],'Arena scene has new edits; preserving them requires a rebase'
scene_before=scene.read_bytes();(rollback/'arena.world.studio').write_bytes(scene_before)
before=json.loads((review/'LIVE_SAVE_BEFORE.json').read_text());assert all(sha(p)==h for p,h in before.items()),'Live saves changed during private tests'
try:
 for name in links:
  path=Path(name);temp=path.with_name('shaders.surface-polish.tmp');temp.unlink(missing_ok=True);temp.symlink_to(N/'shared/shaders',target_is_directory=True);os.replace(temp,path)
 atomic(scene,(N/'arena-scene-patch/world.studio').read_bytes())
 subprocess.run([sys.executable,str(N/'check_unified.py'),'--publish'],check=True)
 new=dict(old,runtime_generation=str(N),binary=str(N/'runtime/rpg'),source=str(N/'source-v1'),shared_shaders=str(N/'shared/shaders'),character_textures=str(N/'shared/character-textures'),guide=str(N/'GUIDE.txt'),binary_sha256=sha(N/'runtime/rpg'))
 atomic(manifest,(json.dumps(new,indent=2)+'\n').encode())
 subprocess.run([str(root/'Launch_Valwood.sh'),'arena','--check-only'],cwd=root,check=True)
 assert all(sha(p)==h for p,h in before.items()),'Save changed during promotion'
 (review/'PROMOTION.json').write_text(json.dumps(dict(passed=True,old_generation=old['runtime_generation'],new_generation=str(N),binary_sha256=new['binary_sha256'],shader_projects=len(projects),arena_scene= str(scene),scene_before_sha256=hashlib.sha256(scene_before).hexdigest(),scene_after_sha256=sha(scene),live_saves_preserved=len(before),windows_work_performed=False,rollback=str(rollback)),indent=2)+'\n')
except Exception:
 atomic(manifest,old_bytes);atomic(scene,scene_before)
 for name,target in links.items():
  path=Path(name);temp=path.with_name('shaders.surface-rollback.tmp');temp.unlink(missing_ok=True);temp.symlink_to(target,target_is_directory=True);os.replace(temp,path)
 raise
print('Surface generation published. Existing saves retained; rollback recorded.')
