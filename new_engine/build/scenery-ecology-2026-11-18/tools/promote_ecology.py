from pathlib import Path
import hashlib,json,os,shutil,subprocess,sys
N=Path(__file__).resolve().parents[1];E=N.parents[1];ROOT=E.parent;BASE=N.with_name('arena-scenery-2026-11-18');WORLD=E/'projects/valwood_current';sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
def atomic(path,data):
 temp=path.with_name(path.name+'.ecology.tmp');temp.write_bytes(data);os.replace(temp,path)
def link(path,target):
 temp=path.with_name(path.name+'.ecology.tmp');temp.unlink(missing_ok=True);temp.symlink_to(target);os.replace(temp,path)
m=ROOT/'CURRENT_VALWOOD.json';old_bytes=m.read_bytes();old=json.loads(old_bytes);assert Path(old['runtime_generation'])==BASE
origin=json.loads((N/'WORLD_ORIGIN.json').read_text());assert all(sha(WORLD/p)==h for p,h in origin['files'].items()),'Working world changed during private review'
before=json.loads((N/'review/LIVE_SAVE_BEFORE.json').read_text());assert all(sha(p)==h for p,h in before.items()),'Live progress changed during review'
for f in (BASE/'arena-project/saves').iterdir():
 if f.is_file():shutil.copy2(f,N/'arena-project/saves'/f.name)
for name in ['arena_player_character.cfg','arena_armor_finish.cfg','graphics.settings']:shutil.copy2(BASE/'arena-project'/name,N/'arena-project'/name)
rollback=N/'rollback';rollback.mkdir(exist_ok=True);(rollback/'CURRENT_VALWOOD.json').write_bytes(old_bytes)
files=['world.studio',*['scenery/'+p.name for p in (N/'world-project/scenery').glob('*instances.txt')]]
for name in files:
 target=rollback/'world'/name;target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(WORLD/name,target)
links={}
for name in ['cinematic_grass_0_mid.glb','cinematic_grass_1_mid.glb']:
 p=WORLD/'models'/name
 if p.is_symlink():links[str(p)]=dict(symlink=os.readlink(p))
 else:shutil.copy2(p,rollback/name);links[str(p)]=dict(file=str(rollback/name))
(rollback/'MODEL_LINKS.json').write_text(json.dumps(links,indent=2)+'\n')
new_images=[p for p in (N/'world-project/surfaces/ecology_20261118').iterdir() if p.is_file()];assert not (WORLD/'surfaces/ecology_20261118').exists(),'Material destination already exists'
try:
 (WORLD/'surfaces/ecology_20261118').mkdir()
 for p in new_images:shutil.copy2(p,WORLD/'surfaces/ecology_20261118'/p.name)
 for name in files:atomic(WORLD/name,(N/'world-project'/name).read_bytes())
 for name in ['cinematic_grass_0_mid.glb','cinematic_grass_1_mid.glb']:link(WORLD/'models'/name,N/'owned-grass'/name)
 subprocess.run([sys.executable,str(N/'check_unified.py'),'--publish'],check=True)
 new=dict(old,runtime_generation=str(N),binary=str(N/'runtime/rpg'),source=str(N/'source-v1'),shared_shaders=str(N/'shared/shaders'),character_textures=str(N/'shared/character-textures'),guide=str(N/'GUIDE.txt'),binary_sha256=sha(N/'runtime/rpg'))
 atomic(m,(json.dumps(new,indent=2)+'\n').encode())
 for mode in ('demo','editor','arena','joust','world'):subprocess.run([str(ROOT/'Launch_Valwood.sh'),mode,'--check-only'],cwd=ROOT,check=True)
 assert all(sha(p)==h for p,h in before.items())
except Exception:
 atomic(m,old_bytes)
 for name in files:atomic(WORLD/name,(rollback/'world'/name).read_bytes())
 for name,value in links.items():
  p=Path(name)
  if 'symlink' in value:link(p,value['symlink'])
  else:atomic(p,Path(value['file']).read_bytes())
 for p in new_images:(WORLD/'surfaces/ecology_20261118'/p.name).unlink(missing_ok=True)
 (WORLD/'surfaces/ecology_20261118').rmdir()
 raise
(N/'review/PROMOTION.json').write_text(json.dumps(dict(passed=True,old_generation=str(BASE),new_generation=str(N),binary_unchanged=True,shader_bank_unchanged=True,working_world_files_updated=files,matched_grass_mid_links=links,live_saves_preserved=len(before),arena_progress_copied_from_current=True,all_five_launchers_checked=True,windows_work_performed=False,rollback=str(rollback)),indent=2)+'\n');print('Canopy-aware scenery published for arena and the opening woodland. Progress preserved.')
