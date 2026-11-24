#!/usr/bin/env python3
"""Build a private purchased-furniture opening over the accepted apartment art.

This CPU-only wrapper authors near/mid furniture separately, updates only home
geometry and its furniture collision proxies, and retains the accepted story,
camera settings, lighting and all other areas. No accepted project is edited.
Native visual, walking, performance and Windows acceptance are separate steps.
"""
from __future__ import annotations

import argparse
import copy
import json
import math
from pathlib import Path
import shutil
import tempfile

from build_opening_art_upgrade import (
    BUILD_TOOLS as ART_TOOLS, PRIVATE_UI_STATE, canonical, private_opening_models,
    project_snapshot, relocate_metadata, runtime_snapshot, sha,
)
from check_opening_prologue import ROOT, isolate_project
from opening_home_cinematic import write_home_cinematic


DEFAULT_SOURCE = ROOT / 'build/opening-art-upgrade/v2/project'
DEFAULT_OUTPUT = ROOT / 'build/opening-finward/v6'
DATA = ROOT / 'assets/purchased/finward_house_props'
COOKED = DATA / 'cooked-v2'
ALLOWED = {'models/opening/home.glb', 'models/opening/home_mid.glb',
           'opening/home.physics', 'opening/layout.json', 'opening/cinematic.json',
           'opening/art-upgrade.json', 'opening/finward.json',
           'world.studio', 'scenery/authored_instances.txt'}
BUILD_TOOLS = tuple(dict.fromkeys((*ART_TOOLS, 'build_opening_finward.py',
                                  'opening_home_finward_set.py',
                                  'opening_home_finward.py', 'cook_finward_house.py',
                                  'compact_opening_home.py')))


def private_scenery(project, source):
    """Copy the small instance list; keep other scenery banks read-only."""
    target = project / 'scenery'
    bank = (source / 'scenery').resolve(strict=True)
    if not target.is_symlink() or target.resolve(strict=True) != bank:
        raise ValueError('Expected the isolated scenery bank link')
    target.unlink()
    target.mkdir()
    for entry in sorted(bank.iterdir()):
        resolved = entry.resolve(strict=True)
        if entry.name == 'authored_instances.txt':
            shutil.copyfile(resolved, target / entry.name)
        else:
            (target / entry.name).symlink_to(resolved, target_is_directory=resolved.is_dir())


def register_home_parts(project, render_meshes):
    """Register four rigid furniture groups beside the original home shell."""
    studio = project / 'world.studio'
    instances = project / 'scenery/authored_instances.txt'
    studio_text = studio.read_text()
    instance_text = instances.read_text()
    anchor = [row.split() for row in instance_text.splitlines()
              if row.startswith('nm_opening_home ')]
    if len(anchor) != 1 or len(anchor[0]) != 14 or float(anchor[0][-1]) != 600.:
        raise ValueError('Expected the accepted home instance at its exact origin')
    asset_lines, instance_lines = [], []
    for item in render_meshes:
        stem = Path(item['path']).stem
        if stem == 'home':
            if item['offset'] != [0., 0., 0.]:
                raise ValueError('The home shell origin must stay exact')
            continue
        if stem not in {'home_finward_living', 'home_finward_bedroom',
                        'home_finward_kitchen', 'home_finward_bathroom'}:
            raise ValueError('Unexpected modular home stem: ' + stem)
        if item['offset'] != [0., .18, 0.]:
            raise ValueError('Furniture groups must share the exact floor offset')
        alias = 'nm_opening_' + stem.removeprefix('home_')
        if alias in studio_text or alias in instance_text:
            raise ValueError('Furniture group alias already exists')
        asset_lines.append(f'asset {alias} opening/{stem} 2 0 0 240 75 1')
        row = anchor[0].copy()
        row[0], row[-1] = alias, '600.18'
        instance_lines.append(' '.join(row))
    if len(asset_lines) != 4 or sum(row.startswith('asset ') for row in studio_text.splitlines()) + 4 > 128:
        raise ValueError('Modular home must fit the existing custom asset limit')
    studio.write_text(studio_text + '\n' + '\n'.join(asset_lines) + '\n')
    instances.write_text(instance_text + '\n# Furniture vertices are rebased to the .18m home floor.\n' + '\n'.join(instance_lines) + '\n')
    return dict(assets=asset_lines, instances=instance_lines,
                existing_rows_unchanged=True, furniture_collision_source='opening/home.physics')


def tool_snapshot():
    return {name: sha(ROOT / 'tools' / name) for name in BUILD_TOOLS}


