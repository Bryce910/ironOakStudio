#!/usr/bin/env python3
"""Compose the connected demo quarter after Author.apply_purchased().

Only this module's JSON and staged proof/projection are owned here. apply(author)
does not write the base scene, renderer includes, terrain masks, or placements.
All authored coordinates are east/south metres around the existing village.
"""
from pathlib import Path
import argparse
import collections
import copy
import hashlib
import html
import json
import math
import sys

ROOT=Path(__file__).resolve().parents[1]
SOURCE=ROOT/'projects/valwood_world/scenery/village_demo_district.json'
PROOF=ROOT/'docs/performance/village-demo'


def configuration():
    config=json.loads(SOURCE.read_text());assert config['schema_version']==1
    return config


def transformed_bounds(author,record):
    lo,hi=author.model(record['alias'],True);c,s=math.cos(record['yaw']),math.sin(record['yaw'])
    points=[(record['x']+(c*x+s*z)*record['scale'],record['z']+(-s*x+c*z)*record['scale'])
            for x in [lo[0],hi[0]] for z in [lo[2],hi[2]]]
    return [[min(p[k] for p in points) for k in range(2)],[max(p[k] for p in points) for k in range(2)]]


def apply(author):
    """Return exportable district metadata while mutating in-memory author data."""
    if hasattr(author,'_demo_district_metadata'):
        return copy.deepcopy(author._demo_district_metadata)
    config=configuration();records={r['label']:r for r in author.records}
    protected={label:copy.deepcopy(records[label]) for label in config['protected_frontage_labels']}
    removed=config['removed_home_labels']+config.get('removed_legacy_prop_labels',[])
    assert len(set(removed))==len(removed) and set(removed)<=records.keys()
    assert not set(removed)&protected.keys()
    author.records=[r for r in author.records if r['label'] not in set(removed)]
    streets={s['id']:s for s in config['streets']}
    for street in config['streets']:
        found=next((p for p in author.layout['roads'] if p['id']==street['road_id']),None)
        if found:
            # Preserve the original complete cross-street centreline and NPC
            # turn locations; only the demo quarter's paving width is refined.
            found.update(half_width=street['half_width'],paving=street['paving'])
        else:
            author.layout['roads'].append(dict(id=street['road_id'],a=street['a'],b=street['b'],
                                               half_width=street['half_width'],paving=street['paving']))
    for connector in config['connectors']:
        assert not any(p['id']==connector['id'] for p in author.layout['roads'])
        author.layout['roads'].append(copy.deepcopy(connector))
    added=[];districts=[]
    for slot in config['frontages']:
        street=streets[slot['street']];family=slot['family'];sign=config['native_front_x_sign'][family]
        bank=slot['bank'];axis=street['axis']
        if axis=='x':
            assert bank in ['north','south']
            yaw=math.radians(-90*sign if bank=='north' else 90*sign)
        else:
            assert bank in ['west','east']
            yaw=0 if (bank=='west')==(sign>0) else math.pi
        candidate=dict(alias='purchased_house_'+family,x=0,z=0,yaw=yaw,scale=slot['scale'])
        lo,hi=transformed_bounds(author,candidate)
        if axis=='x':
            candidate['x']=slot['station']
            edge=street['centre']+(-1 if bank=='north' else 1)*street['frontage_offset']
            candidate['z']=edge-(hi[1] if bank=='north' else lo[1])
        else:
            candidate['z']=slot['station']
            edge=street['centre']+(-1 if bank=='west' else 1)*street['frontage_offset']
            candidate['x']=edge-(hi[0] if bank=='west' else lo[0])
        author.add(candidate['alias'],candidate['x'],candidate['z'],yaw,slot['scale'],0,slot['label'])
        author.records[-1].update(demo_district=slot['street'],frontage_bank=bank)
        added.append(slot['label'])
    for street in config['streets']:
        labels=[s['label'] for s in config['frontages'] if s['street']==street['id']]
        districts.append(dict(id=street['id'],title=street['title'],road_ids=[street['road_id']],
                              house_labels=labels,prop_labels=[],category_counts=dict(houses=len(labels)),
                              frontage_clear_width_m=street['frontage_offset']*2))
    districts.append(dict(id='retained_merchants_lane',title='Merchants Lane',road_ids=['merchants_lane'],
                          house_labels=list(protected),prop_labels=[],category_counts=dict(houses=len(protected))))
    author.layout['demo_route_xz']=copy.deepcopy(config['demo_route_xz'])
    author.layout['demo_views']=copy.deepcopy(config['demo_views'])
    metadata=dict(schema_version=1,source_file=str(SOURCE.relative_to(ROOT)),source_sha256=hashlib.sha256(SOURCE.read_bytes()).hexdigest(),
                  removed_labels=removed,added_labels=added,added_homes=len(added),districts=districts,
                  reserved_pockets=config['reserved_pockets'],route_xz=config['demo_route_xz'],views=config['demo_views'],
                  retained_frontages=protected,source_model_policy='existing resident six-family purchased architecture; no asset or image additions')
    metadata['frontage_sites']=[dict(label=r['label'],alias=r['alias'],position_xz=[r['x'],r['z']],
                                    yaw_degrees=math.degrees(r['yaw']),scale=r['scale'],
                                    bounds_xz=transformed_bounds(author,r),district=r['demo_district'],bank=r['frontage_bank'])
                                for r in author.records if r['label'] in set(added)]
    metadata['retained_landmarks']={label:copy.deepcopy(records[label]) for label in ['wayfarers_inn','west_smithy']}
    author._demo_district_metadata=copy.deepcopy(metadata)
    return metadata


