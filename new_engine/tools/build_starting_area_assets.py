"""Source-authentic CC0 waystation kit for the custom Valwood renderer.

Python --download --prepare verifies official source bytes and tiers textures.
Blender --background --threads 2 --python this_file cooks static source meshes.
Python --validate audits the final embedded GLBs, sharing and GPU image budget.
Original source geometry/UVs are selected/decimated, never replaced by boxes.
"""
from pathlib import Path
import copy
import hashlib
import io
import json
import math
import struct
import sys
from concurrent.futures import ThreadPoolExecutor

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'assets/polyhaven/starting_area'
OUT = ROOT / 'projects/valwood_world/models'
sys.path.insert(0, str(ROOT / 'tools'))
from build_cinematic_assets import fetch, sha, texture_infos

KIT = [
    dict(name='starting_gate', source='modular_fort_01', objects=['modular_fort_01_wall_thin_gate_01'], turn=True, budget=2000),
    dict(name='starting_wall', source='modular_fort_01', objects=['modular_fort_01_wall_thin_straight_03'], turn=True, budget=1000),
    dict(name='starting_tower', source='modular_fort_01', objects=['modular_fort_01_tower_round'], budget=6000),
    dict(name='starting_platform', source='modular_wooden_pier', objects=['modular_wooden_pier_planks'], budget=6000),
    dict(name='starting_table', source='wooden_picnic_table', budget=9800, mid=2200),
    dict(name='starting_crate', source='wooden_crate_01', budget=7000),
    dict(name='starting_barrel', source='wooden_barrels_01', objects=['wooden_barrels_01_barrel01'], budget=10000, mid=2400),
    dict(name='starting_bucket', source='wooden_bucket_01', budget=6000),
    dict(name='starting_lantern', source='wooden_lantern_01', remove_glass=True, budget=8500),
    dict(name='starting_axe', source='wooden_axe_02', budget=3000),
    dict(name='starting_stump', source='tree_stump_01', budget=9000, mid=1800),
    dict(name='starting_firepit', source='stone_fire_pit', budget=4500),
    dict(name='starting_celandine', source='celandine_01', objects=['celandine_01_a_LOD0'], plant=True, budget=1000),
    dict(name='starting_wildflower', source='flower_heliophila', objects=['flower_heliophila_small'], plant=True, budget=6000, mid=1500),
]


def sources():
    return sorted({entry['source'] for entry in KIT})


def download():
    SOURCE.mkdir(parents=True, exist_ok=True)
    tasks = []
    for asset in sources():
        folder = SOURCE / asset
        folder.mkdir(exist_ok=True)
        fetch('https://api.polyhaven.com/files/' + asset, folder / 'metadata.json')
        metadata = json.loads((folder / 'metadata.json').read_text())
        tier = '2k' if asset == 'modular_fort_01' else '1k'
        record = metadata['gltf'][tier]['gltf']
        fetch(record['url'], folder / (asset + '.gltf'), record)
        tasks.extend((r['url'], folder / filename, r) for filename, r in record['include'].items())
        for key, value in metadata.items():
            if 'alpha' in key.lower() and tier in value and 'png' in value[tier]:
                record = value[tier]['png']
                tasks.append((record['url'], folder / ('source_' + key + '.png'), record))
        tasks.append(('https://cdn.polyhaven.com/asset_img/primary/' + asset + '.png?height=512', folder / 'preview.png', None))
    with ThreadPoolExecutor(max_workers=4) as pool:
        list(pool.map(lambda args: fetch(*args), tasks))
    provenance()


