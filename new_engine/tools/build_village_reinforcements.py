#!/usr/bin/env python3
"""Append measured hand tools, two mounted guards and four runtime watch posts.

The existing farm/town scene remains an exact prefix. Animated watch guards are
generated runtime stations, never duplicate static scenery records. This file
authors data and CPU geometry proofs; it does not launch the engine or GPU.
"""
from pathlib import Path
import argparse,copy,hashlib,json,math,sys,tempfile
from build_castle_village import Author,PATH,ROOT,MODELS,ALIASES,NON_SOLID,ROUTE,ORIGIN
from build_village_demo_dressing import shape,overlap,route_margin,baseline_equal,support,base_y
from build_village_hero_grove import source_routes
from build_starting_area import glb_bounds,Atlas

OUTPUT=ROOT/'docs/performance/village-reinforcements'
BASELINE=OUTPUT/'before'
TOOL_MANIFEST=MODELS/'village_toolkit_manifest.json'
HORSE_MANIFEST=MODELS/'village_horseman_manifest.json'
WATCH_MANIFEST=MODELS/'village_shield_knight_manifest.json'
PREFIX='village_reinforcement_'
TOOLS=['axe','chisel','claw_hammer','crowbar','firepoker','hatchet','hoe','machete','pickaxe','pitchfork','scythe','shovel']
TOOL_ALIASES={f'village_toolkit_{n}':f'village_toolkit_{n}_near'for n in TOOLS}
ASSET_ALIASES={**TOOL_ALIASES,'village_horseman':'village_horseman_near'}
STATIONS=[dict(x=-8.5,z=-48,yaw=0,phase=0),dict(x=8.5,z=-48,yaw=0,phase=.27),
          dict(x=-10.5,z=193,yaw=math.pi,phase=.53),dict(x=10.5,z=193,yaw=math.pi,phase=.79)]
VIEWS=[dict(id='castle-watch',position_xz=[0,177],target_xz=[0,202],pitch=.03),
       dict(id='castle-farm-tools',position_xz=[106,-166],target_xz=[101.5,-169],pitch=-.18),
       dict(id='castle-watch-close',position_xz=[7.5,188],target_xz=[10.5,193],pitch=-.10)]

def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def canonical_hash(value):return hashlib.sha256(json.dumps(value,sort_keys=True,separators=(',',':')).encode()).hexdigest()

def register(author):
    owner=sys.modules[type(author).__module__]
    owner.ALIASES.update(ASSET_ALIASES);ALIASES.update(ASSET_ALIASES)
    owner.NON_SOLID.update(TOOL_ALIASES);NON_SOLID.update(TOOL_ALIASES)

def inventory():
    data={};hashes={};bounds={}
    for kind,path in [('tools',TOOL_MANIFEST),('horseman',HORSE_MANIFEST),('watch',WATCH_MANIFEST)]:
        value=json.loads(path.read_text());data[kind]=dict(file=str(path.relative_to(ROOT)),sha256=sha(path))
        if kind=='watch':assert value['pose_count']==8 and value['all_adjacent_pose_topology_material_exact']and len(value['assets'])==8
        for row in value['assets']:
            p=MODELS/row['file'];assert sha(p)==row['sha256'],('New source model changed',row['file'])
            low,high=glb_bounds(p);bounds[row['file']]=[low,high];hashes[row['file']]=row['sha256']
        if kind=='tools':
            assert len(value['assets'])==24 and {r['alias']for r in value['assets']}==set(TOOLS)
            assert value['budget']['unique_images']==3 and value['budget']['estimated_rgba8_mips_mib']<=16.01
    watch=json.loads(WATCH_MANIFEST.read_text())
    union=[[min(b[k][i]for f,b in bounds.items()if f.startswith('village_shield_knight_idle_'))if k==0 else
            max(b[k][i]for f,b in bounds.items()if f.startswith('village_shield_knight_idle_'))for i in range(3)]for k in range(2)]
    assert all(abs(union[k][i]-watch[['bounds_min','bounds_max'][k]][i])<1e-5 for k in range(2)for i in range(3))
    return data,hashes,union,watch['cycle_seconds']

def paths(author):
    from build_village_market_hero import WALK as HERO
    from build_castle_expansion import WALK as CASTLE
    from build_village_farms import WALK as FARM,APPROACH
    result=[]
    for points in[ROUTE,author.layout['demo_route_xz'],HERO,CASTLE,FARM,APPROACH]:result+=list(zip(points,points[1:]))
    return result+source_routes('world_village_population.cpp')+source_routes('world_village_pets.cpp')

