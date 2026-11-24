#!/usr/bin/env python3
import sys
sys.dont_write_bytecode=True
from pathlib import Path
import json,hashlib,subprocess
N=Path(__file__).resolve().parent;BASE=N.parent/'arena-adaptive-combat-2026-11-15';P=N/'arena-project';ROOT=N.parents[2];READY=N/'CHARACTER_PRESENTATION_READY.json'
def sha(p):
 with Path(p).open('rb')as f:return hashlib.file_digest(f,'sha256').hexdigest()
def read(p):return json.loads(Path(p).read_text())
def need(value,text):
 if not value:raise RuntimeError(text)
def mutable(p):return p.parts[0] in {'saves','settings','recovery','autosaves'} or p.name in {'graphics.settings','combat.settings','preview.settings','controller.settings','arena_player_character.cfg','arena_orc_character.cfg','studio-layout.ini','imgui.ini'} or p.name.endswith(('.lock','.tmp','.bak','.recovery','.autosave'))
def publish():
 need(not READY.exists(),'Existing visual acceptance retained')
 pins={};links={}
 def pin(p):pins[str(Path(p).resolve())]=sha(p)
 source=BASE/'runtime-native-v1/native-bin/rpg';pin(source)
 for label in ('before-v2','final'):
  d=N/'review'/label;run=read(d/'EXECUTION.json');raw=read(d/'NATIVE_CLASH_RAW.json')
  need(run['exit_code']==0 and raw['passed'] and raw['reviewed_cases']==2 and raw['captures']==8,'Native visual/contact review incomplete')
  for p,digest in run['inputs'].items():need(sha(p)==digest,'Executed visual input changed: '+p);pins[p]=digest
  need(run['inputs'][str(source)]==sha(source),'Visual review used another binary')
  for p in (d/'EXECUTION.json',d/'NATIVE_CLASH_RAW.json',d/'native.log'):pin(p)
  for p in d.glob('*.png'):pin(p)
 need(read(N/'review/before-v2/NATIVE_CLASH_RAW.json')['cases']==read(N/'review/final/NATIVE_CLASH_RAW.json')['cases'],'Visual changes altered observed clash outcomes')
 portrait=read(N/'review/PORTRAITS_REVIEW.json');need(portrait['passed'] and portrait['count']==18,'Eighteen portraits not reviewed')
 for r in portrait['receipts']:
  need(sha(r['producer'])==r['producer_sha256'] and sha(r['render']['source'])==r['render']['source_sha256'] and sha(r['render']['portrait'])==r['render']['portrait_sha256'],'Portrait source/output changed')
 for p in P.rglob('*'):
  if p.is_symlink():links[str(p)]=str(p.resolve());continue
  if p.is_file() and not mutable(p.relative_to(P)):pin(p)
 for p in [Path(__file__),N/'launch_arena.sh',ROOT/'Launch_Valwood_Arena_Visual.sh',N/'VISUAL_NOTES.txt',N/'CHANGE_SCOPE.json',N/'render_portraits.py',N/'review/PORTRAITS_REVIEW.json',N/'review/VISUAL_REVIEW.json',BASE/'ADAPTIVE_COMBAT_READY.json']:
  pin(p)
 pin(P/'environments/arena-overcast.hdr')
 READY.write_text(json.dumps(dict(ready=True,native_only=True,Windows_work_performed=False,binary_unchanged=True,geometry_unchanged=True,combat_unchanged=True,pins=pins,links=links,scope='Arena character presentation first pass. Original-source 512x640 picker renders; actor world/first-person finish and actor normal-mapped IBL; bounded first/third-person native steel-contact comparison. No full-demo, full roster motion or sustained-FPS claim.'),indent=2,sort_keys=True)+'\n')
try:
 subprocess.run([sys.executable,str(BASE/'check_ready.py'),'--check-only'],check=True)
 if '--publish'in sys.argv:publish()
 data=read(READY);need(data['ready'],'Visual candidate not ready')
 for p,digest in data['pins'].items():need(Path(p).is_file() and sha(p)==digest,'Visual file changed: '+p)
 for p,target in data['links'].items():need(Path(p).is_symlink() and str(Path(p).resolve())==target,'Visual asset link changed: '+p)
 print('PASS character presentation: 18 improved portraits and reviewed native first/third-person shading.')
except (OSError,ValueError,KeyError,RuntimeError,subprocess.CalledProcessError)as error:sys.exit(str(error))
