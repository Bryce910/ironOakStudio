#!/usr/bin/env python3
from pathlib import Path
import argparse,hashlib,json,sys
N=Path(__file__).resolve().parent;E=N.parents[1];READY=N/'UNIFIED_READY.json'
def sha(p):
 with Path(p).open('rb') as h:return hashlib.file_digest(h,'sha256').hexdigest()
def stamp(p):
 s=Path(p).stat();return [s.st_dev,s.st_ino,s.st_size,s.st_mtime_ns,s.st_ctime_ns]
def need(ok,why):
 if not ok:raise RuntimeError(why)
def resources():
 from launch import AREAS
 for project in (E/'projects/valwood_current',N/'arena-project',N/'joust-project',*[p for p,_ in AREAS.values()]):
  need((project/'project.valwood').is_file() and (project/'world.studio').is_file(),'Missing mode project: '+str(project))
  need((project/'shaders').resolve()==(N/'shared/shaders').resolve(),'Mode uses different shaders: '+str(project))
  need((project/'character-textures').resolve()==(N/'shared/character-textures').resolve(),'Mode uses different character textures: '+str(project))
  need((project/'saves').is_dir() and not (project/'saves').is_symlink(),'Missing independent durable saves: '+str(project))
 need((E/'projects/valwood_current/opening/road-traffic.settings').is_file(),'Busy road definition missing')
def publish():
 need(not READY.exists(),'Published runtime generation is immutable')
 from launch import configuration
 resources();
 for review in ('review/cpu/REVIEW.json','review/CONFIGURATION_REVIEW.json','review/SURFACE_LIBRARY_REPAIR.json','review/ROAD_PROP_MERGE.json','review/AREA_PROJECT_MIGRATION.json'):
  need(json.loads((N/review).read_text())['passed'] is True,'CPU/configuration check failed: '+review)
 plan=json.loads((N/'PREPARED.json').read_text());need(sha(N/'runtime/rpg')==plan['runtime_sha256'],'Common binary differs from accepted runtime')
 compiled=json.loads((N/'runtime/ORIGIN_BUILD.json').read_text())
 need(compiled['sha256']==sha(N/'runtime/rpg') and compiled['application_units']==104,'Incomplete compiled runtime')
 for name,h in compiled['source_sha256'].items():need(sha(N/'source-v1'/name)==h,'Source/build mismatch: '+name)
 tests=('opening','skip','editor','arena','joust','journey','scenery','road','arena-orcs','equipment','area-city','area-grandstone','area-showcase','area-atlas')
 for name in tests:
  review=json.loads((N/'review'/name/'EXECUTION.json').read_text())
  need(review['passed'] is True and review['binary_sha256']==plan['runtime_sha256'],'Native integration failed: '+name)
 modes={m:configuration(m) for m in ('demo','editor','arena','joust','world')}
 need(len({v[0][0] for v in modes.values()})==1,'Multiple runtime binaries')
 need(modes['demo'][2]==modes['editor'][2]==modes['world'][2],'World content is split')
 files=[N/'runtime/rpg',N/'runtime/ORIGIN_BUILD.json',N/'PREPARED.json',N/'launch.py',Path(__file__),N/'run_native_review.py',N/'shared/character-textures/textures.manifest']
 files += [p for p in (N/'shared/shaders').rglob('*') if p.is_file()]
 eq=E/'build/cinematic-studio-2026-11-15/equipment-assets'
 files += [eq/'HERO_ASSET.json',eq/'loadouts.manifest']
 files += list((eq/'rigs').glob('*.glb'))
 files += [N/'source-v1'/p for p in compiled['source_sha256']]
 files += [N/'review'/name/'EXECUTION.json' for name in tests]
 files += list((N/'review/fixtures').glob('*'))
 files += [N/'check_story_cpu.py',N/'review/cpu/world_first_journey_current_policy_test.cpp',N/'review/cpu/world_first_journey_current_policy_test.log',N/'review/cpu/legacy-guard-policy-failure.log',N/'review/cpu/legacy-fixed-hit-count-assumption.log',N/'review/scenery/POSTCHECK_RESOLVED.json',N/'check_story_cpu.py']
 files += [N/'review/FINAL_REVIEW.json',N/'review/FINAL_REVIEW.md',N/'review/VISUAL_REVIEW.json']
 files += [N/'review/cpu/REVIEW.json',N/'review/CONFIGURATION_REVIEW.json',N/'review/SURFACE_LIBRARY_REPAIR.json',N/'review/ROAD_PROP_MERGE.json',N/'review/AREA_PROJECT_MIGRATION.json',N/'UNIFIED_RUNTIME_GUIDE.md',N/'WRAPPERS.json']
 files += list((N/'previous-launchers').glob('*'))
 files += [E.parent/name for name in json.loads((N/'WRAPPERS.json').read_text())]
 files += [E/'run_current_valwood.py',E.parent/'CURRENT_VALWOOD.json']
 pins={str(p):sha(p) for p in files}
 READY.write_text(json.dumps(dict(ready=True,native_only=True,windows_work_performed=False,one_binary=plan['runtime_sha256'],pins=pins,stats={str(p):stamp(p) for p in files},authoring_project=str(modes['editor'][2]),mutable_authoring_and_saves_not_frozen=True,tests=list(tests)),indent=2)+'\n')
def check():
 d=json.loads(READY.read_text());need(d['ready'] is True,'Unified runtime is not ready');resources()
 for name,digest in d['pins'].items():
  before=stamp(name)
  if before[2]<1024*1024 or before!=d['stats'].get(name):need(sha(name)==digest and stamp(name)==before,'Changed runtime dependency: '+name)
if __name__=='__main__':
 ap=argparse.ArgumentParser();ap.add_argument('--publish',action='store_true');ap.add_argument('--check-only',action='store_true');a=ap.parse_args()
 try:
  publish() if a.publish else check()
  print('Unified Valwood ready: one runtime, shared shaders, textures and working world.')
 except Exception as e:print('Unified check failed:',e,file=sys.stderr);sys.exit(1)
