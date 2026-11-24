#!/usr/bin/env python3
"""Cook selected purchased Nature.zip meshes into bounded, static Valwood GLBs.

python3 tools/import_bep_nature.py --prepare
env OMP_NUM_THREADS=2 OPENBLAS_NUM_THREADS=1 blender -b -t 2 \
  --disable-autoexec --factory-startup --python-exit-code 1 \
  --python tools/import_bep_nature.py
python3 tools/import_bep_nature.py --validate

Source bytes are preserved. Archive names and material labels are data. FBX
import does not execute source scripts, and no source stage/LOD duplicates enter
the exported meshes. The original per-family UV atlases are remapped explicitly.
"""
from pathlib import Path, PurePosixPath
import hashlib
import io
import json
import os
import struct
import sys
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[1]
ASSET = ROOT / 'assets/purchased/bep_nature'
SOURCE = ASSET / 'source'
DERIVED = ASSET / 'derived'
OUT = ROOT / 'projects/valwood_world/models'
ARCHIVE = Path('/home/computer/Downloads/Nature.zip')
MANIFEST = 'bep_nature_manifest.json'
REFERENCE_SCENES = ['Oak_Trees', 'Beech_Trees', 'Bushes_01', 'Bushes_03', 'GrassPlantsFlowers', 'Palm_Trees']
# Source FBXs often contain all authored LOD objects. Select exact objects;
# exporting their overlapping siblings would cause the very shimmer we avoid.
SPECS = [
    dict(alias='oak', source='Oak_01', near='Oak_01', mid='Oak_01_LOD_01', height=22., family='oak', tree=True),
    dict(alias='beech', source='Beech_03', near='Beech_03', mid='Beech_03_LOD_01', height=26., family='beech', tree=True),
    dict(alias='bush_a', source='Bush01_01', near='Bush01_01', mid='Bush01_01_LOD_01', height=2.2, family='bush_a'),
    dict(alias='bush_b', source='Bush03_01', near='Bush03_01', mid='Bush03_01_LOD_01', height=2.6, family='bush_b'),
    dict(alias='grass_long', source='Long_Grass_01', near='Long_Grass_01', mid_source='Long_Grass_01_LOD_01', mid='Long_Grass_01_LOD_01', height=.90, family='grass'),
    dict(alias='grass_broad', source='Large_Grass', near='Long_Grass_01', mid=None, height=.80, family='grass'),
    dict(alias='fern', source='Fern_01', near='Fern_01', mid='Fern_01_LOD_01(Clone)', height=.95, family='fern'),
    dict(alias='reed', source='Reed_Plant_01', near='Reed_Plant_01', mid='Reed_Plant_01_LOD_01(Clone)', height=1.25, family='reed'),
    # The 307-triangle alternate Clover_01 has a >.8m-deep stem. The
    # standalone authored 121-triangle LOD has the usable groundcover shape.
    dict(alias='clover', source='Clover_01_LOD_01', near='Clover_01_LOD_01', mid=None, height=.22, family='clover'),
    dict(alias='palm', source='Palm_plant_01_01_rw', near='Palm_plant_01_01', mid='Palm_plant_01_01_LOD_01', height=4.8, family='palm'),
]
FAMILIES = {
    'oak': (1024, 'Oak_01_Dif.png', 'Oak_01_Nor.png'),
    'beech': (1024, 'Beech_01_Dif.png', 'Beech_01_Nor.png'),
    'bush_a': (512, 'Bush_01_Dif.png', 'Bush_01_Nor.png'),
    'bush_b': (512, 'Bush_03_Dif.png', 'Bush_03_Nor.png'),
    'grass': (512, 'Large_Grass_01_Dif.tga', 'Large_Grass_01_Nor.png'),
    'fern': (512, 'Fern.tga', 'Fern_Nor.png'),
    'reed': (512, 'Reed_Plant_Dif.tga', 'Reed_Plant_Nor.png'),
    'clover': (512, 'Clover_Dif.tga', 'Clover_Nor.png'),
    'palm': (1024, 'Palm_Plant_a_Dif.png', 'Palm_Plant_a_Nor.png'),
}


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def immutable_write(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    if path.exists():
        assert path.read_bytes() == data, 'Preserved source changed: ' + str(path)
    else:
        path.write_bytes(data)


def selected_members():
    names = {'Nature/' + s['source'] + '.fbx' for s in SPECS}
    names.update('Nature/' + s['mid_source'] + '.fbx' for s in SPECS if s.get('mid_source'))
    names.update('Nature/BlendFiles/Textures/' + name for _, *maps in FAMILIES.values() for name in maps)
    names.update('Nature/BlendFiles/' + name + '.blend' for name in REFERENCE_SCENES)
    return sorted(names)


def prepare():
    selected = selected_members()
    records = []
    with zipfile.ZipFile(ARCHIVE) as archive:
        total = 0
        for name in selected:
            entry = archive.getinfo(name)
            parts = PurePosixPath(name)
            assert not parts.is_absolute() and '..' not in parts.parts and ':' not in name
            assert (entry.external_attr >> 16) & 0o170000 != 0o120000
            assert 0 < entry.file_size < 40 * 1024 * 1024
            total += entry.file_size
            assert total < 400 * 1024 * 1024
            data = archive.read(entry)  # ZipFile verifies the source CRC.
            immutable_write(SOURCE / name, data)
            records.append(dict(file=str((SOURCE / name).relative_to(ASSET)), archive_member=name,
                bytes=len(data), crc32=f'{entry.CRC:08x}', sha256=hashlib.sha256(data).hexdigest()))
    ASSET.mkdir(parents=True, exist_ok=True)
    (ASSET / 'PRESERVED.json').write_text(json.dumps(dict(archive=str(ARCHIVE),
        archive_bytes=ARCHIVE.stat().st_size, selected_files=records), indent=2) + '\n')
    print('PRESERVED', len(records), 'selected original files,', total, 'bytes', flush=True)


def prepare_images():
    import numpy as np
    from PIL import Image
    from scipy.ndimage import distance_transform_edt
    DERIVED.mkdir(parents=True, exist_ok=True)
    records = {}
    for family, (side, diffuse, normal) in FAMILIES.items():
        source_image = Image.open(SOURCE / 'Nature/BlendFiles/Textures' / diffuse).convert('RGBA')
        source = np.asarray(source_image, dtype=np.float32) / 255.
        alpha = source[:, :, 3]
        coverage = float((alpha >= .5).mean())
        # Filter colour in linear light and premultiplied coverage. This avoids
        # dark/white fringes from the source's transparent atlas padding.
        rgb = source[:, :, :3]
        rgb = np.where(rgb <= .04045, rgb / 12.92, ((rgb + .055) / 1.055) ** 2.4)
        def resize_float(channel):
            return np.asarray(Image.fromarray(channel.astype('float32'), 'F').resize(
                (side, side), Image.Resampling.LANCZOS), dtype=np.float32)
        resized_alpha = np.clip(resize_float(alpha), 0, 1)
        resized_rgb = np.stack([resize_float(rgb[:, :, c] * alpha) for c in range(3)], axis=2)
        resized_rgb /= np.maximum(resized_alpha[:, :, None], 1.e-6)
        resized_rgb = np.clip(resized_rgb, 0, 1)
        # Preserve the source atlas's .5-mask coverage through resolution
        # reduction; runtime still uses the existing stable masked foliage path.
        low, high = .25, 4.
        for _ in range(24):
            factor = (low + high) * .5
            if float((resized_alpha * factor >= .5).mean()) < coverage:
                low = factor
            else:
                high = factor
        alpha_scale = (low + high) * .5
        resized_alpha = np.clip(resized_alpha * alpha_scale, 0, 1)
        valid = resized_alpha > .02
        assert valid.any()
        # Edge-extend transparent texels from their nearest real artwork so
        # trilinear/anisotropic samples never reveal black premultiplied RGB.
        nearest = distance_transform_edt(~valid, return_distances=False, return_indices=True)
        resized_rgb[~valid] = resized_rgb[nearest[0][~valid], nearest[1][~valid]]
        resized_rgb = np.where(resized_rgb <= .0031308, resized_rgb * 12.92,
            1.055 * resized_rgb ** (1 / 2.4) - .055)
        base = np.uint8(np.clip(np.rint(np.dstack([resized_rgb, resized_alpha]) * 255), 0, 255))
        base_path = DERIVED / (family + '_base.png')
        Image.fromarray(base, 'RGBA').save(base_path, compress_level=6)
        normal_image = Image.open(SOURCE / 'Nature/BlendFiles/Textures' / normal).convert('RGB')
        normal_image = normal_image.resize((side, side), Image.Resampling.LANCZOS)
        vectors = np.asarray(normal_image, dtype=np.float32) / 127.5 - 1
        vectors /= np.maximum(np.linalg.norm(vectors, axis=2, keepdims=True), 1.e-6)
        normal_path = DERIVED / (family + '_normal.png')
        Image.fromarray(np.uint8(np.clip(np.rint((vectors + 1) * 127.5), 0, 255)), 'RGB').save(normal_path, compress_level=6)
        records[family] = dict(side=side, source_diffuse=diffuse, source_normal=normal,
            original_size=list(source_image.size), original_mask_coverage=coverage,
            cooked_mask_coverage=float((base[:, :, 3] >= 128).mean()), alpha_scale=alpha_scale,
            maps=[dict(file=str(path.relative_to(ROOT)), sha256=sha(path), bytes=path.stat().st_size)
                for path in [base_path, normal_path]])
        print('TEXTURES', family, side, 'coverage', coverage, records[family]['cooked_mask_coverage'], flush=True)
    (ASSET / 'TEXTURES.json').write_text(json.dumps(records, indent=2) + '\n')
    return records


def inspect_source_scenes():
    import bpy
    prepare()
    records = []
    for name in REFERENCE_SCENES:
        path = SOURCE / ('Nature/BlendFiles/' + name + '.blend')
        bpy.ops.wm.open_mainfile(filepath=str(path), load_ui=False, use_scripts=False)
        records.append(dict(source=str(path.relative_to(ROOT)),
            units=dict(system=bpy.context.scene.unit_settings.system,
                scale_length=bpy.context.scene.unit_settings.scale_length),
            meshes=[dict(name=o.name, materials=[m.name if m else None for m in o.data.materials])
                for o in bpy.data.objects if o.type == 'MESH'],
            materials=[dict(name=m.name, roughness=m.roughness, nodes=[dict(name=n.name, type=n.type,
                image=n.image.filepath if n.type == 'TEX_IMAGE' and n.image else None)
                for n in m.node_tree.nodes] if m.node_tree else [],
                links=[dict(source=l.from_node.name + '.' + l.from_socket.name,
                    target=l.to_node.name + '.' + l.to_socket.name) for l in m.node_tree.links]
                    if m.node_tree else []) for m in bpy.data.materials],
            images=[dict(name=i.name, path=i.filepath, size=list(i.size), packed=bool(i.packed_file))
                for i in bpy.data.images],
            texts=[dict(name=t.name, lines=len(t.lines)) for t in bpy.data.texts]))
        print('INSPECTED source scene', name, flush=True)
    (ASSET / 'BLEND_REFERENCE_INSPECTION.json').write_text(json.dumps(records, indent=2) + '\n')


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
    encoded = json.dumps(data, separators=(',', ':')).encode()
    encoded += b' ' * (-len(encoded) % 4)
    binary += b'\0' * (-len(binary) % 4)
    Path(path).write_bytes(struct.pack('<III', 0x46546c67, 2, 28 + len(encoded) + len(binary)) +
        struct.pack('<II', len(encoded), 0x4e4f534a) + encoded +
        struct.pack('<II', len(binary), 0x004e4942) + binary)


def attach_material(path, spec, roughness):
    data, binary = read_glb(path)
    output = bytearray(binary)
    data['images'] = []
    data['textures'] = []
    data['samplers'] = [dict(magFilter=9729, minFilter=9987, wrapS=10497, wrapT=10497)]
    for i, suffix in enumerate(['base', 'normal']):
        output += b'\0' * (-len(output) % 4)
        encoded = (DERIVED / (spec['family'] + '_' + suffix + '.png')).read_bytes()
        view = len(data['bufferViews'])
        data['bufferViews'].append(dict(buffer=0, byteOffset=len(output), byteLength=len(encoded)))
        output += encoded
        data['images'].append(dict(name='bep_nature_' + spec['family'] + '_' + suffix,
            bufferView=view, mimeType='image/png'))
        data['textures'].append(dict(sampler=0, source=i))
    data['materials'] = [dict(name='Purchased ' + spec['family'] + ' masked atlas',
        pbrMetallicRoughness=dict(baseColorFactor=[1, 1, 1, 1], baseColorTexture=dict(index=0),
            metallicFactor=0, roughnessFactor=roughness), normalTexture=dict(index=1),
        emissiveFactor=[0, 0, 0], alphaMode='MASK', alphaCutoff=.5, doubleSided=True)]
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
    assert len(data['meshes']) == len(data['materials']) == 1 and len(data['images']) == 2
    assert not any(data.get(k) for k in ['skins', 'animations', 'cameras', 'extensionsRequired'])
    assert len(data['meshes'][0]['primitives']) == 1
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
    indices = accessor(primitive['indices']).reshape(-1, 3)
    assert len(p) == len(n) == len(uv) and all(np.isfinite(a).all() for a in [p, n, uv])
    assert len(indices) and indices.max() < len(p)
    assert np.max(np.abs(np.linalg.norm(n, axis=1) - 1)) < .005
    areas = np.linalg.norm(np.cross(p[indices[:, 1]] - p[indices[:, 0]], p[indices[:, 2]] - p[indices[:, 0]]), axis=1)
    # Source foliage sometimes includes nearly collapsed stem/leaf triangles;
    # degenerate export geometry is removed during the offline cook.
    assert (areas > 1.e-12).all(), (str(path), int((areas <= 1.e-12).sum()))
    for node in data['nodes']:
        assert not node.get('matrix') and node.get('translation', [0, 0, 0]) == [0, 0, 0]
        assert node.get('scale', [1, 1, 1]) == [1, 1, 1]
    images = []
    for record in data['images']:
        v = data['bufferViews'][record['bufferView']]
        encoded = binary[v.get('byteOffset', 0):v.get('byteOffset', 0) + v['byteLength']]
        decoded = Image.open(io.BytesIO(encoded))
        images.append(dict(name=record['name'], size=list(decoded.size),
            sha256=hashlib.sha256(encoded).hexdigest(), bytes=len(encoded), mode=decoded.mode))
    lo, hi = p.min(axis=0), p.max(axis=0)
    material = data['materials'][0]
    assert material['alphaMode'] == 'MASK' and material['alphaCutoff'] == .5 and material['doubleSided']
    assert material['pbrMetallicRoughness']['metallicFactor'] == 0
    assert material['emissiveFactor'] == [0, 0, 0]
    return dict(triangles=len(indices), vertices=len(p), primitive_count=1, material_count=1,
        bounds_min=lo.tolist(), bounds_max=hi.tolist(), dimensions_m=(hi-lo).tolist(), images=images,
        materials=data['materials'], bytes=path.stat().st_size, sha256=sha(path),
        geometry_checks=dict(finite=True, unit_normals=True, indices_in_range=True,
            nondegenerate_triangles=len(indices), uv0_only=True))


def validate(directory=OUT):
    directory = Path(directory)
    manifest = json.loads((directory / MANIFEST).read_text())
    textures = json.loads((ASSET / 'TEXTURES.json').read_text())
    assert len(manifest['assets']) == 20
    images = {}
    for record in manifest['assets']:
        actual = inspect(directory / record['file'])
        assert actual['sha256'] == record['sha256']
        assert actual['triangles'] == record['triangles'] and 0 < actual['triangles'] < 10000
        if record['alias'] in ['grass_long', 'grass_broad']:
            assert actual['triangles'] < 1000
        assert abs(actual['bounds_min'][1]) < .00005
        assert max(actual['dimensions_m']) > .1
        assert [i['sha256'] for i in actual['images']] == [m['sha256'] for m in textures[record['family']]['maps']]
        for image in actual['images']:
            images[image['sha256']] = image
    for spec in SPECS:
        near, mid = [r for r in manifest['assets'] if r['alias'] == spec['alias']]
        assert mid['triangles'] < near['triangles']
        assert near['scale_to_metres'] == mid['scale_to_metres']
        assert abs(near['dimensions_m'][1] - spec['height']) < .0001
        # Leaf card simplification changes extremal tips slightly; both LODs
        # retain exact root placement and source-to-metre scale.
        assert abs(mid['dimensions_m'][1] - near['dimensions_m'][1]) < max(.12, spec['height'] * .02)
    for record in json.loads((ASSET / 'PRESERVED.json').read_text())['selected_files']:
        assert sha(ASSET / record['file']) == record['sha256']
    assert len(images) == 18
    estimated_bytes = sum(i['size'][0] * i['size'][1] * 4 * 4 / 3 for i in images.values())
    assert estimated_bytes <= 60 * 1048576
    budget = dict(families=len(FAMILIES), variations=len(SPECS), models=20, unique_images=len(images),
        estimated_rgba8_mips_mib=estimated_bytes / 1048576,
        near_triangles=sum(r['triangles'] for r in manifest['assets'] if r['lod'] == 'near'),
        mid_triangles=sum(r['triangles'] for r in manifest['assets'] if r['lod'] == 'mid'),
        model_bytes=sum(r['bytes'] for r in manifest['assets']))
    print('VALIDATED purchased Nature:', json.dumps(budget), flush=True)
    return budget


def preview():
    """CPU-only contact sheet of the actual runtime GLBs, each fit to its panel."""
    import bpy
    from mathutils import Vector
    from PIL import Image, ImageDraw
    panels = []
    for spec in SPECS:
        bpy.ops.wm.read_factory_settings(use_empty=True)
        bpy.ops.import_scene.gltf(filepath=str(OUT / ('bep_nature_' + spec['alias'] + '_near.glb')))
        meshes = [o for o in bpy.context.scene.objects if o.type == 'MESH']
        points = [o.matrix_world @ v.co for o in meshes for v in o.data.vertices]
        lo = Vector([min(v[i] for v in points) for i in range(3)])
        hi = Vector([max(v[i] for v in points) for i in range(3)])
        centre = (lo + hi) * .5
        side = max(hi-lo)
        bpy.ops.mesh.primitive_plane_add(size=side * 8, location=(0, 0, -.005))
        ground = bpy.data.materials.new('Neutral preview floor')
        ground.diffuse_color = (.17, .19, .16, 1)
        bpy.context.object.data.materials.append(ground)
        world = bpy.data.worlds.new('Preview daylight')
        world.use_nodes = True
        world.node_tree.nodes['Background'].inputs[0].default_value = (.65, .76, .9, 1)
        world.node_tree.nodes['Background'].inputs[1].default_value = .8
        bpy.context.scene.world = world
        bpy.ops.object.light_add(type='AREA', location=centre + Vector((-side, -side, side * 1.7)))
        bpy.context.object.data.energy = 750 * side * side
        bpy.context.object.data.shape = 'DISK'
        bpy.context.object.data.size = side
        bpy.ops.object.camera_add(location=centre + Vector((side * 1.1, -side * 1.8, side * .65)))
        camera = bpy.context.object
        camera.rotation_euler = (centre-camera.location).to_track_quat('-Z', 'Y').to_euler()
        camera.data.type = 'ORTHO'
        camera.data.ortho_scale = side * 1.18
        scene = bpy.context.scene
        scene.camera = camera
        scene.render.engine = 'CYCLES'
        scene.cycles.device = 'CPU'
        scene.cycles.samples = 16
        scene.cycles.use_denoising = False
        scene.render.threads_mode = 'FIXED'
        scene.render.threads = 2
        scene.render.resolution_x = scene.render.resolution_y = 384
        scene.render.resolution_percentage = 100
        scene.view_settings.view_transform = 'AgX'
        scene.render.image_settings.file_format = 'PNG'
        path = ASSET / ('preview-' + spec['alias'] + '.png')
        scene.render.filepath = str(path)
        bpy.ops.render.render(write_still=True)
        panel = Image.new('RGB', (384, 426), (24, 27, 25))
        panel.paste(Image.open(path).convert('RGB'), (0, 0))
        draw = ImageDraw.Draw(panel)
        draw.text((10, 389), spec['alias'] + ' | actual purchased GLB', fill=(230, 235, 230))
        draw.text((10, 406), 'height ' + str(spec['height']) + 'm | individually framed', fill=(170, 190, 170))
        panels.append(panel)
    contact = Image.new('RGB', (5 * 384, 2 * 426))
    for i, panel in enumerate(panels):
        contact.paste(panel, ((i % 5) * 384, (i // 5) * 426))
    contact.save(ASSET / 'cooked-runtime-contact.png')
    print('CPU_PREVIEW', str(ASSET / 'cooked-runtime-contact.png'), flush=True)


def cook(reuse_images=False):
    import bpy
    import bmesh
    from mathutils import Matrix, Vector
    prepare()
    textures = json.loads((ASSET / 'TEXTURES.json').read_text()) if reuse_images else prepare_images()
    records = []
    source_records = []
    OUT.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='.bep-nature-', dir=OUT) as temporary:
        stage = Path(temporary)
        for spec in SPECS:
            bpy.ops.wm.read_factory_settings(use_empty=True)
            source_path = SOURCE / ('Nature/' + spec['source'] + '.fbx')
            bpy.ops.import_scene.fbx(filepath=str(source_path), use_anim=False)
            near_source = bpy.data.objects[spec['near']]
            roughness = float(near_source.data.materials[0].roughness)
            if spec.get('mid_source'):
                bpy.ops.import_scene.fbx(filepath=str(SOURCE / ('Nature/' + spec['mid_source'] + '.fbx')), use_anim=False)
            selected = [near_source, bpy.data.objects[spec['mid']] if spec.get('mid') else near_source]
            selected_names = [obj.name for obj in selected]
            originals = []
            for source in selected:
                obj = source.copy()
                obj.data = source.data.copy()
                originals.append(obj)
            p = [near_source.matrix_world @ v.co for v in near_source.data.vertices]
            lo = Vector([min(v[i] for v in p) for i in range(3)])
            hi = Vector([max(v[i] for v in p) for i in range(3)])
            scale = spec['height'] / (hi.z-lo.z)
            # Tree roots preserve authored trunk XY0. Plants recenter their
            # footprint; Large_Grass's source is displaced >2m from its origin.
            origin = Vector((0, 0, lo.z)) if spec.get('tree') else Vector(((lo.x+hi.x)*.5, (lo.y+hi.y)*.5, lo.z))
            canonical = Matrix.Scale(scale, 4) @ Matrix.Translation(-origin)
            source_records.append(dict(alias=spec['alias'], source_file=str(source_path.relative_to(ROOT)),
                source_objects=selected_names, source_bounds=[list(lo), list(hi)],
                scale_to_metres=scale, roughness_from_fbx=roughness,
                material_names=[m.name for m in near_source.data.materials],
                source_uv_layers=[u.name for u in near_source.data.uv_layers]))
            for lod, source in zip(['near', 'mid'], originals):
                bpy.ops.object.select_all(action='SELECT')
                bpy.ops.object.delete(use_global=False)
                obj = source.copy()
                obj.data = source.data.copy()
                bpy.context.collection.objects.link(obj)
                obj.parent = None
                obj.matrix_world = canonical @ source.matrix_world
                bpy.context.view_layer.objects.active = obj
                obj.select_set(True)
                bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
                obj.data.materials.clear()
                for polygon in obj.data.polygons:
                    polygon.material_index = 0
                # Only the two sources without authored matching mid objects
                # need offline simplification. UVs/material identity stay fixed.
                if lod == 'mid' and spec['mid'] is None:
                    source_limits = [(min(v.co[i] for v in obj.data.vertices), max(v.co[i] for v in obj.data.vertices))
                        for i in range(3)]
                    modifier = obj.modifiers.new('Offline source silhouette LOD', 'DECIMATE')
                    modifier.ratio = .49
                    modifier.use_collapse_triangulate = True
                    bpy.ops.object.modifier_apply(modifier=modifier.name)
                    if obj.data.has_custom_normals:
                        bpy.ops.mesh.customdata_custom_splitnormals_clear()
                    # Collapsing a low-poly stem can extrapolate below its
                    # authored root. Keep simplified vertices inside the
                    # original bounds rather than moving the entire clump up.
                    for vertex in obj.data.vertices:
                        for axis, (minimum, maximum) in enumerate(source_limits):
                            vertex.co[axis] = min(max(vertex.co[axis], minimum), maximum)
                obj.data.calc_loop_triangles()
                degenerate = any((obj.data.vertices[t.vertices[1]].co-obj.data.vertices[t.vertices[0]].co).cross(
                    obj.data.vertices[t.vertices[2]].co-obj.data.vertices[t.vertices[0]].co).length <= 1.e-12
                    for t in obj.data.loop_triangles)
                if degenerate:
                    mesh = bmesh.new()
                    mesh.from_mesh(obj.data)
                    bmesh.ops.dissolve_degenerate(mesh, dist=1.e-7, edges=list(mesh.edges))
                    mesh.to_mesh(obj.data)
                    mesh.free()
                # Ground each native LOD at Y0 (BlenderZ0), correcting only the
                # tiny root extrema change, never moving its root in XZ.
                dz = -min(v.co.z for v in obj.data.vertices)
                obj.data.transform(Matrix.Translation((0, 0, dz)))
                obj.name = 'bep_nature_' + spec['alias']
                path = stage / (obj.name + '_' + lod + '.glb')
                bpy.ops.export_scene.gltf(filepath=str(path), export_format='GLB', use_selection=True,
                    export_yup=True, export_animations=False, export_cameras=False,
                    export_lights=False, export_materials='NONE', export_apply=True)
                attach_material(path, spec, roughness)
                record = dict(alias=spec['alias'], runtime_alias=obj.name, family=spec['family'],
                    file=path.name, lod=lod, source_file=str(source_path.relative_to(ROOT)),
                    source_object=selected_names[0 if lod == 'near' else 1],
                    scale_to_metres=scale, source_origin=list(origin), lod_root_correction_m=dz,
                    **inspect(path))
                records.append(record)
                print('COOKED', path.name, record['triangles'], record['dimensions_m'], flush=True)
        manifest = dict(schema_version=1, source='User-supplied purchased Nature.zip',
            provenance='assets/purchased/bep_nature/PROVENANCE.json',
            origin='Metres, Y up, minY0. Trees retain authored trunkXZ0; lower plants are footprint-centred.',
            material_note='Explicit source family UV0 remap, shared masked sRGB colour and linear tangent normal. Original normal orientation retained; no unproven spec-to-roughness conversion. Nonmetal; constant imported FBX roughness; no emission.',
            lod_note='Eight native authored near/mid pairs; broad grass and clover use offline half-triangle mid. Only one source LOD mesh per output, no overlaid duplicates.',
            assets=records)
        (stage / MANIFEST).write_text(json.dumps(manifest, indent=2) + '\n')
        manifest['budget'] = validate(stage)
        (stage / MANIFEST).write_text(json.dumps(manifest, indent=2) + '\n')
        provenance = dict(schema_version=1, archive=str(ARCHIVE), archive_bytes=ARCHIVE.stat().st_size,
            source_files='PRESERVED.json', source_inspection=source_records, textures=textures,
            source_preservation='Original ZIP unchanged; selected member bytes preserved and SHA256 recorded; ZIP CRC checked on extraction. FBX import under disabled script execution.',
            license='User supplied purchased content; no separate license included in archive. No public redistribution or open-content claim.',
            texture_adaptation='Three family atlases at1024px, six at512px. Linear-light premultiplied resize, .5mask coverage preservation, transparentRGB edge extension; tangent normals renormalized. Source normalY retained, no inferred inversion. Legacy spec/TL maps omitted because their conversion semantics are not established.',
            geometry_adaptation='Original UV0, normals and authored LOD geometry retained except degenerate cleanup; metric height adaptation and root grounding; grass_broad/clover mid simplified offline. No source lights/cameras/stages or co-located LOD meshes.',
            validation=manifest['budget'])
        (ASSET / 'PROVENANCE.json').write_text(json.dumps(provenance, indent=2) + '\n')
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
    elif '--preview' in args:
        preview()
    elif '--inspect-source-scenes' in args:
        inspect_source_scenes()
    else:
        cook(reuse_images='--geometry-only' in args)
