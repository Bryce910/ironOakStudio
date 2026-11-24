#!/usr/bin/env python3
"""Author the compact Silverpine Waystation from an editable local layout.

The JSON is the terrain/path source. This tool composes named, bounded pockets,
checks real GLB footprints and atlas elevations, and writes ordinary editable
instance text plus a placement manifest and an SVG plan. Existing outputs are
preserved unless --force is supplied; --validate-only checks direct text edits.
"""
from pathlib import Path
import argparse
import ctypes as C
import hashlib
import html
import json
import math
import random
import struct
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
DIRECTORY = ROOT / 'projects/valwood_world/scenery'
MODELS = ROOT / 'projects/valwood_world/models'
LEGACY = {'pine': 'cinematic_pine_near', 'fir': 'cinematic_fir_near',
          'broadleaf': 'cinematic_broadleaf_near', 'pine_young': 'cinematic_pine_young_near',
          **{f'rock{i}': f'cinematic_rock_{i}' for i in range(3)},
          **{f'fern{i}': f'woodland_fern_{i}' for i in range(3)},
          'deadwood': 'cinematic_deadwood'}
PLANTS = {'starting_celandine', 'starting_wildflower', 'fern0', 'fern1', 'fern2'}
TREES = {'pine', 'fir', 'broadleaf', 'pine_young'}
SUPPORT = {'rock0', 'rock1', 'rock2', 'deadwood', 'starting_crate', 'starting_barrel',
           'starting_bucket', 'starting_lantern', 'starting_axe', 'starting_stump', 'starting_firepit'}
ELEVATED = {'starting_crate', 'starting_barrel', 'starting_bucket', 'starting_lantern', 'starting_axe'}
STARTING = {'starting_gate', 'starting_wall', 'starting_tower', 'starting_platform', 'starting_table',
            'starting_crate', 'starting_barrel', 'starting_bucket', 'starting_lantern', 'starting_axe',
            'starting_stump', 'starting_firepit', 'starting_celandine', 'starting_wildflower'}
MAX_BURIAL = {'pine':1, 'fir':1, 'broadleaf':.5, 'pine_young':.25, 'deadwood':.3,
              **{f'rock{i}':2 for i in range(3)}, **{f'fern{i}':.2 for i in range(3)},
              **{name:.5 for name in ['starting_gate','starting_wall','starting_tower','starting_platform']},
              **{name:.3 for name in ['starting_table','starting_crate','starting_barrel','starting_stump','starting_firepit']},
              **{name:.15 for name in ['starting_bucket','starting_lantern','starting_axe','starting_celandine','starting_wildflower']}}


def multiply(a, b):
    return [sum(a[k * 4 + r] * b[c * 4 + k] for k in range(4)) for c in range(4) for r in range(4)]


IDENTITY = [1., 0., 0., 0., 0., 1., 0., 0., 0., 0., 1., 0., 0., 0., 0., 1.]


def node_matrix(node):
    if 'matrix' in node:
        return node['matrix']
    x, y, z, w = node.get('rotation', [0, 0, 0, 1])
    sx, sy, sz = node.get('scale', [1, 1, 1])
    tx, ty, tz = node.get('translation', [0, 0, 0])
    return [(1 - 2 * (y*y + z*z))*sx, 2*(x*y + z*w)*sx, 2*(x*z-y*w)*sx, 0,
            2*(x*y-z*w)*sy, (1-2*(x*x+z*z))*sy, 2*(y*z+x*w)*sy, 0,
            2*(x*z+y*w)*sz, 2*(y*z-x*w)*sz, (1-2*(x*x+y*y))*sz, 0, tx, ty, tz, 1]


