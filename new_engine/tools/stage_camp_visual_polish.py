#!/usr/bin/env python3
"""Cook a private camp finish and expose a guarded, reversible C12 patch.

No staging command writes a project or shared model. Only explicit --apply
repoints C12's private aliases and changes three ordinary C12 data files.
"""
from pathlib import Path
import argparse
import copy
import hashlib
import json
import math
import os
import struct
import sys

import numpy as np

from build_opening_prologue import Glb, array, read_glb
from build_arrival_camp_assets import terrain
from visual_ground_finish import segment_distance

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'build/journey-opening/candidate-v11/project'
OUT = ROOT / 'docs/scenery-polish/2026-11-05/camp-v1'
CENTER = np.array([-1486.37664868499, 18., -10224.9106110642])
ALIAS = 'bep_opening_camp_ground_finish'
ASSET_ROW = 'asset ' + ALIAS + ' opening/camp_ground_finish 1 0 0 110 30 1'
WIDTH = 100800.
DEPTH = WIDTH * 2818 / 5009
INSTANCE_ROW = (f'{ALIAS} {CENTER[0]/WIDTH+.5:.17g} {CENTER[2]/DEPTH+.5:.17g}'
                ' 0 1 0 0 0 0 0 1 1 1 18')
FACTOR_CHANGES = {
    'Weathered peeled camp branches': [.62, .56, .45, 1.],
    'Pale exposed cut timber': [.48, .36, .22, 1.],
    'Charcoal and dry splits': [.065, .052, .041, 1.],
    'Dry endgrain rings': [.25, .17, .10, 1.],
}


