#!/usr/bin/env python3
"""Validate authored camp data, then optionally run sequential native evidence.

Default: static source/manifest checks only. --native explicitly enables the
existing locked launcher; --captures records untimed pictures instead of FPS.
No build/cook commands or graphics-preference writes are performed here.
"""
import argparse
from collections import Counter
import fcntl
import json
import math
import os
from pathlib import Path
import subprocess
import sys

from PIL import Image
import benchmark_graphics_settings as common

ROOT = Path(__file__).resolve().parents[1]
PROJECT = ROOT / 'projects/valwood_world'
SCENERY = PROJECT / 'scenery'
LAYOUT = SCENERY / 'tanthoon_bandit_camp_layout.json'
MANIFEST = SCENERY / 'tanthoon_bandit_camp_manifest.json'
INSTANCES = SCENERY / 'tanthoon_bandit_camp_instances.txt'
VIEWS = ('bandit-camp', 'bandit-supplies', 'bandit-lookout', 'bandit-fire')
RUNS = ('day', 'dusk', 'lights-off', 'route', 'return')
ACTOR_LAYOUT = ROOT / 'assets/purchased/tanthoon_bandits/ACTOR_LAYOUT.json'
ACTOR_MANIFEST = PROJECT / 'models/tanthoon_bandit_manifest.json'
WIDTH, HEIGHT, WARMUP = 1600, 900, 120
require = common.require


def read_records(path):
    records = []
    seen = set()
    for number, line in enumerate(path.read_text().splitlines(), 1):
        text = line.split('#', 1)[0].strip()
        if not text:
            continue
        row = text.split()
        require(len(row) in (6, 7), f'{path.name}:{number}: expected six/seven columns')
        alias = row[0]
        require(alias.isascii() and alias.replace('_', '').isalnum() and not any(c in alias for c in '/\\.'), f'Unexpected alias/path: {alias}')
        values = [float(x) for x in row[1:]]
        require(all(math.isfinite(x) for x in values), f'{path.name}:{number}: nonfinite value')
        u, v, yaw, scale, burial = values[:5]
        elevation = values[5] if len(values) == 6 else 0.
        require(0 <= u <= 1 and 0 <= v <= 1 and scale > 0, f'{path.name}:{number}: invalid pose')
        key = (alias, u, v, elevation)
        require(key not in seen, f'{path.name}:{number}: duplicate alias/root/elevation')
        seen.add(key)
        records.append((alias, u, v, yaw, scale, burial, elevation))
    return records


def point_segment_distance(point, a, b):
    dx, dz = b[0] - a[0], b[1] - a[1]
    length2 = dx * dx + dz * dz
    t = min(1., max(0., ((point[0] - a[0]) * dx + (point[1] - a[1]) * dz) / length2)) if length2 else 0.
    return math.hypot(point[0] - a[0] - t * dx, point[1] - a[1] - t * dz)


def segment_segment_distance(a, b, c, d):
    ux, uz, vx, vz = b[0] - a[0], b[1] - a[1], d[0] - c[0], d[1] - c[1]
    denominator = ux * vz - uz * vx
    if abs(denominator) > 1.e-14:
        dx, dz = c[0] - a[0], c[1] - a[1]
        t, s = (dx * vz - dz * vx) / denominator, (dx * uz - dz * ux) / denominator
        if 0 <= t <= 1 and 0 <= s <= 1:
            return 0.
    return min(point_segment_distance(a, c, d), point_segment_distance(b, c, d),
               point_segment_distance(c, a, b), point_segment_distance(d, a, b))


