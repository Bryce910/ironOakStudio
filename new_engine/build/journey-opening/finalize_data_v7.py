#!/usr/bin/env python3
"""Bind fresh C7 data to finalized R6 without changing either project or origin."""
from pathlib import Path
import hashlib
import json
import sys

ROOT = Path(__file__).resolve().parents[2]
HERE = Path(__file__).resolve().parent
SOURCE = HERE / 'candidate-v6'
CANDIDATE = HERE / 'candidate-v7'
BUILD = HERE / 'runtime-v6'
sys.path.insert(0, str(ROOT / 'tools'))
from check_opening_prologue import mutable_snapshot


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def reference(path):
    return dict(path=str(path), sha256=sha(path))


def main():
    destination = CANDIDATE / 'DATA_REVISION.json'
    assert not destination.exists(), 'Immutable proof already exists'
    ready = json.loads((BUILD / 'READY.json').read_text())
    review = json.loads((BUILD / 'BUILD_REVIEW.json').read_text())
    integration = json.loads((BUILD / 'INTEGRATION_INPUTS.json').read_text())
    origin_path = CANDIDATE / 'local-origin.json'
    origin = json.loads(origin_path.read_text())
    prep = json.loads((CANDIDATE / 'PREPARATION.json').read_text())
    source_proof = json.loads((SOURCE / 'DATA_REVISION.json').read_text())
    project = CANDIDATE / 'project'
    assert ready['ready'] and ready['version'] == 6
    assert review['compiled'] and review['accepted_inputs_unchanged']
    assert integration['private_project'] == str(project)
    assert origin['runtime'] is None and origin['data_revision'] == 7
    assert sha(origin_path) == ready['project_origin_sha256'] == review['private_origin_sha256']
    assert ready['review_sha256'] == sha(BUILD / 'BUILD_REVIEW.json')
    assert ready['source_freeze_sha256'] == sha(BUILD / 'SOURCE_FREEZE.json')
    assert ready['integration_inputs_sha256'] == sha(BUILD / 'INTEGRATION_INPUTS.json')
    for name, digest in prep['source_proofs_sha256'].items():
        assert sha(SOURCE / name) == digest, name
    source_state = mutable_snapshot(SOURCE / 'project')
    assert source_state == prep['source_snapshot']
    for name, digest in origin['generated_sha256'].items():
        assert sha(project / name) == digest, name
    old_origin = json.loads((SOURCE / 'local-origin.json').read_text())
    old_generated = old_origin['generated_sha256']
    assert set(old_generated) == set(origin['generated_sha256'])
    changed_files = sorted(name for name, digest in origin['generated_sha256'].items() if old_generated[name] != digest)
    assert changed_files == ['opening/first-journey.routes.json'], changed_files
    route_revision = origin['town_route_revision']
    assert route_revision['root_authorized'] and route_revision['file'] == changed_files[0]
    assert route_revision['source_sha256'] == old_generated[changed_files[0]]
    assert route_revision['sha256'] == origin['generated_sha256'][changed_files[0]]
    assert sha(Path(route_revision['proof']['path'])) == route_revision['proof']['sha256']
    links = {item.name: str(item.resolve(strict=True)) for item in sorted((project / 'models').iterdir())}
    assert links == origin['model_wrapper_links']
    for platform in ['native', 'windows']:
        assert sha(Path(ready[platform])) == ready[platform + '_sha256']
    freeze = json.loads((BUILD / 'SOURCE_FREEZE.json').read_text())
    for name, digest in freeze['source_sha256'].items():
        assert sha(ROOT / name) == digest, name
    references = []
    for row in source_proof['asset_reference_matrix']:
        row = dict(row)
        row['path'] = row['path'].replace('/candidate-v6/', '/candidate-v7/')
        path = Path(row['path'])
        assert str(path.resolve(strict=True)) == row['resolved'] and sha(path) == row['sha256']
        references.append(row)
    cpu_path = ROOT / 'docs/first-journey/2026-11-05/data-v7-cpu/result.json'
    cpu = json.loads(cpu_path.read_text())
    assert cpu['passed'] and cpu['actual_cpu_bounds_and_prepare'] and cpu['references'] == 16
    assert [row['path'] for row in cpu['rows']] == [row['path'] for row in references]
    assert all(row['passed'] and row['bounds_ok'] and row['prepare_ok'] for row in cpu['rows'])
    shared = {**integration['character_runtime_files_sha256'], **integration['runtime_files_sha256']}
    for name, digest in shared.items():
        assert sha(Path(name)) == digest, name
    assert not any((project / 'saves').iterdir())
    result = dict(
        schema='valwood.first-journey.data-revision', version=2, ready=True,
        data_revision=7, runtime_version=6,
        source_candidate=str(SOURCE), source_project=str(SOURCE / 'project'),
        source_origin=reference(SOURCE / 'local-origin.json'),
        source_data_revision=reference(SOURCE / 'DATA_REVISION.json'),
        compiled_origin=reference(origin_path), candidate_origin=reference(origin_path),
        runtime_ready=reference(BUILD / 'READY.json'),
        build_review=reference(BUILD / 'BUILD_REVIEW.json'),
        source_freeze=reference(BUILD / 'SOURCE_FREEZE.json'),
        compiled_origin_sha256=ready['project_origin_sha256'],
        generated_sha256=origin['generated_sha256'],
        mutable_preferences=['graphics.settings', 'studio-layout.ini'],
        model_wrapper_links=links,
        base_alias_corrections=origin['model_reference_corrections'],
        asset_reference_matrix=references, cpu_loader_proof=reference(cpu_path),
        source_candidate_unchanged=True, compiled_runtime_unchanged=True,
        engine_source_unchanged=True, source_state_before=prep['source_snapshot'],
        source_state_after=source_state, shared_runtime_bank_files_sha256=shared,
        native=dict(path=ready['native'], sha256=ready['native_sha256']),
        windows=dict(path=ready['windows'], sha256=ready['windows_sha256']),
        ordinary_project_files_changed=changed_files, model_aliases_added=[],
        town_route_revision=route_revision,
        model_aliases_preserved=origin['model_reference_corrections'],
        new_saves_empty=True, visual_acceptance_pending=True, gpu_used=False,
        runtime_recompiled=True, activation_performed=False,
    )
    with destination.open('x') as stream:
        json.dump(result, stream, indent=2, allow_nan=False)
        stream.write('\n')
    print(json.dumps(dict(ready=True, proof=str(destination), sha256=sha(destination), data_revision=7, runtime_version=6)))


if __name__ == '__main__':
    main()
