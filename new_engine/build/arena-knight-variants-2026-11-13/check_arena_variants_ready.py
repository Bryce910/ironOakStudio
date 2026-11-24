#!/usr/bin/env python3
"""Fail closed unless this private Linux arena and its actual proofs still match.

--refresh is an explicit release step. It never launches or closes a game.
--check-only is the public launcher's read-only preflight.
"""
from pathlib import Path
from datetime import datetime, timezone
import argparse
import hashlib
import json
import os
import re
from urllib.parse import unquote, urlsplit
import struct
import subprocess
import sys

sys.dont_write_bytecode = True
BASE = Path(__file__).resolve().parent
ENGINE = BASE.parents[1]
WORKSPACE = ENGINE.parent
SOURCE = BASE / 'source-v1'
BANK = BASE / 'pose-cook/bank/models'
PROJECT = BASE / 'arena-project'
BINARY = BASE / 'runtime-native-v1/native-bin/rpg'
PUBLIC = WORKSPACE / 'Launch_Valwood_Knight_Arena_Variants_Test.sh'
READY = BASE / 'ARENA_VARIANTS_READY.json'
SCHEMA = 'valwood.arena-knight-variants.private-ready.v1'
ASSET = ENGINE / 'assets/purchased/modular_knights/derived/polish-v4.1/modular_knight_hero_v4_1.glb'
# Exact accepted hero hash; no accepted source or existing launcher is rewritten.
ASSET_SHA = '80687a8d3710c5e82feaa368b31b9f8b82721927b0bf874d7c8f191e0bc5077d'
PRIOR = {
 'Launch_Valwood_New_Knight_Test.sh':'8aa5402d3410cc3ed8449e630c29333f6b996c609ebd47e15891b847c03cdad5',
 'Launch_Valwood_Knight_Locomotion_Test.sh':'5c1f77335d07ad6f492cf6090ea05454ecbaf877487663d3d83a6887c66ff8ab',
 'Launch_Valwood_Knight_Combat_Test.sh':'4354b86c2b749caeb470a4e336a8b6864dbd100e7f6ed2809e7989324b52d8ba',
}


def require(ok, message):
    if not ok:
        raise RuntimeError(message)


def signature(path):
    stat = path.stat()
    return (stat.st_dev, stat.st_ino, stat.st_size, stat.st_mtime_ns, stat.st_ctime_ns)


class Closure:
    def __init__(self):
        self.pins = {}
        self.resolved = {}
        self.stats = {}
        self.cache = {}

    def pin(self, value, expected=None):
        path = Path(value).absolute()
        require(path.is_file(), 'Missing verified file: ' + str(path))
        target = path.resolve(strict=True)
        before = signature(path)
        key = (str(target), before)
        actual = self.cache.get(key)
        if actual is None:
            with path.open('rb') as stream:
                actual = hashlib.file_digest(stream, 'sha256').hexdigest()
            require(signature(path) == before and path.resolve(strict=True) == target,
                    'File changed during verification: ' + str(path))
            self.cache[key] = actual
        require(expected is None or expected == actual, 'Changed verified input: ' + str(path))
        name = str(path)
        require(name not in self.pins or self.pins[name] == actual, 'Conflicting proof input: ' + name)
        self.pins[name] = actual
        self.resolved[name] = str(target)
        self.stats[name] = before
        return actual

    def hashes(self, values, root=None, nonempty=True):
        require(isinstance(values, dict) and (bool(values) or not nonempty), 'Missing dependency hash map')
        for name, expected in values.items():
            path = Path(name)
            require(isinstance(expected, str) and len(expected) == 64, 'Invalid dependency digest: ' + name)
            if not path.is_absolute():
                require(root is not None, 'Relative proof dependency has no root: ' + name)
                path = Path(root) / path
            self.pin(path, expected)

    def rows(self, rows):
        require(isinstance(rows, list) and rows, 'Missing dependency hash rows')
        for row in rows:
            self.pin(row['path'], row['sha256'])

    def proof(self, relative, accepted=None, schema=None):
        path = BASE / relative
        self.pin(path)
        data = json.loads(path.read_text())
        require(isinstance(data, dict), 'Invalid proof object: ' + str(path))
        if accepted:
            require(data.get(accepted) is True, 'Proof is not accepted: ' + str(path))
        if schema:
            require(data.get('schema') == schema, 'Wrong proof schema: ' + str(path))
        require(self.pin(path) == self.pins[str(path)], 'Proof changed during parsing: ' + str(path))
        return data

    def stable(self):
        for name, before in self.stats.items():
            path = Path(name)
            require(path.is_file() and signature(path) == before and
                    str(path.resolve(strict=True)) == self.resolved[name],
                    'Release input changed during verification: ' + name)


