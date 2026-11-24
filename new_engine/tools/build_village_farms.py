#!/usr/bin/env python3
"""Append-only corn/wheat farms on source-measured open town land.

The standalone CLI stages records and proof only. The common scene builder
owns publication. Crop clusters are stable authored rows, not per-stalk scene
records; all existing homes, roads, moat, torches and actor routes remain.
"""
from pathlib import Path
import argparse,copy,hashlib,json,math,sys,tempfile
from build_castle_village import Author,PATH,ROOT,MODELS,ALIASES,ROUTE,ORIGIN
from build_village_demo_dressing import shape,overlap,route_margin,baseline_equal,support,base_y
from build_village_hero_grove import source_routes
from build_starting_area import Atlas

OUTPUT=ROOT/'docs/performance/village-farms'
ASSET_MANIFEST=MODELS/'village_crops_manifest.json'
ASSET_ALIASES={f'village_crop_{crop}':f'village_crop_{crop}_near'for crop in ['corn','wheat']}
WIND_GUARD=.45
PLOTS=[dict(id='corn',alias='village_crop_corn',bounds_xz=[109,-184,136,-140],
            centres_x=[112,116,120,124,128,132],row_spacing_m=1,row_phase_m=.5),
       dict(id='wheat',alias='village_crop_wheat',bounds_xz=[147,-184,174,-140],
            centres_x=[150,154,158,162,166,170],row_spacing_m=.25,row_phase_m=.125)]
ROW_Z=list(range(-144,-181,-4))
ROADS=[dict(id='farm_approach',a=[95,-75],b=[95,-136],half_width=2.2,paving=.30),
       dict(id='farm_south_headland',a=[95,-136],b=[178,-136],half_width=2.2,paving=.30),
       dict(id='farm_central_aisle',a=[141,-136],b=[141,-187],half_width=2,paving=.18),
       dict(id='farm_north_headland',a=[106,-187],b=[178,-187],half_width=1.7,paving=.18),
       dict(id='farm_east_return',a=[178,-187],b=[178,-136],half_width=1.7,paving=.18),
       dict(id='farm_corn_side',a=[106,-136],b=[106,-187],half_width=1.4,paving=.18)]
WALK=[[141,-136],[141,-187],[178,-187],[178,-136],[141,-136]]
APPROACH=[[95,-75],[95,-136],[141,-136]]
VIEWS=[dict(id='castle-farms',position_xz=[141,-136],target_xz=[141,-166],pitch=-.04),
       dict(id='castle-farms-reverse',position_xz=[178,-187],target_xz=[128,-155],pitch=-.035)]

def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def register(author):
    owner=sys.modules[type(author).__module__]
    owner.ALIASES.update(ASSET_ALIASES);ALIASES.update(ASSET_ALIASES)
    owner.PLANTS.update(ASSET_ALIASES);owner.NON_SOLID.update(ASSET_ALIASES)

def inventory():
    assert ASSET_MANIFEST.is_file(),'Author the actual corn/wheat source models first'
    data=json.loads(ASSET_MANIFEST.read_text());rows={}
    for row in data['assets']:
        assert row['alias']in['corn','wheat']and row['lod']in['near','mid']
        assert sha(MODELS/row['file'])==row['sha256'],'Crop source model changed'
        assert row['cluster_footprint_m']==[4,4]and 0<=row['wind_reach_m']<=WIND_GUARD
        limit=1.8001 if row['alias']=='corn'else 1.9501
        assert max(abs(row['bounds_min'][i])for i in[0,2])<=limit
        assert max(abs(row['bounds_max'][i])for i in[0,2])<=limit
        rows[row['alias'],row['lod']]=row
    assert set(rows)=={(crop,lod)for crop in['corn','wheat']for lod in['near','mid']}
    for plot in PLOTS:
        near,mid=rows[plot['id'],'near'],rows[plot['id'],'mid']
        assert near['plant_roots_sha256']==mid['plant_roots_sha256']and near['plant_count']==mid['plant_count']
        assert near['plant_count']==(20 if plot['id']=='corn'else 352),'Unexpected final crop planting density'
        assert all(near['bounds_min'][i]<=mid['bounds_min'][i]+1e-7 and near['bounds_max'][i]>=mid['bounds_max'][i]-1e-7 for i in range(3)), 'Mid source escaped proved near envelope'
        for row in[near,mid]:
            assert row['row_spacing_m']==plot['row_spacing_m']and row['row_phase_x_m']==plot['row_phase_m'],'Actual crop rows differ from cultivated furrows'
    return data,rows

