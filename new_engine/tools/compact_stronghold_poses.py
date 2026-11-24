#!/usr/bin/env python3
"""Losslessly index triangle-corner duplicates across each complete pose bank.

An index is shared only when position, normal and UV agree in EVERY pose. This
retains every rendered triangle while meeting the native100k-vertex pose cap.
"""
from pathlib import Path
import copy,json,tempfile,sys
import numpy as np
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'tools'))
from build_visual_upgrade_assets import read_glb
from compose_visual_upgrade import accessor
from build_cinematic_diversity import Writer
OUT=ROOT/'projects/valwood_stronghold/models/stronghold'
def main():
    report=[]
    for family in ['king','agvid','bedvar','orme']:
        paths=sorted(OUT.glob(f'dwarf_{family}_*.glb'));assert len(paths)==12,(family,len(paths))
        data=[read_glb(p)[:2]for p in paths];first,binary=data[0];plans=[];before=after=0
        def primitives_of(d):
            return[p for m in d['meshes']for p in m['primitives']if not(family in ['king','broddi']and any(term in d['materials'][p.get('material',0)].get('name','').lower()for term in ['axe','weapon']))]
        primitives=primitives_of(first)
        for pi,p in enumerate(primitives):
            arrays=[]
            for d,b in data:
                q=primitives_of(d)[pi];a=q['attributes']
                arrays.append(np.concatenate([accessor(d,b,a['POSITION']),accessor(d,b,a['NORMAL']),accessor(d,b,a['TEXCOORD_0'])],axis=1))
            reference,indices,inverse=np.unique(arrays[0],axis=0,return_index=True,return_inverse=True)
            bad=np.zeros(len(indices),dtype=bool)
            for a in arrays:
                invalid=np.max(np.abs(a-a[indices][inverse]),axis=1)>1e-7
                bad[inverse[invalid]]=True
            # Coincident corners that diverge in a later pose retain their own
            # original indices. No bind-pose-only welding can tear a joint.
            key=np.stack([inverse,np.where(bad[inverse],np.arange(len(inverse)),-1)],axis=1)
            _,indices,inverse=np.unique(key,axis=0,return_index=True,return_inverse=True)
            for a in arrays:assert np.max(np.abs(a-a[indices][inverse]))<1e-7
            plans.append((indices,inverse));before+=len(arrays[0]);after+=len(indices)
        assert after<=100000,(family,after)
        with tempfile.TemporaryDirectory(prefix='grandstone-image-')as folder:
            images=[]
            for i,image in enumerate(first.get('images',[])):
                v=first['bufferViews'][image['bufferView']];path=Path(folder)/f'{i}.image';path.write_bytes(binary[v.get('byteOffset',0):v.get('byteOffset',0)+v['byteLength']]);images.append(dict(uri=str(path),mimeType=image['mimeType']))
            for path,(d,b)in zip(paths,data):
                w=Writer(copy.deepcopy(d['materials']),copy.deepcopy(d.get('textures',[])),copy.deepcopy(images))
                if 'extras'in d['asset']:w.g['asset']['extras']=copy.deepcopy(d['asset']['extras'])
                for i,(p,(indices,inverse))in enumerate(zip(primitives_of(d),plans)):
                    a=p['attributes'];pos=accessor(d,b,a['POSITION']);normal=accessor(d,b,a['NORMAL']);uv=accessor(d,b,a['TEXCOORD_0']);tri=accessor(d,b,p['indices']).reshape(-1,3)
                    w.mesh(f'dwarf_{family}_{i}',pos[indices],normal[indices],uv[indices],inverse[tri],p.get('material',0))
                temp=path.with_suffix('.compact');w.save(temp,embedded=True);temp.replace(path)
        row=dict(family=family,poses=12,before_vertices=before,after_vertices=after,all_pose_triangle_attributes_preserved=True);report.append(row);print(json.dumps(row),flush=True)
    (ROOT/'docs/stronghold/2026-10-31/pose-compaction.json').write_text(json.dumps(report,indent=2)+'\n')
if __name__=='__main__':main()
