#!/usr/bin/env python3
"""Losslessly compact a static embedded opening GLB before project publication.

Only unreferenced core glTF resources and redundant binary-view payloads are
removed. Referenced geometry arrays, images, PBR properties, sampler settings,
node transforms and extras remain exact. Unknown extension reference graphs
are rejected rather than silently rewritten. No source/project writes occur
in ``compact_home_glb``; the optional CLI requires a NEW output file.
"""
from __future__ import annotations

import argparse
import copy
import fnmatch
import hashlib
import json
from pathlib import Path
import struct
import io

from PIL import Image
import numpy as np


_MATERIAL_TEXTURES = (
    ('pbrMetallicRoughness', 'baseColorTexture'),
    ('pbrMetallicRoughness', 'metallicRoughnessTexture'),
    (None, 'normalTexture'), (None, 'occlusionTexture'),
    (None, 'emissiveTexture'),
)
_SCALAR_MATERIAL_EXTENSIONS = {'KHR_materials_emissive_strength'}


def _read(raw):
    if len(raw) < 28 or struct.unpack_from('<III', raw) != (0x46546c67, 2, len(raw)):
        raise ValueError('Expected a complete GLB version2')
    length, kind = struct.unpack_from('<II', raw, 12)
    if kind != 0x4e4f534a:
        raise ValueError('Expected GLB JSON first')
    doc = json.loads(raw[20:20 + length])
    offset = 20 + length
    size, kind = struct.unpack_from('<II', raw, offset)
    if kind != 0x004e4942 or offset + 8 + size != len(raw):
        raise ValueError('Expected exactly one GLB binary chunk')
    return doc, raw[offset + 8:]


def _encode(doc, binary):
    encoded = json.dumps(doc, separators=(',', ':'), allow_nan=False).encode()
    encoded += b' ' * (-len(encoded) % 4)
    binary += b'\0' * (-len(binary) % 4)
    return (struct.pack('<III', 0x46546c67, 2, 28 + len(encoded) + len(binary)) +
            struct.pack('<II', len(encoded), 0x4e4f534a) + encoded +
            struct.pack('<II', len(binary), 0x004e4942) + binary)


def _indices(values, resources, label):
    result = sorted(set(values))
    if any(type(value) is not int or not 0 <= value < len(resources) for value in result):
        raise ValueError('Invalid ' + label + ' reference')
    return result, {old: new for new, old in enumerate(result)}


def _texture_infos(material):
    for parent_key, key in _MATERIAL_TEXTURES:
        parent = material if parent_key is None else material.get(parent_key, {})
        if key in parent:
            yield parent[key]


