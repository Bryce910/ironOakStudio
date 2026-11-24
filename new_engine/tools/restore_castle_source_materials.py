#!/usr/bin/env python3
"""Restore source repeating masonry UV/PBR without changing castle geometry.

Blender --background --threads 1 --python this_file -- --metadata
Python this_file --stage / --publish (validated stage and preserved originals).
Only our own code executes. Purchased FBX and Downloads are read-only inputs.
"""
import argparse
import copy
import hashlib
import json
import os
from pathlib import Path
import shutil
import sys
sys.path.insert(0, str(Path(__file__).resolve().parent))
import numpy as np
from build_purchased_village import (ROOT, SOURCE, OUT, FAMILIES, MATERIALS, glb,
                                    encode_glb, inspect_glb, material_source_settings, sha, validate_from_rows)
from build_purchased_village_facades import accessor, compact
from validate_purchased_village_fidelity import key

DEST = SOURCE/'castle-source-materials'
STAGE = DEST/'staged'
BACKUP = DEST/'before'
FAMILIES_TO_RESTORE = ('castle', 'gate', 'wall')
TILE_KEYS = {'MI_Tileable_Castle_Walls_01', 'MI_Tileable_Castle_Walls_02',
             'MI_Tileable_Wall_Stone_01', 'MI_Tileable_Wall_Sone_02'}


def metadata():
    import bpy
    from mathutils import Matrix, Vector
    DEST.mkdir(parents=True, exist_ok=True)
    for family in FAMILIES_TO_RESTORE:
        bpy.ops.wm.read_factory_settings(use_empty=True)
        filename, budget, _ = FAMILIES[family]
        bpy.ops.import_scene.fbx(filepath=str(SOURCE/'source_meshes'/filename), use_anim=False, use_custom_normals=True)
        objects = [o for o in bpy.context.scene.objects if o.type == 'MESH' and not o.name.startswith('UCX_') and ('_LOD0' in o.name or '_LOD' not in o.name)]
        assert len(objects) == 1
        obj = objects[0]
        for other in list(bpy.context.scene.objects):
            if other != obj:
                bpy.data.objects.remove(other, do_unlink=True)
        obj.data = obj.data.copy()
        obj.data.transform(obj.matrix_world)
        obj.matrix_world = Matrix.Identity(4)
        bpy.context.view_layer.objects.active = obj
        obj.select_set(True)
        tris = sum(len(p.vertices)-2 for p in obj.data.polygons)
        if tris > budget:
            modifier = obj.modifiers.new('Native near triangle budget', 'DECIMATE')
            modifier.ratio = budget/tris*.995
            bpy.ops.object.modifier_apply(modifier=modifier.name)
        xyz = [v.co.copy() for v in obj.data.vertices]
        lo = Vector([min(p[k] for p in xyz) for k in range(3)])
        hi = Vector([max(p[k] for p in xyz) for k in range(3)])
        obj.data.transform(Matrix.Translation(Vector([-(lo.x+hi.x)/2, -(lo.y+hi.y)/2, -lo.z])))
        original = obj.data.uv_layers[0]
        for layer in list(obj.data.uv_layers):
            if layer != original:
                obj.data.uv_layers.remove(layer)
        obj.data.uv_layers.active = original
        original.active_render = True
        for material in obj.data.materials:
            material.use_nodes = True
            nodes = material.node_tree.nodes
            nodes.clear()
            bsdf = nodes.new('ShaderNodeBsdfPrincipled')
            output = nodes.new('ShaderNodeOutputMaterial')
            material.node_tree.links.new(bsdf.outputs[0], output.inputs['Surface'])
        path = DEST/(family+'_metadata.glb')
        bpy.ops.export_scene.gltf(filepath=str(path), export_format='GLB', export_yup=True,
                                 export_animations=False, export_cameras=False, export_lights=False,
                                 export_tangents=True, export_colors=False)
        print('SOURCE_METADATA', family, path.stat().st_size, flush=True)


def triangle_records(doc, binary):
    records = []
    for mesh in doc['meshes']:
        for p in mesh['primitives']:
            xyz = accessor(doc, binary, p['attributes']['POSITION'], 3)
            normal = accessor(doc, binary, p['attributes']['NORMAL'], 3)
            uv = accessor(doc, binary, p['attributes']['TEXCOORD_0'], 2)
            tangent = accessor(doc, binary, p['attributes']['TANGENT'], 4)
            for tri in accessor(doc, binary, p['indices'], 1).reshape(-1, 3):
                records.append(dict(xyz=xyz[tri], normal=normal[tri], uv=uv[tri], tangent=tangent[tri], material=doc['materials'][p['material']]['name'].split('.')[0]))
    return records


