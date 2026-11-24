#!/usr/bin/env python3
"""Guard the native stadium/reaction release and preserve prior sword Arena."""
from pathlib import Path
import argparse,hashlib,importlib.util,json,os,struct,sys
sys.dont_write_bytecode=True
N=Path(__file__).resolve().parent;E=N.parents[1];ROOT=E.parent
L=N.parent/'joust-controls-2026-11-16';K=N.parent/'character-ultra4k-2026-11-15'
SOURCE=N/'source-v1';RUNTIME=N/'runtime-native-v1';BINARY=RUNTIME/'native-bin/rpg'
READY=N/'JOUST_STADIUM_READY.json'
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
def parent():
    audit=read(N/'BASELINE_AUDIT.json');old=read(L/'JOUST_CONTROLS_READY.json')
    need(audit.get('passed')is True and audit.get('before_root_promotion')is True,'Missing pre-promotion baseline audit')
    need(old.get('ready')is True and sha(L/'JOUST_CONTROLS_READY.json')==audit['parent_sha256'],'Previous joust release changed')
    excluded={str(ROOT/name) for name in WRAPPERS}
    inherited={name:h for name,h in old['inputs'].items() if name not in excluded}
    pins(inherited,old['immutable_stats'])
    for name in WRAPPERS:
        need(sha(N/'previous-launchers'/name)==old['inputs'][str(ROOT/name)]==audit['wrapper_sha256'][name],'Prior wrapper archive changed: '+name)
    for name,target in old['project_links'].items():need(Path(name).is_symlink() and str(Path(name).resolve())==target,'Prior project link changed')
def closure():
    origin=E/'build/arena-joust-2026-11-15/check_joust_ready.py'
    spec=importlib.util.spec_from_file_location('joust_stadium_closure',origin);m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
    m.N=N;m.SOURCE=SOURCE;m.RUNTIME=RUNTIME;m.BINARY=BINARY
    result=m.verify_build()
    old=read(L/'runtime-native-v1/READY.json')['source_sha256'];new=result['source_sha256']
    changed={name for name in old if old[name]!=new.get(name)}|set(new).difference(old)
    allowed={'engine/world_viewer.cpp','engine/world_joust.h','engine/world_joust_runtime.inc','engine/world_joust_render.h','engine/world_joust_qa.inc','engine/world_joust_arm_private.h','engine/world_joust_view.h','engine/world_joust_crowd_private.h','engine/world_joust_rider_private.h','tests/world_joust_test.cpp'}
    need(changed and changed<=allowed,'Unreviewed source scope: '+str(sorted(changed-allowed)))
    return result
def report(name):
    d=read(N/'review'/name)
    need(d.get('passed')is True or d.get('status')=='pass','Incomplete review: '+name)
    if d.get('inputs'):pins(d['inputs'])
    return d
