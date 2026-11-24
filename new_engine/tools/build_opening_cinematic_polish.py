#!/usr/bin/env python3
"""Author the cinematic polish as a new, isolated Finward opening revision."""
from pathlib import Path
import argparse
import json
import math
import shutil

import numpy as np

from check_opening_prologue import isolate_project, mutable_snapshot, sha
from opening_home_cinematic import audit_shots
from opening_cinematic_bill import rotate_bill

ROOT = Path(__file__).resolve().parents[1]
HOME = np.array([-3500., 600., -11000.])


def write_json(path, value):
    path.write_text(json.dumps(value, indent=2, allow_nan=False) + '\n')


def polish_shader(project):
    path = project/'shaders/world_foliage.fs.glsl'
    text = path.read_text()
    before = sha(path)
    export = json.loads((ROOT/'assets/purchased/finward_house_props/scene_export/scene.json').read_text())
    boxes = []
    for row in export['placements']:
        if not Path(row['source_prefab']).stem.startswith('Window_'):
            continue
        a, b = row['bounds_min'], row['bounds_max']
        lo = HOME + [a['x']-.025, a['y']-.025, -b['z']-.025]
        hi = HOME + [b['x']+.025, b['y']+.025, -a['z']+.025]
        def vec(v):
            return 'vec3(' + ','.join(format(float(x), '.9f') for x in v) + ')'
        boxes.append('    if(all(greaterThanEqual(p,'+vec(lo)+'))&&all(lessThanEqual(p,'+vec(hi)+')))return true;')
    helper = ('// VALWOOD_CINEMATIC_WINDOW_BALANCE_V1\n'
              'bool referenceDaylightPane(vec3 p){\n' + '\n'.join(boxes) +
              '\n    return false;\n}\n')
    anchor = 'float referenceRoomWeight(vec3 p){'
    assert text.count(anchor) == 1 and len(boxes) == 8
    text = text.replace(anchor, helper+anchor)
    anchor = '    vec3 color=indirect+brdf(base.rgb,metal,rough,N,V,uSun)*sunlight*3.14159265*shadow;'
    assert text.count(anchor) == 1
    text = text.replace(anchor, '''    // A restrained native interior grade lifts furniture mids while keeping
    // the source's darkest shadows, switched circuits and highlights distinct.
    if(homeArtInteriorReceiver()){
        float roomLuma=dot(max(indirect,vec3(0.)),vec3(.2126,.7152,.0722));
        indirect*=1.+.18*(1.-smoothstep(.08,.9,roomLuma));
    }
'''+anchor)
    anchor = '    color+=max(uEmissiveFactor,vec3(0.))*(uHasEmissive?texture(uEmissive,vUV).rgb:vec3(1.))*homeFixtureEmissionWeight();'
    assert text.count(anchor) == 1
    text = text.replace(anchor, '''    vec3 fixtureEmission=max(uEmissiveFactor,vec3(0.))*(uHasEmissive?texture(uEmissive,vUV).rgb:vec3(1.))*homeFixtureEmissionWeight();
    // The publisher's luminous display windows otherwise clip to a white
    // rectangle. Only their original eight spatial domains receive this trim;
    // the TV, clocks, lamps and all outdoor emission keep their existing scale.
    vec3 fixtureWorld=vec3(vPos.x+uWorldAnchor.x,vPos.y,vPos.z+uWorldAnchor.y);
    if(homeArtInteriorReceiver()&&referenceDaylightPane(fixtureWorld))fixtureEmission*=.40;
    color+=fixtureEmission;''')
    path.write_text(text)
    return dict(before_sha256=before, sha256=sha(path), window_domains=len(boxes),
                window_emission_scale=.40, maximum_interior_midtone_lift=.18,
                scope='Private interior shader only; source geometry/textures/bake bytes and outdoor shading unchanged.')


def reference_metadata(project, shader):
    path = project/'opening/reference.json'
    reference = json.loads(path.read_text())
    reference['shader']['previous_reference_sha256'] = reference['shader']['sha256']
    reference['shader']['sha256'] = shader['sha256']
    reference['shader']['cinematic_polish'] = shader
    reference['scope'] = 'Supplied demo arrangement with cinematic polish; native acceptance recorded separately.'
    write_json(path, reference)


