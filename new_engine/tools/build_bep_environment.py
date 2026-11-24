#!/usr/bin/env python3
"""Compose purchased BEP vegetation and furnishings in bounded authored pockets.

The seven-column-capable scenery format stays editable; this source uses six
columns and terrain-rooted placement. The original six sources are read-only.
Real runtime model bounds, the actual CPU atlas, public routes, water, farm
plots, and existing structures determine whether a candidate may be published.
"""
import argparse
from collections import Counter
import ctypes as C
import hashlib
import json
import math
from pathlib import Path
import random
import re
import tempfile

from build_starting_area import Atlas, Sample, TrailPoint, glb_bounds, segment_distance
from build_castle_streets import ROADS as STREET_ROADS, TOUR as STREET_TOUR
from build_village_farms import ROADS as FARM_ROADS, PLOTS as FARM_PLOTS, WALK as FARM_WALK, APPROACH as FARM_APPROACH
from build_village_hero_grove import source_routes

ROOT=Path(__file__).resolve().parents[1]
DIRECTORY=ROOT/'projects/valwood_world/scenery'
MODELS=ROOT/'projects/valwood_world/models'
ORIGIN=(-1310.4,-10207.58474745458)
RF_ORIGIN=(-26712.,9640.496705929329)
WIDTH=100800.;DEPTH=WIDTH*2818/5009
SOURCE_NAMES=('authored','starting_area','rainforest','castle_village','tanthoon_bandit_camp','castle_streets')
SOURCES=[DIRECTORY/(n+'_instances.txt')for n in SOURCE_NAMES]
NATURE=[f'bep_nature_{n}'for n in ['oak','beech','bush_a','bush_b','grass_long','grass_broad','fern','reed','clover','palm']]
PROPS=[f'bep_prop_{n}'for n in ['barrel','chest','bench','bucket','fence','fence_broken']]+['bep_structure_well','bep_structure_storehouse']
TREES={'bep_nature_oak','bep_nature_beech'}
SHRUBS={'bep_nature_bush_a','bep_nature_bush_b','bep_nature_palm'}
LOW=set(NATURE)-TREES-SHRUBS


def sha(path):
    h=hashlib.sha256()
    with Path(path).open('rb')as f:
        for data in iter(lambda:f.read(1024*1024),b''):h.update(data)
    return h.hexdigest()


def protected():
    return [dict(file=str(p.relative_to(ROOT)),sha256=sha(p),records=sum(bool(l.strip())and not l.lstrip().startswith('#')for l in p.read_text().splitlines()))for p in SOURCES]


class Overlay(C.Structure):
    _fields_=[(n,C.c_double)for n in ['height_delta','path','paving','clearing','grass_exclusion']]


