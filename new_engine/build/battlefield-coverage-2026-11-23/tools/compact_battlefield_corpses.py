#!/usr/bin/env python3
"""Losslessly compact the four static corpse GLBs for per-primitive loaders.

Back up every preimage in review/corpse-compaction/preimages. Preserve triangle
order/winding and every indexed attribute bit, plus exact PBR/embedded images.
No engine source, world settings, placements, collisions or quality are edited.
"""
from pathlib import Path
import copy
import hashlib
import json
import os
import struct
import numpy as np

N=Path(__file__).resolve().parents[1]
OUT=N/'review/corpse-compaction'
DTYPE={5120:'i1',5121:'u1',5122:'<i2',5123:'<u2',5125:'<u4',5126:'<f4'}
WIDTH={'SCALAR':1,'VEC2':2,'VEC3':3,'VEC4':4,'MAT2':4,'MAT3':9,'MAT4':16}


def sha(data):return hashlib.sha256(data).hexdigest()


def load(data):
    assert struct.unpack_from('<III',data)==(0x46546c67,2,len(data))
    size,kind=struct.unpack_from('<II',data,12);assert kind==0x4e4f534a
    doc=json.loads(data[20:20+size]);length,kind=struct.unpack_from('<II',data,20+size)
    assert kind==0x004e4942
    binary=data[28+size:28+size+length]
    return doc,binary


def access(doc,binary,index):
    item=doc['accessors'][index];view=doc['bufferViews'][item['bufferView']]
    assert 'sparse'not in item
    dtype=np.dtype(DTYPE[item['componentType']]);width=WIDTH[item['type']]
    stride=view.get('byteStride',dtype.itemsize*width)
    return np.ndarray((item['count'],width),dtype=dtype,buffer=binary,
        offset=view.get('byteOffset',0)+item.get('byteOffset',0),
        strides=(stride,dtype.itemsize)).copy()


def images(doc,binary):
    encoded=[]
    for image in doc['images']:
        assert 'uri'not in image
        view=doc['bufferViews'][image['bufferView']];first=view.get('byteOffset',0)
        encoded.append(binary[first:first+view['byteLength']])
    return encoded


def validate(before,after):
    a,ab=load(before);b,bb=load(after)
    assert len(a['meshes'])==len(b['meshes'])==1
    for key in a:
        if key not in ('accessors','bufferViews','buffers','meshes','images'):
            assert a[key]==b[key],key
    assert [{k:v for k,v in i.items()if k!='bufferView'}for i in a['images']]==[
        {k:v for k,v in i.items()if k!='bufferView'}for i in b['images']]
    assert images(a,ab)==images(b,bb)
    ap=a['meshes'][0]['primitives'];bp=b['meshes'][0]['primitives']
    assert len(ap)==len(bp)==23
    assert {k:v for k,v in a['meshes'][0].items()if k!='primitives'}=={
        k:v for k,v in b['meshes'][0].items()if k!='primitives'}
    old_vertices=new_vertices=indices=0;bounds=[[],[]];corners=[]
    for number,(pa,pb)in enumerate(zip(ap,bp)):
        assert {k:v for k,v in pa.items()if k not in ('attributes','indices')}=={
            k:v for k,v in pb.items()if k not in ('attributes','indices')}
        assert pa['attributes'].keys()==pb['attributes'].keys()
        ia=access(a,ab,pa['indices']).reshape(-1);ib=access(b,bb,pb['indices']).reshape(-1)
        assert len(ia)==len(ib)and len(ia)%3==0;indices+=len(ia)
        digest=hashlib.sha256()
        for name in pa['attributes']:
            xa=access(a,ab,pa['attributes'][name]);xb=access(b,bb,pb['attributes'][name])
            aa=a['accessors'][pa['attributes'][name]];ba=b['accessors'][pb['attributes'][name]]
            for key in ('componentType','type','normalized'):
                assert aa.get(key)==ba.get(key)
            assert ia.max()<len(xa)and ib.max()<len(xb)
            ca=xa[ia].tobytes();cb=xb[ib].tobytes()
            # Exact ordered triangle-corner equality is stronger than a
            # multiset check: winding, repeated triangles and order agree too.
            assert ca==cb,(number,name)
            digest.update(name.encode());digest.update(ca)
            if name=='POSITION':
                assert np.isfinite(xa).all()and np.isfinite(xb).all()
                old_vertices+=len(xa);new_vertices+=len(xb)
                bounds[0].append(xa[ia]);bounds[1].append(xb[ib])
        corners.append(digest.hexdigest())
    actual=[]
    for chunks in bounds:
        value=np.concatenate(chunks);actual.append([value.min(0).tolist(),value.max(0).tolist()])
    assert actual[0]==actual[1]
    return {'triangles':indices//3,'primitives':len(ap),'source_loader_vertices':old_vertices,
        'compact_loader_vertices':new_vertices,'source_loader_vbo_bytes':old_vertices*68,
        'compact_loader_vbo_bytes':new_vertices*68,'ebo_bytes_unchanged':indices*4,
        'vbo_bytes_saved':(old_vertices-new_vertices)*68,'same_exact_bounds':actual[1],
        'ordered_triangle_attribute_sha256':corners,'all_triangle_attributes_bit_identical':True,
        'same_54_embedded_image_bytes':len(images(a,ab))==54,'metadata_pbr_samplers_nodes_identical':True}


