#!/usr/bin/env python3
"""CPU receiving-domain proof against both actual encoded tavern LODs.

Read actual shader feather widths, actual source triangle surfaces and the
published emitter include. This isolates clipping from inverse-square falloff,
source distance and BRDF normals; it makes no rendered-image or FPS claim.
"""
import copy
import json
import math
from pathlib import Path
import re
import sys

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from build_starting_area import glb_bounds


def shader_fades():
    expected=None
    for name in ['world_foliage.fs.glsl','world_terrain.fs.glsl','world_water.fs.glsl']:
        text=re.sub(r'\s+','',(ROOT/'shaders'/name).read_text())
        assert 'if(plane<=0.||min(edge.x,edge.y)<=0.||vertical<=0.)return0.;'in text,name
        match=re.search(r'returnwindow\*window/max\(d2,\.25\)\*smoothstep\(0\.,([0-9.]+),plane\)\*smoothstep\(0\.,([0-9.]+),min\(edge.x,edge.y\)\)\*smoothstep\(0\.,([0-9.]+),vertical\);',text)
        assert match,('Actual shader clip formula changed; review CPU proof',name)
        values=tuple(float(v)for v in match.groups())
        assert all(math.isfinite(v)and v>0 for v in values)
        assert expected is None or values==expected,('Render passes disagree on clip feathers',name)
        expected=values
    return expected


def clip_weight(emitter,point,fades):
    x,y,z=point;nx,ny,nz,d=emitter['plane'];xmin,zmin,xmax,zmax=emitter['receiving_bounds_local_xz'];ymin,ymax=emitter['receiving_y']
    plane=nx*x+ny*y+nz*z+d;edge=min(x-xmin,z-zmin,xmax-x,zmax-z);vertical=min(y-ymin,ymax-y)
    if min(plane,edge,vertical)<=0:return 0.
    def smooth(width,value):
        t=max(0.,min(1.,value/width));return t*t*(3-2*t)
    return smooth(fades[0],plane)*smooth(fades[1],edge)*smooth(fades[2],vertical)


def area_normal(triangle):
    a,b,c=triangle;u=[b[i]-a[i]for i in range(3)];v=[c[i]-a[i]for i in range(3)]
    n=[u[1]*v[2]-u[2]*v[1],u[2]*v[0]-u[0]*v[2],u[0]*v[1]-u[1]*v[0]]
    return math.sqrt(sum(x*x for x in n))*.5,n


def centroid(triangle):return [sum(p[i]for p in triangle)/3 for i in range(3)]


def actual_plane(triangles,axis,value,minimum_area=.05):
    found=[]
    for triangle in triangles:
        if max(abs(p[axis]-value)for p in triangle)>1.e-5:continue
        area,n=area_normal(triangle);p=centroid(triangle)
        if axis==1 and n[1]<=0:continue
        if area>=minimum_area and(axis==1 or .2<p[1]<4.3):found.append(p)
    assert found,('No actual encoded triangles at expected source surface',axis,value)
    return found