def prepare():
    from PIL import Image
    for asset in sources():
        folder = SOURCE / asset
        gltf = json.loads((folder / (asset + '.gltf')).read_text())
        original_textures = copy.deepcopy(gltf['textures'])
        original_images = copy.deepcopy(gltf['images'])
        new_images, new_textures, mapping = [], [], {}
        plant = any(c.get('plant') and c['source'] == asset for c in KIT)
        for material_index, material in enumerate(gltf['materials']):
            pbr = material['pbrMetallicRoughness']
            for kind, view in [('base', pbr.get('baseColorTexture')), ('normal', material.get('normalTexture')),
                               ('arm', pbr.get('metallicRoughnessTexture'))]:
                if not view:
                    continue
                old = original_textures[view['index']]
                uri = original_images[old['source']]['uri']
                hero = asset == 'modular_fort_01'
                limit = (2048 if hero else 1024) if kind == 'base' else ((1024 if hero else 512) if kind == 'normal' else (512 if hero else 256))
                key = (uri, limit, kind)
                if key not in mapping:
                    image = Image.open(folder / uri).convert('RGBA' if plant and kind == 'base' else 'RGB')
                    if plant and kind == 'base':
                        masks = list(folder.glob('source_*[Aa]lpha*.png'))
                        assert len(masks) == 1, (asset, masks)
                        mask = Image.open(masks[0]).convert('L')
                        if mask.size != image.size:
                            mask = mask.resize(image.size, Image.Resampling.LANCZOS)
                        image.putalpha(mask)
                    image.thumbnail((limit, limit), Image.Resampling.LANCZOS)
                    filename = 'runtime_' + str(material_index) + '_' + kind + '.png'
                    image.save(folder / filename, optimize=True)
                    mapping[key] = len(new_textures)
                    new_images.append({'uri': filename, 'mimeType': 'image/png'})
                    new_textures.append({'source': len(new_images)-1, 'sampler': 0})
                view['index'] = mapping[key]
            # Source ARM R is already AO; the loader consumes the packed map.
            material.pop('occlusionTexture', None)
            if plant:
                material.update(alphaMode='MASK', alphaCutoff=.35, doubleSided=True)
        if asset == 'wooden_lantern_01':
            # This renderer has no sorted glass pass. Retain the original frame,
            # hinges, handle and candle, removing only the transparent panes.
            for mesh in gltf['meshes']:
                mesh['primitives'] = [p for p in mesh['primitives'] if '_glass' not in gltf['materials'][p['material']].get('name', '')]
                assert mesh['primitives']
        gltf['images'] = new_images
        gltf['textures'] = new_textures
        gltf['samplers'] = [{'magFilter': 9729, 'minFilter': 9987, 'wrapS': 10497, 'wrapT': 10497}]
        (folder / 'runtime_prepared.gltf').write_text(json.dumps(gltf, indent=2))
        print('PREPARED', asset, flush=True)
    provenance()


def provenance():
    records = []
    for asset in sources():
        folder = SOURCE / asset
        if not folder.exists():
            continue
        for path in sorted(p for p in folder.rglob('*') if p.is_file()):
            reference = path.name in ['metadata.json', 'preview.png']
            records.append(dict(path=str(path.relative_to(SOURCE)), bytes=path.stat().st_size,
                                sha256=sha(path), source='https://polyhaven.com/a/' + asset,
                                license='Poly Haven website/API reference' if reference else 'CC0-1.0',
                                reference_only=reference, derived=path.name.startswith('runtime_')))
    (SOURCE / 'provenance.json').write_text(json.dumps(dict(asset_license='https://polyhaven.com/license',
        api_terms='https://github.com/Poly-Haven/Public-API', api_credit='Powered by Poly Haven', files=records), indent=2))


