"""Cook the free, attributed Danimal Defender into bounded native-engine poses.

Call ``cook(output, selected=None)`` from Blender.  Only ``knight`` is owned by
this module.  The downloaded original remains unchanged; runtime exports use a
fixed body scale, the supplied walking clip, and an explicitly authored idle.
No rendering, networking, external scripts in .blend files, or engine changes.
"""
from pathlib import Path
import hashlib
import json
import math
import struct

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'assets/characters/incoming/danimal_defender/source/Defender.blend'
SOURCE_URL = 'https://opengameart.org/content/animated-defender-aka-brigand'
LICENSE_URL = 'https://creativecommons.org/licenses/by/3.0/'
CREDIT = ('Animated Defender AKA Brigand: Konstantin Maystrenko (concept), '
          'AnthonyMyers (model), Danimal (retopology, rigging, animation).')
BODY_HEIGHT = 1.82
# Frames 1 and 2 contain a bind-pose transition. The repeat measured between
# frames 28 and 61 is one complete, steady 33-frame mocap gait cycle at 25 fps.
WALK_START, WALK_PERIOD, IDLE_FRAME = 28.0, 33.0, 32.0


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def _read_glb(path):
    raw = Path(path).read_bytes()
    if struct.unpack_from('<III', raw) != (0x46546C67, 2, len(raw)):
        raise ValueError(f'Invalid GLB: {path}')
    length, kind = struct.unpack_from('<II', raw, 12)
    assert kind == 0x4E4F534A
    gltf = json.loads(raw[20:20 + length])
    offset = 20 + length
    binary_size, binary_kind = struct.unpack_from('<II', raw, offset)
    assert binary_kind == 0x004E4942
    return gltf, raw[offset + 8:offset + 8 + binary_size]


def _maps(bpy):
    """Rebuild 2.7 diffuse/normal/spec inputs as nonemissive glTF PBR.

    Legacy spec is NOT a metalness map: exposed skin is bright in it.  A
    conservative cool/neutral steel mask excludes warm skin, leather, hair,
    and the poleaxe's brown shaft. No lighting or AO is baked into these maps.
    The source diffuse is retained with its original painted surface detail.
    """
    import numpy as np

    materials = []
    recipes = [
        ('Valwood Defender Steel Leather Skin', 'Done3.png',
         'Done2_NMoverlay2.png', 'Done3Spec.png'),
        ('Valwood Defender Poleaxe', 'DoneWeps.png',
         'DoneWeps_NM2.png', 'DoneWeps_Spec.png'),
    ]
    for number, (name, color_name, normal_name, spec_name) in enumerate(recipes):
        map_root = SOURCE.parents[1] / 'source_maps'
        color, normal, spec = [bpy.data.images.load(str(map_root / n), check_existing=False)
                               for n in (color_name, normal_name, spec_name)]
        # Colorspace changes can reload the original packed/file pixels, so
        # select them before scaling; then enforce the exported image cap.
        color.colorspace_settings.name = 'sRGB'
        normal.colorspace_settings.name = 'Non-Color'
        spec.colorspace_settings.name = 'Non-Color'
        for image in (color, normal, spec):
            if max(image.size) > 1024:
                ratio = 1024.0 / max(image.size)
                image.scale(round(image.size[0] * ratio), round(image.size[1] * ratio))
            assert max(image.size) <= 1024
        width, height = normal.size[:]
        if color.size[:] != normal.size[:]:
            color.scale(width, height)
        if spec.size[:] != normal.size[:]:
            spec.scale(width, height)
        diffuse = np.array(color.pixels[:], dtype=np.float32).reshape(height, width, 4)
        legacy_spec = np.array(spec.pixels[:], dtype=np.float32).reshape(height, width, 4)
        rgb = diffuse[:, :, :3]
        luminance = rgb.mean(axis=2)
        intensity = legacy_spec[:, :, :3].mean(axis=2)
        r, g, b = rgb[:, :, 0], rgb[:, :, 1], rgb[:, :, 2]
        # Skin and leather are warm in the original palette.  This is a
        # conservative artistic conversion, not a claim of captured PBR data.
        neutral = np.clip((np.minimum(g, b) - r + 0.035) / 0.055, 0.0, 1.0)
        value_gate = np.clip((luminance - 0.12) / 0.20, 0.0, 1.0)
        spec_gate = np.clip((intensity - 0.10) / 0.30, 0.0, 1.0)
        metal = 0.82 * neutral * value_gate * spec_gate
        rough = np.clip(0.77 - 0.29 * intensity - 0.13 * metal, 0.34, 0.80)
        warm = (r > g * 1.08) & (r > b * 1.14)
        metal[warm] = 0.0
        rough[warm] = np.maximum(rough[warm], 0.60)
        orm = np.ones((height, width, 4), dtype=np.float32)
        orm[:, :, 1], orm[:, :, 2] = rough, metal
        packed = bpy.data.images.new(f'Defender_{number}_RoughnessMetalness',
                                     width=width, height=height, alpha=True)
        packed.colorspace_settings.name = 'Non-Color'
        packed.pixels.foreach_set(orm.ravel())
        packed.file_format = 'PNG'
        packed.pack()
        # Generated image copies keep the scaled pixel buffers rather than
        # asking Blender to reload legacy relative paths while packing.
        clean_images = []
        for suffix, original in [('BaseColor', color), ('Normal', normal)]:
            clean = bpy.data.images.new(f'Defender_{number}_{suffix}', width=width, height=height, alpha=True)
            clean.colorspace_settings.name = 'sRGB' if suffix == 'BaseColor' else 'Non-Color'
            clean.pixels.foreach_set(np.array(original.pixels[:], dtype=np.float32))
            clean.file_format = 'PNG'
            clean.pack()
            clean_images.append(clean)
        color, normal = clean_images
        material = bpy.data.materials.new(name)
        material.use_nodes = True
        material.diffuse_color = (0.55, 0.50, 0.44, 1.0)
        nodes, links = material.node_tree.nodes, material.node_tree.links
        principled = nodes.get('Principled BSDF')
        principled.inputs['Roughness'].default_value = 0.65
        principled.inputs['Metallic'].default_value = 0.0
        principled.inputs['Emission Color'].default_value = (0, 0, 0, 1)
        c = nodes.new('ShaderNodeTexImage'); c.image = color
        links.new(c.outputs['Color'], principled.inputs['Base Color'])
        n = nodes.new('ShaderNodeTexImage'); n.image = normal
        normal_node = nodes.new('ShaderNodeNormalMap')
        # The native material path reads tangent-space maps at full strength.
        # Export that same strength so source previews and game metadata agree.
        normal_node.inputs['Strength'].default_value = 1.0
        links.new(n.outputs['Color'], normal_node.inputs['Color'])
        links.new(normal_node.outputs['Normal'], principled.inputs['Normal'])
        orm_node = nodes.new('ShaderNodeTexImage'); orm_node.image = packed
        channels = nodes.new('ShaderNodeSeparateColor'); channels.mode = 'RGB'
        links.new(orm_node.outputs['Color'], channels.inputs['Color'])
        links.new(channels.outputs['Green'], principled.inputs['Roughness'])
        links.new(channels.outputs['Blue'], principled.inputs['Metallic'])
        materials.append(material)
    return materials


