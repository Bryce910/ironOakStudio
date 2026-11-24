#!/usr/bin/env python3
"""Run CPU differential validation; use --benchmark only with an isolated lane."""
from pathlib import Path
import argparse
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--benchmark', action='store_true')
args = parser.parse_args()
with tempfile.TemporaryDirectory(prefix='valwood-grass-transition-linear-') as directory:
    tmp = Path(directory)
    obj = tmp/'atlas.o'
    subprocess.run(['cc', '-std=c99', '-O2', '-Wall', '-Wextra', '-Werror', '-I'+str(ROOT), '-c', str(ROOT/'engine/world_atlas.c'), '-o', str(obj)], check=True)
    subprocess.run(['c++', '-std=c++17', '-O2', '-Wall', '-Wextra', '-Werror', '-I'+str(ROOT), str(ROOT/'tests/world_grass_transition_differential_test.cpp'), str(ROOT/'engine/world_cover.cpp'), str(obj), '-lm', '-o', str(tmp/'test')], check=True)
    subprocess.run(['c++', '-std=c++17', '-O2', '-Wall', '-Wextra', '-Werror', '-I'+str(ROOT), str(ROOT/'tests/world_grass_transition_radix_edges_test.cpp'), '-o', str(tmp/'edges')], check=True)
    subprocess.run([str(tmp/'edges')], check=True)
    subprocess.run([str(tmp/'test')] + (['--benchmark'] if args.benchmark else []), check=True)