class Author:
    def __init__(self,atlas):
        self.atlas=atlas;self.api=atlas.api;self.records=[];self.models={};self.rejects=Counter();self.pockets=[]
        self.api.world_atlas_local_water.argtypes=[C.c_double,C.c_double,C.c_void_p,C.c_void_p];self.api.world_atlas_local_water.restype=C.c_bool
        self.api.world_atlas_village_sample.argtypes=[C.c_double,C.c_double];self.api.world_atlas_village_sample.restype=Overlay
        self.api.world_atlas_rainforest_trail_points.argtypes=[C.POINTER(C.c_size_t)];self.api.world_atlas_rainforest_trail_points.restype=C.POINTER(TrailPoint)
        count=C.c_size_t();points=self.api.world_atlas_rainforest_trail_points(C.byref(count))
        self.rf_trail=[(points[i].x,points[i].z,points[i].half_width)for i in range(count.value)]
        self.aliases={m[1]:(m[2],m[3],m[4])for m in re.finditer(r'\{"([^"]+)",\s*"([^"]+)"[^\n]+Grounding::(\w+),\s*Collision::(\w+)',(ROOT/'engine/world_scenery.cpp').read_text())}
        self.aliases.update({a:(a,'Root','Trunk'if a in TREES else'None')for a in NATURE})
        self.aliases.update({a:(a,'Foundation','Box')for a in PROPS})
        village=json.loads((DIRECTORY/'castle_village_layout.json').read_text())
        self.roads=[(r['id'],r['a'],r['b'],r['half_width'])for r in village['roads']]
        self.roads += [(r[0],r[1],r[2],r[3])for r in STREET_ROADS]
        self.roads += [(r['id'],r['a'],r['b'],r['half_width'])for r in FARM_ROADS]
        self.routes=[]
        for points in [village['benchmark_route_xz'],*([village['demo_route_xz']]if'demo_route_xz'in village else[]),STREET_TOUR,FARM_WALK,FARM_APPROACH]:
            self.routes +=list(zip(points,points[1:]))
        district=DIRECTORY/'village_demo_district.json'
        if district.is_file():
            data=json.loads(district.read_text())
            if 'demo_route_xz'in data:self.routes+=list(zip(data['demo_route_xz'],data['demo_route_xz'][1:]))
        self.routes+=source_routes('world_village_population.cpp')+source_routes('world_village_pets.cpp')
        self.obstacles=[];self.old_trees=[]
        for source in SOURCES:
            for number,line in enumerate(source.read_text().splitlines(),1):
                if not line.strip()or line.lstrip().startswith('#'):continue
                p=line.split();alias=p[0];x=(float(p[1])-.5)*WIDTH;z=(float(p[2])-.5)*DEPTH
                if min(math.dist((x,z),ORIGIN),math.dist((x,z),RF_ORIGIN))>460:continue
                if alias not in self.aliases:raise ValueError(f'Unknown existing alias: {alias}')
                stem,ground,col=self.aliases[alias]
                if col=='Trunk':self.old_trees.append((x,z))
                elif col!='None':
                    m=self.model(alias)
                    self.obstacles.append(dict(alias=alias,x=x,z=z,yaw=float(p[3]),scale=float(p[4]),model=m,id=f'{source.name}:{number}'))
        # Existing natural logs and roots are also obstacles on the rainforest
        # trail margins. Flexible plants may deliberately interweave each other.

    def model(self,alias):
        if alias not in self.models:
            stem,ground,col=self.aliases[alias]
            file=next((p for p in[MODELS/(stem+'_near.glb'),MODELS/(stem+'.glb')]if p.is_file()),None)
            if file is None:raise ValueError(f'Cook the real runtime asset before authoring: {stem}')
            lo,hi=glb_bounds(file)
            lods=[]
            if alias in NATURE+PROPS:
                for lod in ['near','mid']:
                    path=MODELS/(stem+'_'+lod+'.glb');assert path.is_file(),path
                    low,high=glb_bounds(path)
                    lods.append(dict(lod=lod,file=str(path.relative_to(ROOT)),sha256=sha(path),bounds_min=low,bounds_max=high))
                lo=[min(row['bounds_min'][i]for row in lods)for i in range(3)]
                hi=[max(row['bounds_max'][i]for row in lods)for i in range(3)]
            self.models[alias]=dict(file=str(file.relative_to(ROOT)),sha256=sha(file),bounds_min=lo,bounds_max=hi,grounding=ground,collision=col,lods=lods)
        return self.models[alias]

    def reject(self,reason):self.rejects[reason]+=1;return None

    def footprint_radius(self,alias,scale):
        m=self.model(alias);lo,hi=m['bounds_min'],m['bounds_max']
        return math.hypot(max(abs(lo[0]),abs(hi[0])),max(abs(lo[2]),abs(hi[2])))*scale

    def obstacle_gap(self,x,z,radius,obstacle):
        if math.dist((x,z),(obstacle['x'],obstacle['z']))>radius+60:return math.inf
        t=math.radians(obstacle['yaw']);c,s=math.cos(t),math.sin(t);dx=x-obstacle['x'];dz=z-obstacle['z']
        xx=c*dx-s*dz;zz=s*dx+c*dz;m=obstacle['model'];lo,hi=m['bounds_min'],m['bounds_max'];scale=obstacle['scale']
        return math.hypot(max(lo[0]*scale-xx,0,xx-hi[0]*scale),max(lo[2]*scale-zz,0,zz-hi[2]*scale))-radius

    def candidate(self,id,alias,x,z,yaw,scale,burial,zone,pocket):
        m=self.model(alias);lo,hi=m['bounds_min'],m['bounds_max'];rad=self.footprint_radius(alias,scale)
        tree=alias in TREES;prop=alias in PROPS;root_radius=.65*scale if tree else rad
        ground=self.atlas.sample(x,z)
        if ground.height<=.1 or ground.shore<.6 or ground.land<.75:return self.reject('ocean_or_shore')
        # Every rendered walking route is reserved with a plant-width guard;
        # trees may overhang but their roots/colliders remain beyond the route.
        trail_radius=rad if zone.startswith('village') else root_radius
        for trail in [self.atlas.trail,self.rf_trail]:
            for a,b in zip(trail,trail[1:]):
                if segment_distance((x,z),a,b)<max(a[2],b[2])+trail_radius+.4:return self.reject('atlas_trail')
        local=(x-ORIGIN[0],z-ORIGIN[1])
        road_gap=math.inf
        if zone.startswith('village'):
            for _,a,b,width in self.roads:
                gap=segment_distance(local,a,b)-width-root_radius-.45
                road_gap=min(road_gap,gap)
                if gap<0:return self.reject('village_road')
            for a,b in self.routes:
                if segment_distance(local,a,b)<root_radius+1:return self.reject('actor_or_walking_route')
            overlay=self.api.world_atlas_village_sample(x,z)
            if overlay.path>.04 or overlay.paving>.12:return self.reject('cached_path_or_paving')
            # Cultivated plots stay free from wild growth, including the full
            # plant source footprint and the existing crop wind allowance.
            for plot in FARM_PLOTS:
                xmin,zmin,xmax,zmax=plot['bounds_xz']
                if xmin-rad-.6<local[0]<xmax+rad+.6 and zmin-rad-.6<local[1]<zmax+rad+.6:return self.reject('cultivated_field')
        obstruction_radius=rad
        for old in self.obstacles:
            if self.obstacle_gap(x,z,obstruction_radius+.12,old)<0:return self.reject('existing_rigid_footprint')
        # Tree trunks reserve natural clustered root separation. Low plants
        # intentionally grow beneath canopies, while rigid furnishings do not.
        if tree or prop:
            for xx,zz in self.old_trees:
                if math.hypot(xx-x,zz-z)<(3.4 if tree else rad+.6):return self.reject('existing_tree_root')
        for old in self.records:
            dist=math.hypot(old['world_xz'][0]-x,old['world_xz'][1]-z)
            if old['alias']in TREES and tree and dist<5.5:return self.reject('new_tree_separation')
            if old['alias']in PROPS or prop:
                oldrad=self.footprint_radius(old['alias'],old['scale'])
                if dist<oldrad+root_radius+.15:return self.reject('new_furnishing_footprint')
            if old['alias']==alias and dist<(.26 if alias in LOW else .75):return self.reject('coincident_same_species')
        # Finite root and footprint terrain samples are the same atlas called
        # by runtime placement. Terrestrial leaves never begin below water.
        t=math.radians(yaw);c,s=math.cos(t),math.sin(t);heights=[]
        for iz in range(5):
            for ix in range(5):
                if tree:px=(-.65+1.3*ix/4)*scale;pz=(-.65+1.3*iz/4)*scale
                else:px=(lo[0]+(hi[0]-lo[0])*ix/4)*scale;pz=(lo[2]+(hi[2]-lo[2])*iz/4)*scale
                xx=x+c*px+s*pz;zz=z-s*px+c*pz;sample=self.atlas.sample(xx,zz)
                if sample.height<=.1 or sample.shore<.5 or sample.land<.75 or self.api.world_atlas_local_water(xx,zz,None,None):return self.reject('wet_full_footprint')
                heights.append(sample.height)
        relief=max(heights)-min(heights)
        if relief>(.25 if prop else .75 if tree else max(.12,hi[1]*scale*.55)):return self.reject('excess_ground_relief')
        record=dict(id=id,alias=alias,pocket=pocket,zone=zone,world_xz=[x,z],map_uv=[x/WIDTH+.5,z/DEPTH+.5],yaw_degrees=(yaw+180)%360-180,scale=scale,burial_m=burial,ground_y_m=ground.height-burial,footprint_relief_m=relief,road_clearance_m=None if math.isinf(road_gap)else road_gap)
        self.records.append(record);return record

    def pocket(self,id,center,radii,count,species,zone,angle=0,scales=(.72,1.15),burial=.008):
        rng=random.Random('valwood-bep-environment-v1:'+id)
        c,s=math.cos(math.radians(angle)),math.sin(math.radians(angle));accepted=[]
        for index in range(count):
            alias=species[index%len(species)]
            for trial in range(900):
                # Two offset lobes make compact crescents and gaps instead of
                # evenly seeded circles or repeated ornamental ring patterns.
                a=rng.random()*math.tau;q=math.sqrt(rng.random());lobe=-.35 if index%3 else .45
                px=(math.cos(a)*q*.62+lobe)*radii[0];pz=math.sin(a)*q*radii[1]
                x=center[0]+c*px+s*pz;z=center[1]-s*px+c*pz
                scale=rng.uniform(*scales)
                if alias=='bep_nature_palm':scale*=rng.uniform(.8,1.05)
                record=self.candidate(f'{id}_{index:03}',alias,x,z,rng.uniform(-180,180),scale,burial,zone,id)
                if record is not None:accepted.append(record['id']);break
            else:raise ValueError(f'No safe placement for {id}/{alias}/{index}; bounded pocket exhausted')
        self.pockets.append(dict(id=id,zone=zone,center_world_xz=list(center),radii_m=list(radii),angle_degrees=angle,instances=count,species=species,labels=accepted))


