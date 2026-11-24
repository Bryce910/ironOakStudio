#!/usr/bin/env python3
"""Guard the visible limb-bending joust pass and its native motion evidence."""
from pathlib import Path
import argparse,hashlib,importlib.util,json,os,struct,sys
sys.dont_write_bytecode=True
N=Path(__file__).resolve().parent;E=N.parents[1];ROOT=E.parent
O=N.parent/'joust-reactions-audio-2026-11-16';K=N.parent/'character-ultra4k-2026-11-15'
SOURCE=N/'source-v1';RUNTIME=N/'runtime-native-v1';BINARY=RUNTIME/'native-bin/rpg';READY=N/'JOUST_LIMB_READY.json'
WRAPPERS=('Launch_Valwood_Joust.sh','Launch_Valwood_Arena.sh','Launch_Valwood_Arena_Visual.sh')
def read(p):return json.loads(Path(p).read_text())
def need(ok,why):
    if not ok:raise RuntimeError(why)
def stat(p):
    s=Path(p).stat();return [s.st_dev,s.st_ino,s.st_size,s.st_mtime_ns,s.st_ctime_ns]
_cache={}
def sha(p):
    key=tuple(stat(p))
    if key not in _cache:
        with Path(p).open('rb') as stream:_cache[key]=hashlib.file_digest(stream,'sha256').hexdigest()
    return _cache[key]
def pins(values,stats=None):
    need(isinstance(values,dict) and values,'Missing input provenance')
    for name,h in values.items():
        need(Path(name).is_file(),'Reviewed input missing: '+name)
        if not stats or stat(name)!=stats.get(name):need(sha(name)==h,'Reviewed input changed: '+name)
def report(name):
    d=read(N/'review'/name)
    need(d.get('passed')is True or d.get('status')=='pass','Incomplete review: '+name)
    if d.get('inputs'):
        values=d['inputs']
        if isinstance(values,list):
            need(all(set(row)=={'path','sha256'} for row in values) and len({row['path'] for row in values})==len(values),'Malformed review input rows: '+name)
            values={row['path']:row['sha256'] for row in values}
        pins(values)
    return d
