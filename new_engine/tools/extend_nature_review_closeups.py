#!/usr/bin/env python3
"""Append exact material baseline cuts to an audited review; original route stays intact."""
from pathlib import Path
import copy, hashlib, json, math
import numpy as np
from audit_nature_publisher_cameras import Scene

ROOT = Path(__file__).resolve().parents[1]
PROJECT = ROOT/'projects/valwood_showcase'
DOCS = ROOT/'docs/stronghold/2026-11-01'
NAMES = ['qa-material-column', 'qa-material-glass', 'qa-material-king']

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def main():
    original = PROJECT/'qa/source_review.route'
    original_metadata = original.with_suffix('.json')
    views = DOCS/'showcase/material-closeups.views'
    unchanged = sha(original), sha(original_metadata)
    report = copy.deepcopy(json.loads(original_metadata.read_text()))
    assert report['capture_checkpoints'] == 17 and report['cpu_geometry_audit'] and report['obstruction_cuts'] == 0
    project = json.loads((PROJECT/'publisher_showcase.json').read_text())
    rows = {}
    for row in views.read_text().splitlines():
        values = row.split()
        if values and values[0] in NAMES:
            assert values[0] not in rows and len(values) == 6
            rows[values[0]] = [float(value) for value in values[1:]]
    assert set(rows) == set(NAMES)
    scene = Scene()
    lines = original.read_text().splitlines()
    audit = []
    for name in NAMES:
        x, y, z, yaw, pitch = rows[name]
        pose = [x, y, z, yaw, pitch, 1.0471975512]
        source = np.array([x-project['origin'][0]+500, y-project['floor'], 500-(z-project['origin'][1])])
        # Native Z reflects source Unity Z. Preserve the exact baseline yaw,
        # pitch and60-degree vertical FOV, including their decimal precision.
        forward = np.array([math.sin(yaw)*math.cos(pitch), math.sin(pitch), math.cos(yaw)*math.cos(pitch)])
        right = np.array([math.cos(yaw), 0., -math.sin(yaw)])
        up = np.cross(forward, right)
        checks = []
        directions = [forward, -forward, right, -right, up, -up]
        for index, direction in enumerate(directions):
            hit = scene.ray(source, direction, .30, near_clip=.005)
            checks.append(dict(kind='near_volume_ray', direction=index, obstruction=hit))
            assert hit is None, (name, 'camera near volume intersects source geometry', hit)
        for side, vertical in [(0,0),(.20,0),(-.20,0),(0,.15),(0,-.15)]:
            hit = scene.ray(source+right*side+up*vertical, forward, .40, near_clip=.005)
            checks.append(dict(kind='offset_forward_ray', offset=[side, vertical], obstruction=hit))
            assert hit is None, (name, 'camera clearance fan blocked', hit)
        floor = scene.ray(source, [0,-1,0], 200, near_clip=.005)
        terrain = scene.terrain_at(source)
        assert floor and floor['distance'] > .30, (name, 'no safe source floor below eye')
        assert not terrain or not terrain['solid'] or terrain['clearance'] > .30
        node = dict(label=name, transition_frames=0, hold_frames=90, capture=1, pose=pose,
                    source_position=source.tolist(), baseline_view_row=views.read_text().splitlines()[NAMES.index(name)],
                    source_time=None, deliberate_cut=True, stationary_closeup=True)
        index = len(report['nodes']); report['nodes'].append(node)
        report['segments'].append(dict(label=name, purpose='exact stationary material baseline comparison',
            first_node=index, last_node=index, sampled_distance_m=0, obstruction_cuts=0, hits=[], source_times=None))
        audit.append(dict(label=name, source_position=source.tolist(), near_clearance_checks=checks,
                          down_ray=floor, terrain=terrain, look_ray=scene.ray(source, forward, 25, near_clip=.30),
                          dynamic_residents_excluded=True))
        lines.append(f'{name} 0 90 1 '+' '.join(format(value, '.12g') for value in pose))
    output = PROJECT/'qa/source_review_final.route'
    output.write_text('\n'.join(lines)+'\n')
    report.update(route=str(output), capture_checkpoints=20,
        requested_ticks=len(report['nodes'])+sum(n['transition_frames']+n['hold_frames'] for n in report['nodes']),
        parent_route=str(original), parent_route_sha256=unchanged[0], parent_metadata_sha256=unchanged[1],
        baseline_views=str(views), baseline_views_sha256=sha(views), static_closeup_audit=audit,
        exact_baseline_camera_pose_and_fov=True, animation_phase_matches_standalone_baseline=False,
        capture_metrics_note='Readback-contaminated visual QA only. Held/motion frame-ms values are diagnostic; no FPS claim.')
    output.with_suffix('.json').write_text(json.dumps(report, indent=2)+'\n')
    assert unchanged == (sha(original), sha(original_metadata))
    assert output.read_bytes().startswith(original.read_bytes())
    print('READY', output, report['capture_checkpoints'], 'checkpoints;', len(audit)*11, 'new source clearance rays; original17 unchanged')

if __name__ == '__main__':
    main()
