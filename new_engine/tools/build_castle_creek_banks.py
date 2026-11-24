#!/usr/bin/env python3
"""Four small authored creek-bank pockets using resident rocks and woodland.

Placements use the actual excavated CPU atlas and the native rigid grounding
rule. Whole source bounds protect roads/actors; real source vertices establish
terrain contact and exposed rock surfaces. Existing scene and model bytes are
read-only. No global scatter or renderer density is changed.
"""
from pathlib import Path
import argparse,copy,ctypes as C,hashlib,json,math,sys,tempfile
from build_castle_village import Author,PATH,ROOT,MODELS,ALIASES,NON_SOLID,PURCHASED_BUILDINGS,ORIGIN,ROUTE,separated_bounds
from build_starting_area import Atlas,glb_bounds
from build_village_demo_dressing import shape,overlap,route_margin,baseline_equal
from build_village_hero_grove import source_routes
from build_village_market_lively import TRADERS,OLD_STATIONS,PET_STATIONS,PET_RADII
from build_castle_expansion import TRAINING_STATIONS,WALK as EXPANSION_WALK

OUTPUT=ROOT/'docs/performance/castle-expansion/creek'
ASSET_ALIASES={**{f'rock{i}':f'cinematic_rock_{i}'for i in range(3)},
    'deadwood':'cinematic_deadwood',**{f'fern{i}':f'woodland_fern_{i}'for i in range(3)}}
POCKETS=[dict(id='east_middle',side=1,kind='middle',bounds_xz=[207.5,-127,217,-103]),
         dict(id='west_middle',side=-1,kind='middle',bounds_xz=[-217,-127,-207.5,-103]),
         dict(id='east_spring',side=1,kind='spring',bounds_xz=[207.5,-190,217,-170]),
         dict(id='west_spring',side=-1,kind='spring',bounds_xz=[-217,-190,-207.5,-170])]
MIDDLE=[('rock0',213,-106.5,.38,.2),('rock1',211.3,-109.6,.40,.8),
 ('rock2',213.8,-113,.37,-.4),('rock0',211.5,-117,.45,1.7),
 ('rock1',214,-120.2,.32,.3),('rock2',211.8,-123.8,.34,-.7),
 ('fern0',209.1,-106,.65,.5),('fern2',210,-113.2,.61,-.8),
 ('deadwood',209.5,-117.3,.46,math.pi/2+.14)]
SPRING=[('rock2',212,-185,.45,.4),('rock0',211,-181.8,.44,.7),
 ('rock2',214.4,-178.4,.42,1.2),('rock1',208.8,-183.5,.26,-.4),
 ('rock0',215,-186,.45,-.2),('rock1',212.4,-174.8,.42,.6),
 ('fern1',209,-179.8,.63,.9),('fern0',211,-187.5,.72,-.5),
 ('fern2',209,-173,.57,.2)]

def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()

def register_aliases(author):
    owner=sys.modules[type(author).__module__];owner.ALIASES.update(ASSET_ALIASES)
    ALIASES.update(ASSET_ALIASES)

def specs():
    records=[]
    for pocket in POCKETS:
        side=pocket['side'];rows=MIDDLE if pocket['kind']=='middle'else SPRING
        for i,(alias,x,z,scale,yaw) in enumerate(rows):
            records.append(dict(alias=alias,x=side*x,z=z,yaw=side*yaw+(0 if side>0 else .13),
                scale=scale,burial=.015 if alias.startswith('rock')else .005,elevation=0,
                label=f'castle_creek_bank_{pocket["id"]}_{i}',bank_pocket=pocket['id']))
    return records

def paths(author):
    routes=dict(original=list(zip(ROUTE,ROUTE[1:])),
        district=list(zip(author.layout['demo_route_xz'],author.layout['demo_route_xz'][1:])),
        hero=list(zip(author.demo_market_hero['walking_route_xz'],author.demo_market_hero['walking_route_xz'][1:])),
        expansion=list(zip(EXPANSION_WALK,EXPANSION_WALK[1:])),
        people=source_routes('world_village_population.cpp'),pets=source_routes('world_village_pets.cpp'))
    return routes