def expanded_shape(author,r):
    if r['alias']in {'village_crop_corn','village_crop_wheat'}:
        from build_village_farms import expanded_shape as crop_shape
        return crop_shape(author,r)
    return shape(author,r)

def add(author,alias,x,z,label,yaw=0,scale=1,**extra):
    r=dict(alias=alias,x=x,z=z,yaw=yaw,scale=scale,burial=0,elevation=0,label=PREFIX+label,**extra)
    author.add(alias,x,z,yaw,scale,0,r['label']);author.records[-1].update(r);return author.records[-1]

def add_supported(author,name,parent,local,label,yaw=0):
    r,proof=support(author,dict(alias='village_toolkit_'+name,label=PREFIX+label,scale=1,yaw_degrees=math.degrees(yaw)),parent,local,
                    [.45,.80]if parent['alias']in {'starting_table','castle_yard_bench'}else [.3,1.2])
    proof['surface_range_m']=[.45,.80]if parent['alias']in {'starting_table','castle_yard_bench'}else [.3,1.2]
    author.add(r['alias'],r['x'],r['z'],r['yaw'],1,0,r['label'],r['elevation']);author.records[-1].update(r)
    return proof

def triangles(author,alias):
    if alias not in author.triangles:author.triangles[alias]=glb_bounds(MODELS/(ALIASES[alias]+'.glb'),True)[3]
    return author.triangles[alias]

def z_surface(author,alias,x,y,side='min'):
    hits=[]
    for a,b,c in triangles(author,alias):
        ax,ay=b[0]-a[0],b[1]-a[1];bx,by=c[0]-a[0],c[1]-a[1];d=ax*by-ay*bx
        if abs(d)<1e-12:continue
        u=((x-a[0])*by-(y-a[1])*bx)/d;v=(ax*(y-a[1])-ay*(x-a[0]))/d
        if u>=-1e-6 and v>=-1e-6 and u+v<=1.000001:hits.append(a[2]+u*(b[2]-a[2])+v*(c[2]-a[2]))
    assert hits,('Missing real fence contact',alias,x,y)
    return (min if side=='min'else max)(hits)

def tool_forward_slice(author,alias,y):
    points=[]
    for face in triangles(author,alias):
        for a,b in zip(face,face[1:]+face[:1]):
            if abs(a[1]-b[1])<1e-12:continue
            t=(y-a[1])/(b[1]-a[1])
            if 0<=t<=1:points.append([a[i]+t*(b[i]-a[i])for i in range(3)])
    assert points,('Missing tool contact slice',alias,y)
    return max(points,key=lambda p:p[2])

def add_lean(author,name,parent,label):
    alias='village_toolkit_'+name
    # Fence centre is an actual horizontal rail (topY.845). Contact just below
    # that rail's top lets taller handles lean over it without a bounding-box
    # false collision. The positive-Z tool surface meets its north face.
    rail_top=author.surface(parent['alias'],0,0,.5,1.1);height=rail_top-.006
    contact=tool_forward_slice(author,alias,height)
    face=z_surface(author,parent['alias'],contact[0],height)
    gap=.006;z=parent['z']+face-contact[2]-gap
    r=add(author,alias,parent['x'],z,label,surface_parent=parent['label'],support_kind='ground_and_fence')
    return dict(label=r['label'],parent=parent['label'],tool_source_contact_xyz=contact,
        parent_source_contact_xyz=[contact[0],height,face],contact_gap_m=gap,ground_base_y=0,
        contact_height_m=height,source_rail_top_m=rail_top)

def include(metadata):
    def f(v):
        s=format(v,'.9g');return s+('f'if any(k in s for k in '.eE')else'.0f')
    lines=['/* Generated watch stations; village-local XZ metres, fixed roots and source-facing yaw. */',
        'static constexpr WorldVillageWatchStation world_village_watch_stations[]={']
    for s in metadata['watch_stations']:lines.append('    {'+','.join([format(s['x'],'.12g'),format(s['z'],'.12g'),f(s['yaw']),f(s['phase'])])+'},')
    return '\n'.join(lines+['};','static constexpr float world_village_watch_cycle_seconds='+f(metadata['watch_cycle_seconds'])+';',''])

