#!/usr/bin/env python3
from pathlib import Path
import json,hashlib,sys,importlib.util
N=Path(__file__).resolve().parent;E=N.parents[1];ROOT=E.parent;B=N.parent/'arena-material-quality-2026-11-15';READY=N/'WOODLAND_READY.json'
def sha(p):
 with Path(p).open('rb')as f:return hashlib.file_digest(f,'sha256').hexdigest()
def read(p):return json.loads(Path(p).read_text())
def need(v,s):
 if not v:raise RuntimeError(s)
def inherited():
 spec=importlib.util.spec_from_file_location('material_review',B/'check_material_ready.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m);m.baseline();d=read(B/'MATERIAL_QUALITY_READY.json');need(d['ready'],'Character rendering baseline incomplete')
 for name,digest in d['inputs'].items():
  p=Path(name)
  if p.parent==ROOT and p.name in('Launch_Valwood_Arena.sh','Launch_Valwood_Arena_Visual.sh'):p=N/'previous-launchers'/p.name
  need(p.is_file()and sha(p)==digest,'Accepted character input changed: '+name)
 for name,target in d['project_links'].items():need(Path(name).is_symlink()and str(Path(name).resolve())==target,'Accepted character asset link changed: '+name)
def publish():
 need(not READY.exists(),'Existing receipt is immutable');inherited();b=read(N/'runtime-native-v1/READY.json');need(b['ready']and b['native_only']and not b['Windows_work_performed'],'Native renderer incomplete')
 need({str(p.relative_to(N/'source-v1')):sha(p)for p in(N/'source-v1').rglob('*')if p.is_file()}==b['source_sha256'],'Build source changed')
 need(read(N/'review/LAYOUT_CHECK.json')['passed'],'Arena clearance failed')
 need(read(N/'review/TERRAIN_EDGE_CHECK.json')['passed'],'Terrain boundary seam check failed')
 for label,report in [('approach-high','NATIVE_ROSTER_RAW.json'),('high','NATIVE_CLASH_RAW.json'),('capacity-low','NATIVE_ARENA_SETTINGS_RAW.json')]:
  run=read(N/'review'/label/'EXECUTION.json');need(run['exit_code']==0 and read(N/'review'/label/report)['passed'],'Native rehearsal failed: '+label)
  for name,h in run['inputs'].items():need(sha(name)==h,'Executed input changed: '+name)
 need(read(N/'review/SCENERY_VISUAL_REVIEW.json')['passed'],'Visual review incomplete')
 pins={}
 def pin(p):pins[str(p.absolute())]=sha(p)
 for folder in('source-v1','arena-project/scenery','arena-project/shaders'):
  for p in(N/folder).rglob('*'):
   if p.is_file():pin(p)
 for name in('compose_arena.py','check_woodland_ready.py','launch_arena.sh','PLACEMENTS.json','LAYOUT.json','WOODLAND_NOTES.md','arena-project/ground/showcase.height','arena-project/world.studio','arena-project/city.physics','arena-project/combat_arena.json','runtime-native-v1/READY.json','runtime-native-v1/native-bin/rpg','review/check_layout.py','review/run_matched.py','review/run_capacity_low.py','review/run_environment.py','review/analyze_performance.py','review/LAYOUT_CHECK.json','review/TERRAIN_EDGE_CHECK.json','review/libarena_atlas.so','review/SCENERY_VISUAL_REVIEW.json','review/PERFORMANCE_COMPARISON.json'):pin(N/name)
 for label in('approach-high','high','capacity-low','environment','environment-baseline'):
  for p in(N/'review'/label).iterdir():
   if p.is_file():pin(p)
 for name in('Launch_Valwood_Arena.sh','Launch_Valwood_Arena_Visual.sh'):pin(ROOT/name)
 links={str(p):str(p.resolve())for p in(N/'arena-project').iterdir()if p.is_symlink()}
 READY.write_text(json.dumps(dict(ready=True,native_only=True,windows_work_performed=False,inputs=pins,project_links=links,scope='Woodland arena scenery, visible and shadowed in gameplay. Inherited combat and selectable character textures preserved. Bounded native review; no AAA or sustained maximum-crowd frame-rate guarantee.'),indent=2,sort_keys=True)+'\n')
if __name__=='__main__':
 try:
  if '--publish'in sys.argv:publish()
  inherited();d=read(READY);need(d['ready'],'Arena scenery is not accepted')
  for name,h in d['inputs'].items():need(Path(name).is_file()and sha(name)==h,'Reviewed arena input changed: '+name)
  for name,t in d['project_links'].items():need(Path(name).is_symlink()and str(Path(name).resolve())==t,'Arena asset link changed: '+name)
  print('PASS: reviewed native woodland arena, clear combat floor, character texture options and inherited combat.')
 except(OSError,RuntimeError,ValueError,KeyError)as e:sys.exit(str(e))