def bank_paths():
    require(BANK.is_dir(), 'The private purchased-knight bank is missing.')
    paths = {}
    for dirname, dirs, files in os.walk(BANK, followlinks=True):
        here = Path(dirname)
        # Reject directory cycles and dependencies escaping this engine workspace.
        target = here.resolve(strict=True)
        require(target.is_relative_to(ENGINE), 'Model directory escapes the engine workspace: ' + str(here))
        ancestor = here.parent
        while ancestor.is_relative_to(BANK):
            require(ancestor.resolve(strict=True) != target, 'Cyclic model directory: ' + str(here))
            if ancestor == BANK:
                break
            ancestor = ancestor.parent
        dirs.sort()
        for filename in sorted(files):
            path = here / filename
            require(path.is_file(), 'Broken model-bank reference: ' + str(path))
            require(path.resolve(strict=True).is_relative_to(ENGINE), 'Model reference escapes the engine workspace: ' + str(path))
            paths[str(path.relative_to(BANK))] = path
    require(len(paths) >= 1000, 'The full private model bank is incomplete.')
    return paths


def check_binding():
    require((PROJECT / 'models').resolve(strict=True) == BANK.resolve(strict=True),
            'The private project no longer selects the verified knight bank.')



def pin_loaded_model(closure, path):
    """Pin a native-loaded model plus every external buffer/image it references."""
    path = Path(path).absolute()
    closure.pin(path)
    if path.suffix.lower() == '.glb':
        with path.open('rb') as stream:
            magic, version, total = struct.unpack('<4sII', stream.read(12))
            require(magic == b'glTF' and version == 2, 'Invalid loaded GLB: ' + str(path))
            length, kind = struct.unpack('<II', stream.read(8))
            require(kind == 0x4e4f534a and length <= 16 * 1024 * 1024, 'Invalid loaded GLB JSON chunk: ' + str(path))
            document = json.loads(stream.read(length))
    else:
        document = json.loads(path.read_text())
    for item in document.get('images', []) + document.get('buffers', []):
        uri = item.get('uri')
        if not uri or uri.startswith('data:'):
            continue
        parts = urlsplit(uri)
        require(not parts.scheme and not parts.netloc, 'Remote resource in loaded arena model: ' + uri)
        # Match world_model_image_bytes: borrowed GLBs are file symlinks,
        # and their ../textures/shared paths belong to the resolved bank.
        resource = (path.resolve(strict=True).parent / unquote(parts.path)).absolute()
        require(resource.resolve(strict=True).is_relative_to(ENGINE), 'Loaded model resource escapes engine workspace.')
        closure.pin(resource)


def pin_native_loaded_models(closure, log):
    loaded = []
    for value in re.findall(r'^\[WorldModels\] (.+\.(?:glb|gltf)): [0-9]+ meshes', Path(log).read_text(), re.M):
        path = Path(value)
        if not path.is_absolute():
            path = PROJECT / path if (PROJECT / path).is_file() else ENGINE / path
        require(path.is_file(), 'A native-loaded arena model is missing: ' + str(path))
        pin_loaded_model(closure, path)
        loaded.append(str(path.absolute()))
    require(loaded, 'Native smoke has no actual loaded model paths.')
    # Bow models are an arena resource directory outside the models bank.
    arena = PROJECT / 'arena_assets'
    require(arena.is_dir(), 'Private arena assets are missing.')
    for path in sorted(arena.rglob('*')):
        if path.is_file():
            closure.pin(path)
            if path.suffix.lower() in ('.glb', '.gltf'):
                pin_loaded_model(closure, path)
    return sorted(set(loaded))


