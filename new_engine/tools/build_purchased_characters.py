#!/usr/bin/env python3
"""Cook the user's purchased NPC Character Pack into bounded civilian poses.

Preparation (system Python with Pillow/numpy):
  python3 tools/build_purchased_characters.py --prepare-only
Offline skeleton/mesh cook (two CPU threads, no GPU rendering):
  OMP_NUM_THREADS=2 OPENBLAS_NUM_THREADS=1 blender -b -t 2 --factory-startup \
    --python-exit-code 1 --python tools/build_purchased_characters.py

The archive contains five male appearances and no animation clips. All five
families are cooked; existing female characters remain separate. Families01/02
have no imported skeleton/skin weights in any provided FBX variant, so weights
are authored offline by surface transfer from the matching Character04 bind rig.
Walking and breathing are explicitly authored offline adaptations, not supplied
animations. Original FBXs/TGAs remain unchanged under assets/purchased/characters.
"""
import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import struct
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
ASSETS = ROOT/'assets/purchased/characters'
SOURCE = ASSETS/'source'
TEXTURES = ASSETS/'textures'
OUTPUT = ROOT/'projects/valwood_world/models'
ARCHIVE = Path('/home/computer/Downloads/NPC_Character_Pack_Update.zip')
ROLES = {'trader': (3, [('idle', 4)]), 'traveler': (5, [('walk', 8), ('idle', 4)]),
         'merchant': (1, [('idle', 4)]), 'craftsman': (2, [('walk', 8), ('idle', 4)]),
         'wayfarer': (4, [('walk', 8), ('idle', 4)])}
PROPRIETARY = 'User-provided purchased proprietary asset; no redistribution license asserted by this conversion.'


def sha(path):
    digest = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for block in iter(lambda: stream.read(1024*1024), b''):
            digest.update(block)
    return digest.hexdigest()


