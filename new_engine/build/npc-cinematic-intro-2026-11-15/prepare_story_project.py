#!/usr/bin/env python3
"""Snapshot the actual complete story project without sharing mutable source inodes.

This is a seed/provenance receipt, not cinematic/native acceptance. Later authored
cinematic edits and playthrough save changes must receive their own final closure.
"""
import hashlib
import json
import os
from pathlib import Path
import shutil
import stat
import subprocess
import tempfile

N = Path(__file__).resolve().parent
SOURCE = N.parents[1] / 'projects/valwood_design'
DEST = N / 'story-project'
REVIEW = N / 'review'
BACKUP = REVIEW / 'story-source-state-backup'
REPORT = REVIEW / 'STORY_PROJECT_SNAPSHOT.json'
REQUIRED = (
    'project.valwood', 'world.studio', 'opening.config', 'opening/home.settings',
    'opening/home.physics', 'opening/game-start.settings', 'opening/cinematic.settings',
    'opening/reference.json', 'opening/interior.settings', 'opening/room-controls.settings',
    'opening/arrival-camp.settings', 'opening/arrival-redesign.json',
    'opening/first-journey.settings', 'opening/first-journey.town-people.settings',
    'opening/first-journey.routes.json', 'CHARACTER_POLISH.json',
    'blood_assets/BLOOD_ASSETS.json', 'blood_assets/blood.bank', 'minimap.config',
    'models/unarmed_hero/UNARMED-HERO.json', 'models/aquatic_hero/AQUATIC-HERO.json',
    'models/fatality_hero/FATALITY-BANK.json', 'models/weapon_loadouts/WEAPON-LOADOUTS.json',
    'models/combat_anatomy/anatomy.bin', 'models/combat_anatomy/caps/cap_171.glb')


def require(ok, message):
    if not ok:
        raise RuntimeError(message)


