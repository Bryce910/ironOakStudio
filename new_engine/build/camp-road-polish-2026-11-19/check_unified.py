"""Verify the camp/road presentation generation and its native build closure."""
from pathlib import Path
import argparse
import hashlib
import json
import os
import sys

N = Path(__file__).resolve().parent
E = N.parents[1]
BASE = N.with_name('apartment-collision-2026-11-18')
BUILD_BASE = N.with_name('opening-stance-2026-11-18')
READY = N / 'UNIFIED_READY.json'
NATIVE = ('opening-natural', 'journey-final', 'npc-intro-bounded', 'arena-regression')
RECOMPILED = {'engine/world_trail_camp.cpp', 'engine/world_viewer.cpp', 'engine/world_road_traffic.cpp'}

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
    from launch import configuration, AREAS
    need((N / 'motion').resolve() == (BASE / 'motion').resolve(), 'Character motion bank alias changed')
    for bank in ('story_hero', 'plain-father', 'walkers', 'apartment-aligned'):
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
    origin = read(N / 'runtime/ORIGIN_BUILD.json')
    built = read(N / 'runtime-native-v1/READY.json')
    need(origin == built and origin.get('ready') is True and origin.get('application_units') == 104
         and origin.get('sha256') == binary and binary != baseline['binary_sha256']
         and origin.get('full_application_dependency_closure_verified') is True
         and origin.get('native_only') is True and origin.get('Windows_work_performed') is False,
         'New 104-unit native build incomplete')
    need(sha(N / 'runtime-native-v1/native-bin/rpg') == binary, 'Published executable differs from native output')
    need(Path(origin['source']) == N / 'source-v1' and not (N / 'source-v1').is_symlink(), 'Build escaped candidate source')
    source = {p: h for p, h in origin['source_sha256'].items() if not ignored_cache(p)}
    need(inventory(N / 'source-v1') == source, 'Frozen source inventory differs from compiled source')
    source_origin = read(N / 'SOURCE_ORIGIN.json')
    need(Path(source_origin['source']) == N / 'source-v1' and Path(source_origin['baseline']) == BASE / 'source-v1'
         and source_origin.get('canonical_source_edited') is False, 'Source origin is not the protected apartment baseline')
    expected_base = {p: h for p, h in source_origin['baseline_sha256'].items() if not ignored_cache(p)}
    need(inventory(BASE / 'source-v1') == expected_base, 'Source baseline inventory changed')
    plan = read(N / 'runtime-native-v1/BUILD_PLAN.json')
    need(plan['source_sha256'] == origin['source_sha256'] and plan.get('skip_player') is False
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
    need(seed.get('copied_verified_objects') == 101 and set(seed.get('recompile', [])) == RECOMPILED
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

def unchanged_resources():
    need(not (N / 'world-project/opening/home.physics').is_symlink(), 'Physics document must be an owned regular file for O_NOFOLLOW loading')
    need(not (N / 'shared/shaders').is_symlink() and inventory(N / 'shared/shaders') == inventory(BASE / 'shared/shaders'), 'Owned shader bytes differ from accepted apartment generation')
    need(inventory(N / 'motion') == inventory(BASE / 'motion'), 'Owned character motion changed')
    need(inventory(N / 'world-project/opening') == inventory(BASE / 'world-project/opening'), 'Apartment/story settings changed during scenery polish')
    for relative in ('opening.config', 'opening.scene', 'scenery/authored_instances.txt', 'scenery/bep_environment_instances.txt'):
        need(sha(N / 'world-project' / relative) == sha(BASE / 'world-project' / relative), 'Protected apartment/landscape data changed ' + relative)

def evidence(binary):
    for name in NATIVE:
        result = read(N / 'review' / name / 'EXECUTION.json')
        need(result.get('passed') is True and result.get('binary_sha256') == binary
             and result.get('baseline_comparison') is False and result.get('exit_code') == 0
             and result.get('world', {}).get('gl_error') == 0 and not result.get('timed_out', False), 'Native review failed ' + name)
        need(result.get('scene_inputs_unchanged') is True, 'Native scene changed during review ' + name)
        verify_hashes(result.get('scene_input_sha256', {}))
        raw, world = result.get('raw', {}), result.get('world', {})
        if name == 'opening-natural':
            need(result.get('test') == 'opening' and raw.get('passed') is True and raw.get('natural_mode') is True
                 and raw.get('controller_skip') is False and raw.get('shot_mask') == 15
                 and raw.get('early_visible_body_verified') is True and raw.get('settled_camera_checked') is True
                 and raw.get('third_person') is True and raw.get('qa_player_pose_writes') == 0
                 and raw.get('qa_story_state_writes') == 0, 'Natural third-person opening incomplete')
        elif name == 'journey-final':
            qa = world.get('first_journey_qa', {})
            need(result.get('test') == 'journey' and qa.get('passed') is True and qa.get('walked_m', 0) > 700
                 and qa.get('retries', 0) >= 1 and qa.get('restores', 0) >= 1 and qa.get('save_verified') is True
                 and qa.get('restore_verified') is True and qa.get('qa_player_pose_writes') == 0
                 and qa.get('qa_story_state_writes') == 0 and qa.get('qa_health_writes') == 0
                 and world.get('first_journey', {}).get('complete') is True, 'Connected ordinary-input chapter incomplete')
        elif name == 'npc-intro-bounded':
            need(result.get('test') == 'npc-intro' and raw.get('passed') is True and raw.get('intro_starts') == 1
                 and raw.get('intro_completions') == 1 and raw.get('visual_preview_shot_mask') == 31
                 and raw.get('actual_skipped') is True and raw.get('actual_natural_native_completion') is False
                 and raw.get('live_state_injected') is False and raw.get('preview_does_not_advance_state') is True,
                 'Bounded NPC cinematic/dialogue controls incomplete')
        else:
            need(result.get('test') == 'arena' and raw.get('passed') is True and raw.get('actual_training_events') is True
                 and raw.get('actual_owned_geometry') is True and raw.get('events_injected') is False
                 and raw.get('reviewed_cases', 0) >= 2, 'Existing arena clash regression incomplete')
    comparison = N / 'review/baseline-journey/EXECUTION.json'
    if comparison.is_file():
        c = read(comparison)
        need(c.get('baseline_comparison') is True and c.get('binary_sha256') == sha(BASE / 'runtime/rpg'), 'Baseline comparison mislabeled as current acceptance')
    npc = read(N / 'review/npc-presentation/NPC_PRESENTATION_CPU_REVIEW.json')
    need(npc.get('passed') is True and npc.get('trail_camp_syntax_passed') is True, 'NPC presentation CPU review failed')
    for name in ('source_sha256', 'test_sha256', 'asset_sha256', 'setting_sha256'):
        verify_hashes(npc[name])
    need(sha(npc['raw_receipt']) == npc['raw_sha256'], 'NPC CPU raw receipt changed')
    budget = read(N / 'review/route-budget/CPU_BUDGET_REVIEW.json')
    need(budget.get('passed') is True and budget.get('gpu_used') is False and budget.get('project_or_live_modified') is False
         and budget.get('changed_instances') == 608 and budget.get('new_detail_instances_reviewed') == 607 and budget.get('supported_roast_placement', {}).get('passed') is True, 'Independent route/geometry budget review failed')
    verify_hashes(budget['inputs_sha256'])
    fire = read(N / 'review/fire/FIRE_ASSETS_REVIEW.json')
    need(fire.get('passed') is True and fire.get('shader_unchanged') is True
         and fire.get('geometry_collision_and_texture_payloads_unchanged') is True and len(fire['models']) == 4,
         'Campfire asset review incomplete')
    files = []
    for row in fire['models']:
        asset, alias = Path(row['asset']), N / 'world-project' / row['project_alias']
        need(asset.is_relative_to(N / 'assets/camp-road/fire') and asset.is_file()
             and sha(asset) == row['after_sha256'] and sha(alias) == row['after_sha256']
             and alias.resolve() == asset.resolve() and row.get('binary_geometry_and_images_exact') is True
             and sha(row['source']) == row['source_sha256'], 'Owned campfire asset/alias changed')
        files.extend((asset, alias))
        if str(row['project_alias']).endswith('_near.glb'):
            primary = N / 'world-project' / str(row['project_alias']).replace('_near.glb', '.glb')
            need(primary.resolve() == asset.resolve() and sha(primary) == row['after_sha256'], 'Primary campfire alias changed')
            files.append(primary)
    need(sha(N / 'world-project/showcase.flames') == fire['flame_change']['after_sha256'], 'Campfire flame settings changed')
    road = read(N / 'review/ROAD_CACHE_GPU_REVIEW.json')
    final = read(N / 'review/FINAL_REVIEW.json')
    need(road.get('passed') is True and road.get('binary_sha256', binary) == binary, 'Road cache GPU review incomplete')
    need(final.get('passed') is True and final.get('native_visual_passed') is True
         and final.get('binary_sha256', binary) == binary, 'Final native visual review incomplete')
    need((N / 'review/PROGRESS_AFTER_CLOSE.json').is_file(), 'Post-close progress receipt missing')
    return files

def immutable_file(path):
    relative = path.relative_to(N)
    if ignored_cache(relative) or any(part in ('.logs', 'saves', 'settings') for part in relative.parts):
        return False
    if path.suffix in ('.o', '.log', '.logs', '.progress', '.save', '.exe'):
        return False
    if path.name in ('PUBLICATION.json', 'PROMOTION.json', 'PUBLISH_RECEIPT.json', 'POST_PUBLICATION.json'):
        return False
    # Keep CPU fixtures built here; external game executables are comparison
    # inputs only, never part of this generation's frozen review artifacts.
    if 'review' in relative.parts and (path.name == 'rpg' or (path.is_symlink() and path.resolve().is_relative_to(BASE / 'runtime'))):
        return False
    return True

def publish():
    need(not READY.exists(), 'Published generations are immutable')
    resources()
    verify_chain(BASE)
    binary, source, closure = exact_runtime()
    unchanged_resources()
    dependencies = evidence(binary)
    files = [N / p for p in ('runtime/rpg', 'runtime/ORIGIN_BUILD.json', 'runtime-native-v1/READY.json',
                             'runtime-native-v1/BUILD_PLAN.json', 'launch.py', 'check_unified.py', 'GUIDE.txt',
                             'BASELINE.json', 'SOURCE_ORIGIN.json', 'CACHE_SEED_REVIEW.json',
                             'build_native_candidate.py', 'seed_native_cache.py', 'run_native_review.py',
                             'review/PROGRESS_AFTER_CLOSE.json')]
    files += [BASE / 'UNIFIED_READY.json', *closure, *dependencies]
    files += [N / 'source-v1' / relative for relative in source]
    files += [N / 'world-project' / relative for relative in ('world.studio', 'scenery/demo_polish_instances.txt',
               'scenery/bep_environment_instances.txt', 'scenery/authored_instances.txt', 'scenery/starting_area_instances.txt', 'showcase.flames', 'opening.config', 'opening.scene')]
    for folder in ('tools', 'shared/shaders', 'assets/camp-road', 'world-project/opening', 'review'):
        files += [p for p in (N / folder).rglob('*') if p.is_file() and immutable_file(p)]
    files = sorted(set(files))
    need(all(p.is_file() for p in files), 'Publication input missing')
    live = (E / 'projects/valwood_current').resolve()
    need(not any('saves' in p.parts or p.resolve().is_relative_to(live) for p in files), 'Mutable live authoring/save in publication pins')
    pins = {str(p): sha(p) for p in files}
    stats = {str(p): stamp(p) for p in files}
    need(all(stamp(p) == stats[str(p)] for p in files), 'Publication inputs changed while freezing')
    temporary = READY.with_suffix('.json.tmp')
    temporary.write_text(json.dumps(dict(ready=True, one_binary=binary, pins=pins, stats=stats, baseline=str(BASE),
                         baseline_ready_sha256=sha(BASE / 'UNIFIED_READY.json'), native_only=True,
                         application_units=104, verified_cached_units=101, compiled_units=sorted(RECOMPILED),
                         full_application_dependency_closure_verified=True, Windows_work_performed=False,
                         mutable_authoring_and_saves_not_frozen=True), indent=2) + '\n')
    os.replace(temporary, READY)

def check():
    document = read(READY)
    need(document['ready'], 'Not ready')
    resources()
    verify_pins(document)
    verify_chain(BASE)
    current = read(E.parent / 'CURRENT_VALWOOD.json')
    need(Path(current['runtime_generation']) == N and current['binary_sha256'] == document['one_binary'], 'Current pointer differs')

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--publish', action='store_true')
    parser.add_argument('--check-only', action='store_true')
    args = parser.parse_args()
    try:
        publish() if args.publish else check()
        print('Camp/road presentation generation and 104-unit native closure verified.')
    except Exception as error:
        print(str(error), file=sys.stderr)
        sys.exit(1)