def project_bootstrap_files():
    mutable = {'graphics.settings', 'player-profile.config', 'studio-layout.ini', 'world.studio.workspace'}
    rows = {}
    for dirname, dirs, files in os.walk(PROJECT, followlinks=False):
        here = Path(dirname)
        dirs[:] = [name for name in dirs if name not in {'saves', 'profiles', 'models'} and not (here / name).is_symlink()]
        for name in files:
            if name in mutable or name.endswith(('.progress', '.tmp', '.workspace')):
                continue
            path = here / name
            if path.is_file():
                rows[str(path.relative_to(PROJECT))] = path
    return rows

def prior_preflights(closure):
    results = {}
    for filename, expected in PRIOR.items():
        path = WORKSPACE / filename
        closure.pin(path, expected)
        result = subprocess.run([str(path), '--check-only'], capture_output=True, text=True, timeout=180)
        require(result.returncode == 0, 'Previous accepted knight preflight failed: ' + filename + '\n' + result.stderr.strip())
        closure.pin(path, expected)
        results[filename] = dict(returncode=0, stdout=result.stdout.strip(), launcher_sha256=expected)
    return results


def refresh():
    c = Closure()
    check_binding()
    native = c.proof('runtime-native-v1/READY.json', 'ready')
    require(native.get('native_only') is True and native.get('Windows_work_performed') is False and
            native.get('application_units') == 102 and native.get('full_application_closure_rebuilt') is True,
            'Incomplete native application closure.')
    require(Path(native['binary']).resolve() == BINARY.resolve() and Path(native['source']).resolve() == SOURCE.resolve(),
            'Native proof belongs to another candidate.')
    c.hashes(native['source_sha256'], SOURCE)
    require(set(native['source_sha256']) == {str(p.relative_to(SOURCE)) for p in SOURCE.rglob('*') if p.is_file()},
            'The private source inventory changed after the native build.')
    c.pin(BINARY, native['sha256'])
    plan = c.proof('runtime-native-v1/BUILD_PLAN.json')
    require(len(plan['units']) == 102 and plan.get('skip_player') is False, 'Incomplete native build plan.')
    c.pin(ENGINE / 'build/physics/libJolt.a', plan['jolt_library_sha256'])
    for unit in plan['units']:
        receipt_path = Path(unit['target']).with_suffix('.json')
        # Native builder uses foo.cpp.json beside foo.cpp.o.
        receipt = json.loads(receipt_path.read_text())
        c.pin(receipt_path)
        require(receipt['logical_source'] == unit['logical_source'] and receipt['command'] == unit['command'],
                'Native unit receipt differs from its build plan.')
        c.hashes(receipt['actual_dependency_sha256'])
    c.pin(ASSET, ASSET_SHA)
    origin = c.proof('SOURCE_ORIGIN.json')
    require(origin.get('canonical_source_edited') is False, 'Private source origin is not isolated.')
    baseline_source = Path(origin['baseline'])
    c.pin(baseline_source.parent / 'COMBAT_POLISH_READY.json', origin['baseline_ready_sha256'])

    intake = c.proof('assets-intake/FINAL_INTAKE.json', 'passed', 'valwood.arena-knight-final-intake.v1')
    c.rows(intake['dependencies'])
    require(intake['lod_before_after']['morphs_unchanged'] and intake['geometry']['max_discarded_weight'] == 0 and
            intake['materials']['rgba8_full_mips_mib'] <= 100, 'Purchased asset fitting or texture budget failed.')
    cook = c.proof('pose-cook/POSE_BANK_READY.json', 'ready', 'valwood.purchased-knight-enemy-bank.v1')
    require(cook['source_modified'] is False and cook['poses'] == 172 and 1 <= cook['mesh_count'] <= 32,
            'Incomplete purchased-knight pose bank.')
    c.hashes(cook['inputs'])
    c.hashes(cook['files'], BASE / 'pose-cook')
    owned = c.proof('pose-cook/OWNED_BANK_CHECK.json', 'passed', 'valwood.purchased-knight-enemy-bank-check.v1')
    require(owned['poses'] == 172 and owned['mesh_count'] == cook['mesh_count'] and
            owned['sword_attribute_index_streams_bit_exact'] is True and owned['topology_and_anatomy_signatures_exact'] is True and
            owned['standing_boot_sole_max_error_m'] < .00001 and owned['unique_decoded_RGBA8_full_mips_MiB'] <= 100,
            'Owned bank layout, original sword, support or texture checks failed.')
    c.hashes(owned['inputs'])
    for path, row in owned['image_files'].items():
        c.pin(path, row['sha256'])
    sockets = c.proof('pose-cook/SOCKETS_CHECK.json', 'passed',
                      'valwood.purchased-knight-enemy-socket-closure.v1')
    require(sockets['poses'] == 172 and sockets['source_sha256'] == sockets['destination_sha256'] and
            Path(sockets['destination']).resolve() == (BANK / 'longsword_mocap/combat_weapon_sockets.bin').resolve(),
            'Weapon socket closure is incomplete or changed the accepted blade endpoints.')
    c.hashes(sockets['dependencies_sha256'])
    c.pin(sockets['source'], sockets['source_sha256'])
    c.pin(sockets['destination'], sockets['destination_sha256'])
    require(str(Path(sockets['destination']).relative_to(BASE / 'pose-cook')) in cook['files'],
            'The producer pose-bank receipt omits its required weapon sockets.')
    contact = c.proof('pose-cook/CONTACT_CHECK.json', 'passed',
                      'valwood.purchased-knight-enemy-contact-check.v1')
    require(contact['actual_new_anatomy_used'] is True and contact['body_and_steel_checks'] == 320 and
            contact['guard_endpoint_samples'] >= 120 and contact['guard_endpoint_max_error_m'] < .00001 and
            contact['before_sha256'] == contact['after_sha256'],
            'Final purchased-body contact checks are incomplete or ran on changing inputs.')
    c.hashes(contact['before_sha256'])
    c.hashes(contact['reports_sha256'])
    c.pin(BASE / 'pose-cook/contact-review/contact-fixture', contact['binary_sha256'])
    fit = c.proof('pose-cook/OFFLINE_FIT_REVIEW.json', 'accepted')
    c.hashes(fit['dependencies_sha256'])
    layout = c.proof('pose-cook/BODY_LAYOUT.json')
    require(layout['poses'] == 172 and layout['mesh_count'] == cook['mesh_count'] and len(layout['layout']) == cook['mesh_count'],
            'Body layout and pose bank disagree.')
    for pose in range(172):
        for path in (BANK / f'longsword_mocap/combat_body_{pose:03}.glb', BANK / f'combat_anatomy/caps/cap_{pose:03}.glb'):
            require(str(path.relative_to(BASE / 'pose-cook')) in cook['files'], 'Unproven owned pose: ' + str(path))
    anatomy = BANK / 'combat_anatomy/anatomy.bin'
    with anatomy.open('rb') as stream:
        header = stream.read(20)
    require(header[:8] == b'VWANA002' and struct.unpack_from('<III', header, 8) == (172, cook['mesh_count'], 8),
            'Anatomy metadata does not match the actual union layout.')
    player = c.proof('PLAYER_STREAM_CORRESPONDENCE.json', 'accepted')
    require(player['first_person_payloads_preserved'] and player['other_loadout_payloads_preserved'] and
            player['bank0_baked_body_overrides_disabled'] and player['new_body_vertices'] == cook['vertices_per_union'] and
            player['body_counts'] == [x['vertices'] for x in layout['layout']], 'Player streams do not match the new body layout.')
    c.hashes(player['dependencies_sha256'])
    borrowed = c.proof('BANK_BORROWED_REFERENCES.json')
    require(borrowed['changed_body_anatomy_and_sockets_not_seeded'] is True, 'Unsafe inherited model-bank staging.')
    stream_exceptions = {'hero_grips/arm_grips.bin', 'hero_locomotion/directional_locomotion.bin'}
    for relative in borrowed['added_unrelated_references']:
        path = BANK / relative
        ancestor = Path(borrowed['baseline']) / relative
        if relative in stream_exceptions:
            require(relative in player['records'] and path.resolve().is_relative_to(BANK.resolve()) and not path.is_symlink(),
                    'A player-stream exception is not a verified private replacement: ' + relative)
            c.pin(ancestor, player['records'][relative]['before_sha256'])
            c.pin(path, player['records'][relative]['after_sha256'])
        else:
            # Texture intake requires regular files, so byte-identical private
            # copies are valid in addition to the ordinary borrowed symlinks.
            if ancestor.is_dir():
                require(path.is_dir() and path.is_symlink() and
                        path.resolve(strict=True) == ancestor.resolve(strict=True),
                        'A borrowed model directory was redirected: ' + relative)
                # Its recursive logical/resolved/size inventory is checked without hashing unrelated asset bytes.
            else:
                c.pin(path, c.pin(ancestor))


    implementation = c.proof('RUNTIME_IMPLEMENTATION.json')
    require(implementation['canonical_and_accepted_sources_unchanged'] is True and
            implementation['selected_actor_mask'] == [1, 2, 4, 8, 1, 2, 4, 8], 'Stable arena appearance dispatch is missing.')
    c.hashes(implementation['source_sha256'])
    c.hashes(implementation['baseline_sha256'])
    build = c.proof('runtime-unit-checks/SUPPORT_BUILD_INPUTS.json', 'passed')
    require(build['before_sha256'] == build['after_sha256'], 'Support test inputs changed during its build.')
    c.hashes(build['before_sha256'])
    c.pin(BASE / 'runtime-unit-checks/test_model_variant_support', build['binary_sha256'])
    support = c.proof('runtime-unit-checks/SUPPORT_TEST_REPORT.json', 'passed')
    require(all(support.get(k) is True for k in ('actual_gl_model_support','actual_gl_selected_draw','origin_preserved',
            'legacy_origin_normalization_unchanged','invalid_selections_fail_closed')), 'Runtime mechanism checks are incomplete.')
    c.hashes(support['dependencies_sha256'])
    real = c.proof('runtime-real-checks/REAL_VARIANTS_REVIEW.json', 'accepted',
                   'valwood.arena-knight-variants.real-runtime-review.v1')
    require(real.get('passed') is True, 'Actual purchased-bank runtime checks failed.')
    c.hashes(real['dependencies_sha256'])
    smoke = c.proof('variants-review/WORLD_SMOKE_ACCEPTED.json', 'accepted')
    require(smoke['binary_sha256'] == native['sha256'] and smoke['asset_sha256'] == ASSET_SHA and
            Path(smoke['model_bank']).resolve() == BANK.resolve(), 'Actual game smoke used different release inputs.')
    c.pin(smoke['log'], smoke['log_sha256'])
    c.pin(smoke['capture'], smoke['capture_sha256'])
    if 'dependencies_sha256' in smoke:
        c.hashes(smoke['dependencies_sha256'])

    loaded_models = pin_native_loaded_models(c, smoke['log'])
    bootstrap = project_bootstrap_files()
    for path in bootstrap.values():
        c.pin(path)
    inventory = {}
    critical_targets = {c.resolved[name]: value for name, value in c.pins.items()}
    for relative, path in bank_paths().items():
        target = str(path.resolve(strict=True))
        content = critical_targets.get(target)
        if content is not None:
            c.pin(path, content)
        inventory[relative] = dict(resolved_path=target, size=path.stat().st_size,
                                   sha256=content, symlink=path.is_symlink(),
                                   content_scope='critical-runtime' if content is not None else 'unused-inherited-metadata-only')
    c.pin(BASE / 'launch_knight_candidate.sh')
    c.pin(PUBLIC)
    c.pin(Path(__file__))
    previous = prior_preflights(c)
    c.stable()
    require(set(inventory) == set(bank_paths()), 'The model-bank inventory changed during release verification.')
    result = dict(schema=SCHEMA, accepted=True, accepted_at_utc=datetime.now(timezone.utc).isoformat(),
        scope='Private Linux eight-opponent arena, four purchased Modular Knight appearances, existing polished longsword player.',
        binary=str(BINARY), binary_sha256=native['sha256'], asset=str(ASSET), asset_sha256=ASSET_SHA,
        project_models=str(PROJECT / 'models'), verified_model_bank=str(BANK),
        dependencies_sha256=c.pins, dependency_resolved_paths=c.resolved, model_bank_inventory=inventory,
        content_hash_scope='Producer-owned 172 poses/caps/anatomy/sockets/textures, current real-runtime proof closure, explicit inherited root files, actual native-loaded GLB/image/buffer closure, arena bow assets and local bootstrap resources. Unused inherited directory contents retain logical path/resolved path/size inventory only.',
        native_loaded_models=loaded_models, project_bootstrap_inventory=sorted(bootstrap),
        checks=dict(pose_count=172, body_mesh_count=cook['mesh_count'], variant_masks=[1,2,4,8],
                    native_application_units=102, original_sword_streams_preserved=True,
                    first_person_and_other_loadouts_preserved=True, previous_launchers=previous),
        limitations=['Scripted CPU and offscreen software OpenGL checks; hardware frame pacing and interactive controller feel need player review.',
                     'The separate private test retains its eight-opponent arena baseline; the canonical game and editor remain unchanged.',
                     'Existing source rig and captured animation limitations remain; this pass changes enemy appearances.',
                     'Unused inherited model directory contents are inventoried by logical path, resolved target and size, without hashing unrelated asset bytes on every launch.'],
        windows_work_performed=False)
    temporary = READY.with_suffix('.json.tmp')
    temporary.write_text(json.dumps(result, indent=2) + '\n')
    temporary.replace(READY)
    print(f'Arena knight variants ready: {len(c.pins)} dependencies, {len(inventory)} model-bank files.')


