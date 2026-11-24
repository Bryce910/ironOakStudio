#!/usr/bin/env python3
"""Append measured, source-resident market workstations to the castle demo.

Counter displays, covered dairies, delivery carts and the northern tasting
court are authored groups. Bounded local placement keeps comfortable public
streets and the exact human circulation/stationary capsules clear. No earlier
record, model, image, renderer policy, or recipe is modified.
"""
from pathlib import Path
import argparse
import collections
import copy
import hashlib
import json
import math
import tempfile

from build_castle_village import Author, PATH, MODELS, ALIASES, NON_SOLID, PURCHASED_BUILDINGS, ROUTE, ORIGIN, WIDTH, DEPTH, separated_bounds
from build_village_demo_dressing import LEGACY, base_y, shape, overlap, support, baseline_equal, obstacle_shapes, route_margin
from build_village_demo_frontages import route_clear, crowd_segments

ROOT=Path(__file__).resolve().parents[1]
PROOF=ROOT/'docs/performance/market-lively'
TRADERS=[[-13.8,1.5],[13.8,1.5],[-13.8,16.5],[13.8,16.5],[-25.8,1.5],[-25.8,16.5],[-33.7,.5],[-33.7,15.5],[-25.8,40.5],[27.8,40.5]]
OLD_STATIONS=[*[[-25.35,z] for z in [-6,9,24]],*[[25.35,z] for z in [-6,9,24]],*[[-13.35,z]for z in [-6,24]],*[[13.35,z]for z in [-6,24]]]
PET_STATIONS=[('cat',[20.8,42.0]),('cat',[-20.8,40.8]),('cat',[45.8,26]),
              ('dog',[-45.8,26]),('dog',[23,186]),('dog',[-23,186])]
PET_ROUTES=[
 ('cat',[[21.2,44.8],[21.2,48.2],[22.4,48.2],[22.4,44.8]]),
 ('cat',[[-20.8,45.5],[-20.8,48.5],[-22,48.5],[-22,45.5]]),
 ('cat',[[46,30],[46,42],[47.2,42],[47.2,30]]),
 ('dog',[[-46,30],[-46,42],[-47.6,42],[-47.6,30]]),
 ('dog',[[19,180],[19,192],[20.6,192],[20.6,180]]),
 ('dog',[[-19,180],[-19,192],[-20.6,192],[-20.6,180]]),
]
PET_RADII={'cat':.4,'dog':.75}
# The exact runtime merchant assignments are part of the recipe. Work tables
# face the aisle; signed inward cargo offsets give keepers a clear outer side.
TABLES=[
 ('baker',[-12,1.5],1),('dairy',[12,1.5],-1),
 ('roaster',[-12,16.5],1),('brewer',[12,16.5],-1),
 ('cooper',[-24,1.5],1),('provisions',[-24,16.5],1),
 ('tasting_west',[-24,40.5],1),('tasting_east',[26,40.5],-1),
]
GOODS={
 'baker':['bread','bread','cheese','bread','tankard','bread'],
 'dairy':['cheese','cheese','bread','cheese','tankard','cheese'],
 'roaster':['chicken','bread','chicken','cheese','tankard','bread'],
 'brewer':['tankard','tankard','bread','tankard','cheese','tankard'],
 'cooper':['tankard','tankard','cheese','tankard','bread','tankard'],
 'provisions':['bread','cheese','chicken','bread','cheese','tankard'],
 'tasting_west':['bread','tankard','cheese','tankard','chicken','tankard'],
 'tasting_east':['chicken','tankard','bread','tankard','cheese','tankard'],
}


def preserved_prefix(author, baseline):
    return len(author.records)>=len(baseline) and baseline_equal(author.records[:len(baseline)],baseline)


def corridors_clear(author,r):
    if not route_clear(author,r):return False
    if not all(route_margin(author,r,p,p)>.80 for p in TRADERS+OLD_STATIONS):return False
    if not all(route_margin(author,r,p,p)>PET_RADII[species]+.12 for species,p in PET_STATIONS):return False
    return all(route_margin(author,r,a,b)>PET_RADII[species]+.12
               for species,points in PET_ROUTES for a,b in zip(points,points[1:]+points[:1]))