def sha(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def inventory(root):
    result = {}
    for directory, names, files in os.walk(root, followlinks=False):
        for name in names:
            require(not (Path(directory) / name).is_symlink(), 'Source directory link rejected')
        for name in files:
            path = Path(directory) / name
            st = path.lstat()
            require(stat.S_ISREG(st.st_mode), 'Nonregular project entry: ' + str(path))
            result[path.relative_to(root).as_posix()] = (st.st_dev, st.st_ino, st.st_size, st.st_mtime_ns)
    return result


def digests(root, records):
    cache, result = {}, {}
    for relative, identity in records.items():
        key = identity[:3]
        if key not in cache:
            cache[key] = sha(root / relative)
        result[relative] = cache[key]
    return result


def main():
    require(SOURCE.is_dir() and not SOURCE.is_symlink(), 'Expected complete regular source project')
    require(not DEST.exists() and not BACKUP.exists() and not REPORT.exists(),
            'Existing snapshot refused; preserve it and choose a versioned stage')
    REVIEW.mkdir(exist_ok=True)
    require(all((SOURCE / path).is_file() for path in REQUIRED), 'Incomplete story source')
    before = inventory(SOURCE)
    print('Hashing source', len(before), 'files; hardlink aliases hash once', flush=True)
    hashes = digests(SOURCE, before)
    require(inventory(SOURCE) == before, 'Source changed while hashing')
    unique = {value[:2]: value[2] for value in before.values()}
    stage = Path(tempfile.mkdtemp(prefix='.story-snapshot-', dir=N))
    try:
        subprocess.run(['cp', '-a', '--reflink=auto', str(SOURCE) + '/.', str(stage)], check=True)
        copied = inventory(stage)
        require(set(copied) == set(before), 'Snapshot file inventory differs')
        require(all(copied[name][:2] != before[name][:2] for name in before),
                'Snapshot shares a mutable source inode')
        print('Verifying independent copied bytes', flush=True)
        require(digests(stage, copied) == hashes, 'Snapshot bytes differ from source')
        require(inventory(SOURCE) == before, 'Source changed while copying; do not publish this snapshot')
        stage.rename(DEST)
    finally:
        if stage.exists():
            shutil.rmtree(stage)
    BACKUP.mkdir()
    # Preserve existing state/configs outside the playable project. Nothing is
    # discarded, and no user progress is imported into a fresh QA story.
    for name in ('saves', 'player-profile.config', 'studio-layout.ini', 'world.studio.workspace',
                 'world.studio.recovery', 'world.studio.recovery.1', 'world.studio.recovery.2'):
        path = DEST / name
        if path.exists():
            path.rename(BACKUP / name)
    (DEST / 'saves').mkdir()
    profile_tokens = (SOURCE / 'player-profile.config').read_text().split()
    require(profile_tokens[:2] == ['VALWOOD_PLAYER_PROFILE', '1']
            and profile_tokens[2] == 'path' and profile_tokens[4:] == ['end'],
            'Invalid original profile config')
    original_profile = (SOURCE / profile_tokens[3]).resolve()
    source_profile_present = original_profile.is_file()
    if source_profile_present:
        shutil.copy2(original_profile, BACKUP / 'source-shared-player.progress')
    shared = N / 'shared-profile'
    require(not shared.exists(), 'Existing shared profile folder refused')
    shared.mkdir()
    helper = REVIEW / 'write_fresh_story_profile.cpp'
    helper.write_text('''#include "engine/world_player_profile.h"
#include <cstdio>
#include <filesystem>
int main(int argc,char**argv){
 if(argc!=2)return 2;
 const std::filesystem::path root=argv[1];
 if(!std::filesystem::is_regular_file(root/"STORY_PROFILE_SEED_ONLY"))return 2;
 const auto path=root/"player.progress";
 if(std::filesystem::exists(path))return 2;
 world_player_profile::State state,actual;std::string error;
 if(!world_player_profile::save(path,state,error)||world_player_profile::load(path,actual,error)!=world_arena_progress::Load::Loaded||
    world_player_profile::encode(actual)!=world_player_profile::encode(state)){
  std::fprintf(stderr,"%s\\n",error.c_str());return 1;}
 std::printf("Fresh default story profile: coins0, inventory empty, revision0, next arena run1\\n");
}
''')
    marker = shared / 'STORY_PROFILE_SEED_ONLY'
    marker.write_text('Independent fresh story profile; no user profile modified.\n')
    binary = helper.with_suffix('')
    command = ['/usr/bin/g++', '-std=c++17', '-O2', '-I' + str(N / 'source-v1'), str(helper), '-o', str(binary)]
    subprocess.run(command, check=True)
    subprocess.run([str(binary), str(shared)], check=True)
    (DEST / 'player-profile.config').write_text('VALWOOD_PLAYER_PROFILE 1\npath ../shared-profile/player.progress\nend\n')
    # World data are copied exactly; only the visible project label changes.
    project = (DEST / 'project.valwood').read_text().splitlines()
    require(all(not value.split('=', 1)[1].startswith('/') for value in project if '=' in value),
            'Project contains an absolute root reference')
    project = ['name=Valwood | First Contact Cinematic' if line.startswith('name=') else line for line in project]
    (DEST / 'project.valwood').write_text('\n'.join(project) + '\n')
    shader_overrides = {}
    for shader in sorted((N / 'shaders').iterdir()):
        if shader.is_file():
            shutil.copy2(shader, DEST / 'shaders' / shader.name)
            shader_overrides[shader.name] = sha(shader)
    require(not any(path.is_symlink() for path in DEST.rglob('*')), 'Final snapshot must have no links')
    require(not (DEST / 'saves/opening.progress').exists(), 'Fresh title would inherit story progress')
    require((DEST / '../shared-profile/player.progress').resolve() == shared / 'player.progress',
            'Profile target leaves the new stage')
    # Source remains unchanged through all seed changes, including its saves.
    require(inventory(SOURCE) == before, 'Source project changed during preparation')
    report = {
        'schema': 'valwood.first-contact.story-project-snapshot.v1', 'passed': True,
        'source_project': str(SOURCE), 'project': str(DEST), 'source_files': len(before),
        'logical_source_bytes': sum(value[2] for value in before.values()),
        'distinct_source_inodes': len(unique), 'distinct_source_inode_bytes': sum(unique.values()),
        'copy_recipe': ['cp', '-a', '--reflink=auto', 'SOURCE/.', 'independent-stage'],
        'source_inodes_shared': 0, 'symlinks': 0, 'source_stat_guard_unchanged': True,
        'all_snapshot_bytes_verified': True, 'source_inventory_sha256': hashes,
        'historical_source_stat_guard': before, 'preserved_source_state': str(BACKUP),
        'original_shared_profile': str(original_profile), 'original_shared_profile_present': source_profile_present,
        'fresh_profile': str(shared / 'player.progress'), 'fresh_profile_sha256': sha(shared / 'player.progress'),
        'default_title_no_opening_save': True, 'latest_shader_overrides': shader_overrides,
        'seed_runtime_changes': ['Project display name only; world coordinates/terrain key preserved.',
                                 'Private stage-local shared-profile with a production-encoded fresh default profile.',
                                 'Existing source saves/UI recovery state preserved outside the playable copy.',
                                 'Project shaders match the current stage renderer.'],
        'inputs': {str(Path(__file__).resolve()): sha(__file__), str(helper): sha(helper), str(binary): sha(binary)},
        'seed_file_sha256': {name: sha(DEST / name) for name in REQUIRED},
        'limits': ['Historical seed/provenance receipt; later cinematic authoring and runtime saves require their own final input closure.',
                   'No native runtime, visual, collision or story-playthrough acceptance is claimed.']}
    REPORT.write_text(json.dumps(report, indent=2, sort_keys=True) + '\n')
    print('PASS independent complete story seed:', REPORT, flush=True)


if __name__ == '__main__':
    main()
