#!/usr/bin/env python3
"""CPU layout checks and untimed, full-resolution apartment inspection views.

Inspection views deliberately select authored cameras. They are visual evidence,
not evidence of walking, collision or story completion. Use the existing
check_opening_prologue.py separately for the ordinary SDL-input journey.
"""
from __future__ import annotations

import argparse
import fcntl
import json
import math
from pathlib import Path
import subprocess
import time

from PIL import Image

from benchmark_graphics_settings import native_environment
from check_opening_prologue import ROOT, isolate_project, mutable_snapshot, sha
from opening_home_cinematic import home_render_meshes

EVIDENCE = ROOT/'docs/opening-apartment-refresh/2026-11-02'
ROUTE_SOURCE = EVIDENCE/'check_apartment_route.cpp'
MODEL_CHECKER = ROOT/'docs/opening-prologue/2026-11-02/environment/check_models'


def point(values):
    if not isinstance(values, list) or len(values) != 3 or not all(
            isinstance(v, (int, float)) and not isinstance(v, bool) and
            math.isfinite(v) and abs(v) <= 1e6 for v in values):
        raise ValueError('Invalid apartment coordinate')
    return list(map(float, values))


def route_input(scene):
    home = scene['home']
    points = [point(p) for p in home['walk_path']]
    if not 5 <= len(points) <= 128:
        raise ValueError('Apartment route must contain 5–128 points')
    if 'blocking_checks' in home:
        checks=home['blocking_checks']
        if not isinstance(checks,list) or len(checks)!=2 or any(len(x)!=2 for x in checks):
            raise ValueError('Provide exactly two authored blocking controls')
        rows=['VALWOOD_APARTMENT_ROUTE 1',f'points {len(points)}']
        rows.extend(' '.join(format(v,'.17g') for v in p) for p in points)
        rows.append('blocked 2')
        rows.extend(' '.join(format(v,'.17g') for v in point(a)+point(b)) for a,b in checks)
        return '\n'.join(rows)+'\n'
    # These negative controls use actual objective positions: the coffee table
    # must block a walk into its center and the closed front leaf must block exit.
    focus, door = point(home['inspect']['position']), point(home['door']['position'])
    inspect_from = point(home['inspect']['approach'])
    door_from = point(home['door']['approach'])
    dx, dz = door[0]-door_from[0], door[2]-door_from[2]
    distance = math.hypot(dx, dz)
    if distance < .01:
        raise ValueError('Front door approach has no direction')
    door_beyond = [door[0]+dx/distance*.7, door_from[1], door[2]+dz/distance*.7]
    negatives = [(inspect_from, [focus[0], inspect_from[1], focus[2]]), (door_from, door_beyond)]
    rows = ['VALWOOD_APARTMENT_ROUTE 1', f'points {len(points)}']
    rows.extend(' '.join(format(v, '.17g') for v in p) for p in points)
    rows.append('blocked 2')
    rows.extend(' '.join(format(v, '.17g') for v in a+b) for a, b in negatives)
    return '\n'.join(rows)+'\n'


def inspection_views(project, explicit=None):
    source = Path(explicit) if explicit else project/'opening/inspection.views.json'
    data = json.loads(source.read_text())
    if isinstance(data, dict):
        data = data['views']
    if not isinstance(data, list) or not 1 <= len(data) <= 8:
        raise ValueError('Provide 1–8 authored inspection cameras')
    result = []
    for item in data:
        name = item['name']
        if not isinstance(name, str) or not name or len(name) > 64 or any(
                not (c.isalnum() or c in '_-') for c in name):
            raise ValueError('Invalid inspection name')
        eye, target = point(item['eye']), point(item['target'])
        dx, dy, dz = [b-a for a, b in zip(eye, target)]
        if math.sqrt(dx*dx+dy*dy+dz*dz) < .1:
            raise ValueError('Inspection camera has no look direction')
        fov = item.get('fov', math.pi/3)
        if not isinstance(fov, (float, int)) or not math.isfinite(fov) or not .35 <= fov <= 1.5:
            raise ValueError('Inspection FOV is outside its bounds')
        result.append(dict(name=name, eye=eye, target=target, fov=fov,
                           yaw=math.atan2(dx, -dz), pitch=math.atan2(dy, math.hypot(dx, dz))))
    if len({v['name'] for v in result}) != len(result):
        raise ValueError('Duplicate inspection name')
    return result


