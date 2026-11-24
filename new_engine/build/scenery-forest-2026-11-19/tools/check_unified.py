"""Fail-closed verifier for a scene-only forest generation; no new runtime."""
from pathlib import Path
import argparse
import hashlib
import importlib.util
import json
import os
import sys

sys.dont_write_bytecode = True
N = Path(__file__).resolve().parents[1]
E = N.parents[1]
BASE = N.with_name('apartment-camera-2026-11-19')
LIVE = E / 'projects/valwood_current'
READY = N / 'UNIFIED_READY.json'
APPROVED_BINARY_SHA = 'a1a37ad698393fa238428a3598307c7f37d00c498e49f4bc41bfad9bbdd7e51c'  # Root reviewed the final native, visual and CPU evidence.
VIEWS = ('camp-eye', 'camp-west-creek', 'village-road', 'woodland-crossing')


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


def exact_runtime(require_approval=True):
    baseline=read(N/'BASELINE.json')
    need(Path(baseline['baseline'])==BASE and sha(BASE/'UNIFIED_READY.json')==baseline['ready_sha256'],'Baseline readiness changed')
    binary=sha(N/'runtime/rpg')
    need(binary==sha(BASE/'runtime/rpg')==baseline['binary_sha256'],'Scene-only generation changed the executable')
    if require_approval:
        need(APPROVED_BINARY_SHA is not None and binary==APPROVED_BINARY_SHA,'Final scene-only release is not approved')
    origin=read(N/'runtime/ORIGIN_BUILD.json')
    need(origin==dict(binary_sha256=binary,reused_exact_reviewed_runtime=str(BASE/'runtime/rpg'),
                     build_ready=str(BASE/'runtime-native-v1/READY.json'),application_source_changed=False),'Wrong reused build origin')
    need(not (N/'source-v1').is_symlink() and inventory(N/'source-v1')==inventory(BASE/'source-v1'),'Scene-only source differs from accepted baseline')
    built=read(BASE/'runtime-native-v1/READY.json')
    need(built.get('ready') is True and built.get('sha256')==binary and built.get('application_units')==104
         and built.get('full_application_dependency_closure_verified') is True
         and built.get('native_only') is True and built.get('Windows_work_performed') is False,'Inherited 104-unit native build is not verified')
    return binary


def mutable_project_file(relative):
    p=Path(relative)
    return (any(part in ('saves','settings','__pycache__') for part in p.parts)
            or p.name in ('graphics.settings','studio-layout.ini','world.studio.workspace')
            or (p.name.startswith('arena_') and p.suffix=='.cfg') or '.recovery' in p.name
            or p.suffix in ('.log','.lock','.pyc','.pyo'))


def allowed_scene_path(value):
    p=Path(value)
    return (isinstance(value,str) and not p.is_absolute() and '..' not in p.parts
            and (p in (Path('world.studio'),Path('city.scene'))
                 or len(p.parts)==2 and p.parts[0]=='scenery' and p.suffix=='.txt'))


def calibrated_city_scene(candidate,baseline):
    expected={'sky':(.47,.50,.53),'ground':(.18,.19,.15),'zenith':(.22,.25,.27)}
    before=baseline.read_bytes().decode('utf-8').splitlines(keepends=True)
    after=candidate.read_bytes().decode('utf-8').splitlines(keepends=True)
    need(len(before)==len(after),'City calibration changed scene structure')
    found=set()
    for b,a in zip(before,after):
        tokens=b.split();key=tokens[0] if tokens else ''
        if key in expected:
            row=a.split()
            need(key not in found and len(row)==4 and row[0]==key
                 and tuple(float(v) for v in row[1:])==expected[key], 'Unreviewed city calibration '+key)
            found.add(key)
        else:need(a==b,'City calibration changed an unrelated scene row')
    need(found==set(expected),'Incomplete city sky/ground/zenith calibration')


