#!/usr/bin/env python3
"""Carry final C9 save/preferences bytes into unbound C10 before source freeze.

Run only after root has closed the exact live C9 session. No source project,
accepted build, launcher, model bank, or immutable authored data is written.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import sys

ROOT = Path(__file__).resolve().parents[2]
HERE = Path(__file__).resolve().parent
SOURCE = HERE / 'candidate-v9'
CANDIDATE = HERE / 'candidate-v10'
BUILD = HERE / 'runtime-v9'
sys.path.insert(0, str(ROOT / 'tools'))
from check_opening_prologue import mutable_snapshot


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def atomic(path, payload):
    temporary = path.with_name(path.name + '.user-state-refresh.tmp')
    with temporary.open('xb') as stream:
        stream.write(payload)
        stream.flush()
        os.fsync(stream.fileno())
    os.replace(temporary, path)


def dump(path, value):
    atomic(path, (json.dumps(value, indent=2, allow_nan=False) + '\n').encode())


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-closed', action='store_true', required=True)
    parser.parse_args()
    assert not (BUILD / 'SOURCE_FREEZE.json').exists(), 'Refresh must precede freeze'
    report = CANDIDATE / 'USER_STATE_REFRESH.json'
    assert not report.exists(), 'One guarded refresh only'
    project, source = CANDIDATE / 'project', SOURCE / 'project'
    origin_path, prep_path = CANDIDATE / 'local-origin.json', CANDIDATE / 'PREPARATION.json'
    origin, prep = json.loads(origin_path.read_text()), json.loads(prep_path.read_text())
    source_origin = json.loads((SOURCE / 'local-origin.json').read_text())
    assert origin['runtime'] is None and origin['data_revision'] == 10
    existing_saves = {p.relative_to(project).as_posix(): sha(p)
                      for p in sorted((project / 'saves').rglob('*')) if p.is_file()}
    assert existing_saves == origin['copied_user_saves_sha256'], 'C10 save snapshot changed; do not overwrite it'
    changed = {name for name, digest in origin['generated_sha256'].items()
               if digest != source_origin['generated_sha256'][name]}
    revision = origin['authored_data_revision']
    assert revision['root_authorized'] and changed == {'opening/cinematic.settings'}
    assert revision['changed_files'] == sorted(changed)
    assert revision['source_sha256'] == source_origin['generated_sha256']['opening/cinematic.settings']
    assert revision['sha256'] == origin['generated_sha256']['opening/cinematic.settings']
    for name, digest in origin['generated_sha256'].items():
        assert sha(project / name) == digest, name
        assert sha(source / name) == source_origin['generated_sha256'][name], name
    before = mutable_snapshot(source)
    save_files = sorted(p for p in (source / 'saves').rglob('*') if p.is_file())
    assert all(not p.is_symlink() and p.stat().st_size <= 16 * 1024 * 1024 for p in save_files)
    payloads = {p.relative_to(source).as_posix(): p.read_bytes() for p in save_files}
    existing = {p.relative_to(project).as_posix() for p in (project / 'saves').rglob('*') if p.is_file()}
    assert existing <= payloads.keys(), 'Source removed a save; preserve previous copy for review'
    for name in ['graphics.settings', 'studio-layout.ini']:
        p = source / name
        if p.is_file():
            assert not p.is_symlink() and p.stat().st_size <= 1024 * 1024
            payloads[name] = p.read_bytes()
    assert mutable_snapshot(source) == before, 'Source changed during read'
    prior = dict(origin=json.loads(origin_path.read_text()), preparation=json.loads(prep_path.read_text()),
                 copied_save_sha256=origin['copied_user_saves_sha256'])
    for name, payload in payloads.items():
        target = project / name
        assert not target.is_symlink()
        target.parent.mkdir(parents=True, exist_ok=True)
        atomic(target, payload)
    assert mutable_snapshot(source) == before, 'Source changed after closed-session guard'
    copied = {name: hashlib.sha256(payload).hexdigest() for name, payload in payloads.items() if name.startswith('saves/')}
    for name, digest in copied.items():
        assert sha(source / name) == sha(project / name) == digest
    proofs = {name: sha(SOURCE / name) for name in ['local-origin.json', 'DATA_REVISION.json', 'PREPARATION.json']}
    origin.update(source_guard=before, source_origin_sha256=proofs['local-origin.json'],
                  copied_user_saves_sha256=copied, private_saves_empty=not copied,
                  source_saves_copied=True)
    dump(origin_path, origin)
    prep.update(source_snapshot=before, source_proofs_sha256=proofs,
                source_origin_sha256=proofs['local-origin.json'], user_saves_preserved=copied,
                saves_empty=not copied, origin_sha256=sha(origin_path), final_user_state_refreshed=True)
    dump(prep_path, prep)
    dump(report, dict(passed=True, source_closed_asserted_by_root=True, before=prior,
                      source_unchanged=True, source_snapshot=before, copied_user_saves_sha256=copied,
                      origin_sha256=sha(origin_path), preparation_sha256=sha(prep_path),
                      immutable_authored_files_unchanged=True, activation_performed=False, gpu_used=False))
    integration_path = BUILD / 'INTEGRATION_INPUTS.json'
    integration = json.loads(integration_path.read_text())
    assert not integration['ready']
    integration['journey_scene_files_sha256'][str(prep_path)] = sha(prep_path)
    integration['journey_scene_files_sha256'][str(report)] = sha(report)
    dump(integration_path, integration)
    build_prep_path = BUILD / 'PREPARATION.json'
    build_prep = json.loads(build_prep_path.read_text())
    build_prep.update(candidate_preparation_sha256=sha(prep_path), candidate_origin_sha256=sha(origin_path),
                      current_user_saves_preserved=copied, first_journey_scene_input_count=len(integration['journey_scene_files_sha256']))
    dump(build_prep_path, build_prep)
    print(json.dumps(dict(passed=True, copied_user_saves_sha256=copied, source_modified=False, source_freeze_present=False)))


if __name__ == '__main__':
    main()