def compact(raw):
    source,binary=load(raw)
    assert not source.get('skins')and not source.get('animations')
    doc=copy.deepcopy(source);doc['accessors']=[];doc['bufferViews']=[]
    output=bytearray()
    def view(data,target=None):
        output.extend(b'\0'*(-len(output)%4));offset=len(output);output.extend(data)
        result={'buffer':0,'byteOffset':offset,'byteLength':len(data)}
        if target:result['target']=target
        doc['bufferViews'].append(result);return len(doc['bufferViews'])-1
    for mesh in doc['meshes']:
        for primitive in mesh['primitives']:
            old_index=primitive['indices'];indices=access(source,binary,old_index).reshape(-1)
            used,remap=np.unique(indices,return_inverse=True)
            for name,old in list(primitive['attributes'].items()):
                values=access(source,binary,old)[used]
                item=copy.deepcopy(source['accessors'][old]);item.pop('byteOffset',None)
                item['count']=len(values);item['bufferView']=view(values.tobytes(),34962)
                if 'min'in item:item['min']=values.min(0).tolist()
                if 'max'in item:item['max']=values.max(0).tolist()
                doc['accessors'].append(item);primitive['attributes'][name]=len(doc['accessors'])-1
            item=copy.deepcopy(source['accessors'][old_index]);item.pop('byteOffset',None)
            encoded=np.asarray(remap,dtype=DTYPE[item['componentType']])
            item['bufferView']=view(encoded.tobytes(),34963)
            if 'min'in item:item['min']=[int(encoded.min())]
            if 'max'in item:item['max']=[int(encoded.max())]
            doc['accessors'].append(item);primitive['indices']=len(doc['accessors'])-1
    for image,encoded in zip(doc['images'],images(source,binary)):
        image['bufferView']=view(encoded)
    doc['buffers']=[{'byteLength':len(output)}]
    encoded=json.dumps(doc,separators=(',',':')).encode();encoded+=b' '*(-len(encoded)%4)
    output+=b'\0'*(-len(output)%4)
    result=struct.pack('<III',0x46546c67,2,28+len(encoded)+len(output))+struct.pack('<II',len(encoded),0x4e4f534a)+encoded+struct.pack('<II',len(output),0x004e4942)+output
    validate(raw,result)
    return result


def main():
    OUT.mkdir(exist_ok=True);preimages=OUT/'preimages';preimages.mkdir(exist_ok=True)
    receipt=OUT/'receipt.json'
    previous=json.loads(receipt.read_text())if receipt.exists()else None
    if previous:
        for entry in previous['assets']:
            path=N/entry['relative_path'];backup=preimages/path.name
            assert sha(path.read_bytes())==entry['compact_sha256']
            assert sha(backup.read_bytes())==entry['preimage_sha256']
            validate(backup.read_bytes(),path.read_bytes())
        print(json.dumps({'passed':True,'mode':'already_compacted_verified'}));return
    staged=[]
    for suffix in ('back','side','back_mid','side_mid'):
        path=N/'world-project/models/battlefield'/('corpse_'+suffix+'.glb')
        assert path.is_file()and not path.is_symlink()
        raw=path.read_bytes();backup=preimages/path.name
        if backup.exists():assert backup.read_bytes()==raw
        else:backup.write_bytes(raw)
        result=compact(raw);report=validate(raw,result)
        report.update(relative_path=str(path.relative_to(N)),preimage_sha256=sha(raw),
            compact_sha256=sha(result),preimage_bytes=len(raw),compact_bytes=len(result))
        staged.append((path,result,report))
    # All four complete validated preimages exist before replacing any asset.
    for path,data,_ in staged:
        temporary=path.with_suffix('.compact.tmp');temporary.write_bytes(data);os.replace(temporary,path)
    result={'passed':True,'lossless':True,'engine_source_edited':False,'assets':[r for _,_,r in staged],
        'vbo_bytes_saved_four_color_assets':sum(r['vbo_bytes_saved']for _,_,r in staged),
        'native_visual_and_performance_acceptance_required':True}
    receipt.write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps({key:value for key,value in result.items()if key!='assets'}))
    for _,_,report in staged:print(json.dumps({key:report[key]for key in ('relative_path','triangles',
        'source_loader_vertices','compact_loader_vertices','vbo_bytes_saved','compact_sha256')}))


if __name__=='__main__':main()