def apply(author):
    if hasattr(author,'_reinforcements_metadata'):return copy.deepcopy(author._reinforcements_metadata)
    register(author);manifests,model_hashes,union,cycle=inventory();before=copy.deepcopy(author.records);roads=copy.deepcopy(author.layout['roads'])
    assert len(before)==2640 and len(roads)==24 and not any(r['label'].startswith(PREFIX)for r in before)
    known={r['label']:r for r in author.records};supports=[];lean=[]
    for crop,x,z in [('corn',101.5,-169),('wheat',182,-177)]:
        table=add(author,'starting_table',x,z,crop+'_worktable')
        for name,local,yaw in [('axe',[-.45,-.45],0),('machete',[.45,-.45],0),('claw_hammer',[-.25,.65],math.pi/2),
                               ('hatchet',[.35,.70],math.pi/2),('chisel',[0,.15],math.pi/2)]:
            supports.append(add_supported(author,name,table,local,crop+'_table_'+name,yaw))
        bench=add(author,'castle_yard_bench',x,-174 if crop=='corn'else-181,crop+'_bench')
        add(author,'purchased_crate',x,-176 if crop=='corn'else-174,crop+'_cargo')
        for name,local in [('hatchet',[-.55,.15]),('chisel',[.5,.15])]:
            supports.append(add_supported(author,name,bench,local,crop+'_bench_'+name,math.pi/2))
        for i,name in enumerate(['hoe','pitchfork','shovel','scythe','pickaxe','crowbar','firepoker']):
            parent=known[f'village_farm_{crop}_front_fence_{i*2}']
            lean.append(add_lean(author,name,parent,crop+'_fence_'+name))
    for name,parent,label in [('chisel','demo_workshop_purchased_crate_5','workshop_chisel'),
                              ('claw_hammer','castle_expansion_armory_crate_a','armory_hammer')]:
        supports.append(add_supported(author,name,known[parent],[0,0],label,math.pi/2))
    for side,x in [('west',-10.5),('east',10.5)]:add(author,'village_horseman',x,201,'mounted_'+side,math.pi,guard_role='static_mounted_gate_watch')
    added=copy.deepcopy(author.records[len(before):])
    metadata=dict(schema_version=1,recipe_file=str(Path(__file__).relative_to(ROOT)),recipe_sha256=sha(Path(__file__)),
        labels=[r['label']for r in added],existing_records_preserved=len(before),baseline_records_sha256=canonical_hash(before),
        existing_road_count=len(roads),baseline_roads_sha256=canonical_hash(roads),aliases=ASSET_ALIASES,
        existing_view_count=len(author.layout['demo_views']),baseline_views_sha256=canonical_hash(author.layout['demo_views']),
        baseline_demo_route_sha256=canonical_hash(author.layout['demo_route_xz']),
        source_manifests=manifests,model_sha256=model_hashes,
        reused_model_sha256={r['alias']:sha(MODELS/(ALIASES[r['alias']]+'.glb'))for r in added if r['alias']not in ASSET_ALIASES},
        surface_supports=supports,lean_supports=lean,watch_stations=copy.deepcopy(STATIONS),watch_cycle_seconds=cycle,
        watch_union_bounds_yup=union,watch_pose_count=8,generated_include='engine/world_village_watch.inc',
        views=copy.deepcopy(VIEWS),tools=30,mounted_guards=2,animated_watch_guards=4,new_furniture=6,
        old_population_unchanged=True,old_routes_unchanged=True,animated_guards_have_no_static_scenery_duplicates=True,
        watch_plaza_exception='Only the two castle-front fixed watch posts may occupy the broad castle_court paving cap; crown_avenue and all actor/walk corridors remain source-envelope clear.',
        earlier_layer_support_policy='Later tool attachments with reproduced triangle contacts are omitted only from the earlier expansion parent-AABB pass. Complete support and all other source-volume checks remain mandatory in this layer.',
        source_route_sha256={name:sha(ROOT/'engine'/name)for name in ['world_village_population.cpp','world_village_pets.cpp']})
    author._reinforcements_baseline=before;author._reinforcements_roads=roads
    author._reinforcements_expected=added;author._reinforcements_metadata=metadata
    return copy.deepcopy(metadata)

