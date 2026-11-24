#!/usr/bin/env python3
"""Cook the user's actual purchased shield knight, retaining its genuine guard idle.

python3 tools/import_village_shield_knight.py --prepare
OMP_NUM_THREADS=2 OPENBLAS_NUM_THREADS=1 blender -b -t 2 --disable-autoexec \
  --factory-startup --python-exit-code 1 --python tools/import_village_shield_knight.py
python3 tools/import_village_shield_knight.py --validate

Only the copied source rig is evaluated, with source scripts disabled. Geometry
has fixed triangulation and fixed UV/normal seam IDs before skeletal sampling.
The eight evenly spaced source poses form a GPU interpolated guarded weight
shift; the original clip's endpoints are not claimed to be a seamless loop.
"""
from pathlib import Path, PurePosixPath
import argparse
import hashlib
import json
import math
import os
import shutil
import stat
import struct
import sys
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[1]
ASSET = ROOT / 'assets/purchased/shield_knight'
SOURCE = ASSET / 'source'
DERIVED = ASSET / 'derived'
OUT = ROOT / 'projects/valwood_world/models'
ARCHIVE = Path('/home/computer/Downloads/knight-with-a-shield(1).zip')
MANIFEST = 'village_shield_knight_manifest.json'
NAMES = ['Knight', 'Helm', 'Shield', 'Sword']
SIDE = 1024
POSE_COUNT = 8
ACTION = 'Idle'
START, END = 1., 105.
HEIGHT = 1.8
MAPS = ['basecolor', 'normal_opengl', 'orm']


def sha(path):
    h = hashlib.sha256()
    with Path(path).open('rb') as f:
        for block in iter(lambda: f.read(1024 * 1024), b''):
            h.update(block)
    return h.hexdigest()


def publish(path, content):
    path.parent.mkdir(parents=True, exist_ok=True)
    if path.exists() and path.read_bytes() == content:
        return
    pending = path.with_suffix(path.suffix + '.pending')
    pending.write_bytes(content)
    os.replace(pending, path)


def json_write(path, data):
    publish(path, (json.dumps(data, indent=2) + '\n').encode())


def safe_member(info):
    p = PurePosixPath(info.filename.replace('\\', '/'))
    assert not p.is_absolute() and '..' not in p.parts and ':' not in info.filename
    assert not stat.S_ISLNK(info.external_attr >> 16)
    assert not info.flag_bits & 1, 'Encrypted source member'
    return p


def extract(archive_path, destination, selected=None):
    """CRC validate by streaming each member, then preserve originals immutably."""
    with zipfile.ZipFile(archive_path) as z:
        assert len(z.infolist()) < 1000
        assert sum(i.file_size for i in z.infolist()) < 2 * 1024**3
        for i in z.infolist():
            p = safe_member(i)
            if i.is_dir() or selected is not None and i.filename not in selected:
                continue
            dest = destination / str(p)
            dest.parent.mkdir(parents=True, exist_ok=True)
            with tempfile.NamedTemporaryFile(dir=dest.parent, delete=False) as f:
                temporary = Path(f.name)
                with z.open(i) as source:
                    shutil.copyfileobj(source, f, 1024 * 1024)
            try:
                assert temporary.stat().st_size == i.file_size
                if dest.exists():
                    assert sha(dest) == sha(temporary), 'Staged original differs: ' + str(dest)
                else:
                    os.replace(temporary, dest)
            finally:
                temporary.unlink(missing_ok=True)