def scene_scope():
    scope=read(N/'review/SCENE_CHANGE_SCOPE.json')
    need(scope.get('schema')=='valwood.forest-scene-change-scope.v1' and scope.get('reviewed') is True,'Forest scene scope is not reviewed')
    rows=scope.get('scene_changes',[])
    need(isinstance(rows,list) and rows and len({r.get('path') for r in rows})==len(rows),'Empty or duplicate scene scope')
    guard=read(N/'review/LIVE_BEFORE.json')
    for row in rows:
        rel=row.get('path')
        need(allowed_scene_path(rel),'Scene publication escaped world.studio/scenery/exact city calibration')
        p,q=N/'world-project'/rel,BASE/'world-project'/rel
        need(p.is_file() and not p.is_symlink() and q.is_file(),'Scene change must have an owned candidate and existing baseline leaf '+rel)
        need(row.get('before_sha256')==sha(q)==guard.get(str(LIVE/rel)),'Scene before hash differs from snapshot/live guard '+rel)
        need(row.get('after_sha256')==sha(p) and row['after_sha256']!=row['before_sha256'],'Scene after hash is not the actual changed candidate '+rel)
        if rel=='city.scene':calibrated_city_scene(p,q)
    return scope


def draft_scene_scope():
    """Preliminary byte/alias audit; never substitutes for reviewed release scope."""
    rows=[]
    for p in sorted((N/'world-project').rglob('*')):
        relative=p.relative_to(N/'world-project')
        if not p.is_file() or mutable_project_file(relative):continue
        q=BASE/'world-project'/relative
        if not q.is_file() or sha(p)==sha(q):continue
        need(allowed_scene_path(str(relative)),'Unexpected draft scene change '+str(relative))
        need(not p.is_symlink(),'Draft scene change is not owned '+str(relative))
        if relative==Path('city.scene'):calibrated_city_scene(p,q)
        rows.append(dict(path=str(relative),before_sha256=sha(q),after_sha256=sha(p)))
    need(bool(rows),'No draft scene changes found')
    return dict(scene_changes=rows)


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


def unchanged_resources(scope=None):
    scope=scene_scope() if scope is None else scope
    rows={r['path']:r for r in scope['scene_changes']}
    need(not (N/'shared/shaders').is_symlink() and inventory(N/'shared/shaders')==inventory(BASE/'shared/shaders'),'Scene-only shader bytes changed')
    need((N/'motion').resolve()==(BASE/'motion').resolve(),'Motion bank changed')
    need({p.name for p in (N/'assets').iterdir()}=={p.name for p in (BASE/'assets').iterdir()},'Asset bank inventory changed')
    for p in (BASE/'assets').iterdir():
        need((N/'assets'/p.name).resolve()==p.resolve(),'Asset bank changed '+p.name)
    need((N/'shared/character-textures').resolve()==(BASE/'shared/character-textures').resolve(),'Character texture bank changed')
    # The source snapshot initially inherited a LIVE model alias. It has been
    # rebound to the *same resolved bank as BASE*, not copied or substituted.
    # Retain the actual equality review and reject a later return to LIVE.
    models=N/'world-project/models';base_models=BASE/'world-project/models'
    need(models.is_symlink() and models.resolve()==base_models.resolve()
         and not models.resolve().is_relative_to(LIVE),'World model bank is not the protected baseline target')
    alias=read(N/'review/MODEL_ALIAS_REVIEW.json')
    need(alias.get('passed') is True and alias.get('no_model_bytes_changed') is True
         and Path(alias.get('new_alias','')).resolve()==models.resolve(),'Model alias equality review missing')
    leaves=alias.get('identical_direct_leaves_sha256',{})
    directories=alias.get('identical_directory_alias_targets',{})
    need(isinstance(leaves,dict) and leaves and isinstance(directories,dict) and directories,'Empty logical model-bank review')
    for name,h in leaves.items():
        relative=Path(name.lstrip('/'))
        need('..' not in relative.parts and len(h)==64 and (models/relative).is_file()
             and (models/relative).resolve()==(base_models/relative).resolve(),'Reviewed model leaf no longer aliases BASE '+name)
    for name,target in directories.items():
        relative=Path(name.lstrip('/'))
        need('..' not in relative.parts and (models/relative).is_dir()
             and (models/relative).resolve()==(base_models/relative).resolve()==Path(target).resolve(),
             'Inherited model directory alias changed '+name)
    files=same_project(N/'world-project',BASE/'world-project',rows)
    for mode in ('arena','joust'):files+=same_project(N/(mode+'-project'),BASE/(mode+'-project'))
    for area in ('city','grandstone','showcase','atlas'):files+=same_project(N/'area-projects'/area,BASE/'area-projects'/area)
    return files


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