def compose(a):
    furnish(a)
    def v(p):return(ORIGIN[0]+p[0],ORIGIN[1]+p[1])
    def rf(p):return(RF_ORIGIN[0]+p[0],RF_ORIGIN[1]-p[1])
    # Public town streets retain their original paving and architecture. The
    # enclosed gardens are cropped low; outside the wall, growth is taller.
    village_beds=[('tavern_west_green',(-185,-83),(6,13),20),('tavern_back_herbs',(-147,-121),(7,6),14),
      ('brewer_back_verge',(-187,-148),(6,11),18),('residents_west_green',(-185,10),(6,16),20),
      ('south_residents_verge',(-185,111),(7,10),18),('east_copse_floor',(185,104),(9,14),20),
      ('east_north_green',(186,-92),(8,14),20),('south_west_garden',(-154,168),(19,11),22),
      ('south_east_green',(153,171),(18,10),22),('northeast_wall_garden',(158,-113),(15,7),22)]
    for name,center,radii,count in village_beds:
        a.pocket(name,v(center),radii,count,['bep_nature_clover','bep_nature_grass_broad','bep_nature_clover','bep_nature_fern'],'village_garden',scales=(.65,.95))
    # Eleven separated creek-bank groups follow the OUTER side of the stream.
    # The middle gate and drawbridge view stay completely open.
    bank_beds=[('creek_northwest',(-147,-226.5),(14,4)),('creek_north',(-63,-226.5),(18,4)),('creek_northeast',(138,-226.5),(15,4)),
      ('creek_west_upper',(-226.5,-139),(4,19)),('creek_west_mid',(-226.5,-49),(4,17)),('creek_west_lower',(-226.5,143),(4,17)),
      ('creek_east_upper',(226.5,-145),(4,18)),('creek_east_mid',(226.5,-48),(4,17)),('creek_east_lower',(226.5,142),(4,18)),
      ('creek_southwest',(-118,226.5),(20,4)),('creek_southeast',(117,226.5),(20,4))]
    for name,center,radii in bank_beds:
        a.pocket(name,v(center),radii,22,['bep_nature_grass_long','bep_nature_reed','bep_nature_grass_broad','bep_nature_clover'],'village_outer_bank',scales=(.78,1.15))
    shrub_beds=[('tavern_hedge',(-186,-82),(6,16),8),('residents_hedge',(-186,7),(6,16),8),
      ('south_green_hedge',(-165,169),(17,9),9),('east_green_hedge',(183,109),(10,15),9),
      ('north_green_hedge',(180,-100),(12,13),9),('creek_west_hedge',(-236,-123),(5,17),8),
      ('creek_east_hedge',(236,-122),(5,17),8),('creek_south_hedge',(144,238),(17,5),8)]
    for name,center,radii,count in shrub_beds:
        a.pocket(name,v(center),radii,count,['bep_nature_bush_a','bep_nature_bush_b'],'village_shrub',scales=(.68,1.04),burial=.015)
    # Large canopies are outside the wall, behind garden foliage. They frame
    # the skyline without filling the castle courtyard or hiding shop fronts.
    grove_beds=[('northwest_oak_grove',(-150,-244),(30,11),5),('northeast_beech_grove',(131,-244),(31,12),5),
      ('west_bank_grove',(-244,-97),(12,30),5),('east_bank_grove',(244,-85),(12,32),5),
      ('southwest_bank_grove',(-146,244),(31,12),4),('southeast_bank_grove',(145,244),(32,12),4)]
    for name,center,radii,count in grove_beds:
        a.pocket(name,v(center),radii,count,['bep_nature_oak','bep_nature_beech'],'village_outer_canopy',scales=(.72,1.03),burial=.025)
    # In the rainforest, broad leaves and fern layers replace the visual
    # repetition of grass while roots keep every bend and pool approach clear.
    rf_beds=[('rainforest_arrival_west',(-26,-85),(6,11)),('rainforest_arrival_east',(0,-76),(7,10)),
      ('rainforest_hollow_west',(-15,-48),(7,11)),('rainforest_hollow_east',(11,-43),(7,11)),
      ('rainforest_cathedral_west',(-17,-9),(7,10)),('rainforest_cathedral_east',(13,1),(7,11)),
      ('rainforest_north_west',(-16,76),(7,12)),('rainforest_north_east',(13,89),(7,12)),
      ('rainforest_pool_farbank',(-40,25),(4,15)),('rainforest_pool_northbank',(-23,50),(12,4))]
    for name,center,radii in rf_beds:
        a.pocket(name,rf(center),radii,20,['bep_nature_fern','bep_nature_grass_broad','bep_nature_clover','bep_nature_fern'],'rainforest_understory',scales=(.78,1.19))
    for name,center,radii in [('rainforest_hollow_scrub',(-20,-53),(10,13)),('rainforest_canopy_scrub',(22,3),(9,15)),('rainforest_far_scrub',(-23,85),(10,13))]:
        a.pocket(name,rf(center),radii,12,['bep_nature_bush_a','bep_nature_bush_b','bep_nature_palm'],'rainforest_scrub',scales=(.75,1.12),burial=.015)
    for name,center in [('rainforest_south_crown',(-52,-70)),('rainforest_north_crown',(52,72))]:
        a.pocket(name,rf(center),(15,19),4,['bep_nature_oak','bep_nature_beech'],'rainforest_canopy',scales=(.86,1.14),burial=.025)


