#!/usr/bin/env python3
"""Install the purchased publisher layout in an isolated, reproducible project.

Rebuilds the selected project's authored layout; preserves its settings, saves
and purchased source files. Spatial chunks retain exact source elevations via
the existing absolute pivot field rather than independent terrain grounding.
"""
from pathlib import Path
import argparse, hashlib, json, math, shutil, struct
import numpy as np
from import_world_asset import cook

ROOT = Path(__file__).resolve().parents[1]
PRIVATE = ROOT.parent / '.local-tools/unity-source'
X, Z, FLOOR = -43344., -23987.82415651827, 1130.8125
FILES = ['authored', 'starting_area', 'rainforest', 'castle_village',
         'tanthoon_bandit_camp', 'castle_streets', 'bep_environment',
         'demo_finish', 'demo_polish']

def private_models(project, original):
    models = project / 'models'
    if models.is_symlink():
        models.unlink()
    models.mkdir(exist_ok=True)
    for source in (original / 'models').iterdir():
        if source.name == 'imports':
            continue
        target = models / source.name
        if not target.exists() and not target.is_symlink():
            target.symlink_to(source.resolve(), target_is_directory=source.is_dir())
    (models / 'imports').mkdir(exist_ok=True)

def heightfield(project):
    heights = np.fromfile(PRIVATE / 'showcase-native/terrain/00-height.f32', '<f4').reshape(2049, 2049)
    values = np.ascontiguousarray(FLOOR + heights[::-1] * 400, dtype='<f4')
    path = project / 'ground/showcase.height'
    path.parent.mkdir(exist_ok=True)
    path.write_bytes(struct.pack('<8sIIdddd', b'VWHGT1\0\0', 2049, 2049,
                                X - 500, Z - 500, 1000 / 2048, 1000 / 2048) + values.tobytes())

def publisher_sky(project):
    folder = project / 'environments'
    if folder.is_symlink():
        folder.unlink()
    folder.mkdir(exist_ok=True)
    source = ROOT / 'assets/purchased/nature_castle/showcase/environment/publisher_cloudy_sky.hdr'
    if not source.is_file():
        return ''
    shutil.copy2(source, folder / source.name)
    return 'environment environments/publisher_cloudy_sky.hdr 154 1.86638 1 1\n'

def publisher_views(project):
    data = json.loads((ROOT / 'docs/stronghold/2026-11-01/showcase/publisher-camera-recommended.json').read_text())
    rows = []
    for name, key in [('stronghold-town', 'castle_cliff_and_heads'),
                      ('stronghold-gate', 'entrance_approach'),
                      ('stronghold-overlook', 'upper_courtyard'),
                      ('stronghold-hall', 'temple_hall_longitudinal')]:
        view = data[key]
        x, y, z = view['source_position']
        rows.append(f"{name} {X+x-500:.12g} {FLOOR+y:.12g} {Z+500-z:.12g} {view['native_yaw_radians']:.12g} {view['native_pitch_radians']:.12g}")
    (project / 'showcase.views').write_text('\n'.join(rows) + '\n')

