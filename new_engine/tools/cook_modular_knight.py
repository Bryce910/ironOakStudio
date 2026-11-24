"""Assemble exact purchased Unity geometry on the audited animated rig.

Run with Blender --background --python this_file -- [options]. Original assets
are read only. Unity's geometry exporter resolves authored fitting morphs; this
tool emits one ordinary four-weight skinned mesh for the maintained GLB reader.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import sys

import bpy
from mathutils import Matrix, Vector

ENGINE = Path(__file__).resolve().parents[1]
ASSET = ENGINE / "assets/purchased/modular_knights"


def matrix(values):
    return Matrix([values[i:i + 4] for i in range(0, 16, 4)])


def image_node(nodes, path, *, linear=False):
    node = nodes.new("ShaderNodeTexImage")
    node.image = bpy.data.images.load(str(path), check_existing=True)
    node.image.colorspace_settings.name = "Non-Color" if linear else "sRGB"
    return node


def material(path, entry):
    mat = bpy.data.materials.new(entry["name"])
    mat["unity_material_path"] = path
    mat.use_nodes = True
    mat.diffuse_color = entry["base_factorRGBA"]
    mat.use_backface_culling = not entry["double_sided"]
    mat.blend_method = "CLIP" if entry["alpha_mode"] == "MASK" else "OPAQUE"
    mat.alpha_threshold = entry["alpha_cutoff"]
    nodes, links = mat.node_tree.nodes, mat.node_tree.links
    bsdf = nodes.get("Principled BSDF")
    base = image_node(nodes, entry["base"])
    links.new(base.outputs["Color"], bsdf.inputs["Base Color"])
    if entry["alpha_mode"] == "MASK":
        links.new(base.outputs["Alpha"], bsdf.inputs["Alpha"])
    orm = image_node(nodes, entry["orm"], linear=True)
    split = nodes.new("ShaderNodeSeparateRGB")
    links.new(orm.outputs["Color"], split.inputs[0])
    links.new(split.outputs["G"], bsdf.inputs["Roughness"])
    links.new(split.outputs["B"], bsdf.inputs["Metallic"])
    normal_image = image_node(nodes, entry["normal"], linear=True)
    normal = nodes.new("ShaderNodeNormalMap")
    normal.inputs["Strength"].default_value = entry["normal_scale"]
    links.new(normal_image.outputs["Color"], normal.inputs["Color"])
    links.new(normal.outputs["Normal"], bsdf.inputs["Normal"])
    settings = bpy.data.node_groups.get("glTF Material Output")
    if settings is None:
        settings = bpy.data.node_groups.new("glTF Material Output", "ShaderNodeTree")
        settings.interface.new_socket(name="Occlusion", in_out="INPUT", socket_type="NodeSocketFloat")
    output = nodes.new("ShaderNodeGroup")
    output.node_tree = settings
    links.new(split.outputs["R"], output.inputs["Occlusion"])
    return mat


def append_longsword(rig, vertices, normals, uv, faces, face_materials, weights, material_list):
    """Use the owned capture's full-length weapon with its measured socket.

    Its source FBX has no supplied textures. Two explicit plain PBR derivative
    materials distinguish the steel and grip; this does not invent a texture
    from another product or claim a publisher-authored finish.
    """
    source = ENGINE / "assets/purchased/longsword_mocap/source/Objects/Sword/SM_Erantis_NoviceSword_Sword_V2.FBX"
    socket_path = ASSET / "derived/weapon_socket_fit.json"
    socket = json.loads(socket_path.read_text())
    existing = set(bpy.data.objects)
    bpy.ops.import_scene.fbx(filepath=str(source), use_anim=False)
    added = set(bpy.data.objects) - existing
    objects = [o for o in added if o.type == "MESH"]
    if len(objects) != 1:
        raise ValueError("Expected the single owned capture sword mesh")
    sword = objects[0]
    mesh = sword.data
    mesh.calc_loop_triangles()
    mesh.calc_normals_split()
    attach = matrix([x for row in socket["target_hand_local_socket"] for x in row])
    attachment_bone = "valwood_weapon_r" if "valwood_weapon_r" in rig.data.bones else socket["target_bone"]
    hand = rig.data.bones[attachment_bone]
    transform = rig.matrix_world @ hand.matrix_local
    if attachment_bone != "valwood_weapon_r":
        transform = transform @ attach
    transform = transform @ Matrix.Scale(100., 4)
    normal_transform = transform.to_3x3().inverted().transposed()
    source_normal = sword.matrix_world.to_3x3().inverted().transposed()
    world_positions = [sword.matrix_world @ v.co for v in mesh.vertices]
    steel = bpy.data.materials.new("Valwood_LongSword_Steel")
    leather = bpy.data.materials.new("Valwood_LongSword_Leather")
    for mat, base, metal, rough in ((steel, (.24, .27, .31, 1.), .95, .28),
                                     (leather, (.045, .018, .009, 1.), 0., .72)):
        mat.use_nodes = True
        shader = mat.node_tree.nodes.get("Principled BSDF")
        shader.inputs["Base Color"].default_value = base
        shader.inputs["Metallic"].default_value = metal
        shader.inputs["Roughness"].default_value = rough
    material_base = len(material_list)
    material_list.extend((steel, leather))
    first = len(vertices)
    cache = {}
    for tri in mesh.loop_triangles:
        face = []
        centroid = sum((world_positions[i] for i in tri.vertices), Vector()) / 3.
        grip = -.17 < centroid.z < .075 and abs(centroid.y) < .027
        for vi, li in zip(tri.vertices, tri.loops):
            n = (source_normal @ mesh.loops[li].normal).normalized()
            key = (vi, tuple(round(x, 6) for x in n))
            if key not in cache:
                index = len(vertices)
                cache[key] = index
                vertices.append(tuple(transform @ world_positions[vi]))
                normals.append(tuple((normal_transform @ n).normalized()))
                uv.append((0., 0.))
                weights.append([(attachment_bone, 1.)])
            face.append(cache[key])
        if transform.to_3x3().determinant() < 0:
            face.reverse()
        faces.append(face)
        face_materials.append(material_base + int(grip))
    report = dict(source=str(source), source_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),
                  vertices=len(vertices) - first, triangles=len(mesh.loop_triangles),
                  attach_bone=attachment_bone, socket=str(socket_path),
                  materials="Explicit plain steel/leather PBR derivative; source supplied no textures",
                  visual_grip_accepted=False)
    for obj in added:
        bpy.data.objects.remove(obj, do_unlink=True)
    return report


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--geometry", type=Path, default=ASSET / "selected_geometry.json")
    parser.add_argument("--selection", type=Path, default=ASSET / "selection.json")
    parser.add_argument("--materials", type=Path, default=ASSET / "derived/materials-balanced/materials.json")
    parser.add_argument("--rig", type=Path, default=ASSET / "derived/knight_rig_actions.blend")
    parser.add_argument("--output", type=Path, default=ASSET / "derived/modular_knight_hero.raw.glb")
    parser.add_argument("--report", type=Path, default=ASSET / "derived/assembly_report.json")
    parser.add_argument("--sword-socket", type=Path, help="Audited target hand-local rigid sword attachment matrix JSON")
    parser.add_argument("--save-blend", type=Path)
    parser.add_argument("--longsword", action="store_true", help="Append the owned capture sword for motion inspection")
    args = parser.parse_args(sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else [])
    for output in (args.output, args.report, args.save_blend):
        if output and (ASSET / "source").resolve() in output.resolve().parents:
            raise ValueError("Outputs cannot overwrite purchased source assets")
    geometry = json.loads(args.geometry.read_text())
    if geometry.get("coordinate_contract") != "renderer_local_with_authored_morphs":
        raise ValueError("Geometry must declare validated renderer-local BakeMesh coordinates")
    selection = json.loads(args.selection.read_text())
    materials = json.loads(args.materials.read_text())
    selected = {piece["mesh_name"]: piece for piece in selection["pieces"]}
    if len(selected) != len(selection["pieces"]):
        raise ValueError("Duplicate selected geometry names")
    bpy.ops.wm.open_mainfile(filepath=str(args.rig))
    rigs = [o for o in bpy.data.objects if o.type == "ARMATURE"]
    if len(rigs) != 1 or len(bpy.data.objects) != 1:
        raise ValueError("Expected the armature-only retarget derivative")
    rig = rigs[0]
    rig.animation_data.action = None
    for bone in rig.pose.bones:
        bone.matrix_basis = Matrix.Identity(4)
    bpy.context.scene.frame_set(1)
    bpy.context.view_layer.update()
    # Unity is left handed, Y-up; the original FBX imported in Blender faces -Y
    # with anatomical left +X. This reflection also requires triangle winding
    # reversal. The rig audit checks named joints, rather than just bounds.
    unity_to_blender = Matrix(((-1., 0., 0., 0.), (0., 0., -1., 0.),
                               (0., 1., 0., 0.), (0., 0., 0., 1.)))
    alignment = []
    for transform in geometry["prefab_transforms"]:
        name = transform["name"]
        if transform["is_bone"] and name in rig.data.bones and not name.endswith("_end"):
            a = unity_to_blender @ matrix(transform["local_to_prefab_root"]).translation
            b = (rig.matrix_world @ rig.data.bones[name].matrix_local).translation
            alignment.append({"bone": name, "error_m": (a - b).length})
    if not alignment or max(x["error_m"] for x in alignment) > .025:
        raise ValueError("Unity and Blender named rest joints do not align within 2.5cm")
    vertices, normals, uv, faces, face_materials, weights = [], [], [], [], [], []
    material_slots, material_list, piece_report = {}, [], []
    sword_socket = json.loads(args.sword_socket.read_text()) if args.sword_socket else None
    used = set()
    for piece in geometry["pieces"]:
        name = piece["mesh_name"]
        if name not in selected:
            raise ValueError("Exported geometry outside selected outfit: " + name)
        used.add(name)
        config = selected[name]
        is_weapon = config.get("role") == "right_hand_weapon"
        # Keep the publisher sword at its authored sheathed location until an
        # explicit captured-motion socket is supplied. This is never guessed
        # from a bounding-box midpoint.
        transform = unity_to_blender @ matrix(piece["local_to_prefab_root"])
        if is_weapon and sword_socket:
            hand = rig.data.bones[sword_socket["bone"]]
            transform = rig.matrix_world @ hand.matrix_local @ matrix(sword_socket["mesh_to_bone"])
        normal_transform = transform.to_3x3().inverted().transposed()
        winding = transform.to_3x3().determinant() < 0
        first = len(vertices)
        part_vertices = []
        for i in range(piece["vertex_count"]):
            position = transform @ Vector(piece["positions"][3 * i:3 * i + 3])
            normal = normal_transform @ Vector(piece["normals"][3 * i:3 * i + 3])
            if not all(math.isfinite(x) for x in position) or normal.length < 1.e-8:
                raise ValueError("Invalid vertex or normal: " + name)
            vertices.append(tuple(position))
            part_vertices.append(position)
            normals.append(tuple(normal.normalized()))
            uv.append(piece["uv"][2 * i:2 * i + 2])
            if is_weapon and sword_socket:
                vertex_weights = [(sword_socket["bone"], 1.)]
            else:
                vertex_weights = []
                for j in range(4):
                    weight = piece["bone_weights"][4 * i + j]
                    if weight > 0:
                        bone_name = piece["bones"][piece["bone_indices"][4 * i + j]]["name"]
                        if bone_name not in rig.data.bones:
                            raise ValueError("Missing target bone " + bone_name)
                        vertex_weights.append((bone_name, weight))
            if abs(sum(w for _, w in vertex_weights) - 1.) > 1.e-5:
                raise ValueError("Non-normalized skin weights")
            weights.append(vertex_weights)
        paths = config["material_paths"]
        for submesh in piece["submeshes"]:
            path = paths[min(submesh["index"], len(paths) - 1)]
            if path not in material_slots:
                if path not in materials:
                    raise ValueError("Material not converted: " + path)
                material_slots[path] = len(material_list)
                material_list.append(material(path, materials[path]))
            indices = submesh["triangles"]
            for i in range(0, len(indices), 3):
                tri = [first + x for x in indices[i:i + 3]]
                if winding:
                    tri.reverse()
                faces.append(tri)
                face_materials.append(material_slots[path])
        bounds = [[min(v[d] for v in part_vertices) for d in range(3)],
                  [max(v[d] for v in part_vertices) for d in range(3)]]
        piece_report.append(dict(name=name, vertices=piece["vertex_count"],
                                 triangles=piece["triangle_count"], bounds= bounds,
                                 authored_fitting_morphs=piece["authored_blendshape_weights"],
                                 role=config.get("role", "body")))
    if used != set(selected):
        raise ValueError("Missing selected pieces: " + str(set(selected) - used))
    weapon_report = append_longsword(rig, vertices, normals, uv, faces, face_materials, weights, material_list) if args.longsword else None
    bounds = [[min(v[d] for v in vertices) for d in range(3)],
              [max(v[d] for v in vertices) for d in range(3)]]
    height = bounds[1][2] - bounds[0][2]
    if not 1.5 < height < 2.2 or any(abs(x) > 3 for row in bounds for x in row):
        raise ValueError("Assembly is not in expected humanoid metre units: " + str(bounds))
    mesh = bpy.data.meshes.new("ModularKnight_Hero_Skinned")
    mesh.from_pydata(vertices, [], faces)
    mesh.update()
    obj = bpy.data.objects.new("ModularKnight_Hero", mesh)
    bpy.context.collection.objects.link(obj)
    for mat in material_list:
        mesh.materials.append(mat)
    uv_layer = mesh.uv_layers.new(name="UVMap")
    for polygon, mat_index in zip(mesh.polygons, face_materials):
        polygon.material_index = mat_index
        polygon.use_smooth = True
        for loop_index in polygon.loop_indices:
            uv_layer.data[loop_index].uv = uv[mesh.loops[loop_index].vertex_index]
    mesh.use_auto_smooth = True
    mesh.normals_split_custom_set_from_vertices(normals)
    groups = {bone.name: obj.vertex_groups.new(name=bone.name) for bone in rig.data.bones}
    for index, vertex_weights in enumerate(weights):
        for name, weight in vertex_weights:
            groups[name].add([index], weight, "REPLACE")
    modifier = obj.modifiers.new("Authored skeletal skin", "ARMATURE")
    modifier.object = rig
    obj.parent = rig
    obj.matrix_world = Matrix.Identity(4)
    bpy.ops.object.select_all(action="DESELECT")
    rig.select_set(True)
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    bpy.context.scene.render.fps = 30
    bpy.context.scene.render.fps_base = 1
    args.output.parent.mkdir(parents=True, exist_ok=True)
    if args.save_blend:
        args.save_blend.parent.mkdir(parents=True, exist_ok=True)
        bpy.ops.wm.save_as_mainfile(filepath=str(args.save_blend))
    bpy.ops.export_scene.gltf(
        filepath=str(args.output), export_format="GLB", use_selection=True,
        export_animations=True, export_animation_mode="ACTIONS", export_frame_range=False,
        export_force_sampling=True, export_optimize_animation_size=True,
        export_anim_slide_to_zero=True,
        export_def_bones=False, export_skins=True, export_all_influences=False,
        export_morph=False, export_yup=True, export_image_format="AUTO",
        export_normals=True, export_tangents=True, export_extras=True,
    )
    report = dict(schema="valwood.modular-knight-assembly.v1", source_modified=False,
                  geometry=str(args.geometry), geometry_sha256=hashlib.sha256(args.geometry.read_bytes()).hexdigest(),
                  selection=str(args.selection), vertex_count=len(vertices), triangle_count=len(faces),
                  materials=len(material_list), rest_bounds_blender= bounds, height_m=height,
                  rest_joint_alignment=alignment, pieces=piece_report,
                  sword=weapon_report if weapon_report else ("captured socket" if sword_socket else "not wielded"),
                  output=str(args.output), output_bytes=args.output.stat().st_size,
                  motion_visual_acceptance=False, gameplay_integrated=False)
    args.report.write_text(json.dumps(report, indent=2) + "\n")
    print("MODULAR_KNIGHT_ASSEMBLED " + json.dumps({k: report[k] for k in
          ("vertex_count", "triangle_count", "materials", "height_m", "output_bytes", "sword")}))


if __name__ == "__main__":
    main()