def obstacles(author):
    rows=obstacle_shapes(author);present={r[0] for r in rows}
    # Decorative goods and foliage still occupy visible volume even when
    # deliberately omitted from the native player's collision list.
    rows.extend((r['label'],shape(author,r),r)for r in author.records if r['label']not in present and r['alias']!='starting_gate')
    return rows


def clear(author,r,rows,exclude=()):
    if not corridors_clear(author,r):return False
    box=shape(author,r)
    for label,other,record in rows:
        if label in exclude:continue
        if (r['alias']in PURCHASED_BUILDINGS or record['alias']in PURCHASED_BUILDINGS) and not separated_bounds(author,r,record,.17):return False
        if overlap(box,other,.025):return False
    return True


def ground_record(author,alias,label,x,z,scale=1,yaw=0):
    lo,_=author.model(alias,True)
    return dict(alias=alias,label=label,x=x,z=z,yaw=yaw,scale=scale,burial=0,
                elevation=-lo[1]*scale+(.05 if alias in LEGACY else 0))


def apply(author):
    if hasattr(author,'_market_lively_metadata'):return copy.deepcopy(author._market_lively_metadata)
    baseline=copy.deepcopy(author.records);resident={r['alias']for r in baseline}
    assert not any(r['label'].startswith('lively_market_')for r in baseline)
    rows=obstacles(author);known={r['label']:r for r in baseline};labels=[];ground=[];proofs=[];groups=[]
    def register(r,proof=None):
        assert r['label']not in known and r['alias']in resident,('New market family or duplicate label',r['label'])
        author.add(r['alias'],r['x'],r['z'],r['yaw'],r['scale'],r['burial'],r['label'],r['elevation'])
        author.records[-1].update({k:v for k,v in r.items()if k not in author.records[-1]})
        actual=author.records[-1];known[r['label']]=actual;labels.append(r['label']);rows.append((r['label'],shape(author,r),actual))
        if proof:proofs.append(proof);author.attachments.append(proof)
        else:ground.append(r['label'])
        return actual
    def grounded(alias,label,x,z,scale=1,yaw=0,adjust=0):
        # Local alternatives are for real millimetre footprint gaps, never a
        # broad scatter search. Main counters and merchant positions are exact.
        shifts=[(0,0)]+[(dx,dz)for dx,dz in [(.2,0),(-.2,0),(0,.2),(0,-.2),(.4,0),(-.4,0),(0,.4),(0,-.4)]if math.hypot(dx,dz)<=adjust]
        for dx,dz in shifts:
            r=ground_record(author,alias,label,x+dx,z+dz,scale,yaw)
            if clear(author,r,rows):return register(r)
        raise AssertionError(('No clear authored market slot',label,alias,[x,z]))
    def attached(parent,name,alias,local,scale=.9,yaw=0,vertical=None):
        _,hi=author.model(parent['alias'],True)
        spec=dict(label=name,alias=alias,scale=scale,yaw_degrees=math.degrees(yaw))
        r,proof=support(author,spec,parent,local,vertical or [hi[1]-.12,hi[1]+.025])
        assert clear(author,r,rows,[parent['label']]),('Market supported volume overlap',name)
        return register(r,proof)
    for index,(story,position,inward)in enumerate(TABLES):
        before=len(labels);prefix='lively_market_'+story;x,z=position
        table=grounded('starting_table',prefix+'_counter',x+inward*.15,z,.94)
        # Six whole-footprint rays place the different stock on real scan wood.
        for i,(lx,lz)in enumerate([[-.28,-.85],[.3,-.82],[-.28,0],[.3,.03],[-.28,.84],[.3,.87]]):
            kind=GOODS[story][i]
            attached(table,prefix+'_display_'+str(i),'purchased_goods_'+kind,[lx,lz],.83 if kind!='tankard'else.9,(-.1 if i%2 else .12))
        for n,(alias,dx,dz,scale,yaw)in enumerate([
            ('purchased_crate',0,3.0,.75,.06),
            ('purchased_barrel',0,-3.1,.82,.12),
            ('purchased_prop_barrel_small',inward*1.15,-2.9,1.0,-.1),
            ('purchased_prop_sack',inward*1.15,2.95,.95,.16),
        ]):
            p=grounded(alias,prefix+'_stock_'+str(n),x+dx,z+dz,scale,yaw,.4)
            if n in [0,1]:attached(p,prefix+'_stock_top_'+str(n),'purchased_goods_'+('bread'if n==0 else'tankard'),[0,0],.8,.04)
        groups.append(dict(id=story,title=story.replace('_',' ').title()+' counter and provisions',labels=labels[before:],centre_xz=position))
    # Two source cloth roofs define a covered western arcade, with measured
    # integrated counter stock. The original twelve coloured booths remain.
    for index,z in enumerate([1.5,16.5]):
        before=len(labels);prefix=f'lively_market_covered_{index}'
        parent=grounded('purchased_shop_d',prefix+'_canopy',-31,z,.82,0)
        for i,(lx,lz)in enumerate([[-.37,-1.7],[-.37,-.95],[-.37,-.2],[.3,-1.7],[.3,-.95],[.3,-.2]]):
            kind=['bread','cheese','chicken','tankard','bread','cheese'][(i+index)%6]
            attached(parent,prefix+'_display_'+str(i),'purchased_goods_'+kind,[lx,lz],.78,0,[.95,1.12])
        for n,(alias,dx,dz,scale)in enumerate([
            ('purchased_crate',.2,3.0,.78),('purchased_prop_sack',1.2,2.9,.92),
            ('purchased_barrel',-.2,-3.3,.86),('purchased_prop_barrel_small',1.0,-3.4,.95),
        ]):
            p=grounded(alias,prefix+'_warehouse_'+str(n),-31+dx,z+dz,scale,n*.13,.4)
            if n in[0,2]:attached(p,prefix+'_warehouse_top_'+str(n),'purchased_goods_'+('cheese'if n==0 else'tankard'),[0,0],.76)
        groups.append(dict(id='covered_'+str(index),title=['Covered dairy arcade','Covered victualler arcade'][index],labels=labels[before:],centre_xz=[-31,z]))
    for index,(x,z,yaw)in enumerate([(-31,8.7,.08),(-30,33.5,-.13)]):
        before=len(labels);prefix=f'lively_market_delivery_{index}'
        cart=grounded('village_cart',prefix+'_cart',x,z,.78,yaw)
        for i,(alias,local,scale)in enumerate([('purchased_goods_bread',[-.65,-.22],.75),('purchased_goods_cheese',[-.65,.23],.8),('purchased_goods_chicken',[.45,0],.9)]):
            attached(cart,prefix+'_cargo_'+str(i),alias,local,scale,0,[1.69,1.71])
        groups.append(dict(id='delivery_'+str(index),title='Arriving produce handcart',labels=labels[before:],centre_xz=[x,z]))
    for side,x in enumerate([-8.3,8.3]):
        before=len(labels)
        for index,z in enumerate([5.5,20.5]):grounded('village_banner',f'lively_market_avenue_banner_{side}_{index}',x,z,.86,0)
        groups.append(dict(id='avenue_'+str(side),title='Market avenue colour and framing',labels=labels[before:],centre_xz=[x,13]))
    assert author.records[:len(baseline)]==baseline
    author._market_lively_baseline=baseline
    author._market_lively_metadata=dict(schema_version=1,recipe_file=str(Path(__file__).relative_to(ROOT)),
        recipe_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),labels=labels,groups=groups,
        ground_labels=ground,surface_supports=proofs,existing_records_preserved=len(baseline),
        source_model_sha256={alias:hashlib.sha256((MODELS/(ALIASES[alias]+'.glb')).read_bytes()).hexdigest()
                             for alias in sorted({known[label]['alias']for label in labels})},
        trader_stations_xz=TRADERS,stationary_capsule_clearance_m=.8,
        pet_stations=[dict(species=species,position_xz=point,radius_m=PET_RADII[species])for species,point in PET_STATIONS],
        pet_routes=[dict(species=species,loop_xz=points,radius_m=PET_RADII[species])for species,points in PET_ROUTES],
        pet_edge_margin_m=.12,
        pedestrian_corridors='Existing public roads, all nine human loops, original benchmark and fixed merchant stations',
        new_model_families=0,new_texture_policies=0)
    return copy.deepcopy(author._market_lively_metadata)


