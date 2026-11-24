#!/usr/bin/env python3
"""CPU-only exact-triangle contact checks for authored city support assets."""
from pathlib import Path
import argparse,json,hashlib
import numpy as np
from scipy.ndimage import map_coordinates
from scipy.spatial import cKDTree
from cook_village_publisher_scene import ROOT,Source,C
from repair_city_boundary_materials import GLB
from build_city_boundary_foundations import Ground

PLINTH='SM_City_Castle_Backdrop_Plinth'
FOUNDATION='SM_City_Boundary_Rock_Foundations'

class Surface:
    def __init__(self,triangles):
        self.triangles=triangles;self.tree=cKDTree(triangles[:,:,[0,2]].mean(1))
        self.radius=np.linalg.norm(triangles[:,:,[0,2]]-triangles[:,:,[0,2]].mean(1)[:,None],axis=2).max()+.02
    def heights(self,points,lower=-np.inf,upper=np.inf,minimum=False):
        output=np.full(len(points),np.nan);lower=np.broadcast_to(lower,(len(points),));upper=np.broadcast_to(upper,(len(points),))
        for i,(q,ids)in enumerate(zip(points,self.tree.query_ball_point(points,self.radius))):
            if not ids:continue
            tri=self.triangles[ids];a,b,c=tri[:,0][:,[0,2]],tri[:,1][:,[0,2]],tri[:,2][:,[0,2]]
            den=(b[:,1]-c[:,1])*(a[:,0]-c[:,0])+(c[:,0]-b[:,0])*(a[:,1]-c[:,1]);good=abs(den)>1e-9
            tri,a,b,c,den=tri[good],a[good],b[good],c[good],den[good]
            u=((b[:,1]-c[:,1])*(q[0]-c[:,0])+(c[:,0]-b[:,0])*(q[1]-c[:,1]))/den
            w=((c[:,1]-a[:,1])*(q[0]-c[:,0])+(a[:,0]-c[:,0])*(q[1]-c[:,1]))/den;t=1-u-w
            h=u*tri[:,0,1]+w*tri[:,1,1]+t*tri[:,2,1];hit=(u>=-1e-5)&(w>=-1e-5)&(t>=-1e-5)&(h>=lower[i])&(h<=upper[i])
            if hit.any():output[i]=h[hit].min()if minimum else h[hit].max()
        return output

def safe_ground(ground,q):
    flat=q+ground.origin[[0,2]];out=np.full(len(q),np.nan)
    for values,origin,step in [ground.city,ground.country]:
        uv=(flat-origin)/step;inside=(uv>=0).all(1)&(uv<=np.array(values.shape)[::-1]-1).all(1)&~np.isfinite(out)
        out[inside]=map_coordinates(values,np.array([uv[inside,1],uv[inside,0]]),order=1,mode='nearest')-ground.origin[1]
    return out

def read_asset(catalog,name,lod):
    asset=next(a for a in catalog['assets']if a['name']==name);placement=next(p for p in catalog['placements']if p['asset']==name)
    g=GLB(asset['files'][lod]);m=np.array(placement['matrix_native']).reshape(4,4).T;pieces=[]
    for primitive in g.doc['meshes'][0]['primitives'][:-1]:
        p=g.read(primitive['attributes']['POSITION']).astype(np.float64);p+=asset['pivot_offset_native'];p=p@m[:3,:3].T+m[:3,3]
        ix=g.read(primitive['indices']).ravel().astype(int).reshape(-1,3);pieces.append((p,ix))
    return pieces

def audit(catalog):
    source=Source();ground=Ground();boulders=[]
    for lod in [0,1]:
        pieces=read_asset(catalog,PLINTH,lod);counts=[]
        for rock in catalog['new_background']['rocks']:
            v,parts,_=source.geometry(rock['source_mesh'],lod);counts.append(len(np.unique(parts[0]['indices'])))
        if len(pieces)==1:
            p,ix=pieces[0];prefix=len(p)-sum(counts);shell=p[:prefix].reshape(-1,3,3);rocks=p[prefix:]
        else:
            shell=pieces[0][0][pieces[0][1]];rocks=pieces[1][0]
        surface=Surface(shell);offset=0
        for index,(rock,count)in enumerate(zip(catalog['new_background']['rocks'],counts)):
            q=rocks[offset:offset+count];offset+=count;terrain=safe_ground(ground,q[:,[0,2]]);rock_top=surface.heights(q[:,[0,2]])
            support=np.fmax(terrain,rock_top);gap=q[:,1]-support;valid=np.isfinite(gap)
            boulders.append(dict(lod=lod,index=index,source_mesh=rock['source_mesh'],tier=rock['tier'],
                minimum_surface_gap=float(np.nanmin(gap))if valid.any()else None,
                unsupported=bool(not valid.any()or np.nanmin(gap)>.05),outside_field_vertices=int(np.sum(~np.isfinite(terrain)))))
    # The original boundary bases meet the generated upper cap; sample actual
    # bottom vertices rather than guessing which visible wall caused a gap.
    pieces=read_asset(catalog,FOUNDATION,0);foundation=Surface(pieces[0][0][pieces[0][1]]);walls=[]
    rows={row['component']:row for row in catalog['boundary_foundations']['supports']}
    for placement in catalog['placements']:
        if not placement['asset'].startswith('SM_Boundary_'):continue
        row=rows[placement['source_component']]
        if not row['triangles']:continue
        v,_,_=source.geometry(placement['asset'],0);m=np.array(placement['matrix_native']).reshape(4,4).T
        p=(v[:,:3]@C.T/100)@m[:3,:3].T+m[:3,3];base=p[:,1].min();q=np.unique(np.round(p[p[:,1]<base+.025][:,[0,2]],5),axis=0)
        top=foundation.heights(q,upper=base+.5);top=np.fmax(top,safe_ground(ground,q));gap=base-top;valid=np.isfinite(gap)
        walls.append(dict(source_component=placement['source_component'],asset=placement['asset'],bottom_samples=len(q),
            contact_samples=int(valid.sum()),maximum_gap=float(np.nanmax(gap))if valid.any()else None,
            cap_overlap_minimum=float(np.nanmin(top-base))if valid.any()else None,portal=row['portal_cut']))
    points=ground.road[::10];h=ground.height(points);ceiling=foundation.heights(points,lower=h+.2,minimum=True);valid=np.isfinite(ceiling)
    report=dict(schema=1,method='Exact source-bottom / GLB triangle barycentric vertical rays; terrain bilinear in installed composed domains',
        boulders=boulders,unsupported_boulder_indices=sorted({r['index']for r in boulders if r['unsupported']}),
        boundary_contacts=walls,boundary_uncovered=[r for r in walls if r['contact_samples']==0],
        maximum_boundary_gap=max(r['maximum_gap']for r in walls if r['maximum_gap']is not None),
        road_ceiling_samples=int(valid.sum()),minimum_road_ceiling_height_m=float(np.min(ceiling[valid]-h[valid]))if valid.any()else None,
        source_fields_unchanged=True)
    return report

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--catalog',type=Path,required=True);ap.add_argument('--output',type=Path,required=True);args=ap.parse_args()
    report=audit(json.loads(args.catalog.read_text()));report.update(catalog=str(args.catalog.resolve()),catalog_sha256=hashlib.sha256(args.catalog.read_bytes()).hexdigest())
    args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps({k:v for k,v in report.items()if k not in ['boulders','boundary_contacts']},indent=2))

if __name__=='__main__':main()
