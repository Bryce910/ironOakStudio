#!/usr/bin/env python3
"""Build the village's licensed free character imports for the native engine.

Run from the engine root:
  blender -b -t 2 --factory-startup --disable-autoexec --python-exit-code 1 --python tools/build_village_characters.py

Original downloads stay under assets/characters/incoming. Skeleton evaluation,
material conversion and texture atlasing happen offline. Runtime receives 48
bounded static poses, shared textures, and no network or skeleton dependency.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import struct
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'projects/valwood_world/models'
ARCHETYPES = {'knight': (8, 4), 'archer': (0, 4), 'trader': (0, 4),
              'traveler': (8, 4), 'traderwoman': (0, 4), 'travelerwoman': (8, 4)}
SOURCES = {
    'defender': {'page': 'https://opengameart.org/content/animated-defender-aka-brigand',
        'license': 'CC BY 3.0', 'license_url': 'https://creativecommons.org/licenses/by/3.0/',
        'credit': 'Konstantin Maystrenko (concept), AnthonyMyers (model), Danimal (retopology, rigging, animation)',
        'original': 'assets/characters/incoming/danimal_defender/source/Defender.blend'},
    'modular_rpg': {'page': 'https://opengameart.org/content/modular-rpg-characters',
        'license': 'CC0-1.0', 'license_url': 'https://creativecommons.org/publicdomain/zero/1.0/',
        'credit': 'System G6 (Qoma)',
        'original': 'assets/characters/incoming/system_g6_modular/modular_rpg_characters.7z'},
    'sintel': {'page': 'https://github.com/jonburesh/sintelgame',
        'license': 'CC-BY-3.0 art; MIT project modifications',
        'license_url': 'https://durian.blender.org/sharing/',
        'credit': 'Blender Foundation and Sintel The Game contributors',
        'original': 'assets/characters/incoming/newcandidate/sintelgame/sintel_rig_2.blend'},
}
CREDITS = '''Free village character imports
==============================

Animated Defender AKA Brigand
Concept: Konstantin Maystrenko. Model: AnthonyMyers.
Retopology, rigging and animation: Danimal.
Source: https://opengameart.org/content/animated-defender-aka-brigand
License: Creative Commons Attribution 3.0 Unported
https://creativecommons.org/licenses/by/3.0/
Changes for Valwood: legacy materials converted to PBR; images capped at 1024px;
body normalized to 1.82m; root motion removed; supplied walking cycle sampled;
ambient weapon carry and breathing idle authored; static poses exported to GLB.

Modular RPG Characters
Creator: System G6 (Qoma).
Source: https://opengameart.org/content/modular-rpg-characters
License: CC0 1.0 Universal
https://creativecommons.org/publicdomain/zero/1.0/
Changes for Valwood: selected male civilian, vendor and ranger outfits;
diffuse maps atlased; body normalized to 1.82m; source Idle animation sampled;
Run animation adapted into a grounded slower gait (no source Walk clip exists);
authored bow/quiver; static poses exported to GLB.

Sintel female travelers and traders
Art: copyright Blender Foundation | www.sintel.org
Original character modeling: Angela Guenette. Original rig: Nathan Vegdahl.
Game adaptation and supplied walk/idle: Sintel The Game contributors.
Source: https://github.com/jonburesh/sintelgame
Art license: Creative Commons Attribution3.0 Unported
https://durian.blender.org/sharing/
https://creativecommons.org/licenses/by/3.0/
Game project modifications: MIT, copyright2016 Jonathan Buresh.
Changes for Valwood: fixed rest triangulation, bounded source-texture atlases,
secondary-UV AO remapped into primary UV, documented legacy-PBR approximation,
source skeleton clips sampled offline, closed out-and-back idle, sole grounding,
1.82m normalization and source +Y rotated into glTF+Z. No lighting baked in.
Original sources, notices, map hashes and inspection remain under
assets/characters/incoming/newcandidate/sintelgame. Source rig UI/game scripts
are not executed or embedded in runtime GLBs.

Original downloads, source-page evidence, license text and SHA-256 provenance
are retained in assets/characters/incoming. Downloaded source files are kept
unchanged. No purchase or paid asset pack was used for these character imports.
'''

def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def glb(path):
    raw = Path(path).read_bytes()
    assert struct.unpack_from('<III', raw) == (0x46546C67, 2, len(raw))
    size, kind = struct.unpack_from('<II', raw, 12)
    assert kind == 0x4E4F534A
    data = json.loads(raw[20:20+size])
    start = 20+size
    length, kind = struct.unpack_from('<II', raw, start)
    assert kind == 0x004E4942
    return data, raw[start+8:start+8+length]


def expected(selected):
    return {f'village_character_{role}_{mode}_{frame:02}.glb'
        for role in selected for mode, count in zip(('walk', 'idle'), ARCHETYPES[role])
        for frame in range(count)}


def inspect_assets(directory, records):
    """Inspect actual encoded GLBs, independently of cooker mesh counters."""
    textures = {}
    geometry_bytes = 0
    for record in records:
        path = directory / record['file']
        data, binary = glb(path)
        assert not data.get('skins') and not data.get('animations'), path
        assert not data.get('extensionsRequired'), path
        assert 'Icosphere' not in json.dumps(data.get('meshes', [])), path
        primitives = [p for mesh in data['meshes'] for p in mesh['primitives']]
        assert 0 < len(primitives) <= 8, path
        triangles = 0
        for primitive in primitives:
            assert 'material' in primitive and primitive.get('mode', 4) == 4, path
            assert 'COLOR_0' not in primitive['attributes'], path
            indices = data['accessors'][primitive['indices']]
            assert indices['count'] % 3 == 0
            triangles += indices['count']//3
        assert 0 < triangles <= 20000 and triangles == record['triangles'], (path, triangles)
        image_views = set()
        for material in data.get('materials', []):
            pbr = material.get('pbrMetallicRoughness', {})
            views = [(pbr.get('baseColorTexture'), True), (material.get('normalTexture'), False),
                     (pbr.get('metallicRoughnessTexture'), False)]
            for view, srgb in views:
                if not view:
                    continue
                assert view.get('texCoord', 0) == 0 and not view.get('extensions'), path
                texture = data['textures'][view['index']]
                image = data['images'][texture['source']]
                assert 'uri' not in image
                image_views.add(image['bufferView'])
                buffer_view = data['bufferViews'][image['bufferView']]
                offset = buffer_view.get('byteOffset', 0)
                encoded = binary[offset:offset+buffer_view['byteLength']]
                assert encoded[:8] == b'\x89PNG\r\n\x1a\n', path
                width, height = struct.unpack_from('>II', encoded, 16)
                assert 0 < width <= 2048 and 0 < height <= 2048, path
                sampler = data.get('samplers', [])[texture['sampler']] if 'sampler' in texture else {}
                policy = (srgb, sampler.get('minFilter', 9987), sampler.get('magFilter', 9729),
                          sampler.get('wrapS', 10497), sampler.get('wrapT', 10497))
                key = (hashlib.sha256(encoded).hexdigest(), policy)
                textures[key] = {'width': width, 'height': height, 'encoded_bytes': len(encoded)}
        geometry_bytes += sum(view['byteLength'] for i, view in enumerate(data['bufferViews']) if i not in image_views)
        record['primitive_count'] = len(primitives)
        record['sha256'] = sha(path)
        record['bytes'] = path.stat().st_size
    estimated_mib = sum(i['width']*i['height']*4*4/3 for i in textures.values())/1048576
    assert len(textures) <= 24 and estimated_mib <= 96, (len(textures), estimated_mib)
    assert geometry_bytes <= 64*1048576, geometry_bytes
    return {'pose_count': len(records), 'unique_texture_policies': len(textures),
            'estimated_rgba_mips_mib': estimated_mib, 'geometry_bytes': geometry_bytes,
            'max_image_side': max(max(i['width'], i['height']) for i in textures.values()),
            'texture_encoded_bytes': sum(i['encoded_bytes'] for i in textures.values()),
            'max_pose_triangles': max(r['triangles'] for r in records),
            'max_pose_primitives': max(r['primitive_count'] for r in records)}


def cook(selected=None, output=OUT):
    sys.path.insert(0, str(ROOT/'tools'))
    from village_defender import cook as defender
    from village_civilians import cook as civilians
    from build_sintel_characters import cook as sintel
    output = Path(output)
    output.mkdir(parents=True, exist_ok=True)
    selected = set(selected or ARCHETYPES)
    assert selected and selected <= set(ARCHETYPES)
    old = output/'village_character_manifest.json'
    previous = json.loads(old.read_text()) if old.exists() else None
    if selected != set(ARCHETYPES):
        assert previous and previous['schema_version'] == 2, 'Run a complete free-character cook first'
    with tempfile.TemporaryDirectory(prefix='valwood-characters-') as temporary:
        stage = Path(temporary)
        civilian_roles = selected - {'knight','traderwoman','travelerwoman'}
        records = defender(stage, selected) if 'knight' in selected else []
        if civilian_roles:
            records += civilians(stage, civilian_roles)
        female_roles=selected & {'traderwoman','travelerwoman'}
        if female_roles:
            records += sintel(stage,female_roles)
        # Static posed export can retessellate source knight quads. Keep one
        # fixed index topology so adjacent poses can interpolate on the GPU.
        from canonicalize_village_knight_topology import canonicalize_knight_poses
        canonicalize_knight_poses(stage, records)
        assert len({r['file'] for r in records}) == len(records)
        assert {r['file'] for r in records} == expected(selected)
        budget = inspect_assets(stage, records)
        if previous and selected != set(ARCHETYPES):
            for record in previous['assets']:
                if record['role'] not in selected:
                    (stage/record['file']).write_bytes((output/record['file']).read_bytes())
                    records.append(record)
            budget = inspect_assets(stage, records)
        assert {r['file'] for r in records} == expected(ARCHETYPES) and len(records) == 48
        records.sort(key=lambda r: (list(ARCHETYPES).index(r['role']), r['mode'] != 'walk', r['frame']))
        manifest = {'schema_version': 2, 'provenance': 'Downloaded free licensed game models; original source downloads retained unchanged.',
            'sources': {name: {**info, 'sha256': sha(ROOT/info['original'])} for name, info in SOURCES.items()},
            'archetypes': {name: {'walk_poses': counts[0], 'idle_poses': counts[1]} for name, counts in ARCHETYPES.items()},
            'forward': 'glTF +Z (Blender -Y); runtime yaw=atan2(dx,dz)',
            'notes': '48 offline static poses; six source appearances, 54 ambient residents. Source skeleton clips and authored adaptations are described per asset. Runtime remains a bounded scripted crowd, without NPC dialogue, commerce or offline AI.',
            'budget': budget, 'total_bytes': sum(r['bytes'] for r in records), 'assets': records}
        staged_manifest = stage/'village_character_manifest.json'
        staged_manifest.write_text(json.dumps(manifest, indent=2)+'\n')
        # Validate the complete set before replacing any live model. Atomic
        # file renames publish each finished GLB; the manifest is written last.
        for record in records:
            if record['role'] in selected:
                target = output/record['file']
                pending = target.with_suffix('.glb.pending')
                pending.write_bytes((stage/record['file']).read_bytes())
                os.replace(pending, target)
        pending = old.with_suffix('.json.pending')
        pending.write_text(staged_manifest.read_text())
        os.replace(pending, old)
        if output.resolve() == OUT.resolve():
            (ROOT/'projects/valwood_world/CREDITS_CHARACTERS.txt').write_text(CREDITS)
        print('FREE VILLAGE CHARACTER COOK COMPLETE', json.dumps(budget), flush=True)
        return manifest


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--roles', default=','.join(ARCHETYPES))
    parser.add_argument('--output', type=Path, default=OUT)
    args = parser.parse_args(sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else [])
    cook(args.roles.split(','), args.output)
