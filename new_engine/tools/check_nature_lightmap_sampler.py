#!/usr/bin/env python3
"""CPU fixtures for source UV orientation, HDR and directional lightmaps."""
from pathlib import Path
import json,tempfile
import numpy as np
from PIL import Image
from nature_lightmap_sampler import LightmapSampler


def main():
    with tempfile.TemporaryDirectory(prefix='nature-lightmap-fixture-')as temporary:
        folder=Path(temporary)
        # Cache row0 is Unity bottom row, with uniquely identifiable corners.
        color=np.array([[[.123456,0,0],[0,2,0]],[[0,0,3],[4,4,4]]],np.float32)
        np.save(folder/'map.npy',color)
        direction=np.broadcast_to(np.array([128,255,128,255],np.uint8),(2,2,4)).copy()
        Image.fromarray(direction).save(folder/'dir.png')
        manifest={'atlas':[{'index':0,'cache':str(folder/'map.npy'),'direction_source':str(folder/'dir.png')}]}
        (folder/'manifest.json').write_text(json.dumps(manifest))
        sampler=LightmapSampler(folder/'manifest.json')
        uv=np.array([[.25,.25],[.75,.25],[.25,.75],[.75,.75]],np.float32)
        raw=sampler.sample_rgba(0,uv)
        assert np.allclose(raw[:,:3],color.reshape(-1,3),atol=1e-7)
        assert np.all(raw[:,3]==1)
        assert np.all(sampler.sample_rgba(65534,uv)==0)
        assert np.all(sampler.sample_rgba(0,uv,valid=False)==0)
        assert np.all(sampler.sample_rgba(0,np.array([[2,2]],np.float32))==0)
        up=np.broadcast_to(np.array([0,1,0],np.float32),(4,3))
        down=-up
        assert np.allclose(sampler.sample_rgba(0,uv,up)[:,:3],raw[:,:3])
        assert np.all(sampler.sample_rgba(0,uv,down)[:,:3]==0)
        assert np.isclose(raw[0,0],.123456,atol=1e-7)
        center=sampler.sample_rgba(0,np.array([[.5,.5]],np.float32))
        assert np.allclose(center[0,:3],color.mean((0,1)))
        print(json.dumps({'UV_bottom_left_orientation':'passed','bilinear_centers':'passed','linear_HDR_precision':'passed',
                          'unbaked65534':'passed','explicit_invalid_flag':'passed','invalid_UV':'passed','source_directional_halfLambert':'passed','stats':sampler.stats},indent=2))


if __name__=='__main__':main()
