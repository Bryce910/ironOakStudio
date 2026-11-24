#!/usr/bin/env python3
"""Author a short, optional cold open using the accepted apartment geometry.

All coordinates below are in the accepted house's local metre space. This
helper writes only camera support files in a new private project; the GLBs,
colliders, story config, player spawn and shared assets remain untouched. The
last frame is computed from that project's real house_spawn, never guessed.
"""
from pathlib import Path, PurePosixPath
import argparse
import hashlib
import json
import math
import struct

import numpy as np


FORMAT = 'VALWOOD_HOME_CINEMATIC 1'
SETTINGS = Path('opening/cinematic.settings')
META = Path('opening/cinematic.json')
FOV_PLAY = 1.0471975512


def _sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def _read_spawn(project):
    rows = (project/'opening.config').read_text(encoding='ascii').splitlines()
    matches = [row.split()[1:] for row in rows if row.startswith('house_spawn ')]
    if len(matches) != 1 or len(matches[0]) != 5:
        raise ValueError('A unique authored house_spawn is required')
    values = [float(x) for x in matches[0]]
    if not all(math.isfinite(x) for x in values):
        raise ValueError('Nonfinite house_spawn')
    return values


def home_render_meshes(project, layout=None, *, include_mid=False):
    """Resolve the optional modular home list without escaping its model bank.

    Paths in ``layout.render_meshes`` name near GLBs relative to the project;
    each offset places that GLB back in the house's local metre coordinates.
    Mid siblings use the same offset and ``_mid.glb`` filename convention.
    An absent list retains the original single home/home_mid contract. QA may
    symlink the model bank read-only, but individual files cannot escape the
    resolved ``models/opening`` bank through symlinks or path traversal.
    """
    project = Path(project)
    if layout is None:
        layout = json.loads((project/'opening/layout.json').read_text())
    entries = layout.get('render_meshes', [dict(path='models/opening/home.glb',
                                               offset=[0., 0., 0.])])
    if not isinstance(entries, list) or not 1 <= len(entries) <= 16:
        raise ValueError('Home render_meshes must contain 1–16 near mesh records')
    bank = (project/'models/opening').resolve(strict=True)
    if not bank.is_dir():
        raise ValueError('Opening model bank must be a directory')
    result = []
    seen = set()
    for item in entries:
        if not isinstance(item, dict):
            raise ValueError('Invalid home render mesh record')
        value = item.get('path')
        if not isinstance(value, str) or '\\' in value or '\x00' in value:
            raise ValueError('Invalid home render mesh path')
        relative = PurePosixPath(value)
        if (relative.is_absolute() or '..' in relative.parts or
                str(relative) != value or relative.parts[:2] != ('models', 'opening') or
                len(relative.parts) < 3 or relative.suffix != '.glb' or
                relative.name.endswith('_mid.glb')):
            raise ValueError('Home near mesh must be a canonical models/opening/*.glb path')
        offset = item.get('offset')
        if (not isinstance(offset, list) or len(offset) != 3 or
                any(isinstance(v, bool) or not isinstance(v, (int, float)) or
                    not math.isfinite(v) or abs(v) > 1e6 for v in offset)):
            raise ValueError('Invalid home render mesh offset')
        variants = [('near', relative)]
        if include_mid:
            variants.append(('mid', relative.with_name(relative.stem+'_mid.glb')))
        for tier, path in variants:
            if str(path) in seen:
                raise ValueError('Duplicate home render mesh path')
            seen.add(str(path))
            candidate = project/str(path)
            resolved = candidate.resolve(strict=True)
            if not resolved.is_relative_to(bank) or not resolved.is_file():
                raise ValueError('Home render mesh escaped the opening model bank')
            result.append(dict(path=str(path), offset=list(map(float, offset)), tier=tier))
    return result


