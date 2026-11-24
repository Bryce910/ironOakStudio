#!/usr/bin/env python3
"""Measured existing-asset details around the re-authored jungle pool saddle."""
import argparse
from collections import Counter
import json
import math
from pathlib import Path
import random
import tempfile
from build_demo_finish import Author, ORIGIN, WIDTH, DEPTH, sha, sat_gap
from build_starting_area import Atlas, segment_distance
from build_demo_polish import APPROACH
from refine_demo_composition import instance, TRAIL
ROOT=Path(__file__).resolve().parents[1]
SCENE=ROOT/'projects/valwood_world/scenery'
OUT=ROOT/'docs/demo-completion/2026-10-29'
CX,CZ=-26712.,9640.496705929329


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--publish',action='store_true');args=parser.parse_args()
    previous=sorted(p for p in SCENE.glob('*_instances.txt') if p.name!='demo_completion_instances.txt')
    protected={p.name:sha(p) for p in previous};rng=random.Random(0x5add1e);rejects=Counter();accepted=[]
    with tempfile.TemporaryDirectory(prefix='valwood-saddle-author-') as tmp:
        a=Author(Atlas(tmp));old=[]
        for path in previous:
            for number,line in enumerate(path.read_text().splitlines(),1):
                t=line.split('#',1)[0].split()
                if not t:continue
                x,z=(float(t[1])-.5)*WIDTH,(float(t[2])-.5)*DEPTH
                if abs(x-CX)>100 or abs(z-CZ)>180:continue
                old.append(dict(id=f'{path.name}:{number}',alias=t[0],world_xz=[x,z],local_xz=[x-ORIGIN[0],z-ORIGIN[1]],yaw_degrees=float(t[3]),scale=float(t[4])))
        def admit(id,alias,lx,lz,scale,yaw,burial):
            x,z=CX+lx,CZ+lz;m=a.model(alias);lo,hi=m['bounds_min'],m['bounds_max']
            radius=math.hypot(max(abs(lo[0]),abs(hi[0])),max(abs(lo[2]),abs(hi[2])))*scale
            gap=min(segment_distance((x,z),p,q) for p,q in zip(TRAIL,TRAIL[1:]))-radius-2.2
            gap=min(gap,min(segment_distance((x,z),p,q) for p,q in zip(APPROACH,APPROACH[1:]))-radius-1.1)
            if gap<.25:rejects['walking_envelope']+=1;return False
            if any(math.hypot(lx-px,lz-pz)<radius+2.5 for px,pz in [(-68,-28),(-74,-32)]):rejects['animal_clearance']+=1;return False
            if -11-radius<lx<35+radius and 12-radius<lz<48+radius:rejects['camp_clearance']+=1;return False
            r=dict(id=id,alias=alias,world_xz=[x,z],local_xz=[x-ORIGIN[0],z-ORIGIN[1]],yaw_degrees=(yaw+180)%360-180,scale=scale,burial_m=burial,elevation_m=0.,zone='jungle_saddle_vignettes',parent=None)
            for other in old+accepted:
                if math.dist((x,z),other['world_xz'])>25:continue
                om=a.model(other['alias'])
                if om['collision']=='None':continue
                # Reserve the actual trunk base; crowns can frame understory.
                if om['collision']=='Trunk':
                    if math.dist((x,z),other['world_xz'])<radius+other['scale']*.65+.20:rejects['trunk_base']+=1;return False
                elif sat_gap(r,m,other,om)<.10:rejects['rigid_footprint']+=1;return False
            for other in accepted:
                if other['alias']==alias and math.dist((x,z),other['world_xz'])<.55:rejects['species_spacing']+=1;return False
            heights=[];c,s=math.cos(math.radians(yaw)),math.sin(math.radians(yaw))
            for iz in range(5):
                for ix in range(5):
                    px=(lo[0]+(hi[0]-lo[0])*ix/4)*scale;pz=(lo[2]+(hi[2]-lo[2])*iz/4)*scale
                    wx,wz=x+c*px+s*pz,z-s*px+c*pz;sample=a.atlas.sample(wx,wz)
                    if sample.land<.75 or sample.height<.1 or a.atlas.api.world_atlas_local_water(wx,wz,None,None):rejects['wet_support']+=1;return False
                    heights.append(sample.height)
            relief=max(heights)-min(heights)
            if relief>(.28 if m['collision']!='None' else .40):rejects['support_relief']+=1;return False
            r.update(ground_height_min_m=min(heights),ground_height_max_m=max(heights),full_footprint_grid=[5,5],minimum_road_obb_gap_m=gap,minimum_actor_route_obb_gap_m=gap,minimum_other_source_obb_gap_m=None)
            accepted.append(r);return True
        # Low offset foreground anchors, with leaves growing in the protected
        # spaces between them. Existing sources and source identities stay intact.
        pockets=[(-44,-58),(-35,5),(-26,-55),(-17,75),(18,-48),(28,-91)]
        for i,(x,z) in enumerate(pockets):
            species=['deadwood' if i<2 else 'showcase_roots','rock0' if i%2 else 'rock1','bep_nature_fern','bep_nature_fern','showcase_sorrel','bep_nature_clover']
            for j,alias in enumerate(species):
                for trial in range(1200):
                    angle=rng.random()*math.tau;radius=math.sqrt(rng.random())*8.0
                    scale=rng.uniform(.36,.54) if j<2 else rng.uniform(.66,.94)
                    if admit(f'completion_saddle_{i}_{j}',alias,x+math.cos(angle)*radius,z+math.sin(angle)*radius,scale,rng.uniform(-180,180),.04 if j<2 else .008):break
                else:rejects['optional_vignette_admission']+=1
        assert 24<=len(accepted)<=36
        assert sum(r['alias']=='deadwood' for r in accepted)>=1
        text='# Authored jungle saddle: measured old-asset vignettes; prior nine sources preserved.\n'+'\n'.join(instance(r) for r in accepted)+'\n'
        target=OUT/'saddle-stage';target.mkdir(exist_ok=True);(target/'demo_completion_instances.txt').write_text(text)
        report=dict(schema=1,generator='tools/author_demo_saddle.py',generator_sha256=sha(__file__),records=len(accepted),placements=accepted,models=a.models,protected_sources=protected,rejections=dict(rejects),new_texture_images=0,new_model_files=0,maximum_added_instances=36,staged_sha256=sha(target/'demo_completion_instances.txt'),runtime_published=args.publish)
        (target/'manifest.json').write_text(json.dumps(report,indent=2)+'\n')
        if args.publish:
            (SCENE/'demo_completion_instances.txt').write_text(text);(SCENE/'demo_completion_manifest.json').write_text(json.dumps(report,indent=2)+'\n')
        assert all(sha(SCENE/name)==digest for name,digest in protected.items())
        print(json.dumps({k:report[k] for k in ('records','new_texture_images','new_model_files','runtime_published','rejections')},indent=2))


if __name__=='__main__':main()
