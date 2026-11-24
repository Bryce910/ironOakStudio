"""Cook free CC0 System G6 village people into bounded static poses.

Called by the village character builder inside Blender. Original online source
archives and rigged role sources remain under assets/characters/incoming/.
Idle is the source clip. Walking is an explicitly adapted, reduced Run clip
with level feet and a planted sole in every sampled pose, not a claimed source
Walk animation. Only this offline cooker evaluates a skeleton.
"""
from pathlib import Path
import hashlib
import json
import math
import struct

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'assets/characters/incoming/system_g6_modular/role_sources'
ROLE_SPECS = {
    'trader': ('male', [('idle', 4)]),
    'traveler': ('male', [('walk', 8), ('idle', 4)]),
    'traderwoman': ('female', [('idle', 4)]),
    'travelerwoman': ('female', [('walk', 8), ('idle', 4)]),
    'archer': ('male', [('idle', 4)]),
}
SOURCE_PAGE = 'https://opengameart.org/content/modular-rpg-characters'


def _sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def _atlas(bpy, originals, role):
    """One 2048x1024 RGBA atlas and one body draw, including masked hair."""
    import numpy as np
    images = {}
    material_image = {}
    for obj in originals:
        for material in obj.data.materials:
            if material is None:
                raise ValueError(f'{role}: materialless source body')
            p = next((n for n in material.node_tree.nodes if n.type == 'BSDF_PRINCIPLED'), None)
            if p is None or not p.inputs['Base Color'].links:
                raise ValueError(f'{role}: missing diffuse material {material.name}')
            textures = [n for n in material.node_tree.nodes if n.type == 'TEX_IMAGE' and n.image]
            # Blender's importer inserts the source's constant white vertex
            # color multiplier. All components are verified below, then the
            # redundant attribute is removed: runtime ignores COLOR_0.
            if len(textures) != 1:
                raise ValueError(f'{role}: expected one diffuse image in {material.name}')
            image = textures[0].image
            images[image.name] = image
            material_image[material.name] = image.name
    ordered = sorted(images.values(), key=lambda i: (-i.size[0]*i.size[1], i.name))
    assert len(ordered) <= 5, (role, 'Atlas budget', len(ordered))
    assert max(max(i.size) for i in ordered) <= 1024
    pixels = np.zeros((1024, 2048, 4), dtype=np.float32)
    pixels[:, :, 3] = 1.0
    layouts = [(0, 0, 1024), (1024, 0, 512), (1536, 0, 512),
               (1024, 512, 512), (1536, 512, 512)]
    rects = {}
    for image, (x, y, tile) in zip(ordered, layouts):
        # Two texel extruded gutters prevent adjacent UV islands from bleeding.
        gutter = 2
        width = tile - gutter * 2
        copy = image.copy()
        copy.scale(width, width)
        data = np.empty(width * width * 4, dtype=np.float32)
        copy.pixels.foreach_get(data)
        data = data.reshape((width, width, 4))
        pixels[y:y+tile, x:x+tile] = np.pad(data, ((gutter, gutter), (gutter, gutter), (0, 0)), mode='edge')
        rects[image.name] = ((x+gutter)/2048, (y+gutter)/1024, width/2048, width/1024)
        bpy.data.images.remove(copy)
    atlas = bpy.data.images.new('Valwood_' + role + '_diffuse_atlas', width=2048, height=1024, alpha=True)
    atlas.colorspace_settings.name = 'sRGB'
    atlas.pixels.foreach_set(pixels.ravel())
    atlas.file_format = 'PNG'
    atlas.pack()
    material = bpy.data.materials.new('Valwood_' + role + '_cloth_skin_hair')
    material.use_nodes = True
    nodes = material.node_tree.nodes
    p = nodes.get('Principled BSDF')
    p.inputs['Roughness'].default_value = .72
    texture = nodes.new('ShaderNodeTexImage')
    texture.image = atlas
    texture.interpolation = 'Linear'
    texture.extension = 'EXTEND'
    material.node_tree.links.new(texture.outputs['Color'], p.inputs['Base Color'])
    material.node_tree.links.new(texture.outputs['Alpha'], p.inputs['Alpha'])
    material.blend_method = 'CLIP'
    material.alpha_threshold = .5
    material.use_backface_culling = False
    return material, material_image, rects