def stationary():
    archers=[(side*x,z)for side in[-1,1]for x,start in[(35,-72),(30,-64)]for z in[start-j*5 for j in range(4)]]
    return [(p,.85)for p in TRADERS+OLD_STATIONS+archers]+[(p,PET_RADII[s]+.15)for s,p in PET_STATIONS]+[
        (s['position_xz'],s['reserved_radius_m'])for s in TRAINING_STATIONS]

def runtime_support(author,r,atlas,source_points):
    """Match world_scenery.cpp supported_height exactly, using actual terrain."""
    lo,hi=author.model(r['alias'],True);c,s=math.cos(r['yaw']),math.sin(r['yaw']);scale=r['scale']
    at=lambda x,z:atlas.sample(ORIGIN[0]+x,ORIGIN[1]+z).height
    root=at(r['x'],r['z'])
    water_level=C.c_float();water_depth=C.c_float()
    water=atlas.api.world_atlas_local_water(ORIGIN[0]+r['x'],ORIGIN[1]+r['z'],C.byref(water_level),C.byref(water_depth))
    if water:return None
    samples=[]
    if r['alias'].startswith('fern'):
        base=root-r['burial']+r['elevation']
    else:
        for iz in range(5):
            for ix in range(5):
                x=(lo[0]+ix/4*(hi[0]-lo[0]))*scale;z=(lo[2]+iz/4*(hi[2]-lo[2]))*scale
                samples.append(at(r['x']+c*x+s*z,r['z']-s*x+c*z))
        base=min([root,*samples])-r['burial']-.05+r['elevation']
    differences=[];root_contacts=[];exposed=0
    for p in source_points:
        x=r['x']+(c*p[0]+s*p[2])*scale;z=r['z']+(-s*p[0]+c*p[2])*scale
        surface=at(x,z);gap=base+p[1]*scale-surface;differences.append(gap)
        if p[1]*scale<.08:root_contacts.append(gap)
        if gap>.10 and base+p[1]*scale>22.95:exposed+=1
    minimum=min(differences);height=(hi[1]-lo[1])*scale
    if r['alias'].startswith('fern'):
        if not root_contacts or min(root_contacts)<-.065 or min(root_contacts)>.025:return None
        if minimum<-.14:return None
    else:
        # At least one actual source vertex penetrates/touches terrain; at
        # least ten stay exposed. A sphere-only flat-ground test cannot pass.
        if minimum>.015 or minimum<-max(.18,height*.78) or exposed<10:return None
        if max(differences)<.16:return None
    return dict(root_terrain_y=root,runtime_base_y=base,terrain_support_grid_samples=len(samples),
        actual_source_vertex_rays=len(differences),minimum_source_to_terrain_gap_m=minimum,
        maximum_exposed_height_m=max(differences),exposed_source_vertices=exposed,
        source_bounds_y_up=[lo,hi],root_is_dry=True)

def volume(author,r,support):
    box=list(shape(author,r));box[6]=support['runtime_base_y']-24
    box[7]=box[6]+author.model(r['alias'])[1][1]*r['scale'];return tuple(box)

def clear(author,r,support,rows,route_sets):
    box=volume(author,r,support);pocket=next(p for p in POCKETS if p['id']==r['bank_pocket'])
    x0,z0,x1,z1=pocket['bounds_xz']
    if box[8]<x0 or box[9]>x1 or box[10]<z0 or box[11]>z1:return False
    for road in author.layout['roads']:
        if route_margin(author,r,road['a'],road['b'])<=road['half_width']+.37:return False
    for segments in route_sets.values():
        if any(route_margin(author,r,a,b)<.85 for a,b in segments):return False
    for point,radius in stationary():
        if route_margin(author,r,point,point)<=radius+.20:return False
    for label,other,record in rows:
        if r['label']==label:continue
        if record['alias']in PURCHASED_BUILDINGS and not separated_bounds(author,r,record,.17):return False
        if overlap(box,other,.025):return False
    return True

def atlas_api(directory):
    a=Atlas(directory)
    a.api.world_atlas_local_water.argtypes=[C.c_double,C.c_double,C.POINTER(C.c_float),C.POINTER(C.c_float)]
    a.api.world_atlas_local_water.restype=C.c_bool
    return a

