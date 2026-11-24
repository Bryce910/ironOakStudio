#!/usr/bin/env python3
"""Cook the resolved purchased demo scene, including original baked lighting.

This is separate from the accepted fifteen-prefab export/cook. Source world
transforms, material-slot overrides, UV channels and lightmap bindings come
from NativeFinwardSceneExport. No runtime, playable project or source asset is
edited. Only a NEW output directory is published after complete CPU checks.
"""
from __future__ import annotations

import argparse
import collections
import copy
import hashlib
import json
import math
from pathlib import Path, PurePosixPath
import re
import shutil
import struct
import tempfile

import numpy as np
from scipy.spatial import cKDTree
import yaml

from build_opening_prologue import Glb, read_glb
from cook_finward_house import Materials, DATA, ROOT, SHADER_GUID, unity_documents, sha
from nature_lightmap_sampler import LightmapSampler


def write_json(path, value):
    path.write_text(json.dumps(value, indent=2, allow_nan=False) + '\n')


def safe_child(root, value):
    relative = PurePosixPath(value)
    if relative.is_absolute() or '..' in relative.parts or '\\' in value or str(relative) != value:
        raise ValueError('Unsafe scene data path: ' + repr(value))
    path = (root / str(relative)).resolve(strict=True)
    if not path.is_relative_to(root.resolve()) or not path.is_file():
        raise ValueError('Scene data escaped its source directory')
    return path


class Source:
    def __init__(self, path):
        self.path = path.resolve(strict=True)
        self.root = self.path.parent
        self.doc = json.loads(self.path.read_text())
        self.meshes = {item['hash']: item for item in self.doc['meshes']}
        self.materials = {item.get('key', item['path']): item for item in self.doc['materials']}
        self.inputs = {str(self.path): sha(self.path)}
        self.cache = {}

    def mesh(self, key):
        if key in self.cache:
            return self.cache[key]
        info = self.meshes[key]
        if info['stride'] != 64 or not 0 < info['vertices'] <= 1000000:
            raise ValueError('Unreviewed native scene mesh layout')
        path = safe_child(self.root, info['binary'])
        raw = path.read_bytes()
        digest = hashlib.sha256(raw).hexdigest()
        if digest != key:
            raise ValueError('Scene binary differs from its exported mesh hash')
        self.inputs[str(path)] = digest
        vertex_bytes = info['vertices'] * 64
        if vertex_bytes > len(raw):
            raise ValueError('Truncated source scene vertex buffer')
        vertices = np.ndarray((info['vertices'], 16), '<f4', raw).copy()
        if not np.isfinite(vertices).all() or not info['uv0_present']:
            raise ValueError('Source scene mesh has invalid geometry/UV0')
        indices = []
        for part in info['submeshes']:
            offset, count = part['index_offset'], part['indices']
            if count < 0 or count % 3 or offset < vertex_bytes or offset + count * 4 > len(raw):
                raise ValueError('Invalid source scene submesh buffer')
            index = np.ndarray((count,), '<i4', raw, offset).copy().reshape(-1, 3)
            if index.size and (index.min() < 0 or index.max() >= len(vertices)):
                raise ValueError('Invalid source scene triangle index')
            indices.append(index)
        self.cache[key] = (vertices, indices)
        return vertices, indices