def prepare():
    import numpy as np
    from PIL import Image
    extract(ARCHIVE, SOURCE, set(['Knight with shield.blend', 'Knight with shield.fbx',
                                'Shield.fbx', 'Sword.fbx', 'Textures.zip']))
    extract(SOURCE / 'Textures.zip', SOURCE)
    textures = SOURCE / 'Textures/Texture set 1'
    DERIVED.mkdir(parents=True, exist_ok=True)
    def source_map(name, mode):
        with Image.open(textures / ('Knight_' + name + '.png')) as im:
            assert im.size == (4096, 4096)
            return im.convert(mode).resize((SIDE, SIDE), Image.Resampling.LANCZOS)
    base = source_map('AlbedoTransparency', 'RGB')
    normal = np.asarray(source_map('Normal', 'RGB'), dtype=np.float32) / 127.5 - 1.
    normal /= np.maximum(np.linalg.norm(normal, axis=2, keepdims=True), 1.e-6)
    normal = Image.fromarray(np.uint8(np.clip(np.rint((normal + 1.) * 127.5), 0, 255)))
    metallic_smooth = source_map('MetallicSmoothness', 'RGBA')
    orm = Image.merge('RGB', (source_map('AO', 'L'),
        Image.eval(metallic_smooth.getchannel('A'), lambda x: 255 - x),
        metallic_smooth.getchannel('R')))
    maps = {}
    for name, im in zip(MAPS, [base, normal, orm]):
        with tempfile.NamedTemporaryFile(suffix='.png') as f:
            im.save(f.name, compress_level=6)
            publish(DERIVED / (name + '.png'), Path(f.name).read_bytes())
        maps[name] = dict(file=str((DERIVED / (name + '.png')).relative_to(ROOT)),
                         sha256=sha(DERIVED / (name + '.png')), size=[SIDE, SIDE])
    provenance = dict(schema_version=1, source_archive=str(ARCHIVE),
        archive_bytes=ARCHIVE.stat().st_size, archive_sha256=sha(ARCHIVE),
        source='User-supplied purchased Knight with shield, correct numbered ZIP archive.',
        license='Purchased asset supplied by the user; source archive has no explicit license document. No free redistribution rights asserted.',
        source_scripts_executed=False, original_archive_modified=False,
        source_files=[dict(file=str(p.relative_to(ASSET)), bytes=p.stat().st_size, sha256=sha(p))
                      for p in sorted(SOURCE.rglob('*')) if p.is_file()],
        derived_maps=maps, color_variant=1,
        material_conversion='Original green/gold color variant; 4K sources resized to 1K; normals renormalized retaining source green channel; AO in R, roughness=1-original smoothness alpha in G, original metallic red in B.',
        material_note='Source normal channel orientation retained rather than inferred from its generic filename. Native visual verification remains required.',
        runtime_texture_bytes_rgba8_with_mips=3 * SIDE * SIDE * 4 * 4 // 3,
        omitted_files=['Knight with shield.unitypackage', 'Knight with shield.stl'])
    json_write(ASSET / 'PROVENANCE.json', provenance)
    print('SHIELD_KNIGHT_PREPARED', json.dumps(maps), flush=True)


def write_glb(path, arrays, label):
    import numpy as np
    p, n, uv, tangent, indices = arrays
    blob = bytearray()
    views, accessors = [], []
    def append(raw, target=None):
        blob.extend(b'\0' * (-len(blob) % 4))
        view = dict(buffer=0, byteOffset=len(blob), byteLength=len(raw))
        if target:
            view['target'] = target
        views.append(view)
        blob.extend(raw)
        return len(views) - 1
    def accessor(a, typ, component, target, bounds=False):
        a = np.ascontiguousarray(a)
        item = dict(bufferView=append(a.tobytes(), target), componentType=component,
                    count=len(a), type=typ)
        if bounds:
            item.update(min=a.min(axis=0).tolist(), max=a.max(axis=0).tolist())
        accessors.append(item)
        return len(accessors) - 1
    attributes = {}
    for key, a, typ in [('POSITION', p, 'VEC3'), ('NORMAL', n, 'VEC3'),
                        ('TEXCOORD_0', uv, 'VEC2'), ('TANGENT', tangent, 'VEC4')]:
        attributes[key] = accessor(a.astype('<f4'), typ, 5126, 34962, key == 'POSITION')
    component, dtype = (5123, '<u2') if len(p) < 65536 else (5125, '<u4')
    index_accessor = accessor(indices.reshape(-1).astype(dtype), 'SCALAR', component, 34963)
    images = [dict(name='purchased_shield_knight_' + name,
                   bufferView=append((DERIVED / (name + '.png')).read_bytes()), mimeType='image/png')
              for name in MAPS]
    doc = dict(asset=dict(version='2.0', generator='Valwood deterministic purchased shield knight cooker'),
        scene=0, scenes=[dict(nodes=[0])], nodes=[dict(name=label, mesh=0)],
        meshes=[dict(name=label, primitives=[dict(attributes=attributes, indices=index_accessor, material=0, mode=4)])],
        materials=[dict(name='Purchased knight original green gold shared 1K PBR',
            pbrMetallicRoughness=dict(baseColorFactor=[1, 1, 1, 1], baseColorTexture=dict(index=0),
                metallicFactor=1, roughnessFactor=1, metallicRoughnessTexture=dict(index=2)),
            normalTexture=dict(index=1, scale=1), occlusionTexture=dict(index=2, strength=1),
            alphaMode='OPAQUE', doubleSided=False)],
        images=images, samplers=[dict(magFilter=9729, minFilter=9987, wrapS=10497, wrapT=10497)],
        textures=[dict(sampler=0, source=i) for i in range(3)],
        accessors=accessors, bufferViews=views, buffers=[dict(byteLength=len(blob))],
        extras=dict(valwood_pose_contract_version=1, source_parts=NAMES,
                    actual_source_action=ACTION, source_scripts_executed=False))
    raw = json.dumps(doc, separators=(',', ':')).encode()
    raw += b' ' * (-len(raw) % 4)
    blob.extend(b'\0' * (-len(blob) % 4))
    output = struct.pack('<III', 0x46546c67, 2, 28 + len(raw) + len(blob)) + \
        struct.pack('<II', len(raw), 0x4e4f534a) + raw + \
        struct.pack('<II', len(blob), 0x004e4942) + blob
    publish(path, output)


