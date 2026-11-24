"""Verify the river swimming repair, complete native closure and preserved forest."""
from pathlib import Path
import argparse,hashlib,importlib.util,json,os,sys
sys.dont_write_bytecode=True
N=Path(__file__).resolve().parents[1]
E=N.parents[1]
BASE=N.with_name('scenery-forest-2026-11-19')
BUILD_BASE=N.with_name('apartment-camera-2026-11-19')
LIVE=E/'projects/valwood_current'
READY=N/'UNIFIED_READY.json'
APPROVED_BINARY_SHA='a06b9225132fdbad137b88eb38834880f15c71d3576c7250187fe1bc98a9c5b9'  # Root approves only after fresh native regression evidence.
EXPECTED_SOURCE_CHANGES=frozenset(('engine/world_viewer.cpp','engine/world_story_hero_swim_qa.inc','engine/world_first_journey_qa.inc','shaders/world_water.fs.glsl'))
EXPECTED_SHADER_CHANGES=frozenset(('world_water.fs.glsl',))

def need(ok, message):
    if not ok: raise RuntimeError(message)

def read(path): return json.loads(Path(path).read_text())

def sha(path):
    with Path(path).open('rb') as f: return hashlib.file_digest(f, 'sha256').hexdigest()

def ignored_cache(path):
    p=Path(path)
    return '__pycache__' in p.parts or p.suffix in ('.pyc','.pyo')

def inventory(root):
    root=Path(root)
    return {str(p.relative_to(root)):sha(p) for p in sorted(root.rglob('*'))
            if p.is_file() and not ignored_cache(p.relative_to(root))}

def stamp(path):
    s=Path(path).stat()
    return [s.st_dev,s.st_ino,s.st_size,s.st_mtime_ns,s.st_ctime_ns]

def verify_hashes(pins):
    need(isinstance(pins,dict) and pins,'Missing reviewed input hashes')
    for p,h in pins.items(): need(Path(p).is_file() and sha(p)==h,'Reviewed input changed '+str(p))

def verify_pins(document):
    need(document.get('pins'),'Empty publication pins')
    for p,h in document['pins'].items():
        before=stamp(p)
        if before[2]<1024*1024 or before!=document.get('stats',{}).get(p):
            need(sha(p)==h and stamp(p)==before,'Pinned dependency changed '+p)

def verify_chain(base):
    seen=set()
    while (base/'UNIFIED_READY.json').is_file():
        need(base not in seen,'Baseline cycle');seen.add(base)
        document=read(base/'UNIFIED_READY.json')
        need(document.get('ready') is True,'Baseline not ready')
        if base.name=='unified-runtime-2026-11-16':
            document=dict(document,pins={p:h for p,h in document['pins'].items() if p!=str(E.parent/'CURRENT_VALWOOD.json')})
        verify_pins(document)
        if not document.get('baseline'):break
        p=Path(document['baseline']);base=p if p.is_absolute() else base.with_name(str(p))

def mutable_project_file(relative):
    p=Path(relative)
    return (any(part in ('saves','settings','__pycache__') for part in p.parts)
            or p.name in ('graphics.settings','studio-layout.ini','world.studio.workspace','player-profile.config')
            or (p.name.startswith('arena_') and p.suffix=='.cfg') or '.recovery' in p.name
            or p.suffix in ('.log','.lock','.pyc','.pyo'))

def same_project(snapshot,baseline,changes=None):
    changes=changes or {};changed=set();owned=[]
    need(snapshot.is_dir() and not snapshot.is_symlink(),'Candidate project root must be owned '+str(snapshot))
    def walk(a,b,relative=Path()):
        left={p.name for p in a.iterdir() if not mutable_project_file(relative/p.name)}
        right={p.name for p in b.iterdir() if not mutable_project_file(relative/p.name)}
        need(left==right,'Scene snapshot inventory differs '+str(a))
        for name in sorted(left):
            p,q,rel=a/name,b/name,relative/name
            if rel==Path('shaders'):
                need(p.is_symlink() and p.resolve()==(N/'shared/shaders').resolve()
                     and q.resolve()==(BASE/'shared/shaders').resolve(),'Snapshot shader alias changed')
            elif p.is_symlink() and p.is_dir():
                need(p.resolve()==q.resolve(),'Read-only asset folder changed '+str(p))
            elif p.is_dir():
                need(q.is_dir() and not q.is_symlink(),'Snapshot directory type differs '+str(p));walk(p,q,rel)
            elif p.is_file():
                need(q.is_file(),'Snapshot leaf missing '+str(p))
                h,before=sha(p),sha(q)
                if h!=before:
                    r=changes.get(str(rel))
                    need(r is not None and r['before_sha256']==before and r['after_sha256']==h,'Unreviewed scene change '+str(p))
                    changed.add(str(rel))
                if not p.is_symlink():owned.append(p)
            else:need(False,'Unsupported snapshot entry '+str(p))
    walk(snapshot,baseline)
    need(changed==set(changes),'Reviewed scene changes do not match actual snapshot delta')
    return owned

