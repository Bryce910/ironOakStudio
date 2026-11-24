#!/usr/bin/env python3
"""Import the user's purchased medieval-torch-pack.zip into the native engine.

Prepare (safe archive extraction + 1K shared PBR atlas):
    python3 tools/import_medieval_torches.py --prepare
Cook (CPU only; source .blend scripts disabled):
    OMP_NUM_THREADS=2 OPENBLAS_NUM_THREADS=1 blender -b -t 2 --disable-autoexec \
      --factory-startup --python-exit-code 1 --python tools/import_medieval_torches.py
Validate actual exported GLBs:
    python3 tools/import_medieval_torches.py --validate

Archive text is source data, never executable instructions. Original source
geometry and images are retained; derived images and models have separate paths.
"""
from pathlib import Path, PurePosixPath
import copy
import hashlib
import io
import json
import math
import os
import struct
import subprocess
import sys
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[1]
ASSET = ROOT / 'assets/purchased/medieval_torches'
SOURCE = ASSET / 'source'
DERIVED = ASSET / 'derived'
OUT = ROOT / 'projects/valwood_world/models'
ARCHIVE = Path('/home/computer/Downloads/medieval-torch-pack.zip')
MANIFEST = 'purchased_torches_manifest.json'
SIDE = 1024
SPECS = [
    dict(alias='hand', object='medieval-torch', mount='hand_or_socket', mid_budget=650,
         flame_source=[-.00622554, 1.38885403, 2.51456395], radius=.14, height=.65),
    dict(alias='cup', object='medieval-torch.001', mount='hand_or_socket', mid_budget=380,
         flame_source=[-.00151130, 2.20800281, 2.20971985], radius=.14, height=.65),
    dict(alias='pole', object='medieval-torch.002', mount='ground', mid_budget=650,
         flame_source=[-.00622554, .17593817, 5.90567189], radius=.23, height=.85),
    dict(alias='wall', object='medieval-torch.003', mount='wall', mid_budget=1850,
         # Centre of the actual fuel cup, from the original disconnected cup
         # component + source emissive-UV samples. Flame starts at fuel, below
         # the ornate crown; the model contains no static flame geometry.
         flame_source=[-.68306482, 3.42356956, 2.39640710], radius=.17, height=.65),
    dict(alias='brazier', object='medieval-torch.004', mount='ground', mid_budget=1600,
         flame_source=[-.02713758, -1.66440117, .57694322], radius=.54, height=1.15),
]


def sha(path):
    h = hashlib.sha256()
    with Path(path).open('rb') as f:
        for block in iter(lambda: f.read(1024 * 1024), b''):
            h.update(block)
    return h.hexdigest()


def immutable_write(path, content):
    if path.exists():
        assert path.read_bytes() == content, 'Original source differs: ' + str(path)
    else:
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(content)


def safe_member(name):
    p = PurePosixPath(name.replace('\\', '/'))
    assert not p.is_absolute() and '..' not in p.parts and ':' not in name, name
    return p


