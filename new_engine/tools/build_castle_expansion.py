#!/usr/bin/env python3
"""Append-only authored training court, Defender monument and armory terrace.

The six new mesh families share exact already-resident image/sampler policies.
This layer never rewrites old structures, routes, resident assignments or pets.
CLI stages proof only; apply(author) is the scene builder integration contract.
"""
from pathlib import Path
import argparse,copy,hashlib,json,math,tempfile
from build_castle_village import Author,PATH,MODELS,ALIASES,NON_SOLID,ORIGIN,ROUTE,separated_bounds
from build_village_demo_dressing import base_y,shape,overlap,route_margin,baseline_equal,obstacle_shapes
from build_village_hero_grove import source_routes
from build_starting_area import Atlas
from build_castle_expansion_assets import FAMILIES,validate as validate_assets
ROOT=Path(__file__).resolve().parents[1]
OUTPUT=ROOT/'docs/performance/castle-expansion'
ASSET_ALIASES={name:name for name in FAMILIES}
ALIASES.update(ASSET_ALIASES)
# The drill animation owner measures complete source weapon sweeps. These
# centers are shared unchanged with world_castle_training, not NPC routes.
TRAINING_STATIONS=[dict(id='pair_a_west',position_xz=[-51,-73],reserved_radius_m=2.5),
 dict(id='pair_a_east',position_xz=[-47,-73],reserved_radius_m=2.5),
 dict(id='pair_b_west',position_xz=[-51,-84],reserved_radius_m=2.5),
 dict(id='pair_b_east',position_xz=[-47,-84],reserved_radius_m=2.5),
 dict(id='dummy_a',position_xz=[-55,-72],reserved_radius_m=2.5),
 dict(id='dummy_b',position_xz=[-55,-83],reserved_radius_m=2.5),
 dict(id='instructor',position_xz=[-42.5,-68],reserved_radius_m=.8)]
DUMMY_CONTACTS=[dict(dummy_label='castle_expansion_dummy_a',trainee_station_xz=[-55,-72],dummy_xz=[-56.6,-72]),
                dict(dummy_label='castle_expansion_dummy_b',trainee_station_xz=[-55,-83],dummy_xz=[-56.6,-83])]
SQUARES=[dict(id='castle_training_court',center=[-51,-78],half=[12,13.6],paving=.74),
         dict(id='castle_defender_court',center=[47,-78],half=[11.5,12.5],paving=.83),
         dict(id='castle_armory_terrace',center=[-48,-123],half=[10,10],paving=.81)]
ROADS=[dict(id='castle_training_walk',a=[-27,-66],b=[-38,-66],half_width=1.5,paving=.76),
       dict(id='castle_monument_north_walk',a=[27,-60.5],b=[39,-60.5],half_width=1.6,paving=.83),
       dict(id='castle_monument_walk',a=[39,-60.5],b=[39,-69],half_width=1.6,paving=.83)]
# Start at the dry exterior end of the OUTER town drawbridge. The avenue
# section reuses the continuous, source-clear retained district return path;
# the compact inner-court loop remains unchanged after arriving from town.
COURT_WALK=[[0,-30],[0,-66],[-36,-66],[-27,-66],[-27,-100],[-33,-112],[-27,-100],[-27,-66],[0,-66],
      [27,-60.5],[39,-60.5],[39,-69],[37,-69],[39,-69],[39,-60.5],[27,-60.5],[0,-66],[0,-30]]
OUTER_APPROACH=[[0,225],[0,220],[0,125],[0,55]]
WALK=OUTER_APPROACH+COURT_WALK+list(reversed(OUTER_APPROACH))
VIEWS=[dict(id='castle-training',position_xz=[-36,-66],target_xz=[-55,-79],pitch=.035),
       dict(id='castle-statue',position_xz=[35.8,-80.2],target_xz=[47,-78],pitch=.16),
       dict(id='castle-armory',position_xz=[-33,-112],target_xz=[-49,-123],pitch=.08)]