def furnish(a):
    # A bounded western garden outbuilding receives practical furnishings.
    # All asset families are represented without multiplying market clutter.
    def v(p):return(ORIGIN[0]+p[0],ORIGIN[1]+p[1])
    specifications=[('garden_storehouse','bep_structure_storehouse',-181,171,0,1,1.06),
      ('garden_well','bep_structure_well',-161,177,15,1,.76),
      ('garden_well_bucket','bep_prop_bucket',-157.8,177.3,12,.85,.015),
      ('garden_rest_bench','bep_prop_bench',-169,178,0,1,.02),
      ('garden_delivery_chest','bep_prop_chest',-175.5,173.5,23,.92,.015),
      ('garden_cask_a','bep_prop_barrel',-176,169,15,.95,.015),
      ('garden_cask_b','bep_prop_barrel',-175.8,167.5,-22,.85,.015),
      ('garden_boundary_a','bep_prop_fence',-180,180,0,.85,.03),
      ('garden_boundary_b','bep_prop_fence',-175.7,180,0,.85,.03),
      ('garden_boundary_broken','bep_prop_fence_broken',-184.5,180,0,.85,.03),
      ('east_garden_seat','bep_prop_bench',163,164,180,1,.02),
      ('east_garden_water','bep_prop_bucket',161.3,164.1,6,.9,.015),
      ('east_garden_boundary_a','bep_prop_fence',164,179,0,.85,.03),
      ('east_garden_boundary_b','bep_prop_fence',168.3,179,0,.85,.03),
      ('east_garden_boundary_broken','bep_prop_fence_broken',172.8,179,0,.85,.03),
      ('east_garden_cask_a','bep_prop_barrel',159,173,23,1,.015),
      ('east_garden_cask_b','bep_prop_barrel',160.6,173.5,-13,.9,.015),
      ('east_garden_chest','bep_prop_chest',158.2,169,9,.85,.015),
      ('north_verge_seat','bep_prop_bench',183,-111,90,1,.02),
      ('north_verge_bucket','bep_prop_bucket',183.2,-113.1,16,.85,.015),
      ('tavern_back_chest','bep_prop_chest',-149,-119,0,.85,.015),
      ('tavern_back_cask','bep_prop_barrel',-151.1,-120,18,.92,.015),
      ('tavern_back_bucket','bep_prop_bucket',-147.6,-120.1,-12,.85,.015),
      ('residents_garden_seat','bep_prop_bench',-187,1,90,.9,.02)]
    for index,(id,alias,x,z,yaw,scale,burial)in enumerate(specifications):
        # Props are authored exact coordinates, never silently jittered. If
        # measured source bounds do not fit, the author must adjust the plan.
        assert a.candidate(id,alias,*v((x,z)),yaw,scale,burial,'village_furnishing','western_garden'),id


