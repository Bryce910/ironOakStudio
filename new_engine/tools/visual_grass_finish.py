#!/usr/bin/env python3
"""Compose irregular meadow grass without changing projects or purchased assets.

Twelve shared HIGH/MID root sites replace the old four-by-three planting grid.
Short shoots fill around narrower, uneven tall grass. The same owned 512-square
alpha page has a restrained linear-light root darkening; alpha stays identical.
This produces candidates only. No engine, shaders, projects or saves are edited.
"""
from pathlib import Path
import argparse
import hashlib
import io
import json
import struct

import numpy as np
from PIL import Image

from cook_bep_reforged_vegetation import Cook, ROOT, normalize
from cook_bep_countryside_materials import linear, srgb


DEFAULT_OUT = ROOT / 'docs/visual-cohesion/2026-11-04/grass'
DEFAULT_PROJECT = ROOT / 'build/opening-finward-reference/v13/project'
SOURCE_TEXTURE = ROOT / 'assets/purchased/bep_reforged/countryside-grass-clumps/textures/d9426dc830cfeb7056db_base.png'
SOURCE_GUID = '4cacb6d81ae591f43a0421451adf40a3'


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def glb(path):
    data = Path(path).read_bytes()
    assert struct.unpack_from('<III', data) == (0x46546C67, 2, len(data))
    size, kind = struct.unpack_from('<II', data, 12)
    assert kind == 0x4E4F534A
    doc = json.loads(data[20:20+size])
    start = 20 + size
    size, kind = struct.unpack_from('<II', data, start)
    assert kind == 0x004E4942
    binary = data[start+8:start+8+size]
    return doc, binary


def array(doc, binary, index):
    accessor = doc['accessors'][index]
    view = doc['bufferViews'][accessor['bufferView']]
    dtype = {5126: '<f4', 5125: '<u4', 5123: '<u2', 5121: 'u1'}[accessor['componentType']]
    width = {'SCALAR': 1, 'VEC2': 2, 'VEC3': 3, 'VEC4': 4}[accessor['type']]
    assert 'byteStride' not in view
    return np.frombuffer(binary, dtype=dtype, count=accessor['count']*width,
                         offset=view.get('byteOffset', 0)+accessor.get('byteOffset', 0)).reshape(-1, width)


def inspect(path):
    doc, binary = glb(path)
    points = np.concatenate([array(doc, binary, p['attributes']['POSITION'])
                             for p in doc['meshes'][0]['primitives']])
    images = []
    for image in doc.get('images', []):
        view = doc['bufferViews'][image['bufferView']]
        data = binary[view.get('byteOffset', 0):view.get('byteOffset', 0)+view['byteLength']]
        im = Image.open(io.BytesIO(data)).convert('RGBA')
        images.append(dict(size=list(im.size), rgba_sha256=hashlib.sha256(im.tobytes()).hexdigest(),
                           gpu_RGBA8_mips_bytes=int(sum(max(1, im.width >> k)*max(1, im.height >> k)*4
                                                       for k in range(1+int(np.log2(max(im.size))))))))
    return dict(path=str(Path(path).resolve()), sha256=sha(path), bytes=Path(path).stat().st_size,
                raw_bounds=[points.min(0).tolist(), points.max(0).tolist()],
                vertices=len(points), triangles=sum(len(array(doc, binary, p['indices']))//3
                                                    for p in doc['meshes'][0]['primitives']),
                images=images, materials=doc['materials'])


def sources(project):
    aliases = {}
    for line in (project / 'world.studio').read_text().splitlines():
        bits = line.split()
        if bits and bits[0] == 'asset' and bits[1] in ['bep_nature_grass_long', 'bep_nature_grass_broad']:
            stem = project / 'models' / bits[2]
            aliases[bits[1]] = dict(near=Path(str(stem)+'.glb'), mid=Path(str(stem)+'_mid.glb'),
                                   studio_row=line)
    assert len(aliases) == 2
    return aliases


