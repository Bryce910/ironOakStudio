#!/usr/bin/env python3
"""Source-resident barracks and quartermaster wings inside the castle walls.

Two full-size purchased buildings fit north of the new armory and south of the
untouched old keep service courts. Semantic aliases retain the55 residential
homes count. No mesh, image, published layout or source file is written.
"""
from pathlib import Path
import argparse,copy,hashlib,json,math,tempfile
from build_castle_village import Author,PATH,MODELS,ALIASES,ROUTE,ORIGIN,separated_bounds
from build_village_demo_dressing import shape,overlap,route_margin,baseline_equal,support
from build_village_hero_grove import source_routes
from build_starting_area import Atlas
ROOT=Path(__file__).resolve().parents[1]
OUTPUT=ROOT/'docs/performance/castle-expansion/inner-wings'
ASSET_ALIASES={'castle_barracks':'purchased_village_house_a_near',
               'castle_quartermaster':'purchased_village_house_b_near'}
ALIASES.update(ASSET_ALIASES)
SITES=[dict(id='barracks',alias='castle_barracks',source_alias='purchased_house_a',x=-47,z=-138.5,yaw=math.pi,
            front_sign=1,entry_path_xz=[[-25,-120],[-40,-138.5]]),
       dict(id='quartermaster',alias='castle_quartermaster',source_alias='purchased_house_b',x=47,z=-138.5,yaw=math.pi,
            front_sign=-1,entry_path_xz=[[25,-120],[40,-138.5]])]
ROADS=[dict(id='castle_barracks_entry',a=[-25,-120],b=[-40,-138.5],half_width=1.3,paving=.83),
       dict(id='castle_quartermaster_entry',a=[25,-120],b=[40,-138.5],half_width=1.3,paving=.83)]
VIEWS=[dict(id='castle-barracks',position_xz=[-27,-126],target_xz=[-47,-138.5],pitch=.12),
       dict(id='castle-quartermaster',position_xz=[27,-126],target_xz=[47,-138.5],pitch=.12)]


def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def specs():
    records=[]
    for site in SITES:
        side=-1 if site['x']<0 else 1;prefix='castle_inner_'+site['id']
        records.append(dict(alias=site['alias'],x=site['x'],z=site['z'],yaw=site['yaw'],scale=1,burial=0,elevation=0,label=prefix))
        for alias,x,z,yaw,scale,name in[
            ('village_banner',side*41,-132.5,math.pi/2,.88,'livery'),
            ('purchased_prop_lamp',side*38.8,-133.5,math.pi/2,.90,'lamp'),
            ('castle_yard_bench',side*39.7,-141.9,-side*math.pi/2,.90,'bench'),
            ('purchased_crate',side*39.6,-134.2,0,.88,'cargo'),
            ('purchased_barrel',side*39.6,-143.7,.10*side,.90,'barrel'),
            ('purchased_prop_sack',side*38.3,-143.6,.30*side,.85,'sack')]:
            records.append(dict(alias=alias,x=x,z=z,yaw=yaw,scale=scale,burial=0,elevation=0,label=prefix+'_'+name))
    return records


def clear(author,record,others):
    for road in author.layout['roads']:
        if route_margin(author,record,road['a'],road['b'])<=road['half_width']+.37:return False
    box=shape(author,record)
    for other in others:
        if record['label']==other['label']:continue
        if overlap(box,shape(author,other),.018):return False
        if record['alias']in ASSET_ALIASES and other['alias'].startswith(('purchased_house_','castle_barracks','castle_quartermaster')):
            if not separated_bounds(author,record,other,.15):return False
    return True


def apply(author):
    if hasattr(author,'_castle_inner_wing_metadata'):return copy.deepcopy(author._castle_inner_wing_metadata)
    before=copy.deepcopy(author.records);roads=copy.deepcopy(author.layout['roads']);squares=copy.deepcopy(author.layout['squares'])
    assert not any(r['label'].startswith('castle_inner_')for r in before)
    assert not({r['id']for r in roads}&{r['id']for r in ROADS})
    author.layout['roads']+=copy.deepcopy(ROADS)
    resident={r['alias']for r in before};assert {'purchased_house_a','purchased_house_b','castle_yard_bench'}<=resident
    records=specs()
    for record in records:
        assert clear(author,record,author.records),('Castle wing does not fit source volumes',record['label'])
        author.add(record['alias'],record['x'],record['z'],record['yaw'],record['scale'],0,record['label'],0)
    known={r['label']:r for r in author.records}
    # One genuine supported ration/supply item percrate keeps the detail pass
    # bounded and proves actual source triangle contact rather than AABB tops.
    supports=[]
    for site in SITES:
        prefix='castle_inner_'+site['id'];parent=known[prefix+'_cargo']
        child,proof=support(author,dict(alias='purchased_goods_tankard',label=prefix+'_supported_tankard',scale=.90),parent,[0,0],[.92,1.01])
        assert clear(author,child,[r for r in author.records if r['label']!=parent['label']]),('Unsupported wing supply item',child['label'])
        author.add(child['alias'],child['x'],child['z'],child['yaw'],child['scale'],child['burial'],child['label'],child['elevation'])
        author.records[-1]['surface_parent']=parent['label'];supports.append(proof)
    metadata=dict(schema_version=1,title='Valdoa guard barracks and quartermaster wings',
        recipe_file=str(Path(__file__).relative_to(ROOT)),recipe_sha256=sha(Path(__file__)),
        labels=[r['label']for r in author.records[len(before):]],existing_records_preserved=len(before),
        baseline_records_sha256=hashlib.sha256(json.dumps(before,sort_keys=True,separators=(',',':')).encode()).hexdigest(),
        semantic_aliases=ASSET_ALIASES,structures=copy.deepcopy(SITES),inspection_points=VIEWS,surface_supports=supports,ground_roads=ROADS,
        source_model_sha256={alias:sha(MODELS/(stem+'.glb'))for alias,stem in ASSET_ALIASES.items()},
        new_mesh_bytes=0,new_image_texture_policies=0,full_scale_structures=2,
        entry_paths='CPU-clear source-envelope walking approaches, not new interiors; source doors remain closed',
        residential_home_count_unchanged=True)
    author._castle_inner_wing_baseline=before;author._castle_inner_wing_roads=roads;author._castle_inner_wing_squares=squares
    author._castle_inner_wing_metadata=metadata
    return copy.deepcopy(metadata)