def prepare():
    from PIL import Image
    import numpy as np
    SOURCE.mkdir(parents=True, exist_ok=True)
    wanted = ['Blend.rar', 'Games Engine Textures.rar', 'PBR 4K.rar',
              'Obj.rar', 'cgtrader_optimized_Fbx.fbx']
    with zipfile.ZipFile(ARCHIVE) as archive:
        for entry in archive.infolist():
            safe_member(entry.filename)
            assert (entry.external_attr >> 16) & 0o170000 != 0o120000, entry.filename
        for member in wanted:
            immutable_write(SOURCE / member, archive.read(member))
    for name in wanted:
        if not name.endswith('.rar'):
            continue
        # Validate RAR paths/link metadata before allowing the extractor to
        # write. 7zip here can list RAR5 but cannot decode its compressed data.
        listing = subprocess.check_output(['7z', 'l', '-slt', str(SOURCE / name)], text=True)
        records = listing.split('----------\n', 1)[1].split('\n\n')
        members = []
        for block in records:
            info = dict(line.split(' = ', 1) for line in block.splitlines() if ' = ' in line)
            if 'Path' not in info:
                continue
            safe_member(info['Path'])
            assert all(not info.get(k) for k in ['Symbolic Link', 'Hard Link', 'Copy Link'])
            if info.get('Folder') == '-':
                members.append(info['Path'])
        with tempfile.TemporaryDirectory(prefix='torch-source-', dir=ASSET) as temporary:
            stage = Path(temporary)
            subprocess.run(['unrar', 'x', '-o+', '-idq', str(SOURCE / name), str(stage) + '/'], check=True)
            for member in members:
                p = safe_member(member)
                immutable_write(SOURCE / str(p), (stage / str(p)).read_bytes())
    DERIVED.mkdir(exist_ok=True)
    maps = {}
    originals = SOURCE / 'PBR 4K'

    def resized(name, mode='RGB'):
        im = Image.open(originals / name).convert(mode)
        assert im.size == (4096, 4096)
        return im.resize((SIDE, SIDE), Image.Resampling.LANCZOS)

    base = resized('01___Default_1_Base_color.jpg')
    normal = np.asarray(resized('01___Default_1_Normal_OpenGL.jpg')).copy()
    n = normal.astype(np.float32) / 127.5 - 1
    n /= np.maximum(np.linalg.norm(n, axis=2, keepdims=True), 1e-6)
    normal = Image.fromarray(np.uint8(np.clip(np.rint((n + 1) * 127.5), 0, 255)))
    orm = Image.merge('RGB', (resized('01___Default_1_Mixed_AO.jpg', 'L'),
                              resized('01___Default_1_Roughness.jpg', 'L'),
                              resized('01___Default_1_Metallic.jpg', 'L')))
    emissive = resized('01___Default_1_Emissive.jpg')
    for name, im in [('basecolor', base), ('normal_opengl', normal), ('orm', orm), ('emissive', emissive)]:
        path = DERIVED / (name + '.png')
        im.save(path, compress_level=6)
        maps[name] = dict(path=str(path.relative_to(ROOT)), sha256=sha(path), size=list(im.size))
    files = [dict(file=str(p.relative_to(ASSET)), bytes=p.stat().st_size, sha256=sha(p))
             for p in sorted(SOURCE.rglob('*')) if p.is_file()]
    provenance = dict(schema_version=1, source_archive=str(ARCHIVE), archive_sha256=sha(ARCHIVE),
        archive_bytes=ARCHIVE.stat().st_size, source='User-supplied purchased medieval-torch-pack.zip.',
        license='Purchased asset as supplied by the user; archive contains no explicit license document. No free redistribution right is asserted.',
        source_preservation='Original archive unchanged in Downloads; staged originals byte-identical, no source code/scripts executed.',
        source_files=files, derived_maps=maps,
        modifications='Each source mesh isolated, transforms applied, canonical origins, original UVs/PBR atlas retained; 4K maps reduced to shared 1024px, OpenGL tangent normals renormalized, AO/Roughness/Metallic packed into RGB, offline mid LODs.',
        omitted_maps='8K duplicates, DirectX normal and displacement: native PBR uses the provided OpenGL normal with no displacement.',
        no_source_flame_geometry=True)
    (ASSET / 'PROVENANCE.json').write_text(json.dumps(provenance, indent=2) + '\n')
    print('PREPARED purchased torches: immutable source geometry + 4 shared 1K PBR maps', flush=True)


def read_glb(path):
    raw = Path(path).read_bytes()
    assert struct.unpack_from('<III', raw) == (0x46546c67, 2, len(raw))
    length, kind = struct.unpack_from('<II', raw, 12)
    assert kind == 0x4e4f534a
    data = json.loads(raw[20:20 + length])
    at = 20 + length
    length, kind = struct.unpack_from('<II', raw, at)
    assert kind == 0x004e4942
    return data, raw[at + 8:at + 8 + length]


def write_glb(path, data, binary):
    raw = json.dumps(data, separators=(',', ':')).encode()
    raw += b' ' * (-len(raw) % 4)
    binary += b'\0' * (-len(binary) % 4)
    Path(path).write_bytes(struct.pack('<III', 0x46546c67, 2, 28 + len(raw) + len(binary)) +
        struct.pack('<II', len(raw), 0x4e4f534a) + raw +
        struct.pack('<II', len(binary), 0x004e4942) + binary)


