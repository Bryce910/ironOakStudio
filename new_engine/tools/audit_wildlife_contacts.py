#!/usr/bin/env python3
"""Audit continuous wildlife contact surfaces against generated support bounds.

Read-only CPU analysis of the actual encoded GLBs; no engine or GPU work.
Example: python3 tools/audit_wildlife_contacts.py --output \
    docs/performance/wildmesh/contact-envelope-audit.json
Only an explicitly supplied --output file is written. The analytic extrema
use float64 arithmetic over the GLBs' stored float32 positions.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re

import numpy as np

from audit_village_pose_compatibility import glb, values

ROOT = Path(__file__).resolve().parents[1]
BAND = .035
SPECIES = ('chicken', 'cow', 'calf', 'goat', 'buck', 'fox', 'wildboar', 'leopard')


def exact_contact_half_extents(a, b, edges):
    """Return maximum absolute X/Z below BAND over an entire affine pose pair."""
    a = np.asarray(a, dtype=np.float64)
    b = np.asarray(b, dtype=np.float64)
    low = np.concatenate([a[a[:, 1] <= BAND], b[b[:, 1] <= BAND]])
    delta = b - a
    crossing = (a[:, 1] < BAND) != (b[:, 1] < BAND)
    if crossing.any():
        t = (BAND - a[crossing, 1]) / delta[crossing, 1]
        low = np.concatenate([low, a[crossing] + delta[crossing] * t[:, None]])
    assert len(low), 'Pose pair has no contact-band geometry'
    half = np.abs(low[:, [0, 2]]).max(axis=0)

    # At a fixed time a clipped triangle's extrema are its source vertices or
    # edge/plane intersections. An intersection coordinate is N(t)/D(t), with
    # N quadratic and D affine. Its extrema occur at interval endpoints,
    # temporal vertex/plane crossings, or the roots of N'D - ND'.
    ea, eb = a[edges[:, 0]], a[edges[:, 1]]
    da, db = delta[edges[:, 0]], delta[edges[:, 1]]
    d0, d1 = eb[:, 1] - ea[:, 1], db[:, 1] - da[:, 1]
    with np.errstate(divide='ignore', invalid='ignore'):
        endpoints = [np.zeros(len(edges)), np.ones(len(edges)),
                     (BAND - ea[:, 1]) / da[:, 1], (BAND - eb[:, 1]) / db[:, 1]]
    for output_axis, axis in enumerate((0, 2)):
        n0 = ea[:, axis] * (eb[:, 1] - BAND) - eb[:, axis] * (ea[:, 1] - BAND)
        n1 = (da[:, axis] * (eb[:, 1] - BAND) + ea[:, axis] * db[:, 1]
              - db[:, axis] * (ea[:, 1] - BAND) - eb[:, axis] * da[:, 1])
        n2 = da[:, axis] * db[:, 1] - db[:, axis] * da[:, 1]
        qa, qb, qc = n2 * d1, 2 * n2 * d0, n1 * d0 - n0 * d1
        discriminant = qb * qb - 4 * qa * qc
        with np.errstate(divide='ignore', invalid='ignore'):
            root = np.sqrt(np.maximum(discriminant, 0))
            r0, r1 = (-qb - root) / (2 * qa), (-qb + root) / (2 * qa)
            linear = -qc / qb
        quadratic = (np.abs(qa) > 1e-20) & (discriminant >= -1e-20)
        r0, r1 = np.where(quadratic, r0, np.nan), np.where(quadratic, r1, np.nan)
        linear = np.where((np.abs(qa) <= 1e-20) & (np.abs(qb) > 1e-20), linear, np.nan)
        for t in endpoints + [r0, r1, linear]:
            t = np.where(np.isfinite(t), t, -1.)
            ay, by = ea[:, 1] + da[:, 1] * t, eb[:, 1] + db[:, 1] * t
            denominator = d0 + d1 * t
            valid = ((t >= -1e-12) & (t <= 1 + 1e-12)
                     & (np.minimum(ay, by) <= BAND + 1e-12)
                     & (np.maximum(ay, by) >= BAND - 1e-12)
                     & (np.abs(denominator) > 1e-14))
            if valid.any():
                q = t[valid]
                val = (n0[valid] + n1[valid] * q + n2[valid] * q * q) / denominator[valid]
                half[output_axis] = max(half[output_axis], np.abs(val).max())
    assert np.isfinite(half).all()
    return half


def digest(path):
    h = hashlib.sha256()
    with path.open('rb') as source:
        for block in iter(lambda: source.read(1024 * 1024), b''):
            h.update(block)
    return h.hexdigest()


def label(path):
    path = path.resolve()
    return str(path.relative_to(ROOT)) if path.is_relative_to(ROOT) else str(path)


def run(models, data):
    rows = {}
    for line in data.read_text().splitlines():
        match = re.search(r'\{"wildmesh_([^\"]+)",(.+)\},', line)
        if match:
            fields = [float(v.rstrip('f')) for v in match.group(2).split(',')]
            assert len(fields) == 7 and match.group(1) not in rows
            rows[match.group(1)] = np.array(fields[-2:])
    assert tuple(rows) == SPECIES, 'Unexpected generated wildlife species layout'
    records, inputs = [], []
    for name, half in rows.items():
        bound = np.zeros(2)
        pairs, contributors = [], [None, None]
        for mode, count in [('walk', 8), ('idle', 4)]:
            points, indices = [], None
            for i in range(count):
                path = models / f'wildmesh_{name}_{mode}_{i:02}.glb'
                doc, binary = glb(path)
                assert not doc.get('skins') and not doc.get('animations')
                assert len(doc['meshes']) == 1 and len(doc['meshes'][0]['primitives']) == 1
                nodes = [n for n in doc['nodes'] if 'mesh' in n]
                assert len(doc['nodes']) == len(nodes) == 1
                node = nodes[0]
                assert node['mesh'] == 0 and not any(k in node for k in ('matrix', 'translation', 'rotation', 'scale'))
                primitive = doc['meshes'][0]['primitives'][0]
                assert primitive.get('mode', 4) == 4
                vertices = values(doc, binary, primitive['attributes']['POSITION'])
                assert vertices.shape[1] == 3 and np.isfinite(vertices).all()
                topology = values(doc, binary, primitive['indices']).reshape(-1, 3)
                assert not len(points) or vertices.shape == points[0].shape
                assert indices is None or np.array_equal(indices, topology)
                indices = topology
                points.append(vertices)
                inputs.append(dict(file=label(path), sha256=digest(path)))
            edges = np.unique(np.sort(np.concatenate([
                indices[:, [0, 1]], indices[:, [1, 2]], indices[:, [2, 0]]]), axis=1), axis=0)
            for i, (a, b) in enumerate(zip(points, points[1:] + points[:1])):
                actual = exact_contact_half_extents(a, b, edges)
                pair = dict(mode=mode, from_pose=i, to_pose=(i + 1) % count,
                            contact_half_xz_m=actual.tolist())
                pairs.append(pair)
                for axis in range(2):
                    if actual[axis] > bound[axis]:
                        contributors[axis] = dict(mode=mode, from_pose=i, to_pose=(i + 1) % count)
                bound = np.maximum(bound, actual)
        margin = half - bound
        records.append(dict(species=name, pose_count=12, adjacent_pair_count=len(pairs),
            contact_half_xz_m=bound.tolist(), published_support_half_xz_m=half.tolist(),
            remaining_margin_xz_m=margin.tolist(), minimum_margin_m=float(margin.min()),
            maximum_contributors_xz=contributors, pairs=pairs, passed=bool((margin >= 0).all())))
    passed = all(r['passed'] for r in records)
    return dict(schema_version=1, passed=passed, contact_band_m=BAND,
        method='Analytic extrema of clipped triangle surfaces over every continuous adjacent-pose affine blend, including wrap pairs; float64 arithmetic over stored float32 positions.',
        scope='Source contact-envelope containment only. No engine, GPU, terrain, FPS or continuous-terrain validation is performed.',
        generated_data=dict(file=label(data), sha256=digest(data)),
        auditor=dict(file=label(Path(__file__)), sha256=digest(Path(__file__))),
        species_count=len(records), pose_count=len(inputs),
        adjacent_pair_count=sum(r['adjacent_pair_count'] for r in records),
        minimum_margin_m=min(r['minimum_margin_m'] for r in records),
        species=records, pose_inputs=inputs)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--models', type=Path, default=ROOT / 'projects/valwood_world/models')
    parser.add_argument('--data', type=Path, default=ROOT / 'engine/world_wildlife_data.inc')
    parser.add_argument('--output', type=Path, help='Optional JSON evidence destination')
    args = parser.parse_args()
    result = run(args.models.resolve(), args.data.resolve())
    encoded = json.dumps(result, indent=2, allow_nan=False) + '\n'
    if args.output:
        args.output.write_text(encoded)
        print(f'{"PASS" if result["passed"] else "FAIL"}: {result["species_count"]} species, '
              f'{result["pose_count"]} poses, {result["adjacent_pair_count"]} adjacent pairs; '
              f'minimum contact-envelope margin {result["minimum_margin_m"] * 1000:.5f} mm; '
              f'evidence {args.output}')
    else:
        print(encoded, end='')
    return 0 if result['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
