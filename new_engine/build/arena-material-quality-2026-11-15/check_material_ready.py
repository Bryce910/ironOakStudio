#!/usr/bin/env python3
"""Verify this native character presentation; preserve accepted combat history."""
from pathlib import Path
import hashlib,json,sys,importlib.util
N=Path(__file__).resolve().parent;ROOT=N.parents[2];BASE=N.parent/'arena-approach-guard-2026-11-15';READY=N/'MATERIAL_QUALITY_READY.json'
def sha(p):
 with Path(p).open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def read(p):return json.loads(Path(p).read_text())
def need(v,message):
 if not v:raise RuntimeError(message)
def baseline():
 spec=importlib.util.spec_from_file_location('guarded_baseline',BASE/'check_guard_ready.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m);m.historical()
 d=read(BASE/'APPROACH_GUARD_READY.json');need(d['ready'],'Previous combat revision incomplete')
 for name,digest in d['inputs'].items():
  p=Path(name)
  if p.parent==ROOT and p.name in ('Launch_Valwood_Arena.sh','Launch_Valwood_Arena_Visual.sh'):p=N/'previous-launchers'/p.name
  need(p.is_file()and sha(p)==digest,'Accepted combat input changed: '+name)
 for name,target in d['project_links'].items():need(Path(name).is_symlink()and str(Path(name).resolve())==target,'Accepted asset link changed: '+name)
def publish():
 need(not READY.exists(),'Existing acceptance preserved');baseline()
 b=read(N/'runtime-native-v1/READY.json');need(b['ready']and b['native_only']and not b['Windows_work_performed'],'Native build incomplete')
 source={str(p.relative_to(N/'source-v1')):sha(p)for p in (N/'source-v1').rglob('*')if p.is_file()};need(source==b['source_sha256'],'Source changed after compilation')
 checks=read(N/'review/FINAL_CHECKS.json');need([r['label']for r in checks]==['baseline','high','low','medium','approach-high','settings-high','capacity-high']and all(r['exit_code']==0 for r in checks),'Native final checks incomplete')
 for label in ['baseline','high','low','medium','approach-high','settings-high','capacity-high']:
  run=read(N/'review'/label/'EXECUTION.json');need(run['exit_code']==0,'Native check failed: '+label)
  for name,digest in run['inputs'].items():need(sha(name)==digest,'Executed input changed: '+name)
  if label!='baseline':need(run['inputs'][b['binary']]==b['sha256'],'Executed binary differs: '+label)
 for label in ['baseline','high','low','medium']:need(read(N/'review'/label/'NATIVE_CLASH_RAW.json')['passed'],'Contact check failed')
 need(read(N/'review/approach-high/NATIVE_ROSTER_RAW.json')['passed'],'Guarded entry failed')
 need(read(N/'review/capacity-high/NATIVE_ARENA_SETTINGS_RAW.json')['passed'],'Maximum crowd check failed')
 need(read(N/'review/TEXTURE_COVERAGE.json')['passed'],'Actual roster texture coverage incomplete')
 need(read(N/'review/ui-change/UI_NATIVE_RESULT.json')['native_save_to_medium'],'Native texture Apply/save failed')
 need(read(N/'review/ui-native/UI_NATIVE_RESULT.json')['passed'] and not read(N/'review/ui-native/UI_NATIVE_RESULT.json')['visual_review_pending'],'Actual F10 panel not reviewed')
 need((N/'review/TEXTURE_TEST.txt').read_text().startswith('PASS')and(N/'review/SETTINGS_TEST.txt').read_text().startswith('PASS'),'CPU checks incomplete')
 pins={}
 def pin(p):pins[str(Path(p).absolute())]=sha(p)
 for p in (N/'source-v1').rglob('*'):
  if p.is_file():pin(p)
 for p in (N/'arena-project/shaders').rglob('*'):
  if p.is_file():pin(p)
 for p in (N/'character-textures').rglob('*'):
  if p.is_file()and 'high-source' not in p.parts:pin(p)
 for label in ['baseline','high','low','medium','approach-high','settings-high','capacity-high']:
  for p in (N/'review'/label).iterdir():
   if p.is_file():pin(p)
 for p in [N/'review/TEXTURE_LEVEL_MEMORY.json',N/'review/comparison-first.png',N/'review/FINAL_CHECKS.json',N/'review/TEXTURE_COVERAGE.json',N/'review/TEXTURE_SOURCE_REVIEW.json',N/'review/TEXTURE_TEST.txt',N/'review/SETTINGS_TEST.txt',N/'review/TEXTURE_REJECTION_TEST.txt',N/'review/ui-native/UI_NATIVE_RESULT.json',N/'review/ui-native/graphics-panel.png',N/'review/ui-change/UI_NATIVE_RESULT.json',N/'review/ui-change/graphics-panel.png',N/'review/run_native_ui.py',N/'review/inspect_settings_x11.py',N/'review/run_matched.py',N/'review/run_final_checks.py',N/'review/libtexture_observer.so',N/'review/gl_texture_observer.cpp',N/'launch_arena.sh',N/'check_material_ready.py',N/'MATERIAL_QUALITY_NOTES.md',N/'arena-project/world.studio',N/'prepare_character_textures.py',N/'runtime-native-v1/READY.json',N/'runtime-native-v1/native-bin/rpg',BASE/'APPROACH_GUARD_READY.json',ROOT/'Launch_Valwood_Arena.sh',ROOT/'Launch_Valwood_Arena_Visual.sh']:
  pin(p)
 links={str(p):str(p.resolve())for p in(N/'arena-project').iterdir()if p.is_symlink()}
 READY.write_text(json.dumps(dict(ready=True,native_only=True,windows_work_performed=False,inputs=pins,project_links=links,scope='Character texture selection with restart, owned 2K maps, calibrated arena material lighting, explicit actor occlusion; accepted physical combat inherited unchanged; bounded visual/memory rehearsal, not AAA certification'),indent=2,sort_keys=True)+'\n')
if __name__=='__main__':
 try:
  if '--publish'in sys.argv:publish()
  baseline();d=read(READY);need(d['ready'],'Character presentation not reviewed')
  for name,digest in d['inputs'].items():need(Path(name).is_file()and sha(name)==digest,'Reviewed character input changed: '+name)
  for name,target in d['project_links'].items():need(Path(name).is_symlink()and str(Path(name).resolve())==target,'Character asset bank changed: '+name)
  print('PASS: reviewed native arena character lighting, selectable 512/1K/2K textures and inherited combat.')
 except (OSError,KeyError,ValueError,RuntimeError)as e:sys.exit(str(e))