def bounded_path(root, relative):
    if not isinstance(relative, str) or Path(relative).is_absolute():
        raise ValueError('Asset input must be relative to its declared root')
    root = Path(root).resolve(strict=True)
    path = (root / relative).resolve(strict=True)
    if not path.is_relative_to(root) or not path.is_file():
        raise ValueError('Asset input escaped its declared root: ' + relative)
    return path


def asset_snapshot():
    """Verify the completed cook and retain exact purchased-source evidence."""
    review_path = COOKED / 'COOK_REVIEW.json'
    review = json.loads(review_path.read_bytes())
    if review.get('passed') is not True or review.get('source_inputs_unchanged') is not True:
        raise ValueError('A completed, unchanged Finward cook is required')
    sources = review.get('source_inputs')
    if not isinstance(sources, dict) or not sources:
        raise ValueError('Cook review has no purchased source hashes')
    source_hashes = {}
    for relative, expected in sorted(sources.items()):
        actual = sha(bounded_path(DATA, relative))
        if actual != expected:
            raise ValueError('Purchased cook source changed: ' + relative)
        source_hashes[relative] = actual
    selection = json.loads((DATA / 'selection.json').read_bytes())
    staged = selection.get('staged_files')
    if not isinstance(staged, list) or not staged:
        raise ValueError('Exact prefab staging source hashes are missing')
    selected_source_hashes = {}
    for entry in staged:
        relative = 'source/' + entry['path']
        actual = sha(bounded_path(DATA, relative))
        if actual != entry['sha256']:
            raise ValueError('Selected original prefab/model/metadata changed: ' + relative)
        selected_source_hashes[relative] = actual
    catalog = json.loads((COOKED / 'catalog.json').read_bytes())
    if not isinstance(catalog, list) or not catalog:
        raise ValueError('Cooked furniture catalog is empty')
    cooked_hashes = {}
    for asset in catalog:
        models = asset.get('models')
        if not models or {model['lod'] for model in models} != {0, 1}:
            raise ValueError('Each cooked furniture asset needs near/mid source LODs')
        for model in models:
            relative = model['file']
            actual = sha(bounded_path(COOKED, relative))
            if actual != model['sha256']:
                raise ValueError('Cooked furniture changed: ' + relative)
            cooked_hashes[relative] = actual
    manifest_hashes = {name: sha(COOKED / name)
                       for name in ('catalog.json', 'COOK_REVIEW.json', 'material-report.json')}
    documentary = {name: sha(DATA / name) for name in
                   ('INVENTORY.json', 'PROVENANCE.json', 'SOURCE_REVIEW.json', 'selection.json')}
    return dict(cooked_root=str(COOKED), manifests=manifest_hashes,
                purchased_source_inputs=source_hashes, cooked_models=cooked_hashes,
                selected_prefab_source_inputs=selected_source_hashes,
                purchased_provenance=documentary)


def comparable(left, right, *, tolerance=1e-6):
    """LOD-only authoring must retain matching furniture proxies and layout."""
    if isinstance(left, bool) or isinstance(right, bool):
        return type(left) is type(right) and left == right
    if isinstance(left, (int, float)) and isinstance(right, (int, float)):
        return math.isfinite(left) and math.isfinite(right) and abs(left-right) <= tolerance
    if type(left) is not type(right):
        return False
    if isinstance(left, list):
        return len(left) == len(right) and all(comparable(a, b, tolerance=tolerance)
                                             for a, b in zip(left, right))
    if isinstance(left, dict):
        return left.keys() == right.keys() and all(comparable(left[k], right[k], tolerance=tolerance)
                                                   for k in left)
    return left == right


def preservation(before, after, allowed):
    protected = {name: value for name, value in before.items()
                 if not name.endswith('/') and not name.startswith('saves/')
                 and name not in PRIVATE_UI_STATE and name not in allowed}
    checks = {name: after.get(name) == value for name, value in protected.items()}
    unexpected = sorted(name for name in after if not name.endswith('/')
                        and not name.startswith('saves/') and name not in before
                        and name not in allowed)
    if unexpected or not all(checks.values()):
        raise ValueError('Furniture staging changed protected project data: ' +
                         json.dumps(dict(changed=[key for key, same in checks.items() if not same],
                                         unexpected=unexpected)))
    return checks


