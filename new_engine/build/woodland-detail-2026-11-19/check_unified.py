"""Verify the frozen woodland scene, model overlay and rebuilt native closure."""
from pathlib import Path
import hashlib, importlib.util, json, os, sys
sys.dont_write_bytecode = True
N = Path(__file__).resolve().parent
B = N.with_name('campsite-look-2026-11-19')
C = N.with_name('river-swim-2026-11-19')
E = N.parents[1]


def sha(p):
    with Path(p).open('rb') as f:
        return hashlib.file_digest(f, 'sha256').hexdigest()


def read(p):
    return json.loads(Path(p).read_text())


def need(condition, message):
    if not condition:
        raise RuntimeError(message)


def stat(p):
    s = Path(p).stat()
    return [s.st_dev, s.st_ino, s.st_size, s.st_mtime_ns, s.st_ctime_ns]


def inventory(root):
    root = Path(root)
    return {str(p.relative_to(root)): sha(p) for p in sorted(root.rglob('*'))
            if p.is_file() and '__pycache__' not in p.relative_to(root).parts
            and p.suffix not in ('.pyc', '.pyo')}


EXPECTED_SOURCE_CHANGES = frozenset((
    'engine/world_model_cpu.cpp', 'engine/world_model_cpu.h',
    'engine/world_model_facade.h', 'engine/world_models.cpp',
    'engine/world_skeletal_hero_loadouts.cpp', 'engine/world_skeletal_hero_loadouts.h',
    'engine/world_swim_viewmodel.h', 'engine/world_viewer.cpp',
    'engine/world_river_reflection_scissor.h', 'engine/world_surface_rivers.inc',
    'engine/world_main_material_warm.inc',
    'engine/world_graphics_settings.h', 'engine/world_graphics_settings.cpp',
    'engine/world_graphics_viewer.inc', 'tests/world_graphics_settings_test.cpp',
    'shaders/world_foliage.fs.glsl', 'shaders/world_water.fs.glsl',
    'shaders/rig_first_person.fs.glsl'))
EXPECTED_SHADER_CHANGES = frozenset(('world_foliage.fs.glsl', 'world_water.fs.glsl',
                                     'rig_first_person.fs.glsl'))
EXPECTED_MODEL_CHANGES = frozenset(
    'imports/bep_nature_'+name+'/'+guid+'/asset_'+lod+'.glb'
    for name, guid in (('oak', '031e070a7b2612525972c671'),
                       ('beech', 'bbc4abea35407b633d26b6b8'))
    for lod in ('near', 'mid'))
COLLISION_CATALOG = 'collision/structures/catalog'
COLLISION_REVIEW = 'review/foliage-optimization/COLLISION_BINDING_REVIEW.json'
COLLISION_PREIMAGE = 'review/foliage-optimization/catalog.before-leaf-binding'
COLLISION_ALIASES = frozenset(('nm_city_region_beech', 'nm_city_region_oak'))


def checked_scope(path):
    scope = read(path)
    need(scope.get('reviewed') is True and scope.get('root_freeze_authorized') is True,
         'Source/shader scope is not explicitly reviewed and frozen: '+str(path))
    return scope


def source_and_shader_scope():
    source, original = inventory(N/'source-v1'), inventory(B/'source-v1')
    delta = {p: dict(before=original.get(p), after=source.get(p))
             for p in set(source)|set(original) if source.get(p) != original.get(p)}
    scope = checked_scope(N/'review/SOURCE_CHANGE_SCOPE.json')
    need(set(delta) == EXPECTED_SOURCE_CHANGES and scope.get('changes') == delta,
         'Source differs from its explicit reviewed scope')
    bank, oldbank = inventory(N/'shared/shaders'), inventory(B/'shared/shaders')
    need(set(bank) == set(oldbank), 'Shader inventory changed')
    changed = {p for p in bank if bank[p] != oldbank[p]}
    need(changed == EXPECTED_SHADER_CHANGES, 'Unexpected shader scope')
    shader = checked_scope(N/'review/SHADER_CHANGE_SCOPE.json')
    need(shader.get('candidate_sha256') == bank and
         shader.get('changed_shader_sha256') == {p: bank[p] for p in changed},
         'Reviewed shader hashes differ from the final bank')
    for p in changed:
        need(sha(N/'source-v1/shaders'/p) == bank[p], 'Source/runtime shader mismatch: '+p)
    return source, original, scope, shader, bank