def native():
    summary=report('NATIVE_REVIEW.json');seen=set();proofs=[]
    for row in summary.get('runs',[]):
        directory=Path(row['directory']);mode=row['mode']
        need(directory.is_relative_to(N/'review') and mode in ('controller','controller-third','keyboard') and mode not in seen,'Invalid native rehearsal route')
        seen.add(mode);execution=read(directory/'EXECUTION.json');raw=read(directory/'JOUST_NATIVE_RAW.json')
        need(execution.get('exit_code')==0 and execution.get('native_passed')is True and execution.get('private_saves')is True and execution.get('native_GPU')is True,'Native rehearsal failed/not isolated')
        pins(execution['inputs']);need(execution['inputs'].get(str(BINARY))==sha(BINARY),'Native test used another binary')
        need(execution['inputs'].get(str(RUNTIME/'READY.json'))==sha(RUNTIME/'READY.json'),'Native test used another READY receipt')
        command=execution.get('command',[])
        need(command and command[0]==str(BINARY) and '--project' in command and command[command.index('--project')+1]==str(directory/'private-project'),'Native test used another command/project')
        need(raw.get('schema')=='valwood.joust.native-sdl-rehearsal.v6' and raw.get('passed')is True and raw.get('error')=='','Native stadium proof incomplete')
        need(execution.get('mode')==raw.get('mode')==mode and raw.get('difficulty')==(2 if mode.startswith('controller') else 0) and raw.get('passes')==(3 if mode.startswith('controller') else 1),'Native test route/difficulty/pass mismatch')
        need(raw.get('actual_SDL_event_route')is True and raw.get('owned_virtual_controller')is True and raw.get('contacts_injected')is False and raw.get('live_state_injected')is False,'Rehearsal bypassed physical input/contact route')
        need(raw.get('GL_errors')==raw.get('renderer_failures')==raw.get('renderer_arm_failures')==0,'Native renderer/driver failure')
        need(raw.get('spectators')==240 and raw.get('crowd_draws',0)>100 and raw.get('crowd_uploads',0)>0,'Animated stadium crowd missing')
        need(raw.get('crowd_families')==5 and raw.get('crowd_hair_families')==4 and raw.get('crowd_styles')==5 and raw.get('seated_spectators')==48 and raw.get('animated_spectators')==240 and raw.get('crowd_pose_pairs')==160,'Source-derived crowd variety/seated cheering incomplete')
        need(raw.get('audience_ready')is True and raw.get('audience_events',0)>0 and raw.get('audience_updates',0)>100,'Actual audience audio missing')
        need(raw.get('audio_master_gain',0)>.99 and raw.get('muted_postmix_samples',0)>0 and raw.get('crowd_postmix_nonzero',0)>1000 and raw.get('crowd_postmix_rms',0)>.01 and raw['crowd_postmix_rms']>raw.get('muted_postmix_rms',1)+.003 and raw.get('cheer_postmix_rms',0)>.01 and raw.get('cheer_postmix_nonzero',0)>1000,'Saved master/crowd actual PCM evidence missing')
        landings=raw.get('observed_ground_edges',0)
        need(landings>0 and raw.get('landing_events')==raw.get('landing_dust_bursts')==landings and raw.get('landing_audio_edge_checks',0)>0 and raw.get('peak_landing_dust_particles',0)>0 and raw.get('landing_dust_draws',0)>0 and raw.get('landing_dust_pause_checks')==2,'Actual landing dust/audio edge or pause/resume checks missing')
        need(.55<raw.get('maximum_forward_extension_m',0)<=.601 and raw.get('maximum_rendered_grip_error_m',1)<.002 and raw.get('maximum_rendered_hand_surface_drift_m',1)<.002,'Thrust travel and actual hand grip disagree')
        need(raw.get('physical_contacts',0)>0 and raw['physical_contacts']==raw.get('active_thrust_contacts')==raw.get('verified_contact_proxy_sweeps'),'Physical contacts not verified')
        need(raw.get('peak_rendered_unhorsed_riders',0)>0 and raw.get('peak_rendered_grounded_riders',0)>0 and raw.get('riderless_horse_draws',0)>0,'Fall/landing/empty horse not rendered')
        need(raw.get('released_gear_draws',0)>0 and raw.get('minimum_released_gear_support_y_m',-1)>=.019,'Dropped lance/shield not drawn with valid ground support')
        need(raw.get('unhorsed_rider_draws',0)>0 and raw.get('minimum_fallen_vertex_y_m',-1)>=.019,'Actual falling source mesh crossed the floor')
        need(raw.get('fall_joint_count')==73 and raw.get('varied_fall_updates',0)>10 and .4<raw.get('maximum_fall_secondary_angle_rad',0)<1.65 and .05<raw.get('maximum_fall_release_phase_span',0)<=1 and raw.get('maximum_fall_segment_length_error_m',1)<.00001,'Impact-reactive original-rig fall proof missing')
        need(len(raw.get('independent_limb_excursion_m',[]))==4 and min(raw['independent_limb_excursion_m'])>.25,'Hands and feet did not articulate independently')
        side=0 if mode.startswith('controller') else 1
        need(raw.get('falling_frames',[0,0])[side]>0 and raw.get('grounded_frames',[0,0])[side]>0,'Helmet fall did not complete')
        need(raw.get('peak_fall_pose_progress',[0,0])[side]>.95 and raw.get('peak_fall_settle_progress',[0,0])[side]>.95 and raw.get('peak_fall_ground_time',[0,0])[side]>=1.05 and raw.get('articulated_fall_updates',0)>0 and raw.get('settled_fall_updates',0)>0,'Rider did not release the mounted pose and settle on the ground')
        stages={row.get('capture') for row in raw.get('fall_stages_reviewed',[])}
        need(all(any(str(name).endswith('-'+stage) for name in stages) for stage in ('ejection','midfall','first-floor','settled-body')),'Actual articulated fall-stage captures missing')
        if mode.startswith('controller'):
            need(raw.get('third_person_player_unhorse_view' if mode=='controller-third' else 'first_person_player_unhorse_view')is True and all(raw.get('received_hit_types',[0]*4)[i]>0 for i in (1,2,3)),'Incoming shield/body/head reactions and tested camera view missing')
            need(raw.get('cross_source_neutral_checks')==3 and raw.get('right_stick_directions')==8,'Controller safety/directional coverage missing')
        need(raw.get('match_starts')==raw.get('match_completions')==1,'Full match did not complete')
        paths=raw.get('capture_paths',[])
        need(len(paths)==raw.get('captures') and len(paths)>=12 and len(set(paths))==len(paths) and set(map(Path,paths))==set(directory.glob('*.bmp')),'Native captures incomplete/duplicated')
        for name in paths:
            path=Path(name);need(path.parent==directory and path.is_file() and path.with_suffix('.png').is_file(),'Native capture missing')
            with path.open('rb') as stream:head=stream.read(30)
            need(head[:2]==b'BM' and struct.unpack_from('<ii',head,18)==(1600,900),'Wrong native BMP resolution')
            with path.with_suffix('.png').open('rb') as stream:head=stream.read(24)
            need(head[:8]==b'\x89PNG\r\n\x1a\n' and struct.unpack('>II',head[16:24])==(1600,900),'Wrong native PNG resolution')
            proofs.extend([path,path.with_suffix('.png')])
        proofs.extend([directory/'EXECUTION.json',directory/'JOUST_NATIVE_RAW.json'])
    need(seen=={'controller','controller-third','keyboard'},'Keyboard and both incoming-controller camera routes required')
    visual=report('VISUAL_REVIEW.json')
    for path in proofs:need(visual.get('inputs',{}).get(str(path))==sha(path),'Visual review does not pin native evidence')