def tangents(p, n, uv, indices, signs=None):
    import numpy as np
    a, b, c = (indices[:, i] for i in range(3))
    e1, e2 = p[b] - p[a], p[c] - p[a]
    d1, d2 = uv[b] - uv[a], uv[c] - uv[a]
    determinant = d1[:, 0] * d2[:, 1] - d1[:, 1] * d2[:, 0]
    valid = np.abs(determinant) > 1.e-12
    safe = np.where(valid, determinant, 1.)[:, None]
    t = (e1 * d2[:, 1, None] - e2 * d1[:, 1, None]) / safe
    bdir = (e2 * d1[:, 0, None] - e1 * d2[:, 0, None]) / safe
    t[~valid] = 0
    bdir[~valid] = 0
    total, bitangent = np.zeros_like(p), np.zeros_like(p)
    for ids in (a, b, c):
        np.add.at(total, ids, t)
        np.add.at(bitangent, ids, bdir)
    total -= n * np.sum(total * n, axis=1)[:, None]
    lengths = np.linalg.norm(total, axis=1)
    for i in np.where(lengths < 1.e-10)[0]:
        total[i] = np.cross(n[i], [0, 1, 0] if abs(n[i, 1]) < .9 else [1, 0, 0])
    total /= np.maximum(np.linalg.norm(total, axis=1)[:, None], 1.e-12)
    if signs is None:
        signs = np.where(np.sum(np.cross(n, total) * bitangent, axis=1) < 0, -1., 1.)
    return np.column_stack([total, signs]).astype('<f4')