def stage():
    STAGE.mkdir(parents=True, exist_ok=True)
    manifest = json.loads((OUT/'purchased_village_manifest.json').read_text())
    reports = []
    for family in FAMILIES_TO_RESTORE:
        name = 'purchased_village_'+family+'_near'
        baseline = (BACKUP if (BACKUP/(name+'.glb')).exists() else OUT)/(name+'.glb')
        doc, old_binary = glb(baseline)
        doc = copy.deepcopy(doc)
        binary = bytearray(old_binary)
        source, data = glb(DEST/(family+'_metadata.glb'))
        candidates = {}
        for row in triangle_records(source, data):
            if row['material'] in TILE_KEYS:
                candidates.setdefault(key(row['xyz']), []).append(row)
        assert candidates, (family, 'No source masonry')
        groups, retained, moved = {}, [], 0
        for primitive in doc['meshes'][0]['primitives']:
            attrs = primitive['attributes']
            xyz = accessor(doc, old_binary, attrs['POSITION'], 3)
            normal = accessor(doc, old_binary, attrs['NORMAL'], 3)
            remaining = []
            for tri in accessor(doc, old_binary, primitive['indices'], 1).reshape(-1, 3):
                points = xyz[tri]
                choices = candidates.get(key(points), [])
                if not choices:
                    remaining.extend(int(i) for i in tri)
                    continue
                source = min(choices, key=lambda r: float(np.sum((np.mean(r['normal'], axis=0)-np.mean(normal[tri], axis=0))**2)))
                order = [int(np.argmin(np.sum((source['xyz']-p)**2, axis=1))) for p in points]
                assert len(set(order)) == 3 and np.max(np.abs(source['xyz'][order]-points)) < 2e-5
                assert np.max(np.abs(source['normal'][order]-normal[tri])) < .002
                vertices = groups.setdefault(source['material'], [])
                tiling = material_source_settings(source['material'])[1]
                for j, index in enumerate(tri):
                    vertices.append(np.concatenate([points[j], normal[index], source['uv'][order[j]]*tiling, source['tangent'][order[j]]]).astype('<f4'))
                moved += 1
            if remaining:
                a = append_data(doc, binary, np.asarray(remaining, dtype='<u4'), 'SCALAR', 5125, 34963)
                retained.append(dict(primitive, indices=a))
        assert moved >= 20 and moved == sum(len(rows) for rows in candidates.values()), (family, 'Not all source masonry triangles matched the live geometry')

        def source_texture(stem, role):
            path = SOURCE/'prepared'/(stem+'_'+role+'.png')
            encoded = path.read_bytes()
            binary.extend(b'\0'*((-len(binary))%4))
            view = len(doc['bufferViews'])
            doc['bufferViews'].append(dict(buffer=0, byteOffset=len(binary), byteLength=len(encoded)))
            binary.extend(encoded)
            image = len(doc['images'])
            doc['images'].append(dict(name=path.stem, mimeType='image/png', bufferView=view))
            sampler = len(doc['samplers'])
            doc['samplers'].append(dict(magFilter=9729, minFilter=9987, wrapS=10497, wrapT=10497))
            index = len(doc['textures'])
            doc['textures'].append(dict(source=image, sampler=sampler))
            return index

        for material, vertices in sorted(groups.items()):
            stem = MATERIALS[material]
            bc, n, arm = (source_texture(stem, k) for k in ('BC', 'N', 'ARM'))
            mat = len(doc['materials'])
            doc['materials'].append(dict(name='Purchased_Source_'+material, alphaMode='OPAQUE', doubleSided=True,
                                        pbrMetallicRoughness=dict(baseColorTexture=dict(index=bc), metallicRoughnessTexture=dict(index=arm), metallicFactor=1, roughnessFactor=1),
                                        normalTexture=dict(index=n), occlusionTexture=dict(index=arm)))
            packed = np.asarray(vertices, dtype='<f4')
            indices = np.arange(len(packed), dtype='<u4')
            binary.extend(b'\0'*((-len(binary))%4))
            view = len(doc['bufferViews'])
            doc['bufferViews'].append(dict(buffer=0, byteOffset=len(binary), byteLength=packed.nbytes, byteStride=48, target=34962))
            binary.extend(packed.tobytes())
            attrs = {}
            for label, offset, kind in [('POSITION', 0, 'VEC3'), ('NORMAL', 12, 'VEC3'), ('TEXCOORD_0', 24, 'VEC2'), ('TANGENT', 32, 'VEC4')]:
                a = dict(bufferView=view, byteOffset=offset, componentType=5126, count=len(packed), type=kind)
                if label == 'POSITION':
                    a.update(min=packed[:, :3].min(axis=0).tolist(), max=packed[:, :3].max(axis=0).tolist())
                attrs[label] = len(doc['accessors'])
                doc['accessors'].append(a)
            a = append_data(doc, binary, indices, 'SCALAR', 5125, 34963)
            retained.append(dict(attributes=attrs, indices=a, material=mat, mode=4))
        doc['meshes'][0]['primitives'] = retained
        doc, binary = compact(doc, binary)
        path = STAGE/(name+'.glb')
        path.write_bytes(encode_glb(doc, binary))
        entry = inspect_glb(path)
        row = next(r for r in manifest['assets'] if r['asset'] == name)
        assert entry['triangles'] == row['triangles'] and entry['bounds_y_up'] == row['bounds_y_up']
        before = triangle_records(*glb(baseline))
        after = triangle_records(doc, binary)
        assert sorted(key(r['xyz']) for r in before) == sorted(key(r['xyz']) for r in after)
        row.update(entry)
        row.update(source_masonry=dict(schema=1, source_mesh_sha256=sha(SOURCE/'source_meshes'/row['source']), source_metadata_sha256=sha(DEST/(family+'_metadata.glb')), triangles=moved, uv0_restored=True, geometry_exact=True),
                   surface_representation='Original repeating castle masonry UV/PBR + exact residual atlas; existing middle LOD retained')
        reports.append(dict(family=family, source_triangles=moved, total_triangles=entry['triangles'], geometry_exact=True, original_glb_sha256=sha(baseline), staged_sha256=sha(path), primitives=entry['primitive_count']))
        print('STAGED_MASONRY', family, moved, entry['primitive_count'], flush=True)
    manifest['gpu_rgba8_mip_bytes'] = validate_from_rows(manifest)
    (STAGE/'purchased_village_manifest.json').write_text(json.dumps(manifest, indent=2)+'\n')
    report = dict(schema=1, source_downloads_unchanged=True, original_geometry_exact=True, all_mid_files_unchanged=True,
                  architecture_rgba8_mip_mib=manifest['gpu_rgba8_mip_bytes']/1024**2, records=reports)
    (STAGE/'validation.json').write_text(json.dumps(report, indent=2)+'\n')
    print('STAGED_GPU_MIB', report['architecture_rgba8_mip_mib'], flush=True)