class SourceProbeSamples:
    """Exact Unity CPU SH evaluation for the source's unlightmapped renderers."""
    def __init__(self, source, manifest, atlas_indices):
        self.path = manifest.resolve(strict=True)
        self.root = self.path.parent
        self.doc = json.loads(self.path.read_text())
        if (self.doc.get('schema') != 'valwood.finward-light-probe-vertex-samples' or
                self.doc.get('version') != 1 or self.doc.get('gpu_used') or self.doc.get('scene_saved') or
                self.doc.get('source_export_sha256') != sha(source.path)):
            raise ValueError('Source probe samples do not bind the frozen scene export')
        self.samples = {r['scene_filter_id']: r for r in self.doc['samples']}
        expected = {p['scene_filter_id'] for p in source.doc['placements']
                    if p['lod_mask'] & 3 and p['lightmap_index'] not in atlas_indices}
        if len(self.samples) != len(self.doc['samples']) or set(self.samples) != expected:
            raise ValueError('Source probe records must exactly cover unique invalid-atlas renderer IDs')
        self.source = source
        self.source.inputs[str(self.path)] = sha(self.path)
        self.cache = {}

    def values(self, placement, count):
        record = self.samples.get(placement['scene_filter_id'])
        if record is None:
            return None
        if (record['mesh'] != placement['mesh'] or record['vertices'] != count or
                record['lod_mask'] != placement['lod_mask'] or
                record['lightmap_index'] != placement['lightmap_index'] or
                record['light_probe_usage'] != 'BlendProbes' or record.get('probe_anchor')):
            raise ValueError('Source probe geometry/renderer binding differs from frozen placement')
        if record['sha256'] not in self.cache:
            path = safe_child(self.root, record['binary'])
            if path.stat().st_size != count * 12 or record['bytes'] != count * 12 or sha(path) != record['sha256']:
                raise ValueError('Invalid exact source probe vertex buffer')
            value = np.frombuffer(path.read_bytes(), '<f4').copy().reshape(count, 3)
            if not np.isfinite(value).all() or np.any(value < 0) or np.any(value > 65504):
                raise ValueError('Unreviewed exact source probe irradiance range')
            self.cache[record['sha256']] = value
            self.source.inputs[str(path)] = record['sha256']
        value = self.cache[record['sha256']]
        if len(value) != count:
            raise ValueError('Source probe buffer hash aliases a different vertex count')
        return value


class SceneMaterials(Materials):
    """Use authoritative source snapshots, including embedded scene materials."""
    def __init__(self, source, output, size, cache=None, tier=None):
        super().__init__(output, size)
        self.snapshots = {}
        self.source_records = source.materials
        folder = output / 'material_snapshots'
        folder.mkdir()
        for key, info in source.materials.items():
            raw = info.get('raw_yaml', '')
            if not raw:
                raise ValueError('Resolved material lacks authoritative source YAML: ' + key)
            parsed = next(item['Material'] for item in unity_documents_text(raw) if 'Material' in item)
            if parsed['m_Shader'].get('guid') != SHADER_GUID:
                raise ValueError('Scene material needs an explicit shader conversion: ' + key + ' ' + repr(parsed['m_Shader']))
            path = folder / (hashlib.sha256(key.encode()).hexdigest() + '.mat')
            path.write_text(raw)
            self.snapshots[key] = path
        if cache is not None:
            records = json.loads((cache / (tier + '-material-report.json')).read_text())
            for key, record in records.items():
                if key not in self.snapshots or record['source_sha256'] != sha(self.snapshots[key]):
                    raise ValueError('Prepared material cache differs from current resolved source material')
                for relative in record['paths'].values():
                    path = safe_child(cache / (tier + '_materials'), relative)
                    if path.suffix != '.png' or sha(path) != path.stem:
                        raise ValueError('Prepared texture cache hash mismatch')
                    target = output / relative
                    target.parent.mkdir(exist_ok=True)
                    if not target.exists():
                        shutil.copy2(path, target)
                self.records[key] = record

    def source_path(self, path):
        if path in self.snapshots:
            return self.snapshots[path]
        return super().source_path(path)

    def make(self, path):
        record = super().make(path)
        record['source_material'] = self.source_records[path]
        record['source_snapshot'] = str(self.snapshots[path].relative_to(self.output))
        return record


def unity_documents_text(text):
    text = re.sub(r'^%.*\n', '', text, flags=re.M)
    text = re.sub(r'^--- !u!\d+ &-?\d+', '---', text, flags=re.M)
    return [item for item in yaml.safe_load_all(text) if item]