def segment_box_distance(a, b, record):
    """Exact continuous XZ segment distance to a measured rotated source box."""
    yaw = math.radians(record['yaw_degrees'])
    c, s = math.cos(yaw), math.sin(yaw)
    x, z = record['world_xz']
    def local(point):
        dx, dz = point[0] - x, point[1] - z
        return c * dx - s * dz, s * dx + c * dz
    a, b = local(a), local(b)
    scale = record['scale']
    xmin, xmax = record['bounds_min'][0] * scale, record['bounds_max'][0] * scale
    zmin, zmax = record['bounds_min'][2] * scale, record['bounds_max'][2] * scale
    dx, dz = b[0] - a[0], b[1] - a[1]
    low, high = 0., 1.
    intersects = True
    for p, q in ((-dx, a[0] - xmin), (dx, xmax - a[0]), (-dz, a[1] - zmin), (dz, zmax - a[1])):
        if p == 0:
            if q < 0:
                intersects = False
                break
        elif p < 0:
            low = max(low, q / p)
        else:
            high = min(high, q / p)
        if low > high:
            intersects = False
            break
    if intersects:
        return 0.
    def endpoint_distance(p):
        return math.hypot(max(xmin - p[0], 0., p[0] - xmax), max(zmin - p[1], 0., p[1] - zmax))
    corners = ((xmin, zmin), (xmin, zmax), (xmax, zmin), (xmax, zmax))
    return min(endpoint_distance(a), endpoint_distance(b), *(point_segment_distance(p, a, b) for p in corners))


def geometry_check(layout, manifest):
    records = manifest['records']
    require(len(records) == manifest['record_count'], 'Grounded source bounds are incomplete')
    ox, oz = manifest['origin_world_xz']
    for record in records:
        require(all(math.isfinite(x) for x in record['world_xz'] + record['bounds_min'] + record['bounds_max']) and record['scale'] > 0,
                'Invalid measured source bounds')
    routes = []
    for path in layout['paths']:
        points = [(ox + p[0], oz + p[1]) for p in path['points_local_xz']]
        require(len(points) >= 2, 'Authored path has no segment')
        # Whole continuous lane, including its circular end caps, rather than
        # a finite set of sampled centers. Full source boxes conservatively
        # include geometry which is not a body collider, including foliage.
        minimum, obstacle = math.inf, None
        for a, b in zip(points, points[1:]):
            for record in records:
                clearance = segment_box_distance(a, b, record) - path['half_width_m'] - .37
                if clearance < minimum:
                    minimum, obstacle = clearance, record['id']
        require(minimum >= -.00001, f'{path["id"]}: measured source intrudes into the full walking lane/capsule at {obstacle}: {minimum:.6f}m')
        routes.append({'id': path['id'], 'minimum_source_lane_capsule_margin_m': minimum, 'closest_record': obstacle})
    actors = json.loads(ACTOR_LAYOUT.read_text())
    model_manifest = json.loads(ACTOR_MANIFEST.read_text())
    require(actors['source_manifest_sha256'] == common.digest(ACTOR_MANIFEST), 'Actor layout references a different source-pose manifest')
    require(sum(family['pose_count'] for family in model_manifest['families']) == 40, 'Expected bounded40-pose bandit bank')
    require(all(len(family['assets']) == family['pose_count'] for family in model_manifest['families']), 'Source pose inventory is incomplete')
    require(tuple(actors['origin']) == (ox, oz), 'Actor/camp origins differ')
    modes = actors['source_modes']
    bodies = [math.hypot(max(abs(mode['bounds_min'][0]), abs(mode['bounds_max'][0])),
                        max(abs(mode['bounds_min'][2]), abs(mode['bounds_max'][2]))) + .01 for mode in modes]
    actor_proofs = []
    for index, actor in enumerate(actors['actors']):
        radius = bodies[actor['mode']]
        points = actors['paths'][actor['path']]['points'] if actor['path'] >= 0 else [(actor['x'], actor['z']), (actor['x'], actor['z'])]
        minimum, obstacle = math.inf, None
        for a, b in zip(points, points[1:]):
            for record in records:
                clearance = segment_box_distance(a, b, record) - radius
                if clearance < minimum:
                    minimum, obstacle = clearance, record['id']
        require(minimum >= -.00001, f'Actor{index}: full blended-source XZ body envelope intersects {obstacle}: {minimum:.6f}m')
        actor_proofs.append({'actor': index, 'mode': actor['mode'], 'path': actor['path'], 'source_xz_radius_m': radius,
                             'minimum_source_body_margin_m': minimum, 'closest_record': obstacle})
    sentries = [actor for actor in actors['actors'] if actor['path'] < 0]
    station_margin = math.inf
    for actor in sentries:
        point = (actor['x'], actor['z'])
        for path in actors['paths']:
            distance = min(point_segment_distance(point, a, b) for a, b in zip(path['points'], path['points'][1:]))
            station_margin = min(station_margin, distance - .8)
        for other in sentries:
            if other is not actor:
                station_margin = min(station_margin, math.hypot(actor['x'] - other['x'], actor['z'] - other['z']) - .8)
    require(station_margin >= -.00001, f'Station intersects another stationary/moving .4m actor capsule: {station_margin:.6f}m')
    path_margin = math.inf
    for i, path in enumerate(actors['paths']):
        for other in actors['paths'][i + 1:]:
            path_margin = min(path_margin, min(segment_segment_distance(a, b, c, d)
                for a, b in zip(path['points'], path['points'][1:]) for c, d in zip(other['points'], other['points'][1:])) - .8)
    require(path_margin >= -.00001, f'Separate actor routes intersect .4m body capsules: {path_margin:.6f}m')
    return {'paths': routes, 'actors': actor_proofs, 'station_to_moving_or_stationary_capsule_margin_m': station_margin,
            'separate_actor_path_capsule_margin_m': path_margin,
            'scope': 'Independent continuous segment-to-measured-source-OBB proof; complete visual lane plus0.37m player capsule and all actor source-bound XZ envelopes. Does not implement engine compound collisions or terrain support.'}


