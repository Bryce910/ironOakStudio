from pathlib import Path
import hashlib,json,os,subprocess,sys
N=Path(__file__).resolve().parents[1];E=N.parents[1];ROOT=E.parent
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
def atomic(p,data):
 temp=p.with_name(p.name+'.third-person-flow.tmp');temp.write_bytes(data);os.replace(temp,p)
m=ROOT/'CURRENT_VALWOOD.json';old_bytes=m.read_bytes();old=json.loads(old_bytes)
assert Path(old['runtime_generation']).name=='staff-combat-polish-2026-11-17','Active generation changed; rebase before promotion'
before=json.loads((N/'review/LIVE_SAVE_BEFORE.json').read_text());assert all(sha(p)==h for p,h in before.items()),'Progress changed during private review'
rollback=N/'rollback';rollback.mkdir(exist_ok=True);(rollback/'CURRENT_VALWOOD.json').write_bytes(old_bytes)
subprocess.run([sys.executable,str(N/'check_unified.py'),'--publish'],check=True)
new=dict(old,runtime_generation=str(N),binary=str(N/'runtime/rpg'),source=str(N/'source-v1'),shared_shaders=str(N/'shared/shaders'),character_textures=str(N/'shared/character-textures'),guide=str(N/'GUIDE.txt'),binary_sha256=sha(N/'runtime/rpg'))
try:
 atomic(m,(json.dumps(new,indent=2)+'\n').encode())
 for mode in ('demo','editor','arena','joust','world'):subprocess.run([str(ROOT/'Launch_Valwood.sh'),mode,'--check-only'],cwd=ROOT,check=True)
 assert all(sha(p)==h for p,h in before.items()),'Progress changed during promotion'
except Exception:
 atomic(m,old_bytes);raise
(N/'review/PROMOTION.json').write_text(json.dumps(dict(passed=True,old_generation=old['runtime_generation'],new_generation=str(N),binary_sha256=new['binary_sha256'],live_saves_preserved=len(before),world_documents_modified=False,shared_shaders_unchanged=True,windows_work_performed=False,rollback=str(rollback)),indent=2)+'\n')
print('Third-person combat flow generation published. Shared shaders, authored scenes and saves retained.')
