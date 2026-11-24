#!/usr/bin/env python3
"""Recover exact publisher Timeline cameras and validate them against source data.

Read-only for scene/import assets. This deliberately includes the infinite clip
rotation when transforming position keys (the earlier timeline inventory did not).
CPU triangle rays are two-sided, so transparent/cutout hits are reported as such.
"""
from pathlib import Path
import argparse, bisect, functools, json, math
import numpy as np

ROOT = Path(__file__).resolve().parents[1]
PRIVATE = ROOT.parent / '.local-tools/unity-source'
DOCS = ROOT / 'docs/stronghold/2026-11-01/showcase'

def vec(v):
    return np.array([v[k] for k in 'xyz'], float)

def euler_matrix(e):
    """Unity Quaternion.Euler: Z then X then Y, column-vector convention."""
    x, y, z = np.radians(e)
    sx,cx,sy,cy,sz,cz = math.sin(x),math.cos(x),math.sin(y),math.cos(y),math.sin(z),math.cos(z)
    return np.array([[cy,0,sy],[0,1,0],[-sy,0,cy]]) @ np.array([[1,0,0],[0,cx,-sx],[0,sx,cx]]) @ np.array([[cz,-sz,0],[sz,cz,0],[0,0,1]])

def evaluate(keys, time):
    ts = [k['time'] for k in keys]
    i = bisect.bisect_right(ts,time)-1
    if i < 0:return vec(keys[0]['value'])
    if i >= len(keys)-1:return vec(keys[-1]['value'])
    a,b = keys[i],keys[i+1]
    dt=b['time']-a['time'];u=(time-a['time'])/dt
    av,bv=vec(a['value']),vec(b['value'])
    out=np.array([float(a['outSlope'][k]) for k in 'xyz'])
    ins=np.array([float(b['inSlope'][k]) for k in 'xyz'])
    # Unity stepped curves hold the outgoing key until the next key.
    return np.where(np.isfinite(out)&np.isfinite(ins),
        (2*u**3-3*u**2+1)*av+(u**3-2*u**2+u)*dt*np.nan_to_num(out,posinf=0,neginf=0)+(-2*u**3+3*u**2)*bv+(u**3-u**2)*dt*np.nan_to_num(ins,posinf=0,neginf=0),av)

class Cameras:
    def __init__(self):
        self.j=json.loads((DOCS/'publisher-camera-timeline.json').read_text())
        t=self.j['track'];self.offset=vec(t['m_InfiniteClipOffsetPosition']);self.rotation=euler_matrix(vec(t['m_InfiniteClipOffsetEulerAngles']))
        self.parent=vec(self.j['camera_parent_position'])
        self.pkeys=self.j['clip']['m_PositionCurves'][0]['curve']['m_Curve']
        self.ekeys=self.j['clip']['m_EulerCurves'][0]['curve']['m_Curve']
    def at(self,t):
        p=evaluate(self.pkeys,t);e=evaluate(self.ekeys,t)
        position=self.parent+self.offset+self.rotation@p
        rotation=self.rotation@euler_matrix(e);forward=rotation[:,2]
        native_forward=forward*np.array([1,1,-1])
        yaw=math.atan2(native_forward[0],-native_forward[2]);pitch=math.atan2(native_forward[1],np.linalg.norm(native_forward[[0,2]]))
        return {'time':t,'source_position':position.tolist(),'source_raw_clip_position':p.tolist(),'source_raw_clip_euler':e.tolist(),
            'source_forward':forward.tolist(),'source_up':rotation[:,1].tolist(),'source_right':rotation[:,0].tolist(),
            'source_world_euler_pitch_yaw_roll':[float(e[0]),float((e[1]+310.1328)%360),float(e[2])],
            'native_yaw_radians':yaw,'native_pitch_radians':pitch,'native_yaw_degrees':math.degrees(yaw),'native_pitch_degrees':math.degrees(pitch),'vertical_fov_degrees':60.000003814697266}