def static_check():
    layout, manifest = json.loads(LAYOUT.read_text()), json.loads(MANIFEST.read_text())
    require(manifest.get('schema_version') == 1 and layout.get('schema_version') == 1, 'Unsupported camp schema')
    require(manifest.get('layout_source') == LAYOUT.name and manifest.get('instances_source') == INSTANCES.name, 'Manifest points at a different layout/source')
    require(common.digest(LAYOUT) == manifest['layout_sha256'], 'Camp layout hash differs from manifest')
    require(common.digest(INSTANCES) == manifest['instances_sha256'], 'Camp instance hash differs from manifest')
    records = read_records(INSTANCES)
    require(len(records) == manifest['record_count'] and records, 'Camp record count differs from manifest')
    counts = Counter(row[0] for row in records)
    require(dict(counts) == manifest['counts'], 'Camp alias inventory differs from manifest')
    protected = manifest['protected_sources']
    expected_sources = {'authored_instances.txt', 'starting_area_instances.txt', 'rainforest_instances.txt', 'castle_village_instances.txt'}
    require({Path(row['file']).name for row in protected} == expected_sources and len(protected) == 4, 'Original four source proofs are incomplete')
    total = len(records)
    combined = counts.copy()
    for source in protected:
        path = SCENERY / Path(source['file']).name
        require(common.digest(path) == source['sha256'], f'Original source modified: {source["file"]}')
        old = read_records(path)
        require(len(old) == source['records'], f'Original record count changed: {source["file"]}')
        total += len(old)
        combined.update(row[0] for row in old)
    require(total <= 8192 and max(combined.values()) <= 4096, 'Combined scenery exceeds current runtime caps')
    require(manifest.get('combined_record_count') == total, 'Combined record count differs from manifest')
    if 'largest_combined_alias_batch' in manifest:
        require(manifest['largest_combined_alias_batch'] == max(combined.values()), 'Combined alias budget differs from manifest')
    route = manifest['route']
    require(math.isfinite(route['length_m']) and route['length_m'] > 0, 'Missing/invalid authored route length')
    require(0 < route['sampled_step_m'] <= .5, 'Authored route needs samples at most0.5m apart')
    require(route.get('clear_capsule_radius_m', 0) >= .37, 'Route proof uses an undersized walking capsule')
    require(math.isfinite(route['max_absolute_grade']) and route['max_absolute_grade'] < .7, 'Authored route grade is not walkable')
    # These are checks of the authoring evidence, not a replacement collider or
    # an unverified claim that a new Python AABB test matches runtime geometry.
    for field in ('minimum_path_footprint_margin_m', 'minimum_spawn_footprint_margin_m'):
        if field in manifest:
            require(math.isfinite(manifest[field]) and manifest[field] >= 0, f'Negative authoring clearance: {field}')
    geometry = geometry_check(layout, manifest)
    return {'record_count': len(records), 'combined_records': total, 'counts': dict(counts), 'continuous_geometry': geometry,
            'protected_sources': protected, 'route': route,
            'scope': 'Actual record/hash/inventory checks plus verification of declared authoring proof fields. Runtime collision/grounding still requires its actual fixture and native route.'}, layout, manifest


