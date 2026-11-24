#!/usr/bin/env python3
"""Build the small, editable authored scenery composition from the frozen atlas.

This is an explicit set of trail-side compositions, not a world scatterer. The
result is ordinary six-column text; edit it directly after the initial build.
Existing data is preserved unless --force is explicitly supplied. A temporary
CPU-only atlas library supplies the real trail and grounding/suitability math.
"""
from pathlib import Path
import argparse
import ctypes as C
import math
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


class Sample(C.Structure):
    _fields_ = [('height', C.c_float), ('region', C.c_int), ('forest', C.c_float),
                ('moisture', C.c_float), ('shore', C.c_float), ('land', C.c_float)]


class TrailPoint(C.Structure):
    _fields_ = [('x', C.c_double), ('z', C.c_double), ('half_width', C.c_float)]


def build(atlas, output):
    atlas.world_atlas_init.restype = C.c_bool
    assert atlas.world_atlas_init(), 'Atlas initialization failed'
    atlas.world_atlas_trail.argtypes = [C.POINTER(C.c_size_t)]
    atlas.world_atlas_trail.restype = C.POINTER(TrailPoint)
    atlas.world_atlas_sample.argtypes = [C.c_double, C.c_double]
    atlas.world_atlas_sample.restype = Sample
    atlas.world_atlas_world_to_uv.argtypes = [C.c_double, C.c_double, C.POINTER(C.c_double), C.POINTER(C.c_double)]
    atlas.world_atlas_uv_to_world.argtypes = [C.c_double, C.c_double, C.POINTER(C.c_double), C.POINTER(C.c_double)]
    count = C.c_size_t()
    points = atlas.world_atlas_trail(C.byref(count))
    trail = [(points[k].x, points[k].z, points[k].half_width) for k in range(count.value)]
    lengths = [math.hypot(b[0] - a[0], b[1] - a[1]) for a, b in zip(trail, trail[1:])]
    lines = ['# Valwood authored scenery v1 — editable global-map coordinates',
             '# alias u v yaw_degrees scale burial_meters',
             '# Fixed aliases: pine fir broadleaf pine_young rock0..2 fern0..2 deadwood cliff',
             '# Ground Y is sampled at load; positive burial embeds the model.',
             '# Source trail: world_atlas_trail(); all roots keep its walking surface clear.',
             '# This file places scenery only; it does not define collision or gameplay.']
    totals = {}

    def spine(distance, side):
        for (x, z, _), (xx, zz, _), length in zip(trail, trail[1:], lengths):
            if distance <= length:
                tx, tz = (xx - x) / length, (zz - z) / length
                return x + tx * distance - tz * side, z + tz * distance + tx * side, math.degrees(math.atan2(-tz, tx))
            distance -= length
        raise ValueError('Authored placement extends beyond the trail')

    def anchor(u, v):
        x, z = C.c_double(), C.c_double()
        atlas.world_atlas_uv_to_world(u, v, C.byref(x), C.byref(z))
        return x.value, z.value

    def place(alias, x, z, yaw, scale, burial, label):
        sample = atlas.world_atlas_sample(x, z)
        if sample.height < .1 or sample.shore < .5:
            raise ValueError(f'{label}: authored root is wet; choose another explicit position')
        if alias in ['cliff', 'deadwood']:
            gx = atlas.world_atlas_sample(x + 4, z).height - atlas.world_atlas_sample(x - 4, z).height
            gz = atlas.world_atlas_sample(x, z + 4).height - atlas.world_atlas_sample(x, z - 4).height
            # The long local X axis is (cos(yaw), -sin(yaw)); orient it along
            # the terrain contour instead of spanning the downhill gradient.
            if alias == 'cliff' or math.hypot(gx, gz) / 8 > .08:
                contour = math.degrees(math.atan2(gx, gz))
                yaw = min([contour - 180, contour, contour + 180], key=lambda a: abs((a - yaw + 180) % 360 - 180))
        u, v = C.c_double(), C.c_double()
        atlas.world_atlas_world_to_uv(x, z, C.byref(u), C.byref(v))
        yaw = (yaw + 180) % 360 - 180
        lines.append(f'{alias:10s} {u.value:.10f} {v.value:.10f} {yaw:8.3f} {scale:.3f} {burial:.3f} # {label}')
        totals[alias] = totals.get(alias, 0) + 1

    def at_trail(alias, distance, side, yaw, scale, burial, label):
        x, z, tangent_yaw = spine(distance, side)
        place(alias, x, z, tangent_yaw + yaw, scale, burial, label)

    lines += ['', '# Entrance and first 350m from the forest inspection entrance: twelve framing trees.']
    hero = [('pine', 90, -18, 17, 1.08), ('fir', 119, 20, -12, 1.0),
            ('broadleaf', 196, -21, 45, 2.20), ('pine_young', 175, 10, -70, 1.2),
            ('pine', 204, -20, 90, 1.0), ('fir', 232, 18, 130, .94),
            ('broadleaf', 218, 21, -40, 2.15), ('pine', 296, -19, 28, .92),
            ('pine_young', 328, 9, -80, 1.15), ('fir', 365, -20, 15, 1.02),
            ('broadleaf', 402, 17, 65, .9), ('pine', 445, -22, -20, 1.06)]
    for k, (alias, distance, side, yaw, scale) in enumerate(hero):
        at_trail(alias, distance, side, yaw, scale, .035, f'entrance_hero_{k:02d}')

    lines += ['', '# Five asymmetric rock shelves, eight stones per composition.']
    rock_shapes = [(-6, 9), (-3, 12), (0, 8), (2, 11), (5, 9), (7, 14), (9, 10), (12, 13)]
    for pocket, distance in enumerate([105, 164, 224, 294, 369]):
        side = -1 if pocket % 2 else 1
        for k, (along, offset) in enumerate(rock_shapes):
            at_trail(f'rock{(k + pocket) % 3}', distance + along, side * offset,
                     37 * k + 23 * pocket, .40 + .055 * ((k * 3 + pocket) % 8), .12,
                     f'entrance_rock_shelf_{pocket}_{k}')

    lines += ['', '# Ten fern pockets with open gaps between them, twelve plants each.']
    fern_shapes = [(-3.4, 5.0), (-2.2, 6.4), (-1.4, 4.8), (-.3, 6.0), (.4, 7.1), (1.1, 5.1),
                   (2.0, 6.3), (2.6, 4.7), (3.5, 5.7), (4.1, 7.2), (4.8, 5.1), (5.6, 6.3)]
    for pocket, distance in enumerate([94, 124, 155, 187, 219, 252, 285, 319, 359, 406]):
        side = 1 if pocket % 2 else -1
        for k, (along, offset) in enumerate(fern_shapes):
            at_trail(f'fern{(k + pocket) % 3}', distance + along, side * offset,
                     53 * k + 31 * pocket, .66 + .045 * ((k + pocket * 3) % 9), .018,
                     f'entrance_fern_pocket_{pocket}_{k}')

    lines += ['', '# Fallen trunks frame the path; none crosses its walking surface.']
    for k, distance in enumerate([101, 143, 191, 241, 282, 327, 379, 431]):
        at_trail('deadwood', distance, (-1 if k % 2 else 1) * (13 + k % 3 * 2),
                 [-12, 25, -35, 10][k % 4], .78 + (k % 3) * .08, .14, f'entrance_deadwood_{k}')

    lines += ['', '# Rest of the two-kilometer trail: isolated landmarks and four understory pockets.']
    for k, distance in enumerate([610, 810, 1040, 1290, 1540, 1780, 2070, 2230]):
        at_trail(['pine', 'broadleaf', 'fir', 'pine_young'][k % 4], distance,
                 (-1 if k % 2 else 1) * (17 + k % 3 * 3), 43 * k, .9 + k % 3 * .08, .025, f'outer_trail_tree_{k}')
    for pocket, distance in enumerate([650, 1100, 1630, 2170]):
        side = -1 if pocket % 2 else 1
        for k, (along, offset) in enumerate(rock_shapes[:4]):
            at_trail(f'rock{(k + pocket) % 3}', distance + along, side * offset, 63 * k, .48 + .1 * (k % 3), .13,
                     f'outer_trail_stones_{pocket}_{k}')
        for k, (along, offset) in enumerate(fern_shapes[:8]):
            at_trail(f'fern{(k + pocket) % 3}', distance + along, -side * offset, 71 * k, .75 + .035 * k, .02,
                     f'outer_trail_ferns_{pocket}_{k}')

    lines += ['', '# Ulasse shore foreground: four tree frames, eight shore stones, two logs, sixteen ferns.']
    lx, lz = anchor(.4540, .1800)
    for k, (alias, dx, dz) in enumerate([('pine', -34, 22), ('fir', 38, 28), ('broadleaf', 63, -12), ('pine_young', -44, 18)]):
        place(alias, lx + dx, lz + dz, 63 * k, .9, .025, f'lake_tree_{k}')
    for k, (dx, dz) in enumerate([(-20, -10), (-15, -14), (-7, -12), (10, -11), (16, -16), (24, -10), (29, -20), (37, -17)]):
        place(f'rock{k % 3}', lx + dx, lz + dz, 49 * k, .45 + .07 * (k % 4), .15, f'lake_stone_{k}')
    for k, (dx, dz) in enumerate([(-25, 12), (30, 18)]):
        place('deadwood', lx + dx, lz + dz, 85 + 20 * k, .85, .15, f'lake_deadwood_{k}')
    for k in range(16):
        pocket, index = divmod(k, 8)
        along, side = fern_shapes[index]
        place(f'fern{k % 3}', lx + (-23 if pocket else 18) + along, lz + 9 + side,
              47 * k, .75 + .03 * (k % 5), .02, f'lake_fern_pocket_{pocket}_{index}')

    lines += ['', '# Crosswind headland: contour-aligned outcrops, grounded over their full footprint.']
    cx, cz = anchor(.5220, .92525)
    for k, (dx, dz, yaw, scale) in enumerate([(-55, -15, -20, .76), (68, -35, 75, .9), (110, -85, -55, .66)]):
        place('cliff', cx + dx, cz + dz, yaw, scale, 1.8, f'coast_outcrop_{k}')
    for k, (dx, dz) in enumerate([(-26, -8), (-17, -18), (-9, -10), (11, -14), (23, -7), (32, -24), (40, -15), (-40, -23), (56, -30), (65, -18)]):
        place(f'rock{k % 3}', cx + dx, cz + dz, 67 * k, .48 + .07 * (k % 4), .16, f'coast_stone_{k}')
    for k, (dx, dz) in enumerate([(-29, -31), (35, -42)]):
        place('deadwood', cx + dx, cz + dz, -5 + 180 * k, .82, .17, f'coast_deadwood_{k}')
    for k, (dx, dz) in enumerate([(-45, -75), (75, -110)]):
        place('pine_young', cx + dx, cz + dz, 85 * k, .8, .02, f'coast_young_pine_{k}')

    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text('\n'.join(lines) + '\n')
    atlas.world_atlas_shutdown()
    print(f'Wrote {sum(totals.values())} authored instances to {output}')
    print(totals)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=ROOT / 'projects/valwood_world/scenery/authored_instances.txt')
    parser.add_argument('--force', action='store_true', help='Explicitly replace the editable composition')
    args = parser.parse_args()
    if args.output.exists() and not args.force:
        parser.error('Output already exists; edit it directly, or pass --force to rebuild')
    with tempfile.TemporaryDirectory(prefix='valwood-authored-atlas-') as temporary:
        library = Path(temporary) / 'atlas.so'
        subprocess.run(['cc', '-std=c99', '-O2', '-shared', '-fPIC', '-I', str(ROOT), str(ROOT / 'engine/world_atlas.c'), '-lm', '-o', str(library)], check=True)
        build(C.CDLL(str(library)), args.output)


if __name__ == '__main__':
    main()
