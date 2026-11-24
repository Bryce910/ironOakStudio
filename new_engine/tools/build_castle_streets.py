#!/usr/bin/env python3
"""Author an additive western castle-town district; never rewrite older scenery.

Geometry is measured from actual runtime GLBs. The atlas overlay contains only
paving/grass masks, never heights. A deterministic sixth source shares existing
house/prop LODs and the separately authored tavern/shelter materials.
"""
import argparse
from collections import Counter
import ctypes as C
import hashlib
import html
import json
import math
from pathlib import Path
import re
import struct
import tempfile

from build_starting_area import Atlas, Sample, glb_bounds, rectangle_distance

ROOT=Path(__file__).resolve().parents[1]
SCENERY=ROOT/'projects/valwood_world/scenery'
MODELS=ROOT/'projects/valwood_world/models'
STEM='castle_streets'
ORIGIN=(-1310.4000000000012,-10207.58474745458)
WIDTH,DEPTH=100800.,100800.*2818/5009
PROTECTED=[SCENERY/(s+'_instances.txt')for s in ('authored','starting_area','rainforest','castle_village','tanthoon_bandit_camp')]
# Widths are HALF widths, matching VillageRoad and the existing atlas cache.
ROADS=[('west_high_street',[-130,-163],[-130,125],3.2,.88),
       ('tavern_square_link',[-130,-62],[-145,-62],3.2,.90),
       ('tavern_door_walk',[-145,-62],[-145,-82],1.8,.92),
       ('west_north_link',[-181,-35],[-95,-35],3.,.82),
       ('west_middle_link',[-181,55],[-95,55],3.,.82),
       ('west_south_link',[-181,125],[-95,125],3.,.82),
       ('residents_lane',[-151,-35],[-151,125],2.4,.68),
       ('shelter_front_walk',[-151,23],[-173,23],2.,.40),
       ('shelter_entry',[-166,23],[-166,16],1.4,.32),
       ('coopers_cut',[-151,-5],[-130,-5],1.8,.60),
       ('laundry_cut',[-181,85],[-95,85],2.,.56)]
TOUR=[[-145,-62],[-130,-62],[-130,-35],[-151,-35],[-151,23],[-166,23],[-166,16.5],[-166,23],[-151,23],[-151,55],[-130,55],[-130,125],[-151,125],[-151,85],[-151,-35],[-130,-35],[-130,-62],[-145,-62],[-145,-73],[-145,-77],[-145,-82],[-144,-84],[-144,-87.3],[-145.5,-87.3],[-144,-87.3],[-144,-84],[-145,-82],[-145,-77],[-145,-73],[-145,-62]]
WALKERS=[[-130,-28],[-130,46],[-132,46],[-132,-28]]
CAMERAS=[('castle-west-street','Valdoa | West High Street',[-130,116],[-130,55],-.025),
         ('castle-tavern','Valdoa | Copper Lantern tavern',[-141,-54],[-145,-82],.045),
         ('castle-tavern-interior','Valdoa | Copper Lantern interior',[-145,-77],[-147.55,-87],-.06),
         ('castle-shelter','Valdoa | Residents courtyard',[-153,29],[-166,15],-.07),
         ('castle-alleys','Valdoa | Residential lanes',[-151,78],[-151,25],-.025)]
NEW_ALIASES={
 'village_tavern':('village_tavern','Foundation','Village'),
 'castle_residents_shelter':('castle_residents_shelter','Foundation','Village'),
 'castle_residents_bedroll':('castle_residents_bedroll','Foundation','None')}


def sha(path):
    digest=hashlib.sha256()
    with path.open('rb')as f:
        for b in iter(lambda:f.read(1048576),b''):digest.update(b)
    return digest.hexdigest()


def protected():
    return [dict(file=str(p.relative_to(ROOT)),records=sum(bool(s.strip())and not s.lstrip().startswith('#')for s in p.read_text().splitlines()),sha256=sha(p))for p in PROTECTED]


