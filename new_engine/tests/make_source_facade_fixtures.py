#!/usr/bin/env python3
"""Reproduce tiny authored layer/invalid-input GLB regression fixtures."""
import io,json,struct
from pathlib import Path
from PIL import Image

DEST=Path(__file__).resolve().parent/'fixtures'/'source_facade'
DEST.mkdir(parents=True,exist_ok=True)
for name,mode,paint in [('wood_zero',1,0.),('wood_worn',1,1.),('plaster',2,1.),('bad_mode',3,1.),('bad_paint',1,1.1),('missing_paint',1,1.),('bad_index',1,1.),('bad_scale',1,1.),('bad_version',1,1.)]:
    binary=bytearray();views=[];accessors=[]
    def append(data):
        binary.extend(b'\0'*((-len(binary))%4));i=len(views);views.append(dict(buffer=0,byteOffset=len(binary),byteLength=len(data)));binary.extend(data);return i
    def floats(values,typ,count,**extra):
        i=len(accessors);accessors.append(dict(bufferView=append(struct.pack('<'+'f'*len(values),*values)),componentType=5126,type=typ,count=count,**extra));return i
    attrs=dict(POSITION=floats([-.8,0,0,.8,0,0,0,1.2,0],'VEC3',3,min=[-.8,0,0],max=[.8,1.2,0]),NORMAL=floats([0,0,1]*3,'VEC3',3),TEXCOORD_0=floats([.25,.25]*3,'VEC2',3),TANGENT=floats([1,0,0,1]*3,'VEC4',3))
    if name!='missing_paint':attrs['_VALWOOD_PAINT']=floats([paint]*3,'SCALAR',3)
    indices=len(accessors);accessors.append(dict(bufferView=append(struct.pack('<3H',0,1,2)),componentType=5123,type='SCALAR',count=3))
    images=[]
    for color in [(240,30,20,0),(20,240,30,255),(128,128,255,255),(255,160,0,255),(255,255,255,255)]:
        stream=io.BytesIO();Image.new('RGBA',(16,16),color).save(stream,format='PNG')
        images.append(dict(bufferView=append(stream.getvalue()),mimeType='image/png'))
    params=[1,mode,1,4,-1,1,1,2] if mode!=2 else [1,2,2,3,4,1.2,1.5,1.2]
    if name=='bad_index':params[2]=99
    if name=='bad_scale':params[5]=0
    if name=='bad_version':params[0]=2
    material=dict(alphaMode='OPAQUE',pbrMetallicRoughness=dict(baseColorTexture=dict(index=0),metallicRoughnessTexture=dict(index=3),metallicFactor=1,roughnessFactor=1),normalTexture=dict(index=2),extensions={'VALWOOD_materials_facade':dict(parameters=params)})
    doc=dict(asset=dict(version='2.0'),scene=0,scenes=[dict(nodes=[0])],nodes=[dict(mesh=0)],buffers=[dict(byteLength=len(binary))],bufferViews=views,accessors=accessors,images=images,textures=[dict(source=i) for i in range(len(images))],materials=[material],meshes=[dict(primitives=[dict(attributes=attrs,indices=indices,material=0)])],extensionsUsed=['VALWOOD_materials_facade'])
    text=json.dumps(doc,separators=(',',':')).encode();text+=b' '*((-len(text))%4);binary+=b'\0'*((-len(binary))%4)
    encoded=struct.pack('<III',0x46546c67,2,28+len(text)+len(binary))+struct.pack('<II',len(text),0x4e4f534a)+text+struct.pack('<II',len(binary),0x004e4942)+binary
    (DEST/(name+'.glb')).write_bytes(encoded)
