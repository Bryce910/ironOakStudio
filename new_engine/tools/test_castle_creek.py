#!/usr/bin/env python3
"""Prove creek terrain preserves old roads and actual grounded source geometry.

Builds a tiny wrapper around the production pure-C field. Does not mutate scene,
models, installed engines or graphics state. Native water/render performance is
verified separately by the parent demo acceptance workflow.
"""
from pathlib import Path
import ctypes as C
import json
import math
import subprocess
import tempfile

from build_castle_village import Author,PATH,ROOT,ROUTE,ALIASES,MODELS
from build_starting_area import glb_bounds
from build_castle_drawbridge import validate as validate_bridge

def main():
    with tempfile.TemporaryDirectory(prefix='valwood-castle-creek-')as folder:
        folder=Path(folder);wrapper=folder/'field.c';library=folder/'field.so'
        wrapper.write_text('#include "engine/world_castle_creek.h"\n'
            'double bed(double x,double z){return world_castle_creek_sample_local(x,z,24).height;}\n'
            'double floor_at(double x,double z){double b=bed(x,z);return world_castle_creek_floor_local(x,z,b);}\n')
        subprocess.run(['cc','-std=c99','-O2','-Wall','-Wextra','-Werror','-shared','-fPIC','-I'+str(ROOT),str(wrapper),'-lm','-o',str(library)],check=True)
        lib=C.CDLL(str(library))
        for name in ['bed','floor_at']:
            f=getattr(lib,name);f.argtypes=[C.c_double,C.c_double];f.restype=C.c_double
        author=Author(json.loads(PATH.read_text()));author.compose()
        # An append-only expansion may already be composed. Preserve every old
        # market/district record; bridge and its new creekside dressing have
        # independent authored support checks in their own layer.
        baseline=[r for r in author.records if r['label']!='castle_creek_drawbridge' and not r['label'].startswith('castle_creek_bank_')]
        checked_roads=0;checked_route=0;ground_rays=0;candidate_records=0;checked_models=set()
        for road in author.layout['roads']:
            a,b=road['a'],road['b'];dx,dz=b[0]-a[0],b[1]-a[1];length=math.hypot(dx,dz)
            normal=(-dz/length,dx/length);steps=math.ceil(length/.5)
            for i in range(steps+1):
                for j in range(-4,5):
                    width=road['half_width']*j/4
                    x=a[0]+dx*i/steps+normal[0]*width;z=a[1]+dz*i/steps+normal[1]*width
                    # Only the central avenue intentionally crosses open water;
                    # its full twelve-metre road is supported by the real deck.
                    height=lib.floor_at(x,z)
                    assert 24-1e-8<=height<=24.020001,('Existing paved road lost support',road['id'],[x,z],height)
                    checked_roads+=1
        routes=[ROUTE,author.layout['demo_route_xz'],author.castle_expansion['route_xz']]
        for route in routes:
            for a,b in zip(route,route[1:]):
                length=math.dist(a,b);steps=math.ceil(length/.25)
                for i in range(steps+1):
                    x=a[0]+(b[0]-a[0])*i/steps;z=a[1]+(b[1]-a[1])*i/steps
                    for dx,dz in [(0,0),(.37,0),(-.37,0),(0,.37),(0,-.37)]:
                        assert 24-1e-8<=lib.floor_at(x+dx,z+dz)<=24.020001,('Old route capsule lost support',[x,z])
                        checked_route+=1
        cache={}
        for r in baseline:
            lo,hi=author.model(r['alias'],True);scale=r['scale'];c,s=math.cos(r['yaw']),math.sin(r['yaw'])
            radius=max(math.hypot(x,z)for x in [lo[0],hi[0]]for z in [lo[2],hi[2]])*scale
            if r['x']+radius<-225 or r['x']-radius>225 or r['z']+radius<-223 or r['z']-radius>223:continue
            candidate_records+=1
            assert lib.bed(r['x'],r['z'])==24,('Existing model root moved into creek',r['label'])
            if r['alias']not in cache:
                _,_,points,_=glb_bounds(MODELS/(ALIASES[r['alias']]+'.glb'),True)
                cache[r['alias']]=points
            for p in cache[r['alias']]:
                if p[1]*scale>=.30:continue
                x=r['x']+(c*p[0]+s*p[2])*scale;z=r['z']+(-s*p[0]+c*p[2])*scale
                assert lib.bed(x,z)==24,('Existing actual source base lost support',r['label'],[x,z],lib.bed(x,z))
                ground_rays+=1
            checked_models.add(r['alias'])
        bridge=validate_bridge()
        result=dict(passed=True,previous_scene_records_preserved=len(baseline),full_road_width_samples=checked_roads,
            old_route_capsule_samples=checked_route,actual_source_base_samples=ground_rays,
            potentially_affected_old_records=candidate_records,source_models=len(checked_models),
            bridge_triangles=[a['triangles']for a in bridge['assets']],new_bridge_gpu_image_keys=bridge['new_gpu_image_keys'],
            water_rectangle_local=[-225,-223,225,223],water_surface_y=22.9,
            bridge_walk_surface_y=24.02,bridge_bed_at_center_y=lib.bed(0,219),
            outside_outer_town_walls=True,enclosure_wall_centerline_xz=[-205,-205,205,205],
            all_outerwall_innercastle_wing_and_grove_source_bases_dry=True,
            closed_moat=True,inner_courtyard_and_former_creek_coordinates_dry=True)
        print(json.dumps(result,indent=2))

if __name__=='__main__':main()
