#!/usr/bin/env python3
"""A bounded forty-metre, source-resident market street composition.

Full-size cloth-roofed counters occupy measured gaps between existing houses;
three actual façade-supported cloth lines frame the walking view. Ground-level
shop work, supported stock and tall hooks create readable foreground/midground
layers. Existing scene records, routes, residents, pets and model bytes remain
unchanged. All append-only placements retain real whole-building eave gaps.
"""
from pathlib import Path
import argparse
import collections
import copy
import hashlib
import json
import math
import tempfile

from build_castle_village import Author,PATH,MODELS,ALIASES,PURCHASED_BUILDINGS,ORIGIN,WIDTH,DEPTH,separated_bounds
from build_village_demo_dressing import base_y,shape,overlap,support,baseline_equal,route_margin
from build_village_market_lively import ground_record,obstacles,corridors_clear,TRADERS,OLD_STATIONS,PET_STATIONS,PET_ROUTES,PET_RADII
from build_village_demo_bunting import anchors
from build_starting_area import glb_bounds

ROOT=Path(__file__).resolve().parents[1]
PROOF=ROOT/'docs/performance/market-hero'
REGION=[-84,46.2,-37,62.5]
WALK=[[-82,55],[-38,55],[-82,55]]
VIEWS=[dict(id='castle-hero-entry',position_xz=[-80,55],target_xz=[-48,55],pitch=.035),
       dict(id='castle-hero-mid',position_xz=[-64,55],target_xz=[-43,55.4],pitch=.025),
       dict(id='castle-hero-reverse',position_xz=[-42,55],target_xz=[-72,55.4],pitch=.035)]
CLOTH=[dict(id='entry',axis='x',station=-70,height=6.4,flags=6,parents=['demo_home_market_row_north_0','demo_home_market_row_south_1']),
       dict(id='middle',axis='x',station=-60,height=6.4,flags=6,parents=['demo_home_market_row_north_1','demo_home_market_row_south_2']),
       dict(id='exit',axis='x',station=-48,height=6.4,flags=6,parents=['demo_home_market_row_north_2','demo_home_market_row_south_2'])]
CANOPIES=[('baker',-65.8,49.0,['bread','bread','bread','tankard','cheese','bread']),
          ('dairy',-54.5,49.0,['cheese','cheese','bread','tankard','cheese','cheese'])]
DISPLAYS=[('bread_delivery',-71.1,52.4,['bread','bread','cheese']),
          ('brewer',-71.0,57.6,['tankard','tankard','bread']),
          ('victualler',-60.0,57.6,['chicken','cheese','bread']),
          ('cooper',-42.3,52.4,['tankard','tankard','cheese'])]
LAMPS=[(-74.8,51.95),(-60.0,51.95),(-47.2,51.95),(-74.2,58.15),(-64.8,58.15),(-51.5,58.15)]


def triangle_box_intersects(triangle,low,high):
    """Independent 13-axis source-triangle versus cloth-volume SAT."""
    centre=[(low[i]+high[i])*.5 for i in range(3)];half=[(high[i]-low[i])*.5 for i in range(3)]
    vertices=[[p[i]-centre[i]for i in range(3)]for p in triangle]
    cross=lambda a,b:[a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]]
    edges=[[vertices[(j+1)%3][i]-vertices[j][i]for i in range(3)]for j in range(3)]
    basis=[[1,0,0],[0,1,0],[0,0,1]];axes=basis+[cross(edges[0],edges[1])]+[cross(edge,axis)for edge in edges for axis in basis]
    for axis in axes:
        if sum(x*x for x in axis)<1e-16:continue
        radius=sum(half[i]*abs(axis[i])for i in range(3));projection=[sum(p[i]*axis[i]for i in range(3))for p in vertices]
        if min(projection)>radius+1e-7 or max(projection)<-radius-1e-7:return False
    return True


