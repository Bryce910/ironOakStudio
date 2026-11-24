#!/usr/bin/env python3
"""Close the purchased display-room cutaways without moving source geometry.

The supplied room floor union is expanded by .25m before the .14m wall ring
is placed entirely outside it. The extra floor/roof follow that stepped union,
not the scene's large rectangular bounds. Core PBR material/image payloads are
copied exactly from the frozen scene cook. New surfaces receive an approximate
spatial interpolation of the publisher's original SH probes; existing source
lightmaps and exact per-vertex probe samples remain untouched.

This module is CPU-only. It returns bytes and provenance, and never edits the
input project, cook, source package, runtime, or renderer.
"""
from __future__ import annotations

import copy
import hashlib
import json
import math
from pathlib import Path
import re

import numpy as np
from scipy.spatial import cKDTree

from build_opening_prologue import read_glb
from cook_finward_scene import SceneGlb


ROOT = Path(__file__).resolve().parents[1]
EVIDENCE = ROOT/'docs/opening-finward-reference/2026-11-03'
FLOOR_REPORT = EVIDENCE/'SOURCE_ENVELOPE_REVIEW.json'
PROBE_REPORT = EVIDENCE/'LIGHTPROBE_REPORT.json'
EXPANSION = .25
WALL_THICKNESS = .14
FLOOR_TOP = -.015
FLOOR_BOTTOM = FLOOR_TOP-.14
WALL_TOP = 2.7
CEILING_BOTTOM = 2.62
CEILING_TOP = CEILING_BOTTOM+.14
FACE_STEP = .5
TEXTURES = (('pbrMetallicRoughness', 'baseColorTexture'),
            ('pbrMetallicRoughness', 'metallicRoughnessTexture'),
            (None, 'normalTexture'), (None, 'occlusionTexture'),
            (None, 'emissiveTexture'))


def _sha(raw):
    return hashlib.sha256(raw).hexdigest()


def _grid_union(rectangles):
    """Exact finite orthogonal union, including any holes, using cell occupancy."""
    inner = np.asarray(rectangles, dtype=float).copy()
    outer = inner.copy()
    for data, amount in ((inner, EXPANSION), (outer, EXPANSION+WALL_THICKNESS)):
        data[:, :2] -= amount
        data[:, 2:] += amount
    xs = np.unique(np.round(np.concatenate((inner[:, [0, 2]].ravel(), outer[:, [0, 2]].ravel())), 9))
    zs = np.unique(np.round(np.concatenate((inner[:, [1, 3]].ravel(), outer[:, [1, 3]].ravel())), 9))
    xx, zz = np.meshgrid((xs[:-1]+xs[1:])*.5, (zs[:-1]+zs[1:])*.5)

    def occupied(rects):
        return np.any((xx[None] > rects[:, 0, None, None]) &
                      (xx[None] < rects[:, 2, None, None]) &
                      (zz[None] > rects[:, 1, None, None]) &
                      (zz[None] < rects[:, 3, None, None]), axis=0)

    inside, outside = occupied(inner), occupied(outer)
    if np.any(inside & ~outside):
        raise ValueError('Enclosure outer union must contain the inner union')
    return xs, zs, inside, outside


def _rectangles(mask, xs, zs):
    """Merge equal horizontal runs over consecutive rows; cells never overlap."""
    active, result = {}, []
    for row in range(len(zs)-1):
        padded = np.r_[False, mask[row], False].astype(np.int8)
        changes = np.diff(padded)
        runs = list(zip(np.flatnonzero(changes == 1), np.flatnonzero(changes == -1)))
        current = {}
        for left, right in runs:
            key = (int(left), int(right))
            current[key] = active.pop(key, [float(xs[left]), float(zs[row]), float(xs[right]), float(zs[row])])
            current[key][3] = float(zs[row+1])
        result.extend(active.values())
        active = current
    return result+list(active.values())