def plant_population(assets):
    return {crop:dict(clusters=60,plants_per_cluster=assets[crop,'near']['plant_count'],
        total_plants=60*assets[crop,'near']['plant_count'],root_sha256=assets[crop,'near']['plant_roots_sha256'])for crop in['corn','wheat']}

def crop_specs():
    result=[]
    for plot in PLOTS:
        for col,x in enumerate(plot['centres_x']):
            for row,z in enumerate(ROW_Z):
                result.append(dict(alias=plot['alias'],x=x,z=z,yaw=math.pi*((col+row)%2),scale=1,
                    burial=0,elevation=0,label=f"village_farm_{plot['id']}_{col}_{row}",farm_plot=plot['id']))
    return result

def detail_specs():
    result=[]
    def add(alias,x,z,label,yaw=0,scale=1):
        result.append(dict(alias=alias,x=x,z=z,yaw=yaw,scale=scale,burial=0,elevation=0,label='village_farm_'+label))
    # Short weathered rail runs define the plots; the broad central gateway
    # and side paths remain open. No artificial fence crosses the public lane.
    for crop,start,side_x in [('corn',111.35,108.3),('wheat',147.65,175.4)]:
        for i in range(14):add('castle_training_fence',start+1.86*i,-139.2,f'{crop}_front_fence_{i}')
        for i in range(5):add('castle_training_fence',start+1.86*i,-184.6,f'{crop}_back_fence_{i}')
        for i in range(3):add('castle_training_fence',side_x,-142-1.86*i,f'{crop}_side_fence_{i}',math.pi/2)
    for alias,x,z,label,yaw,scale in [
        ('village_cart',102,-146,'corn_cart',math.pi/2,1),('village_cart',181.5,-147,'wheat_cart',-math.pi/2,1),
        ('purchased_crate',102,-151,'corn_crate',.04,.95),('purchased_crate',181.5,-152,'wheat_crate',-.08,.95),
        ('purchased_barrel',101.5,-153,'corn_barrel',.12,.92),('purchased_barrel',182,-154.2,'wheat_barrel',-.12,.92),
        ('purchased_prop_sack',100.8,-149,'corn_seed_sack',.24,.90),('purchased_prop_sack',182.2,-150.3,'wheat_seed_sack',-.18,.92),
        ('castle_yard_bench',181.5,-171,'resting_bench',math.pi/2,1),('purchased_prop_logs',101.5,-159,'wood_store',.12,.8),
        ('purchased_prop_barrel_small',101.5,-161,'water_barrel',.1,1),('purchased_crate',182,-157,'harvest_crate',.14,.85)]:
        add(alias,x,z,label,yaw,scale)
    return result

def expanded_shape(author,record):
    box=list(shape(author,record))
    if record['alias']in ASSET_ALIASES:
        box[2]+=WIND_GUARD;box[3]+=WIND_GUARD
        for i in[8,10]:box[i]-=WIND_GUARD
        for i in[9,11]:box[i]+=WIND_GUARD
    return tuple(box)

def margin(author,record,a,b):
    return route_margin(author,record,a,b)-(WIND_GUARD if record['alias']in ASSET_ALIASES else 0)

def add_record(author,record):
    author.add(record['alias'],record['x'],record['z'],record['yaw'],record['scale'],record['burial'],record['label'],record['elevation'])
    author.records[-1].update({k:v for k,v in record.items()if k not in author.records[-1]})
    return author.records[-1]

