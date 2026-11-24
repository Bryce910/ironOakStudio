#!/usr/bin/env python3
"""Verify the roster executable's exact isolated source and inherited hero."""
from pathlib import Path
import difflib
import hashlib
import json

HERE = Path(__file__).resolve().parent


def sha(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def require(value, reason):
    if not value:
        raise RuntimeError(reason)


def files(root):
    return {str(path.relative_to(root)): sha(path) for path in sorted(root.rglob('*'))
            if path.is_file()}


def main():
    inputs = {}

    def pin(path, expected=None):
        path = Path(path).resolve()
        actual = sha(path)
        require(expected is None or actual == expected, 'Changed source dependency: ' + str(path))
        inputs[str(path)] = actual
        return actual

    origin = json.loads((HERE / 'SOURCE_ORIGIN.json').read_text())
    baseline = Path(origin['baseline'])
    current = HERE / 'source-v1'
    before = files(baseline)
    after = files(current)
    require(before == origin['baseline_sha256'], 'Accepted baseline source changed')
    changed = {name for name in before.keys() & after.keys() if before[name] != after[name]}
    added = after.keys() - before.keys()
    removed = before.keys() - after.keys()
    require(changed == {'engine/world_viewer.cpp', 'engine/world_models.cpp',
                        'engine/world_combat_arena_runtime.inc', 'engine/world_arena_combat.h'},
            'Unexpected source mutation')
    require(added == {'engine/world_orc_roster.h', 'engine/world_orc_roster_qa.inc'} and
            not removed, 'Unexpected source additions/removals')
    native = json.loads((HERE / 'runtime-native-v1/READY.json').read_text())
    require(native['ready'] and native['source_sha256'] == after and native['native_only'] and
            native['Windows_work_performed'] is False and native['full_application_closure_rebuilt'],
            'Native executable was not compiled from the exact reviewed source closure')
    pin(native['binary'], native['sha256'])
    for name, digest in before.items():
        pin(baseline / name, digest)
    for name, digest in after.items():
        pin(current / name, digest)
    for path in ['SOURCE_ORIGIN.json', 'ROSTER_STAGE_ORIGIN.json', 'runtime-native-v1/READY.json',
                 'HERO_ASSET.json', 'GNUR_ROSTER_RELOCATION.json', 'source-v1/engine/world_orc_roster.h',
                 'roster-review/ROSTER_CONTRACT_TESTS.json', 'arena-project/arena.config',
                 'launch_arena_opponents_candidate.sh', 'review_orc_roster_source.py']:
        pin(HERE / path)
    hero = json.loads((HERE / 'HERO_ASSET.json').read_text())
    baseline_hero = json.loads((baseline.parent / 'HERO_ASSET.json').read_text())
    require(hero == baseline_hero, 'Existing player hero selection changed')
    pin(hero['path'], hero['sha256'])
    pin(hero['shoulder_fit_ready'])
    for name in ['loadout-rigs', 'pose-cook']:
        require((HERE / name).resolve() == (baseline.parent / name).resolve(),
                'Existing player/loadout bank differs from accepted motion stage')
    project = HERE / 'arena-project'
    require(project.is_dir() and not project.is_symlink() and
            (project / 'arena_orcs_roster').resolve() == HERE / 'arena_orcs_roster',
            'Private project does not resolve the new owned roster')
    roster = json.loads((HERE / 'arena_orcs_roster/ORC_ROSTER.json').read_text())
    ids = [row['id'] for row in roster['entries']]
    require(len(ids) == 14 and len(set(ids)) == 14, 'Source review requires the entire actual roster')
    pin(HERE / 'arena_orcs_roster/ORC_ROSTER.json')
    coherence_path = HERE / 'intake_roster/ANATOMY_ROSTER_COHERENCE.json'
    coherence = json.loads(coherence_path.read_text())
    require(coherence.get('passed') is True, 'Global anatomy-source coherence audit failed')
    pin(coherence_path)
    for field in ('inputs', 'dependencies_sha256', 'outputs_sha256', 'before_sha256', 'after_sha256'):
        for path, digest in coherence.get(field, {}).items():
            pin(path, digest)
    fidelity_path = HERE / 'roster-review/ORC04_SOURCE_FIDELITY_REVIEW.json'
    fidelity = json.loads(fidelity_path.read_text())
    require(fidelity.get('passed') is True, 'Detailed Orc source fidelity review failed')
    pin(fidelity_path)
    for field in ('inputs', 'dependencies_sha256', 'outputs_sha256'):
        for path, digest in fidelity.get(field, {}).items():
            pin(path, digest)
    diff = []
    for name in sorted(changed | set(added)):
        left = (baseline / name).read_text().splitlines(True) if name in before else []
        right = (current / name).read_text().splitlines(True)
        diff.extend(difflib.unified_diff(left, right, fromfile='baseline/' + name,
                                         tofile='roster/' + name))
    patch = HERE / 'roster-review/ROSTER_SOURCE.patch'
    patch.write_text(''.join(diff))
    pin(patch)
    report = dict(schema='valwood.orc-roster.source-review.v1', passed=True,
                  reviewed_ids=sorted(ids), binary_sha256=native['sha256'],
                  baseline_source_files=len(before), current_source_files=len(after),
                  changed_files=sorted(changed), added_files=sorted(added), removed_files=[],
                  independent_source=True, original_hero_unchanged=True,
                  inherited_player_loadout_and_pose_banks_unchanged=True,
                  Windows_work_performed=False, simultaneous_distinct_orc_types=False,
                  inputs=inputs,
                  reviewed_behavior=[
                      'A selected owned character supplies its own render, anatomical and actual weapon geometry.',
                      'Start loads the selected bank before allocating a run, applying that bank physical profile.',
                      'Measured body radius plus movement margin drives pair separation and player sweep clearance.',
                      'Only one detailed Orc bank resides at a time; failed replacement reloads the previous bank.',
                      'Native test hooks require explicit private QA environment plus bounded frame execution.',
                      'An explicit exact owned-weapon material marker opts the new genuine source into rigid certification.'])
    target = HERE / 'roster-review/ROSTER_SOURCE_REVIEW.json'
    target.write_text(json.dumps(report, indent=2, sort_keys=True) + '\n')
    print('PASS isolated native source, inherited player resources and complete14character roster')


if __name__ == '__main__':
    main()