def active_catalog(project):
    """Retain the installed visual revision when rebuilding this project."""
    metadata = project / 'publisher_showcase.json'
    if metadata.is_file():
        installed = json.loads(metadata.read_text())
        path = installed.get('active_geometry_catalog') or installed.get('visual_finish', {}).get('catalog')
        if path:
            candidate = Path(path)
            if not candidate.is_file():
                raise ValueError('Installed geometry catalog is missing: ' + str(candidate))
            return candidate
    return ROOT / 'assets/purchased/nature_castle/showcase_gi/catalog.json'

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--catalog', type=Path, help='Defaults to this project\'s installed visual revision')
    parser.add_argument('--extra-catalog', type=Path, default=ROOT / 'assets/purchased/nature_castle/showcase_lava_smooth/catalog.json')
    parser.add_argument('--project', type=Path, default=ROOT / 'projects/valwood_showcase')
    args = parser.parse_args()
    args.catalog = args.catalog or active_catalog(args.project)
    catalog = json.loads(args.catalog.read_text())
    report_file = args.catalog.parent / 'report.json'
    if not report_file.is_file():
        raise ValueError('Cook is incomplete: no final report')
    report = json.loads(report_file.read_text())
    declared_count = report.get('cooked_chunks', report.get('chunks', len(catalog)))
    if declared_count != len(catalog) or not 9 <= len(catalog) <= 128:
        raise ValueError('Catalog is incomplete or exceeds the native asset budget')
    for asset in catalog:
        paths = [Path(path) for path in asset.get('files', [])]
        if len(paths) != 2 or not all(path.is_file() for path in paths):
            raise ValueError('Catalog has an incomplete near/MID pair')
        expected = asset.get('file_sha256')
        if expected and expected != [hashlib.sha256(path.read_bytes()).hexdigest() for path in paths]:
            raise ValueError('Catalog geometry changed since validation: ' + asset['name'])
    if not all(a.get('lod_alignment', {}).get('shared_datum') for a in catalog):
        raise ValueError('Every source chunk requires a shared LOD elevation datum')
    if args.extra_catalog:
        catalog += json.loads(args.extra_catalog.read_text())
        if len(catalog) > 128 or not all(a.get('lod_alignment', {}).get('shared_datum') for a in catalog):
            raise ValueError('Combined source catalogs exceed native limits or lack aligned LODs')
    project = args.project.resolve()
    original = ROOT / 'projects/valwood_stronghold'
    project.mkdir(exist_ok=True)
    for source in original.iterdir():
        if source.name in ['models', 'ground', 'scenery', 'saves', 'shaders', 'environments', 'texture_blocks']:
            continue
        target = project / source.name
        if source.is_dir() and not target.exists():
            target.symlink_to(source.resolve(), target_is_directory=True)
    if not (project / 'shaders').is_dir():
        shutil.copytree(original / 'shaders', project / 'shaders')
    if not (project / 'graphics.settings').is_file():
        shutil.copy2(original / 'graphics.settings', project / 'graphics.settings')
    private_models(project, original)
    for folder in ['ground', 'scenery', 'saves']:
        (project / folder).mkdir(exist_ok=True)
    heightfield(project)
    publisher_views(project)
    if not (project / 'ground/grass.png').exists():
        for source in (original / 'ground').iterdir():
            if source.is_file() and not (project / 'ground' / source.name).exists():
                (project / 'ground' / source.name).symlink_to(source.resolve())
    definitions, records = [], []
    for index, asset in enumerate(catalog):
        alias = 'nm_scene_' + asset['name'].removeprefix('nms_')
        sources = [Path(p) for p in asset['files']]
        stem = cook(project, alias, (sources[0], sources[1], None), 1.)
        dx, dy, dz = asset['engine_local_anchor']
        x, y, z = X + dx, FLOOR + dy, Z + dz
        # Absolute source pivots are intentional for bridges, galleries, and
        # mountain backdrops; offset=0 keeps ordinary editor limits unchanged.
        u, v = x / 100800 + .5, z / (100800 * 2818 / 5009) + .5
        if not all(math.isfinite(a) for a in [u, v, y]) or not 0 < u < 1 or not 0 < v < 1:
            raise ValueError('Source chunk lies outside the world atlas: ' + alias)
        definitions.append(f'asset {alias} {stem} 2 0 0 2000 35 1')
        records.append(f'{alias} {u:.17g} {v:.17g} 0 1 0 0 0 0 0 1 1 1 {y:.12g}')
        print(f'Installed {index + 1}/{len(catalog)} {alias}', flush=True)
    for i, name in enumerate(FILES):
        (project / 'scenery' / f'{name}_instances.txt').write_text(
            '# Purchased publisher scene, exact source transforms and elevations.\n' +
            '\n'.join(records[i::len(FILES)]) + '\n')
    # Solve the native solar arc against the source sun matrix. Its source
    # intensity2.5 is reconstructed through the native clear/cloud multiplier.
    (project / 'world.studio').write_text('VALWOOD_STUDIO 7\nlook .25 1.881963264 1 1.0 16.313173943 -63.221994754 .25 0 .38 .18\n' + '\n'.join(definitions) + '\n' + publisher_sky(project) + 'entities "VALWOOD_ENTITIES 4 1 0"\nsurfaces "VALWOOD_SURFACES 2"\n')
    (project / 'project.valwood').write_text(
        'name=Legends of Valwood | Grandstone Stronghold\nterrain=valwood_atlas\n'
        f'spawn={X + 57.2697:.9f} {Z - 81.01725:.9f} -0.7655\n'
        'camera=0 0.3514 7\nday_fraction=0.60\n')
    metadata = dict(version=1, name='Grandstone | Publisher-authored castle scene',
                    source='Purchased NatureManufacture Demo Castle.unity',
                    origin=[X, Z], floor=FLOOR, chunks=len(catalog),
                    catalog_sha256=hashlib.sha256(args.catalog.read_bytes()).hexdigest(),
                    active_geometry_catalog=str(args.catalog.resolve()),
                    extra_catalog=str(args.extra_catalog) if args.extra_catalog else None,
                    extra_catalog_sha256=hashlib.sha256(args.extra_catalog.read_bytes()).hexdigest() if args.extra_catalog else None,
                    source_triangles=report.get('triangles', [sum(r['triangles'][i] for r in catalog) for i in [0,1]]),
                    source_terrain_layers=8, absolute_source_elevations=True,
                    collision_status='Source chunks retain open visualization bounds; triangle navigation pending')
    (project / 'stronghold.json').write_text(json.dumps(metadata, indent=2) + '\n')
    (project / 'publisher_showcase.json').write_text(json.dumps(metadata, indent=2) + '\n')
    print(json.dumps(metadata, indent=2))

if __name__ == '__main__':
    main()
