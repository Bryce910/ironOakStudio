"""Publish only the reviewed runtime and shader bindings; keep live scenes intact."""
from pathlib import Path
import argparse
import fcntl
import hashlib
import importlib.util
import json
import os
import shutil
import stat
import subprocess
import sys

sys.dont_write_bytecode = True
N = Path(__file__).resolve().parents[1]
B = N.with_name('father-idle-polish-2026-11-19')
E = N.parents[1]
ROOT = E.parent
LIVE = E / 'projects/valwood_current'
CURRENT = ROOT / 'CURRENT_VALWOOD.json'

def need(ok, message):
    if not ok:
        raise RuntimeError(message)

def read(path):
    return json.loads(Path(path).read_text())

def sha(path):
    with Path(path).open('rb') as handle:
        return hashlib.file_digest(handle, 'sha256').hexdigest()

def atomic_bytes(path, data, mode=None):
    temporary = path.with_name(path.name + '.apartment-camera-' + str(os.getpid()) + '.tmp')
    need(not temporary.exists(), 'Unexpected temporary file ' + str(temporary))
    try:
        with temporary.open('xb') as handle:
            handle.write(data)
            handle.flush()
            os.fsync(handle.fileno())
        temporary.chmod(mode if mode is not None else stat.S_IMODE(path.stat().st_mode) if path.exists() else 0o664)
        os.replace(temporary, path)
    finally:
        temporary.unlink(missing_ok=True)

def atomic_link(path, target):
    temporary = path.with_name(path.name + '.apartment-camera-' + str(os.getpid()) + '.tmp')
    need(not temporary.exists() and not temporary.is_symlink(), 'Unexpected temporary link ' + str(temporary))
    try:
        temporary.symlink_to(target, target_is_directory=Path(target).is_dir())
        os.replace(temporary, path)
    finally:
        temporary.unlink(missing_ok=True)

def user_files(projects):
    files = set()
    for project in projects:
        for name in ('saves', 'settings'):
            folder = project / name
            if folder.exists():
                need(folder.is_dir() and not folder.is_symlink(), 'Unexpected state directory ' + str(folder))
                files.update(p for p in folder.rglob('*') if p.is_file())
        files.update(p for p in project.glob('*.settings') if p.is_file())
        files.update(p for p in project.glob('arena_*.cfg') if p.is_file())
        if (project / 'player-profile.config').is_file():
            files.add(project / 'player-profile.config')
    return {str(p): sha(p) for p in sorted(files)}

def authoring_before():
    expected = {}
    for name, digest in read(N / 'review/LIVE_BEFORE.json').items():
        p = Path(name)
        need(p.is_relative_to(LIVE), 'Authoring guard escaped the working project')
        relative = p.relative_to(LIVE)
        if any(part in ('saves', 'settings', 'ai') for part in relative.parts):
            continue
        if p.suffix in ('.progress', '.living') or p.name in ('graphics.settings', 'studio-layout.ini', 'world.studio.workspace'):
            continue
        need(sha(p) == digest, 'Live authored document changed since preparation ' + str(relative))
        expected[name] = digest
    need(bool(expected), 'Empty authoring guard')
    return expected

def load_checker():
    sys.path.insert(0, str(N))
    spec = importlib.util.spec_from_file_location('apartment_camera_promotion_checker', N / 'tools/check_unified.py')
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module

def preflight():
    need(not (N / 'UNIFIED_READY.json').exists() and not (N / 'PUBLICATION.json').exists(), 'Published generations are immutable')
    need(not (N / 'rollback').exists(), 'Review retained rollback before another attempt')
    old = CURRENT.read_bytes()
    current, baseline = json.loads(old), read(N / 'BASELINE.json')
    need(Path(current['runtime_generation']) == B and Path(current['working_world']) == LIVE
         and Path(baseline['baseline']) == B and sha(CURRENT) == baseline['current_pointer_sha256'], 'Current generation/pointer changed')
    checker = load_checker()
    checker.verify_chain(B)
    binary, _, _ = checker.exact_runtime()
    checker.unchanged_resources()
    checker.evidence(binary)
    authored = authoring_before()
    progress = {str(p): sha(p) for p in sorted((LIVE / 'saves').rglob('*')) if p.is_file()}
    need(progress == read(N / 'review/PROGRESS_AFTER_CLOSE.json'), 'Latest post-close progress changed')
    areas = [Path(p) for p in current['area_projects'].values()]
    need(all(p.resolve().is_relative_to(E / 'projects') for p in areas), 'Unexpected area project')
    shader_changes = [(p / 'shaders', N / 'shared/shaders') for p in sorted(set([LIVE, *areas]))]
    for path, _ in shader_changes:
        need(path.is_symlink() and path.resolve() == (B / 'shared/shaders').resolve(), 'Unexpected shader link ' + str(path))
    projects = [LIVE, *areas, B / 'arena-project', B / 'joust-project', B / 'shared']
    return old, current, binary, authored, shader_changes, projects