def cook():
    import bpy
    from mathutils import Matrix, Vector
    from bake_gltf_uv import bake_uv_transforms
    OUT.mkdir(parents=True, exist_ok=True)
    manifest = []
    def triangles(obj):
        return sum(len(p.vertices)-2 for p in obj.data.polygons)
    for config in KIT:
        for lod in ['near', 'mid'] if config.get('mid') else ['near']:
            bpy.ops.wm.read_factory_settings(use_empty=True)
            bpy.context.scene.render.threads_mode = 'FIXED'
            bpy.context.scene.render.threads = 2
            bpy.ops.import_scene.gltf(filepath=str(SOURCE / config['source'] / 'runtime_prepared.gltf'))
            chosen = config.get('objects')
            meshes = [o for o in bpy.context.scene.objects if o.type == 'MESH' and (chosen is None or o.name in chosen)]
            assert meshes and (chosen is None or len(meshes) == len(chosen)), (config, [o.name for o in meshes])
            for obj in list(bpy.context.scene.objects):
                if obj not in meshes:
                    bpy.data.objects.remove(obj, do_unlink=True)
            # Bake all original assembly transforms first, preserving attachments
            # such as the crate lid, bucket handle and lantern door.
            for obj in meshes:
                obj.data = obj.data.copy()
                obj.data.transform(obj.matrix_world)
                obj.matrix_world = Matrix.Identity(4)
                if config.get('turn'):
                    obj.data.transform(Matrix.Rotation(math.pi/2, 4, 'Z'))
            total = sum(triangles(o) for o in meshes)
            budget = config.get('mid') if lod == 'mid' else config['budget']
            if total > budget:
                for obj in meshes:
                    bpy.context.view_layer.objects.active = obj
                    modifier = obj.modifiers.new('Bounded original source geometry', 'DECIMATE')
                    modifier.ratio = min(1., budget / total)
                    bpy.ops.object.modifier_apply(modifier=modifier.name)
            points = [obj.matrix_world @ v.co for obj in meshes for v in obj.data.vertices]
            lo = Vector([min(p[k] for p in points) for k in range(3)])
            hi = Vector([max(p[k] for p in points) for k in range(3)])
            shift = Matrix.Translation(Vector([-(lo.x+hi.x)*.5, -(lo.y+hi.y)*.5, -lo.z]))
            for obj in meshes:
                obj.data.transform(shift)
                obj.data.update()
                obj.select_set(True)
            for material in bpy.data.materials:
                material.use_backface_culling = False
                if material.node_tree:
                    for node in material.node_tree.nodes:
                        if node.type == 'TEX_IMAGE' and node.image:
                            node.image.pack()
            name = config['name'] + ('_mid' if lod == 'mid' else '')
            path = OUT / (name + '.glb')
            bpy.ops.export_scene.gltf(filepath=str(path), export_format='GLB', export_yup=True,
                export_animations=False, export_cameras=False, export_lights=False, export_image_format='AUTO')
            bake_uv_transforms(path)
            points = [obj.matrix_world @ v.co for obj in meshes for v in obj.data.vertices]
            bounds = [[min(p[k] for p in points), max(p[k] for p in points)] for k in range(3)]
            # Manifest uses runtime Y-up order, with Z sign reversed on export.
            y_bounds = [bounds[0], bounds[2], [-bounds[1][1], -bounds[1][0]]]
            entry = dict(asset=name, source=config['source'], source_url='https://polyhaven.com/a/' + config['source'],
                source_objects=chosen or [o.name for o in meshes], license='CC0-1.0', triangles=sum(triangles(o) for o in meshes),
                bounds_y_up=y_bounds, bytes=path.stat().st_size, sha256=sha(path), vegetation=3 if config.get('plant') else 0,
                grounding='foundation' if config['source']=='modular_fort_01' else ('plant' if config.get('plant') else 'support'),
                adaptations=['selected original source geometry', 'horizontal center and ground origin', 'tiered embedded PBR', 'UV0 transforms baked'] +
                    (['removed glass panes; unlit source frame/candle retained'] if config.get('remove_glass') else []))
            manifest.append(entry)
            (OUT / 'starting_manifest.json').write_text(json.dumps(manifest, indent=2))
            print('COOKED', name, entry['triangles'], y_bounds, flush=True)
    print('Starting area cook complete', len(manifest), 'GLBs', flush=True)


def glb(path):
    data = path.read_bytes()
    assert struct.unpack_from('<III', data) == (0x46546C67, 2, len(data))
    length, kind = struct.unpack_from('<II', data, 12)
    assert kind == 0x4E4F534A
    gltf = json.loads(data[20:20+length])
    length, kind = struct.unpack_from('<II', data, 20+length)
    assert kind == 0x004E4942
    return gltf, data[-length:]


