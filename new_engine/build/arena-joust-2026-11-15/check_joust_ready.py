#!/usr/bin/env python3
"""Gate the specific, natively rehearsed standalone jousting practice release.

Publishing performs the inherited arena audit once. Launch checks only this
release's own immutable inputs and inherited link/receipt identities; it does
not recursively read every purchased original asset on each startup.
"""
from pathlib import Path
import argparse,hashlib,json,math,os,shlex,struct,subprocess,sys
sys.dont_write_bytecode=True
N=Path(__file__).resolve().parent
E=N.parents[1];ROOT=E.parent
B=N.parent/'arena-closeup-finish-2026-11-15'
SOURCE=N/'source-v1';PROJECT=N/'joust-project';RUNTIME=N/'runtime-native-v1'
BINARY=RUNTIME/'native-bin/rpg';READY=N/'JOUST_READY.json'
NATIVE_RUNS={'native-keyboard-v7':('keyboard',0,1),'native-controller-v2':('controller',2,3)}
_sha_cache={}
def need(value,message):
    if not value:raise RuntimeError(message)
def sha(path):
    path=Path(path);st=path.stat();key=(st.st_dev,st.st_ino,st.st_size,st.st_mtime_ns)
    if key not in _sha_cache:
        with path.open('rb') as stream:_sha_cache[key]=hashlib.file_digest(stream,'sha256').hexdigest()
    return _sha_cache[key]
def read(path):return json.loads(Path(path).read_text())
def hash_inputs(items,description='reviewed inputs'):
    need(isinstance(items,dict) and items,'Missing '+description)
    for name,digest in items.items():
        path=Path(name)
        need(path.is_absolute() and path.is_file() and sha(path)==digest,description+' changed: '+name)
def tree(root):return {str(p.relative_to(root)):sha(p) for p in sorted(root.rglob('*')) if p.is_file()}
def finite_number(value):return isinstance(value,(int,float)) and math.isfinite(value)
def bmp(path):
    with path.open('rb') as stream:header=stream.read(30)
    need(len(header)>=30 and header[:2]==b'BM','Missing native BMP data: '+str(path))
    width,height=struct.unpack_from('<ii',header,18)
    need((width,abs(height))==(1600,900),'Wrong native BMP resolution: '+str(path))
def png(path):
    with path.open('rb') as stream:header=stream.read(24)
    need(len(header)==24 and header[:8]==b'\x89PNG\r\n\x1a\n' and header[12:16]==b'IHDR' and struct.unpack('>II',header[16:24])==(1600,900),'Wrong native PNG resolution: '+str(path))
def receipt(path):
    value=read(path);need(value.get('passed') is True,'Incomplete/failed review: '+str(path))
    if 'inputs' in value:hash_inputs(value['inputs'],str(path))
    return value
def verify_build():
    build=read(RUNTIME/'READY.json');plan=read(RUNTIME/'BUILD_PLAN.json')
    need(build.get('ready') is True and build.get('native_only') is True and build.get('Windows_work_performed') is False,'Native build is incomplete')
    need(build.get('application_units')==104 and build.get('full_application_dependency_closure_verified') is True,'Incomplete application closure')
    need(build.get('source')==str(SOURCE) and plan.get('source')==str(SOURCE),'Build used another source checkout')
    need(build.get('binary')==str(BINARY) and sha(BINARY)==build.get('sha256'),'Reviewed native binary differs from build')
    with BINARY.open('rb') as stream:need(stream.read(4)==b'\x7fELF','Joust binary is not native ELF')
    current=tree(SOURCE)
    need(current==build.get('source_sha256')==plan.get('source_sha256'),'Source differs from final compiled source snapshot')
    units=plan.get('units',[]);need(len(units)==104 and len({r['logical_source'] for r in units})==104,'Repeated/missing application units')
    logical={r['logical_source'] for r in units};need({'engine/world_viewer.cpp','engine/world_models.cpp','engine/world_skeletal_hero.cpp','engine/world_road_traffic.cpp'}<=logical,'Core application units omitted')
    dependency_union=set()
    for row in units:
        target=Path(row['target']);dep=Path(row['dependency_file'])
        need(target.is_relative_to(RUNTIME) and target.is_file() and dep.is_file(),'Missing isolated application object: '+row['logical_source'])
        item=read(target.with_suffix('.json'))
        for key in ('logical_source','target','dependency_file','command'):need(item.get(key)==row[key],'Object build command mismatch: '+row['logical_source'])
        dependencies=item.get('actual_dependency_sha256',{});hash_inputs(dependencies,'Compiled dependency')
        raw=dep.read_text().replace('\\\n',' ').split('\n',1)[0].split(':',1)[1]
        actual={str((SOURCE/p).resolve()) if not Path(p).is_absolute() else str(Path(p).resolve()) for p in shlex.split(raw)}
        need(actual==set(dependencies),'Compiler dependency receipt differs from actual dependency file: '+row['logical_source'])
        for name in actual:
            p=Path(name);need(p.is_relative_to(SOURCE) or p.is_relative_to('/usr/include') or p.is_relative_to('/usr/lib'),'Object escaped isolated source: '+name)
        dependency_union.update(actual)
    for name in ('engine/world_joust.h','engine/world_joust_runtime.inc','engine/world_joust_render.h','engine/world_joust_impacts.h','engine/world_joust_qa.inc'):
        need(str(SOURCE/name) in dependency_union,'Final joust source did not participate in actual compilation: '+name)
    need(plan.get('link',[None])[0] and str(BINARY) in plan['link'],'Link did not produce this binary')
    return build
