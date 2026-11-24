#!/usr/bin/env python3
"""Source-resident doorstep/work clusters on the demo's outer paving shoulders.

The full six-metre paving is a visual street surface. New props leave its
central three metres clear, plus the real NPC and original benchmark corridors.
All purchased architectural eave envelopes still retain a strict15cm gap.
apply(author) appends a separate layer after district/dressing/bunting; no other
scene recipe, model, renderer, geometry, or metadata is mutated.
"""
from pathlib import Path
import argparse
import collections
import copy
import hashlib
import json
import math
import tempfile

from build_castle_village import (Author,PATH,MODELS,ALIASES,NON_SOLID,PURCHASED_BUILDINGS,
                                  ROUTE,ORIGIN,separated_bounds)
from build_village_demo_district import configuration,same_retained_instance
from build_village_demo_dressing import (LEGACY,base_y,shape,overlap,obstacle_shapes,
                                         route_margin,support)

ROOT=Path(__file__).resolve().parents[1]
PROOF=ROOT/'docs/performance/village-demo'
SHOULDER_ROADS={'cross_55','demo_southgate_lane','demo_coopers_return'}
STORY_NAMES=['brewer','delivery','firewood','household','cooper','herbs']
STORIES=[
 [('purchased_barrel',.85,.13),('purchased_prop_barrel_small',1.05,-.12),('purchased_goods_tankard',.78,0,1)],
 [('purchased_crate',.72,.04),('purchased_prop_sack',.90,-.18),('starting_wildflower',1.25,.21)],
 [('purchased_prop_logs',.65,0),('purchased_prop_sack',.92,.13),('fern0',.50,-.10)],
 [('purchased_prop_barrel_small',1.0,-.13),('starting_bucket',1.0,.15),('starting_celandine',1.3,-.20)],
 [('purchased_crate',.75,-.07),('purchased_prop_barrel_small',1.0,.17),('purchased_goods_tankard',.78,.14,0)],
 [('fern1',.45,-.12),('purchased_prop_sack',.92,.12),('starting_bucket',1.0,-.17)],
]
# Deliberately bring the first visible work groups around the camera-side
# corners rather than leaving all three objects behind an inspection camera.
SHOWCASE={
 'demo_home_market_row_north_0':(-75,0),
 'demo_home_market_row_south_0':(-74.5,1),
 'demo_home_southgate_lane_west_1':(109.5,0),
 'demo_home_southgate_lane_east_2':(110.5,2),
 'demo_home_southgate_lane_west_3':(166,5),
 'demo_home_southgate_lane_east_4':(166.5,4),
}


def crowd_segments():
    polygons=[[[ -18,-75],[18,-75],[18,-85],[-18,-85]],
              [[-1,-42],[-1,178],[1,178],[1,-42]],
              [[2.4,55],[93,55],[93,125],[2.4,125]],
              [[-2.4,125],[-93,125],[-93,55],[-2.4,55]],
              [[-4.7,-42],[-4.7,44],[-3.3,44],[-3.3,-42]],
              [[60.95,-25],[60.95,47],[62.55,47],[62.55,-25]],
              [[17,-12],[17,32],[18.8,32],[18.8,-12]],
              [[-36.8,68],[-36.8,115],[-35.2,115],[-35.2,68]],
              [[25,150.2],[65,150.2],[65,151.8],[25,151.8]]]
    return [(a,b) for p in polygons for a,b in zip(p,p[1:]+p[:1])]


def route_clear(author,r):
    for road in author.layout['roads']:
        half=1.5 if road['id'] in SHOULDER_ROADS else road['half_width']
        if route_margin(author,r,road['a'],road['b'])<=half+.37:return False
    if any(route_margin(author,r,a,b)<.75 for a,b in zip(ROUTE,ROUTE[1:])):return False
    return not any(route_margin(author,r,a,b)<.72 for a,b in crowd_segments())


