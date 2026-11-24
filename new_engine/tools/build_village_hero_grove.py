#!/usr/bin/env python3
"""Small, authored source-resident groves behind the hero street's roof gaps.

Only apply(author) appends in-memory records. The CLI stages proof/placements;
it never publishes scenery, changes landscape density, or creates models.
"""
from pathlib import Path
import argparse
import copy
import hashlib
import json
import math
import re
import struct
import tempfile

from build_castle_village import (Author,PATH,ROOT,MODELS,ALIASES,NON_SOLID,
    PURCHASED_BUILDINGS,ROUTE,ORIGIN,separated_bounds)
from build_village_demo_dressing import shape,overlap,route_margin,baseline_equal
from build_starting_area import Atlas

OUTPUT=ROOT/'docs/performance/market-hero/grove'
PLANTS={'fern0','fern1','fern2'}
STRUCTURES=PURCHASED_BUILDINGS|{'purchased_wall','purchased_gate','starting_wall',
    'starting_tower','starting_gate','village_keep','village_inn','village_smithy'}
# Unequal heights and root spacing make overlapping crowns instead of an even
# tree row. Inner and outer groups sit on either side of the actual205m wall.
GROVES=[
    dict(id='east-inner',trees=[(187.6,32.0,2.45,.04),(190.8,52.5,2.05,-.07),(184.7,75.5,1.75,.13)],
         plants=[(182.8,29.0),(185.4,36.5),(188.2,46.5),(193.4,57.3),(180.2,71.3),(187.2,80.8)]),
    dict(id='east-outer',trees=[(216.8,41.0,2.1,.03),(218.1,62.5,1.55,-.08),(215.8,88.0,1.95,math.pi)],
         plants=[(213.3,36.8),(219.4,47.8),(215.2,57.8),(220.7,68.8),(212.1,82.3),(218.9,93.0)]),
    dict(id='west-inner',trees=[(-188.6,33.5,2.2,math.pi+.06),(-190.1,57.0,2.45,math.pi-.06),(-185.8,79.0,1.75,-.08)],
         plants=[(-184.6,28.2),(-191.5,39.3),(-186.4,51.4),(-192.9,63.0),(-181.5,73.5),(-188.3,84.8)]),
    dict(id='west-outer',trees=[(-217.2,44.8,1.85,math.pi+.03),(-215.9,68.0,2.15,.02),(-218.0,91.0,1.55,-.06)],
         plants=[(-213.8,39.2),(-220.1,50.0),(-212.6,62.7),(-218.4,74.6),(-214.5,85.2),(-220.3,96.0)])
]
VIEWS=[('castle-hero-entry',[-80,55],[-48,55]),
       ('castle-hero-mid',[-64,55],[-43,55.4]),
       ('castle-hero-reverse',[-42,55],[-72,55.4])]

def source_routes(filename):
    """Read actual runtime four-point routes rather than a copied route list."""
    text=(ROOT/'engine'/filename).read_text()
    block=text.split('constexpr Route routes[]={',1)[1].split('\n};',1)[0]
    rows=re.findall(r'\{\{\{(.*?)\}\}\}',block)
    routes=[]
    for row in rows:
        points=[[float(x),float(z)] for x,z in re.findall(r'(-?\d+(?:\.\d*)?),(-?\d+(?:\.\d*)?)',row)]
        assert len(points)==4,(filename,row)
        routes.append(points)
    assert len(routes)>=6,(filename,'Missing runtime routes')
    return [(a,b) for points in routes for a,b in zip(points,points[1:]+points[:1])]

def corridors(author):
    human=source_routes('world_village_population.cpp');pets=source_routes('world_village_pets.cpp')
    return dict(original=list(zip(ROUTE,ROUTE[1:])),
        demo=list(zip(author.layout['demo_route_xz'],author.layout['demo_route_xz'][1:])),
        human=human,pets=pets)

def clear(author,r,existing,paths):
    for road in author.layout['roads']:
        if route_margin(author,r,road['a'],road['b'])<=road['half_width']+.37:return False
    if any(route_margin(author,r,a,b)<=.75 for group in paths.values() for a,b in group):return False
    # All present human/pet positions also fall inside this deliberately wider
    # central activity envelope. Both groves are beyond it, including foliage.
    s=shape(author,r)
    if not(s[9]<-105 or s[8]>105):return False
    for other in existing:
        if other['alias'] in STRUCTURES:
            if not separated_bounds(author,r,other,.15):return False
        elif other['alias']=='broadleaf':
            if math.hypot(r['x']-other['x'],r['z']-other['z'])<2.4:return False
        elif other['alias']not in NON_SOLID and overlap(s,shape(author,other),.025):return False
    return True

def add(author,record):
    author.add(record['alias'],record['x'],record['z'],record['yaw'],record['scale'],record['burial'],record['label'],record['elevation'])
    author.records[-1]['hero_grove']=record['hero_grove']
    return author.records[-1]

