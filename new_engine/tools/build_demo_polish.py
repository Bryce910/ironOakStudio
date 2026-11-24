#!/usr/bin/env python3
"""Measured, additive art layer for the street, tavern and travelers' clearing.

Uses owned, already-cooked meshes only. Each placement records its actual LOD
bounds; the existing eight scene sources remain unchanged. No runtime scatter.
"""
from pathlib import Path
from collections import Counter
import argparse,hashlib,json,math,random,tempfile,re
from build_demo_finish import Author,sha,sat_gap,corners,path_gap,ORIGIN,WIDTH,DEPTH
from build_starting_area import Atlas,segment_distance

ROOT=Path(__file__).resolve().parents[1]
SCENE=ROOT/'projects/valwood_world/scenery'
CX,CZ,CY=-26688.,9670.496705929329,138.235
SHARED_LAYOUT=ROOT/'engine/world_demo_polish_layout.h'
APPROACH=[tuple(map(float,p))for p in re.findall(r'\{([-\d.]+),([-\d.]+)\}',SHARED_LAYOUT.read_text().split('world_trail_camp_approach[][2]={',1)[1])]
SOURCE_NAMES=('authored','starting_area','rainforest','castle_village','tanthoon_bandit_camp','castle_streets','bep_environment','demo_finish')

