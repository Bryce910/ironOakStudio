"""Independent read-only geometry and production trunk-bound checks for V3."""
from pathlib import Path
import ctypes
import importlib.util
import json
import math
import re
import subprocess

import numpy as np


def audit(n, baseline, sha):
    out = n/'review/forest-scene'
    project = n/'world-project'
    manifest_path = n/'review/camp-composition/V3_PATCH.json'
    patch = json.loads(manifest_path.read_text())
    pairs = patch['tree_replacements']
    assert len(pairs) == patch['tree_replacement_count'] == 12
    assert sum(p['new_alias'] == 'pine' for p in pairs) == 6
    assert sum(p['new_alias'] == 'fir' for p in pairs) == 6
    assert sha(project/'world.studio') == patch['world_studio_unchanged_sha256']
    inputs = {str(manifest_path): sha(manifest_path), str(Path(__file__)): sha(Path(__file__))}

    spec = importlib.util.spec_from_file_location('independent_tree_geometry', n/'tools/review_forest_layout.py')
    gmod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(gmod)
    inputs[str(n/'tools/review_forest_layout.py')] = sha(n/'tools/review_forest_layout.py')

    source_path = n/'source-v1/engine/world_scenery.cpp'
    source = source_path.read_text()
    inputs[str(source_path)] = sha(source_path)
    # Execute these exact production functions, rather than restating the
    # float transforms in Python. The small adapter never creates a GL context.
    def block(start):
        begin = source.index(start)
        brace = source.index('{', begin)
        depth = 0
        for i in range(brace, len(source)):
            if source[i] == '{': depth += 1
            elif source[i] == '}': depth -= 1
            if depth == 0:
                return source[begin:i+1]+(';' if start.startswith('struct ') else '')
        raise ValueError(start)

    fragments = [block('struct Placement {'), block('struct Collider {'),
                 block('vw_transform::M placement_matrix('), block('bool extended('),
                 block('void append_collider(')]
    helper = '#include <cmath>\n#include <vector>\n#include <limits>\n#include <cstdint>\n#include <iostream>\n#include <iomanip>\n#include "engine/world_transform_math.h"\nconstexpr double pi=3.14159265358979323846;\n'
    helper += '\n'.join(fragments)
    helper += '''
int main(){Placement p{};float h=0,r=0;std::cout<<std::setprecision(15);
while(std::cin>>p.x>>p.z>>p.y>>p.yaw>>p.scale>>p.pitch>>p.roll>>p.sx>>p.sy>>p.sz>>h>>r){
std::vector<Collider> result;append_collider(result,p,0,0,0,0,0,h,r);
if(result.size()!=1)return 2;const auto&c=result[0];
std::cout<<c.x<<' '<<c.z<<' '<<c.ymin<<' '<<c.ymax<<' '<<c.radius<<' '<<c.min_x<<' '<<c.max_x<<' '<<c.min_z<<' '<<c.max_z<<' '<<c.circle<<'\n';}}
'''.replace("<<'\n';", "<<'\\n';")
    helper_path = out/'controlled-tree-production-collider.cpp'
    helper_path.write_text(helper)
    binary = helper_path.with_suffix('')
    command = ['g++', '-std=c++17', '-O2', '-I'+str(n/'source-v1'), str(helper_path), '-o', str(binary)]
    built = subprocess.run(command, capture_output=True, text=True)
    (out/'controlled-tree-collider-build.log').write_text(built.stdout+built.stderr)
    assert built.returncode == 0, built.stderr

    routes_path = project/'opening/first-journey.routes.json'
    routes = json.loads(routes_path.read_text())['paths']
    segments = []
    for points in routes.values():
        points = np.asarray(points)[:, [0, 2]]
        segments.extend(zip(points[:-1], points[1:]))
    traffic_path = project/'opening/road-traffic.settings'
    traffic = np.array([[float(q[1]), float(q[3])] for line in traffic_path.read_text().splitlines()
                        if (q := line.split()) and q[0] == 'point'])
    segments.extend(zip(traffic[:-1], traffic[1:]))
    segments = np.array(segments)

    bank = n.parents[1]/'build/camp-road-polish-2026-11-19'
    libpath = bank/'review/route-budget/sample-height.so'
    for rel in ['engine/world_surface_authoring.cpp', 'engine/world_surface_field.cpp',
                'engine/world_surface_authoring.h', 'engine/world_surface_field.h',
                'engine/world_river_network.h', 'engine/world_height_region.h']:
        assert (bank/'source-v1'/rel).read_bytes() == (n/'source-v1'/rel).read_bytes()
    lib = ctypes.CDLL(str(libpath))
    lib.load.argtypes = [ctypes.c_char_p, ctypes.c_char_p]
    lib.load.restype = ctypes.c_int
    lib.sample_many.argtypes = [ctypes.c_uint, ctypes.POINTER(ctypes.c_double), ctypes.POINTER(ctypes.c_float), ctypes.c_int]
    lib.sample_many.restype = ctypes.c_int
    assert lib.load(str(project/'ground/showcase.height').encode(), str(project/'world.studio').encode()) == 1
    def ground(points):
        pp = np.ascontiguousarray(points, dtype=np.float64).reshape(-1, 2)
        result = np.empty(len(pp), np.float32)
        assert lib.sample_many(len(pp), pp.ctypes.data_as(ctypes.POINTER(ctypes.c_double)), result.ctypes.data_as(ctypes.POINTER(ctypes.c_float)), 1) == 1
        return result.astype(float)

    for p in [routes_path, traffic_path, libpath, project/'ground/showcase.height', project/'world.studio']:
        inputs[str(p)] = sha(p)
    geometries = {}
    radii = {}
    for alias in ['pine', 'fir']:
        path = project/'models'/('cinematic_'+alias+'_near.glb')
        inputs[str(path)] = sha(path)
        assert sha(path) == sha(baseline/'world-project/models'/path.name)
        geometries[alias], triangles, shift = gmod.geometry(path)
        match = re.search(r'\{"'+alias+r'",[^\n]*Collision::Trunk,\s*([.\d]+)f', source)
        assert match
        radii[alias] = float(match[1])

    updates = {}
    data = []
    preliminary = []
    for p in pairs:
        assert p['old_alias'] in ['nm_city_region_oak', 'nm_city_region_beech']
        assert p['new_alias'] in geometries
        relative = p['file']
        old = p['before_row']
        new = p['after_row']
        old_lines = (baseline/'world-project'/relative).read_text().splitlines()
        new_lines = (project/relative).read_text().splitlines()
        assert old_lines.count(old) == 1 and new_lines.count(new) == 1
        assert new_lines[p['line']-1] == new
        key = (Path(relative).name, old)
        assert key not in updates
        updates[key] = new
        oldq, oldp = gmod.decode(old)
        q, pose = gmod.decode(new)
        assert [float(t) for t in oldq[1:3]] == [float(t) for t in q[1:3]], 'XZ changed'
        assert np.float32(math.radians(oldp['yaw'])) == np.float32(math.radians(pose['yaw'])), 'native yaw changed'
        assert np.array_equal(pose['axes'], [1., 1., 1.]) and pose['pitch'] == pose['roll'] == 0
        assert pose['burial'] == pose['offset'] == pose['elevation'] == 0
        assert .15 <= pose['scale'] <= 2.
        g = geometries[q[0]]
        v = g*pose['scale'] @ gmod.rot(pose['yaw'], 0., 0.).T
        height = float(np.ptp(v[:, 1]))
        assert 14.-2e-5 <= height <= 22.+2e-5
        point = np.array([pose['x'], pose['z']])
        radius = float(np.linalg.norm(v[:, [0, 2]], axis=1).max())
        route = gmod.dist(point, segments)-radius
        camp = float(np.linalg.norm(point-[-11988., 5172.]))
        assert route >= 2. and camp >= 18.
        bottom = v[v[:, 1] <= v[:, 1].min()+max(.02, height*.025)]
        if len(bottom) > 64: bottom = bottom[np.linspace(0, len(bottom)-1, 64, dtype=int)]
        hh = ground(point+bottom[:, [0, 2]])
        expected_y = float(np.quantile(hh-bottom[:, 1], .35))-.045
        assert abs(expected_y-pose['y']) < 2e-5
        values = [pose['x'], pose['z'], pose['y'], math.radians(pose['yaw']), pose['scale'], 0, 0, 1, 1, 1,
                  float(g[:, 1].max()), radii[q[0]]]
        data.append(' '.join(format(value, '.17g') for value in values))
        preliminary.append((p, pose, v, point, radius, route, camp, expected_y))

    result = subprocess.run([str(binary)], input='\n'.join(data)+'\n', capture_output=True, text=True)
    assert result.returncode == 0, result.stderr
    (out/'controlled-tree-production-collider-raw.txt').write_text(result.stdout)
    colliders = [list(map(float, row.split())) for row in result.stdout.splitlines()]
    assert len(colliders) == 12
    records = []
    native_queries = ['12']
    for (p, pose, v, point, radius, route, camp, expected_y), c in zip(preliminary, colliders):
        assert len(c) == 10 and np.isfinite(c).all() and c[-1] == 1 and c[4] > 0 and c[3] > c[2]
        assert abs(c[0]-pose['x']) < 1e-6 and abs(c[1]-pose['z']) < 1e-6
        assert abs(c[2]-pose['y']) < 2e-5 and abs(c[3]-(pose['y']+v[:, 1].max())) < 2e-5
        expected_radius = float(np.float32(radii[p['new_alias']])*np.float32(pose['scale']))
        assert abs(c[4]-expected_radius) < 1e-7
        assert np.max(np.abs(np.array(c[5:9])-[point[0]-c[4], point[0]+c[4], point[1]-c[4], point[1]+c[4]])) < 1e-6
        collider_route = gmod.dist(point, segments)-c[4]
        assert collider_route >= 2.
        native_queries.append(' '.join(format(value, '.15g') for value in [*point, c[2], c[4], c[3]-c[2]]))
        records.append(dict(file=p['file'], line=p['line'], old_alias=p['old_alias'], new_alias=p['new_alias'],
                            old_row=p['before_row'], new_row=p['after_row'],
                            full_mesh_route_clearance_m=route, collider_route_clearance_m=collider_route,
                            camp_root_distance_m=camp, actual_tree_height_m=float(np.ptp(v[:, 1])),
                            support_error_m=abs(expected_y-pose['y']),
                            production_collider=dict(x=c[0], z=c[1], ymin=c[2], ymax=c[3], radius=c[4], circle=True)))
    query_path = out/'controlled-tree-queries.txt'
    query_path.write_text('\n'.join(native_queries)+'\n')
    report = dict(passed=True, controlled_tree_replacements=12, trees=records,
                  source_derived_functions_executed=['placement_matrix', 'extended', 'append_collider'],
                  production_source_sha256=sha(source_path), generated_helper_sha256=sha(helper_path),
                  build_command=command, helper_binary_sha256=sha(binary),
                  queries_path=str(query_path), inputs_sha256=inputs,
                  limitations=['This adapter executes the exact production trunk-bound constructor without loading a GPU scenery scene.',
                               'Existing trunk cylinders enclose the central trunk at body height; decorative low root flares are not triangle-exact collision.',
                               'Full rendered-mesh route clearance is independently checked; gameplay movement still requires native testing.'])
    (out/'CONTROLLED_TREE_REVIEW.json').write_text(json.dumps(report, indent=2)+'\n')
    return updates, report