def cloth_source_clearance(author,records,cloth_labels):
    # Full architectural eave envelopes can overhang the thin suspended cloth
    # without touching it. Prove that distinction against actual transformed
    # source triangles; the ground-level15cm envelope rule remains unchanged.
    buildings=[r for r in author.records if r['alias']in PURCHASED_BUILDINGS]
    candidates=triangles=bounds_tests=0
    for label in cloth_labels:
        cloth=records[label];box=shape(author,cloth);low=[box[8],box[6],box[10]];high=[box[9],box[7],box[11]]
        for building in buildings:
            if not overlap(box,shape(author,building),0):continue
            candidates+=1;alias=building['alias']
            if alias not in author.triangles:author.triangles[alias]=glb_bounds(MODELS/(ALIASES[alias]+'.glb'),True)[3]
            c,s=math.cos(building['yaw']),math.sin(building['yaw']);scale=building['scale'];base=base_y(building)
            for source in author.triangles[alias]:
                bounds_tests+=1
                world=[(building['x']+(c*p[0]+s*p[2])*scale,base+p[1]*scale,building['z']+(-s*p[0]+c*p[2])*scale)for p in source]
                if any(max(p[i]for p in world)<low[i]or min(p[i]for p in world)>high[i]for i in range(3)):continue
                triangles+=1
                assert not triangle_box_intersects(world,low,high),('Hero cloth intersects actual architectural triangle',label,building['label'])
    return candidates,bounds_tests,triangles


