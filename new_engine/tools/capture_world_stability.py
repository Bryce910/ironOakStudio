#!/usr/bin/env python3
"""Native, deterministic display-sequence QA; never use its readbacks as an FPS benchmark."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess

import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
SCENES = {'grove': 'grove', 'castle': 'castle-market', 'alley': 'castle-alley'}


def measure(stem):
    metadata = json.loads(Path(str(stem) + '.bmp.json').read_text())
    assert metadata['capture_ok'] and not metadata['offscreen']
    assert 'AMD Radeon RX 5300' in metadata['renderer']
    paths = [Path(f'{stem}-{i:03}.png') for i in range(32)]
    frames = np.stack([np.asarray(Image.open(p).convert('RGB'), dtype=np.float32) for p in paths])
    assert frames.shape == (32, 900, 1600, 3)
    luma = frames @ np.array([.2126, .7152, .0722], dtype=np.float32)
    # The entire frozen scene is static. Report both all pixels and the lower
    # scene, which excludes most of the low-frequency sky from the measurement.
    result = {}
    for name, region in [('full_frame', luma), ('lower_scene', luma[:, 300:, :])]:
        result[name] = {
            'mean_temporal_luma_variance': float(region.var(axis=0).mean()),
            'mean_consecutive_luma_change': float(np.abs(np.diff(region, axis=0)).mean()),
            'p95_temporal_luma_std': float(np.percentile(region.std(axis=0), 95)),
        }
    return result


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--phase', choices=['before', 'after'])
    p.add_argument('--scene', choices=list(SCENES))
    p.add_argument('--output', type=Path, default=ROOT / 'docs/performance/global-stability/frozen')
    p.add_argument('--compare', action='store_true')
    p.add_argument('--baseline-source', type=Path,
                   default=ROOT / 'docs/performance/global-stability/before-source',
                   help='Preserved source directory for before shader controls')
    a = p.parse_args()
    out = a.output.resolve()
    if a.compare:
        result = {'note': 'Identical frozen wind/cloud/resident time; Halton jitter remains active. Native display pixels, not FPS timings.', 'scenes': {}}
        scenes = [a.scene] if a.scene else [s for s in SCENES
            if (out / 'before' / (s + '.bmp.json')).is_file()
            and (out / 'after' / (s + '.bmp.json')).is_file()]
        if not scenes:
            p.error('No matching before/after scene captures')
        for scene in scenes:
            before = measure(out / 'before' / scene)
            after = measure(out / 'after' / scene)
            reductions = {k: 100 * (1 - after[k]['mean_temporal_luma_variance'] /
                max(before[k]['mean_temporal_luma_variance'], 1.e-12)) for k in before}
            result['scenes'][scene] = {'before': before, 'after': after,
                                     'variance_reduction_percent': reductions}
        (out / 'comparison.json').write_text(json.dumps(result, indent=2) + '\n')
        print(json.dumps(result, indent=2))
        return
    if not a.phase or not a.scene:
        p.error('--phase and --scene required for capture')
    stem = out / a.phase / a.scene
    stem.parent.mkdir(parents=True, exist_ok=True)
    env = os.environ.copy()
    env.update(SDL_AUDIODRIVER='dummy', VALWOOD_QA_FIXED_TIME='1',
               VALWOOD_FOLIAGE_SEQUENCE=str(stem), VALWOOD_FOLIAGE_SEQUENCE_FULL='1')
    env.pop('VALWOOD_QA_TEMPORAL_FRAGMENT', None)
    env.pop('VALWOOD_QA_FOLIAGE_FRAGMENT', None)
    if a.phase == 'before':
        shader = a.baseline_source.resolve() / 'shaders/world_temporal.fs.glsl'
        assert shader.is_file()
        env['VALWOOD_QA_TEMPORAL_FRAGMENT'] = str(shader)
        env['VALWOOD_QA_FOLIAGE_FRAGMENT'] = str(shader.parent / 'world_foliage.fs.glsl')
    cmd = [str(ROOT.parent / 'Launch_Valwood_World.sh'), '--scenic', SCENES[a.scene],
           '--width', '1600', '--height', '900', '--frames', '360', '--no-hud',
           '--capture', str(stem) + '.bmp']
    inputs = {}
    for name, path in {
        'executable': ROOT / 'rpg',
        'temporal_fragment': Path(env.get('VALWOOD_QA_TEMPORAL_FRAGMENT', ROOT / 'shaders/world_temporal.fs.glsl')),
        'foliage_fragment': Path(env.get('VALWOOD_QA_FOLIAGE_FRAGMENT', ROOT / 'shaders/world_foliage.fs.glsl')),
    }.items():
        inputs[name] = {'path': str(path), 'sha256': hashlib.sha256(path.read_bytes()).hexdigest()}
    Path(str(stem) + '.inputs.json').write_text(json.dumps(inputs, indent=2) + '\n')
    with stem.with_suffix('.log').open('w') as log:
        subprocess.run(cmd, cwd=ROOT, env=env, stdout=log, stderr=subprocess.STDOUT, check=True)
    meta = json.loads(Path(str(stem) + '.bmp.json').read_text())
    assert meta['capture_ok'] and not meta['offscreen'] and 'AMD Radeon RX 5300' in meta['renderer']
    for path in [stem.with_suffix('.bmp'), *[Path(f'{stem}-{i:03}.bmp') for i in range(32)]]:
        Image.open(path).save(path.with_suffix('.png'))
        # Preserve every raw pixel losslessly, without 138MB of duplicate BMPs.
        if path != stem.with_suffix('.bmp'):
            path.unlink()
    print(json.dumps({'phase': a.phase, 'scene': a.scene, 'metrics': measure(stem)}), flush=True)


if __name__ == '__main__':
    main()
