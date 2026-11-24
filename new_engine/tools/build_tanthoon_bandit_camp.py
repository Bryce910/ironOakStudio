#!/usr/bin/env python3
"""Author the additive Old Tanthoon outlaw camp from existing licensed models.

No original scenery, model, terrain height or preference file is modified.
The bounded ground include contains material/ecology masks only. CPU-only;
--atlas-library reuses an existing current atlas without compiling a helper.
"""
import argparse
from collections import Counter
import ctypes as C
import hashlib
import html
import json
import math
from pathlib import Path
import random
import re
import struct
import tempfile

from build_starting_area import Atlas, Sample, glb_bounds, rectangle_distance, segment_distance

ROOT=Path(__file__).resolve().parents[1]
SCENERY=ROOT/'projects/valwood_world/scenery'
MODELS=ROOT/'projects/valwood_world/models'
STEM='tanthoon_bandit_camp'
ORIGIN=(-20600.,8946.320622878819)
WIDTH,DEPTH=100800.,100800.*2818/5009
LOOP=[[0,30],[-10,18],[-12,0],[-10,-18],[0,-24],[12,-18],[14,0],[12,18],[0,30]]
PATHS=[dict(id='patrol_loop',points_local_xz=LOOP,half_width_m=2.,feather_m=1.2),
       dict(id='south_approach',points_local_xz=[[0,88],[0,52],[0,30]],half_width_m=2.4,feather_m=1.2),
       dict(id='escape_path',points_local_xz=[[-10,-18],[-25,-30],[-42,-48],[-53,-58]],half_width_m=1.8,feather_m=1.2)]
CAMERAS=[('bandit-camp','Tanthoon | Outlaw camp',0,48,0.,-.045),
         ('bandit-supplies','Tanthoon | Stolen supplies',17,7,math.atan2(9,7),-.08),
         ('bandit-lookout','Tanthoon | Lookout',15,-8,math.atan2(11,10),.16),
         ('bandit-fire','Tanthoon | Campfire',7,8,-.72,-.16)]
PROTECTED=[SCENERY/(s+'_instances.txt')for s in ('authored','starting_area','rainforest','castle_village')]


def sha(path):
    digest=hashlib.sha256()
    with path.open('rb')as source:
        for block in iter(lambda:source.read(1024*1024),b''):digest.update(block)
    return digest.hexdigest()


def protected():
    return [dict(file=str(p.relative_to(ROOT)),records=sum(bool(s.strip())and not s.lstrip().startswith('#')for s in p.read_text().splitlines()),sha256=sha(p))for p in PROTECTED]


class Terrain:
    def __init__(self,library=None):
        self.temporary=None
        if library:self.api=C.CDLL(str(Path(library).resolve()))
        else:
            self.temporary=tempfile.TemporaryDirectory(prefix='valwood-bandit-authoring-')
            self.owner=Atlas(self.temporary.name);self.api=self.owner.api
        self.api.world_atlas_init.restype=C.c_bool
        assert self.api.world_atlas_init()
        self.api.world_atlas_sample.argtypes=[C.c_double,C.c_double];self.api.world_atlas_sample.restype=Sample
        self.api.world_atlas_height.argtypes=[C.c_double,C.c_double];self.api.world_atlas_height.restype=C.c_float
        self.api.world_atlas_local_water.argtypes=[C.c_double,C.c_double,C.c_void_p,C.c_void_p];self.api.world_atlas_local_water.restype=C.c_bool
    def height(self,x,z):return self.api.world_atlas_height(ORIGIN[0]+x,ORIGIN[1]+z)
    def sample(self,x,z):return self.api.world_atlas_sample(ORIGIN[0]+x,ORIGIN[1]+z)
    def wet(self,x,z):return self.api.world_atlas_local_water(ORIGIN[0]+x,ORIGIN[1]+z,None,None)