def validate():
    import numpy as np
    from PIL import Image
    entries = json.loads((OUT / 'starting_manifest.json').read_text())
    unique, by_asset = {}, {}
    for entry in entries:
        path = OUT / (entry['asset'] + '.glb')
        gltf, binary = glb(path)
        assert sha(path) == entry['sha256'] and not gltf.get('animations') and not gltf.get('skins')
        def read_accessor(index):
            ac = gltf['accessors'][index]
            bv = gltf['bufferViews'][ac['bufferView']]
            dimensions = {'SCALAR': 1, 'VEC2': 2, 'VEC3': 3, 'VEC4': 4}[ac['type']]
            dtype = np.dtype({5121: 'u1', 5123: '<u2', 5125: '<u4', 5126: '<f4'}[ac['componentType']])
            return np.ndarray((ac['count'], dimensions), dtype, binary,
                offset=bv.get('byteOffset', 0)+ac.get('byteOffset', 0),
                strides=(bv.get('byteStride', dimensions*dtype.itemsize), dtype.itemsize))
        count = 0
        for mesh in gltf['meshes']:
            for primitive in mesh['primitives']:
                assert primitive.get('mode', 4) == 4 and 'TEXCOORD_0' in primitive['attributes']
                accessor = gltf['accessors'][primitive['indices']]
                count += accessor['count'] // 3
                for key in ['POSITION', 'NORMAL', 'TEXCOORD_0']:
                    ac = gltf['accessors'][primitive['attributes'][key]]
                    assert ac['componentType'] == 5126
                    array = read_accessor(primitive['attributes'][key])
                    assert np.isfinite(array).all()
                positions = read_accessor(primitive['attributes']['POSITION'])
                indices = read_accessor(primitive['indices']).reshape(-1, 3)
                assert indices.max() < len(positions)
                normals = read_accessor(primitive['attributes']['NORMAL'])
                assert np.max(np.abs(np.linalg.norm(normals, axis=1)-1)) < .001
                triangles = positions[indices]
                area = np.sum(np.cross(triangles[:,1]-triangles[:,0], triangles[:,2]-triangles[:,0])**2, axis=1)
                assert np.all(area > 1e-18), entry['asset']
        assert count == entry['triangles'] and count <= 10000
        for material in gltf['materials']:
            assert material.get('alphaMode', 'OPAQUE') != 'BLEND'
            for view in texture_infos(material):
                assert view.get('texCoord', 0) == 0 and not view.get('extensions', {}).get('KHR_texture_transform')
        hashes = set()
        for image in gltf['images']:
            assert 'uri' not in image
            bv = gltf['bufferViews'][image['bufferView']]
            encoded = binary[bv.get('byteOffset', 0):bv.get('byteOffset', 0)+bv['byteLength']]
            digest = hashlib.sha256(encoded).hexdigest()
            with Image.open(io.BytesIO(encoded)) as im:
                w, h = im.size
            assert max(w, h) <= (2048 if entry['source']=='modular_fort_01' else 1024)
            memory = 0
            while True:
                memory += w*h*4
                if w == h == 1:
                    break
                w, h = max(1, w//2), max(1, h//2)
            unique[digest] = memory
            hashes.add(digest)
        by_asset[entry['asset']] = hashes
        print('VALIDATED', entry['asset'], count, 'triangles', len(hashes), 'images', flush=True)
    for entry in KIT:
        if entry.get('mid'):
            assert by_asset[entry['name']] == by_asset[entry['name']+'_mid']
    assert by_asset['starting_gate'] == by_asset['starting_wall'] == by_asset['starting_tower']
    memory = sum(unique.values())
    assert memory <= 160*1024*1024, memory
    report = dict(assets=len(entries), source_families=len(sources()), triangles=sum(e['triangles'] for e in entries),
        unique_embedded_images=len(unique), rgba8_mip_bytes=memory, rgba8_mip_mib=memory/(1024*1024),
        shared_texture_groups=[['starting_gate','starting_wall','starting_tower']] + [[c['name'], c['name']+'_mid'] for c in KIT if c.get('mid')])
    (SOURCE / 'validation.json').write_text(json.dumps(report, indent=2))
    print('PASS starting-area source/GLB validation', json.dumps(report), flush=True)


if __name__ == '__main__':
    if '--download' in sys.argv:
        download()
    if '--prepare' in sys.argv:
        prepare()
    if '--validate' in sys.argv:
        validate()
    if not any(arg in sys.argv for arg in ['--download', '--prepare', '--validate']):
        cook()