def apply(author):
    if hasattr(author,'_market_hero_metadata'):return copy.deepcopy(author._market_hero_metadata)
    baseline=copy.deepcopy(author.records);rows=obstacles(author);known={r['label']:r for r in baseline}
    baseline_squares=copy.deepcopy(author.layout['squares']);baseline_roads=copy.deepcopy(author.layout['roads'])
    assert not any(r['label'].startswith('hero_market_')for r in baseline)
    resident={r['alias']for r in baseline};labels=[];ground=[];proofs=[];groups=[];cloth=[]
    def clear(r,exclude=()):
        if not corridors_clear(author,r):return False
        if r['alias']in PURCHASED_BUILDINGS and any(route_margin(author,r,road['a'],road['b'])<=road['half_width']+.37 for road in author.layout['roads']):return False
        a=shape(author,r)
        for label,b,record in rows:
            if label in exclude:continue
            if (r['alias']in PURCHASED_BUILDINGS or record['alias']in PURCHASED_BUILDINGS)and not separated_bounds(author,r,record,.17):return False
            if overlap(a,b,.025):return False
        return True
    def register(r,proof=None,overhead=False):
        assert r['label']not in known and r['alias']in resident,('Hero introduced a model family or label collision',r['label'])
        author.add(r['alias'],r['x'],r['z'],r['yaw'],r['scale'],r['burial'],r['label'],r['elevation'])
        author.records[-1].update({k:v for k,v in r.items()if k not in author.records[-1]})
        r=author.records[-1];known[r['label']]=r;labels.append(r['label']);rows.append((r['label'],shape(author,r),r))
        if proof:proofs.append(proof);author.attachments.append(proof)
        elif not overhead:ground.append(r['label'])
        return r
    def grounded(alias,label,x,z,scale=1,yaw=0,adjust=0):
        shifts=[(0,0)]+[(dx,dz)for dx,dz in[(.2,0),(-.2,0),(.4,0),(-.4,0),(.6,0),(-.6,0),(.8,0),(-.8,0)]if math.hypot(dx,dz)<=adjust]
        for dx,dz in shifts:
            r=ground_record(author,alias,label,x+dx,z+dz,scale,yaw)
            if clear(r):return register(r)
        raise AssertionError(('No authored hero street slot',label,alias,[x,z]))
    def attached(parent,label,alias,local,scale=.85,vertical=None,yaw=0):
        _,hi=author.model(parent['alias'],True)
        r,proof=support(author,dict(label=label,alias=alias,scale=scale,yaw_degrees=math.degrees(yaw)),parent,local,vertical or[hi[1]-.12,hi[1]+.025])
        assert clear(r,[parent['label']]),('Hero supported volume intersects scene',label)
        return register(r,proof)
    for story,x,z,goods in CANOPIES:
        before=len(labels);prefix='hero_market_'+story
        canopy=grounded('purchased_shop_d',prefix+'_canopy',x,z,1,math.pi)
        for i,local in enumerate([[-.37,-1.7],[-.37,-.95],[-.37,-.2],[.3,-1.7],[.3,-.95],[.3,-.2]]):
            attached(canopy,prefix+'_counter_stock_'+str(i),'purchased_goods_'+goods[i],local,.92,[.95,1.12],.04 if i%2 else-.04)
        groups.append(dict(id=story,title=story.title()+' full-size covered counter',labels=labels[before:],kind='source_cloth_infill',position_xz=[x,z]))
    for story,x,z,goods in DISPLAYS:
        before=len(labels);prefix='hero_market_'+story
        left=grounded('purchased_crate',prefix+'_counter_left',x-.48,z,.72,.035,adjust=.6)
        right=grounded('purchased_crate',prefix+'_counter_right',x+.43,z,.75,-.045,adjust=.6)
        stack=attached(left,prefix+'_raised_display','purchased_crate',[0,0],.46)
        attached(stack,prefix+'_stock_0','purchased_goods_'+goods[0],[0,0],.82)
        attached(right,prefix+'_stock_1','purchased_goods_'+goods[1],[0,0],.86)
        reserve_offset=2.9 if story=='brewer'else 1.43
        stock=grounded('purchased_prop_barrel_small'if story in['brewer','cooper']else'purchased_prop_sack',prefix+'_reserve',x+reserve_offset,z,.9,.13,adjust=.6)
        if stock['alias']=='purchased_prop_barrel_small':attached(stock,prefix+'_stock_2','purchased_goods_tankard',[0,0],.75)
        groups.append(dict(id=story,title=story.replace('_',' ').title()+' street workstation',labels=labels[before:],kind='source_stock_workstation',position_xz=[x,z]))
    before=len(labels)
    for i,(x,z)in enumerate(LAMPS):grounded('purchased_prop_lamp',f'hero_market_street_hook_{i}',x,z,1,0,adjust=.8)
    groups.append(dict(id='street_hooks',title='Tall street-edge merchant hooks',kind='vertical_framing',labels=labels[before:]))
    lo,hi=author.model('purchased_prop_rope',True);length=hi[2]-lo[2]
    for recipe in CLOTH:
        ends,span,yaw=anchors(author,recipe);a,b=[e['world_xz']for e in ends]
        count=math.ceil(span/(length*.55));scale=span/(length*count);rope_labels=[];flag_labels=[]
        for i in range(count):
            t=(i+.5)/count;label=f'hero_market_{recipe["id"]}_rope_{i}'
            r=dict(alias='purchased_prop_rope',label=label,x=a[0]+(b[0]-a[0])*t,z=a[1]+(b[1]-a[1])*t,yaw=yaw,scale=scale,burial=0,elevation=recipe['height']-(hi[1]+lo[1])*.5*scale)
            register(r,overhead=True);rope_labels.append(label)
        for i in range(recipe['flags']):
            alias='purchased_prop_flag_'+('a'if i%2==0 else'b');flo,fhi=author.model(alias,True);scale=1.03 if i%2==0 else.95;t=(i+1)/7
            label=f'hero_market_{recipe["id"]}_cloth_{i}'
            r=dict(alias=alias,label=label,x=a[0]+(b[0]-a[0])*t,z=a[1]+(b[1]-a[1])*t,yaw=yaw,scale=scale,burial=0,elevation=recipe['height']-fhi[1]*scale)
            register(r,overhead=True);flag_labels.append(label)
        cloth.append(dict(id=recipe['id'],anchors=ends,span_m=span,height_m=recipe['height'],rope_labels=rope_labels,flag_labels=flag_labels))
    assert author.records[:len(baseline)]==baseline
    aprons=[]
    for story,_,_,_ in CANOPIES:
        parent=known['hero_market_'+story+'_canopy'];box=shape(author,parent)
        bounds=[box[8]-.10,box[10]-.10,box[9]+.10,box[11]+.90]
        square=dict(id='hero_market_'+story+'_apron',center=[(bounds[0]+bounds[2])*.5,(bounds[1]+bounds[3])*.5],
                    half=[(bounds[2]-bounds[0])*.5,(bounds[3]-bounds[1])*.5],paving=.83)
        assert square['id']not in {r['id']for r in author.layout['squares']},('Duplicate hero apron',square['id'])
        author.layout['squares'].append(square)
        aprons.append(dict(id=square['id'],parent=parent['label'],bounds_xz=bounds,
                           source_world_bounds_xz=[box[8],box[10],box[9],box[11]],
                           side_extension_m=.10,back_extension_m=.10,street_extension_m=.90,paving=.83))
    author._market_hero_baseline=baseline
    author._market_hero_baseline_squares=baseline_squares;author._market_hero_baseline_roads=baseline_roads
    author._market_hero_metadata=dict(schema_version=1,title='Market Row | Bakers and Brewers',
        recipe_file=str(Path(__file__).relative_to(ROOT)),recipe_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        bounds_xz=REGION,walking_route_xz=WALK,clear_walking_width_m=3,route_capsule_radius_m=.37,views=VIEWS,
        purchased_canopy_full_paved_road_edge_margin_m=.37,
        labels=labels,ground_labels=ground,surface_supports=proofs,cloth_lines=cloth,groups=groups,ground_aprons=aprons,
        existing_records_preserved=len(baseline),source_model_sha256={alias:hashlib.sha256((MODELS/(ALIASES[alias]+'.glb')).read_bytes()).hexdigest()for alias in sorted({known[label]['alias']for label in labels})},
        source_envelopes=[dict(label=label,whole_bounds_xz=list(shape(author,known[label])[8:12]))for label in labels if known[label]['alias']in PURCHASED_BUILDINGS],
        protected_merchant_stations_xz=TRADERS+OLD_STATIONS,merchant_edge_clearance_m=.8,
        protected_pet_stations=[dict(species=species,position_xz=point,radius_m=PET_RADII[species])for species,point in PET_STATIONS],
        protected_pet_routes=[dict(species=species,loop_xz=points,radius_m=PET_RADII[species])for species,points in PET_ROUTES],
        protected_pet_edge_margin_m=.12,
        pedestrian_policy='Existing nine 128-human route envelopes, all fixed keepers, six pet pads and six conservative pet loops retained',
        new_model_families=0,new_texture_policies=0)
    return copy.deepcopy(author._market_hero_metadata)