def inspection_cameras(a):
    rows=[]
    for id,origin,position,target,pitch in [
      ('castle-purchased-gardens',ORIGIN,[-165,158],[-179,174],-.025),
      ('castle-creek-garden',ORIGIN,[-232,-117],[-218,-148],-.035),
      ('rainforest-purchased-understory',RF_ORIGIN,[-1,70],[-14,43],-.04)]:
        x,z=origin[0]+position[0],origin[1]+position[1]
        tx,tz=origin[0]+target[0],origin[1]+target[1]
        rows.append(dict(id=id,world_xz=[x,z],eye_y_m=a.atlas.sample(x,z).height+1.75,
          target_world_xz=[tx,tz],yaw_radians=math.atan2(tx-x,-(tz-z)),pitch_radians=pitch))
    return rows


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--force',action='store_true');parser.add_argument('--validate-only',action='store_true');args=parser.parse_args()
    output=DIRECTORY/'bep_environment_instances.txt';manifest=DIRECTORY/'bep_environment_manifest.json'
    if not args.validate_only and (output.exists()or manifest.exists())and not args.force:parser.error('Outputs exist; use --validate-only or explicitly --force')
    baseline=protected()
    with tempfile.TemporaryDirectory(prefix='valwood-bep-layout-')as directory:
        a=Author(Atlas(directory))
        if args.validate_only:
            for number,line in enumerate(output.read_text().splitlines(),1):
                text,*comment=line.split('#',1);p=text.split()
                if not p:continue
                assert len(p)==6
                alias=p[0];u,v,yaw,scale,burial=map(float,p[1:]);assert all(math.isfinite(value)for value in[u,v,yaw,scale,burial]);assert alias in NATURE+PROPS
                id=comment[0].strip()if comment else f'line_{number}'
                zone='rainforest_validate'if math.dist(((u-.5)*WIDTH,(v-.5)*DEPTH),RF_ORIGIN)<450 else'village_validate'
                assert a.candidate(id,alias,(u-.5)*WIDTH,(v-.5)*DEPTH,yaw,scale,burial,zone,'validated'),(number,id,a.rejects)
            data=json.loads(manifest.read_text());assert data['source_sha256']==sha(output);assert data['records']==len(a.records)
            assert data['original_sources']==baseline,'Original six protected sources changed since publication'
            assert data['generator_sha256']==sha(__file__),'Published evidence belongs to a different authoring recipe'
            assert data['models']=={k:v for k,v in a.models.items()if k in NATURE+PROPS},'Published asset bounds or bytes changed'
            assert data['aliases']==dict(sorted(Counter(r['alias']for r in a.records).items()))
        else:
            compose(a)
            assert set(NATURE+PROPS)=={r['alias']for r in a.records},'All selected purchased families must be visible'
            assert len(a.records)<=1300 and sum(r['alias']in TREES for r in a.records)<=44
            lines=['# BEP environment composition v1 — source-authentic purchased meshes',
              '# alias map_u map_v yaw_degrees scale burial_meters',
              '# Original six scenery files and all public routes remain untouched.',
              '# Creek plants are OUTSIDE the outer town wall; no planted water roots.',
              '# Full bounds checked against structures, routes, terrain and local water.']
            zone=None
            for r in a.records:
                if r['pocket']!=zone:zone=r['pocket'];lines+=['',f'# Pocket: {zone}']
                u,v=r['map_uv'];line=f"{r['alias']} {u:.10f} {v:.10f} {r['yaw_degrees']:.5f} {r['scale']:.6f} {r['burial_m']:.3f} # {r['id']}";assert len(line.encode())<=256;lines.append(line)
            output.write_text('\n'.join(lines)+'\n')
            assert output.stat().st_size<=256*1024
            data=dict(schema_version=1,title='Purchased BEP environment | Valdoa gardens, outer creek and Old Tanthoon understory',generator='tools/build_bep_environment.py',generator_sha256=sha(__file__),seed='valwood-bep-environment-v1',source_file=str(output.relative_to(ROOT)),source_sha256=sha(output),records=len(a.records),aliases=dict(sorted(Counter(r['alias']for r in a.records).items())),zones=dict(Counter(r['zone']for r in a.records)),original_sources=baseline,existing_records_preserved=sum(p['records']for p in baseline),total_records=sum(p['records']for p in baseline)+len(a.records),user_supplied_archives=['/home/computer/Downloads/Nature.zip','/home/computer/Downloads/Buildings&PropsTextures.zip','/home/computer/Downloads/Building+Elements.zip','/home/computer/Downloads/Props.zip'],placement_checks=dict(actual_runtime_bounds=True,full_footprint_water_grid=[5,5],original_routes_reserved=True,cultivated_fields_reserved=True,existing_rigid_obb_test=True,tree_crowns_clear_buildings=True,minimum_new_tree_root_separation_m=5.5,rejected_candidates=dict(a.rejects),maximum_footprint_ground_relief_m=max(r['footprint_relief_m']for r in a.records)),models={k:v for k,v in a.models.items()if k in NATURE+PROPS},pockets=a.pockets,inspection_cameras=inspection_cameras(a),placements=a.records)
            assert data['total_records']<=8192;manifest.write_text(json.dumps(data,indent=2)+'\n')
        assert protected()==baseline,'Original sources changed during layout build'
        print(json.dumps(dict(records=len(a.records),aliases=dict(Counter(r['alias']for r in a.records)),checks='passed',protected_sources=len(baseline),rejected=dict(a.rejects)),indent=2))

if __name__=='__main__':main()
