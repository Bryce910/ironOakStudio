#!/usr/bin/env python3
"""Compose Old Tanthoon's rainforest from an editable, bounded local layout.

Uses the starting-area author's actual GLB lower-body/prop footprints and the
real CPU atlas. Existing outputs require --force; --validate-only checks the
actual edited instance file without rewriting any evidence or placements.
"""
import argparse
import ctypes as C
import hashlib
import html
import json
import math
from pathlib import Path
import random
import tempfile

from build_starting_area import (Author, Atlas, DIRECTORY, LEGACY, MAX_BURIAL, PLANTS, SUPPORT, TREES,
                                 rectangle_distance)

WIDTH = 100800.0
DEPTH = WIDTH * 2818 / 5009
ALIASES = {'broadleaf', 'fir', 'pine_young', 'deadwood',
           'fern0', 'fern1', 'fern2', 'rock0', 'rock1', 'rock2',
           'starting_celandine', 'starting_wildflower'}
SHOWCASE = {'showcase_roots','showcase_stump','showcase_shrub','showcase_shrub_b','showcase_sorrel'}
ALIASES.update(SHOWCASE)
LEGACY.update({alias:alias for alias in SHOWCASE})
MAX_BURIAL.update(showcase_roots=.45,showcase_stump=.20,showcase_shrub=.10,showcase_shrub_b=.10,showcase_sorrel=.03)
SUPPORT.update(['showcase_roots','showcase_stump'])
PLANTS.update(['showcase_shrub','showcase_shrub_b','showcase_sorrel'])


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def existing_sources(layout):
    result = []
    for entry in layout['preserve']['existing_sources']:
        path = DIRECTORY / entry['file']
        assert digest(path) == entry['sha256'], f'Protected source changed: {path}'
        aliases = [line.split()[0] for line in path.read_text().splitlines()
                   if line.strip() and not line.lstrip().startswith('#')]
        assert len(aliases) == entry['records'], path
        result.append(dict(file=entry['file'], records=len(aliases), sha256=digest(path), aliases=aliases))
    return result