def validate(author):
    meta=author._market_lively_metadata;known={r['label']:r for r in author.records};labels=meta['labels']
    assert preserved_prefix(author,author._market_lively_baseline),'Existing market baseline changed'
    assert len(labels)==len(set(labels))and len(labels)>=125 and len(labels)<=200
    assert len(known)==len(author.records)and set(labels)<=known.keys()
    assert len(author.records)>=len(author._market_lively_baseline)+len(labels)
    assert len(meta['groups'])==14 and sum(len(g['labels'])for g in meta['groups'])==len(labels)
    assert meta['recipe_sha256']==hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),'Market recipe hash changed'
    for alias,digest in meta['source_model_sha256'].items():
        assert digest==hashlib.sha256((MODELS/(ALIASES[alias]+'.glb')).read_bytes()).hexdigest(),('Market source model bytes changed',alias)
    rows=obstacles(author);grounds=set(meta['ground_labels']);proofs={p['label']:p for p in meta['surface_supports']}
    comparisons=eaves=0;minimum_station=minimum_route=minimum_pet=math.inf
    for label in labels:
        r=known[label];lo,_=author.model(r['alias'],True)
        assert corridors_clear(author,r),('Market obstructs pedestrian or trader corridor',label)
        minimum_station=min(minimum_station,*(route_margin(author,r,p,p)for p in TRADERS+OLD_STATIONS))
        minimum_route=min(minimum_route,*(route_margin(author,r,a,b)for a,b in crowd_segments()))
        minimum_pet=min(minimum_pet,*(route_margin(author,r,p,p)-PET_RADII[species]for species,p in PET_STATIONS),
                        *(route_margin(author,r,a,b)-PET_RADII[species]for species,points in PET_ROUTES for a,b in zip(points,points[1:]+points[:1])))
        if label in grounds:assert abs(base_y(r)+lo[1]*r['scale'])<1e-6,('Market ground contact',label)
        else:
            p=proofs[label];parent=known[p['parent']]
            target=base_y(parent)+max(p['actual_surface_samples_m'])*parent['scale']+.002
            assert len(p['actual_surface_samples_m'])==5 and max(p['actual_surface_samples_m'])-min(p['actual_surface_samples_m'])<.025
            assert abs(base_y(r)+lo[1]*r['scale']-target)<1e-6,('Market supported contact',label)
        for other,b,record in rows:
            if other==label or other==r.get('surface_parent')or record.get('surface_parent')==label:continue
            if r['alias']in PURCHASED_BUILDINGS or record['alias']in PURCHASED_BUILDINGS:
                assert separated_bounds(author,r,record,.15),('Market whole architectural eave gap',label,other);eaves+=1
            assert not overlap(shape(author,r),b,.018),('Market prop volume intersects scene',label,other);comparisons+=1
    return dict(instances=len(labels),authored_groups=len(meta['groups']),new_covered_source_stalls=2,
        new_counters=8,new_delivery_handcarts=2,ground_objects=len(grounds),supported_objects=len(proofs),
        actual_mesh_support_rays=len(proofs)*5,volume_clearance_checks=comparisons,whole_architectural_eave_checks=eaves,
        exact_trader_stations=len(TRADERS),minimum_stationary_capsule_distance_m=minimum_station,
        minimum_actual_npc_route_distance_m=minimum_route,existing_records_preserved=len(author._market_lively_baseline),
        protected_pet_station_pads=len(PET_STATIONS),protected_pet_loops=len(PET_ROUTES),minimum_pet_envelope_margin_m=minimum_pet,
        new_model_families=0,new_texture_policies=0,
        aliases=dict(sorted(collections.Counter(known[x]['alias']for x in labels).items())))


