#!/usr/bin/env python3
"""Motivated evening lighting for a private copy of the opening apartment.

The exterior sky remains unchanged. A bounds-guarded shader correction reduces
the global outdoor environment inside the home, while the seven existing
practicals provide the warm/cool pools. Positions and room-switch bindings are
preserved. No new lights, texture samples, painted shadows, material emission,
geometry or collisions are introduced.
"""
from pathlib import Path
import hashlib
import json


MARKER = 'VALWOOD_OPENING_HOME_ART_LIGHTING_V1'
BOUNDS = dict(minimum=[-3506., 599., -11006.],
              maximum=[-3494., 604., -10994.])
INDIRECT_SCALE = [.45, .46, .48]
SUNLIGHT_SCALE = .10

# Exact established positions also identify six electrical circuit bindings.
# Keep the non-electrical window source independent of all four room switches.
LIGHTS = [
    dict(name='Living floor lamp', position=[-3504.69, 601.60, -10996.48],
         before=[4.5, 1.35, 1., .78, .55],
         after=[4.5, 3.8, 1., .68, .40], room='living'),
    dict(name='Cool window key', position=[-3495.3, 602.15, -10996.9],
         before=[8., 2.4, .72, .84, 1.],
         after=[6.5, 1.8, .55, .70, 1.], room=None),
    dict(name='Existing living ceiling fill', position=[-3501., 602.95, -10998.7],
         before=[8., .55, 1., .91, .78],
         after=[5., 1.2, 1., .80, .58], room='living'),
    dict(name='Kitchen undercabinet bar', position=[-3500.22, 601.58, -11004.48],
         before=[3.8, .9, 1., .88, .69],
         after=[3.1, 1.05, 1., .84, .65], room='kitchen'),
    dict(name='Kitchen pendant', position=[-3500.22, 602.45, -11001.25],
         before=[4.8, .9, 1., .82, .61],
         after=[4.5, 1.15, 1., .76, .49], room='kitchen'),
    dict(name='Bedroom lamp', position=[-3494.97, 600.99, -11003.97],
         before=[2.6, .65, 1., .79, .54],
         after=[2.6, .85, 1., .70, .42], room='bedroom'),
    dict(name='Bathroom practical', position=[-3495.65, 602.55, -10999.55],
         before=[3.4, .85, 1., .90, .78],
         after=[3., 1., 1., .85, .67], room='bathroom'),
]


def _sha(payload):
    return hashlib.sha256(payload).hexdigest()


def _private_file(project, relative):
    root = Path(project)
    if root.is_symlink() or not root.is_dir():
        raise ValueError('An ordinary private project directory is required')
    path = root / relative
    current = path
    while current != root:
        if current.is_symlink():
            raise ValueError('Lighting edits must not follow shared symlinks: ' + str(current))
        current = current.parent
    if not path.is_file():
        raise ValueError('Missing private project file: ' + str(path))
    return path