def validate(author):
    from build_village_demo_dressing import route_margin
    meta=author._market_hero_metadata;known={r['label']:r for r in author.records};labels=meta['labels'];baseline=author._market_hero_baseline
    assert len(author.records)>=len(baseline)+len(labels)and baseline_equal(author.records[:len(baseline)],baseline),'Hero changed earlier scene records'
    assert len(known)==len(author.records)and len(labels)==len(set(labels))and 100<=len(labels)<=175
    old_roads=author._market_hero_baseline_roads
    assert author.layout['roads'][:len(old_roads)]==old_roads,'Hero changed existing public roads'
    assert len({road['id']for road in author.layout['roads']})==len(author.layout['roads']),'Duplicate public road id'
    old_squares=author._market_hero_baseline_squares
    assert author.layout['squares'][:len(old_squares)]==old_squares,'Hero changed existing paving squares'
    assert len({row['id']for row in author.layout['squares']})==len(author.layout['squares']),'Duplicate hero paving square id'
    assert len(meta['ground_aprons'])==2
    squares={row['id']:row for row in author.layout['squares']};road=next(row for row in author.layout['roads']if row['id']=='cross_55')
    minimum_apron_overlap=math.inf
    for apron in meta['ground_aprons']:
        parent=known[apron['parent']];box=shape(author,parent);expected=[box[8]-.10,box[10]-.10,box[9]+.10,box[11]+.90]
        assert max(abs(a-b)for a,b in zip(apron['bounds_xz'],expected))<2e-6,'Hero apron differs from actual source bounds'
        assert max(abs(a-b)for a,b in zip(apron['source_world_bounds_xz'],[box[8],box[10],box[9],box[11]]))<2e-6,'Hero apron source envelope changed'
        assert REGION[0]<=expected[0]<expected[2]<=REGION[2]and REGION[1]<=expected[1]<expected[3]<=REGION[3],'Hero apron leaves bounded street'
        square=squares[apron['id']];actual=[square['center'][0]-square['half'][0],square['center'][1]-square['half'][1],square['center'][0]+square['half'][0],square['center'][1]+square['half'][1]]
        assert max(abs(a-b)for a,b in zip(actual,expected))<2e-6,'Hero paving square does not cover measured apron'
        assert square['paving']==apron['paving']==.83
        overlap_m=expected[3]-(road['a'][1]-road['half_width']);minimum_apron_overlap=min(minimum_apron_overlap,overlap_m)
        assert overlap_m>.40,'Hero apron fails to meet existing road paving'
    assert meta['recipe_sha256']==hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),'Hero recipe hash changed'
    for alias,digest in meta['source_model_sha256'].items():assert digest==hashlib.sha256((MODELS/(ALIASES[alias]+'.glb')).read_bytes()).hexdigest(),('Hero source model bytes changed',alias)
    overhead={x for row in meta['cloth_lines']for x in row['rope_labels']+row['flag_labels']};grounds=set(meta['ground_labels']);proofs={p['label']:p for p in meta['surface_supports']};rows=obstacles(author)
    comparisons=eaves=0;minimum_spine=minimum_canopy_road=math.inf;minimum_cloth=math.inf;anchor_checks=0
    for label in labels:
        r=known[label];lo,hi=author.model(r['alias'],True);box=shape(author,r)
        assert REGION[0]<=box[8]<=box[9]<=REGION[2]and REGION[1]<=box[10]<=box[11]<=REGION[3],('Hero prop leaves bounded street',label)
        if label in overhead:
            assert box[6]>4.8,('Hero cloth lowered into walking volume',label)
            minimum_cloth=min(minimum_cloth,box[6]);continue
        assert corridors_clear(author,r),('Hero obstructs crowd, pets or walking corridor',label)
        if r['alias']in PURCHASED_BUILDINGS:
            for road in author.layout['roads']:
                road_edge=route_margin(author,r,road['a'],road['b'])-road['half_width'];minimum_canopy_road=min(minimum_canopy_road,road_edge)
                assert road_edge>.37,('Hero purchased canopy blocks full paved road',label,road['id'],road_edge)
        margin=min(route_margin(author,r,a,b)for a,b in zip(WALK,WALK[1:]));minimum_spine=min(minimum_spine,margin)
        assert margin>1.87,('Hero clear walking spine narrowed',label)
        if label in grounds:assert abs(base_y(r)+lo[1]*r['scale'])<1e-6,('Hero ground contact',label)
        else:
            p=proofs[label];parent=known[p['parent']];target=base_y(parent)+max(p['actual_surface_samples_m'])*parent['scale']+.002
            assert len(p['actual_surface_samples_m'])==5 and max(p['actual_surface_samples_m'])-min(p['actual_surface_samples_m'])<.025
            assert abs(base_y(r)+lo[1]*r['scale']-target)<1e-6,('Hero supported contact',label)
        for other,b,record in rows:
            if other==label or other==r.get('surface_parent')or record.get('surface_parent')==label:continue
            if other in overhead:continue
            if r['alias']in PURCHASED_BUILDINGS or record['alias']in PURCHASED_BUILDINGS:
                assert separated_bounds(author,r,record,.15),('Hero whole architectural eave gap',label,other);eaves+=1
            assert not overlap(box,b,.018),('Hero prop volume intersects scene',label,other);comparisons+=1
    for recipe,row in zip(CLOTH,meta['cloth_lines']):
        expected,span,yaw=anchors(author,recipe);assert abs(span-row['span_m'])<2e-6
        for actual,reference in zip(row['anchors'],expected):
            assert actual['parent']==reference['parent']and actual['model_sha256']==reference['model_sha256']
            assert math.dist(actual['source_local_xyz'],reference['source_local_xyz'])<2e-6 and math.dist(actual['world_xz'],reference['world_xz'])<2e-6,'Hero façade anchor mismatch';anchor_checks+=1
        a,b=[e['world_xz']for e in expected]
        for i,label in enumerate(row['rope_labels']):
            r=known[label];lo,hi=author.model(r['alias'],True);t=(i+.5)/len(row['rope_labels'])
            assert math.dist([r['x'],r['z']],[a[0]+(b[0]-a[0])*t,a[1]+(b[1]-a[1])*t])<2e-6,'Hero rope interval shifted'
            assert abs((hi[2]-lo[2])*r['scale']-span/len(row['rope_labels']))<2e-6
            assert abs(r['elevation']+(hi[1]+lo[1])*.5*r['scale']-row['height_m'])<2e-6
        for i,label in enumerate(row['flag_labels']):
            r=known[label];lo,hi=author.model(r['alias'],True);t=(i+1)/7
            assert math.dist([r['x'],r['z']],[a[0]+(b[0]-a[0])*t,a[1]+(b[1]-a[1])*t])<2e-6,'Hero hanging cloth shifted'
            assert abs(r['elevation']+hi[1]*r['scale']-row['height_m'])<2e-6
    source_candidates,source_bounds_tests,source_triangle_checks=cloth_source_clearance(author,known,[x for row in meta['cloth_lines']for x in row['flag_labels']])
    return dict(instances=len(labels),authored_commerce_groups=len(meta['groups']),full_size_purchased_roofed_counters=2,
        overhead_cloth_rows=len(meta['cloth_lines']),actual_source_triangle_anchors=anchor_checks,
        hanging_cloths=sum(len(r['flag_labels'])for r in meta['cloth_lines']),lowest_cloth_height_m=minimum_cloth,
        ground_objects=len(grounds),supported_objects=len(proofs),actual_mesh_support_rays=len(proofs)*5,
        volume_checks=comparisons,whole_architectural_eave_checks=eaves,minimum_clear_spine_half_width_m=minimum_spine,
        minimum_purchased_canopy_full_road_edge_clearance_m=minimum_canopy_road,
        overhead_actual_architecture_candidates=source_candidates,overhead_real_triangle_bounds_checks=source_bounds_tests,overhead_exact_triangle_box_checks=source_triangle_checks,
        local_walking_route_length_m=sum(math.dist(a,b)for a,b in zip(WALK,WALK[1:])),layered_views=len(VIEWS),
        source_measured_ground_aprons=len(meta['ground_aprons']),minimum_apron_overlap_with_existing_road_m=minimum_apron_overlap,
        existing_roads_preserved=len(author._market_hero_baseline_roads),existing_paving_squares_preserved=len(old_squares),
        existing_records_preserved=len(baseline),new_model_families=0,new_texture_policies=0,
        aliases=dict(sorted(collections.Counter(known[x]['alias']for x in labels).items())))


