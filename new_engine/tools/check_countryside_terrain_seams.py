#!/usr/bin/env python3
"""Measure actual GLB boundaries at every neighboring terrain LOD pairing."""
import argparse,json
from pathlib import Path
import numpy as np
from build_village_showcase_project import glb_geometry

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--catalog',type=Path,required=True);ap.add_argument('--output',type=Path,required=True);ap.add_argument('--maximum-gap',type=float,default=.0001);args=ap.parse_args()
    data=json.loads(args.catalog.read_text());assets={a['name']:a for a in data['assets']};edge={};areas=[]
    for r in data['placements']:
        if not r['asset'].startswith('BEP_Countryside_Terrain_'):continue
        a=assets[r['asset']];m=np.array(r['matrix_native']).reshape(4,4).T.copy();m[:3,3]+=m[:3,:3]@a['pivot_offset_native'];xi,zi=map(int,r['asset'].split('_')[-2:]);x0=-220+110*xi;z0=-180+115*zi
        for lod,path in enumerate(a['files']):
            v,t=glb_geometry(path);cross=np.cross(v[t[:,1]]-v[t[:,0]],v[t[:,2]]-v[t[:,0]]);keep=np.linalg.norm(cross,axis=1)>1e-7
            assert np.all(cross[keep,1]>0),'Downward or overlapping terrain triangle'
            areas.append(dict(tile=[xi,zi],lod=lod,projected_area_m2=float(cross[keep,1].sum()/2)))
            v=v[np.unique(t[keep])];v=v@m[:3,:3].T+m[:3,3]
            for axis,bound,side in [(0,x0,'w'),(0,x0+110,'e'),(2,z0,'s'),(2,z0+115,'n')]:
                q=v[np.abs(v[:,axis]-bound)<1e-3];k=2 if axis==0 else 0
                if len(q):
                    q=q[np.argsort(q[:,k])];coords,idx=np.unique(q[:,k].round(5),return_index=True);edge[xi,zi,lod,side]=(coords,q[idx,1])
    rows=[]
    for zi in range(4):
        for xi in range(4):
            for dx,dz,a,b in [(1,0,'e','w'),(0,1,'n','s')]:
                if xi+dx>3 or zi+dz>3:continue
                for la,lb in [(0,0),(0,1),(1,0),(1,1)]:
                    u,h=edge[xi,zi,la,a];v,g=edge[xi+dx,zi+dz,lb,b];x=np.unique(np.r_[u,v]);lo=max(u[0],v[0]);hi=min(u[-1],v[-1]);x=x[(x>=lo)&(x<=hi)];error=np.abs(np.interp(x,u,h)-np.interp(x,v,g))
                    rows.append(dict(tiles=[[xi,zi],[xi+dx,zi+dz]],lods=[la,lb],max_gap_m=float(error.max())))
    gap=max(r['max_gap_m']for r in rows);report=dict(passed=gap<=args.maximum_gap,edge_pairings=len(rows),max_gap_m=gap,worst=sorted(rows,key=lambda x:x['max_gap_m'],reverse=True)[:8],areas=areas)
    args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({k:v for k,v in report.items()if k!='areas'},indent=2));raise SystemExit(0 if report['passed']else 1)

if __name__=='__main__':main()
