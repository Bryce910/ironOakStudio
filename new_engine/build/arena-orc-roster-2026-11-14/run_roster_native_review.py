#!/usr/bin/env python3
"""One private offscreen full-game startup, ordinary setup selection for all14."""
from pathlib import Path
import argparse,hashlib,json,os,re,shlex,shutil,subprocess,sys,time
from PIL import Image,ImageDraw,ImageStat
sys.dont_write_bytecode=True
N=Path(__file__).resolve().parent;R=N/'arena_orcs_roster';P=N/'roster-review';S=N/'source-v1';O=N/'arena-project';PROJECT=P/'native-roster-private-project';D=P/'native-cycle'
B=N/'runtime-native-v1/native-bin/rpg';READY=N/'runtime-native-v1/READY.json';REPORT=P/'ROSTER_NATIVE_REVIEW.json'
IDS={'orc01','orc02','orc03','orc04','orc05','orc06','azrak','costalmed','dardan','girra','gnur','kanit','magur','ursag'}
def require(ok,msg):
 if not ok:raise RuntimeError(msg)
def sha(p):
 with Path(p).open('rb')as f:return hashlib.file_digest(f,'sha256').hexdigest()
def pin(p,d,h=None):
 p=Path(p).resolve();v=sha(p);require(h is None or v==h,'Changed dependency: '+str(p));d[str(p)]=v;return v
def prepare(inputs):
 marker=PROJECT/'PRIVATE_CAPTURE_PROJECT.json';require(not PROJECT.exists()or marker.is_file(),'Refusing to change unmarked capture project');PROJECT.mkdir(exist_ok=True)
 marker.write_text(json.dumps(dict(schema='valwood.private-orc-roster-native-project.v1',origin=str(O),private_profiles_only=True),indent=2)+'\n')
 copied={};links={}
 for original in sorted(O.iterdir()):
  if original.name in {'saves','player.progress','studio-layout.ini','world.studio.workspace','arena_orc_character.cfg'}:continue
  target=PROJECT/original.name
  if original.is_symlink()or original.is_dir():
   resolved=original.resolve();require(not target.exists()or target.is_symlink()and target.resolve()==resolved,'Private asset link changed')
   if not target.is_symlink():target.symlink_to(resolved,target_is_directory=original.is_dir())
   links[str(target)]=str(resolved)
  elif original.is_file():shutil.copy2(original,target);copied[str(target)]=sha(target)
 roster=PROJECT/'arena_orcs_roster'
 require(not roster.exists()or roster.is_symlink()and roster.resolve()==R,'Private project points to another roster')
 if not roster.is_symlink():roster.symlink_to(R,target_is_directory=True)
 links[str(roster)]=str(R)
 # Only reset the one explicitly marked private fixture, never origin saves.
 (PROJECT/'arena_orc_character.cfg').write_text('VALWOOD_ORC_CHARACTER 1 gnur end\n')
 outputs={};commands=[]
 for name in ('write','read'):
  source=P/(name+'_capture_profile.cpp');exe=P/(name+'_capture_profile');command=['/usr/bin/g++','-std=c++17','-O2','-I'+str(S),str(source),'-o',str(exe)];commands.append(command)
  deps=subprocess.check_output(command[:-2]+['-MM'],text=True).replace('\\\n',' ')
  for p in shlex.split(deps.split(':',1)[1]):pin(p,inputs)
  subprocess.run(command,check=True);outputs[str(exe)]=sha(exe)
 subprocess.run([str(P/'write_capture_profile'),str(PROJECT),'orc'],check=True)
 before=json.loads(subprocess.check_output([str(P/'read_capture_profile'),str(PROJECT)],text=True));require(before['balance']==before['wins']==before['active_run_id']==0 and before['next_run_id']==1,'Private production profile did not start fresh')
 receipt=dict(schema='valwood.orc-roster.native-project-preparation.v1',passed=True,private_project=str(PROJECT),origin_saves_not_read_or_written=True,copied_static_files=copied,asset_links=links,profile_before=before,profile_before_sha256=sha(PROJECT/'saves/player.progress'),inputs=inputs,outputs_sha256=outputs,commands=commands)
 path=P/'ROSTER_NATIVE_PROJECT.json';path.write_text(json.dumps(receipt,indent=2,sort_keys=True)+'\n');pin(path,inputs);inputs.update(outputs);return before

