#!/usr/bin/env python3
"""Retain generated prop geometry and reuse owned CC0 deadwood PBR scans."""
from pathlib import Path
import copy,json,struct,hashlib
N=Path(__file__).resolve().parents[1];P=N/'world-project/models'
def read(p):
 b=p.read_bytes();n=struct.unpack_from('<I',b,12)[0];d=json.loads(b[20:20+n]);off=20+n;z=struct.unpack_from('<I',b,off)[0];return d,bytearray(b[off+8:off+8+z])
def write(p,d,b):
 d['buffers']=[{'byteLength':len(b)}];j=json.dumps(d,separators=(',',':')).encode();j+=b' '*((-len(j))%4);b+=b'\0'*((-len(b))%4);p.write_bytes(struct.pack('<III',0x46546c67,2,28+len(j)+len(b))+struct.pack('<II',len(j),0x4e4f534a)+j+struct.pack('<II',len(b),0x004e4942)+b)
s,raw=read(P/'cinematic_deadwood.glb');out=[]
for name in ['barricade','discarded_arms']:
 path=P/'battlefield'/f'{name}.glb';d,b=read(path)
 if d.get('images'):continue
 d['images']=[];d['textures']=copy.deepcopy(s['textures']);d['samplers']=copy.deepcopy(s.get('samplers',[]))
 for im in s['images']:
  v=s['bufferViews'][im['bufferView']];whilepad=(-len(b))%4;b+=b'\0'*whilepad;at=len(b);b.extend(raw[v.get('byteOffset',0):v.get('byteOffset',0)+v['byteLength']]);vi=len(d['bufferViews']);d['bufferViews'].append({'buffer':0,'byteOffset':at,'byteLength':v['byteLength']});new=copy.deepcopy(im);new['bufferView']=vi;d['images'].append(new)
 for mat,tint in [(0,[.70,.63,.53,1]),(4,[.95,.83,.65,1])]+([(2,[.34,.095,.065,1]),(3,[.10,.18,.28,1])]if name=='discarded_arms'else[]):
  oldname=d['materials'][mat]['name'];m=copy.deepcopy(s['materials'][0]);m['name']=oldname+' | retained owned CC0 deadwood PBR';m['pbrMetallicRoughness']['baseColorFactor']=tint;m['pbrMetallicRoughness']['roughnessFactor']=.95;m['doubleSided']=True;d['materials'][mat]=m
 write(path,d,b);out.append({'path':str(path),'bytes':path.stat().st_size,'sha256':hashlib.sha256(path.read_bytes()).hexdigest()})
(N/'assets/battlefield/PROP_MATERIALS.json').write_text(json.dumps({'source':str(P/'cinematic_deadwood.glb'),'geometry_unchanged':True,'props':out},indent=2)+'\n');print(json.dumps(out))
