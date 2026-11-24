#!/usr/bin/env python3
"""Stage the free apartment art pass in a new, isolated opening project.

The accepted cinematic project, its private saves, source asset banks and both
accepted runtimes are read-only inputs. This helper launches no game. Runtime
placement, camera composition and ordinary-input acceptance remain separate.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import shutil
import tempfile

from check_opening_prologue import ROOT, isolate_project, mutable_snapshot
from opening_home_cinematic import write_home_cinematic


DEFAULT_SOURCE = ROOT / 'build/opening-cinematic-entry/v1/project'
DEFAULT_OUTPUT = ROOT / 'build/opening-art-upgrade/v1'
ART_FILES = {'models/opening/home.glb', 'models/opening/home_mid.glb',
             'opening/layout.json', 'opening/cinematic.json',
             'opening/art-upgrade.json',
             'world.studio', 'shaders/world_foliage.fs.glsl'}
RUNTIME_INPUTS = (
    'build/opening-cinematic-entry/bin/rpg',
    'build/opening-cinematic-entry/windows/rpg.exe',
    'engine/world_viewer.cpp',
    'engine/world_opening_prologue.h',
    'engine/world_opening_prologue_runtime.inc',
    'engine/world_opening_prologue_qa.inc',
    'engine/world_opening_room_lights.h',
    'engine/world_opening_cinematic_settings.h',
    'engine/world_opening_cinematic_audio.h',
)
OPENING_MODELS = ('models/opening/home.glb', 'models/opening/home_mid.glb',
                  'models/opening/waystone.glb', 'models/opening/waystone_mid.glb')
PRIVATE_UI_STATE = {'studio-layout.ini'}
BUILD_TOOLS = ('build_opening_art_upgrade.py', 'build_opening_prologue.py',
               'opening_home_art_upgrade.py', 'opening_home_art_lighting.py',
               'opening_home_cinematic.py', 'opening_home_materials.py')


def sha(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def canonical(value):
    return (json.dumps(value, sort_keys=True, indent=2) + '\n').encode('utf-8')


def runtime_snapshot():
    return {name: sha(ROOT / name) for name in RUNTIME_INPUTS}


def project_snapshot(project):
    # The accepted project's entire models bank may itself be a symlink.
    # Fingerprint these precise inputs symmetrically rather than relying on
    # mutable_snapshot's intentionally bounded traversal of ordinary overlays.
    result = mutable_snapshot(project)
    for relative in OPENING_MODELS:
        path = Path(project) / relative
        if not path.is_file():
            raise ValueError('Accepted opening model is missing: ' + relative)
        result[relative] = sha(path)
    return result


def private_opening_models(project, source):
    """Replace the cloned bank link with a tiny writable home-only overlay."""
    models = project / 'models'
    if not models.is_symlink():
        raise ValueError('Expected the isolated models bank link')
    bank = (source / 'models').resolve(strict=True)
    if models.resolve(strict=True) != bank:
        raise ValueError('Unexpected isolated models bank')
    models.unlink()
    models.mkdir()
    for entry in sorted(bank.iterdir()):
        resolved = entry.resolve(strict=True)
        target = models / entry.name
        if entry.name == 'opening':
            if not resolved.is_dir():
                raise ValueError('Accepted opening models are not a directory')
            if sum(p.stat().st_size for p in resolved.rglob('*') if p.is_file()) > 128 * 1024 * 1024:
                raise ValueError('Opening model overlay unexpectedly exceeds 128 MiB')
            shutil.copytree(resolved, target, symlinks=False)
        else:
            target.symlink_to(resolved, target_is_directory=resolved.is_dir())
    if not (models / 'opening').is_dir() or (models / 'opening').is_symlink():
        raise ValueError('Private opening model overlay is missing')


def relocate_metadata(value, stage, destination):
    """Keep documentary material source paths valid after atomic publication."""
    if isinstance(value, str):
        prefix = str(stage) + '/'
        return str(destination) + value[len(str(stage)):] if value.startswith(prefix) else value
    if isinstance(value, list):
        return [relocate_metadata(item, stage, destination) for item in value]
    if isinstance(value, dict):
        result = {relocate_metadata(key, stage, destination):
                  relocate_metadata(item, stage, destination) for key, item in value.items()}
        if len(result) != len(value):
            raise ValueError('Documentary path relocation would collapse keys')
        return result
    return value


def preserved_files(before, after):
    """Every authored source file except the explicit art allowlist is exact."""
    source_files = {key: value for key, value in before.items()
                    if not key.endswith('/') and not key.startswith('saves/')
                    and key not in PRIVATE_UI_STATE}
    result = {key: value == after.get(key) for key, value in source_files.items()
              if key not in ART_FILES}
    unexpected = sorted(key for key in after if not key.endswith('/') and
                        not key.startswith('saves/') and key not in source_files and key not in ART_FILES)
    if unexpected or not all(result.values()):
        raise ValueError('Art build changed protected project inputs: ' +
                         json.dumps(dict(changed=[key for key, same in result.items() if not same],
                                         unexpected=unexpected)))
    return result


def build(source, destination):
    tools_before = {name: sha(ROOT / 'tools' / name) for name in BUILD_TOOLS}
    # Defer incoming helper imports until an explicit build is requested.
    import build_opening_prologue as home_builder
    from opening_home_art_lighting import patch_home_art_lighting

    source = Path(source)
    if source.is_symlink() or not source.is_dir():
        raise ValueError('Accepted source must be an ordinary project directory')
    source = source.resolve(strict=True)
    destination = Path(destination).resolve()
    if destination.exists() or destination.is_symlink():
        raise ValueError('Choose a NEW output root')
    if destination.is_relative_to(source) or destination.is_relative_to(ROOT / 'projects'):
        raise ValueError('Output cannot be inside an accepted/source project')
    source_origin = source.parent / 'opening-origin.json'
    if source_origin.is_symlink() or not source_origin.is_file():
        raise ValueError('Accepted opening-origin manifest is required')
    required = ('opening/game-start.settings', 'opening/cinematic.settings',
                'opening/cinematic.json', 'opening/layout.json')
    if any((source / name).is_symlink() or not (source / name).is_file() for name in required):
        raise ValueError('Accepted story/cinematic support is missing or linked')
    if (source / 'opening/game-start.settings').read_bytes() != b'VALWOOD_GAME_START 1\n':
        raise ValueError('The accepted console New Game story must be enabled')
    settings_before = (source / 'opening/cinematic.settings').read_bytes()
    before = project_snapshot(source)
    runtimes_before = runtime_snapshot()
    origin_hash = sha(source_origin)
    old_layout = json.loads((source / 'opening/layout.json').read_bytes())
    destination.parent.mkdir(parents=True, exist_ok=True)
    stage = Path(tempfile.mkdtemp(prefix='.opening-art-', dir=destination.parent))
    try:
        project = stage / 'project'
        copied = isolate_project(source, project)
        private_opening_models(project, source)
        scratch = stage / 'scratch'
        scratch.mkdir()
        # author_house historically chooses a shared procedural-material output.
        # Redirect only that callable during this synchronous CPU build, keeping
        # this pass's generated, embedded materials in its own retained folder.
        original_material_builder = home_builder.build_materials
        home_builder.build_materials = lambda _ignored: original_material_builder(stage / 'materials')
        try:
            house = home_builder.author_house(scratch, game_start_story=True, art_upgrade=True)
        finally:
            home_builder.build_materials = original_material_builder
        if 'art_upgrade' not in house.get('dressing', {}):
            raise ValueError('The art-upgrade hook did not run')
        collision_names = ('home.physics', 'home.settings', 'room-lights.settings', 'game-start.settings')
        collision_preserved = {}
        for name in collision_names:
            relative = 'opening/' + name
            same = (scratch / relative).read_bytes() == (source / relative).read_bytes()
            collision_preserved[relative] = same
        if not all(collision_preserved.values()):
            raise ValueError('Fresh art authoring changed accepted collision/story support: ' +
                             json.dumps(collision_preserved))
        for key in ('collision_boxes', 'collision_bodies', 'house_origin', 'floor_world_y'):
            if house[key] != old_layout[key]:
                raise ValueError('Fresh art authoring changed accepted layout collision field: ' + key)
        for relative in ('models/opening/home.glb', 'models/opening/home_mid.glb'):
            shutil.copyfile(scratch / relative, project / relative)
            if sha(project / relative) != sha(scratch / relative):
                raise ValueError('New home copy verification failed')
            if sha(project / relative) == sha(source / relative):
                raise ValueError('Art upgrade did not change home geometry/material payload')
        house = relocate_metadata(house, stage, destination)
        (project / 'opening/layout.json').write_bytes(canonical(house))
        for relative in ('opening/cinematic.settings', 'opening/cinematic.json'):
            (project / relative).unlink()
        cinematic = write_home_cinematic(project)
        if (project / 'opening/cinematic.settings').read_bytes() != settings_before:
            raise ValueError('Regenerated cinematic changed accepted cameras or timing')
        cinematic.update(geometry_modified=True, collision_modified=False,
                         original_camera_settings_retained=True,
                         policy='Accepted original camera rails/timing over the new free apartment art. No body/hand animation or recorded voice added; exact collision and playable camera retained.')
        (project / 'opening/cinematic.json').write_bytes(canonical(cinematic))
        lighting = patch_home_art_lighting(project)
        (project / 'opening/art-upgrade.json').write_bytes(canonical(dict(
            schema='valwood.opening-art-upgrade', version=1,
            art=house['dressing']['art_upgrade'], lighting=lighting,
            home_sha256=sha(project / 'models/opening/home.glb'),
            collision_unchanged=True, camera_settings_unchanged=True,
            scope='Source art and lighting provenance; native visual and gameplay acceptance are separate.')))
        shutil.rmtree(scratch)
        if any((project / 'saves').iterdir()):
            raise ValueError('Construction must leave private saves empty')
        after = project_snapshot(project)
        preservation = preserved_files(before, after)
        # All authored physics (including the meadow/city file) stay exact.
        physics_preserved = {name: after.get(name) == value for name, value in before.items()
                             if name.endswith('.physics')}
        if not physics_preserved or not all(physics_preserved.values()):
            raise ValueError('A protected physics input changed')
        source_after = project_snapshot(source)
        runtimes_after = runtime_snapshot()
        if source_after != before or sha(source_origin) != origin_hash:
            raise ValueError('Accepted source or opening-origin changed during staging')
        if runtimes_after != runtimes_before:
            raise ValueError('An accepted runtime/source changed during staging')
        generated = {str(path.relative_to(project)): sha(path)
                     for path in sorted(project.rglob('*'))
                     if path.is_file() and not path.is_symlink()}
        tools = {name: sha(ROOT / 'tools' / name) for name in BUILD_TOOLS}
        if tools != tools_before:
            raise ValueError('An authoring helper changed during staging; rebuild with the completed source')
        manifest = dict(schema='valwood.opening-art-upgrade-private-copy', version=1, passed=True,
            source_project=str(source), project=str(destination / 'project'),
            source_origin=dict(path=str(source_origin), sha256=origin_hash),
            source_snapshot=before, source_final_snapshot=source_after, source_unchanged=True,
            runtime_snapshot=runtimes_before, runtime_final_snapshot=runtimes_after, runtime_unchanged=True,
            copied_mutable_inputs=copied, generated=generated, tools=tools,
            tools_initial_snapshot=tools_before, tools_unchanged=True,
            preserved_files=preservation, all_physics_preserved=physics_preserved,
            fresh_authoring_support_preserved=collision_preserved,
            cinematic_settings_identical=True, cinematic=cinematic,
            house=house, lighting=lighting, private_saved_state_files=0,
            private_ui_state_not_copied=sorted(PRIVATE_UI_STATE.intersection(before)),
            procedural_materials='Retained separately under materials; embedded in home GLB; previous generated materials untouched',
            scope='CPU staging only. Native composition, room-switch walking, ordinary SDL journey, Windows/Wine and transport acceptance remain pending.',
            policy='New private mutable state and opening model overlay; other asset banks linked read-only. No source project, saves, accepted runtime or shared asset edits.')
        (stage / 'opening-origin.json').write_bytes(canonical(manifest))
        # The snapshot and generated manifest have been formed before publishing;
        # readers never see a partially authored destination project.
        if destination.exists() or destination.is_symlink():
            raise ValueError('Output appeared while staging')
        stage.rename(destination)
        return dict(passed=True, project=str(destination / 'project'),
                    manifest=str(destination / 'opening-origin.json'),
                    source_unchanged=True, runtime_unchanged=True,
                    physics_unchanged=all(physics_preserved.values()),
                    cinematic_settings_identical=True, private_saved_state_files=0,
                    house_bytes=house['glb_bytes'], home_sha256=generated['models/opening/home.glb'],
                    scope=manifest['scope'])
    finally:
        if stage.exists():
            shutil.rmtree(stage)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, default=DEFAULT_SOURCE)
    parser.add_argument('--output-root', type=Path, default=DEFAULT_OUTPUT)
    args = parser.parse_args()
    try:
        result = build(args.source, args.output_root)
    except (OSError, ValueError, KeyError, TypeError) as error:
        parser.exit(1, 'Opening art staging failed: ' + str(error) + '\n')
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