def _boundary(mask, xs, zs):
    segments = {}

    def edge(axis, coordinate, sign, a, b):
        segments.setdefault((axis, float(coordinate), sign), []).append((float(a), float(b)))

    rows, columns = mask.shape
    for z, x in zip(*np.nonzero(mask)):
        if x == 0 or not mask[z, x-1]: edge('x', xs[x], -1, zs[z], zs[z+1])
        if x == columns-1 or not mask[z, x+1]: edge('x', xs[x+1], 1, zs[z], zs[z+1])
        if z == 0 or not mask[z-1, x]: edge('z', zs[z], -1, xs[x], xs[x+1])
        if z == rows-1 or not mask[z+1, x]: edge('z', zs[z+1], 1, xs[x], xs[x+1])
    result = []
    for (axis, coordinate, sign), spans in sorted(segments.items()):
        start = end = None
        for a, b in sorted(spans):
            if end is not None and abs(a-end) < 1e-8:
                end = b
            else:
                if end is not None:
                    result.append(dict(axis=axis, coordinate=coordinate, outward_sign=sign, start=start, end=end))
                start, end = a, b
        result.append(dict(axis=axis, coordinate=coordinate, outward_sign=sign, start=start, end=end))
    return result


def _basis(normals):
    x, y, z = np.asarray(normals, dtype=float).T
    return np.stack((np.ones(len(x)), y, z, x, x*y, y*z, 3*z*z-1, x*z, x*x-y*y), axis=1)


class _ProbeField:
    def __init__(self, path):
        raw = path.read_bytes()
        data = json.loads(raw)
        if data.get('gpu_used') or data.get('scene_saved') or data['probe_count'] != data['baked_probe_count']:
            raise ValueError('Enclosure lighting needs the frozen CPU source-probe report')
        self.positions = np.array([[p['position'][k] for k in 'xyz'] for p in data['probes']], dtype=float)
        self.coefficients = np.array([p['sh'] for p in data['probes']], dtype=float).reshape(-1, 3, 9)
        if len(self.positions) < 8 or not np.isfinite(self.positions).all() or not np.isfinite(self.coefficients).all():
            raise ValueError('Invalid source-probe field')
        self.tree = cKDTree(self.positions)
        error, evaluations = 0., 0
        for item in data['unlightmapped_renderers']:
            directions = np.array([[v[k] for k in 'xyz'] for v in item['evaluation_directions']])
            reference = np.array([[v[k] for k in 'xyz'] for v in item['evaluated_linear_rgb']])
            computed = np.maximum(_basis(directions) @ np.asarray(item['interpolated_sh']).reshape(3, 9).T, 0)
            error = max(error, float(np.max(np.abs(computed-reference))))
            evaluations += len(reference)
        if error > 2e-6 or evaluations < 6:
            raise ValueError('SH basis differs from exact source Unity evaluation')
        self.report = dict(source=str(path), source_sha256=_sha(raw), probes=len(self.positions),
            exact_basis_reference_evaluations=evaluations, exact_basis_max_abs_error=error,
            spatial_method='8 nearest original source probes, inverse-square distance weighting; .08m distance floor',
            basis='[1,y,z,x,xy,yz,3z*z-1,xz,x*x-y*y], channel-major source coefficients',
            coordinates='Engine native Z reflected back to original Unity for positions and normals',
            scope='Approximate new-surface spatial irradiance; existing source bakes and exact source renderer samples are unchanged')

    def sample(self, position, normal):
        point, direction = np.asarray(position, dtype=float).copy(), np.asarray(normal, dtype=float).copy()
        point[:, 2] *= -1
        direction[:, 2] *= -1
        distance, index = self.tree.query(point, k=8)
        weight = 1/np.maximum(distance, .08)**2
        weight /= weight.sum(axis=1, keepdims=True)
        sh = np.einsum('nk,nkci->nci', weight, self.coefficients[index])
        rgb = np.maximum(np.einsum('ni,nci->nc', _basis(direction), sh), 0)
        if not np.isfinite(rgb).all() or np.any(rgb > 65504):
            raise ValueError('Invalid interpolated source irradiance')
        return np.column_stack((rgb, np.ones(len(rgb)))).astype('<f4')


