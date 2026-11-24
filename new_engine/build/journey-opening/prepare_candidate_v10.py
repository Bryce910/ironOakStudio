#!/usr/bin/env python3
"""Fresh C9 data clone for apartment-objective R9 runtime, preserving user saves; never launch it."""
from pathlib import Path
import copy
import hashlib
import json
import shutil
import sys

ROOT = Path(__file__).resolve().parents[2]
HERE = Path(__file__).resolve().parent
SOURCE_ROOT = HERE / 'candidate-v9'
SOURCE = SOURCE_ROOT / 'project'
DEST = HERE / 'candidate-v10'
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
    assert old['runtime']['binary'] == str(HERE / 'runtime-v8/bin/rpg') and old['data_revision'] == 9
    assert sha(Path(old['runtime']['binary'])) == old['runtime']['sha256']
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
    clone(SOURCE / 'saves', PROJECT / 'saves')
    save_copy = {p.relative_to(PROJECT).as_posix(): sha(p) for p in sorted((PROJECT / 'saves').rglob('*')) if p.is_file()}
    assert save_copy == {p.relative_to(SOURCE).as_posix(): sha(p) for p in sorted((SOURCE / 'saves').rglob('*')) if p.is_file()}
    cinematic = PROJECT / 'opening/cinematic.settings'
    authored_before = cinematic.read_bytes()
    assert authored_before.count(b'"Just one quiet night."') == 1
    cinematic.write_bytes(authored_before.replace(b'"Just one quiet night."', b'"I\'ll play a video game. Just one hour."'))
    revision = dict(root_authorized=True, changed_files=['opening/cinematic.settings'], source_sha256=sha(SOURCE / 'opening/cinematic.settings'), sha256=sha(cinematic), reason='Third cinematic shot directly states the immediate video-game objective; all camera/timing data unchanged.')
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
        and not item.is_relative_to(PROJECT / 'saves')
    }
    assert set(generated) == set(old['generated_sha256'])
    assert {name for name in generated if generated[name] != old['generated_sha256'][name]} == {'opening/cinematic.settings'}
    assert before == mutable_snapshot(SOURCE), 'C9 changed while copying'
    assert proof_before == {name: sha(SOURCE_ROOT / name) for name in proof_names}
    origin = copy.deepcopy(old)
    origin.update(
        data_revision=10, project=str(PROJECT), source_project=str(SOURCE),
        source_origin_sha256=source_origin_sha, source_guard=before,
        runtime=None, generated_sha256=generated, model_wrapper_links=links, authored_data_revision=revision,
        private_saves_empty=not save_copy, source_saves_copied=True, copied_user_saves_sha256=save_copy,
        source_unchanged=True, engine_sources_modified=False, gpu_used=False,
        activated=False, activation_pending=True,
        runtime_build_pending=str(HERE / 'runtime-v9'),
    )
    origin.pop('compiled_runtime_reference', None)
    for row in origin['model_reference_corrections']:
        row['base'] = str(models / Path(row['base']).name)
    dump(DEST / 'local-origin.json', origin)
    dump(DEST / 'PREPARATION.json', dict(
        schema='valwood.first-journey.candidate-preparation', version=1,
        ready=True, project=str(PROJECT), data_revision=10,
        source_project=str(SOURCE), source_origin_sha256=source_origin_sha,
        source_proofs_sha256=proof_before, source_snapshot=before,
        source_unchanged=True, ordinary_project_data_changed=['opening/cinematic.settings'], authored_data_revision=revision,
        generated_files=len(generated), model_wrapper_links=len(links),
        three_required_base_aliases_preserved=True,
        graphics_preferences_copied_exactly=sha(PROJECT / 'graphics.settings') == sha(SOURCE / 'graphics.settings'),
        saves_empty=not save_copy, user_saves_preserved=save_copy, runtime_bound=False, runtime_build_pending=9,
        origin_sha256=sha(DEST / 'local-origin.json'), gpu_used=False,
        activation_performed=False,
    ))
    print(json.dumps(dict(ready=True, project=str(PROJECT), generated_files=len(generated), model_links=len(links), source_unchanged=True, runtime_bound=False)))


if __name__ == '__main__':
    main()