def cultivated_ground():
    return [dict(id='farm_'+p['id'],crop=p['id'],bounds_xz=p['bounds_xz'],surface='tilled earth',row_axis='Z',
        row_spacing_m=p['row_spacing_m'],row_phase_x_m=p['row_phase_m'])for p in PLOTS]

def include(metadata):
    lines=['/* Generated cultivated plots. Bounds in village-local XZ, rows along Z. */',
           'static const WorldVillageFarmField world_village_farm_fields[]={']
    for row in metadata['cultivated_ground']:
        values=[*row['bounds_xz'],row['row_spacing_m'],row['row_phase_x_m']]
        lines.append('    {'+','.join(format(v,'.12g')for v in values)+'},')
    return '\n'.join(lines+['};',''])

def apply(author):
    if hasattr(author,'_village_farms_metadata'):return copy.deepcopy(author._village_farms_metadata)
    register(author);data,assets=inventory();before=copy.deepcopy(author.records)
    assert len(before)==2459 and not any(r['label'].startswith('village_farm_')for r in before),'Unexpected farm baseline'
    before_roads=copy.deepcopy(author.layout['roads']);assert not({r['id']for r in before_roads}&{r['id']for r in ROADS})
    author.layout['roads']+=copy.deepcopy(ROADS)
    for record in crop_specs()+detail_specs():add_record(author,record)
    known={r['label']:r for r in author.records};supported=[]
    for crop in['corn','wheat']:
        parent=known[f'village_farm_{crop}_cart']
        for alias,local,name,scale in [('purchased_prop_sack',[-.75,0],'harvest_sack',.85),
                                      ('starting_bucket',[.70,0],'bucket',.85)]:
            child,proof=support(author,dict(alias=alias,label=f'village_farm_{crop}_cart_{name}',scale=scale),parent,local,[.25,1.35])
            add_record(author,child);supported.append(proof)
    # A real tool rests on the crate's measured wooden top.
    child,proof=support(author,dict(alias='starting_axe',label='village_farm_corn_tool',scale=.9),
                        known['village_farm_corn_crate'],[0,0],[.96,1.01])
    add_record(author,child);supported.append(proof)
    added=copy.deepcopy(author.records[len(before):]);labels=[r['label']for r in added]
    metadata=dict(schema_version=1,recipe_file=str(Path(__file__).relative_to(ROOT)),recipe_sha256=sha(Path(__file__)),
        labels=labels,existing_records_preserved=len(before),
        baseline_records_sha256=hashlib.sha256(json.dumps(before,sort_keys=True,separators=(',',':')).encode()).hexdigest(),
        existing_road_count=len(before_roads),baseline_roads_sha256=hashlib.sha256(json.dumps(before_roads,sort_keys=True,separators=(',',':')).encode()).hexdigest(),
        aliases=ASSET_ALIASES,asset_manifest=str(ASSET_MANIFEST.relative_to(ROOT)),asset_manifest_sha256=sha(ASSET_MANIFEST),
        generated_include='engine/world_village_farms.inc',source_budget=data['budget'],
        source_provenance_file=data['provenance'],source_provenance_sha256=sha(ROOT/data['provenance']),
        source_recipe_file=data['recipe'],source_recipe_sha256=sha(ROOT/data['recipe']),
        model_sha256={r['file']:r['sha256']for r in data['assets']},plots=copy.deepcopy(PLOTS),
        reused_model_sha256={r['alias']:sha(MODELS/(ALIASES[r['alias']]+'.glb'))for r in added if r['alias']not in ASSET_ALIASES},
        cultivated_ground=cultivated_ground(),
        wind_horizontal_guard_m=WIND_GUARD,ground_roads=copy.deepcopy(ROADS),walk_xz=copy.deepcopy(WALK),
        approach_xz=copy.deepcopy(APPROACH),walking_route_xz=copy.deepcopy(WALK),views=copy.deepcopy(VIEWS),surface_supports=supported,
        gateway=dict(bounds_xz=[136.5,-142,146.5,-136],clear_width_m=10,open=True),
        crop_instances=120,corn_instances=60,wheat_instances=60,fence_instances=44,detail_instances=len(added)-120,
        source_plant_population=plant_population(assets),
        inside_outer_town_walls=True,outside_inner_castle=True,old_routes_changed=False,old_population_changed=False,
        crop_collision='none; stable source clusters with full source+wind route clearance')
    author._village_farms_baseline=before;author._village_farms_before_roads=before_roads
    author._village_farms_expected=added;author._village_farms_metadata=metadata
    return copy.deepcopy(metadata)