def verify_native_build():
    baseline = read(N/'BASELINE.json')
    need(baseline.get('baseline') == str(B) and
         sha(B/'UNIFIED_READY.json') == baseline.get('ready_sha256') and
         sha(B/'runtime/rpg') == baseline.get('binary_sha256') and
         baseline.get('runtime_reused') is False and
         baseline.get('Windows_work_performed') is False, 'Baseline readiness/delivery scope changed')
    source, original, scope, shader, bank = source_and_shader_scope()
    ready, plan = read(N/'runtime-native-v1/READY.json'), read(N/'runtime-native-v1/BUILD_PLAN.json')
    binary = sha(N/'runtime/rpg')
    need(ready.get('ready') is True and ready.get('application_units') == 104 and
         ready.get('full_application_dependency_closure_verified') is True and
         ready.get('native_only') is True and ready.get('Windows_work_performed') is False and
         ready.get('sha256') == binary == sha(N/'runtime-native-v1/native-bin/rpg') and
         Path(ready.get('source', '')) == N/'source-v1', 'Exact 104-unit native build missing')
    need(not (N/'source-v1').is_symlink() and
         source == ready.get('source_sha256') == plan.get('source_sha256'),
         'Current source inventory differs from the compiled frozen source')
    need(read(N/'runtime/ORIGIN_BUILD.json') == dict(
         build_ready=str(N/'runtime-native-v1/READY.json'), binary_sha256=binary,
         source=str(N/'source-v1')), 'Runtime is not the exact reviewed build output')
    origin = read(N/'SOURCE_ORIGIN.json')
    need(origin.get('source') == str(N/'source-v1') and
         origin.get('baseline') == str(B/'source-v1') and
         origin.get('build_baseline') == str(C) and
         origin.get('canonical_source_edited') is False and
         origin.get('baseline_sha256') == original and
         origin.get('baseline_plan_sha256') == sha(C/'runtime-native-v1/BUILD_PLAN.json'),
         'Frozen source/build origin changed')
    need(plan.get('source') == str(N/'source-v1') and
         plan.get('output') == str(N/'runtime-native-v1') and plan.get('skip_player') is False and
         plan.get('native_only') is True and plan.get('Windows_work_performed') is False and
         plan.get('full_application_dependency_closure_verified') is True and
         plan.get('baseline_plan_sha256') == origin['baseline_plan_sha256'], 'Build plan scope changed')
    for name, path in (('reviewed_source_scope_sha256', N/'review/SOURCE_CHANGE_SCOPE.json'),
                       ('reviewed_shader_scope_sha256', N/'review/SHADER_CHANGE_SCOPE.json')):
        need(plan.get(name) == ready.get(name) == sha(path), 'Build scope digest changed: '+name)
    need(plan.get('reviewed_shader_sha256') == ready.get('reviewed_shader_sha256') == bank,
         'Build did not freeze the complete final shader bank')
    base_plan = read(C/'runtime-native-v1/BUILD_PLAN.json')
    rows, base_rows = plan.get('units', []), base_plan['units']
    names = [r['logical_source'] for r in rows]
    need(len(rows) == 104 and len(set(names)) == 104 and
         names == [r['logical_source'] for r in base_rows], 'Application link closure changed')
    def mapped(value):
        return str(value).replace(str(C/'source-v1'), str(N/'source-v1')).replace(
            str(C/'runtime-native-v1'), str(N/'runtime-native-v1'))
    need(plan['link'] == [mapped(x) for x in base_plan['link']], 'Link flags/order changed')
    dependencies, objects = {}, {}
    for row, old in zip(rows, base_rows):
        logical = row['logical_source']; target = N/'runtime-native-v1/native-objects'/(logical+'.o')
        need(row.get('command') == [mapped(x) for x in old['command']] and
             row.get('target') == str(target) and row.get('dependency_file') == str(target.with_suffix('.d')),
             'Compile flags/paths changed: '+logical)
        need(target.is_file() and not target.is_symlink() and target.with_suffix('.d').is_file(),
             'Native object/dependency missing: '+logical)
        receipt = read(target.with_suffix('.json'))
        need(all(receipt.get(k) == row[k] for k in ('logical_source', 'target', 'dependency_file', 'command')),
             'Object receipt differs: '+logical)
        actual = receipt.get('actual_dependency_sha256', {})
        need(actual and actual.get(str(N/'source-v1'/logical)) == source[logical],
             'Object lacks its actual source: '+logical)
        for name, h in actual.items():
            p = Path(name)
            need(p.is_relative_to(N/'source-v1') or p.is_relative_to('/usr/include') or
                 p.is_relative_to('/usr/lib'), 'Compiler dependency escaped source: '+name)
            need(name not in dependencies or dependencies[name] == h, 'Conflicting compiler dependency')
            dependencies[name] = h
        objects[str(target)] = sha(target)
    need(ready.get('object_sha256') == objects, 'Frozen object output inventory changed/missing')
    for p, h in dependencies.items():
        need(Path(p).is_file() and sha(p) == h, 'Compiled dependency changed: '+p)
    import shlex
    for row in rows:
        raw = Path(row['dependency_file']).read_text().replace('\\\n', ' ').split('\n', 1)[0]
        paths = shlex.split(raw.split(':', 1)[1])
        dep = {str((Path(p) if Path(p).is_absolute() else N/'source-v1'/p).resolve()) for p in paths}
        actual = read(Path(row['target']).with_suffix('.json'))['actual_dependency_sha256']
        need(dep == set(actual), 'Object dependency file/receipt differs: '+row['logical_source'])
    library = E/'build/physics/libJolt.a'
    need(str(library) in plan['link'] and sha(library) == plan.get('jolt_library_sha256') ==
         base_plan['jolt_library_sha256'], 'Pinned physics library changed')
    return binary, source, bank


