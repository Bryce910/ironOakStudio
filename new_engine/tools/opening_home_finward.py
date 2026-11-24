#!/usr/bin/env python3
"""Place reviewed, cooked Finward furniture in an opening GLB accumulator.

The caller supplies a *static baked GLB*, not an original FBX or Unity prefab.
The upstream cook must resolve the original Unity material references, convert
active URP Lit maps to native ARM (R=AO, G=roughness, B=metallic), choose the source LOD,
and bake all hierarchy/unit transforms. This importer does not guess channels,
replace material factors, add collision, or edit any source/project file.

``import_finward(g, path, x, z, width=2.7, yaw=180)`` uniformly scales the
source to the requested width. Further dimensions override their own axes;
unspecified axes follow the first supplied dimension (width, depth, height).
Alternatively ``axis_scale=(sx, sy, sz)`` accepts an explicit signed scale;
negative axes mirror geometry with corrected triangle winding. All placements
are centered in X/Z, grounded at ``y``, then rotated around Y by ``yaw``.

The accumulator is the existing ``build_opening_prologue.Glb``. Its texture
cache deduplicates embedded images. The default cap is 1K; a caller may opt
selected close-up props into 2K. Review the fitted silhouette, actual surface
contact and the existing collider separately before accepting a replacement.
"""
from __future__ import annotations

import copy
import hashlib
import math
from pathlib import Path

import numpy as np

from build_opening_prologue import array, read_glb


_TEXTURES = (
    ('pbrMetallicRoughness', 'baseColorTexture'),
    ('pbrMetallicRoughness', 'metallicRoughnessTexture'),
    (None, 'normalTexture'),
    (None, 'occlusionTexture'),
    (None, 'emissiveTexture'),
)


def _identity_node(node):
    if node.get('skin') is not None or node.get('weights'):
        raise ValueError('Furniture must be baked static geometry without skin/morph weights')
    if node.get('children'):
        raise ValueError('Bake the source node hierarchy before placing furniture')
    for key, expected in (
        ('translation', [0., 0., 0.]),
        ('rotation', [0., 0., 0., 1.]),
        ('scale', [1., 1., 1.]),
        ('matrix', np.eye(4).reshape(-1).tolist()),
    ):
        if key in node:
            value = np.asarray(node[key], dtype=float)
            if value.shape != np.shape(expected) or not np.isfinite(value).all() or not np.allclose(value, expected, atol=1e-8, rtol=0):
                raise ValueError('Bake the source node ' + key + ' before placing furniture')
    if node.get('extensions'):
        raise ValueError('Furniture node extensions require an explicit upstream bake')


def _accessor(doc, binary, index, kind, *, indices=False):
    item = doc['accessors'][index]
    if item.get('sparse') or item.get('normalized') or item['type'] != kind:
        raise ValueError('Furniture needs ordinary, unnormalized baked ' + kind + ' accessors')
    allowed = (5121, 5123, 5125) if indices else (5126,)
    if item['componentType'] not in allowed:
        raise ValueError('Unexpected furniture accessor component type')
    view = doc['bufferViews'][item['bufferView']]
    if view.get('buffer', 0) != 0:
        raise ValueError('Furniture must use one embedded GLB buffer')
    result = array(doc, binary, index)
    if not np.isfinite(result).all():
        raise ValueError('Nonfinite furniture geometry')
    return result