class RainforestAuthor(Author):
    def __init__(self, layout, atlas):
        super().__init__(layout,atlas)
        atlas.api.world_atlas_local_water.argtypes=[C.c_double,C.c_double,C.POINTER(C.c_float),C.POINTER(C.c_float)]
        atlas.api.world_atlas_local_water.restype=C.c_bool
        pond=layout['pond'];surface=C.c_float();depth=C.c_float()
        assert atlas.api.world_atlas_local_water(*self.world(*pond['center_rf']),C.byref(surface),C.byref(depth))
        assert abs(surface.value-pond['surface_y'])<.001 and depth.value>0

    def pool_overlap(self,record):
        pond=self.layout['pond'];low,high=self.walking_bounds(record['alias'],record['scale']);scale=record['scale']
        angle=math.radians(record['heading']);co,si=math.cos(angle),math.sin(angle)
        cx,cz=(low[0]+high[0])*scale*.5,(low[2]+high[2])*scale*.5
        center=(record['r']+co*cx+si*cz,record['f']+si*cx-co*cz)
        if record['alias'] not in TREES:
            # A conservative normalized ellipse/circumcircle bound keeps the
            # entire dry-bank prop beyond the water, not just its root.
            # Only tree trunks retain the larger bank reserve; photographed
            # groundcover/root clusters may occupy its dry feather.
            radius=math.hypot(max(abs(low[0]),abs(high[0])),max(abs(low[2]),abs(high[2])))*scale
            q=math.hypot((record['r']-pond['center_rf'][0])/pond['radii_rf'][0],(record['f']-pond['center_rf'][1])/pond['radii_rf'][1])
            return q<1+radius/min(pond['radii_rf'])+.04
        minimum,maximum=pond['reserve_min_rf'],pond['reserve_max_rf'];middle=[(a+b)*.5 for a,b in zip(minimum,maximum)];half=[(b-a)*.5 for a,b in zip(minimum,maximum)]
        hx,hz=(high[0]-low[0])*scale*.5,(high[2]-low[2])*scale*.5;u,v=(co,si),(si,-co)
        for ax,az in [(1,0),(0,1),u,v]:
            distance=abs((center[0]-middle[0])*ax+(center[1]-middle[1])*az)
            if distance>half[0]*abs(ax)+half[1]*abs(az)+hx*abs(u[0]*ax+u[1]*az)+hz*abs(v[0]*ax+v[1]*az):return False
        return True

    def spawn_clearance(self, record):
        low, high = self.walking_bounds(record['alias'], record['scale'])
        angle = math.radians(record['heading']); co, si = math.cos(angle), math.sin(angle)
        clearance=math.inf
        for view in [self.layout['spawn']]+self.layout.get('extra_clear_views',[]):
            r,f=view['position_rf'];dx,dz=r-record['r'],f-record['f'];point=[co*dx+si*dz,si*dx-co*dz]
            clearance=min(clearance,rectangle_distance(point,point,[low[0]*record['scale'],low[2]*record['scale']],
                [high[0]*record['scale'],high[2]*record['scale']])-view['clear_radius_m'])
        return clearance

    def place(self, alias, r, f, heading=0, scale=1, burial=0, label='', pocket=False, support=None):
        assert alias in ALIASES, f'Unsupported rainforest alias: {alias}'
        assert support is None, 'Rainforest has no stacked props or architecture'
        record = dict(alias=alias, r=r, f=f, heading=heading, scale=scale)
        crowded = alias in TREES and any(old['alias'] in TREES and math.hypot(old['r']-r, old['f']-f)
                                         < self.layout['minimum_tree_separation_m'] for old in self.records)
        if crowded or self.spawn_clearance(record) < .12 or self.path_clearance(record) < .16 or self.pool_overlap(record):
            if pocket:
                key = label.split('_')[0]; self.skipped[key] = self.skipped.get(key, 0)+1
                return None
            raise ValueError(f'{label}: blocked spawn/path/pool reserve or crowded tree roots')
        result = super().place(alias, r, f, heading, scale, burial, label, pocket, support)
        if result is None:
            return None
        # Full prop footprint stays on dry ground. This is independent of the
        # route check and uses the actual rotated/scaled model bounds.
        low, high = self.walking_bounds(alias,scale); angle = math.radians(self.base_yaw+heading)
        co, si = math.cos(angle), math.sin(angle)
        heights = []
        for iz in range(5):
            for ix in range(5):
                px = (low[0]+(high[0]-low[0])*ix/4)*scale
                pz = (low[2]+(high[2]-low[2])*iz/4)*scale
                ground = self.atlas.sample(result['x']+co*px+si*pz, result['z']-si*px+co*pz)
                assert ground.height > .1 and ground.shore > .5 and ground.land > .75, label
                surface=C.c_float();depth=C.c_float()
                wet=self.atlas.api.world_atlas_local_water(result['x']+co*px+si*pz,result['z']-si*px+co*pz,C.byref(surface),C.byref(depth))
                if wet:
                    if pocket:
                        self.records.pop();key=label.split('_')[0];self.skipped[key]=self.skipped.get(key,0)+1
                        return None
                    raise ValueError(f'{label}: actual wet pool footprint')
                heights.append(ground.height)
        result['footprint_ground_relief_m'] = max(heights)-min(heights)
        return result

    def root_height(self, record):
        if record['alias'] not in SUPPORT:
            return super().root_height(record)
        low, high = self.model(record['alias']); scale = record['scale']
        angle = math.radians(self.base_yaw+record['heading']); co, si = math.cos(angle), math.sin(angle)
        lowest = record['ground_height']; steps_x = 8 if record['alias'] == 'deadwood' else 4
        for iz in range(5):
            for ix in range(steps_x+1):
                px = (low[0]+(high[0]-low[0])*ix/steps_x)*scale
                pz = (low[2]+(high[2]-low[2])*iz/4)*scale
                lowest = min(lowest, self.atlas.sample(record['x']+co*px+si*pz, record['z']-si*px+co*pz).height)
        return lowest-record['burial']-.05


def trail_r(layout, f):
    points = layout['paths'][0]['points_rf']
    for a, b in zip(points, points[1:]):
        if a[1] <= f <= b[1]:
            return a[0]+(b[0]-a[0])*(f-a[1])/(b[1]-a[1])
    return points[0 if f < points[0][1] else -1][0]


