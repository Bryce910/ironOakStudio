#!/usr/bin/env python3
"""Three bounded cloth lines anchored to actual opposing source facades.

apply(author) runs after demo district/dressing, appends only resident rope/flag
models, and returns independent metadata. It never changes author.overheads,
existing placements, recipe JSON, model assets, or native renderer files.
--test includes real-mesh anchors, serialized-instance roundtrip, low-cloth and
floating-anchor negative controls. All work is CPU-only.
"""
from pathlib import Path
import argparse
import collections
import copy
import hashlib
import json
import math
import sys

ROOT=Path(__file__).resolve().parents[1]
PROOF=ROOT/'docs/performance/village-demo'
ROWS=[dict(id='southgate',axis='z',station=98,height=6.6,flags=6,
           parents=['demo_home_southgate_lane_west_0','demo_home_southgate_lane_east_1']),
      dict(id='marketrow',axis='x',station=-77,height=6.6,flags=6,
           parents=['demo_home_market_row_north_0','demo_home_market_row_south_0']),
      dict(id='coopers',axis='x',station=61.5,height=6.6,flags=6,
           parents=['demo_home_coopers_return_north_7','demo_home_coopers_return_south_8'])]
ALIASES={'purchased_prop_rope','purchased_prop_flag_a','purchased_prop_flag_b'}


def anchors(author,row):
    from build_village_demo_district import configuration
    sign_by_family=configuration()['native_front_x_sign'];records={r['label']:r for r in author.records}
    result=[]
    for label in row['parents']:
        parent=records[label];assert parent['alias'].startswith('purchased_house_')
        sign=sign_by_family[parent['alias'][-1]];c,s=math.cos(parent['yaw']),math.sin(parent['yaw'])
        if row['axis']=='z':
            assert abs(s)<1e-7 and abs(abs(c)-1)<1e-7,'Facade is not aligned across this street'
            local_z=(row['station']-parent['z'])/(c*parent['scale'])
        else:
            assert abs(c)<1e-7 and abs(abs(s)-1)<1e-7,'Facade is not aligned across this street'
            local_z=(row['station']-parent['x'])/(s*parent['scale'])
        local_y=(row['height']-parent['elevation']+parent['burial'])/parent['scale']
        local_x=author.wall_surface(parent['alias'],local_y,local_z,'max' if sign>0 else 'min')
        x=parent['x']+(c*local_x+s*local_z)*parent['scale']
        z=parent['z']+(-s*local_x+c*local_z)*parent['scale']
        # Independently locate the actual source triangle that owns the anchor.
        # Broad eave/AABB extrema are never used as cable support surfaces.
        support=None
        for i,(a,b,d) in enumerate(author.triangles[parent['alias']]):
            ay,az=b[1]-a[1],b[2]-a[2];by,bz=d[1]-a[1],d[2]-a[2];det=ay*bz-az*by
            if abs(det)<1e-10:continue
            u=((local_y-a[1])*bz-(local_z-a[2])*by)/det
            v=(ay*(local_z-a[2])-az*(local_y-a[1]))/det
            if u>=-1e-5 and v>=-1e-5 and u+v<=1+1e-5:
                actual=a[0]+u*(b[0]-a[0])+v*(d[0]-a[0])
                if abs(actual-local_x)<1e-6:support=i;break
        assert support is not None,('No actual source triangle supports cloth line',label)
        result.append(dict(parent=label,alias=parent['alias'],native_side='+X' if sign>0 else '-X',
                           source_local_xyz=[local_x,local_y,local_z],world_xz=[x,z],height_m=row['height'],
                           outward_xz=[sign*c,-sign*s],source_triangle=support,
                           model_sha256=hashlib.sha256((ROOT/'projects/valwood_world/models'/
                             (__import__('build_castle_village').ALIASES[parent['alias']]+'.glb')).read_bytes()).hexdigest()))
    a,b=result;dx,dz=b['world_xz'][0]-a['world_xz'][0],b['world_xz'][1]-a['world_xz'][1]
    span=math.hypot(dx,dz);assert 6<=span<=10,('Unexpected facade span',row['id'],span)
    direction=[dx/span,dz/span]
    for anchor,sign in [(a,1),(b,-1)]:
        assert sum(anchor['outward_xz'][k]*direction[k]*sign for k in range(2))>.999,'Cloth line reaches the back of a house'
    return result,span,math.atan2(dx,dz)