def verify_assets():
    mount=receipt(N/'joust-assets/JOUST_MOUNT_ASSETS.json')
    need(len(mount['charge'])==16 and len(mount['idle'])==4,'Mounted source cycle is incomplete')
    need(mount.get('lance_length')==4.8 and mount.get('lance_tip_local')==[0,0,4.8] and mount.get('lance_grip_local')==[0,0,0],'Owned lance geometry contract differs')
    need(mount.get('forward')=='+Z' and mount.get('up')=='+Y','Mounted source axes differ')
    for row in mount['charge']+mount['idle']+[mount['lance'],mount['shield']]:
        path=N/'joust-assets'/row['path'];need(path.is_relative_to(N/'joust-assets') and path.is_file() and sha(path)==row['sha256'],'Owned model differs from accepted cook: '+str(path))
    cpu=receipt(N/'review/assets/JOUST_ASSETS_CPU_REVIEW.json')
    need(cpu.get('actual_glb_decoding') is True and cpu.get('topology_identical') is True and cpu.get('baked_shield_absent') is True and cpu.get('poses')==20,'Mounted asset topology/geometry not independently checked')
    receipt(N/'review/assets/JOUST_ASSETS_VISUAL_REVIEW.json')
    separation=receipt(N/'review/assets/SHIELD_SEPARATION_CPU_REVIEW.json')
    need(separation.get('socket_unchanged') is True and separation.get('shared_atlas_unchanged') is True,'Shield extraction changed mounted socket/atlas')
    venue=receipt(N/'review/JOUST_ARENA_ASSET.json');chip=receipt(N/'review/JOUST_CHIP_ASSET.json')
    for value,path in [(venue,N/'joust-assets/arena.glb'),(chip,N/'joust-assets/wood_chip.glb')]:need(value.get('asset')==str(path) and sha(path)==value.get('sha256'),'Venue/chip differs from audited asset')
    need(venue['course']['lane_obstructions']==[] and venue.get('ground_base_y')==0 and venue.get('ordinary_base_aligned_loader_safe') is True,'Course clearance/base alignment audit failed')
    need(chip.get('triangles')==12 and chip.get('size_m')==[.03,.03,.22] and chip.get('gore') is False,'Temporary wood shard contract changed')
    receipt(N/'review/JOUST_ARENA_IMPORT_CHECK.json');receipt(N/'review/JOUST_CHIP_IMPORT_CHECK.json')
    impacts=receipt(N/'review/JOUST_IMPACTS_CPU_REVIEW.json')
    need(impacts.get('bounded_pool') is True and impacts.get('pause_freezes') is True and impacts.get('deterministic_emit') is True and impacts.get('expires') is True and impacts.get('pool_capacity')==48,'Wood feedback pool not verified')
    core=receipt(N/'review/CORE_CPU_REVIEW.json')
    need(core.get('normal_and_ASAN_UBSAN') is True and core.get('checks_per_run',0)>=384294,'Mounted rule tests incomplete')
    need(len(core.get('results',[]))==2 and all(r.get('exit_code')==0 for r in core['results']),'Core normal/sanitized runs did not both pass')
    ground=receipt(N/'review/JOUST_GROUND_CPU_REVIEW.json')
    need((PROJECT/'ground').is_dir() and not (PROJECT/'ground').is_symlink(),'Joust course ground must be a private copy')
    need(ground.get('private_copy')==str(PROJECT/'ground') and ground.get('shared_ground_modified') is False and ground.get('source_shared_hashes_unchanged') is True,'Ground pass modified the shared baseline')
    need(tree(PROJECT/'ground')==ground.get('after_file_hashes'),'Private course ground differs from its CPU review')
    need(ground.get('flat_height_y')==64 and ground.get('visible_sand_y')==64.08 and ground.get('venue_touching_bilinear_cell_max_y',100)<=64.02,'Terrain can bury the joust sand/course')
    scenery=receipt(N/'review/SCENERY_GROUND_CPU_REVIEW.json')
    hash_inputs(scenery.get('after_file_hashes',{}),'Audited course scenery')
    need(scenery.get('project_only') is True and scenery.get('models_modified') is False and scenery.get('shared_ground_modified') is False,'Course cleanup changed shared source assets')
    need(scenery.get('preserved_fields_except_height') is True and scenery.get('maximum_root_offset_error_m',1)<.001,'Scenery grounding changed local transforms/root offsets')
    need(scenery.get('fallback_props')==0 and scenery.get('remaining_rows',0)>0 and scenery.get('removed_inside_course',0)>0,'Course scenery clearance/grounding review incomplete')
    controller=receipt(N/'review/CONTROLLER_BACKGROUND_SDL_DIAGNOSTIC.json')
    need(controller.get('actual_SDL_window_and_virtual_controller') is True and controller.get('no_OpenGL_context') is True and controller.get('source_and_project_unchanged') is True,'Hidden controller accommodation lacks a bounded actual-SDL diagnostic')
