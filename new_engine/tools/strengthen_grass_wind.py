#!/usr/bin/env python3
"""Stage and publish stronger grass wind without changing tree/water wind."""
from pathlib import Path
import argparse
import json
import os
import shutil

from check_opening_prologue import isolate_project, mutable_snapshot, sha

ROOT = Path(__file__).resolve().parents[1]
DOC = ROOT/'docs/grass-wind/2026-11-04'
PAIR = ['world_foliage.vs.glsl','world_foliage.fs.glsl']
COMPANIONS = ['world_skeletal.vs.glsl','world_dynamic.vs.glsl']


def write_json(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2)+'\n')


def patch(source, rooted):
    vs, fs = source['world_foliage.vs.glsl'], source['world_foliage.fs.glsl']
    old = 'float amplitude=(uVegetation==2||uVegetation==5) ? pow(clamp(posePosition.y/.4,0.,1.),2.)*.045 : min(max(p.y,0.)*.011,.23);'
    assert vs.count(old) == 1
    vs = vs.replace(old, old.replace('*.045', '*.075'))
    old = 'float amplitude=small ? pow(clamp(height/.4,0.,1.),2.)*.045 : min(height*.011,.23);'
    assert fs.count(old) == 1
    fs = fs.replace(old, old.replace('*.045', '*.075'))
    if rooted:
        anchor = 'out vec3 vPos,vNormal,vRelative,vTangent,vBitangent;'
        assert vs.count(anchor) == 1
        vs = vs.replace(anchor, anchor+'\nout float vRootedGrassWindSpeed;')
        anchor = 'void main() {'
        assert vs.count(anchor) == 1
        vs = vs.replace(anchor, anchor+'\n    vRootedGrassWindSpeed=0.;')
        anchor = 'p+=warp.displacement;grassDerivative=warp.derivative;'
        assert vs.count(anchor) == 1
        vs = vs.replace(anchor, anchor+'vRootedGrassWindSpeed=warp.speed_bound;')
        anchor = 'in vec3 vPos,vNormal,vRelative,vTangent,vBitangent;'
        assert fs.count(anchor) == 1
        fs = fs.replace(anchor, anchor+'\nin float vRootedGrassWindSpeed;')
        anchor = '    if(!uWindHistoryEnabled)return minimum_stability;'
        assert fs.count(anchor) == 1
        fs = fs.replace(anchor, anchor+'''
    // Bound from the exact rooted-grass VS deformation, including instance
    // scale and local height. Keep moving tips crisp without destabilizing
    // other foliage or changing any static scene history.
    if(uVegetation==3&&vRootedGrassWindSpeed>0.){
        float pixels=vRootedGrassWindSpeed*clamp(uWindFrameDelta,0.,.1)*uPixelScale/max(distance,.1);
        return mix(.98,minimum_stability,smoothstep(.02,.35,pixels));
    }''')
    result = {'world_foliage.vs.glsl':vs, 'world_foliage.fs.glsl':fs}
    if rooted:
        result['world_city_grass_wind.glsl'] = (DOC/'candidate/world_city_grass_wind.glsl').read_text()
        # The shared foliage fragment also shades actors and rigid props.
        # Their companion vertex programs must explicitly supply a neutral
        # value for the new interpolator, preserving those program interfaces.
        for name in COMPANIONS:
            text=source[name]
            anchor='out vec3 vPos,vNormal,vRelative,vTangent,vBitangent;'
            assert text.count(anchor)==1 and text.count('void main() {')==1
            text=text.replace(anchor,anchor+'\nout float vRootedGrassWindSpeed;')
            result[name]=text.replace('void main() {','void main() {\n    vRootedGrassWindSpeed=0.;')
    return result