def sha(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def dump(path, value):
    with path.open('x') as stream:
        json.dump(value, stream, indent=2, allow_nan=False)
        stream.write('\n')


def write_glb(path, doc, binary):
    payload = json.dumps(doc, separators=(',', ':')).encode()
    payload += b' ' * (-len(payload) % 4)
    data = bytes(binary) + b'\0' * (-len(binary) % 4)
    path.write_bytes(struct.pack('<III', 0x46546c67, 2, 28+len(payload)+len(data)) +
                     struct.pack('<II', len(payload), 0x4e4f534a) + payload +
                     struct.pack('<II', len(data), 0x004e4942) + data)


class SmallGlb(Glb):
    def texture(self, raw, size=512):
        return super().texture(raw, min(size, 512))


def model_review(path):
    doc, binary = read_glb(path)
    positions = np.concatenate([array(doc, binary, p['attributes']['POSITION'])
                                for m in doc['meshes'] for p in m['primitives']])
    return dict(path=str(path), sha256=sha(path), bytes=path.stat().st_size,
                vertices=len(positions), triangles=sum(doc['accessors'][p['indices']]['count']//3
                    for m in doc['meshes'] for p in m['primitives']),
                minimum=positions.min(0).tolist(), maximum=positions.max(0).tolist(),
                materials=len(doc['materials']), images=len(doc.get('images', [])))


def darker_camp(lod, out):
    source = SOURCE / f'models/opening/arrival_camp_{lod}.glb'
    original, binary = read_glb(source)
    doc = copy.deepcopy(original)
    changes = []
    for material in doc['materials']:
        if material.get('name') not in FACTOR_CHANGES:
            continue
        pbr = material['pbrMetallicRoughness']
        before = pbr['baseColorFactor'][:]
        pbr['baseColorFactor'] = FACTOR_CHANGES[material['name']]
        changes.append(dict(material=material['name'], before=before,
                            after=pbr['baseColorFactor']))
    assert len(changes) == 4
    path = out / f'arrival_camp_{lod}.glb'
    write_glb(path, doc, binary)
    check, after_binary = read_glb(path)
    assert after_binary == binary
    restored = copy.deepcopy(check)
    for target, baseline in zip(restored['materials'], original['materials']):
        if target.get('name') in FACTOR_CHANGES:
            target['pbrMetallicRoughness']['baseColorFactor'] = baseline['pbrMetallicRoughness']['baseColorFactor']
    assert restored == original, 'Only the four material color factors may change'
    review = model_review(path)
    review.update(source=str(source.resolve()), source_alias=str(source), source_sha256=sha(source),
                  material_changes=changes, binary_payload_exact=True,
                  positions_normals_indices_uvs_images_nodes_bounds_exact=True)
    return review


def ground_finish(out):
    surface, catalog = terrain(SOURCE)
    g = SmallGlb()
    maps = ROOT / 'assets/polyhaven/castle_village'
    leaves = ROOT / 'assets/polyhaven/visual_upgrade/forest_leaves_03'
    char = g.material('Charred soil between cooking logs', [.095, .083, .068], 1.,
                      base=maps/'wood_base.png', normal=maps/'wood_normal.png')
    ash = g.material('Loose cold ash around the live hearth', [.40, .38, .34], 1.,
                     base=maps/'stone_base.png', normal=maps/'stone_normal.png')
    litter = []
    for name, factor in [('Dry beech litter', [.72, .68, .56]), ('Weathered dark litter', [.55, .52, .43])]:
        litter.append(g.material(name, factor, .98, base=leaves/'diff.png',
                                 normal=leaves/'nor_gl.png', arm=leaves/'arm.png'))
    geometry, records = [], []

    def patch(name, center, rx, rz, angle, material, seed, offset=.007, segments=15):
        rng = np.random.default_rng(seed)
        phase = rng.uniform(0, math.tau)
        theta = np.arange(segments)*math.tau/segments
        radius = 1 + .13*np.sin(theta*3+phase) + .09*np.sin(theta*5-phase*.7)
        local = np.column_stack((np.cos(theta)*radius*rx, np.sin(theta)*radius*rz))
        rotation = np.array([[math.cos(angle), -math.sin(angle)], [math.sin(angle), math.cos(angle)]])
        xz = np.vstack((np.zeros((1, 2)), local)) @ rotation.T + np.asarray(center)
        world = xz + CENTER[[0, 2]]
        heights = surface.sample(world, 0)
        p = np.column_stack((xz[:, 0], heights-CENTER[1]+offset, xz[:, 1]))
        normals = np.tile([0., 1., 0.], (len(p), 1))
        uv = world*.73 + rng.uniform(-4, 4, 2)
        # Perimeter is deliberately broken by holes in leaf patches. Char/ash
        # pieces are small disconnected islands rather than a broad round decal.
        ix = []
        for j in range(segments):
            if 'litter' in name and (j+seed) % 5 == 0:
                continue
            ix.extend([0, 1+(j+1) % segments, 1+j])
        g.mesh(p, normals, uv, ix, material)
        geometry.append((p, np.asarray(ix).reshape(-1, 3)))
        records.append(dict(name=name, local_xz=list(map(float, center)), rx=rx, rz=rz,
                            seed=seed, vertices=len(p), triangles=len(ix)//3,
                            minimum_height_above_high_m=float(offset)))

    patch('small irregular charred hearth', [0, 0], .56, .50, .13, char, 3041, .009, 36)
    # Ash spills between the ring stones, with sparse grains beyond it.
    for i, (x, z, rx, rz) in enumerate([(-.62, .05, .17, .11), (.61, -.08, .14, .11),
            (-.16, -.66, .19, .08), (.24, .66, .15, .08), (-.77, -.28, .095, .055),
            (.75, .33, .10, .055), (-.35, .80, .08, .045), (.40, -.81, .07, .045),
            (-.91, .08, .045, .035), (.86, -.48, .04, .03)]):
        patch('irregular loose ash', [x, z], rx, rz, i*.83, ash, 3100+i, .006, 11)
    # Existing routes and traveler approaches remain visibly clear. These
    # render-only details never add collider proxies or touch authored soil.
    routes = json.loads((SOURCE/'opening/first-journey.routes.json').read_text())['paths']
    segments = []
    for points in routes.values():
        p = np.asarray(points, float)[:, [0, 2]]-CENTER[[0, 2]]
        segments.extend(np.stack((p[:-1], p[1:]), axis=1).tolist())
    qa = np.array([[0, 5.2], [-2.15, 1.8], [2.2, 1.8], [3.6, 1.], [3.6, -2.],
                   [1.9, -2.], [1.9, -2.9], [1.9, -2.], [3.6, -2.], [3.6, 3.5], [0, 5.2]])
    segments.extend(np.stack((qa[:-1], qa[1:]), axis=1).tolist())
    segments = np.asarray(segments)
    actors = np.array([[-2.15, -.65], [2.2, -.65], [.1, -3.2]])
    candidates = [(-5.8, -2.3), (-5.8, -4.6), (-4.8, -5.9), (-2.8, -5.9),
                  (-1.3, -5.6), (.8, -5.4), (2.4, -5.5), (4.4, -4.4),
                  (5.4, -2.8), (5.6, -.2), (5.5, 2.4), (4.7, 4.4),
                  (2.4, 5.7), (-2.7, 5.5), (-4.7, 3.4), (-5.8, 1.3)]
    rejected = []
    for i, center in enumerate(candidates):
        radius = .36+.04*(i % 3)
        clearance = float(segment_distance(np.asarray([center]), segments)[0]-radius*1.23)
        actor_clearance = float(np.linalg.norm(actors-center, axis=1).min()-radius*1.23)
        if clearance < .70 or actor_clearance < 1.0:
            rejected.append(dict(local_xz=center, lane_clearance=clearance, actor_clearance=actor_clearance))
            continue
        patch('broken leaf litter', center, radius, radius*.67, i*.71, litter[i % 2], 3300+i, .006, 18)
        # Detached leaf fragments dissolve each patch into the surrounding soil.
        for k in range(3):
            a=i*.71+k*2.1
            fragment=np.asarray(center)+[math.cos(a)*radius*.85, math.sin(a)*radius*.65]
            patch('litter fragment', fragment, .065, .035, a, litter[(i+k) % 2], 3500+i*3+k, .010, 8)
        records[-4]['lane_footprint_clearance_m'] = clearance
        records[-4]['actor_footprint_clearance_m'] = actor_clearance
    # Static loader normalizes minimumY. A zero-area invisible datum preserves
    # the authored 18m pivot while every visible point lies on actual terrain.
    datum = g.material('Invisible ground-finish pivot', [.1, .1, .1], 1.)
    g.mesh([[0, 0, 0]]*3, [[0, 1, 0]]*3, [[0, 0]]*3, [0, 1, 2], datum)
    path = out/'camp_ground_finish.glb'
    path.write_bytes(g.finish())
    contact = []
    for positions, triangles in geometry:
        for weights in [[1, 0, 0], [0, 1, 0], [0, 0, 1], [1/3]*3, [.6, .2, .2]]:
            points = (positions[triangles]*np.asarray(weights)[None, :, None]).sum(1)+CENTER
            contact.extend((points[:, 1]-surface.sample(points[:, [0, 2]], 0)).tolist())
    assert surface.fallback_queries == [0, 0]
    assert min(contact) >= -.002 and max(contact) <= .025, (min(contact), max(contact))
    review = model_review(path)
    review.update(origin_world=CENTER.tolist(), high_contact_error_m=dict(min=min(contact), max=max(contact), samples=len(contact)),
                  terrain_height_or_coverage_changed=False, terrain_render_geometry_changed=False,
                  collision_proxies_added=0, fallback_queries=surface.fallback_queries,
                  patches=records, rejected_litter_sites=rejected, source_catalog=str(catalog), source_catalog_sha256=sha(catalog),
                  detail_policy='Broken matte ground islands, no broad disk, transparent plane or wall of grass; existing routes remain clear.')
    return review


def stage():
    assert not OUT.exists(), 'Use a fresh output bank'
    OUT.mkdir(parents=True)
    models = OUT/'models'
    models.mkdir()
    protected = ['world.studio', 'showcase.flames', 'scenery/authored_instances.txt', 'city.physics',
                 'ground/countryside.height', 'ground/countryside.coverage', 'opening/arrival-camp.settings',
                 'opening/first-journey.settings', 'opening/first-journey.routes.json', 'opening/first-journey.town-people.settings']
    before = {name: sha(SOURCE/name) for name in protected}
    camp = [darker_camp(lod, models) for lod in ['near', 'mid']]
    ground = ground_finish(models)
    sources = {str(SOURCE/name): digest for name, digest in before.items()}
    for path in [ROOT/'assets/polyhaven/castle_village/provenance.json',
                 ROOT/'assets/polyhaven/castle_village/wood_base.png', ROOT/'assets/polyhaven/castle_village/wood_normal.png',
                 ROOT/'assets/polyhaven/castle_village/stone_base.png', ROOT/'assets/polyhaven/castle_village/stone_normal.png',
                 ROOT/'assets/polyhaven/visual_upgrade/forest_leaves_03/provenance.json',
                 ROOT/'assets/polyhaven/visual_upgrade/forest_leaves_03/diff.png',
                 ROOT/'assets/polyhaven/visual_upgrade/forest_leaves_03/nor_gl.png',
                 ROOT/'assets/polyhaven/visual_upgrade/forest_leaves_03/arm.png']:
        sources[str(path)] = sha(path)
    assert before == {name: sha(SOURCE/name) for name in protected}
    flame_rows = [row for row in (SOURCE/'showcase.flames').read_text().splitlines() if row and not row.startswith('VALWOOD_')]
    assert len(flame_rows) == 1
    fields = flame_rows[0].split()
    assert len(fields) == 7 and abs(float(fields[0])-CENTER[0]) < 1e-5 and abs(float(fields[2])-CENTER[2]) < 1e-5
    new_fields = fields[:]
    new_fields[3:6] = ['.45', '.46', '.22']
    recipe = dict(schema='valwood.camp-visual-polish', version=1, staged=True,
                  source_project=str(SOURCE), source_inputs_sha256=sources, camp_models=camp,
                  ground_finish=ground, fire=dict(before=flame_rows[0], after=' '.join(new_fields),
                    dimensions_intensity=[.45, .46, .22], existing_camp_light_power_unchanged=1.1),
                  studio_append=ASSET_ROW, authored_instances_append=INSTANCE_ROW,
                  allowed_data_changes=['showcase.flames', 'world.studio', 'scenery/authored_instances.txt'],
                  model_aliases_repointed=['models/opening/arrival_camp.glb', 'models/opening/arrival_camp_near.glb', 'models/opening/arrival_camp_mid.glb'],
                  model_aliases_added=['models/opening/camp_ground_finish.glb'],
                  immutable_story_collision_ground_unchanged=True, no_cpp_or_shader_changes=True,
                  source_project_unchanged=True, native_gpu_used=False, visual_acceptance_pending=True,
                  license='Existing owned models retained; new finish uses already-downloaded Poly Haven CC0 maps, local provenance bound above.',
                  recipe_sha256=sha(Path(__file__)))
    dump(OUT/'CAMP_REVIEW.json', recipe)
    print(json.dumps(dict(staged=True, review=str(OUT/'CAMP_REVIEW.json'), sha256=sha(OUT/'CAMP_REVIEW.json'),
                         camp_models=[m['sha256'] for m in camp], ground_triangles=ground['triangles'])))


def apply(project):
    project = Path(project).resolve(strict=True)
    assert project == ROOT/'build/journey-opening/candidate-v12/project', 'C12 private candidate only'
    review = json.loads((OUT/'CAMP_REVIEW.json').read_text())
    assert review['staged'] and review['no_cpp_or_shader_changes']
    aliases = project/'models/opening'
    assert aliases.is_dir() and not aliases.is_symlink()
    for row in review['camp_models']:
        target = Path(row['path'])
        assert sha(target) == row['sha256']
        names = ['arrival_camp.glb', 'arrival_camp_near.glb'] if target.name.endswith('_near.glb') else ['arrival_camp_mid.glb']
        for name in names:
            alias = aliases/name
            assert alias.is_symlink(), alias
            assert sha(alias) in [row['source_sha256'], row['sha256']], alias
    for rel in review['allowed_data_changes']:
        assert (project/rel).is_file() and not (project/rel).is_symlink(), rel
    flame = project/'showcase.flames'
    lines = flame.read_text().splitlines()
    assert lines.count(review['fire']['before']) + lines.count(review['fire']['after']) == 1
    studio = project/'world.studio'
    instances = project/'scenery/authored_instances.txt'
    assert sum(line.startswith('asset ') for line in studio.read_text().splitlines()) + (ASSET_ROW not in studio.read_text().splitlines()) <= 128
    audit = dict(before={rel: sha(project/rel) for rel in review['allowed_data_changes']},
                 aliases_before={str(p.relative_to(project)):str(p.resolve(strict=True)) for p in
                                 [aliases/'arrival_camp.glb', aliases/'arrival_camp_near.glb', aliases/'arrival_camp_mid.glb']})
    for row in review['camp_models']:
        target = Path(row['path'])
        for name in (['arrival_camp.glb', 'arrival_camp_near.glb'] if target.name.endswith('_near.glb') else ['arrival_camp_mid.glb']):
            alias = aliases/name
            alias.unlink()
            alias.symlink_to(target)
    target = Path(review['ground_finish']['path'])
    assert sha(target) == review['ground_finish']['sha256']
    alias = aliases/'camp_ground_finish.glb'
    if alias.exists() or alias.is_symlink():
        assert alias.is_symlink() and alias.resolve(strict=True) == target
    else:
        alias.symlink_to(target)
    lines = [review['fire']['after'] if line == review['fire']['before'] else line for line in lines]
    flame.write_text('\n'.join(lines)+'\n')
    for path, row in [(studio, ASSET_ROW), (instances, INSTANCE_ROW)]:
        lines = path.read_text().splitlines()
        if row not in lines:
            assert not any(ALIAS in line for line in lines), 'Conflicting finish alias'
            path.write_text(path.read_text().rstrip()+'\n'+row+'\n')
    audit.update(applied=True, after={rel:sha(project/rel) for rel in review['allowed_data_changes']},
                 review_sha256=sha(OUT/'CAMP_REVIEW.json'), target_project=str(project),
                 source_c11_unchanged=all(sha(Path(path)) == digest for path,digest in review['source_inputs_sha256'].items()),
                 native_gpu_used=False)
    assert audit['source_c11_unchanged']
    return audit


def revert(project):
    """Remove only this camp patch, retaining unrelated apartment changes."""
    project = Path(project).resolve(strict=True)
    assert project == ROOT/'build/journey-opening/candidate-v12/project'
    review = json.loads((OUT/'CAMP_REVIEW.json').read_text())
    aliases = project/'models/opening'
    assert aliases.is_dir() and not aliases.is_symlink()
    for row in review['camp_models']:
        names = ['arrival_camp.glb', 'arrival_camp_near.glb'] if Path(row['path']).name.endswith('_near.glb') else ['arrival_camp_mid.glb']
        for name in names:
            alias = aliases/name
            assert alias.is_symlink() and sha(alias) in [row['sha256'], row['source_sha256']]
            alias.unlink()
            alias.symlink_to(Path(row['source']))
    extra = aliases/'camp_ground_finish.glb'
    if extra.exists() or extra.is_symlink():
        assert extra.is_symlink() and extra.resolve(strict=True) == Path(review['ground_finish']['path'])
        extra.unlink()
    for path, row in [(project/'world.studio', ASSET_ROW),
                      (project/'scenery/authored_instances.txt', INSTANCE_ROW)]:
        assert path.is_file() and not path.is_symlink()
        path.write_text('\n'.join(line for line in path.read_text().splitlines() if line != row)+'\n')
    path = project/'showcase.flames'
    assert path.is_file() and not path.is_symlink()
    rows = path.read_text().splitlines()
    assert rows.count(review['fire']['before']) + rows.count(review['fire']['after']) == 1
    path.write_text('\n'.join(review['fire']['before'] if line == review['fire']['after'] else line for line in rows)+'\n')
    return dict(reverted=True, only_exact_camp_rows_removed=True, apartment_rows_untouched=True,
                target_project=str(project), source_c11_unchanged=True, native_gpu_used=False)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--apply', type=Path)
    parser.add_argument('--revert', type=Path)
    args = parser.parse_args()
    assert not (args.apply and args.revert)
    if args.revert:
        print(json.dumps(revert(args.revert), indent=2))
    elif args.apply:
        print(json.dumps(apply(args.apply), indent=2))
    else:
        stage()