def check_saved_preferences():
    path = PROJECT / 'graphics.settings'
    require(path.is_file(), 'Save Custom85% manual Balanced effects in the graphics menu first; helper will not change preferences')
    values = {}
    for line in path.read_text().splitlines():
        text = line.strip()
        if not text or text.startswith('#'):
            continue
        key, separator, value = text.partition('=')
        require(separator and key.strip() not in values, 'Malformed/duplicate graphics preference field')
        values[key.strip()] = value.strip()
    exact = {'version': '1', 'preset': 'Custom', 'auto_quality': '0', 'target_fps': '60',
             'shadow_side': '2048', 'reflections': '1', 'contact_shadows': '1', 'volumetric_fog': '1', 'clouds': '1'}
    require(all(values.get(k) == v for k, v in exact.items()), 'Saved graphics preferences are not Custom/manual/60FPS/Balanced effects')
    require(abs(float(values['render_scale']) - .85) < .00001 and abs(float(values['reflection_scale']) - .5) < .00001,
            'Saved graphics preferences are not85% scale / half reflections')
    require(.70 <= float(values['min_render_scale']) <= .85, 'Saved minimum render scale is invalid')
    return common.preference_state()


def route_frames(manifest):
    route = manifest['route']
    distance = route.get('return_walk_m', route['length_m'])
    require(math.isfinite(distance) and distance > 0, 'Missing complete benchmark route distance')
    # Two seconds of margin, rounded to whole two-second windows. The frozen
    #130.112m loop therefore uses2520 total frames including120 warmup.
    return math.ceil((math.ceil(distance / 3.5 * 60) + WARMUP + 120) / 120) * 120