def stage():
    source = ROOT/'build/opening-finward-reference/v12/project'
    dest = ROOT/'build/opening-finward-reference/v13'
    if dest.exists():
        raise ValueError('v13 already exists')
    old = mutable_snapshot(source)
    project = dest/'project'
    isolate_project(source,project)
    changes = patch({name:(project/'shaders'/name).read_text() for name in PAIR+COMPANIONS},True)
    for name, text in changes.items():
        (project/'shaders'/name).write_text(text)
    reference_path=project/'opening/reference.json'
    reference=json.loads(reference_path.read_text())
    reference['shader']['before_grass_temporal_sha256']=reference['shader']['sha256']
    reference['shader']['sha256']=sha(project/'shaders/world_foliage.fs.glsl')
    reference['shader']['grass_temporal_update']='Rooted outdoor grass speed varying only; interior lighting policy retained.'
    write_json(reference_path,reference)
    write_json(project/'opening/grass-wind.json',dict(version=1,rooted_tip_swing_gain_approx=2.3,
        small_grass_amplitude_gain=5/3,rooted_maximum_bend_m=.18,
        rooted_maximum_bend_height_fraction=.22,roots_fixed=True,
        color_shadow_same_vertex_shader=True,temporal_speed_matches_rooted_deformation=True))
    origin = json.loads((source.parent/'opening-origin.json').read_text())
    generated = {p.relative_to(project).as_posix():sha(p) for p in project.rglob('*')
                 if p.is_file() and not p.is_symlink()}
    write_json(dest/'opening-origin.json',dict(passed=True,project=str(project),source_project=str(source),
        generated=generated,source_assets_unchanged=True,reference_scene_copied=True,
        cook_review=origin['cook_review'],scope='Grass wind and matching temporal stability only; indoor cinematic, assets, physics, story and runtime unchanged.'))
    authored=lambda snap:{k:v for k,v in snap.items() if not k.startswith('saves/') and k!='graphics.settings'}
    assert authored(old)==authored(mutable_snapshot(source))
    write_json(DOC/'STAGED.json',dict(passed=True,project=str(project),
        changed_shaders={name:sha(project/'shaders'/name) for name in changes},
        source_unchanged=True,runtime_rebuilt=False))
    return project


def publish():
    # Per-file surgical edits preserve city GI, apartment materials, winter
    # suppression and artist-authored content in each current project variant.
    families = [ROOT/'shaders',ROOT/'projects/valwood_city_showcase/shaders',
                ROOT/'projects/valwood_visual/shaders',ROOT/'projects/valwood_showcase/shaders',
                ROOT/'build/playtest-slices/city/project/shaders',
                ROOT/'build/playtest-slices/jungle/project/shaders',
                ROOT/'build/playtest-slices/grandstone/project/shaders']
    records = []
    plans = []
    for folder in families:
        rooted = (folder/'world_city_grass_wind.glsl').exists()
        changes = patch({n:(folder/n).read_text() for n in PAIR+(COMPANIONS if rooted else [])},rooted)
        for name,text in changes.items():
            path=folder/name
            if path.is_symlink():
                raise ValueError('Refuse implicit linked shader write: '+str(path))
            backup=DOC/'before'/path.relative_to(ROOT)
            plans.append((path,backup,text))
    # Validate every variant before the first write, then preserve exact inputs.
    for path,backup,text in plans:
        if backup.exists():
            raise ValueError('Publication already attempted: '+str(backup))
    for path,backup,text in plans:
        backup.parent.mkdir(parents=True,exist_ok=True)
        shutil.copy2(path,backup)
        tmp=path.with_suffix(path.suffix+'.wind-new');tmp.write_text(text);os.replace(tmp,path)
        records.append(dict(path=str(path),before_sha256=sha(backup),sha256=sha(path),backup=str(backup)))
    write_json(DOC/'PUBLICATION.json',dict(passed=True,files=records,
        original_archived_releases_unchanged=True,water_and_tree_wind_unchanged=True))
    return records


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('action',choices=['stage','publish'])
    args=p.parse_args()
    result=stage() if args.action=='stage' else publish()
    print(result)