def apply(author):
    if hasattr(author,'_hero_grove_metadata'):return copy.deepcopy(author._hero_grove_metadata)
    baseline=copy.deepcopy(author.records);resident={r['alias']for r in baseline}
    assert {'broadleaf',*PLANTS}<=resident,'Groves may only reuse resident assets'
    assert not any(r['label'].startswith('hero_grove_')for r in baseline)
    paths=corridors(author);existing=list(baseline);labels=[];groups=[]
    for index,grove in enumerate(GROVES):
        group=[]
        for number,(x,z,scale,yaw) in enumerate(grove['trees']):
            r=dict(alias='broadleaf',x=x,z=z,scale=scale,yaw=yaw,burial=.025,elevation=0,
                label=f"hero_grove_{grove['id']}_tree_{number}",hero_grove=grove['id'])
            assert clear(author,r,existing,paths),('Authored grove tree does not fit',r['label'])
            record=add(author,r);existing.append(record);labels.append(r['label']);group.append(r['label'])
        for number,(x,z) in enumerate(grove['plants']):
            r=dict(alias=f'fern{(number+index)%3}',x=x,z=z,scale=.75+(number%3)*.10,
                yaw=[.2,1.3,-.7,2.4,-2.1,.6][number],burial=0,elevation=0,
                label=f"hero_grove_{grove['id']}_fern_{number}",hero_grove=grove['id'])
            assert clear(author,r,existing,paths),('Authored grove understory does not fit',r['label'])
            record=add(author,r);existing.append(record);labels.append(r['label']);group.append(r['label'])
        groups.append(dict(id=grove['id'],labels=group))
    author._hero_grove_baseline=baseline
    author._hero_grove_metadata=dict(schema_version=1,recipe_file=str(Path(__file__).relative_to(ROOT)),
        recipe_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),labels=labels,groves=groups,
        existing_records_preserved=len(baseline),tree_instances=12,understory_instances=24,
        new_model_families=0,new_image_families=0,new_texture_policies=0,global_density_changed=False)
    return copy.deepcopy(author._hero_grove_metadata)

def triangles(path):
    raw=path.read_bytes();length,kind=struct.unpack_from('<II',raw,12)
    assert kind==0x4e4f534a
    data=json.loads(raw[20:20+length])
    return sum(data['accessors'][p['indices']]['count']//3 if 'indices'in p else
        data['accessors'][p['attributes']['POSITION']]['count']//3
        for mesh in data['meshes']for p in mesh['primitives'])

def validate(author):
    meta=author._hero_grove_metadata;baseline=author._hero_grove_baseline
    assert baseline_equal(author.records[:len(baseline)],baseline),'Existing scene changed'
    records={r['label']:r for r in author.records};assert len(records)==len(author.records)
    assert len(meta['labels'])==36 and len(author.records)>=len(baseline)+36
    paths=corridors(author);minimum={name:math.inf for name in paths};eaves=0;road_checks=0
    for label in meta['labels']:
        r=records[label]
        assert r['alias']in {'broadleaf',*PLANTS} and 0<r['scale']<=2.5
        assert 180<abs(r['x'])<222 and 20<r['z']<100,'Grove escaped authored verge'
        assert r['elevation']==0 and r['burial']==(.025 if r['alias']=='broadleaf'else 0),'Grove root contact changed'
        assert clear(author,r,[v for v in author.records if v['label']!=label],paths),('Grove blocks a corridor or source volume',label)
        for name,segments in paths.items():minimum[name]=min(minimum[name],*(route_margin(author,r,a,b)for a,b in segments))
        for other in author.records:
            if other['alias']in STRUCTURES:
                assert separated_bounds(author,r,other,.15),('Grove violates source eave/wall clearance',label,other['label']);eaves+=1
        road_checks+=len(author.layout['roads'])
    composition=[]
    for name,position,target in VIEWS:
        direction=math.atan2(target[1]-position[1],target[0]-position[0]);visible=[]
        for label in meta['labels']:
            r=records[label]
            if r['alias']!='broadleaf':continue
            dx,dz=r['x']-position[0],r['z']-position[1];distance=math.hypot(dx,dz)
            bearing=math.remainder(math.atan2(dz,dx)-direction,2*math.pi)
            if abs(bearing)<math.radians(20):
                lo,hi=author.model('broadleaf',True)
                visible.append(dict(label=label,distance_m=distance,bearing_degrees=math.degrees(bearing),
                    crown_top_above_eye_degrees=math.degrees(math.atan2(hi[1]*r['scale']-1.75,distance)),
                    active_lods=[lod for lod,a,b in [('near',0,82),('mid',62,265),('far',215,1680)]if a<=distance<b]))
        assert len(visible)>=5,(name,'Insufficient grove framing')
        composition.append(dict(view=name,candidates_in_central_40_degree_view=visible,
            note='Analytical framing only; intervening architecture occlusion needs native visual review'))
    from build_castle_village import WIDTH,DEPTH
    protected_distances={}
    trees=[records[label]for label in meta['labels']if records[label]['alias']=='broadleaf']
    for source in author.layout['preserve']:
        minimum_distance=math.inf
        for line in (PATH.parent/source['file']).read_text().splitlines():
            fields=line.split()
            if not fields or fields[0].startswith('#'):continue
            x=(float(fields[1])-.487)*WIDTH;z=(float(fields[2])-.320)*DEPTH
            minimum_distance=min(minimum_distance,*(math.hypot(x-r['x'],z-r['z'])for r in trees))
        assert minimum_distance>1000,('Protected source is near new grove',source['file'],minimum_distance)
        protected_distances[source['file']]=minimum_distance
    return dict(instances=36,trees=12,understory=24,source_eave_wall_checks=eaves,road_checks=road_checks,
        minimum_full_model_distance_from_corridors_m=minimum,protected_stationary_activity_envelope_xz=[-105,-100,105,200],
        tree_triangles_per_instance={level:triangles(MODELS/f'cinematic_broadleaf_{level}.glb')for level in ['near','mid','far']},
        runtime_route_sources_sha256={name:hashlib.sha256((ROOT/'engine'/name).read_bytes()).hexdigest()
            for name in ['world_village_population.cpp','world_village_pets.cpp']},
        protected_source_minimum_root_distance_m=protected_distances,
        model_sha256={alias:hashlib.sha256((MODELS/(ALIASES[alias]+'.glb')).read_bytes()).hexdigest()for alias in ['broadleaf',*sorted(PLANTS)]},
        composition=composition,new_gpu_image_bytes=0,new_model_families=0)

