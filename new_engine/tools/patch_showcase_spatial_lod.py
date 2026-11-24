#!/usr/bin/env python3
"""Opt the isolated publisher shaders into source-batch surface-distance LOD.

Native scenery supplies the flag only for nm_scene_ spatial batches. Ordinary
aliases and all other project shaders keep their original root-distance policy.
"""
import argparse
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def patch(project):
    folder = Path(project) / 'shaders'
    color = folder / 'world_foliage.fs.glsl'
    shader = color.read_text()
    if 'uniform bool uSourceChunkDistance' not in shader:
        needle = 'uniform vec3 uPropLodRange;'
        if shader.count(needle) != 1:
            raise ValueError('Unrecognized private color LOD declaration')
        shader = shader.replace(needle, 'uniform bool uSourceChunkDistance=false; // source-batch surface distance\n' + needle)
    old = 'float distance=length(vRelative),groundDistance=length(vRootRelative.xz);'
    new = 'float distance=length(vRelative),groundDistance=uSourceChunkDistance?length(vRelative):length(vRootRelative.xz);'
    shader = shader.replace(old, new).replace('uSourceChunkDistance?length(vRelative.xz)', 'uSourceChunkDistance?length(vRelative)')
    if new not in shader:
        raise ValueError('Unrecognized private color distance expression')
    if shader != color.read_text():
        color.write_text(shader)

    shadow = folder / 'world_foliage_shadow.fs.glsl'
    shader = shadow.read_text()
    if 'uniform bool uSourceChunkDistance' not in shader:
        shader = shader.replace('in vec2 vUV;', 'in vec2 vUV;\nin vec3 vRelative;')
        shader = shader.replace('uniform vec3 uPropLodRange;', 'uniform vec3 uPropLodRange;\nuniform bool uSourceChunkDistance=false;')
        shader = shader.replace('    float intervalLo=0.,intervalHi=1.;', '    float groundDistance=uSourceChunkDistance?length(vRelative):length(vRootRelative.xz);\n    float intervalLo=0.,intervalHi=1.;')
        shader = shader.replace('uPropLodRange.y,length(vRootRelative.xz)', 'uPropLodRange.y,groundDistance')
        shader = shader.replace('uSceneryDistanceFade.y,length(vRootRelative.xz)', 'uSceneryDistanceFade.y,groundDistance')
    shader = shader.replace('uSourceChunkDistance?length(vRelative.xz)', 'uSourceChunkDistance?length(vRelative)')
    if 'float groundDistance=uSourceChunkDistance?length(vRelative):length(vRootRelative.xz);' not in shader:
        raise ValueError('Unrecognized private shadow distance expression')
    if shader != shadow.read_text():
        shadow.write_text(shader)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--project', type=Path, default=ROOT / 'projects/valwood_showcase')
    patch(parser.parse_args().project)