def validate(author):
    metadata=author._village_farms_metadata;before=author._village_farms_baseline
    assert baseline_equal(author.records[:len(before)],before),'Existing town records changed'
    assert baseline_equal(author.records[len(before):len(before)+len(metadata['labels'])],author._village_farms_expected),'Farm authored transforms changed'
    old_roads=author._village_farms_before_roads
    assert author.layout['roads'][:len(old_roads)]==old_roads and author.layout['roads'][len(old_roads):len(old_roads)+len(ROADS)]==ROADS,'Farm public paths changed'
    assert len({r['id']for r in author.layout['roads']})==len(author.layout['roads'])
    assert metadata['plots']==PLOTS and metadata['walk_xz']==WALK and metadata['ground_roads']==ROADS
    assert metadata['walking_route_xz']==WALK and metadata['cultivated_ground']==cultivated_ground() and metadata['views']==VIEWS
    assert metadata['wind_horizontal_guard_m']==WIND_GUARD and metadata['crop_instances']==120
    data,assets=inventory();assert metadata['asset_manifest_sha256']==sha(ASSET_MANIFEST)
    assert metadata['source_plant_population']==plant_population(assets)
    assert metadata['model_sha256']=={r['file']:r['sha256']for r in data['assets']}
    assert metadata['reused_model_sha256']=={r['alias']:sha(MODELS/(ALIASES[r['alias']]+'.glb'))for r in author._village_farms_expected if r['alias']not in ASSET_ALIASES}
    known={r['label']:r for r in author.records};assert len(known)==len(author.records)
    added=[known[label]for label in metadata['labels']];source_checks=road_checks=route_checks=0
    paths=[*zip(ROUTE,ROUTE[1:]),*zip(author.layout['demo_route_xz'],author.layout['demo_route_xz'][1:])]
    from build_village_market_hero import WALK as HERO_WALK
    from build_castle_expansion import WALK as CASTLE_WALK
    for points in[HERO_WALK,CASTLE_WALK]:paths+=list(zip(points,points[1:]))
    paths+=source_routes('world_village_population.cpp')+source_routes('world_village_pets.cpp')
    for r in added:
        box=expanded_shape(author,r)
        assert box[8]>80 and box[9]<195 and box[10]>-195 and box[11]<-131,'Farm escaped safe town land'
        for road in author.layout['roads']:
            assert margin(author,r,road['a'],road['b'])>road['half_width']+.37,(r['label'],'Public road obstructed',road['id'])
            road_checks+=1
        for a,b in paths:
            assert margin(author,r,a,b)>.75,(r['label'],'Protected character/demo route obstructed');route_checks+=1
        for other in author.records:
            if other['label']==r['label']or other['label']==r.get('surface_parent')or other.get('surface_parent')==r['label']:continue
            if r['alias']in ASSET_ALIASES and other['alias']in ASSET_ALIASES:continue
            assert not overlap(box,expanded_shape(author,other),.018),(r['label'],'Source volume overlap',other['label'])
            source_checks+=1
        if r['alias']in ASSET_ALIASES:
            p=next(p for p in PLOTS if p['alias']==r['alias']);xmin,zmin,xmax,zmax=p['bounds_xz']
            assert xmin<=box[8]<=box[9]<=xmax and zmin<=box[10]<=box[11]<=zmax,'Source+wind escapes cultivated plot'
    # New paths are checked against every OLD source footprint, including
    # flowers/ferns, so preserving records cannot leave old weeds in the path.
    minimum_old_path=math.inf
    for road in ROADS:
        for r in before:
            value=route_margin(author,r,road['a'],road['b'])-road['half_width']-.37
            assert value>0,(road['id'],'Existing source occupies new farm path',r['label'])
            minimum_old_path=min(minimum_old_path,value);road_checks+=1
    for points in[WALK,APPROACH]:
        for a,b in zip(points,points[1:]):
            assert math.dist(a,b)>0
            for r in author.records:
                assert margin(author,r,a,b)>.52,('Farm inspection/approach obstructed',r['label']);route_checks+=1
    for proof in metadata['surface_supports']:
        child,parent=known[proof['label']],known[proof['parent']]
        actual=[author.surface(parent['alias'],x,z,.25,1.35)for x,z in proof['actual_footprint_xz']]
        assert max(abs(a-b)for a,b in zip(actual,proof['actual_surface_samples_m']))<1e-6
        assert child['surface_parent']==parent['label'] and abs(child['elevation']-proof['ground_elevation_m'])<1e-6
    return dict(instances=len(added),corn_clusters=60,wheat_clusters=60,corn_stalks=assets['corn','near']['plant_count']*60,
        wheat_stems=assets['wheat','near']['plant_count']*60,plots=2,cultivated_area_m2=2376,
        existing_records_preserved=len(before),fence_sections=44,supported_items=5,source_support_rays=25,
        source_volume_checks=source_checks,road_checks=road_checks,protected_route_checks=route_checks,
        minimum_old_source_clearance_from_new_paths_m=minimum_old_path,
        inspection_walk_length_m=sum(math.dist(a,b)for a,b in zip(WALK,WALK[1:])),
        approach_length_m=sum(math.dist(a,b)for a,b in zip(APPROACH,APPROACH[1:])),
        full_source_and_wind_clear=True,inside_town_outside_castle=True,old_population_and_routes_preserved=True)