def clearance(author,record,a,b):
    from build_starting_area import rectangle_distance
    lo,hi=author.model(record['alias'],True);c,s=math.cos(record['yaw']),math.sin(record['yaw'])
    local=lambda p:[c*(p[0]-record['x'])-s*(p[1]-record['z']),s*(p[0]-record['x'])+c*(p[1]-record['z'])]
    return rectangle_distance(local(a),local(b),[lo[0]*record['scale'],lo[2]*record['scale']],
                              [hi[0]*record['scale'],hi[2]*record['scale']])


def same_retained_instance(actual,expected):
    """Instance-file decimal roundtrip, never a geometry clearance exemption."""
    numeric={'x','z','yaw','scale','burial','elevation'}
    return actual.keys()==expected.keys() and all(
        math.isclose(actual[k],value,rel_tol=0,abs_tol=1e-6) if k in numeric else actual[k]==value
        for k,value in expected.items())


def validate(author):
    from build_castle_village import NON_SOLID,PURCHASED_BUILDINGS,ROUTE,separated_bounds
    config=configuration();metadata=author._demo_district_metadata
    records={r['label']:r for r in author.records};assert len(records)==len(author.records)
    added=[records[label] for label in metadata['added_labels']]
    assert len(added)==42 and {r['alias'][-1] for r in added}==set('abcdef')
    assert all(same_retained_instance(records[label],r) for label,r in metadata['retained_frontages'].items()),'Existing lane changed'
    assert all(same_retained_instance(records[label],r) for label,r in metadata['retained_landmarks'].items()),'Existing workshop/inn changed'
    assert not set(metadata['removed_labels'])&records.keys()
    homes=[r for r in author.records if r['alias'].startswith(('village_house_','purchased_house_'))]
    assert len(homes)==55
    pair_checks=solid_checks=road_checks=route_checks=0;minimum_road=math.inf
    for i,a in enumerate(homes):
        for b in homes[i+1:]:
            assert separated_bounds(author,a,b),('Whole-house/eave overlap',a['label'],b['label'])
            pair_checks+=1
    for a in added:
        for b in author.records:
            if a is b or b['alias'] in NON_SOLID or b.get('parent_structure')==a['label']:continue
            assert separated_bounds(author,a,b),('New house intersects existing solid',a['label'],b['label'])
            solid_checks+=1
        for road in author.layout['roads']:
            margin=clearance(author,a,road['a'],road['b'])-road['half_width']
            assert margin>.37,('House blocks a paved road capsule',a['label'],road['id'],margin)
            minimum_road=min(minimum_road,margin);road_checks+=1
    # Reserved delivery/planting bays must be genuine free parcels rather than
    # labels drawn through the back half of a new house.
    bay_checks=0
    for pocket in config['reserved_pockets']:
        lo,hi=pocket['bounds_xz']
        for house in homes+[records['wayfarers_inn'],records['west_smithy']]:
            a,b=transformed_bounds(author,house)
            assert b[0]+.15<lo[0] or hi[0]+.15<a[0] or b[1]+.15<lo[1] or hi[1]+.15<a[1],('Reserved bay crosses building eaves',pocket['id'],house['label'])
            bay_checks+=1
    # Existing walking NPC corridors are immutable. Sweep each entire segment,
    # independent of their analytic animation timing or current camera.
    npc_routes=[([[-18,-75],[18,-75],[18,-85],[-18,-85]],.48),
                ([[-1,-42],[-1,178],[1,178],[1,-42]],.38),
                ([[2.4,55],[93,55],[93,125],[2.4,125]],.38),
                ([[-2.4,125],[-93,125],[-93,55],[-2.4,55]],.38),
                ([[-4.7,-42],[-4.7,44],[-3.3,44],[-3.3,-42]],.38),
                ([[60.95,-25],[60.95,47],[62.55,47],[62.55,-25]],.38),
                ([[17,-12],[17,32],[18.8,32],[18.8,-12]],.38)]
    minimum_npc=math.inf;minimum_original=math.inf
    for r in added:
        for route,radius in npc_routes:
            for a,b in zip(route,route[1:]+route[:1]):
                margin=clearance(author,r,a,b)-radius
                assert margin>.15,('New house obstructs existing NPC corridor',r['label'],a,b,margin)
                minimum_npc=min(minimum_npc,margin);route_checks+=1
        for a,b in zip(ROUTE,ROUTE[1:]):
            margin=clearance(author,r,a,b)-.37
            assert margin>.15,('New house obstructs original 806m route',r['label'],margin)
            minimum_original=min(minimum_original,margin);route_checks+=1
    # Every paired street has a measured7m eave corridor and joins the connected
    # road graph. The short lane connector closes the old0.1m paving gap.
    from build_starting_area import segments_distance
    roads=author.layout['roads'];connected={'crown_avenue'}
    for connector in config['connectors']:
        actual=next((r for r in roads if r['id']==connector['id']),None)
        assert actual and all(actual[k]==connector[k] for k in ['a','b','half_width','paving']),('Missing/moved lane connector',connector['id'])
    for slot in config['frontages']:
        street=next(s for s in config['streets'] if s['id']==slot['street']);r=records[slot['label']]
        lo,hi=transformed_bounds(author,r);axis=1 if street['axis']=='x' else 0
        edge=hi[axis] if slot['bank'] in ['north','west'] else lo[axis]
        expected=street['centre']+(-1 if slot['bank'] in ['north','west'] else 1)*street['frontage_offset']
        assert abs(edge-expected)<1e-7,('Authored street frontage changed',slot['label'],edge,expected)
    while True:
        previous=len(connected)
        for a in roads:
            if a['id'] in connected:continue
            if any(b['id'] in connected and segments_distance(a['a'],a['b'],b['a'],b['b'])<
                   a['half_width']+b['half_width']-.15 for b in roads):connected.add(a['id'])
        if len(connected)==previous:break
    assert all(s['road_id'] in connected for s in config['streets'])
    assert all(s['id'] in connected for s in config['connectors'])
    return dict(homes=len(homes),added_homes=len(added),removed_sparse_homes=len(config['removed_home_labels']),
                retained_lane_homes=11,new_streets=3,whole_house_eave_pair_checks=pair_checks,
                solid_footprint_checks=solid_checks,road_capsule_checks=road_checks,
                old_route_and_population_corridor_checks=route_checks,
                reserved_delivery_bay_building_checks=bay_checks,
                minimum_house_road_margin_m=minimum_road,minimum_house_npc_margin_m=minimum_npc,
                minimum_house_original_route_margin_m=minimum_original,
                house_alias_counts=dict(sorted(collections.Counter(r['alias'] for r in homes).items())),
                added_resident_images=0,scene_records=len(author.records),demo_route_length_m=sum(math.dist(a,b) for a,b in zip(config['demo_route_xz'],config['demo_route_xz'][1:])))


