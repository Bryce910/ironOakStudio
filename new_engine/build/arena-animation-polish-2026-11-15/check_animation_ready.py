#!/usr/bin/env python3
"""Validate the independently reviewed native animation arena and owned assets."""
from pathlib import Path
import argparse,hashlib,json,re,subprocess,sys
N=Path(__file__).resolve().parent;ROOT=N.parents[2];BASE=N.parent/'arena-blade-clash-2026-11-14';READY=N/'ANIMATION_READY.json'
REVIEWS=[
 'clash-review/CLASH_CORE_REVIEW.json',
 'clash-review/CLASH_FEEDBACK_REVIEW.json',
 'clash-review/PHYSICAL_CLASH_SOURCE_CPU_REVIEW.json',
 'animation-review/ANIMATION_PRESENTATION_CPU_REVIEW.json',
 'animation-review/NPC_REACTION_CONTINUITY_CPU_REVIEW.json',
 'animation-review/NPC_REACTION_CORE_WINDOW_CPU_REVIEW.json',
 'animation-review/NPC_REAL_STAGGER_INTEGRATION_CPU_REVIEW.json',
 'animation-review/NPC_REACTION_BRIDGE_SEVER_DEATH_GPU_REVIEW.json',
 'animation-review/ANIMATION_COVERAGE_INVENTORY.json',
 'hero-polish-review/HERO_ANIMATION_POLISH_CPU_REVIEW.json',
 'hero-polish-review/HERO_ANIMATION_POLISH_GPU_REVIEW.json',
 'hero-polish-review/ORIGINAL_WORLD_NORMALS_REVIEW.json',
 'animation-review/ANIMATION_FRAME_ANALYSIS.json',
 'animation-review/HERO_FP_INDEPENDENT_VISUAL_REVIEW.json',
 'animation-review/ANIMATION_NATIVE_REVIEW.json',
 'animation-review/VISUAL_REVIEW.json',
]
def require(v,m):
 if not v:raise RuntimeError(m)
def read(p):return json.loads(Path(p).read_text())
def sha(p):
 with Path(p).open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def stat(p):
 s=Path(p).stat();return [s.st_dev,s.st_ino,s.st_size,s.st_mtime_ns,s.st_ctime_ns]
def main():
 p=argparse.ArgumentParser();p.add_argument('--write',action='store_true');p.add_argument('--check-only',action='store_true');p.add_argument('--verify-full',action='store_true');a=p.parse_args()
 if a.write:
  subprocess.run([sys.executable,str(BASE/'check_blade_clash_ready.py'),'--check-only'],check=True)
  base=read(BASE/'BLADE_CLASH_READY.json');inputs=dict(base['inputs']);frozen=dict(base['frozen_stats'])
  def pin(p,h=None):
   p=str(Path(p).resolve());old=inputs.get(p);actual=old if old is not None and stat(p)==frozen.get(p) else sha(p)
   require(h is None or actual==h,'Changed reviewed dependency: '+p);require(old is None or old==actual,'Accepted dependency changed: '+p)
   inputs[p]=actual;frozen[p]=stat(p)
  def evidence(x):
   if isinstance(x,list):
    for v in x:evidence(v)
   elif isinstance(x,dict):
    if x and all(isinstance(k,str) and Path(k).is_absolute() and isinstance(v,str) and re.fullmatch('[0-9a-f]{64}',v) for k,v in x.items()):
     for p,h in x.items():pin(p,h)
    if isinstance(x.get('path'),str) and Path(x['path']).is_absolute() and isinstance(x.get('sha256'),str):pin(x['path'],x['sha256'])
    for v in x.values():evidence(v)
  pin(BASE/'BLADE_CLASH_READY.json');native=read(N/'runtime-native-v1/READY.json')
  require(native.get('ready') and native.get('native_only') and not native.get('Windows_work_performed') and native.get('application_units')==103,'Native build closure incomplete')
  binary=N/'runtime-native-v1/native-bin/rpg';pin(binary,native['sha256']);pin(N/'runtime-native-v1/READY.json')
  for p,h in native['source_sha256'].items():pin(N/'source-v1'/p,h)
  for name in REVIEWS:
   r=read(N/name);require(r.get('passed') is True,'Review has not passed: '+name);pin(N/name);evidence(r)
   if name.endswith('/ANIMATION_NATIVE_REVIEW.json'):
    require(r['inputs'].get(str(binary))==native['sha256'],'Native film uses a different executable')
    raw=r.get('raw',{});require(raw.get('passed') and raw.get('reviewed_cases')==30 and raw.get('captures')==120,'Complete native contact rehearsal required')
    matching=[row for row in raw.get('cases',[]) if row.get('mode')==5]
    require(len(matching)==10 and all(row.get('metal_bursts',0)>0 and row.get('damage')==0 and not row.get('air_from_actual_clear_window') for row in matching),'Every matching guard must intercept actual steel')
   if name.endswith('/PHYSICAL_CLASH_SOURCE_CPU_REVIEW.json'):require(len(r.get('reviewed_ids',[]))==32 and r.get('legacy_pairs_unchanged')==217,'Both full NPC rosters and217source pairs required')
  for p in [Path(__file__),N/'launch_animation_polish.sh',ROOT/'Launch_Valwood_Animation_Polish.sh',N/'ANIMATION_NOTES.txt',N/'HERO_ASSET.json',N/'SOURCE_ORIGIN.json',N/'arena-project/arena.config']:pin(p)
  for p in (N/'shaders').rglob('*'):
   if p.is_file():pin(p)
  for p in list(inputs):require(stat(p)==frozen[p],'Dependency changed during publication: '+p)
  r={'schema':'valwood.animation-polish.ready.v1','ready':True,'native_only':True,'native_sha256':native['sha256'],'player_knights':18,'enemy_knights':18,'enemy_orcs':14,'physical_controller_feel_verified':False,'sustained_hardware_fps_verified':False,'inputs':inputs,'frozen_stats':frozen}
  temporary=READY.with_suffix('.tmp');temporary.write_text(json.dumps(r,indent=2,sort_keys=True)+'\n');temporary.replace(READY)
 require(READY.is_file(),'Animation polish build is still being reviewed');r=read(READY);require(r.get('ready') and r.get('native_only'),'Animation review incomplete')
 for p,h in r['inputs'].items():
  if a.verify_full or stat(p)!=r['frozen_stats'][p] or Path(p).stat().st_size<1024*1024:require(sha(p)==h,'Changed since animation verification: '+p)
 print('Animation arena ready:18 playable knights, 18 knight opponents, 14 orcs; source-coherent reactions, Bow return, closed FP sleeves and melee framing.')
if __name__=='__main__':
 try:main()
 except (OSError,ValueError,KeyError,RuntimeError,subprocess.CalledProcessError) as e:print(str(e),file=sys.stderr);sys.exit(1)
