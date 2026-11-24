#!/usr/bin/env python3
"""Render the frozen Gnur bank through production motion and vertex code."""
from pathlib import Path
import hashlib
import json
import os
import subprocess
import time

STAGE = Path(__file__).resolve().parent
OUT = STAGE / 'opponent-render-review'
SCENARIOS = ('idle', 'walk', 'attack', 'guard', 'head', 'arm', 'leg', 'hands', 'crowd')


def sha(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def inputs():
    paths = [Path(__file__), STAGE / 'arena_orcs/ORC_ASSETS.json',
             STAGE / 'ORC_MATERIAL_MAPPING.json', OUT / 'PREVIEW_BUILD.json',
             OUT / 'preview_opponents', STAGE / 'runtime-native-v1/READY.json']
    paths += [p for folder in ('arena_orcs', 'arena-project/shaders')
              for p in (STAGE / folder).rglob('*') if p.is_file()]
    build = json.loads((OUT / 'PREVIEW_BUILD.json').read_text())
    for field in ('source_sha256', 'inputs', 'dependencies_sha256'):
        for name, expected in build.get(field, {}).items():
            p = Path(name)
            if not p.is_absolute():
                p = STAGE / name
            assert sha(p) == expected, ('Preview dependency changed', p)
            paths.append(p)
    return {str(p.resolve()): sha(p) for p in paths}


def main():
    OUT.mkdir(exist_ok=True)
    manifest = json.loads((STAGE / 'arena_orcs/ORC_ASSETS.json').read_text())
    assert manifest['source_name'] == 'Gnur'
    assert manifest['original_textures_present'] and not manifest['materials_provisional']
    before = inputs()
    environment = dict(os.environ, SDL_VIDEODRIVER='offscreen', SDL_AUDIODRIVER='dummy', LP_NUM_THREADS='4')
    results = []
    artifacts = {}
    for scenario in SCENARIOS:
        directory = OUT / scenario
        directory.mkdir(exist_ok=True)
        log = OUT / (scenario + '.log')
        start = time.monotonic()
        command = [str(OUT / 'preview_opponents'), str(STAGE / 'arena_orcs/body'),
                   str(STAGE / 'arena_orcs/anatomy'), str(STAGE / 'arena-project/shaders'),
                   str(directory), scenario]
        with log.open('w') as stream:
            done = subprocess.run(command, env=environment, stdout=stream, stderr=subprocess.STDOUT, timeout=240)
        print(scenario, 'exit', done.returncode, 'seconds', round(time.monotonic() - start, 2), flush=True)
        assert done.returncode == 0, log.read_text()[-3000:]
        rows = [json.loads(line) for line in log.read_text().splitlines() if line.startswith('{')]
        assert rows, ('No fixture result', scenario)
        row = rows[-1]
        assert row['passed'] and row['gl_error'] == 0 and row['updates'] == 240
        assert row['body_poses'] == 172 and row['arm_poses'] == 0 and row['body_only']
        assert row['captures'] == 7 and row['actors_peak'] >= (4 if scenario == 'crowd' else 1)
        images = {str(p): sha(p) for p in directory.glob(scenario + '-*.png')}
        assert len(images) == 7
        row.update(log=str(log), log_sha256=sha(log), images=images)
        artifacts[str(log)] = sha(log)
        artifacts.update(images)
        results.append(row)
    after = inputs()
    assert before == after, 'The bank or production dependencies changed during rendering'
    report = dict(schema='valwood.gnur.production-animation-render.v1', passed=True,
                  scripted_animation_fixture=True, interactive_gameplay_rehearsal=False,
                  physical_controller_test=False, hardware_performance_measured=False,
                  original_textures_present=True, materials_provisional=False,
                  visual_review_complete=False, before_sha256=before, after_sha256=after,
                  outputs_sha256=artifacts, scenarios=results)
    (OUT / 'ORC_ANIMATION_RENDER_REVIEW.json').write_text(json.dumps(report, indent=2, sort_keys=True) + '\n')
    print('All nine scripted production motion / geometry scenarios passed.', flush=True)


if __name__ == '__main__':
    main()