def model_overlay():
    path = N/'review/foliage-optimization/LEAF_OVERLAY.json'; report = read(path)
    need(report.get('baseline') == str(B) and report.get('candidate') == str(N) and
         report.get('published_files_written') is False, 'Model overlay origin changed')
    rows = report.get('private_glbs', [])
    need(len(rows) == 4 and {r['relative_path'] for r in rows} == EXPECTED_MODEL_CHANGES,
         'Unexpected model overlay scope')
    need((N/'world-project/models').is_dir() and not (N/'world-project/models').is_symlink(),
         'Candidate model overlay must be owned')
    changes = {}
    for row in rows:
        relative = row['relative_path']; p=N/'world-project/models'/relative; q=B/'world-project/models'/relative
        need(p.is_file() and not p.is_symlink() and
             sha(p) == row['candidate_glb_sha256'] and sha(q) == row['source_glb_sha256'] and
             row.get('geometry_and_other_images_exact') is True and row.get('ARM_RGA_bytes_exact') is True,
             'Reviewed leaf GLB changed: '+relative)
        changes[relative] = row
    def walk(a, b, rel=Path()):
        need({p.name for p in a.iterdir()} == {p.name for p in b.iterdir()}, 'Model inventory changed: '+str(rel))
        for p in a.iterdir():
            q=b/p.name; r=rel/p.name
            if str(r) in changes: continue
            if p.is_symlink():
                need(p.exists() and p.resolve() == q.resolve(), 'Unreviewed model alias: '+str(r))
            elif p.is_dir():
                need(q.is_dir(), 'Model directory type changed'); walk(p,q,r)
            else:
                need(p.is_file() and q.is_file() and sha(p)==sha(q), 'Unreviewed owned model changed: '+str(r))
    walk(N/'world-project/models', B/'world-project/models')
    return rows


def owned_project_leaf(root, relative):
    root, relative = Path(root), Path(relative)
    need(not relative.is_absolute() and '..' not in relative.parts,
         'Escaping scene target')
    p = root/relative
    need(p.is_file() and not p.is_symlink() and not root.is_symlink() and
         all(not parent.is_symlink() for parent in p.parents if parent.is_relative_to(root)),
         'Scene leaf or parent is not owned: '+str(p))
    return p


def allowed_scene_path(relative):
    return (relative == 'world.studio' or relative == COLLISION_CATALOG or
            relative in {str(p.relative_to(B/'world-project'))
                         for p in (B/'world-project/scenery').glob('*.txt')})


