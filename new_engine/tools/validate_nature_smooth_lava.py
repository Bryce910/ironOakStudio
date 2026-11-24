#!/usr/bin/env python3
"""Verify the lava-only repair preserves source surfaces and native budgets."""
from pathlib import Path
import argparse,json
import numpy as np
from scipy.spatial import cKDTree
import cook_nature_showcase as base
from bake_nature_material_fidelity import glb_document

def accessor(doc,binary,index):
    a=doc['accessors'][index];v=doc['bufferViews'][a['bufferView']];width={'SCALAR':1,'VEC2':2,'VEC3':3,'VEC4':4}[a['type']]
    dtype={5125:'<u4',5126:'<f4'}[a['componentType']]
    return np.ndarray((a['count'],width),dtype,binary,v.get('byteOffset',0)+a.get('byteOffset',0),strides=(v.get('byteStride',width*4),4))

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('catalog',type=Path);args=parser.parse_args();records=json.loads(args.catalog.read_text());source=base.Source(base.DEFAULT_SOURCE);report=[]
    old={r['name']:r for r in json.loads((base.ROOT/'assets/purchased/nature_castle/showcase_lava/catalog.json').read_text())}
    assert len(records)==4
    for record in records:
        path=record['source_material'];expected=[[],[]]
        for p in source.doc['placements']:
            if path not in p['materials']:continue
            v,indices=source.mesh(p['mesh']);matrix=np.array(p['matrix']).reshape(4,4,order='F');positions=(v[:,:3]@matrix[:3,:3].T+matrix[:3,3]).astype(np.float32)
            for sub,index in enumerate(indices):
                if source.material(p,sub)!=path:continue
                tri=positions[index].reshape(-1,3,3)
                area=np.linalg.norm(np.cross(tri[:,1]-tri[:,0],tri[:,2]-tri[:,0]),axis=1)
                centroids=tri[area>1e-8].mean(1)
                for lod in[0,1]:
                    if p['lod_mask']&(1<<lod):expected[lod].append(centroids)
        assert np.allclose(record['pivot'],old[record['name']]['pivot'],atol=1e-4),'Original source placement pivot changed'
        for lod,file in enumerate(record['files']):
            file=Path(file);doc,binary=glb_document(file);verts=0;visible=[];heats=[];uvranges=[]
            assert len(doc['materials'])==1,'Face-partitioned materials remain'
            for mesh in doc['meshes']:
                for p in mesh['primitives']:
                    attrs=p['attributes'];position=accessor(doc,binary,attrs['POSITION']);indices=accessor(doc,binary,p['indices']).reshape(-1)
                    verts+=len(position);tri=position[indices].reshape(-1,3,3);area=np.linalg.norm(np.cross(tri[:,1]-tri[:,0],tri[:,2]-tri[:,0]),axis=1);keep=area>1e-8
                    if not keep.any():continue
                    uv=accessor(doc,binary,attrs['TEXCOORD_0']);uvranges.append([float(uv.min()),float(uv.max())]);assert np.isfinite(uv).all()
                    world=tri[keep].copy();world[:,:,2]*=-1;world+=np.array(record['origin']);visible.append(world.mean(1))
                    if record['continuous_heat']:
                        a=doc['accessors'][attrs['_VALWOOD_LAVA_HEAT']];heat=accessor(doc,binary,attrs['_VALWOOD_LAVA_HEAT']).reshape(-1)
                        assert a['type']=='SCALAR'and a['componentType']==5126 and len(heat)==len(position)
                        assert np.isfinite(heat).all()and heat.min()>=0 and heat.max()<=1
                        assert '_VALWOOD_PAINT'not in attrs;heats.append(heat)
            actual=np.concatenate(visible);wanted=np.concatenate(expected[lod]);assert len(actual)==len(wanted),'Source triangle count changed'
            error=max(float(cKDTree(actual).query(wanted)[0].max()),float(cKDTree(wanted).query(actual)[0].max()))
            assert error<.001,'Source geometry moved'
            assert verts<=1000000 and len(actual)<=1000000 and len(doc['nodes'])<=512 and len(doc['images'])<=128 and file.stat().st_size<=128*1024**2
            factor=doc['materials'][0]['emissiveFactor'];strength=doc['materials'][0]['extensions']['KHR_materials_emissive_strength']['emissiveStrength'];assert max(factor)*strength<=32,'Unbounded source HDR emission remains'
            row={'file':str(file),'bytes':file.stat().st_size,'source_triangles':len(wanted),'visible_triangles':len(actual),'maximum_source_centroid_error_m':error,'vertices':verts,'one_shared_material':True,'uv_ranges':uvranges,'emissive_strength':strength}
            if heats:
                heat=np.concatenate(heats);row['heat_range']=[float(heat.min()),float(heat.max())];row['heat_percentiles']=np.percentile(heat,[10,50,90]).tolist()
            report.append(row)
    target=args.catalog.parent/'validation.json';target.write_text(json.dumps({'status':'PASS','files':report},indent=2)+'\n');print('PASS lava source geometry/triangles/pivots, continuous heat, one material, bounded HDR, native budgets;',len(report),'GLBs')

if __name__=='__main__':main()