def add_material(path):
    """Attach four shared byte-identical maps, avoiding Blender graph guesses."""
    data, binary = read_glb(path)
    output = bytearray(binary)
    data['images'] = []
    data['textures'] = []
    data['samplers'] = [dict(magFilter=9729, minFilter=9987, wrapS=10497, wrapT=10497)]
    for i, name in enumerate(['basecolor', 'normal_opengl', 'orm', 'emissive']):
        output += b'\0' * (-len(output) % 4)
        content = (DERIVED / (name + '.png')).read_bytes()
        view = len(data['bufferViews'])
        data['bufferViews'].append(dict(buffer=0, byteOffset=len(output), byteLength=len(content)))
        output += content
        data['images'].append(dict(name='purchased_torch_' + name, bufferView=view, mimeType='image/png'))
        data['textures'].append(dict(sampler=0, source=i))
    data['materials'] = [dict(name='Purchased medieval torch shared original PBR atlas',
        pbrMetallicRoughness=dict(baseColorFactor=[1, 1, 1, 1], baseColorTexture=dict(index=0),
            metallicFactor=1, roughnessFactor=1, metallicRoughnessTexture=dict(index=2)),
        normalTexture=dict(index=1, scale=1), occlusionTexture=dict(index=2, strength=1),
        emissiveTexture=dict(index=3), emissiveFactor=[1, 1, 1], alphaMode='OPAQUE', doubleSided=False)]
    for mesh in data['meshes']:
        for primitive in mesh['primitives']:
            primitive['material'] = 0
    data['buffers'] = [dict(byteLength=len(output))]
    data.pop('extensionsUsed', None)
    data.pop('extensionsRequired', None)
    write_glb(path, data, bytes(output))


def inspect(path):
    import numpy as np
    from PIL import Image
    data, binary = read_glb(path)
    assert not data.get('skins') and not data.get('animations') and not data.get('extensionsRequired')
    assert len(data['meshes']) == 1 and len(data['meshes'][0]['primitives']) == 1
    primitive = data['meshes'][0]['primitives'][0]
    assert primitive.get('mode', 4) == 4

    def accessor(index):
        a = data['accessors'][index]
        v = data['bufferViews'][a['bufferView']]
        dtype = np.dtype({5123: '<u2', 5125: '<u4', 5126: '<f4'}[a['componentType']])
        n = {'SCALAR': 1, 'VEC2': 2, 'VEC3': 3, 'VEC4': 4}[a['type']]
        return np.ndarray((a['count'], n), dtype=dtype, buffer=binary,
            offset=v.get('byteOffset', 0) + a.get('byteOffset', 0),
            strides=(v.get('byteStride', n * dtype.itemsize), dtype.itemsize))

    p = accessor(primitive['attributes']['POSITION'])
    n = accessor(primitive['attributes']['NORMAL'])
    uv = accessor(primitive['attributes']['TEXCOORD_0'])
    idx = accessor(primitive['indices']).reshape(-1, 3)
    assert np.isfinite(p).all() and np.isfinite(n).all() and np.isfinite(uv).all()
    assert len(p) == len(n) == len(uv) and idx.max() < len(p)
    assert np.max(np.abs(np.linalg.norm(n, axis=1) - 1)) < .003
    face = np.cross(p[idx[:, 1]] - p[idx[:, 0]], p[idx[:, 2]] - p[idx[:, 0]])
    valid = np.linalg.norm(face, axis=1) > 1e-10
    facing = np.einsum('ij,ij->i', face, n[idx].sum(axis=1))
    bad = int(((facing < -1e-9) & valid).sum())
    assert bad / max(1, int(valid.sum())) < .025, (path, bad, int(valid.sum()))
    for node in data.get('nodes', []):
        assert 'matrix' not in node and node.get('scale', [1, 1, 1]) == [1, 1, 1]
        assert node.get('translation', [0, 0, 0]) == [0, 0, 0]
    images = []
    for record in data['images']:
        v = data['bufferViews'][record['bufferView']]
        content = binary[v.get('byteOffset', 0):v.get('byteOffset', 0) + v['byteLength']]
        im = Image.open(io.BytesIO(content))
        assert im.size == (SIDE, SIDE)
        images.append(dict(name=record['name'], size=list(im.size), bytes=len(content),
                           sha256=hashlib.sha256(content).hexdigest()))
    assert len(images) == 4
    low, high = p.min(axis=0), p.max(axis=0)
    return dict(triangles=len(idx), primitive_count=1, bounds_min=low.tolist(), bounds_max=high.tolist(),
        dimensions_m=(high - low).tolist(), images=images, materials=data['materials'],
        bytes=path.stat().st_size, sha256=sha(path),
        geometry_checks=dict(finite=True, normals_unit=True, indices_in_range=True,
            nondegenerate_triangles=int(valid.sum()), opposing_normal_triangles=bad))


