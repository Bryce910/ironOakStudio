"""Guarded camp/road publication. Running without --check-only commits it."""
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
B = N.with_name('apartment-collision-2026-11-18')
E = N.parents[1]
ROOT = E.parent
LIVE = E / 'projects/valwood_current'
CURRENT = ROOT / 'CURRENT_VALWOOD.json'
BINARY_SHA = '73f93a046e3776f6280e9773411b45531f55c965c8655bc40c0639cfbbb9c7ea'
DOCUMENTS = {'world.studio', 'scenery/demo_polish_instances.txt', 'scenery/starting_area_instances.txt', 'showcase.flames'}
ALIASES = {'models/opening/' + p + suffix + '.glb'
           for p in ('camp_hearth_polish_20261119', 'camp_roast_polish_20261119')
           for suffix in ('', '_near', '_mid')}

def need(ok, message):
    if not ok:
        raise RuntimeError(message)

def read(path):
    return json.loads(Path(path).read_text())

def sha(path):
    with Path(path).open('rb') as handle:
        return hashlib.file_digest(handle, 'sha256').hexdigest()

def atomic_bytes(path, data, mode=None):
    temporary = path.with_name(path.name + '.camp-road-' + str(os.getpid()) + '.tmp')
    need(not temporary.exists(), 'Unexpected temporary publication file ' + str(temporary))
    try:
        with temporary.open('xb') as handle:
            handle.write(data)
            handle.flush()
            os.fsync(handle.fileno())
        if mode is None:
            mode = stat.S_IMODE(path.stat().st_mode) if path.exists() else 0o664
        temporary.chmod(mode)
        os.replace(temporary, path)
    finally:
        temporary.unlink(missing_ok=True)

def atomic_link(path, target):
    temporary = path.with_name(path.name + '.camp-road-' + str(os.getpid()) + '.tmp')
    need(not temporary.exists() and not temporary.is_symlink(), 'Unexpected temporary link ' + str(temporary))
    try:
        temporary.symlink_to(target, target_is_directory=Path(target).is_dir())
        os.replace(temporary, path)
    finally:
        temporary.unlink(missing_ok=True)

def user_files(projects):
    files = set()
    for project in projects:
        for folder in ('saves', 'settings'):
            base = project / folder
            if base.exists():
                need(base.is_dir() and not base.is_symlink(), 'Unexpected user-state directory ' + str(base))
                files.update(p for p in base.rglob('*') if p.is_file())
        files.update(p for p in project.glob('*.settings') if p.is_file())
        for name in ('arena_player_character.cfg', 'arena_armor_finish.cfg', 'player-profile.config'):
            p = project / name
            if p.is_file():
                files.add(p)
    return {str(p): sha(p) for p in sorted(files)}

def load_checker():
    sys.path.insert(0, str(N))
    spec = importlib.util.spec_from_file_location('camp_road_publication_checker', N / 'check_unified.py')
    checker = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(checker)
    return checker

