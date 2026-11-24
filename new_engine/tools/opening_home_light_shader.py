#!/usr/bin/env python3
"""Room-switch emission policy for the isolated opening apartment fixtures.

This edits only a built project's private shader. Ordinary diffuse materials,
the television, window daylight and console power indicator keep their normal
appearance. Actual practical-light intensities belong to the runtime helper.
"""
from pathlib import Path
import hashlib


# House-local metres; order matches runtime uniform x=living, y=kitchen,
# z=bedroom, w=bathroom. The owned bedroom lamps have no emissive materials.
FIXTURES = [
    dict(name='Living room floor lamp shade and diffuser', room='living', component='x',
         minimum=[-5.02,1.48,3.18], maximum=[-4.36,1.90,3.86]),
    dict(name='Kitchen undercabinet bar', room='kitchen', component='y',
         minimum=[-1.26,1.59,-4.56], maximum=[.82,1.66,-4.46]),
    dict(name='Kitchen pendant', room='kitchen', component='y',
         minimum=[-.54,2.45,-1.57], maximum=[.10,2.74,-.93]),
    dict(name='Bathroom vanity diffuser', room='bathroom', component='w',
         minimum=[5.20,2.30,.84], maximum=[5.34,2.39,1.42]),
    dict(name='Bathroom ceiling diffuser', room='bathroom', component='w',
         minimum=[4.42,3.10,.08], maximum=[4.78,3.17,.44]),
]


def _vec3(values):
    return 'vec3('+','.join(format(v,'.8g')+('.' if float(v).is_integer() else '')
                            for v in values)+')'


def _fixture_glsl():
    body = '''
// VALWOOD_OPENING_FIXTURE_SWITCHES_V1
// Safe fallback for ordinary source-camera viewing and shader preparation.
// Runtime updates this every frame, including reflected and warmed programs.
uniform vec4 uHomeRoomLights=vec4(1.);
float homeFixtureEmissionWeight() {
    if(uVegetation!=0||uGrass||all(lessThanEqual(uEmissiveFactor,vec3(0.))))return 1.;
    vec3 homeLocal=vec3(vPos.x+uWorldAnchor.x+3500.,vPos.y-600.,
                        vPos.z+uWorldAnchor.y+11000.);
    if(any(lessThan(homeLocal,vec3(-5.1,1.4,-4.6)))||
       any(greaterThan(homeLocal,vec3(5.4,3.2,3.9))))return 1.;
'''
    for fixture in FIXTURES:
        body += ('    // '+fixture['name']+'\n'
                 '    if(all(greaterThanEqual(homeLocal,'+_vec3(fixture['minimum'])+'))&&\n'
                 '       all(lessThanEqual(homeLocal,'+_vec3(fixture['maximum'])+')))\n'
                 '        return clamp(uHomeRoomLights.'+fixture['component']+',0.,1.);\n')
    return body+'    return 1.;\n}\n'


def patch_home_lights(project: Path) -> dict:
    """Add a guarded fixture-only factor to the existing emission term."""
    path = Path(project)/'shaders/world_foliage.fs.glsl'
    source = path.read_text()
    if 'VALWOOD_OPENING_FIXTURE_SWITCHES_V1' in source:
        raise ValueError('Room fixture switches already installed; rebuild private shader from source')
    marker = '// VALWOOD_GRAPH_SURFACE\nvoid main() {'
    if source.count(marker) != 1:
        raise ValueError('Missing unique production foliage main marker')
    original = '    color+=max(uEmissiveFactor,vec3(0.))*(uHasEmissive?texture(uEmissive,vUV).rgb:vec3(1.));'
    if source.count(original) != 1:
        raise ValueError('Missing unique production material emission term')
    updated = source.replace(marker, _fixture_glsl()+'\n'+marker, 1)
    updated = updated.replace(original,
        '    color+=max(uEmissiveFactor,vec3(0.))*(uHasEmissive?texture(uEmissive,vUV).rgb:vec3(1.))*homeFixtureEmissionWeight();', 1)
    path.write_text(updated)
    return dict(original_sha256=hashlib.sha256(source.encode()).hexdigest(),
                patched_sha256=hashlib.sha256(updated.encode()).hexdigest(),
                uniform='uHomeRoomLights', default_weights=[1,1,1,1],
                components=dict(x='living', y='kitchen', z='bedroom', w='bathroom'),
                local_origin=[-3500,600,-11000], fixtures=FIXTURES,
                bedroom_source_emission=False,
                independent=['Television animation','Window daylight','Console power indicator'],
                scope='Only material emission inside actual apartment lamp bounds; original shared shaders unchanged')