def test(author):
    result=validate(author);metadata=apply(author);before=copy.deepcopy(author.records)
    assert apply(author)==metadata and author.records==before
    def clone():
        a=copy.copy(author);a.records=copy.deepcopy(author.records)
        a._market_lively_metadata=copy.deepcopy(author._market_lively_metadata);return a
    rounded=clone()
    for r in rounded.records:
        r['x']=(float(format(.487+r['x']/WIDTH,'.12g'))-.487)*WIDTH
        r['z']=(float(format(.320+r['z']/DEPTH,'.12g'))-.320)*DEPTH
        r['yaw']=math.radians(float(format(math.degrees(r['yaw']),'.12g')))
        for key in['scale','elevation','burial']:r[key]=float(format(r[key],'.12g'))
    validate(rounded)
    def rejects(a,text):
        try:validate(a)
        except AssertionError as error:assert text in str(error),(text,str(error))
        else:raise AssertionError('Negative control passed: '+text)
    edited=clone();edited.records[0]['x']+=.01;rejects(edited,'Existing market baseline changed')
    blocked=clone();next(r for r in blocked.records if r['label']==metadata['ground_labels'][0]).update(x=0)
    rejects(blocked,'Market obstructs pedestrian or trader corridor')
    floating=clone();next(r for r in floating.records if r['label']==metadata['ground_labels'][0])['elevation']+=.03
    rejects(floating,'Market ground contact')
    float_product=clone();next(r for r in float_product.records if r['label']==metadata['surface_supports'][0]['label'])['elevation']+=.03
    rejects(float_product,'Market supported contact')
    collision=clone();a=next(r for r in collision.records if r['label']==metadata['ground_labels'][0]);b=next(r for r in collision.records if r['label']==metadata['ground_labels'][1]);b.update(x=a['x'],z=a['z'])
    rejects(collision,'Market prop volume intersects scene')
    bad_source=clone();key=next(iter(bad_source._market_lively_metadata['source_model_sha256']))
    bad_source._market_lively_metadata['source_model_sha256'][key]='0'*64
    rejects(bad_source,'Market source model bytes changed')
    from build_starting_area import Atlas
    terrain_rays=0
    with tempfile.TemporaryDirectory(prefix='valwood-market-ground-')as directory:
        atlas=Atlas(directory)
        for label in metadata['ground_labels']:
            r=next(r for r in author.records if r['label']==label);lo,hi=author.model(r['alias'],True)
            c,s=math.cos(r['yaw']),math.sin(r['yaw']);heights=[]
            for x,z in[(0,0),(lo[0],lo[2]),(lo[0],hi[2]),(hi[0],lo[2]),(hi[0],hi[2])]:
                xx=r['x']+(c*x+s*z)*r['scale'];zz=r['z']+(-s*x+c*z)*r['scale']
                heights.append(atlas.sample(ORIGIN[0]+xx,ORIGIN[1]+zz).height);terrain_rays+=1
            assert max(abs(y-24)for y in heights)<1e-4,('Nonlevel market ground',label,heights)
    result.update(idempotent=True,serialized_instance_roundtrip=True,baseline_mutation_rejected=True,
        blocked_route_rejected=True,ground_floating_rejected=True,supported_floating_rejected=True,
        visual_volume_overlap_rejected=True,source_bytes_hash_rejected=True,actual_cpu_terrain_rays=terrain_rays)
    return result


def main():
    parser=argparse.ArgumentParser();parser.add_argument('--stage',action='store_true');parser.add_argument('--test',action='store_true')
    args=parser.parse_args();assert args.stage or args.test
    author=Author(json.loads(PATH.read_text()));author.compose();metadata=apply(author)
    result=test(author)if args.test else validate(author)
    if args.stage:
        PROOF.mkdir(parents=True,exist_ok=True)
        (PROOF/'market-stage.json').write_text(json.dumps(dict(metadata=metadata,validation=result,
          placements=[r for r in author.records if r['label']in set(metadata['labels'])]),indent=2)+'\n')
    print(json.dumps(result,indent=2))


if __name__=='__main__':main()