def _part(bpy, evaluated_mesh, material_slot, name, material):
    """Split by source face slot without losing normals, UV seams or winding."""
    import bmesh
    mesh = evaluated_mesh.copy()
    bm = bmesh.new(); bm.from_mesh(mesh)
    unwanted = [face for face in bm.faces if face.material_index != material_slot]
    bmesh.ops.delete(bm, geom=unwanted, context='FACES')
    loose = [v for v in bm.verts if not v.link_faces]
    if loose:
        bmesh.ops.delete(bm, geom=loose, context='VERTS')
    for face in bm.faces:
        face.material_index = 0
    bm.to_mesh(mesh); bm.free()
    mesh.materials.clear(); mesh.materials.append(material)
    mesh.name = name
    mesh.validate(clean_customdata=False)
    mesh.update()
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.collection.objects.link(obj)
    return obj


def cook(output: Path, selected=None):
    """Return metadata for 8 walk + 4 idle canonical knight GLBs.

    ``selected`` is a role iterable (or a comma-separated role string).  Other
    roles are left to the main cooker.  ``output`` must be a staging directory
    when preparing a new set so a failed cook cannot partially replace actors.
    """
    if isinstance(selected, str):
        selected = selected.split(',')
    if selected is not None and 'knight' not in set(selected):
        return []
    import bpy
    from mathutils import Matrix, Vector
    import sys
    sys.path.insert(0, str(ROOT / 'tools'))
    from bake_gltf_uv import bake_uv_transforms

    output = Path(output); output.mkdir(parents=True, exist_ok=True)
    assert SOURCE.is_file(), SOURCE
    # Disable source auto-execution even when this module is called from an
    # existing Blender session.  No embedded Python text is executed.
    bpy.context.preferences.filepaths.use_scripts_auto_execute = False
    bpy.ops.wm.open_mainfile(filepath=str(SOURCE), load_ui=False, use_scripts=False)
    scene = bpy.context.scene
    scene.render.fps = 25
    original = bpy.data.objects['Defender']
    arm = bpy.data.objects['Defender_Ske']
    assert original.type == 'MESH' and arm.type == 'ARMATURE'
    action = bpy.data.actions.get('Walk-normal.001')
    assert action and len(action.fcurves) > 80, 'Missing genuine source walk'
    arm.animation_data_create()
    for track in arm.animation_data.nla_tracks:
        track.mute = True
    arm.animation_data.action = action
    assert [m.name for m in original.data.materials] == ['Defender_Weapon', 'Defender']
    materials = _maps(bpy)

    def sample(frame):
        scene.frame_set(math.floor(frame), subframe=frame % 1.0)
        bpy.context.view_layer.update()
        graph = bpy.context.evaluated_depsgraph_get()
        evaluated = original.evaluated_get(graph)
        mesh = bpy.data.meshes.new_from_object(evaluated, depsgraph=graph)
        # The original has a large unusual rotation/scale; use the evaluated
        # world transform once, not its local dimensions or per-frame bbox.
        mesh.transform(evaluated.matrix_world)
        evaluated_arm = arm.evaluated_get(graph)
        hip = evaluated_arm.matrix_world @ evaluated_arm.pose.bones['Hips'].head
        grip = evaluated_arm.matrix_world @ evaluated_arm.pose.bones['RightFingerBase'].head
        return mesh, hip, grip

    # Fixed normalization is measured on a stable double-contact pose.  Only
    # root translation and sole-height correction vary in subsequent poses.
    reference_mesh, reference_hip, reference_grip = sample(IDLE_FRAME)
    body_indices = {i for p in reference_mesh.polygons if p.material_index == 1
                    for i in p.vertices}
    reference_points = [reference_mesh.vertices[i].co.copy() for i in body_indices]
    low = min(v.z for v in reference_points); high = max(v.z for v in reference_points)
    fixed_scale = BODY_HEIGHT / (high - low)
    assert 0.5 < fixed_scale < 1.5
    # Retain this evaluated reference only while cooking the original rigid
    # poleaxe. The mocap rotates its finger bone through large sweeps; a calm
    # carry follows the grip position with the authored stable orientation.
    # Native mocap faces Blender -Y, which export_yup maps to glTF +Z.
    # The +X source left-hand side is the expected facing of that convention.
    facing = Matrix.Identity(4)
    records = []
    image_hashes = None
    for mode, count in [('walk', 8), ('idle', 4)]:
        for frame in range(count):
            phase = math.tau * frame / count
            source_frame = WALK_START + WALK_PERIOD * frame / count if mode == 'walk' else IDLE_FRAME
            evaluated, hip, grip = sample(source_frame)
            body = _part(bpy, evaluated, 1, 'Body_knight', materials[0])
            gear = _part(bpy, reference_mesh, 0, 'Gear_Poleaxe', materials[1])
            grip_delta = grip - reference_grip
            for vertex in gear.data.vertices:
                vertex.co += grip_delta
            bpy.data.meshes.remove(evaluated)
            objects = [body, gear]
            # Both parts share the same root correction; no re-centering body
            # or gear independently, so the authored grip stays attached.
            root = Vector((hip.x, hip.y, 0.0))
            for obj in objects:
                for vertex in obj.data.vertices:
                    p = facing @ ((vertex.co - root) * fixed_scale)
                    if mode == 'idle':
                        height = p.z if obj is body else (grip.z * fixed_scale)
                        chest = max(0.0, min(1.0, (height - 0.85) / 0.75))
                        p.z += math.sin(phase) * 0.003 * chest
                        p.y += math.sin(phase) * 0.004 * chest
                    vertex.co = p
            floor = min(v.co.z for v in body.data.vertices)
            for obj in objects:
                for vertex in obj.data.vertices:
                    vertex.co.z -= floor
                obj.data.update()
            body_height = max(v.co.z for v in body.data.vertices)
            assert 1.68 < body_height < 1.94, (mode, frame, body_height)
            assert abs(min(v.co.z for v in body.data.vertices)) < 1e-6
            triangles = sum(len(p.vertices) - 2 for o in objects for p in o.data.polygons)
            assert triangles <= 18000, triangles
            bpy.ops.object.select_all(action='DESELECT')
            for obj in objects:
                obj.select_set(True)
            bpy.context.view_layer.objects.active = body
            path = output / f'village_character_knight_{mode}_{frame:02}.glb'
            bpy.ops.export_scene.gltf(
                filepath=str(path), export_format='GLB', use_selection=True,
                export_yup=True, export_animations=False, export_skins=False,
                export_image_format='AUTO', export_cameras=False,
                export_lights=False, export_extras=False)
            bake_uv_transforms(path)
            gltf, binary = _read_glb(path)
            assert not gltf.get('skins') and not gltf.get('animations')
            assert len(gltf['meshes']) == 2 and len(gltf['materials']) == 2
            names = [m['name'] for m in gltf['meshes']]
            assert set(names) == {'Body_knight', 'Gear_Poleaxe'}, names
            for material in gltf['materials']:
                pbr = material['pbrMetallicRoughness']
                assert pbr.get('baseColorTexture') and pbr.get('metallicRoughnessTexture')
                assert material.get('normalTexture')
                assert not material.get('emissiveTexture')
            hashes = []
            for image in gltf['images']:
                view = gltf['bufferViews'][image['bufferView']]
                start = view.get('byteOffset', 0)
                image_data = binary[start:start + view['byteLength']]
                assert image_data[:8] == b'\x89PNG\r\n\x1a\n'
                assert max(struct.unpack_from('>II', image_data, 16)) <= 1024
                hashes.append(hashlib.sha256(image_data).hexdigest())
            assert len(hashes) == 6
            if image_hashes is None:
                image_hashes = sorted(hashes)
            assert image_hashes == sorted(hashes), 'Pose exports must share identical map bytes'
            records.append({
                'file': path.name, 'role': 'knight', 'mode': mode, 'frame': frame,
                'source': str(SOURCE.relative_to(ROOT)), 'source_url': SOURCE_URL,
                'source_sha256': sha(SOURCE), 'license': 'CC BY 3.0',
                'license_url': LICENSE_URL, 'credit': CREDIT,
                'source_action': action.name if mode == 'walk' else None,
                'source_frame': source_frame, 'source_fps': 25,
                'triangles': triangles, 'source_body_meshes': 1,
                'body_meshes': ['Body_knight'], 'gear_meshes': ['Gear_Poleaxe'],
                'body_height_m': body_height, 'bodyheight': body_height,
                'reference_body_height_m': BODY_HEIGHT,
                'fixed_reference_scale': fixed_scale,
                'root_motion_removed': 'Hips X/Y; foot sole grounded after skinning',
                'gear_method': 'authored poleaxe rigid carry from frame32, following RightFingerBase grip translation',
                'forward': 'glTF +Z (Blender -Y)',
                'bytes': path.stat().st_size, 'sha256': sha(path),
                'image_sha256': sorted(hashes), 'image_count': len(hashes),
                'method': ('supplied Walk-normal.001 steady 33-frame cycle; fixed body scale'
                           if mode == 'walk' else
                           'authored gentle breathing on stable supplied walk frame; no supplied idle claim'),
                'material_method': 'original diffuse/normal; conservative authored spec-to-roughness/metal mask; skin/leather nonmetal',
                'modifications': 'static pose bake, root motion removal, grounding, fixed body normalization, legacy PBR material conversion, 1024px image cap',
            })
            for obj in objects:
                mesh = obj.data
                bpy.data.objects.remove(obj, do_unlink=True)
                bpy.data.meshes.remove(mesh)
            print('DEFENDER POSE', path.name, triangles, round(body_height, 4), path.stat().st_size, flush=True)
    bpy.data.meshes.remove(reference_mesh)
    assert len(records) == 12
    (output / 'defender_cook_manifest.json').write_text(json.dumps({
        'source': SOURCE_URL, 'credit': CREDIT, 'license': 'CC BY 3.0',
        'license_url': LICENSE_URL, 'forward': 'glTF +Z (Blender -Y)',
        'reference_body_height_m': BODY_HEIGHT, 'fixed_reference_scale': fixed_scale,
        'walk_source_frames': [WALK_START, WALK_START + WALK_PERIOD],
        'idle_source_frame': IDLE_FRAME, 'assets': records,
        'total_bytes': sum(r['bytes'] for r in records),
        'unique_image_count': len(image_hashes),
    }, indent=2) + '\n')
    return records


if __name__ == '__main__':
    import argparse
    import sys
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', type=Path, default=Path('/tmp/valwood-free-guard-stage'))
    args = parser.parse_args(sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else [])
    cooked = cook(args.output)
    print('DEFENDER COOK COMPLETE', len(cooked), sum(r['bytes'] for r in cooked), flush=True)