def main():
 parser=argparse.ArgumentParser();parser.add_argument('--prepare-only',action='store_true');parser.add_argument('--review-existing',action='store_true');parser.add_argument('--capture-before-strict',action='store_true',help='Private diagnostic capture only; final acceptance still requires strict14 and --review-existing');args=parser.parse_args();P.mkdir(exist_ok=True);D.mkdir(exist_ok=True)
 require(sum((args.prepare_only,args.review_existing,args.capture_before_strict))<=1,'Choose one preparation/capture/review mode');inputs={};pin(Path(__file__),inputs);pin(READY,inputs);pin(B,inputs)
 ready=json.loads(READY.read_text());require(ready['ready']is True and ready['sha256']==sha(B),'Native binary not current')
 for p,h in ready['source_sha256'].items():pin(S/p,inputs,h)
 if not(args.prepare_only or args.capture_before_strict):
  subprocess.run([sys.executable,str(N/'check_orc_roster_assets.py'),'--check-only'],check=True)
  assets_path=P/'ROSTER_ASSET_REVIEW.json';pin(assets_path,inputs);assets=json.loads(assets_path.read_text());require(set(assets['reviewed_ids'])==IDS and assets['strict_complete'],'Actual strict14character assets did not pass');inputs.update(assets['inputs']);inputs.update(assets['outputs_sha256'])
 roster_path=R/'ORC_ROSTER.json';pin(roster_path,inputs);roster=json.loads(roster_path.read_text());entries=roster['entries'];require(len(entries)==14 and {r['id']for r in entries}==IDS,'Roster identity mismatch')
 if args.capture_before_strict:
  # Pin the actual physical frozen roster before capture, independently of
  # producer paths in the copied Gnur manifest. This is never a ready gate.
  for p in sorted(R.rglob('*')):
   if p.is_file():pin(p,inputs)
 hero_manifest=N/'HERO_ASSET.json';pin(hero_manifest,inputs);hero=Path(json.loads(hero_manifest.read_text())['path']);pin(hero,inputs)
 loadouts=N/'loadout-rigs/loadouts.manifest';pin(loadouts,inputs);tokens=shlex.split(loadouts.read_text());require(tokens[:2]==['VALWOOD_SKELETAL_LOADOUTS','1']and len(tokens)==11,'Invalid actual Hero loadout rigs')
 for i in (3,6,9):pin(tokens[i],inputs)
 for p in (N/'shaders/world_skeletal.vs.glsl',N/'shaders/rig_first_person.fs.glsl'):pin(p,inputs)
 if args.review_existing:
  prep=P/'ROSTER_NATIVE_PROJECT.json';pin(prep,inputs);preparation=json.loads(prep.read_text());before=preparation['profile_before'];inputs.update(preparation['inputs']);inputs.update(preparation['outputs_sha256'])
  execution=D/'NATIVE_CAPTURE_EXECUTION.json'
  if execution.is_file():
   pin(execution,inputs);capture=json.loads(execution.read_text());require(capture['capture_completed']and not capture['final_acceptance'],'Delayed native capture execution proof missing')
   for p,h in capture['inputs'].items():pin(p,inputs,h)
   for p,h in capture['outputs_sha256'].items():pin(p,inputs,h)
 else:before=prepare(inputs)
 if args.prepare_only:print('PASS marked private roster project prepared; no game launched');return
 log=D/'native-roster-cycle.log';raw_path=D/'NATIVE_ROSTER_CYCLE.json';command=[str(B),'--project',str(PROJECT),'--play','--scenic','city-walk','--frames','2000','--hidden','--width','1280','--height','720','--graphics-preset','performance','--auto-quality','off','--render-scale','0.70']
 env=dict(os.environ)
 for k in list(env):
  if k.startswith(('VALWOOD_QA_','VALWOOD_TEST_','VALWOOD_PLAYTEST_','VALWOOD_DISABLE_','VALWOOD_CAPTURE_'))or k.endswith('_SEQUENCE')or k in {'VALWOOD_OPENING_PROLOGUE','VALWOOD_SHOWCASE','VALWOOD_MODULAR_KNIGHT_THIRD_PERSON'}:env.pop(k,None)
 env.update(SDL_VIDEODRIVER='offscreen',SDL_AUDIODRIVER='dummy',LP_NUM_THREADS='4',MESA_DEBUG='1',LIBGL_ALWAYS_SOFTWARE='1',VALWOOD_PRESENTATION='1',VALWOOD_PLAYTEST_SLICE='city',VALWOOD_COMBAT_ARENA='1',VALWOOD_QA_ORC_ROSTER='1',VALWOOD_QA_ORC_ROSTER_DIR=str(D),VALWOOD_MODULAR_KNIGHT=str(hero),VALWOOD_MODULAR_KNIGHT_SHADER=str(N/'shaders/world_skeletal.vs.glsl'),VALWOOD_MODULAR_KNIGHT_FIRST_PERSON_SHADER=str(N/'shaders/rig_first_person.fs.glsl'),VALWOOD_SKELETAL_LOADOUTS=str(loadouts))
 # Private software-render diagnostics share RAM with the user's live game.
 # Lower internal rendering and promptly return released bank allocations;
 # keep1280x720 output and every genuine original mesh/map/contact path.
 env.update(MALLOC_ARENA_MAX='2',MALLOC_TRIM_THRESHOLD_='131072')
 started=time.monotonic()
 if not args.review_existing:
  # Independent offscreen process/private files. Do not acquire, focus or close
  # the designer's existing window or its global instance lock.
  with log.open('w')as f:subprocess.run(command,cwd=N.parents[1],env=env,stdout=f,stderr=subprocess.STDOUT,check=True)
 elapsed=time.monotonic()-started;text=log.read_text();require(raw_path.is_file(),'Native ordinary-path cycle did not finish');raw=json.loads(raw_path.read_text())
 require(raw.get('passed')is True and raw['expected_characters']==raw['reviewed_characters']==14 and raw['captures']==28 and raw['peak_resident_orc_banks']==1 and raw['coins_awarded']==0,'Native actual roster load/selection/resource/purse contract failed')
 if args.capture_before_strict:
  require(inputs=={p:sha(p)for p in inputs},'Frozen capture inputs changed during actual native startup/cycle')
  execution=D/'NATIVE_CAPTURE_EXECUTION.json';execution.write_text(json.dumps(dict(schema='valwood.orc-roster.private-native-delayed-acceptance.v1',capture_completed=True,final_acceptance=False,strict14_required_for_final_acceptance=True,inputs=inputs,outputs_sha256={str(raw_path):sha(raw_path),str(log):sha(log)},command=command,wall_seconds=elapsed),indent=2,sort_keys=True)+'\n')
  print('CAPTURE ONLY actual native14cycle complete; final strict14 + --review-existing still required');return
 rows=raw['characters'];require(len(rows)==14 and {r['id']for r in rows}==IDS,'Native cycle identities differ');outputs={str(raw_path):sha(raw_path),str(log):sha(log)}
 for row in rows:
  require(row['body_poses']==172 and row['rigid_pairs']==217 and row['pose_uploads']>0 and row['active_actors']>0 and row['resident_orc_banks']==1 and row['original_textures']is True,'Native source-bank actors not genuinely rendered')
  cfg=(R/row['id']/'body/combat_presentation.cfg').read_text().split()
  for field,key in [('body_radius_m','body_radius_m'),('body_height_m','body_height_m'),('stride_m','walk_stride_m')]:require(abs(row[field]-float(cfg[cfg.index(key)+1]))<1e-6,'Native measured source profile differs from selected bank')
  expected_movement=float(cfg[cfg.index('body_radius_m')+1])+.04
  require(abs(row['movement_radius_m']-expected_movement)<1e-6 and abs(row['pair_clearance_m']-max(.88,2*expected_movement))<1e-6,'Native selected body movement/pair margin differs from actual measured bank')
  require(re.search(r'\[Arena Orc character\] loaded='+row['id']+r' resident=1 peak=1',text),'Actual native character load not logged: '+row['id'])
 require('[Orc roster QA] PASS 14/14 characters, 28 captures:'in text,'Native bounded cycle did not report completion')
 require(not re.search(r'GL error|GL_INVALID|shader (?:compile|link).*fail|Orc character could not load|Orc roster QA\] FAIL',text,re.I),'Native GL/source path failure recorded')
 require(re.search(r'\[Skeletal hero candidate\].*draws=[1-9][0-9]*.*failures=0',text)and re.search(r'\[Skeletal first person\] draws=[1-9][0-9]* failures=0',text),'Ordinary Human player TP/FP rigs did not render cleanly')
 profile_after=json.loads(subprocess.check_output([str(P/'read_capture_profile'),str(PROJECT)],text=True));require(profile_after['balance']==before['balance']==0 and profile_after['wins']==before['wins']==0 and profile_after['next_run_id']==15,'Private purse or ordinary run allocation differs')
 images=[];sheet=Image.new('RGB',(4*320,7*205),(18,23,29));draw=ImageDraw.Draw(sheet)
 for i,entry in enumerate(entries):
  for j,view in enumerate(('third','first')):
   capture=D/(entry['id']+'-'+view+'.bmp');require(capture.is_file(),'Missing actual native capture');pin(capture,outputs)
   with Image.open(capture)as image:
    require(image.size==(1280,720),'Wrong actual native resolution');rgb=image.convert('RGB');require(sum(ImageStat.Stat(rgb).stddev)>12,'Blank actual native scene');png=capture.with_suffix('.png');rgb.save(png);pin(png,outputs);images.append(str(png))
    rgb.thumbnail((320,180));index=i*2+j;x=(index%4)*320;y=(index//4)*205;sheet.paste(rgb,(x,y+25));draw.text((x+8,y+6),entry['display_name']+' / '+view,fill='white')
 sheet_path=D/'native-roster-contact-sheet.jpg';sheet.save(sheet_path,quality=90);pin(sheet_path,outputs);pin(PROJECT/'saves/player.progress',outputs);pin(PROJECT/'arena_orc_character.cfg',outputs)
 require(inputs=={p:sha(p)for p in inputs},'Frozen inputs changed during actual native test')
 report=dict(schema='valwood.orc-roster.native-review.v1',passed=True,expected_ids=sorted(IDS),reviewed_ids=sorted(r['id']for r in rows),inputs=inputs,outputs_sha256=outputs,native_cycle=raw,profile_before=before,profile_after=profile_after,captures=images,contact_sheet=str(sheet_path),command=command,environment={k:v for k,v in env.items()if k.startswith('VALWOOD_')or k in {'SDL_VIDEODRIVER','SDL_AUDIODRIVER','LP_NUM_THREADS','LIBGL_ALWAYS_SOFTWARE','MALLOC_ARENA_MAX','MALLOC_TRIM_THRESHOLD_'}},wall_seconds=None if args.review_existing else elapsed,
  private_saves_only=True,live_game_or_editor_not_touched=True,ordinary_setup_and_arena_begin=True,original_human_player_preserved=True,hardware_performance_accepted=False,controller_feel_accepted=False,
  limits=['Bounded sequential native loading and TP/FP rendering under software OpenGL, private Performance preset/0.70internal render scale for RAM headroom while preserving1280x720 output and all original character assets. This does not certify interactive controller feel, sustained hardware FPS, or simultaneous mixed-character streaming. The per-character fixture/contact checks provide separate combat/sever coverage.'])
 REPORT.write_text(json.dumps(report,indent=2,sort_keys=True)+'\n');print('PASS actual native14character TP/FP ordinary selection cycle:',REPORT)
if __name__=='__main__':main()