def _static_parts(doc, binary):
    if any(doc.get(key) for key in ('skins', 'animations', 'extensionsRequired', 'extensionsUsed')):
        raise ValueError('Cook furniture to core static glTF without skins, animations or extensions')
    if len(doc.get('buffers', [])) != 1 or doc['buffers'][0].get('uri'):
        raise ValueError('Furniture requires one embedded GLB buffer')
    nodes = doc.get('nodes', [])
    for node in nodes:
        _identity_node(node)
    scenes = doc.get('scenes', [])
    scene_index = doc.get('scene', 0)
    if not scenes or not 0 <= scene_index < len(scenes):
        raise ValueError('Furniture has no valid source scene')
    selected = scenes[scene_index].get('nodes', [])
    if len(set(selected)) != len(selected):
        raise ValueError('Duplicate node in furniture source scene')
    parts = []
    for index in selected:
        node = nodes[index]
        if 'mesh' not in node:
            continue
        mesh = doc['meshes'][node['mesh']]
        if mesh.get('weights'):
            raise ValueError('Bake furniture morph weights before placement')
        for primitive in mesh['primitives']:
            if primitive.get('mode', 4) != 4 or primitive.get('targets') or primitive.get('extensions'):
                raise ValueError('Furniture needs static triangle primitives')
            attributes = primitive['attributes']
            required = ('POSITION', 'NORMAL', 'TEXCOORD_0')
            if any(key not in attributes for key in required) or 'indices' not in primitive:
                raise ValueError('Furniture primitive needs positions, normals, UV0 and indices')
            # The accumulator generates tangent frames from UV0. Other vertex
            # attributes must be resolved upstream rather than silently lost.
            extra = set(attributes).difference((*required, 'TANGENT'))
            if extra:
                raise ValueError('Resolve unsupported furniture attributes upstream: ' + ', '.join(sorted(extra)))
            position = _accessor(doc, binary, attributes['POSITION'], 'VEC3')
            normal = _accessor(doc, binary, attributes['NORMAL'], 'VEC3')
            uv = _accessor(doc, binary, attributes['TEXCOORD_0'], 'VEC2')
            indices = _accessor(doc, binary, primitive['indices'], 'SCALAR', indices=True).reshape(-1)
            if not len(position) or len(position) != len(normal) or len(position) != len(uv):
                raise ValueError('Invalid furniture vertex arrays')
            if not len(indices) or len(indices) % 3 or indices.max() >= len(position):
                raise ValueError('Invalid furniture triangle indices')
            if np.any(np.linalg.norm(normal, axis=1) < 1e-8):
                raise ValueError('Zero furniture vertex normal')
            material = primitive.get('material')
            if material is None or not 0 <= material < len(doc.get('materials', [])):
                raise ValueError('Furniture primitive needs an explicit cooked material')
            parts.append((position, normal, uv, indices, material))
    if not parts:
        raise ValueError('Furniture source scene has no static triangle geometry')
    return parts


def _copy_materials(g, doc, binary, selected, texture_size, label):
    textures = {}

    def texture(index):
        if index in textures:
            return textures[index]
        original = doc['textures'][index]
        if original.get('extensions'):
            raise ValueError('Bake furniture texture extensions before placement')
        image = doc['images'][original['source']]
        if image.get('uri') or image.get('mimeType') not in ('image/png', 'image/jpeg'):
            raise ValueError('Furniture textures must be embedded PNG/JPEG')
        view = doc['bufferViews'][image['bufferView']]
        if view.get('buffer', 0) != 0:
            raise ValueError('Furniture image is outside the embedded buffer')
        start, length = view.get('byteOffset', 0), view['byteLength']
        raw = binary[start:start + length]
        if len(raw) != length:
            raise ValueError('Truncated furniture image')
        embedded = g.texture(raw, size=texture_size)
        # g.texture owns image deduplication; retain the source sampler on a
        # texture referencing that cached image rather than copying PNG bytes.
        sampler = copy.deepcopy(doc.get('samplers', [])[original['sampler']]) if 'sampler' in original else {}
        samplers = g.doc.setdefault('samplers', [])
        if sampler not in samplers:
            samplers.append(sampler)
        desired = dict(source=g.doc['textures'][embedded]['source'], sampler=samplers.index(sampler))
        if desired not in g.doc['textures']:
            g.doc['textures'].append(desired)
        textures[index] = g.doc['textures'].index(desired)
        return textures[index]

    materials = {}
    for index in sorted(selected):
        material = copy.deepcopy(doc['materials'][index])
        if material.get('extensions'):
            raise ValueError('Bake furniture material extensions to core PBR before placement')
        source_name = material.get('name', 'material %d' % index)
        material['name'] = 'Finward | ' + label + ' | ' + source_name
        for parent_key, key in _TEXTURES:
            parent = material if parent_key is None else material.get(parent_key, {})
            info = parent.get(key)
            if info is None:
                continue
            if info.get('texCoord', 0) != 0 or info.get('extensions'):
                raise ValueError('Bake furniture texture transforms/alternate UV sets into UV0')
            info['index'] = texture(info['index'])
        # Scalars, colours, normal scale, AO strength, emissive factor and
        # alpha mode/cutoff/double-sided flag remain byte-for-byte values.
        g.doc['materials'].append(material)
        materials[index] = len(g.doc['materials']) - 1
    return materials, textures