def _gear(bpy, role, hands):
    """Small authored bow/quiver; separate body/gear names for verification."""
    from mathutils import Vector
    if role != 'archer':
        return []
    objects = []
    material = bpy.data.materials.new('Village archer bow wood and dark fittings')
    material.use_nodes = True
    p = material.node_tree.nodes.get('Principled BSDF')
    p.inputs['Base Color'].default_value = (.16, .065, .022, 1)
    p.inputs['Roughness'].default_value = .72
    def rod(name, a, b, radius, vertices=8):
        a, b = Vector(a), Vector(b)
        delta = b-a
        bpy.ops.mesh.primitive_cylinder_add(vertices=vertices, radius=radius, depth=delta.length, location=(a+b)*.5)
        o = bpy.context.object
        o.name = 'Gear_archer_' + name
        o.rotation_euler = delta.to_track_quat('Z', 'Y').to_euler()
        o.data.materials.append(material)
        objects.append(o)
    hand = Vector(hands['LeftHand'])
    points = [hand + Vector((0, -.04-.14*math.sin(i*math.pi/8), (i/8-.5)*.92)) for i in range(9)]
    for i in range(8):
        rod('bow_limb', points[i], points[i+1], .016)
    rod('taut_bowstring', points[0], points[-1], .003, 4)
    rod('back_quiver', (.17, .13, .94), (.17, .13, 1.36), .065, 10)
    for i in range(4):
        rod('arrow', (.13+i*.023, .13, 1.01), (.13+i*.023, .13, 1.54), .0045, 5)
    bpy.ops.object.select_all(action='DESELECT')
    for o in objects:
        o.select_set(True)
    bpy.context.view_layer.objects.active = objects[0]
    bpy.ops.object.join()
    objects[0].name = 'Gear_archer_bow_quiver_arrows'
    objects[0].data.name = objects[0].name
    return [objects[0]]