def apply(author):
    if hasattr(author,'_demo_bunting_metadata'):return copy.deepcopy(author._demo_bunting_metadata)
    baseline=copy.deepcopy(author.records);resident={r['alias'] for r in baseline}
    assert ALIASES<=resident,'Bunting must reuse already-resident source props'
    assert not any(r['label'].startswith('demo_bunting_') for r in baseline)
    old_overheads=copy.deepcopy(author.overheads);rows=[];labels=[]
    rlo,rhi=author.model('purchased_prop_rope',True);source_length=rhi[2]-rlo[2]
    assert .5<source_length<1
    for source in ROWS:
        ends,span,yaw=anchors(author,source);a,b=[e['world_xz'] for e in ends]
        count=math.ceil(span/(source_length*.55));scale=span/(source_length*count)
        assert count<=32 and .4<scale<=.55
        ropes=[];flags=[]
        for i in range(count):
            t=(i+.5)/count;label=f'demo_bunting_{source["id"]}_rope_{i}'
            author.add('purchased_prop_rope',a[0]+(b[0]-a[0])*t,a[1]+(b[1]-a[1])*t,
                       yaw,scale,0,label,source['height']-(rhi[1]+rlo[1])*.5*scale)
            ropes.append(label);labels.append(label)
        for i in range(source['flags']):
            alias='purchased_prop_flag_'+('a' if i%2==0 else 'b');lo,hi=author.model(alias,True)
            scale=.92 if i%2==0 else .86;t=(i+1)/(source['flags']+1)
            label=f'demo_bunting_{source["id"]}_flag_{i}'
            author.add(alias,a[0]+(b[0]-a[0])*t,a[1]+(b[1]-a[1])*t,yaw,scale,0,label,
                       source['height']-hi[1]*scale)
            flags.append(label);labels.append(label)
        rows.append(dict(id=source['id'],parents=source['parents'],anchors=ends,span_m=span,
                         axis=source['axis'],station_m=source['station'],height_m=source['height'],
                         rope_labels=ropes,flag_labels=flags,rope_segments=count,
                         source_rope_length_m=source_length))
    assert author.records[:len(baseline)]==baseline and author.overheads==old_overheads
    author._demo_bunting_baseline=baseline;author._demo_bunting_old_overheads=old_overheads
    author._demo_bunting_metadata=dict(schema_version=1,recipe_file=str(Path(__file__).relative_to(ROOT)),
        recipe_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),rows=rows,labels=labels,
        existing_records_preserved=len(baseline),preserved_original_overhead_rows=len(old_overheads),
        preserved_dressing_labels=list(getattr(author,'demo_dressing',{}).get('labels',[])),
        new_model_families=0,new_texture_policies=0,anchor_policy='actual source triangle intersections on opposing facades')
    return copy.deepcopy(author._demo_bunting_metadata)