class SceneGlb(Glb):
    def __init__(self):
        super().__init__()
        self.parts = collections.defaultdict(list)

    def scene_mesh(self, material, position, normal, uv, light_uv, secondary_uv, indices, light):
        self.parts[material].append((position, normal, uv, light_uv, secondary_uv, indices, light))

    def finish(self):
        primitives = []
        for material, pieces in self.parts.items():
            offset = np.cumsum([0] + [len(piece[0]) for piece in pieces[:-1]])
            arrays = [np.concatenate([piece[column] for piece in pieces]) for column in (0, 1, 2, 3, 4, 6)]
            indices = np.concatenate([piece[5] + start for piece, start in zip(pieces, offset)])
            attributes = dict(POSITION=self.accessor(arrays[0], 'VEC3'),
                              NORMAL=self.accessor(arrays[1], 'VEC3'),
                              TEXCOORD_0=self.accessor(arrays[2], 'VEC2'),
                              TEXCOORD_1=self.accessor(arrays[3], 'VEC2'),
                              TEXCOORD_2=self.accessor(arrays[4], 'VEC2'),
                              _VALWOOD_BAKED_LIGHT=self.accessor(arrays[5], 'VEC4'))
            primitives.append(dict(attributes=attributes, indices=self.accessor(indices.reshape(-1), 'SCALAR'),
                                   material=material, mode=4))
        self.doc['meshes'] = [dict(name='Exact purchased demo scene chunk', primitives=primitives)]
        self.doc['nodes'] = [dict(name='Source Unity scene, recentered and Z reflected', mesh=0)]
        self.doc['buffers'] = [dict(byteLength=len(self.binary))]
        self.doc['asset']['extras'] = dict(source_geometry='Resolved purchased demo scene, no layout recreation',
            source_lighting='_VALWOOD_BAKED_LIGHT: original linear source EXR RGB plus validity',
            lightmap_uv='TEXCOORD_1 stores original UV1 transformed by source lightmap ST, then V reflected',
            secondary_uv='TEXCOORD_2 preserves original source channel2 with V reflected')
        payload = json.dumps(self.doc, separators=(',', ':')).encode()
        payload += b' ' * (-len(payload) % 4)
        binary = bytes(self.binary)
        binary += b'\0' * (-len(binary) % 4)
        return (struct.pack('<III', 0x46546c67, 2, 28 + len(payload) + len(binary)) +
                struct.pack('<II', len(payload), 0x4e4f534a) + payload +
                struct.pack('<II', len(binary), 0x004e4942) + binary)


def subdivide(vertices, indices, edge_limit, *, triangle_limit=500000):
    """Split longest edges using linear attributes; source surfaces stay exact."""
    pending = vertices[indices].copy()
    complete = []
    rounds = 0
    while len(pending):
        positions = pending[:, :, :3]
        lengths = np.stack([np.sum((positions[:, a] - positions[:, b]) ** 2, axis=1)
                            for a, b in ((0, 1), (1, 2), (2, 0))], axis=1)
        longest = lengths.argmax(1)
        active = lengths.max(1) > edge_limit ** 2 * (1 + 1e-7)
        if (~active).any():
            complete.append(pending[~active])
        if not active.any():
            break
        following = []
        for edge, order in enumerate(((0, 1, 2), (1, 2, 0), (2, 0, 1))):
            selected = pending[active & (longest == edge)][:, order]
            if not len(selected):
                continue
            midpoint = (selected[:, 0] + selected[:, 1]) * .5
            following.extend((np.stack((selected[:, 0], midpoint, selected[:, 2]), 1),
                              np.stack((midpoint, selected[:, 1], selected[:, 2]), 1)))
        pending = np.concatenate(following)
        rounds += 1
        if rounds > 24 or len(pending) + sum(len(a) for a in complete) > triangle_limit:
            raise ValueError('Source shell subdivision exceeded its bounded triangle budget')
    triangles = np.concatenate(complete)
    # Exact attribute deduplication avoids storing three new vertices per
    # triangle while retaining all UV seams and source normal discontinuities.
    flat = np.ascontiguousarray(triangles.reshape(-1, vertices.shape[1]))
    key = flat.view(np.dtype((np.void, flat.dtype.itemsize * flat.shape[1]))).reshape(-1)
    _, first, inverse = np.unique(key, return_index=True, return_inverse=True)
    return flat[first], inverse.reshape(-1, 3)


def units_for_scene(source):
    units = collections.defaultdict(list)
    for item in source.doc['placements']:
        if not item.get('lod_mask', 0) & 3:
            continue
        key = item.get('prefab_root_id') or item.get('prefab_root_hierarchy') or item['hierarchy']
        units[key].append(item)
    result = []
    for key, placements in sorted(units.items()):
        near = [p for p in placements if p['lod_mask'] & 1]
        if not near:
            raise ValueError('Source logical unit has no authored near surface: ' + key)
        low = np.min([[p['bounds_min'][c] for c in 'xyz'] for p in near], axis=0)
        high = np.max([[p['bounds_max'][c] for c in 'xyz'] for p in near], axis=0)
        materials = {m for p in placements for m in p['materials'] if m}
        result.append(dict(key=key, placements=placements, low=low, high=high,
                           center=(low + high) * .5, materials=materials))
    return result