def verify_launcher(root_required=False):
    stage=N/'launch_joust.sh';text=stage.read_text()
    for token in ('check_joust_ready.py','--check-only','runtime-native-v1/native-bin/rpg','joust-project','VALWOOD_JOUST_ARENA=1','flock -n','unset LD_PRELOAD'):
        need(token in text,'Stage launcher lacks required routing/isolation: '+token)
    need(os.access(stage,os.X_OK),'Stage launcher is not executable')
    old=read(B/'CLOSEUP_FINISH_READY.json')
    for name in ('Launch_Valwood_Arena.sh','Launch_Valwood_Arena_Visual.sh'):
        path=ROOT/name;need(sha(path)==old['inputs'][str(path)],'Existing sword arena launcher was changed: '+name)
    wrapper=ROOT/'Launch_Valwood_Joust.sh'
    if root_required or wrapper.exists():
        need(wrapper.is_file() and os.access(wrapper,os.X_OK),'New root joust launcher missing/not executable')
        wrapper_text=wrapper.read_text()
        need((str(N) in wrapper_text or str(N.relative_to(ROOT)) in wrapper_text) and 'launch_joust.sh' in wrapper_text,'Root joust launcher selects another stage')
def verify_run(label,mode,difficulty,passes,build):
    directory=N/'review'/label;execution=read(directory/'EXECUTION.json');raw=read(directory/'JOUST_NATIVE_RAW.json')
    need(execution.get('exit_code')==0 and execution.get('native_passed') is True and execution.get('mode')==mode,'Native execution failed or mode differs: '+label)
    need(execution.get('private_saves') is True and execution.get('native_GPU') is True,'Native run is not isolated GPU execution: '+label)
    expected_environment={'VALWOOD_PRESENTATION':'1','VALWOOD_PLAYTEST_SLICE':'city','VALWOOD_JOUST_ARENA':'1','VALWOOD_QA_JOUST':'1','VALWOOD_QA_JOUST_DIR':str(directory),'VALWOOD_QA_JOUST_MODE':mode,'SDL_AUDIODRIVER':'dummy','MESA_DEBUG':'1','SDL_JOYSTICK_ALLOW_BACKGROUND_EVENTS':'1'}
    need(execution.get('environment')==expected_environment,'Native replay environment differs or contains unreviewed overrides: '+label)
    need(execution.get('background_controller_accommodation')=='Private hidden native window only; host focus guards remain active','Hidden controller accommodation is not explicitly bounded: '+label)
    inputs=execution.get('inputs',{});hash_inputs(inputs,label+' execution input')
    need(inputs.get(str(BINARY))==build['sha256'],'Native run used another binary: '+label)
    need(inputs.get(str(RUNTIME/'READY.json'))==sha(RUNTIME/'READY.json'),'Native run used another build receipt: '+label)
    private=directory/'private-project'
    needed=[BINARY,RUNTIME/'READY.json',N/'review/run_joust_native.py',private/'world.studio',private/'graphics.settings',private/'playtest.route']
    needed+=list((N/'joust-assets').rglob('*.glb'))+[N/'joust-assets/mount.cfg']
    needed+=list((SOURCE/'engine').glob('world_joust*'))
    needed+=list((private/'shaders').glob('*.glsl'))
    needed+=[p for p in (private/'ground').rglob('*') if p.is_file()]
    needed+=[p for p in (private/'scenery').rglob('*') if p.is_file()]
    need(set(inputs)=={str(p) for p in needed},'Native run omitted or substituted actual build/assets/shaders: '+label)
    for name in ('world.studio','playtest.route'):need(sha(private/name)==sha(PROJECT/name),'Native run used another course/light setup: '+label)
    for shader in (PROJECT/'shaders').glob('*.glsl'):need(sha(private/'shaders'/shader.name)==sha(shader),'Native shader differs from launch project: '+shader.name)
    need(tree(private/'ground')==tree(PROJECT/'ground'),'Native terrain differs from the launch course: '+label)
    need(tree(private/'scenery')==tree(PROJECT/'scenery'),'Native scenery differs from the launch course: '+label)
    need((private/'saves').is_dir() and not (private/'saves').is_symlink(),'Native run used shared/public saves: '+label)
    command=execution.get('command',[])
    need(command and command[0]==str(BINARY) and '--benchmark' not in command and '--editor' not in command and '--play' in command and '--hidden' in command,'Wrong native replay command: '+label)
    for flag,value in [('--project',str(private)),('--width','1600'),('--height','900'),('--scenic','city-walk')]:need(flag in command and command[command.index(flag)+1]==value,'Native replay argument differs: '+flag)
    need(raw.get('schema')=='valwood.joust.native-sdl-rehearsal.v1' and raw.get('passed') is True and raw.get('error')=='','Incomplete native rehearsal: '+label)
    for name in ('actual_SDL_event_route','owned_virtual_controller','private_practice_only','renderer_ready'):need(raw.get(name) is True,'Missing actual native witness: '+name)
    for name in ('live_state_injected','contacts_injected','source_triangle_anatomy_claim'):need(raw.get(name) is False,'Native review injected state/contact or overclaims anatomy: '+name)
    need(raw.get('mode')==mode and raw.get('difficulty')==difficulty and raw.get('passes')==passes,'Wrong native challenge: '+label)
    need(raw.get('match_starts')==1 and raw.get('match_completions')==1,'Native full match not completed exactly once: '+label)
    need(raw.get('GL_errors')==raw.get('renderer_failures')==0,'Native GL/render failure: '+label)
    for name in ('input_events','physical_contacts','verified_contact_proxy_sweeps','feedback_dispatches','audio_dispatches','renderer_shadow_draws'):need(raw.get(name,0)>0,'Missing native activity: '+name)
    need(raw['verified_contact_proxy_sweeps']==raw['physical_contacts'],'Native contacts were not all geometrically verified: '+label)
    need(raw.get('renderer_pose_updates',0)>100 and raw.get('renderer_color_draws',0)>100,'Insufficient native draw coverage: '+label)
    need(finite_number(raw.get('maximum_grip_error_m')) and 0<=raw['maximum_grip_error_m']<.001,'Grip/contact binding differs: '+label)
    checks=set(raw.get('checks',[]))
    required={'private-marked-output-and-owned-SDL-controller','SDL-setup-difficulty-and-pass-selection','SDL-lance-aim-couch-and-horse-motion','pause-freezes-horse-and-simulation-clock','SDL-first-and-third-person-camera-toggle','complete-physical-match-pass-totals-and-no-practice-payout','actual-mounted-pose-color-shadow-and-feedback-dispatch'}
    if mode=='controller':required|={'held-RT-disarmed-after-resume-until-neutral','SDL-next-pass-and-distinct-right-stick-aims'}
    need(required<=checks,'Native input/pause/camera/match checks incomplete: '+label)
    rows=raw.get('passes_reviewed',[]);need(len(rows)==passes and [r['pass'] for r in rows]==list(range(1,passes+1)),'Native pass sequence incomplete: '+label)
    player=opponent=0
    for row in rows:
        need(row.get('player_hit') in range(4) and row.get('opponent_hit') in range(4),'Invalid native hit type')
        player+=row['player_hit'];opponent+=row['opponent_hit']
        need(row['player_score']==player and row['opponent_score']==opponent,'Native scores differ from contact outcomes: '+label)
    need(raw.get('player_score')==player and raw.get('opponent_score')==opponent,'Native final score differs: '+label)
    paths=[Path(p) for p in raw.get('capture_paths',[])]
    need(len(paths)==raw.get('captures') and len(paths)>=7 and len(set(paths))==len(paths),'Incomplete or duplicate native capture coverage: '+label)
    stems={p.stem for p in paths}
    need({'setup','charge-third-person','paused','charge-first-person','match-complete'}|{f'pass{i}-result' for i in range(1,passes+1)}<=stems,'Required native views/results missing: '+label)
    need(any(p.stem.endswith('-impact') for p in paths),'Actual lance contact was never captured: '+label)
    need(set(directory.glob('*.bmp'))==set(paths),'Native raw capture inventory differs: '+label)
    for path in paths:
        need(path.parent==directory and path.suffix=='.bmp','Capture escapes intended native run: '+str(path));bmp(path);png(path.with_suffix('.png'))
    log=(directory/'native.log').read_text(errors='replace')
    need('[Joust QA] PASS mode='+mode in log and '[Joust QA] FAIL' not in log,'Native log lacks successful completed rehearsal: '+label)
    need('GL_INVALID_' not in log and 'Mesa: User error' not in log,'Driver reported GL errors: '+label)
    return len(paths)