def main(check_only=False):
    lock = os.open(E / 'projects/seldonne/.valwood-instance.lock', os.O_RDWR | os.O_CREAT, 0o600)
    try:
        try:
            fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError:
            raise RuntimeError('Valwood is running. Close it before promoting the apartment camera/door fix.')
        old, prior, binary, authored, links, projects = preflight()
        if check_only:
            print('Promotion preflight passed; no live files, links, settings or pointer changed.')
            return
        rollback = N / 'rollback'
        rollback.mkdir(exist_ok=False)
        (rollback / 'CURRENT_VALWOOD.json').write_bytes(old)
        link_before = {str(p): os.readlink(p) for p, _ in links}
        (rollback / 'links.json').write_text(json.dumps(link_before, indent=2) + '\n')
        protected = user_files(projects)
        (rollback / 'USER_STATE_BEFORE.json').write_text(json.dumps(protected, indent=2) + '\n')
        (rollback / 'AUTHORING_BEFORE.json').write_text(json.dumps(authored, indent=2) + '\n')
        # Carry current choices into the new mode projects, not back into the
        # protected generation or the live world. Back up every destination.
        carry = []
        for mode in ('arena', 'joust'):
            source, target = B / (mode + '-project'), N / (mode + '-project')
            need((source / 'saves').is_dir() and (target / 'saves').is_dir()
                 and not (target / 'saves').is_symlink(), 'Invalid mode save destination')
            files = [p for p in (source / 'saves').rglob('*') if p.is_file()]
            files += [p for p in source.glob('arena_*.cfg') if p.is_file()]
            if (source / 'graphics.settings').is_file():
                files.append(source / 'graphics.settings')
            carry += [(p, target / p.relative_to(source)) for p in files]
        graphics = [p / 'graphics.settings' for p in projects if (p / 'graphics.settings').is_file()]
        need(bool(graphics), 'Latest graphics preference missing')
        carry.append((max(graphics, key=lambda p: p.stat().st_mtime_ns), N / 'shared/graphics.settings'))
        backups = []
        for source, target in carry:
            need(not source.is_symlink() and not target.is_symlink(), 'Unexpected user-state leaf alias')
            backup = rollback / 'candidate' / target.relative_to(N)
            if target.exists():
                backup.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(target, backup)
            else:
                backup = None
            backups.append((source, target, backup))
        changed = []
        try:
            for source, target, backup in backups:
                target.parent.mkdir(parents=True, exist_ok=True)
                changed.append((target, backup))
                atomic_bytes(target, source.read_bytes(), stat.S_IMODE(source.stat().st_mode))
                os.utime(target, ns=(source.stat().st_atime_ns, source.stat().st_mtime_ns))
            for path, target in links:
                atomic_link(path, target)
            need(user_files(projects) == protected and authoring_before() == authored, 'Unexpected live authoring/user-state mutation')
            subprocess.run([sys.executable, str(N / 'check_unified.py'), '--publish'], check=True)
            next_ = dict(prior, runtime_generation=str(N), binary=str(N / 'runtime/rpg'), source=str(N / 'source-v1'),
                         shared_shaders=str(N / 'shared/shaders'), character_textures=str(N / 'shared/character-textures'),
                         guide=str(N / 'GUIDE.txt'), binary_sha256=binary)
            atomic_bytes(CURRENT, (json.dumps(next_, indent=2) + '\n').encode())
            for mode in ('demo', 'editor', 'arena', 'joust', 'world'):
                subprocess.run([str(ROOT / 'Launch_Valwood.sh'), mode, '--check-only'], cwd=ROOT, check=True)
            need(user_files(projects) == protected and authoring_before() == authored, 'Publication changed live authoring or saved preferences')
        except BaseException as original:
            errors = []
            def restore(action):
                try:
                    action()
                except BaseException as error:
                    errors.append(str(error))
            restore(lambda: atomic_bytes(CURRENT, old))
            for path, target in link_before.items():
                restore(lambda p=Path(path), t=target: atomic_link(p, t))
            for target, backup in reversed(changed):
                if backup:
                    restore(lambda p=target, b=backup: atomic_bytes(p, b.read_bytes(), stat.S_IMODE(b.stat().st_mode)))
                else:
                    restore(lambda p=target: p.unlink(missing_ok=True))
            restore(lambda: (N / 'UNIFIED_READY.json').unlink(missing_ok=True))
            (rollback / 'ROLLBACK.json').write_text(json.dumps(dict(restored=not errors, original_error=str(original), rollback_errors=errors), indent=2) + '\n')
            if errors:
                raise RuntimeError('Promotion failed; rollback needs review: ' + '; '.join(errors)) from original
            raise
        result = dict(passed=True, generation=str(N), binary_sha256=binary, all_five_launchers_checked=True,
                      live_save_and_settings_hashes_preserved=True, live_scene_authoring_unchanged=True,
                      changed_project_files=[], new_asset_aliases=[], native_only=True, runtime_reused=False,
                      rollback=str(rollback))
        (N / 'PUBLICATION.json').write_text(json.dumps(result, indent=2) + '\n')
        print(json.dumps(result, indent=2))
    finally:
        os.close(lock)

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--check-only', action='store_true', help='Validate only; do not commit')
    main(parser.parse_args().check_only)
