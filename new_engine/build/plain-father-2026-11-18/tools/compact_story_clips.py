#!/usr/bin/env python3
"""Canonical static TRS normalization, then exact constant-key reduction.
Only the three new body clips are touched. The raw matrix export is retained.
Captured dynamic rotation/pelvis keys stay byte-identical; baked static local
origins/scales are canonicalized to remove roundoff from matrix rebasing.
"""
from pathlib import Path
import sys,json,copy,hashlib,math
import numpy as np
BASE=Path(__file__).resolve().parents[1];ENGINE=BASE.parents[1]
sys.path.insert(0,str(ENGINE/'tools'));import finalize_modular_knight_glb as g
ROOT=BASE/'assets/story_hero/story_clips'
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def append(d,b,values,old):return g.append_accessor(d,b,np.asarray(values,dtype='<f4'),old)
def matrix(t,q,s):
 x,y,z,w=q;norm=math.sqrt(x*x+y*y+z*z+w*w);x/=norm;y/=norm;z/=norm;w/=norm
 r=np.array([[1-2*(y*y+z*z),2*(x*y-z*w),2*(x*z+y*w)], [2*(x*y+z*w),1-2*(x*x+z*z),2*(y*z-x*w)],[2*(x*z-y*w),2*(y*z+x*w),1-2*(x*x+y*y)]])
 m=np.eye(4);m[:3,:3]=r@np.diag(s);m[:3,3]=t;return m
def global_pose(d,b,clip,t):
 trs=[{'translation':np.array(n.get('translation',[0,0,0]),float),'rotation':np.array(n.get('rotation',[0,0,0,1]),float),'scale':np.array(n.get('scale',[1,1,1]),float)} for n in d['nodes']]
 for ch in clip['channels']:
  sp=clip['samplers'][ch['sampler']];times=g.array(d,b,sp['input'])[:,0];v=g.array(d,b,sp['output']);k=min(len(times)-1,max(0,np.searchsorted(times,t,side='right')-1));nextk=min(k+1,len(times)-1);u=0 if nextk==k else np.clip((t-times[k])/(times[nextk]-times[k]),0,1);a=v[k].astype(float);z=v[nextk].astype(float)
  if ch['target']['path']=='rotation' and np.dot(a,z)<0:z=-z
  value=a*(1-u)+z*u
  if ch['target']['path']=='rotation':value/=np.linalg.norm(value)
  trs[ch['target']['node']][ch['target']['path']]=value
 parents={c:i for i,n in enumerate(d['nodes']) for c in n.get('children',[])};out={}
 def visit(i):
  if i not in out:
   n=d['nodes'][i];local=np.array(n['matrix']).reshape(4,4).T if 'matrix'in n else matrix(trs[i]['translation'],trs[i]['rotation'],trs[i]['scale']);out[i]=(visit(parents[i]) if i in parents else np.eye(4))@local
  return out[i]
 return np.array([visit(i) for i in range(len(d['nodes']))])