def spec(alias,x,z,yaw=0,scale=1,id=''):
    return dict(alias=alias,x=x,z=z,yaw=yaw,scale=scale,burial=0,elevation=0,label='castle_expansion_'+id)


def authored_specs():
    r=[spec('castle_monument',47,-78,-math.pi/2,id='defender_monument'),
       spec('castle_training_dummy',-56.6,-72,math.pi/2,id='dummy_a'),
       spec('castle_training_dummy',-56.6,-83,math.pi/2,id='dummy_b'),
       spec('castle_archery_target',-61.5,-72,math.pi/2,id='target_a'),
       spec('castle_archery_target',-61.5,-83,math.pi/2,id='target_b'),
       spec('castle_weapon_rack',-60.6,-88.8,math.pi/2,id='yard_rack'),
       spec('castle_yard_bench',-40.3,-77.9,-math.pi/2,id='spectator_a'),
       spec('castle_yard_bench',-40.3,-85.2,-math.pi/2,id='spectator_b'),
       spec('castle_yard_bench',54,-74,-math.pi/2,id='monument_bench_a'),
       spec('castle_yard_bench',54,-84,-math.pi/2,id='monument_bench_b'),
       spec('purchased_shop_d',-49,-123,math.pi,id='armory_canopy'),
       spec('castle_weapon_rack',-44.2,-122.6,math.pi/2,id='armory_rack_a'),
       spec('castle_weapon_rack',-54,-122.6,-math.pi/2,id='armory_rack_b')]
    # A low open-corner rail defines the yard and safe spectator edge. Entrances
    # are visibly wider than a capsule, no fence across the old court avenue.
    for i,x in enumerate([-59.2,-57.6,-56,-54.4,-52.8,-51.2,-49.6,-48,-46.4]):
        r.append(spec('castle_training_fence',x,-65.4,id=f'yard_north_rail_{i}'))
    for i,z in enumerate([-71.0,-72.6,-88.4,-90.0]):
        r.append(spec('castle_training_fence',-40.3,z,math.pi/2,id=f'yard_spectator_rail_{i}'))
    for i,x in enumerate([42.2,43.8,45.4,47,48.6,50.2,51.8,53.4,55]):
        r.append(spec('castle_training_fence',x,-66.4,id=f'monument_north_rail_{i}'))
    for i,z in enumerate([-68,-69.6,-71.2,-72.8,-74.4,-76,-77.6,-79.2,-80.8,-82.4,-84,-85.6,-87.2]):
        r.append(spec('castle_training_fence',57,z,math.pi/2,id=f'monument_east_rail_{i}'))
    # Retained purchased kit supplies color/material continuity and lived-in
    # armory stores: ordered, unequal stacks instead of new texture families.
    for alias,x,z,yaw,scale,id in[
        ('village_banner',-61,-67.9,math.pi/2,.88,'yard_banner_west'),
        ('village_banner',-43,-65.5,math.pi/2,.88,'yard_banner_entry'),
        ('village_banner',42,-69,math.pi/2,.95,'monument_banner_west'),
        ('village_banner',53,-69,math.pi/2,.95,'monument_banner_east'),
        ('purchased_prop_lamp',-41,-74,0,.88,'yard_lamp'),
        ('purchased_prop_lamp',40.5,-72,0,1,'monument_lamp_west'),
        ('purchased_prop_lamp',54,-88,0,1,'monument_lamp_east'),
        ('purchased_barrel',-54.5,-118.7,.2,.95,'armory_barrel_a'),
        ('purchased_barrel',-55.4,-119.4,-.2,.80,'armory_barrel_b'),
        ('purchased_crate',-43,-118.5,.07,1.10,'armory_crate_a'),
        ('purchased_crate',-44.1,-118.4,-.05,.85,'armory_crate_b'),
        ('purchased_prop_sack',-45.2,-118.2,.6,.85,'armory_sack'),
        ('purchased_prop_logs',-54,-127.5,0,.9,'armory_logs'),
        ('purchased_prop_lamp',-43.3,-125.1,0,.95,'armory_lamp'),
        ('village_banner',-45,-127.4,math.pi/2,.90,'armory_banner')]:
        r.append(spec(alias,x,z,yaw,scale,id))
    return r


