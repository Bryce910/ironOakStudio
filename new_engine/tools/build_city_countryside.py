#!/usr/bin/env python3
"""Stage an owned BEP landscape around the immutable publisher city.

Uses the author's Isle_01 sculpt, aligned between Forest_Pass and our castle.
City ground remains the first-priority height service. New terrain and dressing
are explicitly authored extensions, not claimed original Unity placements.
"""
from pathlib import Path
import argparse, copy, hashlib, json, math, struct, tempfile
import numpy as np
from scipy.ndimage import map_coordinates, gaussian_filter1d
from scipy.interpolate import RBFInterpolator, CubicSpline
from scipy.spatial import cKDTree
from scipy.spatial.distance import cdist
from cook_village_publisher_scene import material_writer

ROOT=Path(__file__).resolve().parents[1]
PRIVATE=ROOT.parent/'.local-tools/bep-source'
OUT=ROOT/'assets/purchased/bep_reforged/countryside'
PROJECT=ROOT/'projects/valwood_city_showcase'
BOUNDS=(-220.,-180.,220.,280.)
SOURCE_A=np.array([727.6,773.7]);SOURCE_B=np.array([889.2,937.6])
TARGET_A=np.array([-85.,-5.]);TARGET_B=np.array([-134.5,202.6])
ANGLE=math.atan2(*(TARGET_B-TARGET_A)[::-1])-math.atan2(*(SOURCE_B-SOURCE_A)[::-1])
ROT=np.array([[math.cos(ANGLE),-math.sin(ANGLE)],[math.sin(ANGLE),math.cos(ANGLE)]])
SCALE=np.linalg.norm(TARGET_B-TARGET_A)/np.linalg.norm(SOURCE_B-SOURCE_A)
# Adapt the source island into rolling countryside rather than retaining its
# mountain-scale relief around a road whose longitudinal grade is limited.
SCULPT_VERTICAL_SCALE=.18
ROAD_HALF_WIDTH=3.2
ROAD_SHOULDER_WIDTH=18.0
CITY_ROAD_TRANSITION_METRES=12.0

def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def readfield(path):
    data=Path(path).read_bytes();magic,w,h,x,z,dx,dz=struct.unpack_from('<8sIIdddd',data)
    if magic!=b'VWHGT1\0\0':raise ValueError('Expected native immutable height field')
    return np.frombuffer(data,'<f4',offset=48).reshape(h,w),np.array([x,z]),np.array([dx,dz])
def field(path,values,origin,step):
    Path(path).parent.mkdir(parents=True,exist_ok=True)
    Path(path).write_bytes(struct.pack('<8sIIdddd',b'VWHGT1\0\0',values.shape[1],values.shape[0],*origin,*step)+np.ascontiguousarray(values,dtype='<f4').tobytes())
def sample(values,points,origin,step):
    uv=(np.asarray(points)-origin)/step
    flat=uv.reshape(-1,2)
    return map_coordinates(values,np.array([flat[:,1],flat[:,0]]),order=1,mode='nearest').reshape(uv.shape[:-1])
def source_points(native):return (np.asarray(native)-TARGET_A)@ROT/SCALE+SOURCE_A
def native_points(source):return (np.asarray(source)-SOURCE_A)@ROT.T*SCALE+TARGET_A
def smooth(x):x=np.clip(x,0,1);return x*x*(3-2*x)