def _copy_source_materials(g, project, cooked):
    wanted = {'wall': 'Finward Wall_default', 'ceiling': 'Finward Ceiling_default', 'floor': 'Finward Floor_hardwood'}
    result, provenance, image_cache, texture_cache = {}, {}, {}, {}
    catalog = json.loads((cooked/'catalog.json').read_bytes())
    for number, item in enumerate(catalog):
        record = next(m for m in item['models'] if m['lod'] == 0)
        relative = 'models/opening/'+('home' if number == 0 else item['name'])+'.glb'
        path = project/relative
        raw = path.read_bytes()
        if _sha(raw) != record['sha256']:
            raise ValueError('Enclosure source material model differs from frozen scene cook: '+relative)
        doc, binary = read_glb(path)
        for material_index, original in enumerate(doc.get('materials', [])):
            roles = [role for role, name in wanted.items() if name == original.get('name') and role not in result]
            if not roles:
                continue
            if original.get('extensions'):
                raise ValueError('Enclosure source material requires explicit extension support')
            material = copy.deepcopy(original)
            images = []
            for parent_key, key in TEXTURES:
                parent = material if parent_key is None else material.get(parent_key, {})
                info = parent.get(key)
                if info is None:
                    continue
                if info.get('extensions') or info.get('texCoord', 0) != 0:
                    raise ValueError('Enclosure source material must use baked core UV0')
                texture = doc['textures'][info['index']]
                if texture.get('extensions'):
                    raise ValueError('Unexpected source texture extension')
                image = doc['images'][texture['source']]
                if image.get('uri') or image.get('mimeType') not in ('image/png', 'image/jpeg'):
                    raise ValueError('Source material needs embedded PNG/JPEG')
                view = doc['bufferViews'][image['bufferView']]
                start = view.get('byteOffset', 0)
                payload = bytes(binary[start:start+view['byteLength']])
                if len(payload) != view['byteLength']:
                    raise ValueError('Truncated source texture')
                digest = _sha(payload)
                image_key = (digest, image['mimeType'])
                if image_key not in image_cache:
                    target = copy.deepcopy(image)
                    target['bufferView'] = g.view(payload)
                    g.doc['images'].append(target)
                    image_cache[image_key] = len(g.doc['images'])-1
                sampler = copy.deepcopy(doc['samplers'][texture['sampler']]) if 'sampler' in texture else None
                texture_key = (image_key, json.dumps(sampler, sort_keys=True))
                if texture_key not in texture_cache:
                    target = copy.deepcopy(texture)
                    target['source'] = image_cache[image_key]
                    if sampler is not None:
                        if sampler not in g.doc['samplers']:
                            g.doc['samplers'].append(sampler)
                        target['sampler'] = g.doc['samplers'].index(sampler)
                    g.doc['textures'].append(target)
                    texture_cache[texture_key] = len(g.doc['textures'])-1
                info['index'] = texture_cache[texture_key]
                images.append(dict(role=key, sha256=digest, bytes=len(payload), mime_type=image['mimeType']))
            g.doc['materials'].append(material)
            result[roles[0]] = len(g.doc['materials'])-1
            provenance[roles[0]] = dict(source_model=relative, source_model_sha256=_sha(raw),
                source_material_index=material_index, source_material_name=original['name'],
                source_material=original, image_payloads=images,
                preservation='Exact core PBR values, source sampler descriptors, and encoded image payloads; only texture/image indices remapped')
        if set(result) == set(wanted):
            return result, provenance
    raise ValueError('Source scene cook is missing enclosure PBR materials: '+str(set(wanted)-set(result)))