def preflight():
    need(not (N / 'UNIFIED_READY.json').exists(), 'Published generations are immutable')
    need(not (N / 'PUBLICATION.json').exists(), 'Publication receipt already exists')
    need(not (N / 'rollback').exists(), 'Review the retained rollback before another publication attempt')
    need(sha(N / 'runtime/rpg') == BINARY_SHA, 'Candidate executable differs from reviewed build')
    plan = read(N / 'review/PUBLICATION_PLAN.json')
    prior_bytes = CURRENT.read_bytes()
    prior = json.loads(prior_bytes)
    need(Path(plan['baseline']) == B and Path(prior['runtime_generation']) == B
         and Path(prior['working_world']) == LIVE
         and sha(CURRENT) == plan['current_pointer_before_sha256'], 'Current generation/pointer changed')
    for path, digest in read(N / 'review/LIVE_BEFORE.json').items():
        p = Path(path)
        need(p.is_relative_to(LIVE), 'Authoring guard escaped the working project')
        relative = p.relative_to(LIVE)
        if 'saves' in relative.parts or 'settings' in relative.parts or relative == Path('graphics.settings'):
            continue
        need(sha(p) == digest, 'Live authoring changed since the protected snapshot ' + str(relative))
    need({row['relative'] for row in plan['project_files']} == DOCUMENTS
         and len(plan['project_files']) == 4, 'Publication must replace exactly four ordinary documents')
    need({row['relative'] for row in plan['new_asset_aliases']} == ALIASES
         and len(plan['new_asset_aliases']) == 6, 'Publication must add exactly six fire aliases')
    replacements = []
    for row in plan['project_files']:
        relative = row['relative']
        source, target = N / 'world-project' / relative, LIVE / relative
        need(target.is_file() and not target.is_symlink(), 'Live document is not an ordinary file ' + relative)
        need(source.is_file() and sha(source) == row['after_sha256']
             and sha(target) == row['before_sha256'], 'Live/candidate authoring changed ' + relative)
        replacements.append((relative, source, target))
    new_links = []
    for row in plan['new_asset_aliases']:
        target = LIVE / row['relative']
        asset = Path(row['asset'])
        alias = N / 'world-project' / row['relative']
        need(asset.is_relative_to(N / 'assets/camp-road/fire') and asset.is_file()
             and sha(asset) == row['sha256'] and alias.resolve() == asset.resolve()
             and sha(alias) == row['sha256'], 'Owned fire alias differs ' + row['relative'])
        need(not target.exists() and not target.is_symlink() and target.parent.is_dir()
             and target.parent.resolve().is_relative_to(LIVE.resolve()), 'Fire alias already exists or escapes live project ' + row['relative'])
        new_links.append((target, asset))
    # Saved progress and user graphics settings can legitimately change during
    # play. Check authored documents against the recorded before hashes, then
    # preserve the freshly closed user-state receipt rather than overwriting it.
    for path, digest in read(N / 'review/LIVE_BEFORE.json').items():
        p = Path(path)
        if 'saves' in p.parts or 'settings' in p.parts or p.name == 'graphics.settings':
            continue
        need(p.is_relative_to(LIVE) and sha(p) == digest, 'Live authored input changed ' + str(p))
    after_close = read(N / 'review/PROGRESS_AFTER_CLOSE.json')
    need(bool(after_close), 'Post-close progress receipt is empty')
    for path, digest in after_close.items():
        p = Path(path)
        need(p.is_relative_to(LIVE / 'saves') and sha(p) == digest, 'Post-close save changed ' + path)
    need({str(p): sha(p) for p in (LIVE / 'saves').rglob('*') if p.is_file()} == after_close, 'Post-close save inventory changed')
    for relative in plan['untouched_apartment_documents']:
        need(sha(LIVE / relative) == sha(B / 'world-project' / relative)
             == sha(N / 'world-project' / relative), 'Protected apartment document differs ' + relative)
    areas = [Path(p) for p in prior['area_projects'].values()]
    need(all(p.resolve().is_relative_to(E / 'projects') for p in areas), 'Unexpected area project')
    shader_changes = [(p / 'shaders', N / 'shared/shaders') for p in sorted(set([LIVE, *areas]))]
    for path, _ in shader_changes:
        need(path.is_symlink() and path.resolve() == (B / 'shared/shaders').resolve(), 'Unexpected shader link ' + str(path))
    checker = load_checker()
    checker.verify_chain(B)
    binary, _, _ = checker.exact_runtime()
    need(binary == BINARY_SHA, 'Checker reviewed another executable')
    checker.unchanged_resources()
    checker.evidence(binary)
    projects = [LIVE, *areas, B / 'arena-project', B / 'joust-project']
    return prior_bytes, prior, replacements, new_links, shader_changes, projects