class Landscape:
    def __init__(self,base,city_height_path=None):
        self.base=base;self.origin=np.asarray(json.loads((PROJECT/'city_showcase.json').read_text())['origin'])
        self.city,self.city_origin,self.city_step=readfield(city_height_path or PROJECT/'ground/showcase.height')
        self.city_coverage,_,_=readfield(PROJECT/'ground/city.coverage')
        self.lo=self.city_origin-self.origin[[0,2]];self.hi=self.lo+self.city_step*(np.array(self.city.shape)[::-1]-1)
        self.source=np.load(PRIVATE/'terrain-Isle_01.npz');self.source_h=self.source['height_meters_zx']
        self.pads=[]
        self.residential_pads=base.get('residential_repair',{}).get('pads',[])
        self.castle_support=None
        assets={a['name']:a for a in base['assets']}
        for placement in base['placements']:
            if placement['asset']!='SM_Castle':continue
            asset=assets['SM_Castle'];m=np.asarray(placement['matrix_native']).reshape(4,4).T.copy();m[:3,3]+=m[:3,:3]@np.asarray(asset['pivot_offset_native'])
            lo,hi=np.asarray(asset['bounds']);corners=np.array([[x,y,z,1]for x in [lo[0],hi[0]]for y in [lo[1],hi[1]]for z in [lo[2],hi[2]]])@m.T
            self.castle_support=(corners[:,[0,2]].min(0),corners[:,[0,2]].max(0))
            break
        self.anchor=float(sample(self.source_h,SOURCE_A,np.zeros(2),np.array([1700/2048]*2)))
        # Fit only low-frequency residuals to existing building foundations.
        points=[];heights=[];assets={a['name']:a for a in base['assets']}
        for p in base['placements']:
            if not p['asset'].startswith('SM_House_') or 'Light'in p['asset']:continue
            a=assets[p['asset']];m=np.asarray(p['matrix_native']).reshape(4,4).T.copy();m[:3,3]+=m[:3,:3]@np.asarray(a['pivot_offset_native'])
            low,high=np.asarray(a['bounds']);center=(low+high)/2;center[1]=low[1];v=m@np.r_[center,1]
            support_height=v[1]+self.origin[1]-.025
            # The reviewed relocation is placement index104, whose immutable
            # publisher component ID is1605 (House_Two19).
            if p.get('source_component')==1605:
                original=json.loads((ROOT/'assets/purchased/medieval_village/publisher-backdrop/catalog.json').read_text())
                old=next(q for q in original['placements']if q.get('source_component')==1605);om=np.asarray(old['matrix_native']).reshape(4,4).T.copy();om[:3,3]+=om[:3,:3]@np.asarray(a['pivot_offset_native'])
                old_center=(om@np.r_[center,1])[[0,2]]
                corners=np.array([[x,y,z,1]for x in [low[0],high[0]]for y in [low[1],high[1]]for z in [low[2],high[2]]])@m.T
                support_height=self.city_at(old_center)
                self.pads.append((corners[:,[0,2]].min(0),corners[:,[0,2]].max(0),support_height))
            if np.all(v[[0,2]]>=self.lo)&np.all(v[[0,2]]<=self.hi):continue
            points.append(v[[0,2]]);heights.append(support_height)
        # Border anchors keep the exact city edge without resampling its field.
        for axis in range(2):
            for edge in [self.lo[axis],self.hi[axis]]:
                for t in np.linspace(self.lo[1-axis],self.hi[1-axis],13):
                    p=np.zeros(2);p[axis]=edge;p[1-axis]=t;points.append(p);heights.append(self.city_at(p))
        points=np.asarray(points);heights=np.asarray(heights)
        unique,indices=np.unique(np.round(points,4),axis=0,return_index=True)
        self.support_tree=cKDTree(unique)
        self.residual=RBFInterpolator(unique,(heights-self.sculpt(points))[indices],kernel='thin_plate_spline',smoothing=10)
        # Curving woodland loop; first endpoint is joined to the validated exit.
        controls=np.array([[-83,5.25],[-105,-12],[-145,-35],[-178,-15],[-177,30],[-145,58],[-109,55],[-90,30],[-83,5.25]],float)
        t=np.r_[0,np.cumsum(np.linalg.norm(np.diff(controls,axis=0),axis=1))]
        loop=CubicSpline(t,controls,axis=0,bc_type='periodic')(np.linspace(0,t[-1],1080))
        self.road=np.concatenate((np.column_stack((np.linspace(-53,-83,121),np.full(121,5.25))),loop[1:]))
        self.road_tree=cKDTree(self.road)
        h=gaussian_filter1d(self.unrouted(self.road),12)
        # Longitudinal support avoids small sculpt noise becoming steep steps.
        for _ in range(3):
            for order in [range(1,len(h)),range(len(h)-2,-1,-1)]:
                for i in order:
                    j=i-1 if isinstance(order,range)and order.step>0 else i+1
                    delta=np.linalg.norm(self.road[i]-self.road[j])*.20;h[i]=np.clip(h[i],h[j]-delta,h[j]+delta)
        # The closed loop returns to the spur in world space. Both duplicate
        # junction positions must have one height, not two smoothed endpoints.
        junction=(h[120]+h[-1])*.5;distance=np.linalg.norm(self.road-np.array([-83,5.25]),axis=1);weight=smooth(distance/8)
        h=h*weight+junction*(1-weight)
        # Nearby return/spur segments must also agree in space: limiting only
        # distance along the loop leaves a deep cut beside a higher segment.
        # Both envelopes are20%-grade functions; their midpoint retains that
        # bound and gives coincident junction positions the exact same level.
        reach=cdist(self.road,self.road)*.20
        h=(np.min(h[None,:]+reach,axis=1)+np.max(h[None,:]-reach,axis=1))*.5
        self.road_h=h
    def city_at(self,p):return float(sample(self.city,np.asarray(p)+self.origin[[0,2]],self.city_origin,self.city_step))
    def sculpt(self,p):return self.origin[1]+(sample(self.source_h,source_points(p),np.zeros(2),np.array([1700/2048]*2))-self.anchor)*SCULPT_VERTICAL_SCALE
    def unrouted(self,p):
        p=np.asarray(p);shape=p.shape[:-1];flat=p.reshape(-1,2);h=self.sculpt(flat)
        if hasattr(self,'residual'):
            distance,_=self.support_tree.query(flat)
            correction=np.clip(self.residual(flat).reshape(-1),-15,35)*np.exp(-(distance/70)**2)
            h+=correction
        h=np.maximum(h,5.0)
        nearest=np.minimum(np.maximum(flat,self.lo),self.hi);distance=np.linalg.norm(flat-nearest,axis=1)
        city=sample(self.city,nearest+self.origin[[0,2]],self.city_origin,self.city_step)
        h=city+(h-city)*smooth(distance/28)
        for lo,hi,level in self.pads:
            nearest=np.minimum(np.maximum(flat,lo),hi);dist=np.linalg.norm(flat-nearest,axis=1);weight=1-smooth(dist/5)
            h=h*(1-weight)+level*weight
        # The owned background castle already has a detailed supporting plinth.
        castle=np.linalg.norm((flat-TARGET_B)/np.array([65.,65.]),axis=1);h=np.minimum(h,self.origin[1]+25+smooth((castle-1)/.5)*100)
        if self.castle_support is not None:
            # Fill beneath the entire castle/plinth, including its high rear
            # facade. The separate rock asset supplies the detailed upper mass.
            # Keep this northern ridge entirely away from the city and loop;
            # their existing height samples remain bit-identical belowZ85.
            lo,hi=self.castle_support;nearest=np.minimum(np.maximum(flat,lo),hi);distance=np.linalg.norm(flat-nearest,axis=1)
            weight=(1-smooth(distance/55))*smooth((flat[:,1]-85)/40)
            h+=np.maximum(self.origin[1]+25-h,0)*weight
        return h.reshape(shape)
    def height(self,p):
        p=np.asarray(p);h=self.unrouted(p);dist,idx=self.road_tree.query(p.reshape(-1,2));blend=1-smooth((dist-ROAD_HALF_WIDTH)/ROAD_SHOULDER_WIDTH)
        routed=h.reshape(-1)*(1-blend)+self.road_h[idx]*blend
        # Road grading must not undo the existing town-edge match. Otherwise
        # the exact inside override creates a steep lip just outside paving.
        # Finish both the visual and collision surface with the same C1 ramp.
        flat=p.reshape(-1,2);nearest=np.minimum(np.maximum(flat,self.lo),self.hi)
        distance=np.linalg.norm(flat-nearest,axis=1)
        city=sample(self.city,nearest+self.origin[[0,2]],self.city_origin,self.city_step)
        routed=city+(routed-city)*smooth(distance/CITY_ROAD_TRANSITION_METRES)
        if self.residential_pads:
            # Separate plots own their level pads. Choose the nearest pad so
            # a neighboring terrace cannot tilt an already level foundation.
            distances=[]
            for pad in self.residential_pads:
                lo=np.asarray(pad['native_lo']);hi=np.asarray(pad['native_hi'])
                distances.append(np.linalg.norm(flat-np.minimum(np.maximum(flat,lo),hi),axis=1))
            weights=1-smooth(np.asarray(distances)/4.)
            total=weights.sum(axis=0);weight=np.minimum(total,1.)
            levels=np.asarray([q['support_world_y']for q in self.residential_pads])
            target=(weights*levels[:,None]).sum(axis=0)/np.maximum(total,1e-20)
            routed=routed*(1-weight)+target*weight
        return routed.reshape(p.shape[:-1])

