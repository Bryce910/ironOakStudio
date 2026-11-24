#!/usr/bin/env python3
"""Decode source scene's painted Mesh YAML offline, without loading it in Unity.

Produces the exact NativeShowcaseExport64-byte vertex/uint32 triangle layout.
Mesh overrides remain indexed by original scene prefab/fileIDs for rebinding.
"""
from pathlib import Path
import json,re,hashlib,collections
import numpy as np
import yaml
ROOT=Path(__file__).resolve().parents[1];PRIVATE=ROOT.parent/'.local-tools/unity-source';SOURCE=PRIVATE/'showcase-source/Demo Castle.unity';OUT=PRIVATE/'showcase-painted'
# Unity VertexAttributeFormat; normalized attributes convert to float32.
FORMATS={0:('<f4',1),1:('<f2',1),2:('u1',255),3:('i1',127),4:('<u2',65535),5:('<i2',32767),6:('u1',1),7:('i1',1),8:('<u2',1),9:('<i2',1),10:('<u4',1),11:('<i4',1)}
def docs():
 block=[];kind=None;ident=0
 with SOURCE.open()as f:
  for line in f:
   if line.startswith('--- !u!'):
    if kind==43:yield ident,''.join(block)
    m=re.match(r'--- !u!(\d+) &(\d+)',line);kind=int(m[1]);ident=int(m[2]);block=[]
   elif kind==43:block.append(line)
  if kind==43:yield ident,''.join(block)
def decode(m):
 v=m['m_VertexData'];count=v['m_VertexCount'];channels=v['m_Channels'];raw=bytes.fromhex(v['_typelessdata']);assert len(raw)==v['m_DataSize']
 strides=collections.defaultdict(int)
 for c in channels:
  dim=c['dimension']&15
  if dim:strides[c['stream']]=max(strides[c['stream']],c['offset']+dim*np.dtype(FORMATS[c['format']][0]).itemsize)
 starts={};offset=0
 for s,stride in sorted(strides.items()):starts[s]=offset;offset=(offset+count*stride+15)&~15
 def channel(i,width,default):
  out=np.full((count,width),default,dtype='<f4')
  if i>=len(channels)or not(channels[i]['dimension']&15):return out,False
  c=channels[i];dim=c['dimension']&15;dtype,scale=FORMATS[c['format']];data=np.ndarray((count,dim),dtype=dtype,buffer=raw,offset=starts[c['stream']]+c['offset'],strides=(strides[c['stream']],np.dtype(dtype).itemsize)).astype('<f4')
  if scale!=1:data/=scale
  if c['format']in{3,5}:np.maximum(data,-1,out=data)
  out[:,:min(width,dim)]=data[:,:min(width,dim)];return out,True
 p,_=channel(0,3,0);n,npresent=channel(1,3,0);uv0,u0=channel(4,2,0);uv1,u1=channel(5,2,0);uv2,u2=channel(6,2,0);color,cp=channel(3,4,1)
 if not u1:uv1=uv0.copy()
 if not u2:uv2=uv0.copy()
 if not npresent:n[:,1]=1
 vertices=np.concatenate([p,n,uv0,uv1,uv2,color],axis=1).astype('<f4');binary=bytearray(vertices.tobytes());indexraw=bytes.fromhex(m.get('m_IndexBuffer')or'');indexdtype='<u2'if m.get('m_IndexFormat',0)==0 else'<u4';indexsize=np.dtype(indexdtype).itemsize;submeshes=[]
 for s in m['m_SubMeshes']:
  assert s['topology']==0
  a=np.frombuffer(indexraw,dtype=indexdtype,count=s['indexCount'],offset=s['firstByte']).astype('<u4')+s.get('baseVertex',0)
  if len(a):assert int(a.max())<count
  submeshes.append({'indices':len(a),'index_offset':len(binary)});binary.extend(a.astype('<u4').tobytes())
 return binary,{'vertices':count,'stride':64,'uv0_present':u0,'uv1_present':u1,'uv2_present':u2,'colors_present':cp,'submeshes':submeshes}
def main():
 OUT.mkdir(exist_ok=True);(OUT/'meshes').mkdir(exist_ok=True);layout=json.loads((ROOT/'docs/stronghold/2026-11-01/showcase/publisher-scene-layout.json').read_text());overrides=[]
 for p in layout['prefabs']:
  for m in p['m_Modification']['m_Modifications']:
   if m['propertyPath']=='m_Mesh':overrides.append({'prefab_instance_id':p['id'],'prefab_source':p['source_path'],'target':m['target'],'mesh_scene_file_id':m['objectReference']['fileID'],'name':p['name']})
 manifest={'version':1,'source':str(SOURCE),'meshes':[],'overrides':overrides,'warnings':[]};byhash=set();total=0
 for ident,text in docs():
  text=re.sub(r'(?m)^(\s*(?:m_IndexBuffer|_typelessdata):) ([0-9a-fA-F]+)$',r"\1 '\2'",text)
  m=yaml.load(text,Loader=yaml.CSafeLoader)['Mesh'];binary,info=decode(m);digest=hashlib.sha256(binary).hexdigest();path='meshes/'+digest+'.bin'
  if digest not in byhash:(OUT/path).write_bytes(binary);byhash.add(digest);total+=len(binary)
  manifest['meshes'].append({'scene_file_id':ident,'name':m.get('m_Name',''),'hash':digest,'binary':path,**info})
  if len(manifest['meshes'])%500==0:print('Decoded',len(manifest['meshes']),'source painted meshes,',total//(1024**2),'MiB',flush=True)
 (OUT/'painted.json').write_text(json.dumps(manifest,indent=2)+'\n');print(json.dumps({'meshes':len(manifest['meshes']),'unique_meshes':len(byhash),'overrides':len(overrides),'bytes':total,'output':str(OUT/'painted.json')},indent=2),flush=True)
if __name__=='__main__':main()