def resources(require_live=True):
    sys.path.insert(0,str(N))
    from launch import configuration,private_configuration,AREAS
    for mode in ('demo','editor','world'):
        need(configuration(mode)[2]==LIVE,'Public mode lost shared working world')
    modes=[configuration(mode) for mode in ('demo','editor','arena','joust','world')]
    need({r[0][0] for r in modes}=={str(N/'runtime/rpg')},'Modes use different runtimes')
    for mode in ('demo','editor','arena','joust','world'):
        need(private_configuration(mode)[2].is_relative_to(N),'QA escaped candidate')
    projects=set([r[2] for r in modes]+[p for p,_ in AREAS.values()])
    for project in projects:
        need((project/'world.studio').is_file() and (project/'project.valwood').is_file(),'Missing project '+str(project))
        need((project/'saves').is_dir() and not (project/'saves').is_symlink(),'Invalid save directory '+str(project))
        if require_live or project.is_relative_to(N):
            need((project/'shaders').resolve()==(N/'shared/shaders').resolve(),'Shader alias mismatch '+str(project))
        need((project/'character-textures').resolve()==(N/'shared/character-textures').resolve(),'Character bank mismatch '+str(project))

def immutable_file(p):
    r=p.relative_to(N)
    if ignored_cache(r) or any(k in ('.logs','saves','settings','rollback') for k in r.parts):return False
    if mutable_project_file(r) or p.suffix in ('.o','.log','.progress','.save','.exe'):return False
    if p.name in ('UNIFIED_READY.json','PUBLICATION.json','POST_PUBLICATION.json','PROMOTION.json'):return False
    if 'review' in r.parts:
        with p.open('rb') as f:
            if f.read(4)==b'\x7fELF':return False
    return True

def source_scope():
    scope=read(N/'review/SOURCE_CHANGE_SCOPE.json')
    need(scope.get('schema')=='valwood.river-swim.source-change-scope.v1' and scope.get('reviewed') is True
         and scope.get('root_freeze_authorized') is True,'River swimming source scope is not reviewed/frozen')
    changed,recompiled=scope.get('source_changes',[]),scope.get('recompiled_units',[])
    need(isinstance(changed,list) and changed and len(set(changed))==len(changed) and isinstance(recompiled,list) and recompiled and len(set(recompiled))==len(recompiled),'Empty/duplicate source or compile scope')
    for p in changed+recompiled:
        need(isinstance(p,str) and not Path(p).is_absolute() and '..' not in Path(p).parts,'Source scope escaped owned source')
    need(set(changed)==EXPECTED_SOURCE_CHANGES,'Source repair must contain exactly the reviewed viewer, swim/completion QA and water fragment')
    need(scope.get('baseline_sha256')=={p:sha(BASE/'source-v1'/p) for p in changed}
         and scope.get('candidate_sha256')=={p:sha(N/'source-v1'/p) for p in changed},
         'Reviewed source scope hashes differ from current baseline/candidate')
    return set(changed),set(recompiled)

