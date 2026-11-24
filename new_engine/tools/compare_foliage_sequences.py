#!/usr/bin/env python3
"""Compare consecutive, unmodified native display crops (wind stays enabled).

These are image-change measurements, not a separation of wind from shimmer.
Use the foliage and temporal GL regression fixtures for isolated correctness.
Enable VALWOOD_FOLIAGE_SEQUENCE=/absolute/prefix on a fixed 300-frame launch;
the world renderer captures its last32 frames before drawing the HUD.
Never enable sequence readbacks while measuring FPS.
"""
import argparse
import json
from pathlib import Path

import numpy as np
from PIL import Image


def measure(directory, name):
    prefix = directory / name
    metadata = json.loads(Path(str(prefix) + '.bmp.json').read_text())
    assert metadata['capture_ok'] and not metadata['offscreen']
    assert metadata['renderer'].startswith('AMD Radeon RX 5300')
    files = [Path(f'{prefix}-{i:03}.bmp') for i in range(32)]
    images = np.stack([np.asarray(Image.open(f).convert('RGB'), dtype=np.float32) for f in files])
    assert images.shape == (32, 360, 960, 3)
    luma = images @ np.array([.2126, .7152, .0722], dtype=np.float32)
    return {
        'frames': len(files), 'crop_width': 960, 'crop_height': 360,
        'mean_temporal_luma_variance': float(luma.var(axis=0).mean()),
        'mean_consecutive_luma_change': float(np.abs(np.diff(luma, axis=0)).mean()),
        'p95_temporal_luma_std': float(np.percentile(luma.std(axis=0), 95)),
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--directory', type=Path, required=True)
    args = parser.parse_args()
    output = {'note': 'Native display crops including wind and camera motion; not pure flicker variance.', 'scenes': {}}
    for scene in ['grove-static', 'grove-moving']:
        before = measure(args.directory / 'before', scene)
        after = measure(args.directory / 'after', scene)
        output['scenes'][scene] = {
            'before': before, 'after': after,
            'temporal_variance_reduction_percent': 100 * (1 - after['mean_temporal_luma_variance'] / before['mean_temporal_luma_variance']),
            'consecutive_change_reduction_percent': 100 * (1 - after['mean_consecutive_luma_change'] / before['mean_consecutive_luma_change']),
        }
    (args.directory / 'sequence-comparison.json').write_text(json.dumps(output, indent=2) + '\n')
    print(json.dumps(output, indent=2))


if __name__ == '__main__':
    main()
