"""Deterministic near-pool rainforest planting for the finishing author.

This module never writes runtime sources. append_rainforest(author) uses the
caller's measured model inventory and production CPU atlas, appends200 bounded
records, and returns an independent support/clearance/provenance proof. The
seven previous source files stay read-only. Dense irregular pockets leave the
water core, old roots, complete rainforest trail, and pool inspection eye free.
"""
from collections import Counter
import ctypes as C
import hashlib
import math
from pathlib import Path
import random

from build_starting_area import TrailPoint,segment_distance

ROOT=Path(__file__).resolve().parents[1]
DIRECTORY=ROOT/'projects/valwood_world/scenery'
ORIGIN=(-1310.4,-10207.58474745458)
WIDTH=100800.;DEPTH=WIDTH*2818/5009
SOURCE_NAMES=('authored','starting_area','rainforest','castle_village','tanthoon_bandit_camp','castle_streets','bep_environment')
EXPECTED_RECORDS=200

class Pool(C.Structure):
    _fields_=[(name,C.c_double)for name in('x','z','radius_x','radius_z','surface_y','bank_feather')]

def sha(path):
    path=Path(path)
    h=hashlib.sha256()
    with path.open('rb')as stream:
        for block in iter(lambda:stream.read(1024*1024),b''):h.update(block)
    return h.hexdigest()

def radius(model,scale):
    lo,hi=model['bounds_min'],model['bounds_max']
    return math.hypot(max(abs(lo[0]),abs(hi[0])),max(abs(lo[2]),abs(hi[2])))*scale

def box_gap(x,z,radius_,old):
    ox,oz=old['world_xz'];angle=math.radians(old['yaw_degrees']);c,s=math.cos(angle),math.sin(angle)
    xx=c*(x-ox)-s*(z-oz);zz=s*(x-ox)+c*(z-oz)
    lo,hi=old['model']['bounds_min'],old['model']['bounds_max'];scale=old['scale']
    return math.hypot(max(lo[0]*scale-xx,0,xx-hi[0]*scale),max(lo[2]*scale-zz,0,zz-hi[2]*scale))-radius_

