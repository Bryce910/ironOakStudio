#!/usr/bin/env python3
"""Raster actual opaque source MID architecture to a bounded snow ceiling cache."""
from pathlib import Path
import argparse,collections,json,re,struct,subprocess
import numpy as np
import cook_nature_showcase as base

EXCLUDED={'Spawned Rocks','Rocks Walls','Rocks','Obsidian Rocks','Background Mountains','Lava Rivers and Lakes','Emission'}
def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--ground',type=Path,default=base.ROOT/'projects/valwood_showcase/ground/showcase.height');parser.add_argument('--output',type=Path,default=base.ROOT/'projects/valwood_showcase/ground/showcase.roofs');parser.add_argument('--resolution',type=int,default=513);parser.add_argument('--floor',type=float,default=1130.8125);args=parser.parse_args()
    source=base.Source(base.DEFAULT_SOURCE);metadata={m['path']:m for m in source.doc['materials']};stats=collections.Counter()
    private=base.PRIVATE/'showcase-roofs';private.mkdir(exist_ok=True);triangles=private/'triangles.f32';samples=private/'heights.f32';exe=private/'raster'
    def opaque(path):
        if re.search(r'glass|cutoff|ornam|lava|emissiv|water',path,re.I):return False
        raw=metadata.get(path,{}).get('raw_yaml','');queue=re.findall(r'm_CustomRenderQueue: (-?\d+)',raw)
        return not queue or int(queue[-1])<2450
    with triangles.open('wb')as out:
        for p in source.doc['placements']:
            if p['group']in EXCLUDED or not p['lod_mask']&2:continue
            vertex,indices=source.mesh(p['mesh']);matrix=np.array(p['matrix']).reshape(4,4,order='F')
            v=vertex[:,:3]@matrix[:3,:3].T+matrix[:3,3];normal=vertex[:,3:6]@np.linalg.inv(matrix[:3,:3]);normal/=np.maximum(np.linalg.norm(normal,axis=1,keepdims=True),1e-10)
            for sub,index in enumerate(indices):
                path=source.material(p,sub)
                if not opaque(path):stats['excluded_transparent_or_cutout_parts']+=1;continue
                index=index.reshape(-1,3);up=normal[index,1].mean(1)>.2;tri=v[index[up]].copy()
                if not len(tri):continue
                tri[:,:,1]+=args.floor;tri[:,:,2]=1000-tri[:,:,2]
                out.write(np.ascontiguousarray(tri,dtype='<f4').tobytes());stats['triangles']+=len(tri);stats['parts']+=1;stats['group:'+p['group']]+=len(tri)
    print('SOURCE ROOFS',dict(stats),flush=True)
    subprocess.run(['g++','-O3','-std=c++17',str(base.ROOT/'tools/raster_nature_showcase_roofs.cpp'),'-o',str(exe)],check=True)
    subprocess.run([str(exe),str(triangles),str(samples),str(args.resolution)],check=True)
    header=args.ground.read_bytes()[:48];magic,w,h,x,z,sx,sz=struct.unpack('<8sIIdddd',header)
    assert magic==b'VWHGT1\0\0'
    step_x=sx*(w-1)/(args.resolution-1);step_z=sz*(h-1)/(args.resolution-1)
    raw=samples.read_bytes();assert len(raw)==args.resolution**2*4
    args.output.write_bytes(struct.pack('<8sIIdddd',magic,args.resolution,args.resolution,x,z,step_x,step_z)+raw)
    heights=np.frombuffer(raw,dtype='<f4').reshape(args.resolution,args.resolution)
    def at_source(px,pz):
        fx,fz=px/1000*(args.resolution-1),(1000-pz)/1000*(args.resolution-1);ix,iz=int(fx),int(fz);dx,dz=fx-ix,fz-iz
        values=heights[iz:iz+2,ix:ix+2]
        value=values[0,0]*(1-dx)*(1-dz)+values[0,1]*dx*(1-dz)+values[1,0]*(1-dx)*dz+values[1,1]*dx*dz
        return {'source_xz':[px,pz],'nearest_height':float(heights[round(fz),round(fx)]),'bilinear_height':float(value),'source_height':float(value-args.floor)if value else 0}
    report={'output':str(args.output),'format':'VWHGT1; 48byte littleendian header8sIIdddd; float32[z,x]','resolution':args.resolution,'world_origin':[x,z],'world_step':[step_x,step_z],'default_no_ceiling':0,'absolute_y':True,'source_y_offset':args.floor,'source_z_reversed':True,'nonzero_samples':int(np.count_nonzero(heights)),'maximum_absolute_height':float(heights.max()),'stats':dict(stats),'checks':{'long_hall':at_source(239.06292255,615.14478442),'hall':at_source(238.12291015,635.01479338),'open_field':at_source(900,100)}}
    base.atomic_json(args.output.with_suffix('.roofs.json'),report);print(json.dumps(report['checks']),flush=True)
    assert report['checks']['hall']['bilinear_height']>args.floor+180,'Expected source temple roof missing'
    assert report['checks']['open_field']['bilinear_height']==0,'Open field gained a phantom ceiling'
    print('ROOFS READY',args.output,flush=True)

if __name__=='__main__':main()