def compact_home_glb(raw):
    """Return ``(compacted_bytes, report)`` without mutating the input bytes.

    All nodes/scenes are retained, including alternate scene roots. Only mesh
    resources with no node reference are removed. Sparse accessor views and
    arbitrary core vertex attributes are remapped along with ordinary arrays.
    Binary views are copied byte-for-byte and deduplicated only when both
    their non-offset descriptors and complete payload bytes are identical.
    """
    source, binary = _read(bytes(raw))
    if any(source.get(key) for key in ('skins', 'animations', 'cameras')):
        raise ValueError('Opening compaction accepts static camera-free GLBs')
    if set(source.get('extensionsUsed', []) + source.get('extensionsRequired', [])) - _SCALAR_MATERIAL_EXTENSIONS:
        raise ValueError('Unknown GLB extension reference graph')
    buffers = source.get('buffers', [])
    if len(buffers) != 1 or buffers[0].get('uri') or buffers[0]['byteLength'] > len(binary):
        raise ValueError('Expected one embedded source buffer')
    for node in source.get('nodes', []):
        if node.get('extensions') or 'skin' in node or 'camera' in node:
            raise ValueError('Unexpected static node resource reference')
    doc = copy.deepcopy(source)
    mesh_ids, meshes = _indices([node['mesh'] for node in doc.get('nodes', []) if 'mesh' in node], source.get('meshes', []), 'mesh')
    doc['meshes'] = [doc['meshes'][index] for index in mesh_ids]
    accessors = []
    materials = []
    for mesh in doc['meshes']:
        for primitive in mesh['primitives']:
            if primitive.get('extensions'):
                raise ValueError('Primitive extensions need explicit reference remapping')
            accessors.extend(primitive.get('attributes', {}).values())
            if 'indices' in primitive:
                accessors.append(primitive['indices'])
            for target in primitive.get('targets', []):
                accessors.extend(target.values())
            if 'material' in primitive:
                materials.append(primitive['material'])
    accessor_ids, accessor_map = _indices(accessors, source.get('accessors', []), 'accessor')
    material_ids, material_map = _indices(materials, source.get('materials', []), 'material')
    doc['accessors'] = [doc['accessors'][index] for index in accessor_ids]
    doc['materials'] = [doc['materials'][index] for index in material_ids]
    for node in doc.get('nodes', []):
        if 'mesh' in node:
            node['mesh'] = meshes[node['mesh']]
    for mesh in doc['meshes']:
        for primitive in mesh['primitives']:
            primitive['attributes'] = {name: accessor_map[index] for name, index in primitive.get('attributes', {}).items()}
            if 'indices' in primitive:
                primitive['indices'] = accessor_map[primitive['indices']]
            for target in primitive.get('targets', []):
                for name, index in target.items():
                    target[name] = accessor_map[index]
            if 'material' in primitive:
                primitive['material'] = material_map[primitive['material']]
    texture_ids = []
    for material in doc['materials']:
        if set(material.get('extensions', {})) - _SCALAR_MATERIAL_EXTENSIONS:
            raise ValueError('Material extensions need explicit reference remapping')
        texture_ids.extend(info['index'] for info in _texture_infos(material))
    texture_ids, texture_map = _indices(texture_ids, source.get('textures', []), 'texture')
    doc['textures'] = [doc['textures'][index] for index in texture_ids]
    image_ids, image_map = _indices([texture['source'] for texture in doc['textures']], source.get('images', []), 'image')
    sampler_ids, sampler_map = _indices([texture['sampler'] for texture in doc['textures'] if 'sampler' in texture], source.get('samplers', []), 'sampler')
    doc['images'] = [doc['images'][index] for index in image_ids]
    doc['samplers'] = [doc['samplers'][index] for index in sampler_ids]
    for material in doc['materials']:
        for info in _texture_infos(material):
            info['index'] = texture_map[info['index']]
    for texture in doc['textures']:
        if texture.get('extensions'):
            raise ValueError('Texture extensions need explicit reference remapping')
        texture['source'] = image_map[texture['source']]
        if 'sampler' in texture:
            texture['sampler'] = sampler_map[texture['sampler']]
    views = []
    for accessor in doc['accessors']:
        if accessor.get('extensions'):
            raise ValueError('Accessor extensions need explicit reference remapping')
        if 'bufferView' in accessor:
            views.append(accessor['bufferView'])
        for item in accessor.get('sparse', {}).values():
            if isinstance(item, dict) and 'bufferView' in item:
                views.append(item['bufferView'])
    for image in doc['images']:
        if image.get('uri') or image.get('extensions') or 'bufferView' not in image:
            raise ValueError('Expected an embedded core material image')
        views.append(image['bufferView'])
    view_ids, _ = _indices(views, source.get('bufferViews', []), 'binary view')
    packed = bytearray()
    new_views = []
    view_map = {}
    seen = {}
    for index in view_ids:
        view = copy.deepcopy(source['bufferViews'][index])
        if view.get('buffer', 0) != 0 or view.get('extensions'):
            raise ValueError('Unexpected binary view buffer/extension')
        start, length = view.get('byteOffset', 0), view['byteLength']
        if start < 0 or length < 0 or start + length > buffers[0]['byteLength']:
            raise ValueError('Binary view exceeds source buffer')
        payload = binary[start:start + length]
        descriptor = {key: value for key, value in view.items() if key not in ('buffer', 'byteOffset')}
        key = (json.dumps(descriptor, sort_keys=True), hashlib.sha256(payload).digest())
        if key in seen:
            previous, previous_payload = seen[key]
            if previous_payload != payload:
                raise ValueError('Binary payload hash collision')
            view_map[index] = previous
            continue
        packed.extend(b'\0' * (-len(packed) % 4))
        view.update(buffer=0, byteOffset=len(packed))
        packed.extend(payload)
        view_map[index] = len(new_views)
        seen[key] = (len(new_views), payload)
        new_views.append(view)
    for accessor in doc['accessors']:
        if 'bufferView' in accessor:
            accessor['bufferView'] = view_map[accessor['bufferView']]
        for item in accessor.get('sparse', {}).values():
            if isinstance(item, dict) and 'bufferView' in item:
                item['bufferView'] = view_map[item['bufferView']]
    for image in doc['images']:
        image['bufferView'] = view_map[image['bufferView']]
    doc['bufferViews'] = new_views
    doc['buffers'] = [copy.deepcopy(buffers[0])]
    doc['buffers'][0]['byteLength'] = len(packed)
    result = _encode(doc, bytes(packed))
    return result, dict(input_bytes=len(raw), output_bytes=len(result), bytes_removed=len(raw) - len(result),
        input_sha256=hashlib.sha256(raw).hexdigest(), output_sha256=hashlib.sha256(result).hexdigest(),
        resources={key: dict(before=len(source.get(key, [])), after=len(doc.get(key, [])))
                   for key in ('meshes', 'accessors', 'materials', 'textures', 'images', 'samplers', 'bufferViews')},
        source_indices=dict(meshes=mesh_ids, accessors=accessor_ids, materials=material_ids,
                            textures=texture_ids, images=image_ids, samplers=sampler_ids),
        referenced_binary_payloads_exact=True, referenced_json_values_exact_except_indices=True)


