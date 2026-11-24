#!/usr/bin/env python3
"""Import the user's Adam Knight Horseman as a static mounted village guard.

Run with system Python: python3 tools/import_village_horseman.py [--preview]
Original archive/source files are retained. FBX skeletal evaluation happens
offline in Blender; the engine receives two static, embedded, atlas-sharing
GLBs. The source contains long horse/rider timelines; this import deliberately
uses their aligned frame 1 and does not claim runtime horseback animation.
"""
import argparse
import hashlib
import json
import math
import os
from pathlib import Path, PurePosixPath
import shutil
import struct
import subprocess
import sys
import zipfile

ROOT = Path(__file__).resolve().parents[1]
ASSET = ROOT / 'assets/purchased/horseman'
SOURCE = ASSET / 'source'
OUT = ROOT / 'projects/valwood_world/models'
ARCHIVE = Path('/home/computer/Downloads/adam-knight-horseman-l(1).zip')
FBXS = ['AdamKnight_Horse.FBX', 'AdamKnight_Horsenam.FBX']
RIDER_PARTS = {'Adam_body_All', 'AK_ChestArmor', 'AK_HandsArmor', 'AK_Helmet_A',
               'AK_Gloves', 'AK_ShouldersArmor', 'AK_LegArmor', 'AK_Boots',
               'AK_Shield_B', 'AK_Sword', 'AK_Scabbard'}
# Six 512px tiles and three 256px tiles in a 2048x1024 atlas. Material UVs
# remain independent; every outfit/horse component is retained as geometry.
RECIPES = {
    'horse_body': ('Horse_Textures/Horse_Black.tif', 'Horse_Textures/Horse_Normals.tif', 'Horse_Textures/Horse_Black_Specular.tif', False, (0, 0, 512)),
    'horse_armor': ('Horse_Textures/HellHorse_Armor.tif', 'Horse_Textures/HellHorse_Armor_Normals.tif', 'Horse_Textures/HellHorse_Armor_Specular.tif', True, (512, 0, 512)),
    'rider_armor': ('AdamKnight_Armor_A.tif', 'AdamKnight_Armor_Normals.tif', 'AdamKnight_Armor_Specular_A.tif', True, (1024, 0, 512)),
    'rider_body': ('AdamKnight_Body.tif', 'AdamKnight_Body_normals.tif', 'AdamKnight_Body_Specular.tif', False, (1536, 0, 512)),
    'weapons': ('AdamKnight_Weapons_A.tif', 'AdamKnight_Weapons_Normals.tif', 'AdamKnight_Weapons_Specular.tif', True, (0, 512, 512)),
    'shield': ('AdamKnight_Shield_ClearMetal.tif', 'AdamKnight_Shield_Normals.tif', 'AdamKnight_Shield_Specular.tif', True, (512, 512, 512)),
    'head': ('Adam_Head_A.tif', 'Adam_Head_A_normals.tif', 'Adam_Head_A_specular.tif', False, (1024, 512, 256)),
    'horse_hair': ('Horse_Textures/Horse_Hair_Black.tif', 'Horse_Textures/Horse_Hair_Normals.tif', 'Horse_Textures/Horse_Hair_Black_Specular.tif', False, (1280, 512, 256)),
    'feather': ('AK_Feather.tif', None, 'AK_Feather_Specular.tif', False, (1536, 512, 256)),
}
MATERIALS = {'Horse_Black': 'horse_body', 'HH_Armor': 'horse_armor',
             'horse_hair_Black': 'horse_hair', 'AdamKnight_Armor_A': 'rider_armor',
             'Adam_Body': 'rider_body', 'Adam_Head': 'head', 'Adam_Eyes': 'head',
             'Adam_Eyelids': 'head', 'AdamKnight_Weapons_A': 'weapons',
             'AK_MarkShield_ClearMetal': 'shield', 'AdamKnight_Feather': 'feather'}


def sha(path):
    result = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for chunk in iter(lambda: stream.read(1024*1024), b''):
            result.update(chunk)
    return result.hexdigest()


