#!/usr/bin/env python3
"""Author bounded, editable Valdoa village data and shared CPU terrain masks.

Only this new village's files are written. --validate-only checks committed
placements against real model bounds, road capsules and the CPU atlas.
"""
import argparse
import collections
import ctypes as C
import hashlib
import html
import json
import math
from pathlib import Path
import random
import tempfile

from build_starting_area import ROOT, DIRECTORY, MODELS, Atlas, glb_bounds, rectangle_distance, segment_distance

WIDTH=100800.0
DEPTH=WIDTH*2818/5009
ORIGIN=[(.487-.5)*WIDTH,(.320-.5)*DEPTH]
PATH=DIRECTORY/'castle_village_layout.json'
STEMS=['village_house_a','village_house_b','village_inn','village_smithy',
       'village_stall_red','village_stall_blue','village_stall_gold','village_keep',
       'village_well','village_banner','village_cart']
SIZES=dict(zip(STEMS,[(10,12,12),(12,14,14),(18,16,14),(12,10,10),
                     (4,3.3,3),(4,3.3,3),(4,3.3,3),(36,35,28),
                     (3,3.8,3),(1.6,4.8,.5),(3.2,2,1.8)]))
ROUTE=[(0,220),(0,130),(0,55),(0,-50),(0,-88),(0,-50),(0,55),
       (-95,55),(-95,125),(0,125),(0,220)]
PLANTS={'starting_celandine','starting_wildflower','fern0','fern1','fern2'}
NON_SOLID=PLANTS|{'village_banner','starting_bucket','starting_lantern','starting_axe'}
PURCHASED_GOODS={'purchased_goods_'+name for name in ['bread','cheese','chicken','tankard']}
PURCHASED_BUILDINGS={'purchased_house_'+v for v in 'abcdef'}|{'purchased_shop_'+v for v in 'abcd'}|{'purchased_castle'}
PURCHASED_PROPS={'purchased_prop_'+v for v in ['lamp','flag_a','flag_b','sack','logs','rope','barrel_small']}
PURCHASED_SOLIDS=PURCHASED_BUILDINGS|{'purchased_gate','purchased_wall','purchased_barrel','purchased_crate'}|PURCHASED_PROPS-{'purchased_prop_flag_a','purchased_prop_flag_b','purchased_prop_rope','purchased_prop_lamp'}
NON_SOLID|=PURCHASED_GOODS
NON_SOLID|={'purchased_prop_flag_a','purchased_prop_flag_b','purchased_prop_rope','purchased_prop_lamp'}
ALIASES={**{a:a for a in STEMS},**{a:a for a in ['starting_gate','starting_wall','starting_tower',
    'starting_table','starting_crate','starting_barrel','starting_bucket','starting_lantern','starting_axe',
    'starting_stump','starting_firepit','starting_celandine','starting_wildflower']},
    **{f'fern{i}':f'woodland_fern_{i}' for i in range(3)},'broadleaf':'cinematic_broadleaf_near',
    **{a:a+'_near' for a in PURCHASED_GOODS},
    **{a:a+'_near' for a in PURCHASED_PROPS},
    **{'purchased_'+a:'purchased_village_'+b+'_near' for a,b in
       [(f'house_{v}',f'house_{v}') for v in 'abcdef']+[(f'shop_{v}',f'stall_{v}') for v in 'abcd']+[(v,v) for v in ['castle','gate','wall','barrel','crate']]}}

def digest(path):return hashlib.sha256(path.read_bytes()).hexdigest()

def write_generated(path,text):
    # Identical scene recooks must not invalidate every C/C++ translation unit
    # through the shared generated-header dependency list.
    if not path.exists() or path.read_text()!=text:path.write_text(text)

def initial_layout():
    protected=[]
    for name in ['authored_instances.txt','starting_area_instances.txt','rainforest_instances.txt']:
        path=DIRECTORY/name
        protected.append(dict(file=name,sha256=digest(path),records=sum(bool(s.strip()) and not s.lstrip().startswith('#') for s in path.read_text().splitlines())))
    return dict(schema_version=1,title='Valdoa | Crown and market',seed='valdoa-castle-village-v1',
        description='A fortified fantasy market village with open south gate, residential lanes, workshops, cultivated gardens and a northern castle courtyard.',
        origin=dict(map_uv=[.487,.320],world_xz=ORIGIN,axes='east +X / south +Z',terrace_height_m=24),
        bounds=dict(min_xz=[-280,-280],max_xz=[280,280],flat_half_extent_m=225,feather_end_m=280),
        roads=[dict(id='crown_avenue',a=[0,225],b=[0,-132],half_width=6,paving=1),
               dict(id='west_loop',a=[-95,165],b=[-95,-75],half_width=4,paving=.65),
               dict(id='east_loop',a=[95,165],b=[95,-75],half_width=4,paving=.65),
               *[dict(id=f'cross_{z}',a=[-172,z],b=[172,z],half_width=5,paving=.65) for z in [-35,55,125]],
               dict(id='castle_court',a=[0,-60],b=[0,-112],half_width=27,paving=.9)],
        squares=[dict(id='market',center=[0,8],half=[39,34],paving=.95),
                 dict(id='castle_forecourt',center=[0,-88],half=[48,25],paving=.9)],
        benchmark_route_xz=[list(p) for p in ROUTE],route_capsule_radius_m=.37,
        spawn=dict(position_xz=[0,220],yaw=0,pitch=.02,eye_height=1.75),
        views=[dict(id='village',position_xz=[0,220],target_xz=[0,-100],pitch=.06),
               dict(id='village-market',position_xz=[0,46],target_xz=[0,-80],pitch=.04),
               dict(id='village-castle',position_xz=[0,-82],target_xz=[0,-145],pitch=.15)],
        preserve=protected)

