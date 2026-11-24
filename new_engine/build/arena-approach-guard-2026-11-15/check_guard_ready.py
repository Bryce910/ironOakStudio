#!/usr/bin/env python3
"""Verify the guarded-approach native revision; preserve historical acceptance."""
from pathlib import Path
import hashlib,json,sys
N=Path(__file__).resolve().parent;ROOT=N.parents[2];READY=N/'APPROACH_GUARD_READY.json'
def sha(p):
 with Path(p).open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def read(p):return json.loads(Path(p).read_text())
def need(v,s):
 if not v:raise RuntimeError(s)
def historical():
 for stage,receipt in [('arena-adaptive-combat-2026-11-15','ADAPTIVE_COMBAT_READY.json'),('arena-character-presentation-2026-11-15','CHARACTER_PRESENTATION_READY.json')]:
  document=read(N.parent/stage/receipt);need(document['ready'],'Historical revision incomplete')
  for name,digest in document.get('inputs',document.get('pins',{})).items():
   p=Path(name)
   if p.parent==ROOT and p.name in {'Launch_Valwood_Arena.sh','Launch_Valwood_Arena_Visual.sh'}:p=N/'previous-launchers'/p.name
   need(p.is_file() and sha(p)==digest,'Historical reviewed input changed: '+name)
  for name,target in document.get('project_links',document.get('links',{})).items():need(Path(name).is_symlink() and str(Path(name).resolve())==target,'Historical asset link changed: '+name)
def publish():
 need(not READY.exists(),'Existing acceptance retained');historical();build=read(N/'runtime-native-v1/READY.json');need(build['ready'] and build['native_only'] and not build['Windows_work_performed'],'Native build incomplete')
 source={str(p.relative_to(N/'source-v1')):sha(p) for p in (N/'source-v1').rglob('*') if p.is_file()};need(source==build['source_sha256'],'Build source changed')
 need(read(N/'review/CPU_EXECUTION.json')['passed'],'Final CPU execution missing')
 for f in ['core-result.json','clash-result.json','state-result.json','GAIT_RESULTS.json','APPROACH_GEOMETRY_RESULTS.json']:
  need(read(N/'review'/f)['passed'],'Review incomplete: '+f)
 need(read(N/'review/GAIT_RESULTS.json')['banks']==32,'Full knight/orc gait review incomplete')
 run=read(N/'review/native/NATIVE_EXECUTION.json');raw=read(N/'review/native/NATIVE_ROSTER_RAW.json');need(run['exit_code']==0 and raw['passed'] and raw['matches']==2 and raw['captures']==4,'Native first-approach review incomplete')
 need(run['inputs'][build['binary']]==build['sha256'],'Native review used another binary')
 for report in [run,read(N/'review/CPU_EXECUTION.json')]:
  for path,digest in report['inputs'].items():need(sha(path)==digest,'Executed source changed: '+path)
 pins={}
 def pin(p):pins[str(Path(p).resolve())]=sha(p)
 for p in (N/'source-v1').rglob('*'):
  if p.is_file():pin(p)
 for p in [N/'launch_arena.sh',N/'check_guard_ready.py',N/'GUARDED_APPROACH_NOTES.md',N/'runtime-native-v1/READY.json',N/'runtime-native-v1/native-bin/rpg',ROOT/'Launch_Valwood_Arena.sh',ROOT/'Launch_Valwood_Arena_Visual.sh']:
  pin(p)
 for p in (N/'review').rglob('*'):
  if p.is_file() and not p.is_symlink() and 'private-project' not in p.parts and p.suffix in {'.json','.cpp','.inc','.png'}:pin(p)
 for p in (N/'arena-project/shaders').rglob('*'):
  if p.is_file():pin(p)
 links={str(p):str(p.resolve()) for p in (N/'arena-project').iterdir() if p.is_symlink()}
 READY.write_text(json.dumps(dict(ready=True,native_only=True,windows_work_performed=False,inputs=pins,project_links=links,scope='Guarded approach for 18 knights and 14 orcs; entering perception/real contacts; complete-frame movement and early defeat ownership. First-approach native Human/Orc overhead source-steel checks, not universal block odds.'),indent=2,sort_keys=True)+'\n')
if __name__=='__main__':
 try:
  if '--publish' in sys.argv:publish()
  historical();data=read(READY);need(data['ready'],'Guarded approach not reviewed')
  for name,digest in data['inputs'].items():need(Path(name).is_file() and sha(name)==digest,'Reviewed input changed: '+name)
  for name,target in data['project_links'].items():need(Path(name).is_symlink() and str(Path(name).resolve())==target,'Asset bank changed: '+name)
  print('PASS: native arena guarded approach, real steel contact and complete-frame movement.')
 except (OSError,KeyError,ValueError,RuntimeError)as e:sys.exit(str(e))