def corridors(author):
    return dict(original=list(zip(ROUTE,ROUTE[1:])),
        demo=list(zip(author.layout['demo_route_xz'],author.layout['demo_route_xz'][1:])),
        human=source_routes('world_village_population.cpp'),pets=source_routes('world_village_pets.cpp'))


def clear(author,r,existing):
    for road in author.layout['roads']:
        if route_margin(author,r,road['a'],road['b'])<=road['half_width']+.37:return False
    s=shape(author,r)
    for label,b,other in existing:
        if overlap(s,b,.018):return False
    return True


def apply(author):
    if hasattr(author,'_castle_expansion_metadata'):return copy.deepcopy(author._castle_expansion_metadata)
    baseline=copy.deepcopy(author.records);squares=copy.deepcopy(author.layout['squares'])
    roads=copy.deepcopy(author.layout['roads'])
    assert not({r['id']for r in ROADS}&{r['id']for r in roads})
    author.layout['roads']+=copy.deepcopy(ROADS)
    assert not any(r['label'].startswith('castle_expansion_')for r in baseline)
    assert all((MODELS/(name+'.glb')).exists()for name in FAMILIES),'Cook castle expansion assets first'
    rows=obstacle_shapes(author);present={label for label,_,_ in rows}
    rows.extend((r['label'],shape(author,r),r)for r in baseline if r['label']not in present and r['alias']!='starting_gate')
    records=authored_specs()
    for r in records:
        # Fence segments share a terminal post at exact endpoints. The joined
        # visual rail is one fence; it is not a clearance exception for props.
        connected=r['alias']=='castle_training_fence'
        filtered=[row for row in rows if not(connected and row[2]['alias']=='castle_training_fence')]
        assert clear(author,r,filtered),('Authored expansion prop does not fit',r['label'])
        author.add(r['alias'],r['x'],r['z'],r['yaw'],r['scale'],0,r['label'],0)
        rows.append((r['label'],shape(author,r),r))
    author.layout['squares']+=copy.deepcopy(SQUARES)
    meta=dict(schema_version=1,title='Valdoa Defender court, training yard and armory terrace',
        recipe_file=str(Path(__file__).relative_to(ROOT)),recipe_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        asset_manifest='assets/purchased/castle_expansion/castle_expansion_asset_manifest.json',
        labels=[r['label']for r in records],existing_records_preserved=len(baseline),
        baseline_records_sha256=hashlib.sha256(json.dumps(baseline,sort_keys=True,separators=(',',':')).encode()).hexdigest(),
        models=ASSET_ALIASES,new_model_families=len(FAMILIES),new_gpu_image_bytes=0,
        statue_label='castle_expansion_defender_monument',training_stations=TRAINING_STATIONS,
        dummy_contacts=DUMMY_CONTACTS,ground_squares=SQUARES,ground_roads=ROADS,route_xz=WALK,views=VIEWS,
        court_bounds_xz=[-63,-132,58,-64],bridge_owned_by='build_castle_creek.py')
    author._castle_expansion_baseline=baseline;author._castle_expansion_squares=squares;author._castle_expansion_roads=roads
    author._castle_expansion_metadata=meta
    return copy.deepcopy(meta)


