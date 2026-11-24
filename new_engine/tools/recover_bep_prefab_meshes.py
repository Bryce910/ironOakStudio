#!/usr/bin/env python3
"""Recover exact embedded BEP Unity prefab meshes and their dependency closure.

Original Unity coordinates and mesh attributes remain unmodified. This is a
data-only source stage, not a GLB cooker or production project importer.
"""
from pathlib import Path
import argparse
import hashlib
import json
import sys

from audit_bep_unity_sources import Sources, OUT
import numpy as np
from UnityPy.helpers.MeshHelper import MeshHandler

DEFAULTS = [
    'c618fe5bfa375084691c1af3579755fe', # Oak_01
    '6a9e9884164f6924f8a16989015a5e28', # Oak_01 authored lower LOD
    '8ab88abcedd9d5f499a4869a492f6283', # Beech_03
    'cd5a7d581aab2174699751c55859dc96', # Beech_03 authored lower LOD
    '4384371648ebb954fb1f484651563807', # Bush01_01
    '2721ae6d86a0bfb4f99c0750de265625', # Bush03_01
    '803835cab50068744a1bc9d93f9a6604', # Fern_01 three-tier composition
    '0360caee8d17d794781b37335a4d88a4', # Reed_Plant_01 three-tier composition
    '683be07145cdf3a4aa2d304ad946a9b8', # Clover_01 two-tier composition
    '10b5e6c535500bb428915dabdd363307', # LongGrass_02
    '35f5c4027fbdbdb4683c5efa5353151d', # LongGrass_03
    'dba0ef41e3e74b849a75b78132c74ef3', # LongGrass_01
    'f9b84573e5a4f304b8148e8f10c40167', # DetailGrass_01 broad clump
    '85854064e9964d649b88d308c711d0cf', # DetailGrass_02
    'daf54308ceb25724ca7acc63707fb297', # DetailGrass_03
    'd65aef26594da8c488f9ed20bb70b3bf', # Rock_01 original four-tier wrapper
    '254a96ac4b785c040996a548e22a311f', # Rock_02 original four-tier wrapper
]


def attribute(values, n, dimensions, default):
    if values is None or not len(values):
        return np.tile(np.asarray(default,dtype=np.float32),(n,1))
    result=np.asarray(values,dtype=np.float32)[:,:dimensions]
    assert result.shape==(n,dimensions) and np.isfinite(result).all()
    return np.ascontiguousarray(result)


def recover(guids):
    source=Sources()
    dest=OUT/'prefab-meshes'
    dest.mkdir(exist_ok=True)
    seen=set();pending=list(guids);assets=[];unresolved=[];total=0
    while pending:
        guid=pending.pop(0)
        if guid in seen:continue
        seen.add(guid)
        row=source.rows.get(guid)
        if not row or not source.path(guid).exists():
            unresolved.append(dict(guid=guid,reason='Source asset not selectively recovered'));continue
        env=source.load(guid)
        objects=[];meshes=[];dependencies=set()
        for obj in env.objects:
            if obj.type.name=='Mesh':
                mesh=obj.read();handler=MeshHandler(mesh);handler.process()
                n=handler.m_VertexCount
                values=dict(position=attribute(handler.m_Vertices,n,3,[0,0,0]),
                            normal=attribute(handler.m_Normals,n,3,[0,1,0]),
                            uv0=attribute(handler.m_UV0,n,2,[0,0]),
                            uv1=attribute(handler.m_UV1,n,2,[0,0]),
                            tangent=attribute(handler.m_Tangents,n,4,[1,0,0,1]),
                            color=attribute(handler.m_Colors,n,4,[1,1,1,1]))
                parts=[]
                for slot,(sub,indices) in enumerate(zip(mesh.m_SubMeshes,handler.get_triangles())):
                    index=np.asarray(indices,dtype=np.int32).reshape(-1,3)
                    base=int(getattr(sub,'baseVertex',0)or 0)
                    index=index+base
                    assert not len(index)or(index.min()>=0 and index.max()<n)
                    values['indices_'+str(slot)]=index
                    parts.append(dict(slot=slot,triangles=len(index),base_vertex=base))
                path=dest/f'{guid}-{obj.path_id}.npz'
                np.savez_compressed(path,**values)
                total+=path.stat().st_size
                assert total<256*1024*1024,'Selected mesh recovery budget exceeded'
                position=values['position']
                native=position*np.array([1,1,-1],dtype=np.float32)
                meshes.append(dict(id=obj.path_id,name=mesh.m_Name,vertices=n,submeshes=parts,
                                   arrays=str(path.relative_to(OUT)),sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
                                   bounds_unity=[position.min(0).tolist(),position.max(0).tolist()],
                                   bounds_native_unpivoted=[native.min(0).tolist(),native.max(0).tolist()],
                                   vertex_color_min=values['color'].min(0).tolist(),
                                   vertex_color_max=values['color'].max(0).tolist(),
                                   vertex_color_mean=values['color'].mean(0).tolist(),
                                   source_attributes={k:bool(getattr(handler,k) is not None) for k in
                                                      ['m_Vertices','m_Normals','m_UV0','m_UV1','m_Tangents','m_Colors']}))
            elif obj.type.name in {'GameObject','Transform','MeshRenderer','MeshFilter','LODGroup','Material','Tree'}:
                data=source.refs(obj,obj.read_typetree())
                objects.append(dict(id=obj.path_id,type=obj.type.name,data=data))
                def refs(value):
                    if isinstance(value,dict):
                        if value.get('guid') and (value.get('path')or'').endswith('.prefab'):dependencies.add(value['guid'])
                        for v in value.values():refs(v)
                    elif isinstance(value,list):
                        for v in value:refs(v)
                refs(data)
        assets.append(dict(source=row,meshes=meshes,objects=objects,dependency_prefabs=sorted(dependencies)))
        pending.extend(sorted(dependencies-seen))
    report=dict(coordinates='Original Unity local X,Y,Z metres, Y up; no Z/UV/winding flips or re-grounding',
                npz_attributes='position Nx3, normal Nx3, uv0 Nx2, uv1 Nx2, tangent Nx4, color Nx4, indices_<materialSlot> Mx3',
                note='Tree vertex color/UV1 may encode wind, not diffuse tint. Exact original streams retained.',
                requested_guids=guids,assets=assets,unresolved=unresolved,
                unique_meshes=sum(len(r['meshes'])for r in assets),mesh_file_bytes=total)
    (dest/'manifest.json').write_text(json.dumps(report,indent=2)+'\n')
    print('READY',dest/'manifest.json',len(assets),'source assets',report['unique_meshes'],'meshes',len(unresolved),'unresolved',flush=True)


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('guids',nargs='*')
    args=parser.parse_args()
    recover(args.guids or DEFAULTS)