def parent():
    spec=importlib.util.spec_from_file_location('prior_joust_guard',O/'check_joust_reactions_ready.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m);m.parent()
    receipt=read(O/'JOUST_REACTIONS_READY.json');baseline=read(N/'BASELINE_AUDIT.json')
    need(receipt.get('ready')is True and sha(O/'JOUST_REACTIONS_READY.json')==baseline['parent_sha256'],'Prior release changed')
    excluded={str(ROOT/name)for name in WRAPPERS};pins({p:h for p,h in receipt['inputs'].items()if p not in excluded},receipt['immutable_stats'])
    for name in WRAPPERS:need(sha(N/'previous-launchers'/name)==receipt['inputs'][str(ROOT/name)]==baseline['wrapper_sha256'][name],'Prior launcher archive changed')
    for name,target in receipt['project_links'].items():need(Path(name).is_symlink()and str(Path(name).resolve())==target,'Prior project link changed')
def closure():
    origin=E/'build/arena-joust-2026-11-15/check_joust_ready.py';spec=importlib.util.spec_from_file_location('limb_closure',origin);m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
    m.N=N;m.SOURCE=SOURCE;m.RUNTIME=RUNTIME;m.BINARY=BINARY;build=m.verify_build()
    old=read(O/'runtime-native-v1/READY.json')['source_sha256'];new=build['source_sha256'];changed={p for p in old if old[p]!=new.get(p)}|set(new).difference(old)
    need(changed=={'engine/world_joust_fall_private.h','engine/world_joust_render.h','engine/world_joust_qa.inc'},'Unexpected source change: '+str(sorted(changed)))
    return build

def routing(promoted):
    for file in ['launch_joust.sh','launch_arena.sh']:
        s=(N/file).read_text();need('check_joust_limb_ready.py' in s and 'flock -n' in s and 'unset LD_PRELOAD' in s and os.access(N/file,os.X_OK),'Unsafe launcher')
    need('runtime-native-v1/native-bin/rpg' in(N/'launch_joust.sh').read_text(),'Wrong joust binary')
    need(str(K.relative_to(E))in(N/'launch_arena.sh').read_text(),'Prior sword arena must remain unchanged')
    if promoted:
        for name in WRAPPERS:need(str(N.relative_to(ROOT))in(ROOT/name).read_text(),'Wrong root launcher')
def verify(promoted=False):
    parent();build=closure();routing(promoted)
    fall=report('FALL_CPU_REVIEW.json');need(fall.get('source_and_asset_inputs_unchanged_during_checks')is True,'Fall source changed during tests')
    for name in ['normal_actual_renderer_protocol','asan_ubsan_actual_renderer_protocol']:
        value=fall[name];need(value.get('passed')is True and len(value.get('independent_limb_excursion_m',[]))==4 and min(value['independent_limb_excursion_m'])>.30,'Four visible limb excursions required')
        need(value['arm_failures']==value['unreleased_resources']==0 and value['min_fallen_vertex_y_m']>=.019 and value['minimum_actual_fallen_post_clearance_m']>.04 and value['max_segment_length_error_m']<.00001,'Geometry support/joint/resource failure')
        need(value['ground_arrival_max_pnt_discontinuity']<.00001 and value['matched_terminal_variant_error_m']<.00003,'Landing or terminal continuity failure')
    report('VISUAL_REVIEW.json');native();return build

def collect():
    values={}
    def pin(p):values[str(p.absolute())]=sha(p)
    for p in SOURCE.rglob('*'):
        if p.is_file():pin(p)
    for p in (N/'joust-project').rglob('*'):
        if p.is_file()and 'saves'not in p.relative_to(N/'joust-project').parts and p.name not in('graphics.settings','studio-layout.ini','world.studio.workspace','player-profile.config','joust-audio.settings','config.ini'):pin(p)
    for p in N.iterdir():
        if p.is_file()and p.suffix in('.py','.sh','.json')and p.name not in(READY.name,'RELEASE_SWITCH.json'):pin(p)
    for p in(N/'review').rglob('*'):
        if p.is_file()and 'private-project'not in p.parts and '__pycache__'not in p.parts and p.suffix in('.py','.json','.log','.cpp','.png','.bmp','.mp4','.gif','.txt','.stderr'):pin(p)
    for name in ['READY.json','BUILD_PLAN.json','native-bin/rpg']:pin(RUNTIME/name)
    for name in WRAPPERS:pin(ROOT/name);pin(N/'previous-launchers'/name)
    pin(O/'JOUST_REACTIONS_READY.json')
    for path in(N/'review').glob('native-*/EXECUTION.json'):
        for item in read(path).get('inputs',{}):pin(Path(item))
    return values

def publish():
    need(not READY.exists(),'Published receipt is immutable');build=verify(True);inputs=collect()
    value=dict(schema='valwood.joust-limb.release.v1',ready=True,native_only=True,windows_work_performed=False,binary_sha256=build['sha256'],inputs=inputs,immutable_stats={p:stat(p)for p in inputs},project_links={str(p):str(p.resolve())for p in(N/'joust-project').iterdir()if p.is_symlink()})
    temp=READY.with_suffix('.tmp');temp.write_text(json.dumps(value,indent=2)+'\n');temp.replace(READY)
def check():
    d=read(READY);need(d.get('ready')is True and d.get('schema')=='valwood.joust-limb.release.v1','Limb pass not ready');parent();pins(d['inputs'],d['immutable_stats']);routing(True)
    for p,target in d['project_links'].items():need(Path(p).is_symlink()and str(Path(p).resolve())==target,'Project link changed')
def main():
    ap=argparse.ArgumentParser();g=ap.add_mutually_exclusive_group(required=True)
    for name in['preflight','publish','check-only']:g.add_argument('--'+name,action='store_true')
    a=ap.parse_args()
    if a.publish:publish()
    elif a.preflight:verify()
    else:check()
    print('PASS: visible independent limbs, native fall motion and preserved crowd/audio/gameplay.')
if __name__=='__main__':
    try:main()
    except Exception as error:print('FAIL:',error,file=sys.stderr);sys.exit(1)
