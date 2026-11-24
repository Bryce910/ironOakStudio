#!/usr/bin/env python3
"""CPU-only source articulation, shared texture/topology and seat-contact checks."""
import hashlib
import json
import math
import re
from pathlib import Path
import struct
import numpy as np

ROOT=Path(__file__).resolve().parents[1]
MODELS=ROOT/'projects/valwood_world/models'


def load(path):
    raw=path.read_bytes();assert raw[:4]==b'glTF'
    size=struct.unpack_from('<I',raw,12)[0]
    return json.loads(raw[20:20+size]),raw[28+size:]


def accessor(data,binary,index):
    a=data['accessors'][index];v=data['bufferViews'][a['bufferView']]
    dtype={5126:'<f4',5123:'<u2',5125:'<u4'}[a['componentType']]
    count={'SCALAR':1,'VEC2':2,'VEC3':3,'VEC4':4}[a['type']]
    stride=v.get('byteStride',np.dtype(dtype).itemsize*count)
    return np.ndarray((a['count'],count),dtype=dtype,buffer=binary,
        offset=v.get('byteOffset',0)+a.get('byteOffset',0),strides=(stride,np.dtype(dtype).itemsize)).copy()


def clip(poly,axis,value,sign):
    out=[]
    for a,b in zip(poly,poly[1:]+poly[:1]):
        da=(a[axis]-value)*sign;db=(b[axis]-value)*sign
        if da>=0:out.append(a)
        if da*db<0:out.append(a+(b-a)*(da/(da-db)))
    return out


def seat_fit(points,triangles,forward):
    points=points.copy();points[:,2]+=forward
    clipped=[]
    for indices in triangles:
        poly=list(points[indices])
        for axis,value,sign in [(0,-1.6,1),(0,1.6,-1),(2,-.23,1),(2,.23,-1)]:
            if poly:poly=clip(poly,axis,value,sign)
        clipped.extend(poly)
    assert clipped
    gap=min(p[1]for p in clipped)+.015-.495
    assert -.002<gap<.002,(forward,gap)
    return gap


def runtime_uv_contract():
    """Audit the real public source registry, reading only each GLB JSON header."""
    header=(ROOT/'engine/world_village_population.h').read_text()
    appearances=re.findall(r'\{"([^"]+)",(\d+),(\d+),(\d+),(\d+)(?:,"([^"]+)")?\}',header)
    assert len(appearances)==10
    count=0
    def inspect(node,path):
        if isinstance(node,dict):
            for key,value in node.items():
                if key.endswith('Texture')and isinstance(value,dict)and 'index'in value:
                    assert value.get('texCoord',0)==0 and not value.get('extensions'),(path,key,value)
                inspect(value,path)
        elif isinstance(node,list):
            for value in node:inspect(value,path)
    for name,walk,idle,_,_,prefix in appearances:
        for mode,n in [('walk',int(walk)),('idle',int(idle))]:
            for frame in range(n):
                path=MODELS/f'{prefix or "village_character"}_{name}_{mode}_{frame:02}.glb'
                with path.open('rb')as stream:
                    prefix_bytes=stream.read(20);size=struct.unpack_from('<I',prefix_bytes,12)[0]
                    data=json.loads(stream.read(size))
                inspect(data['materials'],path.name);count+=1
    assert count==80
    print('PASS runtime registry UV contract: all80 pose assets/material texture infos use TEXCOORD_0 with no texture transforms; offline corner IDs are never a rendered texture mapping')