def build(source, destination):
    if destination.exists():
        raise ValueError('Choose a new revision directory')
    before = mutable_snapshot(source)
    project = destination/'project'
    isolate_project(source, project)
    models = project/'models'
    bank = models.resolve(strict=True)
    models.unlink()
    models.mkdir()
    for item in bank.iterdir():
        target = models/item.name
        if item.name == 'opening':
            shutil.copytree(item.resolve(strict=True), target)
        else:
            target.symlink_to(item.resolve(strict=True), target_is_directory=item.is_dir())
    bill = rotate_bill(project)
    shader = polish_shader(project)
    reference_metadata(project, shader)
    def world(p):
        return (HOME+np.asarray(p)).tolist()
    def shot(name, duration, eye_from, eye_to, target_from, target_to, fov_from, fov_to, subtitle, purpose):
        return dict(name=name, duration=duration, eye_from=world(eye_from), eye_to=world(eye_to),
                    target_from=world(target_from), target_to=world(target_to),
                    fov_from=fov_from, fov_to=fov_to, subtitle=subtitle, purpose=purpose)
    shots = [
        shot('late-shift-room', 3.4, [2,1.55,-5], [1.9,1.50,-4.8],
             [.626,1.,-2.04], [.7,1.,-2.04], .90,.86, 'Another late shift.',
             'A restrained forward move prioritizes the worn evening room and table, with less ceiling and window glare.'),
        shot('still-behind', 5.1, [1.915,1.35,-2.95], [1.915,1.29,-2.90],
             [1.915,.549,-2.102], [1.915,.549,-2.102], .56,.52,
             "Rent. Power. Food. Somehow I'm still behind.",
             'An upright, tighter rent reminder carries the burden of everyday costs; existing second-shot focus isolates the paper.'),
        shot('one-quiet-night', 5., [2.2,1.35,-3.8], [2.1,1.3,-3.65],
             [.626,.82,-2.04], [.626,.82,-2.04], .72,.66, 'Just one quiet night.',
             'A measured screen-and-console composition answers the bills; starting the game remains the player choice.'),
        shot('take-control', 2.5, [2.2,1.72,-4.75], [2,1.75,-5],
             [2,1.69,-3.65], [2,1.75+math.tan(-.03),-4], 1.,1.0471975512, '',
             'A short, forward-facing settle reaches the exact standing camera without the previous sweeping turn.')
    ]
    assert sum(s['duration'] for s in shots) == 16.
    rows = ['VALWOOD_HOME_CINEMATIC 1']
    for s in shots:
        values = [s['duration'], *s['eye_from'], *s['eye_to'], *s['target_from'], *s['target_to'], s['fov_from'], s['fov_to']]
        rows.append('shot '+' '.join(format(x,'.17g') for x in values)+' '+json.dumps(s['subtitle']))
    rows.append('end')
    (project/'opening/cinematic.settings').write_text('\n'.join(rows)+'\n')
    write_json(project/'opening/cinematic.json',dict(schema='valwood.home-cinematic',version=1,
        seconds=16.,shots=shots,endpoint_exact_spawn=True,scope='Cinematic polish; native acceptance recorded separately.'))
    layout = json.loads((project/'opening/layout.json').read_text())
    audit = audit_shots(project, shots, HOME, layout)
    write_json(destination/'camera-review.json', audit)
    if not audit['passed']:
        raise ValueError('Unsafe camera: '+json.dumps(audit['failures']))
    def authored(snapshot):
        return {k:v for k,v in snapshot.items() if not k.startswith('saves/') and k != 'graphics.settings'}
    unchanged = authored(before) == authored(mutable_snapshot(source))
    assert unchanged, 'Source revision was modified'
    generated = {p.relative_to(project).as_posix():sha(p) for p in project.rglob('*')
                 if p.is_file() and not p.is_symlink()}
    source_origin = json.loads((source.parent/'opening-origin.json').read_text())
    write_json(destination/'opening-origin.json',dict(passed=True,project=str(project),source_project=str(source),
        generated=generated,source_assets_unchanged=True,reference_scene_copied=True,
        cook_review=source_origin['cook_review'],scope='Camera, authored bill UV orientation and restrained native interior lighting polish; geometry/story/physics/runtime preserved.'))
    result = dict(passed=True,project=str(project),source_project_unchanged=unchanged,
                  cinematic_seconds=16.,shots=4,bill=bill,shader=shader,camera_passed=True,
                  camera_samples=audit['sample_count'],runtime_rebuild=False)
    write_json(destination/'cinematic-polish.json',result)
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source',type=Path,default=ROOT/'build/opening-finward-reference/v11/project')
    parser.add_argument('--output-root',type=Path,default=ROOT/'build/opening-finward-reference/v12')
    args = parser.parse_args()
    print(json.dumps(build(args.source.resolve(),args.output_root.resolve()),indent=2))
