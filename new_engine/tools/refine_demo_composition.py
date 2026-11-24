#!/usr/bin/env python3
"""Append measured trade details and shaped forest pockets to the existing demo.

Old placement lines stay byte-identical and in order, retaining source identities.
Only owned model families are used. Dry support, full OBBs, clear walking paths,
resident routes, optional-candidate rejection and bounded art budgets are recorded.
"""
import argparse
from collections import Counter
import hashlib
import json
import math
from pathlib import Path
import tempfile
from build_demo_finish import Author, ORIGIN, WIDTH, DEPTH, sha, sat_gap, path_gap
from build_demo_polish import compose as existing, APPROACH, SOURCE_NAMES
from build_starting_area import Atlas, segment_distance

ROOT=Path(__file__).resolve().parents[1]
SCENE=ROOT/'projects/valwood_world/scenery'
EVIDENCE=ROOT/'docs/demo-polish/2026-10-29'
TRAIL=[(-26727.,9735.496705929329),(-26710.,9700.496705929329),
       (-26720.,9660.496705929329),(-26707.,9615.496705929329),
       (-26717.,9575.496705929329),(-26700.,9530.496705929329)]


def instance(r):
    x,z=r['world_xz']
    return f"{r['alias']} {x/WIDTH+.5:.14f} {z/DEPTH+.5:.14f} {r['yaw_degrees']:.6f} {r['scale']:.6f} {r['burial_m']:.6f} {r['elevation_m']:.6f} # {r['id']}"


def source_records():
    rows=[]
    for name in SOURCE_NAMES:
        for number,line in enumerate((SCENE/(name+'_instances.txt')).read_text().splitlines(),1):
            t=line.split('#',1)[0].split()
            if not t:
                continue
            x=(float(t[1])-.5)*WIDTH;z=(float(t[2])-.5)*DEPTH
            rows.append(dict(id=f'{name}_instances.txt:{number}',alias=t[0],world_xz=[x,z],
                local_xz=[x-ORIGIN[0],z-ORIGIN[1]],yaw_degrees=float(t[3]),scale=float(t[4]),
                burial_m=float(t[5]),elevation_m=float(t[6]) if len(t)>6 else 0))
    return rows