def apply(author):
    if hasattr(author,'_castle_creek_banks_metadata'):return copy.deepcopy(author._castle_creek_banks_metadata)
    register_aliases(author);baseline=copy.deepcopy(author.records)
    assert not any(r['label'].startswith('castle_creek_bank_')for r in baseline)
    rows=[(r['label'],shape(author,r),r)for r in baseline if r['alias']!='starting_gate']
    route_sets=paths(author);source={a:glb_bounds(MODELS/(stem+'.glb'),True)[2]for a,stem in ASSET_ALIASES.items()}
    labels=[];supports=[];placements=[]
    with tempfile.TemporaryDirectory(prefix='valwood-bank-support-')as directory:
        atlas=atlas_api(directory)
        for original in specs():
            chosen=None
            # A bounded sub-metre contact adjustment within the same authored
            # pocket finds solid terrain, never scatters over the wall length.
            shifts=[(0,0),(.25,0),(.5,0),(.75,0),(0,.3),(0,-.3),(.5,.3),(.5,-.3),(-.25,0)]
            side=1 if original['x']>0 else-1
            for dx,dz in shifts:
                r=copy.deepcopy(original);r['x']+=side*dx;r['z']+=dz
                support=runtime_support(author,r,atlas,source[r['alias']])
                if support and clear(author,r,support,rows,route_sets):chosen=(r,support);break
            assert chosen,('No supported dry bank placement',original['label'],original['alias'],[original['x'],original['z']])
            r,support=chosen;support['label']=r['label']
            author.add(r['alias'],r['x'],r['z'],r['yaw'],r['scale'],r['burial'],r['label'],r['elevation'])
            author.records[-1]['bank_pocket']=r['bank_pocket'];rows.append((r['label'],volume(author,r,support),r))
            labels.append(r['label']);supports.append(support);placements.append(r)
    author._castle_creek_banks_baseline=baseline
    author._castle_creek_banks_metadata=dict(schema_version=1,title='Four authored creek bank and spring pockets',
        recipe_file=str(Path(__file__).relative_to(ROOT)),recipe_sha256=sha(Path(__file__)),
        terrain_helper_file='engine/world_castle_creek.h',terrain_helper_sha256=sha(ROOT/'engine/world_castle_creek.h'),
        labels=labels,existing_records_preserved=len(baseline),pockets=copy.deepcopy(POCKETS),
        baseline_records_sha256=hashlib.sha256(json.dumps(baseline,sort_keys=True,separators=(',',':')).encode()).hexdigest(),
        source_models={a:dict(file=str((MODELS/(s+'.glb')).relative_to(ROOT)),sha256=sha(MODELS/(s+'.glb')))for a,s in ASSET_ALIASES.items()},
        rocks=24,ferns=10,deadwood=2,new_model_families=0,new_gpu_image_keys=0,global_density_changed=False,
        outside_outer_town_walls=True,enclosure_wall_centerline_xz=[-205,-205,205,205],
        support_policy='actual native25point rigid grounding + source vertex/terrain contact; fern root contact',
        supports=supports,placements=placements)
    return copy.deepcopy(author._castle_creek_banks_metadata)