def validate(directory=OUT):
    directory = Path(directory)
    manifest = json.loads((directory / MANIFEST).read_text())
    assert len(manifest['assets']) == 10
    provenance = json.loads((ASSET / 'PROVENANCE.json').read_text())
    semantics = ['basecolor', 'normal_opengl', 'orm', 'emissive']
    expected_hashes = [provenance['derived_maps'][name]['sha256'] for name in semantics]
    hashes = set()
    for record in manifest['assets']:
        actual = inspect(directory / record['file'])
        assert actual['sha256'] == record['sha256'] and actual['triangles'] == record['triangles']
        budget = next(s['mid_budget'] for s in SPECS if s['alias'] == record['alias']) if record['lod'] == 'mid' else 6500
        assert 0 < actual['triangles'] <= budget
        assert abs(actual['bounds_min'][1]) < .025
        assert abs(actual['bounds_min'][0] + actual['bounds_max'][0]) < .04
        if record['alias'] == 'wall':
            assert abs(actual['bounds_min'][2]) < .025
            assert record['flame_socket_local_m'][2] > .6
        else:
            assert abs(actual['bounds_min'][2] + actual['bounds_max'][2]) < .04
        low, high = actual['bounds_min'], actual['bounds_max']
        socket = record['flame_socket_local_m']
        assert all(low[i] - .01 <= socket[i] <= high[i] + .01 for i in range(3))
        for image in actual['images']:
            hashes.add(image['sha256'])
        assert [image['sha256'] for image in actual['images']] == expected_hashes, 'PBR source channel order/content changed'
        assert actual['materials'][0]['pbrMetallicRoughness']['metallicRoughnessTexture']['index'] == 2
        assert actual['materials'][0]['normalTexture']['index'] == 1
        assert actual['materials'][0]['emissiveTexture']['index'] == 3
    assert len(hashes) == 4, 'All five variants/LODs must share byte-identical images'
    for spec in SPECS:
        pair = [a for a in manifest['assets'] if a['alias'] == spec['alias']]
        assert len(pair) == 2
        assert pair[0]['flame_socket_local_m'] == pair[1]['flame_socket_local_m']
        assert max(abs(pair[0]['dimensions_m'][i] - pair[1]['dimensions_m'][i]) for i in range(3)) < .045
    for original in provenance['source_files']:
        assert sha(ASSET / original['file']) == original['sha256'], original['file']
    for image in provenance['derived_maps'].values():
        assert sha(ROOT / image['path']) == image['sha256']
    if ARCHIVE.exists():
        assert sha(ARCHIVE) == provenance['archive_sha256']
    budget = dict(unique_images=4, max_image_side=SIDE, estimated_rgba8_mips_mib=4 * SIDE * SIDE * 4 * 4 / 3 / 1048576,
        total_model_bytes=sum(a['bytes'] for a in manifest['assets']), max_near_triangles=max(a['triangles'] for a in manifest['assets'] if a['lod'] == 'near'),
        max_mid_triangles=max(a['triangles'] for a in manifest['assets'] if a['lod'] == 'mid'))
    print('VALIDATED purchased torch models:', json.dumps(budget), flush=True)
    return budget


