#!/usr/bin/env python3
"""Fresh C8 data clone for QA raw-pad bookkeeping R8 runtime; never launch it."""
from pathlib import Path
import copy
import hashlib
import json
import shutil
import sys

ROOT = Path(__file__).resolve().parents[2]
HERE = Path(__file__).resolve().parent
SOURCE_ROOT = HERE / 'candidate-v8'
SOURCE = SOURCE_ROOT / 'project'
DEST = HERE / 'candidate-v9'
PROJECT = DEST / 'project'
MUTABLE = {'graphics.settings', 'studio-layout.ini'}
sys.path.insert(0, str(ROOT / 'tools'))
from check_opening_prologue import mutable_snapshot


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def dump(path, value):
    with path.open('x') as stream:
        json.dump(value, stream, indent=2, allow_nan=False)
        stream.write('\n')


def clone(source, dest):
    if source.is_symlink():
        target = source.resolve(strict=True)
        dest.symlink_to(target, target_is_directory=target.is_dir())
    elif source.is_dir():
        dest.mkdir()
        for item in sorted(source.iterdir()):
            clone(item, dest / item.name)
    elif source.is_file():
        shutil.copyfile(source, dest)
    else:
        raise ValueError('Unsupported source: ' + str(source))


def main():
    assert not DEST.exists() and not DEST.is_symlink(), 'Use a fresh candidate'
    source_origin_path = SOURCE_ROOT / 'local-origin.json'
    source_origin_sha = sha(source_origin_path)
    old = json.loads(source_origin_path.read_text())
    assert old['runtime'] is None and old['data_revision'] == 8
    for name, digest in old['generated_sha256'].items():
        assert sha(SOURCE / name) == digest, name
    proof_names = ['local-origin.json', 'DATA_REVISION.json', 'PREPARATION.json']
    proof_before = {name: sha(SOURCE_ROOT / name) for name in proof_names}
    before = mutable_snapshot(SOURCE)
    DEST.mkdir()
    PROJECT.mkdir()
    for item in sorted(SOURCE.iterdir()):
        if item.name not in {'models', 'saves'}:
            clone(item, PROJECT / item.name)
    (PROJECT / 'saves').mkdir()
    models = PROJECT / 'models'
    models.mkdir()
    links = {}
    for item in sorted((SOURCE / 'models').iterdir()):
        target = item.resolve(strict=True)
        (models / item.name).symlink_to(target, target_is_directory=target.is_dir())
        links[item.name] = str(target)
    assert len(links) == 797 and links == old['model_wrapper_links']
    generated = {
        item.relative_to(PROJECT).as_posix(): sha(item)
        for item in sorted(PROJECT.rglob('*'))
        if item.is_file() and not item.is_symlink()
        and item.relative_to(PROJECT).as_posix() not in MUTABLE
    }
    assert generated == old['generated_sha256']
    assert before == mutable_snapshot(SOURCE), 'C8 changed while copying'
    assert proof_before == {name: sha(SOURCE_ROOT / name) for name in proof_names}
    origin = copy.deepcopy(old)
    origin.update(
        data_revision=9, project=str(PROJECT), source_project=str(SOURCE),
        source_origin_sha256=source_origin_sha, source_guard=before,
        runtime=None, generated_sha256=generated, model_wrapper_links=links,
        private_saves_empty=True, source_saves_copied=False,
        source_unchanged=True, engine_sources_modified=False, gpu_used=False,
        activated=False, activation_pending=True,
        runtime_build_pending=str(HERE / 'runtime-v8'),
    )
    origin.pop('compiled_runtime_reference', None)
    for row in origin['model_reference_corrections']:
        row['base'] = str(models / Path(row['base']).name)
    dump(DEST / 'local-origin.json', origin)
    dump(DEST / 'PREPARATION.json', dict(
        schema='valwood.first-journey.candidate-preparation', version=1,
        ready=True, project=str(PROJECT), data_revision=9,
        source_project=str(SOURCE), source_origin_sha256=source_origin_sha,
        source_proofs_sha256=proof_before, source_snapshot=before,
        source_unchanged=True, ordinary_project_data_changed=[],
        generated_files=len(generated), model_wrapper_links=len(links),
        three_required_base_aliases_preserved=True,
        graphics_preferences_copied_exactly=sha(PROJECT / 'graphics.settings') == sha(SOURCE / 'graphics.settings'),
        saves_empty=True, runtime_bound=False, runtime_build_pending=8,
        origin_sha256=sha(DEST / 'local-origin.json'), gpu_used=False,
        activation_performed=False,
    ))
    print(json.dumps(dict(ready=True, project=str(PROJECT), generated_files=len(generated), model_links=len(links), source_unchanged=True, runtime_bound=False)))


if __name__ == '__main__':
    main()