def compose(a):
    old=[]
    def organic(id):
        # Independent per-vignette seed: editing one pocket does not move all
        # downstream plants, and reproducible proof checks see the same scene.
        return random.Random(int.from_bytes(hashlib.sha256(id.encode()).digest()[:8],'little'))
    for name in SOURCE_NAMES:
        for number,line in enumerate((SCENE/(name+'_instances.txt')).read_text().splitlines(),1):
            t=line.split('#',1)[0].split()
            if not t:continue
            x=(float(t[1])-.5)*WIDTH;z=(float(t[2])-.5)*DEPTH
            r=dict(id=f'{name}_instances.txt:{number}',alias=t[0],local_xz=[x-ORIGIN[0],z-ORIGIN[1]],world_xz=[x,z],yaw_degrees=float(t[3]),scale=float(t[4]),burial_m=float(t[5]),elevation_m=float(t[6])if len(t)>6 else 0)
            old.append(r)
    a.previous=[r for r in old if abs(r['local_xz'][0])<245 and abs(r['local_xz'][1])<245 and a.model(r['alias'])['collision']!='None']
    # Work areas flank the pavement, with source-supported goods rather than
    # unmeasured floating stacks. Existing road/actor reservations stay active.
    for i,(x,z)in enumerate([(-136,51),(-126,51),(-136,-22),(-126,-22),(-136,90),(-126,102),(-152,-69),(-161,-69)]):
        for j,(dx,dz,alias,scale)in enumerate([(0,0,'bep_prop_barrel',.72),(.85,.2,'purchased_prop_sack',.62),(-.65,.55,'bep_prop_bucket',.68)]):
            a.add(f'polish_delivery_{i}_{j}',alias,x+dx,z+dz,i*41+j*27,scale,zone='street_work_pockets',required=False)
        for j in range(4):a.add(f'polish_wall_herbs_{i}_{j}','bep_nature_clover'if j%2 else'starting_celandine',x-.9+j*.35,z+.8+j%2*.2,j*73+i*29,.65,zone='street_soft_edges',required=False)
    tavern=next(r for r in old if r['alias']=='village_tavern')
    for i,(x,z,yaw)in enumerate([(-150.,-89.62,0),(-140.,-74.38,180)]):
        a.add(f'polish_tavern_wall_lamp_{i}','purchased_prop_lamp',x,z,yaw,.24,elevation=2.02,zone='tavern_wall_practicals',parent=tavern['id'])
    # Actual table triangles are selected in the75cm--1.15m band: no cups on
    # canopy roofs, seats, or the floor; the doorway and staff aisle stay free.
    for i,(x,z)in enumerate([(-5.,3.4),(5.,3.4),(-5.,-3.7),(6.2,-1.)]):
        for j,(dx,dz,alias,scale)in enumerate([(-.42,-.18,'purchased_goods_tankard',.8),(.40,.20,'purchased_goods_tankard',.72),(0,.04,'purchased_goods_bread',.8),(.14,-.28,'purchased_goods_cheese',.65)]):
            a.supported(f'polish_tavern_table_{i}_{j}',alias,tavern,x+dx,z+dz,i*33+j*47,scale,zone='tavern_table_service',support_interval=(.74,1.16))
    # Stock the actual back-bar shelves, with deliberately uneven groups and
    # empty spaces. Every object's complete footprint must sit on source
    # triangles at the requested shelf height, not on a guessed room plane.
    for row,height in enumerate((.715,1.715,2.565)):
        for column,x in enumerate((-8.35,-7.55,-4.45,-3.50,-2.55)):
            alias=('purchased_goods_tankard','purchased_goods_bread','purchased_goods_cheese')[(row+column)%3]
            a.supported(f'polish_back_bar_stock_{row}_{column}',alias,tavern,x,-7.43,
                row*47+column*31,.68 if alias.endswith('tankard')else .58,
                zone='tavern_back_bar_stock',support_interval=(height-.02,height+.025))
    for i,x in enumerate((-7.6,-6.1,-4.4,-2.4)):
        for j,(alias,dx,dz,scale)in enumerate((('purchased_goods_tankard',0,0,.82),('purchased_goods_bread',.3,.2,.65))):
            a.supported(f'polish_bar_service_{i}_{j}',alias,tavern,x+dx,-3.7+dz,23*i+37*j,
                scale,zone='tavern_bar_service',support_interval=(1.10,1.16))
    a.add('polish_hearth_fuel','purchased_prop_logs',-136.4,-86.4,90,.55,
        zone='tavern_hearth_work',parent=tavern['id'],required=False)
    a.add('polish_hearth_bucket','bep_prop_bucket',-136.3,-84.7,-19,.68,
        zone='tavern_hearth_work',parent=tavern['id'],required=False)
    # Strengthen the already-walkable arcade rather than widening its reserved
    # carriageway. Framing plants and delivery details sit beside real source
    # facades; all existing public and actor corridor guards remain active.
    finish=json.loads((SCENE/'demo_finish_manifest.json').read_text())
    for i,h in enumerate(r for r in finish['placements']if r['zone']=='bakers_arcade'):
        parent=next(r for r in old if r['alias']==h['alias']and math.dist(r['local_xz'],h['local_xz'])<.001)
        model=a.model(parent['alias']);front=model['bounds_max'][2]*parent['scale'];half=max(abs(model['bounds_min'][0]),abs(model['bounds_max'][0]))*parent['scale']
        for j in range(5):
            variation=organic(f'arcade_footing_{i}_{j}')
            lx=(-.65+j*.22)*half+variation.uniform(-.22,.22)
            lz=front+.25+(j%2)*.12+variation.uniform(0,.18)
            x,z=a.local(parent,lx,lz)
            a.add(f'polish_arcade_footing_{i}_{j}','bep_nature_clover'if j%2 else'starting_celandine',
                x,z,i*23+j*41,.38+variation.uniform(0,.23),zone='arcade_footing_plants',parent=parent['id'],required=False)
        x,z=a.local(parent,half*.64,front+.52)
        a.add(f'polish_arcade_delivery_bucket_{i}','bep_prop_bucket',x,z,i*43,.68,
            zone='arcade_delivery_details',parent=parent['id'],required=False)
    # Entrance work corners remain outside the public route and both new
    # shield-watch stations. Small grounded furnishings soften the wide lots.
    a.routes += [('polish_watch_arcade',(-97.5,39.5),(-97.5,39.5)),
                 ('polish_watch_tavern',(-151,-71.6),(-151,-71.6))]
    for i,(x,z) in enumerate([(-157,-66),(-159,-71),(-134,-68),(-126,-65),(-134,66),(-126,113)]):
        for j,(alias,dx,dz,scale) in enumerate([('bep_prop_barrel',0,0,.65),
                ('purchased_prop_sack',.85,.1,.55),('bep_prop_bucket',-.65,.4,.58)]):
            a.add(f'demo_work_corner_{i}_{j}',alias,x+dx,z+dz,31*i+47*j,scale,
                zone='demo_street_work_corners',required=False)
        for j in range(5):
            a.add(f'demo_work_green_{i}_{j}','bep_nature_clover'if j%2 else'starting_celandine',
                x-1.3+j*.44,z+.9+(j%2)*.2,31*j+13*i,.44,
                zone='demo_street_footing_greens',required=False)
    # World-space addition helper. Collision-bearing meshes are checked against
    # complete near/mid OBBs; plants are allowed to form intentional clusters.
    rejected=Counter();minimum_route=math.inf
    trail=[(-26727.,9735.496705929329),(-26710.,9700.496705929329),(-26720.,9660.496705929329),(-26707.,9615.496705929329),(-26717.,9575.496705929329),(-26700.,9530.496705929329)]
    def world(id,alias,x,z,yaw=0,scale=1,burial=0,elevation=0,zone='jungle_composition',reserve=True):
        nonlocal minimum_route
        yaw=(yaw+180)%360-180
        assert .15<=scale<=4 and 0<=burial<=2 and 0<=elevation<=3,(id,scale,burial,elevation)
        m=a.model(alias);lo,hi=m['bounds_min'],m['bounds_max']
        assert burial<=hi[1]*scale*.6,(id,'burial hides too much of the model',burial,hi[1]*scale*.6)
        rad=math.hypot(max(abs(lo[0]),abs(hi[0])),max(abs(lo[2]),abs(hi[2])))*scale
        if m['collision']=='Trunk':rad=.65*scale
        # Main216m trail and camp approach always retain a clear pedestrian core.
        gap=min(segment_distance((x,z),p,q) for p,q in zip(trail,trail[1:]))-rad-2.2
        if reserve:gap=min(gap,min(segment_distance((x,z),p,q)for p,q in zip(APPROACH,APPROACH[1:]))-rad-1.1)
        if gap<.15:rejected['route']+=1;return
        r=dict(id=id,alias=alias,local_xz=[x-ORIGIN[0],z-ORIGIN[1]],world_xz=[x,z],yaw_degrees=yaw,scale=scale,burial_m=burial,elevation_m=elevation,zone=zone,parent=None)
        if m['collision']!='None':
            for other in old+a.records:
                if math.dist((x,z),other['world_xz'])>45:continue
                if zone=='camp_hearth' and other.get('zone')==zone:continue # Intentional interlocked fuel/stone assembly.
                om=a.model(other['alias'])
                if om['collision']!='None' and sat_gap(r,m,other,om)<.08:rejected['solid_obb']+=1;return
        heights=[];angle=math.radians(yaw);c,s=math.cos(angle),math.sin(angle)
        for iz in range(3):
            for ix in range(3):
                lx=(lo[0]+(hi[0]-lo[0])*ix/2)*scale;lz=(lo[2]+(hi[2]-lo[2])*iz/2)*scale
                if m['collision']=='Trunk':lx=(ix-1)*.65*scale;lz=(iz-1)*.65*scale
                wx=x+c*lx+s*lz;wz=z-s*lx+c*lz;sample=a.atlas.sample(wx,wz)
                if sample.land<.75 or sample.height<.1 or a.atlas.api.world_atlas_local_water(wx,wz,None,None):rejected['water']+=1;return
                heights.append(sample.height)
        support_limit=2.2 if zone=='jungle_fractured_banks' else .5 if zone in('jungle_root_stone_anchors','jungle_canopy_frames','jungle_fallen_trees') else .12
        if m['collision']!='None' and max(heights)-min(heights)>support_limit:rejected['support']+=1;return
        r.update(ground_height_min_m=min(heights),ground_height_max_m=max(heights),minimum_road_obb_gap_m=gap,minimum_actor_route_obb_gap_m=gap,minimum_other_source_obb_gap_m=None)
        a.records.append(r);minimum_route=min(minimum_route,gap);return r
    # The fire is a compact ring of real stones and crossed split logs. The
    # clearing retains an open west approach; living routes occupy its east.
    for i in range(10):
        t=i*math.tau/10;world(f'camp_hearth_stone_{i}','rock0'if i%2 else'rock1',CX+.83*math.cos(t),CZ+.83*math.sin(t),i*37,.15,.035,zone='camp_hearth',reserve=False)
    for i,(x,z,yaw)in enumerate([(-.28,0,35),(.25,.06,-35),(0,.23,90)]):world(f'camp_firewood_{i}','purchased_prop_logs',CX+x,CZ+z,yaw,.20,zone='camp_hearth',reserve=False)
    # Source center seat=.44032091m; Support roots sit50mm below terrain.
    # Scale brings its physical seat to the retained sitting clip's.495m seat.
    for i,z in enumerate([-3.3,3.3]):world(f'camp_rest_bench_{i}','bep_prop_bench',CX-1.0,CZ+z,180 if i else 0,1.2377333,zone='camp_rest',reserve=False)
    for i,(dx,dz,alias,scale)in enumerate([(-.8,-7.5,'purchased_shop_d',.56),(-2.8,-5.9,'bep_prop_chest',.70),(-3.1,5.4,'bep_prop_barrel',.76),(-2.1,5.7,'purchased_prop_sack',.7),(-3.0,6.7,'bep_prop_bucket',.8),(-2.8,-7.8,'purchased_prop_rope',.65)]):
        world(f'camp_supply_{i}',alias,CX+dx,CZ+dz,33*i,scale,zone='camp_shelter_supplies',reserve=False)
    rng=random.Random(42071)
    # A few complete crowns establish foreground/midground silhouettes. Low
    # plants form intentional islands, leaving quiet soil and readable paths.
    for i,(dx,dz)in enumerate([(-7,-90),(17,-62),(-22,-45),(19,-19),(-22,12),(18,55)]):
        world(f'canopy_frame_{i}','bep_nature_oak'if i%2 else'bep_nature_beech',-26712+dx,9640.496705929329+dz,71*i,.82+.04*(i%3),zone='jungle_canopy_frames')
    for pocket,(dx,dz)in enumerate([(-21,83),(12,68),(-20,49),(11,37),(-23,13),(16,4),(-18,-15),(15,-38),(-20,-58),(22,-78),(-4,-100),(31,48)]):
        px,pz=-26712+dx,9640.496705929329+dz
        for j in range(13):
            t=rng.random()*math.tau;r=math.sqrt(rng.random())*3.7
            alias=('bep_nature_fern','showcase_sorrel','bep_nature_clover','bep_nature_bush_b','bep_nature_grass_broad')[j%5]
            world(f'forest_floor_island_{pocket}_{j}',alias,px+math.cos(t)*r,pz+math.sin(t)*r,rng.random()*360,.58+rng.random()*.35,.008,zone='jungle_layered_floor')
        world(f'forest_root_anchor_{pocket}','showcase_roots'if pocket%3==0 else'rock1',px+2.8,pz-1.1,pocket*53,.24 if pocket%3==0 else .42,.045,zone='jungle_root_stone_anchors')
    # Eight shaped banks establish silhouettes; low species are clustered
    # into finite vignettes rather than a global density multiplier.
    banks=[tuple(map(float,r))for r in re.findall(r'\{(-?\d+),(-?\d+),(\d+),(\d+),([\d.]+)\}',
        (ROOT/'engine/world_demo_contours.h').read_text().split('world_demo_banks[]={',1)[1].split('};',1)[0])]
    assert len(banks)==8
    for i,(bx,bz,rx,rz,height) in enumerate(banks):
        px,pz=-26712+bx,9640.496705929329+bz
        for j in range(11):
            angle=j*2.399963229728653+i*.6;r=math.sqrt((j+.5)/11)*3.7
            alias=('bep_nature_fern','fern1','showcase_sorrel','bep_nature_bush_a','bep_nature_grass_long')[j%5]
            world(f'demo_bank_botanical_{i}_{j}',alias,px+math.cos(angle)*r,pz+math.sin(angle)*r,
                angle*180/math.pi,.45+(j%4)*.10,.012,zone='demo_bank_botanicals')
        for j,alias in enumerate(('rock1','showcase_roots','showcase_stump')):
            world(f'demo_bank_anchor_{i}_{j}',alias,px-2+j*1.5,pz+2.8,i*53+j*71,
                .20 if j!=1 else .18,.045,zone='jungle_root_stone_anchors')
    # Small, alternating trail-edge groups join the dirt feather to forest
    # litter while every full model footprint retains the walking clearance.
    for i,(p,q) in enumerate(zip(trail,trail[1:])):
        dx,dz=q[0]-p[0],q[1]-p[1];length=math.hypot(dx,dz)
        for side,t in [(-1,.28),(1,.64)]:
            for j in range(6):
                variation=organic(f'trail_edge_{i}_{side}_{j}')
                along=(j-2.5)*.50+variation.uniform(-.24,.24)
                offset=3.6+(j%3)*.42+variation.uniform(-.12,.26)
                x=p[0]+dx*t+dx/length*along-dz/length*side*offset
                z=p[1]+dz*t+dz/length*along+dx/length*side*offset
                world(f'demo_trail_feather_{i}_{side}_{j}',
                    ('bep_nature_clover','starting_celandine','showcase_sorrel')[j%3],
                    x,z,i*43+j*61+variation.uniform(-25,25),.25+variation.uniform(0,.12),.008,zone='demo_trail_edge_groups')
    # Distinct landscape moments with the already-owned scanned rock/log kit.
    # Solid OBB/route tests still reject unsafe placement. Low plants conceal
    # interpenetrating stone bases; they do not obscure the walking core.
    # Search small off-path lots on the contour toes. Each accepted face must
    # pass the same support, full OBB and path-clearance checks as fixed props.
    for i,(cx,cz)in enumerate([(-50,-20),(52,3),(-41,-93),(35,-115)]):
        placed=False
        for alias,scale in [('cliff',.16),('rock1',.82)]:
            for ring in range(4):
                for j in range(8):
                    t=j*math.tau/8+i*.7;x=cx+ring*2.1*math.cos(t);z=cz+ring*2.1*math.sin(t)
                    yaw=(j*45+90)%360
                    if world(f'jungle_bluff_face_{i}',alias,-26712+x,9640.496705929329+z,yaw,scale,.18,zone='jungle_fractured_banks'):
                        placed=True;break
                if placed:break
            if placed:break
    for i,(x,z,yaw,scale)in enumerate([(-29,65,21,.85),(19,71,113,.75),(-20,35,-42,.65),(23,-12,72,.95),(-21,-71,155,.70),(21,-98,31,.85)]):
        world(f'jungle_fallen_trunk_{i}','deadwood',-26712+x,9640.496705929329+z,yaw,scale,.07,zone='jungle_fallen_trees')
        world(f'jungle_exposed_root_{i}','showcase_roots',-26712+x+2.3,9640.496705929329+z+.6,yaw+34,.34,.07,zone='jungle_root_stone_anchors')
        for j in range(4):
            variation=organic(f'new_root_footing_{i}_{j}')
            world(f'jungle_root_fern_{i}_{j}',('bep_nature_fern','showcase_sorrel')[j%2],
                -26712+x+variation.uniform(-2,2),9640.496705929329+z+variation.uniform(-1.3,1.3),
                variation.random()*360,.32+variation.random()*.22,.008,zone='jungle_root_footing')
    route_min=math.inf;checked=0
    for other in old+a.records:
        if a.aliases[other['alias']][2]=='None':continue
        model=a.model(other['alias']);radius=math.hypot(max(abs(model['bounds_min'][0]),abs(model['bounds_max'][0])),max(abs(model['bounds_min'][2]),abs(model['bounds_max'][2])))*other['scale']
        if math.dist(other['world_xz'],(CX,CZ))>50+radius:continue
        if model['collision']=='Trunk':model={**model,'bounds_min':[-.65,0,-.65],'bounds_max':[.65,1,.65]}
        for p,q in zip(APPROACH,APPROACH[1:]):
            gap=path_gap(other,model,[p[0]-ORIGIN[0],p[1]-ORIGIN[1]],[q[0]-ORIGIN[0],q[1]-ORIGIN[1]])
            assert gap>.95,('Camp approach blocked',other['id'],other['alias'],other['world_xz'],p,q,gap)
            route_min=min(route_min,gap);checked+=1
    return dict(camp=dict(center=[CX,CY,CZ],approach=APPROACH,npcs=4,walking_goats=2,fire=True,shared_layout_sha256=sha(SHARED_LAYOUT),whole_route_obb_pairs=checked,minimum_solid_path_gap_m=route_min),rejected=dict(rejected),minimum_route_gap_m=minimum_route)

