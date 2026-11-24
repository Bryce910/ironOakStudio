"""Source Unity static-lighting sampling without per-instance image baking.

Public API:
    sampler = LightmapSampler()
    rgba = sampler.sample_rgba(index, uv1*scale.xy+offset.zw, normal_source_world)

UVs are the original unflipped Unity lightmap UV1. Normals, if supplied, are
unit source-Unity-world normals before relocating/Z-reflecting the demo. Raw
color-only sampling is available when normals=None. Invalid/unbaked vertices
return zero RGB and alpha0, preserving normal runtime lighting fallback.
"""
from pathlib import Path
import functools,json
import numpy as np
from PIL import Image

ROOT=Path(__file__).resolve().parents[1]
DEFAULT=ROOT.parent/'.local-tools/unity-source/showcase-lightmaps/manifest.json'


def _bilinear(image,uv):
    """UVv0 corresponds to image row0; clamp-to-edge matches source import."""
    h,w=image.shape[:2];xy=np.clip(uv,0,1)*np.array([w,h],np.float32)-.5
    lo=np.floor(xy).astype(np.int32);mix=(xy-lo).astype(np.float32)
    x0=np.clip(lo[:,0],0,w-1);x1=np.clip(lo[:,0]+1,0,w-1)
    y0=np.clip(lo[:,1],0,h-1);y1=np.clip(lo[:,1]+1,0,h-1)
    fx=mix[:,0:1];fy=mix[:,1:2]
    return (image[y0,x0]*(1-fx)+image[y0,x1]*fx)*(1-fy)+(image[y1,x0]*(1-fx)+image[y1,x1]*fx)*fy


class LightmapSampler:
    def __init__(self,manifest=DEFAULT):
        self.manifest=json.loads(Path(manifest).read_text())
        self.registry={int(record['index']):record for record in self.manifest['atlas']}
        self.stats={'calls':0,'vertices':0,'valid':0,'unbaked':0,'invalid_uv':0,'directional':0}

    @functools.lru_cache(maxsize=8)
    def color(self,index):
        return np.load(self.registry[index]['cache'],mmap_mode='r',allow_pickle=False)

    @functools.lru_cache(maxsize=8)
    def direction(self,index):
        path=self.registry[index].get('direction_source')
        if not path:return None
        with Image.open(ROOT/path)as image:
            # Standard PNG row0 is UVv1. Direction is non-color UNORM data.
            # Retain source8-bit storage in the cache rather than expanding all
            # eight 2K direction maps to512MiB of float32 CPU memory.
            return np.asarray(image.convert('RGBA'),np.uint8)[::-1].copy()

    def sample(self,index,uv,normals=None,valid=True):
        uv=np.asarray(uv,np.float32)
        if uv.ndim!=2 or uv.shape[1]!=2:raise ValueError('Lightmap UV array must be Nx2')
        n=len(uv);rgb=np.zeros((n,3),np.float32)
        enabled=np.broadcast_to(np.asarray(valid,bool),(n,)).copy()
        self.stats['calls']+=1;self.stats['vertices']+=n
        if index is None or int(index)not in self.registry:
            self.stats['unbaked']+=n;return rgb,np.zeros(n,bool)
        index=int(index)
        finite=np.isfinite(uv).all(1);inside=(uv>=-.001).all(1)&(uv<=1.001).all(1)
        enabled&=finite&inside
        self.stats['invalid_uv']+=int((~(finite&inside)).sum())
        if enabled.any():
            selected=uv[enabled];sample=_bilinear(self.color(index),selected)
            if normals is not None:
                normals=np.asarray(normals,np.float32)
                if normals.shape!=(n,3):raise ValueError('Source world normal array must be Nx3')
                normal=normals[enabled];length=np.linalg.norm(normal,axis=1,keepdims=True)
                if not np.isfinite(normal).all()or np.any(length<1e-8):raise ValueError('Invalid source world normals')
                normal=normal/length;direction=self.direction(index)
                if direction is not None:
                    d=_bilinear(direction,selected)/255
                    half_lambert=(normal*(d[:,:3]-.5)).sum(1,keepdims=True)+.5
                    sample*=half_lambert/np.maximum(d[:,3:4],1e-4)
                    self.stats['directional']+=len(selected)
            if not np.isfinite(sample).all():raise ValueError('Non-finite sampled source irradiance')
            rgb[enabled]=np.clip(sample,0,65504)
        self.stats['valid']+=int(enabled.sum())
        return rgb,enabled

    def sample_rgba(self,index,uv,normals=None,valid=True):
        rgb,enabled=self.sample(index,uv,normals,valid)
        return np.column_stack((rgb,enabled.astype(np.float32))).astype(np.float32)
