"""Verify the grounded idle generation and its native build closure."""
from pathlib import Path
import argparse
import hashlib
import json
import os
import sys

N = Path(__file__).resolve().parents[1]
E = N.parents[1]
BASE = N.with_name('camp-road-polish-2026-11-19')
BUILD_BASE = BASE
READY = N / 'UNIFIED_READY.json'
NATIVE = ('walking-stowed', 'opening-natural', 'arena-regression')
APPROVED_BINARY_SHA = 'c69153e1b60b17dd6e1c7daca0c11c242d54d132267b7f9c209927e817dbf642'
RECOMPILED = {'engine/world_skeletal_hero.cpp'}

def need(ok, message):
    if not ok:
        raise RuntimeError(message)

def read(path):
    return json.loads(Path(path).read_text())

def sha(path):
    with Path(path).open('rb') as handle:
        return hashlib.file_digest(handle, 'sha256').hexdigest()

def ignored_cache(path):
    p = Path(path)
    return '__pycache__' in p.parts or p.suffix in ('.pyc', '.pyo')

def inventory(root):
    root = Path(root)
    return {str(p.relative_to(root)): sha(p) for p in sorted(root.rglob('*'))
            if p.is_file() and not ignored_cache(p.relative_to(root))}

def stamp(path):
    s = Path(path).stat()
    return [s.st_dev, s.st_ino, s.st_size, s.st_mtime_ns, s.st_ctime_ns]

def verify_hashes(pins):
    need(bool(pins), 'Missing reviewed input hashes')
    for path, digest in pins.items():
        need(sha(path) == digest, 'Reviewed input changed ' + str(path))

def verify_pins(document):
    for path, digest in document['pins'].items():
        before = stamp(path)
        if before[2] < 1024 * 1024 or before != document.get('stats', {}).get(path):
            need(sha(path) == digest and stamp(path) == before, 'Pinned dependency changed ' + path)

def verify_chain(base):
    seen = set()
    while (base / 'UNIFIED_READY.json').is_file():
        need(base not in seen, 'Baseline cycle')
        seen.add(base)
        document = read(base / 'UNIFIED_READY.json')
        need(document['ready'], 'Baseline not ready')
        if base.name == 'unified-runtime-2026-11-16':
            document = dict(document, pins={p: h for p, h in document['pins'].items()
                                           if p != str(E.parent / 'CURRENT_VALWOOD.json')})
        verify_pins(document)
        if not document.get('baseline'):
            break
        path = Path(document['baseline'])
        base = path if path.is_absolute() else base.with_name(str(path))

def resources():
    sys.path.insert(0, str(N))
    from launch import configuration, private_configuration, AREAS
    for mode in ('demo', 'editor', 'world'):
        need(configuration(mode)[2] == E / 'projects/valwood_current', 'Public mode lost the working world')
    for mode in ('demo', 'editor', 'arena', 'joust', 'world'):
        need(private_configuration(mode)[2].is_relative_to(N), 'QA escaped its private project')
    need((N / 'motion').resolve() == (BASE / 'motion').resolve(), 'Character motion bank alias changed')
    need({p.name for p in (N / 'assets').iterdir()} == {p.name for p in (BASE / 'assets').iterdir()}, 'Asset bank inventory changed')
    for bank in (p.name for p in (BASE / 'assets').iterdir()):
        need((N / 'assets' / bank).resolve() == (BASE / 'assets' / bank).resolve(), 'Protected asset bank alias changed ' + bank)
    need((N / 'shared/character-textures').resolve() == (BASE / 'shared/character-textures').resolve(), 'Character texture bank changed')
    modes = [configuration(mode) for mode in ('demo', 'editor', 'arena', 'joust', 'world')]
    need({row[0][0] for row in modes} == {str(N / 'runtime/rpg')}, 'Modes use different executables')
    for project in [row[2] for row in modes] + [p for p, _ in AREAS.values()]:
        need((project / 'world.studio').is_file() and (project / 'project.valwood').is_file(), 'Missing scene ' + str(project))
        need((project / 'shaders').resolve() == (N / 'shared/shaders').resolve(), 'Shader mismatch ' + str(project))
        need((project / 'character-textures').resolve() == (N / 'shared/character-textures').resolve(), 'Character bank mismatch ' + str(project))
        need((project / 'saves').is_dir() and not (project / 'saves').is_symlink(), 'Invalid save directory ' + str(project))