def patch_home_art_lighting(project: Path) -> dict:
    """Patch only this project's shader and its seven existing light rows.

    Prepare and validate both updated payloads before writing either file. The
    returned metadata contains exact input/output hashes and authored values.
    Native captures must still verify bill readability and each room switch.
    """
    shader_path = _private_file(project, 'shaders/world_foliage.fs.glsl')
    studio_path = _private_file(project, 'world.studio')
    shader_bytes = shader_path.read_bytes()
    studio_bytes = studio_path.read_bytes()
    shader = shader_bytes.decode('utf-8')
    studio = studio_bytes.decode('utf-8')
    if MARKER in shader:
        raise ValueError('Home art lighting already applied; use a fresh private project')
    main_marker = '// VALWOOD_GRAPH_SURFACE\nvoid main() {'
    if shader.count(main_marker) != 1:
        raise ValueError('Missing unique production foliage main marker')
    guard = '''// VALWOOD_OPENING_HOME_ART_LIGHTING_V1
bool homeArtInteriorReceiver() {
    if(uVegetation!=0||uGrass||uActorReactive>0.)return false;
    vec3 world=vec3(vPos.x+uWorldAnchor.x,vPos.y,vPos.z+uWorldAnchor.y);
    return all(greaterThan(world,vec3(-3506.,599.,-11006.)))&&
           all(lessThan(world,vec3(-3494.,604.,-10994.)));
}

'''
    updated_shader = shader.replace(main_marker, guard + main_marker, 1)
    hook = '    if(!leaf)indirect=opaque_environment(base.rgb,metal,rough,geometricN,V,skyLight,groundLight,ambient,ao);'
    if updated_shader.count(hook) != 1:
        raise ValueError('Missing unique production opaque indirect hook')
    updated_shader = updated_shader.replace(hook, hook + '''
    // The apartment receives a restrained cool evening environment. Existing
    // local practicals and emissive fixtures remain independent and unscaled.
    // No exterior surface, foliage, actor, source bake or material changes.
    if(homeArtInteriorReceiver()) {
        indirect*=vec3(.45,.46,.48);
        sunlight*=.10;
    }''', 1)

    # Match coordinates numerically, retain their original literal text, and
    # reject unexpected prior edits instead of quietly overwriting artist work.
    found = set()
    updated_rows = []
    light_metadata = []
    total_lights = 0
    for row in studio.splitlines(keepends=True):
        fields = row.split()
        if not fields or fields[0] != 'light':
            updated_rows.append(row)
            continue
        total_lights += 1
        if len(fields) != 13:
            raise ValueError('Unsupported Studio light row')
        values = list(map(float, fields[1:]))
        matches = [i for i, entry in enumerate(LIGHTS)
                   if all(abs(values[axis] - entry['position'][axis]) < 1e-6
                          for axis in range(3))]
        if not matches:
            updated_rows.append(row)
            continue
        if len(matches) != 1 or matches[0] in found:
            raise ValueError('Ambiguous or duplicate home light position')
        index = matches[0]
        found.add(index)
        entry = LIGHTS[index]
        if any(abs(a-b) > 1e-6 for a, b in zip(values[3:8], entry['before'])):
            raise ValueError('Unexpected prior home light edit: ' + entry['name'])
        if values[8:] != [0., -45., 180., 0.]:
            raise ValueError('Unexpected prior light direction/flicker edit: ' + entry['name'])
        replacement = ' '.join(fields[:4] + [format(v, '.9g') for v in entry['after']] + fields[9:])
        replacement += '\n' if row.endswith('\n') else ''
        updated_rows.append(replacement)
        light_metadata.append(dict(name=entry['name'], room=entry['room'],
                                   before=row.rstrip('\r\n'), after=replacement.rstrip('\n'),
                                   position_preserved=fields[1:4] == replacement.split()[1:4]))
    if found != set(range(len(LIGHTS))):
        raise ValueError('Expected all seven established home lights exactly once')
    updated_studio = ''.join(updated_rows)
    output_shader = updated_shader.encode('utf-8')
    output_studio = updated_studio.encode('utf-8')
    shader_path.write_bytes(output_shader)
    studio_path.write_bytes(output_studio)
    return dict(schema='valwood.opening-home-art-lighting', version=1,
                shader=dict(path='shaders/world_foliage.fs.glsl',
                            before_sha256=_sha(shader_bytes), after_sha256=_sha(output_shader),
                            bounds=BOUNDS, indirect_scale=INDIRECT_SCALE,
                            sunlight_scale=SUNLIGHT_SCALE,
                            vegetation_actors_grass_excluded=True,
                            extra_texture_samples=0),
                studio=dict(path='world.studio', before_sha256=_sha(studio_bytes),
                            after_sha256=_sha(output_studio),
                            total_light_count_before=total_lights,
                            total_light_count_after=total_lights, lights=light_metadata),
                room_switch_positions_preserved=True,
                emission_materials_unchanged=True,
                geometry_collision_and_gameplay_unchanged=True,
                native_visual_review_required=True,
                scope='Private home shader bounds and seven established Studio light rows only; shared shaders and exterior lighting unchanged.')


if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--project', type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(patch_home_art_lighting(args.project), indent=2))
