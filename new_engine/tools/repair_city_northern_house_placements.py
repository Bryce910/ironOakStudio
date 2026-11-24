#!/usr/bin/env python3
"""Stage explicit repairs for18 intersecting/unsupported backdrop houses.

Every source asset, yaw and scale stays exact. Only the declared18 placement
matrices change. The returned rectangular plot pads are applied by the caller
after its final terrain/road evaluation; this script never installs a project.
"""
from pathlib import Path
import argparse,copy,hashlib,json
import numpy as np
from scipy.spatial import cKDTree
from cook_village_publisher_scene import ROOT,Source,C
from build_city_countryside import Landscape,readfield
from audit_purchased_village_scene import ScenePackage,SOURCE as SOURCE_MAP

COMPONENTS=list(range(1541,1555))+[1632,1578,1556,1581]
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def rectangle_distance(a,b):return float(np.linalg.norm(np.maximum(np.maximum(a[0]-b[1],b[0]-a[1]),0)))
def asset_bounds(asset,placement):
    lo,hi=np.asarray(asset['bounds']);m=np.asarray(placement['matrix_native']).reshape(4,4).T.copy();m[:3,3]+=m[:3,:3]@asset['pivot_offset_native']
    corners=np.array([[x,y,z,1]for x in [lo[0],hi[0]]for y in [lo[1],hi[1]]for z in [lo[2],hi[2]]])@m.T
    return np.array([corners[:,[0,2]].min(0),corners[:,[0,2]].max(0)])

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--catalog',type=Path,default=ROOT/'assets/purchased/bep_reforged/countryside/catalog.json')
    ap.add_argument('--harmonic-height',type=Path,default=ROOT/'assets/purchased/bep_reforged/city-ground-support/showcase.height')
    ap.add_argument('--output',type=Path,default=ROOT/'assets/purchased/bep_reforged/northern-house-repair')
    args=ap.parse_args();base=json.loads(args.catalog.read_text());source=Source();assets={a['name']:a for a in base['assets']}
    if args.output.exists():raise ValueError('Immutable placement candidate exists')
    # Match the root pass: retain original road/RBF, swap only the scalar city
    # support grid. The new plots will be authoritative final height pads.
    land=Landscape(base);harmonic,origin,step=readfield(args.harmonic_height)
    if not(np.array_equal(origin,land.city_origin)and np.array_equal(step,land.city_step)):raise ValueError('Harmonic field grid mismatch')
    land.city=harmonic
    scene=json.loads((ROOT.parent/'.local-tools/village-source/publisher-scene.json').read_text());records={r['export_index']:r for r in scene['placements']}
    package=ScenePackage(SOURCE_MAP);visibility=[];selected=[]
    for index,placement in enumerate(base['placements']):
        component=placement.get('source_component')
        if component not in COMPONENTS:continue
        record=records[component];owner=package.props(record['owner_index']);properties=record['properties']
        flags={k:v for scope in [owner,properties]for k,v in scope.items()if any(s in k.lower()for s in ['hidden','visible','editoronly','attachparent'])}
        if flags:raise ValueError(('Explicit visibility/attachment flag requires review',component,flags))
        visibility.append(dict(source_component=component,owner_index=record['owner_index'],hierarchy=record['hierarchy'],actor_folder=record.get('actor_folder'),visibility_flags=flags,
            conclusion='Normal independently placed source StaticMeshActor; no explicit hidden/editor-only/attachment override'))
        family=placement['asset'].split('__')[0];v,_,_=source.geometry(family,0);m=np.asarray(placement['matrix_native']).reshape(4,4).T
        p=(v[:,:3]@C.T/100)@m[:3,:3].T+m[:3,3];bounds=np.array([p.min(0),p.max(0)])
        selected.append(dict(index=index,placement=placement,m=m,positions=p,bounds=bounds,area=float(np.prod(np.ptp(p[:,[0,2]],axis=0)))))
    if len(selected)!=18:raise ValueError('Unexpected source house count')
    # Put the two largest houses on opposite outer corners; all other lots
    # retain broad20m/22m center spacing and source orientation.
    lots=np.array([[x,z]for z in [78.,98.,118.,138.]for x in [-195.,-173.,-151.,-129.]]+[[ -107.,118.],[-107.,138.]])
    order=[0,15,3,12,1,2,4,5,6,7,8,9,10,11,13,14,16,17];selected.sort(key=lambda r:(-r['area'],r['placement']['source_component']))
    result=copy.deepcopy(base);repairs=[];pads=[];plot_bounds=[];placement_repairs=[]
    original_catalog=Path(base['opening']['unmodified_original_catalog'])
    original=json.loads(original_catalog.read_text())
    canonical=lambda record:hashlib.sha256(json.dumps(record,sort_keys=True,separators=(',',':')).encode()).hexdigest()
    for item,lot_index in zip(selected,order):
        center=lots[lot_index];p=item['positions'];xzcenter=(p[:,[0,2]].min(0)+p[:,[0,2]].max(0))/2;delta=center-xzcenter
        q=p.copy();q[:,0]+=delta[0];q[:,2]+=delta[1];lo=q[:,[0,2]].min(0);hi=q[:,[0,2]].max(0)
        xx,zz=np.meshgrid(np.linspace(lo[0],hi[0],7),np.linspace(lo[1],hi[1],7));grid=np.column_stack((xx.ravel(),zz.ravel()));h=land.height(grid)
        # The source actor origin is its intended grade plane. Its stone
        # basement/root extends below that plane by its original local offset.
        level=float(np.median(h));dy=level-land.origin[1]-item['m'][1,3];m=item['m'].copy();m[0,3]+=delta[0];m[2,3]+=delta[1];m[1,3]+=dy
        newmatrix=m.T.flatten().tolist();old=item['placement'];result['placements'][item['index']]['matrix_native']=newmatrix
        original_record=original['placements'][item['index']]
        if original_record!=old:raise ValueError(('Expected unchanged original source record before explicit repair',old['source_component']))
        replacement=result['placements'][item['index']]
        placement_repairs.append(dict(original_index=item['index'],source_component=old['source_component'],asset=old['asset'],
            original_record=copy.deepcopy(original_record),replacement_record=copy.deepcopy(replacement),
            original_record_sha256=canonical(original_record),replacement_record_sha256=canonical(replacement)))
        repair=dict(source_component=old['source_component'],placement_index=item['index'],actor_label=old.get('actor_label'),asset=old['asset'],
            old_matrix_native=old['matrix_native'],new_matrix_native=newmatrix,lot_center_native=center.tolist(),native_lo=lo.tolist(),native_hi=hi.tolist(),
            support_world_y=level,original_below_grade_root_offset_m=float(item['m'][1,3]-q[:,1].min()),terrain_before_min_world_y=float(h.min()),terrain_before_max_world_y=float(h.max()),terrain_before_relief_m=float(np.ptp(h)),
            rotation_and_scale_byte_exact=bool(np.array_equal(m[:3,:3],item['m'][:3,:3])))
        if not repair['rotation_and_scale_byte_exact']:raise ValueError('Source rotation or scale changed')
        repairs.append(repair);plot_bounds.append(np.array([lo,hi]));pads.append(dict(source_component=old['source_component'],native_lo=(lo-.1).tolist(),native_hi=(hi+.1).tolist(),support_world_y=level,blend_metres=4.))
    pairwise=min(rectangle_distance(a,b)for i,a in enumerate(plot_bounds)for b in plot_bounds[i+1:])
    if pairwise<3.:raise ValueError(('New full source house footprints are too close',pairwise))
    obstacle=[]
    for placement in base['placements']:
        name=placement['asset']
        if placement.get('source_component')in COMPONENTS:continue
        if name.startswith(('SM_House_','SM_Shop_','SM_Boundary_'))or name in ['SM_Castle','SM_City_Castle_Backdrop_Plinth']:
            obstacle.append((name,placement.get('source_component'),asset_bounds(assets[name],placement)))
    minimum_obstacle=min((rectangle_distance(a,b),name,component)for a in plot_bounds for name,component,b in obstacle)
    if minimum_obstacle[0]<3.:raise ValueError(('New plot intersects existing architecture',minimum_obstacle))
    minimum_road=min(float(np.linalg.norm(np.maximum(np.maximum(a[0]-land.road,land.road-a[1]),0),axis=1).min())for a in plot_bounds)
    if minimum_road<6.:raise ValueError(('New plot occupies proven road corridor',minimum_road))
    dressing=[]
    for index,placement in enumerate(base['placements']):
        if not placement['asset'].startswith('BEP_Reforged_'):continue
        box=asset_bounds(assets[placement['asset']],placement)
        if any(rectangle_distance(box,p)<4. for p in plot_bounds):dressing.append(dict(placement_index=index,asset=placement['asset']))
    repair=dict(schema=1,source_catalog=str(args.catalog.resolve()),source_catalog_sha256=sha(args.catalog),harmonic_field=str(args.harmonic_height.resolve()),harmonic_field_sha256=sha(args.harmonic_height),
        reason='Original visible northern backdrop houses intersect at incompatible elevations and lack source-ground support; explicit authorized placement cleanup',
        repairs=repairs,pads=pads,source_visibility_audit=visibility,minimum_house_clearance_m=pairwise,minimum_architecture_clearance_m=minimum_obstacle[0],minimum_proven_road_clearance_m=minimum_road,
        dressing_requiring_clearance=dressing,apply_order='Apply plot pads last to terrain height; clear dressing inside plots and lanes; then rebuild collision from these matrices',
        source_models_and_asset_records_exact=True,all_other_placement_matrices_exact=True)
    result['residential_repair']=repair;result['placement_repairs']=placement_repairs
    northern=[]
    for placement in base['placements']:
        if not placement['asset'].startswith('SM_House_')or'Light'in placement['asset']:continue
        oldmatrix=np.asarray(placement['matrix_native']).reshape(4,4).T
        if not 60<=oldmatrix[2,3]<=100:continue
        family=placement['asset'].split('__')[0];v,_,_=source.geometry(family,0)
        p=(v[:,:3]@C.T/100)@oldmatrix[:3,:3].T+oldmatrix[:3,3];low=p[:,1].min();roots=np.unique(np.round(p[p[:,1]<low+.05],5),axis=0)
        gap=roots[:,1]+land.origin[1]-land.height(roots[:,[0,2]])
        northern.append(dict(source_component=placement['source_component'],actor_label=placement.get('actor_label'),repaired=placement['source_component']in COMPONENTS,
            source_root_samples=len(roots),prior_minimum_root_gap_m=float(gap.min()),prior_maximum_root_gap_m=float(gap.max()),
            unrepaired_whole_house_floating=bool(placement['source_component']not in COMPONENTS and gap.min()>.3)))
    if len(northern)!=23 or any(r['unrepaired_whole_house_floating']for r in northern):raise ValueError('Additional unsupported north house remains')
    repair['all_23_northern_houses_audit']=northern
    for index,(old,new)in enumerate(zip(base['placements'],result['placements'])):
        if old.get('source_component')not in COMPONENTS and old!=new:raise ValueError(('Protected placement changed',index))
    if base['assets']!=result['assets']:raise ValueError('Source asset changed')
    args.output.mkdir(parents=True);(args.output/'catalog.json').write_text(json.dumps(result,indent=2,allow_nan=False)+'\n');(args.output/'residential-repair.json').write_text(json.dumps(repair,indent=2)+'\n')
    print(json.dumps(dict(status='STAGED_18_HOUSE_PLACEMENTS',catalog=str((args.output/'catalog.json').resolve()),repairs=len(repairs),minimum_house_clearance_m=pairwise,minimum_architecture_clearance_m=minimum_obstacle[0],minimum_road_clearance_m=minimum_road,dressing_to_clear=len(dressing),lots=[dict(component=r['source_component'],center=r['lot_center_native'],height=r['support_world_y'],prior_relief=r['terrain_before_relief_m'])for r in repairs]),indent=2))

if __name__=='__main__':main()