def shader_scope():
    scope=read(N/'review/SHADER_CHANGE_SCOPE.json')
    need(scope.get('schema')=='valwood.river-swim.shader-change-scope.v1' and scope.get('reviewed') is True
         and scope.get('root_freeze_authorized') is True,'Water shader derivative is not reviewed/frozen')
    need(scope.get('shader_changes')==sorted(EXPECTED_SHADER_CHANGES),'Only the water fragment may change in the shared bank')
    bank=N/'shared/shaders';base=BASE/'shared/shaders'
    need(bank.is_dir() and not bank.is_symlink(),'Candidate shader bank must be owned')
    actual,original=inventory(bank),inventory(base)
    need(set(actual)==set(original),'Shared shader inventory changed')
    need({p for p in actual if actual[p]!=original[p]}==EXPECTED_SHADER_CHANGES,'Unexpected shared shader delta')
    need(scope.get('baseline_sha256')=={p:original[p] for p in EXPECTED_SHADER_CHANGES}
         and scope.get('candidate_sha256')=={p:actual[p] for p in EXPECTED_SHADER_CHANGES},'Water shader scope hashes changed')
    for p in EXPECTED_SHADER_CHANGES:
        target=bank/p;source=N/'source-v1/shaders'/p;baseline_source=BASE/'source-v1/shaders'/p
        need(target.is_file() and not target.is_symlink() and source.is_file() and not source.is_symlink()
             and sha(source)==actual[p] and sha(baseline_source)==original[p],
             'Shared water fragment is not the exact owned source derivative')
    return scope

def exact_runtime(require_approval=True):
    baseline = read(N / 'BASELINE.json')
    need(Path(baseline['baseline']) == BASE and sha(BASE / 'UNIFIED_READY.json') == baseline['ready_sha256'], 'Wrong baseline readiness')
    need(sha(BASE / 'runtime/rpg') == baseline['binary_sha256'], 'Baseline executable changed')
    binary = sha(N / 'runtime/rpg')
    if require_approval:need(APPROVED_BINARY_SHA is not None and binary == APPROVED_BINARY_SHA, 'Final river swimming build is not approved')
    origin = read(N / 'runtime/ORIGIN_BUILD.json')
    built = read(N / 'runtime-native-v1/READY.json')
    need(origin == dict(build_ready=str(N / 'runtime-native-v1/READY.json'),
                        binary_sha256=binary, source=str(N / 'source-v1')),
         'Runtime origin differs from exact reviewed build')
    need(built.get('ready') is True and built.get('application_units') == 104
         and built.get('sha256') == binary and binary != baseline['binary_sha256']
         and built.get('full_application_dependency_closure_verified') is True
         and built.get('native_only') is True and built.get('Windows_work_performed') is False,
         'New 104-unit native build incomplete')
    need(sha(N / 'runtime-native-v1/native-bin/rpg') == binary, 'Published executable differs from native output')
    need(Path(built['source']) == N / 'source-v1' and not (N / 'source-v1').is_symlink(), 'Build escaped candidate source')
    source = {p: h for p, h in built['source_sha256'].items() if not ignored_cache(p)}
    need(inventory(N / 'source-v1') == source, 'Frozen source inventory differs from compiled source')
    source_origin = read(N / 'SOURCE_ORIGIN.json')
    need(Path(source_origin['source']) == N / 'source-v1' and Path(source_origin['baseline']) == BASE / 'source-v1'
         and source_origin.get('canonical_source_edited') is False and Path(source_origin.get('build_baseline','')) == BUILD_BASE, 'Source/build origin is not the protected forest/native baseline')
    expected_base = {p: h for p, h in source_origin['baseline_sha256'].items() if not ignored_cache(p)}
    need(inventory(BASE / 'source-v1') == expected_base, 'Source baseline inventory changed')
    changed, recompiled = source_scope()
    need(set(expected_base) <= set(source) and {p for p in source if source[p] != expected_base.get(p)} == changed,
         'River swimming source differs from its reviewed exact scope')
    plan = read(N / 'runtime-native-v1/BUILD_PLAN.json')
    need(plan['source_sha256'] == built['source_sha256'] and plan.get('skip_player') is False
         and plan.get('full_application_dependency_closure_verified') is True
         and Path(plan['source']) == N / 'source-v1' and Path(plan['output']) == N / 'runtime-native-v1', 'Build plan differs from compiled source')
    shader=shader_scope()
    need(plan.get('reviewed_shader_scope_sha256')==built.get('reviewed_shader_scope_sha256')
         ==sha(N/'review/SHADER_CHANGE_SCOPE.json') and plan.get('reviewed_shader_sha256')==shader['candidate_sha256']
         ==built.get('reviewed_shader_sha256'),'Native build lacks the exact reviewed shader derivative')
    need(plan['baseline_plan_sha256'] == sha(BUILD_BASE / 'runtime-native-v1/BUILD_PLAN.json')
         == source_origin['baseline_plan_sha256'], 'Native build closure origin changed')
    rows = plan['units']
    names = [row['logical_source'] for row in rows]
    need(len(rows) == 104 and len(set(names)) == 104, 'Application closure is not 104 unique units')
    base_names = {row['logical_source'] for row in read(BUILD_BASE / 'runtime-native-v1/BUILD_PLAN.json')['units']}
    need(set(names) == base_names, 'Application link closure differs from accepted game/editor')
    seed = read(N / 'CACHE_SEED_REVIEW.json')
    need(recompiled <= set(names) and seed.get('copied_verified_objects') == 104 - len(recompiled)
         and set(seed.get('recompile', [])) == recompiled
         and seed.get('hardlinks') is False, 'Unexpected object reuse scope')
    dependencies = {}
    receipts = []
    for row in rows:
        logical = row['logical_source']
        target = N / 'runtime-native-v1/native-objects' / (logical + '.o')
        need(Path(row['target']) == target and Path(row['dependency_file']) == target.with_suffix('.d') and target.is_file(), 'Object path differs ' + logical)
        receipt_path = target.with_suffix('.json')
        receipt = read(receipt_path)
        need(all(receipt.get(k) == row[k] for k in ('logical_source', 'target', 'dependency_file', 'command')), 'Object receipt/flags differ ' + logical)
        command = row['command']
        need(command[command.index('-c') + 1] == str(N / 'source-v1' / logical)
             and command[command.index('-o') + 1] == str(target), 'Object command escaped candidate ' + logical)
        actual = receipt.get('actual_dependency_sha256', {})
        need(actual.get(str(N / 'source-v1' / logical)) == source[logical], 'Object lacks its actual source dependency ' + logical)
        for path, digest in actual.items():
            p = Path(path)
            need(p.is_relative_to(N / 'source-v1') or p.is_relative_to('/usr/include') or p.is_relative_to('/usr/lib'), 'Compiler escaped frozen source ' + path)
            need(path not in dependencies or dependencies[path] == digest, 'Conflicting object dependency ' + path)
            dependencies[path] = digest
        receipts.append(receipt_path)
    verify_hashes(dependencies)
    objects = [str(N / 'runtime-native-v1/native-objects' / (name + '.o')) for name in names]
    need([arg for arg in plan['link'] if arg.endswith('.o')] == objects, 'Link object order/closure differs')
    library = E / 'build/physics/libJolt.a'
    need(str(library) in plan['link'] and sha(library) == plan['jolt_library_sha256'], 'Pinned physics library changed')
    return binary

