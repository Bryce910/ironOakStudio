#!/usr/bin/env python3
"""Validate this immutable Linux surface generation and its shared resources."""
from pathlib import Path
import hashlib,json,sys,argparse
N=Path(__file__).resolve().parent;E=N.parents[1];READY=N/'UNIFIED_READY.json'
TESTS=('road','walking-first','equipment','arena','arena-orcs','joust','opening','editor')
SURFACES=('camp','town','city','grandstone','jungle','wood-floor-diagnostic','armor-clean','armor-blood','armor-first','arena-floor-route')
def sha(p):
 with Path(p).open('rb') as h:return hashlib.file_digest(h,'sha256').hexdigest()
def stamp(p):
 s=Path(p).stat();return [s.st_dev,s.st_ino,s.st_size,s.st_mtime_ns,s.st_ctime_ns]
def need(ok,message):
 if not ok:raise RuntimeError(message)
def resources():
 from launch import configuration,AREAS
 configs={m:configuration(m) for m in ('demo','editor','arena','joust','world')}
 need(len({x[0][0] for x in configs.values()})==1,'Modes use different executables')
 need(all(x[1].get('VALWOOD_CIVILIAN_MODELS')==str(N/'assets/walkers') for x in configs.values()),'Civilian animation bank mismatch')
 need(configs['demo'][2]==configs['editor'][2]==configs['world'][2],'Working world split')
 for project in [x[2] for x in configs.values()]+[p for p,_ in AREAS.values()]:
  need((project/'world.studio').is_file() and (project/'project.valwood').is_file(),'Missing scene '+str(project))
  need((project/'shaders').resolve()==(N/'shared/shaders').resolve(),'Shader resource mismatch')
  need((project/'character-textures').resolve()==(N/'shared/character-textures').resolve(),'Character bank mismatch')
  need((project/'saves').is_dir() and not (project/'saves').is_symlink(),'Invalid progress directory')
 return configs
def publish():
 need(not READY.exists(),'Published generations are immutable')
 configs=resources();build=json.loads((N/'runtime/ORIGIN_BUILD.json').read_text());binary=sha(N/'runtime/rpg')
 need(build['ready'] and build['application_units']==104 and build['sha256']==binary,'Incomplete native build')
 for name,digest in build['source_sha256'].items():need(sha(N/'source-v1'/name)==digest,'Build/source mismatch '+name)
 for name in TESTS:
  r=json.loads((N/'review'/name/'EXECUTION.json').read_text());need(r['passed'] is True and r['binary_sha256']==binary,'Native check failed '+name)
 for name in SURFACES:
  r=json.loads((N/'review'/name/'EXECUTION.json').read_text());need(r['passed'] is True and r['binary_sha256']==binary,'Surface review failed '+name)
 for name in ['ARMOR_REVIEW.json','GROUND_REVIEW.json','INHERITED_ASSETS.json','FINAL_REVIEW.json','VISUAL_REVIEW.json']:
  r=json.loads((N/'review'/name).read_text());need(r.get('passed') is True,'Review failed '+name)
 files=[N/'runtime/rpg',N/'runtime/ORIGIN_BUILD.json',N/'launch.py',Path(__file__),N/'run_native_review.py',N/'GUIDE.txt',E/'run_current_valwood.py']
 files += [N/'source-v1'/name for name in build['source_sha256']]
 files += [p for p in (N/'shared/shaders').rglob('*') if p.is_file()]
 files += [N/'shared/character-textures/textures.manifest']
 files += [p for p in (N/'assets/walkers').glob('*.glb') if p.name.startswith(('village_character_','purchased_character_'))]
 files += [N/'review'/name/'EXECUTION.json' for name in TESTS]
 files += [N/'review'/name/'EXECUTION.json' for name in SURFACES]
 files += [N/'review'/name for name in ['ARMOR_REVIEW.json','GROUND_REVIEW.json','INHERITED_ASSETS.json','FINAL_REVIEW.json','VISUAL_REVIEW.json','IMPLEMENTATION_REVIEW.json','ARMOR_GPU_TEST.log','ARMOR_CONFIG_TEST.log','SCOPE.json']]
 files += [N/'arena-scene-patch/world.studio',N/'arena-scene-patch/ORIGIN.json',N/'tools/review_surfaces.py',N/'tools/write_surface_review.py',N/'tools/promote_surface.py',N/'tools/review_arena_floor_route.py',N/'tools/armor_gpu_test.cpp',N/'tools/armor_wear_test.cpp']
 eq=E/'build/cinematic-studio-2026-11-15/equipment-assets';files += [eq/'HERO_ASSET.json',eq/'loadouts.manifest',*list((eq/'rigs').glob('*.glb'))]
 pins={str(p):sha(p) for p in files}
 READY.write_text(json.dumps({'ready':True,'one_binary':binary,'pins':pins,'stats':{str(p):stamp(p) for p in files},'tests':list(TESTS),'native_only':True,'windows_work_performed':False,'baseline':'animation-polish-2026-11-17','mutable_authoring_and_saves_not_frozen':True},indent=2)+'\n')
def check():
 d=json.loads(READY.read_text());need(d['ready'] is True,'Runtime not ready');resources()
 for name,digest in d['pins'].items():
  s=stamp(name)
  if s[2]<1024*1024 or s!=d['stats'].get(name):need(sha(name)==digest and stamp(name)==s,'Runtime dependency changed '+name)
 manifest=json.loads((E.parent/'CURRENT_VALWOOD.json').read_text())
 need(Path(manifest['runtime_generation'])==N and manifest['binary_sha256']==d['one_binary'],'Current runtime manifest mismatch')
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('--publish',action='store_true');p.add_argument('--check-only',action='store_true');a=p.parse_args()
 try:
  publish() if a.publish else check()
  print('Surface generation verified: one Linux executable and shared resources.')
 except Exception as e:print(str(e),file=sys.stderr);sys.exit(1)