def compose(author):
    layout = author.layout
    def attempt(alias, r, f, label, scale_range, burial, spread=(2, 3), rng=None):
        rng = rng or random.Random(layout['seed']+'_'+label)
        for trial in range(160):
            grow = 1+min(trial, 40)*.18
            pr = r+rng.uniform(-spread[0], spread[0])*grow
            pf = f+rng.uniform(-spread[1], spread[1])*grow
            if not (layout['bounds']['min_rf'][0]+1 <= pr <= layout['bounds']['max_rf'][0]-1 and
                    layout['bounds']['min_rf'][1]+1 <= pf <= layout['bounds']['max_rf'][1]-1):
                continue
            record = author.place(alias, pr, pf, rng.uniform(-180, 180), rng.uniform(*scale_range), burial, label, pocket=True)
            if record is not None:
                return record
        raise ValueError(f'No safe composed position for {label}')
    # Fifteen irregular silhouettes per grove: ten broad crowns interwoven
    # with five taller firs. Each grove's spread/mirror and world-seeded yaw
    # differ; close crowns frame the walk while their lower trunks stay clear.
    for grove in layout['groves']:
        rng = random.Random(layout['seed']+'_'+grove['id'])
        for i, (dr, df) in enumerate(layout['mature_offsets_rf']):
            alias = 'broadleaf' if i < 10 else 'fir'
            f = grove['center_f']+df*grove['spread_rf'][1]
            r = trail_r(layout, f)+dr*grove['spread_rf'][0]*grove['mirror']
            attempt(alias, r, f, f'{grove["id"]}_canopy_{i:02d}',
                    (1.6, 2.35) if alias == 'broadleaf' else (1.2, 1.7), .025, rng=rng)
    for i, (offset, f) in enumerate(layout['regrowth_offsets_rf']):
        attempt('pine_young', trail_r(layout, f)+offset, f, f'regrowth_{i:02d}', (.95, 1.35), .018)
    for bed_index, bed in enumerate(layout['fern_beds']):
        rng = random.Random(layout['seed']+'_'+bed['id'])
        r = trail_r(layout, bed['center_f'])+bed['offset_r']
        for i in range(bed['count']):
            angle = rng.uniform(0, math.tau); radius = math.sqrt(rng.random())
            pr = r+math.cos(angle)*radius*bed['radii_rf'][0]
            pf = bed['center_f']+math.sin(angle)*radius*bed['radii_rf'][1]
            attempt(f'fern{(bed_index*bed["count"]+i)%3}', pr, pf,
                    f'{bed["id"]}_frond_{i:02d}', (.9, 1.6), .012, (.8, 1.2), rng)
    for k, bed in enumerate(layout['rock_beds']):
        rng = random.Random(layout['seed']+'_'+bed['id'])
        for i in range(bed['count']):
            pr = trail_r(layout, bed['center_f'])+bed['offset_r']+rng.uniform(-3.8, 3.8)
            pf = bed['center_f']+rng.uniform(-5.5, 5.5)
            attempt(f'rock{(k*bed["count"]+i)%3}', pr, pf,
                    f'{bed["id"]}_stone_{i:02d}', (.65, 1.35), .14, (1.5, 2), rng)
    for i, (offset, f) in enumerate(layout['fallen_log_offsets_rf']):
        attempt('deadwood', trail_r(layout, f)+offset, f, f'fallen_trunk_{i:02d}', (.9, 1.4), .14)
    for bed in layout['flower_beds']:
        rng = random.Random(layout['seed']+'_'+bed['id'])
        for i in range(bed['count']):
            alias = 'starting_celandine' if i % 3 else 'starting_wildflower'
            attempt(alias, trail_r(layout, bed['center_f'])+bed['offset_r']+rng.uniform(-2.8, 2.8),
                    bed['center_f']+rng.uniform(-3.4, 3.4), f'{bed["id"]}_herb_{i:02d}', (.8, 1.15), .008, (.8, 1), rng)
    for bed in layout['showcase_beds']:
        rng=random.Random(layout['seed']+'_'+bed['id'])
        for i in range(bed['count']):
            angle=rng.uniform(0,math.tau);radius=math.sqrt(rng.random())
            center=bed.get('center_rf',[trail_r(layout,bed['center_f'])+bed['offset_r'],bed['center_f']])
            if bed.get('at_tree_base'):
                tree=min((record for record in author.records if record['alias'] in TREES),key=lambda record:math.dist([record['r'],record['f']],center))
                center=[tree['r'],tree['f']]
            r=center[0]+math.cos(angle)*radius*bed['radii_rf'][0]
            f=center[1]+math.sin(angle)*radius*bed['radii_rf'][1]
            attempt(bed['alias'],r,f,f'{bed["id"]}_detail_{i:03d}',tuple(bed['scale_range']),bed['burial'],tuple(bed.get('jitter_spread_rf',[2,3])),rng)


