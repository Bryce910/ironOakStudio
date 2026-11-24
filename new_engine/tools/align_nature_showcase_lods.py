#!/usr/bin/env python3
"""Keep exact source elevations when the native loader grounds each LOD.

A one-vertex degenerate triangle carries a shared, invisible coordinate datum.
It guarantees the same minimumY in both files without altering visible geometry.
"""
from pathlib import Path
import argparse,json,struct
import numpy as np
from bake_nature_material_fidelity import glb_document

def position_arrays(doc,binary):
    for mesh in doc['meshes']:
        for primitive in mesh['primitives']:
            accessor=doc['accessors'][primitive['attributes']['POSITION']]
            view=doc['bufferViews'][accessor['bufferView']]
            assert accessor['componentType']==5126 and accessor['type']=='VEC3'
            values=np.ndarray((accessor['count'],3),'<f4',binary,view.get('byteOffset',0)+accessor.get('byteOffset',0),strides=(view.get('byteStride',12),4))
            yield accessor,values

def write_glb(path,doc,binary):
    doc['buffers']=[{'byteLength':len(binary)}]
    encoded=json.dumps(doc,separators=(',',':')).encode();encoded+=b' '*(-len(encoded)%4)
    raw=struct.pack('<III',0x46546c67,2,12+8+len(encoded)+8+len(binary))+struct.pack('<II',len(encoded),0x4e4f534a)+encoded+struct.pack('<II',len(binary),0x004e4942)+binary
    temporary=path.with_suffix('.aligning');temporary.write_bytes(raw);temporary.replace(path)

def align(record):
    if record.get('lod_alignment',{}).get('shared_datum'):return record
    files=[Path(p)for p in record['files']];loaded=[glb_document(p)for p in files]
    floors=[min(float(v[:,1].min())for a,v in position_arrays(doc,binary))for doc,binary in loaded]
    tops=[max(float(v[:,1].max())for a,v in position_arrays(doc,binary))for doc,binary in loaded]
    common=min(floors);old_origin=float(record['origin'][1]);new_origin=old_origin+common
    for path,(doc,binary)in zip(files,loaded):
        # Each accessor appears once in Writer's output; never translate twice.
        seen=set()
        for accessor,values in position_arrays(doc,binary):
            ident=accessor['bufferView']
            if ident not in seen:values[:,1]-=common;seen.add(ident)
            accessor['min']=values.min(0).tolist();accessor['max']=values.max(0).tolist()
        # Detach numpy views before growing the bytearray.
        del values
        def array(value,kind):
            values=np.asarray(value,dtype='<u4'if kind=='SCALAR'else'<f4');offset=len(binary);payload=values.tobytes();binary.extend(payload);binary.extend(b'\0'*(-len(binary)%4))
            view=len(doc['bufferViews']);doc['bufferViews'].append({'buffer':0,'byteOffset':offset,'byteLength':len(payload),'target':34963 if kind=='SCALAR' else 34962})
            a={'bufferView':view,'componentType':5125 if kind=='SCALAR' else 5126,'count':len(values),'type':kind}
            if kind=='VEC3':a.update(min=values.min(0).tolist(),max=values.max(0).tolist())
            idx=len(doc['accessors']);doc['accessors'].append(a);return idx
        primitive={'attributes':{'POSITION':array([[0,0,0]],'VEC3'),'NORMAL':array([[0,1,0]],'VEC3'),'TEXCOORD_0':array([[0,0]],'VEC2')},'indices':array([[0],[0],[0]],'SCALAR'),'material':0}
        mesh=len(doc['meshes']);doc['meshes'].append({'name':'Shared source elevation datum','primitives':[primitive],'extras':{'valwood_invisible_source_datum':True}})
        node=len(doc['nodes']);doc['nodes'].append({'name':'Shared source elevation datum','mesh':mesh});doc['scenes'][doc.get('scene',0)]['nodes'].append(node)
        doc.setdefault('asset',{}).setdefault('extras',{})['valwood_shared_lod_floor']=new_origin
        write_glb(path,doc,binary)
    record['origin'][1]=new_origin;record['unity_origin']=record['origin'].copy();record['pivot']=record['origin'].copy();record['engine_local_anchor']=[record['origin'][0]-500,new_origin,500-record['origin'][2]]
    record['dimensions'][1]=max(tops)-common
    record['lod_alignment']={'shared_datum':True,'source_origin_y_before':old_origin,'source_origin_y_after':new_origin,'raw_lod_minima':floors,'datum_triangles_per_lod':1,'visible_geometry_changed':False}
    return record

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('catalog',type=Path);args=parser.parse_args();records=json.loads(args.catalog.read_text())
    for r in records:align(r)
    tmp=args.catalog.with_suffix('.aligning.json');tmp.write_text(json.dumps(records,indent=2)+'\n');tmp.replace(args.catalog)
    print('Aligned',len(records),'chunks to shared source elevation; catalogpivotready')
if __name__=='__main__':main()