def collision_binding():
    """Accept only the independently verified two-row model revision update."""
    report_path=N/COLLISION_REVIEW
    report=read(report_path)
    before=N/COLLISION_PREIMAGE
    after=owned_project_leaf(N/'world-project',COLLISION_CATALOG)
    need(before.is_file() and not before.is_symlink(), 'Missing owned collision preimage')
    expected_before=read(B/'UNIFIED_READY.json').get('pins',{}).get(
        str(E/'projects/valwood_current'/COLLISION_CATALOG))
    need(expected_before and sha(before)==expected_before,
         'Collision preimage differs from immutable baseline receipt')
    need(report.get('schema')=='valwood.leaf-collision-binding.v1' and report.get('passed') is True and
         report.get('baseline')==str(B) and report.get('candidate')==str(N) and
         report.get('catalog_relative_path')==COLLISION_CATALOG and
         report.get('preimage_path')==str(before) and
         report.get('before_sha256')==sha(before) and report.get('after_sha256')==sha(after) and
         set(report.get('changed_model_aliases',[]))==COLLISION_ALIASES,
         'Collision binding review missing, stale or outside scope')
    checks=report.get('checks',{})
    need(all(checks.get(name) is True for name in (
        'model_revision_only','trunk_vwc_unchanged','mesh_bindings_unchanged',
        'geometry_and_pivots_unchanged','production_cache_binding_passed')),
        'Collision binding preservation review did not pass')
    left,right=before.read_text().splitlines(keepends=True),after.read_text().splitlines(keepends=True)
    need(len(left)==len(right), 'Collision catalog rows changed')
    import shlex
    changed=set()
    for old,new in zip(left,right):
        if old==new:continue
        a,b=shlex.split(old),shlex.split(new)
        need(len(a)==len(b)==4 and a[:2]==b[:2] and a[0]=='model' and
             a[1] in COLLISION_ALIASES and a[1] not in changed,
             'Unreviewed collision catalog row changed')
        changed.add(a[1])
    need(changed==COLLISION_ALIASES,'Exactly two collision model revision rows are required')
    for name,h in report.get('inputs_sha256',{}).items():
        need(Path(name).is_file() and sha(name)==h,'Collision review input changed: '+name)
    need(report.get('inputs_sha256',{}).get(str(before))==sha(before) and
         report.get('inputs_sha256',{}).get(str(after))==sha(after),
         'Collision review lacks exact catalog input pins')
    return report


def inherited_catalog_transition():
    report=collision_binding()
    return dict(schema='valwood.shared-live-catalog-transition.v1',
        path=str(E/'projects/valwood_current'/COLLISION_CATALOG),
        before_sha256=report['before_sha256'],after_sha256=report['after_sha256'],
        preimage_path=str(N/COLLISION_PREIMAGE),preimage_sha256=sha(N/COLLISION_PREIMAGE),
        baseline_ready_path=str(B/'UNIFIED_READY.json'),
        baseline_ready_sha256=sha(B/'UNIFIED_READY.json'),
        binding_review_path=str(N/COLLISION_REVIEW),binding_review_sha256=sha(N/COLLISION_REVIEW))


def verify_inherited_chain(*,live_after):
    """Verify the immutable chain plus one explicit shared authoring transition.

    B accidentally pinned the shared LIVE catalog rather than a private copy.
    Its receipt stays byte-identical. Verify that original pin against the exact
    archived preimage, and separately verify LIVE against the reviewed old/new
    state. Every unrelated inherited pin retains the original verifier.
    """
    transition=inherited_catalog_transition()
    need(transition['baseline_ready_sha256']==read(N/'BASELINE.json')['ready_sha256'],
         'Immutable baseline receipt changed')
    live=owned_project_leaf(E/'projects/valwood_current',COLLISION_CATALOG)
    need(sha(live)==transition['after_sha256' if live_after else 'before_sha256'],
         'Shared collision catalog transition conflict')
    spec=importlib.util.spec_from_file_location('woodland_inherited_exact_check',C/'tools/check_unified.py')
    inherited=importlib.util.module_from_spec(spec);spec.loader.exec_module(inherited)
    original=inherited.verify_pins
    baseline=read(B/'UNIFIED_READY.json');visits=[]
    def verify_document(document):
        pins=document.get('pins',{})
        if transition['path'] in pins:
            need(document==baseline and pins[transition['path']]==transition['before_sha256'],
                 'Unexpected inherited catalog pin or generation')
            visits.append(transition['path'])
            remapped=dict(pins)
            need(transition['preimage_path'] not in remapped,'Inherited preimage path conflicts')
            del remapped[transition['path']]
            remapped[transition['preimage_path']]=transition['before_sha256']
            original(dict(document,pins=remapped))
        else:
            original(document)
    inherited.verify_pins=verify_document
    inherited.verify_chain(B)
    need(visits==[transition['path']],'Required inherited catalog transition was not verified')


