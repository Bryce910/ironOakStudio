#!/usr/bin/env python3
"""Native joust right-stick aim and committed right-trigger thrust release."""
from pathlib import Path
import argparse,hashlib,importlib.util,json,os,struct,sys
sys.dont_write_bytecode=True
N=Path(__file__).resolve().parent;E=N.parents[1];ROOT=E.parent
K=N.parent/'character-ultra4k-2026-11-15';J=N.parent/'arena-joust-2026-11-15'
SOURCE=N/'source-v1';RUNTIME=N/'runtime-native-v1';BINARY=RUNTIME/'native-bin/rpg';READY=N/'JOUST_CONTROLS_READY.json'
WRAPPERS=('Launch_Valwood_Arena.sh','Launch_Valwood_Arena_Visual.sh','Launch_Valwood_Joust.sh')
_cache={}
def need(ok,why):
 if not ok:raise RuntimeError(why)
def read(p):return json.loads(Path(p).read_text())
def stat(p):
 s=Path(p).stat();return [s.st_dev,s.st_ino,s.st_size,s.st_mtime_ns,s.st_ctime_ns]
def sha(p):
 key=tuple(stat(p))
 if key not in _cache:
  with Path(p).open('rb') as f:_cache[key]=hashlib.file_digest(f,'sha256').hexdigest()
 return _cache[key]
def pins(d):
 need(isinstance(d,dict) and d,'Missing evidence inputs')
 for name,h in d.items():need(Path(name).is_file() and sha(name)==h,'Reviewed input changed: '+name)
def report(name):
 d=read(N/'review'/name);need(d.get('passed')is True,'Incomplete review: '+name)
 if 'inputs'in d:pins(d['inputs'])
 return d
def parent():
 a=read(N/'BASELINE_AUDIT.json');need(a.get('passed')is True and a.get('before_root_promotion')is True,'Published baseline was not audited before promotion')
 p=K/'ULTRA4K_READY.json';need(sha(p)==a['baseline_sha256'],'Published Ultra baseline receipt changed')
 d=read(p);excluded={str(ROOT/name)for name in WRAPPERS}
 need(d.get('ready')is True,'Inherited Ultra release not ready')
 for name,h in d['inputs'].items():
  if name in excluded:
   archived=N/'previous-launchers'/Path(name).name;need(sha(archived)==h,'Archived baseline wrapper differs');continue
  path=Path(name);need(path.is_file(),'Inherited baseline input missing: '+name)
  if stat(path)!=d.get('immutable_stats',{}).get(name):need(sha(path)==h,'Inherited baseline changed: '+name)
 for name,h in a['wrappers'].items():need(sha(name)==h,'Baseline wrapper archive changed')
 return d