def import_finward(g, path, x, z, *, y=.18, width=None, depth=None,
                   height=None, yaw=0., axis_scale=None, texture_size=1024,
                   label=None):
    """Append a reviewed static furniture GLB and return placement provenance.

    ``width/depth/height`` describe the source's local dimensions before yaw,
    not its rotated world AABB. Supplying one keeps its original proportions.
    Additional dimensions deliberately introduce nonuniform scale. The signed
    ``axis_scale`` alternative is mutually exclusive with target dimensions.
    Normal vectors use the inverse scale and are renormalized; odd reflections
    reverse triangle winding. Nothing is written until the caller finishes its
    existing accumulator. Source native-ARM semantics are never inferred here.
    """
    path = Path(path).resolve(strict=True)
    if texture_size not in (128, 256, 512, 1024, 2048):
        raise ValueError('Use a bounded 128/256/512/1024/2048 texture cap')
    placement = np.asarray([x, y, z, yaw], dtype=float)
    if not np.isfinite(placement).all():
        raise ValueError('Furniture placement must be finite')
    doc, binary = read_glb(path)
    parts = _static_parts(doc, binary)
    points = np.concatenate([part[0] for part in parts])
    low, high = points.min(0), points.max(0)
    dimensions = high - low
    if np.any(dimensions < 1e-8):
        raise ValueError('Furniture needs nonzero source dimensions')
    targets = [(0, width), (2, depth), (1, height)]
    chosen = [(axis, float(value)) for axis, value in targets if value is not None]
    if axis_scale is not None:
        if chosen:
            raise ValueError('Choose target dimensions or explicit axis_scale')
        scale = np.asarray(axis_scale, dtype=float)
        if scale.shape != (3,) or not np.isfinite(scale).all() or np.any(abs(scale) < 1e-8):
            raise ValueError('axis_scale needs three finite nonzero values')
    else:
        if any(not math.isfinite(value) or value <= 0 for _, value in chosen):
            raise ValueError('Furniture target dimensions must be positive and finite')
        uniform = chosen[0][1] / dimensions[chosen[0][0]] if chosen else 1.
        scale = np.full(3, uniform)
        for axis, value in chosen:
            scale[axis] = value / dimensions[axis]
    scaled = points * scale
    minimum, maximum = scaled.min(0), scaled.max(0)
    shift = np.array([-(minimum[0]+maximum[0])*.5, -minimum[1], -(minimum[2]+maximum[2])*.5])
    angle = math.radians(yaw)
    c, s = math.cos(angle), math.sin(angle)
    rotation = np.array([[c, 0., s], [0., 1., 0.], [-s, 0., c]])
    origin = np.asarray([x, y, z], dtype=float)
    mirrored = bool(np.prod(scale) < 0)
    material_map, texture_map = _copy_materials(
        g, doc, binary, {part[4] for part in parts}, texture_size,
        str(label) if label is not None else path.stem)
    output = []
    triangles = 0
    for position, normal, uv, indices, material in parts:
        placed = (position * scale + shift) @ rotation.T + origin
        transformed_normal = (normal / scale) @ rotation.T
        transformed_normal /= np.linalg.norm(transformed_normal, axis=1, keepdims=True)
        indices = indices.copy()
        if mirrored:
            faces = indices.reshape(-1, 3)
            faces[:, [1, 2]] = faces[:, [2, 1]]
        g.mesh(placed, transformed_normal, uv, indices, material_map[material])
        output.append(placed)
        triangles += len(indices) // 3
    output = np.concatenate(output)
    return dict(source=str(path), sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
                position=origin.tolist(), yaw=float(yaw), axis_scale=scale.tolist(),
                source_minimum=low.tolist(), source_maximum=high.tolist(),
                source_dimensions=dimensions.tolist(), minimum=output.min(0).tolist(),
                maximum=output.max(0).tolist(), dimensions=(output.max(0)-output.min(0)).tolist(),
                triangles=triangles, primitives=len(parts), mirrored_winding_corrected=mirrored,
                material_indices={str(k): v for k, v in material_map.items()},
                texture_indices={str(k): v for k, v in texture_map.items()},
                texture_size_cap=texture_size, source_geometry_topology_preserved=True,
                material_policy='Upstream cooked native ARM; original PBR factors, normal/emissive/alpha settings preserved; no roughness/metallic override',
                collision_bodies_added=0)
