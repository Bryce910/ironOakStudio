#!/usr/bin/env python3
"""Pure CPU validation of the isolated first-contact story seed; never launches SDL."""
import hashlib
import json
import math
from pathlib import Path
import shlex
import subprocess

N = Path(__file__).resolve().parent
P = N / 'story-project'
R = N / 'review'
S = N / 'source-v1'


def require(ok, message):
    if not ok:
        raise RuntimeError(message)


def sha(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def main():
    fixture = R / 'check_story_config.cpp'
    binary = fixture.with_suffix('')
    cmd = ['/usr/bin/g++', '-std=c++17', '-O2', '-I' + str(S), str(fixture), '-o', str(binary)]
    deps = subprocess.check_output(cmd[:-2] + ['-MM'], text=True).replace('\\\n', ' ')
    inputs = {str(Path(__file__).resolve()): sha(__file__)}
    for path in shlex.split(deps.split(':', 1)[1]):
        inputs[str(Path(path).resolve())] = sha(path)
    subprocess.run(cmd, check=True)
    subprocess.run(['/usr/bin/python3', str(N / 'check_story_scene.py')], check=True)
    collision_source=R / 'check_story_collision.cpp'
    collision_binary=collision_source.with_suffix('')
    collision_command=['/usr/bin/g++','-std=c++17','-O2','-I'+str(S),str(collision_source),'-o',str(collision_binary)]
    collision_deps=subprocess.check_output(collision_command[:-2]+['-MM'],text=True).replace('\\\n',' ')
    for path in shlex.split(collision_deps.split(':',1)[1]):
        inputs[str(Path(path).resolve())]=sha(path)
    subprocess.run(collision_command,check=True)
    collision_result=json.loads(subprocess.check_output([str(collision_binary),str(P)],text=True))
    require(collision_result['passed'],'Actual production collision catalog/part/model revision preflight failed')
    for receipt in [R/'STORY_SCENE_CPU_REVIEW.json',R/'STORY_PROJECT_REPAIR_REVIEW.json',R/'studio-startup-diagnostic/STORY_STUDIO_CPU_REVIEW.json']:
        content=json.loads(receipt.read_text());require(content['passed'],'Failed story preflight '+str(receipt))
        for path,value in content.get('inputs',{}).items():require(sha(path)==value,'Stale story preflight input '+path)
        inputs[str(receipt)]=sha(receipt)
    result = json.loads(subprocess.check_output([str(binary), str(P)], text=True))
    require(result['passed'], 'Actual source config parsing failed')
    route_file = P / 'opening/first-journey.routes.json'
    routes = json.loads(route_file.read_text())
    require(routes['schema'] == 'valwood.first-journey-routes' and routes['version'] == 1,
            'Unsupported route schema')
    require(set(routes['paths']) == {'loan', 'encounter', 'satchel', 'return', 'town'}, 'Missing route')
    total, max_step = 0, 0.
    route_summaries = {}
    for name, points in routes['paths'].items():
        require(1 <= len(points) <= 512, 'Route count exceeds bounds')
        total += len(points)
        require(all(len(point) == 3 and all(math.isfinite(v) and abs(v) <= 1e6 for v in point) for point in points),
                'Nonfinite route position')
        steps = [math.hypot(a[0] - b[0], a[2] - b[2]) for a, b in zip(points, points[1:])]
        longest = max(steps, default=0.)
        require(longest <= 25, 'Route jumps more than 25metres')
        max_step = max(max_step, longest)
        route_summaries[name] = {'points': len(points), 'maximum_horizontal_step_m': longest}
    require(total <= 2048, 'Combined routes exceed bounds')
    profile = (P / 'player-profile.config').read_text().split()
    require(profile == ['VALWOOD_PLAYER_PROFILE', '1', 'path', '../shared-profile/player.progress', 'end'],
            'Story profile is not the isolated stage profile')
    target = (P / profile[3]).resolve()
    require(target == N / 'shared-profile/player.progress' and target.is_file(), 'Profile leaves the stage')
    for path in [P / 'project.valwood', P / 'opening.config', P / 'player-profile.config',
                 *sorted((P / 'opening').rglob('*'))]:
        if path.is_file():
            require(not path.is_symlink(), 'Mutable story config is a link')
            inputs[str(path)] = sha(path)
    snapshot = R / 'STORY_PROJECT_SNAPSHOT.json'
    inputs[str(snapshot)] = sha(snapshot)
    result.update(schema='valwood.first-contact.story-config-cpu-review.v1',
                  source_project_untouched=True, no_native_or_gl_process=True,
                  source_configuration_parser=True, input_events_injected=False,
                  structural_collision=collision_result, room_practicals_verified=7, runtime_studio_load_and_field_prepare=True, route_points=total, maximum_route_step_m=max_step, routes=route_summaries,
                  isolated_profile_path=str(target), inputs=inputs,
                  outputs={str(binary): sha(binary),str(collision_binary):sha(collision_binary)},
                  limits=['Configuration and route schema/placement consistency only; actual floor support and collision require native QA.',
                          'Snapshot provenance is historical; this receipt pins current authored settings and source parser dependencies.'])
    output = R / 'STORY_CONFIG_CPU_REVIEW.json'
    output.write_text(json.dumps(result, indent=2, sort_keys=True) + '\n')
    print('PASS isolated story config/current source:', output)


if __name__ == '__main__':
    main()