def scene_scope():
    scope=read(N/'review/SCENE_CHANGE_SCOPE.json')
    need(scope.get('schema')=='valwood.river-swim.scene-preservation-scope.v1' and scope.get('reviewed') is True
         and scope.get('source_only') is True and scope.get('scene_changes')==[], 'Source-only repair must not publish any scene rows')
    expected=read(BASE/'PUBLICATION.json')['reviewed_scene_after_sha256']
    need(scope.get('protected_forest_scene_sha256')==expected and len(expected)==11,'Current eleven forest scene documents are not preserved')
    for relative,h in expected.items():
        need(sha(N/'world-project'/relative)==sha(BASE/'world-project'/relative)==h,'Accepted forest scene changed '+relative)
    return scope

def unchanged_resources():
    scene_scope()
    shader_scope()
    need((N/'motion').resolve()==(BASE/'motion').resolve(),'Character motion bank changed')
    need({p.name for p in (N/'assets').iterdir()}=={p.name for p in (BASE/'assets').iterdir()},'Asset bank inventory changed')
    for p in (BASE/'assets').iterdir():need((N/'assets'/p.name).resolve()==p.resolve(),'Asset bank alias changed '+p.name)
    need((N/'shared/character-textures').resolve()==(BASE/'shared/character-textures').resolve(),'Character texture bank changed')
    need((N/'world-project/models').is_symlink() and (N/'world-project/models').resolve()==(BASE/'world-project/models').resolve()
         and not (N/'world-project/models').resolve().is_relative_to(LIVE),'Protected model bank changed')
    files=[]
    for mode in ('world','arena','joust'):files+=same_project(N/(mode+'-project'),BASE/(mode+'-project'))
    for area in ('city','grandstone','showcase','atlas'):files+=same_project(N/'area-projects'/area,BASE/'area-projects'/area)
    return files