def validate(author):
    meta=author._castle_creek_banks_metadata;base=author._castle_creek_banks_baseline
    assert baseline_equal(author.records[:len(base)],base),'Previous castle/bridge records changed'
    assert sha(Path(__file__))==meta['recipe_sha256']
    assert sha(ROOT/meta['terrain_helper_file'])==meta['terrain_helper_sha256']
    assert len(meta['labels'])==36==len(set(meta['labels']))
    known={r['label']:r for r in author.records};assert len(known)==len(author.records)
    actual=[known[label]for label in meta['labels']]
    assert sum(r['alias'].startswith('rock')for r in actual)==24
    assert sum(r['alias'].startswith('fern')for r in actual)==10
    assert sum(r['alias']=='deadwood'for r in actual)==2
    for r,expected in zip(actual,meta['placements']):assert baseline_equal([r],[expected]),('Bank placement changed',r['label'])
    for a,record in meta['source_models'].items():assert sha(ROOT/record['file'])==record['sha256'],('Resident source changed',a)
    resident=set()
    for preserved in author.layout['preserve']:
        for line in (PATH.parent/preserved['file']).read_text().splitlines():
            fields=line.split()
            if fields and not fields[0].startswith('#'):resident.add(fields[0])
    assert set(ASSET_ALIASES)<=resident,'Bank layer must reuse actually resident source aliases'
    routes=paths(author);minima={name:math.inf for name in routes};rays=0;support_samples=0;volume_checks=0
    source={a:glb_bounds(MODELS/(s+'.glb'),True)[2]for a,s in ASSET_ALIASES.items()}
    with tempfile.TemporaryDirectory(prefix='valwood-bank-check-')as directory:
        atlas=atlas_api(directory);supports={}
        for r in actual:
            support=runtime_support(author,r,atlas,source[r['alias']]);assert support,('Floating/buried/underwater bank asset',r['label'])
            supports[r['label']]=support;rays+=support['actual_source_vertex_rays'];support_samples+=support['terrain_support_grid_samples']
        rows=[(r['label'],volume(author,r,supports[r['label']])if r['label']in supports else shape(author,r),r)
            for r in author.records if r['alias']!='starting_gate']
        for r in actual:
            assert clear(author,r,supports[r['label']],rows,routes),('Bank source volume blocks road/actor/old model',r['label'])
            volume_checks+=len(rows)-1
            for name,segments in routes.items():minima[name]=min(minima[name],*(route_margin(author,r,a,b)for a,b in segments))
    assert meta['new_model_families']==0 and meta['new_gpu_image_keys']==0 and not meta['global_density_changed']
    return dict(instances=36,rocks=24,ferns=10,deadwood=2,pockets=4,existing_records_preserved=len(base),
        actual_source_vertex_terrain_rays=rays,native_rigid_support_grid_samples=support_samples,
        source_bound_volume_checks=volume_checks,minimum_full_model_route_distances_m=minima,
        all_roads_full_width_clear=True,all_training_station_sweep_reserves_clear=True,
        no_new_model_families=True,no_new_gpu_image_keys=True)

def test(author):
    proof=validate(author);before=copy.deepcopy(author.records);meta=apply(author)
    assert apply(author)==meta and author.records==before
    bad=copy.copy(author);bad.records=copy.deepcopy(author.records)
    next(r for r in bad.records if r['label']==meta['labels'][0])['x']=0
    try:validate(bad)
    except AssertionError:pass
    else:raise AssertionError('Bank asset in main road negative control passed')
    bad=copy.copy(author);bad.records=copy.deepcopy(author.records)
    next(r for r in bad.records if r['label']==meta['labels'][0])['elevation']=.5
    try:validate(bad)
    except AssertionError:pass
    else:raise AssertionError('Floating bank object negative control passed')
    # Exercise actual support geometry directly, independently of the recipe
    # transform guard above. Raised fern roots and submerged rock roots fail.
    known={r['label']:r for r in author.records}
    with tempfile.TemporaryDirectory(prefix='valwood-bank-negative-')as directory:
        atlas=atlas_api(directory)
        fern=copy.deepcopy(next(known[label]for label in meta['labels']if known[label]['alias'].startswith('fern')))
        points=glb_bounds(MODELS/(ASSET_ALIASES[fern['alias']]+'.glb'),True)[2]
        fern['elevation']=.5
        assert runtime_support(author,fern,atlas,points)is None,'Raised source-root physics negative control passed'
        rock=copy.deepcopy(known[meta['labels'][0]]);rock.update(x=219,z=-125)
        points=glb_bounds(MODELS/(ASSET_ALIASES[rock['alias']]+'.glb'),True)[2]
        assert runtime_support(author,rock,atlas,points)is None,'Submerged source-root physics negative control passed'
    proof.update(idempotent=True,blocked_road_negative_control=True,floating_object_negative_control=True,
        actual_floating_fern_root_negative_control=True,actual_submerged_rock_root_negative_control=True,
        all_aliases_proven_resident_in_preserved_sources=True)
    return proof

def main():
    p=argparse.ArgumentParser();p.add_argument('--stage',action='store_true');p.add_argument('--test',action='store_true');args=p.parse_args()
    assert args.stage or args.test
    author=Author(json.loads(PATH.read_text()));author.compose();metadata=apply(author);proof=test(author)if args.test else validate(author)
    if args.stage:
        OUTPUT.mkdir(parents=True,exist_ok=True)
        (OUTPUT/'bank-dressing-stage.json').write_text(json.dumps(dict(metadata=metadata,validation=proof),indent=2)+'\n')
    print(json.dumps(proof,indent=2))

if __name__=='__main__':main()