class Author:
    def __init__(self,terrain):
        self.terrain=terrain;self.records=[];self.models={};self.aliases={}
        source=(ROOT/'engine/world_scenery.cpp').read_text()
        for m in re.finditer(r'\{"([^"]+)",\s*"([^"]+)"[^\n]+Grounding::(\w+),\s*Collision::(\w+)',source):
            self.aliases[m[1]]=(m[2],m[3],m[4])
    def model(self,alias):
        if alias not in self.models:
            stem,grounding,collision=self.aliases[alias]
            near=MODELS/(stem+'.glb')
            if not near.exists():near=MODELS/(stem+'_near.glb')
            low,high=glb_bounds(near)
            with near.open('rb')as f:
                f.seek(12);n,kind=struct.unpack('<II',f.read(8));assert kind==0x4e4f534a;data=json.loads(f.read(n))
            triangles=sum(data['accessors'][p['indices']]['count']//3 for mesh in data['meshes']for p in mesh['primitives'])
            self.models[alias]=dict(file=str(near.relative_to(ROOT)),bounds_min=low,bounds_max=high,triangles=triangles,grounding=grounding,collision=collision)
        return self.models[alias]
    def add(self,id,alias,x,z,yaw=0,scale=1,burial=0,elevation=0,zone='camp',allow_route=False):
        model=self.model(alias);s=self.terrain.sample(x,z)
        assert s.region==3 and s.shore>100 and s.height>1 and not self.terrain.wet(x,z),(id,'not dry Old Tanthoon')
        assert .1<=scale<=4 and 0<=elevation<=3
        lo,hi=model['bounds_min'],model['bounds_max'];angle=math.radians(yaw);c,si=math.cos(angle),math.sin(angle)
        heights=[self.terrain.height(x+c*lx+si*lz,z-si*lx+c*lz)for lx in[lo[0]*scale+(hi[0]-lo[0])*scale*i/4 for i in range(5)]for lz in[lo[2]*scale+(hi[2]-lo[2])*scale*i/4 for i in range(5)]]
        y=s.height-burial
        if model['grounding']=='Support':y=min([y+burial]+heights)-burial-.05
        if model['grounding']=='GroundTop':y+=.02-hi[1]*scale
        y+=elevation
        record=dict(id=id,alias=alias,zone=zone,local_xz=[x,z],world_xz=[ORIGIN[0]+x,ORIGIN[1]+z],grounded_y=y,yaw_degrees=yaw,scale=scale,burial_m=burial,elevation_m=elevation,terrain_min=min(heights),terrain_max=max(heights),support_spread_m=max(heights)-min(heights),bounds_min=lo,bounds_max=hi)
        # A conservative full-source OBB proof protects the ENTIRE2m lane,
        # stricter than runtime's actual source-specific/trunk collider boxes.
        if not allow_route and model['collision']!='None':
            for path in PATHS:
                for a,b in zip(path['points_local_xz'],path['points_local_xz'][1:]):
                    aa=[c*(a[0]-x)-si*(a[1]-z),si*(a[0]-x)+c*(a[1]-z)]
                    bb=[c*(b[0]-x)-si*(b[1]-z),si*(b[0]-x)+c*(b[1]-z)]
                    assert rectangle_distance(aa,bb,[lo[0]*scale,lo[2]*scale],[hi[0]*scale,hi[2]*scale])>path['half_width_m']+.4,(id,path['id'],'route overlap')
        self.records.append(record);return record


def populate(a):
    # Stolen rough-canvas shelters read as an improvised outlaw settlement,
    # rather than reusing the formal stone castle architecture.
    a.add('leader_hideout','purchased_shop_d',0,-33,180,1.15,zone='leader')
    a.add('west_bunk_shelter','purchased_shop_a',-23,-14,95,1.3,zone='sleeping')
    a.add('southwest_shelter','purchased_shop_b',-23,18,65,.85,zone='sleeping')
    a.add('east_bunk_shelter','purchased_shop_a',23,15,-95,1.35,zone='sleeping')
    a.add('north_lookout','purchased_shop_c',26,-18,-105,.92,zone='lookout')
    a.add('cooking_awning','purchased_shop_d',-26,5,90,.85,zone='cooking')
    # Entrance rails are irregular and interrupted at both real walking exits.
    rng=random.Random(487112)
    for axis,fixed,start,finish in [('x',34,-33,33),('x',-35,-33,33),('z',-35,-32,32),('z',35,-32,32)]:
        for i in range(int((finish-start)/2.1)+1):
            v=start+i*2.1;x,z=(v,fixed)if axis=='x'else(fixed,v)
            if z>30 and abs(x)<7:continue
            if z<-29 and -35<x<-22:continue
            try:a.add(f'perimeter_{axis}_{fixed}_{i}','castle_training_fence',x,z,(0 if axis=='x'else 90)+rng.uniform(-4,4),1.15,zone='barricade')
            except AssertionError:continue
    for i,(x,z,yaw)in enumerate([(-8,34,15),(8,34,-15),(-33,-21,95),(33,25,80)]):a.add(f'log_barricade_{i}','purchased_prop_logs',x,z,yaw,1.1,zone='barricade')
    a.add('main_hearth','starting_firepit',0,0,0,1.25,.03,zone='hearth')
    a.add('hearth_fuel','purchased_prop_logs',0,0,68,.32,zone='hearth')
    for i,(x,z,yaw)in enumerate([(-3,0,90),(3,0,-90),(0,-3,0)]):a.add(f'hearth_seat_{i}','castle_yard_bench',x,z,yaw,.75,zone='hearth')
    a.add('cooking_hearth','starting_firepit',-20,5,15,.8,.02,zone='cooking')
    a.add('cooking_fuel','purchased_prop_logs',-20,5,20,.22,zone='cooking')
    a.add('cook_table','starting_table',-24,8,90,.85,zone='cooking')
    for i,(alias,x,z)in enumerate([('purchased_goods_bread',-24.4,8),('purchased_goods_cheese',-24,8.4),('purchased_goods_chicken',-23.7,7.7),('purchased_goods_tankard',-24.5,8.5)]):
        a.add(f'cook_table_food_{i}',alias,x,z,i*77,.9,elevation=.634,zone='cooking')
    for i,(x,z)in enumerate([(-22,10),(-24,11),(-27,9)]):a.add(f'kitchen_cask_{i}','purchased_prop_barrel_small',x,z,i*31,.9,zone='cooking')
    a.add('cook_water_bucket','starting_bucket',-18.5,7,12,1,zone='cooking')
    a.add('cook_firepoker','village_toolkit_firepoker',-21.6,6.3,48,.8,zone='cooking')
    # Loot is grouped as wagon unloading, locked crates, food and firewood.
    a.add('stolen_wagon','village_cart',27,5,12,1.15,zone='supplies')
    for i,(x,z)in enumerate([(25,0),(26.2,-.2),(27.3,-.1),(25.2,-1.5),(26.4,-1.5),(24,-1.3)]):
        a.add(f'loot_crate_{i}','purchased_crate',x,z,i*11,1,zone='supplies')
    a.add('loot_crate_stack','purchased_crate',25,0,0,.85,elevation=.9931,zone='supplies')
    for i,(x,z)in enumerate([(23,-3),(24.2,-3.3),(25.6,-3.6),(27,-3.7),(28,-2.6)]):a.add(f'loot_sack_{i}','purchased_prop_sack',x,z,i*41,1,zone='supplies')
    for i,(x,z)in enumerate([(29,0),(30.1,.4),(30.4,1.7)]):a.add(f'stolen_ale_{i}','purchased_barrel',x,z,i*33,1,zone='supplies')
    a.add('cargo_rope','purchased_prop_rope',27,2,28,.85,zone='supplies')
    a.add('raided_weapon_rack','castle_weapon_rack',29,-10,180,.9,zone='lookout')
    a.add('leader_plunder_chest','purchased_crate',3,-31,12,1,zone='leader')
    a.add('leader_ale','purchased_barrel',-4,-32,-12,.9,zone='leader')
    a.add('leader_table','starting_table',0,-28,0,.7,zone='leader')
    a.add('leader_cup','purchased_goods_tankard',.3,-28,0,1,elevation=.5222,zone='leader')
    for i,(x,z)in enumerate([(-25,-17),(-26,-14),(25,19),(25,17),(-23,21)]):a.add(f'bunk_sack_{i}','purchased_prop_sack',x,z,i*66,.85,zone='sleeping')
    for i,(x,z)in enumerate([(-26,-11),(26,14),(-24,17)]):a.add(f'bunk_barrel_{i}','purchased_prop_barrel_small',x,z,i*31,.8,zone='sleeping')
    for i,(x,z)in enumerate([(-30,-3),(-30,-.8),(-30,1.4)]):a.add(f'wood_store_{i}','purchased_prop_logs',x,z,8+i*3,1,zone='work')
    a.add('wood_chopping_block','starting_stump',-27,-4,7,1.1,zone='work')
    a.add('wood_axe','starting_axe',-27,-4,25,.8,elevation=.65,zone='work')
    for i,(alias,x,z)in enumerate([('village_toolkit_hatchet',-29,-5),('village_toolkit_shovel',-29.6,-4),('village_toolkit_crowbar',26,3)]):a.add(f'working_tool_{i}',alias,x,z,i*57,.8,zone='work')
    for id,alias,x,z in [('gate_west_fire','purchased_torch_pole',-6,30),('gate_east_fire','purchased_torch_pole',6,30),('leader_brazier','purchased_torch_brazier',5,-31),('lookout_fire','purchased_torch_pole',29,-23)]:a.add(id,alias,x,z,0,.42 if alias=='purchased_torch_pole'else 1,zone='practical_lights')
    # Sparse authored woodland framing remains OUTSIDE the bounded clearing;
    # no extra source models or unique texture images are introduced.
    for i in range(28):
        angle=2*math.pi*i/28;radius=52+rng.uniform(-2,8);x,z=math.cos(angle)*radius,math.sin(angle)*radius
        try:a.add(f'forest_frame_{i}',['pine','fir','broadleaf'][i%3],x,z,rng.uniform(0,360),rng.uniform(.65,1.05),zone='woodland')
        except AssertionError:pass
    for i in range(94):
        angle=rng.uniform(0,2*math.pi);radius=rng.uniform(38,57);x,z=math.cos(angle)*radius,math.sin(angle)*radius
        if any(segment_distance([x,z],p,q)<path['half_width_m']+1 for path in PATHS for p,q in zip(path['points_local_xz'],path['points_local_xz'][1:])):continue
        alias=['fern0','fern1','fern2','showcase_sorrel'][i%4]
        a.add(f'undergrowth_{i}',alias,x,z,rng.uniform(0,360),rng.uniform(.65,1.15),zone='woodland')


def route_proof(a):
    length=0;max_grade=0;minimum=math.inf;maximum=-math.inf
    for path in PATHS:
        for p,q in zip(path['points_local_xz'],path['points_local_xz'][1:]):
            distance=math.dist(p,q)
            if path['id']=='patrol_loop':length+=distance
            steps=math.ceil(distance/.25);previous=None
            for i in range(steps+1):
                t=i/steps;x=p[0]+t*(q[0]-p[0]);z=p[1]+t*(q[1]-p[1]);y=a.terrain.height(x,z)
                assert a.terrain.sample(x,z).region==3 and not a.terrain.wet(x,z)
                minimum=min(minimum,y);maximum=max(maximum,y)
                if previous is not None:max_grade=max(max_grade,abs(y-previous)/(distance/steps))
                previous=y
    assert max_grade<.35
    return dict(benchmark_id='bandit-camp',length_m=length,return_walk_m=length,sampled_step_m=.25,max_absolute_grade=max_grade,terrain_min_m=minimum,terrain_max_m=maximum,clear_capsule_radius_m=.4,visual_lane_half_width_m=2.,proof='Conservative full-source OBB exclusion from each authored path plus dry CPU atlas grade probes; runtime capsule fixture is separate.')


def outputs(a):
    prefix=protected();counts=Counter(r['alias']for r in a.records)
    combined=Counter()
    for p in PROTECTED:
        combined.update(s.split()[0]for s in p.read_text().splitlines()if s.strip()and not s.lstrip().startswith('#'))
    combined.update(counts);assert sum(combined.values())<=8192 and max(combined.values())<=4096
    lines=['# Old Tanthoon outlaw camp; additive source, generated by tools/build_tanthoon_bandit_camp.py.']
    for r in a.records:
        x,z=r['world_xz'];line=f"{r['alias']} {x/WIDTH+.5:.14f} {z/DEPTH+.5:.14f} {r['yaw_degrees']:.6f} {r['scale']:.6f} {r['burial_m']:.6f}"
        if r['elevation_m']:line+=f" {r['elevation_m']:.6f}"
        lines.append(line+' # '+r['id'])
    # Runtime parser expects six/seven tokens; keep IDs on independent comments.
    text='\n'.join(['# '+r['id']+'\n'+line.split(' # ')[0] for r,line in zip(a.records,lines[1:])])+'\n'
    text=lines[0]+'\n'+text
    pads=[dict(id=r['id'],center_local_xz=r['local_xz'],half_extents_xz=[max(abs(r['bounds_min'][0]),abs(r['bounds_max'][0]))*r['scale']+.65,max(abs(r['bounds_min'][2]),abs(r['bounds_max'][2]))*r['scale']+.65],yaw_radians=math.radians(r['yaw_degrees']),feather_m=1.2)for r in a.records if r['zone']!='woodland']
    ground=dict(origin_world_xz=list(ORIGIN),bounds_local_xz=[-68,-70,68,95],bounds_world=[ORIGIN[0]-68,ORIGIN[1]-70,136,165],height_edits=False,clearing_ellipse=dict(center_local_xz=[0,0],radii_xz=[37,35],feather_m=10),paths=PATHS,pads=pads)
    cameras=[]
    for id,name,x,z,yaw,pitch in CAMERAS:
        assert not a.terrain.wet(x,z)
        cameras.append(dict(id=id,name=name,local_xz=[x,z],world_xz=[ORIGIN[0]+x,ORIGIN[1]+z],map_uv=[(ORIGIN[0]+x)/WIDTH+.5,(ORIGIN[1]+z)/DEPTH+.5],yaw_radians=yaw,pitch_radians=pitch,eye_height_m=1.75))
    layout=dict(schema_version=1,title='Old Tanthoon | Crows Rest outlaw camp',origin=dict(world_xz=list(ORIGIN),map_uv=[ORIGIN[0]/WIDTH+.5,ORIGIN[1]/DEPTH+.5],region=3,region_name='Old Tanthoon'),ground=ground,paths=PATHS,inspection_cameras=cameras,actor_stations=[dict(id=id,local_xz=p)for id,p in [('gate',[-4,24]),('lookout',[21,-14]),('leader',[0,-26]),('cook',[-18,4])]],records=a.records)
    route=route_proof(a)
    lights=[];torch_manifest=json.loads((MODELS/'purchased_torches_manifest.json').read_text())
    sockets={r['alias']:r['flame_socket_local_m']for r in torch_manifest['assets']if r['lod']=='near'}
    for id,height,width,intensity in [('main_hearth',.7,.35,38),('cooking_hearth',.5,.24,28),('gate_west_fire',.55,.16,36),('gate_east_fire',.55,.16,36),('leader_brazier',.6,.28,32),('lookout_fire',.5,.16,30)]:
        r=next(r for r in a.records if r['id']==id);x,z=r['local_xz']
        fuel= .18 if id=='main_hearth'else .11 if id=='cooking_hearth'else sockets['brazier'if id=='leader_brazier'else'pole'][1]
        lights.append(dict(id=id,local_x=x,local_z=z,world_x=ORIGIN[0]+x,world_z=ORIGIN[1]+z,base_y=r['grounded_y']+fuel*r['scale'],height=height,width=width,intensity=intensity,seed=78401+len(lights)*7919,source_record=id,fuel_socket_local_y_m=fuel))
    layout['lights']=lights
    ground_inc=['/* Generated ecology/material masks only. No terrain height edits. */',f'static const double bandit_origin[2]={{{ORIGIN[0]:.12f},{ORIGIN[1]:.12f}}};',f'static const double bandit_bounds[4]={{{ground["bounds_world"][0]:.12f},{ground["bounds_world"][1]:.12f},136.,165.}};','typedef struct {double ax,az,bx,bz,half_width,feather;} BanditGroundPath;','static const BanditGroundPath bandit_paths[]={']
    for path in PATHS:
        for p,q in zip(path['points_local_xz'],path['points_local_xz'][1:]):ground_inc.append('    {'+','.join(f'{v:.9f}'for v in p+q+[path['half_width_m'],path['feather_m']])+'},')
    ground_inc+=['};','typedef struct {double x,z,hx,hz,yaw,feather;} BanditGroundPad;','static const BanditGroundPad bandit_pads[]={']
    for p in pads:ground_inc.append('    {'+','.join(f'{v:.9f}'for v in p['center_local_xz']+p['half_extents_xz']+[p['yaw_radians'],p['feather_m']])+'},')
    ground_inc+=['};','static const double bandit_clearing[5]={0.,0.,37.,35.,10.};','']
    views='/* Generated camp inspection cameras, normalized full-map UV. */\n'+''.join(f'    {{"{c["id"]}","{c["name"]}",{c["map_uv"][0]:.14f},{c["map_uv"][1]:.14f},{c["yaw_radians"]:.9f}f,{c["pitch_radians"]:.9f}f,1.75f}},\n'for c in cameras)
    route_inc='/* Generated closed return walk; absolute WORLD XZ metres. */\nstatic const double bandit_walk_route[][2]={\n'+''.join(f'    {{{ORIGIN[0]+p[0]:.12f},{ORIGIN[1]+p[1]:.12f}}},\n'for p in LOOP)+'};\n'
    torches=['/* Generated fuel-base sockets; camp-local XZ, absolute world Y. */',f'static constexpr double bandit_camp_center_x={ORIGIN[0]:.12f},bandit_camp_center_z={ORIGIN[1]:.12f};','static constexpr VillageTorchEmitter bandit_torch_emitters[]={']
    for e in lights:torches.append(f'    {{{e["local_x"]:.9f},{e["base_y"]:.9f},{e["local_z"]:.9f},{e["height"]}f,{e["width"]}f,{e["intensity"]}.f,{e["seed"]}u,0.f,0.f,0.f,1.f,-68.f,-70.f,68.f,95.f,0.f,1000.f}}, // {e["id"]}')
    torches+=['};','']
    files={SCENERY/(STEM+'_instances.txt'):text,SCENERY/(STEM+'_layout.json'):json.dumps(layout,indent=2)+'\n',SCENERY/(STEM+'_ground.json'):json.dumps(ground,indent=2)+'\n',ROOT/'engine/world_bandit_ground.inc':'\n'.join(ground_inc),ROOT/'engine/world_bandit_views.inc':views,ROOT/'engine/world_bandit_route.inc':route_inc,ROOT/'engine/world_bandit_torches.inc':'\n'.join(torches)}
    svg=['<svg xmlns="http://www.w3.org/2000/svg" viewBox="-70 -78 140 180" width="840" height="1080">',
         '<rect x="-70" y="-78" width="140" height="180" fill="#ece5cf"/>',
         '<text x="-64" y="-70" font-size="4.2" fill="#302f23">Crows Rest · Old Tanthoon</text>',
         '<text x="-64" y="-64" font-size="2.3">Authored placement plan · metres east/south · north ↑</text>']
    colors={'woodland':'#58794c','leader':'#996b54','sleeping':'#8f8065','lookout':'#b49b69','supplies':'#a9844f','cooking':'#c29a69','hearth':'#b75d3e','barricade':'#77644a','work':'#8d7455','practical_lights':'#dd9d31'}
    for path in PATHS:
        svg.append('<polyline points="'+' '.join(f'{p[0]},{p[1]}'for p in path['points_local_xz'])+f'" fill="none" stroke="#c4b185" stroke-width="{2*path["half_width_m"]}" stroke-linejoin="round"/>')
    for r in a.records:
        x,z=r['local_xz'];lo,hi=r['bounds_min'],r['bounds_max'];scale=r['scale'];fill=colors[r['zone']]
        if r['zone']=='woodland':
            radius=1.8 if a.models[r['alias']]['collision']=='Trunk'else .35
            svg.append(f'<circle cx="{x}" cy="{z}" r="{radius}" fill="{fill}" opacity=".7"/>')
        else:svg.append(f'<rect x="{lo[0]*scale}" y="{lo[2]*scale}" width="{(hi[0]-lo[0])*scale}" height="{(hi[2]-lo[2])*scale}" transform="translate({x},{z}) rotate({-r["yaw_degrees"]})" fill="{fill}" stroke="#544b3b" stroke-width=".12"><title>{html.escape(r["id"])}</title></rect>')
    for label,x,z in [('Leader',0,-39),('Lookout',26,-25),('Stolen supplies',25,-6),('Cooking',-29,13),('Campfire',0,7),('Entrance',0,41),('Escape',-47,-53)]:svg.append(f'<text x="{x}" y="{z}" font-size="2.5" text-anchor="middle" fill="#342e22">{label}</text>')
    for camera in cameras:
        x,z=camera['local_xz'];svg.append(f'<circle cx="{x}" cy="{z}" r=".9" fill="#326d94"><title>{camera["id"]}</title></circle>')
    svg+=['<path d="M40,87 h20 m-20,-1 v2 m20,-2 v2" stroke="#342e22" stroke-width=".3"/>','<text x="50" y="92" font-size="2.5" text-anchor="middle">20 metres</text>','</svg>']
    files[SCENERY/(STEM+'_layout.svg')]='\n'.join(svg)+'\n'
    manifest=dict(schema_version=1,layout_source=STEM+'_layout.json',instances_source=STEM+'_instances.txt',layout_sha256=hashlib.sha256(files[SCENERY/(STEM+'_layout.json')].encode()).hexdigest(),instances_sha256=hashlib.sha256(text.encode()).hexdigest(),record_count=len(a.records),counts=dict(sorted(counts.items())),largest_combined_alias_batch=max(combined.values()),combined_record_count=sum(combined.values()),protected_sources=prefix,origin_world_xz=list(ORIGIN),models=a.models,records=a.records,route=route,inspection_cameras=cameras,lights=lights,performance=dict(new_geometry_files=0,new_unique_texture_images=0,near_source_triangles_upper_bound=sum(a.models[r['alias']]['triangles']for r in a.records),note='All assets already licensed/imported; source LODs/shared images preserved. Native FPS verification belongs to the integrated renderer.'),ground_note='Ground overlay changes only clearing/path/material masks. Natural terrain and all previous scenery inputs remain unchanged.')
    files[SCENERY/(STEM+'_manifest.json')]=json.dumps(manifest,indent=2)+'\n'
    return files,manifest


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--atlas-library',type=Path);parser.add_argument('--validate-only',action='store_true');parser.add_argument('--force',action='store_true');args=parser.parse_args()
    before=protected();a=Author(Terrain(args.atlas_library));populate(a);files,manifest=outputs(a)
    for path,text in files.items():
        if args.validate_only:assert path.read_text()==text,('Stale authored camp output',path)
        else:
            if path.exists():
                if path.read_text()==text:continue
                if not args.force:raise SystemExit(f'{path} differs; pass --force to regenerate deliberately.')
            path.write_text(text)
    assert protected()==before
    print(f'PASS camp authoring: {len(a.records)} additive records; {len(a.models)} existing model families; no new textures; loop {manifest["route"]["length_m"]:.3f}m; max sampled grade {manifest["route"]["max_absolute_grade"]:.5f}; original4sources preserved.')


if __name__=='__main__':main()
