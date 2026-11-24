#!/usr/bin/env python3
"""Read-only production CPU and source-bake contract checks for a scene cook."""
from pathlib import Path
import argparse
import hashlib
import io
import json
import subprocess

import numpy as np
from PIL import Image
from build_opening_prologue import read_glb, array


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cook', type=Path, required=True)
    parser.add_argument('--checker', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    if args.output.exists():
        raise ValueError('Choose a NEW validation report')
    root = args.cook.resolve(strict=True)
    catalog = json.loads((root / 'catalog.json').read_text())
    source_review = json.loads((root / 'COOK_REVIEW.json').read_text())
    models, contracts = [], []
    for chunk in catalog:
        for model in chunk['models']:
            path = (root / 'chunks' / model['file']).resolve(strict=True)
            assert path.is_relative_to(root) and sha(path) == model['sha256']
            doc, binary = read_glb(path)
            positions, radiance, shell_edge = [], [], 0.
            triangles = 0
            for primitive in doc['meshes'][0]['primitives']:
                attributes = primitive['attributes']
                assert {'POSITION', 'NORMAL', 'TEXCOORD_0', 'TEXCOORD_1', 'TEXCOORD_2', '_VALWOOD_BAKED_LIGHT'} <= set(attributes)
                value = doc['accessors'][attributes['_VALWOOD_BAKED_LIGHT']]
                assert value['componentType'] == 5126 and value['type'] == 'VEC4' and not value.get('normalized', False)
                position = array(doc, binary, attributes['POSITION'])
                normal = array(doc, binary, attributes['NORMAL'])
                light = array(doc, binary, attributes['_VALWOOD_BAKED_LIGHT'])
                assert len(position) == len(normal) == len(light)
                assert np.isfinite(position).all() and np.isfinite(normal).all() and np.isfinite(light).all()
                assert (light[:, :3] >= 0).all() and (light[:, :3] <= 65504).all()
                assert np.isin(light[:, 3], [0., 1.]).all()
                assert np.max(np.abs(np.linalg.norm(normal, axis=1) - 1.)) < 2e-5
                index = array(doc, binary, primitive['indices']).reshape(-1, 3)
                assert index.size and index.max() < len(position)
                triangles += len(index)
                positions.append(position)
                radiance.append(light)
            position, light = np.concatenate(positions), np.concatenate(radiance)
            assert triangles == model['triangles'] and len(position) == model['vertices']
            assert int((light[:, 3] > .5).sum()) == model['baked_vertices']
            assert np.allclose(position.min(0), model['minimum'], atol=1e-6)
            assert np.allclose(position.max(0), model['maximum'], atol=1e-6)
            sizes = []
            cap = source_review['near_texture_cap'] if model['lod'] == 0 else source_review['mid_texture_cap']
            for image in doc['images']:
                view = doc['bufferViews'][image['bufferView']]
                offset = view.get('byteOffset', 0)
                with Image.open(io.BytesIO(binary[offset:offset + view['byteLength']])) as decoded:
                    assert max(decoded.size) <= cap
                    sizes.append(list(decoded.size))
            models.append(path)
            contracts.append(dict(file=str(path), sha256=sha(path), triangles=triangles,
                vertices=len(position), baked_vertices=model['baked_vertices'],
                baked_fraction=float(light[:, 3].mean()),
                linear_rgb_range=[float(light[:, :3].min()), float(light[:, :3].max())],
                linear_rgb_mean=light[:, :3].mean(0, dtype=np.float64).tolist(),
                image_dimensions=sizes, source_origin_native=chunk['origin_native'], passed=True))
    result = subprocess.run([str(args.checker.resolve()), *map(str, models)], capture_output=True, text=True, timeout=300)
    production = json.loads(result.stdout)
    assert result.returncode == 0 and production.get('passed') is True
    assert len(production['cases']) == len(models)
    for case, contract in zip(production['cases'], contracts):
        assert case['passed'] and case['cpu_bytes'] < 384 * 1024 ** 2
        assert np.allclose(case['bounds'], [next(m for c in catalog for m in c['models'] if m['sha256'] == contract['sha256'])['minimum'],
                                               next(m for c in catalog for m in c['models'] if m['sha256'] == contract['sha256'])['maximum']], atol=1e-4)
    report = dict(passed=True, scope='No GPU, runtime or project writes; exact completed GLBs checked by production CPU loader and baked vertex attribute contract',
        cook=str(root), catalog_sha256=sha(root / 'catalog.json'), cook_review_sha256=sha(root / 'COOK_REVIEW.json'),
        checker=dict(path=str(args.checker.resolve()), sha256=sha(args.checker)),
        chunks=len(catalog), models=len(models), contracts=contracts, production=production,
        maximum_cpu_bytes=max(c['cpu_bytes'] for c in production['cases']),
        estimated_aggregate_gpu_bytes=sum(c['gpu_bytes'] for c in production['cases']),
        gpu_estimate_scope='Aggregate CPU preparation estimates across all near/MID files before cross-file texture sharing; not measured VRAM')
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2) + '\n')
    print('FINWARD SCENE CPU PASS', len(models), 'models; max CPU', report['maximum_cpu_bytes'], flush=True)


if __name__ == '__main__':
    main()
