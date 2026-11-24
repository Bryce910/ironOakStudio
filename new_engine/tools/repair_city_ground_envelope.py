#!/usr/bin/env python3
"""Stage supported town ground, exact paving cut-outs and residential plots."""
from pathlib import Path
import argparse,copy,json,shutil
import numpy as np
from build_city_countryside import ROOT,Landscape,mesh,field,readfield,sample,sha
from city_ground_ownership import CityGroundOwnership
from countryside_contact import TerrainSurfaces,correct,root_points


def segment_distance(points,segments):
    p=np.asarray(points);a=segments[:,0];d=segments[:,1]-a
    t=np.clip(((p[:,None]-a)*d).sum(2)/(d*d).sum(1),0,1)
    return np.linalg.norm(p[:,None]-(a+d*t[:,:,None]),axis=2).min(1)


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--catalog',type=Path,required=True)
    ap.add_argument('--baseline',type=Path,default=ROOT/'assets/purchased/bep_reforged/countryside-town-transition/catalog.json')
    ap.add_argument('--support',type=Path,default=ROOT/'assets/purchased/bep_reforged/city-ground-support')
    ap.add_argument('--output',type=Path,required=True);args=ap.parse_args()
    out=args.output.resolve()
    if out.exists():raise ValueError('Use a new immutable output directory')
    out.mkdir(parents=True);placement_candidate=json.loads(args.catalog.read_text());baseline=json.loads(args.baseline.read_text())
    # Merge only the reviewed house translations. The placement candidate may
    # predate current terrain, materials, foundation and root-contact repairs.
    data=copy.deepcopy(baseline)
    data['placement_repairs']=copy.deepcopy(placement_candidate['placement_repairs'])
    data['residential_repair']=copy.deepcopy(placement_candidate['residential_repair'])
    for repair in data['placement_repairs']:
        index=repair['original_index']
        if data['placements'][index]!=repair['original_record']:raise ValueError('House repair base record differs from current catalog')
        data['placements'][index]=copy.deepcopy(repair['replacement_record'])
    support=args.support.resolve();audit=json.loads((support/'certified-audit.json').read_text())
    # Preserve the original terrain fit and road longitudinal grade. Only the
    # near-town envelope uses the repaired field; distant scenery is unchanged.
    land=Landscape(baseline,city_height_path=support/'input-primary.height')
    land.city,land.city_origin,land.city_step=readfield(support/'showcase.height')
    land.residential_pads=data['residential_repair']['pads']
    land.ground_ownership=CityGroundOwnership(data,support/'source-ground-footprint.wkb')
    ground=out/'ground';ground.mkdir();shutil.copy2(support/'showcase.height',ground/'showcase.height')
    audit.update(input_primary=str(support/'input-primary.height'),output_height=str(ground/'showcase.height'),output_height_sha256=sha(ground/'showcase.height'))
    (out/'ground-support-audit.json').write_text(json.dumps(audit,indent=2)+'\n')
    data['ground_support_revision']=dict(original_field_sha256=audit['source_height_sha256'],repaired_field=str(ground/'showcase.height'),
        repaired_field_sha256=sha(ground/'showcase.height'),coverage_sha256=audit['coverage_sha256'],
        source_covered_samples_sha256=audit['source_covered_samples_sha256'],audit_path=str(out/'ground-support-audit.json'),audit_sha256=sha(out/'ground-support-audit.json'))
    # Broad lanes between the existing four rows and a short link to the loop.
    pads=land.residential_pads;lo=np.min([p['native_lo']for p in pads],0);hi=np.max([p['native_hi']for p in pads],0)
    segments=[]
    for x in [-184,-162,-140]:segments.append([[x,lo[1]-3],[x,hi[1]+3]])
    # The original boundary wall crosses the southeastern block. Streets stop
    # or turn around it; they must not invite walking through solid masonry.
    segments.append([[-118,108],[-118,hi[1]+3]])
    for z in [88,108,128]:segments.append([[lo[0]-3,z],[min(hi[0]+3,-126)if z==88 else hi[0]+3,z]])
    segments.extend([[[-145,58],[-140,68]],[[-140,68],[-140,88]]]);segments=np.asarray(segments,float)
    land.residential_path_distance=lambda p:segment_distance(p,segments)
    data['residential_repair']['street_segments_native']=segments.tolist()
    # Keep root collars out of buildings and the central walking lanes.
    grass=json.loads((ROOT/'assets/purchased/bep_reforged/countryside-grass-clumps/source-audit.json').read_text())
    sites={q['alias']:q['tuft_sites_native']for q in grass['geometry']}
    assets={a['name']:a for a in data['assets']};records=[];removed=[];root_cache={}
    for i,p in enumerate(data['placements']):
        a=assets[p['asset']];alias=a.get('native',{}).get('alias','');center=np.asarray(p['matrix_native']).reshape(4,4).T[[0,2],3]
        if alias.startswith('bep_nature_'):
            if alias not in root_cache:root_cache[alias]=root_points(a,sites)
            m=np.asarray(p['matrix_native']).reshape(4,4).T;roots=(root_cache[alias]@m[:3,:3].T+m[:3,3])[:,[0,2]]
            in_plot=any(np.any(np.all(roots>=np.asarray(q['native_lo'])-.5,axis=1)&np.all(roots<=np.asarray(q['native_hi'])+.5,axis=1))for q in pads)
            in_lane=bool(np.any(segment_distance(roots,segments)<1.75))
            if in_plot or in_lane:removed.append(dict(original_index=i,placement=p));continue
        records.append(p)
    data['placements']=records;(out/'cleared-dressing.json').write_text(json.dumps(removed,indent=2)+'\n')
    materials=json.loads((ROOT/'assets/purchased/bep_reforged/countryside-materials/ground-materials.json').read_text())
    changed=[]
    for a in data['assets']:
        if not a['name'].startswith('BEP_Countryside_Terrain_'):continue
        xi,zi=map(int,a['name'].split('_')[-2:]);low=np.array([-220+110*xi,-180+115*zi]);high=low+[110,115]
        city=not(np.any(high<land.lo-28.2)or np.any(low>land.hi+28.2))
        neighborhood=not(np.any(high<lo-4.2)or np.any(low>hi+4.2))
        if not(city or neighborhood):continue
        stats=[];files=[]
        for label,side in [('near',129),('mid',49)]:
            x=np.unique(np.r_[np.linspace(low[0],high[0],side),[v for v in [land.lo[0],land.hi[0]]if low[0]<v<high[0]]])
            z=np.unique(np.r_[np.linspace(low[1],high[1],side),[v for v in [land.lo[1],land.hi[1]]if low[1]<v<high[1]]])
            path=out/'models'/f'{a["name"]}_{label}.glb';stats.append(mesh(path,x,z,land,materials,a['pivot_offset_native'][1]));files.append(str(path))
        changed.append(a['name']);a.update(files=files,stats=stats,bounds=stats[0]['bounds'])
    old=np.load(args.baseline.parent/'landscape-review.npz');x,z=old['x_native'],old['z_native'];xx,zz=np.meshgrid(x,z)
    h=land.height(np.stack((xx,zz),-1));bounds=np.asarray(data['countryside']['native_bounds']);origin=bounds[:2]+land.origin[[0,2]];step=(bounds[2:]-bounds[:2])/1024
    field(ground/'countryside.height',h,origin,step)
    # Paving plus its exactly clipped complement now cover this whole region.
    field(ground/'countryside.coverage',np.ones_like(h),origin,step)
    data['countryside']['ground_files']=[str(ground/'countryside.height'),str(ground/'countryside.coverage')]
    np.savez_compressed(out/'landscape-review.npz',height_world=h,x_native=x,z_native=z,road_native=land.road,road_height_world=land.height(land.road))
    old_terrain=TerrainSurfaces(baseline,land.origin,lambda p:sample(old['height_world'],np.asarray(p)-land.origin[[0,2]],np.array([x[0],z[0]]),np.array([x[1]-x[0],z[1]-z[0]])))
    terrain=TerrainSurfaces(data,land.origin,lambda p:land.height(np.asarray(p)-land.origin[[0,2]]))
    indices=[];subset=copy.deepcopy(data);subset['placements']=[]
    for i,p in enumerate(data['placements']):
        a=assets[p['asset']];alias=a.get('native',{}).get('alias','')
        if not alias.startswith('bep_nature_'):continue
        m=np.asarray(p['matrix_native']).reshape(4,4).T;center=m[[0,2],3]
        radius=float(np.linalg.norm((np.asarray(a['bounds'])[1]-np.asarray(a['bounds'])[0])[[0,2]])*np.linalg.norm(m[:3,:3],axis=0).max())
        distance=np.linalg.norm(center-np.minimum(np.maximum(center,land.lo),land.hi))
        nearby=distance<29+radius or(np.all(center>=lo-6-radius)and np.all(center<=hi+6+radius))
        if not nearby:continue
        q=copy.deepcopy(p)
        if alias not in ['bep_nature_grass_long','bep_nature_grass_broad','bep_nature_clover','bep_nature_fern']:
            delta=float(terrain.sample((center+land.origin[[0,2]])[None])[0]-old_terrain.sample((center+land.origin[[0,2]])[None])[0])
            original=np.asarray(q.get('contact_original_matrix_native',q['matrix_native'])).reshape(4,4).T.copy();original[1,3]+=delta;q['contact_original_matrix_native']=original.T.flatten().tolist()
        indices.append(i);subset['placements'].append(q)
    fitted,contacts=correct(subset,terrain,grass)
    for i,p in zip(indices,fitted['placements']):data['placements'][i]=p
    contacts['candidate_indices']=indices;(out/'contact-report.json').write_text(json.dumps(contacts,indent=2)+'\n')
    data['town_ground_envelope']=dict(actual_source_area_m2=land.ground_ownership.polygon.area,rectangle_cut_removed=True,
        changed_terrain_assets=changed,refitted_plants=len(indices),cleared_dressing=len(removed),source_geometry_unchanged=True,
        original_terrain_fit_retained=True,exact_source_footprint=str(support/'source-ground-footprint.wkb'))
    (out/'catalog.json').write_text(json.dumps(data,indent=2,allow_nan=False)+'\n')
    print(json.dumps(dict(catalog=str(out/'catalog.json'),sha256=sha(out/'catalog.json'),changed_models=len(changed)*2,plants=len(indices),cleared=len(removed)),indent=2))


if __name__=='__main__':main()