def extract():
    """Stream relevant nested entries, never loading the 2.6GB ZIP into RAM."""
    ASSET.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(ARCHIVE) as archive:
        inventory = [dict(path=i.filename, bytes=i.file_size, compressed_bytes=i.compress_size,
                          crc32=f'{i.CRC:08x}') for i in archive.infolist()]
        for name in ['CH_Adam_Knight_Horseman_LIGHT.rar', 'AdamHorsemanKnight_OBJ.rar']:
            target = ASSET / name
            if not target.exists() or target.stat().st_size != archive.getinfo(name).file_size:
                with archive.open(name) as source, target.open('wb') as destination:
                    shutil.copyfileobj(source, destination, length=1024*1024)
        (ASSET/'archive-inventory.json').write_text(json.dumps(dict(source=str(ARCHIVE),
            bytes=ARCHIVE.stat().st_size, entries=inventory), indent=2)+'\n')
    rar = ASSET/'CH_Adam_Knight_Horseman_LIGHT.rar'
    names = subprocess.check_output(['unrar', 'lb', str(rar)], text=True).splitlines()
    assert all(not PurePosixPath(n).is_absolute() and '..' not in PurePosixPath(n).parts and ':' not in n for n in names)
    relevant = [n for n in names if n.startswith('Textures/') or
                n.startswith('Export/FBX/') and n.lower().endswith('.fbx')]
    SOURCE.mkdir(exist_ok=True)
    subprocess.run(['unrar', 'x', '-o-', '-idq', str(rar), *relevant, str(SOURCE)+'/'], check=True)
    return inventory


def prepare_maps():
    from PIL import Image
    import numpy as np
    base = Image.new('RGBA', (2048, 1024), (50, 45, 40, 255))
    normal = Image.new('RGB', (2048, 1024), (128, 128, 255))
    orm = Image.new('RGB', (2048, 1024), (255, 190, 0))
    hashes = {}
    for label, (diffuse, normals, specular, metal_surface, rect) in RECIPES.items():
        x, y, tile = rect
        inner = tile-8
        with Image.open(SOURCE/'Textures'/diffuse) as source:
            color = source.convert('RGBA').resize((inner, inner), Image.Resampling.LANCZOS)
        with Image.open(SOURCE/'Textures'/specular) as source:
            spec = np.asarray(source.convert('RGB').resize((inner, inner), Image.Resampling.LANCZOS), dtype=np.float32).mean(axis=2)/255
        rgb = np.asarray(color, dtype=np.float32)[:, :, :3]/255
        neutral = np.clip(1-(rgb.max(axis=2)-rgb.min(axis=2))*2.2, 0, 1)
        metallic = (.82 * neutral * np.clip((spec-.10)/.45, 0, 1)) if metal_surface else np.zeros_like(spec)
        roughness = np.clip(.78-.24*spec-.12*metallic, .36, .82)
        packed = np.stack([np.ones_like(spec), roughness, metallic], axis=2)
        packed = Image.fromarray(np.uint8(np.clip(packed*255, 0, 255)), 'RGB')
        if normals:
            with Image.open(SOURCE/'Textures'/normals) as source:
                detail = source.convert('RGB').resize((inner, inner), Image.Resampling.LANCZOS)
        else:
            detail = Image.new('RGB', (inner, inner), (128, 128, 255))
        for atlas, image in [(base, color), (normal, detail), (orm, packed)]:
            array = np.asarray(image)
            padded = np.pad(array, ((4, 4), (4, 4), (0, 0)), mode='edge')
            # PIL origin is top-left; Blender UV origin is bottom-left.
            atlas.paste(Image.fromarray(padded), (x, 1024-y-tile))
        for relative in [diffuse, normals, specular]:
            if relative:
                hashes[relative] = sha(SOURCE/'Textures'/relative)
    maps = ASSET/'cooked_maps'; maps.mkdir(exist_ok=True)
    base.save(maps/'horseman_base.png', optimize=False)
    normal.resize((1024, 512), Image.Resampling.LANCZOS).save(maps/'horseman_normal.png', optimize=False)
    orm.resize((1024, 512), Image.Resampling.LANCZOS).save(maps/'horseman_orm.png', optimize=False)
    (ASSET/'map-source-hashes.json').write_text(json.dumps(hashes, indent=2)+'\n')