def house_rail_views(project):
    """Three exact samples of the authored rail, without claiming full playback."""
    rows = [row.split()[1:] for row in (project/'opening.config').read_text().splitlines()
            if row.startswith('house_rail ')]
    if len(rows) != 1 or len(rows[0]) != 14:
        raise ValueError('Expected one bounded house rail')
    values = list(map(float, rows[0]))
    if not all(math.isfinite(v) for v in values):
        raise ValueError('Nonfinite house rail')
    result = []
    for name, progress in [('house-rail-start', 0.), ('house-rail-middle', .5), ('house-rail-handoff', 1.)]:
        t = progress**3*(progress*(progress*6.-15.)+10.)
        eye = [values[i]+(values[i+3]-values[i])*t for i in range(3)]
        target = [values[i+6]+(values[i+9]-values[i+6])*t for i in range(3)]
        dx, dy, dz = [b-a for a, b in zip(eye, target)]
        if math.sqrt(dx*dx+dy*dy+dz*dz) < .1:
            raise ValueError('House rail sample has no look direction')
        result.append(dict(name=name, eye=eye, target=target,
                      yaw=math.atan2(dx, -dz), pitch=math.atan2(dy, math.hypot(dx, dz)),
                      fov=values[12]+(values[13]-values[12])*t, authored_rail_progress=progress))
    return result


def cpu_checks(project, output):
    scene = json.loads((project/'opening.scene').read_text())
    route = output/'route.walk'
    route.write_text(route_input(scene))
    binary = output/'check-apartment-route'
    command = ['g++', '-std=c++17', '-O2', '-Wall', '-Wextra', '-Werror', '-I.',
               str(ROUTE_SOURCE), 'engine/world_physics.o', 'build/physics/libJolt.a',
               '-pthread', '-o', str(binary)]
    with (output/'route-build.log').open('w') as log:
        subprocess.run(command, cwd=ROOT, stdout=log, stderr=subprocess.STDOUT, check=True, timeout=120)
    complete = subprocess.run([str(binary), str(project/'opening/home.settings'), str(route)],
                              cwd=ROOT, capture_output=True, text=True, timeout=60)
    (output/'route.log').write_text(complete.stdout+complete.stderr)
    collision = json.loads(complete.stdout) if complete.stdout.strip() else {'passed': False}
    render_meshes = home_render_meshes(project, include_mid=True)
    models = [project/item['path'] for item in render_meshes]
    loaded = subprocess.run([str(MODEL_CHECKER), *map(str, models)], cwd=ROOT,
                            capture_output=True, text=True, timeout=180)
    (output/'models.log').write_text(loaded.stdout+loaded.stderr)
    model_result = json.loads(loaded.stdout) if loaded.stdout.strip() else {'passed': False}
    return dict(passed=complete.returncode == loaded.returncode == 0 and
                collision.get('passed') is True and model_result.get('passed') is True,
                collision=collision, models=model_result,
                collision_source_sha256=sha(ROUTE_SOURCE), collision_binary_sha256=sha(binary),
                model_checker_sha256=sha(MODEL_CHECKER),
                render_meshes=render_meshes,
                model_sha256={str(p.relative_to(project)): sha(p) for p in models})


