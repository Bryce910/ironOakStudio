"""Publish only the tested woodland scene, model and renderer changes, preserving user state."""
from pathlib import Path
import argparse, fcntl, hashlib, importlib.util, json, os, shutil, sys, time
sys.dont_write_bytecode = True
N = Path(__file__).resolve().parents[1]
E = N.parents[1]
R = E.parent
B = N.with_name('campsite-look-2026-11-19')
C = N.with_name('river-swim-2026-11-19')
L = E/'projects/valwood_current'
sys.path.insert(0, str(N))
from check_unified import (sha, read, need, stat, evidence, allowed_scene_path,
                           owned_project_leaf, verify_inherited_chain)


def _temporary(path):
    temporary = Path(path).with_name(Path(path).name+'.woodland.tmp')
    # A dangling alias is occupied too. Never remove somebody else's leftover.
    need(not temporary.exists() and not temporary.is_symlink(), 'Unexpected temporary file')
    return temporary


def write(p, value):
    p = Path(p)
    temporary = _temporary(p)
    created = False
    try:
        with temporary.open('x') as output:
            created = True
            output.write(json.dumps(value, indent=2)+'\n')
        os.replace(temporary, p)
    finally:
        if created:
            temporary.unlink(missing_ok=True)


def atomic_copy(source, target):
    target = Path(target)
    temp = _temporary(target)
    created = False
    try:
        # Reserve the name exclusively before copy2 overwrites our own file.
        with temp.open('xb'):
            created = True
        shutil.copy2(source, temp)
        os.replace(temp, target)
    finally:
        if created:
            temp.unlink(missing_ok=True)


def atomic_link(path, target):
    path = Path(path)
    temp = _temporary(path)
    created = False
    try:
        temp.symlink_to(target, target_is_directory=True)
        created = True
        os.replace(temp, path)
    finally:
        if created:
            temp.unlink(missing_ok=True)


def mutable_state_files(project):
    project = Path(project)
    need(project.is_dir() and not project.is_symlink(), 'User project unexpectedly aliased')
    files = set()
    for folder in ('saves', 'settings'):
        root = project/folder
        need(not root.is_symlink(), 'User state directory unexpectedly aliased')
        if not root.exists():
            continue
        need(root.is_dir(), 'User state directory is not a directory')
        for p in root.rglob('*'):
            need(not p.is_symlink(), 'User state unexpectedly aliased')
            need(p.is_file() or p.is_dir(), 'Unsupported user state entry')
            if p.is_file():
                files.add(p)
    for pattern in ('*.settings', 'arena_*.cfg', 'player-profile.config'):
        for p in project.glob(pattern):
            need(p.is_file() and not p.is_symlink(), 'User state unexpectedly aliased')
            files.add(p)
    return sorted(files)


def relative_state(project, *, graphics=True):
    project = Path(project)
    return {str(p.relative_to(project)): sha(p) for p in mutable_state_files(project)
            if graphics or p.relative_to(project) != Path('graphics.settings')}


def state_files(projects):
    return {str(p): sha(p) for project in projects for p in mutable_state_files(project)}


def copy_state_exact(source, target):
    source, target = Path(source), Path(target)
    expected = relative_state(source)
    existing = mutable_state_files(target)  # Reject aliases before any deletion.
    for folder in ('saves', 'settings'):
        if (target/folder).exists():
            shutil.rmtree(target/folder)
        if (source/folder).exists():
            shutil.copytree(source/folder, target/folder)
    for p in existing:
        if len(p.relative_to(target).parts) == 1:
            p.unlink()
    for p in mutable_state_files(source):
        if len(p.relative_to(source).parts) == 1:
            shutil.copy2(p, target/p.name)
    need(relative_state(source) == expected, 'Accepted state changed while copying')
    need(relative_state(target) == expected, 'New mode did not receive exact accepted state')


