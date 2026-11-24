#!/usr/bin/env python3
"""Bind candidate checks to a completed build; never grants native acceptance."""
from pathlib import Path
import hashlib
import json

N = Path(__file__).resolve().parents[1]
sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
receipt = json.loads((N / 'runtime-native-v1/BUILD_RECEIPT.json').read_text())
assert sha(N / 'runtime/rpg') == receipt['binary_sha256']
for relative, digest in receipt['source_sha256'].items():
    assert sha(N / 'source-v1' / relative) == digest, relative
ready = json.loads((N / 'READY_INPUTS.json').read_text())
plan = json.loads((N / 'review/PUBLISH_PLAN.json').read_text())
paths = set(ready['artifacts'])
paths.update('world-project/' + row['relative_path'] for row in plan['files'])
paths.update(('battlefield-motion/captures.glb', 'GUIDE.txt', 'launch.py',
              'vegetation/creek_dressing_instances.txt',
              'check_unified.py', 'tools/publish_candidate.py',
              'tools/refresh_candidate_inputs.py'))
artifacts = {relative: sha(N / relative) for relative in sorted(paths)}
ready.update(binary_sha256=receipt['binary_sha256'],
             source_sha256=receipt['source_sha256'], artifacts=artifacts)
(N / 'READY_INPUTS.json').write_text(json.dumps(ready, indent=2) + '\n')
proposed = json.loads((N / 'review/BASELINE_MANIFEST.json').read_text())
proposed.update(runtime_generation=str(N), binary=str(N / 'runtime/rpg'),
                source=str(N / 'source-v1'), shared_shaders=str(N / 'shared/shaders'),
                guide=str(N / 'GUIDE.txt'), binary_sha256=receipt['binary_sha256'],
                battlefield_revision={'id': N.name, 'guide': str(N / 'GUIDE.txt'),
                                      'review': str(N / 'review/NATIVE_ACCEPTANCE.json')})
(N / 'CURRENT_VALWOOD.proposed.json').write_text(json.dumps(proposed, indent=2) + '\n')
print('Candidate inputs bound:', receipt['binary_sha256'], len(artifacts), 'artifacts')
print('Native acceptance and publication remain separate requirements.')