def cook(output=OUT):
    import bpy
    import bmesh
    import numpy as np
    from mathutils import Vector
    bpy.context.preferences.filepaths.use_scripts_auto_execute = False
    bpy.ops.wm.open_mainfile(filepath=str(SOURCE / 'Knight with shield.blend'), load_ui=False, use_scripts=False)
    scene = bpy.context.scene
    arm = bpy.data.objects['Knight with shield']
    arm.animation_data_create()
    for track in arm.animation_data.nla_tracks:
        track.mute = True
    arm.animation_data.action = bpy.data.actions[ACTION]
    fps = scene.render.fps / scene.render.fps_base
    assert tuple(arm.animation_data.action.frame_range) == (START, END) and fps == 30
    for name in NAMES:
        mesh = bpy.data.objects[name].data
        bm = bmesh.new()
        bm.from_mesh(mesh)
        bmesh.ops.triangulate(bm, faces=list(bm.faces), quad_method='BEAUTY', ngon_method='BEAUTY')
        bm.to_mesh(mesh)
        bm.free()
        mesh.update()
    # Proper right-handed Blender Z-up to native Y-up: source forward -Y -> +Z.
    basis = np.asarray([[1., 0., 0.], [0., 0., 1.], [0., -1., 0.]])
    topology = {}
    def source_sample(frame, reference=False):
        scene.frame_set(math.floor(frame), subframe=frame % 1.)
        bpy.context.view_layer.update()
        graph = bpy.context.evaluated_depsgraph_get()
        a = arm.evaluated_get(graph)
        hip = a.matrix_world @ a.pose.bones['Hips'].head
        positions, normals, texcoords, triangles = [], [], [], []
        offset = 0
        body_min = 1.e10
        body_max = -1.e10
        for name in NAMES:
            e = bpy.data.objects[name].evaluated_get(graph)
            m = e.to_mesh()
            m.calc_loop_triangles()
            m.calc_normals_split()
            nmatrix = e.matrix_world.to_3x3().inverted().transposed()
            source_positions = np.asarray([tuple(e.matrix_world @ v.co) for v in m.vertices])
            source_normals = np.asarray([tuple((nmatrix @ n.vector).normalized()) for n in m.corner_normals])
            original_uv = np.asarray([tuple(x.uv) for x in m.uv_layers.active.data])
            if name in ['Knight', 'Helm']:
                body_min = min(body_min, float(source_positions[:, 2].min()))
                body_max = max(body_max, float(source_positions[:, 2].max()))
            actual_triangles = np.asarray([list(t.loops) for t in m.loop_triangles], dtype=np.int64)
            if reference:
                mapping, representatives, remap = {}, [], []
                for i, loop in enumerate(m.loops):
                    key = (loop.vertex_index, *np.round(original_uv[i], 8), *np.round(source_normals[i], 6))
                    if key not in mapping:
                        mapping[key] = len(representatives)
                        representatives.append(i)
                    remap.append(mapping[key])
                topology[name] = dict(representatives=np.asarray(representatives),
                    raw_vertices=np.asarray([m.loops[i].vertex_index for i in representatives]),
                    remap=np.asarray(remap), triangles=actual_triangles, vertices=len(m.vertices), loops=len(m.loops))
            fixed = topology[name]
            assert len(m.vertices) == fixed['vertices'] and len(m.loops) == fixed['loops']
            assert np.array_equal(actual_triangles, fixed['triangles'])
            positions.append(source_positions[fixed['raw_vertices']])
            normals.append(source_normals[fixed['representatives']])
            uvs = original_uv[fixed['representatives']].copy()
            uvs[:, 1] = 1. - uvs[:, 1]
            texcoords.append(uvs)
            triangles.append(fixed['remap'][actual_triangles] + offset)
            offset += len(fixed['representatives'])
            e.to_mesh_clear()
        p, n, uv, indices = map(np.concatenate, [positions, normals, texcoords, triangles])
        p[:, :2] -= np.asarray(hip)[:2]
        p[:, 2] -= body_min
        return p @ basis.T, n @ basis.T, uv, indices, body_max - body_min
    p, n, uv, indices, reference_height = source_sample(START, True)
    scale = HEIGHT / reference_height
    p *= scale
    sign = tangents(p, n, uv, indices)[:, 3]
    endpoint = source_sample(END)[0] * scale
    seam = np.linalg.norm(endpoint - p, axis=1)
    poses, records = [], []
    output = Path(output)
    for i in range(POSE_COUNT):
        frame = START + (END - START) * i / POSE_COUNT
        p, n, actual_uv, actual_indices, height = source_sample(frame)
        p *= scale
        assert np.array_equal(actual_uv, uv) and np.array_equal(actual_indices, indices)
        assert np.isfinite(p).all() and np.isfinite(n).all()
        assert abs(float(p[:, 1].min())) < 1.e-6
        tangent = tangents(p, n, uv, indices, sign)
        poses.append(p.copy())
        path = output / f'village_shield_knight_idle_{i:02}.glb'
        write_glb(path, (p, n, uv, tangent, indices), f'Purchased shield knight actual Idle phase {i:02}')
        records.append(dict(file=path.name, mode='idle', phase=i / POSE_COUNT, source_frame=frame,
            source_fps=fps, sha256=sha(path), bytes=path.stat().st_size,
            triangles=len(indices), vertices=len(p), bounds_min=p.min(axis=0).tolist(), bounds_max=p.max(axis=0).tolist()))
        print('SHIELD_KNIGHT_POSE', i, frame, len(p), len(indices), flush=True)
    publish(output / 'village_shield_knight_near.glb', (output / records[0]['file']).read_bytes())
    # Offline static mid LOD only. The animated family retains identical IDs.
    body = bpy.data.meshes.new('Purchased guard static mid source')
    body.from_pydata(poses[0].tolist(), [], indices.tolist())
    body.update()
    layer = body.uv_layers.new(name='UVMap')
    for loop in body.loops:
        layer.data[loop.index].uv = (uv[loop.vertex_index, 0], 1. - uv[loop.vertex_index, 1])
    for poly in body.polygons:
        poly.use_smooth = True
    ob = bpy.data.objects.new('Purchased shield knight mid', body)
    bpy.context.collection.objects.link(ob)
    bpy.ops.object.select_all(action='DESELECT')
    ob.select_set(True)
    bpy.context.view_layer.objects.active = ob
    modifier = ob.modifiers.new('Offline static mid reduction', 'DECIMATE')
    modifier.ratio = min(1., 8000. / len(indices))
    bpy.ops.object.modifier_apply(modifier=modifier.name)
    body = ob.data
    body.calc_loop_triangles()
    body.calc_normals_split()
    # Engine coordinates are already stored in this temporary mesh.
    mids = np.asarray([tuple(v.co) for v in body.vertices])
    mid_uv = np.asarray([tuple(x.uv) for x in body.uv_layers.active.data])
    mid_normals = np.asarray([tuple(n.vector) for n in body.corner_normals])
    reps, keys, remap = [], {}, []
    for i, loop in enumerate(body.loops):
        key = (loop.vertex_index, *np.round(mid_uv[i], 8), *np.round(mid_normals[i], 6))
        if key not in keys:
            keys[key] = len(reps)
            reps.append(i)
        remap.append(keys[key])
    mid_p = mids[[body.loops[i].vertex_index for i in reps]]
    mid_n = mid_normals[reps]
    mid_uv = mid_uv[reps]
    mid_uv[:, 1] = 1. - mid_uv[:, 1]
    mid_indices = np.asarray(remap)[np.asarray([list(t.loops) for t in body.loop_triangles])]
    mid_path = output / 'village_shield_knight_mid.glb'
    write_glb(mid_path, (mid_p, mid_n, mid_uv, tangents(mid_p, mid_n, mid_uv, mid_indices), mid_indices),
              'Purchased shield knight static mid LOD')
    pairs = []
    for i, (a, b) in enumerate(zip(poses, poses[1:] + poses[:1])):
        delta = np.linalg.norm(a - b, axis=1)
        pairs.append(dict(from_phase=i, to_phase=(i + 1) % POSE_COUNT,
                          rms_m=float(np.sqrt(np.mean(delta**2))), max_m=float(delta.max())))
    manifest = dict(schema_version=1, title='Purchased shield knight guarded idle',
        source_archive=str(ARCHIVE), source_archive_sha256=sha(ARCHIVE),
        source_blend=str((SOURCE / 'Knight with shield.blend').relative_to(ROOT)),
        source_blend_sha256=sha(SOURCE / 'Knight with shield.blend'),
        provenance=str((ASSET / 'PROVENANCE.json').relative_to(ROOT)),
        provenance_sha256=sha(ASSET / 'PROVENANCE.json'),
        source_scripts_executed=False, source_modified=False, source_parts=NAMES,
        source_action=ACTION, source_frames=[START, END], source_fps=fps,
        cycle_seconds=(END - START) / fps, pose_count=POSE_COUNT,
        animation='Eight genuine evenly spaced source Idle poses; guarded weight shift with continuous GPU-interpolated wrap. No procedural breathing or additional source motion claimed.',
        source_endpoint_seam_rms_m=float(np.sqrt(np.mean(seam**2))),
        source_endpoint_seam_max_m=float(seam.max()), seamless_raw_source=False,
        sampled_adjacent_pairs=pairs, fixed_triangulation_before_skinning=True,
        all_adjacent_pose_topology_material_exact=True, pose_contract_version=1,
        reference_body_height_m=HEIGHT, fixed_reference_scale=scale,
        root_motion_removed='Evaluated hips horizontal translation; actual boot sole height grounded per pose.',
        facing='+Z', up='+Y', grounding='minimum actual body/gear Y=0; original shield LeftForeArm skinning and sword RightHand skinning retained',
        bounds_min=np.min([p.min(axis=0) for p in poses], axis=0).tolist(),
        bounds_max=np.max([p.max(axis=0) for p in poses], axis=0).tolist(),
        unique_images=3, image_sha256=[sha(DERIVED / (n + '.png')) for n in MAPS],
        texture_bytes_rgba8_with_mips=3 * SIDE * SIDE * 4 * 4 // 3,
        geometry_bytes_per_pose=int(len(p) * 48 + indices.size * 2),
        static_catalog=[dict(file='village_shield_knight_near.glb', sha256=sha(output / 'village_shield_knight_near.glb'),
                             triangles=len(indices), bounds_min=poses[0].min(axis=0).tolist(), bounds_max=poses[0].max(axis=0).tolist()),
                        dict(file=mid_path.name, sha256=sha(mid_path), triangles=len(mid_indices),
                             bounds_min=mid_p.min(axis=0).tolist(), bounds_max=mid_p.max(axis=0).tolist())],
        assets=records)
    json_write(output / MANIFEST, manifest)
    validate(output)
    print('SHIELD_KNIGHT_COOK', json.dumps({k: manifest[k] for k in ['pose_count', 'cycle_seconds', 'bounds_min', 'bounds_max', 'texture_bytes_rgba8_with_mips']}), flush=True)
    return manifest


