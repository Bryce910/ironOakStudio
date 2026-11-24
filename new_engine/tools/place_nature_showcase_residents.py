#!/usr/bin/env python3
"""Place idle dwarves on verified publisher floor triangles, CPU only.

Writes only the isolated source project's optional showcase.residents file and
an evidence report. It never alters the legacy resident table or source meshes.
"""
from pathlib import Path
import argparse,collections,functools,json,math
import numpy as np

ROOT=Path(__file__).resolve().parents[1]
DEFAULT_SOURCE=ROOT.parent/'.local-tools/unity-source/showcase-native/scene.json'
ORIGIN=np.array([-43344.,1130.8125,-23987.824156518269])

def point_triangle_distances(point,triangles):
    a,b,c=triangles[:,0],triangles[:,1],triangles[:,2]
    ab,ac=b-a,c-a;n=np.cross(ab,ac);nn=np.einsum('ij,ij->i',n,n)
    ap=point-a;plane=np.einsum('ij,ij->i',ap,n)
    q=point-n*(plane/np.maximum(nn,1e-20))[:,None]
    v=q-a;d00=np.einsum('ij,ij->i',ab,ab);d01=np.einsum('ij,ij->i',ab,ac);d11=np.einsum('ij,ij->i',ac,ac)
    d20=np.einsum('ij,ij->i',v,ab);d21=np.einsum('ij,ij->i',v,ac);den=d00*d11-d01*d01
    u=(d11*d20-d01*d21)/np.maximum(den,1e-20);w=(d00*d21-d01*d20)/np.maximum(den,1e-20)
    inside=(u>=0)&(w>=0)&(u+w<=1)&(nn>1e-20)
    distances=np.where(inside,plane*plane/np.maximum(nn,1e-20),np.inf)
    for start,end in ((a,b),(b,c),(c,a)):
        edge=end-start;t=np.clip(np.einsum('ij,ij->i',point-start,edge)/np.maximum(np.einsum('ij,ij->i',edge,edge),1e-20),0,1)
        d=point-start-edge*t[:,None];distances=np.minimum(distances,np.einsum('ij,ij->i',d,d))
    return distances

def vertical_hits(triangles,x,z):
    a,b,c=triangles[:,0],triangles[:,1],triangles[:,2]
    ab=b-a;ac=c-a;den=ab[:,0]*ac[:,2]-ab[:,2]*ac[:,0]
    safe=np.where(np.abs(den)>1e-12,den,1.)
    u=((x-a[:,0])*ac[:,2]-(z-a[:,2])*ac[:,0])/safe
    v=(ab[:,0]*(z-a[:,2])-ab[:,2]*(x-a[:,0]))/safe
    inside=(np.abs(den)>1e-12)&(u>=-1e-6)&(v>=-1e-6)&(u+v<=1+1e-6)
    return a[inside,1]+u[inside]*ab[inside,1]+v[inside]*ac[inside,1]