class Terrain:
    def __init__(self,library=None):
        self.owner=None
        if library:self.api=C.CDLL(str(Path(library).resolve()))
        else:
            self.temporary=tempfile.TemporaryDirectory(prefix='valwood-streets-authoring-')
            self.owner=Atlas(self.temporary.name);self.api=self.owner.api
        self.api.world_atlas_init.restype=C.c_bool
        assert self.api.world_atlas_init()
        self.api.world_atlas_sample.argtypes=[C.c_double,C.c_double];self.api.world_atlas_sample.restype=Sample
        self.api.world_atlas_height.argtypes=[C.c_double,C.c_double];self.api.world_atlas_height.restype=C.c_float
        self.api.world_atlas_local_water.argtypes=[C.c_double,C.c_double,C.c_void_p,C.c_void_p];self.api.world_atlas_local_water.restype=C.c_bool
    def height(self,x,z):return self.api.world_atlas_height(ORIGIN[0]+x,ORIGIN[1]+z)
    def wet(self,x,z):return self.api.world_atlas_local_water(ORIGIN[0]+x,ORIGIN[1]+z,None,None)


class Author:
    def __init__(self,terrain):
        self.terrain=terrain;self.records=[];self.models={};self.aliases={}
        source=(ROOT/'engine/world_scenery.cpp').read_text()
        for m in re.finditer(r'\{"([^"]+)",\s*"([^"]+)"[^\n]+Grounding::(\w+),\s*Collision::(\w+)',source):self.aliases[m[1]]=(m[2],m[3],m[4])
        self.aliases.update(NEW_ALIASES)
        self.previous=[]
        for source in PROTECTED:
            for line in source.read_text().splitlines():
                if not line.strip()or line.lstrip().startswith('#'):continue
                p=line.split();x=(float(p[1])-.5)*WIDTH-ORIGIN[0];z=(float(p[2])-.5)*DEPTH-ORIGIN[1]
                if -220<x<-85 and -200<z<155:self.previous.append(dict(alias=p[0],local_xz=[x,z],yaw_degrees=float(p[3]),scale=float(p[4]),id=f'{source.name}:{line}'))
    def model(self,alias):
        if alias not in self.models:
            stem,grounding,collision=self.aliases[alias];p=MODELS/(stem+'.glb')
            if not p.exists():p=MODELS/(stem+'_near.glb')
            lo,hi=glb_bounds(p)
            with p.open('rb')as f:
                f.seek(12);n,kind=struct.unpack('<II',f.read(8));assert kind==0x4e4f534a;data=json.loads(f.read(n))
            triangles=sum(data['accessors'][p['indices']]['count']//3 for mesh in data['meshes']for p in mesh['primitives'])
            self.models[alias]=dict(file=str(p.relative_to(ROOT)),sha256=sha(p),bounds_min=lo,bounds_max=hi,triangles=triangles,grounding=grounding,collision=collision)
        return self.models[alias]
    def add(self,id,alias,x,z,yaw=0,scale=1,burial=0,elevation=0,zone='frontage',parent=None):
        yaw=(yaw+180)%360-180
        model=self.model(alias);lo,hi=model['bounds_min'],model['bounds_max'];angle=math.radians(yaw);c,s=math.cos(angle),math.sin(angle)
        assert -195<x<-98 and -185<z<145,(id,'outside district')
        hs=[self.terrain.height(x+c*lx+s*lz,z-s*lx+c*lz)for lx in[lo[0]*scale+(hi[0]-lo[0])*scale*i/4 for i in range(5)]for lz in[lo[2]*scale+(hi[2]-lo[2])*scale*i/4 for i in range(5)]]
        assert max(hs)-min(hs)<.05 and not self.terrain.wet(x,z),(id,'not level dry terrace',min(hs),max(hs))
        y=self.terrain.height(x,z)-burial
        if model['grounding']=='Support':y=min(hs+[y+burial])-burial-.05
        if model['grounding']=='GroundTop':y+=.02-hi[1]*scale
        r=dict(id=id,alias=alias,zone=zone,local_xz=[x,z],world_xz=[ORIGIN[0]+x,ORIGIN[1]+z],grounded_y=y+elevation,yaw_degrees=yaw,scale=scale,burial_m=burial,elevation_m=elevation,terrain_min=min(hs),terrain_max=max(hs),bounds_min=lo,bounds_max=hi,parent=parent)
        self.records.append(r);return r


def populate(a):
    a.add('copper_lantern_tavern','village_tavern',-145,-82,zone='tavern')
    a.add('residents_open_shelter','castle_residents_shelter',-166,15,zone='residents')
    # Two varied frontages frame a connected public street rather than filling
    # circulation with scattered buildings. Purchased exteriors remain closed.
    houses=[('north_baker','purchased_house_a',-118,-160,-90,.95),('north_weaver','purchased_house_b',-118,-134,-90,.90),
            ('north_tailor','purchased_house_e',-118,-108,-90,1.1),('inn_neighbor','purchased_house_a',-118,-82,-90,.90),
            ('cobbler','purchased_house_b',-118,-57,-90,.85),('potter','purchased_house_a',-118,-16,-90,.90),
            ('draper','purchased_house_e',-118,13,-90,1.1),('east_joiner','purchased_house_a',-118,39,-90,.90),
            ('east_salter','purchased_house_b',-118,73,-90,.85),('south_tallow','purchased_house_e',-118,108,-90,1.0),
            ('west_brewer','purchased_house_c',-163,-164,90,.80),('west_renter','purchased_house_a',-163,-141,90,.90),
            ('west_inn_neighbor','purchased_house_e',-163,-82,90,1.0),('west_roper','purchased_house_a',-163,-57,90,.85),
            ('west_carter','purchased_house_b',-163,-16,90,.85),('west_carpenter','purchased_house_e',-163,38,90,1.0),
            ('west_spinner','purchased_house_e',-163,73,90,.90),('west_chandler','purchased_house_a',-163,110,90,.85)]
    # Compact middle-block frontages bring both streets within a real town
    # envelope, leaving measured side passages and all public crossings open.
    houses += [('north_dyer','purchased_house_e',-141,-158,90,.8),
               ('tavern_candle_shop','purchased_house_e',-141,-143,90,.8),
               ('east_basketmaker','purchased_house_e',-141,-46,90,.8),
               ('east_roper','purchased_house_a',-141,-20,90,.75),
               ('west_caskmaker','purchased_house_e',-141,9,90,.8),
               ('west_mender','purchased_house_e',-141,38,90,.8),
               ('courtyard_neighbor','purchased_house_e',-141,70,90,.85),
               ('shelter_neighbor','purchased_house_e',-141,104,90,.8)]
    for id,alias,x,z,yaw,scale in houses:a.add(id,alias,x,z,yaw,scale,zone='housing')
    # Stoops use the existing foundation as ground; belongings sit beside door
    # approaches and never within the reserved through-lanes.
    for i,(id,alias,x,z,yaw,scale)in enumerate(houses):
        front=a.model(alias)['bounds_max'][2]*scale
        sign=1 if math.sin(math.radians(yaw))>0 else -1
        fx=x+sign*(front+.85)
        a.add(id+'_water','starting_bucket',fx,z+2.3,11*i,.85,zone='frontage',parent=id)
        a.add(id+'_lamp','purchased_prop_lamp',x+sign*(front-.12),z+1.6,yaw, .38,elevation=2.1,zone='frontage',parent=id)
        if i%3==0:a.add(id+'_cask','purchased_prop_barrel_small',fx,z-2.3,i*19,.85,zone='frontage',parent=id)
        elif i%3==1:a.add(id+'_box','purchased_crate',fx,z-2.1,i*9,.72,zone='frontage',parent=id)
        else:a.add(id+'_wood','purchased_prop_logs',fx,z-2.7,90,.65,zone='frontage',parent=id)
        if i%2==0:a.add(id+'_door_basket','purchased_prop_sack',fx+sign*.7,z+2.6,i*31,.65,zone='frontage',parent=id)
    # Square furniture is outside the straight approach to the 2.4m-wide door.
    for i,(x,z,yaw)in enumerate([(-154,-64,90),(-154,-56,90),(-137,-56,-90)]):
        a.add(f'tavern_outdoor_table_{i}','starting_table',x,z,yaw,.85,zone='square')
        for j,side in enumerate([-1,1]):a.add(f'tavern_outdoor_seat_{i}_{j}','castle_yard_bench',x+side*1.8,z,yaw,.75,zone='square')
        for j,(dx,dz,alias)in enumerate([(-.3,0,'purchased_goods_tankard'),(.35,.3,'purchased_goods_bread')]):a.add(f'tavern_table_top_{i}_{j}',alias,x+dx,z+dz,60*j,.85,elevation=.6375,zone='square',parent=f'tavern_outdoor_table_{i}')
    a.add('tavern_water_well','village_well',-158,-48,0,.8,zone='square')
    a.add('tavern_ale_cart','village_cart',-159,-96,0,1,zone='tavern_delivery')
    for i,(x,z)in enumerate([(-160,-92),(-161.3,-92),(-160.2,-90.4)]):a.add(f'tavern_delivery_cask_{i}','purchased_barrel',x,z,i*17,.85,zone='tavern_delivery')
    for i,(x,z)in enumerate([(-159,-99),(-160.2,-99),(-159.3,-100.2)]):a.add(f'tavern_delivery_box_{i}','purchased_crate',x,z,i*13,.85,zone='tavern_delivery')
    # Stalls are shopfronts, not fake enterable buildings with box collisions.
    for i,(x,z,yaw,alias)in enumerate([(-139,-103,180,'purchased_shop_a'),(-151,-103,180,'purchased_shop_d'),(-170,-101,0,'purchased_shop_b')]):
        a.add(f'west_local_stall_{i}',alias,x,z,yaw,.9,zone='trade')
        a.add(f'west_stall_goods_{i}','purchased_prop_sack',x+3,z+1.5,i*30,.8,zone='trade')
    # Four distinct clean resting pads under a weatherproof open shelter. No
    # obstructive clutter occupies their .7m personal root clearance or aisle.
    for i,x in enumerate([-171.5,-167.8,-164.2,-160.5]):
        a.add(f'resident_bedroll_{i}','castle_residents_bedroll',x,12.65,90,1,elevation=.015,zone='residents')
    for i,(x,z)in enumerate([(-172.3,15),(-166,13.8),(-159.2,14.8)]):
        a.add(f'resident_bundle_{i}','purchased_prop_sack',x,z,31*i,.5,elevation=.015,zone='residents')
    a.add('residents_shared_water','starting_bucket',-158.3,18.3,20,.9,zone='residents')
    a.add('residents_food_box','purchased_crate',-174.2,17.9,0,.55,zone='residents')
    a.add('residents_food_bread','purchased_goods_bread',-174.2,17.9,17,.8,elevation=.55*.9931,zone='residents')
    # Small work pockets and delivery clues make intersections legible.
    for i,(x,z)in enumerate([(-136.5,100),(-136.5,102),(-136.5,104)]):a.add(f'cooper_delivery_{i}','purchased_prop_barrel_small',x,z,23*i,.85,zone='work')
    a.add('cooper_rope','purchased_prop_rope',-136.5,98.8,32,.9,zone='work')
    a.add('joiner_logpile','purchased_prop_logs',-171,76,90,.8,zone='work')
    a.add('joiner_stump','starting_stump',-171,72,0,1,zone='work')
    a.add('joiner_axe','starting_axe',-171,72,25,.8,elevation=.65,zone='work')
    a.add('high_street_light','purchased_torch_pole',-136,16,0,.42,zone='street_detail')
    a.add('residents_courtyard_light','purchased_torch_pole',-176,22,0,.42,zone='street_detail')
    for i,(x,z)in enumerate([(-140,-31),(-141,50),(-140,119)]):a.add(f'wayfinding_banner_{i}','village_banner',x,z,90,.7,zone='street_detail')
    # Planted frontage strips are kept outside all circulation and old gardens.
    for i,(id,alias,x,z,yaw,scale)in enumerate(houses):
        for j in range(4):
            front=a.model(alias)['bounds_max'][2]*scale
            sign=1 if math.sin(math.radians(yaw))>0 else -1
            xx=x+sign*(front+.55);zz=z-3.5+j*.58
            a.add(f'{id}_flowers_{j}','starting_celandine'if j%2 else'starting_wildflower',xx,zz,i*13+j*51,.7+(j%2)*.1,zone='planting')


def path_distance(a,b,r,model):
    x,z=r['local_xz'];t=math.radians(r['yaw_degrees']);c,s=math.cos(t),math.sin(t)
    aa=[c*(a[0]-x)-s*(a[1]-z),s*(a[0]-x)+c*(a[1]-z)]
    bb=[c*(b[0]-x)-s*(b[1]-z),s*(b[0]-x)+c*(b[1]-z)]
    if model['collision']=='Trunk':
        # Canopies intentionally frame streets; .5m source half-width exceeds
        # the existing registry's largest .42m trunk collider radius.
        return rectangle_distance(aa,bb,[-.5*r['scale']]*2,[.5*r['scale']]*2)
    return rectangle_distance(aa,bb,[model['bounds_min'][0]*r['scale'],model['bounds_min'][2]*r['scale']],[model['bounds_max'][0]*r['scale'],model['bounds_max'][2]*r['scale']])


def structural_separation(a):
    """Full-source rotated rectangles, not merely route/capsule exclusion."""
    def corners(record,model):
        lo,hi=model['bounds_min'],model['bounds_max'];scale=record['scale']
        if model['collision']=='Trunk':lo=[-.5,0,-.5];hi=[.5,0,.5]
        c,s=math.cos(math.radians(record['yaw_degrees'])),math.sin(math.radians(record['yaw_degrees']))
        x,z=record['local_xz']
        return [(x+c*lx*scale+s*lz*scale,z-s*lx*scale+c*lz*scale)for lx,lz in[(lo[0],lo[2]),(hi[0],lo[2]),(hi[0],hi[2]),(lo[0],hi[2])]]
    minimum=math.inf;closest=None;count=0;seen=set()
    for house in [r for r in a.records if r['zone']=='housing']:
        m=a.model(house['alias']);hc=corners(house,m)
        for other in a.previous+a.records:
            if other is house:continue
            other_model=a.model(other['alias'])
            if other_model['collision']=='None':continue
            pair=tuple(sorted([house['id'],other['id']]))
            if pair in seen:continue
            seen.add(pair);oc=corners(other,other_model);gap=-math.inf
            for angle in[house['yaw_degrees'],other['yaw_degrees']]:
                c,s=math.cos(math.radians(angle)),math.sin(math.radians(angle))
                for axis in[(c,-s),(s,c)]:
                    h=[x*axis[0]+z*axis[1]for x,z in hc];o=[x*axis[0]+z*axis[1]for x,z in oc]
                    gap=max(gap,min(o)-max(h),min(h)-max(o))
            assert gap>.02,(house['id'],other['id'],'full-source structural OBB overlap/contact',gap)
            count+=1
            if gap<minimum:minimum=gap;closest=pair
    return dict(houses=26,pairs_tested=count,minimum_separating_axis_gap_m=minimum,closest_pair=list(closest),
                scope='Each full source house roof/wall OBB versus all other houses, tavern/shelter and rigid old/new props. Noncolliding facade lamps/flowers are intentional attachments; trees use conservative trunks, preserving canopy framing.')


def proof(a):
    # Full-source OBB guard is intentionally stricter than source-specific
    # compound colliders, except the genuinely enterable tavern/shelter.
    minimum=math.inf;probes=0;street_minimum=math.inf
    route_segments=list(zip(TOUR,TOUR[1:]))+list(zip(WALKERS,WALKERS[1:]+WALKERS[:1]))
    for r in a.previous+a.records:
        m=a.model(r['alias'])
        if m['collision']=='None'or r['alias']in NEW_ALIASES:continue
        for p,q in route_segments:
            distance=path_distance(p,q,r,m)-.4
            assert distance>0,(r['id'],'route OBB obstruction',p,q,distance)
            minimum=min(minimum,distance)
        for id,p,q,width,paving in ROADS:
            distance=path_distance(p,q,r,m)-width-.4
            assert distance>0,(r['id'],id,'street corridor obstruction',distance)
            street_minimum=min(street_minimum,distance)
    low=math.inf;high=-math.inf;grade=0
    for p,q in route_segments:
        distance=math.dist(p,q);n=max(1,math.ceil(distance/.25));prior=None
        for i in range(n+1):
            t=i/n;x=p[0]+t*(q[0]-p[0]);z=p[1]+t*(q[1]-p[1]);y=a.terrain.height(x,z);probes+=1
            assert not a.terrain.wet(x,z)
            low=min(low,y);high=max(high,y)
            if prior is not None:grade=max(grade,abs(y-prior)/(distance/n))
            prior=y
    assert grade<.05
    return dict(benchmark_id='castle-streets',length_m=sum(math.dist(p,q)for p,q in zip(TOUR,TOUR[1:])),points_local_xz=TOUR,points_world_xz=[[ORIGIN[0]+x,ORIGIN[1]+z]for x,z in TOUR],sampled_step_m=.25,height_min_m=low,height_max_m=high,max_absolute_grade=grade,clear_capsule_radius_m=.4,source_obb_extra_clearance_m=minimum,street_full_width_extra_clearance_m=street_minimum,terrain_probes=probes,note='Actual source OBB guard for all old/new rigid exteriors. Enterable tavern/shelter require the separate production compound-collider CPU fixture.')


def outputs(a):
    prefix=protected();counts=Counter(r['alias']for r in a.records);combined=Counter()
    for p in PROTECTED:combined.update(s.split()[0]for s in p.read_text().splitlines()if s.strip()and not s.lstrip().startswith('#'))
    combined.update(counts);assert len(a.records)<650 and sum(combined.values())<=8192 and max(combined.values())<=4096
    text='# Additive western castle streets; tools/build_castle_streets.py.\n'
    for r in a.records:
        x,z=r['world_xz'];text+='# '+r['id']+'\n'+f"{r['alias']} {x/WIDTH+.5:.14f} {z/DEPTH+.5:.14f} {r['yaw_degrees']:.6f} {r['scale']:.6f} {r['burial_m']:.6f}"
        if r['elevation_m']:text+=f" {r['elevation_m']:.6f}"
        text+='\n'
    pads=[dict(id='tavern_square',local_xz=[-145,-62],half_extents=[11,10],yaw=0,paving=.85,grass=1)]
    for id,x,z,hx,hz in [('high_street_west_sidewalk',-136,-21,2.8,145),('high_street_east_sidewalk',-124.5,-21,3,145),('residents_lane_west_sidewalk',-156.1,45,3,79),('residents_lane_east_sidewalk',-146,45,2.5,79)]:
        pads.append(dict(id=id,local_xz=[x,z],half_extents=[hx,hz],yaw=0,paving=.70,grass=1))
    for r in a.records:
        if r['zone']=='planting':continue
        lo,hi=r['bounds_min'],r['bounds_max'];s=r['scale']
        pads.append(dict(id=r['id'],local_xz=r['local_xz'],half_extents=[max(abs(lo[0]),abs(hi[0]))*s+.85,max(abs(lo[2]),abs(hi[2]))*s+.85],yaw=math.radians(r['yaw_degrees']),paving=.45 if r['zone']in['housing','frontage','square']else .15,grass=1))
    roads=[dict(id=id,a=p,b=q,half_width_m=w,paving=paving)for id,p,q,w,paving in ROADS]
    ground=dict(origin_world_xz=list(ORIGIN),bounds_local_xz=[-194,-181,-98,140],height_edits=False,roads=roads,pads=pads)
    cameras=[]
    for id,name,p,target,pitch in CAMERAS:
        x,z=p;wx,wz=ORIGIN[0]+x,ORIGIN[1]+z
        cameras.append(dict(id=id,name=name,local_xz=p,world_xz=[wx,wz],map_uv=[wx/WIDTH+.5,wz/DEPTH+.5],yaw_radians=math.atan2(target[0]-x,-(target[1]-z)),pitch_radians=pitch,eye_height_m=1.75))
    route=proof(a);separation=structural_separation(a)
    layout=dict(schema_version=1,title='Valdoa | West High Street and Copper Lantern',origin_world_xz=list(ORIGIN),ground=ground,records=a.records,tavern=dict(center_local_xz=[-145,-82],yaw=0,door_local_xz=[-145,-74],door_width_m=2.5,finished_floor_above_ground_m=.015),residents=dict(shelter_center_local_xz=[-166,15],shelter_yaw=0,roots_local_xz=[[-169.5,13.7],[-162.5,13.7],[-171.5,16.5],[-160.5,16.5]],personal_root_clearance_m=.7),street_walker_route_local_xz=WALKERS,inspection_cameras=cameras,route=route)
    layout['structural_separation']=separation
    sockets={r['alias']:r['flame_socket_local_m']for r in json.loads((MODELS/'purchased_torches_manifest.json').read_text())['assets']if r['lod']=='near'}
    lights=[dict(id='tavern_hearth',local_x=-138.5,local_z=-88.45,base_y=24.35,height=.48,width=.22,intensity=28,seed=17401),
            dict(id='tavern_hanging_lamp',local_x=-149.6,local_z=-82.7,base_y=26.82,height=.22,width=.10,intensity=18,seed=25320)]
    for e in lights:e.update(receiving_bounds_local_xz=[-155,-90,-135,-74],receiving_y=[23.8,32.3],plane=[0,0,1,90]if e['id']=='tavern_hearth'else[0,0,0,1])
    for id in ['high_street_light','residents_courtyard_light']:
        r=next(r for r in a.records if r['id']==id)
        lights.append(dict(id=id,local_x=r['local_xz'][0],local_z=r['local_xz'][1],base_y=r['grounded_y']+sockets['pole'][1]*r['scale'],height=.5,width=.16,intensity=30,seed=33239+len(lights)*7919))
        lights[-1].update(receiving_bounds_local_xz=[-194,-181,-98,140],receiving_y=[0,1000],plane=[0,0,0,1])
    layout['lights']=lights
    inc=['/* Generated additive village-local material/ecology masks. No height edits. */','static const VillageRoad castle_street_roads[]={']
    for r in roads:inc.append('    {'+','.join(f'{v:.9f}'for v in r['a']+r['b']+[r['half_width_m'],r['paving']])+'},')
    inc+=['};','static const VillageRect castle_street_pads[]={']
    for r in pads:inc.append('    {'+','.join(f'{v:.9f}'for v in r['local_xz']+r['half_extents']+[math.cos(r['yaw']),math.sin(r['yaw']),r['paving'],r['grass']])+'},')
    inc+=['};','']
    views='/* Western castle-town inspection views. */\n'+''.join(f'    {{"{c["id"]}","{c["name"]}",{c["map_uv"][0]:.14f},{c["map_uv"][1]:.14f},{c["yaw_radians"]:.9f}f,{c["pitch_radians"]:.9f}f,1.75f}},\n'for c in cameras)
    route_inc='/* Closed castle streets return walk, village-local XZ. */\nstatic const double castle_streets_route[][2]={\n'+''.join(f'    {{{x:.12f},{z:.12f}}},\n'for x,z in TOUR)+'};\n'
    torches=['/* Four generated practical emitters: village-local XZ, absolute Y. */','static constexpr VillageTorchEmitter castle_street_torch_emitters[]={']
    for e in lights:
        clip=','.join(f'{v:.9f}f'for v in e['plane']+e['receiving_bounds_local_xz']+e['receiving_y'])
        torches.append(f'    {{{e["local_x"]:.9f},{e["base_y"]:.9f},{e["local_z"]:.9f},{e["height"]}f,{e["width"]}f,{e["intensity"]}.f,{e["seed"]}u,{clip}}}, // {e["id"]}')
    torches+=['};','']
    files={SCENERY/(STEM+'_instances.txt'):text,SCENERY/(STEM+'_layout.json'):json.dumps(layout,indent=2)+'\n',SCENERY/(STEM+'_ground.json'):json.dumps(ground,indent=2)+'\n',ROOT/'engine/world_castle_streets_ground.inc':'\n'.join(inc),ROOT/'engine/world_castle_streets_views.inc':views,ROOT/'engine/world_castle_streets_route.inc':route_inc,ROOT/'engine/world_castle_streets_torches.inc':'\n'.join(torches)}
    layout_text=files[SCENERY/(STEM+'_layout.json')]
    manifest=dict(schema_version=1,layout_source=STEM+'_layout.json',instances_source=STEM+'_instances.txt',layout_sha256=hashlib.sha256(layout_text.encode()).hexdigest(),instances_sha256=hashlib.sha256(text.encode()).hexdigest(),record_count=len(a.records),combined_record_count=sum(combined.values()),counts=dict(sorted(counts.items())),largest_combined_alias_batch=max(combined.values()),protected_sources=prefix,origin_world_xz=list(ORIGIN),models={k:v for k,v in a.models.items()if k in counts},route=route,structural_separation=separation,inspection_cameras=cameras,lights=lights,performance=dict(new_asset_families=list(NEW_ALIASES),near_source_triangles_upper_bound=sum(a.models[r['alias']]['triangles']for r in a.records),note='Existing imported near/mid assets and shared texture images reused. Native integrated FPS remains separately measured.'),ground_note='Existing village cache only; no height changes, no extra terrain texture.',validation=dict(existing_scenery_sources_preserved=True,production_compound_collision_fixture_required=True))
    files[SCENERY/(STEM+'_manifest.json')]=json.dumps(manifest,indent=2)+'\n'
    svg=['<svg xmlns="http://www.w3.org/2000/svg" viewBox="-210 -210 125 370" width="625" height="1850">','<rect x="-210" y="-210" width="125" height="370" fill="#e4ddc8"/>','<path d="M-205,-205V155" stroke="#756959" stroke-width="4"/>']
    for id,p,q,w,paving in ROADS:svg.append(f'<path d="M{p[0]},{p[1]} L{q[0]},{q[1]}" stroke="#b3a48a" stroke-width="{2*w}"/>')
    for r in a.previous+a.records:
        m=a.model(r['alias']);lo,hi=m['bounds_min'],m['bounds_max'];s=r['scale'];x,z=r['local_xz'];fill='#748369'if r.get('zone')=='planting'else'#aa8062'if r.get('zone')in['housing','tavern']else'#9c957e'if r in a.previous else'#c1a56e'
        svg.append(f'<rect x="{lo[0]*s}" y="{lo[2]*s}" width="{(hi[0]-lo[0])*s}" height="{(hi[2]-lo[2])*s}" transform="translate({x},{z}) rotate({-r["yaw_degrees"]})" fill="{fill}" opacity=".8" stroke="#534e40" stroke-width=".08"><title>{html.escape(r["id"])}</title></rect>')
    for text,x,z in [('West High Street',-130,-183),('Copper Lantern',-145,-95),('Residents courtyard',-166,8),('North Market Street',-145,-31),('Coopers Crossing',-145,59),('Laundry Cut',-145,89)]:svg.append(f'<text x="{x}" y="{z}" text-anchor="middle" font-size="2.9">{text}</text>')
    svg.append('</svg>');files[SCENERY/(STEM+'_layout.svg')]='\n'.join(svg)+'\n'
    return files,manifest


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--atlas-library',type=Path);p.add_argument('--validate-only',action='store_true');p.add_argument('--force',action='store_true');args=p.parse_args()
    before=protected();a=Author(Terrain(args.atlas_library));populate(a);files,m=outputs(a)
    for path,text in files.items():
        if args.validate_only:assert path.read_text()==text,('Stale authored output',path)
        elif not path.exists()or path.read_text()!=text:
            if path.exists()and not args.force:raise SystemExit(f'{path} differs; use --force for deliberate regeneration.')
            path.write_text(text)
    assert before==protected()
    print(f'PASS castle streets: {m["record_count"]} additive records; {m["combined_record_count"]} combined; protected5sources unchanged; {m["route"]["length_m"]:.3f}m tour; maxgrade {m["route"]["max_absolute_grade"]:.6f}; source OBB extra route clearance {m["route"]["source_obb_extra_clearance_m"]:.3f}m.')


if __name__=='__main__':main()