class Scene:
    def __init__(self):
        self.path=PRIVATE/'showcase-native/scene.json';self.doc=json.loads(self.path.read_text())
        self.meshes={m['hash']:m for m in self.doc['meshes']}
        self.parts=[p for p in self.doc['placements']if p['lod_mask']&1 or p['group']=='Background Mountains']
        self.bmin=np.array([vec(p['bounds_min']) for p in self.parts]);self.bmax=np.array([vec(p['bounds_max']) for p in self.parts])
        self.matrices=np.array([np.array(p['matrix']).reshape(4,4,order='F') for p in self.parts])
        self.inverse=np.linalg.inv(self.matrices)
        terrain=self.doc['terrains'][0];self.terrain=terrain
        n=terrain['height_resolution'];self.height=np.fromfile(self.path.parent/terrain['height_binary'],dtype='<f4').reshape(n,n)*terrain['size']['y']
        self.holes=np.fromfile(self.path.parent/terrain['holes_binary'],dtype='u1').reshape(terrain['holes_resolution'],terrain['holes_resolution'])
    def terrain_at(self,position):
        x,z=position[0],position[2];n=self.height.shape[0]
        if not 0<=x<=1000 or not 0<=z<=1000:return None
        px,pz=x*(n-1)/1000,z*(n-1)/1000;x0,z0=int(px),int(pz);x1,z1=min(x0+1,n-1),min(z0+1,n-1)
        dx,dz=px-x0,pz-z0
        h=(self.height[z0,x0]*(1-dx)+self.height[z0,x1]*dx)*(1-dz)+(self.height[z1,x0]*(1-dx)+self.height[z1,x1]*dx)*dz
        hz,hx=min(int(z*self.holes.shape[0]/1000),self.holes.shape[0]-1),min(int(x*self.holes.shape[1]/1000),self.holes.shape[1]-1)
        return {'height':float(h),'solid':bool(self.holes[hz,hx]),'clearance':float(position[1]-h)}
    @functools.lru_cache(maxsize=None)
    def triangles(self,hash):
        m=self.meshes[hash];raw=(self.path.parent/m['binary']).read_bytes();vertices=np.ndarray((m['vertices'],16),'<f4',raw)[:,:3]
        index=np.concatenate([np.ndarray((s['indices'],),'<i4',raw,s['index_offset'])for s in m['submeshes']])
        return vertices[index].reshape(-1,3,3).astype(float)
    def ray(self,origin,direction,limit=1000,near_clip=.3):
        d=np.asarray(direction,float);o=np.asarray(origin,float);d=d/np.linalg.norm(d)
        reciprocal=np.divide(1,d,out=np.full(3,np.inf),where=np.abs(d)>1e-12)
        a=(self.bmin-o)*reciprocal;b=(self.bmax-o)*reciprocal
        near=np.max(np.minimum(a,b),axis=1);far=np.min(np.maximum(a,b),axis=1)
        candidates=np.flatnonzero((far>=np.maximum(near,near_clip))&(near<=limit))
        best=limit;answer=None
        for k in candidates[np.argsort(near[candidates])]:
            if near[k]>best:break
            inv=self.inverse[k];lo=inv[:3,:3]@o+inv[:3,3];ld=inv[:3,:3]@d
            tri=self.triangles(self.parts[k]['mesh']);v0=tri[:,0];e1=tri[:,1]-v0;e2=tri[:,2]-v0
            h=np.cross(ld,e2);det=np.einsum('ij,ij->i',e1,h);ok=np.abs(det)>1e-9
            f=np.divide(1,det,out=np.zeros_like(det),where=ok);s=lo-v0;u=f*np.einsum('ij,ij->i',s,h);q=np.cross(s,e1);v=f*(q@ld);t=f*np.einsum('ij,ij->i',e2,q)
            ok&=(u>=0)&(v>=0)&(u+v<=1)&(t>=near_clip)&(t<best)
            if not ok.any():continue
            best=float(t[ok].min());p=self.parts[k]
            answer={'distance':best,'position':(o+d*best).tolist(),'group':p['group'],'hierarchy':p['hierarchy'],'materials':p['materials']}
        # Terrain itself is not a MeshRenderer. March then bisect its exact height field.
        previous=near_clip
        for t in np.arange(near_clip,best,1.0):
            p=o+d*t;sample=self.terrain_at(p)
            if sample and sample['solid']and sample['clearance']<0:
                low,high=previous,float(t)
                for _ in range(14):
                    mid=(low+high)/2;sample=self.terrain_at(o+d*mid)
                    if sample and sample['solid']and sample['clearance']<0:high=mid
                    else:low=mid
                return {'distance':high,'position':(o+d*high).tolist(),'group':'Terrain','hierarchy':'Terrain','materials':[]}
            previous=float(t)
        return answer
    def validate(self,shot,fan=False):
        o=np.array(shot['source_position']);f=np.array(shot['source_forward']);r=np.array(shot['source_right']);u=np.array(shot['source_up'])
        tan=math.tan(math.radians(shot['vertical_fov_degrees'])/2);aspect=16/9
        rays={}
        for y in([-0.65,0,.65]if fan else[0]):
            for x in([-0.65,0,.65]if fan else[0]):
                d=f+r*x*tan*aspect+u*y*tan
                rays[f'{x:+.2f},{y:+.2f}']=self.ray(o,d)
        shot['terrain']=self.terrain_at(o);shot['ray_fan']=rays
        shot['down_ray']=self.ray(o,[0,-1,0],200)
        return shot

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--times',type=float,nargs='+',default=[0,4.0333333,11.783334,52.066666,70.36667,101.183334,131.1,165.56667,231.23334,237.78334,244.51666,251.68333,260,264.16666,284.26666,291.11667]);parser.add_argument('--fan',action='store_true');parser.add_argument('--output',type=Path,default=DOCS/'publisher-camera-verified.json');args=parser.parse_args()
    cameras=Cameras();scene=Scene();shots=[]
    for time in args.times:
        shot=scene.validate(cameras.at(time),args.fan);shots.append(shot)
        print(time,shot['source_position'],shot['source_world_euler_pitch_yaw_roll'],'terrain',shot['terrain'],'center',shot['ray_fan']['+0.00,+0.00'],flush=True)
    report={'version':2,'status':'Recovered Timeline with infinite-clip rotation and CPU source triangle/terrain ray validation; no GPU quality claim',
        'supersedes':'publisher-camera-timeline.json sample_shots camera_position_if_infinite_offset_applied omitted rotation and is invalid',
        'world_transform':'RenderParent * TrackOffset(identity) * InfiniteClipOffset(position249.6,-85.4,10.4; yaw310.1328) * animatedClipRootTransform',
        'unity_source_evidence':['https://github.com/needle-mirror/com.unity.timeline/blob/master/Runtime/Animation/AnimationTrack.cs#L717-L770','https://github.com/needle-mirror/com.unity.timeline/blob/master/Runtime/Animation/AnimationPlayableAsset.cs#L204-L239'],
        'ray_limitations':'Opaque two-sided geometry; alpha cutouts/glass not sampled. Near plane .3m. Terrain includes source holes. Raster framing/light fidelity still requires native capture.',
        'shots':shots}
    args.output.write_text(json.dumps(report,indent=2,allow_nan=False)+'\n')

if __name__=='__main__':main()
