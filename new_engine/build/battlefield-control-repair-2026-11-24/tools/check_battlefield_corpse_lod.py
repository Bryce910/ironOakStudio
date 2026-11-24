#!/usr/bin/env python3
"""Validate corpse MID geometry/PBR identity and six-view projected silhouettes."""
from pathlib import Path
import hashlib
import json
import struct
import numpy as np
from PIL import Image,ImageDraw

N=Path(__file__).resolve().parents[1]
OUT=N/'review/corpse-lod'
OUT.mkdir(exist_ok=True)


def load(path):
    data=path.read_bytes();assert struct.unpack_from('<III',data)==(0x46546c67,2,len(data))
    length,kind=struct.unpack_from('<II',data,12);assert kind==0x4e4f534a
    doc=json.loads(data[20:20+length]);size,kind=struct.unpack_from('<II',data,20+length)
    assert kind==0x004e4942
    binary=data[28+length:28+length+size]
    def access(index):
        a=doc['accessors'][index];v=doc['bufferViews'][a['bufferView']]
        width={'SCALAR':1,'VEC2':2,'VEC3':3}[a['type']]
        dtype={5126:'<f4',5125:'<u4'}[a['componentType']]
        return np.frombuffer(binary,dtype=dtype,count=a['count']*width,
            offset=v.get('byteOffset',0)+a.get('byteOffset',0)).reshape(-1,width)
    primitives=doc['meshes'][0]['primitives']
    positions=[];normal_chunks=[];uv_chunks=[];triangle_chunks=[];offset=0
    for primitive in primitives:
        attrs=primitive['attributes'];part=access(attrs['POSITION'])
        positions.append(part);normal_chunks.append(access(attrs['NORMAL']));uv_chunks.append(access(attrs['TEXCOORD_0']))
        triangle_chunks.append(access(primitive['indices']).reshape(-1,3)+offset);offset+=len(part)
    position=np.concatenate(positions);normals=np.concatenate(normal_chunks)
    uv=np.concatenate(uv_chunks);triangles=np.concatenate(triangle_chunks)
    assert np.isfinite(position).all()and np.isfinite(normals).all()and np.isfinite(uv).all()
    assert len(triangles)>0 and triangles.min()>=0 and triangles.max()<len(position)
    assert np.max(np.abs(np.linalg.norm(normals,axis=1)-1))<1.e-4
    def images():
        hashes=[]
        for image in doc['images']:
            v=doc['bufferViews'][image['bufferView']];offset=v.get('byteOffset',0)
            hashes.append(hashlib.sha256(binary[offset:offset+v['byteLength']]).hexdigest())
        return hashes
    return dict(doc=doc,positions=position,normals=normals,uv=uv,triangles=triangles,
        image_hashes=images(),sha256=hashlib.sha256(data).hexdigest())


def project(asset,direction,lo,hi):
    forward=np.asarray(direction,dtype=float);forward/=np.linalg.norm(forward)
    up=np.array([0.,1.,0.])if abs(forward[1])<.9 else np.array([0.,0.,1.])
    right=np.cross(up,forward);right/=np.linalg.norm(right);up=np.cross(forward,right)
    basis=np.array([right,up]);centre=(lo+hi)*.5
    radius=np.linalg.norm(hi-lo)*.55
    xy=(asset['positions']-centre)@basis.T
    xy=xy*(220/(2*radius))+128;xy[:,1]=256-xy[:,1]
    mask=Image.new('1',(256,256));draw=ImageDraw.Draw(mask)
    for tri in asset['triangles']:draw.polygon([tuple(v)for v in xy[tri]],fill=1)
    return np.asarray(mask,dtype=bool)