def authored_shots(project):
    project = Path(project)
    layout = json.loads((project/'opening/layout.json').read_text())
    origin = np.asarray(layout['house_origin'], dtype=float)
    if origin.shape != (3,) or not np.isfinite(origin).all():
        raise ValueError('Invalid house origin')
    spawn = _read_spawn(project)
    eye = np.asarray([spawn[0], spawn[1]+1.75, spawn[2]])
    target = eye+np.asarray([math.sin(spawn[3]), math.tan(spawn[4]), -math.cos(spawn[3])])
    def shot(name, seconds, eye_from, eye_to, target_from, target_to, fov_from, fov_to, subtitle, purpose):
        rail = [*map(lambda x: (np.asarray(x)+origin).tolist(),
                     [eye_from, eye_to, target_from, target_to])]
        return dict(name=name, duration=seconds, eye_from=rail[0], eye_to=rail[1],
                    target_from=rail[2], target_to=rail[3], fov_from=fov_from,
                    fov_to=fov_to, subtitle=subtitle, purpose=purpose)
    shots = [
        shot('back-from-work', 3.2,
             [.20, 1.42, 3.05], [.35, 1.36, 3.22],
             [1.48, 1.16, 4.56], [1.56, 1.14, 4.57], .87, .82,
             'Another late shift.',
             'Entry work bag, hooks and closed door establish an ordinary exhausted arrival; no unseen character animation is claimed.'),
        shot('still-behind', 4.1,
             [-2.54, 1.48, 2.00], [-2.54, 1.40, 1.89],
             [-2.55, .817, 1.055], [-2.55, .817, 1.055], .69, .64,
             "Rent. Power. Food. Somehow I'm still behind.",
             'Read the original matte rent reminder on the coffee table without a floating UI document.'),
        shot('one-quiet-night', 4.2,
             [-2.72, 1.57, 2.68], [-2.67, 1.52, 2.45],
             [-2.799567, 1.267020, -.789], [-2.799567, 1.267020, -.789], .86, .80,
             'Just one quiet night.',
             'Hold on the actual standby TV and console; the game starts only when the player chooses it.'),
        shot('room-to-breathe', 4.5,
             [-.60, 1.85, 3.00], (eye-origin).tolist(),
             [-2.10, 1.25, 1.20], (target-origin).tolist(), .96, FOV_PLAY,
             '',
             'A gentle look up settles into the exact playable standing camera before controls and the objective appear.'),
    ]
    return shots, origin, spawn, layout


def _triangles(path):
    raw = Path(path).read_bytes()
    magic, version, total = struct.unpack_from('<III', raw)
    if (magic, version, total) != (0x46546c67, 2, len(raw)):
        raise ValueError('Invalid home GLB')
    length, kind = struct.unpack_from('<II', raw, 12)
    if kind != 0x4e4f534a:
        raise ValueError('Missing GLB JSON')
    doc = json.loads(raw[20:20+length])
    binary_length, kind = struct.unpack_from('<II', raw, 20+length)
    if kind != 0x004e4942:
        raise ValueError('Missing GLB binary')
    binary = raw[28+length:28+length+binary_length]
    def array(index):
        accessor = doc['accessors'][index]
        view = doc['bufferViews'][accessor['bufferView']]
        width = {'SCALAR': 1, 'VEC3': 3}[accessor['type']]
        dtype = {5126: '<f4', 5125: '<u4', 5123: '<u2', 5121: 'u1'}[accessor['componentType']]
        size = np.dtype(dtype).itemsize
        return np.ndarray((accessor['count'], width), dtype, binary,
                          view.get('byteOffset', 0)+accessor.get('byteOffset', 0),
                          strides=(view.get('byteStride', width*size), size)).copy()
    triangles = []
    for node in doc['nodes']:
        if any(k in node for k in ('matrix', 'translation', 'rotation', 'scale', 'children')):
            raise ValueError('Cinematic audit requires explicitly baked home geometry')
        if 'mesh' not in node:
            continue
        for primitive in doc['meshes'][node['mesh']]['primitives']:
            points = array(primitive['attributes']['POSITION'])
            indices = array(primitive['indices']).ravel()
            triangles.append(points[indices].reshape(-1, 3, 3))
    return np.concatenate(triangles).astype(float)


def _first_hit(triangles, eye, target):
    """Closest two-sided real triangle hit along an eye/target segment."""
    delta = target-eye
    length = np.linalg.norm(delta)
    direction = delta/length
    a = triangles[:, 0]
    ab = triangles[:, 1]-a
    ac = triangles[:, 2]-a
    p = np.cross(np.broadcast_to(direction, ac.shape), ac)
    det = np.einsum('ij,ij->i', ab, p)
    valid = abs(det) > 1e-10
    inverse = np.zeros_like(det)
    inverse[valid] = 1/det[valid]
    s = eye-a
    u = np.einsum('ij,ij->i', s, p)*inverse
    q = np.cross(s, ab)
    v = np.einsum('j,ij->i', direction, q)*inverse
    distance = np.einsum('ij,ij->i', ac, q)*inverse
    valid &= (u >= -1e-8) & (v >= -1e-8) & (u+v <= 1+1e-8) & (distance > .01) & (distance < length-.035)
    return None if not np.any(valid) else float(np.min(distance[valid]))