class Author:
    def __init__(self,layout):self.layout=layout;self.records=[];self.bounds={};self.asset_boxes={};self.purchased={};self.attachments=[];self.triangles={};self.overheads=[];self.demo_district={};self.demo_dressing={};self.pre_demo_homes=0
    def model(self,alias,require=False):
        if alias not in self.bounds:
            path=MODELS/(ALIASES[alias]+'.glb')
            if path.exists():self.bounds[alias]=glb_bounds(path)
            else:
                assert alias in SIZES and not require,f'Missing runtime model: {path}'
                w,h,d=SIZES[alias];self.bounds[alias]=[[-w/2,0,-d/2],[w/2,h,d/2]]
        return self.bounds[alias]
    def add(self,alias,x,z,yaw=0,scale=1,burial=0,label='',elevation=0):
        self.records.append(dict(alias=alias,x=x,z=z,yaw=yaw,scale=scale,burial=burial,
                                 elevation=elevation,label=label or f'{alias}_{len(self.records)}'))
    def surface(self,alias,x,z,minimum,maximum):
        if alias not in self.triangles:self.triangles[alias]=glb_bounds(MODELS/(ALIASES[alias]+'.glb'),True)[3]
        hits=[]
        for a,b,c in self.triangles[alias]:
            ax,az=b[0]-a[0],b[2]-a[2];bx,bz=c[0]-a[0],c[2]-a[2]
            denominator=ax*bz-az*bx
            if abs(denominator)<1e-10:continue
            dx,dz=x-a[0],z-a[2];u=(dx*bz-dz*bx)/denominator;v=(ax*dz-az*dx)/denominator
            if u<-.0001 or v<-.0001 or u+v>1.0001:continue
            y=a[1]+u*(b[1]-a[1])+v*(c[1]-a[1])
            if minimum<=y<=maximum:hits.append(y)
        assert hits,('No real support surface',alias,x,z,minimum,maximum)
        return max(hits)
    def wall_surface(self,alias,y,z,side='max'):
        if alias not in self.triangles:self.triangles[alias]=glb_bounds(MODELS/(ALIASES[alias]+'.glb'),True)[3]
        hits=[]
        for a,b,c in self.triangles[alias]:
            ay,az=b[1]-a[1],b[2]-a[2];by,bz=c[1]-a[1],c[2]-a[2];d=ay*bz-az*by
            if abs(d)<1e-10:continue
            u=((y-a[1])*bz-(z-a[2])*by)/d;v=(ay*(z-a[2])-az*(y-a[1]))/d
            if u>=-.0001 and v>=-.0001 and u+v<=1.0001:hits.append(a[0]+u*(b[0]-a[0])+v*(c[0]-a[0]))
        assert hits,('Missing real wall attachment',alias,y,z)
        assert side in {'min','max'}
        return (min if side=='min' else max)(hits)
    def apply_purchased(self):
        filename=self.layout.get('purchased_details')
        if not filename:return
        assert Path(filename).name==filename
        self.purchased=json.loads((DIRECTORY/filename).read_text());assert self.purchased['schema_version']==1
        removals=set(self.purchased.get('removals',[]))
        assert removals <= {r['label'] for r in self.records},('Unknown scene removal',removals)
        self.records=[r for r in self.records if r['label'] not in removals]
        for replacement in self.purchased.get('replacements',[]):
            found=[r for r in self.records if r['label']==replacement['label']];assert len(found)==1
            r=found[0];r['alias']=replacement['alias'];r['burial']=replacement.get('burial',0)
            for field in ['x','z','elevation']:
                if field in replacement:r[field]=replacement[field]
            if 'scale' in replacement:r['scale']=replacement['scale']
            if 'yaw_degrees' in replacement:r['yaw']=math.radians(replacement['yaw_degrees'])
        for p in self.purchased.get('placements',[]):
            self.add(p['alias'],p['x'],p['z'],math.radians(p.get('yaw_degrees',0)),p.get('scale',1),
                p.get('burial',0),p['label'],p.get('elevation',0))
            if 'parent_structure' in p:self.records[-1]['parent_structure']=p['parent_structure']
        for p in self.purchased.get('attachments',[]):
            parents=[r for r in self.records if r['label']==p['parent']];assert len(parents)==1
            parent=parents[0];lx,lz=p['local_xz'];yaw=math.radians(p.get('yaw_degrees',0))
            lo,hi=self.model(p['alias'],True);scale=p.get('scale',1)
            c,s=math.cos(yaw),math.sin(yaw);heights=[];footprint=[]
            # Five actual mesh intersections prove the whole product footprint
            # sits on a tabletop, rather than just a convenient model bound.
            for x,z in [(0,0),(lo[0],lo[2]),(lo[0],hi[2]),(hi[0],lo[2]),(hi[0],hi[2])]:
                px=lx+(c*x+s*z)*scale/parent['scale'];pz=lz+(-s*x+c*z)*scale/parent['scale']
                footprint.append([px,pz])
                heights.append(self.surface(parent['alias'],px,pz,*p['surface_y_range']))
            if 'container_inner_bounds_xz' in p:
                xmin,xmax,zmin,zmax=p['container_inner_bounds_xz']
                assert all(xmin+.005<px<xmax-.005 and zmin+.005<pz<zmax-.005 for px,pz in footprint),('Product intersects bin walls',p['label'],footprint)
            assert max(heights)-min(heights)<.025,(p['label'],'Uneven product support',heights)
            pc,ps=math.cos(parent['yaw']),math.sin(parent['yaw'])
            x=parent['x']+(pc*lx+ps*lz)*parent['scale'];z=parent['z']+(-ps*lx+pc*lz)*parent['scale']
            support_offset=-.05 if parent['alias'] in {'starting_crate','starting_barrel','starting_bucket','starting_lantern','starting_axe'} else 0
            elevation=parent['elevation']-parent['burial']+support_offset+max(heights)*parent['scale']+.002
            self.add(p['alias'],x,z,parent['yaw']+yaw,scale,0,p['label'],elevation)
            self.records[-1]['surface_parent']=p['parent']
            self.attachments.append(dict(label=p['label'],parent=p['parent'],actual_surface_samples_m=heights,
                source_local_xz=[lx,lz],actual_footprint_xz=footprint,ground_elevation_m=elevation,contact_slack_m=.002))
        for row in self.purchased.get('overhead_rows',[]):
            ends=[]
            for index,label in enumerate([row['west_parent'],row['east_parent']]):
                parent=next(r for r in self.records if r['label']==label);c=math.cos(parent['yaw'])
                assert abs(math.sin(parent['yaw']))<1e-8
                local_y=(row['elevation']-parent['elevation']+parent['burial'])/parent['scale']
                local_z=(row['z']-parent['z'])/(c*parent['scale'])
                local_x=self.wall_surface(parent['alias'],local_y,local_z,row.get('source_sides',['max','max'])[index])
                ends.append(parent['x']+c*local_x*parent['scale'])
            assert ends[0]<60 and ends[1]>64 and row['elevation']>=5
            lo,hi=self.model('purchased_prop_rope',True);length=hi[2]-lo[2];span=ends[1]-ends[0]
            count=math.ceil(span/(length*.55));scale=span/(length*count);rope_scale=scale
            for i in range(count):self.add('purchased_prop_rope',ends[0]+span*(i+.5)/count,row['z'],math.pi/2,scale,0,row['id']+'_rope_'+str(i),row['elevation']-(hi[1]+lo[1])*.5*scale)
            for i in range(row['flags']):
                alias='purchased_prop_flag_'+('a' if i%2==0 else 'b');flo,fhi=self.model(alias,True);scale=.92 if i%2==0 else .86
                self.add(alias,ends[0]+span*(i+1)/(row['flags']+1),row['z'],math.pi/2,scale,0,row['id']+'_flag_'+str(i),row['elevation']-fhi[1]*scale)
            self.overheads.append(dict(id=row['id'],parents=[row['west_parent'],row['east_parent']],actual_wall_endpoints_xz=[[ends[0],row['z']],[ends[1],row['z']]],elevation_m=row['elevation'],rope_segments=count,rope_scale=rope_scale,span_m=span,flags=row['flags']))
    def line(self,a,b,scale,label):
        dx,dz=b[0]-a[0],b[1]-a[1];length=math.hypot(dx,dz)
        width=(self.model('starting_wall')[1][0]-self.model('starting_wall')[0][0])*scale
        count=math.ceil(length/(width*.97));step=length/count
        for i in range(count):self.add('starting_wall',a[0]+dx*(i+.5)/count,a[1]+dz*(i+.5)/count,-math.atan2(dz,dx),scale,.22,f'{label}_{i}')
    def compose(self):
        # Outer walls and towers enclose420m without blocking the south opening.
        self.add('starting_gate',0,205,0,2,.22,'south_gate')
        self.line((-205,205),(-7.4,205),2,'southwest_wall');self.line((7.4,205),(205,205),2,'southeast_wall')
        for name,a,b in [('west',(-205,-205),(-205,205)),('east',(205,-205),(205,205)),('north',(-205,-205),(205,-205))]:self.line(a,b,2,name+'_wall')
        for i,(x,z) in enumerate([(-205,-205),(205,-205),(-205,205),(205,205),(-205,0),(205,0),(-23,205),(23,205)]):self.add('starting_tower',x,z,0,1.28,.22,f'outer_tower_{i}')
        # A separate walled castle court and the front-facing keep.
        self.add('starting_gate',0,-55,0,1.6,.20,'castle_gate')
        self.line((-65,-55),(-5.9,-55),1.6,'castle_southwest');self.line((5.9,-55),(65,-55),1.6,'castle_southeast')
        self.line((-65,-195),(65,-195),1.6,'castle_north');self.line((-65,-195),(-65,-55),1.6,'castle_west');self.line((65,-195),(65,-55),1.6,'castle_east')
        for i,(x,z) in enumerate([(-65,-55),(65,-55),(-65,-195),(65,-195)]):self.add('starting_tower',x,z,0,1,.20,f'castle_tower_{i}')
        # Keep its thin timber floor15mm above the terrace, rather than
        # drawing it exactly coplanar with the terrain inside the doorway.
        self.add('village_keep',0,-145,math.pi,1,.005,'royal_keep')
        # Four distinct residential blocks face the loop streets and courtyards.
        for column,x in enumerate([-145,-58,58,145]):
            rows=[-64,-8,22,88,156] if abs(x)>100 else [-10,22,156]
            for row,z in enumerate(rows):
                alias='village_house_a' if (row+column)%2==0 else 'village_house_b'
                facing=-math.pi/2 if x<0 else math.pi/2
                self.add(alias,x,z,facing,.92+(row%3)*.04,.02,f'home_{column}_{row}')
        for column,x in enumerate([-175,175,-119,119]):
            rows=[-155,-112,-73,-7,68,142] if abs(x)>150 else [-150,-105,-67,-7,82,150]
            for row,z in enumerate(rows):
                facing=-math.pi/2 if x<0 else math.pi/2
                self.add('village_house_a' if (row+column)%2 else 'village_house_b',x,z,facing,.92+(row%2)*.04,.02,f'outer_home_{column}_{row}')
        self.add('village_inn',52,84,math.pi/2,1,.02,'wayfarers_inn')
        self.add('village_smithy',-52,84,-math.pi/2,1,.02,'west_smithy')
        for side,x in enumerate([-24,24]):
            for row,z in enumerate([-6,9,24]):self.add(['village_stall_red','village_stall_blue','village_stall_gold'][(row+side)%3],x,z,-math.pi/2 if x<0 else math.pi/2,1,0,f'market_stall_{side}_{row}')
        for side,x in enumerate([-12,12]):
            for row,z in enumerate([-6,9,24]):self.add(['village_stall_gold','village_stall_red','village_stall_blue'][(row+side)%3],x,z,-math.pi/2 if x<0 else math.pi/2,1,0,f'inner_market_stall_{side}_{row}')
        # Trees frame both ends of the market without closing the avenue.
        for side,x in enumerate([-14,14]):
            for row,z in enumerate([-20,44]):self.add('broadleaf',x,z,0,.75,.025,f'market_tree_{side}_{row}')
        self.add('village_well',18,39,0,1,0,'market_well')
        for side,sign in enumerate([-1,1]):
            self.add('starting_table',sign*9,38,0,1,0,f'market_bench_{side}')
            self.add('starting_lantern',sign*9,38,0,1,0,f'market_bench_lantern_{side}',.75)
            for i,z in enumerate([-2,13,28]):self.add('starting_barrel',sign*8.3,z,.2*i,1.2,0,f'market_barrel_{side}_{i}')
            self.add('starting_crate',sign*17,33,0,1.5,0,f'market_cargo_{side}_0')
            self.add('starting_crate',sign*18.5,33,.15,1.5,0,f'market_cargo_{side}_1')
            self.add('starting_crate',sign*17,33,0,1.5,0,f'market_cargo_{side}_stack',.52443)
            for i,z in enumerate([-27,34]):self.add('village_banner',sign*8,z,0,1,0,f'market_banner_{side}_{i}')
        self.add('village_cart',-22,78,.23,1,0,'smithy_cart');self.add('village_cart',33,115,-.45,1,0,'merchant_cart')
        for i,(x,z) in enumerate([(-8,187),(8,187),(-12,-63),(12,-63),(-25,-125),(25,-125),(-41,48),(41,48)]):self.add('village_banner',x,z,0,1,0,f'royal_banner_{i}')
        # Work/cargo groups stay well off the shared navigation corridors.
        for group,(x,z) in enumerate([(-27,84),(29,103),(-165,100),(166,151),(-156,-110),(153,-110),(39,-22),(-39,-22)]):
            for i in range(6):self.add('starting_crate',x+(i%3)*1.05,z+(i//3)*1.2,.1*(i%2),1.5,0,f'cargo_{group}_{i}')
            for i in range(3):self.add('starting_barrel',x-1.3,z+i*1.1,.2*i,1.2,0,f'barrel_{group}_{i}')
        for i,(x,z) in enumerate([(-42,96),(42,96),(-165,45),(165,45),(-35,176),(35,176)]):
            self.add('starting_table',x,z,math.pi/2,1,0,f'bench_{i}')
            self.add('starting_lantern',x,z,.1,1,0,f'lantern_{i}',.75)
        # Garden beds and cultivated rows add variation without navigation boxes.
        rng=random.Random(self.layout['seed'])
        solid=list(self.records)
        def garden_clear(x,z):
            if any(segment_distance((x,z),p['a'],p['b'])<p['half_width']+2 for p in self.layout['roads']):return False
            for r in solid:
                if r['alias'] in NON_SOLID:continue
                lo,hi=self.model(r['alias']);c=math.cos(r['yaw']);s=math.sin(r['yaw']);dx,dz=x-r['x'],z-r['z'];px=c*dx-s*dz;pz=s*dx+c*dz
                if lo[0]*r['scale']-1<px<hi[0]*r['scale']+1 and lo[2]*r['scale']-1<pz<hi[2]*r['scale']+1:return False
            return True
        for i in range(360):
            side=-1 if i%2==0 else 1
            for _ in range(1000):
                x=side*rng.uniform(112,185);z=rng.choice([172,-120])+rng.uniform(-11,11)
                if garden_clear(x,z):break
            else:raise ValueError('Garden bed exhausted')
            alias='starting_wildflower' if i%3 else 'starting_celandine'
            self.add(alias,x,z,rng.uniform(-math.pi,math.pi),rng.uniform(.7,1.4),0,f'garden_{i}')
        for i in range(70):
            for _ in range(1000):
                x=rng.choice([-185,185])+rng.uniform(-3,3);z=rng.uniform(-170,160)
                if garden_clear(x,z):break
            else:raise ValueError('Hedge bed exhausted')
            self.add('fern'+str(i%3),x,z,rng.uniform(-math.pi,math.pi),rng.uniform(.6,1.1),0,f'hedge_{i}')
        self.apply_purchased()
        self.pre_demo_home_labels={r['label'] for r in self.records if r['alias'].startswith(('village_house_','purchased_house_'))}
        self.pre_demo_homes=len(self.pre_demo_home_labels)
        if self.layout.get('demo_district'):
            from build_village_demo_district import apply
            self.demo_district=apply(self)
        if self.layout.get('demo_dressing'):
            from build_village_demo_dressing import apply
            self.demo_dressing=apply(self)
        if self.layout.get('demo_dressing') and self.layout.get('demo_district'):
            from build_village_demo_bunting import apply
            self.demo_bunting=apply(self)
            from build_village_demo_frontages import apply
            self.demo_frontages=apply(self)
        if self.layout.get('demo_market_lively') and self.layout.get('demo_dressing') and self.layout.get('demo_district'):
            from build_village_market_lively import apply
            self.demo_market_lively=apply(self)
            if self.layout.get('demo_market_hero'):
                from build_village_market_hero import apply
                self.demo_market_hero=apply(self)
                self.layout['demo_views']+=self.demo_market_hero['views']
                if self.layout.get('demo_hero_grove'):
                    from build_village_hero_grove import apply
                    self.demo_hero_grove=apply(self)
        if self.layout.get('castle_expansion') and getattr(self,'demo_hero_grove',None):
            import build_castle_expansion as expansion
            ALIASES.update(expansion.ASSET_ALIASES)
            self.castle_expansion=expansion.apply(self)
            self.layout['demo_views']+=self.castle_expansion['views']
            import build_castle_drawbridge as bridge
            ALIASES['castle_drawbridge']='castle_drawbridge'
            NON_SOLID.add('castle_drawbridge') # rails use the independent compound source boxes
            self.castle_creek=bridge.apply(self)
            self.layout['demo_views']+=self.castle_creek['views']
            if self.layout.get('castle_inner_wing'):
                import build_castle_inner_wing as wings
                ALIASES.update(wings.ASSET_ALIASES)
                self.castle_inner_wing=wings.apply(self)
            if self.layout.get('castle_creek_banks'):
                import build_castle_creek_banks as banks
                ALIASES.update(banks.ASSET_ALIASES)
                self.demo_castle_creek_banks=banks.apply(self)
            if self.layout.get('village_torches') and getattr(self,'demo_castle_creek_banks',None):
                import build_village_torches as torches
                self.village_torches=torches.apply(self)
            if self.layout.get('village_farms') and getattr(self,'village_torches',None):
                import build_village_farms as farms
                self.village_farms=farms.apply(self)
                self.layout['demo_views']+=self.village_farms['views']
                if self.layout.get('village_reinforcements'):
                    import build_village_reinforcements as reinforcements
                    self.village_reinforcements=reinforcements.apply(self)
                    self.layout['demo_views']+=self.village_reinforcements['views']
        return self.records

def watch_ground_rectangles(metadata):
    """Virtual watch posts clear rooted grass using the actual all-pose union."""
    if not metadata:return []
    low,high=metadata['watch_union_bounds_yup'];result=[]
    assert len(metadata['watch_stations'])==4
    cx,cz=(low[0]+high[0])*.5,(low[2]+high[2])*.5
    for index,station in enumerate(metadata['watch_stations']):
        c,s=math.cos(station['yaw']),math.sin(station['yaw'])
        result.append(dict(id=f'village_watch_station_{index}',values=[station['x']+c*cx+s*cz,station['z']-s*cx+c*cz,
            (high[0]-low[0])*.5+.25,(high[2]-low[2])*.5+.25,c,s,0,1],
            extra_grass_clearance_m=.25,source='all-eight-pose union',station=station))
    return result

def mask_include(author):
    out=['/* Generated by tools/build_castle_village.py. Local east/south meters. */',
         'static const VillageRoad village_roads[]={']
    for p in author.layout['roads']:out.append('    {'+','.join(format(v,'.12g') for v in [*p['a'],*p['b'],p['half_width'],p['paving']])+'},')
    out+=['};','static const VillageRect village_rects[]={']
    for s in author.layout['squares']:out.append('    {'+','.join(format(v,'.12g') for v in [*s['center'],*s['half'],1,0,s['paving'],1])+'},')
    for r in author.records:
        if r['alias'] in PLANTS or r['alias'] in NON_SOLID or r['alias']=='purchased_torch_wall':continue
        lo,hi=author.model(r['alias']);cx=(lo[0]+hi[0])*r['scale']/2;cz=(lo[2]+hi[2])*r['scale']/2;c=math.cos(r['yaw']);s=math.sin(r['yaw'])
        values=[r['x']+c*cx+s*cz,r['z']-s*cx+c*cz,(hi[0]-lo[0])*r['scale']/2+.7,(hi[2]-lo[2])*r['scale']/2+.7,c,s,0,1]
        out.append('    {'+','.join(format(v,'.12g') for v in values)+'},')
    for row in watch_ground_rectangles(getattr(author,'village_reinforcements',None)):
        out.append('    {'+','.join(format(v,'.12g')for v in row['values'])+'},')
    out+=['};',''];return '\n'.join(out)

def collision_include(require_final=False):
    final=MODELS/'village_asset_manifest.json'
    path=final if final.exists() else ROOT/'assets/polyhaven/castle_village/collision_plan.json'
    assert path.exists() and (not require_final or final.exists()),'Final village asset collision manifest is required'
    payload=json.loads(path.read_text());seen=set();out=['/* Generated from '+str(path.relative_to(ROOT))+'. */',
        'static const VillageCollisionBox village_collision_boxes[]={']
    for asset in payload['assets']:
        alias=asset.get('family',asset['asset'])
        if alias not in STEMS or alias in seen:continue
        seen.add(alias)
        for box in asset.get('collision_boxes',[]):
            assert len(box)==6 and all(math.isfinite(v) for v in box)
            assert box[0]<box[1] and box[2]<box[3] and box[4]<box[5]
            out.append('    {"'+alias+'",'+','.join(format(v,'.12g')+'f' if '.' in format(v,'.12g') or 'e' in format(v,'.12g') else format(v,'.12g')+'.f' for v in box)+'},')
    assert set(STEMS[:8])<=seen
    # NPC path QA consumes this independent measured-box inventory too. The
    # imported runtime aliases use Collision::Box directly from loaded models,
    # so these entries cannot accidentally apply prototype doorway openings.
    layout=json.loads(PATH.read_text())
    if layout.get('purchased_details'):
        config=json.loads((DIRECTORY/layout['purchased_details']).read_text())
        used={p['alias'] for group in ['replacements','placements'] for p in config.get(group,[])}
        for alias in sorted(PURCHASED_SOLIDS&used):
            lo,hi=glb_bounds(MODELS/(ALIASES[alias]+'.glb'))
            box=[lo[0],hi[0],lo[2],hi[2],lo[1],hi[1]]
            out.append('    {"'+alias+'",'+','.join(format(v,'.12g')+'f' if '.' in format(v,'.12g') or 'e' in format(v,'.12g') else format(v,'.12g')+'.f' for v in box)+'},')
    extra=ROOT/'assets/purchased/castle_expansion/castle_expansion_asset_manifest.json'
    if layout.get('castle_expansion'):
        for asset in json.loads(extra.read_text())['assets']:
            alias=asset['asset']
            for box in asset['collision_boxes']:
                assert len(box)==6 and box[0]<box[1] and box[2]<box[3] and box[4]<box[5]
                out.append('    {"'+alias+'",'+','.join(format(v,'.12g')+'f' if '.' in format(v,'.12g') or 'e' in format(v,'.12g') else format(v,'.12g')+'.f' for v in box)+'},')
    out+=['};',''];return '\n'.join(out)

def check_preserved(layout):
    for p in layout['preserve']:assert digest(DIRECTORY/p['file'])==p['sha256'],f'Existing source changed: {p["file"]}'

def separated_bounds(author,a,b,margin=.15):
    def footprint(r):
        lo,hi=author.model(r['alias']);c,s=math.cos(r['yaw']),math.sin(r['yaw'])
        cx,cz=(lo[0]+hi[0])*r['scale']*.5,(lo[2]+hi[2])*r['scale']*.5
        return r['x']+c*cx+s*cz,r['z']-s*cx+c*cz,(hi[0]-lo[0])*r['scale']*.5,(hi[2]-lo[2])*r['scale']*.5,c,s
    ax,az,ahx,ahz,ac,asi=footprint(a);bx,bz,bhx,bhz,bc,bsi=footprint(b)
    for x,z in [(ac,-asi),(asi,ac),(bc,-bsi),(bsi,bc)]:
        distance=abs((ax-bx)*x+(az-bz)*z)
        radius=ahx*abs(ac*x-asi*z)+ahz*abs(asi*x+ac*z)+bhx*abs(bc*x-bsi*z)+bhz*abs(bsi*x+bc*z)
        if distance>radius+margin:return True
    return False

def horizontal_section_bounds(author,alias,height):
    """Read the real source mesh, excluding decorative overhangs at other heights."""
    if alias not in author.triangles:author.triangles[alias]=glb_bounds(MODELS/(ALIASES[alias]+'.glb'),True)[3]
    points=[]
    for triangle in author.triangles[alias]:
        for a,b in zip(triangle,triangle[1:]+triangle[:1]):
            if min(a[1],b[1])<height<max(a[1],b[1]):
                t=(height-a[1])/(b[1]-a[1])
                points.append([a[k]+(b[k]-a[k])*t for k in [0,2]])
    assert points,(alias,'Missing physical cross section',height)
    return [min(p[k] for p in points) for k in [0,1]],[max(p[k] for p in points) for k in [0,1]]

def validate_masonry(author):
    """Prove the actual closed wall meshes meet without overlapping side faces.

    The old prototype placement spacing deliberately intersected broad blocks.
    Reusing it with thin tiled panels creates nearly coplanar visible surfaces.
    These interval checks use loaded GLB bounds, not authoring size estimates;
    disjoint interior intervals prove positive-area wall sides cannot overlap.
    """
    runs=author.purchased.get('masonry_runs',[])
    if not runs:return {}
    records={r['label']:r for r in author.records};run_by_id={r['id']:r for r in runs}
    assert len(run_by_id)==len(runs)
    seen=set();panels=[];joins=0;maximum_gap=0;maximum_overlap=0
    for run in runs:
        a,b=run['a'],run['b'];length=math.dist(a,b);axis=[(b[k]-a[k])/length for k in [0,1]]
        previous=0;tops=[]
        for index,label in enumerate(run['panels']):
            assert label not in seen;seen.add(label);r=records[label];assert r['alias']=='purchased_wall'
            lo,hi=author.model(r['alias'],True);c,s=math.cos(r['yaw']),math.sin(r['yaw'])
            assert abs(s-axis[0])<1e-8 and abs(c-axis[1])<1e-8,(label,'Wrong panel axis')
            corners=[(r['x']+(c*x+s*z)*r['scale'],r['z']+(-s*x+c*z)*r['scale']) for x in [lo[0],hi[0]] for z in [lo[2],hi[2]]]
            interval=[min((p[0]-a[0])*axis[0]+(p[1]-a[1])*axis[1] for p in corners),max((p[0]-a[0])*axis[0]+(p[1]-a[1])*axis[1] for p in corners)]
            gap=interval[0]-previous;maximum_gap=max(maximum_gap,gap);maximum_overlap=max(maximum_overlap,-gap)
            assert -.00001<=gap<=.00001,(label,'Wall side-face overlap or gap',gap)
            expected_start=length*index/len(run['panels']);expected_end=length*(index+1)/len(run['panels'])
            assert abs(interval[0]-expected_start)<.00001 and abs(interval[1]-expected_end)<.00001
            previous=interval[1];tops.append(hi[1]*r['scale']-r['burial']+r['elevation'])
            panels.append((r,axis,corners))
            if index:joins+=1
        assert abs(previous-length)<.00001 and max(tops)-min(tops)<.00001,(run['id'],'Incoherent wall tops or final join')
    assert seen=={r['label'] for r in author.records if r['alias']=='purchased_wall'},'Every detailed wall must belong to one checked run'
    # Check all collinear panels, including those in separately named runs.
    pair_checks=0
    for i,(a,axis,corners_a) in enumerate(panels):
        for b,axis_b,corners_b in panels[i+1:]:
            if abs(axis[0]*axis_b[1]-axis[1]*axis_b[0])>1e-8:continue
            if abs((b['x']-a['x'])*axis[1]-(b['z']-a['z'])*axis[0])>.00001:continue
            ia=[min(p[0]*axis[0]+p[1]*axis[1] for p in corners_a),max(p[0]*axis[0]+p[1]*axis[1] for p in corners_a)]
            ib=[min(p[0]*axis[0]+p[1]*axis[1] for p in corners_b),max(p[0]*axis[0]+p[1]*axis[1] for p in corners_b)]
            overlap=min(ia[1],ib[1])-max(ia[0],ib[0]);assert overlap<.00001,('Collinear wall face overlap',a['label'],b['label'],overlap)
            pair_checks+=1
    side_gate_checks=0
    for join in author.purchased.get('side_gate_joins',[]):
        gate=records[join['gate']];assert gate['alias']=='purchased_gate'
        # Body intersections avoid the wider parapets: bounding-box alignment
        # alone leaves a16cm slit beside this particular source gate.
        sections=[horizontal_section_bounds(author,gate['alias'],y) for y in join['source_section_y']]
        low=max(section[0][1] for section in sections);high=min(section[1][1] for section in sections)
        c=math.cos(gate['yaw']);assert abs(math.sin(gate['yaw']))<1e-8
        negative,positive=sorted([gate['z']+c*low*gate['scale'],gate['z']+c*high*gate['scale']])
        left=run_by_id[join['negative_run']]['b'];right=run_by_id[join['positive_run']]['a']
        penetration=join['penetration_m'];assert .001<=penetration<=.005
        assert abs(left[0]-gate['x'])<.00001 and abs(right[0]-gate['x'])<.00001
        assert abs(left[1]-(negative+penetration))<.00001 and abs(right[1]-(positive-penetration))<.00001
        # The gate's closed collision box and the wall's actual bounds overlap
        # at each physical join, so no player-sized gap opens at walk height.
        lo,hi=author.model(gate['alias'],True)
        gate_z=sorted([gate['z']+c*lo[2]*gate['scale'],gate['z']+c*hi[2]*gate['scale']])
        assert gate_z[0]<left[1] and gate_z[1]>right[1]
        side_gate_checks+=2
    # Remaining wall ends sit inside the existing tower or open-gate piers.
    protected_ends=0
    for run in runs:
        for end in [run['a'],run['b']]:
            if any(math.dist(end,p)<.01 for join in author.purchased.get('side_gate_joins',[]) for p in [run_by_id[join['negative_run']]['b'],run_by_id[join['positive_run']]['a']]):continue
            protected=False
            for r in author.records:
                if r['alias']=='starting_tower':
                    lo,hi=author.model(r['alias'],True);radius=min(hi[0]-lo[0],hi[2]-lo[2])*r['scale']*.45
                    if math.dist(end,[r['x'],r['z']])<radius:protected=True;break
                elif r['alias']=='starting_gate':
                    lo,hi=author.model(r['alias'],True)
                    if abs(end[1]-r['z'])<.01 and min(abs(end[0]-(r['x']+lo[0]*r['scale'])),abs(end[0]-(r['x']+hi[0]*r['scale'])))<.04:protected=True;break
            assert protected,('Wall end lacks a tower or gate-pier connection',run['id'],end)
            protected_ends+=1
    return dict(masonry_runs=len(runs),masonry_panels=len(panels),masonry_adjacent_joins=joins,
        masonry_collinear_pair_checks=pair_checks,masonry_gate_body_joins=side_gate_checks,
        masonry_tower_or_open_gate_joins=protected_ends,masonry_maximum_gap_m=maximum_gap,
        masonry_maximum_positive_overlap_m=maximum_overlap)

def validate_authored_lanes(author,atlas):
    """Check the composed street using the same real bounds as the main route.

    An appealing camera alone is insufficient: both ends must connect to the
    shared streets and every cargo group must leave the entire walking line
    clear. These paths are additional coverage, never exemptions from the
    original benchmark, house/eave or mesh support checks.
    """
    lanes=author.purchased.get('authored_lanes',[])
    records={r['label']:r for r in author.records};checks=[]
    for lane in lanes:
        assert any(p['id']==lane['id'] and p['a']==lane['a'] and p['b']==lane['b']
                   and p['half_width']==lane['half_width'] for p in author.layout['roads']),('Lane paving mismatch',lane['id'])
        labels=lane['frontage_labels']+lane['prop_labels']
        assert len(set(labels))==len(labels) and set(labels)<=records.keys()
        assert all(records[label]['alias'].startswith('purchased_house_') for label in lane['frontage_labels'])
        route=lane['walking_route_xz'];radius=lane['clearance_radius_m']
        assert len(route)>=2 and .3<=radius<=.5
        # The scenic route must meet the existing public east-west streets.
        for end in [route[0],route[-1]]:
            assert any(p['id']!=lane['id'] and segment_distance(end,p['a'],p['b'])<p['half_width']-radius
                       for p in author.layout['roads']),('Disconnected lane end',lane['id'],end)
        minimum=math.inf;footprint_checks=0;stations=0
        for r in author.records:
            if r['alias'] in NON_SOLID or r['alias']=='starting_gate':continue
            lo,hi=author.model(r['alias']);c,s=math.cos(r['yaw']),math.sin(r['yaw'])
            for a,b in zip(route,route[1:]):
                local=lambda p:[c*(p[0]-r['x'])-s*(p[1]-r['z']),s*(p[0]-r['x'])+c*(p[1]-r['z'])]
                margin=rectangle_distance(local(a),local(b),[lo[0]*r['scale'],lo[2]*r['scale']],
                    [hi[0]*r['scale'],hi[2]*r['scale']])-radius
                assert margin>.15,('Authored lane blocked',lane['id'],r['label'],margin)
                minimum=min(minimum,margin);footprint_checks+=1
        for a,b in zip(route,route[1:]):
            steps=math.ceil(math.dist(a,b)/.5)
            for i in range(steps+1):
                x,z=[a[k]+(b[k]-a[k])*i/steps for k in [0,1]]
                assert abs(atlas.sample(ORIGIN[0]+x,ORIGIN[1]+z).height-24)<.001
                stations+=1
        a,b=lane['a'],lane['b'];length=math.dist(a,b)
        axis=[(b[k]-a[k])/length for k in [0,1]];normal=[axis[1],-axis[0]]
        facades=[[],[]];heights=[]
        for label in lane['frontage_labels']:
            r=records[label];lo,hi=author.model(r['alias']);c,s=math.cos(r['yaw']),math.sin(r['yaw'])
            corners=[(r['x']+(c*x+s*z)*r['scale'],r['z']+(-s*x+c*z)*r['scale'])
                     for x in [lo[0],hi[0]] for z in [lo[2],hi[2]]]
            project=lambda direction:[sum((p[k]-a[k])*direction[k] for k in [0,1]) for p in corners]
            cross=project(normal);along=project(axis)
            side=0 if sum(cross)<0 else 1
            facades[side].append((min(along),max(along),max(cross) if side==0 else min(cross)))
            heights.append((hi[1]-lo[1])*r['scale'])
        gaps=[right[2]-left[2] for left in facades[0] for right in facades[1]
              if min(left[1],right[1])>max(left[0],right[0])]
        assert gaps and min(gaps)>2*lane['half_width'],('Lane eaves intrude on paved corridor',lane['id'])
        checks.append(dict(id=lane['id'],frontages=len(lane['frontage_labels']),
            route_length_m=sum(math.dist(a,b) for a,b in zip(route,route[1:])),
            actual_footprint_checks=footprint_checks,route_capsule_minimum_margin_m=minimum,
            minimum_facing_eave_gap_m=min(gaps),frontage_height_range_m=[min(heights),max(heights)],
            ground_stations=stations,connected_ends=2))
    return checks

def validate_dressing(author):
    """Check physical cargo volumes and façade-supported overhead clearance."""
    by_label={r['label']:r for r in author.records};labels=[];checks=0
    def vertical(r):
        lo,hi=author.model(r['alias']);base=r['elevation']-r['burial']
        if r['alias'] in {'starting_crate','starting_barrel','starting_bucket','starting_lantern','starting_axe'}:base-=.05
        return base+lo[1]*r['scale'],base+hi[1]*r['scale']
    for pocket in author.purchased.get('dressing_pockets',[]):
        assert pocket['labels'] and set(pocket['labels'])<=by_label.keys()
        labels.extend(pocket['labels'])
        for label in pocket['labels']:
            a=by_label[label]
            if a['alias'] in PURCHASED_GOODS:continue
            alo,ahi=vertical(a)
            for b in author.records:
                if b is a or b['alias'] in NON_SOLID or b['alias']=='starting_gate':continue
                # Contact with the named support is proven by five mesh rays;
                # a crate's bevel can exceed its supporting deck's height.
                if a.get('surface_parent')==b['label'] or b.get('surface_parent')==a['label']:continue
                blo,bhi=vertical(b)
                if ahi<=blo+.003 or bhi<=alo+.003:continue
                assert separated_bounds(author,a,b,.008),('Intersecting street cargo volumes',label,b['label'])
                checks+=1
    assert len(labels)==len(set(labels)), 'A dressing object belongs to more than one pocket'
    flags=ropes=0;minimum=math.inf
    for row in author.overheads:
        assert len(row['actual_wall_endpoints_xz'])==2 and 4<row['span_m']<15
        objects=[r for r in author.records if r['label'].startswith(row['id']+'_')]
        assert len(objects)==row['rope_segments']+row['flags']
        for r in objects:
            low,_=vertical(r);minimum=min(minimum,low)
            assert low>4.5,('Overhead object intrudes on walking volume',r['label'],low)
            flags+=r['alias'].startswith('purchased_prop_flag_');ropes+=r['alias']=='purchased_prop_rope'
    return dict(dressing_pockets=len(author.purchased.get('dressing_pockets',[])),dressing_objects=len(labels),
        dressing_volume_clearance_checks=checks,overhead_rows=len(author.overheads),overhead_flags=flags,
        overhead_rope_segments=ropes,overhead_minimum_clearance_m=minimum if author.overheads else None)

def validate(author,atlas,require_models=False):
    check_preserved(author.layout)
    for alias in {r['alias'] for r in author.records}:
        author.model(alias,require_models)
        if require_models:assert (MODELS/(ALIASES[alias]+'.glb')).exists(),alias
    assert 600<=len(author.records)<=3500 and sum(p['records'] for p in author.layout['preserve'])+len(author.records)<=8192
    assert max(collections.Counter(r['alias'] for r in author.records).values())<=4096
    homes=[r for r in author.records if r['alias'].startswith(('village_house_','purchased_house_'))]
    extra_homes=sum(p['alias'].startswith('purchased_house_') for p in author.purchased.get('placements',[]))
    if author.demo_district:
        assert len(homes)==author.pre_demo_homes-len(author.pre_demo_home_labels.intersection(author.demo_district['removed_labels']))+author.demo_district['added_homes']
    else:assert len(homes)==40+extra_homes
    for i,a in enumerate(homes):
        for b in homes[i+1:]:
            assert separated_bounds(author,a,b),('Overlapping whole-house/eave bounds',a['label'],b['label'])
    purchased_checks=0
    product_checks=0
    goods=[r for r in author.records if r['alias'] in PURCHASED_GOODS]
    for i,a in enumerate(goods):
        for b in goods[i+1:]:
            if a.get('surface_parent')!=b.get('surface_parent'):continue
            assert separated_bounds(author,a,b,.008),('Intersecting product footprints',a['label'],b['label'])
            product_checks+=1
    for a in [r for r in author.records if r['alias'] in PURCHASED_BUILDINGS]:
        for b in author.records:
            if a is b or b['alias'] in NON_SOLID or b.get('parent_structure')==a['label']:continue
            assert separated_bounds(author,a,b),('Purchased footprint overlap',a['label'],b['label'])
            purchased_checks+=1
        lo,hi=author.model(a['alias']);c,s=math.cos(a['yaw']),math.sin(a['yaw'])
        for road in author.layout['roads']:
            la=[c*(road['a'][0]-a['x'])-s*(road['a'][1]-a['z']),s*(road['a'][0]-a['x'])+c*(road['a'][1]-a['z'])]
            lb=[c*(road['b'][0]-a['x'])-s*(road['b'][1]-a['z']),s*(road['b'][0]-a['x'])+c*(road['b'][1]-a['z'])]
            clearance=rectangle_distance(la,lb,[lo[0]*a['scale'],lo[2]*a['scale']],[hi[0]*a['scale'],hi[2]*a['scale']])-road['half_width']
            assert clearance>.37,('Purchased building blocks road',a['label'],road['id'],clearance)
    minimum=math.inf
    for r in author.records:
        assert all(math.isfinite(r[k]) for k in ['x','z','yaw','scale','burial','elevation'])
        inside=abs(r['x'])<225 and abs(r['z'])<225
        exterior_bridge=r['label']=='castle_creek_drawbridge' and r['x']==0 and r['z']==225
        assert 0<r['scale']<=3 and (inside or exterior_bridge)
        if r['alias'] in NON_SOLID or r['alias']=='starting_gate':continue
        lo,hi=author.model(r['alias']);c=math.cos(r['yaw']);s=math.sin(r['yaw'])
        for a,b in zip(ROUTE,ROUTE[1:]):
            la=[c*(a[0]-r['x'])-s*(a[1]-r['z']),s*(a[0]-r['x'])+c*(a[1]-r['z'])]
            lb=[c*(b[0]-r['x'])-s*(b[1]-r['z']),s*(b[0]-r['x'])+c*(b[1]-r['z'])]
            clearance=rectangle_distance(la,lb,[lo[0]*r['scale'],lo[2]*r['scale']],[hi[0]*r['scale'],hi[2]*r['scale']])-.37
            minimum=min(minimum,clearance);assert clearance>.15,(r['label'],clearance)
    max_grade=0;stations=0
    for a,b in zip(ROUTE,ROUTE[1:]):
        length=math.hypot(b[0]-a[0],b[1]-a[1]);steps=math.ceil(length/.5);previous=None
        for i in range(steps+1):
            x=ORIGIN[0]+a[0]+(b[0]-a[0])*i/steps;z=ORIGIN[1]+a[1]+(b[1]-a[1])*i/steps
            h=atlas.walk_height(x,z)
            expected=24.02 if abs(x-ORIGIN[0])<=6.5 and 212.5<=z-ORIGIN[1]<=225 else 24.
            assert abs(h-expected)<.001
            if previous is not None:max_grade=max(max_grade,abs(h-previous)/(length/steps))
            previous=h;stations+=1
    demo_checks={}
    if author.demo_district:
        from build_village_demo_district import validate as validate_demo_district
        demo_checks['demo_district']=validate_demo_district(author)
    if author.demo_dressing:
        from build_village_demo_dressing import validate as validate_demo_dressing
        demo_checks['demo_dressing']=validate_demo_dressing(author)
    if getattr(author,'demo_bunting',None):
        from build_village_demo_bunting import validate as validate_demo_bunting
        demo_checks['demo_bunting']=validate_demo_bunting(author)
    if getattr(author,'demo_frontages',None):
        from build_village_demo_frontages import validate as validate_demo_frontages
        demo_checks['demo_frontages']=validate_demo_frontages(author)
    if getattr(author,'demo_market_lively',None):
        from build_village_market_lively import validate as validate_market_lively
        demo_checks['demo_market_lively']=validate_market_lively(author)
    if getattr(author,'demo_market_hero',None):
        from build_village_market_hero import validate as validate_market_hero
        demo_checks['demo_market_hero']=validate_market_hero(author)
    if getattr(author,'demo_hero_grove',None):
        from build_village_hero_grove import validate as validate_hero_grove
        demo_checks['demo_hero_grove']=validate_hero_grove(author)
    if getattr(author,'castle_expansion',None):
        from build_castle_expansion import validate as validate_expansion
        from build_castle_drawbridge import validate_placement
        expansion_view=author
        if getattr(author,'village_reinforcements',None):
            from build_village_reinforcements import earlier_expansion_view
            expansion_view=earlier_expansion_view(author)
        demo_checks['castle_expansion']=validate_expansion(expansion_view)
        demo_checks['castle_creek']=validate_placement(author)
    if getattr(author,'castle_inner_wing',None):
        from build_castle_inner_wing import validate as validate_wings
        demo_checks['castle_inner_wing']=validate_wings(author)
    if getattr(author,'demo_castle_creek_banks',None):
        from build_castle_creek_banks import validate as validate_banks
        demo_checks['castle_creek_banks']=validate_banks(author)
    if getattr(author,'village_torches',None):
        from build_village_torches import validate as validate_torches
        demo_checks['village_torches']=validate_torches(author)
    if getattr(author,'village_farms',None):
        from build_village_farms import validate as validate_farms
        demo_checks['village_farms']=validate_farms(author)
    if getattr(author,'village_reinforcements',None):
        from build_village_reinforcements import validate as validate_reinforcements
        demo_checks['village_reinforcements']=validate_reinforcements(author)
    return dict(records=len(author.records),homes=len(homes),whole_house_eave_overlaps=0,combined_records=sum(p['records'] for p in author.layout['preserve'])+len(author.records),
                route_capsule_minimum_margin_m=minimum,route_stations=stations,route_max_grade=max_grade,
                route_length_m=sum(math.dist(a,b) for a,b in zip(ROUTE,ROUTE[1:])),actual_models_required=require_models,
                purchased_buildings=sum(r['alias'] in PURCHASED_BUILDINGS for r in author.records),
                purchased_goods=sum(r['alias'] in PURCHASED_GOODS for r in author.records),
                purchased_footprint_clearance_checks=purchased_checks,product_surface_checks=len(author.attachments)*5,
                product_pair_clearance_checks=product_checks,authored_lanes=validate_authored_lanes(author,atlas),
                **validate_masonry(author),**validate_dressing(author),**demo_checks)

def outputs(author,validation):
    lines=['# Valdoa castle village. Generated from castle_village_layout.json.',
           '# alias map_u map_v yaw_degrees scale burial_meters [elevation_meters]']
    for r in author.records:
        values=[.487+r['x']/WIDTH,.320+r['z']/DEPTH,math.degrees(r['yaw']),r['scale'],r['burial']]
        if r['elevation']:values.append(r['elevation'])
        lines.append(r['alias']+' '+' '.join(format(v,'.12g') for v in values)+' # '+r['label'])
    text='\n'.join(lines)+'\n';(DIRECTORY/'castle_village_instances.txt').write_text(text)
    manifest=dict(schema_version=1,title=author.layout['title'],origin_world_xz=ORIGIN,validation=validation,
                  aliases=dict(sorted(collections.Counter(r['alias'] for r in author.records).items())),
                  instances_sha256=hashlib.sha256(text.encode()).hexdigest(),placements=author.records)
    manifest['models']={a:dict(file=ALIASES[a]+'.glb',bounds_y_up=author.model(a),sha256=digest(MODELS/(ALIASES[a]+'.glb')))
                        for a in sorted({r['alias'] for r in author.records}) if (MODELS/(ALIASES[a]+'.glb')).exists()}
    manifest['collision_manifest_sha256']=digest(MODELS/'village_asset_manifest.json') if (MODELS/'village_asset_manifest.json').exists() else None
    manifest['collision_include_sha256']=digest(ROOT/'engine/world_village_colliders.inc')
    # Effective recipe overlay data is shared by ground authoring, demo QA and
    # the generated runtime route. The editable base layout stays independent.
    manifest['roads']=author.layout['roads'];manifest['squares']=author.layout['squares']
    if author.demo_district:
        manifest['demo_district']=author.demo_district
        manifest['demo_districts']=author.demo_district['districts']
        manifest['demo_route_xz']=author.layout['demo_route_xz']
        manifest['demo_views']=author.layout['demo_views']
    if author.demo_dressing:manifest['demo_dressing']=author.demo_dressing
    if getattr(author,'demo_bunting',None):manifest['demo_bunting']=author.demo_bunting
    if getattr(author,'demo_frontages',None):manifest['demo_frontages']=author.demo_frontages
    if getattr(author,'demo_market_lively',None):manifest['demo_market_lively']=author.demo_market_lively
    if getattr(author,'demo_market_hero',None):manifest['demo_market_hero']=author.demo_market_hero
    if getattr(author,'demo_hero_grove',None):manifest['demo_hero_grove']=author.demo_hero_grove
    if getattr(author,'castle_expansion',None):manifest['castle_expansion']=author.castle_expansion
    if getattr(author,'castle_creek',None):manifest['castle_creek']=author.castle_creek
    if getattr(author,'castle_inner_wing',None):manifest['castle_inner_wing']=author.castle_inner_wing
    if getattr(author,'demo_castle_creek_banks',None):manifest['demo_castle_creek_banks']=author.demo_castle_creek_banks
    if getattr(author,'village_torches',None):
        manifest['village_torches']={**author.village_torches,
            'generated_include_sha256':digest(ROOT/author.village_torches['generated_include'])}
        source=json.loads((MODELS/'purchased_torches_manifest.json').read_text())
        manifest['purchased_torches']=dict(manifest='projects/valwood_world/models/purchased_torches_manifest.json',
            sha256=digest(MODELS/'purchased_torches_manifest.json'),provenance=source['provenance'],
            provenance_sha256=digest(ROOT/source['provenance']),placed_aliases=author.village_torches['aliases'],
            available_source_variants=5,used_source_variants=3,available_lod_models=10,
            maximum_near_lights=8,shared_atlas_images=4,budget=source['budget'])
    if getattr(author,'village_farms',None):
        manifest['village_farms']={**author.village_farms,
            'generated_include_sha256':digest(ROOT/author.village_farms['generated_include'])}
    if getattr(author,'village_reinforcements',None):
        manifest['village_reinforcements']={**author.village_reinforcements,
            'generated_include_sha256':digest(ROOT/author.village_reinforcements['generated_include'])}
    if author.purchased:
        manifest['purchased_details']=dict(file=author.layout['purchased_details'],
            sha256=digest(DIRECTORY/author.layout['purchased_details']),surface_attachments=author.attachments,
            supported_overhead_rows=author.overheads,dressing_pockets=author.purchased.get('dressing_pockets',[]))
        manifest['authored_lanes']=author.purchased.get('authored_lanes',[])
    (DIRECTORY/'castle_village_manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    svg=['<svg xmlns="http://www.w3.org/2000/svg" viewBox="-285 -285 570 570"><rect x="-280" y="-280" width="560" height="560" fill="#526446"/>']
    for p in author.layout['roads']:svg.append(f'<path d="M {p["a"][0]} {p["a"][1]} L {p["b"][0]} {p["b"][1]}" stroke="#c3b18b" stroke-width="{p["half_width"]*2}"/>')
    for r in author.records:
        if r['alias'] in PLANTS:continue
        lo,hi=author.model(r['alias']);color='#948b7d' if 'starting_wall' in r['alias'] else '#c6aa71'
        svg.append(f'<g transform="translate({r["x"]} {r["z"]}) rotate({-math.degrees(r["yaw"])})"><rect x="{lo[0]*r["scale"]}" y="{lo[2]*r["scale"]}" width="{(hi[0]-lo[0])*r["scale"]}" height="{(hi[2]-lo[2])*r["scale"]}" fill="{color}" stroke="#39322a" stroke-width=".4"><title>{html.escape(r["label"])}</title></rect></g>')
    for lane in author.purchased.get('authored_lanes',[]):
        path=' '.join(('M' if i==0 else 'L')+f'{p[0]} {p[1]}' for i,p in enumerate(lane['walking_route_xz']))
        svg.append(f'<path d="{path}" fill="none" stroke="#163f5b" stroke-width="1.2"><title>{html.escape(lane["title"])}</title></path>')
    svg.append('<path d="'+' '.join(('M' if i==0 else 'L')+f'{p[0]} {p[1]}' for i,p in enumerate(ROUTE))+'" fill="none" stroke="#a52e28" stroke-width="1.2"/></svg>')
    (DIRECTORY/'castle_village_layout.svg').write_text('\n'.join(svg)+'\n')

def demo_include(author):
    out=['/* Generated by tools/build_castle_village.py; demo route in local east/south metres. */',
         'static constexpr double castle_demo_route[][2]={']
    for p in author.layout.get('demo_route_xz',ROUTE):out.append('    {'+','.join(format(v,'.12g') for v in p)+'},')
    out+=['};','static constexpr double castle_market_hero_route[][2]={']
    for p in getattr(author,'demo_market_hero',{}).get('walking_route_xz',[[0,220],[0,221],[0,220]]):
        out.append('    {'+','.join(format(v,'.12g') for v in p)+'},')
    out+=['};','static constexpr double castle_expansion_route[][2]={']
    for p in getattr(author,'castle_expansion',{}).get('route_xz',[[0,-30],[0,-31],[0,-30]]):
        out.append('    {'+','.join(format(v,'.12g') for v in p)+'},')
    out+=['};','static constexpr double castle_farms_route[][2]={']
    for p in getattr(author,'village_farms',{}).get('walking_route_xz',[[141,-136],[141,-137],[141,-136]]):
        out.append('    {'+','.join(format(v,'.12g') for v in p)+'},')
    out+=['};',''];return '\n'.join(out)

def demo_views_include(author):
    titles={'castle-demo':'Castle district demo','castle-housing':'Southgate Lane',
            'castle-marketrow':'Market Row','castle-forecourt':'Keep approach','castle-hall':'Castle receiving hall',
            'castle-hero-entry':'Bakers and Brewers | west approach','castle-hero-mid':'Bakers and Brewers | covered counters',
            'castle-hero-reverse':'Bakers and Brewers | return view',
            'castle-training':'Defender training yard','castle-statue':'Defender monument',
            'castle-armory':'Royal armory terrace','castle-drawbridge':'The lowered drawbridge',
            'castle-creek':'Castle creek bank','castle-farms':'Corn and wheat farms',
            'castle-farms-reverse':'Farm headlands and castle','castle-watch':'Southgate mounted and shield watch',
            'castle-farm-tools':'Farm workshop and source tools','castle-watch-close':'Shield watch armor and idle'}
    out=['/* Generated demo inspection cameras from the authored recipe. */']
    def number(value):
        value=format(value,'.9g')
        return value+('f' if any(k in value for k in '.eE') else '.0f')
    for row in author.layout.get('demo_views',[]):
        x,z=row['position_xz'];tx,tz=row['target_xz'];yaw=math.atan2(tx-x,-(tz-z))
        assert row['id'] in titles
        out.append('    {"'+row['id']+'", "Valdoa | '+titles[row['id']]+'", '+
                   format(.487+x/WIDTH,'.15g')+', '+format(.320+z/DEPTH,'.15g')+', '+
                   number(yaw)+', '+number(row['pitch'])+', 1.75},')
    out.append('');return '\n'.join(out)

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--force',action='store_true');parser.add_argument('--validate-only',action='store_true');parser.add_argument('--require-models',action='store_true');args=parser.parse_args()
    if not PATH.exists():assert not args.validate_only;PATH.write_text(json.dumps(initial_layout(),indent=2)+'\n')
    layout=json.loads(PATH.read_text());author=Author(layout);author.compose()
    if args.validate_only:
        actual=[]
        for line in (DIRECTORY/'castle_village_instances.txt').read_text().splitlines():
            line=line.split('#',1)[0].strip()
            if line:actual.append(line.split())
        assert len(actual)==len(author.records)
        for tokens,r in zip(actual,author.records):
            assert tokens[0]==r['alias'];values=list(map(float,tokens[1:]));r.update(x=(values[0]-.487)*WIDTH,z=(values[1]-.320)*DEPTH,yaw=math.radians(values[2]),scale=values[3],burial=values[4],elevation=values[5] if len(values)>5 else 0)
    else:
        destination=DIRECTORY/'castle_village_instances.txt';assert args.force or not destination.exists(),'Use --force to replace this village only'
        write_generated(ROOT/'engine/world_village_layout.inc',mask_include(author))
        write_generated(ROOT/'engine/world_village_colliders.inc',collision_include(args.require_models))
        write_generated(ROOT/'engine/world_castle_demo.inc',demo_include(author))
        write_generated(ROOT/'engine/world_castle_demo_views.inc',demo_views_include(author))
        if getattr(author,'village_torches',None):
            from build_village_torches import include as torch_include
            write_generated(ROOT/author.village_torches['generated_include'],torch_include(author.village_torches))
        if getattr(author,'village_farms',None):
            from build_village_farms import include as farms_include
            write_generated(ROOT/author.village_farms['generated_include'],farms_include(author.village_farms))
        if getattr(author,'village_reinforcements',None):
            from build_village_reinforcements import include as reinforcement_include
            write_generated(ROOT/author.village_reinforcements['generated_include'],reinforcement_include(author.village_reinforcements))
    with tempfile.TemporaryDirectory(prefix='valdoa-author-') as directory:validation=validate(author,Atlas(directory),args.require_models)
    if not args.validate_only:outputs(author,validation)
    print(json.dumps(validation,indent=2))

if __name__=='__main__':main()
