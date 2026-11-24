#!/usr/bin/env python3
"""Private ordinary arena starts: actual eighteen-knight picker and mixed appearances."""
from pathlib import Path
import argparse,hashlib,json,os,shlex,shutil,subprocess,time
from PIL import Image,ImageDraw,ImageStat
N=Path(__file__).resolve().parent;P=N/'roster-review';S=N/'source-v1';O=N/'arena-project'
PROJECT=P/'native-character-private-project';D=P/'native-character-cycle';B=N/'runtime-native-v1/native-bin/rpg'
def require(ok,msg):
 if not ok:raise RuntimeError(msg)
def sha(p):
 with Path(p).open('rb')as f:return hashlib.file_digest(f,'sha256').hexdigest()
def pin(p,d,h=None):
 p=Path(p).resolve();v=sha(p);require(h is None or v==h,'Changed input: '+str(p));d[str(p)]=v
def prepare(inputs):
 require(not PROJECT.exists()or(PROJECT/'PRIVATE_CAPTURE_PROJECT.json').is_file(),'Unmarked private project')
 PROJECT.mkdir(exist_ok=True)
 (PROJECT/'PRIVATE_CAPTURE_PROJECT.json').write_text(json.dumps({'private_profiles_only':True,'origin':str(O)})+'\n')
 (PROJECT/'.valwood-arena-roster-qa').write_text('VALWOOD_PRIVATE_ARENA_ROSTER_V1\n')
 for source in O.iterdir():
  if source.name in {'saves','settings','player.progress','studio-layout.ini','world.studio.workspace','arena_player_character.cfg','graphics.settings'}:continue
  target=PROJECT/source.name
  if source.is_dir()or source.is_symlink():
   require(not target.exists()or target.is_symlink()and target.resolve()==source.resolve(),'Private link differs')
   if not target.is_symlink():target.symlink_to(source.resolve(),target_is_directory=source.is_dir())
  elif source.is_file():shutil.copy2(source,target)
 old=N.parent/'arena-orc-roster-2026-11-14/roster-review'
 for name in ('write','read'):
  src=P/(name+'_capture_profile.cpp');shutil.copy2(old/src.name,src);exe=P/(name+'_capture_profile')
  cmd=['g++','-std=c++17','-O2','-I'+str(S),str(src),'-o',str(exe)]
  deps=subprocess.check_output(cmd[:-2]+['-MM'],text=True).replace('\\\n',' ')
  for dep in shlex.split(deps.split(':',1)[1]):pin(dep,inputs)
  subprocess.run(cmd,check=True);pin(exe,inputs)
 subprocess.run([str(P/'write_capture_profile'),str(PROJECT),'human'],check=True)
 choice=PROJECT/'arena_player_character.cfg'
 choice.write_text('VALWOOD_ARENA_PLAYER_CHARACTER 1\nplayer knight09\nend\n')
 before=json.loads(subprocess.check_output([str(P/'read_capture_profile'),str(PROJECT)],text=True))
 require(before['balance']==before['wins']==0 and before['next_run_id']==1,'Private profile not fresh')
 return before