def glb_bounds(path, with_vertices=False):
    """Read real vertex positions/nodes, never source preview dimensions."""
    with path.open('rb') as file:
        magic, version, total = struct.unpack('<III', file.read(12))
        assert magic == 0x46546c67 and version == 2 and total == path.stat().st_size, path
        length, kind = struct.unpack('<II', file.read(8)); assert kind == 0x4e4f534a
        data = json.loads(file.read(length))
        length, kind = struct.unpack('<II', file.read(8)); assert kind == 0x004e4942
        binary_offset = file.tell()
        accessor_cache = {}

        def positions(index):
            if index not in accessor_cache:
                accessor = data['accessors'][index]
                assert accessor['componentType'] == 5126 and accessor['type'] == 'VEC3'
                assert not accessor.get('sparse')
                view = data['bufferViews'][accessor['bufferView']]
                stride = view.get('byteStride', 12)
                file.seek(binary_offset + view.get('byteOffset', 0) + accessor.get('byteOffset', 0))
                raw = file.read(stride * (accessor['count'] - 1) + 12)
                accessor_cache[index] = [struct.unpack_from('<fff', raw, i*stride) for i in range(accessor['count'])]
            return accessor_cache[index]

        minimum, maximum = [math.inf]*3, [-math.inf]*3
        transformed=[];triangles=[]

        def visit(index, parent, depth=0):
            assert depth < 512
            node = data['nodes'][index]; matrix = multiply(parent, node_matrix(node))
            if 'mesh' in node:
                for primitive in data['meshes'][node['mesh']]['primitives']:
                    first=len(transformed)
                    for point in positions(primitive['attributes']['POSITION']):
                        p = [sum(matrix[k*4+j]*point[k] for k in range(3))+matrix[12+j] for j in range(3)]
                        assert all(math.isfinite(v) for v in p)
                        if with_vertices: transformed.append(p)
                        for j in range(3):
                            minimum[j] = min(minimum[j], p[j]); maximum[j] = max(maximum[j], p[j])
                    if with_vertices:
                        count=len(transformed)-first
                        indices=list(range(count))
                        if 'indices' in primitive:
                            accessor=data['accessors'][primitive['indices']];view=data['bufferViews'][accessor['bufferView']]
                            kind={5121:'B',5123:'H',5125:'I'}[accessor['componentType']];size=struct.calcsize('<'+kind)
                            stride=view.get('byteStride',size)
                            file.seek(binary_offset+view.get('byteOffset',0)+accessor.get('byteOffset',0))
                            raw=file.read(stride*(accessor['count']-1)+size)
                            indices=[struct.unpack_from('<'+kind,raw,i*stride)[0] for i in range(accessor['count'])]
                        assert len(indices)%3==0
                        triangles.extend(tuple(transformed[first+indices[i+j]] for j in range(3))
                                         for i in range(0,len(indices),3))
            for child in node.get('children', []):
                visit(child, matrix, depth+1)

        for node in data['scenes'][data.get('scene', 0)]['nodes']:
            visit(node, IDENTITY)
    assert all(math.isfinite(v) for v in minimum + maximum)
    floor=minimum[1]
    maximum[1] -= floor; minimum[1] = 0.  # Same root normalization as world_models.
    if with_vertices:
        for point in transformed:point[1]-=floor
        return minimum,maximum,transformed,triangles
    return minimum,maximum


class Sample(C.Structure):
    _fields_ = [('height', C.c_float), ('region', C.c_int), ('forest', C.c_float),
                ('moisture', C.c_float), ('shore', C.c_float), ('land', C.c_float)]


class TrailPoint(C.Structure):
    _fields_ = [('x', C.c_double), ('z', C.c_double), ('half_width', C.c_float)]


class Atlas:
    def __init__(self, directory):
        library = Path(directory) / 'atlas.so'
        subprocess.run(['cc', '-std=c99', '-O2', '-shared', '-fPIC', '-I', str(ROOT),
                        str(ROOT/'engine/world_atlas.c'), '-lm', '-o', str(library)], check=True)
        self.api = C.CDLL(str(library)); self.api.world_atlas_init.restype = C.c_bool
        assert self.api.world_atlas_init()
        self.api.world_atlas_sample.argtypes = [C.c_double, C.c_double]; self.api.world_atlas_sample.restype = Sample
        self.api.world_atlas_walk_height.argtypes = [C.c_double, C.c_double]
        self.api.world_atlas_walk_height.restype = C.c_float
        self.api.world_atlas_trail.argtypes = [C.POINTER(C.c_size_t)]
        self.api.world_atlas_trail.restype = C.POINTER(TrailPoint)
        n = C.c_size_t(); points = self.api.world_atlas_trail(C.byref(n))
        self.trail = [(points[i].x, points[i].z, points[i].half_width) for i in range(n.value)]

    def sample(self, x, z):
        return self.api.world_atlas_sample(x, z)

    def walk_height(self, x, z):
        return self.api.world_atlas_walk_height(x, z)


def segment_distance(p, a, b):
    dx, dz = b[0]-a[0], b[1]-a[1]
    t = max(0, min(1, ((p[0]-a[0])*dx+(p[1]-a[1])*dz)/max(dx*dx+dz*dz, 1e-20)))
    return math.hypot(p[0]-a[0]-dx*t, p[1]-a[1]-dz*t)


def segments_distance(a, b, c, d):
    ux, uz, vx, vz = b[0]-a[0], b[1]-a[1], d[0]-c[0], d[1]-c[1]
    den = ux*vz-uz*vx
    if abs(den) > 1e-12:
        wx, wz = c[0]-a[0], c[1]-a[1]
        t, u = (wx*vz-wz*vx)/den, (wx*uz-wz*ux)/den
        if 0 <= t <= 1 and 0 <= u <= 1:
            return 0.
    return min(segment_distance(a,c,d), segment_distance(b,c,d), segment_distance(c,a,b), segment_distance(d,a,b))


