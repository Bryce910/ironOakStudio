#!/usr/bin/env python3
"""Verify once by content, then cheaply detect changes to frozen reviewed assets."""
from pathlib import Path
import argparse,hashlib,json,re,sys
N=Path(__file__).resolve().parent;ROOT=N.parents[2];READY=N/'ARENA_CHARACTERS_READY.json'
REVIEWS=[
 'roster-review/PLAYER_APPEARANCE_CPU.json',
 'hero-gpu-review/PLAYER_APPEARANCE_GPU.json',
 'hero-gpu-review/PLAYER_TEXTURE_STORAGE_GPU.json',
 'character-review/KNIGHT_ROSTER_CONTRACT_REVIEW.json',
 'character-review/NPC_SOURCE_BINARY_REVIEW.json',
 'compact-review/COMPACT_PNT_CPU_REVIEW.json',
 'compact-review/COMPACT_RENDER_REVIEW.json',
 'compact-review/GL_MEMORY_TRACKER_SMOKE.json',
 'compact-review/UNARMED_BOOT_FIT_CPU_REVIEW.json',
 'roster-review/ROSTER_ASSIGNMENT_CPU.json',
 'roster-review/ARENA_START_SOURCE_AUDIT.json',
 'roster-review/SOURCE_REVIEW.json',
 'roster-review/ARENA_ROSTER_NATIVE.json',
 'roster-review/VISUAL_REVIEW.json',
]
def require(v,msg):
 if not v:raise RuntimeError(msg)
def sha(p):
 with Path(p).open('rb')as f:return hashlib.file_digest(f,'sha256').hexdigest()
def read(p):return json.loads(Path(p).read_text())
def fingerprint(p):
 s=Path(p).stat();return[s.st_dev,s.st_ino,s.st_size,s.st_mtime_ns,s.st_ctime_ns]
def main():
 parser=argparse.ArgumentParser();parser.add_argument('--write',action='store_true');parser.add_argument('--check-only',action='store_true');parser.add_argument('--verify-full',action='store_true');args=parser.parse_args()
 if args.write:
  inputs={}
  def pin(p,h=None):
   p=Path(p).resolve();key=str(p);actual=inputs.get(key)
   if actual is None:actual=sha(p);inputs[key]=actual
   require(h is None or actual==h,'Changed reviewed input: '+key)
  def evidence(x):
   if isinstance(x,list):
    for v in x:evidence(v)
   elif isinstance(x,dict):
    if x and all(isinstance(k,str)and Path(k).is_absolute()and isinstance(v,str)and re.fullmatch('[0-9a-f]{64}',v)for k,v in x.items()):
     for p,h in x.items():pin(p,h)
    if isinstance(x.get('path'),str)and Path(x['path']).is_absolute()and isinstance(x.get('sha256'),str):pin(x['path'],x['sha256'])
    for v in x.values():evidence(v)
  baseline=N.parent/'arena-contact-feedback-2026-11-14/COMBAT_FEEDBACK_READY.json';base=read(baseline)
  require(base.get('ready')and base.get('orc_characters')==14,'Accepted combat feedback baseline unavailable');pin(baseline)
  for p,h in base['inputs'].items():pin(p,h)
  native_path=N/'runtime-native-v1/READY.json';native=read(native_path)
  require(native.get('ready')and native.get('native_only')and not native.get('Windows_work_performed')and native['application_units']==103,'Native source closure incomplete');pin(native_path)
  binary=N/'runtime-native-v1/native-bin/rpg';pin(binary,native['sha256'])
  for p,h in native['source_sha256'].items():pin(N/'source-v1'/p,h)
  for name in REVIEWS:
   p=N/name;r=read(p);require(r.get('passed')is True,'Review incomplete: '+name);pin(p);evidence(r)
   if name.endswith('/ARENA_ROSTER_NATIVE.json'):require(r['inputs'].get(str(binary))==native['sha256'],'Arena review uses a different binary')
   if name.endswith('/NPC_SOURCE_BINARY_REVIEW.json'):require(len(r['reviewed_ids'])==18 and r['body_poses_per_character']==210,'The complete18-knight source pose banks were not reviewed')
   if name.endswith('/COMPACT_PNT_CPU_REVIEW.json'):
    require(len(r['characters'])==32 and set(r['reviewed_ids'])==set(r['expected_orc_ids']+r['expected_knight_ids'])and len(r['reviewed_ids'])==32 and all(c['passed']for c in r['characters'].values()),'Both full opponent rosters must pass pose parity')
   if name.endswith('/PLAYER_APPEARANCE_GPU.json'):require(r['character_count']==18 and r['weapon_rig_sets']==72 and r['actual_draws']==288,'All18actualknights/fourrigs must render')
  roster=read(N/'knights_roster/KNIGHT_ASSETS.json');require(roster.get('producer_complete')and roster['character_count']==18,'Actual18publisherknights incomplete')
  # Every runtime bank (including packet/index/caps/texture references) is in
  # this independently content-verified closure. Shared accepted assets remain
  # in the inherited verified closure above. Mutable saves/settings are absent.
  for dirname in ('knights_roster','npc_knights','compact_orcs','shaders'):
   for p in (N/dirname).rglob('*'):
    if p.is_file():pin(p)
  for p in [Path(__file__),N/'launch_character_candidate.sh',ROOT/'Launch_Valwood_Arena_Characters.sh',N/'SOURCE_ORIGIN.json',N/'HERO_ASSET.json',N/'arena-project/arena.config',N/'loadout-rigs/loadouts.manifest']:pin(p)
  # Hash all collected dependencies once more before publishing immutable stat
  # fingerprints. Launches need not rescan many gigabytes of unchanged meshes.
  frozen={}
  for p,h in inputs.items():require(sha(p)==h,'Changed during publication: '+p);frozen[p]=fingerprint(p)
  result={'schema':'valwood.arena-character-roster.ready.v1','ready':True,'native_only':True,'player_knights':18,'enemy_knights':18,'enemy_orcs':14,'native_sha256':native['sha256'],'inputs':inputs,'frozen_stats':frozen,'physical_controller_feel_verified':False,'sustained_hardware_fps_verified':False}
  temp=READY.with_suffix('.tmp');temp.write_text(json.dumps(result,indent=2,sort_keys=True)+'\n');temp.replace(READY)
 require(READY.is_file(),'The18-knight arena is still being verified');result=read(READY)
 require(result.get('ready')and result.get('player_knights')==18 and result.get('enemy_orcs')==14,'Arena roster incomplete')
 for p,h in result['inputs'].items():
  changed=fingerprint(p)!=result['frozen_stats'][p]
  if args.verify_full or changed or Path(p).stat().st_size<1024*1024:require(sha(p)==h,'Changed since roster verification: '+p)
 print('Arena ready:18 selectable knights; randomized18-knight or14-Orc opponents.')
if __name__=='__main__':
 try:main()
 except(OSError,ValueError,KeyError,RuntimeError)as error:print(str(error),file=sys.stderr);sys.exit(1)
