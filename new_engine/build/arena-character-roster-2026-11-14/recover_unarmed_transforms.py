#!/usr/bin/env python3
"""Exact authored unarmed32 motion, independently aligned to accepted legs.
Purchased source skin/rest remains read-only. The current legacy head's later
surface reshaping is not transferred onto another purchased character's head.
"""
import os
os.environ.setdefault('OPENBLAS_NUM_THREADS','2')
from pathlib import Path
import json,hashlib,sys,numpy as np
R=Path('/home/computer/Desktop/final_engine/new_engine');NEW=Path(__file__).resolve().parent
OLD=R/'build/modular-knights-combat-2026-11-13/enemy-guards-v6';sys.path.insert(0,str(OLD))
import skin_recovery as skin
from anatomy_source import read

def sha(p):
 with Path(p).open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def main():
 raw=R/'build/player-inventory-2026-11-08/project-v1/models/unarmed_hero';accepted=R/'projects/valwood_design/models/unarmed_hero';uv=np.asarray(skin.DATA['source_uv'])
 doc,base=read(accepted/'unarmed_idle_000.glb');mapping=[]
 for pi,p in enumerate(base):
  name=doc['materials'][p['material']].get('name','')
  if not name.startswith('VALWOOD_EQUIPMENT:'):continue
  mask=int(name.split(':')[1].split()[0])
  if mask not in [64,256]:continue
  lut={}
  for i,u in enumerate(uv):
   if int(skin.REGIONS[i])==mask:lut.setdefault(tuple(np.rint(u*1e5).astype(int)),[]).append(i)
  for vertex,u in enumerate(p['attrs']['TEXCOORD_0']):
   candidates=lut.get(tuple(np.rint(u*1e5).astype(int)),[])
   if len(candidates)==1 and np.max(abs(uv[candidates[0]]-u))<2e-6:mapping.append((pi,vertex,candidates[0]))
 assert len(mapping)>1500
 matrices=[];rows=[];inputs={str(Path(__file__).resolve()):sha(__file__),str(OLD/'skin_recovery.py'):sha(OLD/'skin_recovery.py'),str(OLD/'anatomy_source.py'):sha(OLD/'anatomy_source.py'),str(skin.WORK/'armor-source-v1/armor-source.json'):sha(skin.WORK/'armor-source-v1/armor-source.json')}
 for mode in ['idle','walk']:
  for key in range(16):
   src=raw/f'unarmed_{mode}_{key:03}.glb';dst=accepted/src.name;sd,sp=read(src);d,pr=read(dst);inputs[str(src.resolve())]=sha(src);inputs[str(dst.resolve())]=sha(dst)
   assert len(sp)==1 and len(sp[0]['attrs']['POSITION'])==skin.N and np.max(abs(sp[0]['attrs']['TEXCOORD_0']-uv))<1e-6
   source=sp[0]['attrs']['POSITION'].astype(float);T,error=skin.skin(source);assert error<2e-5
   a=np.asarray([source[i]for _,_,i in mapping]);b=np.asarray([pr[pi]['attrs']['POSITION'][v]for pi,v,_ in mapping],float)
   ac=a[:,[0,2]].mean(0);bc=b[:,[0,2]].mean(0);u,_,v=np.linalg.svd((a[:,[0,2]]-ac).T@(b[:,[0,2]]-bc));rotation=u@v
   if np.linalg.det(rotation)<0:u[:,-1]*=-1;rotation=u@v
   yaw=np.eye(3);yaw[np.ix_([0,2],[0,2])]=rotation;translation=(b-a@yaw).mean(0);residual=np.linalg.norm(a@yaw+translation-b,axis=1)
   assert float(residual.max())<2e-5,(mode,key,'Actual accepted leg motion differs from authored source',residual.max())
   transformed=T.reshape(-1,4,3)@yaw;transformed[:,3]+=translation;T=transformed.reshape(-1,3)
   matrices.append(T);rows.append(dict(mode=mode,key=key,source_lbs_residual_metres=float(error),matched_accepted_vertices=len(mapping),accepted_leg_residual_metres=float(residual.max()),accepted_rotation=yaw.tolist(),accepted_translation=translation.tolist()))
   print(mode,key,'source',error,'accepted',float(residual.max()),flush=True)
 out=NEW/'intake/npc-knights/unarmed-transforms.npz';out.parent.mkdir(parents=True,exist_ok=True);np.savez_compressed(out,transforms=np.asarray(matrices),bones=np.asarray(skin.BONES),mapping=np.asarray(mapping))
 proof=dict(schema='valwood.accepted-unarmed-source-lbs.v2',passed=True,inputs=inputs,outputs={str(out):sha(out)},bone_order=skin.BONES,rows=rows,policy='Exact original11422-vertex authored unarmed source, original50-bone rest/skin LBS recovery; independently matched actual accepted1637 lower-body source UV/region vertices for whole-pose yaw/origin; no transfer of later legacy face surface reshaping',maximum_source_lbs_residual_metres=max(r['source_lbs_residual_metres']for r in rows),maximum_accepted_leg_residual_metres=max(r['accepted_leg_residual_metres']for r in rows))
 (out.parent/'UNARMED_TRANSFORMS_REVIEW.json').write_text(json.dumps(proof,indent=2)+'\n')
if __name__=='__main__':main()