def cook(output: Path, selected=None):
    """Write canonical static pose GLBs and return only newly cooked records.

    selected is an iterable of role names (default all five civilian/ranger
    roles). Caller merges the records with separately cooked knight records.
    """
    import bpy
    from mathutils import Matrix, Vector
    output = Path(output)
    output.mkdir(parents=True, exist_ok=True)
    selected = set(ROLE_SPECS if selected is None else selected)
    assert selected <= set(ROLE_SPECS), selected
    records = []
    for role, (sex, modes) in ROLE_SPECS.items():
        if role not in selected:
            continue
        source = SOURCE / (role + '.glb')
        bpy.ops.wm.read_factory_settings(use_empty=True)
        bpy.ops.import_scene.gltf(filepath=str(source))
        scene = bpy.context.scene
        scene.render.fps = 24
        arms = [o for o in scene.objects if o.type == 'ARMATURE']
        assert len(arms) == 1
        arm = arms[0]
        helpers = {b.custom_shape for b in arm.pose.bones if b.custom_shape}
        originals = [o for o in scene.objects if o.type == 'MESH' and o not in helpers and len(o.data.materials) and not o.hide_render]
        assert len(originals) == 5, (role, [o.name for o in originals])
        for obj in originals:
            for attribute in obj.data.color_attributes:
                for value in attribute.data:
                    assert all(abs(c-1.0) < .001 for c in value.color), (role, 'Nonwhite unsupported vertex color', tuple(value.color))
        idle = next(a for a in bpy.data.actions if 'idle' in a.name)
        run = next(a for a in bpy.data.actions if '_run' in a.name)
        for track in arm.animation_data.nla_tracks:
            track.mute = True
        def evaluate(action, fraction):
            arm.animation_data.action = action
            start, end = action.frame_range
            value = start + fraction*(end-start)
            scene.frame_set(int(value), subframe=value-int(value))
            bpy.context.view_layer.update()
        def transforms():
            return {b.name: b.matrix_basis.copy() for b in arm.pose.bones}
        evaluate(idle, 0)
        idle_reference = transforms()
        reference_feet = {n: arm.pose.bones[n].matrix.to_quaternion().copy() for n in ['foot_L', 'foot_R']}
        graph = bpy.context.evaluated_depsgraph_get()
        all_points = [o.matrix_world @ v.co for o in originals for v in o.evaluated_get(graph).data.vertices]
        reference_low = min(v.z for v in all_points)
        reference_high = max(v.z for v in all_points)
        fixed_scale = 1.82 / (reference_high-reference_low)
        # Ground probes are actual weighted boot sole/toe vertices, not a skirt.
        foot_indices = {}
        for o in originals:
            groups = {g.index:g.name for g in o.vertex_groups}
            sides = {s:[] for s in ['L', 'R']}
            for v in o.data.vertices:
                for side in sides:
                    if sum(g.weight for g in v.groups if groups.get(g.group) in ['foot_'+side, 'toes_'+side, 'heel_'+side]) > .4:
                        sides[side].append(v.index)
            if any(sides.values()):
                foot_indices[o.name] = sides
        assert all(any(p[s] for p in foot_indices.values()) for s in ['L', 'R']), role
        # Evaluated Blender meshes may reuse UV custom-data storage between
        # frames. Always remap from this immutable source snapshot, never from
        # a previously evaluated UV value.
        source_uvs = {o.name:[tuple(v.uv) for v in o.data.uv_layers.active.data] for o in originals}
        source_materials = {o.name:[m.name for m in o.data.materials] for o in originals}
        source_material_indices = {o.name:[p.material_index for p in o.data.polygons] for o in originals}
        atlas_material, material_images, atlas_rects = _atlas(bpy, originals, role)
        atlas_hash = None
        uv_hash = None
        for mode, count in modes:
            for frame in range(count):
                fraction = frame/count
                if mode == 'idle':
                    evaluate(idle, fraction)
                    method = 'Original System G6 Idle clip; fixed reference scale and sole grounding'
                else:
                    evaluate(run, fraction)
                    running = transforms()
                    arm.animation_data.action = None
                    # The source has Run, not Walk. Reduce its excursion toward
                    # relaxed Idle while preserving authored opposing limbs.
                    for bone in arm.pose.bones:
                        a = idle_reference[bone.name]
                        b = running[bone.name]
                        al, aq, asc = a.decompose()
                        bl, bq, bsc = b.decompose()
                        leg_blend = .54 if role=='travelerwoman' else .42
                        blend = leg_blend if bone.name.startswith(('leg_', 'foot_', 'toes_', 'heel_')) else .34
                        bone.matrix_basis = Matrix.LocRotScale(al.lerp(bl, blend), aq.slerp(bq, blend), asc.lerp(bsc, blend))
                    bpy.context.view_layer.update()
                    # Restore each foot's neutral world orientation; unlike the
                    # original running clip, the support sole stays level.
                    for name, rotation in reference_feet.items():
                        bone = arm.pose.bones[name]
                        bone.matrix = Matrix.LocRotScale(bone.matrix.translation, rotation, Vector((1, 1, 1)))
                    bpy.context.view_layer.update()
                    method = f'Adapted source Run: {leg_blend:.0%} leg / 34% upper-body excursion, neutral foot orientation, planted sole; played as walking'
                graph = bpy.context.evaluated_depsgraph_get()
                # Use the pelvis as the horizontal origin. Frame bounds must
                # never recenter people as arms and legs swing.
                hips = arm.matrix_world @ arm.pose.bones['pelvis'].head
                baked = []
                minimum_feet = {'L':float('inf'), 'R':float('inf')}
                for original in originals:
                    mesh = bpy.data.meshes.new_from_object(original.evaluated_get(graph), depsgraph=graph)
                    mesh.transform(original.matrix_world)
                    obj = bpy.data.objects.new('Body_' + role + '_' + original.name, mesh)
                    bpy.context.collection.objects.link(obj)
                    baked.append(obj)
                    for side, indices in foot_indices.get(original.name, {}).items():
                        for index in indices:
                            minimum_feet[side] = min(minimum_feet[side], mesh.vertices[index].co.z)
                    uv = mesh.uv_layers.active
                    assert len(uv.data) == len(source_uvs[original.name])
                    for polygon_index, polygon in enumerate(mesh.polygons):
                        material_index = source_material_indices[original.name][polygon_index]
                        image_name = material_images[source_materials[original.name][material_index]]
                        ox, oy, sx, sy = atlas_rects[image_name]
                        for index in polygon.loop_indices:
                            old = source_uvs[original.name][index]
                            assert -.002 <= old[0] <= 1.002 and -.002 <= old[1] <= 1.002, (role, 'UV outside atlas', old)
                            uv.data[index].uv = (ox+max(0, min(1, old[0]))*sx, oy+max(0, min(1, old[1]))*sy)
                        polygon.material_index = 0
                    mesh.materials.clear()
                    mesh.materials.append(atlas_material)
                    for attribute in list(mesh.color_attributes):
                        mesh.color_attributes.remove(attribute)
                ground = min(minimum_feet.values())
                assert math.isfinite(ground)
                origin = Vector((hips.x, hips.y, ground))
                hands = {public:list((arm.matrix_world @ arm.pose.bones[bone].head-origin)*fixed_scale)
                         for public, bone in [('LeftHand', 'palm_L'), ('RightHand', 'palm_R')]}
                for obj in baked:
                    for vertex in obj.data.vertices:
                        vertex.co = (vertex.co-origin)*fixed_scale
                    obj.data.update()
                body_low = min(v.co.z for o in baked for v in o.data.vertices)
                body_high = max(v.co.z for o in baked for v in o.data.vertices)
                foot_heights = {s:(v-ground)*fixed_scale for s,v in minimum_feet.items()}
                assert abs(min(foot_heights.values())) < .00001, (role, mode, frame, foot_heights)
                assert body_low >= -.025, (role, mode, frame, body_low)
                bpy.ops.object.select_all(action='DESELECT')
                for obj in baked:
                    obj.select_set(True)
                bpy.context.view_layer.objects.active = baked[0]
                bpy.ops.object.join()
                body = baked[0]
                body.name = f'Body_{role}_{mode}_{frame:02}'
                body.data.name = body.name
                frame_uv_hash = hashlib.sha256(b''.join(struct.pack('<ff', *v.uv) for v in body.data.uv_layers.active.data)).hexdigest()
                if uv_hash is None:
                    uv_hash = frame_uv_hash
                assert frame_uv_hash == uv_hash, (role, 'Changing UV layout between poses')
                gear = _gear(bpy, role, hands)
                exports = [body] + gear
                bpy.ops.object.select_all(action='DESELECT')
                for obj in exports:
                    obj.select_set(True)
                path = output / f'village_character_{role}_{mode}_{frame:02}.glb'
                bpy.ops.export_scene.gltf(filepath=str(path), export_format='GLB', use_selection=True,
                    export_yup=True, export_animations=False, export_skins=False, export_image_format='AUTO',
                    export_cameras=False, export_lights=False)
                raw = path.read_bytes()
                gltf = json.loads(raw[20:20+struct.unpack_from('<I', raw, 12)[0]])
                assert not gltf.get('skins') and not gltf.get('animations')
                assert len(gltf.get('images', [])) == 1
                image_view = gltf['bufferViews'][gltf['images'][0]['bufferView']]
                bin_start = 20 + struct.unpack_from('<I', raw, 12)[0] + 8
                start = bin_start + image_view.get('byteOffset', 0)
                encoded_hash = hashlib.sha256(raw[start:start+image_view['byteLength']]).hexdigest()
                if atlas_hash is None:
                    atlas_hash = encoded_hash
                assert encoded_hash == atlas_hash, (role, 'Changing atlas bytes across poses')
                assert 'Icosphere' not in json.dumps(gltf.get('meshes', []))
                assert all('COLOR_0' not in primitive['attributes'] for mesh in gltf['meshes'] for primitive in mesh['primitives'])
                triangles = sum(sum(len(p.vertices)-2 for p in o.data.polygons) for o in exports)
                body_triangles = sum(len(p.vertices)-2 for p in body.data.polygons)
                record = dict(file=path.name, role=role, mode=mode, frame=frame,
                    triangles=triangles, body_triangles=body_triangles, body_meshes=1,
                    source_body_meshes=len(originals), body_height_m=body_high-body_low,
                    body_min_y_m=body_low, body_max_y_m=body_high, min_foot_m=min(foot_heights.values()),
                    foot_heights_m=foot_heights, reference_body_height_m=1.82,
                    fixed_reference_scale=fixed_scale, horizontal_origin='rig pelvis; no animated bounds recentering',
                    bytes=path.stat().st_size, sha256=_sha(path), source=str(source.relative_to(ROOT)),
                    source_sha256=_sha(source), source_page=SOURCE_PAGE, creator='System G6 (Qoma)', license='CC0-1.0',
                    method=method, atlas_dimensions=[2048, 1024], atlas_sha256=atlas_hash, uv_sha256=uv_hash,
                    hands=hands, gear_meshes=len(gear), source_vertex_colors='Constant opaque white verified; redundant COLOR_0 removed')
                records.append(record)
                print('FREE VILLAGE POSE', role, mode, frame, triangles, path.stat().st_size, foot_heights, flush=True)
                for obj in exports:
                    mesh = obj.data
                    bpy.data.objects.remove(obj, do_unlink=True)
                    if mesh.users == 0:
                        bpy.data.meshes.remove(mesh)
    return records


if __name__ == '__main__':
    import argparse
    import sys
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', type=Path, default=Path('/tmp/valwood-free-civilian-stage'))
    parser.add_argument('--roles', default=','.join(ROLE_SPECS))
    args = parser.parse_args(sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else [])
    records = cook(args.output, args.roles.split(','))
    (args.output/'civilian_manifest.json').write_text(json.dumps({'assets':records}, indent=2)+'\n')