def inspect_glb(path):
    raw = path.read_bytes()
    assert struct.unpack_from('<III', raw) == (0x46546C67, 2, len(raw))
    length = struct.unpack_from('<I', raw, 12)[0]
    gltf = json.loads(raw[20:20+length])
    binary = raw[28+length:]
    assert not gltf.get('skins') and not gltf.get('animations') and not gltf.get('extensionsRequired')
    primitives = [p for m in gltf['meshes'] for p in m['primitives']]
    assert len(primitives) == 1 and 'material' in primitives[0]
    assert 'COLOR_0' not in primitives[0]['attributes']
    triangles = sum(gltf['accessors'][p['indices']]['count']//3 for p in primitives)
    images = []
    for image in gltf['images']:
        assert 'uri' not in image
        view = gltf['bufferViews'][image['bufferView']]
        start = view.get('byteOffset', 0)
        data = binary[start:start+view['byteLength']]
        assert data[:8] == b'\x89PNG\r\n\x1a\n'
        width, height = struct.unpack_from('>II', data, 16)
        images.append(dict(sha256=hashlib.sha256(data).hexdigest(), width=width, height=height, encoded_bytes=len(data)))
    assert len(images) == 3
    return dict(file=path.name, bytes=path.stat().st_size, sha256=sha(path), triangles=triangles,
                primitives=len(primitives), images=images)


def blender_cook(preview=False):
    import bpy
    from mathutils import Vector
    bpy.ops.wm.read_factory_settings(use_empty=True)
    imported = []
    for name in FBXS:
        previous = set(bpy.data.objects)
        bpy.ops.import_scene.fbx(filepath=str(SOURCE/'Export/FBX'/name))
        objects = [o for o in bpy.data.objects if o not in previous]
        imported += [o for o in objects if o.type == 'MESH' and
                     (name == FBXS[0] or o.name in RIDER_PARTS)]
    assert RIDER_PARTS <= {o.name for o in imported}
    scene = bpy.context.scene
    scene.frame_set(1); bpy.context.view_layer.update()
    graph = bpy.context.evaluated_depsgraph_get()
    material = bpy.data.materials.new('Valwood Horseman Shared Atlas')
    material.use_nodes = True
    nodes, links = material.node_tree.nodes, material.node_tree.links
    p = nodes.get('Principled BSDF')
    textures = {}
    for key, filename, color_space in [('base', 'horseman_base.png', 'sRGB'),
                                     ('normal', 'horseman_normal.png', 'Non-Color'),
                                     ('orm', 'horseman_orm.png', 'Non-Color')]:
        image = bpy.data.images.load(str(ASSET/'cooked_maps'/filename), check_existing=False)
        image.colorspace_settings.name = color_space; image.pack()
        node = nodes.new('ShaderNodeTexImage'); node.image = image; textures[key] = node
    links.new(textures['base'].outputs['Color'], p.inputs['Base Color'])
    normal = nodes.new('ShaderNodeNormalMap'); normal.inputs['Strength'].default_value = 1
    links.new(textures['normal'].outputs['Color'], normal.inputs['Color']); links.new(normal.outputs['Normal'], p.inputs['Normal'])
    channels = nodes.new('ShaderNodeSeparateColor'); channels.mode = 'RGB'
    links.new(textures['orm'].outputs['Color'], channels.inputs['Color'])
    links.new(channels.outputs['Green'], p.inputs['Roughness']); links.new(channels.outputs['Blue'], p.inputs['Metallic'])
    baked, parts = [], []
    for original in imported:
        mesh = bpy.data.meshes.new_from_object(original.evaluated_get(graph), depsgraph=graph)
        mesh.transform(original.matrix_world)
        obj = bpy.data.objects.new('Horseman_'+original.name, mesh); scene.collection.objects.link(obj)
        uv = mesh.uv_layers.active
        assert uv
        for poly in mesh.polygons:
            source_material = mesh.materials[poly.material_index].name
            assert source_material in MATERIALS, source_material
            x, y, tile = RECIPES[MATERIALS[source_material]][4]
            for index in poly.loop_indices:
                old = uv.data[index].uv.copy()
                assert -.002 <= old.x <= 1.002 and -.002 <= old.y <= 1.002, (original.name, tuple(old))
                uv.data[index].uv = ((x+4+min(1, max(0, old.x))*(tile-8))/2048,
                                    (y+4+min(1, max(0, old.y))*(tile-8))/1024)
            poly.material_index = 0
        for attribute in list(mesh.color_attributes):mesh.color_attributes.remove(attribute)
        mesh.materials.clear(); mesh.materials.append(material)
        parts.append(dict(name=original.name, triangles=sum(len(p.vertices)-2 for p in mesh.polygons)))
        baked.append(obj)
    points = [v.co.copy() for o in baked for v in o.data.vertices]
    low = Vector(tuple(min(p[i] for p in points) for i in range(3)))
    high = Vector(tuple(max(p[i] for p in points) for i in range(3)))
    scale = 3.12/(high.z-low.z)
    # Fixed horizontal origin is the source rig centre, preserving alignment.
    for obj in baked:
        for vertex in obj.data.vertices:
            vertex.co = Vector((vertex.co.x, vertex.co.y, vertex.co.z-low.z))*scale
        obj.data.update()
    bpy.ops.object.select_all(action='DESELECT')
    for obj in baked:obj.select_set(True)
    bpy.context.view_layer.objects.active = baked[0]; bpy.ops.object.join()
    body = baked[0]; body.name = 'Village_Horseman_Near'; body.data.name = body.name
    # The imported source rigs and alternative outfits stay out of exports.
    for obj in list(scene.objects):
        if obj != body:bpy.data.objects.remove(obj, do_unlink=True)
    source_triangles = sum(len(p.vertices)-2 for p in body.data.polygons)
    records = []
    OUT.mkdir(parents=True, exist_ok=True)
    for lod, cap in [('near', 55000), ('mid', 16000)]:
        obj = body.copy(); obj.data = body.data.copy(); scene.collection.objects.link(obj)
        obj.name = 'Village_Horseman_'+lod; obj.data.name = obj.name
        bpy.context.view_layer.objects.active = obj
        obj.select_set(True); body.select_set(False)
        if source_triangles > cap:
            modifier = obj.modifiers.new('Bounded '+lod+' detail', 'DECIMATE')
            modifier.ratio = cap/source_triangles
            modifier.use_collapse_triangulate = True
            bpy.ops.object.modifier_apply(modifier=modifier.name)
        triangles = sum(len(p.vertices)-2 for p in obj.data.polygons)
        assert triangles <= cap+10, (lod, triangles)
        path = OUT/f'village_horseman_{lod}.glb'
        pending = OUT/f'village_horseman_{lod}.pending.glb'
        bpy.ops.export_scene.gltf(filepath=str(pending), export_format='GLB', use_selection=True,
            export_yup=True, export_animations=False, export_skins=False,
            export_image_format='AUTO', export_cameras=False, export_lights=False)
        record = inspect_glb(pending)
        record['file'] = path.name
        record['bounds_blender'] = [[min(v.co[i]for v in obj.data.vertices)for i in range(3)],
                                    [max(v.co[i]for v in obj.data.vertices)for i in range(3)]]
        record['height_m'] = record['bounds_blender'][1][2]-record['bounds_blender'][0][2]
        assert 3.10 <= record['height_m'] <= 3.14 and abs(record['bounds_blender'][0][2]) < .002
        records.append(record)
        bpy.data.objects.remove(obj, do_unlink=True)
    assert {i['sha256']for i in records[0]['images']} == {i['sha256']for i in records[1]['images']}
    for lod in ['near', 'mid']:
        os.replace(OUT/f'village_horseman_{lod}.pending.glb', OUT/f'village_horseman_{lod}.glb')
    manifest = dict(schema_version=1, source_archive=str(ARCHIVE), source_archive_sha256=sha(ARCHIVE),
        provenance='User-supplied Adam Knight Horseman commercial asset; no bundled license text found in the inspected LIGHT source pack. Original sources retained; no public redistribution license inferred.',
        source_fbxs=[dict(file=str((SOURCE/'Export/FBX'/name).relative_to(ROOT)),sha256=sha(SOURCE/'Export/FBX'/name))for name in FBXS],
        source_frame=1, runtime_animation=False, original_embedded_timelines_retained=True,
        forward='Blender -Y / glTF +Z', normalization_height_m=3.12,
        source_triangles=source_triangles, retained_components=parts,
        excluded='Alternative helmet, duplicate shields, alternate hand weapons, spare long weapons and hidden hair under helmet; no selected armour dropped.',
        material_method='Source diffuse and tangent normals atlased; conservative authored legacy-specular to roughness/metallic conversion. Shared 2048x1024 base +1024x512 normal/ORM; no baked lighting.',
        unique_texture_count=3, estimated_rgba_mips_mib=16.0, assets=records)
    (ASSET/'horseman_manifest.json').write_text(json.dumps(manifest, indent=2)+'\n')
    (OUT/'village_horseman_manifest.json').write_text(json.dumps(manifest, indent=2)+'\n')
    if preview:
        # Preview the exact encoded runtime near asset, including any GLB
        # material conversion and the final 55K-triangle topology.
        bpy.data.objects.remove(body, do_unlink=True)
        bpy.ops.import_scene.gltf(filepath=str(OUT/'village_horseman_near.glb'))
        bpy.ops.mesh.primitive_plane_add(size=200)
        ground = bpy.context.object; ground.name = 'Preview_ground'
        mat = bpy.data.materials.new('Preview floor'); mat.diffuse_color = (.11, .12, .10, 1)
        ground.data.materials.append(mat)
        world = bpy.data.worlds.new('Preview sky'); world.use_nodes = True
        world.node_tree.nodes['Background'].inputs[0].default_value = (.22, .29, .36, 1)
        world.node_tree.nodes['Background'].inputs[1].default_value = .8; scene.world = world
        for position, energy, size in [((1, -4, 7), 1800, 5), ((-4, 1, 5), 1100, 4)]:
            bpy.ops.object.light_add(type='AREA', location=position)
            light = bpy.context.object; light.data.energy = energy; light.data.shape = 'DISK'; light.data.size = size
            light.rotation_euler = (Vector((0, 0, 1.5))-light.location).to_track_quat('-Z', 'Y').to_euler()
        bpy.ops.object.camera_add(location=(4.5, -6, 3.6))
        camera = bpy.context.object; camera.rotation_euler = (Vector((0, -.25, 1.52))-camera.location).to_track_quat('-Z', 'Y').to_euler()
        camera.data.type = 'ORTHO'; camera.data.ortho_scale = 4.4; scene.camera = camera
        scene.render.engine = 'CYCLES'; scene.cycles.device = 'CPU'; scene.cycles.samples = 24
        scene.cycles.use_denoising = False
        scene.render.resolution_x = 900; scene.render.resolution_y = 900; scene.render.resolution_percentage = 100
        scene.render.filepath = str(ASSET/'horseman-preview.png')
        bpy.ops.render.render(write_still=True)
    print('HORSEMAN IMPORT COMPLETE', json.dumps(dict(assets=[(r['file'],r['triangles'])for r in records], textures_mib=16.0)), flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--preview', action='store_true')
    parser.add_argument('--blender-phase', action='store_true')
    args = parser.parse_args(sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else []) if 'bpy' in sys.modules else parser.parse_args()
    if args.blender_phase:
        blender_cook(args.preview)
    else:
        extract(); prepare_maps()
        command = ['/usr/bin/blender', '-b', '-t', '2', '--factory-startup', '--python-exit-code', '1', '--python', str(Path(__file__).resolve()), '--', '--blender-phase']
        if args.preview:command.append('--preview')
        subprocess.run(command, check=True)


if __name__ == '__main__':main()