def _horizontal_faces(rectangles, y, sign, role):
    for xmin, zmin, xmax, zmax in rectangles:
        yield (np.array([xmin, y, zmin]), np.array([xmax-xmin, 0., 0.]),
               np.array([0., 0., zmax-zmin]), np.array([0., sign, 0.]), role)


def _vertical_faces(edges, bottom, top, role):
    for edge in edges:
        axis, fixed, sign, start, end = [edge[k] for k in ('axis', 'coordinate', 'outward_sign', 'start', 'end')]
        if axis == 'x':
            yield (np.array([fixed, bottom, start]), np.array([0., 0., end-start]),
                   np.array([0., top-bottom, 0.]), np.array([sign, 0., 0.]), role)
        else:
            yield (np.array([start, bottom, fixed]), np.array([end-start, 0., 0.]),
                   np.array([0., top-bottom, 0.]), np.array([0., 0., sign]), role)


def _face_arrays(origin, along, up, normal, step):
    width, height = np.linalg.norm(along), np.linalg.norm(up)
    nu, nv = max(1, math.ceil(width/step)), max(1, math.ceil(height/step))
    u, v = np.meshgrid(np.linspace(0, 1, nu+1), np.linspace(0, 1, nv+1))
    position = origin+u.reshape(-1, 1)*along+v.reshape(-1, 1)*up
    normals = np.broadcast_to(normal, position.shape).copy()
    uv = np.column_stack((u.ravel()*width/3., v.ravel()*height/3.))
    left = (np.arange(nv)[:, None]*(nu+1)+np.arange(nu)).ravel()
    indices = np.column_stack((left, left+1, left+nu+2, left, left+nu+2, left+nu+1)).reshape(-1, 3)
    if np.dot(np.cross(along, up), normal) < 0:
        indices = indices[:, [0, 2, 1]]
    return position, normals, uv, indices