def rectangle_distance(a, b, low, high):
    if any(low[0] <= p[0] <= high[0] and low[1] <= p[1] <= high[1] for p in [a,b]):
        return 0.
    corners = [(low[0],low[1]),(high[0],low[1]),(high[0],high[1]),(low[0],high[1])]
    return min(segments_distance(a,b,c,d) for c,d in zip(corners,corners[1:]+corners[:1]))


class Author:
    def __init__(self, layout, atlas):
        self.layout, self.atlas = layout, atlas
        self.origin = layout['origin']; self.bounds = {}; self.records = []; self.skipped = {}
        self.tree_vertices={};self.tree_triangles={};self.body_bounds={};self.triangles={}
        self.base_yaw = self.origin['model_yaw_for_right_degrees']
        self.routes = [(a,b,p['half_width_m']) for p in layout['paths'] for a,b in zip(p['points_rf'],p['points_rf'][1:])]

    def world(self, r, f):
        o=self.origin; return [o['world_xz'][i]+o['right_xz'][i]*r+o['forward_xz'][i]*f for i in range(2)]

    def local(self, x, z):
        o=self.origin; d=[x-o['world_xz'][0],z-o['world_xz'][1]]
        return [sum(d[i]*o[key][i] for i in range(2)) for key in ['right_xz','forward_xz']]

    def model(self, alias):
        if alias not in self.bounds:
            stem = LEGACY.get(alias, alias)
            if alias not in LEGACY and alias not in STARTING:
                raise ValueError(f'Unsupported fixed model alias: {alias}')
            path=MODELS/(stem+'.glb')
            if alias in TREES:
                low,high,vertices,triangles=glb_bounds(path,True)
                self.bounds[alias]=[low,high];self.tree_vertices[alias]=vertices;self.tree_triangles[alias]=triangles
            else:self.bounds[alias]=glb_bounds(path)
            with path.open('rb') as file:
                file.seek(12);length,kind=struct.unpack('<II',file.read(8));assert kind==0x4e4f534a
                data=json.loads(file.read(length))
            self.triangles[alias]=sum(data['accessors'][p['indices']]['count']//3 if 'indices' in p else
                                      data['accessors'][p['attributes']['POSITION']]['count']//3
                                      for mesh in data['meshes'] for p in mesh['primitives'])
        return self.bounds[alias]

    def walking_bounds(self, alias, scale):
        self.model(alias)
        if alias not in TREES:return self.bounds[alias]
        # Branches may frame the walk overhead. The actual vertices below a
        # conservative2.2m player volume define the lower footprint; round the
        # local cutoff upward so varying scale never omits head-height branches.
        upper=math.ceil(2.2/scale*4)/4;key=(alias,upper)
        if key not in self.body_bounds:
            points=[]
            for triangle in self.tree_triangles[alias]:
                points.extend(p for p in triangle if p[1]<=upper)
                for a,b in zip(triangle,triangle[1:]+triangle[:1]):
                    if (a[1]<upper<b[1]) or (b[1]<upper<a[1]):
                        t=(upper-a[1])/(b[1]-a[1]);points.append([a[i]+t*(b[i]-a[i]) for i in range(3)])
            assert points,alias
            self.body_bounds[key]=[[min(p[i] for p in points) for i in range(3)],
                                   [max(p[i] for p in points) for i in range(3)]]
        return self.body_bounds[key]

    def path_clearance(self, record):
        alias, r, f, heading, scale = [record[k] for k in ['alias','r','f','heading','scale']]
        if alias == 'starting_platform':
            return math.inf
        low, high = self.walking_bounds(alias,scale); co, si = math.cos(math.radians(heading)), math.sin(math.radians(heading))
        def transform(p):
            dx,dz=p[0]-r,p[1]-f; return [co*dx+si*dz,si*dx-co*dz]
        boxes = [([low[0]*scale,low[2]*scale],[high[0]*scale,high[2]*scale])]
        if alias == 'starting_gate':
            boxes = [([low[0]*scale,low[2]*scale],[-1.750665*scale,high[2]*scale]),
                     ([1.750665*scale,low[2]*scale],[high[0]*scale,high[2]*scale])]
        return min(rectangle_distance(transform(a),transform(b),lo,hi)-width
                   for a,b,width in self.routes for lo,hi in boxes)

    def original_clearance(self, record):
        if record['alias'] in ['starting_gate','starting_platform']:
            return math.inf
        x,z=self.world(record['r'],record['f']); low,high=self.model(record['alias'])
        radius=math.hypot(max(abs(low[0]),abs(high[0])),max(abs(low[2]),abs(high[2])))*record['scale']
        return min(segment_distance([x,z],a,b)-max(a[2],b[2])-radius-.35
                   for a,b in zip(self.atlas.trail,self.atlas.trail[1:]))

    def place(self, alias, r, f, heading=0, scale=1, burial=0, label='', pocket=False, support=None):
        if not all(math.isfinite(v) for v in [r,f,heading,scale,burial]):
            raise ValueError(f'{label}: nonfinite placement')
        max_scale = 2.5 if alias=='broadleaf' else (2 if alias in TREES else 4)
        if not .15 <= scale <= max_scale or not 0 <= burial <= MAX_BURIAL.get(alias,-1):
            raise ValueError(f'{label}: invalid scale/burial')
        if any(not self.layout['bounds']['min_rf'][i] <= v <= self.layout['bounds']['max_rf'][i]
               for i,v in enumerate([r,f])):
            raise ValueError(f'{label}: root outside bounded starting-area field')
        record=dict(alias=alias,r=r,f=f,heading=heading,scale=scale,burial=burial,elevation=0.,label=label,support=support)
        if pocket and alias in TREES:
            # Canopies overlap, trunks do not form repeated double-root trees
            # or emerge through authored masonry and work furniture.
            for old in self.records:
                if old['alias'] in TREES and math.hypot(old['r']-r,old['f']-f)<(1.7 if alias=='pine_young' else 3.):
                    self.skipped[label.split('_')[0]]=self.skipped.get(label.split('_')[0],0)+1
                    return None
                if old['alias'] in ['starting_gate','starting_wall','starting_tower','starting_table','starting_platform']:
                    low,high=self.model(old['alias']);ang=math.radians(old['heading']);co,si=math.cos(ang),math.sin(ang)
                    dx,dz=r-old['r'],f-old['f'];p=[co*dx+si*dz,si*dx-co*dz]
                    if rectangle_distance(p,p,[low[0]*old['scale'],low[2]*old['scale']],
                                          [high[0]*old['scale'],high[2]*old['scale']])<.85:
                        self.skipped[label.split('_')[0]]=self.skipped.get(label.split('_')[0],0)+1
                        return None
        new_clearance=self.path_clearance(record); old_clearance=self.original_clearance(record)
        if new_clearance < .12 or old_clearance < 0:
            if pocket:
                self.skipped[label.split('_')[0]]=self.skipped.get(label.split('_')[0],0)+1
                return None
            raise ValueError(f'{label}: footprint blocks path: loop {new_clearance:.3f} / original {old_clearance:.3f}m')
        x,z=self.world(r,f); sample=self.atlas.sample(x,z)
        assert sample.height > .1 and sample.shore > .5, label
        assert burial <= self.model(alias)[1][1]*scale*.6, f'{label}: burial hides model'
        record['x'],record['z']=x,z; record['ground_height']=sample.height
        record['root_y']=self.root_height(record)
        if support:
            parent=next(v for v in self.records if v['label']==support)
            top=parent['root_y']+self.model(parent['alias'])[1][1]*parent['scale']
            record['elevation']=top-record['root_y']+.003
            assert alias in ELEVATED and 0 <= record['elevation'] <= 3, label
            record['root_y']+=record['elevation']
        self.records.append(record); return record

    def root_height(self, record):
        low,high=self.model(record['alias']); x,z=record['x'],record['z']
        height=record['ground_height']-record['burial']; scale=record['scale']
        if record['alias'] in SUPPORT:
            yaw=math.radians(self.base_yaw+record['heading']); co,si=math.cos(yaw),math.sin(yaw)
            support=record['ground_height']
            for iz in range(5):
                for ix in range(5):
                    px=(low[0]+(high[0]-low[0])*ix/4)*scale; pz=(low[2]+(high[2]-low[2])*iz/4)*scale
                    support=min(support,self.atlas.sample(x+co*px+si*pz,z-si*px+co*pz).height)
            height=support-record['burial']-.05
        if record['alias']=='starting_platform': height+=.02-high[1]*scale
        return height