def prepare():
    """Extract only selected originals and convert actual source PBR channels."""
    from zipfile import ZipFile
    import shutil
    import numpy as np
    from PIL import Image
    ASSETS.mkdir(parents=True, exist_ok=True)
    TEXTURES.mkdir(parents=True, exist_ok=True)
    entries = []
    with ZipFile(ARCHIVE) as archive:
        selected = []
        for entry in archive.infolist():
            name = entry.filename
            if name.endswith('/'):
                continue
            texture = name.lower().endswith('.tga') and '_ID.' not in name and (
                any(f'/NPC_Character_{family:02}/Textures/' in name for family in range(1, 6)) or
                '/NPC_Character_02/Textures/Heads/' in name)
            model = any(name.endswith(f'/NPC_Character_{family:02}/Mesh_UE4/Full/SK_NPC_Character_{family:02}_Full_01.FBX') for family in range(1, 6))
            if texture or model:
                selected.append(entry)
        for entry in selected:
            relative = Path(entry.filename).relative_to('NPC_Character_Pack/UE4_Models')
            path = SOURCE/relative
            path.parent.mkdir(parents=True, exist_ok=True)
            if not path.exists() or path.stat().st_size != entry.file_size:
                with archive.open(entry) as stream, path.open('wb') as target:
                    shutil.copyfileobj(stream, target)
            entries.append({'archive_entry': entry.filename, 'path': str(path.relative_to(ROOT)),
                            'bytes': entry.file_size, 'archive_crc32': f'{entry.CRC:08x}', 'sha256': sha(path)})

    def image(path, size, mode):
        result = Image.open(path).convert(mode)
        return result.resize((size, size), Image.Resampling.LANCZOS)

    def convert(label, folder, albedo, normal, ao, metallic, sizes):
        base_size, normal_size, orm_size = sizes
        outputs = {}
        base = image(folder/albedo, base_size, 'RGBA')
        # Cloth03 has opaque RGB input; preserve real source alpha on hair and
        # layered cloth05, rather than turning cards into opaque rectangles.
        base_path = TEXTURES/f'{label}_base.png'
        base.save(base_path, compress_level=6)
        # Source UE4 normal maps use DirectX Y. glTF/native tangent space uses
        # OpenGL +Y; invert green once offline and renormalize after filtering.
        normals = np.asarray(image(folder/normal, normal_size, 'RGB'), dtype=np.float32)/127.5-1.
        normals[:, :, 1] *= -1.
        normals /= np.maximum(np.linalg.norm(normals, axis=2, keepdims=True), .001)
        normal_path = TEXTURES/f'{label}_normal.png'
        Image.fromarray(np.uint8(np.clip((normals+1)*127.5, 0, 255)), 'RGB').save(normal_path, compress_level=6)
        source_ao = np.asarray(image(folder/ao, orm_size, 'RGB'))[:, :, 0]
        source_metal = np.asarray(image(folder/metallic, orm_size, 'RGBA'))
        # The supplied metallic maps have equal RGB metalness and a separate
        # alpha roughness channel (matte cloth high, skin/hair mid-range).
        roughness = np.clip(source_metal[:, :, 3], 92 if label.startswith('head') else 72, 255)
        packed = np.stack([source_ao, roughness, source_metal[:, :, 0]], axis=2)
        orm_path = TEXTURES/f'{label}_orm.png'
        Image.fromarray(packed, 'RGB').save(orm_path, compress_level=6)
        for name, path in [('base', base_path), ('normal', normal_path), ('orm', orm_path)]:
            outputs[name] = {'path': str(path.relative_to(ROOT)), 'sha256': sha(path),
                             'dimensions': list(Image.open(path).size)}
        print('PURCHASED TEXTURES', label, json.dumps(outputs), flush=True)
        return outputs

    textures = {}
    head_folder = SOURCE/'NPC_Character_02/Textures/Heads'
    textures['head'] = convert('head', head_folder, 'T_NPC_Male_Heads_Albedo.tga',
        'T_NPC_Male_Heads_Normals.tga', 'T_NPC_Male_Heads_Ao.tga', 'T_NPC_Male_Heads_Metallic.tga', (2048, 1024, 512))
    # Character01 has its own supplied head atlas. Preserve it, with 1K detail
    # for the extra appearance rather than inventing a face/UV substitution.
    textures['head01'] = convert('head01', SOURCE/'NPC_Character_01/Textures/Heads',
        'T_NPC_Character_01_Heads_Albedo.tga', 'T_NPC_Character_01_Heads_Normals.tga',
        'T_NPC_Character_01_Heads_Ao.tga', 'T_NPC_Character_01_Heads_Metallic.tga', (1024, 512, 256))
    for family in range(1, 6):
        base = SOURCE/f'NPC_Character_{family:02}/Textures'
        prefix = 'T_NPC_Character_Cloth_02' if family==2 else f'T_NPC_Character_{family:02}'
        textures[f'body{family:02}'] = convert(f'body{family:02}', base if family==4 else base/'Character',
            prefix+'_Albedo.tga', prefix+('_Normal.tga' if family==2 else '_Normals.tga'), prefix+'_Ao.tga', prefix+'_Metallic.tga',
            (2048, 1024, 1024) if family in [3,5] else (1024,512,512))
        if family==4: continue  # Bald source appearance; no artificial hair material.
        prefix = 'NPC_Character_05_Hair' if family==5 else f'T_NPC_Character_{family:02}_Hair'
        textures[f'hair{family:02}'] = convert(f'hair{family:02}', base/'Hair',
            prefix+('_Albedo_01.tga' if family in [1,2] else '_Albedo.tga'), prefix+'_Normals.tga', prefix+'_Ao.tga', prefix+'_Metallic.tga',
            (1024,512,256) if family in [3,5] else (512,256,256))
    estimated_mib = sum(math.prod(value['dimensions'])*4*4/3 for spec in textures.values() for value in spec.values())/1048576
    assert estimated_mib <= 160, estimated_mib
    provenance = {'source': 'NPC Character Pack Update, purchased archive supplied by the user',
        'archive_external_original': str(ARCHIVE), 'archive_bytes': ARCHIVE.stat().st_size,
        'license': PROPRIETARY, 'selected_source_entries': entries, 'textures': textures,
        'estimated_texture_rgba8_mips_mib': estimated_mib,
        'conversion': 'Real source albedo, normal and AO; RGB metallic + alpha roughness repacked to ORM. UE4 normal green inverted for glTF; normals renormalized after reduction. Source alpha retained.',
        'appearance_scope': 'All five supplied male families are active; existing female appearances are retained. Families01/02 require authored offline skin-weight transfer from the matching04 bind rig; sources themselves remain untouched.'}
    (ASSETS/'PROVENANCE.json').write_text(json.dumps(provenance, indent=2)+'\n')
    (ASSETS/'LICENSE_SOURCE.txt').write_text(PROPRIETARY+'\nOriginal source: '+str(ARCHIVE)+'\nThe original archive and extracted selected FBX/TGA files are retained unchanged.\n')
    return provenance