def main():
    runtime_uv_contract()
    manifest=json.loads((MODELS/'village_seated_manifest.json').read_text())
    assert manifest['license']=='CC0-1.0'
    assert manifest['source_sha256']==hashlib.sha256((ROOT/manifest['source']).read_bytes()).hexdigest()
    assert len(manifest['assets'])==4 and manifest['new_rgba8_mips_upper_bound_bytes']<=16*1024*1024
    positions=[];reference_uv=None;reference_indices=None;reference_images=None;reference_material=None
    for r in manifest['assets']:
        path=MODELS/r['file'];assert hashlib.sha256(path.read_bytes()).hexdigest()==r['sha256']
        data,binary=load(path)
        assert not data.get('skins')and not data.get('animations')and len(data['nodes'])==1
        assert set(data['nodes'][0])=={'mesh','name'} # Identity root; actual +Z forward.
        primitives=[p for m in data['meshes']for p in m['primitives']]
        assert len(primitives)==1 and r['triangles']==3884
        p=primitives[0];verts=accessor(data,binary,p['attributes']['POSITION'])
        normals=accessor(data,binary,p['attributes']['NORMAL'])
        uv=accessor(data,binary,p['attributes']['TEXCOORD_0'])
        ids=accessor(data,binary,p['attributes']['TEXCOORD_1'])
        assert np.std(uv[:,1])>.05 and np.all(ids[:,1]==.5)and not np.array_equal(uv,ids)
        indices=accessor(data,binary,p['indices'])
        assert np.isfinite(verts).all()and np.isfinite(normals).all()
        assert np.max(np.abs(np.linalg.norm(normals,axis=1)-1))<.002
        assert 0<=verts[:,1].min()<.002 and 1.55<verts[:,1].max()<1.65
        assert verts[:,0].min()>-.4 and verts[:,0].max()<.4
        assert verts[:,2].min()>-.22 and .62<verts[:,2].max()<.66
        posterior=verts[(np.abs(verts[:,0])<.193)&(np.abs(verts[:,2])<.152)&(verts[:,1]>.28)]
        assert len(posterior)>0 and abs(posterior[:,1].min()+.015-.495)<.004
        material=data['materials'][0];pbr=material['pbrMetallicRoughness']
        assert material['normalTexture']and material['occlusionTexture']and pbr['baseColorTexture']and pbr['metallicRoughnessTexture']
        for info in [material['normalTexture'],material['occlusionTexture'],pbr['baseColorTexture'],pbr['metallicRoughnessTexture']]:
            assert info.get('texCoord',0)==0 and not info.get('extensions'),(r['file'],info)
        if reference_material is None:reference_material=material
        else:assert material==reference_material
        images=[]
        for image in data['images']:
            view=data['bufferViews'][image['bufferView']];start=view.get('byteOffset',0)
            encoded=binary[start:start+view['byteLength']]
            assert encoded[:8]==b'\x89PNG\r\n\x1a\n'and struct.unpack_from('>II',encoded,16)==(1024,1024)
            images.append(hashlib.sha256(encoded).hexdigest())
        if reference_uv is None:reference_uv=uv;reference_indices=indices;reference_images=images
        else:
            assert np.array_equal(uv,reference_uv)and np.array_equal(indices,reference_indices)
            assert images==reference_images
        # A rigidly tilted standing mesh cannot satisfy natural, bent knees
        # and two stable foot contacts from the original source skeleton.
        bones=r['source_bone_positions_blender_xyz']
        for side in ['L','R']:
            hip,knee,foot=[np.array(bones[name+'_'+side])for name in ['UpLeg','Leg','Foot']]
            thigh=knee-hip;calf=foot-knee
            angle=math.degrees(math.acos(float(np.clip(np.dot(thigh,calf)/np.linalg.norm(thigh)/np.linalg.norm(calf),-1,1))))
            assert 65<angle<115 and foot[2]<.08 and hip[2]>.65
        positions.append(verts)
    layout=json.loads((ROOT/'projects/valwood_world/scenery/village_life_layout.json').read_text())
    forward=layout['seated_root_forward_offset_m']
    triangles=reference_indices.reshape(-1,3)
    seat_gaps=[]
    for a,b in zip(positions,positions[1:]+positions[:1]):
        assert a.shape==b.shape and np.max(np.linalg.norm(a-b,axis=1))<.15
        seat_gaps.append(seat_fit(a,triangles,forward))
        # Affine blending preserves the measured floor and posterior bounds.
        for weight in [.25,.5,.75]:
            blend=a*(1-weight)+b*weight
            assert blend[:,1].min()>=-.00001 and blend[:,1].min()<.002
            seat_gaps.append(seat_fit(blend,triangles,forward))
    furniture=json.loads((MODELS/'castle_tavern_manifest.json').read_text())
    tavern=next(a for a in furniture['assets']if a['alias']=='village_tavern'and a['lod']=='near')
    for label,bench_z in [('left_table',2),('right_table',2)]:
        table=next(c['box']for c in tavern['collision_boxes']if c['label']==label)
        # Genuine hands/apron stay wholly before the actual tabletop front;
        # this also proves clearance for every affine between-key pose.
        assert max(float(v[:,2].max())for v in positions)+forward+bench_z<table[2]
    print('PASS actual furniture fit: clipped triangle support gap %.3f..%.3fmm over16 cyclic poses; full source body stays before both occupied tabletops'%(min(seat_gaps)*1000,max(seat_gaps)*1000))
    print('PASS seated source assets: genuine Sitting1,4 compatible3884-triangle keys; articulated65–115degree knees, grounded feet, .480m root-relative posterior+.015m floor=.495m seat support, shared diffuse/normal/AO atlas<=16MiB; no skins/animations/runtime rig')


if __name__=='__main__':main()
