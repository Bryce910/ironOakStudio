#!/usr/bin/env python3
"""Verify the reviewed native story demo, leaving saves/settings/media editable."""
from pathlib import Path
import argparse,hashlib,json,re,sys
N=Path(__file__).resolve().parent
ROOT=N.parents[2]
READY=N/'NPC_INTRO_READY.json'
REVIEWS=['review/STORY_PROJECT_SNAPSHOT.json','review/STORY_CONFIG_FINAL_CPU_REVIEW.json','review/STORY_PROJECT_REPAIR_REVIEW.json','review/STORY_SCENE_CPU_REVIEW.json',
 'review/studio-startup-diagnostic/STORY_STUDIO_CPU_REVIEW.json',
 'npc-intro-review/NPC_INTRO_CPU_REVIEW.json','media-review/NPC_INTRO_MEDIA_CPU_REVIEW.json',
 'review/NPC_INTRO_FINAL_NATIVE_REVIEW.json','review/NPC_INTRO_VISUAL_REVIEW.json']
def sha(p):
 with Path(p).open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def stat(p):
 s=Path(p).stat();return [s.st_dev,s.st_ino,s.st_size,s.st_mtime_ns,s.st_ctime_ns]
def read(p):return json.loads(Path(p).read_text())
def require(v,message):
 if not v:raise RuntimeError(message)
def main():
 p=argparse.ArgumentParser();p.add_argument('--write',action='store_true');p.add_argument('--check-only',action='store_true');a=p.parse_args()
 if a.write:
  inputs={};stats={};inode_hash={};external_evidence={}
  def pin(path,expected=None):
   path=Path(path).resolve();s=stat(path);key=tuple(s)
   h=inode_hash.get(key)
   if h is None:h=sha(path);inode_hash[key]=h
   require(expected is None or h==expected,'Changed reviewed file: '+str(path))
   inputs[str(path)]=h;stats[str(path)]=s
  def evidence_pin(path,expected):
   path=Path(path).resolve()
   if path.is_relative_to(N):pin(path,expected)
   else:
    # Compiler/regression inputs and the designer's original project establish
    # publication provenance. The independently copied demo must keep working
    # when those original files are subsequently edited or tools are updated.
    require(sha(path)==expected,'Changed publication evidence: '+str(path))
    external_evidence[str(path)]=expected
  def evidence(value):
   if isinstance(value,list):
    for v in value:evidence(v)
   elif isinstance(value,dict):
    if value and all(isinstance(k,str) and Path(k).is_absolute() and isinstance(v,str) and re.fullmatch('[0-9a-f]{64}',v) for k,v in value.items()):
     for path,h in value.items():evidence_pin(path,h)
    if isinstance(value.get('path'),str) and Path(value['path']).is_absolute() and re.fullmatch('[0-9a-f]{64}',str(value.get('sha256',''))):evidence_pin(value['path'],value['sha256'])
    for v in value.values():evidence(v)
  native=read(N/'runtime-native-v1/READY.json')
  require(native.get('ready') and native.get('native_only') and not native.get('Windows_work_performed'),'Native build incomplete')
  require(native.get('application_units')==103,'Native source closure incomplete')
  pin(N/'runtime-native-v1/native-bin/rpg',native['sha256']);pin(N/'runtime-native-v1/READY.json')
  for path,h in native['source_sha256'].items():pin(N/'source-v1'/path,h)
  for name in REVIEWS:
   r=read(N/name);require(r.get('passed') is True,'Review incomplete: '+name);pin(N/name)
   # Snapshot source inventories are historical provenance; the independently
   # copied story assets are pinned below, without tying play to designer edits.
   if not name.endswith('STORY_PROJECT_SNAPSHOT.json'):evidence(r)
  for path in (N/'story-project').rglob('*'):
   if not path.is_file():continue
   relative=path.relative_to(N/'story-project')
   if relative.parts[0]=='saves' or relative.parts[:2]==('opening','npc_intro'):continue
   if relative.name in {'graphics.settings','combat.settings','studio-layout.ini'} or relative.name.startswith('world.studio.recovery') or relative.name=='world.studio.workspace':continue
   pin(path)
  for path in [Path(__file__),N/'launch_npc_intro.sh',N/'INTRO_NOTES.txt',N/'NPC_INTRO_SCRIPT.txt',N/'NPC_INTRO_SCRIPT.srt',N/'SOURCE_ORIGIN.json',N/'source-v1/tools/import_npc_intro_media.py',ROOT/'Launch_Valwood_NPC_Intro.sh',ROOT/'Launch_Valwood_Opening.sh',ROOT/'Import_Valwood_NPC_Voice.sh']:pin(path)
  for path,s in stats.items():require(stat(path)==s,'File changed during review publication: '+path)
  r={'schema':'valwood.npc-intro.ready.v1','ready':True,'native_only':True,'native_sha256':native['sha256'],
   'maximum_video_dimensions':[3840,2160],'maximum_video_fps':30,'final_user_voice_or_video_supplied':False,
   'inputs':inputs,'frozen_stats':stats,'external_evidence_at_publication':external_evidence,
   'mutable_paths':['story-project/saves','story-project/graphics.settings','story-project/combat.settings','story-project/opening/npc_intro','shared-profile/player.progress']}
  temporary=READY.with_suffix('.tmp');temporary.write_text(json.dumps(r,indent=2,sort_keys=True)+'\n');temporary.replace(READY)
 require(READY.is_file(),'The NPC cinematic demo is still being reviewed')
 r=read(READY);require(r.get('ready'),'The NPC cinematic demo is not ready')
 verified_inodes={}
 for path,h in r['inputs'].items():
  current=stat(path)
  if current!=r['frozen_stats'][path] or current[2]<1048576:
   key=tuple(current)
   actual=verified_inodes.get(key)
   if actual is None:
    actual=sha(path);require(stat(path)==current,'File changed during launch verification: '+path);verified_inodes[key]=actual
   require(actual==h,'Reviewed file changed: '+path)
 print('First-NPC cinematic ready: five in-game shots, optional MP3 narration, subtitles and pause/skip.')
if __name__=='__main__':
 try:main()
 except (OSError,ValueError,KeyError,RuntimeError) as e:print(str(e),file=sys.stderr);sys.exit(1)