def resize_home_glb_textures(raw, caps_by_material):
    """Apply explicit material/role texture caps, then compact the new GLB.

    Rules are an insertion-ordered ``{material_name_or_glob: cap_or_roles}``.
    Later matching rules override earlier rules. An integer caps every role;
    a dict uses ``base/normal/arm/emissive`` keys (AO follows ARM). Omitted
    roles stay unchanged. A shared image keeps the largest cap required by
    *any* material referencing it; an uncapped reference preserves the image.
    Only images change: source geometry, PBR parameters and samplers remain
    exact. Existing authoring's Lanczos policy is used; normal sign is retained.
    No files or source objects are modified.
    """
    doc, binary = _read(bytes(raw))
    original = copy.deepcopy(doc)
    references = {}
    roles = {'baseColorTexture': 'base', 'metallicRoughnessTexture': 'arm',
             'normalTexture': 'normal', 'occlusionTexture': 'arm',
             'emissiveTexture': 'emissive'}
    used_materials = {primitive['material'] for mesh in doc.get('meshes', [])
                      for primitive in mesh['primitives'] if 'material' in primitive}
    for index in sorted(used_materials):
        material = doc['materials'][index]
        name = material.get('name', '')
        limits = {}
        for pattern, value in caps_by_material.items():
            if fnmatch.fnmatchcase(name, pattern):
                limits.update({role: value for role in ('base', 'normal', 'arm', 'emissive')}
                              if isinstance(value, int) else value)
        for parent_key, key in _MATERIAL_TEXTURES:
            parent = material if parent_key is None else material.get(parent_key, {})
            if key not in parent:
                continue
            role = roles[key]
            cap = limits.get(role)
            if cap is not None and (type(cap) is not int or cap < 64 or cap > 8192):
                raise ValueError('Texture caps must be integer pixels in64..8192')
            image = doc['textures'][parent[key]['index']]['source']
            references.setdefault(image, []).append(dict(material=name, role=role, cap=cap))
    packed = bytearray(binary)
    changes = []
    for index, uses in sorted(references.items()):
        if any(item['cap'] is None for item in uses):
            continue
        cap = max(item['cap'] for item in uses)
        image = doc['images'][index]
        if image.get('uri') or image.get('extensions') or 'bufferView' not in image:
            raise ValueError('Expected embedded core material image')
        view = doc['bufferViews'][image['bufferView']]
        start = view.get('byteOffset', 0)
        encoded = binary[start:start + view['byteLength']]
        with Image.open(io.BytesIO(encoded)) as source:
            source.load()
            before = source.size
            if max(before) <= cap:
                continue
            result = source.convert('RGBA' if 'A' in source.getbands() else 'RGB')
            result.thumbnail((cap, cap), Image.Resampling.LANCZOS)
            output = io.BytesIO()
            result.save(output, format='PNG', compress_level=6)
            resized = output.getvalue()
        packed.extend(b'\0' * (-len(packed) % 4))
        image['bufferView'] = len(doc['bufferViews'])
        image['mimeType'] = 'image/png'
        doc['bufferViews'].append(dict(buffer=0, byteOffset=len(packed), byteLength=len(resized)))
        packed.extend(resized)
        changes.append(dict(image=index, before=list(before), after=list(result.size),
                            source_sha256=hashlib.sha256(encoded).hexdigest(),
                            output_sha256=hashlib.sha256(resized).hexdigest(), uses=uses))
    doc['buffers'][0]['byteLength'] = len(packed)
    resized = _encode(doc, bytes(packed))
    compacted, report = compact_home_glb(resized)
    report.update(original_input_bytes=len(raw), original_input_sha256=hashlib.sha256(raw).hexdigest(),
                  texture_changes=changes, texture_caps=copy.deepcopy(caps_by_material),
                  referenced_geometry_bytes_exact=True, image_resample='Lanczos bounded cap; no normal Y/channel reinterpretation',
                  texture_resize_applied=bool(changes))
    return compacted, report


