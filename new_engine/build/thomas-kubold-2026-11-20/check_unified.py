"""Verify the Thomas/Kubold derivative while preserving its accepted runtime."""
from pathlib import Path
import argparse
import hashlib
import importlib.util
import json
import shlex
import struct
import subprocess
import sys

sys.dont_write_bytecode = True
N = Path(__file__).resolve().parent
E = N.parents[1]
B = N.with_name('camp-atmosphere-2026-11-20')


def need(ok, message):
    if not ok:
        raise RuntimeError(message)


def sha(path):
    with Path(path).open('rb') as handle:
        return hashlib.file_digest(handle, 'sha256').hexdigest()


def read(path):
    return json.loads(Path(path).read_text())


def glb(path):
    path = Path(path)
    with path.open('rb') as handle:
        magic, version, size = struct.unpack('<4sII', handle.read(12))
        length, kind = struct.unpack('<II', handle.read(8))
        need(magic == b'glTF' and version == 2 and size == path.stat().st_size and
             kind == 0x4e4f534a and 2 <= length <= size - 20,
             'Invalid GLB header: ' + str(path))
        return json.loads(handle.read(length))


def owned(path):
    path = Path(path)
    need(path.is_file() and not path.is_symlink() and
         all(not q.is_symlink() for q in path.parents if q.is_relative_to(N)),
         'Candidate payload must be owned: ' + str(path))
    return path


def aliases():
    result = {}
    # rglob does not recurse through the inherited directory aliases.
    for path in sorted(N.rglob('*')):
        if path.is_symlink():
            need(path.exists(), 'Dangling candidate alias: ' + str(path))
            result[str(path.relative_to(N))] = str(path.resolve())
    return result


def payloads():
    paths = [N / name for name in ('BASELINE.json', 'GUIDE.txt', 'launch.py', 'check_unified.py',
                                   'run_native_review.py')]
    paths += sorted(p for p in (N/'tools').rglob('*')
                    if p.is_file() and p.suffix in ('.py', '.cpp') and '__pycache__' not in p.parts)
    paths += [N/'assets/story_hero/THOMAS_BUILD.json']
    paths += [N/'motion'/name for name in ('KUBOLD_SOURCE_SELECTION.json','KUBOLD_EXPORT.json',
                                         'KUBOLD_RIGS.json','kubold_clips.glb',
                                         'export_kubold.py','append_kubold.py')]
    paths += sorted(p for parent in (N/'assets/story_hero/civilian', N/'assets/story_hero/armored', N/'motion/rigs')
                    for p in parent.rglob('*') if p.is_file() and not p.is_symlink())
    return {str(owned(path).relative_to(N)): sha(path) for path in paths}


def source_inputs():
    """Bind the derivatives to their actual geometry, materials and captures."""
    inputs = {}
    report = read(owned(N/'assets/story_hero/THOMAS_BUILD.json'))
    need(report.get('passed') is True, 'Thomas conversion did not pass')
    for path, expected in report['outputs'].items():
        need(Path(path).is_file() and sha(path) == expected, 'Thomas conversion output changed: '+path)
    source = Path(report['source'])
    need(sha(source) == report['source_sha256'], 'Thomas extracted geometry changed')
    inputs[str(source)] = report['source_sha256']
    canonical = B/'motion/rigs/sword.glb'
    need(sha(canonical) == report['canonical_sha256'], 'Thomas canonical fit rig changed')
    inputs[str(canonical)] = report['canonical_sha256']
    inspection = source.parent
    fbx = inspection.parent/'thomas/Assets/David Grette/Adventure_Pack/Thomas/FBX/Thomas/SK_Thomas.fbx'
    need(sha(fbx) == report['source_fbx_sha256'], 'Purchased Thomas source geometry changed')
    inputs[str(fbx)] = report['source_fbx_sha256']
    for name in ('thomas_geometry.json','materials_converted.json','unity_materials.json',
                 'dump_thomas_geometry.py','convert_thomas_materials.py'):
        path = inspection/name
        inputs[str(path)] = sha(path)
    # Source material files and both source/converted maps are conversion inputs.
    materials = read(inspection/'materials_converted.json')['materials']
    for row in materials.values():
        candidates = [row.get('unity_material')]
        candidates += list(row.get('source_textures', {}).values())
        candidates += list(row.get('textures', {}).values())
        for value in candidates:
            if not value:
                continue
            path = Path(value if isinstance(value, str) else value['path'])
            if not path.is_absolute():
                path = inspection/path
            inputs[str(path)] = sha(path)
    for name in ('KUBOLD_EXPORT.json','KUBOLD_RIGS.json'):
        receipt = read(owned(N/'motion'/name))
        need(receipt.get('passed') is True, 'Kubold conversion did not pass: '+name)
        for path, expected in receipt['inputs'].items():
            need(Path(path).is_file() and sha(path) == expected, 'Kubold conversion input changed: '+path)
            inputs[path] = expected
        for path, expected in receipt.get('outputs', {}).items():
            need(Path(path).is_file() and sha(path) == expected, 'Kubold conversion output changed: '+path)
    rigs = read(N/'motion/KUBOLD_RIGS.json')['rigs']
    need({row['name'] for row in rigs} == {'sword','sword_shield','great_axe','staff'},
         'Kubold derivative must retain all four equipped rigs')
    for row in rigs:
        need(Path(row['path']) == N/'motion/rigs'/(row['name']+'.glb') and
             sha(row['path']) == row['sha256'] and
             row.get('unreplaced_clips_geometry_materials_skin_bind_exact') is True,
             'Kubold rig receipt changed: '+row['name'])
    return inputs


