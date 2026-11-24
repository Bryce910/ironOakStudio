#!/usr/bin/env python3
"""Check actual city/countryside LOD surfaces and walking-height transitions.

CPU only. The source town mesh/field are never altered. The west entrance is
sampled at 2.5cm intervals; all four rectangular field edges are also reported.
"""
from pathlib import Path
import argparse, copy, hashlib, json, sys
import numpy as np
from build_city_countryside import readfield, sample
from countryside_contact import TerrainSurfaces

ROOT=Path(__file__).resolve().parents[1]

def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def metric(values, points):
    values=np.asarray(values);valid=np.isfinite(values)
    if not valid.any():return dict(max_abs=None,missing=int(len(values)))
    i=int(np.nanargmax(np.abs(values)))
    return dict(max_abs=float(abs(values[i])),signed=float(values[i]),native_xz=points[i].tolist(),missing=int((~valid).sum()))

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--catalog',type=Path,required=True)
    parser.add_argument('--project',type=Path,default=ROOT/'projects/valwood_city_showcase')
    parser.add_argument('--review',type=Path,help='Candidate landscape-review.npz; overrides fallback field samples')
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--max-grade',type=float,default=.225,help='Dense entrance grade limit, including source mesh variation')
    args=parser.parse_args()
    data=json.loads(args.catalog.read_text());origin=np.asarray(json.loads((args.project/'city_showcase.json').read_text())['origin'])
    primary_path=args.project/'ground/showcase.height'
    city,city_origin,city_step=readfield(primary_path)
    low=city_origin-origin[[0,2]];high=low+city_step*(np.array(city.shape)[::-1]-1)
    if args.review:
        review=np.load(args.review);country=review['height_world'];country_origin=np.array([review['x_native'][0],review['z_native'][0]])+origin[[0,2]]
        country_step=np.array([np.diff(review['x_native'])[0],np.diff(review['z_native'])[0]])
        fallback_path=args.review
    else:
        fallback_path=Path(data['countryside']['ground_files'][0]);country,country_origin,country_step=readfield(fallback_path)
    def primary(p):return sample(city,np.asarray(p)+origin[[0,2]],city_origin,city_step)
    def fallback(p):return sample(country,np.asarray(p)+origin[[0,2]],country_origin,country_step)
    def collision(p):
        p=np.asarray(p);inside=np.all(p>=low,axis=-1)&np.all(p<=high,axis=-1)
        return np.where(inside,primary(p),fallback(p))
    # Reuse the exact indexed-triangle barycentric service, including stitched
    # edge fans. The synthetic name only selects the source ground for this
    # read-only service; no catalog or placement is written.
    ground=data['ground_asset'];source=copy.deepcopy(next(a for a in data['assets']if a['name']==ground))
    source['name']='BEP_Countryside_Terrain_SourceGround'
    placements=[]
    for p in data['placements']:
        if p['asset']==ground:
            q=copy.deepcopy(p);q['asset']=source['name'];placements.append(q)
    missing=lambda p:np.full(np.asarray(p).shape[:-1],np.nan)
    town=TerrainSurfaces(dict(assets=[source],placements=placements),origin,missing)
    landscape=TerrainSurfaces(data,origin,missing)
    def visual(p,lod):
        world=np.asarray(p)+origin[[0,2]];source_h=town.sample(world,lod);outside_h=landscape.sample(world,lod)
        return np.where(np.isfinite(source_h),source_h,outside_h)
    lines=[];passed=True
    for z in [3.,5.25,5.75,7.]:
        # The immutable source mesh rim and its raster bounds differ by about
        # 0.2mm from export rounding. Straddle that harmless gap by 1mm rather
        # than declaring an uncovered terrain hole at a submillimetre sample.
        x=np.r_[np.arange(low[0]-16,low[0]+.75,.025),low[0]-.001,low[0]+.001]
        x=np.unique(np.sort(x));x=x[abs(x-low[0])>1e-8]
        p=np.column_stack((x,np.full(len(x),z)));dx=np.diff(x)
        walking=collision(p);row=dict(native_z=z,world_z=float(z+origin[2]),samples=len(p),spacing_m=.025)
        switch=np.array([[low[0]-.0001,z],[low[0]+.0001,z]])
        row['collision_switch_m']=float(abs(np.diff(collision(switch))[0]))
        row['collision_max_grade']=float(np.max(abs(np.diff(walking)[dx>=.02]/dx[dx>=.02])))
        row['lods']=[]
        for lod in range(2):
            heights=visual(p,lod);delta=np.diff(heights);grade=abs(delta/dx)
            # Exact-boundary epsilon pair isolates a hard discontinuity, while
            # the regular 2.5cm pairs catch a short steep ramp beside the rim.
            finite=np.isfinite(heights);step=float(np.nanmax(abs(delta)));max_grade=float(np.nanmax(grade[dx>=.02]));i=int(np.nanargmax(abs(delta)))
            result=dict(lod='HIGH'if lod==0 else'MID',missing=int((~finite).sum()),max_adjacent_rise_m=step,max_grade=max_grade,
                missing_native_x=x[~finite].tolist(),worst_step_native_x=[float(x[i]),float(x[i+1])],visual_minus_collision=metric(heights-walking-.05,p))
            result['passed']=bool(finite.all()and step<.03 and max_grade<=args.max_grade and row['collision_switch_m']<.03 and row['collision_max_grade']<=args.max_grade)
            row['lods'].append(result);passed &=result['passed']
        lines.append(row)
    edges=[]
    for axis,name,sign in [(0,'west',-1),(0,'east',1),(1,'south',-1),(1,'north',1)]:
        p=np.zeros((1025,2));p[:,axis]=low[axis]if sign<0 else high[axis];p[:,1-axis]=np.linspace(low[1-axis],high[1-axis],len(p))
        outside=p.copy();outside[:,axis]+=sign*.0001
        edge=dict(edge=name,field_switch=metric(primary(p)-fallback(outside),p),offroad_limits_informational=True)
        for distance in [.15,1.]:
            q=p.copy();q[:,axis]+=sign*distance
            edge[f'HIGH_difference_{distance:g}m_outside']=metric(primary(p)+.05-landscape.sample(q+origin[[0,2]],0),p)
        edges.append(edge)
    report=dict(kind='Actual source ground + countryside HIGH/MID triangle and primary/fallback entrance transition',passed=bool(passed),
        catalog=str(args.catalog.resolve()),catalog_sha256=digest(args.catalog),primary_field_sha256=digest(primary_path),fallback_data=str(fallback_path.resolve()),fallback_sha256=digest(fallback_path),
        native_bounds=[low.tolist(),high.tolist()],world_origin=origin.tolist(),criteria=dict(max_lip_m=.03,max_grade=args.max_grade,nominal_road_grade=.20,spacing_m=.025),entrance=lines,all_edges=edges,
        note='All-edge figures are diagnostic: source coverage/fill can jump off-road. Entrance lines are the pass/fail gate. No GPU or project writes.')
    args.output.mkdir(parents=True,exist_ok=True);(args.output/'report.json').write_text(json.dumps(report,indent=2,allow_nan=False)+'\n')
    print(json.dumps(dict(passed=bool(passed),report=str(args.output/'report.json'),entrance=[dict(z=v['native_z'],switch=v['collision_switch_m'],lods=[dict(lod=r['lod'],lip=r['max_adjacent_rise_m'],grade=r['max_grade'],missing=r['missing'])for r in v['lods']])for v in lines]),indent=2))
    return 0 if passed else 1

if __name__=='__main__':sys.exit(main())
