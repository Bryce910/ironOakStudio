#!/usr/bin/env python3
"""Bury exposed authored tree root collars after a terrain revision."""
from pathlib import Path
import argparse,copy,json,shutil
import numpy as np
from build_city_countryside import ROOT,readfield,sample
from countryside_contact import TerrainSurfaces,root_points,posed_points


def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--catalog',type=Path,required=True);ap.add_argument('--output',type=Path,required=True);args=ap.parse_args()
    out=args.output.resolve()
    if out.exists():raise ValueError('Use a new immutable output directory')
    out.mkdir(parents=True);data=json.loads(args.catalog.read_text());origin=np.asarray(json.loads((ROOT/'projects/valwood_city_showcase/city_showcase.json').read_text())['origin'])
    primary,po,ps=readfield(data['ground_support_revision']['repaired_field']);country,co,cs=readfield(data['countryside']['ground_files'][0])
    def fallback(p):
        p=np.asarray(p);inside=np.all(p>=po,axis=-1)&np.all(p<=po+ps*(np.array(primary.shape)[::-1]-1),axis=-1)
        return np.where(inside,sample(primary,p,po,ps),sample(country,p,co,cs))
    terrain=TerrainSurfaces(data,origin,fallback);assets={a['name']:a for a in data['assets']};roots={};changes=[]
    for i,p in enumerate(data['placements']):
        a=assets[p['asset']];alias=a.get('native',{}).get('alias','')
        if alias not in ['bep_nature_oak','bep_nature_beech']:continue
        if alias not in roots:roots[alias]=root_points(a,{})
        points=posed_points(p,roots[alias],origin);gap=float((points[:,1]-terrain.sample(points[:,[0,2]],0)).max())
        if gap<=.055:continue
        drop=gap+.025
        for name in ['matrix_native','contact_original_matrix_native']:
            if name not in p:continue
            m=np.asarray(p[name]).reshape(4,4).T.copy();m[1,3]-=drop;p[name]=m.T.flatten().tolist()
        changes.append(dict(index=i,alias=alias,native_xz=np.asarray(p['matrix_native']).reshape(4,4).T[[0,2],3].tolist(),vertical_delta=-drop,maximum_original_exposure_m=gap))
    data['town_ground_envelope']['tree_collar_settlement']=changes
    (out/'catalog.json').write_text(json.dumps(data,indent=2,allow_nan=False)+'\n');shutil.copy2(args.catalog.parent/'landscape-review.npz',out/'landscape-review.npz')
    (out/'tree-collar-audit.json').write_text(json.dumps(dict(changes=changes,kind='Down-only soil collar burial, authored underground roots retained, XZ/rotation/scales exact'),indent=2)+'\n')
    print(json.dumps(dict(catalog=str(out/'catalog.json'),trees_settled=len(changes)),indent=2))


if __name__=='__main__':main()