def append_rainforest(author):
    api=author.atlas.api
    api.world_atlas_rainforest_pool.restype=C.POINTER(Pool);pool=api.world_atlas_rainforest_pool().contents
    api.world_atlas_local_water.argtypes=[C.c_double,C.c_double,C.c_void_p,C.c_void_p];api.world_atlas_local_water.restype=C.c_bool
    api.world_atlas_rainforest_trail_points.argtypes=[C.POINTER(C.c_size_t)];api.world_atlas_rainforest_trail_points.restype=C.POINTER(TrailPoint)
    n=C.c_size_t();raw=api.world_atlas_rainforest_trail_points(C.byref(n));assert n.value==6
    trail=[(raw[i].x,raw[i].z,raw[i].half_width)for i in range(n.value)]
    sources=[];obstacles=[];old_roots=[]
    for name in SOURCE_NAMES:
        path=DIRECTORY/(name+'_instances.txt');sources.append(dict(file=str(path.relative_to(ROOT)),sha256=sha(path)))
        for line in path.read_text().splitlines():
            p=line.split('#',1)[0].split()
            if not p:continue
            x=(float(p[1])-.5)*WIDTH;z=(float(p[2])-.5)*DEPTH
            if abs(x-pool.x)>85 or abs(z-pool.z)>85:continue
            alias=p[0];model=author.model(alias);collision=model['collision']
            if collision=='Trunk':old_roots.append((x,z,float(p[4])*.65));continue
            if collision=='None':continue
            obstacles.append(dict(alias=alias,world_xz=[x,z],yaw_degrees=float(p[3]),scale=float(p[4]),model=model))
    accepted=[];rejects=Counter();probes=0;min_trail=math.inf;min_old=math.inf

    def try_place(id,alias,dx,dz,yaw,scale,burial,zone):
        nonlocal probes,min_trail,min_old
        x,z=pool.x+dx,pool.z+dz;model=author.model(alias);rad=radius(model,scale)
        lo,hi=model['bounds_min'],model['bounds_max'];collision=model['collision']
        # Keep the near inspection eye and the open water sightline. The side
        # pockets frame that opening instead of filling it with a screen of leaves.
        if math.hypot(dx-19,dz)<rad+2.25 or(dx>12 and abs(dz)<4.5+rad):rejects['inspection_opening']+=1;return False
        gap=min(segment_distance((x,z),a,b)-max(a[2],b[2])-1.2-rad for a,b in zip(trail,trail[1:]))
        if gap<.4:rejects['complete_trail_envelope']+=1;return False
        source_gap=min((box_gap(x,z,rad+.12,old)for old in obstacles),default=math.inf)
        if source_gap<0:rejects['old_rigid_root_or_stone']+=1;return False
        # Leaves can interweave, but no new plant begins inside an old trunk.
        if any(math.hypot(x-ox,z-oz)<root+.25 for ox,oz,root in old_roots):rejects['old_tree_root']+=1;return False
        for old in accepted:
            distance=math.dist((x,z),old['world_xz'])
            if old['alias']==alias and distance<(1.5 if alias=='bep_nature_palm'else .42):rejects['same_species_spacing']+=1;return False
            if collision!='None' or old['model']['collision']!='None':
                if box_gap(x,z,rad+.08,old)<0:rejects['new_rigid_footprint']+=1;return False
        c,s=math.cos(math.radians(yaw)),math.sin(math.radians(yaw));heights=[]
        for iz in range(5):
            for ix in range(5):
                lx=(lo[0]+(hi[0]-lo[0])*ix/4)*scale;lz=(lo[2]+(hi[2]-lo[2])*iz/4)*scale
                xx=x+c*lx+s*lz;zz=z-s*lx+c*lz;sample=author.atlas.sample(xx,zz);probes+=1
                if sample.height<=0 or sample.shore<.5 or sample.land<.75 or api.world_atlas_local_water(xx,zz,None,None):
                    rejects['full_footprint_water']+=1;return False
                heights.append(sample.height)
        relief=max(heights)-min(heights)
        if relief>min(.55,max(.12,hi[1]*scale*.42)):rejects['ground_relief']+=1;return False
        record=dict(id=id,alias=alias,world_xz=[x,z],local_xz=[x-ORIGIN[0],z-ORIGIN[1]],yaw_degrees=(yaw+180)%360-180,
                    scale=scale,burial_m=burial,elevation_m=0,zone=zone,parent=None,
                    ground_height_min_m=min(heights),ground_height_max_m=max(heights),
                    minimum_road_obb_gap_m=1.e9,minimum_actor_route_obb_gap_m=gap,
                    minimum_other_source_obb_gap_m=None if math.isinf(source_gap)else source_gap,
                    rainforest_pool_local_xz=[dx,dz])
        author.records.append(record);accepted.append({**record,'model':model})
        min_trail=min(min_trail,gap);min_old=min(min_old,source_gap);return True

    def pocket(name,center,radii,count,species,scales,zone,burial=.008):
        rng=random.Random('valwood-rainforest-finish-v1:'+name)
        for index in range(count):
            alias=species[index%len(species)]
            for trial in range(1400):
                # Offset unequal lobes create connected leaf masses and open
                # gaps, without a continuous ornamental ring around the pool.
                angle=rng.uniform(-math.pi,math.pi);q=math.sqrt(rng.random())
                lobe=.28 if index%4==0 else-.18
                dx=center[0]+(math.cos(angle)*q*.78+lobe)*radii[0]
                dz=center[1]+math.sin(angle)*q*radii[1]
                scale=rng.uniform(*scales)
                if try_place('rf_finish_'+name+f'_{index:02}',alias,dx,dz,rng.uniform(-180,180),scale,burial,zone):break
            else:raise ValueError(f'Rainforest finish pocket exhausted: {name}/{alias}/{index}; rejected{dict(rejects)}')

    # Rigid source stones first: subsequent plant envelopes keep these visible.
    for i,center in enumerate([(-15,-17),(-17,13),(-22,-3),(-7,-21),(5,20),(5,-28)]):
        radii=(6.,5.)if i==5 else(3.8,3.4)
        pocket('stone_cropping_'+str(i),center,radii,2,['rock0','rock1','rock2'],(.47,.73),'rainforest_stone_croppings',.12)
    for name,center,radii,count in[
        ('northwest',(-10,-15),(5,4),12),('southwest',(-11,14),(5,4),12),
        ('far_bank',(-19,0),(6,12),12),('near_north',(8,-16),(4,5),6),('near_south',(8,17),(4,5),6)]:
        pocket('ferns_'+name,center,radii,count,['bep_nature_fern','bep_nature_fern','fern1'],(.82,1.20),'rainforest_layered_ferns')
    for name,center,radii in[('north',(-6,-16),(6,3)),('west',(-12,0),(3,8)),('south',(-5,17),(6,3))]:
        pocket('reeds_'+name,center,radii,12,['bep_nature_reed'],(.68,1.03),'rainforest_reed_fringe')
    for i,(center,radii)in enumerate([((7,-16),(5,4)),((7,16),(5,4)),((-16,-8),(4,5)),((-16,9),(4,5)),((-4,-23),(6,3)),((-4,23),(6,3))]):
        pocket('broad_grass_'+str(i),center,radii,7,['bep_nature_grass_broad','bep_nature_grass_long'],(.78,1.16),'rainforest_broad_grass')
    for name,center,radii,count in[('far',(-25,0),(7,17),10),('north',(-6,-25),(9,4),4),('south',(-9,25),(9,4),6)]:
        pocket('bushes_'+name,center,radii,count,['bep_nature_bush_a','bep_nature_bush_b'],(.78,1.16),'rainforest_shrub_masses',.014)
    for name,center,radii,count in[('back_north',(-27,-15),(9,9),6),('back_south',(-27,15),(8,9),6),
                                  ('north',(0,-26),(9,5),3),('south',(3,27),(8,5),3)]:
        pocket('palms_'+name,center,radii,count,['bep_nature_palm'],(.92,1.22),'rainforest_palm_under_canopy',.018)
    for i,center in enumerate([(-3,-19),(-7,20),(-18,-9),(-18,8)]):
        pocket('leaf_mats_'+str(i),center,(6,3),6,['bep_nature_clover','showcase_sorrel'],(.85,1.17),'rainforest_leaf_mats')
    assert len(accepted)==EXPECTED_RECORDS
    assert Counter(r['alias']for r in accepted)['bep_nature_palm']==18
    for source in sources:assert sha(ROOT/source['file'])==source['sha256'],'Previous source mutated during rainforest recipe'
    return dict(generator='tools/demo_rainforest_finish.py',generator_sha256=sha(__file__),records=len(accepted),
                aliases=dict(Counter(r['alias']for r in accepted)),zones=dict(Counter(r['zone']for r in accepted)),
                protected_sources=sources,production_cpu_atlas=True,full_footprint_water_grid=[5,5],terrain_probes=probes,
                minimum_complete_trail_clearance_m=min_trail,minimum_old_rigid_gap_m=None if math.isinf(min_old)else min_old,
                old_tree_roots_reserved=True,inspection_eye_and_water_sightline_reserved=True,
                pool_core_untouched=True,pool_center_world_xz=[pool.x,pool.z],palms=18,
                deterministic_seed='valwood-rainforest-finish-v1',rejected=dict(rejects))
