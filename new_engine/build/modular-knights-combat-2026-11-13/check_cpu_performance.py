#!/usr/bin/env python3
"""Compare pose CPU cost after all concurrent rendering/build work has stopped."""
from pathlib import Path
import hashlib
import json
import subprocess

BASE = Path(__file__).resolve().parent
ROOT = BASE.parents[1]
OLD = ROOT / 'build/modular-knights-locomotion-2026-11-13'
ASSET = ROOT / 'assets/purchased/modular_knights/derived/polish-v4.1/modular_knight_hero_v4_1.glb'
PINS = {}


def pin(path):
    with path.open('rb') as stream:
        PINS[str(path)] = hashlib.file_digest(stream, 'sha256').hexdigest()


def main():
    fixture = BASE / 'benchmark_locomotion.cpp'
    pin(fixture)
    pin(Path(__file__))
    pin(ASSET)
    results = {}
    for name, build in (('baseline', OLD), ('candidate', BASE)):
        source = build / 'source-v1'
        runtime = build / 'runtime-native-v1'
        binary = BASE / ('benchmark_' + name)
        objects = [runtime / 'native-objects/engine' / (unit + '.cpp.o')
                   for unit in ('world_skeletal_hero', 'world_animation')]
        for unit, obj in zip(('world_skeletal_hero', 'world_animation'), objects):
            receipt = json.loads(obj.with_suffix('.json').read_text())
            for filename, expected in receipt['actual_dependency_sha256'].items():
                path = Path(filename)
                pin(path)
                if PINS[str(path)] != expected:
                    raise RuntimeError('Stale benchmark object: ' + str(obj))
            pin(obj)
        command = ['/usr/bin/x86_64-linux-gnu-g++-13', '-O2', '-std=c++17',
                   '-I', str(source), str(fixture), *map(str, objects),
                   '-o', str(binary)]
        subprocess.run(command, check=True)
        pin(binary)
        result = json.loads(subprocess.check_output([str(binary), str(ASSET)], text=True))
        if result['pose_checksum'] == 0:
            raise RuntimeError('Benchmark did not evaluate poses')
        output = BASE / ('benchmark-' + name + '-final.json')
        output.write_text(json.dumps(result, indent=2) + '\n')
        pin(output)
        results[name] = result

    for filename, expected in PINS.items():
        with Path(filename).open('rb') as stream:
            if hashlib.file_digest(stream, 'sha256').hexdigest() != expected:
                raise RuntimeError('Benchmark input changed: ' + filename)
    worst_p95 = max(s['p95_us'] for s in results['candidate']['scenarios'].values())
    transition = results['candidate']['scenarios']['transitions']
    result = {
        'schema': 'valwood.knight-combat.cpu-cost.v1',
        'accepted': worst_p95 < 1000 and transition['p99_us'] < 5000,
        'scope': 'Single hero CPU pose evaluation, ten scenarios, 1200 measured frames each after 300 warmup frames; includes chained attacks, guard switches, hit reactions and movement transitions.',
        'budget_us': 1000,
        'budget_reason': 'At most 1 ms for this single hero pose pass; total renderer/game frame time is outside this fixture.',
        'maximum_candidate_p95_us': worst_p95,
        'transition_p99_budget_us': 5000,
        'transition_p99_us': transition['p99_us'],
        'transition_maximum_observed_us': transition['max_us'],
        'comparison': {name: {
            'baseline_p95_us': before['p95_us'],
            'candidate_p95_us': results['candidate']['scenarios'][name]['p95_us'],
            'added_p95_us': results['candidate']['scenarios'][name]['p95_us'] - before['p95_us'],
        } for name, before in results['baseline']['scenarios'].items()},
        'sha256': PINS,
        'limits': ['Sequential CPU timing on this host; no hardware gameplay FPS or GPU frame-pacing claim.'],
    }
    (BASE / 'CPU_PERFORMANCE_REVIEW.json').write_text(json.dumps(result, indent=2) + '\n')
    if not result['accepted']:
        raise RuntimeError('Hero pose CPU budget exceeded')
    print(json.dumps({k: result[k] for k in ('accepted', 'maximum_candidate_p95_us')}))


if __name__ == '__main__':
    main()
