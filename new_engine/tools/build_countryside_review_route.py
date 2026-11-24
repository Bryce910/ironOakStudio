#!/usr/bin/env python3
"""Create native camera stations and a separately collision-audited timing path."""
from pathlib import Path
import hashlib,json,math,sys
import numpy as np
from build_city_countryside import ROOT,OUT,PROJECT,readfield,sample

def main():
    qa=ROOT/'docs/countryside/2026-11-02';qa.mkdir(parents=True,exist_ok=True)
    report=json.loads((qa/'final-cpu-audit/report.json').read_text());assert report['passed']
    road=np.load(OUT/'landscape-review.npz')['road_native'];o=np.asarray(json.loads((PROJECT/'city_showcase.json').read_text())['origin'])
    city,co,cs=readfield(PROJECT/'ground/showcase.height');country,go,gs=readfield(OUT/'ground/countryside.height')
    def eye(p):
        w=np.asarray(p)+o[[0,2]];uv=(w-co)/cs
        field,origin,step=(city,co,cs)if np.all(uv>=0)and np.all(uv<=np.array(city.shape)[::-1]-1)else(country,go,gs)
        return float(sample(field,w,origin,step))+1.75
    def camera(p,target,offset=0):
        y=eye(p)+offset;v=np.array([target[0]-p[0],target[1]-y,target[2]-p[1]]);yaw=math.atan2(v[0],-v[2]);pitch=math.atan2(v[1],math.hypot(v[0],v[2]));return [p[0]+o[0],y,p[1]+o[2],yaw,pitch,math.radians(60)]
    original=next(s for s in (PROJECT/'showcase.views').read_text().splitlines()if s.startswith('city-camera-11 '));views=[('city-regression',list(map(float,original.split()[1:])))]
    approach=camera(road[80],[0,30,0]);meadow=camera(road[450],[-134.5,83,202.6])
    woodland=camera(road[850],[road[920,0],eye(road[920]),road[920,1]])
    views += [('city-exit',camera(np.array([-47,5.75]),[-75,22,5.25])),('city-approach',approach),('city-meadow',meadow),('city-woodland',woodland),('woodland-path',camera(road[600],[road[700,0],eye(road[700]),road[700,1]])),('country-overview',camera(np.array([-174,-45]),[-120,57,100],8))]
    rows=['VALWOOD_SOURCE_REVIEW 1 60']+[name+' 0 60 1 '+' '.join(format(float(v),'.15g')for v in values)for name,values in views]
    path=qa/'countryside-stations.route';path.write_text('\n'.join(rows)+'\n');path.with_suffix('.json').write_text(json.dumps(dict(kind='Countryside camera stations; explicit cuts, not walking simulation',cpu_geometry_audit=str(qa/'final-cpu-audit/report.json'),capture_checkpoints=len(views),requested_ticks=len(views)*61,obstruction_cuts=0,route_sha256=hashlib.sha256(path.read_bytes()).hexdigest()),indent=2)+'\n')
    (qa/'countryside.views').write_text('\n'.join(name+' '+' '.join(format(float(v),'.15g')for v in values)for name,values in views if name in ['city-approach','city-meadow','city-woodland'])+'\n')
    # Travel from original spawn into the meadow, without display readbacks.
    points=[np.array([12.,-5]),np.array([-14.5,-5.75]),np.array([-48.775606536865,5.75])]+list(road[:451:6]);motion=[];distance=0.
    for a,b in zip(points,points[1:]):
        length=float(np.linalg.norm(b-a));count=max(1,math.ceil(length/.3));yaw=math.atan2(b[0]-a[0],-(b[1]-a[1]))
        for i in range(1,count+1):
            p=a+(b-a)*i/count;motion.append((p,yaw,max(1,round(length/count/2.5*60))))
        distance+=length
    rows=['VALWOOD_SOURCE_REVIEW 1 60','start 0 60 0 '+' '.join(format(v,'.15g')for v in camera(points[0],[points[1][0],eye(points[1]),points[1][1]]))]
    for i,(p,yaw,ticks)in enumerate(motion):rows.append(f'country-{i} {ticks} 0 0 {p[0]+o[0]:.15g} {eye(p):.15g} {p[1]+o[2]:.15g} {yaw:.15g} 0 {math.radians(60):.15g}')
    path=qa/'countryside-motion.route';path.write_text('\n'.join(rows)+'\n');path.with_suffix('.json').write_text(json.dumps(dict(kind='Render timing from city through separately Jolt-checked countryside; camera replay, not gameplay simulation',cpu_geometry_audit=str(qa/'final-cpu-audit/report.json'),capture_checkpoints=0,requested_ticks=61+sum(t+1 for p,y,t in motion),obstruction_cuts=0,distance_m=distance,target_translation_mps=2.5,route_sha256=hashlib.sha256(path.read_bytes()).hexdigest()),indent=2)+'\n')
    print(json.dumps(dict(capture_stations=len(views),motion_distance_m=distance,motion_points=len(motion)),indent=2))

if __name__=='__main__':main()