def projection(author,path,overview=False):
    config=configuration();added=set(author._demo_district_metadata['added_labels'])
    view='-220 -210 440 440' if overview else '-120 -45 240 240'
    svg=[f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="{view}">',
         '<rect x="-220" y="-210" width="440" height="440" fill="#445544"/>']
    for square in author.layout['squares']:
        x,z=square['center'];w,d=square['half']
        svg.append(f'<rect x="{x-w}" y="{z-d}" width="{w*2}" height="{d*2}" fill="#8c8774"/>')
    for road in author.layout['roads']:
        a,b=road['a'],road['b'];svg.append(f'<path d="M{a[0]} {a[1]}L{b[0]} {b[1]}" fill="none" stroke="#b8aa8d" stroke-width="{road["half_width"]*2}"/>')
    for r in author.records:
        if not r['alias'].startswith(('purchased_house_','village_house_','village_inn','village_smithy','village_keep','purchased_shop_','starting_wall','purchased_wall','starting_gate')):continue
        lo,hi=author.model(r['alias'],True);color='#daab60' if r['label'] in added else '#8f9da5'
        svg.append(f'<g transform="translate({r["x"]} {r["z"]}) rotate({-math.degrees(r["yaw"])})"><rect x="{lo[0]*r["scale"]}" y="{lo[2]*r["scale"]}" width="{(hi[0]-lo[0])*r["scale"]}" height="{(hi[2]-lo[2])*r["scale"]}" fill="{color}" stroke="#252c2c" stroke-width=".3"><title>{html.escape(r["label"])}</title></rect></g>')
    for pocket in config['reserved_pockets']:
        (x,z),(xx,zz)=pocket['bounds_xz'];svg.append(f'<rect x="{x}" y="{z}" width="{xx-x}" height="{zz-z}" fill="none" stroke="#8bca9c" stroke-width=".5"><title>{html.escape(pocket["id"])}</title></rect>')
    route=config['demo_route_xz'];svg.append('<path d="'+' '.join(('M' if i==0 else 'L')+f'{p[0]} {p[1]}' for i,p in enumerate(route))+'" fill="none" stroke="#d55d50" stroke-width=".5"/>')
    svg.append('</svg>');path.write_text('\n'.join(svg)+'\n')