def refine(a):
    base=existing(a)
    count=len(a.records)
    old=source_records()
    finish=json.loads((SCENE/'demo_finish_manifest.json').read_text())
    houses=[r for r in finish['placements'] if r['id'] in ['arcade_baker','arcade_apothecary','arcade_clothier','arcade_roper']]
    # Physical trade silhouettes hang above head height. Their brackets are
    # anchored to actual facade triangles; the oversized source roof OBB is
    # intentionally not used as a fictitious ground collision for these mounts.
    tavern=next(r for r in old if r['alias']=='village_tavern')
    sign_targets=[h for h in houses if h['id'] in ('arcade_baker','arcade_apothecary')]
    sign_targets.append(dict(id='tavern',alias='village_tavern',local_xz=tavern['local_xz']))
    for h in sign_targets:
        parent=next(r for r in old if r['alias']==h['alias'] and math.dist(r['local_xz'],h['local_xz'])<.001)
        kind='baker' if 'baker' in h['id'] else 'herbalist' if 'apothecary' in h['id'] else 'tavern'
        lx=-2.9 if h['id']=='tavern' else -1.25 if kind=='baker' else 1.25
        model=a.model(parent['alias']);scale=parent['scale']
        # Probe the source facade at this mounting point; use real front depth.
        px,py=lx/scale,2.55/scale
        if parent['alias'] not in a.geometry:
            from build_starting_area import glb_bounds
            a.geometry[parent['alias']]=glb_bounds(ROOT/model['file'],True)[3]
        depths=[]
        for p,q,r in a.geometry[parent['alias']]:
            ax,ay=q[0]-p[0],q[1]-p[1];bx,by=r[0]-p[0],r[1]-p[1];den=ax*by-ay*bx
            if abs(den)<1e-10:
                continue
            dx,dy=px-p[0],py-p[1];u=(dx*by-dy*bx)/den;v=(ax*dy-ay*dx)/den
            if u>=0 and v>=0 and u+v<=1:
                depths.append(p[2]+u*(q[2]-p[2])+v*(r[2]-p[2]))
        if not depths:
            continue
        x,z=a.local(parent,lx,max(depths)*scale+.015)
        r=dict(id='finish_sign_'+h['id'],alias='demo_sign_'+kind,local_xz=[x,z],
            world_xz=[x+ORIGIN[0],z+ORIGIN[1]],yaw_degrees=parent['yaw_degrees'],
            scale=1.,burial_m=0.,elevation_m=2.55,zone='physical_trade_signs',parent=parent['id'])
        a.model(r['alias'])
        assert r['elevation_m']+a.model(r['alias'])['bounds_min'][1]>2.4
        a.records.append(r)
        a.supports.append(dict(id=r['id'],parent=parent['id'],actual_source_wall_triangle_probe=True,
            parent_local_xy=[px,py],parent_front_z=max(depths),clearance_m=.015,minimum_head_clearance_m=2.4))
    # Tight, varied goods beside door approaches. Ground props still pass every
    # normal road/actor/source OBB guard, and tabletop goods need real support.
    for i,h in enumerate(houses):
        parent=next(r for r in old if r['alias']==h['alias'] and math.dist(r['local_xz'],h['local_xz'])<.001)
        model=a.model(parent['alias']);front=model['bounds_max'][2]*parent['scale']
        width=(model['bounds_max'][0]-model['bounds_min'][0])*parent['scale']
        for j,(alias,side,offset,scale) in enumerate([
                ('purchased_prop_barrel_small',-.40,.58,.65),('purchased_prop_rope',.36,.54,.65),
                ('purchased_prop_sack',-.31,1.10,.52),('bep_prop_bucket',.44,.87,.52)]):
            x,z=a.local(parent,width*side,front+offset)
            r=a.add(f'finish_trade_{i}_{j}',alias,x,z,parent['yaw_degrees']+j*29,scale,
                zone='individual_trade_possessions',required=False)
            if r and alias=='purchased_prop_barrel_small':
                a.supported(f'finish_trade_tool_{i}','village_toolkit_claw_hammer',r,0,0,33,.45,
                    zone='individual_trade_possessions')
        for j in range(4):
            x,z=a.local(parent,width*(.40+j*.025),front+.18+(j%2)*.16)
            a.add(f'finish_wall_green_{i}_{j}','bep_nature_clover',x,z,67*j+31*i,.22+j*.025,
                zone='restrained_foundation_growth',required=False)
    # Separate table groups give food/service a purpose without filling the
    # walking aisle. Goods are supported by actual table/shelf triangles.
    for i,(x,z) in enumerate([(-6.05,3.4),(-3.95,3.4),(3.95,3.4),(6.05,3.4),(5.15,-1),(7.25,-1)]):
        for j,(alias,dx,dz,scale) in enumerate([
                ('purchased_goods_bread',0,.02,.48),('purchased_goods_cheese',.28,-.18,.38),
                ('purchased_goods_tankard',-.20,.22,.48)]):
            a.supported(f'finish_tavern_service_{i}_{j}',alias,tavern,x+dx,z+dz,19*i+47*j,scale,
                zone='tavern_served_tables',support_interval=(.80,.82))
    # Forest placement admission keeps all paths and camp approaches clear.
    # Existing exact dry-ground checks continue to use the production CPU atlas.
    rejected=Counter()
    def forest(id,alias,x,z,yaw,scale,zone,burial=.01):
        model=a.model(alias);lo,hi=model['bounds_min'],model['bounds_max']
        radius=.65*scale if model['collision']=='Trunk' else math.hypot(max(abs(lo[0]),abs(hi[0])),max(abs(lo[2]),abs(hi[2])))*scale
        gap=min(segment_distance((x,z),p,q) for p,q in zip(TRAIL,TRAIL[1:]))-radius-2.2
        gap=min(gap,min(segment_distance((x,z),p,q) for p,q in zip(APPROACH,APPROACH[1:]))-radius-1.1)
        if gap<.15:
            rejected['walking_clearance']+=1
            return None
        r=dict(id=id,alias=alias,world_xz=[x,z],local_xz=[x-ORIGIN[0],z-ORIGIN[1]],
            yaw_degrees=(yaw+180)%360-180,scale=scale,burial_m=burial,elevation_m=0.,zone=zone,parent=None)
        if model['collision']!='None':
            for other in old+a.records:
                if math.dist((x,z),other['world_xz'])>50:
                    continue
                om=a.model(other['alias'])
                if om['collision']!='None' and sat_gap(r,model,other,om)<.10:
                    rejected['solid_overlap']+=1
                    return None
        heights=[];c=math.cos(math.radians(yaw));s=math.sin(math.radians(yaw))
        for j in range(3):
            for i in range(3):
                xx=(lo[0]+(hi[0]-lo[0])*i/2)*scale;zz=(lo[2]+(hi[2]-lo[2])*j/2)*scale
                if model['collision']=='Trunk':
                    xx=(i-1)*.65*scale;zz=(j-1)*.65*scale
                wx=x+c*xx+s*zz;wz=z-s*xx+c*zz
                sample=a.atlas.sample(wx,wz)
                if sample.land<.75 or sample.height<.1 or a.atlas.api.world_atlas_local_water(wx,wz,None,None):
                    rejected['wet_support']+=1
                    return None
                heights.append(sample.height)
        if model['collision']!='None' and max(heights)-min(heights)>(.55 if model['collision']=='Trunk' else .38):
            rejected['support_relief']+=1
            return None
        r.update(ground_height_min_m=min(heights),ground_height_max_m=max(heights),
            minimum_road_obb_gap_m=gap,minimum_actor_route_obb_gap_m=gap,minimum_other_source_obb_gap_m=None)
        a.records.append(r)
        return r
    # Asymmetric pools of ferns/moss around the visible bank and two path turns.
    # Quiet soil remains between pockets; this is not a density multiplier.
    pockets=[(-36,-15),(-33,-39),(-14,-44),(-6,-10),(-17,30),(12,8),(-15,72),(22,52),(36,28),(41,44)]
    for i,(dx,dz) in enumerate(pockets):
        for j in range(9):
            angle=j*2.3999632297+i*.67;radius=math.sqrt((j+.4)/9)*2.8
            alias=('bep_nature_fern','showcase_sorrel','bep_nature_bush_b','bep_nature_clover')[j%4]
            forest(f'finish_forest_pocket_{i}_{j}',alias,-26712+dx+radius*math.cos(angle),
                9640.496705929329+dz+radius*math.sin(angle),j*47+i*31,
                .43+(j%3)*.11,'composed_forest_pockets')
    # Graduated reed fringes interrupt the water's thin material edge without
    # changing the water envelope or introducing colliders across the walkway.
    for i,angle in enumerate([1.0,1.35,1.75,2.0,2.8,3.2,3.65,4.2,4.5]):
        for j in range(2):
            for radius in (1.02,1.12,1.22,1.34):
                x=-26734+10*radius*math.cos(angle+j*.07)
                z=9615.496705929329+16*radius*math.sin(angle+j*.07)
                if forest(f'finish_pool_reed_{i}_{j}','bep_nature_reed',x,z,i*37+j*71,
                          .42+j*.10,'natural_pool_reed_fringe'):
                    break
    for i,(dx,dz) in enumerate([(-46,-38),(-42,-9),(28,13),(54,39),(62,34)]):
        for j in range(6):
            angle=j*math.tau/6
            if forest(f'finish_canopy_{i}','bep_nature_beech' if i%2 else 'bep_nature_oak',
                -26712+dx+math.cos(angle)*2,9640.496705929329+dz+math.sin(angle)*2,
                i*83,.71+i*.035,'pool_canopy_framing',0):
                break
    return base,count,dict(rejected_forest_candidates=dict(rejected))


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--publish',action='store_true')
    parser.add_argument('--validate-only',action='store_true')
    args=parser.parse_args()
    stage=EVIDENCE/'composition-stage';stage.mkdir(exist_ok=True)
    original=(EVIDENCE/'before-inputs/projects/valwood_world/scenery/demo_polish_instances.txt').read_text()
    current=SCENE/'demo_polish_instances.txt'
    protected={name:sha(SCENE/(name+'_instances.txt')) for name in SOURCE_NAMES}
    with tempfile.TemporaryDirectory(prefix='valwood-art-author-') as tmp:
        a=Author(Atlas(tmp));base,count,extra=refine(a)
        old_lines=[line for line in original.splitlines() if line and not line.startswith('#')]
        assert [instance(r) for r in a.records[:count]]==old_lines,'Existing authored rows changed; review before staging'
        additions=a.records[count:]
        assert len(additions)<=150
        assert all(r['scale']>=.15 for r in additions)
        text=original+'# Demo composition pass: measured additions, existing lines retained.\n'+'\n'.join(instance(r)for r in additions)+'\n'
        target=stage/'demo_polish_instances.txt';target.write_text(text)
        report=dict(schema=1,base_rows=count,added_rows=len(additions),existing_rows_byte_preserved=True,
            zones=dict(Counter(r['zone']for r in additions)),placements=additions,
            model_revisions={name:a.models[name] for name in sorted(set(r['alias'] for r in additions))},
            support_checks=a.supports,optional_rejections=a.rejected,forest=extra,
            protected_sources=protected,recipe_sha256=sha(Path(__file__)),new_texture_images=0,
            staged_sha256=sha(target),runtime_published=args.publish)
        (stage/'composition-manifest.json').write_text(json.dumps(report,indent=2)+'\n')
        if args.validate_only:
            assert current.read_text()==text,'Runtime composition differs from validated deterministic recipe'
        if args.publish:
            assert current.read_text()==original or current.read_text()==text or sha(current)==json.loads((SCENE/'demo_composition_manifest.json').read_text())['staged_sha256'],'Concurrent authoring edit'
            current.write_text(text)
            (SCENE/'demo_composition_manifest.json').write_text(json.dumps(report,indent=2)+'\n')
            manifest=json.loads((EVIDENCE/'before-inputs/projects/valwood_world/scenery/demo_polish_manifest.json').read_text())
            manifest.update(base_generator=manifest['generator'],base_generator_sha256=manifest['generator_sha256'],
                generator='tools/refine_demo_composition.py',generator_sha256=sha(Path(__file__)),
                record_count=len(a.records),source_sha256=sha(current),placements=a.records,models=a.models,
                zones=dict(Counter(r['zone'] for r in a.records)),triangle_supported_goods=a.supports,
                new_model_files=6,new_images=0,
                composition_revision=str((SCENE/'demo_composition_manifest.json').relative_to(ROOT)))
            (SCENE/'demo_polish_manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
        assert all(sha(SCENE/(name+'_instances.txt'))==value for name,value in protected.items())
        print(json.dumps({k:report[k]for k in ('base_rows','added_rows','zones','existing_rows_byte_preserved','runtime_published')},indent=2))


if __name__=='__main__':
    main()