def partitions(units, span, max_materials, max_chunks):
    cells = collections.defaultdict(list)
    for unit in units:
        cells[(math.floor(unit['center'][0] / span), math.floor(unit['center'][2] / span))].append(unit)
    result = []
    def split(name, items):
        materials = set().union(*(item['materials'] for item in items))
        if len(materials) <= max_materials:
            result.append((name, items))
            return
        if len(items) == 1:
            raise ValueError('A source assembly exceeds the material chunk budget')
        axis = int(np.ptp([item['center'] for item in items], axis=0).argmax())
        ordered = sorted(items, key=lambda item: item['center'][axis])
        middle = len(ordered) // 2
        split(name + '_a', ordered[:middle])
        split(name + '_b', ordered[middle:])
    for (x, z), items in sorted(cells.items()):
        split('finward_scene_%s_%s' % (str(x).replace('-', 'm'), str(z).replace('-', 'm')), items)
    if len(result) > max_chunks:
        raise ValueError('%d chunks exceed the requested %d; adjust source partition span/material budget' %
                         (len(result), max_chunks))
    return result


def prepare_unit(source, unit, sampler, shell_edge, counters, probes=None):
    prepared = []
    near_samples = []
    for placement in unit['placements']:
        original, source_indices = source.mesh(placement['mesh'])
        matrix = np.asarray(placement['matrix'], dtype=np.float64).reshape(4, 4, order='F')
        if not np.isfinite(matrix).all() or abs(np.linalg.det(matrix[:3, :3])) < 1e-10:
            raise ValueError('Invalid source instance transform')
        transformed = original.copy()
        transformed[:, :3] = original[:, :3] @ matrix[:3, :3].T + matrix[:3, 3]
        transformed[:, 3:6] = original[:, 3:6] @ np.linalg.inv(matrix[:3, :3])
        transformed[:, 3:6] /= np.maximum(np.linalg.norm(transformed[:, 3:6], axis=1, keepdims=True), 1e-10)
        probe = probes.values(placement, len(original)) if probes else None
        if probe is not None:
            # Keep the exact per-source-vertex values attached to every
            # original vertex through submesh remapping. Any shell midpoint
            # would interpolate this data alongside the preserved UVs.
            transformed = np.column_stack((transformed, probe))
            counters['source_sh_renderer_placements'] += 1
        st = np.array([placement['lightmap_scale_offset'][c] for c in 'xyzw'], dtype=np.float32)
        mesh_info = source.meshes[placement['mesh']]
        shell = '/Meshes_building/' in mesh_info.get('source', '') or '/Prefabs/Building/' in placement.get('source_prefab', '')
        for submesh, index in enumerate(source_indices):
            if not len(index):
                continue
            material = placement['materials'][submesh] if submesh < len(placement['materials']) else ''
            if not material or material not in source.materials:
                raise ValueError('Unresolved source material slot: ' + placement['hierarchy'])
            used, remap = np.unique(index, return_inverse=True)
            vertex, indices = transformed[used].copy(), remap.reshape(-1, 3)
            counters['source_triangle_instances'] += len(indices)
            if shell:
                old_count = len(indices)
                vertex, indices = subdivide(vertex, indices, shell_edge)
                counters['shell_triangles_before_subdivision'] += old_count
                counters['shell_triangles_after_subdivision'] += len(indices)
            vertex[:, 3:6] /= np.maximum(np.linalg.norm(vertex[:, 3:6], axis=1, keepdims=True), 1e-10)
            atlas_uv = vertex[:, 8:10] * st[:2] + st[2:]
            light = sampler.sample_rgba(placement['lightmap_index'], atlas_uv, vertex[:, 3:6],
                                         valid=bool(mesh_info.get('uv1_present')))
            if probe is not None:
                if (light[:, 3] > .5).any():
                    raise ValueError('Source probe samples may not replace an authored lightmap')
                light = np.column_stack((vertex[:, 16:19], np.ones(len(vertex), np.float32)))
                counters['source_sh_vertices_used'] += len(vertex)
            if placement['lod_mask'] & 1 and (light[:, 3] > .5).any():
                valid = light[:, 3] > .5
                near_samples.append((vertex[valid, :3], vertex[valid, 3:6], light[valid]))
            prepared.append(dict(placement=placement, material=material, vertex=vertex,
                                 indices=indices, light=light, atlas_uv=atlas_uv,
                                 mirrored_source=np.linalg.det(matrix[:3, :3]) < 0, shell=shell))
    if near_samples:
        position, normal, lighting = [np.concatenate([s[i] for s in near_samples]) for i in range(3)]
        tree = None
        for item in prepared:
            if item['placement']['lod_mask'] & 1:
                continue
            missing = item['light'][:, 3] < .5
            if not missing.any():
                continue
            if tree is None:
                tree = cKDTree(position)
            distance, index = tree.query(item['vertex'][missing, :3], k=min(8, len(position)), workers=1)
            if distance.ndim == 1:
                distance, index = distance[:, None], index[:, None]
            alignment = np.einsum('ijk,ik->ij', normal[index], item['vertex'][missing, 3:6])
            weight = np.maximum(alignment, 0) ** 4 / np.maximum(distance, .005) ** 2
            weight[(alignment < .35) | (distance > .35)] = 0
            total = weight.sum(1)
            valid = total > 1e-12
            target = np.flatnonzero(missing)[valid]
            item['light'][target, :3] = ((lighting[index, :3] * weight[:, :, None]).sum(1) /
                                          np.maximum(total[:, None], 1e-12))[valid]
            item['light'][target, 3] = 1
            counters['mid_vertices_transferred_from_near'] += len(target)
    return prepared


