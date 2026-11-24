#!/usr/bin/env python3
from pathlib import Path
import difflib
import hashlib
import json

N = Path(__file__).resolve().parent
B = N.parent / 'arena-orc-roster-2026-11-14'
P = N / 'feedback-review'
CHANGED = {
    'engine/world_training_combat.h', 'engine/world_training_combat.cpp',
    'engine/world_skeletal_hero.h', 'engine/world_skeletal_hero.cpp',
    'engine/world_combat_motion.cpp', 'engine/world_combat_vfx.cpp',
    'engine/world_viewer.cpp', 'engine/world_combat_arena_runtime.inc',
    'engine/world_arena_qa.inc', 'engine/world_first_journey_runtime.inc',
}
ADDED = {
    'engine/world_combat_feedback.h', 'engine/world_combat_feedback_qa.inc',
    'tests/world_combat_feedback_test.cpp',
    'tests/world_combat_contact_feedback_vfx_test.cpp',
}


def sha(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def inventory(root):
    return {str(p.relative_to(root)): sha(p) for p in root.rglob('*') if p.is_file()}


old, new = inventory(B / 'source-v1'), inventory(N / 'source-v1')
changed = {p for p in old.keys() & new.keys() if old[p] != new[p]}
assert changed == CHANGED, changed ^ CHANGED
assert new.keys() - old.keys() == ADDED
assert not old.keys() - new.keys()
ready = json.loads((N / 'runtime-native-v1/READY.json').read_text())
assert ready['ready'] and ready['native_only'] and not ready['Windows_work_performed']
assert ready['source_sha256'] == new
assert ready['application_units'] == 103
assert sha(ready['binary']) == ready['sha256']
origin = json.loads((N / 'SOURCE_ORIGIN.json').read_text())
assert origin['baseline_sha256'] == old
assert origin['accepted_roster_sha256'] == sha(B / 'ORC_ROSTER_READY.json')
assert (N / 'arena-project/arena_orcs_roster').resolve() == (B / 'arena_orcs_roster').resolve()
assert (N / 'arena-project/models').resolve() == (B / 'arena-project/models').resolve()
assert sha(N / 'HERO_ASSET.json') == sha(B / 'HERO_ASSET.json')
inputs = {str((B / 'source-v1' / p).resolve()): h for p, h in old.items()}
inputs.update({str((N / 'source-v1' / p).resolve()): h for p, h in new.items()})
for path in [Path(__file__), N / 'SOURCE_ORIGIN.json', N / 'STAGE_ORIGIN.json',
             B / 'ORC_ROSTER_READY.json', N / 'runtime-native-v1/READY.json',
             Path(ready['binary']), N / 'HERO_ASSET.json']:
    inputs[str(path.resolve())] = sha(path)
diff = []
for name in sorted(changed | ADDED):
    a = (B / 'source-v1' / name).read_text().splitlines(keepends=True) if name in old else []
    z = (N / 'source-v1' / name).read_text().splitlines(keepends=True)
    diff.extend(difflib.unified_diff(a, z, fromfile='accepted/' + name, tofile='feedback/' + name))
patch = P / 'SOURCE.patch'
patch.write_text(''.join(diff))
report = dict(schema='valwood.combat-feedback.source-review.v1', passed=True,
              changed=sorted(changed), added=sorted(ADDED), removed=[],
              inputs=inputs, outputs_sha256={str(patch): sha(patch)},
              all_four_hero_rigs_and_fourteen_orc_banks_inherited=True,
              native_only=True, Windows_work_performed=False,
              findings=[
                  'Training changes append a separate miss event/outcome latch and move only the presentation swoosh event; physical contact selection, damage, stamina, difficulty, committed attack durations and rewards remain unchanged.',
                  'The current player rig adds a post-steel withdrawal while its contact sampler retains the original entry, momentum and pre-impact reaction state.',
                  'A single serial-deduplicated policy owns cues; ordinary arena and opening HUD paths share it, while inactive private QA hooks require bounded frames and a private marker.',
                  'Owned model, texture, animation-bank and source-asset files are shared with the accepted roster; no asset rebake or Windows build is involved.'])
(P / 'SOURCE_REVIEW.json').write_text(json.dumps(report, indent=2, sort_keys=True) + '\n')
print('PASS source review: ten focused changes, four additions, full native closure, original assets retained')