def cook():
    import bpy
    from mathutils import Matrix, Vector
    assert (ASSET / 'PROVENANCE.json').exists(), 'Run --prepare first'
    bpy.ops.wm.open_mainfile(filepath=str(SOURCE / 'Blend.blend'), load_ui=False, use_scripts=False)
    originals = {spec['alias']: bpy.data.objects[spec['object']].copy() for spec in SPECS}
    for alias, obj in originals.items():
        obj.data = bpy.data.objects[next(s['object'] for s in SPECS if s['alias'] == alias)].data.copy()
    source_info = []
    records = []
    OUT.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='.purchased-torches-', dir=OUT) as temporary:
        stage = Path(temporary)
        for spec in SPECS:
            source = originals[spec['alias']]
            points = [source.matrix_world @ Vector(c) for c in source.bound_box]
            low = Vector([min(v[i] for v in points) for i in range(3)])
            high = Vector([max(v[i] for v in points) for i in range(3)])
            if spec['mount'] == 'wall':
                # Actual backplate component (137 source vertices): use the
                # rear plane Xmax and horizontal centre of its mounting plate.
                origin = Vector((high.x, (3.232140064239502 + 3.6092281341552734) / 2, low.z))
                canonical = Matrix.Rotation(math.pi / 2, 4, 'Z') @ Matrix.Translation(-origin)
            else:
                origin = Vector(((low.x + high.x) / 2, (low.y + high.y) / 2, low.z))
                canonical = Matrix.Translation(-origin)
            socket_b = canonical @ Vector(spec['flame_source'])
            socket = [socket_b.x, socket_b.z, -socket_b.y]
            source_info.append(dict(alias=spec['alias'], object=spec['object'], source_blender_bounds_min=list(low),
                source_blender_bounds_max=list(high), source_origin_blender_m=list(origin),
                source_flame_base_blender_m=spec['flame_source'], source_to_blender_canonical=[list(r) for r in canonical]))
            for lod in ['near', 'mid']:
                bpy.ops.object.select_all(action='SELECT')
                bpy.ops.object.delete(use_global=False)
                obj = source.copy()
                obj.data = source.data.copy()
                bpy.context.collection.objects.link(obj)
                obj.matrix_world = canonical @ source.matrix_world
                bpy.context.view_layer.objects.active = obj
                obj.select_set(True)
                bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
                source_count = sum(len(p.vertices) - 2 for p in obj.data.polygons)
                budget = spec['mid_budget'] if lod == 'mid' else 6500
                for attempt in range(3):
                    count = sum(len(p.vertices) - 2 for p in obj.data.polygons)
                    if count <= budget:
                        break
                    modifier = obj.modifiers.new('Offline silhouette-preserving torch LOD', 'DECIMATE')
                    modifier.ratio = min(.99, budget * .985 / count)
                    modifier.use_collapse_triangulate = True
                    bpy.ops.object.modifier_apply(modifier=modifier.name)
                if lod == 'mid' and obj.data.has_custom_normals:
                    # Decimation changes the cage's thin curved strips. Source
                    # split normals refer to the original tessellation; letting
                    # Blender rebuild them from the retained smoothing flags
                    # avoids carrying incompatible normals onto the new faces.
                    bpy.ops.mesh.customdata_custom_splitnormals_clear()
                obj.name = 'purchased_torch_' + spec['alias']
                name = obj.name + '_' + lod + '.glb'
                path = stage / name
                bpy.ops.export_scene.gltf(filepath=str(path), export_format='GLB', use_selection=True,
                    export_yup=True, export_animations=False, export_cameras=False, export_lights=False,
                    export_materials='NONE', export_apply=True)
                add_material(path)
                record = dict(alias=spec['alias'], lod=lod, file=name, mount=spec['mount'], source_object=spec['object'],
                    source_triangles=source_count, flame_socket_local_m=socket,
                    flame_radius_m=spec['radius'], flame_height_m=spec['height'], **inspect(path))
                records.append(record)
                print('COOKED', name, record['triangles'], record['dimensions_m'], 'flame base', socket, flush=True)
        manifest = dict(schema_version=1, provenance='assets/purchased/medieval_torches/PROVENANCE.json',
            origin='Metres, Y up, applied transforms. Ground/hand models centred XZ with baseY0. Wall has rear mounting planeZ0, plate horizontal centreX0, plate bottomY0, outward+Z.',
            flame_socket_note='LocalXYZ is BASE at fuel/wick, not centre of flame. No static flame mesh is part of purchased source. Socket and radius/height are authored from source geometry and emissive UV mask, same between LODs.',
            material_note='Original PBR basecolor, OpenGL normal, RGB AO/roughness/metallic and glowing fuel emissive atlas. Four shared 1024px images across ten GLBs; no baked light or source scene.',
            source_geometry=source_info, assets=records)
        (stage / MANIFEST).write_text(json.dumps(manifest, indent=2) + '\n')
        manifest['budget'] = validate(stage)
        (stage / MANIFEST).write_text(json.dumps(manifest, indent=2) + '\n')
        for record in records:
            os.replace(stage / record['file'], OUT / record['file'])
        os.replace(stage / MANIFEST, OUT / MANIFEST)
    validate()


if __name__ == '__main__':
    args = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else sys.argv[1:]
    if '--prepare' in args:
        prepare()
    elif '--validate' in args:
        validate()
    else:
        cook()