def stitched_grid(x,z,land):
    """Keep interior triangles, splitting only edges onto a common LOD rim."""
    xx,zz=np.meshgrid(x,z);points=np.column_stack((xx.ravel(),zz.ravel())).tolist()
    ids=np.arange(len(points)).reshape(len(z),len(x));a=ids[:-1,:-1].ravel();b=ids[:-1,1:].ravel();c=ids[1:,:-1].ravel();d=ids[1:,1:].ravel()
    tri=np.concatenate((np.stack((a,c,b),1),np.stack((b,c,d),1)))
    lookup={tuple(np.round(q,9)):i for i,q in enumerate(points)}
    for axis,bound,values in [(0,x[0],z),(0,x[-1],z),(1,z[0],x),(1,z[-1],x)]:
        other=1-axis;lo,hi=values[0],values[-1]
        common=np.unique(np.r_[np.linspace(lo,hi,129),np.linspace(lo,hi,49),[v for v in [land.lo[other],land.hi[other]]if lo<v<hi]])
        common_ids=[]
        for value in common:
            q=[0.,0.];q[axis]=bound;q[other]=value;key=tuple(np.round(q,9))
            if key not in lookup:lookup[key]=len(points);points.append(q)
            common_ids.append(lookup[key])
        p=np.asarray(points);on=np.abs(p[:,axis]-bound)<1e-8;split=on[tri].sum(1)==2
        updated=[tri[~split]];fans=[]
        for face in tri[split]:
            border=face[on[face]];opposite=int(face[~on[face]][0]);low,high=np.sort(p[border,other])
            chain=np.asarray(common_ids)[(common>=low-1e-8)&(common<=high+1e-8)]
            if len(chain)==2:fans.append(face);continue
            for left,right in zip(chain,chain[1:]):
                t=np.array([opposite,left,right]);q=p[t]
                if np.cross(q[1]-q[0],q[2]-q[0])>0:t=t[[0,2,1]]
                fans.append(t)
        if fans:updated.append(np.asarray(fans,dtype=np.int64))
        tri=np.concatenate(updated)
    return np.asarray(points),tri

