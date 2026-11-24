#!/usr/bin/env python3
"""Extend the authored castle rock hill under the full source footprint.

Preserves every castle vertex/placement and the low entrance footprints. Only
the previously added visual plinth is replaced; no runtime/project/GPU work.
"""
from pathlib import Path
import argparse,copy,hashlib,json,math
import numpy as np
from scipy.ndimage import distance_transform_edt,map_coordinates
from scipy.spatial import ConvexHull,cKDTree
from cook_village_publisher_scene import Source,ROOT,C,emit
from build_village_castle_plinth import CASTLE_TRANSLATION,Frozen

NAME='SM_City_Castle_Backdrop_Plinth'
DEFAULT_BASE=ROOT/'assets/purchased/bep_reforged/city-opening/catalog.json'
DEFAULT_OUT=ROOT/'assets/purchased/medieval_village/publisher-backdrop-supported'

def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def smooth(x):x=np.clip(x,0,1);return x*x*(3-2*x)

class Support:
    def __init__(self,source):
        v,parts,_=source.geometry('SM_Castle',0)
        p=v[:,:3]@C.T/100;p[:,1]+=CASTLE_TRANSLATION[1]
        self.hull=ConvexHull(p[:,[0,2]])
        self.center=(p[:,[0,2]].min(0)+p[:,[0,2]].max(0))/2
        self.origin=p[:,[0,2]].min(0)-5;self.step=.60
        shape=np.ceil((p[:,[0,2]].max(0)+5-self.origin)/self.step).astype(int)+1
        raw=np.full(shape[::-1],-np.inf)
        # Upper surface of low foundation geometry, rather than the old hull
        # of the few global-minimum vertices. Tall facade faces/roofs excluded.
        triangles=np.concatenate([r['indices']for r in parts])
        for q in p[triangles]:
            if q[:,1].max()>60.1:continue
            a,b,c=q[:,[0,2]];den=(b[1]-c[1])*(a[0]-c[0])+(c[0]-b[0])*(a[1]-c[1])
            if abs(den)<1e-9:continue
            mn=np.maximum(0,np.floor((q[:,[0,2]].min(0)-self.origin)/self.step).astype(int));mx=np.minimum(shape-1,np.ceil((q[:,[0,2]].max(0)-self.origin)/self.step).astype(int))
            yy,xx=np.mgrid[mn[1]:mx[1]+1,mn[0]:mx[0]+1];xx=self.origin[0]+xx*self.step;yy=self.origin[1]+yy*self.step
            w=((b[1]-c[1])*(xx-c[0])+(c[0]-b[0])*(yy-c[1]))/den
            u=((c[1]-a[1])*(xx-c[0])+(a[0]-c[0])*(yy-c[1]))/den;t=1-w-u
            valid=(w>=-1e-7)&(u>=-1e-7)&(t>=-1e-7)
            h=w*q[0,1]+u*q[1,1]+t*q[2,1];target=raw[mn[1]:mx[1]+1,mn[0]:mx[0]+1]
            np.maximum(target,np.where(valid,h,-np.inf),out=target)
        valid=np.isfinite(raw)
        if valid.sum()<500:raise ValueError('Insufficient actual foundation surface evidence')
        nearest=distance_transform_edt(~valid,return_distances=False,return_indices=True)
        self.values=np.clip(raw[nearest[0],nearest[1]]-.1,35.,58.35)
        low=np.unique(np.round(p[p[:,1]<35.5][:,[0,2]],5),axis=0)
        parents=list(range(len(low)))
        def root(i):
            while parents[i]!=i:i=parents[i]
            return i
        for a,b in cKDTree(low).query_pairs(26):parents[root(b)]=root(a)
        groups={}
        for i,q in enumerate(low):groups.setdefault(root(i),[]).append(q)
        self.gates=[ConvexHull(np.array(q))for q in groups.values()if len(q)>=3]
        # The initial lower-envelope version exposed broad under-wall voids.
        # The repaired hill is solid at the curtain-wall base everywhere, with
        # only narrow entrance recesses along the three source doorway axes.
        self.entrances=[]
        for gate in self.gates:
            center=gate.points[gate.vertices].mean(0)
            direction=center-self.center;direction/=np.linalg.norm(direction)
            angle=math.atan2(direction[1],direction[0])
            outside=self.center+direction*(self.radius(angle)+23)
            self.entrances.append((center,outside))
        self.evidence=dict(castle_bounds_native=(p+np.array([CASTLE_TRANSLATION[0],0,CASTLE_TRANSLATION[2]])).min(0).tolist(),
                           castle_max_native=(p+np.array([CASTLE_TRANSLATION[0],0,CASTLE_TRANSLATION[2]])).max(0).tolist(),
                           foundation_samples=int(valid.sum()),full_source_hull_vertices=len(self.hull.vertices),
                           low_entrance_footprints=[(g.points[g.vertices]+CASTLE_TRANSLATION[[0,2]]).tolist()for g in self.gates],
                           top_range_native_y=[35.,58.4],solid_full_footprint_top_native_y=58.4,
                           low_entrance_y=35.,entrance_half_width_m=3.25,entrance_transition_m=.75,
                           entrance_axes_native=[[(a+CASTLE_TRANSLATION[[0,2]]).tolist(),(b+CASTLE_TRANSLATION[[0,2]]).tolist()]for a,b in self.entrances],
                           source_surface_raster_step_m=self.step,
                           correction='Solid full-footprint curtain-wall support; original lower-envelope field no longer controls top height')

    def gate_distance(self,p):
        p=np.asarray(p);distance=np.full(p.shape[:-1],np.inf)
        for start,end in self.entrances:
            segment=end-start;t=np.clip(((p-start)*segment).sum(-1)/np.dot(segment,segment),0,1)
            d=np.linalg.norm(p-(start+t[...,None]*segment),axis=-1)-3.25
            distance=np.minimum(distance,np.maximum(d,0))
        return distance

    def height(self,p):
        p=np.asarray(p)
        weight=smooth(self.gate_distance(p)/.75)
        return 35.+(58.4-35.)*weight

    def radius(self,angle):
        direction=np.array([math.cos(angle),math.sin(angle)]);equations=self.hull.equations
        dots=equations[:,:2]@direction;distance=-(equations[:,:2]@self.center+equations[:,2])
        return float(np.min(distance[dots>1e-6]/dots[dots>1e-6]))+3.4

    def generate(self,source,near):
        segments=160 if near else 96;cap_rings=28 if near else 17
        rings=[];uvs=[]
        for t in np.linspace(0,1,cap_rings)[1:]:
            angle=np.linspace(0,2*math.pi,segments+1);xz=np.array([self.center+np.array([math.cos(a),math.sin(a)])*self.radius(a)*t for a in angle])
            row=np.column_stack((xz[:,0],self.height(xz),xz[:,1]));rings.append(row);uvs.append(xz/2.6)
        # Source-like stepped ledges extend down into the new countryside ridge.
        fractions=[.08,.18,.29,.40,.51,.63,.76,.88,1.]
        drops=[1/35.6,5/35.6,6/35.6,12/35.6,13/35.6,20/35.6,25/35.6,32/35.6,1.]
        for k,(t,drop)in enumerate(zip(fractions,drops)):
            row=[];uv=[]
            for j in range(segments+1):
                a=j/segments*2*math.pi;r=self.radius(a);width=20+3*math.sin(a*3+.7)+2*math.cos(a*5)
                ripple=(math.sin(a*9+k*.43)+.5*math.sin(a*17-k*.28))*math.sin(math.pi*t)*1.25
                xz=self.center+np.array([math.cos(a),math.sin(a)])*(r+width*t+ripple)
                top=float(self.height(self.center+np.array([math.cos(a),math.sin(a)])*r))
                y=top-(top+.6)*drop+math.sin(math.pi*t)*(.9*math.sin(a*7+k*.5)+.5*math.sin(a*13-k*.3))
                row.append([xz[0],y,xz[1]]);uv.append([a*r/2.6,y/2.6])
            rings.append(np.array(row));uvs.append(np.array(uv))
        positions=[];tex=[];center=np.r_[self.center[0],float(self.height(self.center)),self.center[1]]
        for j in range(segments):
            q=np.array([center,rings[0][j+1],rings[0][j]])
            if np.cross(q[1]-q[0],q[2]-q[0])[1]<0:q=q[::-1]
            positions.extend(q);tex.extend(q[:,[0,2]]/2.6)
        for k in range(len(rings)-1):
            for j in range(segments):
                for corners in[[(k,j),(k+1,j),(k+1,j+1)],[(k,j),(k+1,j+1),(k,j+1)]]:
                    q=np.array([rings[a][b]for a,b in corners]);uv=np.array([uvs[a][b]for a,b in corners])
                    if k<cap_rings-2:
                        wrong=np.cross(q[1]-q[0],q[2]-q[0])[1]<0
                    else:
                        radial=q.mean(0)-[self.center[0],0,self.center[1]];radial[1]=0
                        wrong=np.dot(np.cross(q[1]-q[0],q[2]-q[0]),radial)<0
                    if wrong:q=q[::-1];uv=uv[::-1]
                    positions.extend(q);tex.extend(uv)
        p=np.asarray(positions);uv=np.asarray(tex);faces=p.reshape(-1,3,3)
        normal=np.cross(faces[:,1]-faces[:,0],faces[:,2]-faces[:,0]);normal/=np.maximum(np.linalg.norm(normal,axis=1)[:,None],1e-20);normal=np.repeat(normal,3,axis=0)
        tangent=np.cross(np.tile([0,1,0.],(len(p),1)),normal);flat=np.linalg.norm(tangent,axis=1)<1e-8;tangent[flat]=[1,0,0];tangent/=np.maximum(np.linalg.norm(tangent,axis=1)[:,None],1e-20)
        v=np.zeros((len(p),18),np.float32);v[:,:3]=p@C*100;v[:,3:6]=normal@C;v[:,6:8]=uv;v[:,8:10]=uv;v[:,10:13]=tangent@C;v[:,13]=-1;v[:,17]=1
        family='SM_City_Castle_Full_Footprint_Support';lod=0 if near else 1
        row=dict(name=family+'_LOD'+str(lod),vertices='generated:'+str(lod),source_file='Actual full source castle foundation envelope; new rocky support')
        source.groups[family][lod]=row;source.derived[family,lod]=(v,[dict(slot=0,material='MI_Statue_Rock',source_material='MI_Statue_Rock',indices=np.arange(len(v),dtype=np.uint32).reshape(-1,3))],row)
        return family


