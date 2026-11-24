#!/usr/bin/env python3
"""Read-only, ground-level city/countryside playtest preflight; writes QA evidence only."""
from pathlib import Path
import argparse
import json
import os
import subprocess

import numpy as np

from check_city_countryside import digest, read_entry, field_samples, clearance
from check_village_source_collision import native_checker, exact_ground

ROOT = Path(__file__).resolve().parents[1]


def field(path):
    header, values = field_samples(path)
    return header, values.reshape(header[2], header[1])


def sample(data, x, z):
    (_, cols, rows, xmin, zmin, dx, dz), values = data
    gx, gz = (x-xmin)/dx, (z-zmin)/dz
    if not 0 <= gx <= cols-1 or not 0 <= gz <= rows-1:
        return None
    ix, iz = min(cols-2, int(gx)), min(rows-2, int(gz)); tx, tz = gx-ix, gz-iz
    return float((values[iz, ix]*(1-tx)+values[iz, ix+1]*tx)*(1-tz)
                 +(values[iz+1, ix]*(1-tx)+values[iz+1, ix+1]*tx)*tz)


def walk(path, points):
    path.write_text('VALWOOD_CITY_WALK 1\n'+''.join(f'p{i} {x:.14g} {z:.14g}\n' for i, (x,z) in enumerate(points)))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--project', type=Path, default=ROOT/'projects/valwood_city_showcase')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--checker', type=Path, default=Path('/tmp/valwood-city-countryside-collision-check'))
    parser.add_argument('--build-checker', action='store_true', help='Compile existing CPU helper into isolated /tmp files')
    args = parser.parse_args(); project = args.project.resolve(); out = args.output.resolve()
    if out == project or project in out.parents:
        raise ValueError('QA output must be outside the source project')
    out.mkdir(parents=True, exist_ok=True)
    protected = [project/name for name in ('project.valwood','world.studio','world.entities','city.scene',
                 'showcase.views','city.physics','city_showcase.json','ground/showcase.height',
                 'ground/city.coverage','ground/countryside.height','ground/countryside.coverage')]
    protected += list((project/'scenery').glob('*'))
    protected = [path for path in protected if path.is_file()]
    before = {path.relative_to(project).as_posix():digest(path) for path in protected}
    meta = json.loads((project/'city_showcase.json').read_text())
    catalog_path = Path(meta['active_geometry_catalog']); catalog = json.loads(catalog_path.read_text())
    if digest(catalog_path) != meta['catalog_sha256']:
        raise ValueError('Active catalog bytes do not match installed city metadata')
    if json.loads((project/meta['installed_catalog_snapshot']).read_text()) != catalog:
        raise ValueError('Installed catalog snapshot differs from the active catalog')
    origin = np.asarray(meta['origin'],float)[[0,2]]
    review = catalog_path.parent/'landscape-review.npz'
    with np.load(review) as arrays:
        road = np.asarray(arrays['road_native'],float)[::12]
        endpoint = np.asarray(arrays['road_native'],float)[-1]
    if not np.array_equal(road[-1],endpoint): road = np.vstack((road,endpoint))
    entry = read_entry(ROOT/'docs/village/2026-11-02/countryside-route/reviewed-opening.walk')
    trail = np.vstack((entry,road+origin))
    lanes = np.array([[12,-5],[12,5],[4,5],[4,15],[4,5],[12,5],[12,-5]],float)+origin
    full = np.vstack((lanes,trail[1:],trail[-2::-1]))
    primary_path = project/'ground/showcase.height'; country_path = project/'ground/countryside.height'
    primary, country = field(primary_path), field(country_path)
    def height(x,z):
        value = sample(primary,x,z)
        if value is None: value = sample(country,x,z)
        if value is None: raise ValueError('Route landmark outside active walking fields')
        return value
    checker = native_checker() if args.build_checker else args.checker.resolve()
    if not checker.is_file() or not os.access(checker, os.X_OK):
        raise ValueError('Missing CPU helper; use --build-checker with existing native CPU build prerequisites')
    cases = [('city-market-country-return',full,True)]
    start = full[0]
    for name, offset in [('east',[1,0]),('west',[-1,0]),('north',[0,-1]),('south',[0,1])]:
        cases.append(('spawn-'+name,np.array([start,start+offset,start]),True))
    cases.append(('blocked-house-negative',np.array([[15,-8],[15,3]],float)+origin,False))
    reports = []; good = True
    for name, points, expected_clear in cases:
        path = out/(name+'.walk'); walk(path,points)
        command = [str(checker),str(project/'city.physics'),str(primary_path),str(path),str(country_path)]
        result = subprocess.run(command,capture_output=True,text=True,timeout=120)
        (out/(name+'.log')).write_text(result.stdout+result.stderr)
        data = json.loads(result.stdout)
        passed = (result.returncode == 0 if expected_clear else result.returncode == 1 and data.get('blocked',0)>0)
        passed &= data.get('slope_failures') == 0 and data.get('outside_source_ground') == 0 and data.get('query_heap_fallback') == 0
        scenery = clearance(catalog,points-origin) if expected_clear else None
        if scenery: passed &= scenery['passed']
        reports.append(dict(name=name,expected_clear=expected_clear,passed=bool(passed),result=data,
                            route=str(path),route_sha256=digest(path),scenery_clearance=scenery,command=command))
        good &= passed
    source_ground = ROOT.parent/'.local-tools/village-source/source-ground-native.npz'
    with np.load(source_ground) as ground:
        visual_height = exact_ground(ground['vertices'],ground['triangles'],*(start-origin))+meta['origin'][1]
    spawn = dict(scenic='city-walk',world=[float(start[0]),height(*start)+1.75,float(start[1])],
                 yaw_radians=-2.52781591453,pitch_radians=-.18,fov_y_radians=1.0471975511966,
                 source_ground_y=visual_height,walking_support_y=height(*start),
                 below_source_paving_m=visual_height-height(*start),grounded_by_viewer=True)
    good &= abs(spawn['below_source_paving_m']-.05)<.025
    distance = np.r_[0,np.linalg.norm(np.diff(trail,axis=0),axis=1).cumsum()]
    landmark_indices = [0,1,2]+[int(np.argmin(abs(distance-target))) for target in [100,150,200,250,300,350,distance[-1]]]
    labels = ['town-start','west-lane','town-edge','meadow-approach','meadow-turn','woodland-bend',
              'woodland-path','returning-toward-city','trail-east-bend','turn-around']
    landmarks = [dict(name=name,trail_distance_m=float(distance[i]),world_xz=trail[i].tolist(),
                      ground_y=height(*trail[i]),eye_y=height(*trail[i])+1.75)
                 for name,i in zip(labels,landmark_indices)]
    after = {path.relative_to(project).as_posix():digest(path) for path in protected}
    unchanged = before == after; good &= unchanged
    report = dict(kind='City and countryside grounded light-playtest preflight',passed=bool(good),
        audit_helper_sha256=digest(Path(__file__)),
        project=str(project),catalog=str(catalog_path),catalog_sha256=digest(catalog_path),
        recommended_spawn=spawn,routes=reports,landmarks=landmarks,
        primary_height_priority=True,protected_source_files=before,source_unchanged=unchanged,
        checker_sha256=digest(checker),checker_source_sha256=digest(ROOT/'tools/check_city_collision.cpp'),
        native_gpu_used=False,
        limits=['Actual native Jolt capsule/slope queries; no gameplay input, rendering, performance or whole-city navigation acceptance.',
                'Tree/rock collisions are checked separately with conservative native bounds; decorative ground cover remains noncolliding.',
                'Source building UCX is separately authored fixed geometry; structural editor changes require collision regeneration.',
                'Castle on the distant hill is a visual backdrop; interiors and hilltop navigation are outside the route.',
                'Existing main quest, rewards and combat coordinates are not relocated into this city.'])
    (out/'readiness.json').write_text(json.dumps(report,indent=2,allow_nan=False)+'\n')
    print(json.dumps(dict(passed=bool(good),report=str(out/'readiness.json'),spawn=spawn,
                         route=reports[0]['result'],source_unchanged=unchanged),indent=2))
    return 0 if good else 1


if __name__ == '__main__':
    raise SystemExit(main())