def geometry(alias, bounds):
    """Return two LODs sharing roots and exactly the same HIGH normalization."""
    broad = alias.endswith('broad')
    rng = np.random.default_rng(732190 if broad else 196724)
    # Deliberately unequal clusters and interstitial shoots; no repeated grid.
    centers = np.array([[-.76, -.37], [.64, -.21], [-.10, .71]])
    clusters = [0, 0, 0, 1, 1, 1, 2, 2, 0, 1, 2, 2]
    offsets = np.array([[-.32, -.33], [.16, -.49], [.23, .09], [-.21, -.48],
                        [.37, -.05], [-.02, .31], [-.44, -.08], [.34, .31],
                        [-.39, .44], [.31, .53], [-.39, .25], [.44, -.19]])
    roots = np.array([[*(centers[c]+offsets[i]+rng.uniform(-.105, .105, 2))]
                      for i, c in enumerate(clusters)])
    roots = np.column_stack((roots[:, 0], np.zeros(12), roots[:, 1]))
    short = np.array([i >= (6 if broad else 8) for i in range(12)])
    heights = rng.uniform(.30 if broad else .37, .59 if broad else .75, 12)
    heights[short] = rng.uniform(.14 if broad else .13, .27 if broad else .25, int(short.sum()))
    heights[2] = .79  # A single upper silhouette, rather than one repeated hedge.
    widths = rng.uniform(.43 if broad else .34, .72 if broad else .61, 12)
    widths[short] = rng.uniform(.51, .82, int(short.sum()))
    headings = rng.uniform(-np.pi, np.pi, 12)
    bends = rng.uniform(-.10, .10, (12, 2))
    # 6*2+6*4 or 8*2+4*5 = original 36 cards/144 triangles.
    plane_counts = [4 if s else 2 for s in short] if broad else [5 if s else 2 for s in short]
    assert sum(plane_counts) == 36
    all_lods = []
    for lod in range(2):
        positions, normals, tangents, uv, indices = [], [], [], [], []
        per_site = []
        for site in range(12):
            count = plane_counts[site]
            first_vertex = len(positions)
            # MID retains primary card at all twelve exact root/height sites.
            for plane in range(count if lod == 0 else 1):
                angle = headings[site] + plane*np.pi/count
                right = np.array([np.cos(angle), 0., np.sin(angle)])
                face = np.cross(right, [0., 1., 0.])
                normal = normalize(np.array([face+[0., .55, 0.]]))[0]
                offset = len(positions)
                for level, v in [(0., 1.), (.55, .45), (1., 0.)]:
                    bend = np.array([bends[site, 0], 0., bends[site, 1]])*level*level
                    for side, u in [(-.5, 0.), (.5, 1.)]:
                        positions.append(roots[site]+right*widths[site]*side+[0., heights[site]*level, 0.]+bend)
                        normals.append(normal)
                        tangents.append([*right, -1.])
                        # Alternate mirroring breaks the atlas's distinctive leaf arc.
                        uv.append([1.-u if site % 2 else u, v])
                indices.extend((np.array([[0, 1, 2], [1, 3, 2], [2, 3, 4], [3, 5, 4]])+offset).tolist())
            per_site.append(dict(first_vertex=first_vertex, card_count=count if lod == 0 else 1))
        position = np.array(positions)
        if lod == 0:
            lo, hi = position.min(0), position.max(0)
            targetlo, targethi = np.asarray(bounds)
            scale = (targethi-targetlo)/(hi-lo)
            shift = targetlo-lo*scale
        position = (position*scale+shift).astype(np.float32)
        verts = dict(position=position, normal=normalize(np.array(normals)/scale).astype(np.float32),
                     tangent=np.column_stack((normalize(np.array(tangents)[:, :3]*scale),
                                               np.array(tangents)[:, 3])).astype(np.float32),
                     uv0=np.array(uv, np.float32), uv1=np.zeros((len(position), 2), np.float32),
                     color=np.ones((len(position), 4), np.float32))
        all_lods.append((verts, np.asarray(indices, np.uint32), per_site))
    roots = (roots*scale+shift).astype(np.float32)
    actual_heights = heights*scale[1]
    # Root midpoints are a strict roundtrip-compatible grounding invariant.
    for verts, _, per_site in all_lods:
        reconstructed = np.array([(verts['position'][site['first_vertex']]+verts['position'][site['first_vertex']+1])*.5
                                   for site in per_site])
        assert np.allclose(reconstructed, roots, atol=2e-7)
        assert np.all(verts['position'][:, 1] >= 0.)
        assert np.allclose(np.linalg.norm(verts['normal'], axis=1), 1., atol=2e-7)
    for near_site, mid_site in zip(all_lods[0][2], all_lods[1][2]):
        ni, mi = near_site['first_vertex'], mid_site['first_vertex']
        assert np.array_equal(all_lods[0][0]['position'][ni:ni+6], all_lods[1][0]['position'][mi:mi+6])
    return all_lods, dict(root_sites=roots.tolist(), short_shoot_sites=np.flatnonzero(short).tolist(),
                          heights_m=actual_heights.tolist(), widths_m=(widths*scale[0]).tolist(),
                          primary_headings_rad=headings.tolist(), near_cards_per_site=plane_counts,
                          near_mid_root_midpoints_equal=True, source_grid_replaced=True)