def audit_shots(project, shots, origin, layout):
    project = Path(project)
    meshes = home_render_meshes(project, layout)
    triangles = np.concatenate([_triangles(project/item['path']) +
                                np.asarray(item['offset'], dtype=float)
                                for item in meshes])
    blockers = layout['collision_boxes']
    evidence = []
    failures = []
    for shot in shots:
        collisions = []
        occluded = []
        for index in range(33):
            t = index/32
            # Rail positions are affine; quintic easing changes time only.
            eye = np.asarray(shot['eye_from'])*(1-t)+np.asarray(shot['eye_to'])*t-origin
            target = np.asarray(shot['target_from'])*(1-t)+np.asarray(shot['target_to'])*t-origin
            for blocker in blockers:
                center = np.asarray(blocker['center'])
                half = np.asarray(blocker['size'])/2
                # A small lens margin catches rails near a wall/cabinet, not
                # merely a center point just outside the coarse blocker.
                if np.all(abs(eye-center) < half+.055):
                    collisions.append(dict(sample=index, collider=blocker['name']))
            hit = _first_hit(triangles, eye, target)
            if hit is not None:
                occluded.append(dict(sample=index, hit_m=round(hit, 6),
                                     target_m=round(float(np.linalg.norm(target-eye)), 6)))
        if collisions or occluded:
            failures.append(shot['name'])
        evidence.append(dict(name=shot['name'], samples=33,
                             eye_collisions=collisions, blocked_target_segments=occluded))
    return dict(passed=not failures, failures=failures, triangles=len(triangles),
                sample_count=33*len(shots), near_margin_m=.055,
                target_surface_tolerance_m=.035, shots=evidence,
                render_meshes=[dict(item, sha256=_sha(project/item['path'])) for item in meshes],
                scope='CPU eye-rail versus accepted blocker boxes and the union of actual two-sided near GLB triangles translated by their authored local offsets. Native per-shot composition review is still required.')


def write_home_cinematic(project):
    project = Path(project)
    if project.is_symlink() or not project.is_dir():
        raise ValueError('Cinematic project must be an ordinary private directory')
    support = project/'opening'
    if support.is_symlink() or not support.is_dir():
        raise ValueError('Cinematic support must be a private ordinary directory')
    for relative in (SETTINGS, META):
        if (project/relative).exists() or (project/relative).is_symlink():
            raise ValueError('Cinematic support already exists; use a new private project')
    shots, origin, spawn, layout = authored_shots(project)
    meshes = home_render_meshes(project, layout, include_mid=True)
    audit = audit_shots(project, shots, origin, layout)
    if not audit['passed']:
        raise ValueError('Unsafe cinematic rail: '+json.dumps(audit))
    rows = [FORMAT]
    for shot in shots:
        rail = [*shot['eye_from'], *shot['eye_to'], *shot['target_from'], *shot['target_to'],
                shot['fov_from'], shot['fov_to']]
        rows.append('shot '+format(shot['duration'], '.17g')+' '+
                    ' '.join(format(x, '.17g') for x in rail)+' '+
                    json.dumps(shot['subtitle'], ensure_ascii=True))
    rows.append('end')
    payload = ('\n'.join(rows)+'\n').encode('ascii')
    if len(payload) > 8192 or len(shots) > 6:
        raise ValueError('Cinematic exceeds bounded parser contract')
    (project/SETTINGS).write_bytes(payload)
    meta = dict(schema='valwood.home-cinematic', version=1, original_authored=True,
                seconds=sum(s['duration'] for s in shots), shots=shots,
                settings_sha256=_sha(project/SETTINGS),
                home_sha256=_sha(project/'models/opening/home.glb'),
                home_mid_sha256=_sha(project/'models/opening/home_mid.glb'),
                model_sha256={item['path']: _sha(project/item['path']) for item in meshes},
                render_meshes=meshes,
                layout_sha256=_sha(project/'opening/layout.json'),
                origin=origin.tolist(), house_spawn=spawn,
                endpoint_exact_spawn=True, handoff_fov=FOV_PLAY,
                cpu_sightline_audit=audit,
                geometry_modified=False, collision_modified=False,
                policy='Original authored camera rails and inner thoughts; no new assets, body or hand animation, voice recording, or shared source changes.')
    (project/META).write_text(json.dumps(meta, indent=2)+'\n', encoding='ascii')
    return meta


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--project', type=Path, required=True)
    parser.add_argument('--audit-only', action='store_true')
    args = parser.parse_args()
    if args.audit_only:
        shots, origin, _, layout = authored_shots(args.project)
        result = audit_shots(args.project, shots, origin, layout)
    else:
        result = write_home_cinematic(args.project)
    print(json.dumps(result, indent=2))
    return 0 if result.get('passed', True) else 1


if __name__ == '__main__':
    raise SystemExit(main())