def prepare_capture_project(project):
    """Match the opening's indoor look through existing authored controls.

    Studio sun strength is bounded to >=.25, while the opening runtime uses
    .16. In city mode the scene-linear direct radiance is studio.sun times the
    city.scene sun RGB; .25*(.16/.25)*RGB is exactly .16*RGB. All adjustments
    are private to the inspection copy and are recorded with the evidence.
    """
    removed = []
    for name in ('opening.config', 'opening.scene'):
        path = project/name
        if path.exists():
            path.unlink();removed.append(name)
    studio = project/'world.studio'
    rows = studio.read_text().splitlines()
    matches = [i for i, row in enumerate(rows) if row.startswith('look ')]
    if len(matches) != 1:
        raise ValueError('Inspection project needs one studio lighting record')
    fields = rows[matches[0]].split()
    if len(fields) != 11:
        raise ValueError('Unexpected studio look layout')
    original_look = fields.copy()
    fields[2:5] = ['.25', '.85', '1.12']
    rows[matches[0]] = ' '.join(fields)
    studio.write_text('\n'.join(rows)+'\n')
    scene = project/'city.scene'
    rows = scene.read_text().splitlines()
    matches = [i for i, row in enumerate(rows) if row.startswith('sun ')]
    if len(matches) != 1:
        raise ValueError('Inspection project needs one city sun-color record')
    values = list(map(float, rows[matches[0]].split()[1:]))
    if len(values) != 3 or not all(math.isfinite(v) and 0 <= v <= 16 for v in values):
        raise ValueError('Invalid city sun radiance')
    scaled = [v*(.16/.25) for v in values]
    rows[matches[0]] = 'sun '+' '.join(format(v, '.17g') for v in scaled)
    scene.write_text('\n'.join(rows)+'\n')
    return dict(removed_private_opening_markers=removed,
                original_studio_look=original_look, inspection_studio_look=fields,
                original_city_sun=values, inspection_city_sun=scaled,
                effective_opening_sun_strength=.16, sky_fill=.85, exposure=1.12,
                explanation='Exact city-mode direct-radiance product; private .25 sun control times .64 sun RGB matches opening .16 sun. Same practical lights and source shaders retained.')