def main():
    fades=shader_fades()
    layout=json.loads((ROOT/'projects/valwood_world/scenery/castle_streets_layout.json').read_text())
    manifest=json.loads((ROOT/'projects/valwood_world/models/castle_tavern_manifest.json').read_text())
    pose=next(p for p in layout['records']if p['alias']=='village_tavern');assert pose['yaw_degrees']==0 and pose['scale']==1
    ox,oz=pose['local_xz'];oy=pose['grounded_y']
    emitters=layout['lights'][:2]
    assert [e['id']for e in emitters]==['tavern_hearth','tavern_hanging_lamp']
    include=(ROOT/'engine/world_castle_streets_torches.inc').read_text()
    rows=[[float(v.strip().rstrip('fu'))for v in row.split(',')]for row in re.findall(r'\{([^{}]+)\}',include)]
    assert len(rows)==4 and all(len(row)==17 for row in rows)
    for row,e in zip(rows,emitters):
        assert row[7:11]==e['plane']and row[11:15]==e['receiving_bounds_local_xz']and row[15:17]==e['receiving_y'],'Compiled emitter differs from source layout'
    wall_planes={}
    for collider in manifest['colliders']:
        if collider['alias']!='village_tavern':continue
        label=collider['label'];b=collider['box']
        if label=='side_wall':
            wall_planes['west'if b[0]<0 else'east']=(0,b[1]if b[0]<0 else b[0],b[0]if b[0]<0 else b[1])
        elif label=='front_wall':wall_planes['front']=(2,b[2],b[3])
        elif label=='rear_wall':wall_planes['rear']=(2,b[3],b[2])
    assert len(wall_planes)==4
    summaries=[];probes=0
    for lod in ['near','mid']:
        _,_,_,triangles=glb_bounds(ROOT/'projects/valwood_world/models'/f'village_tavern_{lod}.glb',True)
        groups={'floor':actual_plane(triangles,1,manifest['anchors']['floor_y'],1)}
        outside=[]
        for name,(axis,inner,outer)in wall_planes.items():
            groups[name+'_inner_wall']=actual_plane(triangles,axis,inner)
            outside+=actual_plane(triangles,axis,outer)
        ceiling=[]
        for triangle in triangles:
            area,n=area_normal(triangle);high=max(p[1]for p in triangle)
            if high<8 or min(p[1]for p in triangle)>4.5 or n[1]>=0 or area<50:continue
            # A barycentric point near the ridge, on the ACTUAL underside
            # triangle. Keep it inside the walls even for a corner triangle.
            top=[p for p in triangle if abs(p[1]-high)<1.e-5]
            ridge=[sum(p[i]for p in top)/len(top)for i in range(3)];centre=centroid(triangle)
            t=.05 if len(top)>1 else .20
            point=[ridge[i]*(1-t)+centre[i]*t for i in range(3)]
            assert point[1]>7.5 and abs(point[0])<9.5
            ceiling.append(point)
        assert len(ceiling)==4;groups['upper_ceiling']=ceiling
        world=lambda p:[ox+p[0],oy+p[1],oz+p[2]]
        new={};old={}
        for name,points in groups.items():
            weights=[clip_weight(e,world(p),fades)for e in emitters for p in points];probes+=len(weights)
            assert min(weights)>.999999,(lod,name,'Interior geometry loses practical light',min(weights))
            new[name]={'points':len(points),'minimum_weight':min(weights)}
            previous=[]
            for e in emitters:
                e=copy.deepcopy(e);e['receiving_bounds_local_xz']=[-154.7,-89.7,-135.3,-74.3];e['receiving_y']=[24,31]
                if e['id']=='tavern_hearth':e['plane']=[0,0,1,89.7]
                previous += [clip_weight(e,world(p),fades)for p in points]
            old[name]={'minimum_weight':min(previous),'maximum_weight':max(previous)}
        assert old['floor']['maximum_weight']<.07 and old['upper_ceiling']['maximum_weight']==0
        assert all(old[k]['maximum_weight']==0 for k in groups if k.endswith('_inner_wall'))
        outside_weights=[clip_weight(e,world(p),fades)for e in emitters for p in outside];probes+=len(outside_weights)
        assert max(outside_weights)==0,(lod,'Practical light leaks to actual exterior wall faces')
        summaries.append({'lod':lod,'actual_source_triangles':len(triangles),'corrected':new,'previous_policy':old,'exterior_wall_points':len(outside),'maximum_exterior_weight':max(outside_weights)})
    print(json.dumps({'passed':True,'shader_clip_feathers_m':dict(zip(['plane','horizontal_edge','vertical_edge'],fades)),'weight_probes':probes,'actual_lods':summaries,'scope':'CPU clipping weights on actual encoded floor/wall/roof triangles, all three shader formulas and the published17-field emitter include. Physical distance/BRDF and native appearance remain separately evaluated.'},indent=2))


if __name__=='__main__':main()