def verify_reviews(root_required=False):
    build=verify_build();verify_assets();verify_launcher(root_required)
    count=sum(verify_run(label,*spec,build) for label,spec in NATIVE_RUNS.items())
    visual=receipt(N/'review/VISUAL_REVIEW.json')
    need(isinstance(visual.get('inputs'),dict) and visual['inputs'],'Visual acceptance lacks reviewed input hashes')
    for label in NATIVE_RUNS:
        for name in ('EXECUTION.json','JOUST_NATIVE_RAW.json'):need(visual['inputs'].get(str(N/'review'/label/name))==sha(N/'review'/label/name),'Visual review did not inspect final native run: '+label)
        for path in (N/'review'/label).glob('*.png'):need(visual['inputs'].get(str(path))==sha(path),'Visual review omitted a final native capture: '+str(path))
    return build,count
def links():return {str(p):str(p.resolve()) for p in PROJECT.iterdir() if p.is_symlink()}
def collect_pins():
    pins={}
    def pin(path):
        path=Path(path);need(path.is_file(),'Missing publish input: '+str(path));pins[str(path.absolute())]=sha(path)
    for folder in (SOURCE,N/'joust-assets',PROJECT/'ground',PROJECT/'shaders',PROJECT/'environments',PROJECT/'scenery'):
        for path in folder.rglob('*'):
            if path.is_file():pin(path)
    for path in PROJECT.iterdir():
        # Graphics settings and saves remain editable by their normal controls.
        if path.is_file() and not path.is_symlink() and path.name not in ('graphics.settings','arena_player_character.cfg','studio-layout.ini','world.studio.workspace'):pin(path)
    for path in N.iterdir():
        if path.is_file() and path.suffix in ('.py','.md','.json','.sh') and path.name not in ('JOUST_READY.json','RELEASE_SWITCH.json'):pin(path)
    for path in (RUNTIME/'READY.json',RUNTIME/'BUILD_PLAN.json',BINARY,B/'CLOSEUP_FINISH_READY.json',B/'check_closeup_ready.py'):pin(path)
    for row in read(RUNTIME/'BUILD_PLAN.json')['units']:
        pin(Path(row['target']).with_suffix('.json'));pin(Path(row['dependency_file']))
    for path in (N/'review').rglob('*'):
        if not path.is_file() or '__pycache__' in path.parts or 'private-project' in path.parts:continue
        # Preserve accepted proof and reproduction, not failed-run leftovers.
        relative=path.relative_to(N/'review')
        if relative.parts[0].startswith('native-') and relative.parts[0] not in NATIVE_RUNS:continue
        if path.suffix in ('.json','.py','.c','.cpp','.png','.bmp','.log','.md'):pin(path)
    for label in NATIVE_RUNS:
        for name in read(N/'review'/label/'EXECUTION.json')['inputs']:
            path=Path(name)
            if path.is_relative_to(N):pin(path)
    for name in ('Launch_Valwood_Arena.sh','Launch_Valwood_Arena_Visual.sh','Launch_Valwood_Joust.sh'):pin(ROOT/name)
    return pins