def restore_publication(changes, old_links, rollback, ready_written):
    # An individual failed restoration must not prevent restoring the pointer
    # and every other scene/link. Report the complete incomplete-rollback list.
    errors = []
    operations = [(str(target), lambda target=target: atomic_copy(
        rollback/'world-project'/target.relative_to(L), target)) for _, target in changes]
    operations.extend((path, lambda path=path, target=target: atomic_link(Path(path), target))
                      for path, target in old_links.items())
    operations.append((str(R/'CURRENT_VALWOOD.json'), lambda: atomic_copy(
        rollback/'CURRENT_VALWOOD.json', R/'CURRENT_VALWOOD.json')))
    if ready_written:
        operations.append((str(N/'UNIFIED_READY.json'), lambda: (N/'UNIFIED_READY.json').unlink(missing_ok=True)))
    for label, operation in operations:
        try:
            operation()
        except BaseException as error:
            errors.append(label+': '+type(error).__name__+': '+str(error))
    return errors


def preflight():
    need(not (N/'PUBLICATION.json').exists(), 'Published generations are immutable')
    current = read(R/'CURRENT_VALWOOD.json')
    need(current['runtime_generation'] == str(B) and sha(R/'CURRENT_VALWOOD.json') ==
         read(N/'BASELINE.json')['current_pointer_sha256'], 'Current generation changed')
    verify_inherited_chain(live_after=False)
    manifest = read(N/'review/CHANGESET.json')
    binary = evidence(manifest)
    areas = [Path(p) for p in current['area_projects'].values()]
    for path, h in read(N/'review/LIVE_BEFORE.json').items():
        p = Path(path)
        if p.name in ('graphics.settings', 'studio-layout.ini', 'world.studio.workspace'):
            continue
        need(p.is_file() and sha(p) == h, 'Live authoring changed: '+str(p))
    changes = []
    for row in manifest['scene_changes']:
        relative = Path(row['path'])
        need(allowed_scene_path(str(relative)), 'Unreviewed scene target')
        source = owned_project_leaf(N/'world-project',relative)
        target = owned_project_leaf(L,relative)
        need(sha(source) == row['after_sha256'] and sha(target) == row['before_sha256'], 'Scene conflict')
        changes.append((source, target))
    # Only four existing leaf aliases change. Never replace the artist's
    # owned model directory or copy a mesh through an existing source alias.
    for row in manifest['asset_changes']:
        relative = Path(row['path'])
        need(not relative.is_absolute() and '..' not in relative.parts and
             relative.parts[0] == 'models', 'Escaping model alias target')
        target, source = L/relative, N/'world-project'/relative
        need(source.is_file() and not source.is_symlink() and
             str(source) == row['target'] and sha(source) == row['after_sha256'], 'Model source conflict')
        need(target.is_symlink() and target.is_file() and
             os.readlink(target) == row['before_link'] and
             str(target.resolve()) == row['before_resolved'] and
             sha(target) == row['before_sha256'], 'Live model alias conflict')
        need(all(not p.is_symlink() for p in target.parents if p.is_relative_to(L)),
             'Model alias parent is not owned')
    for project in [L, *areas]:
        need((project/'shaders').is_symlink() and
             (project/'shaders').resolve() == (B/'shared/shaders').resolve(), 'Unexpected shader binding')
    return current, manifest, binary, areas, changes


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--apply', action='store_true')
    a = ap.parse_args()
    lock = os.open(E/'projects/seldonne/.valwood-instance.lock', os.O_RDWR|os.O_CREAT, 0o600)
    fcntl.flock(lock, fcntl.LOCK_EX|fcntl.LOCK_NB)
    current, manifest, binary, areas, changes = preflight()
    projects = [L, *areas, B/'arena-project', B/'joust-project', B/'shared']
    before_state = state_files(projects)
    if not a.apply:
        print('Preflight passed; staged scene changes:', len(changes), 'model aliases:', len(manifest['asset_changes']))
        return
    rollback = N/'rollback'
    need(not rollback.exists(), 'Retain/review existing rollback before retry')
    rollback.mkdir()
    shutil.copy2(R/'CURRENT_VALWOOD.json', rollback/'CURRENT_VALWOOD.json')
    for _, target in changes:
        dest = rollback/'world-project'/target.relative_to(L)
        dest.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(target, dest)
    links = [(p/'shaders', N/'shared/shaders') for p in [L, *areas]]
    links += [(L/row['path'], Path(row['target'])) for row in manifest['asset_changes']]
    old_links = {str(p): os.readlink(p) for p, _ in links}
    write(rollback/'ALIAS_LINKS.json', old_links)
    # Mirror the locked latest state, including deletions since staging. A
    # candidate checkpoint left by QA must never resurrect a deleted user save.
    for mode in ('arena', 'joust'):
        copy_state_exact(B/(mode+'-project'), N/(mode+'-project'))
    settings = [p/'graphics.settings' for p in projects if (p/'graphics.settings').is_file()]
    need(bool(settings), 'No graphics settings to carry')
    newest = max(settings, key=lambda p: p.stat().st_mtime_ns)
    graphics_sha256 = sha(newest)
    for project in (N/'shared', N/'arena-project', N/'joust-project'):
        shutil.copy2(newest, project/'graphics.settings')
    for mode in ('arena', 'joust'):
        need(relative_state(N/(mode+'-project'), graphics=False) ==
             relative_state(B/(mode+'-project'), graphics=False), 'Carried mode state differs')
        need(sha(N/(mode+'-project')/'graphics.settings') == graphics_sha256,
             'Carried graphics settings differ from the latest master')
    ready_written = False
    try:
        for source, target in changes:
            atomic_copy(source, target)
        for path, target in links:
            atomic_link(path, target)
        next_current = dict(current, runtime_generation=str(N), binary=str(N/'runtime/rpg'),
                            source=str(N/'source-v1'), shared_shaders=str(N/'shared/shaders'),
                            character_textures=str(N/'shared/character-textures'), guide=str(N/'GUIDE.txt'),
                            binary_sha256=binary)
        pins = dict(manifest['pins'])
        for p in (N/'review/CHANGESET.json', N/'review/FINAL_REVIEW.json', N/'check_unified.py', N/'launch.py'):
            pins[str(p)] = sha(p)
        ready = dict(schema='valwood.woodland-detail.ready.v1', ready=True, generation=str(N),
                     baseline=str(B), binary_sha256=binary, pins=pins,
                     stats={p: stat(p) for p in pins}, aliases=manifest['aliases'],
                     live_scene_sha256={row['path']: row['after_sha256'] for row in manifest['scene_changes']},
                     live_model_aliases={str(L/row['path']): str(Path(row['target']).resolve())
                                         for row in manifest['asset_changes']},
                     runtime_reused=False, application_units=104,
                     full_application_dependency_closure_verified=True)
        write(N/'UNIFIED_READY.json', ready)
        ready_written = True
        write(R/'CURRENT_VALWOOD.json', next_current)
        from check_unified import check
        check()
        need(state_files(projects) == before_state, 'Existing saved state/settings changed')
        write(N/'PUBLICATION.json', dict(passed=True, generation=str(N), baseline=str(B),
              binary_sha256=binary, runtime_reused=False, Windows_work_performed=False,
              application_units=104, full_application_dependency_closure_verified=True,
              source_changes=manifest['source_changes'], asset_changes=manifest['asset_changes'],
              shared_live_catalog_transition=manifest['shared_live_catalog_transition'],
              live_save_and_settings_hashes_preserved=True, scene_changes=manifest['scene_changes'],
              shader_changes=manifest['shader_changes'], all_five_launchers_checked=True,
              published_unix=time.time(), rollback=str(rollback)))
    except BaseException as original:
        errors = restore_publication(changes, old_links, rollback, ready_written)
        if errors:
            raise RuntimeError('Publication failed; rollback could not restore: '+
                               '; '.join(errors)) from original
        raise
    print('Published tested woodland scene, leaf models and renderer. Existing user saves/settings preserved.')


if __name__ == '__main__':
    main()