class Geometry:
    def __init__(self,path):
        self.path=path;self.doc=json.loads(path.read_text());self.meshes={m['hash']:m for m in self.doc['meshes']}
        self.placements=[p for p in self.doc['placements'] if p['lod_mask']&1 and p['group'] not in ('Emission','Lava Rivers and Lakes','Background Mountains') and 'decal' not in p['name'].lower() and 'ornament_pic' not in p['name'].lower()]
        self.mins=np.array([[p['bounds_min'][c] for c in 'xyz'] for p in self.placements]);self.maxs=np.array([[p['bounds_max'][c] for c in 'xyz'] for p in self.placements])
    @functools.lru_cache(maxsize=1800)
    def mesh(self,key):
        m=self.meshes[key];raw=(self.path.parent/m['binary']).read_bytes();vertices=np.ndarray((m['vertices'],16),'<f4',raw)[:,:6].astype(np.float64)
        indices=np.concatenate([np.ndarray((s['indices'],),'<i4',raw,s['index_offset']) for s in m['submeshes']]).reshape(-1,3)
        return vertices,indices
    def transformed(self,p):
        v,indices=self.mesh(p['mesh']);matrix=np.asarray(p['matrix']).reshape(4,4,order='F')
        points=v[:,:3]@matrix[:3,:3].T+matrix[:3,3]
        normals=v[:,3:6]@np.linalg.inv(matrix[:3,:3]);normals/=np.maximum(np.linalg.norm(normals,axis=1,keepdims=True),1e-12)
        return points[indices],normals[indices].mean(axis=1)
    def nearby(self,point,radius=1.,height=2.4):
        lo=point+np.array([-radius,-.2,-radius]);hi=point+np.array([radius,height,radius])
        mask=np.all(self.maxs>=lo,axis=1)&np.all(self.mins<=hi,axis=1)
        sets=[]
        for index in np.flatnonzero(mask):
            triangles,_=self.transformed(self.placements[index]);keep=np.all(triangles.max(axis=1)>=lo,axis=1)&np.all(triangles.min(axis=1)<=hi,axis=1)
            if keep.any():sets.append(triangles[keep])
        return np.concatenate(sets) if sets else np.empty((0,3,3))
    def floor_candidates(self,group):
        candidates=[]
        for p in self.placements:
            if p['group']!=group or 'floor' not in p['name'].lower():continue
            triangles,normals=self.transformed(p);area=np.linalg.norm(np.cross(triangles[:,1]-triangles[:,0],triangles[:,2]-triangles[:,0]),axis=1)*.5
            for i in np.flatnonzero((normals[:,1]>.8)&(area>.5)):
                tri=triangles[i]
                for weights in ((1/3,1/3,1/3),(.2,.4,.4),(.4,.2,.4),(.4,.4,.2)):
                    point=np.asarray(weights)@tri
                    candidates.append({'point':point,'area':float(area[i]),'hierarchy':p['hierarchy'],'normal_y':float(normals[i,1]),'group':group})
        return candidates
    def check(self,point):
        triangles=self.nearby(point)
        if not len(triangles):return None,'no nearby geometry'
        overhead=self.nearby(point,radius=.6,height=80.)
        support=[];headroom=math.inf
        for angle in range(9):
            dx,dz=(0.,0.) if angle==8 else (.5*math.cos(angle*math.pi/4),.5*math.sin(angle*math.pi/4))
            hits=vertical_hits(triangles,point[0]+dx,point[2]+dz)
            below=hits[np.abs(hits-point[1])<.06]
            if not len(below):return None,'support radius'
            support.extend(below.tolist());ceiling_hits=vertical_hits(overhead,point[0]+dx,point[2]+dz)
            above=ceiling_hits[ceiling_hits>point[1]+.08]
            if len(above):headroom=min(headroom,float(above.min()-point[1]))
        if headroom<1.8:return None,'headroom'
        # Conservative overlapping spheres cover a stationary dwarf's body.
        # Exclude coplanar support/decal triangles, not elevated floor steps.
        obstacles=triangles[triangles[:,:,1].max(axis=1)>point[1]+.08]
        clearance=math.inf
        for height,radius in ((.5,.5),(.85,.5),(1.2,.5),(1.55,.4),(1.8,.25)):
            if not len(obstacles):continue
            distance=float(np.sqrt(point_triangle_distances(point+[0,height,0],obstacles).min()));clearance=min(clearance,distance-radius)
            if distance<radius:return None,'body clearance'
        return {'support_height_range':[min(support),max(support)],'support_radius_m':.5,'minimum_vertical_headroom_m':headroom if math.isfinite(headroom) else None,'minimum_sphere_clearance_m':clearance if math.isfinite(clearance) else None,'local_triangles_checked':len(triangles)},None

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--source',type=Path,default=DEFAULT_SOURCE);ap.add_argument('--project',type=Path,default=ROOT/'projects/valwood_showcase');ap.add_argument('--report',type=Path,default=ROOT/'docs/stronghold/2026-11-01/showcase/resident-placement-audit.json');args=ap.parse_args()
    geo=Geometry(args.source);chosen=[];rejected=collections.Counter()
    quotas={'Temple_Complex':6,'High Castle':3,'Medium Castle':4,'Lower Castle':4,'Barbican':4}
    for group,quota in quotas.items():
        candidates=geo.floor_candidates(group)
        if group=='Temple_Complex':
            # The broad main carpet is an authored indoor floor, not a tower roof.
            candidates.sort(key=lambda c:(0 if '/Carpet/' in c['hierarchy'] else 1,np.linalg.norm(c['point'][[0,2]]-[238.86,651.5]),-c['area']))
        else:candidates.sort(key=lambda c:-c['area'])
        count=0
        for candidate in candidates:
            point=candidate['point']
            if any(np.linalg.norm((point-c['point'])[[0,2]])<3.2 and abs(point[1]-c['point'][1])<2 for c in chosen):continue
            evidence,reason=geo.check(point)
            if not evidence:rejected[group+': '+reason]+=1;continue
            candidate.update(evidence);chosen.append(candidate);count+=1
            print('RESIDENT',len(chosen),group,point.round(3).tolist(),flush=True)
            if count==quota:break
        if count<quota:print('FEWER SAFE FLOORS',group,count,'of',quota,flush=True)
    if not 16<=len(chosen)<=24:raise ValueError('Expected16–24 verified safe idle resident positions; got'+str(len(chosen)))
    rows=['# family x y z yaw scale phase endX endY endZ patrol', '# Engine-world X/Z; absolute world Y. All positions are CPU-verified source floors.']
    report=[];has_king=False
    for index,c in enumerate(chosen):
        king=not has_king and c['group']=='Temple_Complex' and '/Carpet/' in c['hierarchy']
        family=0 if king else (2,3,4)[index%3];has_king|=king
        source=c['point'];world=ORIGIN+np.array([source[0]-500,source[1]+.015,-source[2]+500])
        yaw=math.atan2(250-source[0],source[2]-600) if not king else math.pi
        scale=1.1 if king else 1.;phase=(index*.173)%1
        values=(family,*world,yaw,scale,phase,*world,0)
        rows.append('%u %.9f %.9f %.9f %.8f %.4f %.5f %.9f %.9f %.9f %u'%values)
        r={k:(v.tolist() if isinstance(v,np.ndarray) else v) for k,v in c.items()};r.update(family=family,king=king,world_position=world.tolist(),yaw=yaw,scale=scale,patrol=False);report.append(r)
    if not has_king:raise ValueError('No verified Temple carpet floor available for king')
    args.report.parent.mkdir(parents=True,exist_ok=True)
    args.report.write_text(json.dumps({'source':str(args.source),'row_count':len(report),'world_origin':ORIGIN.tolist(),'source_offset_xz':[500,500],'all_idle':True,'broddi_enabled':False,'rejected_candidates':dict(rejected),'residents':report,'clearance_limit':'LOD0 authored static triangle surfaces; thin non-solid decals excluded. Body tested by conservative overlapping spheres and9verticalsupport/headroom rays. No patrol/capsule-navigation route is implied.'},indent=2,allow_nan=False)+'\n')
    path=args.project/'showcase.residents';temporary=path.with_suffix('.residents.tmp');temporary.write_text('\n'.join(rows)+'\n');temporary.replace(path)
    print('WROTE',path,len(report),'verified idle residents',flush=True)

if __name__=='__main__':main()
