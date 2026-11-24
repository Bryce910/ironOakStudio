"""Atomically publish the reviewed apartment alignment, retaining rollback data."""
from pathlib import Path
import fcntl, hashlib, json, os, shutil, subprocess, sys

N = Path(__file__).resolve().parents[1]
B = N.with_name('opening-stance-2026-11-18')
E = N.parents[1]
R = E.parent
LIVE = E / 'projects/valwood_current'

def sha(p):
    with Path(p).open('rb') as f:
        return hashlib.file_digest(f, 'sha256').hexdigest()

def run(*args):
    subprocess.run([sys.executable, *map(str, args)], check=True)

def saves():
    return {str(p): sha(p) for p in (LIVE / 'saves').rglob('*') if p.is_file()}

def atomic_bytes(p, data):
    temp = p.with_name(p.name + '.apartment-alignment.tmp')
    temp.write_bytes(data)
    os.replace(temp, p)

def atomic_link(p, target):
    temp = p.with_name(p.name + '.apartment-alignment.tmp')
    assert not temp.exists() and not temp.is_symlink(), temp
    temp.symlink_to(target, target_is_directory=Path(target).is_dir())
    os.replace(temp, p)

def main():
    manifest = R / 'CURRENT_VALWOOD.json'
    old = manifest.read_bytes()
    prior = json.loads(old)
    assert prior['runtime_generation'] == str(B), 'Current generation changed'
    assert not (N / 'UNIFIED_READY.json').exists(), 'Published generations are immutable'
    lock = os.open(E / 'projects/seldonne/.valwood-instance.lock', os.O_RDWR | os.O_CREAT, 0o600)
    fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
    assert manifest.read_bytes() == old, 'Current pointer changed while locking'
    for path, digest in json.loads((N / 'review/LIVE_BEFORE.json').read_text()).items():
        assert sha(path) == digest, 'Live authoring changed: ' + path
    progress_before = saves()
    assert progress_before == json.loads((N / 'review/PROGRESS_AFTER_CLOSE.json').read_text()), 'Progress changed since tests'
    final = json.loads((N / 'review/FINAL_REVIEW.json').read_text())
    assert final['passed'] and final['native_visual_passed'], 'Final review incomplete'
    publication = json.loads((N / 'review/DEPENDENT_REFIT_PUBLICATION.json').read_text())
    replacements = [('scenery/authored_instances.txt', N / 'world-project/scenery/authored_instances.txt')]
    new_links = []
    for item in publication['project_files']:
        assert sha(item['candidate']) == item['after_sha256'], 'Candidate changed'
        if item.get('new_unique_asset'):
            target = LIVE / item['relative']
            assert not target.exists() and not target.is_symlink(), 'New asset alias already exists'
            new_links.append((target, Path(item['asset'])))
        else:
            assert sha(LIVE / item['relative']) == item['before_sha256'], 'Live document changed'
            replacements.append((item['relative'], Path(item['candidate'])))
    link_changes = [(LIVE / 'shaders', N / 'shared/shaders')]
    for project in prior['area_projects'].values():
        link_changes.append((Path(project) / 'shaders', N / 'shared/shaders'))
    for p, _ in link_changes:
        assert p.is_symlink() and p.resolve() == (B / 'shared/shaders').resolve(), 'Unexpected shader path: ' + str(p)
    rollback = N / 'rollback'
    rollback.mkdir()
    (rollback / 'CURRENT_VALWOOD.json').write_bytes(old)
    link_before = {str(p): os.readlink(p) for p, _ in link_changes}
    (rollback / 'links.json').write_text(json.dumps(link_before, indent=2) + '\n')
    for rel, _ in replacements:
        out = rollback / 'world' / rel
        out.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(LIVE / rel, out)
    for mode in ('arena', 'joust'):
        for p in (B / (mode + '-project/saves')).iterdir():
            if p.is_file():
                shutil.copy2(p, N / (mode + '-project/saves') / p.name)
        for name in ('arena_player_character.cfg', 'arena_armor_finish.cfg', 'graphics.settings'):
            p = B / (mode + '-project') / name
            if p.is_file():
                shutil.copy2(p, N / (mode + '-project') / name)
    try:
        for rel, source in replacements:
            atomic_bytes(LIVE / rel, source.read_bytes())
        for p, target in new_links + link_changes:
            atomic_link(p, target)
        assert saves() == progress_before, 'Unexpected progress mutation'
        run(N / 'check_unified.py', '--publish')
        next_ = dict(prior, runtime_generation=str(N), binary=str(N / 'runtime/rpg'),
                     source=str(N / 'source-v1'), shared_shaders=str(N / 'shared/shaders'),
                     character_textures=str(N / 'shared/character-textures'), guide=str(N / 'GUIDE.txt'),
                     binary_sha256=sha(N / 'runtime/rpg'))
        atomic_bytes(manifest, (json.dumps(next_, indent=2) + '\n').encode())
        for mode in ('demo', 'editor', 'arena', 'joust', 'world'):
            subprocess.run([str(R / 'Launch_Valwood.sh'), mode, '--check-only'], cwd=R, check=True)
        assert saves() == progress_before, 'Unexpected progress mutation'
    except BaseException:
        atomic_bytes(manifest, old)
        for rel, _ in replacements:
            atomic_bytes(LIVE / rel, (rollback / 'world' / rel).read_bytes())
        for p, _ in new_links:
            p.unlink(missing_ok=True)
        for path, target in link_before.items():
            atomic_link(Path(path), target)
        (N / 'UNIFIED_READY.json').unlink(missing_ok=True)
        raise
    result = dict(passed=True, generation=str(N), all_five_launchers_checked=True,
                  live_save_hashes_preserved=True, native_only=True, runtime_reused=True,
                  binary_sha256=next_['binary_sha256'], changed_project_files=[r for r, _ in replacements],
                  new_asset_aliases=[str(p) for p, _ in new_links], rollback=str(rollback))
    (N / 'PUBLICATION.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result, indent=2))

if __name__ == '__main__':
    main()