def exact_runtime():
    baseline = read(N / 'BASELINE.json')
    need(Path(baseline['baseline']) == BASE and sha(BASE / 'UNIFIED_READY.json') == baseline['ready_sha256'], 'Wrong baseline readiness')
    need(sha(BASE / 'runtime/rpg') == baseline['binary_sha256'], 'Baseline executable changed')
    binary = sha(N / 'runtime/rpg')
    need(APPROVED_BINARY_SHA is not None and binary == APPROVED_BINARY_SHA, 'Final approved posture build is not frozen')
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
         and source_origin.get('canonical_source_edited') is False, 'Source origin is not the protected camp-road baseline')
    expected_base = {p: h for p, h in source_origin['baseline_sha256'].items() if not ignored_cache(p)}
    need(inventory(BASE / 'source-v1') == expected_base, 'Source baseline inventory changed')
    need(set(source) == set(expected_base) and {p for p in source if source[p] != expected_base[p]} == RECOMPILED, 'Idle repair exceeded its single implementation unit')
    plan = read(N / 'runtime-native-v1/BUILD_PLAN.json')
    need(plan['source_sha256'] == built['source_sha256'] and plan.get('skip_player') is False
         and plan.get('full_application_dependency_closure_verified') is True
         and Path(plan['source']) == N / 'source-v1' and Path(plan['output']) == N / 'runtime-native-v1', 'Build plan differs from compiled source')
    need(plan['baseline_plan_sha256'] == sha(BUILD_BASE / 'runtime-native-v1/BUILD_PLAN.json')
         == source_origin['baseline_plan_sha256'], 'Native build closure origin changed')
    rows = plan['units']
    names = [row['logical_source'] for row in rows]
    need(len(rows) == 104 and len(set(names)) == 104, 'Application closure is not 104 unique units')
    base_names = {row['logical_source'] for row in read(BUILD_BASE / 'runtime-native-v1/BUILD_PLAN.json')['units']}
    need(set(names) == base_names, 'Application link closure differs from accepted game/editor')
    seed = read(N / 'CACHE_SEED_REVIEW.json')
    need(seed.get('copied_verified_objects') == 103 and set(seed.get('recompile', [])) == RECOMPILED
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
    return binary, source, receipts + [library]

def mutable_project_file(relative):
    p = Path(relative)
    return (any(part in ('saves', 'settings', '__pycache__') for part in p.parts)
            or p.name in ('graphics.settings', 'studio-layout.ini', 'world.studio.workspace')
            or (p.name.startswith('arena_') and p.suffix == '.cfg') or '.recovery' in p.name or p.suffix in ('.log', '.lock'))

def same_project(snapshot, baseline):
    """Compare owned scene text and the exact read-only asset folder targets."""
    files = []
    def walk(a, b, relative=Path()):
        left = {p.name for p in a.iterdir() if not mutable_project_file(relative / p.name)}
        right = {p.name for p in b.iterdir() if not mutable_project_file(relative / p.name)}
        need(left == right, 'Snapshot inventory differs ' + str(a))
        for name in sorted(left):
            p, q, rel = a / name, b / name, relative / name
            if rel == Path('shaders'):
                need(p.is_symlink() and p.resolve() == (N / 'shared/shaders').resolve()
                     and q.resolve() == (BASE / 'shared/shaders').resolve(), 'Project shader binding changed')
            elif p.is_symlink() and p.is_dir():
                need(p.resolve() == q.resolve(), 'Read-only asset folder changed ' + str(p))
            elif p.is_dir():
                need(q.is_dir(), 'Snapshot folder type differs ' + str(p))
                walk(p, q, rel)
            elif p.is_file():
                need(q.is_file() and sha(p) == sha(q), 'Snapshot data changed ' + str(p))
                if not p.is_symlink():
                    files.append(p)
            else:
                need(False, 'Unsupported snapshot entry ' + str(p))
    walk(snapshot, baseline)
    return files

def unchanged_resources():
    need(not (N / 'shared/shaders').is_symlink() and inventory(N / 'shared/shaders') == inventory(BASE / 'shared/shaders'), 'Shader bytes changed')
    need((N / 'motion').resolve() == (BASE / 'motion').resolve(), 'Motion bank changed')
    need(not (N / 'world-project/opening/home.physics').is_symlink(), 'Physics leaf must be owned for O_NOFOLLOW loading')
    files = []
    for mode in ('world', 'arena', 'joust'):
        files += same_project(N / (mode + '-project'), BASE / (mode + '-project'))
    return files

def head_evidence(binary):
    """Require actual before/after pose tests and matched CPU-skinned images."""
    source = N / 'source-v1/engine/world_skeletal_hero.cpp'
    old_source = N / 'review/pre-head-fix/source-v1/engine/world_skeletal_hero.cpp'
    head = read(N / 'review/head-posture/CPU_REVIEW.json')
    need(head.get('schema') == 'valwood.head-posture-independent-cpu.v1'
         and head.get('passed') is True and head.get('gpu_used') is False
         and head.get('production_modified') is False and head.get('outfit_loadout_cases') == 8
         and head.get('errors') == [], 'Independent head-posture CPU review incomplete')
    need(head.get('candidate_execution', {}).get('source_sha256') == sha(source)
         and head.get('pre_head_v4_execution', {}).get('source_sha256') == sha(old_source)
         == 'ce563d73c1e4594e0091d16c2fb21171282bc4f0266ef656dd0d6d6943cbdc35',
         'Head review does not compare current source with exact pre-head correction')
    verify_hashes(head.get('inputs_sha256', {}))
    phases = head.get('phases', {})
    expected = {'idle', 'retry-idle', 'pause', 'hitstop', 'draw-bridge', 'stow-bridge',
                'armed', 'moving', 'swim', 'dive', 'death'}
    need(set(phases) == expected, 'Head review state coverage incomplete')
    for name, metrics in phases.items():
        bridge = name in ('draw-bridge', 'stow-bridge')
        need(metrics.get('samples', 0) >= 8
             and metrics.get('maximum_local_translation_scale_difference') == 0
             and 0 <= metrics.get('maximum_neck_head_length_error_m', -1) <= 1.e-6,
             'Head correction escaped neck chain or changed bone length: ' + name)
        need(0 <= metrics.get('maximum_non_neck_descendant_component_difference', -1) <= (3.e-6 if bridge else 0)
             and 0 <= metrics.get('maximum_non_neck_position_difference_m', -1) <= (3.e-6 if bridge else 0)
             and 0 <= metrics.get('maximum_non_neck_orientation_difference_deg', -1) <= (.001 if bridge else 0),
             'Head correction changed non-neck pose beyond bridge-only numerical allowance: ' + name)
        if name in ('idle', 'retry-idle', 'pause', 'hitstop'):
            need(.025 <= metrics.get('head_retraction_min_m', 0)
                 <= metrics.get('head_retraction_max_m', 1) <= .04
                 and 0 <= metrics.get('maximum_head_orientation_error_deg', -1) <= .05,
                 'Neutral head position/gaze regression: ' + name)
        if name in ('moving', 'armed', 'swim', 'dive', 'death'):
            need(metrics.get('maximum_palette_difference') == 0, 'Non-idle pose changed: ' + name)
        if name in ('pause', 'hitstop'):
            need(metrics.get('maximum_pause_or_hitstop_pose_change') == 0, 'Frozen head pose changed: ' + name)
        if bridge:
            need(0 <= metrics.get('maximum_head_step_m', -1) <= .012
                 and metrics['maximum_head_step_m'] <= metrics.get('maximum_old_head_step_m', -1) + .005
                 and 0 <= metrics.get('maximum_head_orientation_error_deg', -1) <= 2
                 and 0 <= metrics.get('maximum_world_gaze_shortest_arc_error_deg', -1) <= .05
                 and 0 <= metrics.get('maximum_world_gaze_overshoot_deg', -1) <= .05
                 and 0 <= metrics.get('maximum_head_angular_step_deg', -1) <= 3
                 and metrics['maximum_head_angular_step_deg'] <= metrics.get('maximum_old_head_angular_step_deg', -1) + .5,
                 'Equipment/head transition discontinuity: ' + name)
    need(0 <= head.get('maximum_actual_head_capsule_error_m', -1) <= 1.e-6,
         'Actual head capsule diverged from actual rendered head pose')
    folder = N / 'review/head-profile-preview'
    visual = read(folder / 'VISUAL_REVIEW.json')
    need(visual.get('schema') == 'valwood.head-profile-cpu-visual-review.v1'
         and visual.get('passed') is True and visual.get('gpu_used') is False
         and visual.get('production_data_modified') is False
         and visual.get('native_runtime_binary_sha256') == binary,
         'Current head-profile CPU visual review incomplete')
    inputs = visual.get('current_input_sha256', {})
    need(inputs.get(str(source)) == sha(source), 'Visual head profile used another source')
    verify_hashes(inputs)
    need(visual.get('export_receipt_sha256') == sha(folder / 'EXPORT_RECEIPT.json')
         and visual.get('render_receipt_sha256') == sha(folder / 'RENDER_RECEIPT.json'),
         'Head profile export/render receipt changed')
    equivalence = read(folder / 'FINAL_SETTLED_EQUIVALENCE.json')
    need(visual.get('settled_pose_equivalence_sha256') == sha(folder / 'FINAL_SETTLED_EQUIVALENCE.json')
         and equivalence.get('schema') == 'valwood.head-profile-final-settled-equivalence.v1'
         and equivalence.get('passed') is True and equivalence.get('gpu_used') is False
         and equivalence.get('production_data_modified') is False
         and equivalence.get('final_source_hero_cpp_sha256') == sha(source)
         and equivalence.get('native_runtime_binary_sha256') == binary
         and equivalence.get('all12_raw_and7_comparison_images_unchanged') is True,
         'Final source has not been proved identical to pictured settled poses')
    verify_hashes(equivalence.get('current_input_sha256', {}))
    need(set(equivalence.get('cases', {})) == {'civilian', 'armored', 'helmeted07'},
         'Pictured final pose wardrobe coverage incomplete')
    for case in equivalence['cases'].values():
        need(case.get('pose_json_bytes_identical') is True and case.get('max_component_difference') == 0
             and case.get('previous_pose_sha256') == case.get('final_pose_sha256'),
             'Final settled pose differs from rendered head profile')
    images = visual.get('images_sha256', {})
    for outfit in ('civilian', 'armored', 'helmeted07'):
        for state in ('before', 'after'):
            for view in ('side', 'head'):
                name = outfit + '-' + state + '-' + view + '.png'
                need(name in images, 'Head profile view missing ' + name)
    for name, digest in images.items():
        path = (folder / name).resolve()
        need(path.is_relative_to(folder.resolve()) and sha(path) == digest,
             'Head profile image changed or escaped its review folder')

def evidence(binary):
    head_evidence(binary)
    for name in NATIVE:
        result = read(N / 'review' / name / 'EXECUTION.json')
        need(result.get('passed') is True and result.get('binary_sha256') == binary
             and result.get('baseline_comparison') is False and result.get('exit_code') == 0
             and result.get('world', {}).get('gl_error') == 0 and not result.get('timed_out', False), 'Native review failed ' + name)
        need(result.get('scene_inputs_unchanged') is True, 'Native scene changed during review ' + name)
        verify_hashes(result.get('scene_input_sha256', {}))
        world, raw = result.get('world', {}), result.get('raw', {})
        if name == 'walking-stowed':
            q, w = world.get('walking_controls_qa', {}), world.get('walking_stowed_review', {})
            need(result.get('test') == 'walking-third' and result.get('stowed_motion_requested') is True
                 and q.get('passed') is True and q.get('injected_player_pose') is False
                 and w.get('requested') is True and w.get('boot_stowed') is True
                 and w.get('default_neutral_checked') is True and w.get('default_neutral_samples', 0) >= 6
                 and w.get('ordinary_draw_requested') is True and w.get('ordinary_draw_settled') is True
                 and w.get('ordinary_stow_requested') is True and w.get('full_idle_samples', 0) > 5
                 and w.get('full_moving_samples', 0) > 10, 'Default idle/draw/stow/walking regression incomplete')
        elif name == 'opening-natural':
            need(result.get('test') == 'opening' and raw.get('passed') is True and raw.get('natural_mode') is True
                 and raw.get('controller_skip') is False and raw.get('shot_mask') == 15
                 and raw.get('early_visible_body_verified') is True and raw.get('settled_camera_checked') is True
                 and raw.get('third_person') is True and raw.get('qa_player_pose_writes') == 0
                 and raw.get('qa_story_state_writes') == 0, 'Natural third-person opening incomplete')
        else:
            need(result.get('test') == 'arena' and raw.get('passed') is True and raw.get('actual_training_events') is True
                 and raw.get('actual_owned_geometry') is True and raw.get('events_injected') is False
                 and raw.get('reviewed_cases', 0) >= 2, 'Bounded arena clash regression incomplete')
    cpu = read(N / 'review/idle-grounding/CPU_REVIEW.json')
    need(cpu.get('schema') == 'valwood.idle-grounding-independent-cpu.v1'
         and cpu.get('passed') is True and cpu.get('gpu_used') is False
         and cpu.get('actual_controller_and_meshes') is True
         and cpu.get('outfit_loadout_cases') == 8, 'Grounded idle CPU regression failed')
    need(cpu.get('inputs_sha256', {}).get(str(N / 'source-v1/engine/world_skeletal_hero.cpp'))
         == sha(N / 'source-v1/engine/world_skeletal_hero.cpp'), 'Grounding regression used another source')
    verify_hashes(cpu['inputs_sha256'])
    comparison = cpu.get('comparison', {})
    need(comparison.get('maximum_armed_palette_component_difference') == 0
         and comparison.get('maximum_contact_query_difference') == 0
         and comparison.get('maximum_settled_moving_palette_component_difference') == 0,
         'Grounding regression changed armed, contact or settled moving behavior')
    stance = read(N / 'review/idle-grounding/EXISTING_STANCE_REGRESSION.json')
    result = stance.get('result', {})
    need(stance.get('schema') == 'valwood.existing-stance-regression-rerun.v1'
         and stance.get('passed') is True and stance.get('gpu_used') is False
         and stance.get('returncode') == 0
         and stance.get('source_sha256') == sha(N / 'source-v1/engine/world_skeletal_hero.cpp')
         and result.get('passed') is True and result.get('outfit_weapon_cases') == 8
         and result.get('frames', 0) >= 25472 and result.get('max_armed_pose_component_error') == 0,
         'Existing full-body stance regression incomplete')
    verify_hashes(stance.get('inputs_sha256', {}))
    final = read(N / 'review/FINAL_REVIEW.json')
    need(final.get('passed') is True and final.get('native_visual_passed') is True
         and final.get('binary_sha256', binary) == binary, 'Final posture visual review incomplete')
    need((N / 'review/PROGRESS_AFTER_CLOSE.json').is_file(), 'Post-close progress receipt missing')

def immutable_file(path):
    relative = path.relative_to(N)
    if ignored_cache(relative) or any(part in ('.logs', 'saves', 'settings') for part in relative.parts):
        return False
    if path.suffix in ('.o', '.log', '.logs', '.progress', '.save', '.exe'):
        return False
    if path.name in ('PUBLICATION.json', 'PROMOTION.json', 'PUBLISH_RECEIPT.json', 'POST_PUBLICATION.json'):
        return False
    if 'review' in relative.parts:
        with path.open('rb') as handle:
            if handle.read(4) == b'\x7fELF':
                return False
    return not ('review' in relative.parts and path.name == 'rpg')

def publish():
    need(not READY.exists(), 'Published generations are immutable')
    resources()
    verify_chain(BASE)
    binary, source, closure = exact_runtime()
    snapshots = unchanged_resources()
    evidence(binary)
    files = [N / p for p in ('runtime/rpg', 'runtime/ORIGIN_BUILD.json', 'runtime-native-v1/READY.json',
             'runtime-native-v1/BUILD_PLAN.json', 'launch.py', 'check_unified.py', 'GUIDE.txt', 'BASELINE.json',
             'SOURCE_ORIGIN.json', 'CACHE_SEED_REVIEW.json', 'build_native_candidate.py', 'seed_native_cache.py',
             'run_native_review.py', 'review/PROGRESS_AFTER_CLOSE.json')]
    files += [BASE / 'UNIFIED_READY.json', *closure, *snapshots]
    files += [N / 'source-v1' / p for p in source]
    for folder in ('tools', 'shared/shaders', 'review'):
        files += [p for p in (N / folder).rglob('*') if p.is_file() and immutable_file(p)]
    files = sorted(set(files))
    need(all(p.is_file() for p in files), 'Publication input missing')
    need(not any('saves' in p.parts or p.resolve().is_relative_to(E / 'projects/valwood_current') for p in files), 'Mutable live authoring/save path in immutable pins')
    pins = {str(p): sha(p) for p in files}
    stats = {str(p): stamp(p) for p in files}
    need(all(stamp(p) == stats[str(p)] for p in files), 'Publication input changed while freezing')
    temporary = READY.with_suffix('.json.tmp')
    temporary.write_text(json.dumps(dict(ready=True, one_binary=binary, pins=pins, stats=stats, baseline=str(BASE),
                         baseline_ready_sha256=sha(BASE / 'UNIFIED_READY.json'), native_only=True,
                         application_units=104, verified_cached_units=103, compiled_units=sorted(RECOMPILED),
                         full_application_dependency_closure_verified=True, Windows_work_performed=False,
                         mutable_authoring_and_saves_not_frozen=True), indent=2) + '\n')
    os.replace(temporary, READY)

def check():
    document = read(READY)
    need(document['ready'], 'Not ready')
    resources()
    unchanged_resources()
    verify_pins(document)
    verify_chain(BASE)
    current = read(E.parent / 'CURRENT_VALWOOD.json')
    need(Path(current['runtime_generation']) == N and current['binary_sha256'] == document['one_binary'], 'Current pointer differs')

def cli():
    parser = argparse.ArgumentParser()
    parser.add_argument('--publish', action='store_true')
    parser.add_argument('--check-only', action='store_true')
    args = parser.parse_args()
    try:
        publish() if args.publish else check()
        print('Grounded father idle and exact 104-unit native closure verified.')
    except Exception as error:
        print(str(error), file=sys.stderr)
        sys.exit(1)

if __name__ == '__main__':
    cli()