def captures(binary, project, output, views, width, height, frames):
    # Source camera mode is explicitly distinct from the opening state machine.
    # Only this private copied text file is replaced; shared banks stay read-only.
    # EngineLaunch accepts registered scenic IDs. Map the human-readable art
    # names onto its existing nonwalking city-camera slots in this private copy.
    views = [dict(view, scenic_id=f'city-camera-{index+2:02d}')
             for index, view in enumerate(views)]
    lighting = prepare_capture_project(project)
    (output/'inspection-look.json').write_text(json.dumps(lighting, indent=2)+'\n')
    records = []
    for v in views:
        records.append(' '.join([v['scenic_id'],
                      *(format(x, '.17g') for x in v['eye']),
                      format(v['yaw'], '.17g'), format(v['pitch'], '.17g'), format(v['fov'], '.17g')]))
    (project/'showcase.views').write_text('\n'.join(records)+'\n')
    environment, removed = native_environment()
    for name in ('VALWOOD_VILLAGE_LIGHTING', 'VALWOOD_OPENING_PROLOGUE',
                 'VALWOOD_PLAYTEST_SLICE', 'VALWOOD_SHOWCASE'):
        environment.pop(name, None)
    environment['VALWOOD_PRESENTATION'] = '1'
    results = []
    with (ROOT/'projects/seldonne/.valwood-instance.lock').open('a') as lock:
        fcntl.flock(lock, fcntl.LOCK_EX|fcntl.LOCK_NB)
        for view in views:
            folder = output/view['name'];folder.mkdir()
            profile, image = folder/'profile.json', folder/'view.bmp'
            command = [str(binary), '--project', str(project), '--play', '--no-hud',
                       '--scenic', view['scenic_id'], '--width', str(width),
                       '--height', str(height), '--auto-quality', 'off', '--render-scale', '1',
                       '--frames', str(frames), '--warmup', '60', '--profile', str(profile),
                       '--capture', str(image)]
            (folder/'command.json').write_text(json.dumps(dict(command=command,
                 removed_environment=removed, camera=view, scope='Authored stationary visual inspection; not gameplay acceptance'), indent=2)+'\n')
            started = time.monotonic()
            with (folder/'render.log').open('w') as log:
                try:
                    code = subprocess.run(command, cwd=ROOT, env=environment, stdout=log,
                                          stderr=subprocess.STDOUT, timeout=300).returncode
                except subprocess.TimeoutExpired:
                    code = -1
            metrics = json.loads(profile.read_text()) if profile.exists() else {}
            world_file = Path(str(profile)+'.world.json')
            world = json.loads(world_file.read_text()) if world_file.exists() else {}
            failures = []
            renderer = str(metrics.get('renderer', ''))
            if code:failures.append(f'process exit {code}')
            if not renderer or any(v in renderer.lower() for v in ('llvmpipe', 'softpipe', 'swrast')):
                failures.append('Hardware renderer missing')
            if world.get('scenic') != view['scenic_id']:
                failures.append('Authored camera was not selected')
            camera = world.get('camera_final', {})
            expected_camera = dict(zip(('x', 'y', 'z'), view['eye']))
            expected_camera.update(yaw=view['yaw'], pitch=view['pitch'])
            if any(not isinstance(camera.get(key), (float, int)) or abs(camera[key]-value) > 1e-4
                   for key, value in expected_camera.items()):
                failures.append('Rendered camera moved away from its authored inspection pose')
            viewport = world.get('designer_viewport', {})
            if viewport.get('scene_width') != width or viewport.get('scene_height') != height:
                failures.append('Scene was not rendered at the requested native resolution')
            if world.get('gl_error') != 0 or world.get('remaining_texture_references') != 0:
                failures.append('Renderer GL/texture reference error')
            if world.get('asset_streaming', {}).get('failed') != 0:
                failures.append('Asset streaming failure')
            look = world.get('lighting', {})
            if any(not isinstance(look.get(key), (float, int)) or abs(look[key]-value) > 1e-5
                   for key, value in (('sun_strength', .25), ('sky_fill', .85), ('exposure', 1.12))):
                failures.append('Inspection lighting differs from the recorded opening equivalent')
            if not image.is_file():
                failures.append('Native rendered capture missing')
            else:
                with Image.open(image) as pixels:
                    if pixels.size != (width, height):failures.append('Capture dimensions differ')
                    pixels.convert('RGB').save(image.with_suffix('.png'))
                image.unlink()
            results.append(dict(name=view['name'], scenic_id=view['scenic_id'], passed=not failures, failures=failures,
                           elapsed_seconds=round(time.monotonic()-started, 3), renderer=renderer,
                           png=str(image.with_suffix('.png').relative_to(output)), width=width,
                           height=height, render_scale=1., scope='Authored stationary inspection; not an FPS benchmark or gameplay acceptance'))
    return dict(passed=all(v['passed'] for v in results), views=results, lighting=lighting)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--project', type=Path, default=ROOT/'build/opening-prologue/project')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--mode', choices=['cpu', 'captures', 'both'], default='cpu')
    parser.add_argument('--binary', type=Path)
    parser.add_argument('--views', type=Path)
    parser.add_argument('--include-house-rail', action='store_true',
                        help='Also inspect exact start/middle/handoff rail compositions; not continuous playback')
    parser.add_argument('--width', type=int, default=1600)
    parser.add_argument('--height', type=int, default=900)
    parser.add_argument('--frames', type=int, default=180)
    args = parser.parse_args()
    if args.mode != 'cpu' and not args.binary:parser.error('--binary is required for captures')
    if not 1280 <= args.width <= 1920 or not 720 <= args.height <= 1080 or not 120 <= args.frames <= 600:
        parser.error('Inspection resolution/frame bounds exceeded')
    source = args.project.resolve(strict=True)
    output = args.output.resolve()
    if output.is_relative_to(source):parser.error('Evidence must be outside the playable project')
    output.mkdir(parents=True, exist_ok=False)
    before = mutable_snapshot(source)
    project = output/'project'
    isolate_project(source, project)
    result = dict(schema='valwood.opening-apartment-review', version=1, source=str(source),
                  scope='Production CPU model/capsule checks and optional authored inspection cameras. Separate ordinary SDL opening acceptance is required.')
    if args.mode in ('cpu', 'both'):
        result['cpu'] = cpu_checks(project, output)
    if args.mode in ('captures', 'both'):
        views = inspection_views(source, args.views)
        if args.include_house_rail:
            views += house_rail_views(source)
        if len(views) > 8 or len({view['name'] for view in views}) != len(views):
            raise ValueError('Too many or duplicate authored inspection views')
        binary = args.binary.resolve(strict=True)
        digest = sha(binary)
        result['native_views'] = captures(binary, project, output, views, args.width, args.height, args.frames)
        result['binary_sha256'] = digest
        result['binary_unchanged'] = sha(binary) == digest
    result['source_unchanged'] = before == mutable_snapshot(source)
    result['passed'] = result['source_unchanged'] and result.get('binary_unchanged', True) and all(
        result[name]['passed'] for name in ('cpu', 'native_views') if name in result)
    (output/'results.json').write_text(json.dumps(result, indent=2)+'\n')
    print(json.dumps(result, indent=2))
    return 0 if result['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