def publish():
    need(not READY.exists(),'Published joust receipt is immutable; create a new reviewed revision')
    build,captures=verify_reviews(True)
    # One full inherited audit at publication. Future launches pin its receipt
    # identity and original link destinations without rereading the entire bank.
    result=subprocess.run([sys.executable,str(B/'check_closeup_ready.py'),'--check-only'],cwd=E,text=True,capture_output=True)
    need(result.returncode==0,'Accepted close-up baseline failed its audit: '+result.stdout[-1200:]+result.stderr[-1200:])
    # The inherited audit can take time. Recheck that this candidate still
    # matches its compiled/rehearsed inputs immediately before pinning it.
    build,captures=verify_reviews(True)
    result={'schema':'valwood.joust.release.v1','ready':True,'native_only':True,'windows_work_performed':False,'native_runtime':'runtime-native-v1','binary_sha256':build['sha256'],'complete_native_runs':list(NATIVE_RUNS),'native_captures':captures,'inputs':collect_pins(),'project_links':links(),'scope':'Standalone mounted practice arena; actual SDL keyboard and controller full-match rehearsals, physical swept scoring proxies, owned horse/rider cycle, temporary lance, wood splinters, arena venue and first/third-person views. No free-world riding, exact anatomy, unseating ragdoll, wager or AAA/parity claim.','inherited_baseline_audited_at_publish':str(B/'CLOSEUP_FINISH_READY.json')}
    tmp=READY.with_suffix('.tmp');tmp.write_text(json.dumps(result,indent=2,sort_keys=True)+'\n');tmp.replace(READY)