def test(author):
    result=validate(author);meta=apply(author);before=copy.deepcopy(author.records)
    assert apply(author)==meta and author.records==before
    def clone():
        a=copy.copy(author);a.records=copy.deepcopy(author.records);a.layout=copy.deepcopy(author.layout);a._market_hero_metadata=copy.deepcopy(meta);return a
    rounded=clone()
    for r in rounded.records:
        r['x']=(float(format(.487+r['x']/WIDTH,'.12g'))-.487)*WIDTH;r['z']=(float(format(.320+r['z']/DEPTH,'.12g'))-.320)*DEPTH
        r['yaw']=math.radians(float(format(math.degrees(r['yaw']),'.12g')))
        for key in['scale','elevation','burial']:r[key]=float(format(r[key],'.12g'))
    validate(rounded)
    def rejects(a,text):
        try:validate(a)
        except AssertionError as error:assert text in str(error),(text,str(error))
        else:raise AssertionError('Negative control passed: '+text)
    changed=clone();changed.records[0]['x']+=.01;rejects(changed,'Hero changed earlier scene records')
    changed_road=clone();changed_road.layout['roads'][0]['half_width']+=.01;rejects(changed_road,'Hero changed existing public roads')
    short_apron=clone();next(row for row in short_apron.layout['squares']if row['id']==meta['ground_aprons'][0]['id'])['half'][1]-=.15;rejects(short_apron,'Hero paving square does not cover measured apron')
    blocked=clone();next(r for r in blocked.records if r['label']==meta['ground_labels'][2])['z']=55;rejects(blocked,'Hero obstructs crowd, pets or walking corridor')
    paved_road=clone();next(r for r in paved_road.records if r['label']==meta['ground_labels'][0])['z']=50.2
    # Even a correctly resized apron cannot allow a canopy into the full road.
    apron=paved_road._market_hero_metadata['ground_aprons'][0]
    for key in['bounds_xz','source_world_bounds_xz']:
        for index in[1,3]:apron[key][index]+=1.2
    next(row for row in paved_road.layout['squares']if row['id']==apron['id'])['center'][1]+=1.2
    rejects(paved_road,'Hero purchased canopy blocks full paved road')
    floating=clone();next(r for r in floating.records if r['label']==meta['ground_labels'][0])['elevation']+=.03;rejects(floating,'Hero ground contact')
    unsupported=clone();next(r for r in unsupported.records if r['label']==meta['surface_supports'][0]['label'])['elevation']+=.03;rejects(unsupported,'Hero supported contact')
    low=clone();next(r for r in low.records if r['label']==meta['cloth_lines'][0]['flag_labels'][0])['elevation']-=2;rejects(low,'Hero cloth lowered into walking volume')
    bad_anchor=clone();bad_anchor._market_hero_metadata['cloth_lines'][0]['anchors'][0]['world_xz'][0]+=.1;rejects(bad_anchor,'Hero façade anchor mismatch')
    # A suspended flag moved onto its real source-wall anchor must fail the
    # independent triangle/volume detector, before any recipe-position check.
    wall_hit=clone();flag=next(r for r in wall_hit.records if r['label']==meta['cloth_lines'][0]['flag_labels'][0]);anchor=meta['cloth_lines'][0]['anchors'][0]
    lo,hi=wall_hit.model(flag['alias'],True);flag.update(x=anchor['world_xz'][0],z=anchor['world_xz'][1],elevation=anchor['height_m']-(lo[1]+hi[1])*.5*flag['scale'])
    try:cloth_source_clearance(wall_hit,{r['label']:r for r in wall_hit.records},[flag['label']])
    except AssertionError as error:assert 'Hero cloth intersects actual architectural triangle'in str(error)
    else:raise AssertionError('Source-wall intersection negative control passed')
    from build_starting_area import Atlas
    terrain_rays=0
    with tempfile.TemporaryDirectory(prefix='valwood-hero-ground-')as directory:
        atlas=Atlas(directory)
        for label in meta['ground_labels']:
            r=next(r for r in author.records if r['label']==label);lo,hi=author.model(r['alias'],True);c,s=math.cos(r['yaw']),math.sin(r['yaw']);heights=[]
            for x,z in[(0,0),(lo[0],lo[2]),(lo[0],hi[2]),(hi[0],lo[2]),(hi[0],hi[2])]:
                xx=r['x']+(c*x+s*z)*r['scale'];zz=r['z']+(-s*x+c*z)*r['scale'];heights.append(atlas.sample(ORIGIN[0]+xx,ORIGIN[1]+zz).height);terrain_rays+=1
            assert max(abs(y-24)for y in heights)<1e-4,('Hero terrain contact',label,heights)
        for apron in meta['ground_aprons']:
            x0,z0,x1,z1=apron['bounds_xz'];heights=[]
            for x,z in[[(x0+x1)*.5,(z0+z1)*.5],[x0,z0],[x0,z1],[x1,z0],[x1,z1]]:
                heights.append(atlas.sample(ORIGIN[0]+x,ORIGIN[1]+z).height);terrain_rays+=1
            assert max(abs(y-24)for y in heights)<1e-4,('Hero apron terrain contact',apron['id'],heights)
    result.update(idempotent=True,serialized_instance_roundtrip=True,baseline_negative_control=True,blocked_route_negative_control=True,
        ground_contact_negative_control=True,supported_contact_negative_control=True,low_cloth_negative_control=True,
        floating_anchor_negative_control=True,actual_source_triangle_intersection_negative_control=True,actual_cpu_terrain_rays=terrain_rays)
    result.update(full_paved_road_negative_control=True,existing_road_mutation_negative_control=True,source_measured_apron_negative_control=True)
    return result


def main():
    parser=argparse.ArgumentParser();parser.add_argument('--stage',action='store_true');parser.add_argument('--test',action='store_true');args=parser.parse_args();assert args.stage or args.test
    author=Author(json.loads(PATH.read_text()));author.compose();meta=apply(author);result=test(author)if args.test else validate(author)
    if args.stage:
        PROOF.mkdir(parents=True,exist_ok=True)
        (PROOF/'hero-stage.json').write_text(json.dumps(dict(metadata=meta,validation=result,placements=[r for r in author.records if r['label']in set(meta['labels'])]),indent=2)+'\n')
    print(json.dumps(result,indent=2))


if __name__=='__main__':main()
