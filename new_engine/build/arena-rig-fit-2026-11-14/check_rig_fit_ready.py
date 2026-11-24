#!/usr/bin/env python3
"""Verify the bounded rig-fit revision and its immutable arena baseline."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
from datetime import datetime, timezone

sys.dont_write_bytecode = True
BASE = Path(__file__).resolve().parent
ENGINE = BASE.parents[1]
WORKSPACE = ENGINE.parent
ANCESTOR = BASE.parent / 'arena-body-motion-2026-11-14'
SOURCE = BASE / 'source-v1'
BINARY = BASE / 'runtime-native-v1/native-bin/rpg'
PUBLIC = WORKSPACE / 'Launch_Valwood_Arena_Rig_Fit_Test.sh'
READY = BASE / 'RIG_FIT_READY.json'
SCHEMA = 'valwood.arena-rig-fit.ready.v1'
BASELINE_LAUNCHER = WORKSPACE / 'Launch_Valwood_Arena_Body_Motion_Test.sh'


def require(value, message):
    if not value:
        raise RuntimeError(message)


class Closure:
    def __init__(self):
        self.values = {}
        self.resolved = {}
        self.cache = {}

    def pin(self, name, expected=None):
        path = Path(name).absolute()
        require(path.is_file(), 'Missing input: ' + str(path))
        target = str(path.resolve(strict=True))
        stat = path.stat()
        stamp = (stat.st_dev, stat.st_ino, stat.st_size, stat.st_mtime_ns, stat.st_ctime_ns)
        key = (target, stamp)
        actual = self.cache.get(key)
        if actual is None:
            with path.open('rb') as stream:
                actual = hashlib.file_digest(stream, 'sha256').hexdigest()
            after = path.stat()
            require(stamp == (after.st_dev, after.st_ino, after.st_size, after.st_mtime_ns, after.st_ctime_ns) and
                    target == str(path.resolve(strict=True)), 'Input changed during verification: ' + str(path))
            self.cache[key] = actual
        require(expected is None or actual == expected, 'Changed input: ' + str(path))
        require(str(path) not in self.values or self.values[str(path)] == actual,
                'Conflicting input: ' + str(path))
        self.values[str(path)] = actual
        self.resolved[str(path)] = target
        return actual

    def hashes(self, rows):
        require(isinstance(rows, dict) and rows, 'Missing hash map')
        for path, digest in rows.items():
            require(Path(path).is_absolute(), 'Relative dependency: ' + path)
            self.pin(path, digest)

    def proof(self, name, field):
        path = BASE / name
        self.pin(path)
        data = json.loads(path.read_text())
        require(data.get(field) is True, 'Failed proof: ' + str(path))
        for key in ('dependencies_sha256', 'source_sha256', 'before_sha256', 'after_sha256', 'output_sha256', 'outputs_sha256', 'reports_sha256', 'sha256'):
            rows = data.get(key)
            if isinstance(rows, dict) and rows:
                if key == 'source_sha256' and not all(Path(path).is_absolute() for path in rows):
                    require(name == 'runtime-native-v1/READY.json', 'Unbound relative source digest map')
                    continue  # Native source inventory is bound explicitly to SOURCE below.
                try:
                    self.hashes(rows)
                except RuntimeError as error:
                    raise RuntimeError(str(path) + ': ' + str(error)) from error
        for key in ('log', 'capture', 'report', 'binary'):
            if key in data and key + '_sha256' in data:
                self.pin(data[key], data[key + '_sha256'])
        return data


def bootstrap():
    mutable = {'graphics.settings', 'player-profile.config', 'studio-layout.ini', 'world.studio.workspace'}
    files = {}
    for dirname, dirs, names in os.walk(BASE / 'arena-project', followlinks=False):
        here = Path(dirname)
        dirs[:] = [name for name in dirs if name not in {'saves', 'profiles', 'models'} and not (here / name).is_symlink()]
        for name in names:
            if name in mutable or name.endswith(('.progress', '.tmp', '.workspace')):
                continue
            path = here / name
            if path.is_file():
                files[str(path.relative_to(BASE / 'arena-project'))] = path
    return files


def baseline(c):
    origin = json.loads((BASE / 'SOURCE_ORIGIN.json').read_text())
    require(Path(origin['baseline']) == ANCESTOR / 'source-v1', 'Unexpected source ancestor')
    c.pin(BASE / 'SOURCE_ORIGIN.json')
    c.pin(ANCESTOR / 'BODY_MOTION_READY.json', origin['baseline_ready_sha256'])
    c.pin(BASELINE_LAUNCHER)
    result = subprocess.run([str(BASELINE_LAUNCHER), '--check-only'], capture_output=True, text=True, timeout=180)
    require(result.returncode == 0, 'Accepted baseline no longer verifies: ' + result.stderr.strip())
    require((BASE / 'arena-project/models').resolve(strict=True) == (ANCESTOR / 'pose-cook/bank/models').resolve(strict=True),
            'Rig-fit revision selected another model bank')
    changed = {'engine/world_skeletal_hero.h', 'engine/world_skeletal_hero.cpp',
               'engine/world_viewer.cpp', 'engine/world_training_combat_view.inc',
               'engine/world_combat_timing.h', 'engine/world_training_combat.cpp', 'engine/world_character_terminal.h'}
    added = {'engine/world_skeletal_hero_stationary_cut.inc',
             'engine/world_skeletal_hero_loadouts.h', 'engine/world_skeletal_hero_loadouts.cpp',
             'engine/world_skeletal_hero_viewmodel.h', 'engine/world_skeletal_hero_sleeves.h'}
    current = {str(p.relative_to(SOURCE)): p for p in SOURCE.rglob('*') if p.is_file()}
    original = {str(p.relative_to(ANCESTOR / 'source-v1')): p
                for p in (ANCESTOR / 'source-v1').rglob('*') if p.is_file()}
    require(set(current) == set(original) | added, 'Unexpected rig-fit source inventory')
    for name, path in original.items():
        digest = c.pin(path)
        if name not in changed:
            c.pin(current[name], digest)
    for path in (BASE / 'arena-project').rglob('*'):
        if path.is_symlink():
            previous = ANCESTOR / 'arena-project' / path.relative_to(BASE / 'arena-project')
            require(previous.is_symlink() and path.resolve(strict=True) == previous.resolve(strict=True),
                    'Private shared resource changed: ' + str(path))


def refresh(publish=True):
    c = Closure()
    baseline(c)
    native = c.proof('runtime-native-v1/READY.json', 'ready')
    require(native['native_only'] is True and native['Windows_work_performed'] is False and
            native['application_units'] == 103 and native['full_application_closure_rebuilt'] is True,
            'Incomplete native closure')
    require(Path(native['source']) == SOURCE and Path(native['binary']) == BINARY, 'Wrong native candidate')
    paths = {str(p.relative_to(SOURCE)): p for p in SOURCE.rglob('*') if p.is_file()}
    require(set(paths) == set(native['source_sha256']), 'Changed source inventory')
    for name, digest in native['source_sha256'].items():
        c.pin(paths[name], digest)
    c.pin(BINARY, native['sha256'])
    plan = json.loads((BASE / 'runtime-native-v1/BUILD_PLAN.json').read_text())
    c.pin(BASE / 'runtime-native-v1/BUILD_PLAN.json')
    require(len(plan['units']) == 103 and not plan['skip_player'], 'Partial native plan')
    c.pin(ENGINE / 'build/physics/libJolt.a', plan['jolt_library_sha256'])
    for unit in plan['units']:
        path = Path(unit['target']).with_suffix('.json')
        c.pin(path)
        receipt = json.loads(path.read_text())
        require(receipt['command'] == unit['command'], 'Stale native command')
        c.hashes(receipt['actual_dependency_sha256'])
    for report, inputs in [('hero-animation-polish-check.json', 'ANIMATION_TEST_INPUTS.json'),
                           ('COMBAT_SEAMS.json', 'COMBAT_SEAMS_INPUTS.json')]:
        c.proof(report, 'accepted')
        path = BASE / inputs
        c.pin(path)
        data = json.loads(path.read_text())
        require(data['returncode'] == 0 and data['source_unchanged'] and data['before_sha256'] == data['after_sha256'],
                'CPU regression used changing source')
        c.hashes(data['before_sha256'])
        c.pin(data['report'], data['report_sha256'])
        c.pin(data['execution_command'][0], data['test_executable_sha256'])
    timing = c.proof('timing-review/COMBAT_TIMING_REVIEW.json', 'accepted')
    require(timing['passed'] and timing['production_timing']['checks'] >= 38801 and
            timing['first_person_selector']['checks'] >= 67916, 'Shared timing regression failed')
    queued = c.proof('early-queue-review/EARLY_QUEUE_REVIEW.json', 'passed')
    require(queued['early_queue_cases'] == 400 and queued['triple_combo_cases'] == 400 and
            queued['cancellation_stamina_serial_and_cap_cases'] == 80 and queued['sword_bitwise_trace_unchanged'] and
            queued['maximum_queued_lifetime_s'] <= 1.5, 'Heavy weapon queue regression failed')
    flow = c.proof('attack-flow-review/ATTACK_FLOW_CPU.json', 'accepted')
    require(flow['passed'] and flow['all_25_pairs_each_rate'] and flow['rates_hz'] == [30, 60, 120] and
            flow['production_pairs'] == 75 and flow['fresh_pre_render_queries'] >= 675 and
            flow['current_queries'] >= 675 and flow['pure_query_pose_matrix_delta'] == 0 and
            flow['pre_render_blade_error_m'] < .00001, 'Production attack/contact synchronization is incomplete')
    for row in flow['speeds']:
        require(row['new_peak_mps'] < row['old_peak_mps'] if row['direction'] in (0, 1, 4)
                else row['new_peak_mps'] == row['old_peak_mps'], 'Attack speed comparison failed')
    motion = c.proof('player-motion-review/PLAYER_BODY_MOTION_CPU.json', 'accepted')
    require(motion['passed'] and motion['all25_pairs'] and motion['stationary_and_walking'] and
            motion['cases'] == 150 and motion['rates_hz'] == [30, 60, 120] and
            motion['pure_current_parities'] >= 1200 and motion['arm_reach_solves'] >= 6150 and
            motion['maximum_contact_pose_matrix_error'] == 0 and motion['pure_query_mutation'] == 0 and
            motion['maximum_predicted_rendered_blade_error_m'] < .00001 and
            motion['maximum_segment_length_error_m'] < .00001,
            'Physical commit, momentum, reach or contact parity failed')
    for row in motion['render_link_comparison']:
        require(row['pairs'] == 25 and row['candidate']['zero_wrist_link_frames'] == 0 and
                row['candidate']['mean_wrist_vector_gap_mps'] < row['accepted']['mean_wrist_vector_gap_mps'],
                'Published combo link motion did not improve')
    feet = c.proof('stationary-footwork-review/STATIONARY_FOOTWORK_REVIEW.json', 'accepted')
    checked = feet['candidate']
    require(feet['passed'] and checked['cases'] >= 165 and checked['checks'] >= 74687 and
            checked['lift_released'] == checked['lift_frames'] and checked['lift_frames'] >= 1200 and
            checked['anchor_error_m'] < .0002 and checked['flat_penetration_m'] < .0005 and
            checked['slope_penetration_m'] < .0005 and checked['blade_error_m'] < .00001 and
            feet['maximum_protected_matrix_difference'] < .00001 and checked['segment_error_m'] < .00001,
            'Actual planted-foot, lift, protected torso or uneven-ground review failed')
    require(len(feet['trajectory']) == 6 and all(
        row['candidate']['heel_planar_path_m'] < row['baseline']['heel_planar_path_m'] * .2 and
        row['candidate']['maximum_heel_span_m'] < .16 for row in feet['trajectory']),
        'Stationary Left/Low step reduction failed')
    optimized = c.proof('footwork-optimization-review/FOOTWORK_OPTIMIZATION_REVIEW.json', 'accepted')
    require(optimized['complete_pose_matrices_bit_identical'] and optimized['boot_control_trace_byte_identical'] and
            optimized['complete_global_and_skin_matrix_float_values'] == 68567040 and
            optimized['maximum_complete_matrix_difference'] == 0, 'Footwork cost optimization changed tested poses')
    gpu = c.proof('stationary-footwork-review/FOOTWORK_GPU_REVIEW.json', 'passed')
    require(gpu['schema'] == 'valwood.stationary-cut-footwork.scripted-gpu.v1' and
            gpu['tested_frames'] == 384 and len(gpu['scenarios']) == 4 and
            all(row['passed'] for row in gpu['scenarios']) and gpu['visual_review']['accepted'],
            'Current Left/Low render review failed')
    c.hashes(gpu['visual_review']['viewed_outputs_sha256'])
    geometry = c.proof('attack-geometry-review/CONTACT_CHECK.json', 'passed')
    require(geometry['schema'] == 'valwood.purchased-knight-attack-flow-contact-check.v1' and
            all(geometry.get(key) is True for key in ('actual_new_anatomy_used', 'actual_committed_query_timings',
                'pure_pre_render_explicit_duration_queries', 'pre_render_queries_preserve_pose')) and
            geometry['body_and_steel_checks'] == 320 and geometry['normal']['passed'] and geometry['normal']['checks'] == 160 and
            geometry['combo']['passed'] and geometry['combo']['checks'] == 160 and
            geometry['before_sha256'] == geometry['after_sha256'] and geometry['guard_endpoint_samples'] == 120 and
            geometry['guard_endpoint_max_error_m'] < .00001, 'Actual purchased-model contact acceptance is incomplete')
    c.pin(BASE / 'attack-geometry-review/contact-review/contact-fixture', geometry['binary_sha256'])
    selection = json.loads((BASE / 'HERO_ASSET.json').read_text())
    c.pin(BASE / 'HERO_ASSET.json')
    c.pin(selection['path'], selection['sha256'])
    armor = c.proof('shoulder-fit-review/SHOULDER_FIT_READY.json', 'ready')
    require(armor['head_clipping_resolved_in_dense_samples'] and armor['candidate'] == selection['path'] and
            armor['candidate_sha256'] == selection['sha256'], 'Selected shoulder derivative changed')
    equivalent = c.proof('shoulder-fit-review/DERIVATIVE_EQUIVALENCE.json', 'passed')
    require(equivalent['changed_primitive_indices'] == [6] and equivalent['original_binary_prefix_byte_exact'] and
            equivalent['non_pauldron_primitives_unchanged'] and equivalent['wrist_and_blade_streams_unchanged'],
            'Shoulder derivative altered unrelated anatomy or animation')
    c.proof('shoulder-fit-review/VISUAL_REVIEW.json', 'accepted')
    for name, prefix, weapon in [('sword', '', 0), ('shield', 'Shield_', 2), ('axe', 'Axe_', 3), ('staff', 'Staff_', 4)]:
        clearance = c.proof('shoulder-fit-review/current-source/' + name + '/CLEARANCE.json', 'passed')
        require(clearance['samples'] >= 8560 and clearance['actual_controller_prefix'] == prefix and
                clearance['actual_weapon_enum'] == weapon and clearance['head_materials_tested'] == [15,16,17,18] and
                all(row['intersecting_samples'] == 0 for row in clearance['rows']),
                'Current loadout shoulder/head clearance failed: ' + name)
    staff_fit = c.proof('loadout-rigs/STAFF_PAULDRON_FIT.json', 'passed')
    require(staff_fit['changed_primitive'] == 6 and staff_fit['changed_attribute'] == 'POSITION' and
            staff_fit['side'] == 'left' and staff_fit['maximum_displacement_m'] < .002 and
            staff_fit['unchanged_animations'] and staff_fit['unchanged_skeleton'] and
            staff_fit['unchanged_all_other_primitive_streams'] and
            not any(staff_fit['after_intersections'].values()),
            'Staff shoulder fit changed unrelated streams or still crosses the head')
    c.proof('terminal-review/TERMINAL_LOADOUTS.json', 'accepted')
    for name in ('shield', 'axe', 'staff'):
        actual = c.proof('loadout-rigs/geometry/' + name + '/CONTACT_CHECK.json', 'passed')
        require(actual['actual_production_committed_callbacks'] and actual['body_callbacks'] == 10 and
                actual['pure_pre_render_queries'], 'Actual opponent contact failed: ' + name)
    # Independently generated actual controller/render and switch proofs.
    loadouts = c.proof('loadout-rigs/LOADOUT_ACCEPTANCE.json', 'accepted')
    switches = c.proof('loadout-rigs/LOADOUT_TRANSITIONS.json', 'accepted')
    require(loadouts['passed'] and switches['passed'], 'Weapon rig acceptance failed')
    rig_sources = json.loads((BASE / 'loadout-rigs/LOADOUT_RIGS.json').read_text())
    c.pin(BASE / 'loadout-rigs/LOADOUT_RIGS.json')
    require(len(rig_sources['rigs']) == 3, 'Incomplete additional melee rigs')
    for rig in rig_sources['rigs']:
        c.pin(rig['path'], rig['sha256'])
        c.hashes(rig['source_sha256'])
        require(rig['segment_length_error_m'] < .00001 and rig['ik_wrist_error_m'] < .00001,
                'Retarget changed arm lengths or failed its target')
    require(switches['schema'] == 'valwood.skeletal-loadouts.transitions-cpu.v1', 'Unexpected switch fixture')
    c.pin(BASE / 'loadout-rigs/loadouts.manifest')
    c.pin(BASE / 'shaders/rig_first_person.fs.glsl')
    first_person = c.proof('first-person-review/FIRST_PERSON_PRESENTATION.json', 'accepted')
    require(first_person['schema'] == 'valwood.first-person-presentation-contract.v1' and
            first_person['visual_review']['accepted'],
            'First-person mesh framing and material review is incomplete')
    pitch = c.proof('first-person-review/PITCH_ANCHOR_PROOF.json', 'passed')
    require(pitch['samples'] >= 15488 and pitch['pitch_range_degrees'] == [-60, 60] and
            pitch['all_anchor_ends_behind_eye'] and pitch['original_vertex_branch_unchanged'] and
            pitch['max_camera_space_error_m'] < .00001 and pitch['maximum_clip_w'] < 0,
            'First-person sleeve ends enter the view while looking up or down')
    original = c.proof('first-person-review/ORIGINAL_STREAMS.json', 'passed')
    require(len(original['assets']) == 4 and all(
        row['original_vertex_attributes_and_triangles_exact'] and
        row['skeleton_inverse_binds_all_animation_tracks_exact'] and
        row['original_images_materials_exact'] and row['added_triangles'] == 114 and
        row['max_rim_inset_m'] < .002 and row['reserved_uv_negative_case_rejected'] and
        row['unsupported_topology_negative_case_rejected'] for row in original['assets']),
        'First-person sleeve additions changed the original rig or weapon geometry')
    cost = c.proof('CPU_PERFORMANCE_REVIEW.json', 'accepted')
    require(cost['maximum_candidate_p95_us'] < 1000 and cost['transition_p99_us'] < 5000,
            'Single hero pose pass exceeds its CPU budget')
    other_cost = c.proof('loadout-rigs/LOADOUT_CPU_PERFORMANCE.json', 'passed')
    require(other_cost['schema'] == 'valwood.purchased-loadout-cpu-performance.v1' and
            other_cost['weapons'] == 3 and other_cost['scenarios_per_weapon'] == 10 and
            other_cost['measured_frames'] == 36000 and other_cost['target_met'] and
            other_cost['maximum_p95_us'] < 1000,
            'Additional melee rigs exceed their CPU budget')
    smoke = c.proof('rig-fit-review/WORLD_SMOKE_ACCEPTED.json', 'accepted')
    require(smoke['binary_sha256'] == native['sha256'] and smoke['hero_failures'] == 0 and
            smoke['gl_error'] == 0 and smoke['variants']['available_mask'] == 15 and smoke['updates'] >= 24 and smoke['loadouts'] == 4 and smoke['first_person']['draws'] >= 12 and smoke['first_person']['failures'] == 0,
            'Native integration proof used another build or failed')
    c.pin(smoke['binary'], smoke['binary_sha256'])
    c.pin(smoke['asset'], smoke['asset_sha256'])
    for path in bootstrap().values():
        c.pin(path)
    for path in [BASE / 'launch_knight_candidate.sh', PUBLIC, Path(__file__)]:
        c.pin(path)
    # Re-read each dependency to reject a report or source changing mid-release.
    for path, digest in list(c.values.items()):
        c.pin(path, digest)
    if not publish:
        print('Rig-fit audit passed: ' + str(len(c.values)) + ' verified dependencies; no release published.')
        return
    data = dict(schema=SCHEMA, accepted=True, accepted_utc=datetime.now(timezone.utc).isoformat(),
                binary=str(BINARY), binary_sha256=native['sha256'], dependencies_sha256=c.values,
                resolved_paths=c.resolved, bootstrap_inventory=sorted(bootstrap()),
                baseline_launcher=str(BASELINE_LAUNCHER), windows_work_performed=False,
                limits=['Private Linux arena test with fitted Sword, sword/shield, axe and staff skeletal hero rigs.',
                        'First-person arms use an explicit shared presentation transform; physical contacts remain world-space.',
                        'First-person undersleeves close exposed cuffs; their broad, simple shape remains prototype artwork.',
                        'Hero shoulder fit and stationary Left/Low footwork are finite tested improvements; NPC armor and the existing bow rig remain separate.',
                        'Scripted CPU and offscreen software rendering; interactive controller feel and hardware frame pacing require playtesting.'])
    temporary = READY.with_suffix('.json.tmp')
    temporary.write_text(json.dumps(data, indent=2) + '\n')
    temporary.replace(READY)
    print('Rig-fit build ready: ' + str(len(c.values)) + ' verified dependencies.')


def check():
    require(READY.is_file(), 'Rig-fit build is still being checked')
    data = json.loads(READY.read_text())
    require(data['schema'] == SCHEMA and data['accepted'] is True and not data['windows_work_performed'], 'Not accepted')
    c = Closure()
    c.hashes(data['dependencies_sha256'])
    require(c.resolved == data['resolved_paths'] and data['bootstrap_inventory'] == sorted(bootstrap()), 'Changed release bindings')
    baseline(c)
    require(data['binary'] == str(BINARY) and c.values[str(BINARY)] == data['binary_sha256'], 'Wrong binary')
    print('Rig-fit arena preflight passed.')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument('--refresh', action='store_true')
    mode.add_argument('--audit', action='store_true')
    mode.add_argument('--check-only', action='store_true')
    args = parser.parse_args()
    try:
        refresh(publish=not args.audit) if args.refresh or args.audit else check()
    except (OSError, ValueError, KeyError, RuntimeError, subprocess.SubprocessError) as error:
        print('Rig-fit arena preflight: ' + str(error), file=sys.stderr)
        sys.exit(1)