def evidence(binary):
    scope=scene_scope()
    cpu_rel=scope.get('cpu_review')
    need(isinstance(cpu_rel,str) and cpu_rel.startswith('review/') and '..' not in Path(cpu_rel).parts,'CPU report path escaped review')
    cpu_path=N/cpu_rel;cpu=read(cpu_path)
    need(cpu.get('passed') is True and cpu.get('gpu_used') is False,'Production forest scene CPU review failed')
    pins=cpu.get('inputs_sha256',{})
    need(any(Path(p).is_relative_to(N/'source-v1') for p in pins),'CPU review lacks actual production source')
    for row in scope['scene_changes']:
        p=N/'world-project'/row['path']
        need(pins.get(str(p))==row['after_sha256'],'CPU review lacks final scene file '+row['path'])
    need(not any(Path(p).resolve().is_relative_to(LIVE) for p in pins),'CPU review pins mutable LIVE instead of final snapshot')
    verify_hashes(pins)
    checks=scope.get('native_checks',[])
    need(isinstance(checks,list) and checks and len(set(checks))==len(checks),'Missing or duplicate native reviews')
    receipts={str(cpu_path):sha(cpu_path)}
    for name in checks:
        need(isinstance(name,str) and name.replace('-','').isalnum(),'Native path escaped review')
        p=N/'review'/name/'EXECUTION.json';result=read(p)
        need(result.get('passed') is True and result.get('binary_sha256')==binary and result.get('baseline_comparison') is False
             and result.get('exit_code')==0 and result.get('timed_out') is False and result.get('software_renderer') is False
             and result.get('world',{}).get('gl_error')==0 and result.get('scene_inputs_unchanged') is True,'Native review failed '+name)
        verify_hashes(result.get('scene_input_sha256',{}))
        for row in scope['scene_changes']:
            need(result['scene_input_sha256'].get(str(N/'world-project'/row['path']))==row['after_sha256'],'Native review lacks final scene '+row['path'])
        if result.get('test')=='journey':
            qa=result['world'].get('first_journey_qa',{})
            need(qa.get('passed') is True and qa.get('qa_player_pose_writes')==0 and qa.get('qa_story_state_writes')==0,'Ordinary full journey did not complete')
        receipts[str(p)]=sha(p)
    forest=scope.get('forest_views',{})
    common=forest.get('name','')
    names={stage:forest.get(stage+'_name',common) for stage in ('baseline','candidate')}
    need(all(isinstance(name,str) and name.replace('-','').isalnum() for name in names.values())
         and tuple(forest.get('views',[]))==VIEWS,'All four matched forest views are required')
    paired={}
    for stage in ('baseline','candidate'):
        gen=BASE if stage=='baseline' else N
        for view in VIEWS:
            folder=N/'review/forest-views'/names[stage]/stage/view
            p=folder/'EXECUTION.json';result=read(p);prep=read(folder/'PREPARATION.json')
            need(result.get('passed') is True and result.get('stage')==stage and result.get('view')==view
                 and result.get('binary_sha256')==binary and result.get('exit_code')==0 and result.get('timed_out') is False
                 and result.get('input_unchanged') is True and result.get('exact_camera_checked') is True
                 and result.get('assets_settled_at_capture') is True and result.get('resolution_checked') is True
                 and result.get('gl_error')==0 and result.get('software_renderer') is False,'Matched forest view failed '+stage+'/'+view)
            need(Path(prep['generation'])==gen and result.get('preparation_sha256')==sha(folder/'PREPARATION.json')
                 and prep.get('resolution')==[1600,900],'Matched view uses wrong generation or capture resolution')
            verify_hashes(prep.get('input_sha256',{}))
            for row in scope['scene_changes']:
                expected=row['before_sha256'] if stage=='baseline' else row['after_sha256']
                need(prep['input_sha256'].get(str(gen/'world-project'/row['path']))==expected,'Matched view scene differs '+stage+'/'+row['path'])
            receipts[str(p)]=sha(p)
            paired[(stage,view)]=(prep,result)
    quality=('preset','automatic','target_fps','display_width','display_height',
             'render_width','render_height','effective_scale','minimum_observed_scale',
             'maximum_observed_scale','reflections','reflection_scale','contact_shadows',
             'volumetric_fog','volumetric_clouds')
    for view in VIEWS:
        bp,br=paired[('baseline',view)];cp,cr=paired[('candidate',view)]
        need(bp.get('camera')==cp.get('camera') and bp.get('resolution')==cp.get('resolution')==[1600,900]
             and bp.get('actual_settings_sha256')==cp.get('actual_settings_sha256')
             and bp.get('quality_frozen')==cp.get('quality_frozen')
             and bp.get('fixed_visual_time') is True and cp.get('fixed_visual_time') is True,
             'A/B camera, saved quality or fixed visual time differs '+view)
        bg,cg=br.get('graphics',{}),cr.get('graphics',{})
        need(isinstance(bg,dict) and isinstance(cg,dict) and all(k in bg and k in cg and bg[k]==cg[k] for k in quality),
             'A/B effective graphics quality differs '+view)
    final=read(N/'review/FINAL_REVIEW.json')
    need(final.get('passed') is True and final.get('native_visual_passed') is True
         and final.get('binary_sha256')==binary and final.get('scene_only') is True
         and final.get('Windows_work_performed') is False,'Final scoped Linux visual review incomplete')
    need(final.get('review_receipts_sha256')==receipts,'Final review does not pin exactly the actual required receipts')
    return receipts