def emit_chunk(name, items, near_materials, mid_materials, output, source, counters):
    near_points = np.concatenate([item['vertex'][:, :3] for item in items if item['placement']['lod_mask'] & 1])
    low, high = near_points.min(0), near_points.max(0)
    origin = np.array([(low[0] + high[0]) * .5, low[1], (low[2] + high[2]) * .5])
    records = []
    for lod, label, materials in ((0, 'near', near_materials), (1, 'mid', mid_materials)):
        g = SceneGlb()
        mapping = {}
        triangles = vertices = baked_vertices = shell_triangles = 0
        paths = []
        for item in items:
            if not item['placement']['lod_mask'] & (1 << lod):
                continue
            key = item['material']
            if key not in mapping:
                mapping[key] = materials.append(g, key)
            material, info = mapping[key]
            vertex = item['vertex']
            position = (vertex[:, :3] - origin).copy()
            position[:, 2] *= -1
            normal = vertex[:, 3:6].copy()
            normal[:, 2] *= -1
            uv = vertex[:, 6:8] * info['uv_scale'] + info['uv_offset']
            uv[:, 1] = 1 - uv[:, 1]
            light_uv = item['atlas_uv'].copy()
            light_uv[:, 1] = 1 - light_uv[:, 1]
            secondary_uv = vertex[:, 10:12].copy()
            secondary_uv[:, 1] = 1 - secondary_uv[:, 1]
            index = item['indices'].copy()
            if not item['mirrored_source']:
                index = index[:, [0, 2, 1]]
            cross = np.cross(position[index[:, 1]] - position[index[:, 0]],
                             position[index[:, 2]] - position[index[:, 0]])
            valid = np.linalg.norm(cross, axis=1) > 1e-12
            counters['degenerate_source_triangles_removed'] += int((~valid).sum())
            index = index[valid]
            agreement = np.einsum('ij,ij->i', cross[valid], normal[index].sum(1))
            opposing = int((agreement < -1e-9).sum())
            if opposing / max(1, len(index)) > .02:
                raise ValueError('Source winding/normal mismatch: ' + item['placement']['hierarchy'])
            g.scene_mesh(material, position, normal, uv, light_uv, secondary_uv,
                          index, item['light'])
            triangles += len(index)
            vertices += len(vertex)
            baked_vertices += int((item['light'][:, 3] > .5).sum())
            shell_triangles += len(index) if item['shell'] else 0
            paths.append(item['placement']['hierarchy'])
        if not paths:
            raise ValueError('Source chunk has no authored ' + label + ' geometry')
        path = output / (name + '_' + label + '.glb')
        path.write_bytes(g.finish())
        if path.stat().st_size > 120 * 1024 ** 2 or vertices > 900000 or triangles > 900000:
            raise ValueError('Source chunk exceeds native GLB limits: ' + name)
        doc, binary = read_glb(path)
        minimum = np.min([part[0].min(0) for pieces in g.parts.values() for part in pieces], axis=0)
        maximum = np.max([part[0].max(0) for pieces in g.parts.values() for part in pieces], axis=0)
        records.append(dict(lod=lod, file=path.name, sha256=sha(path), bytes=path.stat().st_size,
                            triangles=triangles, vertices=vertices, baked_vertices=baked_vertices,
                            shell_triangles=shell_triangles, primitives=len(doc['meshes'][0]['primitives']),
                            images=len(doc['images']), minimum=minimum.tolist(), maximum=maximum.tolist(),
                            source_hierarchies=paths))
    print('FINWARD SCENE CHUNK', name, [(r['triangles'], r['baked_vertices']) for r in records], flush=True)
    return dict(name=name, origin=origin.tolist(),
                origin_native=[float(origin[0]), float(origin[1]), float(-origin[2])],
                source_minimum=low.tolist(), source_maximum=high.tolist(),
                dimensions=(high - low).tolist(), models=records,
                coordinates='Source Unity world metres; chunk origin remains Unity-space, GLB local Z and winding reflected; no rotation/rescale/layout changes')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, default=DATA / 'scene_export/scene.json')
    parser.add_argument('--lightmaps', type=Path, required=True)
    parser.add_argument('--output', type=Path, default=DATA / 'scene-cooked-v1')
    parser.add_argument('--near-size', type=int, choices=(256, 512, 1024), default=512)
    parser.add_argument('--mid-size', type=int, choices=(64, 128, 256), default=128)
    parser.add_argument('--chunk-span', type=float, default=16.)
    parser.add_argument('--max-materials', type=int, default=24)
    parser.add_argument('--max-chunks', type=int, default=15)
    parser.add_argument('--shell-edge', type=float, default=.12)
    parser.add_argument('--material-cache', type=Path, help='Completed same-source/caps cook; reuse exact hashed prepared PNGs')
    parser.add_argument('--lightprobes', type=Path, help='Exact Unity per-vertex SH samples for invalid-atlas renderers')
    args = parser.parse_args()
    if args.output.exists() or args.output.is_symlink():
        raise ValueError('Choose a NEW scene cook output directory')
    if not 4 <= args.chunk_span <= 64 or not .05 <= args.shell_edge <= .12 or not 1 <= args.max_chunks <= 16:
        raise ValueError('Scene chunk/subdivision bounds exceeded')
    source = Source(args.source)
    cached_inputs = {}
    if args.material_cache:
        args.material_cache = args.material_cache.resolve(strict=True)
        cached_review = json.loads((args.material_cache / 'COOK_REVIEW.json').read_text())
        if (cached_review.get('source_scene_sha256') != sha(source.path) or
            cached_review.get('near_texture_cap') != args.near_size or
            cached_review.get('mid_texture_cap') != args.mid_size):
            raise ValueError('Material cache must bind the identical source scene and texture caps')
        cached_inputs = cached_review['source_material_inputs']
        for path, expected in cached_inputs.items():
            if sha(path) != expected:
                raise ValueError('Original texture input changed since prepared material cache')
    sampler = LightmapSampler(args.lightmaps)
    probes = SourceProbeSamples(source, args.lightprobes, sampler.registry) if args.lightprobes else None
    for item in sampler.registry.values():
        if sha(item['cache']) != item['cache_sha256'] or sha(item['source']) != item['sha256']:
            raise ValueError('Original lightmap or decoded cache changed')
    units = units_for_scene(source)
    chunks = partitions(units, args.chunk_span, args.max_materials, args.max_chunks)
    print('FINWARD SCENE PLAN', len(units), 'logical assemblies', len(chunks), 'chunks', flush=True)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    temporary = Path(tempfile.mkdtemp(prefix='.finward-scene-cook-', dir=args.output.parent))
    try:
        near_folder, mid_folder = temporary / 'near_materials', temporary / 'mid_materials'
        near_folder.mkdir(); mid_folder.mkdir()
        near = SceneMaterials(source, near_folder, args.near_size, args.material_cache, 'near')
        mid = SceneMaterials(source, mid_folder, args.mid_size, args.material_cache, 'mid')
        counters = collections.Counter()
        catalog = []
        geometry = temporary / 'chunks'
        geometry.mkdir()
        for name, chunk_units in chunks:
            items = []
            for unit in chunk_units:
                items.extend(prepare_unit(source, unit, sampler, args.shell_edge, counters, probes))
            record = emit_chunk(name, items, near, mid, geometry, source, counters)
            record['source_assemblies'] = len(chunk_units)
            catalog.append(record)
        for path, expected in source.inputs.items():
            if sha(path) != expected:
                raise ValueError('Original scene export changed during cook')
        original_inputs = dict(cached_inputs)
        for path, expected in original_inputs.items():
            if sha(path) != expected:
                raise ValueError('Original cached material input changed during cook')
        for material_set in (near, mid):
            for relative, expected in material_set.inputs.items():
                path = DATA / relative
                if sha(path) != expected:
                    raise ValueError('Original material input changed during cook')
                original_inputs[str(path)] = expected
        write_json(temporary / 'catalog.json', catalog)
        write_json(temporary / 'near-material-report.json', near.records)
        write_json(temporary / 'mid-material-report.json', mid.records)
        report = dict(passed=True, source_scene=str(source.path), source_scene_sha256=sha(source.path),
            source_inputs=source.inputs, source_material_inputs=original_inputs,
            prepared_material_cache=dict(path=str(args.material_cache), review_sha256=sha(args.material_cache / 'COOK_REVIEW.json')) if args.material_cache else None,
            source_lightmap_manifest=str(args.lightmaps.resolve()), source_lightmap_manifest_sha256=sha(args.lightmaps),
            source_lightprobe_manifest=str(probes.path) if probes else None,
            source_lightprobe_manifest_sha256=sha(probes.path) if probes else None,
            original_inputs_unchanged=True, source_assets_modified=False, playable_projects_modified=False,
            gpu_rendering_used=False, chunks=len(catalog), source_assemblies=len(units),
            source_renderer_records=len(source.doc['placements']),
            triangles=[sum(c['models'][lod]['triangles'] for c in catalog) for lod in (0, 1)],
            vertices=[sum(c['models'][lod]['vertices'] for c in catalog) for lod in (0, 1)],
            source_stats=dict(counters), lightmap_sampling=sampler.stats,
            material_count=len(near.records), near_texture_cap=args.near_size, mid_texture_cap=args.mid_size,
            shell_subdivision_max_edge_m=args.shell_edge,
            geometry='Exact resolved scene transforms/material overrides; original geometry linearly subdivided only for shell bake fidelity; no source file edits or lossy simplification',
            lighting='Source linear EXRs sampled at original lightmap UV1/ST; optional invalid-atlas renderers use exact Unity CPU SphericalHarmonicsL2.Evaluate at original inverse-transpose world normals; no exposure, albedo, gamma, RGBM or PI multiplier; remaining missing MID bake transfers from near within .35m with normal weighting',
            native_attribute='_VALWOOD_BAKED_LIGHT FLOAT VEC4',
            source_lights=source.doc.get('lights', []), source_cameras=source.doc.get('cameras', []),
            source_settings=source.doc.get('settings', {}), source_warnings=source.doc.get('warnings', []),
            limitations=['Native full-bake replacement shader and room-switch masking are separate caller integration',
                         'Per-vertex source bake approximates per-pixel source lightmaps; shell edges subdivided to requested bound',
                         'Original source MID silhouettes retained; missing MID bake receives bounded nearest-near lighting transfer',
                         'Production CPU-loader budget and actual native visual/collision acceptance are required before activation'])
        write_json(temporary / 'COOK_REVIEW.json', report)
        temporary.replace(args.output)
        print('FINWARD SCENE READY', args.output, report['triangles'], flush=True)
    finally:
        if temporary.exists():
            shutil.rmtree(temporary)


if __name__ == '__main__':
    main()
