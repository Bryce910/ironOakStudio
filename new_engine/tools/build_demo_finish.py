#!/usr/bin/env python3
"""Author the final walking-demo street layer without changing earlier sources.

The recipe lays out named street-front lots and small purposeful work areas.
It reads source GLB geometry for rotated bounds, uses the CPU production atlas,
reserves existing roads and actor routes, and records every rejected lot. There
is no global random scatter or density multiplier. All models/images are reused.
"""
import argparse
from collections import Counter
import ctypes as C
import hashlib
import json
import math
from pathlib import Path
import re
import struct
import tempfile

from build_starting_area import Atlas, glb_bounds, rectangle_distance
from build_castle_streets import ROADS as WEST_ROADS, TOUR as WEST_TOUR
from build_village_farms import ROADS as FARM_ROADS, PLOTS, WALK as FARM_WALK, APPROACH
from build_village_hero_grove import source_routes
from demo_rainforest_finish import append_rainforest

ROOT=Path(__file__).resolve().parents[1]
DIRECTORY=ROOT/'projects/valwood_world/scenery'
MODELS=ROOT/'projects/valwood_world/models'
ORIGIN=(-1310.4,-10207.58474745458)
WIDTH=100800.;DEPTH=WIDTH*2818/5009
SOURCE_NAMES=('authored','starting_area','rainforest','castle_village','tanthoon_bandit_camp','castle_streets','bep_environment')
SOURCES=[DIRECTORY/(n+'_instances.txt')for n in SOURCE_NAMES]
OUTPUT=DIRECTORY/'demo_finish_instances.txt'
MANIFEST=DIRECTORY/'demo_finish_manifest.json'
GROUND=ROOT/'engine/world_demo_finish_ground.inc'
VIEWS=ROOT/'engine/world_demo_finish_views.inc'
ARCADE_PATH=[[-95,36],[-67,36],[-66.8,28],[-66.8,16],[-68.2,4],[-68.2,-8],[-68.2,-17],[-82,-20],[-95,-20]]
NEW_ROADS=[dict(id=f'bakers_arcade_{i}',a=a,b=b,half_width=1.3,paving=.80)for i,(a,b)in enumerate(zip(ARCADE_PATH,ARCADE_PATH[1:]))]


def sha(path):
    h=hashlib.sha256()
    with Path(path).open('rb')as f:
        for data in iter(lambda:f.read(1024*1024),b''):h.update(data)
    return h.hexdigest()


def protected():
    return [dict(file=str(p.relative_to(ROOT)),sha256=sha(p),records=sum(bool(l.strip())and not l.lstrip().startswith('#')for l in p.read_text().splitlines()))for p in SOURCES]


def corners(r,m,trunk=False):
    lo,hi=m['bounds_min'],m['bounds_max'];scale=r['scale']
    if trunk:lo=[-.65,0,-.65];hi=[.65,0,.65]
    t=math.radians(r['yaw_degrees']);c,s=math.cos(t),math.sin(t);x,z=r['local_xz']
    return [(x+c*lx*scale+s*lz*scale,z-s*lx*scale+c*lz*scale)for lx,lz in[(lo[0],lo[2]),(hi[0],lo[2]),(hi[0],hi[2]),(lo[0],hi[2])]]


def sat_gap(a,ma,b,mb):
    aa=corners(a,ma,ma['collision']=='Trunk');bb=corners(b,mb,mb['collision']=='Trunk');gap=-math.inf
    for angle in[a['yaw_degrees'],b['yaw_degrees']]:
        c,s=math.cos(math.radians(angle)),math.sin(math.radians(angle))
        for ax,az in[(c,-s),(s,c)]:
            ap=[x*ax+z*az for x,z in aa];bp=[x*ax+z*az for x,z in bb]
            gap=max(gap,min(bp)-max(ap),min(ap)-max(bp))
    return gap


def path_gap(r,m,a,b):
    x,z=r['local_xz'];t=math.radians(r['yaw_degrees']);c,s=math.cos(t),math.sin(t)
    aa=[c*(a[0]-x)-s*(a[1]-z),s*(a[0]-x)+c*(a[1]-z)]
    bb=[c*(b[0]-x)-s*(b[1]-z),s*(b[0]-x)+c*(b[1]-z)]
    lo,hi=m['bounds_min'],m['bounds_max'];scale=r['scale']
    return rectangle_distance(aa,bb,[lo[0]*scale,lo[2]*scale],[hi[0]*scale,hi[2]*scale])