def check_only():
    require(READY.is_file(), 'The arena knight variants are still being prepared; final checks are not released yet.')
    state = json.loads(READY.read_text())
    require(state.get('schema') == SCHEMA and state.get('accepted') is True and state.get('windows_work_performed') is False,
            'The arena knight variants have not passed their final checks.')
    require(state.get('binary') == str(BINARY) and state.get('asset') == str(ASSET) and
            state.get('verified_model_bank') == str(BANK) and state.get('project_models') == str(PROJECT / 'models'),
            'The private arena release points to another project or model bank.')
    check_binding()
    c = Closure()
    c.hashes(state.get('dependencies_sha256'))
    require(c.resolved == state.get('dependency_resolved_paths'), 'A verified dependency symlink was retargeted.')
    paths = bank_paths()
    inventory = state.get('model_bank_inventory')
    require(isinstance(inventory, dict) and set(inventory) == set(paths), 'The verified model-bank inventory changed.')
    for relative, path in paths.items():
        row = inventory[relative]
        require(row['resolved_path'] == str(path.resolve(strict=True)) and row['size'] == path.stat().st_size and
                row['symlink'] == path.is_symlink(), 'Changed model-bank metadata: ' + relative)
        if row['sha256'] is not None:
            require(c.pin(path) == row['sha256'], 'Changed critical model-bank content: ' + relative)
    require(state.get('project_bootstrap_inventory') == sorted(project_bootstrap_files()), 'The private bootstrap resource inventory changed.')
    require(c.pins.get(str(BINARY)) == state['binary_sha256'] and c.pins.get(str(ASSET)) == ASSET_SHA,
            'The verified binary or player asset no longer matches.')
    c.stable()
    print('Arena knight variants preflight passed.')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument('--refresh', action='store_true', help='Explicitly release only after every mandatory final receipt passes.')
    mode.add_argument('--check-only', action='store_true', help='Read-only verification of an existing release.')
    args = parser.parse_args()
    try:
        refresh() if args.refresh else check_only()
    except (OSError, ValueError, KeyError, RuntimeError, subprocess.SubprocessError) as error:
        print('Arena variants preflight: ' + str(error), file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