def validate_file(author, path):
    for line_number, line in enumerate(path.read_text().splitlines(), 1):
        content, *comment = line.split('#', 1); parts = content.split()
        if not parts:
            continue
        assert len(parts) in [6, 7], f'line{line_number}: expected six fields'
        alias = parts[0]; u, v, yaw, scale, burial = map(float, parts[1:6])
        assert all(math.isfinite(value) for value in [u, v, yaw, scale, burial])
        assert 0 <= u <= 1 and 0 <= v <= 1 and -360 <= yaw <= 360
        assert len(parts) == 6 or float(parts[6]) == 0, 'Rainforest does not use elevated props'
        r, f = author.local((u-.5)*WIDTH, (v-.5)*DEPTH)
        assert not any(old['alias'] == alias and math.hypot(old['r']-r, old['f']-f) < .01 for old in author.records), f'line{line_number}: duplicate root'
        author.place(alias, r, f, yaw-author.base_yaw, scale, burial,
                     comment[0].strip() if comment else f'line{line_number}')


def path_report(author):
    length = 0.; maximum_grade = 0.; heights = []
    for path in author.layout['paths']:
        for a, b in zip(path['points_rf'], path['points_rf'][1:]):
            distance = math.dist(a, b); length += distance; steps = math.ceil(distance/.5)
            previous = None
            for i in range(steps+1):
                r = a[0]+(b[0]-a[0])*i/steps; f = a[1]+(b[1]-a[1])*i/steps
                height = author.atlas.sample(*author.world(r, f)).height; heights.append(height)
                if previous is not None:
                    maximum_grade = max(maximum_grade, abs(height-previous)/(distance/steps))
                previous = height
    assert maximum_grade < 1.2, f'Path exceeds actual walking slope budget: {maximum_grade}'
    return dict(length_m=length, return_walk_m=2*length, clear_width_m=3.2, sampled_step_m=.5,
                max_absolute_grade=maximum_grade, min_height_m=min(heights), max_height_m=max(heights),
                capsule_radius_m=.37, low_body_test_height_m=2.2)