def native():
    summary=report('NATIVE_REVIEW.json');seen=set();proofs=[]
    for row in summary.get('runs',[]):
        directory=Path(row['directory']);mode=row['mode']
        need(directory.is_relative_to(N/'review') and mode in ('controller','keyboard') and mode not in seen,'Invalid native rehearsal route')
        seen.add(mode);execution=read(directory/'EXECUTION.json');raw=read(directory/'JOUST_NATIVE_RAW.json')
        need(execution.get('exit_code')==0 and execution.get('native_passed')is True and execution.get('private_saves')is True and execution.get('native_GPU')is True,'Native rehearsal failed/not isolated')
        pins(execution['inputs']);need(execution['inputs'].get(str(BINARY))==sha(BINARY),'Native test used another binary')
        need(execution['inputs'].get(str(RUNTIME/'READY.json'))==sha(RUNTIME/'READY.json'),'Native test used another READY receipt')
        command=execution.get('command',[])
        need(command and command[0]==str(BINARY) and '--project' in command and command[command.index('--project')+1]==str(directory/'private-project'),'Native test used another command/project')
        need(raw.get('schema')=='valwood.joust.native-sdl-rehearsal.v3' and raw.get('passed')is True and raw.get('error')=='','Native stadium proof incomplete')
        need(execution.get('mode')==raw.get('mode')==mode and raw.get('difficulty')==(2 if mode=='controller' else 0) and raw.get('passes')==(3 if mode=='controller' else 1),'Native test route/difficulty/pass mismatch')
        need(raw.get('actual_SDL_event_route')is True and raw.get('owned_virtual_controller')is True and raw.get('contacts_injected')is False and raw.get('live_state_injected')is False,'Rehearsal bypassed physical input/contact route')
        need(raw.get('GL_errors')==raw.get('renderer_failures')==raw.get('renderer_arm_failures')==0,'Native renderer/driver failure')
        need(raw.get('spectators')==240 and raw.get('crowd_draws',0)>100 and raw.get('crowd_uploads',0)>0,'Animated stadium crowd missing')
        need(raw.get('audience_ready')is True and raw.get('audience_events',0)>0 and raw.get('audience_updates',0)>100,'Actual audience audio missing')
        need(.55<raw.get('maximum_forward_extension_m',0)<=.601 and raw.get('maximum_rendered_grip_error_m',1)<.002 and raw.get('maximum_rendered_hand_surface_drift_m',1)<.002,'Thrust travel and actual hand grip disagree')
        need(raw.get('physical_contacts',0)>0 and raw['physical_contacts']==raw.get('active_thrust_contacts')==raw.get('verified_contact_proxy_sweeps'),'Physical contacts not verified')
        need(raw.get('peak_rendered_unhorsed_riders',0)>0 and raw.get('peak_rendered_grounded_riders',0)>0 and raw.get('riderless_horse_draws',0)>0,'Fall/landing/empty horse not rendered')
        need(raw.get('unhorsed_rider_draws',0)>0 and raw.get('minimum_fallen_vertex_y_m',-1)>=.019,'Actual falling source mesh crossed the floor')
        side=0 if mode=='controller' else 1
        need(raw.get('falling_frames',[0,0])[side]>0 and raw.get('grounded_frames',[0,0])[side]>0,'Helmet fall did not complete')
        if mode=='controller':
            need(raw.get('first_person_player_unhorse_view')is True and all(raw.get('received_hit_types',[0]*4)[i]>0 for i in (1,2,3)),'Incoming shield/body/head reactions missing')
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
    need(seen=={'controller','keyboard'},'Both actual native input routes required')
    visual=report('VISUAL_REVIEW.json')
    for path in proofs:need(visual.get('inputs',{}).get(str(path))==sha(path),'Visual review does not pin native evidence')
def routing(promoted):
    for name in ['launch_joust.sh','launch_arena.sh']:
        p=N/name;s=p.read_text()
        need(os.access(p,os.X_OK) and 'check_joust_stadium_ready.py' in s and 'flock -n' in s and 'unset LD_PRELOAD' in s,'Unsafe launch routing')
    need('runtime-native-v1/native-bin/rpg' in (N/'launch_joust.sh').read_text(),'Joust runtime routing missing')
    need(str(K.relative_to(E)) in (N/'launch_arena.sh').read_text(),'Sword Arena must retain its published Ultra runtime/project')
    if promoted:
        for name in WRAPPERS:
            s=(ROOT/name).read_text();target='launch_joust.sh' if name.endswith('Joust.sh') else 'launch_arena.sh'
            need(str(N.relative_to(ROOT)) in s and target in s,'Root launcher targets another build: '+name)