def validate(author):
    meta=author._castle_expansion_metadata;base=author._castle_expansion_baseline
    assert baseline_equal(author.records[:len(base)],base),'Existing scene prefix changed'
    old_roads=author._castle_expansion_roads
    assert author.layout['roads'][:len(old_roads)]==old_roads and author.layout['roads'][len(old_roads):len(old_roads)+len(ROADS)]==ROADS
    assert len({road['id']for road in author.layout['roads']})==len(author.layout['roads'])
    assert meta['route_xz']==WALK and WALK[0]==WALK[-1]
    assert len(WALK)==26 and len(WALK)<=64 and WALK[0]==[0,225]
    assert WALK[4:4+len(COURT_WALK)]==COURT_WALK
    bridges=[r for r in author.records if r['alias']=='castle_drawbridge']
    assert len(bridges)==1 and abs(bridges[0]['x'])<1e-6 and abs(bridges[0]['z']-225)<1e-6, 'Exterior bridge placement contract not integrated'
    from build_starting_area import segment_distance
    for point in [[0,219],[0,205],[0,55],[-36,-66],[-33,-112],[37,-69]]:
        assert min(segment_distance(point,a,b)for a,b in zip(WALK,WALK[1:]))<1e-6, ('Return route misses exterior bridge, town gate or castle stop',point)
    assert abs(sum(math.dist(a,b)for a,b in zip(WALK,WALK[1:]))-848.9417986638169)<1e-7
    from check_castle_demo import route_clearance,collision_boxes,drawbridge_collision_boxes
    pieces=collision_boxes(ROOT/'engine/world_village_colliders.inc')
    assets=json.loads((ROOT/'assets/purchased/castle_expansion/castle_expansion_asset_manifest.json').read_text())
    pieces.update({a['asset']:a['collision_boxes']for a in assets['assets']})
    pieces['castle_drawbridge']=drawbridge_collision_boxes(ROOT/'engine/world_castle_drawbridge_colliders.inc')
    models={r['alias']:dict(bounds_y_up=author.model(r['alias'],True))for r in author.records}
    route_proof=route_clearance(author.records,models,WALK,pieces)
    assert len(author.layout['squares'])>=len(author._castle_expansion_squares)+len(SQUARES)
    assert author.layout['squares'][:len(author._castle_expansion_squares)]==author._castle_expansion_squares
    assert author.layout['squares'][len(author._castle_expansion_squares):len(author._castle_expansion_squares)+len(SQUARES)]==SQUARES
    assert len({s['id']for s in author.layout['squares']})==len(author.layout['squares'])
    known={r['label']:r for r in author.records};assert len(known)==len(author.records)
    labels=set(meta['labels']);assert len(labels)==len(authored_specs())
    actual={label:known[label]for label in labels};paths=corridors(author)
    minima={name:math.inf for name in paths};checks=0;pose_stations=0;volume_checks=0
    runtime=(ROOT/'engine/world_village_population.cpp').read_text()
    assert 'a.point={i<4?-35.:35.,-72.-(i%4)*5.}'in runtime
    assert 'a.point={i<4?-30.:30.,-64.-(i%4)*5.}'in runtime
    for expected in authored_specs():
        r=known[expected['label']]
        assert baseline_equal([r],[expected]),('Expansion recipe changed',r['label'])
        assert r['elevation']==0 and r['burial']==0 and base_y(r)==0
        others=[(other['label'],shape(author,other),other)for other in author.records
            if other['label']!=r['label']and other['alias']!='starting_gate'
            and not(r['alias']=='castle_training_fence'and other['alias']=='castle_training_fence')]
        assert clear(author,r,others),('Expansion overlap',r['label'])
        volume_checks+=len(others)
        for name,segments in paths.items():
            for a,b in segments:
                distance=route_margin(author,r,a,b);assert distance>.8,(r['label'],name,distance)
                minima[name]=min(minima[name],distance);checks+=1
        # Existing archers are independently static, not in the runtime route
        # arrays. Both eight-person lines use these sixteen stationary roots.
        for side in[-1,1]:
            for x,z in[(side*35,-72-j*5)for j in range(4)]+[(side*30,-64-j*5)for j in range(4)]:
                assert route_margin(author,r,[x,z],[x,z])>.9,(r['label'],'old archer',x,z);pose_stations+=1
    assetproof=validate_assets()
    assert len(actual)==63,(len(actual),'Unexpected authored layer instance count')
    return dict(instances=len(actual),existing_records_preserved=len(base),statue_height_m=author.model('castle_monument')[1][1],
        source_bound_volume_checks=volume_checks,route_capsule_checks=checks,
        existing_archer_station_checks=pose_stations,minimum_full_model_route_distances_m=minima,
        new_model_families=6,new_gpu_image_bytes=assetproof['new_gpu_image_bytes'],
        model_triangles={row['asset']:row['triangles']for row in assetproof['assets']},
        model_sha256={row['asset']:row['sha256']for row in assetproof['assets']},
        paving_regions=3,pedestrian_roads=3,route_length_m=sum(math.dist(a,b)for a,b in zip(WALK,WALK[1:])),
        route_clearance=route_proof,route_waypoints=len(WALK),exterior_bridge_and_outer_gate_crossed=True,
        inner_court_loop_preserved=True,training_dummy_stations=DUMMY_CONTACTS)