def main():
 args=argparse.ArgumentParser();args.add_argument('--prepare-only',action='store_true');args.add_argument('--review-existing',action='store_true');a=args.parse_args()
 P.mkdir(exist_ok=True);D.mkdir(exist_ok=True);inputs={};pin(__file__,inputs)
 ready_path=N/'runtime-native-v1/READY.json';pin(ready_path,inputs);ready=json.loads(ready_path.read_text());pin(B,inputs,ready['sha256']);require(ready['ready'],'Native not ready')
 for p,h in ready['source_sha256'].items():pin(S/p,inputs,h)
 for p in [N/'knights_roster/KNIGHT_ROSTER.cfg',N/'knights_roster/KNIGHT_ASSETS.json',N/'HERO_ASSET.json',N/'loadout-rigs/loadouts.manifest']:pin(p,inputs)
 hero=Path(json.loads((N/'HERO_ASSET.json').read_text())['path']);pin(hero,inputs)
 for i in (3,6,9):pin(shlex.split((N/'loadout-rigs/loadouts.manifest').read_text())[i],inputs)
 for p in [N/'shaders/world_skeletal.vs.glsl',N/'shaders/rig_first_person.fs.glsl']:pin(p,inputs)
 # Freeze the actual runtime assets before random selection. This includes all
 # possible shuffled banks so the selected IDs cannot escape the evidence.
 for dirname in ('knights_roster','npc_knights','compact_orcs'):
  for p in sorted((N/dirname).rglob('*')):
   if p.is_file():pin(p,inputs)
 if not a.review_existing:
  before=prepare(inputs);(D/'PREPARATION.json').write_text(json.dumps({'profile_before':before,'inputs':inputs},indent=2)+'\n')
 else:
  prep=json.loads((D/'PREPARATION.json').read_text());before=prep['profile_before']
  for p,h in prep['inputs'].items():pin(p,inputs,h)
 if a.prepare_only:print('PASS private character fixture prepared');return
 command=[str(B),'--project',str(PROJECT),'--play','--scenic','city-walk','--frames','1800','--hidden','--width','1280','--height','720','--graphics-preset','performance','--auto-quality','off','--render-scale','0.70']
 env=dict(os.environ)
 for k in list(env):
  if k.startswith(('VALWOOD_QA_','VALWOOD_TEST_','VALWOOD_PLAYTEST_','VALWOOD_DISABLE_','VALWOOD_CAPTURE_'))or k.endswith('_SEQUENCE')or k in {'VALWOOD_OPENING_PROLOGUE','VALWOOD_SHOWCASE','VALWOOD_MODULAR_KNIGHT_THIRD_PERSON'}:env.pop(k,None)
 env.update(SDL_VIDEODRIVER='offscreen',SDL_AUDIODRIVER='dummy',LP_NUM_THREADS='2',MESA_DEBUG='1',LIBGL_ALWAYS_SOFTWARE='1',MALLOC_ARENA_MAX='2',MALLOC_TRIM_THRESHOLD_='131072',VALWOOD_PRESENTATION='1',VALWOOD_PLAYTEST_SLICE='city',VALWOOD_COMBAT_ARENA='1',VALWOOD_QA_ARENA_ROSTER='1',VALWOOD_QA_ARENA_ROSTER_DIR=str(D),VALWOOD_MODULAR_KNIGHT=str(hero),VALWOOD_MODULAR_KNIGHT_SHADER=str(N/'shaders/world_skeletal.vs.glsl'),VALWOOD_MODULAR_KNIGHT_FIRST_PERSON_SHADER=str(N/'shaders/rig_first_person.fs.glsl'),VALWOOD_SKELETAL_LOADOUTS=str(N/'loadout-rigs/loadouts.manifest'))
 tracker=N/'compact-review/libvalwood_gl_memory.so';pin(tracker,inputs);env['LD_PRELOAD']=str(tracker)
 log=D/'native-character.log';raw_path=D/'NATIVE_ROSTER_RAW.json';start=time.monotonic()
 if not a.review_existing:
  raw_path.unlink(missing_ok=True)
  with log.open('w')as f:subprocess.run(command,cwd=N.parents[1],env=env,stdout=f,stderr=subprocess.STDOUT,check=True)
 raw=json.loads(raw_path.read_text());require(raw.get('passed')and raw['matches']==2 and raw['captures']==18 and raw['events_injected']is False,'Native ordinary-path roster test failed: '+str(raw))
 text=log.read_text();require(not __import__('re').search(r'GL error|GL_INVALID|shader (?:compile|link).*fail|Arena roster QA\] FAIL',text,__import__('re').I),'Native graphics error')
 outputs={};pin(raw_path,outputs);pin(log,outputs)
 names=['setup','sword-third','shield-first','contact-first','bow-third','bow-drawn-third','bow-first','sheathed-third','sheathed-first']
 sheet=Image.new('RGB',(4*320,5*205),(16,20,26));draw=ImageDraw.Draw(sheet);captures=[]
 for row_i,row in enumerate(raw['rows']):
  require(row['kind']==('Knights'if row_i==0 else'Orcs')and row['player']==('knight01'if row_i==0 else'knight18'),'Wrong selected player')
  require(len(row['opponents'])==len(set(row['opponents']))==8,'Opponent randomization duplicates')
  require(row['actual_serial']>0 and row['actual_outcome']>0,'No real resolved attack')
  require(row['GL_live_buffers']>0 and row['GL_live_textures']>0,'Actual GPU allocation measurements absent')
  require(row['GL_live_buffers']+row['GL_live_textures']<2.4*1024**3,'Actual arena GPU allocations exceed2.4GiB target')
  for bank in row['banks']:require(bank['failures']==0 and bank['uploads']>0 and bank['poses']==(210 if bank['id'].startswith('knight') else 172),'Compact bank did not render actual poses')
  for j,name in enumerate(names):
   capture=D/(('knights-'if row_i==0 else'orcs-')+name+'.bmp');pin(capture,outputs)
   with Image.open(capture)as im:
    require(im.size==(1280,720),'Capture resolution differs');rgb=im.convert('RGB');require(sum(ImageStat.Stat(rgb).stddev)>12,'Blank scene')
    png=capture.with_suffix('.png');rgb.save(png);pin(png,outputs);captures.append(str(png));rgb.thumbnail((320,180))
    idx=row_i*9+j;x=(idx%4)*320;y=(idx//4)*205;sheet.paste(rgb,(x,y+25));draw.text((x+6,y+7),capture.stem,fill='white')
 after=json.loads(subprocess.check_output([str(P/'read_capture_profile'),str(PROJECT)],text=True));require(after['balance']==after['wins']==0 and after['next_run_id']==3,'Private purse or run allocation changed')
 choice=PROJECT/'arena_player_character.cfg';require('player knight18'in choice.read_text(),'Last selected player did not persist');pin(choice,outputs);pin(PROJECT/'saves/player.progress',outputs)
 sheet_path=D/'ARENA_CHARACTER_CONTACT_SHEET.jpg';sheet.save(sheet_path,quality=94);pin(sheet_path,outputs)
 for p,h in inputs.items():require(sha(p)==h,'Input changed while reviewing: '+p)
 report={'schema':'valwood.arena-character-roster.native-review.v1','passed':True,'inputs':inputs,'outputs_sha256':outputs,'raw':raw,'profile_before':before,'profile_after':after,'captures':captures,'contact_sheet':str(sheet_path),'command':command,'wall_seconds':None if a.review_existing else time.monotonic()-start,'user_session_untouched':True,'private_saves_only':True,'hardware_fps_accepted':False,'physical_controller_feel_accepted':False}
 (P/'ARENA_ROSTER_NATIVE.json').write_text(json.dumps(report,indent=2,sort_keys=True)+'\n');print('PASS two real arena starts,18 captures, actual contact, stable eight-enemy decks, saved player choice; user session untouched')
if __name__=='__main__':main()