class Author:
    def __init__(self,atlas):
        self.atlas=atlas;self.records=[];self.models={};self.geometry={};self.rejected=[];self.supports=[];self.probes=0;self.obb_checks=0
        self.aliases={m[1]:(m[2],m[3],m[4])for m in re.finditer(r'\{"([^"]+)",\s*"([^"]+)"[^\n]+Grounding::(\w+),\s*Collision::(\w+)',(ROOT/'engine/world_scenery.cpp').read_text())}
        self.atlas.api.world_atlas_local_water.argtypes=[C.c_double,C.c_double,C.c_void_p,C.c_void_p];self.atlas.api.world_atlas_local_water.restype=C.c_bool
        layout=json.loads((DIRECTORY/'castle_village_layout.json').read_text())
        self.roads=[(r['id'],r['a'],r['b'],r['half_width'])for r in layout['roads']]
        self.roads +=[(r[0],r[1],r[2],r[3])for r in WEST_ROADS]
        self.roads +=[(r['id'],r['a'],r['b'],r['half_width'])for r in FARM_ROADS+NEW_ROADS]
        self.routes=[]
        for id,points in [('castle_benchmark',layout['benchmark_route_xz']),('castle_streets',WEST_TOUR),('farm',FARM_WALK),('farm_approach',APPROACH)]:
            self.routes +=[(id,a,b)for a,b in zip(points,points[1:])]
        for source in ['village_demo_district.json']:
            data=json.loads((DIRECTORY/source).read_text())
            if'demo_route_xz'in data:self.routes +=[(source,a,b)for a,b in zip(data['demo_route_xz'],data['demo_route_xz'][1:])]
        for source in ['world_village_population.cpp','world_village_pets.cpp']:
            self.routes +=[(source,a,b)for a,b in source_routes(source)]
        # Stationary actors have space as well as walkers. Formula guards tie
        # the setup to the production source instead of quietly assuming its
        # anchors still match if population authoring is changed later.
        population=(ROOT/'engine/world_village_population.cpp').read_text()
        assert 'i<4?-35.:35.,-72.-(i%4)*5.'in population
        assert 'i<4?-30.:30.,-64.-(i%4)*5.'in population
        assert 'i<3?-25.35:25.35,-6.+(i%3)*15.'in population
        assert 'i<2?-13.35:13.35,i%2?24.:-6.'in population
        anchors=[(f'archer_original_{i}',(-35 if i<4 else 35,-72-(i%4)*5))for i in range(8)]
        anchors +=[(f'archer_added_{i}',(-30 if i<4 else 30,-64-(i%4)*5))for i in range(8)]
        anchors +=[(f'trader_original_{i}',(-25.35 if i<3 else 25.35,-6+(i%3)*15))for i in range(6)]
        anchors +=[(f'trader_inner_{i}',(-13.35 if i<2 else 13.35,24 if i%2 else -6))for i in range(4)]
        text=population.split('constexpr Point new_trader_points[]={',1)[1].split('};',1)[0]
        anchors +=[(f'trader_new_{i}',(float(x),float(z)))for i,(x,z)in enumerate(re.findall(r'\{(-?[\d.]+),(-?[\d.]+)\}',text))]
        text=(ROOT/'engine/world_village_life_data.inc').read_text()
        for name,x,z,walker in re.findall(r'\{"([^"]+)","[^"]+","[^"]+",\d+,(-?[\d.]+),(-?[\d.]+),[^,]+,[^,]+,\d+,[^,]+,(true|false),',text):
            if walker=='false':anchors.append(('life_'+name,(float(x),float(z))))
        pets=(ROOT/'engine/world_village_pets.cpp').read_text().split('constexpr Point stations[]=',1)[1].split(';',1)[0]
        anchors +=[(f'pet_station_{i}',(float(x),float(z)))for i,(x,z)in enumerate(re.findall(r'\{(-?[\d.]+),(-?[\d.]+)\}',pets))]
        assert len(anchors)==51,('Missing stationary production actor anchors',len(anchors))
        self.stationary_anchors=anchors
        self.routes +=[('stationary_'+name,p,p)for name,p in anchors]
        self.previous=[]
        for source in SOURCES:
            for number,line in enumerate(source.read_text().splitlines(),1):
                p=line.split('#',1)[0].split()
                if not p:continue
                x=(float(p[1])-.5)*WIDTH-ORIGIN[0];z=(float(p[2])-.5)*DEPTH-ORIGIN[1]
                if abs(x)>245 or abs(z)>245:continue
                model=self.model(p[0])
                if model['collision']=='None':continue
                self.previous.append(dict(id=f'{source.name}:{number}',alias=p[0],local_xz=[x,z],yaw_degrees=float(p[3]),scale=float(p[4]),burial_m=float(p[5]),elevation_m=float(p[6])if len(p)>6 else 0))

    def model(self,alias):
        if alias not in self.models:
            stem,grounding,collision=self.aliases[alias]
            candidates=[MODELS/(stem+suffix+'.glb')for suffix in ['_near','','_mid']]
            near=next((p for p in candidates if p.is_file()),None);assert near is not None,(alias,stem)
            paths=[near]
            if near.stem.endswith('_near'):
                mid=near.with_name(near.stem[:-5]+'_mid.glb')
                if mid.is_file():paths.append(mid)
            lods=[]
            for path in paths:
                lo,hi=glb_bounds(path)
                with path.open('rb')as f:f.seek(12);size,kind=struct.unpack('<II',f.read(8));assert kind==0x4e4f534a;data=json.loads(f.read(size))
                tris=sum(data['accessors'][p['indices']]['count']//3 if'indices'in p else data['accessors'][p['attributes']['POSITION']]['count']//3 for mesh in data['meshes']for p in mesh['primitives'])
                lods.append(dict(file=str(path.relative_to(ROOT)),sha256=sha(path),bounds_min=lo,bounds_max=hi,triangles=tris))
            self.models[alias]=dict(file=str(near.relative_to(ROOT)),grounding=grounding,collision=collision,bounds_min=[min(p['bounds_min'][i]for p in lods)for i in range(3)],bounds_max=[max(p['bounds_max'][i]for p in lods)for i in range(3)],lods=lods)
        return self.models[alias]

    def reject(self,r,reason,detail):
        self.rejected.append(dict(id=r['id'],alias=r['alias'],local_xz=r['local_xz'],reason=reason,detail=detail));return None

    def add(self,id,alias,x,z,yaw=0,scale=1,burial=0,elevation=0,zone='street_detail',parent=None,required=True):
        r=dict(id=id,alias=alias,local_xz=[x,z],world_xz=[ORIGIN[0]+x,ORIGIN[1]+z],yaw_degrees=(yaw+180)%360-180,scale=scale,burial_m=burial,elevation_m=elevation,zone=zone,parent=parent)
        m=self.model(alias)
        result=self.check(r,m)
        if result is None:
            if required:raise ValueError(self.rejected[-1])
            return None
        self.records.append(r);return r

    def check(self,r,m):
        x,z=r['local_xz'];lo,hi=m['bounds_min'],m['bounds_max'];scale=r['scale'];parent=r['parent']
        if not(-196<x<195 and -194<z<194):return self.reject(r,'outside_authored_town',None)
        # Full eave/prop rectangles, including LOD union, stay beyond full road
        # width + .4m pedestrian clearance. Existing source files are immutable.
        minimum_road=math.inf
        for id,a,b,w in self.roads:
            gap=path_gap(r,m,a,b)-w-.4;minimum_road=min(minimum_road,gap)
            if gap<.4:return self.reject(r,'public_road_obb',[id,gap])
        minimum_route=math.inf
        for id,a,b in self.routes:
            gap=path_gap(r,m,a,b)-.8;minimum_route=min(minimum_route,gap)
            if gap<0:return self.reject(r,'actor_or_walking_route_obb',[id,gap])
        for field in PLOTS:
            a,b,c,d=field['bounds_xz'];foot=dict(id='cultivated_field',alias='',local_xz=[(a+c)/2,(b+d)/2],yaw_degrees=0,scale=1)
            fm=dict(bounds_min=[-(c-a)/2,0,-(d-b)/2],bounds_max=[(c-a)/2,0,(d-b)/2],collision='Box')
            if sat_gap(r,m,foot,fm)<.65:return self.reject(r,'cultivated_field',field['id'])
        minimum_obb=math.inf
        for old in self.previous+self.records:
            if old['id']==parent:continue
            om=self.model(old['alias'])
            if om['collision']=='None':continue
            if math.dist(r['local_xz'],old['local_xz'])>70:continue
            self.obb_checks+=1;gap=sat_gap(r,m,old,om);minimum_obb=min(minimum_obb,gap)
            if gap<.10:return self.reject(r,'source_obb',[old['id'],gap])
        t=math.radians(r['yaw_degrees']);c,s=math.cos(t),math.sin(t);heights=[]
        for iz in range(5):
            for ix in range(5):
                px=(lo[0]+(hi[0]-lo[0])*ix/4)*scale;pz=(lo[2]+(hi[2]-lo[2])*iz/4)*scale
                wx=ORIGIN[0]+x+c*px+s*pz;wz=ORIGIN[1]+z-s*px+c*pz
                sample=self.atlas.sample(wx,wz);self.probes+=1
                if sample.land<.75 or sample.shore<.5 or sample.height<.1 or self.atlas.api.world_atlas_local_water(wx,wz,None,None):return self.reject(r,'wet_footprint',None)
                heights.append(sample.height)
        if max(heights)-min(heights)>.1:return self.reject(r,'nonlevel_ground',max(heights)-min(heights))
        r.update(ground_height_min_m=min(heights),ground_height_max_m=max(heights),minimum_road_obb_gap_m=minimum_road,minimum_actor_route_obb_gap_m=minimum_route,minimum_other_source_obb_gap_m=None if math.isinf(minimum_obb)else minimum_obb)
        return r

    def local(self,r,lx,lz):
        x,z=r['local_xz'];t=math.radians(r['yaw_degrees']);c,s=math.cos(t),math.sin(t)
        return x+c*lx+s*lz,z-s*lx+c*lz

    def wall_attachment(self,parent,id,alias,lx,height,scale,zone):
        mesh=parent['alias'];ps=parent['scale']
        if mesh not in self.geometry:self.geometry[mesh]=glb_bounds(ROOT/self.model(mesh)['file'],True)[3]
        px,py=lx/ps,height/ps;depths=[]
        for a,b,c in self.geometry[mesh]:
            ax,ay=b[0]-a[0],b[1]-a[1];bx,by=c[0]-a[0],c[1]-a[1];den=ax*by-ay*bx
            if abs(den)<1.e-10:continue
            dx,dy=px-a[0],py-a[1];u=(dx*by-dy*bx)/den;v=(ax*dy-ay*dx)/den
            if u>=-.0001 and v>=-.0001 and u+v<=1.0001:
                z=a[2]+u*(b[2]-a[2])+v*(c[2]-a[2])
                if z>0:depths.append(z)
        if not depths:
            self.rejected.append(dict(id=id,alias=alias,local_xz=list(parent['local_xz']),reason='no_actual_front_wall_triangle',detail=parent['id']));return None
        # The imported lamp/sign's inward face touches the measured facade.
        # This intentional ancestor attachment may share its source roof OBB;
        # all unrelated structures and walking clearances still apply.
        model=self.model(alias);front=max(depths)*ps-model['bounds_min'][2]*scale+.008
        x,z=self.local(parent,lx,front)
        record=self.add(id,alias,x,z,parent['yaw_degrees'],scale,elevation=height,zone=zone,parent=parent['id'],required=False)
        if record:self.supports.append(dict(id=id,parent=parent['id'],actual_source_wall_triangle_probe=True,parent_local_xy=[px,py],parent_front_z=max(depths),clearance_m=.008))
        return record

    def top(self,parent,lx,lz,minimum=0,maximum=4):
        alias=parent['alias'];scale=parent['scale'];lx/=scale;lz/=scale
        if alias not in self.geometry:self.geometry[alias]=glb_bounds(ROOT/self.model(alias)['file'],True)[3]
        heights=[]
        for a,b,c in self.geometry[alias]:
            ax,az=b[0]-a[0],b[2]-a[2];bx,bz=c[0]-a[0],c[2]-a[2];den=ax*bz-az*bx
            if abs(den)<1.e-10:continue
            dx,dz=lx-a[0],lz-a[2];u=(dx*bz-dz*bx)/den;v=(ax*dz-az*dx)/den
            if u>=-.0001 and v>=-.0001 and u+v<=1.0001:
                y=(a[1]+u*(b[1]-a[1])+v*(c[1]-a[1]))*scale
                if minimum<=y<=maximum:heights.append(y)
        assert heights,('No actual top supporting prop',parent['id'],lx,lz)
        # Parent and child Foundation share the atlas root. A Support parent
        # uses atlas-.05m, so account for its actual root before stacking.
        root_delta=-.05 if self.model(alias)['grounding']=='Support'else 0
        return max(heights)+parent['elevation_m']-parent['burial_m']+root_delta+.006

    def supported(self,id,alias,parent,lx,lz,yaw=0,scale=1,zone=None,support_interval=(0,4)):
        try:
            elevation=self.top(parent,lx,lz,*support_interval)
            model=self.model(alias);lo,hi=model['bounds_min'],model['bounds_max'];angle=math.radians(yaw);c,s=math.cos(angle),math.sin(angle)
            heights=[self.top(parent,lx+c*xx*scale+s*zz*scale,lz-s*xx*scale+c*zz*scale,*support_interval)for xx,zz in[(lo[0],lo[2]),(hi[0],lo[2]),(hi[0],hi[2]),(lo[0],hi[2])]]
            if max(heights+[elevation])-min(heights+[elevation])>.035:raise AssertionError('Uneven parent support')
        except AssertionError:
            self.rejected.append(dict(id=id,alias=alias,local_xz=list(self.local(parent,lx,lz)),reason='unsupported_source_footprint',detail=parent['id']));return None
        x,z=self.local(parent,lx,lz)
        r=self.add(id,alias,x,z,parent['yaw_degrees']+yaw,scale,elevation=elevation,zone=zone or parent['zone'],parent=parent['id'],required=False)
        if r:self.supports.append(dict(id=id,parent=parent['id'],actual_source_triangle_probe=True,local_xz=[lx,lz],support_elevation_m=elevation,clearance_m=.006))
        return r


def compose(a):
    houses=[]
    # A narrow working arcade on the vacant west side of the market. Facing
    # pairs frame a continuous passage; the existing central market stays open.
    lots=[]
    # Measured eaves sit about3m from the lane center, rather than the old
    # detached rows9m away. Bay offsets/staggering make a softly winding5–6m
    # facade corridor. The end frontage blocks the horizon; a west dogleg
    # preserves a real exit into the existing west-loop street.
    arcade_bays=[('arcade_baker','a',29,-1,.92,-66.8),
        ('arcade_roper','e',17,-1,.95,-66.8),
        ('arcade_dyer','b',5,-1,.90,-68.2),
        ('arcade_factor','a',-7,-1,.90,-68.2),
        ('arcade_clothier','b',31,1,.90,-66.53),
        ('arcade_spice_store','a',19,1,.90,-66.8),
        ('arcade_cobbler','e',7,1,.95,-67.85),
        ('arcade_candle_store','b',-5,1,.90,-68.2),
        ('arcade_apothecary','e',-16.8,1,.84,-68.2)]
    for name,variant,z,side,scale,center in arcade_bays:
        alias='purchased_house_'+variant;front=a.model(alias)['bounds_max'][2]*scale
        lots.append((name,alias,center+side*(front+3.05),z,90 if side<0 else-90,scale,'bakers_arcade'))
    lots.append(('arcade_end_bakery','purchased_house_a',-67.8,-25.4,0,.95,'arcade_endstop'))
    # Existing high-street blocks receive deliberately placed smaller infill
    # frontages between the detached original houses, keeping all crossings.
    for name,x,z,yaw,variant in [('weaver',-118,-147,-90,'e'),('tailor',-118,-121,-90,'e'),('bookbinder',-118,-95,-90,'a'),
        ('copper_mender',-118,-69,-90,'e'),('cobbler',-118,-2,-90,'b'),('draper',-118,26,-90,'e'),('tallow_shop',-118,96,-90,'e'),
        ('brewer',-163,-128,90,'e'),('carter',-163,-2,90,'e'),('soapmaker',-163,96,90,'e'),
        ('residents_threadshop',-141,-130,90,'e'),('residents_basketmaker',-141,-5,90,'e'),('residents_herbalist',-141,55,90,'e')]:
        lots.append((name,'purchased_house_'+variant,x,z,yaw,.78 if variant=='e'else .80,'western_infill'))
    # Opposite the east loop, shorter fronts link the market junction to the
    # residential street. Farm plots and every farm approach remain untouched.
    for i,(z,variant,scale)in enumerate([(-19,'e',.94),(-4,'a',.90),(12,'b',.88),(28,'e',.98),(42,'a',.84),(75,'e',.94),(99,'a',.86),(112,'e',.86)]):
        lots.append((f'east_loop_{i}','purchased_house_'+variant,106,z,-90,scale,'east_loop_frontages'))
    for name,alias,x,z,yaw,scale,zone in lots:
        h=a.add(name,alias,x,z,yaw,scale,zone=zone,required=False)
        if h:houses.append(h)
    assert len(houses)>=20,('Insufficient deliberate infill lots',len(houses),a.rejected)
    # Purposeful frontage possessions at the side of the door, not across it.
    # Ground-level props all pass source OBB and public-corridor checks.
    for i,h in enumerate(houses):
        m=a.model(h['alias']);front=m['bounds_max'][2]*h['scale'];width=(m['bounds_max'][0]-m['bounds_min'][0])*h['scale']
        for suffix,alias,lx,lz,sc in [('water','bep_prop_bucket',width*.30,front+.7,.8),('delivery','purchased_crate',-width*.30,front+.75,.60),
             ('bundle','purchased_prop_sack',-width*.30+.85,front+.85,.62)]:
            x,z=a.local(h,lx,lz);r=a.add(h['id']+'_'+suffix,alias,x,z,h['yaw_degrees']+i*11,sc,zone=h['zone'],required=False)
            if r and suffix=='delivery'and i%3==0:a.supported(h['id']+'_delivery_bread','purchased_goods_bread',r,0,0,17,.9)
        # Back-of-lot supplies establish a use for side gaps. Alternating
        # trade-specific loads avoid copying the same prop trio everywhere.
        back=m['bounds_min'][2]*h['scale']-.85
        for suffix,alias,lx,lz,sc in [('rear_cask','bep_prop_barrel',width*.28,back,.80),('rear_wood','purchased_prop_logs',-width*.27,back,.72)]:
            if(suffix=='rear_cask')!=(i%2==0):continue
            x,z=a.local(h,lx,lz);a.add(h['id']+'_'+suffix,alias,x,z,h['yaw_degrees']+11,sc,zone=h['zone'],required=False)
        # Tiny plants grow beside retained source walls, never in the paths.
        for j in range(3):
            x,z=a.local(h,width*.37+j*.18,front+.38)
            a.add(h['id']+f'_herb_{j}','starting_celandine'if j%2 else'starting_wildflower',x,z,31*i+67*j,.55+.06*j,zone='frontage_herbs',required=False)
    # New awnings edge the outer market against the row, clear of the cats'
    # route at x=-46 and the retained market walking spaces.
    stalls=[]
    # Small covered trades nest into the actual2.6–3.8m gaps between facades.
    # These roofed booths are solid counters, so the clear pedestrian core is
    # kept beyond their full source bounds rather than pretending to walk
    # through the imported canopy/counter collision box.
    arcade_stalls=[('entry_left_awning',-88.2,42,90,.65),
        ('entry_right_awning',-61.95,37.8,-90,.65),
        ('baker_flour_awning',-70.65,23.3,90,.55),
        ('roper_cordage_awning',-70.8,10.4,90,.60),
        ('dyer_cloth_awning',-72.0,-.8,90,.65),
        ('clothier_wares_awning',-62.55,25.2,-90,.65),
        ('spice_counter_awning',-62.6,13.3,-90,.52),
        ('cobbler_repair_awning',-64.1,.45,-90,.60),
        ('candle_counter_awning',-64.4,-10.65,-90,.62)]
    for name,x,z,yaw,scale in arcade_stalls:
        r=a.add(name,'purchased_shop_d',x,z,yaw,scale,zone='arcade_covered_trade',required=False)
        if r:stalls.append(r)
    for name,x,z,yaw,alias in [('east_loop_wares',113,63,0,'purchased_shop_c'),('east_loop_bread',122,63,0,'purchased_shop_d')]:
        r=a.add(name,alias,x,z,yaw,.82,zone='covered_trade',required=False)
        if r:stalls.append(r)
    # Authored close-view trade loads line the facades, while the door's
    # central approach and all two-way public walking strips stay readable.
    for i,h in enumerate([r for r in houses if r['zone']in['bakers_arcade','arcade_endstop']]):
        m=a.model(h['alias']);front=m['bounds_max'][2]*h['scale'];width=(m['bounds_max'][0]-m['bounds_min'][0])*h['scale']
        for j,(lx,dz,alias,scale)in enumerate([(-width*.36,.48,'bep_prop_barrel',.70),(-width*.23,.50,'purchased_prop_sack',.72),
            (width*.36,.42,'purchased_prop_logs',.47),(width*.21,.48,'bep_prop_bucket',.75),
            (-width*.35,1.23,'purchased_prop_sack',.58),(width*.36,1.13,'purchased_crate',.50)]):
            x,z=a.local(h,lx,front+dz);prop=a.add(h['id']+f'_close_trade_{j}',alias,x,z,h['yaw_degrees']+11*j,scale,zone='arcade_close_trade',required=False)
            if prop and alias=='purchased_crate':a.supported(h['id']+f'_close_bread_{j}','purchased_goods_bread',prop,0,0,13,.7,zone='arcade_close_trade')
        for j,(lx,dz)in enumerate([(width*.41,.45),(width*.43,.75),(-width*.43,.48),(-width*.42,.85)]):
            x,z=a.local(h,lx,front+dz)
            a.add(h['id']+f'_close_herb_{j}','starting_celandine'if j%2 else'starting_wildflower',x,z,19*i+47*j,.70,zone='arcade_close_herbs',required=False)
        a.wall_attachment(h,h['id']+'_wall_lantern','purchased_prop_lamp',-width*.24,2.65,.26,'arcade_wall_details')
        if i%2==0:a.wall_attachment(h,h['id']+'_shop_sign','purchased_prop_flag_a'if i%4 else'purchased_prop_flag_b',width*.25,4.35,.72,'arcade_wall_details')
    # Counter goods use actual low horizontal triangles in the vendor booth;
    # if a point only sees canopy fabric, it is rejected instead of floating
    # bread or tankards on the roof.
    for i,stall in enumerate(stalls):
        if stall['zone']!='arcade_covered_trade':continue
        for j,(lx,lz,alias)in enumerate([(-.22,-.35,'purchased_goods_bread'),(.22,-.35,'purchased_goods_cheese'),(0,.1,'purchased_goods_tankard')]):
            a.supported(stall['id']+f'_counter_goods_{j}',alias,stall,lx,lz,27*j,.72,zone='arcade_counter_goods',support_interval=(.30,1.25))
    # A baker's work counter, dyer's baskets, cooper's deliveries and a public
    # resting pocket are compositions with clear aisles rather than scatter.
    for name,x,z,yaw in [('arcade_baker_counter',-71.7,-20,90),('arcade_herb_counter',-63.8,12,-90),('east_spice_counter',111,24,90)]:
        table=a.add(name,'starting_table',x,z,yaw,.68,zone='artisan_counters',required=False)
        if table:
            for j,(lx,lz,alias)in enumerate([(-.45,-.35,'purchased_goods_bread'),(.1,-.15,'purchased_goods_cheese'),(.4,.5,'purchased_goods_tankard')]):
                a.supported(name+f'_goods_{j}',alias,table,lx,lz,37*j,.8,zone='artisan_counters')
    # Real stoops use low, wide source crate geometry with top support proofs;
    # the primary door's central axis remains a clear approach.
    # The tight remaining gaps are reserved for useful deliveries and supplies.
    work=[('cooper_market_casks',-86,28,'cooper'),('west_alley_cordage',-108,19,'rope'),
          ('east_loop_deliveries',117,86,'delivery'),('tavern_back_store',-154,-115,'tavern'),
          ('tavern_north_court',-175,-114,'tavern'),('market_west_store',-86,-14,'delivery')]
    for name,x,z,trade in work:
        for j,(dx,dz)in enumerate([(0,0),(1.4,.15),(.15,1.45),(1.55,1.6)]):
            alias='purchased_prop_logs'if trade=='rope'and j<2 else'bep_prop_barrel'if trade in['cooper','tavern']else'purchased_crate'
            r=a.add(name+f'_{j}',alias,x+dx,z+dz,9*j,.72 if alias=='purchased_prop_logs'else .78,zone='trade_deliveries',required=False)
            if r and j==0:
                a.supported(name+'_resting_tool','village_toolkit_claw_hammer'if trade=='cooper'else'purchased_goods_tankard',r,0,0,13,.75,zone='trade_deliveries')
        for j,(dx,dz)in enumerate([(-.9,0),(-.8,.8),(2.5,.3)]):a.add(name+f'_sack_{j}','purchased_prop_sack',x+dx,z+dz,39*j,.72,zone='trade_deliveries',required=False)
        a.add(name+'_rope','purchased_prop_rope',x+2,z+2.8,15,.8,zone='trade_deliveries',required=False)
    for name,x,z,yaw in [('east_junction_rest',121,77,90),('west_loop_rest',-108,101,90),('north_market_rest',-86,-25,0),('moat_approach_rest',-12,186,0)]:
        seat=a.add(name,'bep_prop_bench',x,z,yaw,.95,zone='public_rest',required=False)
        if seat:
            a.add(name+'_water','bep_prop_bucket',x+1.7,z+.15,17,.8,zone='public_rest',required=False)
            a.add(name+'_belongings','purchased_prop_sack',x-1.9,z+.1,11,.55,zone='public_rest',required=False)
    # Authored streetside planted crescents are restrained enough to preserve
    # clear ground and facade sightlines. No extra trees hide the architecture.
    for name,x,z in [('arcade_entry_herbs',-84,-26),('east_square_herbs',119,82),('tavern_back_herbs',-174,-112),('moat_return_herbs',-14,190)]:
        for j,(dx,dz)in enumerate([(0,0),(.65,.25),(1.15,.65),(.30,.85),(-.4,.25),(-.7,.9),(1.3,1.2),(.6,1.4)]):
            a.add(name+f'_{j}','bep_nature_clover'if j%3 else'starting_celandine',x+dx,z+dz,47*j,.55+.03*(j%3),zone='planted_rest_pockets',required=False)
    return houses,stalls


def cameras(a):
    result=[]
    for id,title,p,q,pitch in [('castle-finished-arcade','Valdoa | Bakers arcade',[-65.8,40],[-68.2,2],-.025),
       ('castle-finished-west','Valdoa | Finished West High Street',[-130,105],[-130,20],-.018),
       ('castle-finished-east','Valdoa | East market frontage',[95,91],[106,43],-.025)]:
        x,z=p;wx,wz=ORIGIN[0]+x,ORIGIN[1]+z
        result.append(dict(id=id,name=title,local_xz=p,world_xz=[wx,wz],map_uv=[wx/WIDTH+.5,wz/DEPTH+.5],yaw_radians=math.atan2(q[0]-x,-(q[1]-z)),pitch_radians=pitch,eye_height_m=1.75,eye_y_m=a.atlas.sample(wx,wz).height+1.75))
    return result


def output(a,houses,stalls,baseline):
    counts=Counter(r['alias']for r in a.records);assert len(a.records)<=1200
    lines=['# Valdoa finished walking-demo layer v1; tools/build_demo_finish.py.',
        '# Named infill frontages, working counters, deliveries and planted rest pockets.',
        '# Seven earlier scenery sources, model bytes, roads, actor routes and water preserved.',
        '# alias map_u map_v yaw_degrees scale burial_meters [elevation_meters]']
    for r in a.records:
        x,z=r['world_xz'];line=f"{r['alias']} {x/WIDTH+.5:.14f} {z/DEPTH+.5:.14f} {r['yaw_degrees']:.6f} {r['scale']:.6f} {r['burial_m']:.6f}"
        if r['elevation_m']:line+=f" {r['elevation_m']:.6f}"
        line+=' # '+r['id'];assert len(line.encode())<256;lines.append(line)
    text='\n'.join(lines)+'\n';assert len(text.encode())<256*1024
    # Ground include is material/ecology only; root integrates it into the
    # existing cached village atlas. No terrain height or water changes.
    pads=[]
    for h in houses:
        lo,hi=a.model(h['alias'])['bounds_min'],a.model(h['alias'])['bounds_max'];s=h['scale']
        pads.append(dict(id=h['id'],local_xz=h['local_xz'],half_extents=[max(abs(lo[0]),abs(hi[0]))*s+.55,max(abs(lo[2]),abs(hi[2]))*s+1.1],yaw_radians=math.radians(h['yaw_degrees']),paving=.44,grass_exclusion=1))
    inc=['/* Generated demo finish: material/ecology masks only; no height or water edits. */','static const VillageRoad demo_finish_roads[]={']
    for r in NEW_ROADS:inc.append('    {'+','.join(f'{v:.9f}'for v in r['a']+r['b']+[r['half_width'],r['paving']])+'},')
    inc+=['};','static const VillageRect demo_finish_pads[]={']
    for p in pads:inc.append('    {'+','.join(f'{v:.9f}'for v in p['local_xz']+p['half_extents']+[math.cos(p['yaw_radians']),math.sin(p['yaw_radians']),p['paving'],p['grass_exclusion']])+'},')
    inc+=['};',''];ground='\n'.join(inc)
    routes={name:sha(ROOT/'engine'/name)for name in['world_village_population.cpp','world_village_pets.cpp']}
    manifest=dict(schema_version=1,title='Valdoa | Finished working streets and west-market arcade',generator='tools/build_demo_finish.py',generator_sha256=sha(__file__),source_file=str(OUTPUT.relative_to(ROOT)),source_sha256=hashlib.sha256(text.encode()).hexdigest(),record_count=len(a.records),records=len(a.records),combined_record_count=sum(r['records']for r in baseline)+len(a.records),aliases=dict(sorted(counts.items())),semantic_zones=dict(Counter(r['zone']for r in a.records)),protected_sources=baseline,original_sources_unchanged=True,models={alias:a.models[alias]for alias in sorted(counts)},placements=a.records,architecture=dict(infill_houses=len(houses),additional_market_canopies=len(stalls),source_models_reused=True,new_images=0,new_model_families=0,overlap_policy='Union of source near/mid GLB footprints, SAT rectangle separation against existing/new rigid meshes; ancestor exclusion only for triangle-supported tabletop props or measured facade attachments.'),proof=dict(actual_runtime_bounds=True,full_source_roof_obb_separation=True,production_cpu_atlas=True,full_footprint_water_grid=[5,5],terrain_probes=a.probes,obb_pairs_tested=a.obb_checks,maximum_full_footprint_relief_m=max(r['ground_height_max_m']-r['ground_height_min_m']for r in a.records),minimum_public_road_clearance_m=min(r['minimum_road_obb_gap_m']for r in a.records),minimum_actor_route_clearance_m=min(r['minimum_actor_route_obb_gap_m']for r in a.records),reserved_road_count=len(a.roads),reserved_walking_and_actor_segments=len(a.routes),runtime_actor_route_sha256=routes,stationary_actor_anchors=[dict(id=name,local_xz=list(p))for name,p in a.stationary_anchors],stationary_actor_setup_sha256=sha(ROOT/'engine/world_village_life_data.inc'),protected_castle_walk_length_m=sum(math.dist(p,q)for p,q in zip(WEST_TOUR,WEST_TOUR[1:])),existing_routes_preserved=True,cultivated_fields_preserved=True,existing_outer_moat_preserved=True,triangle_supported_props=[p for p in a.supports if p.get('actual_source_triangle_probe')],triangle_mounted_facade_details=[p for p in a.supports if p.get('actual_source_wall_triangle_probe')]),new_arcade_route_local_xz=ARCADE_PATH,new_arcade_route_half_width_m=1.3,new_ground=dict(include=str(GROUND.relative_to(ROOT)),sha256=hashlib.sha256(ground.encode()).hexdigest(),roads=NEW_ROADS,pads=pads,height_edits=False),rejected_authored_candidates=a.rejected,inspection_cameras=cameras(a),performance=dict(new_texture_images=0,additional_authored_records=len(a.records),near_triangle_upper_bound=sum(a.model(r['alias'])['lods'][0]['triangles']for r in a.records),note='Existing resident model and shared texture families only. Native timed integration evidence is produced separately.'))
    # Independent cross-street sections quantify the close opposing eaves.
    # Empty side gaps are retained as shop bays; their covered counters are
    # separately measured, rather than hiding spacing with a density label.
    arcade_profiles=[]
    for z in range(-11,35):
        sides=[[],[]]
        for h in houses:
            if h['zone']!='bakers_arcade':continue
            points=corners(h,a.model(h['alias']));xs=[]
            for p,q in zip(points,points[1:]+points[:1]):
                if abs(q[1]-p[1])<1.e-8:continue
                if min(p[1],q[1])<=z<=max(p[1],q[1]):
                    t=(z-p[1])/(q[1]-p[1]);xs.append(p[0]+t*(q[0]-p[0]))
            if xs:sides[0 if h['yaw_degrees']>0 else 1].append(max(xs)if h['yaw_degrees']>0 else min(xs))
        if sides[0]and sides[1]:
            west,east=max(sides[0]),min(sides[1]);gap=east-west
            assert 4<gap<7.5,('Arcade facade width escaped compact composition',z,gap)
            arcade_profiles.append(dict(local_z=z,west_eave_x=west,east_eave_x=east,clear_eave_gap_m=gap))
    assert len(arcade_profiles)>=22
    # A new public branch must clear the original source colliders too. This
    # is separate from checking newly placed scenery against existing roads:
    # the fixed hero-market booth blocks the earlier direct south opening.
    # Full old/new rigid source footprints are conservative compared with
    # production compound pieces and are intentionally checked here.
    route_pair_count=0;old_min=math.inf;new_min=math.inf;nearest_old=None
    for old,previous in [(r,True)for r in a.previous]+[(r,False)for r in a.records]:
        model=a.model(old['alias'])
        if model['collision']=='None':continue
        for k,(p,q)in enumerate(zip(ARCADE_PATH,ARCADE_PATH[1:])):
            gap=path_gap(old,model,p,q)-1.3-.4;route_pair_count+=1
            assert gap>=.4,(old['id'],'new public route blocked by source footprint',k,gap)
            if previous and gap<old_min:old_min=gap;nearest_old=dict(id=old['id'],alias=old['alias'],segment=k)
            if not previous:new_min=min(new_min,gap)
    terrain_checks=0;route_low=math.inf;route_high=-math.inf
    for p,q in zip(ARCADE_PATH,ARCADE_PATH[1:]):
        dx,dz=q[0]-p[0],q[1]-p[1];length=math.hypot(dx,dz);steps=math.ceil(length/.25)
        for lane in[-1,0,1]:
            offset=lane*(1.3-.37-.03)
            for step in range(steps+1):
                t=step/steps;x=ORIGIN[0]+p[0]+dx*t-dz/length*offset;z=ORIGIN[1]+p[1]+dz*t+dx/length*offset
                sample=a.atlas.sample(x,z);assert not a.atlas.api.world_atlas_local_water(x,z,None,None)
                assert sample.height>0 and sample.land>.75 and sample.shore>.5
                route_low=min(route_low,sample.height);route_high=max(route_high,sample.height);terrain_checks+=1
    assert route_high-route_low<.05
    manifest['proof']['new_arcade_whole_route']=dict(all_seven_previous_rigid_sources_checked=True,new_rigid_source_footprints_checked=True,actual_model_bounds=True,source_obb_segment_pairs=route_pair_count,public_half_width_m=1.3,required_extra_clearance_m=.4,base_pedestrian_guard_m=.4,minimum_previous_source_extra_clearance_m=old_min,minimum_new_source_extra_clearance_m=new_min,nearest_previous_source=nearest_old,length_m=sum(math.dist(p,q)for p,q in zip(ARCADE_PATH,ARCADE_PATH[1:])),sampled_step_m=.25,lanes=3,capsule_radius_m=.37,outer_lane_offset_m=.9,terrain_checks=terrain_checks,terrain_min_m=route_low,terrain_max_m=route_high,dry_full_walking_width=True,entry_local_xz=ARCADE_PATH[0],exit_local_xz=ARCADE_PATH[-1])
    manifest['architecture']['compact_arcade']=dict(facade_sections=arcade_profiles,minimum_eave_gap_m=min(p['clear_eave_gap_m']for p in arcade_profiles),maximum_eave_gap_m=max(p['clear_eave_gap_m']for p in arcade_profiles),public_half_width_m=1.3,blocked_north_vista_house='arcade_end_bakery',public_west_exit_local_xz=ARCADE_PATH[-1],mounted_facade_details=len(manifest['proof']['triangle_mounted_facade_details']),source_triangle_supported_counter_goods=sum(r['zone']=='arcade_counter_goods'for r in a.records))
    assert manifest['combined_record_count']<=8192
    return text,ground,manifest


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--force',action='store_true');p.add_argument('--validate-only',action='store_true');args=p.parse_args();baseline=protected()
    with tempfile.TemporaryDirectory(prefix='valwood-demo-finish-')as directory:
        a=Author(Atlas(directory));houses,stalls=compose(a);rainforest=append_rainforest(a);text,ground,manifest=output(a,houses,stalls,baseline);manifest['rainforest_finish']=rainforest
        views='/* Generated finished walking-demo inspection views. */\n'+''.join(f'    {{"{c["id"]}","{c["name"]}",{c["map_uv"][0]:.14f},{c["map_uv"][1]:.14f},{c["yaw_radians"]:.9f}f,{c["pitch_radians"]:.9f}f,1.75f}},\n'for c in manifest['inspection_cameras'])
        outputs={OUTPUT:text,GROUND:ground,VIEWS:views,MANIFEST:json.dumps(manifest,indent=2)+'\n'}
        for path,value in outputs.items():
            if args.validate_only:assert path.read_text()==value,('Stale authored output',path)
            elif not path.exists()or path.read_text()!=value:
                if path.exists()and not args.force:raise SystemExit(f'{path} differs; use --force for deliberate regeneration.')
                path.write_text(value)
        assert protected()==baseline,'Earlier sources changed during finishing layout'
        print(json.dumps(dict(records=len(a.records),houses=len(houses),canopies=len(stalls),zones=manifest['semantic_zones'],source_count=8,combined_records=manifest['combined_record_count'],dry_and_clear=True,minimum_road_clearance=manifest['proof']['minimum_public_road_clearance_m'],minimum_actor_clearance=manifest['proof']['minimum_actor_route_clearance_m'],rejected=Counter(r['reason']for r in a.rejected)),indent=2))

if __name__=='__main__':main()
