#!/usr/bin/env python3
"""Bake static battlefield corpse MID meshes with Blender, retaining exact PBR.

blender -b --python tools/cook_battlefield_corpse_lod.py -- SOURCE.glb MID.glb
Only geometry changes. Embedded images, materials, node transforms and pivot are
copied from SOURCE. The nearby SOURCE is never rewritten.
"""
import argparse
import copy
import hashlib
import json
from pathlib import Path
import struct
import sys

import bpy
import numpy as np
from mathutils import Vector
from mathutils.bvhtree import BVHTree
from mathutils.geometry import barycentric_transform


def sha(data):
    return hashlib.sha256(data).hexdigest()


def glb(path):
    data=path.read_bytes()
    magic,version,total=struct.unpack_from('<III',data)
    assert magic==0x46546c67 and version==2 and total==len(data)
    count,kind=struct.unpack_from('<II',data,12)
    assert kind==0x4e4f534a
    doc=json.loads(data[20:20+count])
    length,kind=struct.unpack_from('<II',data,20+count)
    assert kind==0x004e4942
    binary=data[28+count:28+count+length]
    return data,doc,binary


def accessor(doc,binary,index):
    a=doc['accessors'][index];v=doc['bufferViews'][a['bufferView']]
    assert 'sparse' not in a and v.get('byteStride',0)==0
    dtype={5126:'<f4',5125:'<u4',5123:'<u2'}[a['componentType']]
    width={'SCALAR':1,'VEC2':2,'VEC3':3,'VEC4':4}[a['type']]
    start=v.get('byteOffset',0)+a.get('byteOffset',0)
    return np.frombuffer(binary,dtype=dtype,count=a['count']*width,offset=start).reshape(-1,width).copy()