def validate(output=OUT):
    import numpy as np
    sys.path.insert(0, str(ROOT / 'tools'))
    from audit_village_pose_compatibility import glb, pose, values
    output = Path(output)
    manifest = json.loads((output / MANIFEST).read_text())
    baseline = None
    baseline_signs = None
    positions = []
    image_hashes = None
    assert manifest['pose_count'] == 8 and manifest['cycle_seconds'] == 104 / 30
    assert manifest['texture_bytes_rgba8_with_mips'] <= 20 * 1048576
    for r in manifest['assets'] + manifest['static_catalog']:
        path = output / r['file']
        assert sha(path) == r['sha256']
        doc, binary = glb(path)
        assert not doc.get('skins') and not doc.get('animations') and not doc.get('extensionsRequired')
        assert len(doc['meshes']) == 1 and len(doc['materials']) == 1 and len(doc['images']) == 3
        hashes = []
        for im in doc['images']:
            view = doc['bufferViews'][im['bufferView']]
            raw = binary[view.get('byteOffset', 0):view.get('byteOffset', 0) + view['byteLength']]
            assert raw[:8] == b'\x89PNG\r\n\x1a\n' and struct.unpack_from('>II', raw, 16) == (SIDE, SIDE)
            hashes.append(hashlib.sha256(raw).hexdigest())
        if image_hashes is None:
            image_hashes = hashes
        assert hashes == image_hashes == manifest['image_sha256']
        prim = doc['meshes'][0]['primitives'][0]
        a = prim['attributes']
        p, n, uv, tangent = [values(doc, binary, a[k]) for k in ['POSITION', 'NORMAL', 'TEXCOORD_0', 'TANGENT']]
        indices = values(doc, binary, prim['indices']).reshape(-1, 3)
        assert all(np.isfinite(x).all() for x in [p, n, uv, tangent])
        assert indices.min() >= 0 and indices.max() < len(p) and len(indices) == r['triangles']
        assert max(abs(np.linalg.norm(n, axis=1) - 1)) < .002
        assert max(abs(np.linalg.norm(tangent[:, :3], axis=1) - 1)) < .002
        assert max(abs(np.sum(n * tangent[:, :3], axis=1))) < .002
        assert set(np.unique(tangent[:, 3])) <= {-1., 1.}
        face = np.cross(p[indices[:, 1]] - p[indices[:, 0]], p[indices[:, 2]] - p[indices[:, 0]])
        valid = np.linalg.norm(face, axis=1) > 1.e-10
        backwards = np.einsum('ij,ij->i', face, n[indices].sum(axis=1)) < -1.e-9
        assert np.sum(backwards & valid) / max(1, np.sum(valid)) < .02
        assert np.allclose(p.min(axis=0), r['bounds_min'], atol=1.e-6)
        assert np.allclose(p.max(axis=0), r['bounds_max'], atol=1.e-6)
        sampled = pose(path)
        signature = [x['signature'] for x in sampled['records']]
        if r.get('mode') == 'idle':
            if baseline is None:
                baseline = signature
                baseline_signs = tangent[:, 3].copy()
            assert signature == baseline
            assert np.array_equal(tangent[:, 3], baseline_signs)
            assert abs(float(p[:, 1].min())) < 1.e-6
            positions.append(p)
        print('SHIELD_KNIGHT_VALID', path.name, len(p), len(indices), flush=True)
    assert manifest['sampled_adjacent_pairs'][-1]['max_m'] <= max(p['max_m'] for p in manifest['sampled_adjacent_pairs'][:-1])
    for a, b in zip(positions, positions[1:] + positions[:1]):
        for weight in np.linspace(0, 1, 17):
            floor = float(((1. - weight) * a + weight * b)[:, 1].min())
            assert -.000001 <= floor <= .002, ('Interpolated sole grounding', floor)
    print('SHIELD_KNIGHT_VALIDATION_PASS: eight direct-compatible actual source poses, held gear, shared original PBR, grounded finite geometry', flush=True)