def split_opening_home_glb(raw):
    """Return five lossless art groups with fixed, tier-independent offsets.

    ``parts`` maps ``home`` and ``home_finward_{living,bedroom,kitchen,bathroom}``
    to embedded GLB bytes. ``report['render_meshes']`` supplies each ``stem``,
    ``pathstem`` and local ``offset``. Furniture positions subtract float32 .18
    from Y; world placement adds .18. All other vertex fields, indices, images,
    material values and samplers remain exact. Reconstruction error is measured
    in double precision against original float32 positions and must be <=1e-7m.
    This specific home is already a single baked identity mesh; unexpected
    hierarchies/transforms are rejected rather than implicitly transformed.
    """
    source, binary = _read(bytes(raw))
    if len(source.get('meshes', [])) != 1 or len(source.get('nodes', [])) != 1:
        raise ValueError('Opening splitter expects the single baked home mesh')
    node = source['nodes'][0]
    if node.get('mesh') != 0 or any(key in node for key in ('matrix', 'translation', 'rotation', 'scale', 'children', 'skin', 'weights')):
        raise ValueError('Opening splitter needs a baked identity node')
    labels = {
        'living-room couch': 'living', 'bedroom bed': 'bedroom',
        'bedside table': 'bedroom', 'bedside lamp': 'bedroom',
        'bathroom toilet': 'bathroom', 'kitchen sink cabinet': 'kitchen',
        'kitchen counter cabinet': 'kitchen', 'kitchen stove': 'kitchen',
        'kitchen refrigerator': 'kitchen',
        'kitchen upper cabinet 1': 'kitchen', 'kitchen upper cabinet 2': 'kitchen',
        'kitchen upper cabinet 3': 'kitchen',
    }
    groups = {name: [] for name in ('shell', 'living', 'bedroom', 'kitchen', 'bathroom')}
    primitives = source['meshes'][0]['primitives']
    for index, primitive in enumerate(primitives):
        name = source['materials'][primitive['material']].get('name', '')
        if name.startswith('Finward | '):
            fields = name.split(' | ')
            if len(fields) < 3 or fields[1] not in labels:
                raise ValueError('Unmapped purchased opening material: ' + name)
            group = labels[fields[1]]
        else:
            group = 'shell'
        groups[group].append(index)
    if any(not values for values in groups.values()):
        raise ValueError('A required opening split group is empty')

    def position_view(doc, payload, accessor_index):
        accessor = doc['accessors'][accessor_index]
        if accessor.get('sparse') or accessor.get('normalized') or accessor['componentType'] != 5126 or accessor['type'] != 'VEC3':
            raise ValueError('Opening positions require plain float32 VEC3')
        view = doc['bufferViews'][accessor['bufferView']]
        if view.get('buffer', 0) != 0:
            raise ValueError('Unexpected opening position buffer')
        return np.ndarray((accessor['count'], 3), dtype='<f4', buffer=payload,
                          offset=view.get('byteOffset', 0) + accessor.get('byteOffset', 0),
                          strides=(view.get('byteStride', 12), 4))

    parts = {}
    records = []
    for group, primitive_indices in groups.items():
        stem = 'home' if group == 'shell' else 'home_finward_' + group
        offset_y = 0. if group == 'shell' else .18
        doc = copy.deepcopy(source)
        doc['meshes'][0]['primitives'] = [doc['meshes'][0]['primitives'][index] for index in primitive_indices]
        payload = bytearray(binary)
        positions = {primitive['attributes']['POSITION'] for primitive in doc['meshes'][0]['primitives']}
        original_points = []
        output_points = []
        changed_views = set()
        for accessor in sorted(positions):
            before = position_view(source, binary, accessor).copy()
            target = position_view(doc, payload, accessor)
            # A position accessor may be shared across primitives. Update each
            # actual position stream only once, even under duplicate accessors.
            item = doc['accessors'][accessor]
            view = doc['bufferViews'][item['bufferView']]
            identity = (item['bufferView'], item.get('byteOffset', 0), item['count'], view.get('byteStride', 12))
            if identity not in changed_views:
                target[:, 1] -= np.float32(offset_y)
                changed_views.add(identity)
            original_points.append(before)
            output_points.append(target.copy())
        original_points = np.concatenate(original_points)
        output_points = np.concatenate(output_points)
        reconstructed = output_points.astype(np.float64) + np.array([0., offset_y, 0.])
        error = float(np.max(np.abs(reconstructed - original_points.astype(np.float64))))
        if error > 1e-7 or not np.array_equal(original_points[:, [0, 2]], output_points[:, [0, 2]]):
            raise ValueError('Opening split exceeds exact world reconstruction tolerance')
        compacted, compaction = compact_home_glb(_encode(doc, bytes(payload)))
        parts[stem] = compacted
        records.append(dict(group=group, stem=stem, pathstem=stem, offset=[0., offset_y, 0.],
                            source_minimum=original_points.min(0).tolist(),
                            source_maximum=original_points.max(0).tolist(),
                            minimum=output_points.min(0).tolist(), maximum=output_points.max(0).tolist(),
                            world_reconstruction_max_error_m=error,
                            source_primitive_indices=primitive_indices,
                            primitives=len(primitive_indices), bytes=len(compacted),
                            sha256=hashlib.sha256(compacted).hexdigest(), compaction=compaction))
    return parts, dict(input_bytes=len(raw), input_sha256=hashlib.sha256(raw).hexdigest(),
                       output_bytes=sum(len(value) for value in parts.values()),
                       render_meshes=records, source_primitives=len(primitives),
                       all_primitives_preserved_once=sum(len(values) for values in groups.values()) == len(primitives),
                       fixed_offsets_shared_by_near_mid=True,
                       images_materials_normals_uv_indices_exact=True,
                       position_policy='Only float32 furniture Y minus .18, with canonical instance offset plus .18; shell unchanged')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--report', type=Path)
    args = parser.parse_args()
    if args.output.exists() or args.output.is_symlink():
        raise ValueError('Choose a NEW compacted output file')
    compacted, report = compact_home_glb(args.source.read_bytes())
    with args.output.open('xb') as stream:
        stream.write(compacted)
    if args.report:
        with args.report.open('x') as stream:
            json.dump(report, stream, indent=2)
            stream.write('\n')
    print(json.dumps({key: value for key, value in report.items() if key != 'source_indices'}, indent=2))


if __name__ == '__main__':
    main()