def clear(author,r,obstacles,exclude=()):
    if not route_clear(author,r):return False
    bounds=shape(author,r)
    for label,other,record in obstacles:
        if label in exclude:continue
        # Architectural envelope separation is mandatory even for a decorative
        # plant or a product above ground. No under-awning/under-eave exception.
        if record['alias'] in PURCHASED_BUILDINGS and not separated_bounds(author,r,record,.17):return False
        if overlap(bounds,other,.025):return False
    return True


def add_record(author,r):
    author.add(r['alias'],r['x'],r['z'],r['yaw'],r['scale'],r['burial'],r['label'],r['elevation'])
    author.records[-1].update({k:v for k,v in r.items() if k not in author.records[-1]})
    return author.records[-1]


def ground_candidate(author,slot,street,spec,label,station,normal):
    alias,scale,jitter=spec[:3];yaw=(math.pi/2 if street['axis']=='x' else 0)+jitter
    lo,_=author.model(alias,True)
    sign=-1 if slot['bank'] in ['north','west'] else 1
    if street['axis']=='x':x,z=station,street['centre']+sign*normal
    else:x,z=street['centre']+sign*normal,station
    return dict(alias=alias,x=x,z=z,yaw=yaw,scale=scale,burial=0,
                elevation=-lo[1]*scale+(.05 if alias in LEGACY else 0),label=label,
                frontage_owner=slot['label'])


def apply(author):
    if hasattr(author,'_demo_frontage_metadata'):return copy.deepcopy(author._demo_frontage_metadata)
    config=configuration();streets={s['id']:s for s in config['streets']}
    baseline=copy.deepcopy(author.records);resident={r['alias'] for r in baseline}
    assert not any(r['label'].startswith('demo_frontage_') for r in baseline)
    assert all(s[0] in resident for story in STORIES for s in story),'Frontages may only reuse resident models'
    obstacles=obstacle_shapes(author);labels=[];homes=[];supports=[];ground=[]
    for index,slot in enumerate(config['frontages']):
        street=streets[slot['street']];preferred,story_index=SHOWCASE.get(slot['label'],(slot['station'],index%len(STORIES)))
        story=STORIES[story_index];created=[];home_labels=[];adjustment=0
        for part,spec in enumerate(story):
            label=f'demo_frontage_{index:02d}_{part}'
            if len(spec)==4:
                parent=created[spec[3]];_,hi=author.model(parent['alias'],True)
                product=dict(alias=spec[0],scale=spec[1],yaw_degrees=math.degrees(spec[2]),label=label)
                r,proof=support(author,product,parent,[0,0],[hi[1]-.12,hi[1]+.025])
                r['frontage_owner']=slot['label']
                assert clear(author,r,obstacles,[parent['label']]),('Unsupported frontage product clearance',label)
                supports.append(proof)
            else:
                wanted=preferred+[-1.35,1.3,2.2][part]
                candidates=[(wanted+d,n) for d in [0,.25,-.25,.5,-.5,.8,-.8,1.2,-1.2,1.6,-1.6,2,-2]
                            for n in [2.6,2.45,2.7,2.55,2.8]]
                # A few tight junctions may use a measured corner of the same
                # property. No global random or unbounded free-space search.
                owner=next(r for r in baseline if r['label']==slot['label']);lo,hi=author.model(owner['alias'],True)
                c,s=math.cos(owner['yaw']),math.sin(owner['yaw'])
                along=(abs(c)*(hi[0]-lo[0])+abs(s)*(hi[2]-lo[2]))*.5*owner['scale'] if street['axis']=='x' else (abs(s)*(hi[0]-lo[0])+abs(c)*(hi[2]-lo[2]))*.5*owner['scale']
                candidates += [(slot['station']+side*(along+.8),n) for side in [-1,1] for n in [2.6,2.45,2.8]]
                for station,normal in candidates:
                    r=ground_candidate(author,slot,street,spec,label,station,normal)
                    if clear(author,r,obstacles):break
                else:raise AssertionError(('No conservative doorstep slot',slot['label'],label))
                adjustment=max(adjustment,abs(station-wanted));ground.append(label)
            r=add_record(author,r);created.append(r);home_labels.append(label);labels.append(label)
            obstacles.append((label,shape(author,r),r))
        homes.append(dict(owner=slot['label'],street=slot['street'],story=STORY_NAMES[story_index],labels=home_labels,
                          preferred_station_m=preferred,maximum_along_front_adjustment_m=adjustment))
    assert author.records[:len(baseline)]==baseline
    author._demo_frontage_baseline=baseline
    author._demo_frontage_metadata=dict(schema_version=1,recipe_file=str(Path(__file__).relative_to(ROOT)),
        recipe_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),labels=labels,homes=homes,
        ground_labels=ground,surface_supports=supports,existing_records_preserved=len(baseline),
        preserved_dressing_labels=list(author.demo_dressing['labels']),
        central_street_half_width_m=1.5,pedestrian_edge_margin_m=.37,npc_segment_clearance_m=.72,
        original_route_clearance_m=.75,minimum_purchased_eave_gap_m=.15,
        new_model_families=0,new_texture_policies=0)
    return copy.deepcopy(author._demo_frontage_metadata)


