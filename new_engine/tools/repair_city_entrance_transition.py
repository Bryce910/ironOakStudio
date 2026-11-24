#!/usr/bin/env python3
"""Stage a continuous town entrance, fields and nearby soil contacts together."""
from pathlib import Path
import argparse, copy, json, shutil
import numpy as np
from build_city_countryside import ROOT, Landscape, mesh, field, sha, CITY_ROAD_TRANSITION_METRES
from countryside_contact import TerrainSurfaces, correct


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--catalog',type=Path,required=True)
    ap.add_argument('--output',type=Path,required=True)
    args=ap.parse_args();out=args.output.resolve()
    if out.exists():raise ValueError('Use a new immutable output directory')
    out.mkdir(parents=True)
    source=json.loads(args.catalog.read_text());data=copy.deepcopy(source)
    land=Landscape(source)
    materials=json.loads((ROOT/'assets/purchased/bep_reforged/countryside-materials/ground-materials.json').read_text())
    changed=[]
    for a in data['assets']:
        if not a['name'].startswith('BEP_Countryside_Terrain_'):continue
        xi,zi=map(int,a['name'].split('_')[-2:]);lo=np.array([-220+110*xi,-180+115*zi]);hi=lo+[110,115]
        # Outside this envelope the final feather is identically one. Keep
        # those immutable model files and their previous validation intact.
        if np.any(hi<land.lo-CITY_ROAD_TRANSITION_METRES-.15)or np.any(lo>land.hi+CITY_ROAD_TRANSITION_METRES+.15):continue
        stats=[];files=[]
        for label,side in [('near',129),('mid',49)]:
            x=np.unique(np.r_[np.linspace(lo[0],hi[0],side),[v for v in [land.lo[0],land.hi[0]]if lo[0]<v<hi[0]]])
            z=np.unique(np.r_[np.linspace(lo[1],hi[1],side),[v for v in [land.lo[1],land.hi[1]]if lo[1]<v<hi[1]]])
            path=out/'models'/f'{a["name"]}_{label}.glb'
            stats.append(mesh(path,x,z,land,materials,a['pivot_offset_native'][1]));files.append(str(path))
        changed.append(dict(asset=a['name'],previous_hashes=[s['sha256']for s in a['stats']],new_hashes=[s['sha256']for s in stats]))
        a.update(files=files,stats=stats,bounds=stats[0]['bounds'])
    review=np.load(args.catalog.parent/'landscape-review.npz')
    x,z=review['x_native'],review['z_native'];xx,zz=np.meshgrid(x,z);height=land.height(np.stack((xx,zz),-1))
    bounds=np.asarray(data['countryside']['native_bounds']);step=(bounds[2:]-bounds[:2])/(np.array(height.shape)[::-1]-1)
    ground=out/'ground/countryside.height';field(ground,height,bounds[:2]+land.origin[[0,2]],step)
    coverage=out/'ground/countryside.coverage';coverage.parent.mkdir(exist_ok=True);shutil.copy2(data['countryside']['ground_files'][1],coverage)
    data['countryside']['ground_files']=[str(ground),str(coverage)]
    np.savez_compressed(out/'landscape-review.npz',height_world=height,x_native=x,z_native=z,road_native=land.road,road_height_world=land.height(land.road))
    # Refit just the plants whose entire footprint might touch the changed
    # twelve-metre band. Existing identities, roots and all distant poses stay.
    indices=[]
    for i,p in enumerate(data['placements']):
        a=next(a for a in data['assets']if a['name']==p['asset'])
        if not a.get('native',{}).get('alias','').startswith('bep_nature_'):continue
        m=np.asarray(p['matrix_native']).reshape(4,4).T;center=m[[0,2],3]
        radius=float(np.linalg.norm((np.asarray(a['bounds'])[1]-np.asarray(a['bounds'])[0])[[0,2]])*np.linalg.norm(m[:3,:3],axis=0).max())
        distance=np.linalg.norm(center-np.minimum(np.maximum(center,land.lo),land.hi))
        if distance<CITY_ROAD_TRANSITION_METRES+radius:indices.append(i)
    subset=copy.deepcopy(data);subset['placements']=[copy.deepcopy(data['placements'][i])for i in indices]
    terrain=TerrainSurfaces(data,land.origin,lambda p:land.height(np.asarray(p)-land.origin[[0,2]]))
    grass=json.loads((ROOT/'assets/purchased/bep_reforged/countryside-grass-clumps/source-audit.json').read_text())
    fitted,contact=correct(subset,terrain,grass)
    for i,p in zip(indices,fitted['placements']):data['placements'][i]=p
    contact['candidate_indices']=indices
    (out/'contact-report.json').write_text(json.dumps(contact,indent=2)+'\n')
    data['town_ground_transition']=dict(feather_metres=CITY_ROAD_TRANSITION_METRES,applied_after_road_grading=True,
        source_catalog=str(args.catalog.resolve()),source_catalog_sha256=sha(args.catalog),source_primary_field_unchanged=True,
        coverage_unchanged=True,changed_models=len(changed)*2,nearby_contact_candidates=len(indices),contact_changes=contact['changed_placements'])
    data['terrain_lod_cleanup']['height_and_coverage_fields_unchanged']=False
    data['terrain_lod_cleanup']['height_revision']='town_ground_transition; original primary field and coverage preserved'
    (out/'catalog.json').write_text(json.dumps(data,indent=2,allow_nan=False)+'\n')
    audit=dict(catalog=str(out/'catalog.json'),catalog_sha256=sha(out/'catalog.json'),tiles=changed,
        maximum_height_change_m=float(abs(height-review['height_world']).max()),source_city_assets_unchanged=True,
        source_city_placements_unchanged=all(a==b for a,b in zip(source['placements'],data['placements'])if not a.get('asset','').startswith('BEP_Reforged_')))
    (out/'audit.json').write_text(json.dumps(audit,indent=2)+'\n');print(json.dumps(audit,indent=2))


if __name__=='__main__':main()
