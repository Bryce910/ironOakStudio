"""Bake shared KHR_texture_transform UV0 transforms for runtime-compatible GLBs.
The source files remain unchanged. Mismatched per-channel transforms are rejected
instead of silently producing incorrect materials. No geometry is regenerated.
"""
import json
import math
from pathlib import Path
import struct

def bake_uv_transforms(path):
    path=Path(path);data=path.read_bytes()
    magic,version,length=struct.unpack_from('<III',data)
    if magic!=0x46546c67 or version!=2 or length!=len(data):raise ValueError('Invalid GLB')
    json_length,json_kind=struct.unpack_from('<II',data,12)
    if json_kind!=0x4e4f534a:raise ValueError('Missing JSON chunk')
    gltf=json.loads(data[20:20+json_length]);offset=20+json_length
    bin_length,bin_kind=struct.unpack_from('<II',data,offset)
    if bin_kind!=0x004e4942:raise ValueError('Missing BIN chunk')
    binary=bytearray(data[offset+8:offset+8+bin_length]);changed=0
    identity={'offset':[0,0],'scale':[1,1],'rotation':0.0}
    for mesh in gltf.get('meshes',[]):
        for primitive in mesh['primitives']:
            if 'material' not in primitive:continue
            material=gltf['materials'][primitive['material']];pbr=material.get('pbrMetallicRoughness',{})
            views=[v for v in [pbr.get('baseColorTexture'),pbr.get('metallicRoughnessTexture'),material.get('normalTexture'),material.get('emissiveTexture')] if v]
            transforms=[]
            for view in views:
                transform={**identity,**view.get('extensions',{}).get('KHR_texture_transform',{})}
                if transform.get('texCoord',view.get('texCoord',0))!=0:raise ValueError('UV1 requires a separate runtime path')
                transforms.append(transform)
            if not transforms or all(t==identity for t in transforms):continue
            transform=transforms[0]
            if any(t!=transform for t in transforms):raise ValueError('Different channel transforms require runtime support')
            accessor=gltf['accessors'][primitive['attributes']['TEXCOORD_0']]
            if accessor['componentType']!=5126 or accessor['type']!='VEC2' or 'sparse' in accessor:raise ValueError('Expected dense float UV0')
            view=gltf['bufferViews'][accessor['bufferView']];stride=view.get('byteStride',8)
            begin=view.get('byteOffset',0)+accessor.get('byteOffset',0)
            uv_offset,scale,rotation=transform['offset'],transform['scale'],transform['rotation'];cosine,sine=math.cos(rotation),math.sin(rotation)
            new_offset=len(binary);assert new_offset%4==0
            for i in range(accessor['count']):
                u,v=struct.unpack_from('<ff',binary,begin+i*stride);u*=scale[0];v*=scale[1]
                binary.extend(struct.pack('<ff',uv_offset[0]+cosine*u-sine*v,uv_offset[1]+sine*u+cosine*v))
            gltf['bufferViews'].append({'buffer':0,'byteOffset':new_offset,'byteLength':len(binary)-new_offset,'target':34962})
            gltf['accessors'].append({'bufferView':len(gltf['bufferViews'])-1,'componentType':5126,'count':accessor['count'],'type':'VEC2'})
            primitive['attributes']['TEXCOORD_0']=len(gltf['accessors'])-1;changed+=1
    if not changed:return 0
    for material in gltf.get('materials',[]):
        pbr=material.get('pbrMetallicRoughness',{})
        for view in [pbr.get('baseColorTexture'),pbr.get('metallicRoughnessTexture'),material.get('normalTexture'),material.get('emissiveTexture')]:
            if view and 'extensions' in view:
                view['extensions'].pop('KHR_texture_transform',None)
                if not view['extensions']:view.pop('extensions')
    # Other extensions are preserved; remove the declaration only when unused.
    if 'KHR_texture_transform' not in json.dumps(gltf.get('materials',[])):
        for key in ['extensionsUsed','extensionsRequired']:
            if key in gltf:gltf[key]=[x for x in gltf[key] if x!='KHR_texture_transform']
    gltf['buffers'][0]['byteLength']=len(binary)
    encoded=json.dumps(gltf,separators=(',',':')).encode();encoded+=b' '*((-len(encoded))%4)
    binary+=b'\0'*((-len(binary))%4)
    result=struct.pack('<III',magic,version,12+8+len(encoded)+8+len(binary))+struct.pack('<II',len(encoded),json_kind)+encoded+struct.pack('<II',len(binary),bin_kind)+binary
    path.write_bytes(result);return changed

if __name__=='__main__':
    import sys
    for path in sys.argv[1:]:print(path,bake_uv_transforms(path),'UV transforms baked')