def check_only():
    release=read(READY);need(release.get('schema')=='valwood.joust.release.v1' and release.get('ready') is True and release.get('native_only') is True,'Joust release is incomplete')
    need(release.get('native_runtime')=='runtime-native-v1' and release.get('complete_native_runs')==list(NATIVE_RUNS),'Joust release refers to another native revision')
    hash_inputs(release.get('inputs',{}),'Published joust input')
    need(links()==release.get('project_links'),'Reviewed shared project links changed')
    need(sha(BINARY)==release.get('binary_sha256'),'Published joust binary changed')
    verify_launcher(True)
def main():
    ap=argparse.ArgumentParser();mode=ap.add_mutually_exclusive_group();mode.add_argument('--preflight',action='store_true');mode.add_argument('--publish',action='store_true');mode.add_argument('--check-only',action='store_true');args=ap.parse_args()
    if args.preflight:
        need(not READY.exists(),'Preflight is for an unpublished candidate');_,captures=verify_reviews(False);print(f'PASS: final native joust preflight; two completed matches, {captures} actual captures.');return
    if args.publish:publish()
    check_only();print('PASS: reviewed native joust practice, owned mount/venue/wood feedback and isolated launch verified.')
if __name__=='__main__':
    try:main()
    except (OSError,ValueError,KeyError,TypeError,IndexError,RuntimeError) as error:sys.exit(str(error))