def svg(author):
    def xy(r, f): return 440+r*3.4, 445-f*3.4
    parts = ['<svg xmlns="http://www.w3.org/2000/svg" width="880" height="920" viewBox="0 0 880 920">',
             '<rect width="880" height="920" fill="#132c25"/>',
             '<text x="32" y="30" fill="#e7e6c7" font-family="sans-serif" font-size="23">Old Tanthoon · rainforest canopy walk</text>']
    for path in author.layout['paths']:
        points = ' '.join(f'{x:.2f},{y:.2f}' for x, y in [xy(*p) for p in path['points_rf']])
        parts.append(f'<polyline points="{points}" fill="none" stroke="#b5aa79" stroke-width="10.88" stroke-linejoin="round" stroke-linecap="round"/>')
    for record in author.records:
        x, y = xy(record['r'], record['f']); alias = record['alias']; low, high = author.model(alias)
        tree = alias in TREES
        radius = max(high[0]-low[0], high[2]-low[2])*record['scale']*1.7 if tree else (3 if alias in PLANTS else 4)
        color = '#518164' if alias == 'broadleaf' else ('#284d3c' if tree else ('#85b277' if alias in PLANTS else '#888f7e'))
        parts.append(f'<circle cx="{x:.2f}" cy="{y:.2f}" r="{radius:.2f}" fill="{color}" opacity="{.38 if tree else .9}" stroke="#a4b487" stroke-width=".35"><title>{html.escape(record["label"])}</title></circle>')
        if tree:
            parts.append(f'<circle cx="{x:.2f}" cy="{y:.2f}" r="2.2" fill="#d6cf9a"/>')
    x, y = xy(*author.layout['spawn']['position_rf'])
    parts.append(f'<circle cx="{x:.2f}" cy="{y:.2f}" r="17" fill="none" stroke="#e9cc80" stroke-width="2"/><text x="{x+22:.2f}" y="{y+5:.2f}" fill="#f6df9f" font-family="sans-serif" font-size="14">5 m clear arrival</text>')
    parts += ['<text x="32" y="865" fill="#dfddba" font-family="sans-serif" font-size="15">Five interlocking broadleaf/fir groves · fern beds · bank rocks · fallen timber</text>',
              '<text x="32" y="890" fill="#b2c2ad" font-family="sans-serif" font-size="13">Actual generated roots/crown bounds; overhead authoring plan, not a native render.</text></svg>']
    return '\n'.join(parts)+'\n'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--layout', type=Path, default=DIRECTORY/'rainforest_layout.json')
    parser.add_argument('--instances', type=Path, default=DIRECTORY/'rainforest_instances.txt')
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument('--force', action='store_true', help='Explicitly regenerate editable output placements')
    mode.add_argument('--validate-only', action='store_true', help='Check actual instance text without writing files')
    args = parser.parse_args(); layout = json.loads(args.layout.read_text())
    outputs = [args.instances, args.instances.parent/'rainforest_manifest.json', args.instances.parent/'rainforest_layout.svg']
    if any(path.exists() for path in outputs) and not (args.force or args.validate_only):
        parser.error('Editable rainforest outputs already exist; edit/validate them or pass --force')
    protected = existing_sources(layout)
    with tempfile.TemporaryDirectory(prefix='valwood-rainforest-author-') as temporary:
        atlas = Atlas(temporary)
        try:
            author = RainforestAuthor(layout, atlas)
            if args.validate_only:
                validate_file(author, args.instances)
            else:
                compose(author)
            assert 300 <= len(author.records) <= layout['budgets']['new_records_max']
            existing = sum(source['records'] for source in protected)
            assert existing+len(author.records) <= layout['budgets']['combined_records_max']
            counts = {alias: sum(record['alias'] == alias for record in author.records) for alias in sorted(author.bounds)}
            combined = [alias for source in protected for alias in source['aliases']]+[r['alias'] for r in author.records]
            largest_batch = max(combined.count(alias) for alias in set(combined)); assert largest_batch <= 768
            output = ['# Old Tanthoon rainforest — editable authored canopy walk',
                      '# alias map_u map_v yaw_degrees scale burial_meters',
                      '# Source layout: rainforest_layout.json; all original placement sources retained.',
                      '# Temperate fantasy rainforest: broadleaf/fir canopy, fern beds, downloaded roots/stumps/shrubs/sorrel; no architecture.',
                      '# Powered by Poly Haven. The local pool/bank reserve and actual water API keep terrestrial bodies dry.']
            for record in author.records:
                yaw = (author.base_yaw+record['heading']+180) % 360-180
                output.append(f'{record["alias"]:20s} {record["x"]/WIDTH+.5:.10f} {record["z"]/DEPTH+.5:.10f} {yaw:9.4f} {record["scale"]:.4f} {record["burial"]:.4f} # {record["label"]}')
            text = '\n'.join(output)+'\n'
            manifest = dict(schema_version=1, layout_source=args.layout.name, instances_source=args.instances.name,
                            layout_sha256=digest(args.layout), instances_sha256=digest(args.instances) if args.validate_only else hashlib.sha256(text.encode()).hexdigest(),
                            seed=layout['seed'], record_count=len(author.records), existing_records=existing,
                            combined_records=existing+len(author.records), counts=counts, largest_combined_alias_batch=largest_batch,
                            protected_sources=[{k: v for k, v in source.items() if k != 'aliases'} for source in protected],
                            route=path_report(author), actual_glb_bounds=author.bounds, near_triangles_by_model=author.triangles,
                            unculled_near_triangle_instances=sum(author.triangles[r['alias']] for r in author.records),
                            minimum_path_footprint_margin_m=min(author.path_clearance(r) for r in author.records),
                            minimum_spawn_footprint_margin_m=min(author.spawn_clearance(r) for r in author.records),
                            minimum_tree_root_separation_m=min(math.hypot(a['r']-b['r'], a['f']-b['f']) for i, a in enumerate(author.records) if a['alias'] in TREES for b in author.records[i+1:] if b['alias'] in TREES),
                            min_ground_height_m=min(r['ground_height'] for r in author.records), max_ground_height_m=max(r['ground_height'] for r in author.records),
                            showcase_records=sum(r['alias'] in SHOWCASE for r in author.records),pool=layout['pond'],
                            skipped_candidates=author.skipped,
                            validation_scope='Actual GLB triangle/2.2m plane intersections for trees, full prop bounds, exact CPU atlas/local-water query, tree-body bank reserve plus conservative full-prop dry-bank ellipse clearance, dry footprint samples, 5m clear arrival/1.5m pool camera, preserved sources. Crowns may overhang. Native capsule/render/performance validation is independent.',
                            records=author.records)
            # Recheck preserved inputs after authoring, before any output write.
            existing_sources(layout)
            if not args.validate_only:
                args.instances.parent.mkdir(parents=True, exist_ok=True)
                args.instances.write_text(text)
                outputs[1].write_text(json.dumps(manifest, indent=2, allow_nan=False)+'\n')
                outputs[2].write_text(svg(author))
            print(json.dumps({k: v for k, v in manifest.items() if k not in ['records', 'actual_glb_bounds']}, indent=2, allow_nan=False))
        finally:
            atlas.api.world_atlas_shutdown()


if __name__ == '__main__':
    main()
