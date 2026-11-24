#!/usr/bin/env python3
"""Read-only normal-input review waypoints on the actual traffic road center.

Returns XYZ targets only. Does not operate the game or modify saves/settings.
Camp and town connectors retain authored endpoints; moving traffic can still
require an ordinary pause/detour at turnaround crossings.
"""
from pathlib import Path
from dataclasses import dataclass
import argparse
import bisect
import hashlib
import json
import math


def xyz(p):
    if len(p)!=3 or not all(math.isfinite(float(v)) and abs(float(v))<=1e6 for v in p):
        raise ValueError('Expected bounded finite XYZ position')
    return tuple(map(float,p))


def distance(a,b):return math.hypot(b[0]-a[0],b[2]-a[2])
def mix(a,b,t):return tuple(x+(y-x)*t for x,y in zip(a,b))


@dataclass(frozen=True)
class Projection:
    station:float
    point:tuple
    distance:float


class Road:
    def __init__(self,settings):
        self.settings=Path(settings)
        raw=self.settings.read_bytes()
        if len(raw)>16384:raise ValueError('Road settings exceed bounded size')
        rows=raw.decode().splitlines()
        if not rows or rows[0]!='VALWOOD_ROAD_TRAFFIC 1':raise ValueError('Unsupported road settings')
        original=[];trim=None;ended=False
        for row in rows[1:]:
            p=row.split()
            if not p or p[0].startswith('#'):continue
            if ended:raise ValueError('Trailing road settings')
            if p[0]=='point' and len(p)==4:original.append(xyz(p[1:]))
            elif p[0]=='trim' and len(p)==3:trim=tuple(map(float,p[1:]))
            elif p[0]=='end' and len(p)==1:ended=True
            elif p[0] not in ('lanes','camp','town','encounter','satchel'):raise ValueError('Unknown road record')
        if not ended or len(original)!=6 or trim is None or not all(math.isfinite(v) and 6<=v<=60 for v in trim):raise ValueError('Incomplete road')
        self.sha256=hashlib.sha256(raw).hexdigest()
        points=original[:]
        def move(a,b,d):
            n=distance(a,b)
            if n<15 or d>=n-3:raise ValueError('Unsafe short road segment')
            return mix(a,b,d/n)
        points[0]=move(points[0],points[1],trim[0]);points[-1]=move(points[-1],points[-2],trim[1])
        self.points=[points[0]]
        def push(p):
            if distance(self.points[-1],p)>1e-6:self.points.append(p)
        def line(a,b):
            count=max(1,math.ceil(distance(a,b)/.6))
            for k in range(1,count+1):push(mix(a,b,k/count))
        previous=points[0]
        for i in range(1,5):
            a=move(points[i],points[i-1],2);b=move(points[i],points[i+1],2)
            line(previous,a)
            for j in range(1,17):
                t=j/16;u=1-t
                push(tuple(u*u*x+2*u*t*y+t*t*z for x,y,z in zip(a,points[i],b)))
            previous=b
        line(previous,points[-1])
        if len(self.points)>1536:raise ValueError('Road exceeds production capacity')
        self.stations=[0.]
        for a,b in zip(self.points,self.points[1:]):self.stations.append(self.stations[-1]+distance(a,b))
        self.length=self.stations[-1]
        if not 100<self.length<1500:raise ValueError('Unsupported road length')

    def center(self,station):
        station=min(self.length,max(0.,float(station)))
        i=min(len(self.points)-2,max(0,bisect.bisect_right(self.stations,station)-1))
        return mix(self.points[i],self.points[i+1],(station-self.stations[i])/(self.stations[i+1]-self.stations[i]))

    def project(self,position):
        p=xyz(position);best=None
        for i,(a,b) in enumerate(zip(self.points,self.points[1:])):
            dx=b[0]-a[0];dz=b[2]-a[2];n=dx*dx+dz*dz
            t=min(1.,max(0.,((p[0]-a[0])*dx+(p[2]-a[2])*dz)/n));q=mix(a,b,t)
            candidate=Projection(self.stations[i]+math.sqrt(n)*t,q,distance(p,q))
            if best is None or candidate.distance<best.distance:best=candidate
        return best


def simplify(points,tolerance=.02):
    if len(points)<3:return points
    a,b=points[0],points[-1];dx=b[0]-a[0];dz=b[2]-a[2];n=dx*dx+dz*dz
    def error(p):
        t=min(1.,max(0.,((p[0]-a[0])*dx+(p[2]-a[2])*dz)/n)) if n else 0.
        return distance(p,mix(a,b,t))
    index=max(range(1,len(points)-1),key=lambda i:error(points[i]))
    if error(points[index])<=tolerance:return [a,b]
    return simplify(points[:index+1],tolerance)[:-1]+simplify(points[index:],tolerance)


def navigation(project,name,start):
    """Return bounded ordinary XYZ walk targets from current root to authored goal."""
    if name not in ('encounter','return','town'):raise ValueError('Only road quest legs are supported')
    project=Path(project);start=xyz(start)
    road=Road(project/'opening/road-traffic.settings')
    paths=json.loads((project/'opening/first-journey.routes.json').read_text())['paths']
    authored=[xyz(p) for p in paths[name]]
    if not 2<=len(authored)<=512:raise ValueError('Invalid authored endpoint route')
    goal=authored[-1];a=road.project(start);b=road.project(goal)
    if a.distance>40 or b.distance>40:raise ValueError('Endpoint is outside the audited road neighborhood')
    direction=1 if b.station>=a.station else -1
    interior=[p for s,p in zip(road.stations,road.points) if min(a.station,b.station)<s<max(a.station,b.station)]
    if direction<0:interior.reverse()
    centers=simplify([a.point,*interior,b.point],.02)
    # Original connector waypoints are retained only beyond the traffic road
    # endpoints, never along the old bent path through the active1.65m lanes.
    connector=[]
    if b.station<1e-5 or b.station>road.length-1e-5:
        for p in authored:
            q=road.project(p)
            if abs(q.station-b.station)<1e-5 and q.distance>4:connector.append(p)
    result=[]
    for p in [start,*centers,*connector,goal]:
        if not result or distance(result[-1],p)>.03:result.append(p)
    if len(result)>1600:raise ValueError('Navigation exceeded bounded waypoints')
    result[-1]=goal
    return {'name':name,'settings_sha256':road.sha256,'road_length_m':road.length,'start_projection':a.__dict__,
            'goal_projection':b.__dict__,'maximum_centerline_shortcut_m':.02,'recommended_tolerance_m':.12,
            'waypoints':[list(p) for p in result],'authored_goal':list(goal),
            'scope':'Read-only walk targets; endpoints and ordinary traffic/collision remain active'}


def waypoints(project,name,start):return navigation(project,name,start)['waypoints']


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('project',type=Path);parser.add_argument('leg',choices=('encounter','return','town'));parser.add_argument('x',type=float);parser.add_argument('y',type=float);parser.add_argument('z',type=float)
    a=parser.parse_args();print(json.dumps(navigation(a.project,a.leg,(a.x,a.y,a.z)),indent=2))