def validate(author):
    meta=author._demo_frontage_metadata;records={r['label']:r for r in author.records}
    assert len(records)==len(author.records) and len(meta['labels'])==126
    assert len(meta['homes'])==42 and all(len(h['labels'])==3 for h in meta['homes'])
    assert len(author.records)>=len(author._demo_frontage_baseline)+126
    assert all(same_retained_instance(a,b) for a,b in zip(author.records,author._demo_frontage_baseline)),'Existing frontage baseline changed'
    assert set(meta['preserved_dressing_labels'])==set(author.demo_dressing['labels']) and len(meta['preserved_dressing_labels'])==479
    ground=set(meta['ground_labels']);proofs={p['label']:p for p in meta['surface_supports']}
    obstacles=obstacle_shapes(author)
    # Add visually solid products/plants too, so a new volume overlap cannot
    # hide merely because the native collision category is nonblocking.
    present={label for label,_,_ in obstacles}
    obstacles += [(label,shape(author,records[label]),records[label]) for label in meta['labels'] if label not in present]
    comparisons=0;minimum_centre=math.inf;minimum_npc=math.inf;minimum_old=math.inf;eaves=0
    for label in meta['labels']:
        r=records[label];lo,_=author.model(r['alias'],True)
        assert route_clear(author,r),('Frontage blocks a protected walking corridor',label)
        if label in ground:
            assert abs(base_y(r)+lo[1]*r['scale'])<1e-6,('Frontage ground contact',label)
        else:
            proof=proofs[label];parent=records[proof['parent']]
            target=base_y(parent)+max(proof['actual_surface_samples_m'])*parent['scale']+.002
            assert abs(base_y(r)+lo[1]*r['scale']-target)<1e-6,('Frontage supported contact',label)
            assert len(proof['actual_surface_samples_m'])==5 and max(proof['actual_surface_samples_m'])-min(proof['actual_surface_samples_m'])<.025
        for other,b,record in obstacles:
            if other==label or other==r.get('surface_parent'):continue
            if record['alias'] in PURCHASED_BUILDINGS:
                assert separated_bounds(author,r,record,.15),('Frontage violates whole purchased eave gap',label,other)
                eaves+=1
            # The supporting parent may meet its own measured product above.
            if record.get('surface_parent')==label:continue
            assert not overlap(shape(author,r),b,.018),('Frontage prop intersects scene',label,other)
            comparisons+=1
        for road in author.layout['roads']:
            if road['id'] in SHOULDER_ROADS:minimum_centre=min(minimum_centre,route_margin(author,r,road['a'],road['b']))
        minimum_npc=min(minimum_npc,*(route_margin(author,r,a,b) for a,b in crowd_segments()))
        minimum_old=min(minimum_old,*(route_margin(author,r,a,b) for a,b in zip(ROUTE,ROUTE[1:])))
    return dict(instances=126,finished_frontages=42,ground_objects=len(ground),
                measured_supported_products=len(proofs),real_product_support_rays=len(proofs)*5,
                volume_comparisons=comparisons,purchased_whole_eave_checks=eaves,
                minimum_distance_from_new_street_centrelines_m=minimum_centre,
                minimum_distance_from_actual_npc_segments_m=minimum_npc,
                minimum_distance_from_original_benchmark_m=minimum_old,
                preserved_dressing_objects=479,preserved_bunting_objects=96,new_model_families=0,new_texture_policies=0,
                aliases=dict(sorted(collections.Counter(records[label]['alias'] for label in meta['labels']).items())))