def run(project, output):
    output.mkdir(parents=True, exist_ok=True)
    original = sources(project)
    before = {a: {lod: inspect(paths[lod]) for lod in ['near', 'mid']} for a, paths in original.items()}
    texture = np.asarray(Image.open(SOURCE_TEXTURE).convert('RGBA')).copy()
    assert texture.shape == (512, 512, 4)
    # Source v=1 is the root. Keep top half unchanged; fade down to .76 linear.
    row = np.arange(512, dtype=float)/511
    root = np.clip((row-.43)/.57, 0., 1.)
    attenuation = 1.-.24*root*root*(3.-2.*root)
    tex = texture.copy()
    tex[:, :, :3] = np.uint8(np.rint(np.clip(srgb(linear(texture[:, :, :3]/255.)*attenuation[:, None, None]), 0., 1.)*255.))
    assert np.array_equal(tex[:, :, 3], texture[:, :, 3])
    cook = Cook(output)
    image_path = cook.image(tex/255., 'root-grounded-grass', 512)
    common = dict(base=image_path, normal=None, arm=None, roughness=1., metallic=0.,
                  alphaMode='MASK', alphaCutoff=.35, doubleSided=True, facade_mode=0, source_paint=False)
    manifest = dict(schema=1, scope='Two owned-source candidate grass aliases only; no project/shader changes',
                    source_texture=dict(path=str(SOURCE_TEXTURE), sha256=sha(SOURCE_TEXTURE), owned_guid=SOURCE_GUID),
                    prepared_texture=dict(path=image_path, sha256=sha(image_path), size=[512, 512],
                                          alpha_identical=True, root_linear_multiplier=.76,
                                          tip_linear_multiplier=1.), assets=[], no_new_source_assets=True)
    for alias, paths in original.items():
        source_bounds = before[alias]['near']['raw_bounds']
        lods, composition = geometry(alias, source_bounds)
        material = alias+'_irregular_meadow'
        factor = [.84, .96, .78, 1.] if alias.endswith('broad') else [.78, 1., .78, 1.]
        cook.rows[material] = dict(common, name=material, factor=factor)
        outputs = []
        for lod, (v, ix, site_data) in zip(['near', 'mid'], lods):
            path = output / 'models' / (alias+'_'+lod+'.glb')
            cook.emit(path, v, [(ix, material)], 0., source_ao=False)
            check = inspect(path)
            assert check['raw_bounds'][0][1] == 0.
            assert check['triangles'] <= before[alias][lod]['triangles']
            assert check['images'][0]['size'] == [512, 512]
            assert len(check['images']) == 1
            assert np.allclose(check['raw_bounds'][1][1], source_bounds[1][1], atol=1e-7)
            # MID shares exact HIGH transform and lies inside original HIGH bounds.
            assert np.all(np.asarray(check['raw_bounds'][0]) >= np.asarray(source_bounds[0])-2e-7)
            assert np.all(np.asarray(check['raw_bounds'][1]) <= np.asarray(source_bounds[1])+2e-7)
            if lod == 'near':
                assert np.array_equal(np.asarray(check['raw_bounds']), np.asarray(source_bounds))
            check['lod'] = lod
            outputs.append(check)
        manifest['assets'].append(dict(alias=alias, studio_row=paths['studio_row'], baseline=before[alias],
                                       candidates=outputs, composition=composition,
                                       bounds_and_registration_unchanged=True, wind_root_y=0.,
                                       near_mid_root_sites_exact=True, no_geometry_or_texture_budget_increase=True))
    assert all(sha(original[a][lod]) == before[a][lod]['sha256'] for a in original for lod in ['near', 'mid'])
    manifest['original_assets_unchanged'] = True
    manifest['unique_texture_gpu_RGBA8_mips_bytes'] = 1398100
    manifest['helper_sha256'] = sha(Path(__file__))
    (output / 'CANDIDATE_REVIEW.json').write_text(json.dumps(manifest, indent=2, allow_nan=False)+'\n')
    (output / 'materials.json').write_text(json.dumps(cook.rows, indent=2)+'\n')
    print(json.dumps(dict(review=str(output / 'CANDIDATE_REVIEW.json'),
                          assets=[dict(alias=a['alias'], files=[c['path'] for c in a['candidates']])
                                  for a in manifest['assets']]), indent=2))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--project', type=Path, default=DEFAULT_PROJECT)
    parser.add_argument('--output', type=Path, default=DEFAULT_OUT)
    args = parser.parse_args()
    run(args.project, args.output)