def material(bpy, name, textures, masked=False):
    result = bpy.data.materials.new('Purchased_'+name)
    result.use_nodes = True
    nodes, links = result.node_tree.nodes, result.node_tree.links
    principled = nodes.get('Principled BSDF')
    for kind in ['base', 'normal', 'orm']:
        node = nodes.new('ShaderNodeTexImage')
        node.name = kind
        node.image = bpy.data.images.load(str(ROOT/textures[kind]['path']), check_existing=True)
        node.image.colorspace_settings.name = 'sRGB' if kind == 'base' else 'Non-Color'
        node.interpolation = 'Linear'
        node.extension = 'REPEAT'
        if kind == 'base':
            links.new(node.outputs['Color'], principled.inputs['Base Color'])
            if masked:
                links.new(node.outputs['Alpha'], principled.inputs['Alpha'])
        elif kind == 'normal':
            normal = nodes.new('ShaderNodeNormalMap')
            normal.inputs['Strength'].default_value = 1.
            links.new(node.outputs['Color'], normal.inputs['Color'])
            links.new(normal.outputs['Normal'], principled.inputs['Normal'])
        else:
            separate = nodes.new('ShaderNodeSeparateColor')
            links.new(node.outputs['Color'], separate.inputs['Color'])
            links.new(separate.outputs['Green'], principled.inputs['Roughness'])
            links.new(separate.outputs['Blue'], principled.inputs['Metallic'])
            group = bpy.data.node_groups.get('glTF Material Output')
            if group is None:
                group = bpy.data.node_groups.new('glTF Material Output', 'ShaderNodeTree')
                group.interface.new_socket(name='Occlusion', in_out='INPUT', socket_type='NodeSocketFloat')
            output = nodes.new('ShaderNodeGroup'); output.node_tree = group
            links.new(separate.outputs['Red'], output.inputs['Occlusion'])
    if masked:
        result.blend_method = 'CLIP'; result.alpha_threshold = .4
    result.use_backface_culling = False
    return result


def glb(path):
    raw = Path(path).read_bytes()
    assert struct.unpack_from('<III', raw) == (0x46546C67, 2, len(raw))
    json_size = struct.unpack_from('<I', raw, 12)[0]
    data = json.loads(raw[20:20+json_size])
    return data, raw[28+json_size:]