def verify_inputs(*, inherited=True):
    baseline = read(N/'BASELINE.json')
    need(baseline.get('schema') == 'valwood.thomas-kubold.baseline.v1' and
         baseline.get('baseline') == str(B) and
         baseline.get('baseline_ready_sha256') == sha(B/'UNIFIED_READY.json'),
         'Accepted runtime baseline changed')
    need(sha(N/'runtime/rpg') == baseline.get('binary_sha256') == sha(B/'runtime/rpg'),
         'Candidate must use the exact accepted campsite binary')
    need((N/'source-v1').is_symlink() and
         (N/'source-v1').resolve() == (B/'source-v1').resolve(),
         'Candidate must use the exact accepted source snapshot')
    for name in ('shaders', 'character-textures'):
        need((N/'shared'/name).is_symlink() and
             (N/'shared'/name).resolve() == (B/'shared'/name).resolve(),
             'Inherited resource bank changed: ' + name)
    if inherited:
        proc = subprocess.run([sys.executable, str(B/'check_unified.py')], cwd=E,
                              capture_output=True, text=True)
        need(proc.returncode == 0, 'Accepted campsite integrity failed: ' + proc.stdout + proc.stderr)
    appearances = {}
    for outfit in ('civilian','armored'):
        body = owned(N/'assets/story_hero'/outfit/'body.glb')
        appearance = owned(body.with_name('appearance.cfg'))
        need('body "body.glb"' in appearance.read_text(), 'Thomas appearance must use its owned body')
        document = glb(body)
        need(document.get('meshes') and document.get('skins') and not document.get('animations'),
             'Thomas appearance must contain skinned geometry and no animation replacement')
        appearances[outfit] = str(appearance)
    hero = read(owned(N/'motion/rigs/HERO_ASSET.json'))
    rig = Path(hero['path'])
    need(rig.is_relative_to(N/'motion/rigs') and rig == rig.resolve() and
         sha(owned(rig)) == hero['sha256'], 'Candidate hero rig is missing or changed')
    clips = [clip.get('name') for clip in glb(rig).get('animations', [])]
    for name in ('Idle', 'Walk_Fwd', 'Run', 'Attack_Left', 'Attack_Right', 'Attack_High',
                 'Attack_Low', 'Attack_Thrust', 'Equipment_Stow', 'Equipment_Relaxed',
                 'Equipment_Draw', 'Story_UnarmedIdle', 'Story_UnarmedWalkFull',
                 'Story_UnarmedRunFull', 'Story_SwimIdle', 'Story_SwimForward', 'Story_Death'):
        need(clips.count(name) == 1, 'Candidate hero requires exactly one clip: ' + name)
    loadouts = owned(N/'motion/rigs/loadouts.manifest')
    entries = [shlex.split(line) for line in loadouts.read_text().splitlines()[1:] if line and line != 'end']
    expected = [[str(code),str(N/'motion/rigs'/(name+'.glb')),prefix]
                for code,name,prefix in ((2,'sword_shield','Shield_'),(3,'great_axe','Axe_'),(4,'staff','Staff_'))]
    need(entries == expected, 'Candidate must select all four owned Kubold rigs')
    spec = importlib.util.spec_from_file_location('thomas_candidate_launch', N/'launch.py')
    launch = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(launch)
    shader_bank = {str(path.relative_to(N/'shared/shaders')):sha(path)
                   for path in (N/'shared/shaders').rglob('*') if path.is_file()}
    for mode in launch.MODES:
        command, env, project = launch.configuration(mode)
        need(command[0] == str(N/'runtime/rpg') and Path(command[0]).resolve() == (B/'runtime/rpg').resolve(),
             'Mode uses another executable: ' + mode)
        actual_shaders = {str(path.relative_to(project/'shaders')):sha(path)
                          for path in (project/'shaders').rglob('*') if path.is_file()}
        need(actual_shaders == shader_bank,
             'Mode uses another shader bank: ' + mode)
        if mode in ('demo', 'world', 'editor'):
            need(env['VALWOOD_STORY_CIVILIAN_APPEARANCE'] == appearances['civilian'],
                 'Mode does not select Thomas: ' + mode)
            need(env['VALWOOD_STORY_ARMORED_APPEARANCE'] == appearances['armored'],
                 'Mode does not retain Thomas when armor equips: ' + mode)
        if mode != 'joust':
            need(env['VALWOOD_MODULAR_KNIGHT'] == str(rig), 'Mode uses another motion bank: ' + mode)
    return dict(binary_sha256=sha(N/'runtime/rpg'), payload_sha256=payloads(),
                source_input_sha256=source_inputs(), aliases=aliases())


def check():
    ready = read(N/'UNIFIED_READY.json')
    need(ready.get('ready') is True and ready.get('generation') == str(N) and
         ready.get('schema') == 'valwood.thomas-kubold.ready.v1', 'Thomas generation is not frozen')
    current = verify_inputs()
    need(all(ready.get(key) == value for key, value in current.items()),
         'Thomas/Kubold inputs changed after verification; rerun QA and freeze')
    return ready


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--inputs-only', action='store_true')
    parser.add_argument('--check-only', action='store_true')
    args = parser.parse_args()
    try:
        verify_inputs() if args.inputs_only else check()
        print('Thomas/Kubold candidate: exact accepted native runtime and selected owned character assets verified.')
    except Exception as error:
        print(str(error), file=sys.stderr)
        sys.exit(1)