def compose(a):
    add=a.place
    add('starting_gate',-.25,14,burial=.30,label='arrival_arch')
    add('starting_tower',-32,27,burial=.08,label='closed_ruin_tower')
    walls=[(-6.3,14,0,.55),(5.8,14,0,.55),(-10,18,-35,.45),(9.5,18,35,.45),
           (-15.5,31,90,.48),(-15.5,34.65,90,.48),(-15.5,38.3,90,.48),
           (7,54,0,.48),(10.65,54,0,.48),(14.3,54,0,.48),(-24,24,45,.45),(44,65,25,.33),
           (53.8,105,0,.24),(-23.5,131,0,.30),
           (-15.5,41.95,90,.48),(16,58,90,.48),
           (29,76,0,.32),(31.35,76,0,.32),(33.7,76,0,.32),(27.7,73.7,90,.32),
           (50.3,105,0,.24),(52.05,105,0,.24),(55,102.7,90,.24),
           (-25.7,131,0,.30),(-27.5,129.5,90,.30)]
    for i,(r,f,h,s) in enumerate(walls): add('starting_wall',r,f,h,s,.06,f'ruin_wall_{i:02d}')
    for label,r,f,nx,nf in [('courtyard_deck',8,38,3,3),('workshop_deck',34,71,2,3),('overlook_deck',48,100,3,2)]:
        for iz in range(nf):
            for ix in range(nx):
                add('starting_platform',r+(ix-(nx-1)/2)*2.50,f+(iz-(nf-1)/2)*2.18,
                    0,1,0,f'{label}_{ix}_{iz}')
    tables=[('court_table',8,38,0),('work_table',33,70,0),('supply_table',41,62,90),
            ('grove_table',-24,125,-25),('overlook_table',50.3,100.3,0)]
    for label,r,f,h in tables:
        add('starting_table',r,f,h,1,.015,label)
        add('starting_lantern',r+.45,f+.15,h,1,0,label+'_lantern',support=label)
        add('starting_bucket',r-.40,f-.30,h,.75,0,label+'_bucket',support=label)
    # Each supply stack has an intentionally dressed top rather than a ground
    # pile. Heights come from actual model bounds and integrated terrain.
    for i,(r,f,h) in enumerate([(12,34,12),(39.4,75,-15),(43,60,20),(-27,121,-18)]):
        base=f'supply_stack_{i}'
        add('starting_crate',r,f,h,1.15,.02,base)
        add('starting_crate',r+.02,f+.02,h+8,1.05,0,base+'_top',support=base)
        add('starting_lantern',r-.16,f+.02,h,.80,0,base+'_lamp',support=base+'_top')
        add('starting_crate',r+1.05,f-.25,h-13,1.0,.025,base+'_side')
    for i,(r,f) in enumerate([(11,33),(13,35),(41,78),(43,80),(44,61),(-28,122),(-30,123),(-12,57)]):
        add('starting_barrel',r,f,31*i, .90+.05*(i%3),.025,f'supply_barrel_{i}')
    for i,(r,f) in enumerate([(31,73),(-15,109)]):
        add('starting_firepit',r,f,21*i,1.20,.04,f'cold_firepit_{i}')
        add('starting_bucket',r+1.35,f+.8,55*i,1,0,f'fire_bucket_{i}')
    for i,(r,f) in enumerate([(30,69),(29,75),(-12,111),(-18,110),(52,104),(-28,129),(15,54),(-9,25)]):
        add('starting_stump',r,f,37*i,.80+.08*(i%3),.035,f'work_stump_{i}')
        if i in [0,2,7]: add('starting_axe',r+.08,f+.03,45*i,1,0,f'work_axe_{i}',support=f'work_stump_{i}')
    trees=[('fir',-51,4,1.10),('pine',47,-1,1.08),('fir',-60,38,1.08),('pine',62,35,.98),
           ('broadleaf',-57,69,1.20),('fir',69,73,1.06),('pine',-63,116,1.04),('fir',67,126,1.10),
           ('broadleaf',-34,148,1.25),('fir',22,153,1.05),('pine',-78,49,1.0),('fir',84,91,1.08),
           ('broadleaf',-76,95,1.15),('pine',75,146,1.02),('fir',-63,149,1.12),('broadleaf',52,145,1.15),
           ('pine_young',-10,2,1.10),('pine_young',14,6,1.16),('pine_young',-28,65,1.08),
           ('pine_young',25,88,1.20),('pine_young',60,93,1.14),('pine_young',36,127,1.05),
           ('pine_young',-34,139,1.15),('pine_young',-57,86,1.20),('pine_young',-48,48,1.06),
           ('broadleaf',-18,143,1.05),('fir',-85,15,1.06),('pine',81,17,.98),('broadleaf',85,121,1.20),
           ('fir',-91,134,1.05),('pine_young',16,101,1.20),('pine_young',-8,91,1.12)]
    for i,(alias,r,f,s) in enumerate(trees): add(alias,r,f,53*i,s,.025,f'frame_tree_{i:02d}')
    # Overlapping cohorts close the long park-like sightlines. Each belt has
    # an authored purpose and a different irregular silhouette; crowns may
    # overhang the loop while the real lower trunk/branch volume stays clear.
    cohorts=[('entrywest',-16,-2,8,11),('entryeast',17,3,8,10),
             ('courtbosk',-28,56,11,10),('courtcanopy',20,55,9,10),
             ('workbosk',39,86,10,10),('eastwood',65,76,13,16),
             ('lookoutrim',66,115,15,13),('groveback',-17,147,12,12),
             ('grovewood',-48,132,15,13),('returnwood',-46,82,15,15)]
    offsets=[(-.78,-.46),(.38,-.89),(-.12,.12),(.84,-.19),(-.60,.78),(.32,.70),(.91,.91)]
    species=['fir','broadleaf','fir','fir','broadleaf','fir','pine']
    for k,(name,r,f,rr,rf) in enumerate(cohorts):
        rng=random.Random('canopy_'+name)
        for i,(dr,df) in enumerate(offsets):
            alias=species[(i+k*2)%len(species)]
            if abs(r+dr*rr)<14 and alias=='broadleaf':alias='fir'
            scale=rng.uniform(1.20,1.75) if alias=='broadleaf' else rng.uniform(.85,1.18)
            add(alias,r+dr*rr+rng.uniform(-.8,.8),f+df*rf+rng.uniform(-.8,.8),
                rng.uniform(-180,180),scale,.025,f'{name}_canopy_{i}',pocket=True)
    for k,(name,r,f) in enumerate([('archregrowth',-10,29),('workregrowth',24,76),
                                  ('lookoutregrowth',57,105),('groveregrowth',-28,137),('returnregrowth',-51,98)]):
        for i,(dr,df) in enumerate([(-2.8,-2.1),(2.1,-1.4),(-.5,1.2),(3.7,3.1),(-3.2,4.8)]):
            add('pine_young',r+dr,f+df,31*k+73*i,1.12+.10*((k+i)%5),.018,
                f'{name}_sapling_{i}',pocket=True)
    # Named asymmetric ground vignettes. Individual candidates are clipped to
    # the actual clear walking strip, never scattered over the whole rectangle.
    beds=[('entryshade',-11,3,5,7,42,8,8),('entrysun',12,3,5,7,34,10,8),
          ('gateleft',-8,20,3,4,26,8,6),('gateright',9,21,3,4,25,8,6),
          ('towerfern',-24,45,7,6,35,8,14),('courtborder',-9,61,6,5,34,8,8),
          ('workedge',44,56,7,4,31,12,8),('worknorth',40,78,6,5,29,8,8),
          ('overlook',62,103,6,7,32,15,8),('grove',-25,128,8,6,43,16,12),
          ('grovesouth',-7,105,6,7,34,10,12),('returnwest',-58,85,7,9,44,12,14),
          ('returnsouth',-44,52,6,6,28,9,10),('insidefern',-19,77,7,8,30,8,14),
          ('courttableleft',2.8,38,.6,2.1,22,8,3),('courttableright',13.8,38,.8,2.1,22,8,3)]
    for bed,r,f,rr,rf,cel,wild,fern in beds:
        rng=random.Random('silverpine_'+bed)
        for i in range(cel+wild+fern):
            angle=rng.uniform(0,math.tau); radius=math.sqrt(rng.uniform(0,1))
            pr=r+math.cos(angle)*radius*rr; pf=f+math.sin(angle)*radius*rf
            alias='starting_celandine' if i<cel else ('starting_wildflower' if i<cel+wild else f'fern{i%3}')
            scale=rng.uniform(.85,1.45) if alias in PLANTS and alias.startswith('starting_') else rng.uniform(.65,1.05)
            add(alias,pr,pf,rng.uniform(-180,180),scale,.008,f'{bed}_plant_{i:03d}',pocket=True)
        for i in range(5):
            angle=(i*1.31+.4); pr=r+math.cos(angle)*rr*.73; pf=f+math.sin(angle)*rf*.70
            add(f'rock{i%3}',pr,pf,41*i,.32+.065*(i%3),.10,f'{bed}_stone_{i}',pocket=True)
    for i,(r,f,h) in enumerate([(-19,10,15),(21,25,-20),(-35,68,30),(58,83,-25),
                               (-39,130,20),(22,140,-25),(-64,99,40),(53,48,15)]):
        add('deadwood',r,f,h,.82,.14,f'fallen_log_{i}',pocket=True)
    edge_beds=[('westentry',-6,8,2.1,3.4),('eastentry',7,8,2.3,3.3),
               ('westarch',-12,23,2.8,3.3),('eastarch',11,25,2.4,3.8),
               ('courtleaf',-10,45,2.7,3.3),('courteast',14,49,2.7,3.0),
               ('courtrear',-23,58,3.4,4.2),('returninside',-25,74,3.3,4.4),
               ('workshade',25,71,3.5,4.0),('workback',37,80,3.2,3.7),
               ('lookoutfern',55,96,3.0,3.6),('lookoutleaf',57,114,3.2,4.3),
               ('groveleaf',-14,126,3.8,3.6),('grovefern',-33,132,3.4,4.3),
               ('westbend',-49,111,3.1,4.0),('southbend',-50,72,3.2,4.4),
               ('insidewood',12,85,3.6,4.0),('insidegrove',17,105,3.6,4.0),
               ('toweredge',-43,36,3.2,4.3),('courtfront',15,34,1.5,3.4)]
    for k,(name,r,f,rr,rf) in enumerate(edge_beds):
        rng=random.Random('frond_'+name)
        for i in range(12):
            angle=rng.uniform(0,math.tau);radius=math.sqrt(rng.random())
            alias=f'fern{(i+k)%3}' if i<10 else 'starting_celandine'
            add(alias,r+math.cos(angle)*rr*radius,f+math.sin(angle)*rf*radius,
                rng.uniform(-180,180),rng.uniform(.78,1.30),.012,f'{name}_frond_{i}',pocket=True)
        for i in range(3):
            add(f'rock{(i+k)%3}',r+rng.uniform(-rr,rr),f+rng.uniform(-rf,rf),
                rng.uniform(-180,180),rng.uniform(.18,.42),.08,f'{name}_bankstone_{i}',pocket=True)
    for i,(r,f,h) in enumerate([(-20,6,18),(24,-1,-32),(-29,53,66),(22,63,-13),
                                (33,85,42),(60,72,18),(63,116,-35),(-16,149,32),
                                (-38,131,-19),(-55,90,44),(-42,52,11),(17,111,-38)]):
        add('deadwood',r,f,h,.68+.065*(i%5),.14,f'canopy_fall_{i}',pocket=True)