def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--base',type=Path,default=DEFAULT_BASE);ap.add_argument('--output',type=Path,default=DEFAULT_OUT);ap.add_argument('--apply-base',action='store_true');args=ap.parse_args()
    base=json.loads(args.base.read_text());prior_sha=sha(args.base);source=Source();profile=Support(source)
    family=profile.generate(source,True);profile.generate(source,False)
    pieces=[dict(family=family,variant=None,matrix=np.eye(4),override=[])];rng=np.random.default_rng(42051);rockaudit=[]
    for tier,count in enumerate([30,28,32]):
        for j in range(count):
            angle=2*math.pi*(j+rng.uniform(-.23,.23))/count;r=profile.radius(angle)+[3.5,11.5,20][tier]+rng.uniform(-1.6,1.6);xz=profile.center+[math.cos(angle)*r,math.sin(angle)*r]
            rock=['SM_Rock_01','SM_Rock_02','SM_Rock_03'][(j+tier)%3];v,_,_=source.geometry(rock,0);native=v[:,:3]@C.T/100;extent=np.ptp(native,axis=0);dimensions=np.array([rng.uniform(9,15),rng.uniform(7,11),rng.uniform(10,17)])
            if tier==2:dimensions*=.78
            scale=dimensions/np.maximum(extent,.01);yaw=angle+math.pi/2+rng.uniform(-.4,.4);cy,sy=math.cos(yaw),math.sin(yaw);rotation=np.array([[cy,0,sy],[0,1,0],[-sy,0,cy]])
            y=float(profile.height(profile.center+np.array([math.cos(angle),math.sin(angle)])*profile.radius(angle)))-[5,17,32][tier]+rng.uniform(-1.7,1.7)
            # No decorative boulder blocks one of the recovered low entrances.
            if tier==0 and profile.gate_distance(xz)<7:continue
            m=np.eye(4);m[:3,:3]=rotation@np.diag(scale);m[:3,3]=[xz[0],y,xz[1]]
            density=float(math.sqrt(scale[0]*scale[2]));pieces.append(dict(family=rock,variant=None,matrix=m,override=[],uv_density_scale=density));rockaudit.append(dict(source_mesh=rock,tier=tier,source_native_position=m[:3,3].tolist(),scale=scale.tolist(),uv_density_scale=density))
    miny=min(float(((source.geometry(p['family'],lod)[0][:,:3]@C.T/100)@p['matrix'][:3,:3].T+p['matrix'][:3,3])[:,1].min())for p in pieces for lod in[0,1])
    args.output.mkdir(parents=True,exist_ok=True);files=[];stats=[]
    for label,lod in[('near',0),('mid',1)]:
        path=args.output/'models'/(NAME+'_'+label+'.glb')
        if path.exists():raise ValueError('Immutable support candidate already exists: '+str(path))
        stats.append(emit(path,pieces,source,lod,Frozen(),miny));files.append(str(path.resolve()))
    old=next(a for a in base['assets']if a['name']==NAME);asset=copy.deepcopy(old)
    asset.update(files=files,pivot_offset_native=[0,miny,0],bounds=stats[0]['bounds'],stats=stats,
                 provenance='New full-footprint source-foundation envelope plus owned source boulders; exact original rock PBR; original low entrance floors preserved')
    result=copy.deepcopy(base);result['assets']=[asset if a['name']==NAME else a for a in result['assets']]
    prior_revision=base.get('authored_support_revision',{})
    previous_asset_hash=hashlib.sha256(json.dumps(old,sort_keys=True,separators=(',',':')).encode()).hexdigest()
    revision=dict(type='Explicit repair of previously authored supporting hill',asset=NAME,
                  previous_asset_sha256=prior_revision.get('previous_asset_sha256',previous_asset_hash),
                  previous_model_hashes=prior_revision.get('previous_model_hashes',[r['sha256']for r in old['stats']]),
                  replaces_asset_sha256=previous_asset_hash,replaces_model_hashes=[r['sha256']for r in old['stats']],new_model_hashes=[r['sha256']for r in stats],
                  original_castle_transform_unchanged=True,original_city_geometry_unchanged=True,collision=False,walkable=False,
                  source_geometry_evidence=profile.evidence,rocks=len(rockaudit),source_base_catalog=str(args.base.resolve()),source_base_catalog_sha256=prior_sha)
    result['authored_support_revision']=revision;result['new_background']=copy.deepcopy(result['new_background'])
    result['new_background'].update(castle_support_top_source_y=[35,58.4],rocks=rockaudit,plinth_bounds_local=stats[0]['bounds'],support_revision=revision)
    if result['placements']!=base['placements']:raise ValueError('Support repair changed a placement')
    if [a for a in result['assets']if a['name']!=NAME]!=[a for a in base['assets']if a['name']!=NAME]:raise ValueError('Support repair changed another asset')
    (args.output/'catalog.json').write_text(json.dumps(result,indent=2,allow_nan=False)+'\n')
    (args.output/'base-before.json').write_text(json.dumps(base,indent=2,allow_nan=False)+'\n')
    (args.output/'support-revision.json').write_text(json.dumps(revision,indent=2,allow_nan=False)+'\n')
    np.savez_compressed(args.output/'support-profile.npz',base_profile=profile.values,origin=profile.origin,step=profile.step)
    if args.apply_base:
        if sha(args.base)!=prior_sha:raise ValueError('Base catalog changed during cook')
        temporary=args.base.with_suffix('.json.tmp');temporary.write_text(json.dumps(result,indent=2,allow_nan=False)+'\n');temporary.replace(args.base)
    print(json.dumps(dict(status='COOKED',catalog=str(args.output/'catalog.json'),base_updated=args.apply_base,stats=stats,revision=revision),indent=2),flush=True)

if __name__=='__main__':main()