def mesh(path,x,z,land,materials,datum_y):
    p,tri=stitched_grid(x,z,land)
    ownership=getattr(land,'ground_ownership',None)
    if ownership is not None:p,tri=ownership.clip(p,tri)
    h=land.height(p)+.05
    eps=.15;nx=(land.height(p+[eps,0])-land.height(p-[eps,0]))/(2*eps);nz=(land.height(p+[0,eps])-land.height(p-[0,eps]))/(2*eps)
    normals=np.stack((-nx,np.ones_like(nx),-nz),axis=-1);normals/=np.linalg.norm(normals,axis=-1)[...,None]
    tangent=np.stack((np.ones_like(nx),nx,np.zeros_like(nx)),-1);tangent/=np.linalg.norm(tangent,axis=-1)[...,None];tangent=np.concatenate((tangent,-np.ones_like(nx)[...,None]),-1)
    flat=p.reshape(-1,2);centers=flat[tri].mean(1)
    if ownership is None:
        inside=np.all(centers>=land.lo,axis=1)&np.all(centers<=land.hi,axis=1);tri=tri[~inside];centers=centers[~inside]
    if not len(tri):return None
    dist,_=land.road_tree.query(centers);grade=np.linalg.norm(np.stack((nx,nz),-1).reshape(-1,2)[tri].mean(1),axis=1)
    if hasattr(land,'residential_path_distance'):dist=np.minimum(dist,land.residential_path_distance(centers))
    types=np.where(dist<2.35,'earthpath',np.where(grade>.65,'rocky','grassground'))
    if ownership is not None:
        infill=np.all(centers>=land.lo,axis=1)&np.all(centers<=land.hi,axis=1)
        types=np.where(infill& (grade<=.65),'earthpath',types)
    anchor=np.array([(x[0]+x[-1])/2,(z[0]+z[-1])/2]);miny=datum_y;pos=np.column_stack((p[:,0]-anchor[0],h-land.origin[1]-miny,p[:,1]-anchor[1]))
    if pos[:,1].min()<0:raise ValueError('Landscape fell below its shared LOD datum')
    writer,mapping=material_writer(materials);primitives=[]
    for name in np.unique(types):
        selected=tri[types==name];used,inv=np.unique(selected,return_inverse=True);uv=flat[used]/np.asarray(materials[name]['source_tile_metres'])
        attrs={'POSITION':writer.array(pos[used],'VEC3',34962),'NORMAL':writer.array(normals.reshape(-1,3)[used],'VEC3',34962),'TEXCOORD_0':writer.array(uv,'VEC2',34962),'TANGENT':writer.array(tangent.reshape(-1,4)[used],'VEC4',34962)}
        primitives.append(dict(attributes=attrs,indices=writer.array(inv.reshape(-1,1),'SCALAR',34963),material=mapping[name]))
    mi=len(writer.g['materials']);writer.g['materials'].append(dict(name='Countryside_Pivot_Datum',pbrMetallicRoughness=dict(baseColorFactor=[0,0,0,1],metallicFactor=0,roughnessFactor=1)))
    primitives.append(dict(attributes={'POSITION':writer.array(np.zeros((3,3)),'VEC3',34962),'NORMAL':writer.array(np.tile([0,1,0],(3,1)),'VEC3',34962)},indices=writer.array(np.arange(3).reshape(-1,1),'SCALAR',34963),material=mi))
    writer.g['meshes']=[dict(name=path.stem,primitives=primitives)];writer.g['nodes']=[dict(name=path.stem,mesh=0)];writer.g['scenes'][0]['nodes']=[0];path.parent.mkdir(parents=True,exist_ok=True)
    # Old immutable imported revisions can share an inode with the source.
    # Replacing a completed temporary file preserves those revisions/backups.
    with tempfile.NamedTemporaryFile(prefix=path.stem+'.',suffix='.glb',dir=path.parent,delete=False)as temporary:
        temporary_path=Path(temporary.name)
    try:
        writer.save(temporary_path,embedded=True)
        temporary_path.replace(path)
    finally:temporary_path.unlink(missing_ok=True)
    used=np.unique(tri);v=np.concatenate((pos[used],np.zeros((1,3))));return dict(bytes=path.stat().st_size,triangles=len(tri),vertices=sum(writer.g['accessors'][p['attributes']['POSITION']]['count']for p in primitives),bounds=[v.min(0).tolist(),v.max(0).tolist()],sha256=sha(path),pivot_offset_native=[0,miny,0],anchor=anchor.tolist())