def build_enclosure(project, cooked):
    """Return a minY=0 static GLB and stable, project-independent provenance."""
    project, cooked = Path(project), Path(cooked)
    floor_raw = FLOOR_REPORT.read_bytes()
    source = json.loads(floor_raw)
    rectangles = np.asarray(source['floor_rectangles_native'], dtype=float)
    if rectangles.ndim != 2 or rectangles.shape[1] != 4 or not np.isfinite(rectangles).all() or np.any(rectangles[:, 2:] <= rectangles[:, :2]):
        raise ValueError('Invalid source floor rectangles')
    xs, zs, inner, outer = _grid_union(rectangles)
    inner_edges, outer_edges = _boundary(inner, xs, zs), _boundary(outer, xs, zs)
    ring = outer & ~inner
    outer_rectangles, ring_rectangles = _rectangles(outer, xs, zs), _rectangles(ring, xs, zs)
    wall_edges = _boundary(ring, xs, zs)
    # Continue walls to the slab underside: starting at the old floor's Y=0
    # would leave a .015m light leak above the new, slightly recessed floor.
    faces = list(_vertical_faces(wall_edges, FLOOR_BOTTOM, WALL_TOP, 'wall'))
    faces += list(_horizontal_faces(ring_rectangles, FLOOR_BOTTOM, -1, 'wall'))
    faces += list(_horizontal_faces(ring_rectangles, WALL_TOP, 1, 'wall'))
    for bottom, top, role in ((FLOOR_BOTTOM, FLOOR_TOP, 'floor'), (CEILING_BOTTOM, CEILING_TOP, 'ceiling')):
        faces += list(_vertical_faces(outer_edges, bottom, top, role))
        faces += list(_horizontal_faces(outer_rectangles, bottom, -1, role))
        faces += list(_horizontal_faces(outer_rectangles, top, 1, role))
    g = SceneGlb()
    materials, provenance = _copy_source_materials(g, project, cooked)
    field = _ProbeField(PROBE_REPORT)
    collision_positions, collision_indices = [], []
    native_bounds = [np.full(3, np.inf), np.full(3, -np.inf)]
    vertex_count = triangle_count = 0
    lighting_min, lighting_max = np.full(3, np.inf), np.full(3, -np.inf)
    for origin, along, up, normal, role in faces:
        position, normals, uv, index = _face_arrays(origin, along, up, normal, FACE_STEP)
        light = field.sample(position, normals)
        lighting_min = np.minimum(lighting_min, light[:, :3].min(0))
        lighting_max = np.maximum(lighting_max, light[:, :3].max(0))
        native_bounds[0] = np.minimum(native_bounds[0], position.min(0))
        native_bounds[1] = np.maximum(native_bounds[1], position.max(0))
        position[:, 1] -= FLOOR_BOTTOM
        zeros = np.zeros_like(uv)
        g.scene_mesh(materials[role], position, normals, uv, zeros, zeros, index, light)
        vertex_count += len(position)
        triangle_count += len(index)
        # Physics uses identical planar surfaces with no unnecessary SH grid.
        coarse, _, _, triangles = _face_arrays(origin, along, up, normal, math.inf)
        start = len(collision_positions)
        collision_positions.extend(coarse.tolist())
        collision_indices.extend((triangles+start).tolist())
    raw = g.finish()
    # SceneGlb's generic label describes source bakes; replace it with accurate
    # provenance for this newly added, approximated closure geometry.
    doc, binary = _decode(raw)
    doc['asset']['extras'] = dict(source_geometry='New stepped enclosure around the supplied floor union; original source geometry unchanged',
        source_lighting='New surfaces: interpolated original source SH irradiance in _VALWOOD_BAKED_LIGHT RGBA, A=1',
        source_materials='Exact source core PBR and encoded texture payloads')
    doc['meshes'][0]['name'] = 'Stepped exterior closure behind purchased display rooms'
    doc['nodes'][0]['name'] = 'Enclosure grounded at zero; place using offset_native'
    raw = _encode(doc, binary)
    area = lambda mask: float(np.sum(np.outer(np.diff(zs), np.diff(xs))*mask))
    report = dict(schema='valwood.finward-reference-enclosure', version=1,
        source_floor_report=str(FLOOR_REPORT), source_floor_report_sha256=_sha(floor_raw),
        cook_root=str(cooked.resolve()), cook_review_sha256=_sha((cooked/'COOK_REVIEW.json').read_bytes()),
        source_preserved=True, gpu_used=False,
        expansion_m=EXPANSION, wall_thickness_m=WALL_THICKNESS, wall_thickness_direction='Entirely outward from inner outline',
        floor_top_native=FLOOR_TOP, floor_bottom_native=FLOOR_BOTTOM,
        ceiling_underside_native=CEILING_BOTTOM, ceiling_top_native=CEILING_TOP,
        wall_bottom_native=FLOOR_BOTTOM, wall_top_native=WALL_TOP,
        source_floor_rectangles_native=rectangles.tolist(), inner_edges_native=inner_edges, outer_edges_native=outer_edges,
        inner_area_m2=area(inner), outer_area_m2=area(outer), wall_ring_area_m2=area(ring),
        outer_cover_rectangles_native=outer_rectangles, wall_ring_rectangles_native=ring_rectangles,
        offset_native=[0., FLOOR_BOTTOM, 0.], native_minimum=native_bounds[0].tolist(), native_maximum=native_bounds[1].tolist(),
        model_minimum=(native_bounds[0]-[0., FLOOR_BOTTOM, 0.]).tolist(),
        model_maximum=(native_bounds[1]-[0., FLOOR_BOTTOM, 0.]).tolist(),
        bytes=len(raw), sha256=_sha(raw), vertices=vertex_count, triangles=triangle_count,
        materials=provenance, lighting=dict(field.report, minimum_rgb=lighting_min.tolist(), maximum_rgb=lighting_max.tolist()),
        uv_world_metres_per_repeat=3., render_face_max_step_m=FACE_STEP,
        collision_bodies_added=1, collision_shape='Static triangle mesh; same planar surfaces as rendered enclosure',
        collision_vertices_native=collision_positions, collision_triangles=collision_indices)
    return raw, report