def inspect(output, records):
    textures = {}; geometry_bytes = 0
    for record in records:
        path = output/record['file']
        data, binary = glb(path)
        assert not data.get('skins') and not data.get('animations') and not data.get('extensionsRequired'), path
        primitives = [p for mesh in data['meshes'] for p in mesh['primitives']]
        assert 0 < len(primitives) <= 4, (path, len(primitives))
        assert all('COLOR_0' not in p['attributes'] and p.get('mode', 4) == 4 for p in primitives), path
        triangles = sum(data['accessors'][p['indices']]['count']//3 for p in primitives)
        assert 0 < triangles <= 18000, (path, triangles)
        image_views = set(); texture_hashes = []
        for mat in data.get('materials', []):
            assert mat.get('normalTexture', {}).get('scale', 1) == 1
            pbr = mat.get('pbrMetallicRoughness', {})
            for view, srgb in [(pbr.get('baseColorTexture'), True), (mat.get('normalTexture'), False), (pbr.get('metallicRoughnessTexture'), False)]:
                if not view: continue
                assert view.get('texCoord', 0) == 0 and not view.get('extensions'), path
                texture = data['textures'][view['index']]
                image = data['images'][texture['source']]
                buffer_view = data['bufferViews'][image['bufferView']]
                image_views.add(image['bufferView'])
                start = buffer_view.get('byteOffset', 0)
                encoded = binary[start:start+buffer_view['byteLength']]
                assert encoded[:8] == b'\x89PNG\r\n\x1a\n'
                width, height = struct.unpack_from('>II', encoded, 16)
                assert 0 < width <= 2048 and 0 < height <= 2048
                sampler = data.get('samplers', [])[texture['sampler']] if 'sampler' in texture else {}
                policy = (srgb, sampler.get('minFilter', 9987), sampler.get('magFilter', 9729), sampler.get('wrapS', 10497), sampler.get('wrapT', 10497))
                key = (hashlib.sha256(encoded).hexdigest(), policy)
                textures[key] = {'width': width, 'height': height, 'encoded_bytes': len(encoded)}
                texture_hashes.append(key[0])
        geometry_bytes += sum(v['byteLength'] for i,v in enumerate(data['bufferViews']) if i not in image_views)
        record.update(triangles=triangles, primitive_count=len(primitives), sha256=sha(path), bytes=path.stat().st_size,
                      texture_sha256=texture_hashes)
    estimated_mib = sum(t['width']*t['height']*4*4/3 for t in textures.values())/1048576
    assert estimated_mib <= 160 and len(textures) <= 33, (estimated_mib, len(textures))
    for role in ROLES:
        hashes = [r['texture_sha256'] for r in records if r['role'] == role]
        assert all(h == hashes[0] for h in hashes), (role, 'Texture bytes changed between poses')
    return {'pose_count':len(records), 'unique_texture_policies':len(textures), 'estimated_rgba_mips_mib':estimated_mib,
        'geometry_bytes':geometry_bytes, 'max_image_side':max(max(t['width'],t['height'])for t in textures.values()),
        'texture_encoded_bytes':sum(t['encoded_bytes']for t in textures.values()),
        'max_pose_triangles':max(r['triangles']for r in records), 'max_pose_primitives':max(r['primitive_count']for r in records)}


def author_missing_skin(bpy, originals, family):
    """Transfer the matching bind rig by closest-triangle barycentric weights.

    All three FBX variants of families01/02 contain meshes but no armature or
    vertex weights. Their bind positions match the supplied04 skeleton. Work
    entirely offline; keep every target's source UV/topology and original FBX.
    Semantic donor sets prevent a beard binding to the collar or hands to cloth.
    """
    from mathutils.bvhtree import BVHTree
    donor_path = SOURCE/'NPC_Character_04/Mesh_UE4/Full/SK_NPC_Character_04_Full_01.FBX'
    before = set(bpy.context.scene.objects)
    bpy.ops.import_scene.fbx(filepath=str(donor_path), automatic_bone_orientation=True)
    imported = [o for o in bpy.context.scene.objects if o not in before]
    arms = [o for o in imported if o.type=='ARMATURE']; assert len(arms)==1
    arm = arms[0]; donors=[o for o in imported if o.type=='MESH']
    groups = {}
    for category in ['head','hands','clothing']:
        choices = [o for o in donors if ('Head' in o.name if category=='head' else
            'Hands' in o.name if category=='hands' else not any(n in o.name for n in ['Head','Eyes','Bag']))]
        vertices=[]; triangles=[]; weights=[]
        for obj in choices:
            names={g.index:g.name for g in obj.vertex_groups}; offset=len(vertices)
            vertices.extend(obj.matrix_world@v.co for v in obj.data.vertices)
            weights.extend({names[g.group]:g.weight for g in v.groups if names[g.group] in arm.data.bones}for v in obj.data.vertices)
            obj.data.calc_loop_triangles()
            triangles.extend(tuple(offset+i for i in t.vertices)for t in obj.data.loop_triangles)
        assert vertices and triangles
        groups[category]=(BVHTree.FromPolygons(vertices,triangles,all_triangles=True),vertices,triangles,weights)
    furthest=0.; weighted=0
    for obj in originals:
        matrix=obj.matrix_world.copy();obj.parent=None;obj.matrix_world=matrix
        for modifier in list(obj.modifiers):obj.modifiers.remove(modifier)
        for name in arm.data.bones.keys():obj.vertex_groups.new(name=name)
        rigid=any(s in obj.name for s in ['Hair','Beard','Mustache','Eyes'])
        category='head' if 'Head' in obj.name else 'hands' if 'Hands' in obj.name else 'clothing'
        tree,vertices,triangles,weights=groups[category]
        for vertex in obj.data.vertices:
            if rigid: combined={'head':1.}
            else:
                point=matrix@vertex.co
                nearest,normal,index,distance=tree.find_nearest(point);assert index is not None
                furthest=max(furthest,distance)
                triangle=triangles[index];a,b,c=(vertices[i]for i in triangle)
                ab=b-a;ac=c-a;ap=nearest-a
                d00=ab.dot(ab);d01=ab.dot(ac);d11=ac.dot(ac);den=d00*d11-d01*d01
                if abs(den)<1e-16: bary=[1.,0.,0.]
                else:
                    v=(d11*ap.dot(ab)-d01*ap.dot(ac))/den
                    w=(d00*ap.dot(ac)-d01*ap.dot(ab))/den
                    bary=[max(0.,1-v-w),max(0.,v),max(0.,w)]
                combined={}
                for i,factor in zip(triangle,bary):
                    for bone,weight in weights[i].items():combined[bone]=combined.get(bone,0.)+factor*weight
                combined=dict(sorted(combined.items(),key=lambda item:item[1],reverse=True)[:4])
                total=sum(combined.values());assert total>1e-6,(obj.name,vertex.index)
                combined={bone:value/total for bone,value in combined.items() if value/total>.00001}
            for bone,value in combined.items():obj.vertex_groups[bone].add([vertex.index],value,'REPLACE')
            weighted+=1
        modifier=obj.modifiers.new('Authored bind-rig surface transfer','ARMATURE');modifier.object=arm
    assert furthest<.12,(family,furthest)
    for obj in imported:
        if obj!=arm:bpy.data.objects.remove(obj,do_unlink=True)
    print('AUTHORED SKIN',family,weighted,'vertices; max donor distance',furthest,flush=True)
    return {'method':'Offline closest-triangle barycentric skin weights from supplied matching Character04 bind rig; head/hair/hand semantic donors, up to four normalized influences. No skeleton/weights supplied for01/02.',
            'donor':str(donor_path.relative_to(ROOT)),'donor_sha256':sha(donor_path),
            'weighted_vertices':weighted,'max_donor_surface_distance_m':furthest}


def cook(output=OUTPUT):
    import bpy
    from mathutils import Matrix, Quaternion, Vector
    output = Path(output); output.mkdir(parents=True, exist_ok=True)
    provenance = json.loads((ASSETS/'PROVENANCE.json').read_text())
    records = []
    with tempfile.TemporaryDirectory(prefix='valwood-purchased-characters-') as directory:
        stage = Path(directory)
        for role, (family, modes) in ROLES.items():
            bpy.ops.wm.read_factory_settings(use_empty=True)
            path = SOURCE/f'NPC_Character_{family:02}'/'Mesh_UE4/Full'/f'SK_NPC_Character_{family:02}_Full_01.FBX'
            bpy.ops.import_scene.fbx(filepath=str(path), automatic_bone_orientation=True)
            scene = bpy.context.scene
            originals = [o for o in scene.objects if o.type == 'MESH']
            arms = [o for o in scene.objects if o.type == 'ARMATURE']
            skin = author_missing_skin(bpy,originals,family) if not arms and family in [1,2] else None
            arms = [o for o in scene.objects if o.type == 'ARMATURE']; assert len(arms) == 1
            arm = arms[0]
            assert not bpy.data.actions, 'Do not silently replace supplied animation clips'
            source_triangles = sum(sum(len(p.vertices)-2 for p in o.data.polygons) for o in originals)
            textures = provenance['textures']
            materials = {'body': material(bpy, f'body{family:02}', textures[f'body{family:02}'], masked=family in [1,4,5]),
                         'head': material(bpy, 'head01' if family==1 else 'head', textures['head01' if family==1 else 'head'])}
            if family!=4:materials['hair']=material(bpy,f'hair{family:02}',textures[f'hair{family:02}'],masked=True)
            # Preserve facial topology ahead of dense folded cloth and tiny
            # hardware. Collapse is evaluated once at bind pose before skinning.
            def budget(name):
                if 'Head' in name: return 4400
                if 'Eyes' in name: return 300
                if 'Hair' in name: return 2900
                if 'Beard' in name or 'Mustache' in name:return 800
                if 'Hands' in name: return 1100
                if 'Shoes' in name: return 1750
                if 'Pants_Detail' in name: return 650
                if 'Pants' in name: return 1600
                if 'Bag' in name: return 450
                if 'Belt' in name: return 400
                if 'Cloth_02' in name: return 450
                if 'Cloth_Detail' in name: return 650
                if 'Cloth_01' in name: return 2600 if family==3 else 1900
                if 'Body_Cloth' in name:return 3200
                if 'Body_Detail' in name:return 650
                raise ValueError(name)
            for obj in originals:
                name = obj.name
                # Source expression morphs remain in the retained FBX. Static
                # runtime poses use the neutral Basis; strip offline morph
                # blocks before topology reduction without baking an emotion.
                if obj.data.shape_keys:
                    for key in obj.data.shape_keys.key_blocks: key.value = 0.
                    obj.shape_key_clear()
                # The source FBX leaves Cloth_02's material unconfigured even
                # though its UVs address the supplied garment atlas. Apply that
                # real cloth/alpha map instead of exporting a flat gray panel.
                kind = 'hair' if any(s in name for s in ['Hair','Beard','Mustache']) else 'head' if any(s in name for s in ['Head','Hands','Eyes']) else 'body'
                obj.data.materials.clear(); obj.data.materials.append(materials[kind])
                for polygon in obj.data.polygons: polygon.material_index = 0
                for attribute in list(obj.data.color_attributes): obj.data.color_attributes.remove(attribute)
                triangles = sum(len(p.vertices)-2 for p in obj.data.polygons)
                bpy.ops.object.select_all(action='DESELECT'); obj.select_set(True); bpy.context.view_layer.objects.active=obj
                modifier = obj.modifiers.new('Offline bounded character geometry', 'DECIMATE')
                modifier.ratio = min(1., budget(name)/triangles)
                modifier.use_collapse_triangulate=True
                while obj.modifiers.find(modifier.name)>0:
                    bpy.ops.object.modifier_move_up(modifier=modifier.name)
                bpy.ops.object.modifier_apply(modifier=modifier.name)
            bpy.context.view_layer.update()
            reduced_triangles = sum(sum(len(p.vertices)-2 for p in o.data.polygons)for o in originals)
            assert reduced_triangles <= 18000, (role, reduced_triangles)
            bones = arm.pose.bones
            world = arm.matrix_world.copy(); inverse = world.inverted()
            def update(): bpy.context.view_layer.update()
            def head(name): return world @ bones[name].head
            def aim(name, direction):
                bone = bones[name]
                matrix = world @ bone.matrix
                start = head(name); current = world @ bone.tail - start
                delta = current.normalized().rotation_difference(Vector(direction).normalized())
                location, rotation, scale = matrix.decompose()
                bone.matrix = inverse @ Matrix.LocRotScale(location, delta @ rotation, scale)
                update()
            def rotate_world(name, axis, angle):
                matrix = world @ bones[name].matrix
                location, rotation, scale = matrix.decompose()
                bones[name].matrix = inverse @ Matrix.LocRotScale(location, Quaternion(Vector(axis), angle) @ rotation, scale)
                update()
            # The imported arms are an A-pose. Relax them before defining the
            # reference body height; never export the source bind pose as idle.
            def relaxed():
                for bone in bones: bone.matrix_basis = Matrix.Identity(4)
                update()
                for side, sign in [('l',1),('r',-1)]:
                    aim('upperarm_'+side,(sign*.13,-.045,-1))
                    aim('lowerarm_'+side,(sign*.025,-.14,-1))
                update()
            relaxed()
            neutral = {bone.name:bone.matrix_basis.copy()for bone in bones}
            neutral_feet = {name:(world @ bones[name].matrix).to_quaternion() for name in ['foot_l','foot_r','ball_l','ball_r']}
            ankle_reference = {side:head('foot_'+side).copy()for side in ['l','r']}
            hip_reference = head('pelvis').copy()
            graph = bpy.context.evaluated_depsgraph_get()
            points = [o.matrix_world @ v.co for o in originals for v in o.evaluated_get(graph).data.vertices]
            reference_low = min(v.z for v in points); reference_high = max(v.z for v in points)
            fixed_scale = 1.82/(reference_high-reference_low)
            foot_indices = {}
            for obj in originals:
                names = {g.index:g.name for g in obj.vertex_groups}; sides={s:[]for s in ['l','r']}
                for vertex in obj.data.vertices:
                    for side in sides:
                        if sum(g.weight for g in vertex.groups if names.get(g.group) in ['foot_'+side,'ball_'+side])>.4:
                            sides[side].append(vertex.index)
                if any(sides.values()): foot_indices[obj.name]=sides
            assert all(any(sides[side]for sides in foot_indices.values())for side in ['l','r']), role

            def leg(side, target):
                hip=head('thigh_'+side); knee=head('calf_'+side); ankle=head('foot_'+side)
                a=(knee-hip).length; b=(ankle-knee).length
                delta=target-hip; direction=delta.normalized(); distance=min(delta.length,a+b-.0001)
                along=(a*a-b*b+distance*distance)/(2*distance)
                bend=Vector((0,-1,0)); bend=(bend-direction*bend.dot(direction)).normalized()
                joint=hip+direction*along+bend*math.sqrt(max(0,a*a-along*along))
                aim('thigh_'+side,joint-hip)
                aim('calf_'+side,target-head('calf_'+side))
                for name in ['foot_'+side,'ball_'+side]:
                    matrix=world @ bones[name].matrix
                    location,rotation,scale=matrix.decompose()
                    bones[name].matrix=inverse @ Matrix.LocRotScale(location,neutral_feet[name],scale)
                    update()

            uv_hash=None
            for mode,count in modes:
                for frame in range(count):
                    fraction=frame/count; phase=2*math.pi*fraction
                    for bone in bones: bone.matrix_basis=neutral[bone.name].copy()
                    update()
                    if mode=='walk':
                        pelvis=world @ bones['pelvis'].matrix
                        pelvis.translation.z-=.055+.008*math.cos(phase*2)
                        bones['pelvis'].matrix=inverse @ pelvis; update()
                        rotate_world('pelvis',(0,0,1),.035*math.sin(phase))
                        rotate_world('spine_02',(0,0,1),-.045*math.sin(phase))
                        for side,offset,sign in [('l',0,1),('r',math.pi,-1)]:
                            cycle=phase+offset
                            target=ankle_reference[side].copy()
                            target.y-=.245*math.cos(cycle)
                            target.z+=.075*max(0.,-math.sin(cycle))
                            leg(side,target)
                            # Imported shoulder pivots sit behind the torso.
                            # A forward bias and modest outward clearance keep
                            # the opposing wrist swing visible, rather than
                            # moving both hands inside/behind the body volume.
                            aim('upperarm_'+side,(sign*.22,-.20+.42*math.cos(cycle),-1))
                            aim('lowerarm_'+side,(sign*.025,-.25+.14*math.cos(cycle),-1))
                        method='Authored 0.9-second grounded two-bone IK gait; relaxed source A-pose, opposing arm swing, knee flexion, neutral foot orientation. No source animation clip supplied.'
                    else:
                        rotate_world('spine_02',(1,0,0),.006*math.sin(phase))
                        rotate_world('head',(0,0,1),.018*math.sin(phase+.4))
                        for side,sign in [('l',1),('r',-1)]:
                            aim('upperarm_'+side,(sign*(.13+.008*math.sin(phase)), -.045+.006*math.sin(phase),-1))
                            aim('lowerarm_'+side,(sign*.025,-.14,-1))
                        method='Authored relaxed breathing idle and subtle head turn; original male source mesh/rig, no supplied idle clip.'
                    update(); graph=bpy.context.evaluated_depsgraph_get()
                    baked=[]; feet={side:float('inf')for side in ['l','r']}
                    for original in originals:
                        mesh=bpy.data.meshes.new_from_object(original.evaluated_get(graph),depsgraph=graph)
                        mesh.transform(original.matrix_world)
                        obj=bpy.data.objects.new('Body_'+role+'_'+original.name,mesh)
                        bpy.context.collection.objects.link(obj);baked.append(obj)
                        for side,indices in foot_indices.get(original.name,{}).items():
                            for index in indices:feet[side]=min(feet[side],mesh.vertices[index].co.z)
                    ground=min(feet.values());assert math.isfinite(ground)
                    origin=Vector((hip_reference.x,hip_reference.y,ground))
                    hands={public:list((head(bone)-origin)*fixed_scale)for public,bone in [('LeftHand','hand_l'),('RightHand','hand_r')]}
                    for obj in baked:
                        for vertex in obj.data.vertices:vertex.co=(vertex.co-origin)*fixed_scale
                        obj.data.update()
                    body_low=min(v.co.z for o in baked for v in o.data.vertices)
                    body_high=max(v.co.z for o in baked for v in o.data.vertices)
                    foot_heights={s:(v-ground)*fixed_scale for s,v in feet.items()}
                    assert abs(min(foot_heights.values()))<.00001 and body_low>=-.025,(role,mode,frame,body_low,foot_heights)
                    bpy.ops.object.select_all(action='DESELECT')
                    for obj in baked:obj.select_set(True)
                    bpy.context.view_layer.objects.active=baked[0];bpy.ops.object.join()
                    body=baked[0];body.name=f'Body_{role}_{mode}_{frame:02}';body.data.name=body.name
                    current_uv=hashlib.sha256(b''.join(struct.pack('<ff',*uv.uv)for uv in body.data.uv_layers.active.data)).hexdigest()
                    if uv_hash is None:uv_hash=current_uv
                    assert current_uv==uv_hash,(role,'Changing UV layout across poses')
                    filename=f'purchased_character_{role}_{mode}_{frame:02}.glb'
                    path_out=stage/filename
                    bpy.ops.export_scene.gltf(filepath=str(path_out),export_format='GLB',use_selection=True,
                        export_yup=True,export_animations=False,export_skins=False,export_image_format='AUTO',
                        export_cameras=False,export_lights=False)
                    record={'file':filename,'role':role,'mode':mode,'frame':frame,'source_family':family,
                        'body_triangles':reduced_triangles,'body_meshes':1,'source_body_meshes':len(originals),
                        'body_height_m':body_high-body_low,'body_min_y_m':body_low,'body_max_y_m':body_high,
                        'min_foot_m':min(foot_heights.values()),'foot_heights_m':foot_heights,
                        'reference_body_height_m':1.82,'fixed_reference_scale':fixed_scale,
                        'horizontal_origin':'fixed source pelvis reference; no animated bounds recentering',
                        'source':str(path.relative_to(ROOT)),'source_sha256':sha(path), 'source_triangles':source_triangles,
                        'creator':'NPC Character Pack creator, purchased source supplied by user', 'license':PROPRIETARY,
                        'method':method,'uv_sha256':uv_hash,'hands':hands,'gear_meshes':0,
                        'source_vertex_colors':'Unsupported vertex colors removed; source textured materials converted to glTF PBR'}
                    if skin:record['authored_skin']=skin
                    records.append(record)
                    print('PURCHASED POSE',role,mode,frame,reduced_triangles,foot_heights,'height',body_high-body_low,flush=True)
                    mesh=body.data;bpy.data.objects.remove(body,do_unlink=True)
                    if not mesh.users:bpy.data.meshes.remove(mesh)
        assert len(records)==sum(count for _,modes in ROLES.values()for _,count in modes)
        for role,(_,modes) in ROLES.items():
            walking=[r for r in records if r['role']==role and r['mode']=='walk']
            if not walking:continue
            left=[r['hands']['LeftHand'][1]for r in walking]
            right=[r['hands']['RightHand'][1]for r in walking]
            assert max(left)-min(left)>.20 and max(right)-min(right)>.20,(role,'Obscured or stationary authored arm swing')
            assert (left[0]-left[4])*(right[0]-right[4])<0,(role,'Walking arms must oppose each other')
        budget=inspect(stage,records)
        manifest={'schema_version':1,'source':'User-provided purchased NPC Character Pack Update',
            'provenance':str((ASSETS/'PROVENANCE.json').relative_to(ROOT)), 'license':PROPRIETARY,
            'notes':'44 purchased male poses across all five supplied families. Existing female, guard and archer assets retained separately. No source animation clips; idle/IK walking authored offline. Families01/02 have no source skin weights; matching04 bind-rig weights transferred offline and recorded per pose.',
            'forward':'glTF +Z (Blender -Y); runtime yaw=atan2(dx,dz)',
            'archetypes':{role:{'walk_poses':sum(count for mode,count in modes if mode=='walk'),'idle_poses':sum(count for mode,count in modes if mode=='idle')}for role,(_,modes)in ROLES.items()},
            'budget':budget,'total_bytes':sum(r['bytes']for r in records),'assets':records}
        for record in records:
            target=output/record['file'];pending=target.with_suffix('.glb.pending')
            pending.write_bytes((stage/record['file']).read_bytes());os.replace(pending,target)
        pending=output/'purchased_character_manifest.json.pending'
        pending.write_text(json.dumps(manifest,indent=2)+'\n');os.replace(pending,output/'purchased_character_manifest.json')
        print('PURCHASED CHARACTER COOK COMPLETE',json.dumps(budget),flush=True)
        return manifest


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--prepare-only',action='store_true')
    parser.add_argument('--output',type=Path,default=OUTPUT)
    arguments = sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else ([] if Path(sys.argv[0]).name.startswith('blender') else sys.argv[1:])
    args=parser.parse_args(arguments)
    if args.prepare_only:prepare()
    else:cook(args.output)