def earlier_expansion_view(author):
    """Avoid treating a real recessed crate top as solid up to its rim AABB."""
    meta=author._reinforcements_metadata;known={r['label']:r for r in author.records};omitted=set()
    for proof in meta['surface_supports']:
        if not proof['parent'].startswith('castle_expansion_'):continue
        child,parent=known[proof['label']],known[proof['parent']]
        assert child['alias']in TOOL_ALIASES and child['label'].startswith(PREFIX)and child['surface_parent']==parent['label']
        samples=[author.surface(parent['alias'],x,z,*proof['surface_range_m'])for x,z in proof['actual_footprint_xz']]
        assert samples==proof['actual_surface_samples_m']and abs(child['elevation']-proof['ground_elevation_m'])<1e-7
        omitted.add(child['label'])
    view=copy.copy(author);view.records=[r for r in author.records if r['label']not in omitted]
    return view

def validate(author):
    meta=author._reinforcements_metadata;before=author._reinforcements_baseline;expected=author._reinforcements_expected
    assert baseline_equal(author.records[:len(before)],before),'Reinforcements changed existing town records'
    assert baseline_equal(author.records[len(before):],expected),'Reinforcement authored transforms changed'
    assert author.layout['roads']==author._reinforcements_roads and len(author.layout['roads'])==24,'Reinforcements changed existing roads'
    assert meta['baseline_records_sha256']==canonical_hash(before)and meta['baseline_roads_sha256']==canonical_hash(author.layout['roads'])
    assert meta['baseline_views_sha256']==canonical_hash(author.layout['demo_views'][:meta['existing_view_count']])
    assert meta['baseline_demo_route_sha256']==canonical_hash(author.layout['demo_route_xz'])
    manifests,hashes,union,cycle=inventory()
    assert meta['source_manifests']==manifests and meta['model_sha256']==hashes and meta['watch_union_bounds_yup']==union
    assert meta['watch_stations']==STATIONS and meta['watch_cycle_seconds']==cycle and meta['views']==VIEWS
    assert meta['tools']==30 and meta['mounted_guards']==2 and meta['animated_watch_guards']==4 and meta['new_furniture']==6
    assert meta['aliases']==ASSET_ALIASES and meta['watch_pose_count']==8
    assert meta['old_population_unchanged']and meta['old_routes_unchanged']and meta['animated_guards_have_no_static_scenery_duplicates']
    assert meta['reused_model_sha256']=={r['alias']:sha(MODELS/(ALIASES[r['alias']]+'.glb'))for r in expected if r['alias']not in ASSET_ALIASES}
    assert meta['source_route_sha256']=={name:sha(ROOT/'engine'/name)for name in ['world_village_population.cpp','world_village_pets.cpp']}
    assert len(expected)==38 and meta['labels']==[r['label']for r in expected]
    known={r['label']:r for r in author.records};assert len(known)==len(author.records)
    added=[known[label]for label in meta['labels']];corridors=paths(author);source_checks=route_checks=road_checks=0
    for r in added:
        box=expanded_shape(author,r)
        for road in author.layout['roads']:
            assert route_margin(author,r,road['a'],road['b'])>road['half_width']+.37,(r['label'],'Reinforcement obstructs road',road['id'])
            road_checks+=1
        for a,b in corridors:
            assert route_margin(author,r,a,b)>.75,(r['label'],'Reinforcement obstructs protected route');route_checks+=1
        for other in author.records:
            if other['label']==r['label']or other['label']==r.get('surface_parent')or other.get('surface_parent')==r['label']:continue
            assert not overlap(box,expanded_shape(author,other),.018),(r['label'],'Reinforcement source volume overlap',other['label'])
            source_checks+=1
        if not r.get('surface_parent')or r.get('support_kind')=='ground_and_fence':
            assert abs(box[6])<.0001,(r['label'],'Reinforcement ground contact changed')
    for proof in meta['surface_supports']:
        child,parent=known[proof['label']],known[proof['parent']]
        actual=[author.surface(parent['alias'],x,z,*proof['surface_range_m'])for x,z in proof['actual_footprint_xz']]
        assert max(abs(a-b)for a,b in zip(actual,proof['actual_surface_samples_m']))<1e-6
        assert child['surface_parent']==parent['label']and abs(child['elevation']-proof['ground_elevation_m'])<1e-6
    for proof in meta['lean_supports']:
        child,parent=known[proof['label']],known[proof['parent']];contact=proof['tool_source_contact_xyz'];face=proof['parent_source_contact_xyz']
        assert child['surface_parent']==parent['label']and child['support_kind']=='ground_and_fence'
        assert max(abs(a-b)for a,b in zip(contact,tool_forward_slice(author,child['alias'],proof['contact_height_m'])))<1e-7
        assert abs(z_surface(author,parent['alias'],face[0],face[1])-face[2])<1e-7
        assert abs(child['x']+contact[0]-parent['x']-face[0])<1e-6
        assert abs(parent['z']+face[2]-(child['z']+contact[2])-proof['contact_gap_m'])<1e-6
    # Linear interpolation of any adjacent source poses stays inside their
    # all-eight-pose AABB union, so these clearance checks cover every phase.
    author.bounds['__watch_union']=union;minimum_watch=math.inf
    for i,s in enumerate(STATIONS):
        r=dict(alias='__watch_union',**s,scale=1,burial=0,elevation=0,label=f'watch_runtime_{i}')
        for road in author.layout['roads']:
            if i<2 and road['id']=='castle_court':continue
            assert route_margin(author,r,road['a'],road['b'])>road['half_width']+.37,('Watch obstructs road',i,road['id']);road_checks+=1
        for a,b in corridors:
            margin=route_margin(author,r,a,b);minimum_watch=min(minimum_watch,margin)
            assert margin>.75,('Watch obstructs protected route',i);route_checks+=1
        for other in author.records:
            assert not overlap(shape(author,r),expanded_shape(author,other),.025),('Watch pose-union source overlap',i,other['label']);source_checks+=1
    author.bounds['__inspection_camera']=[[-.37,0,-.37],[.37,1.75,.37]]
    for camera in VIEWS:
        x,z=camera['position_xz'];r=dict(alias='__inspection_camera',x=x,z=z,yaw=0,scale=1,burial=0,elevation=0,label=camera['id'])
        for other in author.records:
            assert not overlap(shape(author,r),expanded_shape(author,other),.025),('Inspection camera source collision',camera['id'],other['label'])
        for station in STATIONS:
            guard=dict(alias='__watch_union',**station,scale=1,burial=0,elevation=0,label='watch')
            assert not overlap(shape(author,r),shape(author,guard),.025),('Inspection camera watch collision',camera['id'])
    return dict(instances=38,tools=30,tool_types=12,new_furniture=6,mounted_guards=2,animated_watch_guards=4,
        existing_records_preserved=2640,existing_roads_preserved=24,tabletop_supported_tools=len(meta['surface_supports']),
        source_support_rays=len(meta['surface_supports'])*5,fence_supported_tools=len(meta['lean_supports']),
        actual_pose_union_checked=True,all8pose_phases_clear=True,minimum_watch_source_route_clearance_m=minimum_watch,
        source_volume_checks=source_checks,road_checks=road_checks,protected_route_checks=route_checks,
        old_population_and_routes_preserved=True,no_duplicate_animated_guard_scenery=True,source_clear_inspection_cameras=len(VIEWS))