def write_svg(author, path):
    def xy(r,f): return 36+(r+125)*3.2, 740-(f+40)*3.2
    parts=['<svg xmlns="http://www.w3.org/2000/svg" width="880" height="830" viewBox="0 0 880 830">',
           '<rect width="880" height="830" fill="#18251e"/>',
           '<text x="35" y="28" fill="#f1e7c8" font-family="sans-serif" font-size="22">Silverpine Waystation · authored placement plan</text>']
    for p in author.layout['pads']:
        x,y=xy(*p['center_rf']);w,h=[v*3.2 for v in p['half_size_rf']]
        parts.append(f'<rect x="{x-w:.2f}" y="{y-h:.2f}" width="{2*w:.2f}" height="{2*h:.2f}" rx="12" fill="#625b42" opacity=".52"/>')
    for p in author.layout['paths']:
        points=' '.join(f'{x:.2f},{y:.2f}' for x,y in [xy(*v) for v in p['points_rf']])
        parts.append(f'<polyline points="{points}" fill="none" stroke="#d5b780" stroke-width="{p["half_width_m"]*6.4:.1f}" stroke-linejoin="round" stroke-linecap="round"/>')
    for r in author.records:
        x,y=xy(r['r'],r['f']); low,high=author.model(r['alias'])
        color='#82ad70' if r['alias'] in PLANTS else ('#326a4b' if r['alias'] in TREES else '#a5a99c')
        if r['alias'].startswith('starting_') and r['alias'] not in PLANTS:
            color='#c2b89a';w=max((high[0]-low[0])*r['scale']*3.2,2);h=max((high[2]-low[2])*r['scale']*3.2,2)
            parts.append(f'<rect x="{x-w/2:.2f}" y="{y-h/2:.2f}" width="{w:.2f}" height="{h:.2f}" transform="rotate({-r["heading"]:.2f} {x:.2f} {y:.2f})" fill="{color}" stroke="#3d382d" stroke-width=".6"><title>{html.escape(r["label"])}</title></rect>')
        else:
            radius=4.5 if r['alias'] in TREES else (1.2 if r['alias'] in PLANTS else 2.1)
            parts.append(f'<circle cx="{x:.2f}" cy="{y:.2f}" r="{radius}" fill="{color}" opacity=".85"/>')
    for p in author.layout['pads']:
        x,y=xy(*p['center_rf']);parts.append(f'<text x="{x+8:.2f}" y="{y-12:.2f}" fill="#f3e6bf" stroke="#18251e" stroke-width="3" paint-order="stroke" font-family="sans-serif" font-size="13">{html.escape(p["id"].replace("_"," "))}</text>')
    parts.append('<text x="36" y="793" fill="#dacfb4" font-family="sans-serif" font-size="14">350 m return walk · 3 m clear loop · six composed pockets · 250 × 220 m authoring field</text>')
    parts.append('<text x="36" y="815" fill="#aebda9" font-family="sans-serif" font-size="12">Actual generated placements; overhead plan only, not a native render or visual-quality proof.</text></svg>')
    path.write_text('\n'.join(parts)+'\n')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--layout',type=Path,default=DIRECTORY/'starting_area_layout.json')
    parser.add_argument('--instances',type=Path,default=DIRECTORY/'starting_area_instances.txt')
    parser.add_argument('--force',action='store_true',help='Explicitly regenerate editable output placements')
    parser.add_argument('--validate-only',action='store_true',help='Validate existing edited placements without rewriting them')
    args=parser.parse_args(); layout=json.loads(args.layout.read_text())
    outputs=[args.instances,args.instances.parent/'starting_area_manifest.json',args.instances.parent/'starting_area_layout.svg']
    if any(p.exists() for p in outputs) and not (args.force or args.validate_only):
        parser.error('Editable placements already exist; edit directly, validate-only, or pass --force')
    with tempfile.TemporaryDirectory(prefix='valwood-starting-area-') as temporary:
        author=Author(layout,Atlas(temporary))
        if args.validate_only:
            for line_number,line in enumerate(args.instances.read_text().splitlines(),1):
                content,*comment=line.split('#',1);parts=content.split()
                if not parts: continue
                assert len(parts) in [6,7], f'line{line_number}: bad record'
                alias=parts[0];u,v,yaw,scale,burial=map(float,parts[1:6]);elevation=float(parts[6]) if len(parts)==7 else 0
                assert all(math.isfinite(n) for n in [u,v,yaw,scale,burial,elevation])
                assert 0 <= u <= 1 and 0 <= v <= 1 and -360 <= yaw <= 360
                assert 0 <= elevation <= (3 if alias in ELEVATED else 0)
                r,f=author.local((u-.5)*100800,(v-.5)*(100800*2818/5009))
                if any(old['alias']==alias and math.hypot(old['r']-r,old['f']-f)<.01 and
                       abs(old['elevation']-elevation)<.01 for old in author.records):
                    raise ValueError(f'line{line_number}: duplicate alias/root/elevation')
                record=author.place(alias,r,f,yaw-author.base_yaw,scale,burial,comment[0].strip() if comment else f'line{line_number}')
                record['elevation']=elevation;record['root_y']+=elevation
        else: compose(author)
        assert len(author.records)<=layout['budgets']['new_records_max']
        counts={alias:sum(r['alias']==alias for r in author.records) for alias in sorted(author.bounds)}
        output=['# Silverpine Waystation — authored starting area, editable records',
                '# alias map_u map_v yaw_degrees scale burial_meters [elevation_meters]',
                '# Local layout/paths/pads: starting_area_layout.json; original authored_instances.txt is retained.',
                '# Platforms are walkable low floors; gate has measured open passage; tower is closed.',
                '# Optional elevation dresses real tabletop/stack surfaces. No point-light/fire gameplay is implied.']
        for record in author.records:
            r=record;u=r['x']/100800+.5;v=r['z']/(100800*2818/5009)+.5
            yaw=(author.base_yaw+r['heading']+180)%360-180
            output.append(f'{r["alias"]:20s} {u:.10f} {v:.10f} {yaw:9.4f} {r["scale"]:.4f} {r["burial"]:.4f} {r["elevation"]:.5f} # {r["label"]}')
        manifest=dict(schema_version=1,layout_source=args.layout.name,instances_source=args.instances.name,
            layout_sha256=hashlib.sha256(args.layout.read_bytes()).hexdigest(),
            baseline_sha256=hashlib.sha256((DIRECTORY/'authored_instances.txt').read_bytes()).hexdigest(),
            record_count=len(author.records),baseline_records=283,total_records=len(author.records)+283,counts=counts,
            near_triangles_by_model=author.triangles,
            unculled_near_triangle_instances=sum(author.triangles[r['alias']] for r in author.records),
            tree_route_clearance='Real transformed vertices below2.2m worldheight; canopies may overhang. Originaltrail still uses full-crown radius.',
            skipped_path_edge_candidates=author.skipped,route=layout['route_metrics'],actual_glb_bounds=author.bounds,
            minimum_loop_footprint_margin_m=min(author.path_clearance(r) for r in author.records),
            minimum_original_trail_margin_m=min(author.original_clearance(r) for r in author.records),
            min_ground_height_m=min(r['ground_height'] for r in author.records),max_ground_height_m=max(r['ground_height'] for r in author.records),
            validation_scope='Transformed GLB vertices/triangle-plane intersections below2.2m for trees, complete prop bounds, integrated CPU atlas; overhanging crowns allowed. Native capsule/visual tests remain independent.',
            records=author.records)
        if not args.validate_only:
            args.instances.write_text('\n'.join(output)+'\n')
            (args.instances.parent/'starting_area_manifest.json').write_text(json.dumps(manifest,indent=2,allow_nan=False)+'\n')
            write_svg(author,args.instances.parent/'starting_area_layout.svg')
        print(json.dumps({k:v for k,v in manifest.items() if k not in ['records','actual_glb_bounds']},indent=2,allow_nan=False))
        author.atlas.api.world_atlas_shutdown()


if __name__=='__main__': main()