def _decode(raw):
    import struct
    magic, version, length = struct.unpack_from('<III', raw)
    if (magic, version, length) != (0x46546c67, 2, len(raw)):
        raise ValueError('Invalid GLB')
    length, kind = struct.unpack_from('<II', raw, 12)
    if kind != 0x4e4f534a:
        raise ValueError('Invalid GLB JSON chunk')
    doc = json.loads(raw[20:20+length])
    count, kind = struct.unpack_from('<II', raw, 20+length)
    if kind != 0x004e4942:
        raise ValueError('Invalid GLB binary chunk')
    return doc, raw[28+length:28+length+count]


def _encode(doc, binary):
    import struct
    text = json.dumps(doc, separators=(',', ':'), allow_nan=False).encode()
    text += b' '*(-len(text) % 4)
    binary += b'\0'*(-len(binary) % 4)
    return (struct.pack('<III', 0x46546c67, 2, 28+len(text)+len(binary))+
            struct.pack('<II', len(text), 0x4e4f534a)+text+
            struct.pack('<II', len(binary), 0x004e4942)+binary)


def append_enclosure_physics(raw, report, origin_native):
    """Append one static, world-positioned mesh body; preserve original bytes."""
    if not raw.startswith(b'VALWOOD_PHYSICS 1\n') or len(raw) > 16*1024*1024:
        raise ValueError('Unexpected source physics document')
    body_ids = [int(v) for v in re.findall(rb'^body\s+(\d+)\s', raw, flags=re.M)]
    if not body_ids or len(body_ids) >= 512 or len(set(body_ids)) != len(body_ids):
        raise ValueError('Invalid source physics body IDs or body budget')
    origin = np.asarray(origin_native, dtype=float)
    positions = np.asarray(report['collision_vertices_native'], dtype=float)
    triangles = np.asarray(report['collision_triangles'], dtype=np.int64)
    if (origin.shape != (3,) or positions.ndim != 2 or positions.shape[1] != 3 or
            triangles.ndim != 2 or triangles.shape[1] != 3 or not np.isfinite(origin).all() or
            not np.isfinite(positions).all() or not len(triangles) or triangles.min() < 0 or triangles.max() >= len(positions)):
        raise ValueError('Invalid enclosure collision geometry')
    if report['collision_bodies_added'] != 1:
        raise ValueError('Unexpected enclosure collision-body count')
    old_vertices = len(re.findall(rb'^vertex\s', raw, flags=re.M))
    old_triangles = len(re.findall(rb'^triangle\s', raw, flags=re.M))
    if old_vertices+len(positions) > 147456 or old_triangles+len(triangles) > 294912:
        raise ValueError('Enclosure exceeds production physics mesh limits')
    identifier = max(body_ids)+1
    values = lambda v: ' '.join(format(float(x), '.17g') for x in v)
    rows = [f'body {identifier} "Finward stepped exterior enclosure" 3 0 0 0 '+values(origin)+' 0 0 0 1 .5 .5 .5 .6 .01 1000 0 1 0 0 0 0 0 0 1']
    rows.extend(f'vertex {identifier} '+values(p) for p in positions)
    rows.extend(f'triangle {identifier} '+ ' '.join(str(int(v)) for v in t) for t in triangles)
    result = raw+(b'' if raw.endswith(b'\n') else b'\n')+('\n'.join(rows)+'\n').encode('ascii')
    if len(result) > 16*1024*1024:
        raise ValueError('Enclosure exceeds production physics document limit')
    return result
