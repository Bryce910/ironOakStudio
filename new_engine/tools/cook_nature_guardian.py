#!/usr/bin/env python3
"""Cook Unity-exported guardian geometry into the engine's embedded static GLBs."""
from pathlib import Path
import copy
import json
import sys
import numpy as np

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from build_cinematic_diversity import Writer

DATA=ROOT/'assets/purchased/nature_castle'
SOURCE=ROOT.parent/'.local-tools/unity-source/guardian-native-meshes.json'
OUT=DATA/'cooked'


def main():
    source=json.loads(SOURCE.read_text())
    levels={}
    for part in source['parts']:
        p=np.asarray(part['positions'],dtype=np.float32).reshape(-1,3)
        n=np.asarray(part['normals'],dtype=np.float32).reshape(-1,3)
        uv=np.asarray(part['uv'],dtype=np.float32).reshape(-1,2)
        indices=np.asarray(part['indices'],dtype=np.uint32).reshape(-1,3)
        if not np.isfinite(p).all() or not np.isfinite(n).all() or not np.isfinite(uv).all():
            raise ValueError('Non-finite source vertex attribute')
        # Unity's authored left-handed Y-up prefab becomes right-handed Y-up.
        p[:,2]*=-1; n[:,2]*=-1; uv[:,1]=1-uv[:,1]
        normal_length=np.linalg.norm(n,axis=1,keepdims=True)
        if np.any(normal_length<.5):raise ValueError('Missing source normals')
        n/=normal_length
        cross=np.cross(p[indices[:,1]]-p[indices[:,0]],p[indices[:,2]]-p[indices[:,0]])
        area=np.linalg.norm(cross,axis=1)
        score=(cross*n[indices].sum(axis=1)).sum(axis=1)
        if np.median(score[area>1e-9])<0:indices=indices[:,[0,2,1]]
        levels.setdefault(part['lod'],[]).append((part['name'],p,n,uv,indices))
    assert 0 in levels and 1 in levels
    positions=np.concatenate([x[1] for x in levels[0]])
    lo=positions.min(axis=0);hi=positions.max(axis=0)
    origin=np.array([(lo[0]+hi[0])/2,lo[1],(lo[2]+hi[2])/2],dtype=np.float32)
    material={'name':'Purchased carved guardian, dry stone','pbrMetallicRoughness':{
        'baseColorFactor':[1,1,1,1],'metallicFactor':1,'roughnessFactor':1,
        'baseColorTexture':{'index':0},'metallicRoughnessTexture':{'index':2}},
        'normalTexture':{'index':1},'occlusionTexture':{'index':2}}
    images=[{'uri':str(DATA/'textures/guardian'/(role+'.png')),'mimeType':'image/png'} for role in ['base','normal','arm']]
    textures=[{'source':i,'sampler':0} for i in range(3)]
    OUT.mkdir(exist_ok=True);records=[]
    for label,level in [('near',0),('mid',1),('far',max(levels))]:
        writer=Writer(*copy.deepcopy(([material],textures,images)))
        triangles=0
        for name,p,n,uv,indices in levels[level]:
            writer.mesh(name,p-origin,n,uv,indices,0);triangles+=len(indices)
        writer.g['asset']['extras']={'source':'NatureManufacture purchased guardian prefab',
            'uv_origin':'top-left','source_lod':level,'prefab_transforms_preserved':True}
        path=OUT/('guardian_'+label+'.glb');writer.save(path,embedded=True)
        records.append({'lod':label,'source_lod':level,'triangles':triangles,'components':len(levels[level]),
                        'file':str(path.relative_to(ROOT)),'bytes':path.stat().st_size})
    report={'source_prefab':'Prefab_stone_guard_01_full.prefab','height_m':float(hi[1]-lo[1]),
            'width_m':float(hi[0]-lo[0]),'depth_m':float(hi[2]-lo[2]),
            'source_lods_preserved':True,'prefab_transforms_preserved':True,
            'variant':'Static dry stone; secondary base grain uses shape UV on the shield, which has no UV2',
            'material':'assets/purchased/nature_castle/textures/guardian/material.json','records':records}
    (OUT/'guardian.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2))


if __name__=='__main__':main()