def check_inputs(manifest):
    binary, _, bank = verify_native_build()
    need(manifest.get('schema') == 'valwood.woodland-detail.changeset.v1' and
         manifest.get('reviewed') is True and manifest.get('runtime_reused') is False and
         manifest.get('Windows_work_performed') is False and manifest.get('binary_sha256') == binary and
         manifest.get('shader_sha256') == bank and
         manifest.get('build_ready_sha256') == sha(N/'runtime-native-v1/READY.json'),
         'Wrong reviewed delivery scope')
    need(manifest.get('source_changes') == sorted(EXPECTED_SOURCE_CHANGES) and
         manifest.get('shader_changes') == sorted(EXPECTED_SHADER_CHANGES), 'Changed source scope missing')
    model_overlay()
    collision_binding()
    need(manifest.get('collision_binding_review_sha256')==sha(N/COLLISION_REVIEW) and
         manifest.get('shared_live_catalog_transition')==inherited_catalog_transition(),
         'Explicit shared catalog transition evidence missing or stale')
    scenes=manifest.get('scene_changes',[])
    need(scenes and len({r['path']for r in scenes})==len(scenes) and
         all(allowed_scene_path(row['path']) for row in scenes) and
         any(row['path']==COLLISION_CATALOG for row in scenes),
         'Unreviewed or incomplete scene publication scope')
    for row in scenes:
        source=owned_project_leaf(N/'world-project',row['path'])
        before=N/COLLISION_PREIMAGE if row['path']==COLLISION_CATALOG else B/'world-project'/row['path']
        need(row.get('before_sha256')==sha(before) and row.get('after_sha256')==sha(source),
             'Scene changeset differs from reviewed before/after')
    need({r['path'] for r in manifest.get('asset_changes', [])} ==
         {'models/'+r for r in EXPECTED_MODEL_CHANGES}, 'Model publication scope missing')
    for path, h in manifest['pins'].items():
        need(Path(path).is_file() and sha(path) == h, 'Reviewed input changed: '+path)
    for p, target in manifest['aliases'].items():
        need(Path(p).is_symlink() and str(Path(p).resolve()) == target, 'Asset alias changed: '+p)
    return binary


def evidence(manifest):
    binary = check_inputs(manifest)
    for path, h in manifest['evidence'].items():
        need(sha(path) == h, 'Review evidence changed: '+path)
        result = read(path)
        need(result.get('passed') is True, 'Review did not pass: '+path)
        if 'binary_sha256' in result:
            expected = sha(B/'runtime/rpg') if result.get('stage') == 'baseline' or result.get('baseline_comparison') else binary
            need(result['binary_sha256'] == expected, 'Review used another executable')
        if 'shader_input_sha256' in result:
            generation = B if result.get('baseline_comparison') else N
            for name, value in inventory(generation/'shared/shaders').items():
                target = (generation/'shared/shaders'/name).resolve()
                need(any(Path(p).resolve() == target and digest == value
                         for p, digest in result['shader_input_sha256'].items()),
                     'Review did not load final shader: '+name)
    review = read(N/'review/FINAL_REVIEW.json')
    need(review['passed'] and review['visual_review_performed'] and
         not review['AAA_quality_certified'], 'Missing scoped visual acceptance')
    return binary


def check():
    ready = read(N/'UNIFIED_READY.json')
    need(ready['ready'] and ready['generation'] == str(N), 'Generation is not published')
    for path, target in ready['aliases'].items():
        p = Path(path)
        need(p.is_symlink() and p.exists() and str(p.resolve()) == target,
             'Runtime asset alias changed: '+path)
    for path, h in ready['pins'].items():
        p = Path(path)
        need(p.is_file(), 'Missing runtime dependency: '+path)
        current = stat(p)
        if current != ready['stats'].get(path):
            need(sha(p) == h and stat(p) == current, 'Runtime dependency changed: '+path)
    from launch import configuration, AREAS
    for mode in ('demo', 'editor', 'arena', 'joust', 'world'):
        command, _, project = configuration(mode)
        need(command[0] == str(N/'runtime/rpg'), 'Mode uses another executable')
        need((project/'shaders').resolve() == (N/'shared/shaders').resolve(), 'Mode shader mismatch')
        need((project/'character-textures').resolve() == (N/'shared/character-textures').resolve(), 'Character bank mismatch')
    for project, _ in AREAS.values():
        need((project/'shaders').resolve() == (N/'shared/shaders').resolve(), 'Area shader mismatch')
    for relative, h in ready['live_scene_sha256'].items():
        # Working authoring is intentionally mutable after publication. Import
        # and scene editing must not invalidate a correctly installed engine.
        need((E/'projects/valwood_current'/relative).is_file(), 'Working scene missing')
    baseline = read(N/'BASELINE.json')
    need(baseline['baseline'] == str(B) and
         sha(B/'UNIFIED_READY.json') == baseline['ready_sha256'], 'Baseline receipt changed')
    # Models and texture banks retain approved aliases into earlier generations.
    # Preserve their existing verification chain on every normal launch.
    verify_inherited_chain(live_after=True)
    for path, target in ready.get('live_model_aliases', {}).items():
        p = Path(path)
        need(p.is_symlink() and str(p.resolve()) == target, 'Live leaf overlay binding changed: '+path)
    return ready


if __name__ == '__main__':
    check()
    print('Woodland scene, leaf overlay and five shared launch modes verified.')
