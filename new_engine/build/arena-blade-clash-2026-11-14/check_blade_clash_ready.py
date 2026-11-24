#!/usr/bin/env python3
"""Gate the reviewed Linux clash build; reuse frozen, verified asset content."""
from pathlib import Path
import argparse, hashlib, json, re, subprocess, sys
N=Path(__file__).resolve().parent
ROOT=N.parents[2]
BASE=N.parent/'arena-character-roster-2026-11-14'
READY=N/'BLADE_CLASH_READY.json'
REVIEWS=[
 'clash-review/CLASH_CORE_REVIEW.json',
 'clash-review/CLASH_FEEDBACK_REVIEW.json',
 'clash-review/PHYSICAL_CLASH_SOURCE_CPU_REVIEW.json',
 'clash-review/NPC_STRUCTURAL_SURFACE_CPU_REVIEW.json',
 'clash-review/HERO_PHYSICAL_CLASH_CPU_REVIEW.json',
 'clash-review/GUARD_TARGET_REVIEW.json',
 'clash-review/LIVE_GUARD_DEPLOYMENT_CPU_REVIEW.json',
 'clash-review/GUARD_FORECAST_BUDGET_REVIEW.json',
 'clash-review/CLASH_BRIDGE_SEVER_DEATH_GPU_REVIEW.json',
 'clash-review/BLADE_CLASH_NATIVE.json',
 'clash-review/VISUAL_REVIEW.json',
]
def require(v,msg):
 if not v: raise RuntimeError(msg)
def read(p): return json.loads(Path(p).read_text())
def sha(p):
 with Path(p).open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def stat(p):
 s=Path(p).stat();return [s.st_dev,s.st_ino,s.st_size,s.st_mtime_ns,s.st_ctime_ns]
def main():
 parser=argparse.ArgumentParser();parser.add_argument('--write',action='store_true');parser.add_argument('--check-only',action='store_true');parser.add_argument('--verify-full',action='store_true');a=parser.parse_args()
 if a.write:
  # This checks the accepted content fingerprints, rehashing anything changed.
  # Unchanged multi-gigabyte owned model banks do not need duplicate hashing.
  subprocess.run([sys.executable,str(BASE/'check_character_ready.py'),'--check-only'],check=True)
  base=read(BASE/'ARENA_CHARACTERS_READY.json');inputs=dict(base['inputs']);frozen=dict(base['frozen_stats'])
  def pin(p,h=None):
   p=str(Path(p).resolve());old=inputs.get(p)
   if old is not None and stat(p)==frozen.get(p):actual=old
   else:actual=sha(p)
   require(h is None or actual==h,'Changed reviewed dependency: '+p)
   require(old is None or old==actual,'Accepted dependency changed: '+p)
   inputs[p]=actual;frozen[p]=stat(p)
  def evidence(x):
   if isinstance(x,list):
    for v in x:evidence(v)
   elif isinstance(x,dict):
    if x and all(isinstance(k,str) and Path(k).is_absolute() and isinstance(v,str) and re.fullmatch('[0-9a-f]{64}',v) for k,v in x.items()):
     for p,h in x.items():pin(p,h)
    if isinstance(x.get('path'),str) and Path(x['path']).is_absolute() and isinstance(x.get('sha256'),str):pin(x['path'],x['sha256'])
    for v in x.values():evidence(v)
  pin(BASE/'ARENA_CHARACTERS_READY.json')
  native=read(N/'runtime-native-v1/READY.json')
  require(native.get('ready') and native.get('native_only') and not native.get('Windows_work_performed') and native.get('application_units')==103,'Native build closure is incomplete')
  binary=N/'runtime-native-v1/native-bin/rpg';pin(binary,native['sha256']);pin(N/'runtime-native-v1/READY.json')
  for p,h in native['source_sha256'].items():pin(N/'source-v1'/p,h)
  for name in REVIEWS:
   p=N/name;r=read(p);require(r.get('passed') is True,'Review has not passed: '+name);pin(p);evidence(r)
   if name.endswith('/BLADE_CLASH_NATIVE.json'):
    require(r['inputs'].get(str(binary))==native['sha256'],'Native review uses a different binary')
    raw=r.get('raw',{});matching=[row for row in raw.get('cases',[]) if row.get('mode')==5]
    require(raw.get('passed') and raw.get('reviewed_cases')==30 and raw.get('captures')==120 and len(matching)==10,'Complete live guard rehearsal is required')
    require(all(row.get('anatomical') and row.get('metal_bursts',0)>0 and row.get('damage')==0 and not row.get('air_from_actual_clear_window') for row in matching),'Every primary matching guard must intercept actual steel')
   if name.endswith('/PHYSICAL_CLASH_SOURCE_CPU_REVIEW.json'):
    require(len(r.get('reviewed_ids',[]))==32 and r.get('legacy_pairs_unchanged')==217,'Both complete opponent rosters must pass')
   if name.endswith('/LIVE_GUARD_DEPLOYMENT_CPU_REVIEW.json'):
    rows=r.get('rows',[])
    require(len(rows)==30 and all(row.get('outcome')=='steel' for row in rows) and r.get('deployment_rates_hz')==[30,60,120],'Live guard deployment must pass all directions at all three update rates')
  for p in [Path(__file__),N/'launch_blade_clash.sh',ROOT/'Launch_Valwood_Blade_Clash.sh',N/'COMBAT_NOTES.txt',N/'HERO_ASSET.json',N/'SOURCE_ORIGIN.json',N/'arena-project/arena.config']:pin(p)
  for p in (N/'shaders').rglob('*'):
   if p.is_file():pin(p)
  for p in list(inputs):require(stat(p)==frozen[p],'Dependency changed during publication: '+p)
  out={'schema':'valwood.blade-clash.ready.v1','ready':True,'native_only':True,'native_sha256':native['sha256'],'player_knights':18,'enemy_knights':18,'enemy_orcs':14,'physical_controller_feel_verified':False,'sustained_hardware_fps_verified':False,'inputs':inputs,'frozen_stats':frozen}
  temp=READY.with_suffix('.tmp');temp.write_text(json.dumps(out,indent=2,sort_keys=True)+'\n');temp.replace(READY)
 require(READY.is_file(),'Blade clash build is still being reviewed')
 r=read(READY);require(r.get('ready') and r.get('native_only'),'Clash review incomplete')
 for p,h in r['inputs'].items():
  changed=stat(p)!=r['frozen_stats'][p]
  if a.verify_full or changed or Path(p).stat().st_size<1024*1024:require(sha(p)==h,'Changed since clash verification: '+p)
 print('Arena ready: physical blade clashes, interrupted swings, contact feedback;18 selectable knights and both opponent rosters.')
if __name__=='__main__':
 try:main()
 except (OSError,ValueError,KeyError,RuntimeError,subprocess.CalledProcessError) as e:print(str(e),file=sys.stderr);sys.exit(1)