def main():
    cli=argparse.ArgumentParser(description=__doc__);cli.add_argument('--force',action='store_true');cli.add_argument('--validate-only',action='store_true');args=cli.parse_args()
    protected={str(p.relative_to(ROOT)):sha(p)for p in[SCENE/(s+'_instances.txt')for s in SOURCE_NAMES]}
    with tempfile.TemporaryDirectory(prefix='valwood-demo-polish-')as tmp:
        a=Author(Atlas(tmp));detail=compose(a)
        lines=['# Authored demo polish: owned meshes only; tools/build_demo_polish.py.']
        for r in a.records:
            x,z=r['world_xz'];lines.append(f"{r['alias']} {x/WIDTH+.5:.14f} {z/DEPTH+.5:.14f} {r['yaw_degrees']:.6f} {r['scale']:.6f} {r['burial_m']:.6f} {r['elevation_m']:.6f} # {r['id']}")
        text='\n'.join(lines)+'\n';counts=Counter(r['zone']for r in a.records)
        assert counts['camp_hearth']>=10 and counts['camp_rest']==2 and counts['tavern_table_service']>=8 and counts['jungle_layered_floor']>=90,counts
        manifest=dict(schema_version=1,generator='tools/build_demo_polish.py',generator_sha256=sha(__file__),record_count=len(a.records),zones=dict(counts),source_sha256=hashlib.sha256(text.encode()).hexdigest(),source_files=9,new_images=0,new_model_files=0,protected_sources=protected,placements=a.records,models=a.models,details=detail,triangle_supported_goods=a.supports,performance_note='Fixed authored layer; current shared meshes/texture cache and LOD ranges. Native frame-time acceptance is separate.')
        actor='world_village_population.cpp'
        before_actor=ROOT/'docs/performance/demo-route-polish-2026-10-28/before/engine'/actor
        original=json.loads((ROOT/'docs/performance/demo-route-polish-2026-10-28/before/projects/valwood_world/scenery/demo_finish_manifest.json').read_text())['proof']['runtime_actor_route_sha256'][actor]
        assert sha(before_actor)==original
        route_text=lambda p:p.read_text().split('constexpr Route routes[]={',1)[1].split('};',1)[0]
        assert route_text(before_actor)==route_text(ROOT/'engine'/actor)
        manifest['actor_source_updates']={actor:dict(before_sha256=original,after_sha256=sha(ROOT/'engine'/actor),
            before_source=str(before_actor.relative_to(ROOT)),routes_unchanged=True,
            route_definition_sha256=hashlib.sha256(route_text(before_actor).encode()).hexdigest(),
            purpose='Appended resident idle pacing, eased street turns and counter-facing dwell; original128 motion golden regression retained')}
        life='projects/valwood_world/scenery/village_life_layout.json'
        before=json.loads((ROOT/'docs/performance/bep-environment/before-inputs.json').read_text())
        manifest['authored_input_updates']={life:dict(before_sha256=before[life],after_sha256=sha(ROOT/life),purpose='Tavern bartender route and host variety')}
        for name in [*[f'village_character_seated_idle_{i:02}.glb'for i in range(4)],'village_seated_manifest.json']:
            path='projects/valwood_world/models/'+name
            manifest['authored_input_updates'][path]=dict(before_sha256=before[path],after_sha256=sha(ROOT/path),purpose='Restore original Body UVmap on genuine seated poses')
        for p,value in[(SCENE/'demo_polish_instances.txt',text),(SCENE/'demo_polish_manifest.json',json.dumps(manifest,indent=2)+'\n')]:
            if args.validate_only:assert p.read_text()==value,p
            elif args.force or not p.exists():p.write_text(value)
            else:assert p.read_text()==value,'Use --force for deliberate reauthoring'
        assert all(sha(ROOT/p)==h for p,h in protected.items())
        print(json.dumps(dict(records=len(a.records),zones=dict(counts),details=detail),indent=2))
if __name__=='__main__':main()