reports=[];sheet=Image.new('RGB',(6*256,2*282),(24,26,28));label=ImageDraw.Draw(sheet)
compaction_path=N/'review/corpse-compaction/receipt.json'
compaction=json.loads(compaction_path.read_text())if compaction_path.exists()else None
compacted={Path(e['relative_path']).name:e for e in compaction['assets']}if compaction else{}
directions=[(0,1,0),(0,0,1),(1,0,0),(1,.6,1),(-1,.6,1),(1,.6,-1)]
for row,name in enumerate(('back','side')):
    base=N/'world-project/models/battlefield'/('corpse_'+name)
    near=load(base.with_suffix('.glb'));mid=load(base.with_name(base.name+'_mid').with_suffix('.glb'))
    receipt=json.loads(base.with_name(base.name+'_mid').with_suffix('.lod.json').read_text())
    for asset,original,path in ((near,receipt['source_sha256'],base.with_suffix('.glb')),
            (mid,receipt['sha256'],base.with_name(base.name+'_mid').with_suffix('.glb'))):
        if asset['sha256']!=original:
            proof=compacted[path.name]
            assert proof['preimage_sha256']==original and proof['compact_sha256']==asset['sha256']
            assert proof['all_triangle_attributes_bit_identical']
            backup=N/'review/corpse-compaction/preimages'/path.name
            assert hashlib.sha256(backup.read_bytes()).hexdigest()==original
    assert 5000<=len(mid['triangles'])<=8000 and len(near['triangles'])==50336
    assert len(mid['doc']['meshes'][0]['primitives'])==23
    assert [p['material']for p in near['doc']['meshes'][0]['primitives']]==[p['material']for p in mid['doc']['meshes'][0]['primitives']]
    for key in ('materials','textures','samplers','nodes','scenes','scene'):
        assert near['doc'].get(key)==mid['doc'].get(key),key
    assert near['image_hashes']==mid['image_hashes']and len(mid['image_hashes'])==54
    before=np.stack([near['positions'].min(0),near['positions'].max(0)])
    after=np.stack([mid['positions'].min(0),mid['positions'].max(0)])
    assert np.max(np.abs(before-after))<1.e-6 and after[0,1]==0
    # UVs may interpolate across a collapsed triangle but must stay within the
    # source texture domain, with no sampler/material remapping.
    assert np.all(mid['uv'].min(0)>=near['uv'].min(0)-1.e-5)
    assert np.all(mid['uv'].max(0)<=near['uv'].max(0)+1.e-5)
    scores=[]
    for col,direction in enumerate(directions):
        a=project(near,direction,*before);b=project(mid,direction,*before)
        score=float((a&b).sum()/(a|b).sum());scores.append(score)
        overlay=np.full((256,256,3),24,dtype=np.uint8)
        overlay[a&b]=[185,193,200];overlay[a&~b]=[225,85,70];overlay[b&~a]=[65,165,230]
        sheet.paste(Image.fromarray(overlay),(col*256,row*282))
        label.text((col*256+8,row*282+259),f'{name} view{col+1} IoU {score:.4f}',fill='white')
    assert min(scores)>.93,(name,scores)
    reports.append({'variant':name,'near_sha256':near['sha256'],'mid_sha256':mid['sha256'],
        'near_triangles':len(near['triangles']),'mid_triangles':len(mid['triangles']),
        'geometry_reduction_percent':100*(1-len(mid['triangles'])/len(near['triangles'])),
        'bounds_max_difference_m':float(np.max(np.abs(before-after))),
        'same_materials_samplers_and_54_images':True,'same_nodes_pivot':True,
        'unit_normals_finite_uv_valid_indices':True,'silhouette_IoU_256px_six_views':scores})
sheet.save(OUT/'silhouette-comparison.png')
world=(N/'world-project/world.studio').read_text().splitlines()
for name in ('back','side'):
    alias='battlefield_corpse_'+name
    rows=[line.split()for line in world if line.startswith('asset '+alias+' ')]
    assert rows==[['asset',alias,'battlefield/corpse_'+name,'2','0','0','140','19','1']]
result={'passed':True,'reports':reports,'fade_interval_metres':[15,23],
    'quality_fade_intervals_metres':{'Low':[9.75,14.95],'Medium':[15,23],
        'High':[22.5,34.5],'Ultra':[30,46]},
    'collision_definitions_unchanged_none':True,'source_near_rendered_triangles_unchanged':True,
    'source_near_files_unchanged':not bool(compaction),'lossless_per_primitive_compaction':bool(compaction),
    'native_visual_and_performance_acceptance_required':True}
(OUT/'validation.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps(result))