def install_library(project, assets):
    """Give this private editor project ordinary, self-contained GLB copies."""
    selected = assets['cooked_models']
    sources = {name: bounded_path(COOKED, name) for name in selected}
    total = sum(path.stat().st_size for path in sources.values())
    if total > 128 * 1024 * 1024:
        return dict(installed=False, bytes=total,
                    reason='Reviewed cooked library exceeds the bounded 128 MiB copy budget'), set()
    target = project / 'models/finward_house_props'
    if target.exists() or target.is_symlink():
        raise ValueError('Private furniture library already exists; choose a fresh source/output')
    target.mkdir()
    files = {}
    for name, source in sorted(sources.items()):
        if Path(name).name != name:
            raise ValueError('Expected flat cooked furniture filenames for the private library')
        dest = target / name
        shutil.copyfile(source, dest)
        if sha(dest) != selected[name]:
            raise ValueError('Private editor library copy verification failed: ' + name)
        files[dest.relative_to(project).as_posix()] = selected[name]
    provenance = dict(schema='valwood.private-finward-model-library', version=1,
                      files=files, bytes=total,
                      source_product='https://assetstore.unity.com/packages/3d/props/interior/house-props-furniture-vol-2-265523',
                      source_cook_manifest_sha256=assets['manifests']['COOK_REVIEW.json'],
                      license_note='Purchased Unity Asset Store content for the purchaser project. No public asset redistribution is authorized by this staging tool.',
                      policy='Reviewed near/mid core GLBs copied into this private editor project; original purchased sources and shared model banks unchanged.')
    relative = 'models/finward_house_props/library.json'
    (project / relative).write_bytes(canonical(provenance))
    return dict(installed=True, models=len(files), bytes=total, files=files,
                manifest=relative), set(files) | {relative}