def test(author):
    proof=validate(author);before=copy.deepcopy(author.records);assert apply(author)==author._reinforcements_metadata and author.records==before
    rays=0
    with tempfile.TemporaryDirectory(prefix='valwood-reinforcement-terrain-')as directory:
        atlas=Atlas(directory)
        ground=[r for r in author._reinforcements_expected if not r.get('surface_parent')or r.get('support_kind')=='ground_and_fence']
        ground+=[dict(alias='__watch_union',**s,scale=1,burial=0,elevation=0,label='watch')for s in STATIONS]
        ground+=[dict(alias='__inspection_camera',x=v['position_xz'][0],z=v['position_xz'][1],yaw=0,scale=1,burial=0,elevation=0,label=v['id'])for v in VIEWS]
        for r in ground:
            box=shape(author,r)
            for x,z in[(r['x'],r['z']),(box[8],box[10]),(box[8],box[11]),(box[9],box[10]),(box[9],box[11])]:
                assert abs(atlas.sample(ORIGIN[0]+x,ORIGIN[1]+z).height-24)<1e-4,('Unsupported reinforcement terrain',r['label']);rays+=1
    return dict(proof,idempotent=True,actual_terrain_rays=rays)

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--stage',action='store_true');p.add_argument('--test',action='store_true');args=p.parse_args();assert args.stage or args.test
    author=Author(json.loads(PATH.read_text()));author.compose();metadata=apply(author);proof=test(author)if args.test else validate(author)
    if args.stage:
        OUTPUT.mkdir(parents=True,exist_ok=True)
        (OUTPUT/'stage.json').write_text(json.dumps(dict(metadata=metadata,validation=proof,placements=author._reinforcements_expected),indent=2)+'\n')
    print(json.dumps(proof,indent=2))
if __name__=='__main__':main()
