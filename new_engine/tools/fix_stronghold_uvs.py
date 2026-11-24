#!/usr/bin/env python3
"""Repair the Blender UV-origin conversion in already generated local GLBs.

Only this stronghold's cooked architecture and active dwarf poses are touched.
The marker makes the conversion idempotent; original downloads stay intact.
"""
from pathlib import Path
import hashlib,json,struct
import numpy as np
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'projects/valwood_stronghold/models/stronghold'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
    records=[]
    paths=sorted(OUT.glob('source_*.glb'))
    for family in ['king','agvid','bedvar','orme']:paths+=sorted(OUT.glob(f'dwarf_{family}_*.glb'))
    for path in paths:
        raw=path.read_bytes();length,kind=struct.unpack_from('<II',raw,12);assert kind==0x4e4f534a
        d=json.loads(raw[20:20+length]);extras=d['asset'].setdefault('extras',{})
        if extras.get('stronghold_uv_origin')=='top-left':continue
        start=28+length;binary=bytearray(raw[start:]);seen=set();before=sha(path)
        for mesh in d['meshes']:
            for primitive in mesh['primitives']:
                index=primitive['attributes'].get('TEXCOORD_0')
                if index is None or index in seen:continue
                seen.add(index);a=d['accessors'][index];v=d['bufferViews'][a['bufferView']]
                assert a['componentType']==5126 and a['type']=='VEC2'
                array=np.ndarray((a['count'],2),dtype='<f4',buffer=binary,offset=v.get('byteOffset',0)+a.get('byteOffset',0),strides=(v.get('byteStride',8),4))
                array[:,1]=1.-array[:,1]
        extras['stronghold_uv_origin']='top-left';j=json.dumps(d,separators=(',',':')).encode();j+=b' '*(-len(j)%4)
        data=struct.pack('<III',0x46546c67,2,28+len(j)+len(binary))+struct.pack('<II',len(j),0x4e4f534a)+j+struct.pack('<II',len(binary),0x004e4942)+binary
        temp=path.with_suffix('.uvfix');temp.write_bytes(data);temp.replace(path)
        records.append(dict(file=str(path.relative_to(ROOT)),before=before,after=sha(path),uv_accessors=len(seen)))
    dest=ROOT/'docs/stronghold/2026-10-31/uv-origin-repair.json'
    if records:dest.write_text(json.dumps(records,indent=2)+'\n')
    print(f'Corrected {len(records)} generated GLBs; originals unchanged')
if __name__=='__main__':main()