def validate(author):
    metadata=author._castle_inner_wing_metadata;before=author._castle_inner_wing_baseline
    assert baseline_equal(author.records[:len(before)],before),'Castle wing changed earlier scene records'
    old_roads=author._castle_inner_wing_roads
    assert author.layout['roads'][:len(old_roads)]==old_roads and author.layout['roads'][len(old_roads):len(old_roads)+len(ROADS)]==ROADS
    assert len({r['id']for r in author.layout['roads']})==len(author.layout['roads'])
    assert author.layout['squares']==author._castle_inner_wing_squares
    known={r['label']:r for r in author.records};assert len(known)==len(author.records)
    assert len(metadata['labels'])==len(set(metadata['labels']))==16
    old_routes=[*zip(ROUTE,ROUTE[1:]),*zip(author.layout['demo_route_xz'],author.layout['demo_route_xz'][1:]),
        *source_routes('world_village_population.cpp'),*source_routes('world_village_pets.cpp')]
    minimum=math.inf;sourcechecks=0
    for label in metadata['labels']:
        r=known[label]
        excludes={label,r.get('surface_parent')}|{v['label']for v in author.records if v.get('surface_parent')==label}
        assert clear(author,r,[v for v in author.records if v['label']not in excludes]),('Wing source overlap',label)
        for a,b in old_routes:
            margin=route_margin(author,r,a,b);minimum=min(minimum,margin);assert margin>.8,(label,'Existing route blocked',margin)
        sourcechecks+=len(author.records)-len(excludes-{None})
    from check_castle_demo import route_clearance,collision_boxes,drawbridge_collision_boxes
    pieces=collision_boxes(ROOT/'engine/world_village_colliders.inc');pieces['castle_drawbridge']=drawbridge_collision_boxes(ROOT/'engine/world_castle_drawbridge_colliders.inc')
    models={r['alias']:dict(bounds_y_up=author.model(r['alias'],True))for r in author.records}
    paths=[]
    for site in SITES:
        record=known['castle_inner_'+site['id']]
        assert record['scale']==1 and record['alias']==site['alias']
        assert author.model(site['alias'])==author.model(site['source_alias'])
        assert sha(MODELS/(ASSET_ALIASES[site['alias']]+'.glb'))==metadata['source_model_sha256'][site['alias']]
        proof=route_clearance(author.records,models,site['entry_path_xz'],pieces)
        paths.append(dict(id=site['id'],local_xz=site['entry_path_xz'],length_m=sum(math.dist(a,b)for a,b in zip(site['entry_path_xz'],site['entry_path_xz'][1:])),**proof))
    for proof in metadata['surface_supports']:
        child=known[proof['label']];parent=known[proof['parent']]
        assert child['surface_parent']==parent['label']and len(proof['actual_surface_samples_m'])==5
        assert max(proof['actual_surface_samples_m'])-min(proof['actual_surface_samples_m'])<.025
    return dict(instances=16,full_scale_structures=2,source_volume_checks=sourcechecks,
        existing_records_preserved=len(before),minimum_full_source_distance_from_old_routes_m=minimum,
        entry_paths=paths,paved_entry_roads=2,supported_supply_items=2,real_source_support_rays=10,
        new_mesh_bytes=0,new_image_texture_policies=0,residential_home_count_unchanged=True)


def test(author):
    proof=validate(author);before=copy.deepcopy(author.records);metadata=apply(author)
    assert apply(author)==metadata and before==author.records
    bad=copy.copy(author);bad.records=copy.deepcopy(author.records)
    next(r for r in bad.records if r['label']=='castle_inner_barracks')['x']=0
    try:validate(bad)
    except AssertionError:pass
    else:raise AssertionError('Wing over the actual keep accepted')
    rays=0
    with tempfile.TemporaryDirectory(prefix='valwood-castle-inner-wing-')as tmp:
        atlas=Atlas(tmp)
        for label in metadata['labels']:
            r=next(r for r in author.records if r['label']==label)
            if r.get('surface_parent'):continue
            box=shape(author,r)
            for x,z in[(r['x'],r['z']),(box[8],box[10]),(box[8],box[11]),(box[9],box[10]),(box[9],box[11])]:
                assert abs(atlas.sample(ORIGIN[0]+x,ORIGIN[1]+z).height-24)<1e-4,(label,'Actual CPU source base unsupported');rays+=1
    proof.update(idempotent=True,keep_overlap_negative_control=True,actual_cpu_terrain_rays=rays)
    return proof


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--stage',action='store_true');p.add_argument('--test',action='store_true');a=p.parse_args();assert a.stage or a.test
    author=Author(json.loads(PATH.read_text()));author.compose();metadata=apply(author);proof=test(author)if a.test else validate(author)
    if a.stage:
        OUTPUT.mkdir(parents=True,exist_ok=True);(OUTPUT/'stage.json').write_text(json.dumps(dict(metadata=metadata,validation=proof,
            placements=[r for r in author.records if r['label']in metadata['labels']]),indent=2)+'\n')
    print(json.dumps(proof,indent=2))
if __name__=='__main__':main()