def test(author):
    proof=validate(author);before=copy.deepcopy(author.records);metadata=apply(author)
    assert apply(author)==metadata and author.records==before
    terrain_rays=0
    with tempfile.TemporaryDirectory(prefix='valwood-farm-ground-')as temp:
        atlas=Atlas(temp)
        for plot in PLOTS:
            xmin,zmin,xmax,zmax=plot['bounds_xz']
            for x in range(xmin,xmax+1):
                for z in range(zmin,zmax+1):
                    assert abs(atlas.sample(ORIGIN[0]+x,ORIGIN[1]+z).height-24)<1e-4,'Cultivated ground is not flat dry source terrain';terrain_rays+=1
        for r in author.records[len(author._village_farms_baseline):]:
            if r.get('surface_parent'):continue
            box=expanded_shape(author,r)
            for x,z in[(r['x'],r['z']),(box[8],box[10]),(box[8],box[11]),(box[9],box[10]),(box[9],box[11])]:
                assert abs(atlas.sample(ORIGIN[0]+x,ORIGIN[1]+z).height-24)<1e-4,'Farm source footing is not dry supported ground';terrain_rays+=1
    proof.update(idempotent=True,actual_terrain_rays=terrain_rays)
    return proof

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--stage',action='store_true');parser.add_argument('--test',action='store_true')
    args=parser.parse_args();assert args.stage or args.test
    author=Author(json.loads(PATH.read_text()));author.compose();metadata=apply(author);proof=test(author)if args.test else validate(author)
    if args.stage:
        OUTPUT.mkdir(parents=True,exist_ok=True)
        (OUTPUT/'stage.json').write_text(json.dumps(dict(metadata=metadata,validation=proof,placements=author._village_farms_expected),indent=2)+'\n')
    print(json.dumps(proof,indent=2))
if __name__=='__main__':main()