def build():
 spec=importlib.util.spec_from_file_location('joust_closure',J/'check_joust_ready.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m);m.N=N;m.SOURCE=SOURCE;m.RUNTIME=RUNTIME;m.BINARY=BINARY
 d=m.verify_build()
 old=read(K/'runtime-native-v1/READY.json')['source_sha256'];new=d['source_sha256'];changed={p for p in old if old[p]!=new.get(p)}|set(new).difference(old)
 allowed={'engine/world_joust.h','engine/world_joust_runtime.inc','engine/world_joust_render.h','engine/world_joust_qa.inc','tests/world_joust_test.cpp','engine/world_joust_arm_private.h'}
 need(changed and changed<=allowed,'Unreviewed changes outside joust: '+str(sorted(changed-allowed)))
 return d

def verify_native():
 summary=report('NATIVE_REVIEW.json');runs=summary.get('runs',[]);need(len(runs)==2,'Both native controller and keyboard rehearsals required')
 seen=set();proofs=[]
 required={'SDL-four-direction-aim-moves-ready-lance-in-camera-frame','SDL-RT-rising-edge-produces-forward-extension','held-RT-does-not-auto-repeat-and-early-push-expires','held-RT-disarmed-after-resume-until-neutral','RT-release-rearms-next-committed-thrust','SDL-focus-loss-cancels-thrust-and-freezes-horse','focus-resume-held-RT-requires-release-before-push','timed-RT-push-scores-only-during-committed-contact-window'}
 for entry in runs:
  directory=Path(entry['directory']);mode=entry['mode'];need(directory.is_absolute() and directory.is_relative_to(N/'review') and mode in ('controller','keyboard') and mode not in seen,'Invalid/repeated private native run');seen.add(mode)
  ex=read(directory/'EXECUTION.json');need(ex.get('exit_code')==0 and ex.get('native_passed')is True and ex.get('private_saves')is True and ex.get('native_GPU')is True and ex.get('mode')==mode,'Native execution failed or is not private')
  pins(ex['inputs']);need(ex['inputs'].get(str(BINARY))==sha(BINARY) and ex['inputs'].get(str(RUNTIME/'READY.json'))==sha(RUNTIME/'READY.json'),'Native execution used another build')
  need(ex.get('command',[None])[0]==str(BINARY),'Native command used another binary')
  raw=read(directory/'JOUST_NATIVE_RAW.json');need(raw.get('schema')=='valwood.joust.native-sdl-rehearsal.v2' and raw.get('passed')is True and raw.get('error')=='','Native controller/thrust proof failed')
  for name in ('actual_SDL_event_route','owned_virtual_controller','private_practice_only','renderer_ready'):need(raw.get(name)is True,'Native witness missing: '+name)
  need(raw.get('contacts_injected')is False and raw.get('live_state_injected')is False,'Native route injected game state or contacts')
  count=3 if mode=='controller'else 1
  need(raw.get('difficulty')==(2 if mode=='controller'else 0) and raw.get('passes')==count and raw.get('match_starts')==raw.get('match_completions')==1,'Native actual match coverage incomplete')
  need(raw.get('GL_errors')==raw.get('renderer_failures')==0 and raw.get('renderer_pose_updates',0)>100 and raw.get('renderer_color_draws',0)>100 and raw.get('renderer_shadow_draws',0)>0,'Native mounted rendering failed')
  need(raw.get('renderer_arm_updates',0)>100 and raw.get('renderer_arm_failures')==0 and raw.get('maximum_rendered_grip_error_m',1)<.002 and raw.get('maximum_rendered_hand_surface_error_m',1)<.020 and raw.get('maximum_rendered_hand_surface_drift_m',1)<.002,'Actual rendered palm/arm missed the moving lance')
  need(raw.get('physical_contacts',0)>0 and raw['physical_contacts']==raw.get('verified_contact_proxy_sweeps')==raw.get('active_thrust_contacts') and raw.get('audio_dispatches',0)>0 and raw.get('feedback_dispatches',0)>0,'Actual active-stroke contact/feedback witnesses missing')
  need(.28<raw.get('maximum_forward_extension_m',0)<=.351 and .85<raw.get('maximum_thrust_amount',0)<=1.001,'Visible forward stroke is missing or exceeds physical range')
  need(raw.get('timed_trigger_pushes')==(2 if mode=='controller'else 1) and raw.get('player_thrusts')==2+raw['timed_trigger_pushes'],'Held trigger repeated or timed presses did not create one stroke each')
  need(required<=set(raw.get('checks',[])),'Pause/focus/release/directional thrust checks missing')
  rows=raw.get('passes_reviewed',[]);need(len(rows)==count and [row['pass']for row in rows]==list(range(1,count+1)),'Full native match passes missing')
  need(sum(row['player_hit']for row in rows)==raw['player_score'] and sum(row['opponent_hit']for row in rows)==raw['opponent_score'],'Native scores differ from actual physical contacts')
  for row in rows:
   if row.get('deliberate_no_trigger_pass'):need(row['player_hit']==0 and row.get('timed_trigger_press')is False and row.get('player_active_thrust')is False,'No-trigger pass scored a passive contact')
   else:need(row.get('timed_trigger_press')is True and row.get('player_active_thrust')is True and row['player_hit']>0 and .089<=row.get('player_contact_thrust_time',0)<=.431,'Physical hit occurred outside committed thrust window')
  directions=raw.get('directions_reviewed',[]);need(len(directions)==raw.get('right_stick_directions') and len(directions)>=(8 if mode=='controller'else 4),'Directional aim coverage incomplete')
  if mode=='controller':
   need(len(directions)==8 and all(row.get('physical_right_stick')is True for row in directions),'Direction checks did not use actual SDL controller axes')
   need({(row['pass'],row['direction'])for row in directions}=={(step,direction)for step in(1,2)for direction in('up','down','left','right')},'Forward/reverse stick-direction coverage incomplete')
   need(raw.get('cross_source_neutral_checks')==3 and {'keyboard-resume-held-RT-cannot-bypass-neutral-gate','quarter-RT-stays-disarmed-through-right-stick-source-switch'}<=set(raw['checks']),'Held trigger crossed keyboard/controller modes without fresh neutral')
  paths=[Path(x)for x in raw.get('capture_paths',[])];need(len(paths)==raw.get('captures') and len(paths)>=12 and len(set(paths))==len(paths) and set(directory.glob('*.bmp'))==set(paths),'Native captures missing/duplicated')
  for path in paths:
   need(path.parent==directory and path.suffix=='.bmp','Capture escapes private run')
   with path.open('rb')as f:h=f.read(30)
   need(h[:2]==b'BM' and struct.unpack_from('<ii',h,18)==(1600,900),'Wrong native BMP dimensions')
   png=path.with_suffix('.png')
   with png.open('rb')as f:h=f.read(24)
   need(h[:8]==b'\x89PNG\r\n\x1a\n' and struct.unpack('>II',h[16:24])==(1600,900),'Wrong native PNG dimensions');proofs.extend((path,png))
  log=(directory/'native.log').read_text(errors='replace');need('GL_INVALID_'not in log and 'Mesa: User error'not in log,'Native driver reported an error')
  proofs.extend((directory/'EXECUTION.json',directory/'JOUST_NATIVE_RAW.json'))
 need(seen=={'controller','keyboard'},'Both input routes required')
 visual=report('VISUAL_REVIEW.json')
 for path in proofs:need(visual['inputs'].get(str(path))==sha(path),'Visual review omits native evidence: '+str(path))
 return len(proofs)
def routing(promoted):
 for name in ('launch_arena.sh','launch_joust.sh'):
  p=N/name;s=p.read_text();need(os.access(p,os.X_OK) and 'check_joust_controls_ready.py' in s and '--check-only'in s and 'flock -n'in s and 'unset LD_PRELOAD'in s,'Launcher guard/routing missing')
 need(str(K.relative_to(E))in(N/'launch_arena.sh').read_text(),'Sword arena must retain the published Ultra runtime and profile')
 need('runtime-native-v1/native-bin/rpg'in(N/'launch_joust.sh').read_text(),'Joust does not select rebuilt binary')
 if promoted:
  for name in WRAPPERS:
   s=(ROOT/name).read_text();target='launch_joust.sh'if name.endswith('Joust.sh')else'launch_arena.sh'
   need(str(N.relative_to(ROOT))in s and target in s,'Root wrapper selects another stage: '+name)
def verify(promoted=False):
 parent();d=build();routing(promoted)
 cpu=read(N/'review/CORE_CPU_REVIEW.json');need(cpu.get('schema')=='valwood.joust.core.cpu-review.v1' and cpu.get('status')=='pass' and cpu.get('source_frozen')is True,'Core CPU proof incomplete')
 owned=cpu.get('owned_source_files',[]);need({row['path']for row in owned}=={str(SOURCE/'engine/world_joust.h'),str(SOURCE/'tests/world_joust_test.cpp')},'Core source provenance incomplete')
 for row in owned:need(sha(row['path'])==row['sha256'],'Core source changed after tests')
 tests=cpu.get('tests',[]);need(len(tests)==2 and {row['name']for row in tests}=={'normal','asan_ubsan'},'Normal/sanitized core runs missing')
 for row in tests:
  need(row.get('compile_exit')==row.get('run_exit')==0 and row.get('assertions',0)>=384860 and row.get('compile_command') and row.get('run_command') and sha(row['log'])==row['log_sha256'],'Actual core test failed or log changed')
  if row['name']=='asan_ubsan':need('-fsanitize=address,undefined'in row['compile_command'],'Address/undefined sanitizers not enabled')
 report('QA_CPU_REVIEW.json');report('RENDER_CPU_REVIEW.json');verify_native();return d

def collect():
 out={}
 def pin(p):out[str(p.absolute())]=sha(p)
 for p in SOURCE.rglob('*'):
  if p.is_file():pin(p)
 for p in(N/'joust-project').rglob('*'):
  if p.is_file()and 'saves'not in p.relative_to(N/'joust-project').parts and p.name not in ('graphics.settings','studio-layout.ini','world.studio.workspace','player-profile.config'):pin(p)
 for p in N.iterdir():
  if p.is_file()and p.suffix in('.py','.sh','.md','.json')and p.name not in(READY.name,'RELEASE_SWITCH.json'):pin(p)
 for p in(N/'review').rglob('*'):
  if p.is_file()and '__pycache__'not in p.parts and 'private-project'not in p.parts and p.suffix in('.json','.log','.cpp','.py','.png','.bmp','.md'):pin(p)
 for name in('READY.json','BUILD_PLAN.json','native-bin/rpg'):pin(RUNTIME/name)
 for name in WRAPPERS:pin(ROOT/name);pin(N/'previous-launchers'/name)
 pin(K/'ULTRA4K_READY.json');pin(K/'runtime-native-v1/native-bin/rpg')
 for name in('RENDER_CPU_REVIEW.json','QA_CPU_REVIEW.json'):
  for item in read(N/'review'/name).get('inputs',{}):pin(Path(item))
 for row in read(N/'review/NATIVE_REVIEW.json')['runs']:
  for name in read(Path(row['directory'])/'EXECUTION.json')['inputs']:pin(Path(name))
 return out

def publish():
 need(not READY.exists(),'Published receipt is immutable');d=verify(True);p=collect();tmp=READY.with_suffix('.tmp');tmp.write_text(json.dumps({'schema':'valwood.joust-controls.release.v1','ready':True,'native_only':True,'windows_work_performed':False,'binary_sha256':d['sha256'],'inputs':p,'immutable_stats':{name:stat(name)for name in p},'project_links':{str(p):str(p.resolve())for p in(N/'joust-project').iterdir()if p.is_symlink()},'scope':'Joust controller aim/thrust; Sword Arena retains exact prior Ultra runtime and project.'},indent=2)+'\n');tmp.replace(READY)
def check():
 d=read(READY);need(d.get('ready')is True and d.get('schema')=='valwood.joust-controls.release.v1','Joust controls release not ready');parent()
 for name,h in d['inputs'].items():
  need(Path(name).is_file(),'Published input missing: '+name)
  if stat(name)!=d['immutable_stats'][name]:need(sha(name)==h,'Published input changed: '+name)
 for name,target in d['project_links'].items():need(Path(name).is_symlink()and str(Path(name).resolve())==target,'Joust project link changed')
 routing(True)
def main():
 ap=argparse.ArgumentParser();g=ap.add_mutually_exclusive_group(required=True)
 for arg in('preflight','publish','check-only'):g.add_argument('--'+arg,action='store_true')
 a=ap.parse_args()
 if a.preflight:verify();print('PASS: native joust controls preflight.');return
 if a.publish:publish()
 else:check()
 print('PASS: right-stick lance aim and right-trigger thrust; prior Arena/Ultra preserved.')
if __name__=='__main__':
 try:main()
 except Exception as e:print('FAIL:',e,file=sys.stderr);sys.exit(1)
