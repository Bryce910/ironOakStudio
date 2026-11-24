#!/usr/bin/env python3
"""Ground the original city boundary assembly without moving its architecture.

New authored rock footings use the installed composed terrain and retain a
generous arched passage wherever the proven road crosses the support. CPU/data
only; project installation and GPU review are separate.
"""
from pathlib import Path
import argparse,copy,hashlib,json,math,struct
import numpy as np
from scipy.ndimage import map_coordinates
from scipy.spatial import ConvexHull,cKDTree
from cook_village_publisher_scene import Source,ROOT,C,emit
from build_village_castle_plinth import Frozen

NAME='SM_City_Boundary_Rock_Foundations'
ROCK_MATERIAL='BEP_City_Boundary_Tileable_Rock'
BASE=ROOT/'assets/purchased/bep_reforged/city-opening/catalog.json'
OUT=ROOT/'assets/purchased/medieval_village/boundary-foundations'
PROJECT=ROOT/'projects/valwood_city_showcase'
COUNTRY=ROOT/'assets/purchased/bep_reforged/countryside'

def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def smooth(x):x=np.clip(x,0,1);return x*x*(3-2*x)
def readfield(path):
    raw=Path(path).read_bytes();magic,w,h,x,z,dx,dz=struct.unpack_from('<8sIIdddd',raw)
    if magic!=b'VWHGT1\0\0':raise ValueError('Invalid composed source field')
    return np.frombuffer(raw,'<f4',offset=48).reshape(h,w),np.array([x,z]),np.array([dx,dz])

class Ground:
    def __init__(self):
        self.origin=np.array(json.loads((PROJECT/'city_showcase.json').read_text())['origin'])
        self.city=readfield(PROJECT/'ground/showcase.height');self.country=readfield(COUNTRY/'ground/countryside.height')
        road=np.load(COUNTRY/'landscape-review.npz')['road_native']
        entry=np.array([[12,-5],[-14.5,-5.75],[-48.775606536865,5.75],road[0]])
        points=[]
        for a,b in zip(entry,entry[1:]):
            n=max(1,math.ceil(np.linalg.norm(b-a)/.2));points.extend(a+(b-a)*np.arange(n)[:,None]/n)
        self.road=np.concatenate((np.asarray(points),road));self.road_tree=cKDTree(self.road);self.road_y=self.height(self.road)
    def height(self,p):
        p=np.asarray(p);shape=p.shape[:-1];flat=p.reshape(-1,2)+self.origin[[0,2]]
        output=np.zeros(len(flat));assigned=np.zeros(len(flat),bool)
        for values,origin,step in[self.city,self.country]:
            uv=(flat-origin)/step;inside=(uv>=0).all(1)&(uv<=np.array(values.shape)[::-1]-1).all(1)&~assigned
            output[inside]=map_coordinates(values,np.array([uv[inside,1],uv[inside,0]]),order=1,mode='nearest');assigned|=inside
        if not assigned.all():raise ValueError('Foundation extends outside composed ground')
        return (output-self.origin[1]).reshape(shape)