def verify(promoted=False):
    parent();build=closure();routing(promoted)
    cpu=report('CORE_CPU_REVIEW.json')
    need(cpu.get('source_frozen')is True,'Core tests were not frozen')
    need({row['path'] for row in cpu['owned_source_files']}=={str(SOURCE/'engine/world_joust.h'),str(SOURCE/'tests/world_joust_test.cpp')},'Core test/source provenance missing')
    for row in cpu['owned_source_files']:need(sha(row['path'])==row['sha256'],'Core changed after tests')
    need(len(cpu['tests'])==2,'Normal/sanitized core runs missing')
    for row in cpu['tests']:
        need(row['compile_exit']==row['run_exit']==0 and row['assertions']>=591818 and sha(row['log'])==row['log_sha256'],'Core test run/log incomplete')
    report('RENDER_CPU_REVIEW.json')
    qa=report('QA_CPU_REVIEW.json');need(len(qa.get('commands',[]))==3,'QA CPU command coverage missing')
    for row in qa['commands']:need(row['exit_code']==0 and sha(row['log'])==row['log_sha256'],'QA CPU command/log failed')
    host=report('HOST_REVIEW.json');need(len(host.get('runs',[]))==4,'Camera/audio normal and sanitized runs missing')
    for row in host['runs']:need(row['compile_exit']==row['run_exit']==0 and sha(row['log'])==row['log_sha256'],'Camera/audio test command/log failed')
    report('INDEPENDENT_VISUAL_REVIEW.json')
    native()
    return build
def collect():
    values={}
    def pin(p):values[str(p.absolute())]=sha(p)
    for p in SOURCE.rglob('*'):
        if p.is_file():pin(p)
    for p in (N/'joust-project').rglob('*'):
        if p.is_file() and 'saves' not in p.relative_to(N/'joust-project').parts and p.name not in ('graphics.settings','studio-layout.ini','world.studio.workspace','player-profile.config'):pin(p)
    for p in N.iterdir():
        if p.is_file() and p.suffix in ('.py','.sh','.json') and p.name not in (READY.name,'RELEASE_SWITCH.json'):pin(p)
    for p in (N/'review').rglob('*'):
        if p.is_file() and 'private-project' not in p.parts and '__pycache__' not in p.parts and p.suffix in ('.py','.json','.log','.cpp','.png','.bmp','.md','.txt','.html'):pin(p)
    for name in ('READY.json','BUILD_PLAN.json','native-bin/rpg'):pin(RUNTIME/name)
    for name in WRAPPERS:pin(ROOT/name);pin(N/'previous-launchers'/name)
    pin(L/'JOUST_CONTROLS_READY.json');pin(K/'runtime-native-v1/native-bin/rpg')
    for path in (N/'review').glob('native-*/EXECUTION.json'):
        for item in read(path).get('inputs',{}):pin(Path(item))
    return values
def publish():
    need(not READY.exists(),'Published release receipt is immutable')
    build=verify(True);inputs=collect()
    value={'schema':'valwood.joust-stadium.release.v1','ready':True,'native_only':True,'windows_work_performed':False,'binary_sha256':build['sha256'],'inputs':inputs,'immutable_stats':{name:stat(name) for name in inputs},'project_links':{str(p):str(p.resolve()) for p in (N/'joust-project').iterdir() if p.is_symlink()}}
    temporary=READY.with_suffix('.tmp');temporary.write_text(json.dumps(value,indent=2)+'\n');temporary.replace(READY)
def check():
    d=read(READY);need(d.get('schema')=='valwood.joust-stadium.release.v1' and d.get('ready')is True,'Stadium release not ready')
    parent();pins(d['inputs'],d['immutable_stats']);routing(True)
    for name,target in d['project_links'].items():need(Path(name).is_symlink() and str(Path(name).resolve())==target,'Project link changed')
def main():
    ap=argparse.ArgumentParser();g=ap.add_mutually_exclusive_group(required=True)
    for name in ('check-only','preflight','publish'):g.add_argument('--'+name,action='store_true')
    a=ap.parse_args()
    if a.preflight:verify()
    elif a.publish:publish()
    else:check()
    print('PASS: native joust stadium, stronger thrust and mounted reactions; prior Arena/Ultra preserved.')
if __name__=='__main__':
    try:main()
    except Exception as error:print('FAIL:',error,file=sys.stderr);sys.exit(1)
