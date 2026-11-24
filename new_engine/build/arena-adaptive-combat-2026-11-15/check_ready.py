#!/usr/bin/env python3
"""Verify this native combat revision without freezing user progress."""
import sys
sys.dont_write_bytecode=True
from pathlib import Path
import hashlib,json,subprocess
N=Path(__file__).resolve().parent;E=N.parents[1];ROOT=E.parent;P=N/'arena-project';READY=N/'ADAPTIVE_COMBAT_READY.json'
def sha(p):
 with Path(p).open('rb')as f:return hashlib.file_digest(f,'sha256').hexdigest()
def read(p):return json.loads(Path(p).read_text())
def require(ok,message):
 if not ok:raise RuntimeError(message)
def mutable(p):
 return p.parts[0] in {'saves','settings','recovery','autosaves'} or p.name in {'graphics.settings','combat.settings','preview.settings','controller.settings','arena_player_character.cfg','arena_orc_character.cfg','studio-layout.ini','imgui.ini'} or p.name.endswith(('.lock','.tmp','.bak','.recovery','.autosave'))
def publish():
 require(not READY.exists(),'Existing acceptance is retained')
 r=N/'runtime-native-v1';build=read(r/'READY.json');plan=read(r/'BUILD_PLAN.json');s=N/'source-v1'
 require(build['ready'] and build['application_units']==104 and build['native_only'] and not build['Windows_work_performed'],'Native build incomplete')
 inventory={str(p.relative_to(s)):sha(p) for p in s.rglob('*') if p.is_file()}
 require(inventory==build['source_sha256']==plan['source_sha256'],'Build source closure changed')
 require(sha(build['binary'])==build['sha256'],'Binary changed')
 core=read(N/'review/core-result.json');clash=read(N/'review/clash-result.json');geometry=read(N/'review/difficulty/GEOMETRY_REVIEW.json')
 require(core['passed'] and clash['passed'] and geometry['passed'],'CPU checks incomplete')
 native=N/'review/native-capacity';raw=read(native/'NATIVE_ARENA_SETTINGS_RAW.json');execution=read(native/'NATIVE_EXECUTION.json')
 require(raw['passed'] and raw['matches']==2 and raw['captures']==4 and execution['exit_code']==0,'Native encounter check incomplete')
 require(execution['inputs'][str(r/'native-bin/rpg')]==build['sha256'],'Native test used another runtime')
 for report in (geometry,execution):
  for name,digest in report['inputs'].items():require(sha(name)==digest,'Executed review input changed: '+name)
 pins={}
 def pin(p):pins[str(Path(p).resolve())]=sha(p)
 for p in s.rglob('*'):
  if p.is_file():pin(p)
 for p in P.rglob('*'):
  if p.is_symlink():continue
  if p.is_file() and not mutable(p.relative_to(P)):pin(p)
 for p in [Path(__file__),N/'launch_arena.sh',ROOT/'Launch_Valwood_Arena.sh',N/'COMBAT_NOTES.txt',r/'READY.json',r/'BUILD_PLAN.json',r/'native-bin/rpg',N/'review/test_adaptive.cpp',N/'review/test_clash_core.cpp',N/'review/core-result.json',N/'review/clash-result.json',N/'review/difficulty/GEOMETRY_REVIEW.json',native/'NATIVE_EXECUTION.json',native/'NATIVE_ARENA_SETTINGS_RAW.json',N/'review/VISUAL_REVIEW.json']:
  pin(p)
 for path,digest in geometry['inputs'].items():pins[path]=digest
 for row in geometry['runs']:pins[row['log']]=row['log_sha256']
 for p in native.glob('*.png'):pin(p)
 links={str(p):str(p.resolve()) for p in P.iterdir() if p.is_symlink()}
 READY.write_text(json.dumps(dict(ready=True,native_only=True,windows_work_performed=False,inputs=pins,project_links=links,scope='Player timing, arena defense/initiative, real source-geometry contact checks, native 24+6 encounters. Manual controller feel remains a playtest judgment.'),indent=2,sort_keys=True)+'\n')
if __name__=='__main__':
 try:
  parent=E/'build/arena-settings-restoration-2026-11-15/check_arena_ready.py'
  subprocess.run([sys.executable,str(parent),'--check-only'],check=True)
  if '--publish' in sys.argv:publish()
  data=read(READY);require(data['ready'],'Revision not ready')
  for name,digest in data['inputs'].items():require(Path(name).is_file() and sha(name)==digest,'Reviewed file changed: '+name)
  for name,target in data['project_links'].items():require(Path(name).is_symlink() and str(Path(name).resolve())==target,'Project bank changed: '+name)
  print('PASS adaptive native arena: quicker swings, physical guard reads and varied enemy initiative.')
 except (OSError,ValueError,KeyError,RuntimeError,subprocess.CalledProcessError)as error:sys.exit(str(error))