def validate(author):
    from build_village_demo_district import same_retained_instance
    meta=author._demo_bunting_metadata;records={r['label']:r for r in author.records}
    assert len(records)==len(author.records)
    assert len(meta['rows'])==3 and len(meta['labels'])==len(set(meta['labels']))
    assert author.overheads==author._demo_bunting_old_overheads and len(author.overheads)==3,'Original lane overhead rows changed'
    assert len(author.records)>=len(author._demo_bunting_baseline)+len(meta['labels'])
    assert all(same_retained_instance(a,b) for a,b in zip(author.records,author._demo_bunting_baseline)),'Existing district/dressing record changed'
    assert all(label in records for label in meta['preserved_dressing_labels'])
    minimum_cloth=math.inf;anchor_checks=0;rope_checks=0;flag_checks=0
    for source,row in zip(ROWS,meta['rows']):
        expected,span,yaw=anchors(author,source)
        assert abs(span-row['span_m'])<2e-6 and 6<=span<=10
        for actual,reference in zip(row['anchors'],expected):
            assert actual['parent']==reference['parent'] and actual['native_side']==reference['native_side']
            assert actual['model_sha256']==reference['model_sha256']
            assert max(abs(a-b) for a,b in zip(actual['source_local_xyz'],reference['source_local_xyz']))<2e-6,'Facade anchor mismatch'
            assert math.dist(actual['world_xz'],reference['world_xz'])<2e-6,'Facade anchor mismatch'
            anchor_checks+=1
        a,b=[e['world_xz'] for e in expected]
        count=len(row['rope_labels']);assert 1<=count<=32 and count==row['rope_segments']
        previous_end=0
        for i,label in enumerate(row['rope_labels']):
            r=records[label];assert r['alias']=='purchased_prop_rope'
            lo,hi=author.model(r['alias'],True);t=(i+.5)/count
            assert math.dist([r['x'],r['z']],[a[0]+(b[0]-a[0])*t,a[1]+(b[1]-a[1])*t])<2e-6
            assert abs(math.remainder(r['yaw']-yaw,math.tau))<1e-7
            assert abs((hi[2]-lo[2])*r['scale']-span/count)<2e-6
            start=i*span/count;end=(i+1)*span/count
            assert abs(start-previous_end)<1e-7;previous_end=end
            assert abs(r['elevation']+(hi[1]+lo[1])*.5*r['scale']-row['height_m'])<2e-6
            assert r['elevation']+lo[1]*r['scale']>6.5
            rope_checks+=1
        assert abs(previous_end-span)<1e-7
        assert len(row['flag_labels'])==6
        for i,label in enumerate(row['flag_labels']):
            r=records[label];assert r['alias'] in ALIASES-{'purchased_prop_rope'}
            lo,hi=author.model(r['alias'],True);bottom=r['elevation']+lo[1]*r['scale']
            assert bottom>4.8,('Low hanging cloth',label,bottom)
            minimum_cloth=min(minimum_cloth,bottom)
            assert abs(r['elevation']+hi[1]*r['scale']-row['height_m'])<2e-6,'Cloth top is not on the rope'
            t=(i+1)/7
            assert math.dist([r['x'],r['z']],[a[0]+(b[0]-a[0])*t,a[1]+(b[1]-a[1])*t])<2e-6
            assert abs(math.remainder(r['yaw']-yaw,math.tau))<1e-7
            flag_checks+=1
    return dict(rows=3,instances=len(meta['labels']),actual_source_triangle_anchors=anchor_checks,
                rope_interval_checks=rope_checks,flags=flag_checks,lowest_cloth_above_ground_m=minimum_cloth,
                maximum_facade_span_m=max(r['span_m'] for r in meta['rows']),original_overhead_rows_preserved=3,
                dressing_labels_preserved=len(meta['preserved_dressing_labels']),
                new_model_families=0,new_texture_policies=0,
                aliases=dict(sorted(collections.Counter(records[label]['alias'] for label in meta['labels']).items())))


def test(author):
    from build_castle_village import WIDTH,DEPTH
    result=validate(author);meta=apply(author);before=copy.deepcopy(author.records)
    assert apply(author)==meta and author.records==before
    def clone():
        a=copy.copy(author);a.records=copy.deepcopy(author.records);a._demo_bunting_metadata=copy.deepcopy(meta)
        return a
    rounded=clone()
    for r in rounded.records:
        r['x']=(float(format(.487+r['x']/WIDTH,'.12g'))-.487)*WIDTH
        r['z']=(float(format(.320+r['z']/DEPTH,'.12g'))-.320)*DEPTH
        r['yaw']=math.radians(float(format(math.degrees(r['yaw']),'.12g')))
        for field in ['scale','burial','elevation']:r[field]=float(format(r[field],'.12g'))
    validate(rounded)
    low=clone();next(r for r in low.records if r['label']==meta['rows'][0]['flag_labels'][0])['elevation']-=2
    try:validate(low)
    except AssertionError as e:assert 'Low hanging cloth' in str(e)
    else:raise AssertionError('Low cloth negative control passed')
    floating=clone();floating._demo_bunting_metadata['rows'][0]['anchors'][0]['world_xz'][0]+=.1
    try:validate(floating)
    except AssertionError as e:assert 'Facade anchor mismatch' in str(e)
    else:raise AssertionError('Floating anchor negative control passed')
    result.update(serialized_instance_roundtrip=True,low_cloth_negative_control=True,floating_anchor_negative_control=True,idempotent=True)
    return result


def main():
    parser=argparse.ArgumentParser();parser.add_argument('--stage',action='store_true');parser.add_argument('--test',action='store_true')
    args=parser.parse_args();assert args.stage or args.test
    from build_castle_village import Author,PATH
    author=Author(json.loads(PATH.read_text()));author.compose();metadata=apply(author)
    proof=test(author) if args.test else validate(author)
    if args.stage:
        PROOF.mkdir(parents=True,exist_ok=True)
        (PROOF/'bunting-stage.json').write_text(json.dumps(dict(metadata=metadata,validation=proof,
             placements=[r for r in author.records if r['label'] in set(metadata['labels'])]),indent=2)+'\n')
    print(json.dumps(proof,indent=2))


if __name__=='__main__':main()
