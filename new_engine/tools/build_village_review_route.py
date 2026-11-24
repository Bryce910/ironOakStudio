#!/usr/bin/env python3
"""Make source camera captures or a render-timing path over verified lanes.

Capture mode uses explicit station cuts. Timing mode replays eye positions
over separately checked walking lanes; neither mode simulates game input.
"""
from pathlib import Path
import argparse
import hashlib
import json
import math
import struct
import numpy as np

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--project', type=Path, default=ROOT / 'projects/valwood_city_showcase')
    parser.add_argument('--hold', type=int, default=60)
    parser.add_argument('--motion', action='store_true', help='Follow the separately CPU-verified walking lanes without capture readbacks')
    args = parser.parse_args()
    if not 1 <= args.hold <= 600:
        raise ValueError('Station hold must be 1..600 frames')
    qa = args.project.resolve() / 'qa'
    if args.motion:
        motion_route(args.project.resolve())
        return
    source = qa / 'publisher-cameras.json'
    document = json.loads(source.read_text())
    cameras = document['cameras']
    ground = json.loads((qa / 'source-ground-audit.json').read_text())
    if not cameras or not ground['coverage'] > 0:
        raise ValueError('Recovered camera and terrain audits are required')
    rows = ['VALWOOD_SOURCE_REVIEW 1 60']
    for camera in cameras:
        values = [*camera['position_world'], camera['yaw_radians'], camera['pitch_radians'],
                  camera['fov_y_radians']]
        if not all(math.isfinite(v) for v in values):
            raise ValueError('Nonfinite source camera')
        label = camera['source_name'].replace('_', '-').lower()
        rows.append(f"{label} 0 {args.hold} 1 " + ' '.join(format(v, '.15g') for v in values))
    target = qa / 'publisher_stations.route'
    target.write_text('\n'.join(rows) + '\n')
    report = dict(kind='Exact publisher camera stations; capture-only cuts, no interpolated route',
        cpu_geometry_audit=str(qa / 'source-ground-audit.json'),
        source_camera_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),
        requested_ticks=len(cameras) * (args.hold + 1), capture_checkpoints=len(cameras),
        obstruction_cuts=0, interpolated_spans=0,
        walking_verified=False, sustained_performance_verified=False,
        route_sha256=hashlib.sha256(target.read_bytes()).hexdigest())
    target.with_suffix('.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))


def motion_route(project):
    qa = project / 'qa'
    report_path = qa / 'collision/report.json'
    collision = json.loads(report_path.read_text())
    if not collision['passed']:
        raise ValueError('CPU source walking audit must pass before timing')
    for file, key in [('city.physics', 'collision_sha256'), ('ground/showcase.height', 'height_sha256')]:
        if hashlib.sha256((project / file).read_bytes()).hexdigest() != collision[key]:
            raise ValueError('Walking geometry changed after its collision audit')
    lanes = []
    for name in ['statue_lane', 'square_cross_lane', 'market_lane']:
        entry = next(r for r in collision['routes'] if r['name'] == name)
        if not entry['passed'] or not entry['expected_clear']:
            raise ValueError('Timing lane must be independently collision-checked')
        values = [list(map(float, line.split()[1:])) for line in Path(entry['route']).read_text().splitlines()[1:]]
        if lanes and np.max(np.abs(np.asarray(lanes[-1]) - values[0])) > 1e-6:
            raise ValueError('Walking lanes must connect')
        lanes.extend(values if not lanes else values[1:])
    data = (project / 'ground/showcase.height').read_bytes()
    magic, width, height, x0, z0, dx, dz = struct.unpack_from('<8sIIdddd', data)
    if magic != b'VWHGT1\0\0':
        raise ValueError('Source terrain field required')
    heights = np.frombuffer(data, '<f4', offset=48).reshape(height, width)

    def eye(x, z):
        u, v = (x - x0) / dx, (z - z0) / dz
        if not 0 <= u < width - 1 or not 0 <= v < height - 1:
            raise ValueError('Lane left the verified source terrain')
        i, j = math.floor(u), math.floor(v)
        a, b = u - i, v - j
        y = ((1 - a) * heights[j, i] + a * heights[j, i + 1]) * (1 - b)
        y += ((1 - a) * heights[j + 1, i] + a * heights[j + 1, i + 1]) * b
        return float(y) + 1.75

    path = lanes + lanes[-2::-1]
    points, distance = [], 0.
    first_yaw = math.atan2(path[1][0] - path[0][0], -(path[1][1] - path[0][1]))
    points.append(('lane-start', 0, 60, path[0][0], eye(*path[0]), path[0][1], first_yaw))
    yaw = first_yaw
    for segment, (start, end) in enumerate(zip(path, path[1:])):
        length = math.dist(start, end)
        facing = math.atan2(end[0] - start[0], -(end[1] - start[1]))
        turn = math.remainder(facing - yaw, 2 * math.pi)
        if abs(turn) > 1e-6:
            for step in range(1, 9):
                points.append((f'turn-{segment}-{step}', 6, 0, start[0], eye(*start), start[1], yaw + turn * step / 8))
        yaw = facing
        count = math.ceil(length / .25)
        for step in range(1, count + 1):
            t = step / count
            x, z = [a + (b - a) * t for a, b in zip(start, end)]
            points.append((f'lane-{segment}-{step}', 6, 0, x, eye(x, z), z, yaw))
        distance += length
    rows = ['VALWOOD_SOURCE_REVIEW 1 60']
    for label, transition, hold, x, y, z, yaw in points:
        rows.append(f'{label} {transition} {hold} 0 {x:.15g} {y:.15g} {z:.15g} {yaw:.15g} 0 {math.radians(60):.15g}')
    target = qa / 'walking_lanes.route'
    target.write_text('\n'.join(rows) + '\n')
    result = dict(kind='First-person render timing over separately Jolt-verified source lanes; camera replay, not gameplay simulation',
        cpu_geometry_audit=str(report_path), collision_audit_sha256=hashlib.sha256(report_path.read_bytes()).hexdigest(),
        requested_ticks=sum(p[1] + p[2] + 1 for p in points), capture_checkpoints=0, obstruction_cuts=0,
        distance_m=distance, target_translation_mps=2.5, render_readbacks=False,
        walking_collision_audited_separately=True, gameplay_simulated=False,
        route_sha256=hashlib.sha256(target.read_bytes()).hexdigest())
    target.with_suffix('.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
