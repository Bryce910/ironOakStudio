#!/usr/bin/env python3
"""Stage stitched terrain LODs without changing source fields or placements."""
from pathlib import Path
import argparse,copy,json,shutil
import numpy as np
from build_city_countryside import ROOT,Landscape,mesh,sha

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--catalog',type=Path,default=ROOT/'assets/purchased/bep_reforged/countryside/catalog.json')
    ap.add_argument('--base',type=Path,default=ROOT/'assets/purchased/bep_reforged/city-opening/catalog.json')
    ap.add_argument('--output',type=Path,default=ROOT/'assets/purchased/bep_reforged/countryside-terrain-cleanup')
    args=ap.parse_args();data=json.loads(args.catalog.read_text());land=Landscape(json.loads(args.base.read_text()))
    materials=json.loads((ROOT/'assets/purchased/bep_reforged/countryside-materials/ground-materials.json').read_text())
    out=args.output.resolve();out.mkdir(parents=True,exist_ok=True);audit=[]
    for asset in data['assets']:
        if not asset['name'].startswith('BEP_Countryside_Terrain_'):continue
        xi,zi=map(int,asset['name'].split('_')[-2:]);x0=-220+110*xi;z0=-180+115*zi;stats=[];files=[]
        for label,side in [('near',129),('mid',49)]:
            x=np.unique(np.r_[np.linspace(x0,x0+110,side),[v for v in [land.lo[0],land.hi[0]]if x0<v<x0+110]])
            z=np.unique(np.r_[np.linspace(z0,z0+115,side),[v for v in [land.lo[1],land.hi[1]]if z0<v<z0+115]])
            path=out/'models'/f'{asset["name"]}_{label}.glb'
            s=mesh(path,x,z,land,materials,asset['pivot_offset_native'][1]);stats.append(s);files.append(str(path))
        audit.append(dict(asset=asset['name'],old_triangles=[s['triangles']for s in asset['stats']],new_triangles=[s['triangles']for s in stats]))
        asset.update(files=files,stats=stats,bounds=stats[0]['bounds']);asset['native']['split']=260
    source=json.loads(args.catalog.read_text());assert data['placements']==source['placements']
    data['terrain_lod_cleanup']=dict(shared_boundary='Union of near/mid edge sites; original interior triangles retained',high_split_m=260,groundcover_draw_m=170,maximum_tile_half_diagonal_m=79.5717,height_and_coverage_fields_unchanged=True)
    (out/'catalog.json').write_text(json.dumps(data,indent=2)+'\n');shutil.copy2(args.catalog.parent/'landscape-review.npz',out/'landscape-review.npz')
    (out/'audit.json').write_text(json.dumps(dict(catalog_sha256=sha(out/'catalog.json'),tiles=audit),indent=2)+'\n')
    print(json.dumps(dict(catalog=str(out/'catalog.json'),changed_models=len(audit)*2,placements_unchanged=True),indent=2))

if __name__=='__main__':main()