def test(author):
    from build_castle_village import WIDTH,DEPTH
    result=validate(author);meta=apply(author);before=copy.deepcopy(author.records)
    assert apply(author)==meta and author.records==before
    def clone():
        a=copy.copy(author);a.records=copy.deepcopy(author.records)
        return a
    rounded=clone()
    for r in rounded.records:
        r['x']=(float(format(.487+r['x']/WIDTH,'.12g'))-.487)*WIDTH
        r['z']=(float(format(.320+r['z']/DEPTH,'.12g'))-.320)*DEPTH
        r['yaw']=math.radians(float(format(math.degrees(r['yaw']),'.12g')))
        for key in ['scale','burial','elevation']:r[key]=float(format(r[key],'.12g'))
    validate(rounded)
    def rejects(a,text):
        try:validate(a)
        except AssertionError as e:assert text in str(e),(text,str(e))
        else:raise AssertionError('Negative control passed: '+text)
    blocked=clone();next(r for r in blocked.records if r['label']==meta['ground_labels'][0])['z']=55
    rejects(blocked,'Frontage blocks a protected walking corridor')
    floating=clone();next(r for r in floating.records if r['label']==meta['ground_labels'][0])['elevation']+=.02
    rejects(floating,'Frontage ground contact')
    collision=clone();a=next(r for r in collision.records if r['label']==meta['ground_labels'][0]);b=next(r for r in collision.records if r['label']==meta['ground_labels'][1]);b.update(x=a['x'],z=a['z'])
    rejects(collision,'Frontage prop intersects scene')
    bad_support=clone();next(r for r in bad_support.records if r['label']==meta['surface_supports'][0]['label'])['elevation']+=.03
    rejects(bad_support,'Frontage supported contact')
    # The ground proof uses the same CPU atlas as the final native world,
    # sampling centre+four actual footprint corners at the24m village terrace.
    from build_starting_area import Atlas
    ground_rays=0
    with tempfile.TemporaryDirectory(prefix='valwood-frontage-ground-') as directory:
        atlas=Atlas(directory)
        for label in meta['ground_labels']:
            r=next(r for r in author.records if r['label']==label);lo,hi=author.model(r['alias'],True)
            c,s=math.cos(r['yaw']),math.sin(r['yaw'])
            heights=[]
            for x,z in [(0,0),(lo[0],lo[2]),(lo[0],hi[2]),(hi[0],lo[2]),(hi[0],hi[2])]:
                xx=r['x']+(c*x+s*z)*r['scale'];zz=r['z']+(-s*x+c*z)*r['scale']
                heights.append(atlas.sample(ORIGIN[0]+xx,ORIGIN[1]+zz).height);ground_rays+=1
            assert max(abs(y-24) for y in heights)<1e-4,('Nonlevel frontage ground',label,heights)
    result.update(idempotent=True,serialized_instance_roundtrip=True,low_corridor_negative_control=True,
                  volume_overlap_negative_control=True,ground_contact_negative_control=True,
                  real_support_contact_negative_control=True,actual_cpu_terrain_rays=ground_rays)
    return result


def main():
    parser=argparse.ArgumentParser();parser.add_argument('--stage',action='store_true');parser.add_argument('--test',action='store_true')
    args=parser.parse_args();assert args.stage or args.test
    author=Author(json.loads(PATH.read_text()));author.compose();metadata=apply(author)
    proof=test(author) if args.test else validate(author)
    if args.stage:
        PROOF.mkdir(parents=True,exist_ok=True)
        (PROOF/'frontages-stage.json').write_text(json.dumps(dict(metadata=metadata,validation=proof,
            placements=[r for r in author.records if r['label'] in set(metadata['labels'])]),indent=2)+'\n')
    print(json.dumps(proof,indent=2))


if __name__=='__main__':main()
