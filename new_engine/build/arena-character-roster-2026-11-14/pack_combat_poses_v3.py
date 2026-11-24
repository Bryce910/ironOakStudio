#!/usr/bin/env python3
"""Lossless exact posed PNT packet; original GLBs and materials stay immutable."""
import os
os.environ.setdefault('OPENBLAS_NUM_THREADS','2')
from pathlib import Path
import argparse,hashlib,json,struct,sys,numpy as np
sys.path.insert(0,'/home/computer/Desktop/final_engine/new_engine/build/modular-knights-combat-2026-11-13/enemy-guards-v6')
from anatomy_source import read

def sha(p):
 with Path(p).open('rb')as f:return hashlib.file_digest(f,'sha256').hexdigest()
def keys(path):
 raw=Path(path).read_bytes();assert raw[:8]==b'VWANA002';np_,nm,parts=struct.unpack_from('<III',raw,8);assert np_==172 and parts==8;at=20;out=[]
 for m in range(nm):
  n,key=struct.unpack_from('<IQ',raw,at);out.append((n,key));at+=12+n*8*4
 return out

def build(body,anatomy,out):
 body=Path(body).resolve();anatomy=Path(anatomy).resolve();out=Path(out);pose_count=210 if (body/'combat_body_209.glb').is_file() else 204 if (body/'combat_body_172.glb').is_file() else 172;out.parent.mkdir(parents=True,exist_ok=True)
 layout=keys(anatomy/'anatomy.bin');doc,base=read(body/'combat_body_000.glb');assert len(base)==len(layout)<=32
 meshes=[];total=0;inputs={str(Path(__file__).resolve()):sha(__file__),str(anatomy/'anatomy.bin'):sha(anatomy/'anatomy.bin')}
 for m,p in enumerate(base):
  n,key=layout[m];assert n==len(p['attrs']['POSITION']);name=doc['materials'][p['material']].get('name','')
  rigid=name in ['Owned PowerOrc14 axe','Owned Unity Orc Gnur weapon','Owned Unity Orc roster weapon','Owned Unity Knight roster weapon'] or (m==0 and (name.startswith('Owned knight') or 'sword' in name.lower() or 'steel' in name.lower()))
  ids=p['indices'].reshape(-1);v=p['attrs']['POSITION'];x=int(ids[0]);y=int(ids[np.argmax(np.sum((v[ids]-v[x])**2,axis=1))]);z=int(ids[np.argmax(np.sum(np.cross(v[y]-v[x],v[ids]-v[x])**2,axis=1))]);assert not rigid or len({x,y,z})==3
  meshes.append(dict(count=n,key=key,rigid=rigid,triangle=[x,y,z],material=name));total+=n
 assert any(m['rigid'] for m in meshes) and total<=100000,(total,meshes)
 tmp=out.with_suffix(out.suffix+'.tmp')
 with tmp.open('wb')as f:
  f.write(struct.pack('<8sIIII',b'VWPNT002',2,pose_count,len(meshes),total))
  for i,m in enumerate(meshes):f.write(struct.pack('<IIQIIII',m['count'],int(m['rigid']),m['key'],*m['triangle'],len(base[i]['indices'].reshape(-1))if m['rigid'] else 0))
  for p,m in zip(base,meshes):
   if m['rigid']:f.write(p['indices'].astype('<u4').tobytes())
  for pose in range(pose_count):
   path=body/f'combat_body_{pose:03}.glb';d,pr=read(path);inputs[str(path)]=sha(path);assert len(pr)==len(base)
   for m,(p,b) in enumerate(zip(pr,base)):
    assert np.array_equal(p['indices'],b['indices']) and np.array_equal(p['attrs']['TEXCOORD_0'],b['attrs']['TEXCOORD_0']) and np.array_equal(p['attrs']['TANGENT'][:,3],b['attrs']['TANGENT'][:,3]),(pose,m,'layout')
    a=p['attrs'];v=np.c_[a['POSITION'],a['NORMAL'],a['TANGENT'][:,:3]].astype('<f4');assert len(v)==meshes[m]['count'] and np.isfinite(v).all();f.write(v.tobytes())
 tmp.replace(out)
 receipt=dict(schema='valwood.exact-combat-pose-packet.v1',passed=True,inputs=inputs,outputs={str(out.resolve()):sha(out)},poses=pose_count,vertices=total,meshes=meshes,mapped_bytes=out.stat().st_size,geometry_policy='Exact float32 PNT; UV/index/tangent.w invariant and sourced from original body000; no weight truncation, geometry reduction or material substitution',base_body=str(body/'combat_body_000.glb'))
 out.with_suffix('.json').write_text(json.dumps(receipt,indent=2)+'\n');print(json.dumps(dict(packet=str(out),vertices=total,bytes=out.stat().st_size,rigid=[i for i,m in enumerate(meshes)if m['rigid']])),flush=True)
if __name__=='__main__':
 ap=argparse.ArgumentParser();ap.add_argument('--body',required=True);ap.add_argument('--anatomy',required=True);ap.add_argument('--output',required=True);a=ap.parse_args();build(a.body,a.anatomy,a.output)
