#!/usr/bin/env python3
"""CPU-only mock-GL regression of the actual viewer query ring and begin gate."""
from pathlib import Path
import os
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
source = (ROOT / 'engine/world_graphics_viewer.inc').read_text()
start = source.index('struct AdaptiveGPUQueries {')
end = source.index('\nbool graphics_panel(', start)
ring = source[start:end]
viewer = (ROOT / 'engine/world_viewer.cpp').read_text()
gate = re.findall(r'^\s*if\(graphics\.auto_quality\)adaptive_queries\.begin\(frame,quality_epoch,0\.,quality_eligible\);$', viewer, re.M)
assert len(gate) == 1, 'Production manual/automatic query gate must be explicit and unique'
with tempfile.TemporaryDirectory(prefix='valwood-adaptive-queries-') as temporary:
    tmp = Path(temporary)
    (tmp / 'adaptive_query_under_test.inc').write_text(ring)
    (tmp / 'adaptive_query_begin_gate.inc').write_text(gate[0] + '\n')
    binary = tmp / 'test'
    subprocess.run([os.environ.get('CXX', 'c++'), '-std=c++17', '-O2', '-Wall', '-Wextra', '-Werror',
                    '-I', str(ROOT), '-I', str(tmp), str(ROOT / 'tests/world_adaptive_queries_test.cpp'),
                    '-o', str(binary)], check=True, cwd=ROOT)
    subprocess.run([str(binary)], check=True, cwd=ROOT)