def fingerprints_while_idle():
    # The launcher obtains this same lock for every game. Guard the expensive
    # full hashes separately so they never run concurrently with a Valwood GPU
    # session; release it before invoking the independently locked launcher.
    lock = ROOT / 'projects/seldonne/.valwood-instance.lock'
    require(lock.parent.is_dir(), 'Prepare the shared game-instance directory first')
    with lock.open('a') as stream:
        try:
            fcntl.flock(stream.fileno(), fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError as error:
            raise ValueError('Close the current Valwood session before full input fingerprinting') from error
        try:
            return common.fingerprints()
        finally:
            fcntl.flock(stream.fileno(), fcntl.LOCK_UN)


def native_world_check(stem, frames, timed, run, manifest, save_png=True):
    capture = stem.with_suffix('.bmp')
    metadata = json.loads(Path(str(capture) + '.json').read_text())
    renderer = str(metadata.get('renderer', ''))
    require(('AMD' in renderer or 'Radeon' in renderer) and not any(x in renderer.lower() for x in ('llvmpipe', 'softpipe', 'swrast', 'software')), 'Expected native AMD renderer')
    require(metadata.get('capture_ok') is True and metadata.get('offscreen') is False, 'Capture failed or used offscreen/software rendering')
    require((metadata.get('width'), metadata.get('height'), metadata.get('frames')) == (WIDTH, HEIGHT, frames), 'Native drawable/frame count differs')
    with Image.open(capture) as image:
        require(image.size == (WIDTH, HEIGHT), 'BMP is not the full1600x900 drawable')
        if save_png:
            image.save(stem.with_suffix('.png'))
    report = Path(str(stem.with_suffix('.json') if timed else capture) + '.world.json')
    world = json.loads(report.read_text())
    for field in ('gl_error', 'failed_tiles', 'remaining_texture_entries', 'remaining_texture_references', 'occlusion_failures', 'weather_failures'):
        require(world.get(field) == 0, f'{field}: {world.get(field)!r}')
    g = world['graphics']
    require(g.get('preset') == 'Custom' and g.get('automatic') is False and g.get('target_fps') == 60, 'Saved fixed Custom60FPS mode did not apply')
    require(g.get('apply_failures') == 0 and g.get('scale_changes') == 0, 'Fixed-mode allocation/scale regression')
    require((g.get('display_width'), g.get('display_height'), g.get('render_width'), g.get('render_height')) == (1600, 900, 1360, 765), 'Saved85% render/drawable dimensions differ')
    require(all(abs(g[field] - .85) <= .001 for field in ('effective_scale', 'minimum_observed_scale', 'maximum_observed_scale')), 'Fixed85% scale drifted')
    require(g.get('reflections') is True and g.get('reflection_scale') == .5 and g.get('contact_shadows') is True and
            g.get('volumetric_fog') is True and g.get('volumetric_clouds') is True, 'Balanced effects changed')
    require(world.get('shadow_map_side') == 2048 and world.get('soft_shadow_taps') == 9 and world.get('contact_lighting') is True and world.get('volumetric_clouds') is True,
            'Actual Balanced lighting passes differ from saved settings')
    require(world.get('authored_source_files') == 5 and world.get('authored_instances') == manifest['combined_record_count'], 'Camp scene did not merge the expected five sources')
    sources = [s for s in world['authored_sources'] if str(s['path']).endswith(INSTANCES.name)]
    require(len(sources) == 1 and sources[0]['instances'] == manifest['record_count'], 'Actual camp source inventory differs')
    camp = world['bandit_camp']
    require(camp.get('load_failures') == 0 and camp.get('unsafe_roots') == 0, 'Bandit load/ground/collision safety regression')
    require(camp.get('updates', 0) >= frames, 'Bandit update count differs from elapsed frames')
    actor_data = json.loads(ACTOR_LAYOUT.read_text())
    expected_roles = Counter(actor['mode'] for actor in actor_data['actors'])
    require(camp.get('peak_visible', 0) > 0 and camp.get('peak_pose_assets') == 40, 'Camp actors were never visible/resident in the bounded40-pose bank')
    if run == 'return':
        require(camp.get('active') is False and camp.get('actors') == 0 and camp.get('pose_assets') == 0, 'Leaving camp retained its actor bank')
        require(camp.get('loads') == camp.get('unloads') == 1, 'Return check did not load/release exactly one bank')
    else:
        require(camp.get('active') is True and camp.get('actors') == len(actor_data['actors']) and camp.get('pose_assets') == 40, 'Camp actor bank not fully resident')
        require((camp.get('walking'), camp.get('running'), camp.get('sentries')) == (expected_roles[0], expected_roles[1], expected_roles[2]), 'Authored patrol/runner/stance-sentry roles missing')
        require(camp.get('loads') == 1 and camp.get('unloads') == 0, 'Stationary/walking camp unexpectedly changed residency')
    lights_off = run == 'lights-off'
    require(camp.get('lights_off') is lights_off, 'Scoped camp light-cost QA override did not apply')
    for key in ('light_count', 'peak_lights', 'flame_draws', 'peak_visible_flames'):
        require(isinstance(camp.get(key), int) and camp[key] >= 0, f'Missing/invalid camp light counter {key}')
    if lights_off:
        require(all(camp[key] == 0 for key in ('light_count', 'peak_lights', 'flame_draws', 'peak_visible_flames')), 'Lights-off still submitted camp light/flame work')
    elif run != 'return':
        require(camp['peak_lights'] > 0 and camp['flame_draws'] > 0, 'Camp practical/flame draws never occurred')
    if run in ('day', 'dusk', 'lights-off'):
        require(world.get('travel_loading_frames') == 0, 'Stationary profile unexpectedly held loading images')
    if run == 'route':
        route = world['benchmark_route']
        require(route.get('active') is True and 'bandit' in route.get('id', ''), 'Expected camp benchmark route')
        for field in ('collision_blocked_steps', 'slope_blocked_steps', 'sliding_steps'):
            require(route.get(field) == 0, f'Actual camp walk failed: {field}')
        require(route.get('complete') is True and route.get('return_error_m', 1) < .001, 'Actual full camp walk did not finish/return')
    expected_hour = 17.8 if run in ('dusk', 'lights-off') else 17.05
    require(abs(world['lighting']['hour'] - expected_hour) < .01, 'Actual day/dusk lighting differs from requested look')
    return {'renderer': renderer, 'graphics': g, 'bandit_camp': camp, 'lighting': world['lighting'],
            'travel_loading_frames': world['travel_loading_frames'], 'benchmark_route': world['benchmark_route'],
            'world_report': str(report), 'capture': str(capture)}


def load_timing(stem, frames):
    timing = json.loads(stem.with_suffix('.json').read_text())
    require(timing.get('samples') == frames - WARMUP and timing.get('dropped_samples') == 0, 'Measured samples were lost/excluded')
    for field in ('mean_fps', 'one_percent_low_fps', 'p99_ms', 'max_ms'):
        require(common.finite_number(timing.get(field)) and timing[field] > 0, f'Invalid timing field {field}')
    gpu=timing.get('mean_render_total_gpu_ms')
    if timing.get('gpu_stage_timing_available',True):
        require(common.finite_number(gpu) and gpu>0,'Invalid timing field mean_render_total_gpu_ms')
    else:
        require(common.finite_number(gpu) and gpu==0,'Unavailable GPU timing must be explicitly zero')
    return timing


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--native', action='store_true', help='Explicitly launch sequential native jobs after static checks')
    parser.add_argument('--captures', action='store_true', help='Untimed300-frame captures, no FPS claim')
    parser.add_argument('--runs', nargs='+', choices=RUNS, help='Native cases (default: all; captures: day/dusk only)')
    parser.add_argument('--views', nargs='+', choices=VIEWS, help='Stationary views (default: entry; captures: all four)')
    parser.add_argument('--frames', type=int, help='Override frames; route duration must still cover its full walk')
    parser.add_argument('--output', type=Path, default=ROOT / 'docs/performance/tanthoon-bandits/native')
    parser.add_argument('--verify-existing', action='store_true', help='Read existing native reports; never launches or hashes full runtime inputs')
    args = parser.parse_args(argv)
    require(not args.captures or args.native or args.verify_existing, '--captures requires --native or --verify-existing')
    if args.frames is not None and args.frames <= (0 if args.captures else WARMUP):
        parser.error('Profile frames must exceed120; captures must be positive')
    static, layout, manifest = static_check()
    if not args.native and not args.verify_existing:
        print(json.dumps({'static_pass': True, **static}, indent=2))
        return 0
    preferences = check_saved_preferences()
    runs = args.runs or (['day', 'dusk'] if args.captures else list(RUNS))
    views = args.views or (list(VIEWS) if args.captures else ['bandit-camp'])
    require(len(set(runs)) == len(runs) and len(set(views)) == len(views), 'Duplicate native cases/views')
    require(not args.captures or not any(run in ('route', 'return') for run in runs), 'Capture mode does not profile route/return checks')
    require('return' not in runs or args.frames is None or args.frames > 480, 'Return check must pass its480-frame transition')
    require('route' not in runs or args.frames is None or args.frames >= route_frames(manifest), 'Explicit duration is too short for the complete route')
    cases = [(run, view) for run in runs for view in (views if run in ('day', 'dusk', 'lights-off') else ['bandit-camp'])]
    output = args.output.expanduser().resolve()
    for directory in (ROOT / 'engine', ROOT / 'shaders', PROJECT / 'models', PROJECT / 'scenery', PROJECT / 'ground'):
        require(output != directory and not output.is_relative_to(directory), 'Evidence directory cannot be inside frozen runtime inputs')
    suffix = 'view' if args.captures else 'final'
    paths = [output / name for name in ('results.json', 'source-freeze.json', 'gpu-memory.json')]
    for run, view in cases:
        paths.extend(output / (f'{run}-{view}-{suffix}' + ext) for ext in ('.log', '.bmp', '.png', '.json', '.bmp.json', '.bmp.world.json', '.json.world.json', '.json.csv'))
    if not args.verify_existing:
        require(not any(path.exists() for path in paths), 'Evidence already exists; choose a fresh output directory')
        require((ROOT / 'rpg').is_file() and os.access(ROOT / 'rpg', os.X_OK) and (PROJECT / 'project.valwood').is_file(), 'Prepare executable/project first; helper will not build/cook')
        output.mkdir(parents=True, exist_ok=True)
    else:
        require(output.is_dir(), 'Existing evidence directory not found')
    environment, removed = common.native_environment()
    before_metadata = common.metadata() if not args.verify_existing else None
    frozen = fingerprints_while_idle() if not args.verify_existing else None
    if not args.verify_existing:
        require(common.metadata() == before_metadata, 'Inputs changed during pre-run fingerprint preparation')
        (output / 'source-freeze.json').write_text(json.dumps({'sha256': frozen, 'preferences_before': preferences, 'removed_environment_keys': removed,
            'saved_graphics_required': 'Custom85% manual Balanced effects /60target; no graphics CLI overrides'}, indent=2) + '\n')
    rows, failures = [], []
    monitor = common.MemoryMonitor()
    if not args.verify_existing:
        monitor.start()
    try:
        for run, view in cases:
            stem = output / f'{run}-{view}-{suffix}'
            frames = args.frames or (300 if args.captures else 900 if run == 'return' else route_frames(manifest) if run == 'route' else 1800)
            require(run != 'return' or frames > 480, 'Return check must pass its480-frame destination transition')
            require(run != 'route' or frames >= route_frames(manifest), 'Explicit route duration is too short for the full walk')
            timed = not args.captures and run != 'return'
            command = [str(common.LAUNCHER), '--scenic', view, '--width', str(WIDTH), '--height', str(HEIGHT),
                       '--frames', str(frames), '--no-hud', '--capture', str(stem.with_suffix('.bmp'))]
            if timed:
                command += ['--benchmark', '--view', 'river' if run == 'route' else 'sweep', '--warmup', str(WARMUP), '--profile', str(stem.with_suffix('.json'))]
            run_environment = environment.copy()
            run_environment['VALWOOD_VILLAGE_LIGHTING'] = 'dusk' if run in ('dusk', 'lights-off') else 'afternoon'
            if run == 'return':
                run_environment['VALWOOD_QA_BANDIT_RETURN'] = '1'
            if run == 'lights-off':
                run_environment['VALWOOD_QA_BANDIT_LIGHTS_OFF'] = '1'
            row = {'run': run, 'view': view, 'frames': frames, 'timed': timed, 'command': command,
                   'transition_scope': 'resident startup followed by usual destination teleport/unload at480; not a continuous approach' if run == 'return' else None}
            try:
                if not args.verify_existing:
                    print(f'Native Tanthoon {run}: {view}', flush=True)
                    with stem.with_suffix('.log').open('w') as stream:
                        process = subprocess.run(command, cwd=ROOT, env=run_environment, stdout=stream, stderr=subprocess.STDOUT, check=False)
                    require(process.returncode == 0, f'Native job exited{process.returncode}; inspect its preserved log')
                if timed:
                    row['timing'] = load_timing(stem, frames)
                    row['strict_45fps_pass'] = row['timing']['mean_fps'] >= 45 and row['timing']['one_percent_low_fps'] >= 45
                else:
                    row['timing'] = None
                    row['strict_45fps_pass'] = None
                row.update(native_world_check(stem, frames, timed, run, manifest, save_png=not args.verify_existing))
                row['correctness_pass'] = True
            except (OSError, ValueError, KeyError, TypeError) as error:
                row.update(correctness_pass=False, error=str(error))
            rows.append(row)
            print(json.dumps({k: v for k, v in row.items() if k not in ('command', 'timing')}), flush=True)
            if not args.verify_existing and (common.metadata() != before_metadata or common.preference_state() != preferences):
                failures.append('Inputs/preferences changed during batch; remaining native work cancelled')
                break
    finally:
        if not args.verify_existing:
            monitor.stop()
            (output / 'gpu-memory.json').write_text(json.dumps(monitor.samples, indent=2) + '\n')
            try:
                require(fingerprints_while_idle() == frozen, 'Frozen runtime source/assets changed')
                require(common.preference_state() == preferences, 'Graphics preferences changed; helper did not restore/overwrite user edits')
            except (OSError, ValueError) as error:
                failures.append(str(error))
        timed_rows = [row for row in rows if row.get('timing')]
        expected_timed = sum(not args.captures and run != 'return' for run, _ in cases)
        pairs = []
        for view in views:
            on = next((row for row in timed_rows if row['run'] == 'dusk' and row['view'] == view), None)
            off = next((row for row in timed_rows if row['run'] == 'lights-off' and row['view'] == view), None)
            if on and off:
                fields = ('mean_render_total_gpu_ms', 'mean_forest_gpu_ms', 'mean_shadow_gpu_ms',
                          'mean_raw_update_ms', 'mean_raw_render_cpu_ms', 'mean_raw_editor_ms', 'mean_present_ms')
                pairs.append({'view': view, 'on_correctness_pass': on.get('correctness_pass'), 'off_correctness_pass': off.get('correctness_pass'),
                    'observed_on_minus_off_ms': {field: on['timing'][field] - off['timing'][field] for field in fields
                        if common.finite_number(on['timing'].get(field)) and common.finite_number(off['timing'].get(field))},
                    'scope': 'Matched dusk scene/source/actor/scale; only scoped camp lights and flames differ. Observed paired mean differences include driver/system variation.'})
        summary = {'static': static, 'runs': rows, 'failures': failures,
                   'correctness_pass': len(rows) == len(cases) and all(row.get('correctness_pass') for row in rows) and not failures,
                   'strict_45fps_gate': {'definition': 'Mean FPS and1% low both>=45; correctness-only return/captures excluded',
                       'all_pass': len(timed_rows) == expected_timed and all(row['strict_45fps_pass'] for row in timed_rows) if expected_timed else None,
                       'failed_cases': [row['run'] + ':' + row['view'] for row in timed_rows if not row['strict_45fps_pass']]},
                   'preferences_preserved': common.preference_state() == preferences,
                   'paired_camp_light_cost': pairs,
                   'memory': monitor.summary() if not args.verify_existing else None,
                   'note': 'Original timing profiles retain all post-warmup intervals, including walking/streaming holds. FPS gate failures do not stop later runs. Return/captures carry no FPS claim.'}
        if not args.verify_existing:
            (output / 'results.json').write_text(json.dumps(summary, indent=2) + '\n')
        else:
            print(json.dumps(summary, indent=2))
    return 0 if summary['correctness_pass'] else 1


if __name__ == '__main__':
    sys.exit(main())