def immutable_file(p):
    r=p.relative_to(N)
    if ignored_cache(r) or any(k in ('.logs','saves','settings','rollback') for k in r.parts):return False
    if mutable_project_file(r) or p.suffix in ('.o','.log','.progress','.save','.exe'):return False
    if p.name in ('UNIFIED_READY.json','PUBLICATION.json','POST_PUBLICATION.json','PROMOTION.json'):return False
    if 'review' in r.parts:
        with p.open('rb') as f:
            if f.read(4)==b'\x7fELF':return False
    return True


def publish():
    need(not READY.exists(),'Published generations are immutable')
    resources();verify_chain(BASE);binary=exact_runtime();snapshots=unchanged_resources();evidence(binary)
    files=[N/p for p in ('runtime/rpg','runtime/ORIGIN_BUILD.json','BASELINE.json','launch.py','check_unified.py','GUIDE.txt','run_native_review.py')]
    files += [BASE/'UNIFIED_READY.json',*snapshots]
    for folder in ('source-v1','tools','shared/shaders','review'):
        files += [p for p in (N/folder).rglob('*') if p.is_file() and immutable_file(p)]
    files=sorted(set(files))
    need(all(p.is_file() for p in files),'Publication input missing')
    need(not any('saves' in p.parts or p.resolve().is_relative_to(LIVE) for p in files),'Mutable live/save path in publication pins')
    pins={str(p):sha(p) for p in files};stats={str(p):stamp(p) for p in files}
    need(all(stamp(p)==stats[str(p)] for p in files),'Input changed while freezing')
    temporary=READY.with_suffix('.json.tmp')
    need(not temporary.exists(),'Unexpected readiness temporary')
    temporary.write_text(json.dumps(dict(ready=True,one_binary=binary,pins=pins,stats=stats,baseline=str(BASE),
       baseline_ready_sha256=sha(BASE/'UNIFIED_READY.json'),scene_only=True,runtime_reused=True,application_units=104,
       native_only=True,Windows_work_performed=False,mutable_authoring_and_saves_not_frozen=True),indent=2)+'\n')
    os.replace(temporary,READY)


def check():
    document=read(READY);need(document.get('ready') is True,'Not ready')
    resources();unchanged_resources();verify_pins(document);verify_chain(BASE)
    current=read(E.parent/'CURRENT_VALWOOD.json')
    need(Path(current['runtime_generation'])==N and current['binary_sha256']==document['one_binary'],'Current pointer differs')


def cli():
    ap=argparse.ArgumentParser();ap.add_argument('--publish',action='store_true');ap.add_argument('--check-only',action='store_true');ap.add_argument('--audit-source-scene',action='store_true')
    a=ap.parse_args()
    try:
        if a.audit_source_scene:
            binary=exact_runtime(False);draft=draft_scene_scope();unchanged_resources(draft);resources(False)
            print(json.dumps(dict(passed=True,preliminary=True,binary_sha256=binary,
                                  source_shaders_assets_exact_baseline=True,scene_changes=draft['scene_changes'],
                                  reviewed_scope_required_for_release=True,release_approval_or_native_evidence_certified=False),indent=2))
        else:
            publish() if a.publish else check()
            print('Scene-only forest generation and inherited native runtime verified.')
    except Exception as error:print(str(error),file=sys.stderr);raise SystemExit(1)


if __name__=='__main__':cli()