def main(check_only=False):
    # The ordinary launcher passes this same locked descriptor to the game.
    # Holding it throughout commit/check/rollback prevents switching a running
    # public game beneath the player. Private QA must also be finished by root.
    lock = os.open(E / 'projects/seldonne/.valwood-instance.lock', os.O_RDWR | os.O_CREAT, 0o600)
    try:
        try:
            fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError:
            raise RuntimeError('Valwood is running. Close it before publishing the reviewed generation.')
        prior_bytes, prior, replacements, new_links, shader_changes, projects = preflight()
        if check_only:
            print('Promotion preflight passed; no project files, links or current pointer changed.')
            return
        rollback = N / 'rollback'
        rollback.mkdir(exist_ok=False)
        (rollback / 'CURRENT_VALWOOD.json').write_bytes(prior_bytes)
        shader_before = {str(p): os.readlink(p) for p, _ in shader_changes}
        (rollback / 'links.json').write_text(json.dumps(shader_before, indent=2) + '\n')
        protected_before = user_files(projects)
        (rollback / 'USER_STATE_BEFORE.json').write_text(json.dumps(protected_before, indent=2) + '\n')
        for relative, _, target in replacements:
            backup = rollback / 'world' / relative
            backup.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(target, backup)
        # Carry the user's current arena/joust saves and choices forward. All
        # destination files are backed up too so a failed promotion is reversible.
        carry = []
        for mode in ('arena', 'joust'):
            before, after = B / (mode + '-project'), N / (mode + '-project')
            need((before / 'saves').is_dir() and (after / 'saves').is_dir()
                 and not (after / 'saves').is_symlink(), 'Invalid mode save destination ' + mode)
            sources = [p for p in (before / 'saves').rglob('*') if p.is_file()]
            sources += [before / name for name in ('arena_player_character.cfg', 'arena_armor_finish.cfg', 'graphics.settings') if (before / name).is_file()]
            for source in sources:
                relative = source.relative_to(before)
                target = after / relative
                need(not source.is_symlink() and not target.is_symlink(), 'Unexpected mode-state leaf alias ' + str(target))
                backup = rollback / 'candidate' / mode / relative
                existed = target.is_file()
                if existed:
                    backup.parent.mkdir(parents=True, exist_ok=True)
                    shutil.copy2(target, backup)
                carry.append((source, target, backup if existed else None))
        changed_carry = []
        try:
            for source, target, backup in carry:
                target.parent.mkdir(parents=True, exist_ok=True)
                changed_carry.append((target, backup))
                atomic_bytes(target, source.read_bytes(), stat.S_IMODE(source.stat().st_mode))
            for _, source, target in replacements:
                atomic_bytes(target, source.read_bytes())
            for path, target in new_links + shader_changes:
                atomic_link(path, target)
            need(user_files(projects) == protected_before, 'Unexpected live/baseline user-state mutation')
            subprocess.run([sys.executable, str(N / 'check_unified.py'), '--publish'], check=True)
            next_ = dict(prior, runtime_generation=str(N), binary=str(N / 'runtime/rpg'),
                         source=str(N / 'source-v1'), shared_shaders=str(N / 'shared/shaders'),
                         character_textures=str(N / 'shared/character-textures'), guide=str(N / 'GUIDE.txt'),
                         binary_sha256=BINARY_SHA)
            atomic_bytes(CURRENT, (json.dumps(next_, indent=2) + '\n').encode())
            for mode in ('demo', 'editor', 'arena', 'joust', 'world'):
                subprocess.run([str(ROOT / 'Launch_Valwood.sh'), mode, '--check-only'], cwd=ROOT, check=True)
            need(user_files(projects) == protected_before, 'Unexpected user-state mutation during launcher checks')
        except BaseException as original:
            errors = []
            def restore(action):
                try:
                    action()
                except BaseException as error:
                    errors.append(str(error))
            restore(lambda: atomic_bytes(CURRENT, prior_bytes))
            for relative, _, target in replacements:
                backup = rollback / 'world' / relative
                restore(lambda p=target, b=backup: atomic_bytes(p, b.read_bytes(), stat.S_IMODE(b.stat().st_mode)))
            for path, _ in new_links:
                restore(lambda p=path: p.unlink(missing_ok=True))
            for path, target in shader_before.items():
                restore(lambda p=Path(path), t=target: atomic_link(p, t))
            for target, backup in reversed(changed_carry):
                if backup:
                    restore(lambda p=target, b=backup: atomic_bytes(p, b.read_bytes(), stat.S_IMODE(b.stat().st_mode)))
                else:
                    restore(lambda p=target: p.unlink(missing_ok=True))
            restore(lambda: (N / 'UNIFIED_READY.json').unlink(missing_ok=True))
            (rollback / 'ROLLBACK.json').write_text(json.dumps(dict(restored=not errors, original_error=str(original), rollback_errors=errors), indent=2) + '\n')
            if errors:
                raise RuntimeError('Promotion failed and rollback needs review: ' + '; '.join(errors)) from original
            raise
        result = dict(passed=True, generation=str(N), binary_sha256=BINARY_SHA,
                      all_five_launchers_checked=True, live_save_and_settings_hashes_preserved=True,
                      native_only=True, runtime_reused=False,
                      changed_project_files=[r for r, _, _ in replacements],
                      new_asset_aliases=[str(p) for p, _ in new_links], rollback=str(rollback))
        (N / 'PUBLICATION.json').write_text(json.dumps(result, indent=2) + '\n')
        print(json.dumps(result, indent=2))
    finally:
        os.close(lock)

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--check-only', action='store_true', help='Validate closed-game publication prerequisites without committing')
    main(parser.parse_args().check_only)