def test(author):
    from build_castle_village import WIDTH,DEPTH
    result=validate(author);before=copy.deepcopy(author.records);meta=apply(author)
    assert apply(author)==meta and author.records==before
    def clone():
        duplicate=copy.copy(author);duplicate.records=copy.deepcopy(author.records);return duplicate
    def rejected(changed):
        try:validate(changed)
        except AssertionError:return
        raise AssertionError('Invalid grove negative control was accepted')
    bad=clone();blocked=next(r for r in bad.records if r['label']==meta['labels'][0]);blocked.update(x=0,z=55)
    assert not clear(bad,blocked,bad._hero_grove_baseline,corridors(bad)),'Blocked road capsule accepted'
    rejected(bad)
    bad=clone();next(r for r in bad.records if r['label']==meta['labels'][0])['elevation']=.02;rejected(bad)
    bad=clone();next(r for r in bad.records if r['label']==meta['labels'][0])['x']=205;rejected(bad)
    rounded=clone()
    for r in rounded.records:
        r['x']=(float(format(.487+r['x']/WIDTH,'.12g'))-.487)*WIDTH
        r['z']=(float(format(.320+r['z']/DEPTH,'.12g'))-.320)*DEPTH
        r['yaw']=math.radians(float(format(math.degrees(r['yaw']),'.12g')))
        for key in ['scale','burial','elevation']:r[key]=float(format(r[key],'.12g'))
    validate(rounded)
    ground_checks=0
    with tempfile.TemporaryDirectory(prefix='valwood-hero-grove-')as directory:
        atlas=Atlas(directory)
        for label in meta['labels']:
            r=next(r for r in author.records if r['label']==label)
            radius=.20*r['scale']if r['alias']=='broadleaf'else 1.2*r['scale']
            for n in range(9):
                angle=n*math.pi*.25;dx,dz=(0,0)if n==8 else(radius*math.cos(angle),radius*math.sin(angle))
                height=atlas.sample(ORIGIN[0]+r['x']+dx,ORIGIN[1]+r['z']+dz).height
                assert abs(height-24)<1e-4,('Grove root/understory terrain support',label,height);ground_checks+=1
    result.update(idempotent=True,corridor_negative_control=True,wall_volume_negative_control=True,
        root_contact_negative_control=True,serialized_instance_roundtrip=True,actual_cpu_terrain_rays=ground_checks)
    return result

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--stage',action='store_true');parser.add_argument('--test',action='store_true');args=parser.parse_args()
    assert args.stage or args.test
    # The current published manifest is an independent in-memory stage. The
    # parent scene builder can instead call apply on its final composed author.
    layout=json.loads(PATH.read_text())
    author=Author(layout);author.compose();metadata=apply(author)
    proof=test(author)if args.test else validate(author)
    if args.stage:
        OUTPUT.mkdir(parents=True,exist_ok=True)
        (OUTPUT/'grove-stage.json').write_text(json.dumps(dict(metadata=metadata,validation=proof,
            placements=[r for r in author.records if r['label']in set(metadata['labels'])]),indent=2)+'\n')
    print(json.dumps(proof,indent=2))

if __name__=='__main__':main()