def test(author):
    proof=validate(author);old=copy.deepcopy(author.records);squares=copy.deepcopy(author.layout['squares'])
    meta=apply(author);assert apply(author)==meta and author.records==old and author.layout['squares']==squares
    bad=copy.copy(author);bad.records=copy.deepcopy(author.records)
    next(r for r in bad.records if r['label']==meta['statue_label'])['x']=0
    try:validate(bad)
    except AssertionError:pass
    else:raise AssertionError('Road-blocking statue negative control accepted')
    bad=copy.copy(author);bad.records=copy.deepcopy(author.records);bad.records[0]['x']+=.1
    try:validate(bad)
    except AssertionError:pass
    else:raise AssertionError('Changed old scene prefix accepted')
    rounded=copy.copy(author);rounded.records=copy.deepcopy(author.records)
    from build_castle_village import WIDTH,DEPTH
    for r in rounded.records:
        r['x']=(float(format(.487+r['x']/WIDTH,'.12g'))-.487)*WIDTH
        r['z']=(float(format(.320+r['z']/DEPTH,'.12g'))-.320)*DEPTH
        r['yaw']=math.radians(float(format(math.degrees(r['yaw']),'.12g')))
        for key in ['scale','burial','elevation']:r[key]=float(format(r[key],'.12g'))
    validate(rounded)
    rays=0
    with tempfile.TemporaryDirectory(prefix='valwood-castle-expansion-')as tmp:
        atlas=Atlas(tmp)
        for label in meta['labels']:
            r=next(r for r in author.records if r['label']==label);s=shape(author,r)
            for x,z in[(r['x'],r['z']),(s[8],s[10]),(s[8],s[11]),(s[9],s[10]),(s[9],s[11])]:
                assert abs(atlas.sample(ORIGIN[0]+x,ORIGIN[1]+z).height-24)<1e-4,(label,'actual CPU grounding');rays+=1
    proof.update(idempotent=True,road_negative_control=True,old_prefix_negative_control=True,actual_cpu_ground_rays=rays,serialized_roundtrip=True)
    return proof


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--stage',action='store_true');p.add_argument('--test',action='store_true');a=p.parse_args();assert a.stage or a.test
    author=Author(json.loads(PATH.read_text()));author.compose();metadata=apply(author)
    result=test(author)if a.test else validate(author)
    if a.stage:
        OUTPUT.mkdir(parents=True,exist_ok=True)
        (OUTPUT/'expansion-stage.json').write_text(json.dumps(dict(metadata=metadata,validation=result,
            placements=[r for r in author.records if r['label']in set(metadata['labels'])]),indent=2)+'\n')
    print(json.dumps(result,indent=2))
if __name__=='__main__':main()