def build(source, destination):
    # Import only when a real CPU staging build is requested. Completed helper
    # and cooked asset hashes are captured before any authoring begins.
    import build_opening_prologue as home_builder
    from compact_opening_home import split_opening_home_glb, resize_home_glb_textures

    tools_before = tool_snapshot()
    assets_before = asset_snapshot()
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
    required = ('opening/game-start.settings', 'opening/home.physics',
                'opening/cinematic.settings', 'opening/cinematic.json',
                'opening/layout.json', 'opening/art-upgrade.json')
    if any((source / name).is_symlink() or not (source / name).is_file() for name in required):
        raise ValueError('Accepted story/cinematic/art support is missing or linked')
    if (source / 'opening/game-start.settings').read_bytes() != b'VALWOOD_GAME_START 1\n':
        raise ValueError('The console New Game story must be enabled')
    settings_before = (source / 'opening/cinematic.settings').read_bytes()
    old_layout = json.loads((source / 'opening/layout.json').read_bytes())
    inherited_art = json.loads((source / 'opening/art-upgrade.json').read_bytes())
    before = project_snapshot(source)
    runtimes_before = runtime_snapshot()
    origin_hash = sha(source_origin)
    destination.parent.mkdir(parents=True, exist_ok=True)
    stage = Path(tempfile.mkdtemp(prefix='.opening-finward-', dir=destination.parent))
    try:
        project = stage / 'project'
        copied = isolate_project(source, project)
        private_opening_models(project, source)
        private_scenery(project, source)
        library, library_allowed = install_library(project, assets_before)
        scratches, layouts = {}, {}
        original_material_builder = home_builder.build_materials
        home_builder.build_materials = lambda _ignored: original_material_builder(stage / 'materials')
        try:
            for tier in ('near', 'mid'):
                scratch = stage / ('scratch-' + tier)
                scratch.mkdir()
                scratches[tier] = scratch
                layouts[tier] = home_builder.author_house(
                    scratch, game_start_story=True, art_upgrade=True, finward_tier=tier)
                if 'finward' not in layouts[tier].get('dressing', {}):
                    raise ValueError('Purchased furniture hook did not run for ' + tier)
        finally:
            home_builder.build_materials = original_material_builder
        support_checks = {}
        for tier, scratch in scratches.items():
            # Lighting was already patched in the accepted source; it is copied
            # exactly, never patched a second time by this wrapper.
            for path in (scratch / 'opening').glob('*.settings'):
                relative = path.relative_to(scratch).as_posix()
                support_checks[tier + ':' + relative] = path.read_bytes() == (source / relative).read_bytes()
        if not support_checks or not all(support_checks.values()):
            raise ValueError('Fresh furniture authoring changed story/spawn/room-switch settings: ' +
                             json.dumps(support_checks))
        near, mid = layouts['near'], layouts['mid']
        for key in ('house_origin', 'floor_world_y'):
            if near[key] != old_layout[key] or mid[key] != old_layout[key]:
                raise ValueError('Furniture authoring changed the accepted house frame: ' + key)
        for key in ('collision_boxes', 'collision_bodies', 'house_origin', 'floor_world_y'):
            if not comparable(near[key], mid[key]):
                raise ValueError('Near/mid collision/layout mismatch: ' + key)
        near_physics = scratches['near'] / 'opening/home.physics'
        mid_physics = scratches['mid'] / 'opening/home.physics'
        if near_physics.read_bytes() != mid_physics.read_bytes():
            raise ValueError('Near/mid furniture physics must be byte-identical')
        assembly_reports, assembly_allowed, render_meshes = {}, set(), None
        for tier in ('near', 'mid'):
            fresh = scratches[tier] / 'models/opening/home.glb'
            raw = fresh.read_bytes()
            texture_review = None
            if tier == 'near':
                # Retain 1K fabrics, toilet, stove and cabinet/worktop details.
                # These five small or plain surfaces do not need 1K base and
                # normal maps. Recover 40 MiB at the measured loading peak on
                # the 3 GB card without increasing the runtime texture budget.
                raw, texture_review = resize_home_glb_textures(raw, {
                    '*Finward Bed_woodenframes': 512,
                    '*Finward Sidetables': 512,
                    '*Finward Lamps_table_on': 512,
                    '*Finward Cabinets_glass': 512,
                    '*Finward Fridge': 512,
                })
            parts, report = split_opening_home_glb(raw)
            if texture_review is not None:
                report['texture_optimization'] = texture_review
            assembly_reports[tier] = report
            records = [dict(path='models/opening/' + item['stem'] + '.glb',
                            offset=item['offset']) for item in report['render_meshes']]
            if render_meshes is None:
                render_meshes = records
            elif records != render_meshes:
                raise ValueError('Near/mid modular origins differ')
            for stem, raw in parts.items():
                filename = stem + ('_mid' if tier == 'mid' else '') + '.glb'
                output = project / 'models/opening' / filename
                output.write_bytes(raw)
                assembly_allowed.add(output.relative_to(project).as_posix())
        registration = register_home_parts(project, render_meshes)
        shutil.copyfile(near_physics, project / 'opening/home.physics')
        collision_changed = sha(project / 'opening/home.physics') != before['opening/home.physics']
        near = relocate_metadata(near, stage, destination)
        mid = relocate_metadata(mid, stage, destination)
        near['render_meshes'] = render_meshes
        mid['render_meshes'] = render_meshes
        near['render_assembly'] = assembly_reports['near']
        mid['render_assembly'] = assembly_reports['mid']
        near['glb_bytes'] = sum((project / item['path']).stat().st_size for item in render_meshes)
        near['dressing']['finward']['placement_indices_scope'] = 'Pre-split composition indices; render_assembly records the lossless active model groups.'
        (project / 'opening/layout.json').write_bytes(canonical(near))
        for relative in ('opening/cinematic.settings', 'opening/cinematic.json'):
            (project / relative).unlink()
        cinematic = write_home_cinematic(project)
        if (project / 'opening/cinematic.settings').read_bytes() != settings_before:
            raise ValueError('Furniture authoring changed accepted camera rails/timing')
        cinematic.update(geometry_modified=True, collision_modified=collision_changed,
                         original_camera_settings_retained=True,
                         policy='Accepted camera rails/timing retained after actual-triangle and blocker audit over purchased furniture. Furniture collision proxies updated where needed; no father/body/hand animation or voice added.')
        (project / 'opening/cinematic.json').write_bytes(canonical(cinematic))
        # Retain the prior pass and its lighting provenance, while replacing its
        # current-home identity so it never falsely names the previous GLB.
        art = copy.deepcopy(inherited_art)
        art.update(version=2, home_sha256=sha(project / 'models/opening/home.glb'),
                   home_mid_sha256=sha(project / 'models/opening/home_mid.glb'),
                   layout_sha256=sha(project / 'opening/layout.json'),
                   geometry_modified=True, collision_unchanged=not collision_changed,
                   camera_settings_unchanged=True,
                   inherited_from=dict(path=str(source / 'opening/art-upgrade.json'),
                                       sha256=before['opening/art-upgrade.json'],
                                       previous_home_sha256=before['models/opening/home.glb']),
                   scope='Inherited free apartment art and evening lighting; purchased furniture composition is documented separately in opening/finward.json. Detailed prior-pass texture slot evidence describes that pass; current model hashes above describe this build.')
        (project / 'opening/art-upgrade.json').write_bytes(canonical(art))
        finward = dict(schema='valwood.opening-finward-furniture', version=1,
                       near=near['dressing']['finward'], mid=mid['dressing']['finward'],
                       inputs=assets_before, source_project=str(source),
                       previous_art_sha256=before['opening/art-upgrade.json'],
                       home_sha256=sha(project / 'models/opening/home.glb'),
                       home_mid_sha256=sha(project / 'models/opening/home_mid.glb'),
                       home_physics_sha256=sha(project / 'opening/home.physics'),
                       layout_sha256=sha(project / 'opening/layout.json'),
                       furniture_collision_modified=collision_changed,
                       editor_library=library,
                       render_assembly=assembly_reports, modular_registration=registration,
                       near_mid_physics_identical=True, near_mid_layout_tolerance_m=1e-6,
                       camera_settings_unchanged=True, source_lighting_retained_exactly=True,
                       scope='CPU source placement/collider/camera audit only; native visual, walking, performance and Windows acceptance remain pending.')
        (project / 'opening/finward.json').write_bytes(canonical(finward))
        for scratch in scratches.values():
            shutil.rmtree(scratch)
        if any((project / 'saves').iterdir()):
            raise ValueError('Construction must leave private saves empty')
        after = project_snapshot(project)
        preserved = preservation(before, after, ALLOWED | library_allowed | assembly_allowed)
        other_physics = {name: after.get(name) == value for name, value in before.items()
                         if name.endswith('.physics') and name != 'opening/home.physics'}
        if not other_physics or not all(other_physics.values()):
            raise ValueError('Physics outside the apartment furniture changed')
        source_after, runtimes_after = project_snapshot(source), runtime_snapshot()
        tools_after, assets_after = tool_snapshot(), asset_snapshot()
        if source_after != before or sha(source_origin) != origin_hash:
            raise ValueError('Accepted source or opening-origin changed during staging')
        if runtimes_after != runtimes_before:
            raise ValueError('An accepted runtime/source changed during staging')
        if tools_after != tools_before or assets_after != assets_before:
            raise ValueError('An authoring helper or purchased asset changed during staging; rebuild after source completion')
        generated = {path.relative_to(project).as_posix(): sha(path)
                     for path in sorted(project.rglob('*'))
                     if path.is_file() and not path.is_symlink()}
        manifest = dict(schema='valwood.opening-finward-private-copy', version=1, passed=True,
                        source_project=str(source), project=str(destination / 'project'),
                        source_origin=dict(path=str(source_origin), sha256=origin_hash),
                        source_snapshot=before, source_final_snapshot=source_after, source_unchanged=True,
                        runtime_snapshot=runtimes_before, runtime_final_snapshot=runtimes_after, runtime_unchanged=True,
                        copied_mutable_inputs=copied, generated=generated,
                        tools=tools_after, tools_initial_snapshot=tools_before, tools_unchanged=True,
                        purchased_inputs=assets_before, purchased_final_inputs=assets_after,
                        purchased_inputs_unchanged=True, preserved_files=preserved,
                        editor_library=library,
                        protected_other_physics=other_physics,
                        home_physics_modified=collision_changed,
                        near_mid_physics_identical=True, near_mid_layout_tolerance_m=1e-6,
                        fresh_authoring_settings_preserved=support_checks,
                        cinematic_settings_identical=True, cinematic=cinematic,
                        house=near, mid_house=mid, lighting=inherited_art.get('lighting'),
                        private_saved_state_files=0,
                        private_ui_state_not_copied=sorted(PRIVATE_UI_STATE.intersection(before)),
                        scope=finward['scope'],
                        policy='New private opening model/support overlay; other areas, lighting, story, saves, source projects and accepted runtimes untouched. Furniture proxies rebuilt from the selected set; native review remains separate.')
        (stage / 'opening-origin.json').write_bytes(canonical(manifest))
        if destination.exists() or destination.is_symlink():
            raise ValueError('Output appeared while staging')
        stage.rename(destination)
        return dict(passed=True, project=str(destination / 'project'),
                    manifest=str(destination / 'opening-origin.json'),
                    source_unchanged=True, runtime_unchanged=True,
                    purchased_inputs_unchanged=True, other_physics_unchanged=True,
                    home_physics_modified=collision_changed,
                    near_mid_physics_identical=True, cinematic_settings_identical=True,
                    private_saved_state_files=0, home_sha256=generated['models/opening/home.glb'],
                    home_mid_sha256=generated['models/opening/home_mid.glb'],
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
        parser.exit(1, 'Opening Finward staging failed: ' + str(error) + '\n')
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
