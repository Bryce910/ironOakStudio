#!/usr/bin/env python3
"""New visual-only castle backing hill using owned stone material/rock meshes.

Original publisher city records remain identical. The extension is explicitly
new backdrop composition and does not claim castle/hill walking or collision.
"""
from pathlib import Path
import argparse,copy,hashlib,json,math
import numpy as np
from scipy.spatial import ConvexHull
from cook_village_publisher_scene import Source,ROOT,C,emit
OUT=ROOT/'assets/purchased/medieval_village/publisher-backdrop'
BASE=ROOT/'assets/purchased/medieval_village/publisher-bunting/combined-catalog.json'
MATS=ROOT/'assets/purchased/medieval_village/city-materials/materials-frozen.json'
CASTLE_TRANSLATION=np.array([-134.5,68.87494140625,202.6])

class Frozen:
    def __init__(self):self.rows=json.loads(MATS.read_text())
    def get(self,name):return self.rows[name]

def generated_hill(source,near=True):
    castle,_,_=source.geometry('SM_Castle',0);cp=castle[:,:3]@C.T/100;bottom=cp[cp[:,1]<cp[:,1].min()+1][:,[0,2]];hull=ConvexHull(bottom);center=(bottom.min(0)+bottom.max(0))/2
    segments=160 if near else 80;levels=np.array([0,.08,.18,.29,.40,.51,.63,.76,.88,1.]);heights=np.array([35,34,30,29,23,22,15,10,3,-.6]);rings=[];uvs=[]
    def top_radius(angle):
        direction=np.array([math.cos(angle),math.sin(angle)]);dots=hull.equations[:,:2]@direction;distance=-(hull.equations[:,:2]@center+hull.equations[:,2]);return float(np.min(distance[dots>1e-6]/dots[dots>1e-6]))+3.4
    for ring,(t,height)in enumerate(zip(levels,heights)):
        row=[];uv=[]
        for j in range(segments+1):
            angle=j/segments*2*math.pi;r=top_radius(angle);width=20+3*math.sin(angle*3+.7)+2*math.cos(angle*5)
            ripple=(math.sin(angle*9+ring*.43)+.5*math.sin(angle*17-ring*.28))*math.sin(math.pi*t)*1.25
            radius=r+width*t+ripple;point=center+np.array([math.cos(angle),math.sin(angle)])*radius
            y=height+math.sin(math.pi*t)*(.9*math.sin(angle*7+ring*.5)+.5*math.sin(angle*13-ring*.3))
            row.append([point[0],y,point[1]]);uv.append([angle*max(r,1)/2.6,y/2.6])
        rings.append(np.asarray(row));uvs.append(np.asarray(uv))
    positions=[];tex=[]
    # Exact level support covers the convex hull of the castle's actual bottom
    # points plus3.4m; the outer skirts are intentionally irregular and wider.
    for j in range(segments):
        p=np.array([[center[0],35,center[1]],rings[0][j+1],rings[0][j]])
        if np.cross(p[1]-p[0],p[2]-p[0])[1]<0:p=p[::-1]
        positions.extend(p);tex.extend(p[:,[0,2]]/2.6)
    for k in range(len(rings)-1):
        for j in range(segments):
            for corners in[[(k,j),(k+1,j),(k+1,j+1)],[(k,j),(k+1,j+1),(k,j+1)]]:
                p=np.array([rings[a][b]for a,b in corners]);uv=np.array([uvs[a][b]for a,b in corners]);radial=p.mean(0)-[center[0],0,center[1]];radial[1]=0
                if np.dot(np.cross(p[1]-p[0],p[2]-p[0]),radial)<0:p=p[::-1];uv=uv[::-1]
                positions.extend(p);tex.extend(uv)
    p=np.asarray(positions);uv=np.asarray(tex);faces=p.reshape(-1,3,3);norm=np.cross(faces[:,1]-faces[:,0],faces[:,2]-faces[:,0]);norm/=np.maximum(np.linalg.norm(norm,axis=1)[:,None],1e-20);normal=np.repeat(norm,3,axis=0)
    tangent=np.cross(np.tile([0,1,0.],(len(p),1)),normal);flat=np.linalg.norm(tangent,axis=1)<1e-8;tangent[flat]=[1,0,0];tangent/=np.maximum(np.linalg.norm(tangent,axis=1)[:,None],1e-20)
    v=np.zeros((len(p),18),np.float32);v[:,:3]=p@C*100;v[:,3:6]=normal@C;v[:,6:8]=uv;v[:,8:10]=uv;v[:,10:13]=tangent@C;v[:,13]=-1;v[:,17]=1
    name='SM_City_Castle_Rock_Hill';lod=0 if near else 1;row=dict(name=name+'_LOD'+str(lod),vertices='generated:'+str(lod),source_file='new backdrop hill generated from actual source castle support hull')
    source.groups[name][lod]=row;source.derived[name,lod]=(v,[dict(slot=0,material='MI_Statue_Rock',source_material='MI_Statue_Rock',indices=np.arange(len(v),dtype=np.uint32).reshape(-1,3))],row)
    return center,top_radius

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--output',type=Path,default=OUT);args=ap.parse_args();source=Source();base=json.loads(BASE.read_text());center,radius=generated_hill(source,True);generated_hill(source,False)
    pieces=[dict(family='SM_City_Castle_Rock_Hill',variant=None,matrix=np.eye(4),override=[])];rng=np.random.default_rng(42051);rockaudit=[]
    for tier,count in enumerate([30,28,32]):
        for j in range(count):
            angle=2*math.pi*(j+rng.uniform(-.23,.23))/count;r=radius(angle)+[3.5,11.5,20][tier]+rng.uniform(-1.6,1.6);xz=center+[math.cos(angle)*r,math.sin(angle)*r]
            family=['SM_Rock_01','SM_Rock_02','SM_Rock_03'][(j+tier)%3];v,_,_=source.geometry(family,0);native=v[:,:3]@C.T/100;extent=np.ptp(native,axis=0);dimensions=np.array([rng.uniform(9,15),rng.uniform(7,11),rng.uniform(10,17)])
            if tier==2:dimensions*=.78
            scale=dimensions/np.maximum(extent,.01);yaw=angle+math.pi/2+rng.uniform(-.4,.4);cy,sy=math.cos(yaw),math.sin(yaw);rotation=np.array([[cy,0,sy],[0,1,0],[-sy,0,cy]])
            m=np.eye(4);m[:3,:3]=rotation@np.diag(scale);m[:3,3]=[xz[0],[30,18,3.1][tier]+rng.uniform(-1.7,1.7),xz[1]]
            # Original material tiles repeat at the original stone grain scale
            # rather than stretching a sub-metre rock atlas across15metres.
            density=float(math.sqrt(scale[0]*scale[2]));pieces.append(dict(family=family,variant=None,matrix=m,override=[],uv_density_scale=density));rockaudit.append(dict(source_mesh=family,tier=tier,scale=scale.tolist(),uv_density_scale=density))
    miny=float('inf')
    for piece in pieces:
        for lod in[0,1]:
            v,_,_=source.geometry(piece['family'],lod);p=(v[:,:3]@C.T/100)@piece['matrix'][:3,:3].T+piece['matrix'][:3,3];miny=min(miny,float(p[:,1].min()))
    files=[];stats=[]
    for label,lod in[('near',0),('mid',1)]:
        path=args.output/'models'/('SM_City_Castle_Backdrop_Plinth_'+label+'.glb');stats.append(emit(path,pieces,source,lod,Frozen(),miny));files.append(str(path.resolve()))
    asset=dict(name='SM_City_Castle_Backdrop_Plinth',files=files,pivot_offset_native=[0,miny,0],bounds=stats[0]['bounds'],stats=stats,collision=None,
        source_families=['Generated source-castle support hull','SM_Rock_01','SM_Rock_02','SM_Rock_03'],new_dressing=True,background_only=True,
        provenance='New rocky backing hill plus90overlapping owned source boulders; exact purchased MI_Statue_Rock PBR; no snow/new purchase')
    hill=np.eye(4);hill[[0,2],3]=CASTLE_TRANSLATION[[0,2]];castle=np.eye(4);castle[:3,3]=CASTLE_TRANSLATION
    oldassets=copy.deepcopy(base['assets']);oldplacements=copy.deepcopy(base['placements']);result=copy.deepcopy(base);result['assets'].append(asset)
    result['placements'].extend([dict(asset=asset['name'],matrix_native=hill.T.flatten().tolist(),new_background=True,background_only=True,collision=False),
        dict(asset='SM_Castle',matrix_native=castle.T.flatten().tolist(),new_background=True,background_only=True,collision=False)])
    result['new_background']=dict(type='Castle and rocky supporting hill',new_composition=True,source_castle_library_reused=True,walkable=False,collision=False,
        castle_translation_native=CASTLE_TRANSLATION.tolist(),castle_support_top_source_y=35,rocks=rockaudit,plinth_bounds_local=stats[0]['bounds'],
        baseline_catalog=str(BASE.resolve()),baseline_catalog_sha256=hashlib.sha256(BASE.read_bytes()).hexdigest(),source_castle_pivot_y=-33.87494140625)
    if result['assets'][:-1]!=oldassets or result['placements'][:-2]!=oldplacements:raise ValueError('Prior source/bunting catalog records changed')
    args.output.mkdir(parents=True,exist_ok=True);(args.output/'catalog.json').write_text(json.dumps(result,indent=2,allow_nan=False)+'\n');print('READY',args.output/'catalog.json',len(result['assets']),'assets',len(result['placements']),'placements',stats,flush=True)
if __name__=='__main__':main()