def evidence(binary):
    scope=scene_scope();relative=scope.get('cpu_review','')
    need(relative.startswith('review/') and not Path(relative).is_absolute() and '..' not in Path(relative).parts,'CPU review escaped candidate')
    cpu_path=N/relative;cpu=read(cpu_path)
    need(cpu.get('passed') is True and cpu.get('gpu_used') is False,'River swimming CPU regression incomplete')
    pins=cpu.get('inputs_sha256',{})
    changed,_=source_scope()
    need(all(pins.get(str(N/'source-v1'/p))==sha(N/'source-v1'/p) for p in changed),'CPU review lacks final changed production/QA source')
    need(not any(Path(p).resolve().is_relative_to(LIVE) for p in pins),'CPU review pins mutable LIVE paths')
    verify_hashes(pins)
    receipts={str(cpu_path):sha(cpu_path)}
    names=scope.get('native_checks',[])
    need(isinstance(names,list) and names and len(set(names))==len(names),'Missing/duplicate fresh native regressions')
    observed_swim_modes=set()
    for name in names:
        need(isinstance(name,str) and name.replace('-','').isalnum(),'Native path escaped review')
        path=N/'review'/name/'EXECUTION.json';result=read(path)
        need(result.get('passed') is True and result.get('binary_sha256')==binary and result.get('baseline_comparison') is False
             and result.get('exit_code')==0 and result.get('timed_out') is False and result.get('software_renderer') is False
             and result.get('world',{}).get('gl_error')==0 and result.get('scene_inputs_unchanged') is True
             and result.get('shader_inputs_unchanged') is True,'Fresh native regression failed '+name)
        verify_hashes(result.get('scene_input_sha256',{}))
        shader_inputs=result.get('shader_input_sha256',{})
        verify_hashes(shader_inputs)
        bank=(N/'shared/shaders').resolve()
        need(all(Path(p).resolve().is_relative_to(bank) for p in shader_inputs),'Native review loaded another shader bank '+name)
        shader_actual={str(Path(p).resolve().relative_to(bank)):h for p,h in shader_inputs.items()}
        need(len(shader_actual)==len(shader_inputs) and shader_actual==inventory(bank),'Native review omitted/differed from final shader bank '+name)
        preparation=read(path.with_name('PREPARATION.json'))
        need(preparation.get('binary_sha256')==binary and preparation.get('shader_input_sha256')==shader_inputs,
             'Native preparation shader/binary pins differ '+name)
        test=result.get('test','')
        origin=N/('joust-project' if test=='joust' else 'arena-project' if test in ('arena','arena-orcs','equipment') or result.get('compact_scene_for_first_person_controls') is True else 'world-project')
        if origin==N/'world-project':
            for p,h in scope['protected_forest_scene_sha256'].items():
                need(result['scene_input_sha256'].get(str(origin/p))==h,'Native fixture omitted preserved forest scene '+p)
        else:
            # A combat regression loads the actual arena/joust snapshot, not
            # the world forest. Do not relabel its source-scene evidence.
            for p in [origin/'world.studio',origin/'city.scene',*sorted((origin/'scenery').glob('*.txt'))]:
                if p.is_file():need(result['scene_input_sha256'].get(str(p))==sha(p),'Mode regression omitted its actual scene '+str(p))
        if test=='story-swim':
            qa=result['world'].get('story_swim_qa',{})
            pair=lambda key,predicate:isinstance(qa.get(key),list) and len(qa[key])==2 and all(predicate(v) for v in qa[key])
            need(qa.get('enabled') is True and qa.get('passed') is True and qa.get('preloan_complete') is True
                 and qa.get('loaned_complete') is True and qa.get('qa_pose_story_health_writes')==0
                 and qa.get('surface_float_checked')==[True,True]
                 and pair('surface_float_samples',lambda v:isinstance(v,int) and v>=60)
                 and pair('maximum_surface_eye_error_m',lambda v:isinstance(v,(int,float)) and 0<=v<=.045),
                 'Actual preloan/loaned neutral surface swimming incomplete '+name)
            if qa.get('require_deep_dive') is True:
                need(qa.get('preloan_dive_checked') is True and qa.get('loaned_dive_checked') is True
                     and qa.get('neutral_depth_hold_checked')==[True,True] and qa.get('render_eye_underwater_checked')==[True,True]
                     and pair('neutral_depth_hold_samples',lambda v:isinstance(v,int) and v>=45)
                     and pair('maximum_neutral_depth_drift_m',lambda v:isinstance(v,(int,float)) and 0<=v<=.035),
                     'Actual rendered-eye dive/depth hold/resurface/shore replay incomplete '+name)
                observed_swim_modes.add('deep')
            else:
                need(qa.get('require_deep_dive') is False,'Native swim mode was not explicit')
                observed_swim_modes.add('shallow')
        receipts[str(path)]=sha(path)
    required_modes=scope.get('required_swim_modes',[])
    need(isinstance(required_modes,list) and set(required_modes)<=set(('shallow','deep'))
         and set(required_modes)<=observed_swim_modes,'Required native swim modes incomplete')
    final=read(N/'review/FINAL_REVIEW.json')
    need(final.get('passed') is True and final.get('native_visual_passed') is True and final.get('binary_sha256')==binary
         and final.get('source_only') is True and final.get('Windows_work_performed') is False,'Final river swimming visual/control review incomplete')
    need(final.get('review_receipts_sha256')==receipts,'Final acceptance does not pin exact actual CPU/native receipts')
    return receipts