def foundation(source,placement,ground,spacing):
    v,_,_=source.geometry(placement['asset'],0);m=np.array(placement['matrix_native']).reshape(4,4).T
    p=(v[:,:3]@C.T/100)@m[:3,:3].T+m[:3,3];base=float(p[:,1].min());hull=ConvexHull(p[:,[0,2]])
    lo=p[:,[0,2]].min(0)-4;hi=p[:,[0,2]].max(0)+4
    nx,nz=np.ceil((hi-lo)/spacing).astype(int)+1;x=np.linspace(lo[0],hi[0],nx);z=np.linspace(lo[1],hi[1],nz)
    xx,zz=np.meshgrid(x,z);q=np.stack((xx,zz),-1);g=ground.height(q)
    signed=np.max(q@hull.equations[:,:2].T+hull.equations[:,2],axis=-1)
    # A narrow stepped skirt provides visible stone support, with its toe buried
    # below the composed field; no flat under-wall void is left behind.
    # Both source tiers use the same fine support grid. A contact apron wider
    # than its1.1m-cell diagonal keeps interpolation beneath the actual wall
    # hull level, rather than lowering the wall corners into the outer slope.
    w=1-smooth(np.maximum(signed-1.65,0)/(3.5-1.65))
    top=g-.3+(base+.12-(g-.3))*w
    # Keep a buried slab beneath the level cap even where terrain crosses the
    # wall base inside one grid cell. Otherwise rejecting that whole cell
    # exposes a small corner gap on the lower side of the terrain transition.
    bottom=np.minimum(g-.6,base-.6)
    dist,nearest=ground.road_tree.query(q.reshape(-1,2));dist=dist.reshape(q.shape[:-1]);road_y=ground.road_y[nearest].reshape(dist.shape)
    portal=dist<4.2;arch=road_y+3.+np.sqrt(np.maximum(0,4.2**2-dist**2))
    bottom=np.where(portal,np.maximum(bottom,arch),bottom)
    # Only cells enclosing positive solid volume are emitted. Near the road,
    # this opens the full ground-to-arch volume, including the portal mouths.
    valid=(signed[:-1,:-1]+signed[1:,:-1]+signed[:-1,1:]+signed[1:,1:])/4<3.5
    for dz,dx in[(0,0),(0,1),(1,0),(1,1)]:valid&=(top[dz:dz+nz-1,dx:dx+nx-1]-bottom[dz:dz+nz-1,dx:dx+nx-1])>.15
    T=np.stack((xx,top,zz),-1);B=np.stack((xx,bottom,zz),-1);triangles=[]
    for iz,ix in np.argwhere(valid):
        a,b,c,d=T[iz,ix],T[iz,ix+1],T[iz+1,ix],T[iz+1,ix+1]
        aa,bb,cc,dd=B[iz,ix],B[iz,ix+1],B[iz+1,ix],B[iz+1,ix+1]
        triangles.extend([[a,c,b],[b,c,d],[aa,bb,cc],[bb,dd,cc]])
        for neighbor,topedge,botedge in[
            ((iz-1,ix),(b,a),(bb,aa)),((iz+1,ix),(c,d),(cc,dd)),
            ((iz,ix-1),(a,c),(aa,cc)),((iz,ix+1),(d,b),(dd,bb))]:
            rz,rx=neighbor
            if not(0<=rz<nz-1 and 0<=rx<nx-1 and valid[rz,rx]):
                u,v=topedge;uu,vv=botedge;triangles.extend([[u,uu,v],[v,uu,vv]])
    positions=np.asarray(triangles,dtype=np.float32).reshape(-1,3)
    if not len(positions):
        return np.zeros((0,18),np.float32),dict(component=placement.get('source_component'),actor=placement.get('actor_label'),asset=placement['asset'],wall_base_native_y=base,
            source_footprint=hull.points[hull.vertices].tolist(),portal_cut=bool((portal&(signed<1)).any()),triangles=0,already_ground_supported_or_road_clear=True)
    # Use a repeating rock material, never the source boulders' UV atlas.
    # The tangent frame follows these exact UV derivatives; the final writer
    # flips V and the tangent handedness once together.
    faces=positions.reshape(-1,3,3);norm=np.cross(faces[:,1]-faces[:,0],faces[:,2]-faces[:,0]);norm/=np.maximum(np.linalg.norm(norm,axis=1)[:,None],1e-20)
    uv=np.empty((len(faces),3,2),np.float32)
    for i,(face,n)in enumerate(zip(faces,norm)):
        axis=int(np.argmax(abs(n)));axes={0:(2,1),1:(0,2),2:(0,1)}[axis];uv[i]=face[:,axes]/7.
    e1=faces[:,1]-faces[:,0];e2=faces[:,2]-faces[:,0];a=uv[:,1]-uv[:,0];b=uv[:,2]-uv[:,0];det=a[:,0]*b[:,1]-a[:,1]*b[:,0]
    if np.min(abs(det),initial=1)<1e-10:raise ValueError('Degenerate generated foundation UVs')
    tangent=(e1*b[:,1,None]-e2*a[:,1,None])/det[:,None];bitangent=(e2*a[:,0,None]-e1*b[:,0,None])/det[:,None]
    tangent-=norm*np.sum(norm*tangent,axis=1)[:,None];tangent/=np.linalg.norm(tangent,axis=1)[:,None]
    handedness=np.where(np.sum(np.cross(norm,tangent)*bitangent,axis=1)<0,-1.,1.)
    normal=np.repeat(norm,3,axis=0);tangent=np.repeat(tangent,3,axis=0)
    result=np.zeros((len(positions),18),np.float32);result[:,:3]=positions@C*100;result[:,3:6]=normal@C;result[:,6:8]=uv.reshape(-1,2);result[:,8:10]=uv.reshape(-1,2);result[:,10:13]=tangent@C;result[:,13]=np.repeat(handedness,3);result[:,17]=1
    dist_to_hull=np.maximum(signed,0);nearroad=bool((portal&(signed<1)).any())
    return result,dict(component=placement.get('source_component'),actor=placement.get('actor_label'),asset=placement['asset'],wall_base_native_y=base,
                       source_footprint=hull.points[hull.vertices].tolist(),portal_cut=nearroad,triangles=len(faces),bounds_native=[positions.min(0).tolist(),positions.max(0).tolist()])

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--base',type=Path,default=BASE);ap.add_argument('--output',type=Path,default=OUT);args=ap.parse_args()
    base=json.loads(args.base.read_text());before_hash=sha(args.base);source=Source();ground=Ground();boundary=[p for p in base['placements']if p['asset'].startswith('SM_Boundary_')]
    if len(boundary)!=40:raise ValueError('Unexpected original boundary source count')
    assets=[];audits=[];collision_native=None
    family='Generated_Source_Boundary_Foundations'
    for lod,spacing in[(0,1.1),(1,1.1)]:
        rows=[];audit=[]
        for placement in boundary:
            v,row=foundation(source,placement,ground,spacing);rows.append(v);audit.append(row)
        vertices=np.concatenate(rows);record=dict(name=family+'_LOD'+str(lod),vertices='generated:'+str(lod),source_file='Original source boundary footprint rock supports; new authored extension')
        source.groups[family][lod]=record;source.derived[family,lod]=(vertices,[dict(slot=0,material=ROCK_MATERIAL,source_material=ROCK_MATERIAL,indices=np.arange(len(vertices),dtype=np.uint32).reshape(-1,3))],record)
        if lod==0:audits=audit
        else:collision_native=vertices[:,:3]@C.T/100
    pieces=[dict(family=family,variant=None,matrix=np.eye(4),override=[])];rng=np.random.default_rng(211024);rubble=[]
    for i,row in enumerate(audits):
        if not row['triangles']:continue
        points=np.asarray(row['source_footprint']);center=points.mean(0)
        for j in[0,len(points)//2]:
            edge=points[j];direction=edge-center;direction/=max(np.linalg.norm(direction),1e-6);xz=edge+direction*2.1
            if ground.road_tree.query(xz)[0]<8:continue
            rock=['SM_Rock_01','SM_Rock_02','SM_Rock_03'][(i+j)%3];v,_,_=source.geometry(rock,0);extent=np.ptp(v[:,:3]@C.T/100,axis=0);size=np.array([rng.uniform(2.4,4.2),rng.uniform(1.7,3.0),rng.uniform(2.5,4.0)]);scale=size/extent
            yaw=rng.uniform(0,2*math.pi);cy,sy=math.cos(yaw),math.sin(yaw);m=np.eye(4);m[:3,:3]=np.array([[cy,0,sy],[0,1,0],[-sy,0,cy]])@np.diag(scale);m[:3,3]=[xz[0],float(ground.height(xz))+.7,xz[1]]
            pieces.append(dict(family=rock,variant=None,matrix=m,override=[],uv_density_scale=1.));rubble.append(dict(source_mesh=rock,position=m[:3,3].tolist(),scale=scale.tolist()))
    minimum=min(float(((source.geometry(p['family'],lod)[0][:,:3]@C.T/100)@p['matrix'][:3,:3].T+p['matrix'][:3,3])[:,1].min())for p in pieces for lod in[0,1]);miny=math.floor(minimum)-1
    args.output.mkdir(parents=True,exist_ok=True);files=[];stats=[]
    frozen=Frozen();frozen.rows[ROCK_MATERIAL]=dict(json.loads((ROOT/'assets/purchased/bep_reforged/countryside-materials/ground-materials.json').read_text())['rocky'],name=ROCK_MATERIAL)
    for label,lod in[('near',0),('mid',1)]:
        path=args.output/'models'/(NAME+'_'+label+'.glb')
        if path.exists():raise ValueError('Immutable foundation candidate exists')
        stats.append(emit(path,pieces,source,lod,frozen,miny));files.append(str(path.resolve()))
    collision_native[:,1]-=miny;unique,inverse=np.unique(np.round(collision_native,5),axis=0,return_inverse=True);triangles=inverse.reshape(-1,3)
    if len(unique)>40000 or len(triangles)>65000:raise ValueError('Foundation collision proxy exceeds remaining native budget')
    asset=dict(name=NAME,files=files,stats=stats,bounds=stats[0]['bounds'],pivot_offset_native=[0,miny,0],collision=dict(vertices=unique.tolist(),triangles=triangles.tolist()),
               native=dict(alias='nm_city_boundary_rock_foundations',vegetation=0,collision=0,distance=1300,split=65),
               authored_extension='New natural foundations beneath unchanged original boundary architecture',source_families=['Original boundary source footprints',ROCK_MATERIAL,'MI_Statue_Rock','SM_Rock_01','SM_Rock_02','SM_Rock_03'])
    result=copy.deepcopy(base)
    if any(a['name']==NAME for a in result['assets']):raise ValueError('Foundation extension already installed in base')
    result['assets'].append(asset);result['placements'].append(dict(asset=NAME,matrix_native=np.eye(4).T.flatten().tolist(),authored_extension='Grounded source boundary foundations with road arches'))
    result['boundary_foundations']=dict(asset=NAME,source_boundary_components=[r['component']for r in audits],supports=audits,toe_rubble=len(rubble),
        source_base_catalog=str(args.base.resolve()),source_base_catalog_sha256=before_hash,ground_fields=dict(primary_sha256=sha(PROJECT/'ground/showcase.height'),countryside_sha256=sha(COUNTRY/'ground/countryside.height')),
        road_preservation='Global proven-road corridor:8.4mwide portal,3mminimum leg headroom/7.2mcrown; no road or field edits',
        contact_apron_metres=1.65,support_grid_metres=1.1,same_contact_geometry_both_lods=True,
        portal_components=[r['component']for r in audits if r['portal_cut']],proxy_vertices=len(unique),proxy_triangles=len(triangles),original_architecture_transforms_unchanged=True)
    if result['assets'][:-1]!=base['assets']or result['placements'][:-1]!=base['placements']:raise ValueError('Original scene changed')
    (args.output/'base-before.json').write_text(json.dumps(base,indent=2)+'\n');(args.output/'catalog.json').write_text(json.dumps(result,indent=2,allow_nan=False)+'\n')
    (args.output/'foundation-audit.json').write_text(json.dumps(result['boundary_foundations'],indent=2)+'\n')
    print(json.dumps(dict(status='COOKED',catalog=str(args.output/'catalog.json'),definitions=len(result['assets']),placements=len(result['placements']),stats=stats,portal_components=result['boundary_foundations']['portal_components'],proxy_vertices=len(unique),proxy_triangles=len(triangles)),indent=2),flush=True)

if __name__=='__main__':main()