def main():
    args=sys.argv[sys.argv.index('--')+1:]
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source',type=Path);parser.add_argument('destination',type=Path)
    parser.add_argument('--target-tris',type=int,default=7900)
    options=parser.parse_args(args)
    source=options.source.resolve();destination=options.destination.absolute()
    assert source!=destination and not destination.is_symlink()
    raw,doc,binary=glb(source)
    assert len(doc['meshes'])==1 and len(doc['nodes'])==1 and doc['nodes'][0]=={'mesh':0}
    primitives=doc['meshes'][0]['primitives']
    source_positions=accessor(doc,binary,primitives[0]['attributes']['POSITION'])
    before=np.stack([source_positions.min(0),source_positions.max(0)])
    source_count=sum(doc['accessors'][p['indices']]['count']//3 for p in primitives)
    ratio=options.target_tris/source_count
    assert .05<=ratio<=.4
    outline_parts=[];outline_triangles=0
    for number,primitive in enumerate(primitives):
        index=accessor(doc,binary,primitive['indices']).reshape(-1)
        if source_positions[index,1].max()>before[1,1]-.025:
            outline_parts.append(number);outline_triangles+=len(index)//3
    outline_ratio=.5
    body_ratio=(options.target_tris-outline_triangles*outline_ratio)/(source_count-outline_triangles)
    assert .05<body_ratio<.4
    bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
    packed=[];spans=[];part_receipts=[]
    for number,primitive in enumerate(primitives):
        assert primitive.get('mode',4)==4
        attributes=primitive['attributes']
        position=accessor(doc,binary,attributes['POSITION'])
        normal=accessor(doc,binary,attributes['NORMAL'])
        uv=accessor(doc,binary,attributes['TEXCOORD_0'])
        triangles=accessor(doc,binary,primitive['indices']).reshape(-1,3)
        used,inverse=np.unique(triangles,return_inverse=True)
        position=position[used];normal=normal[used];uv=uv[used]
        triangles=inverse.reshape(-1,3)
        mesh=bpy.data.meshes.new('corpse_part_%02d'%number)
        mesh.from_pydata(position.tolist(),[],triangles.tolist());mesh.update()
        layer=mesh.uv_layers.new(name='UVMap')
        for loop in mesh.loops:layer.data[loop.index].uv=uv[loop.vertex_index]
        for polygon in mesh.polygons:polygon.use_smooth=True
        mesh.normals_split_custom_set_from_vertices(normal.tolist())
        obj=bpy.data.objects.new(mesh.name,mesh);bpy.context.collection.objects.link(obj)
        bpy.context.view_layer.objects.active=obj;obj.select_set(True)
        modifier=obj.modifiers.new('Bounded corpse MID collapse','DECIMATE')
        modifier.decimate_type='COLLAPSE';modifier.ratio=outline_ratio if number in outline_parts else body_ratio
        modifier.use_collapse_triangulate=True
        evaluated=obj.evaluated_get(bpy.context.evaluated_depsgraph_get())
        result=evaluated.to_mesh();result.calc_loop_triangles()
        tree=BVHTree.FromPolygons([Vector(p) for p in position],triangles.tolist(),all_triangles=True)
        output=[]
        for face in result.loop_triangles:
            for loop_index in face.loops:
                point=result.vertices[result.loops[loop_index].vertex_index].co
                nearest,_,index,distance=tree.find_nearest(point)
                assert index is not None and distance<.15
                indices=triangles[index]
                interpolated=barycentric_transform(nearest,
                    *[Vector(position[i]) for i in indices],
                    *[Vector(normal[i]) for i in indices])
                if interpolated.length<1.e-6:interpolated=Vector(normal[indices[0]])
                interpolated.normalize()
                tex=result.uv_layers[0].data[loop_index].uv
                output.append((*point,*interpolated,*tex))
        output=np.asarray(output,dtype='<f4')
        assert len(output)>0 and len(output)%3==0 and np.isfinite(output).all()
        output[:,6:8]=np.minimum(np.maximum(output[:,6:8],uv.min(0)),uv.max(0))
        part_receipts.append({'primitive':number,'material':primitive['material'],
            'source_triangles':len(triangles),'mid_triangles':len(output)//3})
        spans.append((len(packed),len(output),primitive['material']))
        packed.extend(output.tolist())
        evaluated.to_mesh_clear();obj.select_set(False)
    vertices=np.asarray(packed,dtype='<f4')
    # Quadric collapse may put a boundary point just below the original flat
    # support plane. Keep the original body's conservative culling envelope.
    vertices[:,:3]=np.minimum(np.maximum(vertices[:,:3],before[0]),before[1])
    after=np.stack([vertices[:,:3].min(0),vertices[:,:3].max(0)])
    # Preserve exact source support plane and culling bounds without moving the
    # object pivot. Collapse loss is tiny; refuse excessive silhouette changes.
    scale=(before[1]-before[0])/(after[1]-after[0])
    assert np.max(np.abs(scale-1))<.05,(before.tolist(),after.tolist(),scale.tolist())
    correction=np.max(np.abs((vertices[:,:3]-after[0])*scale+before[0]-vertices[:,:3]))
    assert correction<.075
    vertices[:,:3]=(vertices[:,:3]-after[0])*scale+before[0]
    vertices[:,3:6]/=scale
    vertices[:,3:6]/=np.linalg.norm(vertices[:,3:6],axis=1)[:,None]
    result=copy.deepcopy(doc)
    result['asset']['generator']='Valwood bounded static corpse MID, exact owned PBR'
    result['bufferViews']=[];result['accessors']=[];result['meshes'][0]['primitives']=[]
    output=bytearray()
    def view(data,target=None):
        output.extend(b'\0'*(-len(output)%4));offset=len(output);output.extend(data)
        entry={'buffer':0,'byteOffset':offset,'byteLength':len(data)}
        if target:entry['target']=target
        result['bufferViews'].append(entry);return len(result['bufferViews'])-1
    attrs={}
    for name,array in [('POSITION',vertices[:,:3]),('NORMAL',vertices[:,3:6]),('TEXCOORD_0',vertices[:,6:8])]:
        array=np.ascontiguousarray(array,dtype='<f4')
        entry={'bufferView':view(array.tobytes(),34962),'componentType':5126,
            'count':len(array),'type':'VEC2'if array.shape[1]==2 else'VEC3'}
        if name=='POSITION':entry.update(min=before[0].tolist(),max=before[1].tolist())
        result['accessors'].append(entry);attrs[name]=len(result['accessors'])-1
    indices=np.arange(len(vertices),dtype='<u4')
    index_view=view(indices.tobytes(),34963)
    for first,count,material in spans:
        result['accessors'].append({'bufferView':index_view,'byteOffset':first*4,
            'componentType':5125,'count':count,'type':'SCALAR'})
        result['meshes'][0]['primitives'].append({'attributes':dict(attrs),
            'indices':len(result['accessors'])-1,'material':material,'mode':4})
    images=[]
    for image in doc['images']:
        value=doc['bufferViews'][image['bufferView']]
        encoded=binary[value.get('byteOffset',0):value.get('byteOffset',0)+value['byteLength']]
        images.append(sha(encoded))
        result['images'][len(images)-1]['bufferView']=view(encoded)
    result['buffers']=[{'byteLength':len(output)}]
    encoded=json.dumps(result,separators=(',',':')).encode();encoded+=b' '*(-len(encoded)%4)
    output+=b'\0'*(-len(output)%4)
    data=struct.pack('<III',0x46546c67,2,28+len(encoded)+len(output))+struct.pack('<II',len(encoded),0x4e4f534a)+encoded+struct.pack('<II',len(output),0x004e4942)+output
    destination.parent.mkdir(parents=True,exist_ok=True);destination.write_bytes(data)
    receipt={'source':str(source),'source_sha256':sha(raw),'destination':str(destination),
        'sha256':sha(data),'source_triangles':source_count,'mid_triangles':len(vertices)//3,
        'source_bounds':before.tolist(),'mid_bounds':[vertices[:,:3].min(0).tolist(),vertices[:,:3].max(0).tolist()],
        'bounds_affine_scale':scale.tolist(),'max_bounds_correction_m':float(correction),
        'embedded_image_sha256':images,'materials_preserved_exactly':result['materials']==doc['materials'],
        'nodes_preserved_exactly':result['nodes']==doc['nodes'],'primitive_receipts':part_receipts}
    destination.with_suffix('.lod.json').write_text(json.dumps(receipt,indent=2)+'\n')
    print(json.dumps({key:receipt[key]for key in ('source_triangles','mid_triangles','sha256','max_bounds_correction_m')}))


if __name__=='__main__':main()
