#!/usr/bin/env python3
"""Compile the real horizon selector and an otherwise identical full-scan control."""
from pathlib import Path
import argparse
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--benchmark', action='store_true', help='Also time isolatedCPU selection; noGPU renderer is used.')
    args = parser.parse_args()
    source = (ROOT / 'engine/world_viewer.cpp').read_text()
    start = source.index('    struct HorizonForest {')
    end = source.index('    struct CoastalOutcrops {', start)
    actual = source[start:end]
    begin = actual.index('            // Horizon grid broad phase begin:')
    stop = actual.index('            for(size_t i=0;i<live.size();i++) {', begin)
    legacy = actual[:begin] + actual[stop:]
    skip = '                if(!spatial_grid.includes(i))continue;\n'
    assert actual.count(skip) == legacy.count(skip) == 1
    legacy = legacy.replace(skip, '').replace('struct HorizonForest {', 'struct LegacyHorizonForest {', 1)
    with tempfile.TemporaryDirectory(prefix='valwood-horizon-grid-') as directory:
        tmp = Path(directory)
        (tmp / 'horizon_grid_viewer.inc').write_text(actual + '\n' + legacy)
        obj = tmp / 'atlas.o'
        subprocess.run(['cc', '-std=c99', '-O2', '-Wall', '-Wextra', '-Werror', '-I'+str(ROOT), '-c', str(ROOT/'engine/world_atlas.c'), '-o', str(obj)], check=True)
        command = ['c++', '-std=c++17', '-O2', '-Wall', '-Wextra', '-Werror', '-I'+str(ROOT), '-I'+str(tmp), str(ROOT/'tests/world_horizon_grid_test.cpp'), str(obj), '-lm', '-o', str(tmp/'test')]
        subprocess.run(command, check=True)
        subprocess.run([str(tmp/'test')] + (['--benchmark'] if args.benchmark else []), check=True)


if __name__ == '__main__':
    main()