def preview(phase=0):
    """CPU-only inspection of the actual final GLB; never a game screenshot."""
    import bpy
    from mathutils import Vector
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.context.preferences.filepaths.use_scripts_auto_execute = False
    assert 0 <= phase < POSE_COUNT
    bpy.ops.import_scene.gltf(filepath=str(OUT / f'village_shield_knight_idle_{phase:02}.glb'))
    scene = bpy.context.scene
    scene.render.engine = 'CYCLES'
    scene.cycles.device = 'CPU'
    scene.cycles.samples = 24
    # The bundled Blender CPU build does not include OpenImageDenoise.
    scene.cycles.use_denoising = False
    scene.cycles.max_bounces = 5
    scene.render.threads_mode = 'FIXED'
    scene.render.threads = 2
    scene.render.resolution_x = 560
    scene.render.resolution_y = 760
    scene.render.resolution_percentage = 100
    scene.world = bpy.data.worlds.new('Neutral CPU inspection world')
    scene.world.use_nodes = True
    scene.world.node_tree.nodes['Background'].inputs['Color'].default_value = (.13, .17, .22, 1)
    scene.world.node_tree.nodes['Background'].inputs['Strength'].default_value = .45
    bpy.ops.mesh.primitive_plane_add(size=200)
    floor = bpy.context.object
    mat = bpy.data.materials.new('Inspection matte ground')
    mat.use_nodes = True
    shader = mat.node_tree.nodes.get('Principled BSDF')
    shader.inputs['Base Color'].default_value = (.075, .08, .09, 1)
    shader.inputs['Roughness'].default_value = .87
    floor.data.materials.append(mat)
    def aim(ob, at):
        ob.rotation_euler = (Vector(at) - ob.location).to_track_quat('-Z', 'Y').to_euler()
    for name, location, power, color, size in [
        ('Soft warm key', (-3, -4, 5), 1150, (1, .84, .67), 3),
        ('Cool sky fill', (3, -1, 3), 650, (.62, .76, 1), 3),
        ('Rear rim', (0, 3, 4), 1250, (1, .92, .77), 2)]:
        data = bpy.data.lights.new(name, 'AREA')
        data.energy, data.color, data.size = power, color, size
        ob = bpy.data.objects.new(name, data)
        scene.collection.objects.link(ob)
        ob.location = location
        aim(ob, (0, 0, 1))
    data = bpy.data.cameras.new('Actual cooked guard inspection')
    camera = bpy.data.objects.new('Actual cooked guard inspection', data)
    scene.collection.objects.link(camera)
    camera.location = (2.4, -4.2, 2.15)
    aim(camera, (-.12, 0, .93))
    data.type, data.ortho_scale = 'ORTHO', 2.6
    scene.camera = camera
    scene.view_settings.view_transform = 'AgX'
    scene.render.image_settings.file_format = 'PNG'
    scene.render.filepath = str(ASSET / f'cooked-idle-{phase:02}-cpu-preview.png')
    bpy.ops.render.render(write_still=True)
    print('SHIELD_KNIGHT_CPU_PREVIEW', scene.render.filepath, flush=True)


if __name__ == '__main__':
    if '--prepare' in sys.argv:
        prepare()
    elif '--validate' in sys.argv:
        validate()
    elif '--preview' in sys.argv:
        preview(int(sys.argv[sys.argv.index('--pose') + 1]) if '--pose' in sys.argv else 0)
    else:
        argv = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
        parser = argparse.ArgumentParser(description=__doc__)
        parser.add_argument('--output', type=Path, default=OUT)
        cook(parser.parse_args(argv).output)