def build_artifacts():
    rows=read(N/'runtime-native-v1/BUILD_PLAN.json')['units']
    return [N/p for p in ('runtime-native-v1/READY.json','runtime-native-v1/BUILD_PLAN.json','SOURCE_ORIGIN.json','CACHE_SEED_REVIEW.json','build_native_candidate.py','seed_native_cache.py','review/SOURCE_CHANGE_SCOPE.json','review/SHADER_CHANGE_SCOPE.json')]+[Path(r['target']).with_suffix('.json') for r in rows]+[E/'build/physics/libJolt.a']

def publish():
    need(not READY.exists(),'Published generations are immutable')
    resources();verify_chain(BASE);binary=exact_runtime();snapshots=unchanged_resources();evidence(binary)
    files=[N/p for p in ('runtime/rpg','runtime/ORIGIN_BUILD.json','BASELINE.json','launch.py','check_unified.py','GUIDE.txt','run_native_review.py')]
    files += [BASE/'UNIFIED_READY.json',BUILD_BASE/'UNIFIED_READY.json',*snapshots,*build_artifacts()]
    for folder in ('source-v1','tools','shared/shaders','review'):
        files += [p for p in (N/folder).rglob('*') if p.is_file() and immutable_file(p)]
    files=sorted(set(files));need(all(p.is_file() for p in files),'Publication input missing')
    need(not any('saves' in p.parts or p.resolve().is_relative_to(LIVE) for p in files),'Mutable authoring/save path in immutable pins')
    pins={str(p):sha(p) for p in files};stats={str(p):stamp(p) for p in files}
    need(all(stamp(p)==stats[str(p)] for p in files),'Input changed while freezing')
    changed,recompiled=source_scope();shader=shader_scope();temp=READY.with_suffix('.json.tmp');need(not temp.exists(),'Unexpected readiness temporary')
    temp.write_text(json.dumps(dict(ready=True,one_binary=binary,pins=pins,stats=stats,baseline=str(BASE),build_baseline=str(BUILD_BASE),
         baseline_ready_sha256=sha(BASE/'UNIFIED_READY.json'),source_only=True,runtime_reused=False,application_units=104,
         verified_cached_units=104-len(recompiled),compiled_units=sorted(recompiled),source_changes=sorted(changed),shader_changes=shader['shader_changes'],full_application_dependency_closure_verified=True,
         native_only=True,Windows_work_performed=False,mutable_authoring_and_saves_not_frozen=True),indent=2)+'\n')
    os.replace(temp,READY)

def check():
    document=read(READY);need(document.get('ready') is True,'Not ready')
    resources();unchanged_resources();verify_pins(document);verify_chain(BASE)
    current=read(E.parent/'CURRENT_VALWOOD.json')
    need(Path(current['runtime_generation'])==N and current['binary_sha256']==document['one_binary'],'Current pointer differs')

def cli():
    ap=argparse.ArgumentParser();ap.add_argument('--publish',action='store_true');ap.add_argument('--check-only',action='store_true');ap.add_argument('--audit-source-scene',action='store_true');a=ap.parse_args()
    try:
        if a.audit_source_scene:
            binary=exact_runtime(False);unchanged_resources();resources(False)
            print('Own native104-unit build and unchanged forest scene/resource audit passed; release not approved.')
        else:
            publish() if a.publish else check()
            print('River swimming runtime/forest preservation verified.')
    except Exception as error:print(str(error),file=sys.stderr);raise SystemExit(1)

if __name__=='__main__':cli()
