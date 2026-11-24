#!/usr/bin/env python3
"""Small, measured preview dressing; source placements and terrain are preserved."""
import argparse,ctypes as C,json,math,tempfile
from pathlib import Path
from build_demo_finish import Author,ORIGIN,WIDTH,DEPTH,sat_gap
from build_starting_area import Atlas,segment_distance
from refine_demo_composition import instance,TRAIL,APPROACH

ROOT=Path(__file__).resolve().parents[1]
def source_records():
    rows=[]
    names=('authored','starting_area','rainforest','castle_village','tanthoon_bandit_camp','castle_streets','bep_environment','demo_finish','demo_polish','demo_completion')
    for name in names:
        for number,line in enumerate((ROOT/'projects/valwood_world/scenery'/(name+'_instances.txt')).read_text().splitlines(),1):
            t=line.split('#',1)[0].split()
            if not t:continue
            x,z=(float(t[1])-.5)*WIDTH,(float(t[2])-.5)*DEPTH
            rows.append(dict(id=f'{name}_instances.txt:{number}',alias=t[0],world_xz=[x,z],local_xz=[x-ORIGIN[0],z-ORIGIN[1]],yaw_degrees=float(t[3]),scale=float(t[4]),burial_m=float(t[5]),elevation_m=float(t[6])if len(t)>6 else 0))
    return rows
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--project',type=Path,required=True);args=p.parse_args()
    project=args.project.resolve();assert project!=ROOT/'projects/valwood_world'
    old=source_records()
    with tempfile.TemporaryDirectory(prefix='valwood-preview-art-')as temp:
        a=Author(Atlas(temp))
        known={r['id'] for r in a.previous}
        a.previous.extend(r for r in old if r['id']not in known and abs(r['local_xz'][0])<245 and abs(r['local_xz'][1])<245)
        tavern=next(r for r in old if r['alias']=='village_tavern')
        # A served meal, a lamp and timber by the hearth. Source triangles
        # determine every tabletop placement; unsupported footprints reject.
        for i,(x,z)in enumerate([(-5.6,3.4),(-4.4,3.4),(4.4,3.4),(5.6,3.4),(6.7,-1)]):
            a.supported(f'preview_tavern_lamp_{i}','purchased_prop_lamp',tavern,x,z,27*i,.38,zone='copper_lantern',support_interval=(.80,.82))
            a.supported(f'preview_tavern_meal_{i}','purchased_goods_chicken',tavern,x+.15,z-.24,37*i,.36,zone='copper_lantern',support_interval=(.80,.82))
        # Small work clusters outside frontages; full rotated footprint checks
        # reserve road width, all resident paths and every prior solid object.
        for i,parent in enumerate(r for r in old if r['alias']in('purchased_house_a','purchased_house_b','purchased_house_e')and -85<r['local_xz'][0]<-52 and -20<r['local_xz'][1]<36):
            model=a.model(parent['alias']);front=model['bounds_max'][2]*parent['scale'];side=(model['bounds_max'][0]-model['bounds_min'][0])*parent['scale']*.38
            for j,(alias,dx,dz,scale)in enumerate([('purchased_prop_sack',-side,.8,.48),('bep_prop_bucket',side,.7,.53),('purchased_prop_rope',-side+.2,1.15,.50)]):
                x,z=a.local(parent,dx,front+dz);a.add(f'preview_trade_{i}_{j}',alias,x,z,parent['yaw_degrees']+i*19,scale,zone='working_frontages',required=False)
        forest=[];rejected=[]
        for i,(gx,gz)in enumerate([(-26740,9685),(-26739,9670),(-26736,9644),(-26714,9637),(-26666,9680),(-26662,9696),(-26744,9602),(-26702,9619)]):
            for j in range(6):
                angle=j*2.399963+ i*.77;radius=math.sqrt((j+.5)/6)*3.1
                x,z=gx+math.cos(angle)*radius,gz+math.sin(angle)*radius
                alias=('bep_nature_fern','showcase_sorrel','bep_nature_bush_b','bep_nature_clover','fern1','fern2')[j]
                model=a.model(alias);scale=.52+(j%3)*.11;lo,hi=model['bounds_min'],model['bounds_max'];foot=math.hypot(max(abs(lo[0]),abs(hi[0])),max(abs(lo[2]),abs(hi[2])))*scale
                path=min(segment_distance((x,z),p,q)for points in (TRAIL,APPROACH)for p,q in zip(points,points[1:]))
                if path<foot+2.4 or math.hypot(x+26688,z-9670.496705929329)<foot+17:
                    rejected.append([i,j,'walking_or_camp_reserve']);continue
                heights=[];safe=True
                for sx,sz in [(-1,-1),(-1,1),(1,-1),(1,1),(0,0)]:
                    wx,wz=x+sx*foot*.55,z+sz*foot*.55;s=a.atlas.sample(wx,wz)
                    if s.land<.75 or a.atlas.api.world_atlas_local_water(wx,wz,None,None):safe=False
                    heights.append(s.height)
                if not safe or max(heights)-min(heights)>.80:rejected.append([i,j,'wet_or_steep']);continue
                r=dict(id=f'preview_understory_{i}_{j}',alias=alias,world_xz=[x,z],local_xz=[x-ORIGIN[0],z-ORIGIN[1]],yaw_degrees=(i*53+j*71)%360-180,scale=scale,burial_m=.01,elevation_m=0,zone='composed_understory',parent=None)
                forest.append(r)
        records=a.records+forest
        out=project/'scenery/presentation_instances.txt'
        assert not out.is_symlink()
        out.write_text('# Presentation-only measured dressing. Existing scenery and terrain are preserved.\n'+'\n'.join(instance(r)for r in records)+'\n')
        # The existing ten-source renderer budget remains unchanged. Append to
        # a private copy of its final source; all original rows/IDs stay intact.
        completion=project/'scenery/demo_completion_instances.txt'
        original=(ROOT/'projects/valwood_world/scenery/demo_completion_instances.txt').read_text()
        count=sum(1 for p in (ROOT/'projects/valwood_world/scenery').glob('*_instances.txt')for line in p.read_text().splitlines()if line.strip()and not line.lstrip().startswith('#'))
        assert count+len(records)<=8192,'Preview scenery exceeds the existing placement budget'
        if completion.is_symlink():completion.unlink()
        completion.write_text(original.rstrip()+'\n'+out.read_text())
        report=dict(placements=records,accepted=len(records),supported=a.supports,rejected=a.rejected+rejected,models=a.models,terrain_height_modified=False,new_images=0,new_models=0)
        (project/'scenery/presentation_manifest.json').write_text(json.dumps(report,indent=2)+'\n')
        print(json.dumps(dict(accepted=len(records),by_zone={z:sum(r['zone']==z for r in records)for z in sorted({r['zone']for r in records})},rejected=len(report['rejected'])),indent=2))
if __name__=='__main__':main()
