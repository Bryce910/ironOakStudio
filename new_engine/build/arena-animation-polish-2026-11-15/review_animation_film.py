#!/usr/bin/env python3
"""Frame-complete film measurements; spikes are diagnostics, not automatic AAA claims."""
import argparse
import hashlib
import json
import math
from pathlib import Path

import numpy as np

N = Path(__file__).resolve().parent


def sha(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def require(value, message):
    if not value:
        raise RuntimeError(message)


def finite(value):
    if isinstance(value, dict):
        return all(finite(child) for child in value.values())
    if isinstance(value, list):
        return all(finite(child) for child in value)
    return not isinstance(value, float) or math.isfinite(value)


def summarize(values):
    a = np.asarray(values, dtype=float)
    return {'samples': len(a), 'maximum': float(a.max()) if len(a) else 0,
            'p95': float(np.percentile(a, 95)) if len(a) else 0,
            'median': float(np.median(a)) if len(a) else 0}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--directory', type=Path, default=N / 'animation-review/native-cases')
    args = parser.parse_args()
    root = args.directory.resolve()
    source = root / 'ANIMATION_FRAMES.jsonl'
    rows = [json.loads(line) for line in source.read_text().splitlines()]
    raw = json.loads((root / 'ANIMATION_FILM_RAW.json').read_text())
    require(raw['passed'] and len(rows) == raw['frames'], 'Film is incomplete')
    require(all(finite(row) for row in rows), 'NaN/infinite animation sample')
    require([r['video_frame'] for r in rows] == list(range(len(rows))), 'Movie frame is missing or repeated')
    groups = {}
    for row in rows:
        require(all(math.isfinite(value) for value in row['root']), 'Invalid controller root')
        groups.setdefault((row['view'], row['label']), []).append(row)
    bow = []
    locomotion = []
    guards = []
    for view in ('third', 'first'):
        release = groups[view, 'bow-release']
        draw = groups[view, 'bow-draw']
        require(max(r['visible_draw'] for r in draw) >= .99, 'Source actor did not reach full draw')
        require(release[0]['draw'] == 0 and release[0]['visible_draw'] > .9,
                'Release teleported the visible source to neutral')
        visible = [r['visible_draw'] for r in release]
        steps = np.abs(np.diff([1.] + visible))
        require(visible[-1] < .001 and max(steps) < .15, 'Visible Bow return failed to settle continuously')
        bow.append({'view': view, 'release_frames': len(release), 'first_visible_fraction': visible[0],
                    'last_visible_fraction': visible[-1], 'maximum_draw_fraction_step': float(max(steps)),
                    'immediate_shot_observed': release[0]['shots'] > 0})
        for label in ('walk-forward', 'strafe-left', 'strafe-right', 'reverse', 'unarmed-run'):
            samples = groups[view, label]
            distance = sum(math.dist(a['root'], b['root']) for a, b in zip(samples, samples[1:]))
            require(distance > .15, 'Ordinary movement was suppressed: ' + view + '/' + label)
            locomotion.append({'view': view, 'label': label, 'distance_m': distance,
                               'duration_s': len(samples) / 60.})
        for label in ('stop', 'reverse-stop', 'unarmed-stop'):
            samples = groups[view, label][-8:]
            speed = [math.dist(a['root'], b['root']) * 60 for a, b in zip(samples, samples[1:])]
            require(max(speed) < .12, 'Controller failed to stop: ' + view + '/' + label)
        previous = None
        for label in ('guard-left', 'guard-right', 'guard-high', 'guard-low', 'guard-thrust'):
            samples = groups[view, label]
            require(samples[-1]['hero_active'], 'Guard silently used a legacy fallback')
            current = np.asarray(samples[-1]['joints'])
            step = float(max(np.linalg.norm(current - previous, axis=1))) if previous is not None else None
            require(step is None or step > .005, 'Guard direction did not change the real source pose')
            guards.append({'view': view, 'label': label, 'settled_pose_delta_m': step})
            previous = current
    fresh = {}
    for row in rows:
        if row.get('fresh_state', 0):
            fresh.setdefault(row['label'], []).append(row)
    require(len(fresh) == raw['fresh_attacks_completed'] == 26, 'Fresh steel-recovery coverage differs')
    fresh_rows = []
    for label, samples in fresh.items():
        last = samples[-1]
        require(any(row['fresh_state'] == 1 and row['player_phase'] != 0 for row in samples),
                'Fresh ordinary action never entered an attack phase: ' + label)
        require(last['fresh_state'] == 2 and last['player_phase'] == 0 and last['resolved_outcome'] == 1
                and last['resolved_serial'] == last['attack_serial'], 'Fresh attack did not resolve genuine air then Ready')
        require(any(event[0] == 8 and event[2] == last['attack_serial'] for row in samples for event in row['events']),
                'Fresh miss lacks its real typed outcome event')
        fresh_rows.append({'label': label, 'serial': last['attack_serial'], 'resolved_outcome': 'Miss',
                           'ready': True, 'frames': len(samples)})
    events = []
    peaks = []
    nonstrike = []
    grip, foot = [], []
    for row in rows:
        if row['hero_active']:
            grip.append(row['grip_error_m'])
            for side, state in enumerate(row['feet']):
                if state['planted'] and state['weight'] > .8:
                    foot.append(state['residual_m'])
        for event in row['events']:
            events.append({'frame': row['video_frame'], 'label': row['label'],
                           'kind': event[0], 'serial': event[1], 'attack_serial': event[2], 'damage': event[3]})
    for before, after in zip(rows, rows[1:]):
        # Opener stage boundaries are real transitions and must be measured.
        # Separate contact scenes, selected appearances and camera views are cuts.
        if (before['view'], before['weapon'], before['opener']) != (after['view'], after['weapon'], after['opener']):
            continue
        if not before['opener'] and before['label'] != after['label']:
            continue
        if after['attack_serial'] < before['attack_serial'] or after['resolved_serial'] < before['resolved_serial']:
            # The existing scene runner announces its next label one frame
            # before arena_begin resets that run's serials and root placement.
            continue
        if after['game_frame'] != before['game_frame'] + 1:
            continue
        metric = {'frame': after['video_frame'], 'label': after['label'], 'view': after['view'],
                  'from_label': before['label'], 'stage_boundary': before['label'] != after['label'],
                  'player_phase': after['player_phase'], 'reaction': after['reaction_weight']}
        if before['hero_active'] and after['hero_active'] and len(before['joints']) == len(after['joints']):
            delta = np.linalg.norm(np.asarray(after['joints']) - np.asarray(before['joints']), axis=1)
            metric['hero_max_joint_step_m'] = float(max(delta))
            if after['player_phase'] == before['player_phase'] == 0 and not after['events']:
                nonstrike.append(float(max(delta)))
        for key in ('blade', 'npc_blade'):
            if key == 'npc_blade' and before.get('fresh_state', 0) == 0 and after.get('fresh_state', 0) == 1:
                metric['explicit_npc_placement_cut'] = True
                continue
            valid = key + '_valid'
            if before[valid] and after[valid]:
                a, b = np.asarray(before[key]).reshape(2, 3), np.asarray(after[key]).reshape(2, 3)
                metric[key + '_maximum_step_m'] = float(max(np.linalg.norm(b - a, axis=1)))
                va, vb = a[1] - a[0], b[1] - b[0]
                denominator = np.linalg.norm(va) * np.linalg.norm(vb)
                metric[key + '_angular_speed_rad_s'] = float(math.acos(np.clip(np.dot(va, vb) / denominator, -1, 1)) * 60) if denominator > 1e-10 else 0
        peaks.append(metric)
    peaks.sort(key=lambda row: max(row.get('hero_max_joint_step_m', 0), row.get('blade_maximum_step_m', 0), row.get('npc_blade_maximum_step_m', 0)), reverse=True)
    inputs = {str(Path(__file__).resolve()): sha(__file__), str(source): sha(source),
              str(root / 'ANIMATION_FILM_RAW.json'): sha(root / 'ANIMATION_FILM_RAW.json'),
              str(root / 'ANIMATION_CONTINUOUS.mp4'): sha(root / 'ANIMATION_CONTINUOUS.mp4')}
    report = {'schema': 'valwood.animation-polish.continuous-film-measurements.v1', 'passed': True,
              'inputs': inputs, 'frames': len(rows), 'fixed_simulation_hz': 60,
              'bow': bow, 'ordinary_movement': locomotion, 'grip_error_m': summarize(grip),
              'guards': guards, 'fresh_actual_attacks': fresh_rows, 'finite_samples': True,
              'planted_foot_residual_m': summarize(foot), 'non_strike_joint_steps_m': summarize(nonstrike),
              'largest_frame_steps_for_visual_review': peaks[:32], 'actual_events': events,
              'limits': ['Frame spikes are diagnostic; intentional fast strikes and impacts require visual/context review.',
                         'Foot residual uses the production support solver; a planted-anchor residual is not a sole-centroid skating measurement.',
                         'Software-rendered footage does not certify hardware FPS, input latency, physical vibration or controller feel.',
                         'Bow return blends owned source poses; no dedicated release capture is claimed.']}
    path = N / 'animation-review/ANIMATION_FRAME_ANALYSIS.json'
    path.write_text(json.dumps(report, indent=2) + '\n')
    print('PASS', len(rows), 'continuous actual frames:', path)


if __name__ == '__main__':
    main()
