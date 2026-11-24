from pathlib import Path
import hashlib,json,os,shutil,subprocess,sys
N=Path(__file__).resolve().parents[1];E=N.parents[1];ROOT=E.parent
BASE=N.with_name('scenery-ecology-2026-11-18');WORLD=E/'projects/valwood_current'
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
def atomic(path,data):
    temp=path.with_name(path.name+'.performance.tmp');temp.write_bytes(data);os.replace(temp,path)
m=ROOT/'CURRENT_VALWOOD.json';old_bytes=m.read_bytes();old=json.loads(old_bytes)
assert Path(old['runtime_generation'])==BASE
assert not (N/'UNIFIED_READY.json').exists(), 'Published generations are immutable'
origin=json.loads((N/'review/AUTHORING_BEFORE.json').read_text())
assert all(sha(p)==h for p,h in origin.items()), 'Working authoring changed during private review'
before=json.loads((N/'review/LIVE_SAVE_BEFORE.json').read_text())
assert all(sha(p)==h for p,h in before.items()), 'Live progress changed during review'
polish=json.loads((N/'review/POLISH.json').read_text())
assert sha(N/'world-project/world.studio')==polish['after_sha256']
for f in (BASE/'arena-project/saves').iterdir():
    if f.is_file():shutil.copy2(f,N/'arena-project/saves'/f.name)
for name in ['arena_player_character.cfg','arena_armor_finish.cfg','graphics.settings']:
    shutil.copy2(BASE/'arena-project'/name,N/'arena-project'/name)
rollback=N/'rollback';rollback.mkdir()
(rollback/'CURRENT_VALWOOD.json').write_bytes(old_bytes)
(rollback/'world.studio').write_bytes((WORLD/'world.studio').read_bytes())
try:
    atomic(WORLD/'world.studio',(N/'world-project/world.studio').read_bytes())
    subprocess.run([sys.executable,str(N/'check_unified.py'),'--publish'],check=True)
    new=dict(old,runtime_generation=str(N),binary=str(N/'runtime/rpg'),source=str(N/'source-v1'),
        shared_shaders=str(N/'shared/shaders'),character_textures=str(N/'shared/character-textures'),
        guide=str(N/'GUIDE.txt'),binary_sha256=sha(N/'runtime/rpg'))
    atomic(m,(json.dumps(new,indent=2)+'\n').encode())
    for mode in ('demo','editor','arena','joust','world'):
        subprocess.run([str(ROOT/'Launch_Valwood.sh'),mode,'--check-only'],cwd=ROOT,check=True)
    assert all(sha(p)==h for p,h in before.items())
except Exception:
    atomic(m,old_bytes);atomic(WORLD/'world.studio',(rollback/'world.studio').read_bytes());raise
(N/'review/PROMOTION.json').write_text(json.dumps(dict(passed=True,old_generation=str(BASE),
    new_generation=str(N),binary_sha256=sha(N/'runtime/rpg'),shader_bank_unchanged=True,
    working_world_files_updated=['world.studio'],live_saves_preserved=len(before),
    arena_progress_copied_from_current=True,all_five_launchers_checked=True,
    windows_work_performed=False,rollback=str(rollback)),indent=2)+'\n')
print('Lossless pose storage and arrival ground polish published. Progress preserved.')
