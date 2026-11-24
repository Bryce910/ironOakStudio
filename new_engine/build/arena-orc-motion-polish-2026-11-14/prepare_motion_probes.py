#!/usr/bin/env python3
"""Pick geometry probes using genuine source skin weights, not guessed screen pixels."""
from pathlib import Path
import hashlib,json
import numpy as np
HERE=Path(__file__).resolve().parent
BASE=HERE.parent/'arena-orcs-textured-2026-11-14'
def sha(p):
    with Path(p).open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def main():
    source=BASE/'arena_orcs/body-semantic.npz';data=np.load(source,allow_pickle=False)
    names=list(map(str,data['bone_names']));parts=len(data['part_names'])
    rest=np.concatenate([data[f'rest_{i}'] for i in range(parts)])
    weights=np.concatenate([data[f'weights_raw_{i}'] for i in range(parts)])
    probes={}
    groups={'sole_l':['foot_l','ball_l'],'sole_r':['foot_r','ball_r'],
            'hand_l':['hand_l'],'hand_r':['hand_r'],'head':['head'],'pelvis':['pelvis']}
    for name,bones in groups.items():
        columns=[i for i,b in enumerate(names) if any(b==q or b.lower()==q.lower() for q in bones)]
        if not columns:raise RuntimeError((name,names))
        strength=weights[:,columns].sum(axis=1);indices=np.flatnonzero(strength>.65)
        if len(indices)<8:raise RuntimeError('Too few genuine weighted vertices for '+name)
        if name.startswith('sole'):indices=indices[rest[indices,1]<=np.quantile(rest[indices,1],.25)]
        probes[name]=list(map(int,indices))
    out=HERE/'opponent-render-review';out.mkdir(exist_ok=True)
    text=f'{len(rest)} {len(probes)}\n'+''.join(f'{name} {len(v)} '+' '.join(map(str,v))+'\n' for name,v in probes.items())
    (out/'motion-probes.txt').write_text(text)
    receipt=dict(schema='valwood.orc.motion-geometry-probes.v1',source=str(source),source_sha256=sha(source),
                 vertex_count=len(rest),probes=probes,method='actual source bone weights >0.65; sole bottom source-Y quartile; world-space geometry centroids, not exact joint centers',
                 probe_file_sha256=sha(out/'motion-probes.txt'))
    (out/'MOTION_PROBES.json').write_text(json.dumps(receipt,indent=2)+'\n')
    print({k:len(v) for k,v in probes.items()})
if __name__=='__main__':main()