def projection_png(author,path,overview=False):
    """Simple orthographic footprint drawing; no native renderer/GPU needed."""
    from PIL import Image,ImageDraw
    config=configuration();added=set(author._demo_district_metadata['added_labels'])
    xmin,zmin,size=(-220,-210,440) if overview else (-120,-45,240)
    image=Image.new('RGB',(1200,1200),'#445544');draw=ImageDraw.Draw(image)
    zoom=1200/size
    p=lambda x,z:((x-xmin)*zoom,(z-zmin)*zoom)
    for square in author.layout['squares']:
        x,z=square['center'];w,d=square['half'];draw.rectangle([p(x-w,z-d),p(x+w,z+d)],fill='#8c8774')
    for road in author.layout['roads']:
        radius=road['half_width']*zoom;a,b=road['a'],road['b']
        draw.line([p(*a),p(*b)],fill='#b8aa8d',width=max(1,round(radius*2)))
        for point in [a,b]:
            x,z=p(*point);draw.ellipse([x-radius,z-radius,x+radius,z+radius],fill='#b8aa8d')
    for r in author.records:
        if not r['alias'].startswith(('purchased_house_','village_house_','village_inn','village_smithy','village_keep','purchased_shop_','starting_wall','purchased_wall','starting_gate')):continue
        lo,hi=author.model(r['alias'],True);c,s=math.cos(r['yaw']),math.sin(r['yaw'])
        points=[p(r['x']+(c*x+s*z)*r['scale'],r['z']+(-s*x+c*z)*r['scale'])
                for x,z in [(lo[0],lo[2]),(hi[0],lo[2]),(hi[0],hi[2]),(lo[0],hi[2])]]
        draw.polygon(points,fill='#daab60' if r['label'] in added else '#8f9da5',outline='#252c2c',width=2)
        if r['label'] in added:draw.text(p(r['x'],r['z']),r['alias'][-1].upper(),fill='#332818',anchor='mm')
    for pocket in config['reserved_pockets']:
        lo,hi=pocket['bounds_xz'];draw.rectangle([p(*lo),p(*hi)],outline='#8bca9c',width=2)
    draw.line([p(*point) for point in config['demo_route_xz']],fill='#d55d50',width=3)
    draw.rectangle([10,10,480,57],fill='#243126')
    draw.text((20,18),'VALDOA DEMO | measured model footprints',fill='#eee2cb')
    draw.text((20,35),'Gold: new facades   Gray: retained   Green: service bays',fill='#d0c5aa')
    image.save(path)


def main():
    parser=argparse.ArgumentParser();parser.add_argument('--stage',action='store_true');parser.add_argument('--validate',action='store_true')
    parser.add_argument('--with-dressing',action='store_true',help='also compose the separately owned dressing module')
    args=parser.parse_args();assert args.stage or args.validate
    from build_castle_village import Author,PATH
    layout=copy.deepcopy(json.loads(PATH.read_text()))
    if not args.with_dressing:layout.pop('demo_dressing',None)
    author=Author(layout);author.compose();metadata=apply(author)
    proof=validate(author);PROOF.mkdir(exist_ok=True,parents=True)
    if args.stage:
        projection(author,PROOF/'district-plan.svg')
        projection(author,PROOF/'district-overview.svg',True)
        projection_png(author,PROOF/'district-plan.png')
        projection_png(author,PROOF/'district-overview.png',True)
        (PROOF/'district-stage.json').write_text(json.dumps(dict(metadata=metadata,validation=proof,
            roads=author.layout['roads'],squares=author.layout['squares'],placements=author.records),indent=2)+'\n')
    print(json.dumps(proof,indent=2))


if __name__=='__main__':main()