def main():
 raw=ROOT/'story_clips.glb';d,b,_=g.load(raw);out=copy.deepcopy(d);binary=bytearray(b);rows=[];normalization=[]
 for original,clip in zip(d['animations'],out['animations']):
  assert clip['name'] in ['Story_SwimIdle','Story_SwimForward','Story_Death']
  before=after=0;normalized=collapsed=0
  for och,ch in zip(original['channels'],clip['channels']):
   old=original['samplers'][och['sampler']];sp=clip['samplers'][ch['sampler']];values=g.array(d,b,old['output']);count=len(values);before+=count;node=d['nodes'][ch['target']['node']];path=ch['target']['path'];canonical=None
   if path=='scale':canonical=np.asarray(node.get('scale',[1,1,1]),dtype='<f4')
   elif path=='translation' and node.get('name')!='pelvis':canonical=np.asarray(node.get('translation',[0,0,0]),dtype='<f4')
   if canonical is not None:
    # These are static by bake_actions' explicit action construction. The raw
    # matrix exporter introduced local floating-point noise while rebasing.
    error=float(np.max(np.abs(values-canonical)));normalization.append({'clip':clip['name'],'node':node['name'],'path':path,'max_raw_component_deviation':error});values=np.broadcast_to(canonical,values.shape).copy();normalized+=1
   constant=np.array_equal(values,np.broadcast_to(values[0],values.shape))
   if constant:
    sp['input']=append(out,binary,[[0.]],{'componentType':5126,'type':'SCALAR'});sp['output']=append(out,binary,values[:1],d['accessors'][old['output']]);after+=1;collapsed+=1
   else:
    assert canonical is None
    # Dynamic outputs/accessors remain byte-for-byte references into raw BIN.
    assert sp==old;after+=count
  rows.append({'name':clip['name'],'keys_before':before,'keys_after':after,'normalized_static_channels':normalized,'exact_constant_channels_collapsed':collapsed})
 target=ROOT/'story_clips.compact.glb';target.write_bytes(g.encode(out,binary));cd,cb,_=g.load(target)
 # Full-body geometry, all skin weights, every 30Hz and60Hz sample (including
 # original key boundaries). This measures normalization, not just key counts.
 body=BASE/'assets/story_hero/civilian/body.glb';bd,bb,_=g.load(body);points=[];joints=[];weights=[];seen=set()
 for primitive in bd['meshes'][0]['primitives']:
  a=primitive['attributes'];key=tuple(a[k] for k in ['POSITION','JOINTS_0','WEIGHTS_0'])
  if key in seen:continue
  seen.add(key);p=g.array(bd,bb,a['POSITION']);points.append(np.c_[p,np.ones(len(p))]);joints.append(g.array(bd,bb,a['JOINTS_0']).astype(int));weights.append(g.array(bd,bb,a['WEIGHTS_0']))
 points=np.concatenate(points);joints=np.concatenate(joints);weights=np.concatenate(weights);ib=g.array(bd,bb,bd['skins'][0]['inverseBindMatrices']).reshape(-1,4,4).transpose(0,2,1);palette=bd['skins'][0]['joints']
 max_world=0.;max_vertex=0.;samples=0;deformations=0
 for original,compact in zip(d['animations'],cd['animations']):
  duration=max(float(g.array(d,b,s['input'])[-1,0]) for s in original['samplers']);times=sorted(set([min(duration,k/60) for k in range(int(math.ceil(duration*60))+1)]+[duration]))
  clip_vertex=0.;clip_world=0.
  for t in times:
   a=global_pose(d,b,original,t);c=global_pose(cd,cb,compact,t);clip_world=max(clip_world,float(np.max(np.linalg.norm(a[:,:3,3]-c[:,:3,3],axis=1))));ma=a[palette]@ib;mc=c[palette]@ib;delta=np.zeros((len(points),4))
   for k in range(4):delta+=np.einsum('nij,nj->ni',(ma-mc)[joints[:,k]],points)*weights[:,k,None]
   clip_vertex=max(clip_vertex,float(np.max(np.linalg.norm(delta[:,:3],axis=1))));samples+=1;deformations+=len(points)
  max_world=max(max_world,clip_world);max_vertex=max(max_vertex,clip_vertex)
  rows[[x['name'] for x in rows].index(original['name'])].update(samples_60hz_including_30hz=len(times),max_world_joint_origin_deviation_m=clip_world,max_vertex_deviation_m=clip_vertex,duration_seconds=duration)
 assert max_world<.00002 and max_vertex<.00003,(max_world,max_vertex)
 # All dynamic capture rotation and pelvis translations must be exactly kept.
 for original,compact in zip(d['animations'],cd['animations']):
  for a,c in zip(original['channels'],compact['channels']):
   name=d['nodes'][a['target']['node']]['name'];path=a['target']['path']
   if path=='rotation' or (path=='translation' and name=='pelvis'):
    aa=original['samplers'][a['sampler']];cc=compact['samplers'][c['sampler']];v=g.array(d,b,aa['output']);w=g.array(cd,cb,cc['output'])
    assert np.array_equal(v,w) or (len(w)==1 and np.array_equal(v,np.broadcast_to(w[0],v.shape)))
 source_receipt=ROOT/'STORY_SOURCE_CLIPS_REVIEW.json';sr=json.loads(source_receipt.read_text());inputs={str(Path(__file__).resolve()):sha(__file__),str(raw):sha(raw),str(body):sha(body),str(source_receipt):sha(source_receipt)};inputs.update(sr['inputs'])
 receipt={'schema':'valwood.story-clip-static-key-compaction.v1','passed':True,'clips':rows,'samples':samples,'sampled_vertex_deformations':deformations,'max_world_joint_origin_deviation_m':max_world,'max_vertex_deviation_m':max_vertex,'normalization':'Only mathematically static local translations except pelvis, and all local scales, canonicalized to exact rest TRS. This removes matrix-rebasing roundoff; measured full-body deviation is reported separately.','constant_key_reduction':'Every normalized constant track collapsed to one exactly equal key; dynamic rotation/pelvis outputs preserved bitwise.','key_budget_unchanged':262144,'raw_matrix_export_retained':str(raw),'normalization_channels':normalization,'inputs':inputs,'outputs':{str(target):sha(target)}}
 (ROOT/'STORY_COMPACT_CLIPS_REVIEW.json').write_text(json.dumps(receipt,indent=2)+'\n');print(json.dumps({k:v for k,v in receipt.items() if k in ['passed','clips','samples','sampled_vertex_deformations','max_world_joint_origin_deviation_m','max_vertex_deviation_m']},indent=2))
if __name__=='__main__':main()