def build():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--bep-catalog',type=Path);ap.add_argument('--base-catalog',type=Path,default=ROOT/'assets/purchased/bep_reforged/city-opening/catalog.json');args=ap.parse_args()
    base_path=args.base_catalog.resolve();base=json.loads(base_path.read_text());land=Landscape(base);OUT.mkdir(parents=True,exist_ok=True)
    materials=json.loads((ROOT/'assets/purchased/bep_reforged/countryside-materials/ground-materials.json').read_text())
    grid=np.linspace([BOUNDS[0],BOUNDS[1]],[BOUNDS[2],BOUNDS[3]],1025);xx,zz=np.meshgrid(grid[:,0],grid[:,1]);p=np.stack((xx,zz),-1);heights=land.height(p)
    lo=np.array(BOUNDS[:2])+land.origin[[0,2]];step=(np.array(BOUNDS[2:])-BOUNDS[:2])/1024
    field(OUT/'ground/countryside.height',heights,lo,step)
    datum_y=float(np.floor(heights.min()-land.origin[1])-2)
    coverage=~(np.all(p>=land.lo,axis=-1)&np.all(p<=land.hi,axis=-1));field(OUT/'ground/countryside.coverage',coverage,lo,step)
    catalog=copy.deepcopy(base);assets=catalog['assets'];placements=catalog['placements'];terrain=[]
    for zi in range(4):
        for xi in range(4):
            x0=BOUNDS[0]+(BOUNDS[2]-BOUNDS[0])*xi/4;x1=BOUNDS[0]+(BOUNDS[2]-BOUNDS[0])*(xi+1)/4;z0=BOUNDS[1]+(BOUNDS[3]-BOUNDS[1])*zi/4;z1=BOUNDS[1]+(BOUNDS[3]-BOUNDS[1])*(zi+1)/4
            files=[];stats=[];name=f'BEP_Countryside_Terrain_{xi}_{zi}'
            for label,side in [('near',129),('mid',49)]:
                x=np.unique(np.r_[np.linspace(x0,x1,side),land.lo[0]if x0<land.lo[0]<x1 else[],land.hi[0]if x0<land.hi[0]<x1 else[]]);z=np.unique(np.r_[np.linspace(z0,z1,side),land.lo[1]if z0<land.lo[1]<z1 else[],land.hi[1]if z0<land.hi[1]<z1 else[]])
                path=OUT/'models'/f'{name}_{label}.glb';s=mesh(path,x,z,land,materials,datum_y)
                if s is None:break
                files.append(str(path));stats.append(s)
            if len(files)!=2:continue
            # Identical grounded pivot for both LODs even if sampling extrema differ.
            if abs(stats[0]['pivot_offset_native'][1]-stats[1]['pivot_offset_native'][1])>.03:raise ValueError('Terrain LOD datum differs; cook with fixed common minimum')
            a=dict(name=name,files=files,stats=stats,bounds=stats[0]['bounds'],pivot_offset_native=stats[0]['pivot_offset_native'],collision=None,native=dict(alias=f'bep_country_terrain_{xi}_{zi}',vegetation=0,collision=0,distance=1300,split=260))
            assets.append(a);m=np.eye(4);m[[0,2],3]=stats[0]['anchor'];placements.append(dict(asset=name,matrix_native=m.T.flatten().tolist(),authored_extension='BEP source-sculpt countryside tile'));terrain.append(name)
    catalog['countryside']=dict(source='Owned BEP Reforged Isle_01 authored sculpt, adapted to existing city',ground_files=[str(OUT/'ground/countryside.height'),str(OUT/'ground/countryside.coverage')],terrain_assets=terrain,source_npz_sha256=sha(PRIVATE/'terrain-Isle_01.npz'),native_bounds=BOUNDS,vertical_scale=SCULPT_VERTICAL_SCALE,road_half_width=ROAD_HALF_WIDTH,road_shoulder_width=ROAD_SHOULDER_WIDTH,source_anchors=[SOURCE_A.tolist(),SOURCE_B.tolist()],native_anchors=[TARGET_A.tolist(),TARGET_B.tolist()],road_native=land.road[::12].tolist(),base_catalog_sha256=sha(base_path))
    if land.castle_support is not None:
        catalog['countryside']['castle_ridge']=dict(native_xz_bounds=[v.tolist()for v in land.castle_support],support_world_y=float(land.origin[1]+25),feather_metres=55,unchanged_below_native_z=85,walkable_castle=False)
    if args.bep_catalog:
        from scatter_city_countryside import add_dressing
        add_dressing(catalog,land,args.bep_catalog)
    (OUT/'catalog.json').write_text(json.dumps(catalog,indent=2,allow_nan=False)+'\n')
    np.savez_compressed(OUT/'landscape-review.npz',height_world=heights,x_native=grid[:,0],z_native=grid[:,1],road_native=land.road,road_height_world=land.road_h)
    print(json.dumps(dict(catalog=str(OUT/'catalog.json'),definitions=len(assets),placements=len(placements),height_range=[float(heights.min()),float(heights.max())],terrain_tiles=len(terrain)),indent=2))

if __name__=='__main__':build()