def append_data(doc, binary, values, kind, component, target):
    binary.extend(b'\0'*((-len(binary))%4))
    view = len(doc['bufferViews'])
    doc['bufferViews'].append(dict(buffer=0, byteOffset=len(binary), byteLength=values.nbytes, target=target))
    binary.extend(values.tobytes())
    index = len(doc['accessors'])
    doc['accessors'].append(dict(bufferView=view, componentType=component, count=len(values), type=kind,
                                 min=[int(values.min())], max=[int(values.max())]))
    return index


def publish():
    report = json.loads((STAGE/'validation.json').read_text())
    manifest = json.loads((STAGE/'purchased_village_manifest.json').read_text())
    live = json.loads((OUT/'purchased_village_manifest.json').read_text())
    assert report['architecture_rgba8_mip_mib'] < 544
    changed = {r['family'] for r in report['records']}
    for row in live['assets']:
        target = next(r for r in manifest['assets'] if r['asset'] == row['asset'])
        if row['family'] not in changed or row['lod'] == 'mid':
            assert target == row and sha(OUT/(row['asset']+'.glb')) == row['sha256']
    BACKUP.mkdir(parents=True, exist_ok=True)
    if not (BACKUP/'purchased_village_manifest.json').exists():
        shutil.copy2(OUT/'purchased_village_manifest.json', BACKUP/'purchased_village_manifest.json')
    for row in report['records']:
        filename = 'purchased_village_'+row['family']+'_near.glb'
        assert sha(STAGE/filename) == row['staged_sha256']
        if not (BACKUP/filename).exists():
            assert sha(OUT/filename) == row['original_glb_sha256']
            shutil.copy2(OUT/filename, BACKUP/filename)
    for row in report['records']:
        filename = 'purchased_village_'+row['family']+'_near.glb'
        temporary = OUT/(filename+'.tmp')
        shutil.copy2(STAGE/filename, temporary)
        os.replace(temporary, OUT/filename)
    temporary = OUT/'purchased_village_manifest.json.tmp'
    shutil.copy2(STAGE/'purchased_village_manifest.json', temporary)
    os.replace(temporary, OUT/'purchased_village_manifest.json')
    print('PUBLISHED castle/gate/wall source materials; source purchases and middle LODs unchanged')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for action in ('metadata', 'stage', 'publish'):
        parser.add_argument('--'+action, action='store_true')
    args = parser.parse_args(sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else None)
    if args.metadata:
        metadata()
    if args.stage:
        stage()
    if args.publish:
        publish()


if __name__ == '__main__':
    main()
